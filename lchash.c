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
#include "php_lchash.h"

#if PHP_VERSION_ID >= 80000
# include "lchash_arginfo.h"
#else
# include "lchash_legacy_arginfo.h"
#endif

#include <errno.h>
#include <string.h>

ZEND_DECLARE_MODULE_GLOBALS(lchash)

/* All per-entry storage uses Zend's emalloc family (request-scoped). The
 * bucket array inside glibc's struct hsearch_data is libc-malloc'd by
 * hcreate_r itself and cannot be redirected; that's the only libc
 * allocation in this extension. */

/* Stored payload layout, identical between glibc and fallback paths:
 *   [size_t length][length bytes][\0]
 * The trailing NUL is for convenience; length is authoritative so values are
 * binary-safe. */
typedef struct _lchash_payload {
	size_t len;
	char data[1];
} lchash_payload;

static inline lchash_payload *lchash_payload_new(const char *src, size_t len)
{
	lchash_payload *p = (lchash_payload *) emalloc(sizeof(size_t) + len + 1);
	p->len = len;
	if (len) {
		memcpy(p->data, src, len);
	}
	p->data[len] = '\0';
	return p;
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
		if (p == 0) {
			return 0;  /* overflow */
		}
	}
	return p;
}

static uint64_t lchash_fnv1a(const char *s)
{
	uint64_t h = 1469598103934665603ULL;
	while (*s) {
		h ^= (unsigned char) *s++;
		h *= 1099511628211ULL;
	}
	return h;
}

static int lchash_fb_create(lchash_table *t, size_t n_entries)
{
	/* Round n_entries up to a power of two and double for headroom. */
	size_t want = n_entries < 8 ? 16 : n_entries * 2;
	size_t cap = lchash_next_pow2(want);
	if (cap == 0) {
		errno = ENOMEM;
		return 0;
	}
	t->slots = (lchash_slot *) ecalloc(cap, sizeof(lchash_slot));
	t->capacity = cap;
	t->count = 0;
	return 1;
}

static lchash_slot *lchash_fb_lookup(lchash_table *t, const char *key)
{
	size_t mask = t->capacity - 1;
	size_t i = (size_t) lchash_fnv1a(key) & mask;
	for (size_t probe = 0; probe < t->capacity; probe++) {
		lchash_slot *s = &t->slots[(i + probe) & mask];
		if (s->key == NULL) {
			return s;  /* empty slot -- key not present */
		}
		if (strcmp(s->key, key) == 0) {
			return s;  /* hit */
		}
	}
	return NULL;  /* table completely full -- shouldn't happen at <0.5 load */
}

static int lchash_fb_insert(lchash_table *t, const char *key, void *data)
{
	if (t->count * 2 >= t->capacity) {
		errno = ENOMEM;
		return 0;
	}
	lchash_slot *s = lchash_fb_lookup(t, key);
	if (!s) {
		errno = ENOMEM;
		return 0;
	}
	if (s->key != NULL) {
		/* Existing key -- glibc hsearch with ENTER returns the existing entry
		 * unchanged. Match that behavior: do not overwrite. */
		return 1;
	}
	s->key = estrdup(key);
	s->data = data;
	t->count++;
	return 1;
}

static lchash_payload *lchash_fb_find(lchash_table *t, const char *key)
{
	lchash_slot *s = lchash_fb_lookup(t, key);
	if (s == NULL || s->key == NULL) {
		return NULL;
	}
	return (lchash_payload *) s->data;
}

static void lchash_fb_destroy(lchash_table *t)
{
	if (!t->slots) {
		return;
	}
	for (size_t i = 0; i < t->capacity; i++) {
		if (t->slots[i].key) {
			efree(t->slots[i].key);
			efree(t->slots[i].data);
		}
	}
	efree(t->slots);
	t->slots = NULL;
	t->capacity = 0;
	t->count = 0;
}
#endif  /* !HAVE_HSEARCH_R */

#ifdef HAVE_HSEARCH_R
/* hsearch_r owns the table buckets but does not free key/data on hdestroy_r.
 * We track inserted keys in a parallel array so destroy can walk and free. */
