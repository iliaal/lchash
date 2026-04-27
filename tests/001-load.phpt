--TEST--
Extension loads and exposes its functions
--EXTENSIONS--
lchash
--FILE--
<?php
var_dump(extension_loaded('lchash'));
var_dump(function_exists('lchash_create'));
var_dump(function_exists('lchash_destroy'));
var_dump(function_exists('lchash_insert'));
var_dump(function_exists('lchash_find'));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
