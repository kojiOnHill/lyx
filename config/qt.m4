dnl check a particular libname
AC_DEFUN([QT_TRY_LINK],
[
	SAVE_LIBS="$LIBS"
	LIBS="$LIBS $1"
	AC_LINK_IFELSE([AC_LANG_PROGRAM([[
	#include <qglobal.h>
	#include <qstring.h>
		]], [[
	QString s("mangle_failure");
	#if (QT_VERSION < 400)
	break_me_(\\\);
	#endif
	]])],[qt_cv_libname=$1],[])
	LIBS="$SAVE_LIBS"
])

dnl check we can do a compile
AC_DEFUN([QT_CHECK_COMPILE],
[
	AC_MSG_CHECKING([for Qt library name])

	AC_CACHE_VAL(qt_cv_libname,
	[
		SAVE_CXXFLAGS=$CXXFLAGS
		CXXFLAGS="$CXXFLAGS $QT_INCLUDES $QT_LDFLAGS"
		qt_corelibs="-lQtCore -lQtCore4"
		qt_guilibs="'-lQtCore -lQtGui' '-lQtCore4 -lQtGui4'"
		if test $USE_QT6 = "yes" ; then
		    qt_corelibs="-lQt6Core"
		    qt_guilibs="-lQt6Core -lQt6Concurrent -lQt6Gui -lQt6Svg -lQt6Widgets"
		else
		    qt_corelibs="-lQt5Core"
		    qt_guilibs="-lQt5Core -lQt5Concurrent -lQt5Gui -lQt5Svg -lQt5Widgets"
		fi
		for libname in $qt_corelibs '-framework QtCore'
		do
			QT_TRY_LINK($libname)
			if test -n "$qt_cv_libname"; then
				QT_CORE_LIB="$qt_cv_libname"
				break;
			fi
		done
		qt_cv_libname=
		for libname in $qt_guilibs \
		               '-framework QtCore -framework QtConcurrent -framework QtSvg -framework QtWidgets -framework QtMacExtras -framework QtGui'\
		               '-framework QtCore -framework QtConcurrent -framework QtSvg -framework QtSvgWidgets -framework QtWidgets -framework QtGui'\
		               '-framework QtCore -framework QtGui'
		do
			QT_TRY_LINK($libname)
			if test -n "$qt_cv_libname"; then
				break;
			fi
		done
		CXXFLAGS=$SAVE_CXXFLAGS
	])

	if test -z "$qt_cv_libname"; then
		if test x$USE_QT6 = xyes ; then
			AC_MSG_RESULT([failed, retrying with Qt5])
		else
			AC_MSG_RESULT([failed])
			AC_MSG_ERROR([cannot compile a simple Qt executable. Check you have the right \$QTDIR.])
		fi
	else
		AC_MSG_RESULT([$qt_cv_libname])
	fi
])


AC_DEFUN([QT_FIND_TOOL],
[
	$1=
	qt_major=5
	if test "x$USE_QT6" != "xno" ; then
		qt_major=6
	fi
	qt_ext="qt$qt_major"

	if test -n "$qt_cv_libexec" ; then
		AC_PATH_PROGS($1, [$2], [], $qt_cv_libexec)
	elif test -n "$qt_cv_bin" ; then
		AC_PATH_PROGS($1, [$2], [], $qt_cv_bin)
	elif qtchooser -l 2>/dev/null | grep -q ^$qt_ext\$ >/dev/null ; then
		AC_PATH_PROG(qtc_path, qtchooser, [], [$PATH])
		AC_PATH_PROG($2_path, $2, [], [$PATH])
		qtc_path=`dirname "$qtc_path"`
		$2_path=`dirname "$$2_path"`
		if test "$qtc_path" = "$$2_path" ; then
			AC_CHECK_PROG($1, $2, [$2 -qt=$qt_ext],, [$PATH])
		fi
	fi
	if test -z "$$1"; then
		AC_CHECK_PROGS($1, [$2-$qt_ext $2$qt_major $2],[],$PATH)
	fi
	if test -z "$$1"; then
		AC_MSG_ERROR([cannot find $2 binary.])
	fi
])


