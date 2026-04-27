<?php

/**
 * @generate-class-entries
 * @generate-function-entries
 * @generate-legacy-arginfo 70000
 */

/**
 * Allocate a new hash table sized for at least n_entries.
 * Only one table per request; calling twice without lchash_destroy() warns.
 */
function lchash_create(int $n_entries): bool {}

/** Free the table and all stored entries. */
function lchash_destroy(): bool {}

/**
 * Insert a string value under a NUL-free key. If the key already exists,
 * the existing value is preserved (matches glibc hsearch ENTER semantics).
 */
function lchash_insert(string $key, string $value): bool {}

/** Look up a key. Returns the stored string, or false if not found. */
function lchash_find(string $key): string|false {}

/**
 * Object-oriented hash table with $obj[$key] dimension access.
 *
 * Stores keys/values as refcounted zend_strings rather than estrdup'd
 * copies, and uses standard PHP-array overwrite semantics on assignment
 * (the procedural API's "first writer wins" is preserved separately).
 */
final class LcHash
{
    public function __construct(int $n_entries = 1048576) {}
}
