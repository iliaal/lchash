--TEST--
Double create without destroy warns and returns false
--EXTENSIONS--
lchash
--FILE--
<?php
var_dump(lchash_create(8));
var_dump(lchash_create(8));
lchash_destroy();
?>
--EXPECTF--
bool(true)

Warning: lchash_create(): Hash table already exists in %s on line %d
bool(false)