dnl get Qt version we're using
AC_DEFUN([QT_GET_VERSION],
[
	AC_CACHE_CHECK([Qt version],lyx_cv_qtversion,
	[
		SAVE_CPPFLAGS=$CPPFLAGS
		CPPFLAGS="$CPPFLAGS $QT_INCLUDES"

		cat > conftest.$ac_ext <<EOF
#line __oline__ "configure"
#include "confdefs.h"
#include <qglobal.h>
"%%%"QT_VERSION_STR"%%%"
EOF
		lyx_cv_qtversion=`(eval "$ac_cpp conftest.$ac_ext") 2>&5 | \
			grep '^"%%%"'  2>/dev/null | \
			sed -e 's/"%%%"//g' -e 's/"//g'`
		rm -f conftest.$ac_ext
		CPPFLAGS=$SAVE_CPPFLAGS
	])

	QTLIB_VERSION=$lyx_cv_qtversion
	AC_SUBST(QTLIB_VERSION)
])

dnl start here
AC_DEFUN([QT_DO_IT_ALL],
[
	dnl this variable is precious
	AC_ARG_VAR(QTDIR, [the place where the Qt files are, e.g. /usr/lib/qt5])

	AC_ARG_WITH(qt-dir, [AS_HELP_STRING([--with-qt-dir], [where the root of Qt is installed])],
		[ qt_cv_dir=`eval echo "$withval"/` ])

	AC_ARG_WITH(qt-includes, [AS_HELP_STRING([--with-qt-includes], [where the Qt includes are])],
		[ qt_cv_includes=`eval echo "$withval"` ])

	AC_ARG_WITH(qt-libraries, [AS_HELP_STRING([--with-qt-libraries], [where the Qt library is installed])],
		[  qt_cv_libraries=`eval echo "$withval"` ])

	dnl pay attention to $QTDIR unless overridden
	if test -z "$qt_cv_dir"; then
		qt_cv_dir=$QTDIR
	fi

	dnl derive inc/lib if needed
	if test -n "$qt_cv_dir"; then
		if test -z "$qt_cv_includes"; then
			qt_cv_includes=$qt_cv_dir/include
		fi
		if test -z "$qt_cv_libraries"; then
			qt_cv_libraries=$qt_cv_dir/lib
		fi
	fi

	dnl compute the binary dir too
	if test -n "$qt_cv_dir"; then
		qt_cv_bin=$qt_cv_dir/bin
	fi

	dnl Preprocessor flags
	QT_CPPFLAGS="-DQT_NO_STL -DQT_NO_KEYWORDS"
	case ${host} in
	*mingw*) QT_CPPFLAGS="-DQT_DLL $QT_CPPFLAGS";;
	esac
	AC_SUBST(QT_CPPFLAGS)

	dnl Check if it possible to do a pkg-config
	PKG_PROG_PKG_CONFIG
	dnl Not possible with Qt6 (QTBUG-86080)
	if test x$USE_QT6 = xno ; then
	    if test -n "$PKG_CONFIG" ; then
		    QT_DO_PKG_CONFIG
	    fi
	    if test "$pkg_failed" != "no" ; then
		    QT_DO_MANUAL_CONFIG
	    fi
	else
	    QT6_QMAKE_CONFIG
	    if test -z "$QT_LIB"; then
		    QT_DO_MANUAL_CONFIG
	    fi
	fi

	if test -z "$QT_LIB"; then
	  dnl Try again with Qt5 and if configuring for Qt6/5 fails
	  if test x$USE_QT6 = xyes ; then
		USE_QT6=no
		AC_SUBST([USE_QT6])
		if test -n "$PKG_CONFIG" ; then
		  QT_DO_PKG_CONFIG
		fi
		if test "$pkg_failed" != "no" ; then
		  QT_DO_MANUAL_CONFIG
		fi
		if test -z "$QT_LIB"; then
		  AC_MSG_ERROR([cannot find qt libraries.])
		fi
	  else
		AC_MSG_ERROR([cannot find qt libraries.])
	  fi
	fi

	dnl Check qt version
	AS_VERSION_COMPARE($QTLIB_VERSION, $1,
	[AC_MSG_ERROR([LyX requires at least version $1 of Qt. Only version $QTLIB_VERSION has been found.])
	])

	case $QTLIB_VERSION in
	6.*) if test $enable_stdlib_debug = "yes" ; then
		    LYX_WARNING([Compiling LyX with stdlib-debug and Qt6 library may lead to
   crashes. Consider dropping --enable-stdlib-debug.])
	     fi;;
	esac

	dnl Specific support for X11 will be built if these are available
	AC_CHECK_HEADERS([xcb/xcb.h])
	AC_CHECK_LIB([xcb], [xcb_send_event])

	save_CPPFLAGS=$CPPFLAGS
	CPPFLAGS="$save_CPPFLAGS $QT_CORE_INCLUDES"
	AC_CHECK_HEADER(QtGui/qtgui-config.h,
	  [lyx_qt_config=QtGui/qtgui-config.h],
	  [lyx_qt_config=qconfig.h])
	CPPFLAGS=$save_CPPFLAGS

	QT_FIND_TOOL([QT_MOC], [moc])
	QT_FIND_TOOL([QT_UIC], [uic])
	QT_FIND_TOOL([QT_RCC], [rcc])

	dnl Safety check
	mocqtver=`$QT_MOC -v 2>&1 | sed -e 's/.*\([[0-9]]\.[[0-9]]*\.[[0-9]]*\).*/\1/'`
	if test "x$mocqtver" != "x$QTLIB_VERSION"; then
		LYX_WARNING([The found moc compiler is for Qt $mocqtver but the Qt library version is $QTLIB_VERSION.])
	fi
])

