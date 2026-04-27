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
#include "zend_exceptions.h"
#include "php_lchash.h"

#if PHP_VERSION_ID >= 80000
# include "lchash_arginfo.h"
#else
# include "lchash_legacy_arginfo.h"
#endif

#include <errno.h>
#include <string.h>

/* RETURN_THROWS arrived in PHP 8.0; before that, throwing an Error from
 * a userland function just falls through without a special macro. */
#if PHP_VERSION_ID < 80000
# define RETURN_THROWS() return
#endif

/* Wire klib's allocator hooks to Zend MM. The bucket array, the
 * khash_t struct itself, and any rehashes go through the request-scoped
 * allocator and so participate in Zend's leak detector on debug builds. */
#define kmalloc(Z)     emalloc(Z)
#define kcalloc(N, Z)  ecalloc((N), (Z))
#define krealloc(P, Z) erealloc((P), (Z))
#define kfree(P)       efree(P)

#include "khash.h"

/* zend_string *-keyed khash flavor. Used by both the procedural API
 * (one global table) and the OO API (per-instance tables). Reuses the
 * engine's cached DJBX33A hash and zend_string_equals (length + memcmp),
 * so per-entry keys are refcount bumps on the caller's string and keys
 * are binary-safe (NUL bytes preserved, length-aware comparison). */
#define lchash_zs_hash_func(s) ((khint_t) zend_string_hash_val(s))
#define lchash_zs_eq_func(a, b) zend_string_equals((a), (b))

KHASH_INIT(lchashz, zend_string *, zend_string *, 1,
           lchash_zs_hash_func, lchash_zs_eq_func)

#define LCHASH_TABLE() ((khash_t(lchashz) *) LCHASH_G(table))

/* Hard ceiling on n_entries. Single-call DoS guard: PHP_INT_MAX-class
 * arguments would otherwise drag the worker through memory_limit. */
#define LCHASH_MAX_ENTRIES (1u << 20)

ZEND_DECLARE_MODULE_GLOBALS(lchash)

#define LCHASH_REQUIRE_INIT() \
	do { \
		if (LCHASH_G(table) == NULL) { \
			php_error_docref(NULL, E_WARNING, "Hash table was not initialized"); \
			RETURN_FALSE; \
		} \
	} while (0)

static void lchash_table_destroy(void)
{
	khash_t(lchashz) *h = LCHASH_TABLE();
	if (!h) {
		return;
	}
	for (khint_t k = kh_begin(h); k != kh_end(h); ++k) {
		if (kh_exist(h, k)) {
			zend_string_release(kh_key(h, k));
			zend_string_release(kh_val(h, k));
		}
	}
	kh_destroy(lchashz, h);
	LCHASH_G(table) = NULL;
}

/* ------------------------------------------------------------------------
 * OO class: LcHash
 * ------------------------------------------------------------------------ */

typedef struct _lchash_object {
	khash_t(lchashz) *table;
	uint32_t max_entries;
	zend_object std;
} lchash_object;

static zend_class_entry *lchash_ce = NULL;
static zend_object_handlers lchash_object_handlers;

#if PHP_VERSION_ID >= 80000
# define LCHASH_OBJ_PARAM    zend_object *object
# define LCHASH_OBJ_INTERN() lchash_obj_from_zend(object)
#else
# define LCHASH_OBJ_PARAM    zval *object
# define LCHASH_OBJ_INTERN() lchash_obj_from_zend(Z_OBJ_P(object))
#endif

static inline lchash_object *lchash_obj_from_zend(zend_object *obj)
{
	return (lchash_object *) ((char *) obj - XtOffsetOf(lchash_object, std));
}

static zend_object *lchash_create_object(zend_class_entry *ce)
{
	lchash_object *intern = zend_object_alloc(sizeof(lchash_object), ce);
	zend_object_std_init(&intern->std, ce);
	object_properties_init(&intern->std, ce);
	intern->std.handlers = &lchash_object_handlers;
	intern->table = NULL;
	intern->max_entries = LCHASH_MAX_ENTRIES;
	return &intern->std;
}

