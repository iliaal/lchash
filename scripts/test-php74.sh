#!/usr/bin/env bash
# test-php74.sh -- build and test lchash inside a stock php:7.4-cli container.
#
# Catches breakage on the PHP 7.4 floor without waiting for GitHub Actions.
# Mounts the working tree read/write so the build artifacts land alongside
# any concurrent local 8.x build (each phpize wipes the previous one anyway).
#
# Requires Docker. No host-side PHP install needed.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

if ! command -v docker >/dev/null 2>&1; then
	echo "docker not found in PATH" >&2
	exit 1
fi

echo ">> php:7.4-cli build + tests"
docker run --rm \
	-v "$REPO_ROOT:/ext" \
	-w /ext \
	php:7.4-cli \
	bash -c '
		set -euo pipefail
		apt-get update -qq
		apt-get install -y -qq make gcc autoconf pkg-config >/dev/null
		phpize --clean >/dev/null 2>&1 || true
		phpize
		./configure --enable-lchash
		make clean >/dev/null 2>&1 || true
		make -j"$(nproc)"
		php --version
		php -d extension="$(pwd)/modules/lchash.so" --ri lchash
		NO_INTERACTION=1 \
		TEST_PHP_EXECUTABLE="$(which php)" \
		TEST_PHP_ARGS="-d extension=$(pwd)/modules/lchash.so" \
		php run-tests.php --show-diff -g FAIL,BORK,LEAK,XLEAK tests/
	'
