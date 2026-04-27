--TEST--
Values are binary-safe (NUL bytes and arbitrary bytes preserved)
--EXTENSIONS--
lchash
--FILE--
<?php
lchash_create(8);
$payload = "a\0b\0c\xff\xfeend";
var_dump(lchash_insert('bin', $payload));
$got = lchash_find('bin');
var_dump($got === $payload);
var_dump(strlen($got));
lchash_destroy();
?>
--EXPECT--
bool(true)
bool(true)
int(10)
