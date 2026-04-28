# lchash

[![Tests](https://github.com/iliaal/lchash/actions/workflows/tests.yml/badge.svg?branch=main)](https://github.com/iliaal/lchash/actions/workflows/tests.yml)
[![Version](https://img.shields.io/github/v/release/iliaal/lchash)](https://github.com/iliaal/lchash/releases)
[![License: PHP-3.01](https://img.shields.io/badge/License-PHP--3.01-green.svg)](http://www.php.net/license/3_01.txt)
[![Follow @iliaa](https://img.shields.io/badge/Follow-@iliaa-000000?style=flat&logo=x&logoColor=white)](https://x.com/intent/follow?screen_name=iliaa)

A small PHP extension exposing a string-keyed hash table backed by
[klib khash](https://github.com/attractivechaos/klib). Two surface APIs:
four procedural functions for a single per-request table (the original
2005 API), plus an `LcHash` class with `$obj[$key]` dimension access for
multiple per-instance tables.

Supports PHP 7.4 through 8.5, both NTS and ZTS, on glibc Linux, musl,
macOS, *BSD, and Windows.

Originally released to PECL in 2005; this 1.0.0 line is a full
modernization for the PHP 7.4+ era.

## When to use this (and when not to)

**Don't use it for raw speed.** PHP arrays remain faster on insert and
lookup at every size we measured. The `bench/bench.php` script in this
repo, run on a release build of PHP 8.4 NTS (glibc Linux x86_64, -O2):

| N entries  | Insert (array) | Insert (lchash proc) | Insert (lchash OO) | Lookup (array) | Lookup (lchash proc) | Lookup (lchash OO) | Mem (array) | Mem (lchash) |
|-----------:|---------------:|---------------------:|-------------------:|---------------:|---------------------:|-------------------:|------------:|-------------:|
|     10,000 |         0.001s |               0.001s |             0.000s |         0.000s |               0.001s |             0.000s |     0.63 MB |      0.25 MB |
|    100,000 |         0.007s |               0.014s |             0.009s |         0.002s |               0.009s |             0.005s |     5.00 MB |      2.03 MB |
|  1,000,000 |         0.111s |               0.161s |             0.185s |         0.052s |               0.102s |             0.101s |    40.0 MB  |     32.5 MB  |

Reproducible via `php -d extension=$(pwd)/modules/lchash.so bench/bench.php <N>`.

Read it as: PHP arrays beat lchash by **1.4x to 1.7x on insert and
2x on lookup** at scale. The gap is structural: PHP's HashTable uses
a packed bucket layout with inlined zvals and opcode-level array-access
specialization that no extension can match.

The flip: **lchash uses less memory than PHP arrays at every size**
(0.4x-0.8x), because keys and values are stored as refcount-shared
zend_strings with no per-entry Bucket overhead. At 10k entries lchash
holds ~40% of the memory PHP arrays do; at 1M it's ~80%.

**Legitimate reasons to reach for lchash:**

- **Memory-tight workloads.** A long-running CLI worker holding
  hundreds of thousands of small string mappings will save real RAM
  vs. native arrays.
- **Porting C code.** If you have a C codebase using POSIX
  `hsearch_r` and want a near-1:1 PHP shim while migrating, the
  procedural API's "first writer wins" semantics line up exactly with
  glibc `hsearch(ENTER)`.
- **Legacy compatibility.** PECL has had this extension since 2005;
  some long-running codebases depend on the function names being
  stable. This release modernizes the implementation without
  changing the four-function surface.
- **Demonstration.** It's a small, focused, header-only-vendor-backed
  PECL extension that's a clean reading example for anyone learning
  PHP extension development.

For most code, use a PHP array.

## Install

### PIE (recommended on PHP 8.x)

[PIE](https://github.com/php/pie) is the PHP Foundation's PECL
successor. It installs from Packagist, builds against the active
`php-config`, and produces a loadable `.so` / `.dll`.

```sh
pie install iliaal/lchash
```

Then add `extension=lchash` to your `php.ini`.

### PECL

The package remains in the PECL channel for legacy installers:

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

Pre-built `.dll` zips are attached to every
[release](https://github.com/iliaal/lchash/releases), covering
PHP 8.3 / 8.4 / 8.5 × x64 / x86 × NTS / TS. Download the matching zip,
extract `php_lchash.dll` into your `ext/` directory, and add
`extension=lchash` to `php.ini`.

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

- **Procedural:** one table per request. Calling `lchash_create()`
  twice without an intervening `lchash_destroy()` returns `false`
  and emits a warning. The table is destroyed at request shutdown
  if userland forgets to call `lchash_destroy()`.
- **OO:** one table per `LcHash` instance, allocated lazily on first
  write. Destroyed when the object is freed.
- `n_entries` is capped at 1,048,576 (`1<<20`). Larger requests are
  rejected (warning + `false` for the procedural API, exception for
  the OO API).
- **Binary-safe.** Keys and values may contain arbitrary bytes
  including NUL. Comparison is length-aware, not strcmp-based.
- Keys must be non-empty.
- **Procedural:** first writer wins. Inserting a key that already
  exists returns `true` without overwriting (matches glibc
  `hsearch(ENTER)`).
- **OO:** last writer wins. `$lc[$key] = $value` overwrites if
  present (matches PHP-array idiom).
- **Procedural** errors are signalled via `E_WARNING` + `false`
  return, not exceptions, for compatibility with the 2005 API.
- **OO** errors throw `Error` (capacity exceeded, empty key, etc.).

## Backend

`lchash` ships a single backend: vendored
[klib khash](https://github.com/attractivechaos/klib) (header-only,
MIT-licensed, embedded in the extension as `khash.h`). No external
dependency, no build-time probe, identical behavior across all
supported platforms.

The hash function is PHP's own DJBX33A (`zend_string_hash_val`) for
both APIs, so attack surface against collision-DoS is identical to
PHP arrays themselves. klib's open-addressing layout degrades
slightly more gracefully than chained buckets under heavy collision.

## License

[PHP License 3.01](LICENSE) for the extension, MIT for the vendored
`khash.h` (header carries the full notice).
