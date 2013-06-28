# Process this file with autoconf to create configure.

AC_PREREQ([2.60])

# ====================
# Version informations
# ====================
m4_define([libmwaw_version_major],[0])
m4_define([libmwaw_version_minor],[1])
m4_define([libmwaw_version_micro],[11])
m4_define([libmwaw_version],[libmwaw_version_major.libmwaw_version_minor.libmwaw_version_micro])

# =============
# Automake init
# =============
AC_INIT([libmwaw],[libmwaw_version])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([1.11 foreign dist-xz dist-bzip2])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_LANG([C++])

# ===========================
# Find required base packages
# ===========================
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_PROG_CC([gcc cc clang])
AC_PROG_CPP([g++ c++ clang++])
AC_PROG_CXX([g++ c++ clang++])
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
LT_INIT([win32-dll disable-static pic-only])
AC_CANONICAL_HOST

PKG_PROG_PKG_CONFIG([0.20])

# ====================
# Find additional apps
# ====================
PKG_CHECK_MODULES([WPD],[
    libwpd-0.9 >= 0.9.5
    libwpd-stream-0.9 >= 0.9.5
])
AC_SUBST(WPD_CFLAGS)
AC_SUBST(WPD_LIBS)

# =================================
# Libtool/Version Makefile settings
# =================================
AC_SUBST(MWAW_MAJOR_VERSION, [libmwaw_version_major])
AC_SUBST(MWAW_MINOR_VERSION, [libmwaw_version_minor])
AC_SUBST(MWAW_MICRO_VERSION, [libmwaw_version_micro])
AC_SUBST(MWAW_VERSION, [libmwaw_version])
# AC_SUBST(LT_RELEASE, [libmwaw_version_major.libmwaw_version_minor])
LT_CURRENT=`expr 100 '*' libmwaw_version_major + libmwaw_version_minor`
# For 1.0.0 comment the first line and uncomment the second
LT_AGE=0
# LT_AGE=libmwaw_version_minor
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION, [libmwaw_version_micro])
AC_SUBST(LT_AGE)

# ==========================
# Platform check for windows
# ==========================
AC_MSG_CHECKING([for native Win32])
AS_CASE([$host],
	[*-*-mingw*], [
		native_win32=yes
		AC_CHECK_TOOL(WINDRES, windres)
		LIBMWAW_WIN32_RESOURCE=libmwaw-win32res.lo
		LIBMWAW_STREAM_WIN32_RESOURCE=libmwaw-stream-win32res.lo
		MWAW2HTML_WIN32_RESOURCE=mwaw2html-win32res.lo
		MWAW2TEXT_WIN32_RESOURCE=mwaw2text-win32res.lo
		MWAW2RAW_WIN32_RESOURCE=mwaw2raw-win32res.lo
		MWAWFILE_WIN32_RESOURCE=mwawFile-win32res.lo
		MWAWZIP_WIN32_RESOURCE=mwawZip-win32res.lo
	], [
		native_win32=no
		LIBMWAW_WIN32_RESOURCE=
		LIBMWAW_STREAM_WIN32_RESOURCE=
		MWAW2HTML_WIN32_RESOURCE=
		MWAW2TEXT_WIN32_RESOURCE=
		MWAW2RAW_WIN32_RESOURCE=
		MWAWFILE_WIN32_RESOURCE=
		MWAWZIP_WIN32_RESOURCE=
	]
)
AC_MSG_RESULT([$native_win32])
AM_CONDITIONAL(OS_WIN32, [test "x$native_win32" = "xyes"])
AC_SUBST(LIBMWAW_WIN32_RESOURCE)
AC_SUBST(LIBMWAW_STREAM_WIN32_RESOURCE)
AC_SUBST(MWAW2HTML_WIN32_RESOURCE)
AC_SUBST(MWAW2TEXT_WIN32_RESOURCE)
AC_SUBST(MWAW2RAW_WIN32_RESOURCE)
AC_SUBST(MWAWFILE_WIN32_RESOURCE)
AC_SUBST(MWAWZIP_WIN32_RESOURCE)

AC_MSG_CHECKING([for Win32 platform in general])
AS_CASE([$host],
	[*-*-mingw*|*-*-cygwin*], [platform_win32=yes],
	[platform_win32=no]
)
AC_MSG_RESULT([$platform_win32])
AM_CONDITIONAL([PLATFORM_WIN32], [test "x$platform_win32" = "xyes"])

