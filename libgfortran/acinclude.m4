m4_include(../config/acx.m4)
m4_include(../config/no-executables.m4)
m4_include(../config/math.m4)
m4_include(../config/ax_check_define.m4)

dnl Check that we have a working GNU Fortran compiler
AC_DEFUN([LIBGFOR_WORKING_GFORTRAN], [
AC_MSG_CHECKING([whether the GNU Fortran compiler is working])
AC_LANG_PUSH([Fortran])
AC_COMPILE_IFELSE([[
      program foo
      real, parameter :: bar = sin (12.34 / 2.5)
      end program foo]],
    [AC_MSG_RESULT([yes])],
    [AC_MSG_RESULT([no])
     AC_MSG_ERROR([GNU Fortran is not working; please report a bug in http://gcc.gnu.org/bugzilla, attaching $PWD/config.log])
    ])
AC_LANG_POP([Fortran])
])


sinclude(../libtool.m4)
dnl The lines below arrange for aclocal not to bring an installed
dnl libtool.m4 into aclocal.m4, while still arranging for automake to
dnl add a definition of LIBTOOL to Makefile.in.
ifelse(,,,[AC_SUBST(LIBTOOL)
AC_DEFUN([AM_PROG_LIBTOOL])
AC_DEFUN([AC_LIBTOOL_DLOPEN])
AC_DEFUN([AC_PROG_LD])
])

dnl Check whether the target supports hidden visibility.
AC_DEFUN([LIBGFOR_CHECK_ATTRIBUTE_VISIBILITY], [
  AC_CACHE_CHECK([whether the target supports hidden visibility],
		 libgfor_cv_have_attribute_visibility, [
  save_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS -Werror"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[void __attribute__((visibility("hidden"))) foo(void) { }]], [])],
		    libgfor_cv_have_attribute_visibility=yes,
		    libgfor_cv_have_attribute_visibility=no)
  CFLAGS="$save_CFLAGS"])
  if test $libgfor_cv_have_attribute_visibility = yes; then
    AC_DEFINE(HAVE_ATTRIBUTE_VISIBILITY, 1,
      [Define to 1 if the target supports __attribute__((visibility(...))).])
  fi])

dnl Check whether the target supports symbol aliases.
AC_DEFUN([LIBGFOR_CHECK_ATTRIBUTE_ALIAS], [
  AC_CACHE_CHECK([whether the target supports symbol aliases],
		 libgfor_cv_have_attribute_alias, [
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[
void foo(void) { }
extern void bar(void) __attribute__((alias("foo")));]],
    [[bar();]])], libgfor_cv_have_attribute_alias=yes, libgfor_cv_have_attribute_alias=no)])
  if test $libgfor_cv_have_attribute_alias = yes; then
    AC_DEFINE(HAVE_ATTRIBUTE_ALIAS, 1,
      [Define to 1 if the target supports __attribute__((alias(...))).])
  fi])

dnl Check whether the target supports __sync_fetch_and_add.
AC_DEFUN([LIBGFOR_CHECK_SYNC_FETCH_AND_ADD], [
  AC_CACHE_CHECK([whether the target supports __sync_fetch_and_add],
		 libgfor_cv_have_sync_fetch_and_add, [
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[int foovar = 0;]], [[
if (foovar <= 0) return __sync_fetch_and_add (&foovar, 1);
if (foovar > 10) return __sync_add_and_fetch (&foovar, -1);]])],
	      libgfor_cv_have_sync_fetch_and_add=yes, libgfor_cv_have_sync_fetch_and_add=no)])
  if test $libgfor_cv_have_sync_fetch_and_add = yes; then
    AC_DEFINE(HAVE_SYNC_FETCH_AND_ADD, 1,
	      [Define to 1 if the target supports __sync_fetch_and_add])
  fi])

