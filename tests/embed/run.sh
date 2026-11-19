#!/bin/sh
# Builds tests/embed/worker-requests.c against a ZTS PHP built with
# --enable-embed and runs it with the given lchash.so loaded.
#
# usage: tests/embed/run.sh /path/to/php-config /path/to/lchash.so
# CC, CFLAGS, and LDFLAGS are honoured, e.g. to match a sanitizer build.
set -eu

if [ $# -ne 2 ]; then
	echo "usage: $0 php-config lchash.so" >&2
	exit 2
fi
php_config=$1
ext=$2
here=$(cd "$(dirname "$0")" && pwd)
prefix=$("$php_config" --prefix)
libdir="$prefix/lib"

if [ ! -e "$libdir/libphp.so" ]; then
	echo "$libdir/libphp.so not found; PHP must be built with --enable-embed" >&2
	exit 2
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT INT TERM

# CFLAGS, LDFLAGS, and --includes are word lists.
# shellcheck disable=SC2046,SC2086
${CC:-cc} ${CFLAGS:-} $("$php_config" --includes) \
	-o "$tmp/worker-requests" "$here/worker-requests.c" \
	-L"$libdir" -Wl,-rpath,"$libdir" ${LDFLAGS:-} -lphp -lpthread

mkdir "$tmp/ini"
printf 'extension=%s\n' "$ext" > "$tmp/ini/lchash.ini"
PHPRC=/dev/null PHP_INI_SCAN_DIR="$tmp/ini" "$tmp/worker-requests"
