dnl File:        aclocal.m4
dnl Created:     00/05/04
dnl Author:      Michel Hoche-Mong (hoche@grok.com)
dnl

dnl
dnl figure out the FD_SETSIZE. sets ac_cv_icb_fd_setsize
dnl
AC_DEFUN(ICB_AC_FD_SETSIZE,
[
	AC_CACHE_CHECK(size of FD_SETSIZE, ac_cv_icb_fd_setsize, 
    [
		cat > conftest.c <<'EOF'
#include <sys/types.h>
Fd_SetSize = FD_SETSIZE
EOF

		fd_setsize=
		if (eval "$CPP $CPPFLAGS conftest.c") 2>/dev/null >conftest.out; then
			#Success
			fd_setsize=`egrep '^Fd_SetSize = ' conftest.out | sed -e 's/^Fd_SetSize = *//' -e 's/ *$//'`
		fi
		rm -f conftest.c conftest.out

		if test x"$fd_setsize" = x ; then
			echo "could not determine FD_SETSIZE. Using default..."
			ac_cv_icb_fd_setsize="256"
		else
			ac_cv_icb_fd_setsize="$fd_setsize"
		fi
    ])
])


