--TEST--
LcHash OO keys are binary-safe (NUL bytes preserved, distinguish entries)
--EXTENSIONS--
lchash
--FILE--
<?php
$lc = new LcHash(8);

// Keys that differ only in NUL placement must be distinct entries.
$k1 = "a\0b";
$k2 = "a\0c";
$k3 = "a";       // strlen=1, no NUL
$k4 = "ab";      // strlen=2, no NUL — distinct from $k1 despite "a\0b" sharing a prefix in C-string land

$lc[$k1] = "first";
$lc[$k2] = "second";
$lc[$k3] = "third";
$lc[$k4] = "fourth";

var_dump($lc[$k1] === "first");
var_dump($lc[$k2] === "second");
var_dump($lc[$k3] === "third");
var_dump($lc[$k4] === "fourth");

// strcmp would collapse k1 and k3 to the same key; the OO path must not.
var_dump($lc[$k1] !== $lc[$k3]);

// has/unset are also binary-safe
var_dump(isset($lc[$k1]));
unset($lc[$k1]);
var_dump(isset($lc[$k1]));
var_dump(isset($lc[$k2]));  // unrelated entry untouched

// Values stay binary-safe too (regression-guard the existing behavior).
$lc["bin_val"] = "x\0y\xff";
var_dump(strlen($lc["bin_val"]));

unset($lc);
echo "DONE\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
bool(true)
int(4)
DONE