static void lchash_free_object(zend_object *obj)
{
	lchash_object *intern = lchash_obj_from_zend(obj);
	if (intern->table) {
		khash_t(lchashz) *h = intern->table;
		for (khint_t k = kh_begin(h); k != kh_end(h); ++k) {
			if (kh_exist(h, k)) {
				zend_string_release(kh_key(h, k));
				zend_string_release(kh_val(h, k));
			}
		}
		kh_destroy(lchashz, h);
		intern->table = NULL;
	}
	zend_object_std_dtor(&intern->std);
}

/* Lazy allocator: defer the bucket-array allocation until the first
 * write. Constructing many empty LcHash instances should not cost
 * max_entries worth of bucket flags+keys+vals up front. */
static inline void lchash_ensure_table(lchash_object *intern)
{
	if (UNEXPECTED(intern->table == NULL)) {
		intern->table = kh_init(lchashz);
		/* Caller's max_entries is a size hint; klib's load factor and
		 * power-of-2 rounding are handled inside kh_resize. */
		kh_resize(lchashz, intern->table, (khint_t) intern->max_entries);
	}
}

static zval *lchash_read_dimension(LCHASH_OBJ_PARAM, zval *offset, int type, zval *rv)
{
	lchash_object *intern = LCHASH_OBJ_INTERN();

	if (UNEXPECTED(offset == NULL)) {
		zend_throw_error(NULL, "Cannot read empty offset on LcHash");
		return &EG(uninitialized_zval);
	}

	zend_string *tmp;
	zend_string *key = zval_get_tmp_string(offset, &tmp);
	if (UNEXPECTED(EG(exception))) {
		zend_tmp_string_release(tmp);
		return &EG(uninitialized_zval);
	}

	if (UNEXPECTED(ZSTR_LEN(key) == 0)) {
		zend_throw_error(NULL, "LcHash key must not be empty");
		zend_tmp_string_release(tmp);
		return &EG(uninitialized_zval);
	}

	if (intern->table != NULL) {
		khint_t iter = kh_get(lchashz, intern->table, key);
		if (iter != kh_end(intern->table)) {
			ZVAL_STR_COPY(rv, kh_val(intern->table, iter));
			zend_tmp_string_release(tmp);
			return rv;
		}
	}
	ZVAL_NULL(rv);
	zend_tmp_string_release(tmp);
	return rv;
}

static void lchash_write_dimension(LCHASH_OBJ_PARAM, zval *offset, zval *value)
{
	lchash_object *intern = LCHASH_OBJ_INTERN();

	if (UNEXPECTED(offset == NULL)) {
		zend_throw_error(NULL, "LcHash does not support append ($lc[] = ...)");
		return;
	}

	zend_string *key_tmp;
	zend_string *key = zval_get_tmp_string(offset, &key_tmp);
	if (UNEXPECTED(EG(exception))) {
		zend_tmp_string_release(key_tmp);
		return;
	}
	if (UNEXPECTED(ZSTR_LEN(key) == 0)) {
		zend_throw_error(NULL, "LcHash key must not be empty");
		zend_tmp_string_release(key_tmp);
		return;
	}

	zend_string *val_tmp;
	zend_string *val_str = zval_get_tmp_string(value, &val_tmp);
	if (UNEXPECTED(EG(exception))) {
		zend_tmp_string_release(key_tmp);
		zend_tmp_string_release(val_tmp);
		return;
	}

	lchash_ensure_table(intern);

	/* Single-probe insert/update: kh_put returns ret==0 if the key already
	 * existed, ret==1 for a new bucket, ret==2 for a deleted-bucket reuse.
	 * Pre-bumping the key's refcount lets klib retain the pointer; we drop
	 * the bump on the existing-key branch since klib won't store it. */
	zend_string *stored_key = zend_string_copy(key);
	int ret;
	khint_t iter = kh_put(lchashz, intern->table, stored_key, &ret);
	if (UNEXPECTED(ret < 0)) {
		zend_string_release(stored_key);
		zend_tmp_string_release(key_tmp);
		zend_tmp_string_release(val_tmp);
		zend_throw_error(NULL, "kh_put failed");
		return;
	}

	if (ret == 0) {
		/* Existing key: replace value. Bump-then-release ordering protects
		 * against $lc[$k] = $lc[$k] aliasing where the read result holds
		 * the only refcount on the prior value. */
		zend_string_release(stored_key);
		zend_string *new_val = zend_string_copy(val_str);
		zend_string *old_val = kh_val(intern->table, iter);
		kh_val(intern->table, iter) = new_val;
		zend_string_release(old_val);
	} else {
		/* New entry. Capacity check happens post-insert: kh_put may have
		 * grown the bucket array; if we now exceed the user's cap, undo
		 * the bucket placement (kh_del marks it deleted, geometry stays). */
		if (UNEXPECTED(kh_size(intern->table) > intern->max_entries)) {
			kh_del(lchashz, intern->table, iter);
			zend_string_release(stored_key);
			zend_tmp_string_release(key_tmp);
			zend_tmp_string_release(val_tmp);
			zend_throw_error(NULL,
				"LcHash at capacity (%u entries)", intern->max_entries);
			return;
		}
		kh_val(intern->table, iter) = zend_string_copy(val_str);
	}

	zend_tmp_string_release(key_tmp);
	zend_tmp_string_release(val_tmp);
}

