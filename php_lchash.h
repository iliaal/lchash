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

PHP_MINIT_FUNCTION(lchash);
PHP_RINIT_FUNCTION(lchash);
PHP_RSHUTDOWN_FUNCTION(lchash);
PHP_MINFO_FUNCTION(lchash);

PHP_FUNCTION(lchash_create);
PHP_FUNCTION(lchash_destroy);
PHP_FUNCTION(lchash_insert);
PHP_FUNCTION(lchash_find);

ZEND_BEGIN_MODULE_GLOBALS(lchash)
	void *table;     /* khash_t(lchash) *, opaque outside lchash.c */
ZEND_END_MODULE_GLOBALS(lchash)

#if defined(ZTS) && defined(COMPILE_DL_LCHASH)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

#define LCHASH_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(lchash, v)

#endif	/* PHP_LCHASH_H */