AC_ARG_ENABLE([def-files],
	[AS_HELP_STRING([--enable-def-files], [Enable use of def files to define exported symbols on win32])],
	[enable_def_files="$enableval"],
	[enable_def_files=no]
)
AM_CONDITIONAL(DEF_FILES, [test "x$enable_def_files" = "xyes" -a "x$native_win32" = "xyes"])

# =========================
# Platform check for darwin
# =========================
AC_MSG_CHECKING([for Darwin (Mac OS X) platform])
AS_CASE([$host],
	[*-*-darwin*], [platform_darwin=yes],
	[platform_darwin=no]
)
AC_MSG_RESULT([$platform_darwin])
AM_CONDITIONAL(PLATFORM_DARWIN, [test "x$platform_darwin" = "xyes"])

# ================
# Check for cflags
# ================
AC_ARG_ENABLE([werror],
	[AS_HELP_STRING([--disable-werror], [Treat all warnings as errors, usefull for development])],
	[enable_werror="$enableval"],
	[enable_werror=yes]
)
AS_IF([test x"$enable_werror" != "xno"], [
	CFLAGS="$CFLAGS -Werror"
	CXXFLAGS="$CXXFLAGS -Werror"
])
AC_ARG_ENABLE([weffc],
	[AS_HELP_STRING([--disable-weffc], [Disable -Weffc++ warnings, usefull when using an old version of gcc or some boost version])],
	[enable_weffc="$enableval"],
	[enable_weffc=yes]
)
AC_ARG_ENABLE([wparanoic],
	[AS_HELP_STRING([--enable-wparanoic], [Enable a lot of warnings...])],
	[enable_wparanoic="$enableval"],
	[enable_wparanoic=no]
)
# Courtesy of Glib: Ensure MSVC-compatible struct packing convention
# is used when compiling for Win32 with gcc.
AS_IF([test "x$native_win32" = "xyes"], [
	AC_CHECK_TOOL(WINDRES, windres)
	AS_IF([test x"$GCC" = xyes], [
		AC_MSG_CHECKING([how to get MSVC-compatible struct packing])
		AS_IF([test -z "$ac_cv_prog_CC"], [
			our_gcc="$CC"
		], [
			our_gcc="$ac_cv_prog_CC"
		])
		AS_IF([$our_gcc -v --help 2>/dev/null | grep ms-bitfields >/dev/null], [
			msnative_struct="-mms-bitfields"
			CFLAGS="$CFLAGS $msnative_struct"
			CXXFLAGS="$CXXFLAGS $msnative_struct"
			AC_MSG_RESULT([${msnative_struct}])
		], [
			AC_MSG_RESULT([no way])
			AC_MSG_WARN([produced libraries might be incompatible with MSVC-compiled code])
		])
	])
	CFLAGS="$CFLAGS -Wall -pedantic"
	CXXFLAGS="$CXXFLAGS -Wall -pedantic"
], [
	AS_IF([test x"$GCC" = xyes], [
		# Be tough with warnings and produce less careless code
		CFLAGS="$CFLAGS -Wall -Wextra -pedantic"
		CXXFLAGS="$CXXFLAGS -Wall -Wextra -pedantic -Wshadow -Wunused-variable"
		AS_IF([test x"$enable_weffc" != "xno"], [
			CXXFLAGS="$CXXFLAGS -Weffc++"
		])
		AS_IF([test x"$enable_wparanoic" != "xno"], [
			CXXFLAGS="$CXXFLAGS -Wcast-align -Wcast-qual -Wchar-subscripts"
			CXXFLAGS="$CXXFLAGS -Wcomment -Wconversion -Wdisabled-optimization"
			CXXFLAGS="$CXXFLAGS -Wfloat-equal -Wformat -Wformat=2"
			CXXFLAGS="$CXXFLAGS -Wformat-nonliteral -Wformat-security"
			CXXFLAGS="$CXXFLAGS -Wformat-y2k -Wimport -Winit-self -Winvalid-pch"
			CXXFLAGS="$CXXFLAGS -Wmissing-braces -Wmissing-field-initializers"
			CXXFLAGS="$CXXFLAGS -Wmissing-format-attribute -Wmissing-include-dirs"
			CXXFLAGS="$CXXFLAGS -Wmissing-noreturn -Wpacked -Wparentheses"
			CXXFLAGS="$CXXFLAGS -Wpointer-arith -Wredundant-decls -Wreturn-type"
			CXXFLAGS="$CXXFLAGS -Wsequence-point -Wsign-compare -Wstrict-aliasing"
			CXXFLAGS="$CXXFLAGS -Wstrict-aliasing=2 -Wswitch -Wswitch-default"
			CXXFLAGS="$CXXFLAGS -Wswitch-enum -Wtrigraphs -Wunknown-pragmas -Wunused"
			CXXFLAGS="$CXXFLAGS -Wunused-function -Wunused-label -Wunused-parameter"
			CXXFLAGS="$CXXFLAGS -Wunused-value -Wvariadic-macros"
			CXXFLAGS="$CXXFLAGS -Wvolatile-register-var -Wwrite-strings"
		])
	])
])
AC_SUBST(DEBUG_CXXFLAGS)