AC_DEFUN([QT_DO_PKG_CONFIG],
[
	dnl tell pkg-config to look also in $qt_cv_dir/lib.
	save_PKG_CONFIG_PATH=$PKG_CONFIG_PATH
	if test -n "$qt_cv_dir" ; then
	  PKG_CONFIG_PATH=$qt_cv_dir/lib:$qt_cv_dir/lib/pkgconfig:$PKG_CONFIG_PATH
	  export PKG_CONFIG_PATH
	fi
	qt_corelibs="Qt5Core"
	qt_guilibs="Qt5Core Qt5Concurrent Qt5Gui Qt5Svg Qt5Widgets"
	lyx_use_x11extras=false
	PKG_CHECK_EXISTS(Qt5X11Extras, [lyx_use_x11extras=true], [])
	if $lyx_use_x11extras; then
		qt_guilibs="$qt_guilibs Qt5X11Extras xcb"
		AC_DEFINE(HAVE_QT5_X11_EXTRAS, 1,
			[Define if you have the Qt5X11Extras module])
	fi
	lyx_use_winextras=false
	PKG_CHECK_EXISTS(Qt5WinExtras, [lyx_use_winextras=true], [])
	if $lyx_use_winextras; then
		qt_guilibs="$qt_guilibs Qt5WinExtras"
	fi
	lyx_use_macextras=false
	PKG_CHECK_EXISTS(Qt5MacExtras, [lyx_use_macextras=true], [])
	if $lyx_use_macextras; then
		qt_guilibs="$qt_guilibs Qt5MacExtras"
	fi
	PKG_CHECK_MODULES(QT_CORE, $qt_corelibs,,[:])
	if test "$pkg_failed" = "no" ; then
		QT_CORE_INCLUDES=$QT_CORE_CFLAGS
		AC_SUBST(QT_CORE_INCLUDES)
		QT_CORE_LDFLAGS=`$PKG_CONFIG --libs-only-L $qt_corelibs`
		AC_SUBST(QT_CORE_LDFLAGS)
		QT_CORE_LIB=`$PKG_CONFIG --libs-only-l $qt_corelibs`
		AC_SUBST(QT_CORE_LIB)
	fi
	PKG_CHECK_MODULES(QT_FRONTEND, $qt_guilibs,,[:])
	if test "$pkg_failed" = "no" ; then
		QT_INCLUDES=$QT_FRONTEND_CFLAGS
		dnl QT_LDFLAGS=$QT_FRONTEND_LIBS
		QT_LDFLAGS=`$PKG_CONFIG --libs-only-L $qt_guilibs`
		AC_SUBST(QT_INCLUDES)
		AC_SUBST(QT_LDFLAGS)
		QTLIB_VERSION=`$PKG_CONFIG --modversion $qt_corelibs`
		AC_SUBST(QTLIB_VERSION)
		QT_LIB=`$PKG_CONFIG --libs-only-l $qt_guilibs`
		AC_SUBST(QT_LIB)
		dnl LIBS="$LIBS `$PKG_CONFIG --libs-only-other $qt_guilibs`"
	fi
	PKG_CONFIG_PATH=$save_PKG_CONFIG_PATH
	dnl Actually, the values of QT_LIB and QT_CORE_LIB can be completely
	dnl wrong on OS X, where everything goes to --libs-only-other.
	dnl As a quick workaround, let us assign better values. A better patch
	dnl exists for next cycle.
	QT_CORE_LIB=$QT_CORE_LIBS
	QT_CORE_LDFLAGS=
	QT_LIB=$QT_FRONTEND_LIBS
	QT_LDFLAGS=
])

