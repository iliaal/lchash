--TEST--
Missing key returns false
--EXTENSIONS--
lchash
--FILE--
<?php
lchash_create(16);
lchash_insert('present', 'yes');
var_dump(lchash_find('present'));
var_dump(lchash_find('absent'));
lchash_destroy();
?>
--EXPECT--
string(3) "yes"
bool(false)
