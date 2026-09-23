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
 * Insert a string value under a key. Keys are binary-safe: embedded NUL
 * bytes are preserved and comparison is length-aware. If the key already
 * exists, the existing value is preserved (matches glibc hsearch ENTER
 * semantics).
 */
function lchash_insert(string $key, string $value): bool {}

/** Look up a key. Returns the stored string, or false if not found. */
function lchash_find(string $key): string|false {}

/**
 * Object-oriented hash table with $obj[$key] dimension access.
 *
 * Assignment overwrites an existing key, as with PHP arrays. The
 * procedural API keeps first-writer-wins semantics.
 */
final class LcHash
{
    public function __construct(int $n_entries = 1048576) {}
}
