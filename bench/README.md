# bench

`bench.php` compares `lchash` to PHP arrays on the same key/value
workload. Its output fills the performance table in the project README.
Re-run it after changing the storage layout or hot paths.

## Run

```sh
phpize && ./configure --enable-lchash && make -j$(nproc)
php -d extension=$(pwd)/modules/lchash.so bench/bench.php [N]
```

`N` defaults to 100,000. The output reports insert and lookup
wall-clock time, Zend MM bytes (`memory_get_usage(false)`), and process
RSS delta (`/proc/self/status`).

## Caveats

- Use a release build of PHP. Debug builds add per-allocation tracking
  that inflates the Zend MM numbers. The README numbers came from
  `~/php-install-PHP-8.4-release`.
- The benchmark inserts unique keys only, so it doesn't exercise
  duplicate-key handling on either side.
- `/proc/self/status` is Linux-only. On macOS and Windows the RSS column
  reads 0; the Zend MM column still works.

## Results

PHP arrays are faster on insert and lookup at every measured size, and
lchash uses less memory. See the README for the numbers and for when
lchash is still worth using.
