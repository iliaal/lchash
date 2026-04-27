# lchash

[![Tests](https://github.com/iliaal/lchash/actions/workflows/tests.yml/badge.svg)](https://github.com/iliaal/lchash/actions/workflows/tests.yml)
[![Version](https://img.shields.io/github/v/release/iliaal/lchash)](https://github.com/iliaal/lchash/releases)
[![License: PHP-3.01](https://img.shields.io/badge/License-PHP--3.01-green.svg)](http://www.php.net/license/3_01.txt)
[![Follow @iliaa](https://img.shields.io/badge/Follow-@iliaa-000000?style=flat&logo=x&logoColor=white)](https://x.com/intent/follow?screen_name=iliaa)

A small PHP extension exposing a single per-request linear-probing hash
table. Wraps glibc `hsearch_r` where available and falls back to an
in-tree open-addressing implementation everywhere else (musl, macOS,
*BSD, Windows). Supports PHP 7.4 through 8.5, both NTS and ZTS.

Originally released to PECL in 2005; this 1.0.0 line is a full
modernization for the PHP 7.4+ era.

## When to use this (and when not to)

**Don't use it for performance.** PHP arrays are heavily optimized
hash tables that, in 20 years of evolution, have left POSIX
`hsearch_r` far behind. The `bench/bench.php` script in this repo
measures both on a release build of PHP 8.4:

| N entries | Insert (PHP array) | Insert (lchash) | Lookup (PHP array) | Lookup (lchash) | RSS (PHP array) | RSS (lchash) |
|----------:|-------------------:|----------------:|-------------------:|----------------:|----------------:|-------------:|
|    10,000 |             0.001s |          0.081s |             0.000s |          0.036s |         1.25 MB |      0.50 MB |
|   100,000 |             0.008s |          1.333s |             0.002s |          0.617s |         5.01 MB |     11.50 MB |
| 1,000,000 |             0.105s |         36.96s  |             0.053s |         17.93s  |        40.10 MB |    120.75 MB |

Numbers from a glibc Linux x86_64 host, PHP 8.4 NTS, `-O2` build.
Reproducible via `php -d extension=$(pwd)/modules/lchash.so bench/bench.php <N>`.

Read it as: PHP arrays beat lchash by **100x to 350x on speed** and
**2x-3x on memory at scale** (lchash only wins on absolute memory
under ~10k entries, and even then by a small margin). The gap widens
as the table grows, because PHP's HashTable is cache-friendly and
SIMD-optimized while glibc's `hsearch_r` is a 1980s POSIX API with
none of those properties.

**Legitimate reasons to reach for lchash anyway:**

- **Porting C code.** If you already have a C codebase that uses
  POSIX `hsearch_r` and you want a near-1:1 PHP shim while migrating,
  the semantics line up exactly. Duplicate-key "first writer wins"
  matches glibc `hsearch(ENTER)`.
- **Legacy compatibility.** PECL has had this extension since 2005;
  some long-running codebases depend on the function names being
  stable. This release modernizes the implementation without
  changing the four-function surface.
- **Demonstration.** It's a small, focused, two-backend PECL
  extension that's a clean reading example for anyone learning PHP
  extension development.

For everything else, use a PHP array.

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

## API

```php
lchash_create(int $n_entries): bool
lchash_destroy(): bool
lchash_insert(string $key, string $value): bool
lchash_find(string $key): string|false
```

### Semantics

- One table per request. Calling `lchash_create()` twice without an
  intervening `lchash_destroy()` returns `false` and emits a warning.
- `n_entries` is capped at 1,048,576 (`1<<20`). Larger requests are
  rejected with a warning.
- Keys must be non-empty and must not contain NUL bytes. Values are
  binary-safe (NULs and arbitrary bytes preserved).
- **First writer wins.** Inserting a key that already exists returns
  `true` without overwriting (matches glibc `hsearch(ENTER)`).
- The table is destroyed automatically at request shutdown if
  userland forgets to call `lchash_destroy()`.
- Errors are signalled via `E_WARNING` + `false` return, not
  exceptions, for compatibility with the 2005 API.

## Backends

`lchash` picks one of two backends at compile time:

| Platform                    | Backend                | Selected by             |
|-----------------------------|------------------------|-------------------------|
| glibc Linux                 | `hsearch_r` (libc)     | `HAVE_HSEARCH_R` define |
| musl, macOS, *BSD, Windows  | In-tree linear probing | fallback                |

Both are exercised by the test suite and CI. Check
`phpinfo()` → `lchash` → `Backend` to see which one the running
build picked.

The in-tree fallback uses seeded FNV-1a with a per-request seed from
the CSPRNG, so attacker-controlled keys can't precompute hash
collisions to mount linear-probe DoS. The glibc path inherits
whatever `hsearch_r` does internally.

## License

[PHP License 3.01](LICENSE).
