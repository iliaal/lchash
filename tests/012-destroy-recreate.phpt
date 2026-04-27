--TEST--
Recreate after destroy works
--EXTENSIONS--
lchash
--FILE--
<?php
lchash_create(8);
lchash_insert('first', 'A');
var_dump(lchash_find('first'));
lchash_destroy();

lchash_create(8);
var_dump(lchash_find('first'));
lchash_insert('second', 'B');
var_dump(lchash_find('second'));
lchash_destroy();
?>
--EXPECT--
string(1) "A"
bool(false)
string(1) "B"