dnl Check for pragma weak.
AC_DEFUN([LIBGFOR_GTHREAD_WEAK], [
  AC_CACHE_CHECK([whether pragma weak works],
		 libgfor_cv_have_pragma_weak, [
  gfor_save_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS -Wunknown-pragmas"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
void foo (void);
#pragma weak foo
]], [[if (foo) foo ();]])],
		    libgfor_cv_have_pragma_weak=yes, libgfor_cv_have_pragma_weak=no)])
  if test $libgfor_cv_have_pragma_weak = yes; then
    AC_DEFINE(SUPPORTS_WEAK, 1,
	      [Define to 1 if the target supports #pragma weak])
  fi
  case "$host" in
    *-*-darwin* | *-*-hpux* | *-*-cygwin* | *-*-mingw* | *-*-musl* )
      AC_DEFINE(GTHREAD_USE_WEAK, 0,
		[Define to 0 if the target shouldn't use #pragma weak])
      ;;
  esac])

dnl Check whether target effectively supports weakref
AC_DEFUN([LIBGFOR_CHECK_WEAKREF], [
  AC_CACHE_CHECK([whether the target supports weakref],
		 libgfor_cv_have_weakref, [
  save_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS -Wunknown-pragmas -Werror"
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[
static int mytoto (int) __attribute__((__weakref__("toto")));
]], [[return (mytoto != 0);]])],
		 libgfor_cv_have_weakref=yes, libgfor_cv_have_weakref=no)
  CFLAGS="$save_CFLAGS"])
  if test $libgfor_cv_have_weakref = yes; then
    AC_DEFINE(SUPPORTS_WEAKREF, 1,
	      [Define to 1 if the target supports weakref])
  fi])

dnl Check whether target can unlink a file still open.
AC_DEFUN([LIBGFOR_CHECK_UNLINK_OPEN_FILE], [
  AC_CACHE_CHECK([whether the target can unlink an open file],
                  libgfor_cv_have_unlink_open_file, [
  AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

int main ()
{
  int fd;

  fd = open ("testfile", O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);
  if (fd <= 0)
    return 0;
  if (unlink ("testfile") == -1)
    return 1;
  write (fd, "This is a test\n", 15);
  close (fd);

  if (open ("testfile", O_RDONLY) == -1 && errno == ENOENT)
    return 0;
  else
    return 1;
}]])], libgfor_cv_have_unlink_open_file=yes, libgfor_cv_have_unlink_open_file=no, [
case "${target}" in
  *mingw*) libgfor_cv_have_unlink_open_file=no ;;
  *) libgfor_cv_have_unlink_open_file=yes;;
esac])])
if test x"$libgfor_cv_have_unlink_open_file" = xyes; then
  AC_DEFINE(HAVE_UNLINK_OPEN_FILE, 1, [Define if target can unlink open files.])
fi])

dnl Check whether CRLF is the line terminator
AC_DEFUN([LIBGFOR_CHECK_CRLF], [
  AC_CACHE_CHECK([whether the target has CRLF as line terminator],
                  libgfor_cv_have_crlf, [
  AC_RUN_IFELSE([AC_LANG_SOURCE([[
/* This test program should exit with status 0 if system uses a CRLF as
   line terminator, and status 1 otherwise.  
   Since it is used to check for mingw systems, and should return 0 in any
   other case, in case of a failure we will not use CRLF.  */
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>

int main ()
{
#ifndef O_BINARY
  exit(1);
#else
  int fd, bytes;
  char buff[5];

  fd = open ("foo", O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
  if (fd < 0)
    exit(1);
  if (write (fd, "\n", 1) < 0)
    perror ("write");
  
  close (fd);
  
  if ((fd = open ("foo", O_RDONLY | O_BINARY, S_IRWXU)) < 0)
    exit(1);
  bytes = read (fd, buff, 5);
  if (bytes == 2 && buff[0] == '\r' && buff[1] == '\n')
    exit(0);
  else
    exit(1);
#endif
}]])], libgfor_cv_have_crlf=yes, libgfor_cv_have_crlf=no, [
case "${target}" in
  *mingw*) libgfor_cv_have_crlf=yes ;;
  *) libgfor_cv_have_crlf=no;;
esac])])
if test x"$libgfor_cv_have_crlf" = xyes; then
  AC_DEFINE(HAVE_CRLF, 1, [Define if CRLF is line terminator.])
fi])

