--TEST--
n_entries=1 produces a usable single-entry table
--EXTENSIONS--
lchash
--FILE--
<?php
var_dump(lchash_create(1));
var_dump(lchash_insert('only', 'value'));
var_dump(lchash_find('only'));
lchash_destroy();
?>
--EXPECT--
bool(true)
bool(true)
string(5) "value"
