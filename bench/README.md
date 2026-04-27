# bench

A single benchmark script that compares `lchash` to PHP arrays on the
same key/value workload. Used to populate the performance table in the
project README; re-run after any change to the storage layout or hot
paths to see whether the gap to PHP arrays moved.

## Run

```sh
phpize && ./configure --enable-lchash && make -j$(nproc)
php -d extension=$(pwd)/modules/lchash.so bench/bench.php [N]
```

`N` defaults to 100,000. Output reports insert and lookup wall-clock
time, Zend MM bytes (`memory_get_usage(false)`), and process RSS delta
(`/proc/self/status`). RSS captures the libc-malloc'd `hsearch_data`
buckets that Zend MM doesn't see.

## Caveats

- Run on a **release build** of PHP. Debug builds add per-allocation
  tracking that inflates Zend MM numbers and obscures the comparison.
  This repo's `~/php-install-PHP-8.4-release` is the convention used
  for the README numbers.
- The benchmark inserts unique keys only, so duplicate-key handling
  isn't exercised on either side. Both implementations are O(1) for
  insert; the relevant path is what each one does per call.
- `/proc/self/status` is Linux-only. On macOS / Windows the RSS column
  will read 0; the Zend MM column still works.

## Verdict

PHP arrays win on every axis at every size we measure. See the README
for the actual numbers and what `lchash` is still useful for despite
that.
