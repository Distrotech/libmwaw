bin_PROGRAMS = mwaw2csv

AM_CXXFLAGS = -I$(top_srcdir)/inc/ $(WPD_CFLAGS) $(DEBUG_CXXFLAGS)

mwaw2csv_DEPENDENCIES = @MWAW2CSV_WIN32_RESOURCE@

if STATIC_TOOLS

mwaw2csv_LDADD = \
	../../lib/@MWAW_OBJDIR@/libmwaw-@MWAW_MAJOR_VERSION@.@MWAW_MINOR_VERSION@.a \
	$(WPD_LIBS) @MWAW2CSV_WIN32_RESOURCE@
mwaw2csv_LDFLAGS = -all-static

else	

mwaw2csv_LDADD = \
	../../lib/libmwaw-@MWAW_MAJOR_VERSION@.@MWAW_MINOR_VERSION@.la \
	$(WPD_LIBS) @MWAW2CSV_WIN32_RESOURCE@

endif

mwaw2csv_SOURCES = \
	CSVGenerator.cpp	 \
	CSVGenerator.h		 \
	mwaw2csv.cpp

if OS_WIN32

@MWAW2CSV_WIN32_RESOURCE@ : mwaw2csv.rc $(mwaw2csv_OBJECTS)
	chmod +x $(top_srcdir)/build/win32/*compile-resource
	WINDRES=@WINDRES@ $(top_srcdir)/build/win32/lt-compile-resource mwaw2csv.rc @MWAW2CSV_WIN32_RESOURCE@
endif

EXTRA_DIST = \
	$(mwaw2csv_SOURCES)	\
	mwaw2csv.rc.in

# These may be in the builddir too
BUILD_EXTRA_DIST = \
	mwaw2csv.rc	 

