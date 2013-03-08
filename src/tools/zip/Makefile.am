bin_PROGRAMS = mwawZip

AM_CXXFLAGS = -I$(top_srcdir) -I$(top_srcdir)/src/lib/ $(LIBMWAW_CXXFLAGS) $(XATTR_CFLAGS) $(ZLIB_CFLAGS) $(DEBUG_CXXFLAGS)

mwawZip_DEPENDENCIES = @MWAWZIP_WIN32_RESOURCE@
mwawZip_LDADD = $(ZLIB_LIBS) @MWAWZIP_WIN32_RESOURCE@

if STATIC_TOOLS
mwawZip_LDFLAGS = -all-static
else
mwawZip_LDFLAGS =
endif

mwawZip_SOURCES = 		\
	input.h			\
	input.cpp		\
	mwawZip.cpp		\
	xattr.h			\
	xattr.cpp		\
	zip.h			\
	zip.cpp

if OS_WIN32
@MWAWZIP_WIN32_RESOURCE@ : mwawZip.rc $(mwawZip_OBJECTS)
	chmod +x $(top_srcdir)/build/win32/*compile-resource
	WINDRES=@WINDRES@ $(top_srcdir)/build/win32/lt-compile-resource mwawZip.rc @MWAWZIP_WIN32_RESOURCE@
endif

EXTRA_DIST = \
	$(mwawZip_SOURCES)	\
	mwawZip.rc.in

# These may be in the builddir too
BUILD_EXTRA_DIST = \
	mwawZip.rc	 
 
