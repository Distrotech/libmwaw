bin_PROGRAMS = mwaw2raw

AM_CXXFLAGS = -I$(top_srcdir) -I$(top_srcdir)/src/lib/ $(WPD_CFLAGS) $(DEBUG_CXXFLAGS)

mwaw2raw_DEPENDENCIES = @MWAW2RAW_WIN32_RESOURCE@

if STATIC_TOOLS

mwaw2raw_LDADD = \
	../../lib/@MWAW_OBJDIR@/libmwaw-@MWAW_MAJOR_VERSION@.@MWAW_MINOR_VERSION@.a \
	$(WPD_LIBS) @MWAW2RAW_WIN32_RESOURCE@
mwaw2raw_LDFLAGS = -all-static

else	

mwaw2raw_LDADD = \
	../../lib/libmwaw-@MWAW_MAJOR_VERSION@.@MWAW_MINOR_VERSION@.la \
	$(WPD_LIBS) @MWAW2RAW_WIN32_RESOURCE@

endif

mwaw2raw_SOURCES = \
	RawDocumentGenerator.cpp	 \
	RawDocumentGenerator.h		 \
	mwaw2raw.cpp

if OS_WIN32

@MWAW2RAW_WIN32_RESOURCE@ : mwaw2raw.rc $(mwaw2raw_OBJECTS)
	chmod +x $(top_srcdir)/build/win32/*compile-resource
	WINDRES=@WINDRES@ $(top_srcdir)/build/win32/lt-compile-resource mwaw2raw.rc @MWAW2RAW_WIN32_RESOURCE@
endif

EXTRA_DIST = \
	$(mwaw2raw_SOURCES)	\
	mwaw2raw.rc.in

# These may be in the builddir too
BUILD_EXTRA_DIST = \
	mwaw2raw.rc	 
 
