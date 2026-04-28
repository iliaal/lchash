# Changelog

All notable changes to lchash are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project
uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.0] - 2026-04-28

### Added
- PHP 7.4, 8.0, 8.1, 8.2, 8.3, 8.4, 8.5 support (NTS and ZTS).
- Vendored [klib khash](https://github.com/attractivechaos/klib) as the
  single hash-table backend (`khash.h`, header-only, MIT-licensed).
  Routes all allocations through Zend MM. Replaces both the legacy glibc
  `hsearch_r` path and the in-tree linear-probing fallback.
- New `LcHash` class with `$obj[$key]` dimension access (`read_dimension`,
  `write_dimension`, `has_dimension`, `unset_dimension`). Multiple tables
  per request, allocated lazily on first write, destroyed when the object
  is freed. Last-writer-wins semantics on assignment (matches PHP-array
  idiom; the procedural API keeps first-writer-wins for legacy compat).
- Generated arginfo (`lchash.stub.php` + `lchash_arginfo.h` /
  `lchash_legacy_arginfo.h`) so reflection sees real signatures, including
  the `LcHash::__construct` method.
- **PIE install support.** `composer.json` declares `type: php-ext`
  with the full `php-ext` block (configure options, ZTS/NTS support
  flags), so `pie install iliaal/lchash` works after the package is
  registered on Packagist.
- Modernized PECL `package.xml` (schema 2.1) keeps the legacy
  `pecl install lchash` flow working alongside PIE.
- Windows binaries on every release: `release-windows.yml` builds
  PHP 8.3/8.4/8.5 x x64/x86 x NTS/TS via `php-windows-builder` and
  uploads each `.dll` zip as a release asset.
- Hard cap of 1,048,576 (`1<<20`) on `n_entries` to prevent a single
  hostile `lchash_create()` from triggering a multi-GiB allocation.
- Test suite covering duplicate-key insert, fill-to-capacity,
  empty / NUL keys, oversize / negative `n_entries`, ZTS,
  binary-safe keys (`tests/070-oo-binary-safe-key.phpt`), and
  userland-forgot-destroy via the leak detector.
- `bench/bench.php` benchmark script comparing lchash to PHP arrays
  for honest performance reporting.

### Changed
- **Keys are now binary-safe.** Both the procedural API and the new OO
  API accept NUL bytes in keys with length-aware comparison. Two keys
  that differ only in NUL placement are distinct entries. Previously
  the procedural API rejected NUL-containing keys with an `E_WARNING`
  and returned `false`; that restriction has been lifted.
- Public functions now have proper return-type info: `lchash_create(int): bool`,
  `lchash_destroy(): bool`, `lchash_insert(string, string): bool`,
  `lchash_find(string): string|false`.
- Both APIs are safe under ZTS. The procedural table lives in
  per-thread module globals; OO tables live per object instance.
- Module entry uses `STANDARD_MODULE_HEADER` unconditionally and exposes
  the backend (`klib khash`) in `phpinfo()`.
- Insert and lookup wall-clock are roughly 100x faster than the previous
  glibc `hsearch_r`-backed implementation at 1M entries (release PHP 8.4
  NTS): proc-API insert dropped from ~37s to ~0.16s, lookup from ~18s to
  ~0.10s. Memory dropped from ~120MB to ~32MB at 1M.

### Fixed
- Calling `lchash_destroy()` no longer leaks per-entry key/value allocations.
- `lchash_insert` validates that keys are not empty.
- Request shutdown destroys the table if userland forgot to.
- Self-assignment `$lc[$key] = $lc[$key]` no longer risks
  use-after-free on the stored value (bump-then-release ordering in
  the OO write path).

### Removed
- Glibc `hsearch_r` backend and `HAVE_HSEARCH_R` build probe.
- In-tree linear-probing fallback with seeded FNV-1a (klib khash
  serves all platforms uniformly now).
- The "Backend" `phpinfo()` row's variable value: it's always `klib khash`.

## [0.9.1] - 2005-09-07

Original PECL release. Error handling fixes, binary-safe data storage.
PHP 4 / PHP 5 only.

[Unreleased]: https://github.com/iliaal/lchash/compare/1.0.0...HEAD
[1.0.0]: https://github.com/iliaal/lchash/releases/tag/1.0.0
