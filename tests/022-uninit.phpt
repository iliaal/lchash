--TEST--
Operations before lchash_create() warn and return false
--EXTENSIONS--
lchash
--FILE--
<?php
var_dump(lchash_insert('k', 'v'));
var_dump(lchash_find('k'));
var_dump(lchash_destroy());
?>
--EXPECTF--
Warning: lchash_insert(): Hash table was not initialized in %s on line %d
bool(false)

Warning: lchash_find(): Hash table was not initialized in %s on line %d
bool(false)

Warning: lchash_destroy(): Hash table was not initialized in %s on line %d
bool(false)
