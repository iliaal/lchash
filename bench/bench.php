<?php
/**
 * lchash vs PHP-array micro-benchmark.
 *
 * Run inside a release-build PHP for representative numbers; debug builds
 * inflate Zend MM allocator overhead and skew the comparison.
 *
 * Reports:
 *   - Insert wall-clock time
 *   - Lookup wall-clock time (full scan over all keys)
 *   - Zend MM bytes (memory_get_usage(false), excludes libc malloc)
 *   - Process RSS delta (captures libc malloc that hsearch_r owns)
 */

ini_set('memory_limit', '4096M');

if (!extension_loaded('lchash')) {
    fwrite(STDERR, "lchash not loaded; pass -d extension=...lchash.so\n");
    exit(1);
}

$N = (int)($argv[1] ?? 100_000);
echo "lchash vs PHP-array bench: N=$N entries\n";
echo "Backend: " . (function_exists('lchash_create') ? 'lchash present' : 'missing') . "\n";

// Pre-generate keys/values so allocation cost isn't on the timed path.
$keys = [];
$values = [];
mt_srand(42);
for ($i = 0; $i < $N; $i++) {
    $keys[$i] = sprintf('k%010d_%s', $i, bin2hex(random_bytes(4)));
    $values[$i] = bin2hex(random_bytes(16));
}

function rss_kib(): int {
    foreach (file('/proc/self/status') ?: [] as $line) {
        if (str_starts_with($line, 'VmRSS:')) {
            return (int)preg_replace('/\D+/', '', $line);
        }
    }
    return 0;
}

function bench_php_array(array $keys, array $values): array {
    $rss0 = rss_kib();
    $mem0 = memory_get_usage();
    $t0 = microtime(true);
    $arr = [];
    $n = count($keys);
    for ($i = 0; $i < $n; $i++) {
        $arr[$keys[$i]] = $values[$i];
    }
    $insert = microtime(true) - $t0;
    $mem1 = memory_get_usage();
    $rss1 = rss_kib();

    $t0 = microtime(true);
    $hits = 0;
    for ($i = 0; $i < $n; $i++) {
        if (isset($arr[$keys[$i]])) $hits++;
    }
    $lookup = microtime(true) - $t0;
    if ($hits !== $n) {
        fwrite(STDERR, "PHP-array hit mismatch: $hits / $n\n");
    }

    return [
        'insert_s' => $insert,
        'lookup_s' => $lookup,
        'zend_mb'  => ($mem1 - $mem0) / 1024 / 1024,
        'rss_mb'   => ($rss1 - $rss0) / 1024,
    ];
}

function bench_lchash(array $keys, array $values): array {
    $n = count($keys);
    $rss0 = rss_kib();
    $mem0 = memory_get_usage();
    $t0 = microtime(true);
    lchash_create($n);
    for ($i = 0; $i < $n; $i++) {
        lchash_insert($keys[$i], $values[$i]);
    }
    $insert = microtime(true) - $t0;
    $mem1 = memory_get_usage();
    $rss1 = rss_kib();

    $t0 = microtime(true);
    $hits = 0;
    for ($i = 0; $i < $n; $i++) {
        if (lchash_find($keys[$i]) !== false) $hits++;
    }
    $lookup = microtime(true) - $t0;
    if ($hits !== $n) {
        fwrite(STDERR, "lchash hit mismatch: $hits / $n\n");
    }

    lchash_destroy();
    return [
        'insert_s' => $insert,
        'lookup_s' => $lookup,
        'zend_mb'  => ($mem1 - $mem0) / 1024 / 1024,
        'rss_mb'   => ($rss1 - $rss0) / 1024,
    ];
}

// Warm caches with a small unrelated allocation so the first bench
// doesn't pay for arena setup.
$_ = array_fill(0, 1024, 'x'); unset($_);

$php = bench_php_array($keys, $values);
$lc  = bench_lchash($keys, $values);

printf("\n%-12s %12s %12s %12s %12s\n",
    'backend', 'insert (s)', 'lookup (s)', 'Zend MM MB', 'RSS Δ MB');
printf("%s\n", str_repeat('-', 64));
printf("%-12s %12.3f %12.3f %12.2f %12.2f\n",
    'PHP array', $php['insert_s'], $php['lookup_s'], $php['zend_mb'], $php['rss_mb']);
printf("%-12s %12.3f %12.3f %12.2f %12.2f\n",
    'lchash', $lc['insert_s'], $lc['lookup_s'], $lc['zend_mb'], $lc['rss_mb']);
printf("%s\n", str_repeat('-', 64));
printf("%-12s %12.2fx %11.2fx %11.2fx %11.2fx\n", 'ratio l/p',
    $lc['insert_s'] / $php['insert_s'],
    $lc['lookup_s'] / $php['lookup_s'],
    $php['zend_mb'] != 0 ? $lc['zend_mb'] / $php['zend_mb'] : 0,
    $php['rss_mb'] != 0 ? $lc['rss_mb'] / $php['rss_mb'] : 0);