AC_DEFUN([QT_DO_MANUAL_CONFIG],
[
	dnl Check for X libraries
	case ${host} in
	*mingw*) ;;
	*) \
	  AC_PATH_X \
	  AC_PATH_XTRA \
	;;
	esac
	case $have_x in
	    yes) LIBS="$X_PRE_LIBS $LIBS $X_LIBS -lX11 $X_EXTRA_LIBS"
	         CPPFLAGS="$CPPFLAGS $X_CFLAGS";;
	     no) AC_MSG_ERROR([cannot find X window libraries and/or headers.]);;
	disable) ;;
	esac

	dnl flags for compilation
	QT_INCLUDES=
	QT_LDFLAGS=
	QT_CORE_INCLUDES=
	QT_CORE_LDFLAGS=
	if test -n "$qt_cv_includes"; then
		QT_INCLUDES="-I$qt_cv_includes"
		for i in Qt QtCore QtGui QtWidgets QtSvg QtConcurrent QtSvgWidgets QtMacExtras; do
			QT_INCLUDES="$QT_INCLUDES -I$qt_cv_includes/$i"
			if test "$lyx_use_packaging" = "macosx" ; then
				QT_INCLUDES="$QT_INCLUDES -I$qt_cv_libraries/${i}.framework/Headers"
			fi
		done
		QT_CORE_INCLUDES="-I$qt_cv_includes -I$qt_cv_includes/QtCore"
	fi
	case "$qt_cv_libraries" in
	"")
		;;
	*)
		if test "$lyx_use_packaging" = "macosx" ; then
			QT_INCLUDES="$QT_INCLUDES -F$qt_cv_libraries"
			QT_CORE_INCLUDES="$QT_CORE_INCLUDES -I$qt_cv_libraries/QtCore.framework/Headers -F$qt_cv_libraries"
			QT_LDFLAGS="-F$qt_cv_libraries"
			QT_CORE_LDFLAGS="-F$qt_cv_libraries"
		else
			QT_LDFLAGS="-L$qt_cv_libraries"
			QT_CORE_LDFLAGS="-L$qt_cv_libraries"
		fi
		;;
	esac
	AC_SUBST(QT_INCLUDES)
	AC_SUBST(QT_CORE_INCLUDES)
	AC_SUBST(QT_LDFLAGS)
	AC_SUBST(QT_CORE_LDFLAGS)

	QT_CHECK_COMPILE

	QT_LIB=$qt_cv_libname;
	AC_SUBST(QT_LIB)
	AC_SUBST(QT_CORE_LIB)

	if test -n "$qt_cv_libname"; then
		QT_GET_VERSION
	fi
])

AC_DEFUN([QT6_QMAKE_CONFIG],
[
	dnl Use first suitable qmake in PATH
	AC_CHECK_PROGS([QT_QMAKE], [qmake-qt6 qmake6], [qmake], $PATH)
	case ${host} in
	*mingw*) ;;
	*) QT_QMAKE="env LC_ALL=C.UTF-8 $QT_QMAKE" ;;
	esac
	AC_MSG_CHECKING([for Qt6])
	qtver=`$QT_QMAKE -v | grep -o "Qt version ."`
	if test "$qtver" = "Qt version 6"; then
	    qt_cv_libexec=`$QT_QMAKE -query QT_INSTALL_LIBEXECS`
	    dnl Use a .pro file for getting qmake's variables
	    lyx_test_qt_dir=`mktemp -d`
	    lyx_test_qt_pro="$lyx_test_qt_dir/test.pro"
	    lyx_test_qt_mak="$lyx_test_qt_dir/Makefile"
	    cat > $lyx_test_qt_pro << EOF1
