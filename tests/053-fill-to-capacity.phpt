--TEST--
Inserting past capacity reports a clean error, not a crash
--EXTENSIONS--
lchash
--FILE--
<?php
// Small table, fill until insert refuses. Both backends should refuse
// gracefully (warning + false) rather than overflow or corrupt state.
lchash_create(4);
$accepted = 0;
for ($i = 0; $i < 100; $i++) {
    if (@lchash_insert("k$i", "v$i")) {
        $accepted++;
    } else {
        break;
    }
}
echo "stopped after accepting $accepted\n";
echo "find first: ", lchash_find('k0'), "\n";
echo "find accepted-1: ", lchash_find('k' . ($accepted - 1)), "\n";
echo "find rejected: ";
var_dump(lchash_find("k$accepted"));
lchash_destroy();
?>
--EXPECTF--
stopped after accepting %d
find first: v0
find accepted-1: v%d
find rejected: bool(false)
