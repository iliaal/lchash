/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2005 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.0 of the PHP license,       |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_0.txt.                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Ilia Alshanetsky <iliaa@php.net>                             |
  +----------------------------------------------------------------------+
*/

/* $Id: lchash.c,v 1.5 2005/09/07 15:52:00 iliaa Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <search.h>
#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_lchash.h"

#define LCHASH_VERSION "0.9.1"

ZEND_DECLARE_MODULE_GLOBALS(lchash)


/* {{{ lchash_functions[]
 */
function_entry lchash_functions[] = {
	PHP_FE(lchash_create,	NULL)
	PHP_FE(lchash_destroy,	NULL)
	PHP_FE(lchash_insert,	NULL)
	PHP_FE(lchash_find,	NULL)

	{NULL, NULL, NULL}	/* Must be the last line in lchash_functions[] */
};
/* }}} */

/* {{{ lchash_module_entry
 */
zend_module_entry lchash_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"lchash",
	lchash_functions,
	NULL,
	NULL,
	PHP_RINIT(lchash),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(lchash),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(lchash),
#if ZEND_MODULE_API_NO >= 20010901
	LCHASH_VERSION, /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_EXTNAME
ZEND_GET_MODULE(lchash)
#endif

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(lchash)
{
	LCHASH_G(is_init) = 0;

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(lchash)
{
	if (LCHASH_G(is_init)) {
		hdestroy();
		LCHASH_G(is_init) = 0;
	}
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(lchash)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "LibC Hash support", "enabled");
	php_info_print_table_header(2, "Extension version", LCHASH_VERSION " - $id$");
	php_info_print_table_end();
}
/* }}} */

#define LCHASH_HT_CHECK	\
	if (!LCHASH_G(is_init)) {	\
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Hash table was not initialized");	\
		RETURN_FALSE;	\
	}	\

/* {{{ proto bool lchash_create(int n_entries)
 * Indicate how many entries should the hash table support
 */
PHP_FUNCTION(lchash_create)
{
	long n_entries;

	if (LCHASH_G(is_init)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Hash table already exists.");
		RETURN_FALSE;
	}

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &n_entries) == FAILURE) {
		RETURN_FALSE;
	}

	if (hcreate(n_entries)) {
		LCHASH_G(is_init) = 1;
		RETURN_TRUE;
	}
	php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s (errno %d)", strerror(errno), errno);
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto bool lchash_destroy()
 * Destroy hash table
 */
PHP_FUNCTION(lchash_destroy)
{
	if (ZEND_NUM_ARGS()) {
		WRONG_PARAM_COUNT;
	}

	LCHASH_HT_CHECK;

	hdestroy();
	LCHASH_G(is_init) = 0;

	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool lchash_insert(string key, string value)
 * Insert a value into a hash table based on a given key
 */
PHP_FUNCTION(lchash_insert)
{
	char *key, *val;
	int key_len, val_len;
	ENTRY e;

	LCHASH_HT_CHECK

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s", &key, &key_len, &val, &val_len) == FAILURE) {
		RETURN_FALSE;
	}

	e.key = strdup(key);
	e.data = malloc(val_len + 1 + sizeof(int));
	memcpy(e.data, &val_len, sizeof(int));
	memcpy(e.data+sizeof(int), val, val_len + 1); /* copy terminating \0 */

	if (hsearch(e, ENTER)) {
		RETURN_TRUE;
	}
	php_error_docref(NULL TSRMLS_CC, E_WARNING, "%s (errno %d)", strerror(errno), errno);
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto string lchash_find(string key)
 * Find a value inside a hash table based on given key, if nothing is found will return FALSE
 */
PHP_FUNCTION(lchash_find)
{
	char *key;
	int key_len;
	ENTRY *found, e;

	LCHASH_HT_CHECK;
	
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &key_len) == FAILURE) {
		RETURN_FALSE;
	}

	e.key = key;
	if ((found = hsearch(e, FIND)) != NULL) {
		int len;
		memcpy(&len, found->data, sizeof(int));
		RETURN_STRINGL((char *) found->data+sizeof(int), len, 1);
	}

	RETURN_FALSE;
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
