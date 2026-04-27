--TEST--
Skipping lchash_destroy() does not leak (RSHUTDOWN cleans up)
--EXTENSIONS--
lchash
--FILE--
<?php
// Userland forgets to destroy. Run-tests.php's leak detector under
// debug builds will surface any uncleaned per-entry state at request
// shutdown. The test passes by running clean under -g LEAK,XLEAK.
lchash_create(64);
for ($i = 0; $i < 50; $i++) {
    lchash_insert("k$i", str_repeat('x', 32));
}
echo "exit without destroy\n";
?>
--EXPECT--
exit without destroy
