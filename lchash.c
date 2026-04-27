/*
  +----------------------------------------------------------------------+
  | Copyright (c) The PHP Group                                          |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,     |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | https://www.php.net/license/3_01.txt                                 |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Ilia Alshanetsky <ilia@ilia.ws>                              |
  +----------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#if PHP_VERSION_ID >= 80000
# include "ext/random/php_random.h"
#else
# include "ext/standard/php_random.h"
#endif
#include "php_lchash.h"

#if PHP_VERSION_ID >= 80000
# include "lchash_arginfo.h"
#else
# include "lchash_legacy_arginfo.h"
#endif

#include <errno.h>
#include <string.h>
#include <time.h>
#ifndef PHP_WIN32
# include <unistd.h>
#endif

/* Hard ceiling on n_entries. Keeps a single hostile lchash_create() call
 * from issuing an 8 GiB allocation that fatals the worker via memory_limit.
 * 1<<20 entries pre-allocates ~16 MiB for the tracking array on the glibc
 * path, which is the largest single allocation at the cap. */
#define LCHASH_MAX_ENTRIES (1u << 20)

ZEND_DECLARE_MODULE_GLOBALS(lchash)

/* All per-entry storage uses Zend's emalloc family + zend_string refcounting
 * (request-scoped). The bucket array inside glibc's struct hsearch_data is
 * libc-malloc'd by hcreate_r itself and cannot be redirected; that's the
 * only libc allocation in this extension. */

/* ------------------------------------------------------------------------
 * Hash seed for the in-tree fallback (DoS resistance against precomputed
 * collision attacks). Re-seeded per request from a CSPRNG so an attacker
 * who somehow recovers the seed within one request gets nothing reusable.
 * ------------------------------------------------------------------------ */
static uint64_t lchash_make_seed(void)
{
	uint64_t seed = 0;
	if (php_random_bytes(&seed, sizeof(seed), 0) == FAILURE) {
		/* Fallback: CSPRNG unavailable (rare). Combine sources individually
		 * weak but together unobservable from PHP userland. */
		seed  = (uint64_t) time(NULL);
#ifndef PHP_WIN32
		seed ^= ((uint64_t) getpid()) << 32;
#endif
		seed ^= (uint64_t)(uintptr_t) &seed;
		seed ^= (uint64_t)(uintptr_t) lchash_make_seed << 16;
	}
	return seed;
}

#ifndef HAVE_HSEARCH_R
/* ------------------------------------------------------------------------
 * Portable fallback: open-addressing linear-probing string-keyed table.
 * Capacity is rounded up to the next power of two so the modulo is a mask.
 * Load factor cap of 0.5 matches glibc hcreate_r's headroom.
 * ------------------------------------------------------------------------ */

static size_t lchash_next_pow2(size_t n)
{
	size_t p = 1;
	while (p < n) {
		p <<= 1;
	}
	return p;  /* upstream LCHASH_MAX_ENTRIES bound prevents overflow */
}

/* Seeded FNV-1a. Seed is XORed into both the basis and a final mixing
 * round so a passive observer of the table state cannot trivially
 * back-solve the seed from collision patterns. */
static uint64_t lchash_fnv1a(const char *s, uint64_t seed)
{
	uint64_t h = (1469598103934665603ULL ^ seed) * 1099511628211ULL;
	while (*s) {
		h ^= (unsigned char) *s++;
		h *= 1099511628211ULL;
	}
	h ^= (seed >> 32);
	h *= 1099511628211ULL;
	return h;
}

static void lchash_fb_create(lchash_table *t, size_t n_entries)
{
	size_t want = n_entries < 8 ? 16 : n_entries * 2;
	size_t cap = lchash_next_pow2(want);
	t->slots = (lchash_slot *) ecalloc(cap, sizeof(lchash_slot));
	t->capacity = cap;
	t->count = 0;
}

/* Returns the slot the key belongs in: the matching slot if present, or
 * the first empty slot encountered along the probe sequence (insertion
 * point). NULL only if the table is catastrophically full, which the load
 * factor cap (0.5) prevents in practice. */
