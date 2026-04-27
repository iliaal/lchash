# Changelog

All notable changes to lchash are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project
uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- PHP 7.4, 8.0, 8.1, 8.2, 8.3, 8.4, 8.5 support (NTS and ZTS).
- Portable in-tree open-addressing fallback for non-glibc systems
  (musl, macOS, *BSD, Windows). Selected automatically by `config.m4`.
- Generated arginfo (`lchash.stub.php` + `lchash_arginfo.h` /
  `lchash_legacy_arginfo.h`) so reflection sees real signatures.
- **PIE install support.** `composer.json` declares `type: php-ext`
  with the full `php-ext` block (configure options, ZTS/NTS support
  flags), so `pie install iliaal/lchash` works after the package is
  registered on Packagist.
- Modernized PECL `package.xml` (schema 2.1) keeps the legacy
  `pecl install lchash` flow working alongside PIE.
- Windows binaries on every release: `release-windows.yml` builds
  PHP 8.3/8.4/8.5 × x64/x86 × NTS/TS via `php-windows-builder` and
  uploads each `.dll` zip as a release asset.
- DoS-resistant fallback hashing: per-request seeded FNV-1a so an
  attacker can't precompute key sets that linear-probe-collide on
  non-glibc platforms.
- Hard cap of 1,048,576 (`1<<20`) on `n_entries` to prevent a single
  hostile `lchash_create()` from triggering a multi-GiB allocation.
- Test suite covering duplicate-key insert, fill-to-capacity,
  empty / NUL keys, oversize / negative `n_entries`, ZTS, both
  backends, and userland-forgot-destroy via the leak detector.
- `bench/bench.php` benchmark script comparing lchash to PHP arrays
  for honest performance reporting.

### Changed
- Public functions now have proper return-type info: `lchash_create(int): bool`,
  `lchash_destroy(): bool`, `lchash_insert(string, string): bool`,
  `lchash_find(string): string|false`.
- Switched from libc `hcreate`/`hsearch`/`hdestroy` (process-global, not
  thread-safe) to glibc `hsearch_r` with a per-request `struct hsearch_data`
  in module globals. This makes the extension safe under ZTS.
- Module entry uses `STANDARD_MODULE_HEADER` unconditionally and exposes the
  backend (`glibc hsearch_r` vs `in-tree linear probing`) in `phpinfo()`.

### Fixed
- Calling `lchash_destroy()` no longer leaks per-entry key/value allocations.
  The previous code relied on libc `hdestroy()` which only frees buckets, not
  the strings handed to `hsearch(ENTER)`.
- `lchash_insert` validates that keys do not contain NUL bytes and that they
  are not empty, instead of corrupting the table silently.
- Request shutdown destroys the table if userland forgot to.

## [0.9.1] - 2005-09-07

Original PECL release. Error handling fixes, binary-safe data storage.
PHP 4 / PHP 5 only.
