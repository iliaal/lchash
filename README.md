# lchash

[![Tests](https://github.com/iliaal/lchash/actions/workflows/tests.yml/badge.svg?branch=main)](https://github.com/iliaal/lchash/actions/workflows/tests.yml)
[![Version](https://img.shields.io/github/v/release/iliaal/lchash)](https://github.com/iliaal/lchash/releases)
[![License: PHP-3.01](https://img.shields.io/badge/License-PHP--3.01-green.svg)](http://www.php.net/license/3_01.txt)
[![Follow @iliaa](https://img.shields.io/badge/Follow-@iliaa-000000?style=flat&logo=x&logoColor=white)](https://x.com/intent/follow?screen_name=iliaa)

A small PHP extension that provides a string-keyed hash table backed by
[klib khash](https://github.com/attractivechaos/klib). It has two APIs:
four procedural functions for a single per-request table (the original
2005 API), and an `LcHash` class with `$obj[$key]` access for any number
of per-instance tables.

Supports PHP 7.4 through 8.5, NTS and ZTS, on glibc Linux, musl, macOS,
*BSD, and Windows.

lchash first shipped on PECL in 2005. The 1.0.0 release rewrites it for
PHP 7.4 and later.

## When to use this (and when not to)

Don't use it for speed. PHP arrays are faster on insert and lookup at
every size we measured. These numbers come from `bench/bench.php` on a
release build of PHP 8.4 NTS (glibc Linux x86_64, -O2):

| N entries  | Insert (array) | Insert (lchash proc) | Insert (lchash OO) | Lookup (array) | Lookup (lchash proc) | Lookup (lchash OO) | Mem (array) | Mem (lchash) |
|-----------:|---------------:|---------------------:|-------------------:|---------------:|---------------------:|-------------------:|------------:|-------------:|
|     10,000 |         0.001s |               0.001s |             0.000s |         0.000s |               0.001s |             0.000s |     0.63 MB |      0.25 MB |
|    100,000 |         0.007s |               0.014s |             0.009s |         0.002s |               0.009s |             0.005s |     5.00 MB |      2.03 MB |
|  1,000,000 |         0.111s |               0.161s |             0.185s |         0.052s |               0.102s |             0.101s |    40.0 MB  |     32.5 MB  |

To reproduce, run `php -d extension=$(pwd)/modules/lchash.so bench/bench.php <N>`.

At 1M entries, PHP arrays are 1.4x to 1.7x faster on insert and 2x
faster on lookup. PHP's HashTable uses a packed bucket layout with
inlined zvals and opcode-level array-access specialization, none of
which an extension can use.

lchash uses less memory than PHP arrays at every size: about 40% of
the array's memory at 10k entries and about 80% at 1M. Keys and values
are refcount-shared zend_strings with no per-entry Bucket overhead.

Reasons to use lchash:

- Memory-tight workloads. A long-running CLI worker that holds hundreds
  of thousands of small string mappings uses less RAM than with arrays.
- Porting C code. If you're migrating a C codebase that uses POSIX
  `hsearch_r`, the procedural API's first-writer-wins semantics match
  glibc `hsearch(ENTER)`.
- Legacy compatibility. The four function names are unchanged from the
  2005 PECL release, for codebases that depend on them.
- Learning. It's a small PECL extension with one vendored header, easy
  to read if you're learning PHP extension development.

For most code, use a PHP array.

## Install

### PIE (recommended on PHP 8.x)

[PIE](https://github.com/php/pie) is the PHP Foundation's successor to
PECL. It installs from Packagist and builds against the active
`php-config`.

```sh
pie install iliaal/lchash
```

Then add `extension=lchash` to your `php.ini`.

### PECL

The package is still on the PECL channel:

```sh
pecl install lchash
```

### From source

```sh
phpize
./configure --enable-lchash
make
make install
```

### Windows

Every [release](https://github.com/iliaal/lchash/releases) has
pre-built `.dll` zips for PHP 8.3 / 8.4 / 8.5 × x64 / x86 × NTS / TS.
Download the matching zip, extract `php_lchash.dll` into your `ext/`
directory, and add `extension=lchash` to `php.ini`.

## API

### Procedural (single per-request table)

```php
lchash_create(int $n_entries): bool
lchash_destroy(): bool
lchash_insert(string $key, string $value): bool
lchash_find(string $key): string|false
```

### Object-oriented (multiple per-instance tables)

```php
$lc = new LcHash(int $n_entries = 1048576);
$lc[$key] = $value;       // write_dimension, last writer wins
$value = $lc[$key];       // read_dimension, returns null on miss
isset($lc[$key]);         // has_dimension
unset($lc[$key]);         // unset_dimension
```

### Semantics

Both APIs:

- `n_entries` is capped at 1,048,576 (`1<<20`).
- Keys and values may contain any bytes, including NUL. Comparison is
  length-aware.
- Keys must be non-empty.

Procedural API:

- One table per request. Calling `lchash_create()` twice without
  `lchash_destroy()` in between emits a warning and returns `false`.
  If you don't call `lchash_destroy()`, the table is freed at request
  shutdown.
- First writer wins. Inserting an existing key returns `true` and keeps
  the old value, like glibc `hsearch(ENTER)`.
- Errors emit `E_WARNING` and return `false`, for compatibility with
  the 2005 API.

OO API:

- Each `LcHash` instance has its own table, allocated on first write and
  freed with the object.
- Last writer wins. `$lc[$key] = $value` overwrites an existing key, as
  with PHP arrays.
- Errors throw `Error` (capacity exceeded, empty key, and so on).

## Backend

lchash uses one backend on every platform: a vendored copy of
[klib khash](https://github.com/attractivechaos/klib) (header-only,
MIT-licensed) in `khash.h`. It has no external dependencies and no
build-time probes.

Both APIs hash keys with PHP's DJBX33A (`zend_string_hash_val`), so
collision-DoS exposure is the same as for PHP arrays. klib's
open-addressing layout degrades slightly more gracefully than chained
buckets under heavy collision.

## License

[PHP License 3.01](LICENSE) for the extension, MIT for the vendored
`khash.h` (header carries the full notice).
