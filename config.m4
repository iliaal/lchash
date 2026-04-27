dnl config.m4 for extension lchash

PHP_ARG_ENABLE([lchash],
  [for lchash support],
  [AS_HELP_STRING([--enable-lchash],
    [Enable libc-style linear-probing hash table support])],
  [no])

if test "$PHP_LCHASH" != "no"; then
  AC_CHECK_HEADERS([search.h])

  dnl hsearch_r is a glibc extension. We detect it explicitly so the
  dnl extension can fall back to its in-tree linear-probing table on
  dnl musl, macOS, *BSD, and Windows. The _GNU_SOURCE define is required
  dnl for the prototype to be visible on glibc systems.
  old_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS -D_GNU_SOURCE"
  AC_CHECK_FUNCS([hsearch_r hcreate_r hdestroy_r], [],
    [dnl one of the trio is missing -- disable the glibc path entirely
     ac_cv_func_hsearch_r=no])
  CPPFLAGS="$old_CPPFLAGS"

  if test "$ac_cv_func_hsearch_r" = "yes" \
      && test "$ac_cv_func_hcreate_r" = "yes" \
      && test "$ac_cv_func_hdestroy_r" = "yes"; then
    AC_DEFINE([HAVE_HSEARCH_R], [1],
      [Define to 1 if you have the glibc hsearch_r/hcreate_r/hdestroy_r trio.])
    AC_MSG_NOTICE([lchash: using glibc hsearch_r backend])
  else
    AC_MSG_NOTICE([lchash: glibc hsearch_r unavailable, using in-tree fallback])
  fi

  PHP_NEW_EXTENSION(lchash, lchash.c, $ext_shared,, -D_GNU_SOURCE)
fi
