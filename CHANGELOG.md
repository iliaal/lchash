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
- PIE install path via `composer.json` `php-ext` block.
- Modernized PECL `package.xml` (schema 2.1) for `pecl install lchash`.

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
