--TEST--
n_entries above LCHASH_MAX_ENTRIES (1<<20) is rejected
--EXTENSIONS--
lchash
--FILE--
<?php
var_dump(lchash_create((1 << 20) + 1));
// Original DoS reproducer on 64-bit hosts.
var_dump(lchash_create(PHP_INT_MAX));
?>
--EXPECTF--
Warning: lchash_create(): Number of entries %d exceeds the cap of 1048576 in %s on line %d
bool(false)

Warning: lchash_create(): Number of entries %d exceeds the cap of 1048576 in %s on line %d
bool(false)
