bin_PROGRAMS = mwawFile

AM_CXXFLAGS = -I$(top_srcdir) -I$(top_srcdir)/src/lib/ $(WPD_CFLAGS) $(XATTR_CFLAGS) $(DEBUG_CXXFLAGS)

mwawFile_DEPENDENCIES = @MWAWFILE_WIN32_RESOURCE@
mwawFile_LDADD = @MWAWFILE_WIN32_RESOURCE@

if STATIC_TOOLS
mwawFile_LDFLAGS = -all-static
else
mwawFile_LDFLAGS =
endif

mwawFile_SOURCES = 		\
	file_internal.h		\
	file.cpp		\
	input.h			\
	input.cpp		\
	ole.h			\
	ole.cpp			\
	rsrc.h			\
	rsrc.cpp		\
	xattr.h			\
	xattr.cpp

if OS_WIN32
@MWAWFILE_WIN32_RESOURCE@ : mwawFile.rc $(mwawFile_OBJECTS)
	chmod +x $(top_srcdir)/build/win32/*compile-resource
	WINDRES=@WINDRES@ $(top_srcdir)/build/win32/lt-compile-resource mwawFile.rc @MWAWFILE_WIN32_RESOURCE@
endif

EXTRA_DIST = \
	$(mwawFile_SOURCES)	\
	mwawFile.rc.in

# These may be in the builddir too
BUILD_EXTRA_DIST = \
	mwawFile.rc	 
 