static lchash_slot *lchash_fb_lookup(lchash_table *t, const char *key, uint64_t seed)
{
	size_t mask = t->capacity - 1;
	size_t i = (size_t) lchash_fnv1a(key, seed) & mask;
	for (size_t probe = 0; probe < t->capacity; probe++) {
		lchash_slot *s = &t->slots[(i + probe) & mask];
		if (s->key == NULL) {
			return s;
		}
		if (strcmp(s->key, key) == 0) {
			return s;
		}
	}
	return NULL;
}

static lchash_fb_result lchash_fb_insert(lchash_table *t, const char *key,
                                         zend_string *value, uint64_t seed)
{
	lchash_slot *s = lchash_fb_lookup(t, key, seed);

	/* Lookup BEFORE the capacity check: a duplicate insert at >0.5 load
	 * must still report EXISTING, not FULL, to match glibc ENTER. */
	if (s != NULL && s->key != NULL) {
		return LCHASH_FB_EXISTING;
	}
	if (s == NULL || t->count * 2 >= t->capacity) {
		return LCHASH_FB_FULL;
	}
	s->key = estrdup(key);
	s->value = value;  /* refcount transferred to the table */
	t->count++;
	return LCHASH_FB_INSERTED;
}

static zend_string *lchash_fb_find(lchash_table *t, const char *key, uint64_t seed)
{
	lchash_slot *s = lchash_fb_lookup(t, key, seed);
	if (s == NULL || s->key == NULL) {
		return NULL;
	}
	return s->value;
}

static void lchash_fb_destroy(lchash_table *t)
{
	if (!t->slots) {
		return;
	}
	for (size_t i = 0; i < t->capacity; i++) {
		if (t->slots[i].key) {
			efree(t->slots[i].key);
			zend_string_release(t->slots[i].value);
		}
	}
	efree(t->slots);
	t->slots = NULL;
	t->capacity = 0;
	t->count = 0;
}
#endif  /* !HAVE_HSEARCH_R */

#ifdef HAVE_HSEARCH_R
/* hsearch_r owns the bucket array but does not free key/value on hdestroy_r.
 * We keep a parallel (key, value) array, pre-allocated to n_entries at
 * create time so destroy is a single linear walk -- no hsearch_r(FIND)
 * round-trip, no dynamic growth, and the maximum size is bounded by what
 * hsearch_r itself accepts. */
static void lchash_track_entry(char *key, zend_string *value)
{
	LCHASH_G(entries)[LCHASH_G(entry_count)].key = key;
	LCHASH_G(entries)[LCHASH_G(entry_count)].value = value;
	LCHASH_G(entry_count)++;
}

static void lchash_glibc_destroy(void)
{
	for (size_t i = 0; i < LCHASH_G(entry_count); i++) {
		efree(LCHASH_G(entries)[i].key);
		zend_string_release(LCHASH_G(entries)[i].value);
	}
	if (LCHASH_G(entries)) {
		efree(LCHASH_G(entries));
		LCHASH_G(entries) = NULL;
	}
	LCHASH_G(entry_count) = 0;
	hdestroy_r(&LCHASH_G(htab));
	memset(&LCHASH_G(htab), 0, sizeof(LCHASH_G(htab)));
}
#endif  /* HAVE_HSEARCH_R */

/* ------------------------------------------------------------------------
 * Module lifecycle
 * ------------------------------------------------------------------------ */

static void php_lchash_init_globals(zend_lchash_globals *g)
{
	memset(g, 0, sizeof(*g));
}

PHP_MINIT_FUNCTION(lchash)
{
	ZEND_INIT_MODULE_GLOBALS(lchash, php_lchash_init_globals, NULL);
	return SUCCESS;
}

