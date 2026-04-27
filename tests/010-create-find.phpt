--TEST--
Insert and find roundtrip
--EXTENSIONS--
lchash
--FILE--
<?php
var_dump(lchash_create(64));
var_dump(lchash_insert('alpha', 'one'));
var_dump(lchash_insert('beta', 'two'));
var_dump(lchash_insert('gamma', 'three'));
var_dump(lchash_find('alpha'));
var_dump(lchash_find('beta'));
var_dump(lchash_find('gamma'));
var_dump(lchash_destroy());
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
string(3) "one"
string(3) "two"
string(5) "three"
bool(true)
