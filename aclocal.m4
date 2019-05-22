dnl File:        aclocal.m4
dnl Created:     00/05/04
dnl Author:      Michel Hoche-Mong (hoche@grok.com)
dnl
dnl $Id: aclocal.m4,v 1.5 2001/07/04 00:36:45 hoche Exp $
dnl

dnl
dnl AC_PROMPT_USER_NO_CACHE(VARIABLE,PROMPT,[DEFAULT])
dnl
dnl
AC_DEFUN(AC_PROMPT_USER_NO_CACHE,
[
if test "x$defaults" = "xno"; then
echo $ac_n "$2 ($3): $ac_c"
read tmpinput
if test "$tmpinput" = "" -a "$3" != ""; then
  tmpinput="$3"
fi
eval $1=\"$tmpinput\"
else
tmpinput="$3"
eval $1=\"$tmpinput\"
fi
]) dnl done AC_PROMPT_USER


dnl
dnl AC_PROMPT_USER(VARIABLE,PROMPT,[DEFAULT])
dnl
dnl
AC_DEFUN(AC_PROMPT_USER,
[
MSG_CHECK=`echo "$2" | tail -1`
AC_CACHE_CHECK($MSG_CHECK, ac_cv_user_prompt_$1,
[echo ""
AC_PROMPT_USER_NO_CACHE($1,[$2],$3)
eval ac_cv_user_prompt_$1=\$$1
echo $ac_n "setting $MSG_CHECK to...  $ac_c"
])
]) dnl done AC_PROMPT_USER_FOR_DEFINE


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


dnl ICB_AC_PATH_SSL_H finds the openssl headers and sets CPPFlags
dnl to include the directory with the headers.
dnl
AC_DEFUN(ICB_AC_PATH_SSL_H, [

  #
  # Ok, lets find the include directory.
  # (The alternative search directory is invoked by --with-ssl-include)
  #
  no_ssl=true
  AC_ARG_WITH(ssl_include, [  --with-ssl-include      directory containing openssl headers], 
		with_ssl_include=${withval})

  AC_MSG_CHECKING(for openssl headers)
  AC_CACHE_VAL(icb_ac_cv_ssl_h_dir,[

	# first check to see if --with-ssl-include was specified
	if test x"${with_ssl_include}" != x ; then
	  if test -f ${with_ssl_include}/openssl/ssl.h ; then
		icb_ac_cv_ssl_h_dir=`(cd ${with_ssl_include}; pwd)`
	  elif test -f ${with_ssl_include}/ssl.h ; then
			icb_ac_cv_ssl_h_dir=`(cd ${with_ssl_include}; pwd)`
	  else
		AC_MSG_ERROR([no headers found in ${with_ssl_include} directory])
	  fi
	fi

	# next check in a few common install locations
	#
	if test x"${prefix}" = xNONE ; then
		 ssl_prefix=.
	else
		 ssl_prefix=${prefix}
	fi

	if test x"${icb_ac_cv_ssl_h_dir}" = x ; then
	  for i in ${ssl_prefix}/include \
			  /usr/pkg/include \
			  /usr/local/ssl/include \
			  /usr/local/include \
			  /usr/include \
			  ; do
		if test -f $i/openssl/ssl.h ; then
		  icb_ac_cv_ssl_h_dir=`(cd $i; pwd)`
		  break
		fi
	  done
	fi
  ])

  dnl ok, we've either found it, or we're hosed.
  dnl
  SSL_H_DIR=
  if test x"${icb_ac_cv_ssl_h_dir}" = x ; then
	AC_MSG_ERROR([no openssl headers found])
  elif test x"${icb_ac_cv_ssl_h_dir}" = x"/usr/include" ; then
	no_ssl=""
	AC_MSG_RESULT([found in the normal location: ${icb_ac_cv_ssl_h_dir}])
  else
	no_ssl=""
	# this hack is cause the SSL_H_DIR won't print if there is a "-I" in it.
	SSL_H_DIR="-I${icb_ac_cv_ssl_h_dir}"
	AC_MSG_RESULT([found in ${icb_ac_cv_ssl_h_dir}])
  fi

  AC_PROVIDE([$0])
  AC_SUBST(SSL_H_DIR)
])



dnl ICB_AD_PATH_SSL_LIB finds the openssl libs and sets SSL_LIB.
dnl
AC_DEFUN(ICB_AC_PATH_SSL_LIB, [
  #
  # the alternative search directory is invoked by --with-ssl-lib
  #

  if test x"${no_ssl}" = x ; then
	# we reset no_ssl incase something fails here
	no_ssl=true
	AC_ARG_WITH(ssl_lib, [  --with-ssl-lib          directory containing the ssl library],
		   with_ssl_lib=${withval})

	AC_MSG_CHECKING([for openssl library])
	AC_CACHE_VAL(icb_ac_cv_ssl_lib,[

		# First check to see if --with-ssl-lib was specified.
		if test x"${with_ssl_lib}" != x ; then
		  if test -f "${with_ssl_lib}/libssl.so" ; then
			icb_ac_cv_ssl_lib="-L`(cd ${with_ssl_lib}; pwd)` -lssl -lcrypto"
		  elif test -f "${with_ssl_lib}/libssl.a" ; then
			icb_ac_cv_ssl_lib="`(cd ${with_ssl_lib}; pwd)`/libssl.a `(cd ${with_ssl_lib}; pwd)`/libcrypto.a"
		  else
			AC_MSG_ERROR([No libraries found in ${with_ssl_lib} directory])
		  fi
		fi

		# check in a few common install locations
		if test x"${prefix}" = xNONE ; then
             ssl_prefix=.
		else
			 ssl_prefix=${prefix}
        fi
		if test x"${icb_ac_cv_ssl_lib}" = x ; then
		  for i in ${ssl_prefix}/lib \
				  /usr/pkg/lib \
				  /usr/local/ssl/lib \
				  /usr/local/lib \
				  /usr/lib \
				  ; do
				if test -f "$i/libssl.so" ; then
				  icb_ac_cv_ssl_lib="-L`(cd $i; pwd)` -lssl -lcrypto"
				  break;
				elif test -f "$i/libssl.a" ; then
				  icb_ac_cv_ssl_lib="`(cd $i; pwd)`/libssl.a `(cd $i; pwd)`/libcrypto.a"
				  break;
				fi
			  if test x"${icb_ac_cv_ssl_lib}" != x ; then
				  break;
			  fi
		  done
		fi

	])

	#
	# ok, done searching. did we find it?
	if test x"${icb_ac_cv_ssl_lib}" = x ; then
	  SSL_LIB="# no openssl libraries found"
	  AC_MSG_WARN(no openssl libraries found)
	else
	  SSL_LIB=${icb_ac_cv_ssl_lib}
	  AC_MSG_RESULT(found $SSL_LIB)
	  no_ssl=
	fi
  fi

  AC_PROVIDE([$0])
  AC_SUBST(SSL_LIB)
])


dnl main entry point
AC_DEFUN(ICB_AC_PATH_SSL, [
  ICB_AC_PATH_SSL_H
  ICB_AC_PATH_SSL_LIB
])