dnl Check whether the st_ino and st_dev stat fields taken together uniquely
dnl identify the file within the system. This is should be true for POSIX
dnl systems; it is known to be false on mingw32.
AC_DEFUN([LIBGFOR_CHECK_WORKING_STAT], [
  AC_CACHE_CHECK([whether the target stat is reliable],
                  libgfor_cv_have_working_stat, [
  AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int main ()
{ 
  FILE *f, *g;
  struct stat st1, st2;

  f = fopen ("foo", "w");
  g = fopen ("bar", "w");
  if (stat ("foo", &st1) != 0 || stat ("bar", &st2))
    return 1;
  if (st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino)
    return 1;
  fclose(f);
  fclose(g);
  return 0;
}]])], libgfor_cv_have_working_stat=yes, libgfor_cv_have_working_stat=no, [
case "${target}" in
  *mingw*) libgfor_cv_have_working_stat=no ;;
  *) libgfor_cv_have_working_stat=yes;;
esac])])
if test x"$libgfor_cv_have_working_stat" = xyes; then
  AC_DEFINE(HAVE_WORKING_STAT, 1, [Define if target has a reliable stat.])
fi])

dnl Checks for fpsetmask function.
AC_DEFUN([LIBGFOR_CHECK_FPSETMASK], [
  AC_CACHE_CHECK([whether fpsetmask is present], libgfor_cv_have_fpsetmask, [
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#if HAVE_FLOATINGPOINT_H
# include <floatingpoint.h>
#endif /* HAVE_FLOATINGPOINT_H */
#if HAVE_IEEEFP_H
# include <ieeefp.h>
#endif /* HAVE_IEEEFP_H */]],[[fpsetmask(0);]])],
    eval "libgfor_cv_have_fpsetmask=yes", eval "libgfor_cv_have_fpsetmask=no")
  ])
  if test x"$libgfor_cv_have_fpsetmask" = xyes; then
    have_fpsetmask=yes
    AC_DEFINE(HAVE_FPSETMASK, 1, [Define if you have fpsetmask.])
  fi
])

dnl Check whether we have a mingw that provides a __mingw_snprintf function
AC_DEFUN([LIBGFOR_CHECK_MINGW_SNPRINTF], [
  AC_CACHE_CHECK([whether __mingw_snprintf is present], libgfor_cv_have_mingw_snprintf, [
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <stdio.h>
extern int __mingw_snprintf (char *, size_t, const char *, ...);
]],[[
__mingw_snprintf (NULL, 0, "%d\n", 1);
]])],
    eval "libgfor_cv_have_mingw_snprintf=yes", eval "libgfor_cv_have_mingw_snprintf=no")
  ])
  if test x"$libgfor_cv_have_mingw_snprintf" = xyes; then
    AC_DEFINE(HAVE_MINGW_SNPRINTF, 1, [Define if you have __mingw_snprintf.])
  fi
])

dnl Check whether we have a __float128 type
AC_DEFUN([LIBGFOR_CHECK_FLOAT128], [
  LIBQUADSPEC=

  if test "x$enable_libquadmath_support" != xno; then

  AC_CACHE_CHECK([whether we have a usable __float128 type],
                 libgfor_cv_have_float128, [
   GCC_TRY_COMPILE_OR_LINK([
    typedef _Complex float __attribute__((mode(TC))) __complex128;

    __float128 foo (__float128 x)
    {

     __complex128 z1, z2;

     z1 = x;
     z2 = x / 7.Q;
     z2 /= z1;

     return (__float128) z2;
    }

    __float128 bar (__float128 x)
    {
      return x * __builtin_huge_valq ();
    }
  ],[
    foo (1.2Q);
    bar (1.2Q);
  ],[
    libgfor_cv_have_float128=yes
  ],[
    libgfor_cv_have_float128=no
])])

  if test "x$libgfor_cv_have_float128" = xyes; then
    AC_DEFINE(HAVE_FLOAT128, 1, [Define if have a usable __float128 type.])

      LIBQUADSPEC="-lquadmath"
      LIBQUADLIB="-lquadmath"
      LIBQUADLIB_DEP=
      LIBQUADINCLUDE=
  else
    # for --disable-quadmath
    LIBQUADLIB=
    LIBQUADLIB_DEP=
    LIBQUADINCLUDE=
  fi

  dnl For the spec file
  AC_SUBST(LIBQUADSPEC)
  AC_SUBST(LIBQUADLIB)
  AC_SUBST(LIBQUADLIB_DEP)
  AC_SUBST(LIBQUADINCLUDE)

  dnl We need a conditional for the Makefile
  AM_CONDITIONAL(LIBGFOR_BUILD_QUAD, [test "x$libgfor_cv_have_float128" = xyes])
])


