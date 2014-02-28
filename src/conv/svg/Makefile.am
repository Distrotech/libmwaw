if BUILD_CONVERTISSORS
bin_PROGRAMS = mwaw2svg

AM_CXXFLAGS = -I$(top_srcdir)/inc/ $(REVENGE_CFLAGS) $(REVENGE_GENERATORS_CFLAGS) $(DEBUG_CXXFLAGS)

mwaw2svg_DEPENDENCIES = @MWAW2SVG_WIN32_RESOURCE@

if STATIC_TOOLS

mwaw2svg_LDADD = \
	../../lib/@MWAW_OBJDIR@/libmwaw-@MWAW_MAJOR_VERSION@.@MWAW_MINOR_VERSION@.a \
	$(REVENGE_LIBS) $(REVENGE_GENERATORS_LIBS) @MWAW2SVG_WIN32_RESOURCE@
mwaw2svg_LDFLAGS = -all-static

else	

mwaw2svg_LDADD = \
	../../lib/libmwaw-@MWAW_MAJOR_VERSION@.@MWAW_MINOR_VERSION@.la \
	$(REVENGE_LIBS) $(REVENGE_GENERATORS_LIBS) @MWAW2SVG_WIN32_RESOURCE@

endif

mwaw2svg_SOURCES = \
	mwaw2svg.cpp

if OS_WIN32

@MWAW2SVG_WIN32_RESOURCE@ : mwaw2svg.rc $(mwaw2svg_OBJECTS)
	chmod +x $(top_srcdir)/build/win32/*compile-resource
	WINDRES=@WINDRES@ $(top_srcdir)/build/win32/lt-compile-resource mwaw2svg.rc @MWAW2SVG_WIN32_RESOURCE@
endif

EXTRA_DIST = \
	$(mwaw2svg_SOURCES)	\
	mwaw2svg.rc.in

# These may be in the builddir too
BUILD_EXTRA_DIST = \
	mwaw2svg.rc	 

endif
