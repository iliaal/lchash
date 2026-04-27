/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: 0b9bae1114307c9450a35691cd840397fb70d491 */

ZEND_BEGIN_ARG_INFO_EX(arginfo_lchash_create, 0, 0, 1)
	ZEND_ARG_INFO(0, n_entries)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_lchash_destroy, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_lchash_insert, 0, 0, 2)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_lchash_find, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_FUNCTION(lchash_create);
ZEND_FUNCTION(lchash_destroy);
ZEND_FUNCTION(lchash_insert);
ZEND_FUNCTION(lchash_find);

static const zend_function_entry ext_functions[] = {
	ZEND_FE(lchash_create, arginfo_lchash_create)
	ZEND_FE(lchash_destroy, arginfo_lchash_destroy)
	ZEND_FE(lchash_insert, arginfo_lchash_insert)
	ZEND_FE(lchash_find, arginfo_lchash_find)
	ZEND_FE_END
};