PHP_RINIT_FUNCTION(lchash)
{
#if defined(COMPILE_DL_LCHASH) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	/* Defense-in-depth: if a prior request leaked through without
	 * RSHUTDOWN unwinding (e.g. fatal exit before zend_deactivate),
	 * tear down before fresh init. Zend MM auto-frees emalloc'd memory
	 * at request end so this is rarely needed, but the libc-malloc'd
	 * hsearch_data buckets are not Zend MM tracked. */
	if (LCHASH_G(is_init)) {
#ifdef HAVE_HSEARCH_R
		lchash_glibc_destroy();
#else
		lchash_fb_destroy(&LCHASH_G(fallback));
#endif
	}
	LCHASH_G(is_init) = 0;
	LCHASH_G(hash_seed) = lchash_make_seed();
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(lchash)
{
	if (LCHASH_G(is_init)) {
#ifdef HAVE_HSEARCH_R
		lchash_glibc_destroy();
#else
		lchash_fb_destroy(&LCHASH_G(fallback));
#endif
		LCHASH_G(is_init) = 0;
	}
	return SUCCESS;
}

PHP_MINFO_FUNCTION(lchash)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "lchash support", "enabled");
	php_info_print_table_row(2, "Version", PHP_LCHASH_VERSION);
	php_info_print_table_row(2, "Backend",
#ifdef HAVE_HSEARCH_R
		"glibc hsearch_r"
#else
		"in-tree linear probing"
#endif
	);
	php_info_print_table_end();
}

/* ------------------------------------------------------------------------
 * Userland helpers
 * ------------------------------------------------------------------------ */

#define LCHASH_REQUIRE_INIT() \
	do { \
		if (!LCHASH_G(is_init)) { \
			php_error_docref(NULL, E_WARNING, "Hash table was not initialized"); \
			RETURN_FALSE; \
		} \
	} while (0)

/* glibc's strerror is not thread-safe; under ZTS, use the GNU strerror_r
 * which writes into a caller-supplied buffer. Other libcs (musl, macOS,
 * Windows, *BSD) ship a thread-safe strerror by default. */
static void lchash_strerror_warn(int err)
{
#if defined(ZTS) && defined(__GLIBC__)
	char buf[128];
	char *msg = strerror_r(err, buf, sizeof(buf));
	php_error_docref(NULL, E_WARNING, "%s (errno %d)", msg, err);
#else
	php_error_docref(NULL, E_WARNING, "%s (errno %d)", strerror(err), err);
#endif
}

/* ------------------------------------------------------------------------
 * Userland functions
 * ------------------------------------------------------------------------ */

PHP_FUNCTION(lchash_create)
{
	zend_long n_entries;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(n_entries)
	ZEND_PARSE_PARAMETERS_END();

	if (n_entries <= 0) {
		php_error_docref(NULL, E_WARNING, "Number of entries must be positive");
		RETURN_FALSE;
	}
	if (n_entries > (zend_long) LCHASH_MAX_ENTRIES) {
		php_error_docref(NULL, E_WARNING,
			"Number of entries " ZEND_LONG_FMT " exceeds the cap of %u",
			n_entries, (unsigned) LCHASH_MAX_ENTRIES);
		RETURN_FALSE;
	}

	if (LCHASH_G(is_init)) {
		php_error_docref(NULL, E_WARNING, "Hash table already exists");
		RETURN_FALSE;
	}

#ifdef HAVE_HSEARCH_R
	memset(&LCHASH_G(htab), 0, sizeof(LCHASH_G(htab)));
	if (hcreate_r((size_t) n_entries, &LCHASH_G(htab)) == 0) {
		lchash_strerror_warn(errno);
		RETURN_FALSE;
	}
	LCHASH_G(entries) = (lchash_entry *) ecalloc(
		(size_t) n_entries, sizeof(lchash_entry));
	LCHASH_G(entry_count) = 0;
	LCHASH_G(entry_capacity) = (size_t) n_entries;
#else
	lchash_fb_create(&LCHASH_G(fallback), (size_t) n_entries);
#endif

	LCHASH_G(is_init) = 1;
	RETURN_TRUE;
}

PHP_FUNCTION(lchash_destroy)
{
	ZEND_PARSE_PARAMETERS_NONE();

	LCHASH_REQUIRE_INIT();

#ifdef HAVE_HSEARCH_R
	lchash_glibc_destroy();
#else
	lchash_fb_destroy(&LCHASH_G(fallback));
#endif
	LCHASH_G(is_init) = 0;
	RETURN_TRUE;
}

