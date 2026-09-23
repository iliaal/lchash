# Changelog

All notable changes to lchash are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project
uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.0] - 2026-04-28

### Added
- PHP 7.4, 8.0, 8.1, 8.2, 8.3, 8.4, 8.5 support (NTS and ZTS).
- Vendored [klib khash](https://github.com/attractivechaos/klib) as the
  only backend (`khash.h`, header-only, MIT-licensed), with all
  allocations going through Zend MM. It replaces both the glibc
  `hsearch_r` path and the in-tree linear-probing fallback.
- `LcHash` class with `$obj[$key]` read, write, `isset()`, and `unset()`,
  so you can use several tables per request. Each table is allocated on
  first write and freed with the object. Assignment overwrites like a PHP
  array; the procedural API keeps first-writer-wins.
- Generated arginfo (`lchash.stub.php`, `lchash_arginfo.h`,
  `lchash_legacy_arginfo.h`), so reflection shows real signatures,
  including `LcHash::__construct`.
- PIE install support: `composer.json` declares `type: php-ext` with a
  `php-ext` block, so `pie install iliaal/lchash` works once the package
  is on Packagist.
- PECL `package.xml` updated to schema 2.1, so `pecl install lchash`
  still works.
- Windows binaries on every release: `release-windows.yml` builds
  PHP 8.3/8.4/8.5 x x64/x86 x NTS/TS via `php-windows-builder` and
  uploads each `.dll` zip as a release asset.
- Hard cap of 1,048,576 (`1<<20`) on `n_entries`, so a single
  `lchash_create()` call can't trigger a multi-GiB allocation.
- Test suite covering duplicate-key insert, fill-to-capacity,
  empty / NUL keys, oversize / negative `n_entries`, ZTS,
  binary-safe keys (`tests/070-oo-binary-safe-key.phpt`), and
  userland-forgot-destroy via the leak detector.
- `bench/bench.php`, which compares lchash to PHP arrays.

### Changed
- Keys are binary-safe in both APIs, compared by length, so keys that
  differ only in NUL placement are distinct. The procedural API used to
  reject keys containing NUL with an `E_WARNING` and `false`.
- Public functions have return-type info: `lchash_create(int): bool`,
  `lchash_destroy(): bool`, `lchash_insert(string, string): bool`,
  `lchash_find(string): string|false`.
- Both APIs are ZTS-safe. The procedural table lives in per-thread
  module globals and OO tables live in each object.
- Module entry uses `STANDARD_MODULE_HEADER` unconditionally and exposes
  the backend (`klib khash`) in `phpinfo()`.
- At 1M entries (release PHP 8.4 NTS), insert and lookup are about 100x
  faster than with the glibc `hsearch_r` backend: procedural insert went
  from ~37s to ~0.16s and lookup from ~18s to ~0.10s. Memory went from
  ~120MB to ~32MB.

### Fixed
- Calling `lchash_destroy()` no longer leaks per-entry key/value allocations.
- `lchash_insert` validates that keys are not empty.
- Request shutdown destroys the table if userland forgot to.
- Self-assignment `$lc[$key] = $lc[$key]` no longer risks a
  use-after-free on the stored value.

### Removed
- Glibc `hsearch_r` backend and `HAVE_HSEARCH_R` build probe.
- In-tree linear-probing fallback with seeded FNV-1a; klib khash now
  serves every platform.
- Variable value in the "Backend" `phpinfo()` row; it's always `klib khash`.

## [0.9.1] - 2005-09-07

Original PECL release. Error handling fixes, binary-safe data storage.
PHP 4 / PHP 5 only.

[Unreleased]: https://github.com/iliaal/lchash/compare/1.0.0...HEAD
[1.0.0]: https://github.com/iliaal/lchash/releases/tag/1.0.0