static int lchash_has_dimension(LCHASH_OBJ_PARAM, zval *offset, int check_empty)
{
	lchash_object *intern = LCHASH_OBJ_INTERN();

	if (offset == NULL || intern->table == NULL) return 0;

	zend_string *tmp;
	zend_string *key = zval_get_tmp_string(offset, &tmp);
	if (UNEXPECTED(EG(exception))) {
		zend_tmp_string_release(tmp);
		return 0;
	}

	int result = 0;
	if (ZSTR_LEN(key) > 0) {
		khint_t iter = kh_get(lchashz, intern->table, key);
		if (iter != kh_end(intern->table)) {
			result = 1;
			if (check_empty) {
				zend_string *v = kh_val(intern->table, iter);
				if (ZSTR_LEN(v) == 0
					|| (ZSTR_LEN(v) == 1 && ZSTR_VAL(v)[0] == '0')) {
					result = 0;
				}
			}
		}
	}
	zend_tmp_string_release(tmp);
	return result;
}

static void lchash_unset_dimension(LCHASH_OBJ_PARAM, zval *offset)
{
	lchash_object *intern = LCHASH_OBJ_INTERN();

	if (offset == NULL || intern->table == NULL) return;

	zend_string *tmp;
	zend_string *key = zval_get_tmp_string(offset, &tmp);
	if (UNEXPECTED(EG(exception))) {
		zend_tmp_string_release(tmp);
		return;
	}

	if (ZSTR_LEN(key) > 0) {
		khint_t iter = kh_get(lchashz, intern->table, key);
		if (iter != kh_end(intern->table)) {
			zend_string_release(kh_key(intern->table, iter));
			zend_string_release(kh_val(intern->table, iter));
			kh_del(lchashz, intern->table, iter);
		}
	}
	zend_tmp_string_release(tmp);
}

PHP_METHOD(LcHash, __construct)
{
	zend_long n_entries = LCHASH_MAX_ENTRIES;

	ZEND_PARSE_PARAMETERS_START(0, 1)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG(n_entries)
	ZEND_PARSE_PARAMETERS_END();

	if (n_entries <= 0) {
		zend_throw_error(NULL, "LcHash n_entries must be positive");
		RETURN_THROWS();
	}
	if (n_entries > (zend_long) LCHASH_MAX_ENTRIES) {
		zend_throw_error(NULL,
			"LcHash n_entries " ZEND_LONG_FMT " exceeds the cap of %u",
			n_entries, (unsigned) LCHASH_MAX_ENTRIES);
		RETURN_THROWS();
	}

	lchash_object *intern = lchash_obj_from_zend(Z_OBJ_P(ZEND_THIS));
	if (UNEXPECTED(intern->table != NULL)) {
		zend_throw_error(NULL, "LcHash is already constructed");
		RETURN_THROWS();
	}
	/* Lazy allocation: don't touch the bucket array until first write. */
	intern->max_entries = (uint32_t) n_entries;
}

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

	lchash_ce = register_class_LcHash();
	lchash_ce->create_object = lchash_create_object;

	memcpy(&lchash_object_handlers, zend_get_std_object_handlers(),
		sizeof(zend_object_handlers));
	lchash_object_handlers.offset = XtOffsetOf(lchash_object, std);
	lchash_object_handlers.free_obj = lchash_free_object;
	lchash_object_handlers.clone_obj = NULL;
	lchash_object_handlers.read_dimension = lchash_read_dimension;
	lchash_object_handlers.write_dimension = lchash_write_dimension;
	lchash_object_handlers.has_dimension = lchash_has_dimension;
	lchash_object_handlers.unset_dimension = lchash_unset_dimension;

	return SUCCESS;
}

