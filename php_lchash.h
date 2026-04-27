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

/* Portable in-tree fallback table. Used when glibc's hsearch_r is not
 * available (musl, macOS, *BSD without _GNU_SOURCE, Windows). Open-addressing,
 * linear probing, FNV-1a key hash, power-of-two capacity. */
typedef struct _lchash_slot {
	char *key;
	void *data;
} lchash_slot;

typedef struct _lchash_table {
	lchash_slot *slots;
	size_t capacity;       /* always a power of two */
	size_t count;
} lchash_table;

PHP_MINIT_FUNCTION(lchash);
PHP_MSHUTDOWN_FUNCTION(lchash);
PHP_RINIT_FUNCTION(lchash);
PHP_RSHUTDOWN_FUNCTION(lchash);
PHP_MINFO_FUNCTION(lchash);

PHP_FUNCTION(lchash_create);
PHP_FUNCTION(lchash_destroy);
PHP_FUNCTION(lchash_insert);
PHP_FUNCTION(lchash_find);

ZEND_BEGIN_MODULE_GLOBALS(lchash)
	zend_bool is_init;
#ifdef HAVE_HSEARCH_R
	struct hsearch_data htab;
	/* hsearch_r leaks key/data on hdestroy_r; we walk this list at destroy. */
	char **keys;
	size_t key_count;
	size_t key_alloc;
#else
	lchash_table fallback;
#endif
ZEND_END_MODULE_GLOBALS(lchash)

#if defined(ZTS) && defined(COMPILE_DL_LCHASH)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

#define LCHASH_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(lchash, v)

#endif	/* PHP_LCHASH_H */