qtHaveModule(core):		QT += core
percent.target = %
percent.commands = @echo -n "\$(\$(@))\ "
QMAKE_EXTRA_TARGETS += percent
QMAKE_PROJECT_DEPTH = 0
EOF1
	    $QT_QMAKE $lyx_test_qt_pro -o $lyx_test_qt_mak 1>/dev/null 2>&1
	    QT_CORE_INCLUDES=`cd $lyx_test_qt_dir; make -s -f $lyx_test_qt_mak INCPATH | sed 's/-I\. //g'`
	    qt_corelibs=`cd $lyx_test_qt_dir; make -s -f $lyx_test_qt_mak LIBS`
	    QT_CORE_LDFLAGS=`echo $qt_corelibs | tr ' ' '\n' | grep -e "^-L" | tr '\n' ' '`
	    if test -z "$QT_CORE_LDFLAGS"; then
		QT_CORE_LDFLAGS="-L`$QT_QMAKE -query QT_INSTALL_LIBS`"
		QT_CORE_LIB="$qt_corelibs"
	    else
		QT_CORE_LIB=`echo $qt_corelibs | tr ' ' '\n' | grep -e "^-l" | tr '\n' ' '`
	    fi
	    if test -z "$QT_CORE_LIB"; then
		AC_MSG_RESULT(no)
	    else
		AC_SUBST(QT_CORE_INCLUDES)
		AC_SUBST(QT_CORE_LDFLAGS)
		AC_SUBST(QT_CORE_LIB)
		cat > $lyx_test_qt_pro << EOF2
QMAKE_EXTRA_VARIABLES = MISSING
qtHaveModule(core)		{QT += core} else {MISSING += core}
qtHaveModule(concurrent)	{QT += concurrent} else {MISSING += concurrent}
qtHaveModule(gui)		{QT += gui} else {MISSING += gui}
qtHaveModule(gui-private)	{QT += gui-private} else {MISSING += gui-private}
qtHaveModule(svg)		{QT += svg} else {MISSING += svg}
qtHaveModule(svgwidgets)	{QT += svgwidgets} else {MISSING += svgwidgets}
qtHaveModule(widgets)		{QT += widgets} else {MISSING += widgets}
percent.target = %
percent.commands = @echo -n "\$(\$(@))\ "
QMAKE_EXTRA_TARGETS += percent
QMAKE_PROJECT_DEPTH = 0
EOF2
		$QT_QMAKE $lyx_test_qt_pro -o $lyx_test_qt_mak 1>/dev/null 2>&1
		QT_INCLUDES=`cd $lyx_test_qt_dir; make -s -f $lyx_test_qt_mak INCPATH | sed 's/-I\. //g'`
		qt_guilibs=`cd $lyx_test_qt_dir; make -s -f $lyx_test_qt_mak LIBS`
		QT_LDFLAGS=`echo $qt_guilibs | tr ' ' '\n' | grep -e "^-L" | tr '\n' ' '`
		if test -z "$QT_LDFLAGS"; then
		    QT_LDFLAGS="-L`$QT_QMAKE -query QT_INSTALL_LIBS`"
		    QT_LIB="$qt_guilibs"
		else
		    QT_LIB=`echo $qt_guilibs | tr ' ' '\n' | grep -v "^-L" | tr '\n' ' '`
		fi
		QTLIB_VERSION=`$QT_QMAKE -v | grep "Qt version" | sed -e 's/.*\([[0-9]]\.[[0-9]]*\.[[0-9]]\).*/\1/'`
		if test -z "$QT_LIB"; then
		    AC_MSG_RESULT(no)
		else
		    QT_MISSING=`cd $lyx_test_qt_dir; make -s -f $lyx_test_qt_mak EXPORT_MISSING | sed 's/^ *//'`
		    if test -n "$QT_MISSING"; then
			    AC_MSG_ERROR([Qt6 module(s) $QT_MISSING not found.])
		    fi
		    AC_MSG_RESULT(yes)
		    AC_SUBST(QT_INCLUDES)
		    AC_SUBST(QT_LDFLAGS)
		    AC_SUBST(QT_LIB)
		    AC_SUBST(QTLIB_VERSION)
		fi
	    fi
	    rm $lyx_test_qt_pro $lyx_test_qt_mak $lyx_test_qt_dir/.qmake.stash
	    rmdir $lyx_test_qt_dir
	else
	    AC_MSG_RESULT(no)
	fi
])
