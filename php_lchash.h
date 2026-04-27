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

#ifndef PHP_LCHASH_H
#define PHP_LCHASH_H

#define PHP_LCHASH_VERSION "1.0.0"

#ifdef HAVE_HSEARCH_R
/* _GNU_SOURCE is set globally in config.m4 for this TU, which is required for
 * glibc's <search.h> to expose hsearch_r/hcreate_r/hdestroy_r prototypes. */
# include <search.h>
#endif

extern zend_module_entry lchash_module_entry;
#define phpext_lchash_ptr &lchash_module_entry

#ifdef PHP_WIN32
# define PHP_LCHASH_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
# define PHP_LCHASH_API __attribute__ ((visibility("default")))
#else
# define PHP_LCHASH_API
#endif

#ifdef ZTS
# include "TSRM.h"
#endif

/* In-tree fallback table for non-glibc platforms (musl, macOS, *BSD,
 * Windows). Open-addressing, linear probing, FNV-1a key hash with a
 * per-request seed, power-of-two capacity, 0.5 load factor cap. */
typedef struct _lchash_slot {
	char *key;
	zend_string *value;
} lchash_slot;

typedef struct _lchash_table {
	lchash_slot *slots;
	size_t capacity;       /* always a power of two */
	size_t count;
} lchash_table;

typedef enum {
	LCHASH_FB_INSERTED,    /* fresh slot taken */
	LCHASH_FB_EXISTING,    /* key already present, no change */
	LCHASH_FB_FULL,        /* load cap reached, new key refused */
} lchash_fb_result;

#ifdef HAVE_HSEARCH_R
/* Parallel tracking array on the glibc path. hsearch_r does not free
 * keys/values on hdestroy_r, so we keep our own list to walk at destroy.
 * Pre-allocated to n_entries at create time -- hsearch_r refuses beyond
 * that capacity, which makes the array size statically sufficient. */
typedef struct _lchash_entry {
	char *key;
	zend_string *value;
} lchash_entry;
#endif

PHP_MINIT_FUNCTION(lchash);
PHP_RINIT_FUNCTION(lchash);
PHP_RSHUTDOWN_FUNCTION(lchash);
PHP_MINFO_FUNCTION(lchash);

PHP_FUNCTION(lchash_create);
PHP_FUNCTION(lchash_destroy);
PHP_FUNCTION(lchash_insert);
PHP_FUNCTION(lchash_find);

ZEND_BEGIN_MODULE_GLOBALS(lchash)
	zend_bool is_init;
	uint64_t hash_seed;        /* FNV-1a seed; reseeded per request */
#ifdef HAVE_HSEARCH_R
	struct hsearch_data htab;
	lchash_entry *entries;     /* pre-allocated to n_entries on create */
	size_t entry_count;
#else
	lchash_table fallback;
#endif
ZEND_END_MODULE_GLOBALS(lchash)

#if defined(ZTS) && defined(COMPILE_DL_LCHASH)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

#define LCHASH_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(lchash, v)

#endif	/* PHP_LCHASH_H */
