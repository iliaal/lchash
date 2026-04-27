/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: 11227c0a-oo1 */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_lchash_create, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, n_entries, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_lchash_destroy, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_lchash_insert, 0, 2, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, value, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_lchash_find, 0, 1, MAY_BE_STRING|MAY_BE_FALSE)
	ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_LcHash___construct, 0, 0, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, n_entries, IS_LONG, 0, "1048576")
ZEND_END_ARG_INFO()

ZEND_FUNCTION(lchash_create);
ZEND_FUNCTION(lchash_destroy);
ZEND_FUNCTION(lchash_insert);
ZEND_FUNCTION(lchash_find);
ZEND_METHOD(LcHash, __construct);

static const zend_function_entry ext_functions[] = {
	ZEND_FE(lchash_create, arginfo_lchash_create)
	ZEND_FE(lchash_destroy, arginfo_lchash_destroy)
	ZEND_FE(lchash_insert, arginfo_lchash_insert)
	ZEND_FE(lchash_find, arginfo_lchash_find)
	ZEND_FE_END
};

static const zend_function_entry class_LcHash_methods[] = {
	ZEND_ME(LcHash, __construct, arginfo_class_LcHash___construct, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};

static zend_class_entry *register_class_LcHash(void)
{
	zend_class_entry ce, *class_entry;

	INIT_CLASS_ENTRY(ce, "LcHash", class_LcHash_methods);
	class_entry = zend_register_internal_class_ex(&ce, NULL);
	class_entry->ce_flags |= ZEND_ACC_FINAL;

	return class_entry;
}