# ============
# Debug switch
# ============
AC_ARG_ENABLE([debug],
	[AS_HELP_STRING([--enable-debug], [Turn on debugging])],
	[enable_debug="$enableval"],
	[enable_debug=no ]
)
AC_ARG_ENABLE([full-debug],
	[AS_HELP_STRING([--enable-full-debug], [Turn on debugging and debug files' creation ])],
	[enable_full_debug="$enableval"],
	[enable_full_debug=no]
)
AS_IF([test "x$enable_full_debug" = "xyes"],
	[ DEBUG_CXXFLAGS="-DDEBUG -DDEBUG_WITH_FILES -g" ],
	[ AS_IF([test "x$enable_debug" = "xyes"],
		[DEBUG_CXXFLAGS="-DDEBUG -g"],
		[DEBUG_CXXFLAGS="-DNDEBUG"])
	]
)
AC_SUBST(DEBUG_CXXFLAGS)

# ============
# Static tools
# ============
AC_ARG_ENABLE([static-tools],
	[AS_HELP_STRING([--enable-static-tools], [Link tools (binaries) statically])],
	[enable_static_tools="$enableval"],
	[enable_static_tools=no]
)
AS_IF([test "x$enable_static_tools" = "xyes"], [
	enable_static="yes"
], [
	AC_DISABLE_STATIC
])
AM_CONDITIONAL(STATIC_TOOLS, [test "x$enable_static_tools" = "xyes"])

AC_ARG_WITH(sharedptr,
	[AS_HELP_STRING([--with-sharedptr=boost|c++11|tr1],
		[Use Boost|std::memory|std::tr1::memory for shared ptr implementation])],
	[with_sharedptr="$withval"],
	[with_sharedptr=boost]
)
AS_CASE([$with_sharedptr],
	[boost|c++11|tr1], [],
	[AC_MSG_ERROR([Unknown value of shared_ptr provider: $with_sharedptr])]
)

AS_IF([test "x$with_sharedptr" = "xc++11"], [
	AC_MSG_CHECKING([for c++11 shared ptr])
	AC_COMPILE_IFELSE([
		AC_LANG_PROGRAM(
			[[#include <memory>]],
			[[std::shared_ptr<int> p;]]
		)
	], [
		AC_MSG_RESULT([yes])
		AC_DEFINE([SHAREDPTR_STD],[1],[Use C++11 implementation of shared_ptr])
	], [
		AC_MSG_RESULT([no])
		AC_MSG_ERROR([Could not find C++11 implementation of shared_ptr])
	], [
		[#include <memory>]
	])
])
AS_IF([test "x$with_sharedptr" = "xtr1"], [
	AC_MSG_CHECKING([for tr1 shared ptr])
	AC_COMPILE_IFELSE([
		AC_LANG_PROGRAM(
			[[#include <tr1/memory>]],
			[[std::tr1::shared_ptr<int> p;]]
		)
	], [
		AC_MSG_RESULT([yes])
		AC_DEFINE([SHAREDPTR_TR1],[1],[Use TR1 implementation of shared_ptr])
	], [
		AC_MSG_RESULT([no])
		AC_MSG_ERROR([Could not find TR1 implementation of shared_ptr])
	], [
		[#include <tr1/memory>]
	])
])
AS_IF([test "x$with_sharedptr" = "xboost"], [
	AC_MSG_CHECKING([for boost shared ptr])
	AC_COMPILE_IFELSE([
		AC_LANG_PROGRAM(
			[[#include <boost/shared_ptr.hpp>]],
			[[boost::shared_ptr<int> p;]]
		)
	], [
		AC_MSG_RESULT([yes])
		AC_DEFINE([SHAREDPTR_BOOST],[1],[Use Boost implementation of shared_ptr])
	], [
		AC_MSG_RESULT([no])
		AC_MSG_ERROR([Could not find Boost implementation of shared_ptr])
	])
])

# =========
# check for file extended attribute functions
# =========
AC_MSG_CHECKING([for extended file system attributes])
AC_COMPILE_IFELSE([
	AC_LANG_PROGRAM(
		[[#include <sys/xattr.h>]],
		[[getxattr("/tmp/toto", "com.apple.FinderInfo", 0, 0, 0, XATTR_SHOWCOMPRESSION);]]
	)
], [
	AC_MSG_RESULT([yes, find Darwin getxattr])
	[XATTR_CFLAGS="-DWITH_EXTENDED_FS=1"]
], [
	AC_COMPILE_IFELSE([
		AC_LANG_PROGRAM(
			[[#include <sys/types.h>]
			 [#include <sys/xattr.h>]],
			[[getxattr("/tmp/toto", "com.apple.FinderInfo", 0, 0);]]
		)
	], [
		AC_MSG_RESULT([yes, find Linux getxattr])
		[XATTR_CFLAGS="-DWITH_EXTENDED_FS=2"]
	], [
		AC_MSG_RESULT([no])
		[XATTR_CFLAGS="-DWITH_EXTENDED_FS=0"]
	])
])
AC_SUBST(XATTR_CFLAGS)

# =========
# Find zlib
# =========
AC_ARG_ENABLE([zip],
	[AS_HELP_STRING([--disable-zip], [Do not use the zip library and so do not accept a zipped document as input.])],
	[with_zip="$enableval"],
	[with_zip=yes]
)
if test "x$with_zip" != "xno"; then
	PKG_CHECK_MODULES([ZLIB],[zlib],[],[
    		AC_CHECK_HEADER(zlib.h, [],
	        	[AC_MSG_ERROR(zlib.h not found. install zlib)], [])
    		AC_CHECK_LIB(z, inflate, [ ZLIB_LIBS=-lz ],
        		[AC_MSG_ERROR(zlib not found or functional)], [])
	])
	AC_DEFINE([USE_ZIP],[1],[Use the zip library to read a zipped document.])
	ZLIB_CFLAGS="$ZLIB_CFLAGS -DUSE_ZIP"
else
	ZLIB_CFLAGS=
	ZLIB_LIBS=
fi
AC_SUBST(ZLIB_CFLAGS)
AC_SUBST(ZLIB_LIBS)
AM_CONDITIONAL([WITH_LIBMWAW_ZIP], [test "x$with_zip" != "xno"])

# =============
# Documentation
# =============
AC_ARG_WITH(docs,
	[AS_HELP_STRING([--without-docs], [Do not build documentation])],
	[with_docs="$withval"],
	[AS_IF([test "x$native_win32" = "xyes"], [with_docs=no], [with_docs=yes])]
)
AS_IF([test "x$with_docs" != "xno"], [
	AC_PATH_PROG(DOXYGEN, [doxygen])
	AS_IF([test -z "$DOXYGEN"], [
		AC_MSG_WARN([*** Could not find doxygen in your PATH.])
		AC_MSG_WARN([*** The documentation will not be built.])
		build_docs=no
	], [build_docs=yes])
], [build_docs=no])
AM_CONDITIONAL([WITH_LIBMWAW_DOCS], [test "x$build_docs" != "xno"])

# =====================
# Prepare all .in files
# =====================
AC_CONFIG_FILES([
Makefile
src/Makefile
src/conv/Makefile
src/conv/html/Makefile
src/conv/html/mwaw2html.rc
src/conv/raw/Makefile
src/conv/raw/mwaw2raw.rc
src/conv/text/Makefile
src/conv/text/mwaw2text.rc
src/tools/Makefile
src/tools/file/Makefile
src/tools/file/mwawFile.rc
src/tools/zip/Makefile
src/tools/zip/mwawZip.rc
src/lib/Makefile
src/lib/libmwaw.rc
docs/Makefile
docs/doxygen/Makefile
build/Makefile
build/win32/Makefile
inc/Makefile
inc/libmwaw/Makefile
libmwaw-$MWAW_MAJOR_VERSION.$MWAW_MINOR_VERSION.pc:libmwaw.pc.in
libmwaw.spec
libmwaw-zip
])
AC_OUTPUT

# ==============================================
# Display final informations about configuration
# ==============================================
AC_MSG_NOTICE([
==============================================================================
Build configuration:
	debug:           ${enable_debug}
	full-debug:      ${enable_full_debug}
	docs:            ${build_docs}
	shared_ptr:      ${with_sharedptr}
	zip:             ${with_zip}
	static-tools:    ${enable_static_tools}
	werror:          ${enable_werror}
==============================================================================
])
