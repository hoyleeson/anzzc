#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])

AC_INIT(anzzc, 1.0.00, hoyleeson@gmail.com)

AM_INIT_AUTOMAKE

AC_CONFIG_SRCDIR([src/common.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB


# Checks for libraries.
AC_PROG_RANLIB
AC_PROG_LIBTOOL

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT64_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([clock_gettime gethostbyname gettimeofday inet_ntoa memset mkdir mkfifo strerror])

# Substitutions for .in files

AC_CHECK_FUNCS(getpagesizes)
AC_CHECK_FUNCS(memcntl)


AC_DEFINE(CONFIG_POOL_THREAD_COUNT, 16, "Max number of threads in the thread pool.")
AC_DEFINE(CONFIG_CONFIGS_FILE_NAME, "configs.conf", "System configs file name.")

AC_CHECK_LIB(pthread, pthread_create) 

# enable debug or not                                                          
AC_ARG_ENABLE(debug,
              AS_HELP_STRING([--enable-debug], [enable DEBUG mode(default=no)]), [],
              [enable_debug=no])
AS_IF([test "${enable_debug}" = "yes"], [
       CFLAGS="-g2 -O0 -DDEBUG -Wall"
       CXXFLAGS="-g2 -O0 -DDEBUG -Wall"
       ], 
	  [test "${enable_debug}" = "no"], [
       CFLAGS="-O2 -Wall"
       CXXFLAGS="-O2 -Wall"
       ], [])     

AC_ARG_ENABLE(verbose, 
              AS_HELP_STRING([--enable-verbose], [enable printf verbose log (default=no)]), [],
              [enable_verbose=no]) 
AS_IF([test "${enable_verbose}" != "no"], [
       CFLAGS="${CFLAGS} -DVDEBUG"
       CXXFLAGS="${CXXFLAGS} -DVDEBUG"
       ])

AC_ARG_WITH(platform, 
              AS_HELP_STRING([--with-platform=PLATFORM], [Specifies the platform(default=x86)]), [],
              [with_platform=x86]) 
AS_IF([test "${with_platform}" == "android"], [
       CFLAGS="${CFLAGS} -DANDROID"
       CXXFLAGS="${CXXFLAGS} -DANDROID"
       COMMON_ADD_LIBS([common],[-llog])
       ])



LIBPTHREAD=""
AC_SEARCH_LIBS(pthread_rwlock_init, pthread pthreads c_r, [
  AS_IF([test "$ac_cv_search_pthread_rwlock_init" != "none required"], [
    LIBPTHREAD="$ac_cv_search_pthread_rwlock_init"
  ])
])
AC_SUBST(LIBPTHREAD)

#LIBRT=""
#AC_CHECK_LIB(rt, clock_gettime, [LIBRT="-lrt"])
#AC_SUBST(LIBRT)

GLOBAL_CFLAGS= -Wl,-rpath
AC_SUBST(GLOBAL_CFLAGS)


AC_CONFIG_FILES([Makefile
				 src/Makefile
				 include/Makefile
				 configs/Makefile
				 tests/Makefile tests/test_case/Makefile
				 docs/Makefile])
AC_OUTPUT
