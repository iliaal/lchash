dnl $Id: config.m4,v 1.1 2005/08/31 14:25:51 iliaa Exp $
dnl config.m4 for extension lchash

PHP_ARG_ENABLE(lchash, for lchash support,
[  --enable-lchash           Enable LibC Hash support])

if test "$PHP_LCHASH" != "no"; then

  PHP_CHECK_FUNC(hcreate, hsearch, hdestroy)

  PHP_NEW_EXTENSION(lchash, lchash.c, $ext_shared)
fi
