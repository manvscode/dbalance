AC_PREREQ(2.67)
AC_INIT([dbalance], [0.0.1], [manvscode@gmail.com], [dbalance], [http://www.manvscode.com/])
AM_INIT_AUTOMAKE([1.13 foreign silent-rules dist-zip -Wall -Werror])

AM_SILENT_RULES([no])

AC_PROG_CC
#AC_PROG_CC_STDC
AC_C_INLINE
#AC_HEADER_STDC
#AC_HEADER_STDBOOL


AC_MSG_NOTICE([---------------------------------------------------------------------])
PKG_CHECK_MODULES([COLLECTIONS], [collections-static],
	[AC_MSG_NOTICE([Found libcollections])],
	[AC_MSG_ERROR([Missing libcollections])],
)
PKG_CHECK_MODULES([XTD], [xtd-static],
	[AC_MSG_NOTICE([Found libxtd])],
	[AC_MSG_ERROR([Missing libxtd])],
)
AC_CHECK_LIB([csv], [csv_init])
AC_MSG_NOTICE([---------------------------------------------------------------------])
if [test -z "$CFLAGS"]; then
	AC_MSG_NOTICE([ CFLAGS: Not set])
else
	AC_MSG_NOTICE([ CFLAGS: ${CFLAGS}])
fi
if [test -z "$LDFLAGS"]; then
	AC_MSG_NOTICE([LDFLAGS: Not set])
else
	AC_MSG_NOTICE([LDFLAGS: ${LDFLAGS}])
fi
AC_MSG_NOTICE([---------------------------------------------------------------------])

AC_CONFIG_FILES([
	Makefile
	src/Makefile
])

AC_OUTPUT
