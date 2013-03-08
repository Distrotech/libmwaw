bin_PROGRAMS = mwaw2html

AM_CXXFLAGS = -I$(top_srcdir)/src/lib/ $(WPD_CFLAGS) $(DEBUG_CXXFLAGS)

mwaw2html_DEPENDENCIES = @MWAW2HTML_WIN32_RESOURCE@

if STATIC_TOOLS

mwaw2html_LDADD = \
	../../lib/@MWAW_OBJDIR@/libmwaw-@MWAW_MAJOR_VERSION@.@MWAW_MINOR_VERSION@.a \
	$(WPD_LIBS) @MWAW2HTML_WIN32_RESOURCE@
mwaw2html_LDFLAGS = -all-static

else	

mwaw2html_LDADD = \
	../../lib/libmwaw-@MWAW_MAJOR_VERSION@.@MWAW_MINOR_VERSION@.la \
	$(WPD_LIBS) @MWAW2HTML_WIN32_RESOURCE@

endif

mwaw2html_SOURCES = \
	HtmlDocumentGenerator.cpp	\
	HtmlDocumentGenerator.h		\
	TableStyle.cpp			\
	TableStyle.h			\
	TextRunStyle.cpp		\
	TextRunStyle.h			\
	mwaw2html.cpp			\
	mwaw2html_internal.cpp		\
	mwaw2html_internal.h

if OS_WIN32

@MWAW2HTML_WIN32_RESOURCE@ : mwaw2html.rc $(mwaw2html_OBJECTS)
	chmod +x $(top_srcdir)/build/win32/*compile-resource
	WINDRES=@WINDRES@ $(top_srcdir)/build/win32/lt-compile-resource mwaw2html.rc @MWAW2HTML_WIN32_RESOURCE@
endif

EXTRA_DIST = \
	mwaw2html.rc.in

# These may be in the builddir too
BUILD_EXTRA_DIST = \
	mwaw2html.rc	 