PHP_FUNCTION(lchash_insert)
{
	char *key, *val;
	size_t key_len, val_len;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STRING(key, key_len)
		Z_PARAM_STRING(val, val_len)
	ZEND_PARSE_PARAMETERS_END();

	LCHASH_REQUIRE_INIT();

	if (key_len == 0) {
		php_error_docref(NULL, E_WARNING, "Key must not be empty");
		RETURN_FALSE;
	}
	if (memchr(key, '\0', key_len) != NULL) {
		php_error_docref(NULL, E_WARNING, "Key must not contain NUL bytes");
		RETURN_FALSE;
	}

	zend_string *zstr = zend_string_init(val, val_len, 0);

#ifdef HAVE_HSEARCH_R
	/* Probe FIND first so a duplicate-key insert doesn't count against the
	 * entry_capacity cap. Only fresh inserts consume an entries[] slot. */
	{
		ENTRY probe = { key, NULL };
		ENTRY *hit = NULL;
		if (hsearch_r(probe, FIND, &hit, &LCHASH_G(htab)) != 0 && hit != NULL) {
			zend_string_release(zstr);
			RETURN_TRUE;
		}
	}
	if (LCHASH_G(entry_count) >= LCHASH_G(entry_capacity)) {
		/* User asked for n_entries slots; hsearch_r's internal rounding
		 * would let us overshoot, but our entries[] tracking is sized
		 * exactly to n_entries. Refuse here to keep them in sync. */
		zend_string_release(zstr);
		errno = ENOMEM;
		lchash_strerror_warn(errno);
		RETURN_FALSE;
	}
	char *key_dup = estrdup(key);
	ENTRY in = { key_dup, zstr };
	ENTRY *out = NULL;
	if (hsearch_r(in, ENTER, &out, &LCHASH_G(htab)) == 0) {
		efree(key_dup);
		zend_string_release(zstr);
		lchash_strerror_warn(errno);
		RETURN_FALSE;
	}
	if (out->data != zstr) {
		/* Existing entry; matches glibc hsearch ENTER semantics
		 * (do not overwrite). The returned entry is the prior one. */
		efree(key_dup);
		zend_string_release(zstr);
		RETURN_TRUE;
	}
	/* Fresh insert: track for destroy. entries[] was sized to n_entries
	 * at create time and hsearch_r refuses beyond that, so this slot
	 * is guaranteed to exist. */
	lchash_track_entry(key_dup, zstr);
	RETURN_TRUE;
#else
	switch (lchash_fb_insert(&LCHASH_G(fallback), key, zstr, LCHASH_G(hash_seed))) {
		case LCHASH_FB_INSERTED:
			RETURN_TRUE;
		case LCHASH_FB_EXISTING:
			zend_string_release(zstr);
			RETURN_TRUE;
		case LCHASH_FB_FULL:
		default:
			zend_string_release(zstr);
			errno = ENOMEM;
			lchash_strerror_warn(errno);
			RETURN_FALSE;
	}
#endif
}

PHP_FUNCTION(lchash_find)
{
	char *key;
	size_t key_len;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(key, key_len)
	ZEND_PARSE_PARAMETERS_END();

	LCHASH_REQUIRE_INIT();

	if (key_len == 0 || memchr(key, '\0', key_len) != NULL) {
		RETURN_FALSE;
	}

	zend_string *zstr = NULL;

#ifdef HAVE_HSEARCH_R
	ENTRY query = { key, NULL };
	ENTRY *found = NULL;
	if (hsearch_r(query, FIND, &found, &LCHASH_G(htab)) != 0 && found != NULL) {
		zstr = (zend_string *) found->data;
	}
#else
	zstr = lchash_fb_find(&LCHASH_G(fallback), key, LCHASH_G(hash_seed));
#endif

	if (zstr == NULL) {
		RETURN_FALSE;
	}
	RETURN_STR_COPY(zstr);
}

/* ------------------------------------------------------------------------
 * Module entry
 * ------------------------------------------------------------------------ */

zend_module_entry lchash_module_entry = {
	STANDARD_MODULE_HEADER,
	"lchash",
	ext_functions,           /* generated by gen_stub.php into lchash_arginfo.h */
	PHP_MINIT(lchash),
	NULL,                    /* MSHUTDOWN */
	PHP_RINIT(lchash),
	PHP_RSHUTDOWN(lchash),
	PHP_MINFO(lchash),
	PHP_LCHASH_VERSION,
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_LCHASH
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(lchash)
#endif
