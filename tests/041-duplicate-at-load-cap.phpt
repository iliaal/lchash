--TEST--
Duplicate insert at the load cap returns true, not "full"
--EXTENSIONS--
lchash
--FILE--
<?php
// Fill to n_entries, then re-insert an existing key. The duplicate
// must return true instead of failing a capacity check.
lchash_create(8);
for ($i = 0; $i < 8; $i++) {
    lchash_insert("key-$i", "v-$i");
}
var_dump(lchash_insert('key-0', 'overwrite-attempt'));
var_dump(lchash_find('key-0'));
lchash_destroy();
?>
--EXPECT--
bool(true)
string(3) "v-0"
