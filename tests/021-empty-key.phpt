--TEST--
Empty key is rejected; NUL-containing keys are accepted (binary-safe)
--EXTENSIONS--
lchash
--FILE--
<?php
lchash_create(8);

// Empty key is rejected.
var_dump(lchash_insert('', 'value'));

// Keys with NUL bytes are accepted and compared by length.
var_dump(lchash_insert("a\0b", 'first'));
var_dump(lchash_insert("a\0c", 'second'));   // distinct from "a\0b"
var_dump(lchash_insert("a", 'third'));       // distinct from "a\0b"

var_dump(lchash_find("a\0b") === 'first');
var_dump(lchash_find("a\0c") === 'second');
var_dump(lchash_find("a") === 'third');

var_dump(lchash_insert('ok', 'value'));
var_dump(lchash_find('ok'));

lchash_destroy();
?>
--EXPECTF--
Warning: lchash_insert(): Key must not be empty in %s on line %d
bool(false)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
string(5) "value"
