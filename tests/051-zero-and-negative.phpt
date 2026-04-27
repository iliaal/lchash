--TEST--
n_entries of 0 or negative is rejected
--EXTENSIONS--
lchash
--FILE--
<?php
var_dump(lchash_create(0));
var_dump(lchash_create(-1));
var_dump(lchash_create(-9999));
?>
--EXPECTF--
Warning: lchash_create(): Number of entries must be positive in %s on line %d
bool(false)

Warning: lchash_create(): Number of entries must be positive in %s on line %d
bool(false)

Warning: lchash_create(): Number of entries must be positive in %s on line %d
bool(false)
