--TEST--
Duplicate-key insert preserves the first writer (matches glibc hsearch ENTER)
--EXTENSIONS--
lchash
--FILE--
<?php
lchash_create(8);
var_dump(lchash_insert('k', 'first'));
var_dump(lchash_insert('k', 'second'));
var_dump(lchash_find('k'));
lchash_destroy();
?>
--EXPECT--
bool(true)
bool(true)
string(5) "first"