static void lchash_track_key(char *key)
{
	if (LCHASH_G(key_count) == LCHASH_G(key_alloc)) {
		size_t new_alloc = LCHASH_G(key_alloc) ? LCHASH_G(key_alloc) * 2 : 16;
		LCHASH_G(keys) = (char **) erealloc(LCHASH_G(keys), new_alloc * sizeof(char *));
		LCHASH_G(key_alloc) = new_alloc;
	}
	LCHASH_G(keys)[LCHASH_G(key_count)++] = key;
}

static void lchash_glibc_destroy(void)
{
	for (size_t i = 0; i < LCHASH_G(key_count); i++) {
		ENTRY query = { LCHASH_G(keys)[i], NULL };
		ENTRY *found = NULL;
		(void) hsearch_r(query, FIND, &found, &LCHASH_G(htab));
		if (found && found->data) {
			efree(found->data);
		}
		efree(LCHASH_G(keys)[i]);
	}
	if (LCHASH_G(keys)) {
		efree(LCHASH_G(keys));
		LCHASH_G(keys) = NULL;
	}
	LCHASH_G(key_count) = 0;
	LCHASH_G(key_alloc) = 0;
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
	LCHASH_G(is_init) = 0;
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
 * Userland functions
 * ------------------------------------------------------------------------ */

#define LCHASH_REQUIRE_INIT() \
	do { \
		if (!LCHASH_G(is_init)) { \
			php_error_docref(NULL, E_WARNING, "Hash table was not initialized"); \
			RETURN_FALSE; \
		} \
	} while (0)

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

	if (LCHASH_G(is_init)) {
		php_error_docref(NULL, E_WARNING, "Hash table already exists");
		RETURN_FALSE;
	}

#ifdef HAVE_HSEARCH_R
	memset(&LCHASH_G(htab), 0, sizeof(LCHASH_G(htab)));
	if (hcreate_r((size_t) n_entries, &LCHASH_G(htab)) == 0) {
		php_error_docref(NULL, E_WARNING, "%s (errno %d)", strerror(errno), errno);
		RETURN_FALSE;
	}
#else
	if (!lchash_fb_create(&LCHASH_G(fallback), (size_t) n_entries)) {
		php_error_docref(NULL, E_WARNING, "%s (errno %d)", strerror(errno), errno);
		RETURN_FALSE;
	}
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
	if (strlen(key) != key_len) {
		php_error_docref(NULL, E_WARNING, "Key must not contain NUL bytes");
		RETURN_FALSE;
	}

	lchash_payload *payload = lchash_payload_new(val, val_len);

#ifdef HAVE_HSEARCH_R
	char *key_dup = estrdup(key);
	ENTRY in = { key_dup, payload };
	ENTRY *out = NULL;
	if (hsearch_r(in, ENTER, &out, &LCHASH_G(htab)) == 0) {
		efree(key_dup);
		efree(payload);
		php_error_docref(NULL, E_WARNING, "%s (errno %d)", strerror(errno), errno);
		RETURN_FALSE;
	}
	if (out->data != payload) {
		/* Existing entry, hsearch_r returned the prior one. Match glibc
		 * semantics: do not overwrite. Free the rejected payload + key dup. */
		efree(key_dup);
		efree(payload);
		RETURN_TRUE;
	}
	lchash_track_key(key_dup);
	RETURN_TRUE;
#else
	if (!lchash_fb_insert(&LCHASH_G(fallback), key, payload)) {
		efree(payload);
		php_error_docref(NULL, E_WARNING, "%s (errno %d)", strerror(errno), errno);
		RETURN_FALSE;
	}
	RETURN_TRUE;
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

	if (key_len == 0 || strlen(key) != key_len) {
		RETURN_FALSE;
	}

	lchash_payload *payload = NULL;

#ifdef HAVE_HSEARCH_R
	ENTRY query = { key, NULL };
	ENTRY *found = NULL;
	if (hsearch_r(query, FIND, &found, &LCHASH_G(htab)) != 0 && found != NULL) {
		payload = (lchash_payload *) found->data;
	}
#else
	payload = lchash_fb_find(&LCHASH_G(fallback), key);
#endif

	if (payload == NULL) {
		RETURN_FALSE;
	}
	RETURN_STRINGL(payload->data, payload->len);
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