PHP_RINIT_FUNCTION(lchash)
{
#if defined(COMPILE_DL_LCHASH) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	/* Defense-in-depth: if a prior request leaked through without
	 * RSHUTDOWN unwinding, tear down before fresh init. */
	if (LCHASH_G(table)) {
		lchash_table_destroy();
	}
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(lchash)
{
	lchash_table_destroy();
	return SUCCESS;
}

PHP_MINFO_FUNCTION(lchash)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "lchash support", "enabled");
	php_info_print_table_row(2, "Version", PHP_LCHASH_VERSION);
	php_info_print_table_row(2, "Backend", "klib khash");
	php_info_print_table_end();
}

/* ------------------------------------------------------------------------
 * Procedural userland functions
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
	if (LCHASH_G(table)) {
		php_error_docref(NULL, E_WARNING, "Hash table already exists");
		RETURN_FALSE;
	}

	khash_t(lchashz) *h = kh_init(lchashz);
	/* Pre-size to avoid rehashes during the first n_entries inserts. */
	kh_resize(lchashz, h, (khint_t) n_entries);
	LCHASH_G(table) = h;
	RETURN_TRUE;
}

PHP_FUNCTION(lchash_destroy)
{
	ZEND_PARSE_PARAMETERS_NONE();

	LCHASH_REQUIRE_INIT();

	lchash_table_destroy();
	RETURN_TRUE;
}

PHP_FUNCTION(lchash_insert)
{
	zend_string *key, *val;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STR(key)
		Z_PARAM_STR(val)
	ZEND_PARSE_PARAMETERS_END();

	LCHASH_REQUIRE_INIT();

	if (ZSTR_LEN(key) == 0) {
		php_error_docref(NULL, E_WARNING, "Key must not be empty");
		RETURN_FALSE;
	}

	khash_t(lchashz) *h = LCHASH_TABLE();

	/* Single-probe insert: kh_put returns ret==0 if the key already
	 * existed (first writer wins, matches glibc hsearch ENTER semantics),
	 * ret==1|2 for a new bucket. Capacity check is post-insert; on
	 * overflow we undo the bucket placement via kh_del. */
	zend_string *stored_key = zend_string_copy(key);
	int ret;
	khint_t iter = kh_put(lchashz, h, stored_key, &ret);
	if (UNEXPECTED(ret < 0)) {
		zend_string_release(stored_key);
		php_error_docref(NULL, E_WARNING, "kh_put failed");
		RETURN_FALSE;
	}
	if (ret == 0) {
		/* First writer wins: drop our refcount bump on the key,
		 * leave the existing value untouched. */
		zend_string_release(stored_key);
		RETURN_TRUE;
	}
	if (UNEXPECTED(kh_size(h) > LCHASH_MAX_ENTRIES)) {
		kh_del(lchashz, h, iter);
		zend_string_release(stored_key);
		php_error_docref(NULL, E_WARNING,
			"Hash table at capacity (%u entries)", LCHASH_MAX_ENTRIES);
		RETURN_FALSE;
	}
	kh_val(h, iter) = zend_string_copy(val);
	RETURN_TRUE;
}

PHP_FUNCTION(lchash_find)
{
	zend_string *key;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(key)
	ZEND_PARSE_PARAMETERS_END();

	LCHASH_REQUIRE_INIT();

	if (ZSTR_LEN(key) == 0) {
		RETURN_FALSE;
	}

	khash_t(lchashz) *h = LCHASH_TABLE();
	khint_t iter = kh_get(lchashz, h, key);
	if (iter == kh_end(h)) {
		RETURN_FALSE;
	}
	RETURN_STR_COPY(kh_val(h, iter));
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
