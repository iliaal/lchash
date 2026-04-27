dnl config.m4 for extension lchash

PHP_ARG_ENABLE([lchash],
  [for lchash support],
  [AS_HELP_STRING([--enable-lchash],
    [Enable libc-style linear-probing hash table support])],
  [no])

if test "$PHP_LCHASH" != "no"; then
  PHP_NEW_EXTENSION(lchash, lchash.c, $ext_shared)
fi
