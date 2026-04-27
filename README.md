# lchash

A small PHP extension exposing a single per-request linear-probing hash
table. Wraps glibc `hsearch_r` where available and falls back to an
in-tree open-addressing implementation everywhere else (musl, macOS,
*BSD, Windows). Supports PHP 7.4 through 8.5, both NTS and ZTS.

## Install

### PIE (recommended on PHP 8.x)

    pie install iliaal/lchash

### PECL

    pecl install lchash

### From source

    phpize
    ./configure --enable-lchash
    make
    make install

## API

```php
lchash_create(int $n_entries): bool
lchash_destroy(): bool
lchash_insert(string $key, string $value): bool
lchash_find(string $key): string|false
```

One table per request. Calling `lchash_create()` twice without an
intervening `lchash_destroy()` returns `false` and emits a warning.
Keys must not be empty and must not contain NUL bytes. Values are
binary-safe.

The table is destroyed automatically at request shutdown if userland
forgets to call `lchash_destroy()`.

## License

[PHP License 3.01](LICENSE).