dnl Check whether we have strerror_r
AC_DEFUN([LIBGFOR_CHECK_STRERROR_R], [
  dnl Check for three-argument POSIX version of strerror_r
  ac_save_CFLAGS="$CFLAGS"
  CFLAGS="-Wimplicit-function-declaration -Werror"
  AC_TRY_COMPILE([#define _GNU_SOURCE 1
	     	  #include <string.h>
		  #include <locale.h>],
		  [char s[128]; strerror_r(5, s, 128);],
		  AC_DEFINE(HAVE_STRERROR_R, 1,
		  [Define if strerror_r is available in <string.h>.]),)
  CFLAGS="$ac_save_CFLAGS"

  dnl Check for two-argument version of strerror_r (e.g. for VxWorks)
  ac_save_CFLAGS="$CFLAGS"
  CFLAGS="-Wimplicit-function-declaration -Werror"
  AC_TRY_COMPILE([#define _GNU_SOURCE 1
	     	  #include <string.h>
		  #include <locale.h>],
		  [char s[128]; strerror_r(5, s);],
		  AC_DEFINE(HAVE_STRERROR_R_2ARGS, 1,
		  [Define if strerror_r takes two arguments and is available in <string.h>.]),)
  CFLAGS="$ac_save_CFLAGS"
])

dnl Check for AVX

AC_DEFUN([LIBGFOR_CHECK_AVX], [
  ac_save_CFLAGS="$CFLAGS"
  CFLAGS="-O2 -mavx"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
  void _mm256_zeroall (void)
        {
           __builtin_ia32_vzeroall ();
        }]], [[]])],
	AC_DEFINE(HAVE_AVX, 1,
	[Define if AVX instructions can be compiled.]),
	[])
  CFLAGS="$ac_save_CFLAGS"
])

dnl Check for AVX2

AC_DEFUN([LIBGFOR_CHECK_AVX2], [
  ac_save_CFLAGS="$CFLAGS"
  CFLAGS="-O2 -mavx2"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
  typedef long long __v4di __attribute__ ((__vector_size__ (32)));
	__v4di
	mm256_is32_andnotsi256  (__v4di __X, __v4di __Y)
        {
	   return __builtin_ia32_andnotsi256 (__X, __Y);
        }]], [[]])],
	AC_DEFINE(HAVE_AVX2, 1,
	[Define if AVX2 instructions can be compiled.]),
	[])
  CFLAGS="$ac_save_CFLAGS"
])

dnl Check for AVX512f

AC_DEFUN([LIBGFOR_CHECK_AVX512F], [
  ac_save_CFLAGS="$CFLAGS"
  CFLAGS="-O2 -mavx512f"
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
	typedef double __m512d __attribute__ ((__vector_size__ (64)));
	__m512d _mm512_add (__m512d a)
	{
	  __m512d b = __builtin_ia32_addpd512_mask (a, a, a, 1, 4);
	  /* For -m64/-mx32 also verify that code will work even if
	     the target uses call saved zmm16+ and needs to emit
	     unwind info for them (e.g. on mingw).  See PR79127.  */
#ifdef __x86_64__
	  asm volatile ("" : : : "zmm16", "zmm17", "zmm18", "zmm19");
#endif
	  return b;
        }]], [[]])],
	AC_DEFINE(HAVE_AVX512F, 1,
	[Define if AVX512f instructions can be compiled.]),
	[])
  CFLAGS="$ac_save_CFLAGS"
])
