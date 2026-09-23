--TEST--
Skipping lchash_destroy() does not leak (RSHUTDOWN cleans up)
--EXTENSIONS--
lchash
--FILE--
<?php
// On debug builds, run-tests.php reports a LEAK if RSHUTDOWN misses
// any entry.
lchash_create(64);
for ($i = 0; $i < 50; $i++) {
    lchash_insert("k$i", str_repeat('x', 32));
}
echo "exit without destroy\n";
?>
--EXPECT--
exit without destroy
