
AM_CFLAGS = -I$(top_srcdir)

noinst_PROGRAMS = test_case
test_case_SOURCES = main.c test_case.h test_common.c
test_case_LDADD = $(top_srcdir)/src/.libs/libanzzc.a  $(LIBS_common) $(LIBPTHREAD)

