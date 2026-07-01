# Security policy

lchash is a pure-C PHP extension providing an in-process, string-keyed
hash table: a procedural single-table API and an OO `LcHash`
multi-table class backed by a vendored, header-only khash. Keys and
values come entirely from PHP. There is no network, file, or
deserialization input path, so the realistic threat surface is the C
data-structure code and ZPP argument parsing.

## Supported versions

| Version | Supported          |
|---------|--------------------|
| 1.0.x   | :white_check_mark: |

The two most recent minor versions receive security fixes.

## Reporting a vulnerability

**Do not file a public GitHub issue for security vulnerabilities.**

Use GitHub's private security advisory feature at
<https://github.com/iliaal/lchash/security/advisories/new>
or email Ilia Alshanetsky <ilia@ilia.ws> directly.

Please include:

- Affected lchash version (`php -r 'echo phpversion("lchash");'`)
- PHP version (`php -v`) and build (NTS or ZTS)
- A minimal reproducing case (PHP code that triggers it, small enough
  to inline in the report)
- Impact: crash / memory corruption / info disclosure / DoS / etc.
- Whether you've coordinated disclosure with anyone else

Acknowledgement within 7 days, fix or status update within 30. Once a
fix is released the advisory becomes public.

## Scope

In scope:

- Crashes, memory corruption, use-after-free, or reference-count bugs
  in `lchash.c` or the vendored `khash.h` reachable from PHP through
  the procedural API (`lchash_create()`, `lchash_destroy()`,
  `lchash_insert()`, `lchash_find()`) or the `LcHash` dimension
  handlers (`$h[$k]`, `$h[$k] = ...`, `isset()`, `unset()`).
- Binary-safe key handling: bugs triggered by keys containing NUL
  bytes, zero-length keys, or non-UTF-8 bytes.
- Allocation-size or integer-overflow bugs driven by the `n_entries`
  argument. The `1 << 20` cap on `n_entries` is a DoS boundary; a
  bypass that triggers an oversized allocation is in scope.
- Per-request and per-object table lifecycle bugs, including leaks or
  double-frees across request shutdown, and any ZTS-specific data race
  in table state.
- Arginfo / ZPP mismatches that cause undefined behavior reachable from
  PHP.

Out of scope:

- Memory growth from deliberately inserting many large keys or values
  within `memory_limit`. Allocations route through Zend MM and obey
  `memory_limit`; overflow paths that bypass it are in scope.
- Hash-collision timing as an abstract property. lchash is an
  in-process data structure for trusted, application-supplied keys, not
  a security primitive for adversarial key streams.
