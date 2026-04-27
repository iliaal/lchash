--TEST--
Empty key and NUL-containing key are rejected on insert
--EXTENSIONS--
lchash
--FILE--
<?php
lchash_create(8);
var_dump(lchash_insert('', 'value'));
var_dump(lchash_insert("a\0b", 'value'));
var_dump(lchash_insert('ok', 'value'));
var_dump(lchash_find('ok'));
lchash_destroy();
?>
--EXPECTF--
Warning: lchash_insert(): Key must not be empty in %s on line %d
bool(false)

Warning: lchash_insert(): Key must not contain NUL bytes in %s on line %d
bool(false)
bool(true)
string(5) "value"
