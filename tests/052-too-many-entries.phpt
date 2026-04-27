--TEST--
n_entries above LCHASH_MAX_ENTRIES (1<<20) is rejected
--EXTENSIONS--
lchash
--FILE--
<?php
// Just over the cap.
var_dump(lchash_create((1 << 20) + 1));
// PHP_INT_MAX on a 64-bit host -- the historic DoS reproducer.
var_dump(lchash_create(PHP_INT_MAX));
?>
--EXPECTF--
Warning: lchash_create(): Number of entries %d exceeds the cap of 1048576 in %s on line %d
bool(false)

Warning: lchash_create(): Number of entries %d exceeds the cap of 1048576 in %s on line %d
bool(false)
