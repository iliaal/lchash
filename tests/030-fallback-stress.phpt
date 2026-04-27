--TEST--
Insert/find at moderate scale exercises probing on the active backend
--EXTENSIONS--
lchash
--FILE--
<?php
$n = 500;
lchash_create($n * 2);

for ($i = 0; $i < $n; $i++) {
    lchash_insert("key-$i", "val-$i");
}

$miss = 0;
for ($i = 0; $i < $n; $i++) {
    if (lchash_find("key-$i") !== "val-$i") {
        $miss++;
    }
}
echo "miss=$miss\n";

var_dump(lchash_find('key-not-present'));

lchash_destroy();
?>
--EXPECT--
miss=0
bool(false)
