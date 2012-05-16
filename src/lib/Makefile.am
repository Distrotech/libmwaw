if OS_WIN32
version_info = -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) -avoid-version
else
version_info = -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)
endif

lib_LTLIBRARIES = libmwaw-@MWAW_MAJOR_VERSION@.@MWAW_MINOR_VERSION@.la $(target_libmwaw_stream)
libmwaw_@MWAW_MAJOR_VERSION@_@MWAW_MINOR_VERSION@_includedir = $(includedir)/libmwaw-@MWAW_MAJOR_VERSION@.@MWAW_MINOR_VERSION@/libmwaw
libmwaw_@MWAW_MAJOR_VERSION@_@MWAW_MINOR_VERSION@_include_HEADERS = libmwaw.hxx MWAWDocument.hxx MWAWPropertyHandler.hxx 

INCLUDES = -I$(srcdir) -I..

AM_CXXFLAGS = $(LIBMWAW_CXXFLAGS) $(DEBUG_CXXFLAGS)

libmwaw_@MWAW_MAJOR_VERSION@_@MWAW_MINOR_VERSION@_la_LIBADD  = $(LIBMWAW_LIBS) @LIBMWAW_WIN32_RESOURCE@
libmwaw_@MWAW_MAJOR_VERSION@_@MWAW_MINOR_VERSION@_la_DEPENDENCIES = @LIBMWAW_WIN32_RESOURCE@  
libmwaw_@MWAW_MAJOR_VERSION@_@MWAW_MINOR_VERSION@_la_LDFLAGS = $(version_info) -export-dynamic  -no-undefined
libmwaw_@MWAW_MAJOR_VERSION@_@MWAW_MINOR_VERSION@_la_SOURCES = \
	libmwaw_internal.cxx		\
	DMWAWContentListener.cxx	\
	MWAWOLEStream.cxx		\
	MWAWPageSpan.cxx		\
	libmwaw_internal.hxx		\
	DMWAWContentListener.hxx	\
	MWAWOLEStream.hxx		\
	MWAWPageSpan.hxx		\
					\
	MWAWCell.cxx			\
	MWAWContentListener.cxx	\
	MWAWDocument.cxx		\
	MWAWHeader.cxx			\
	MWAWList.cxx			\
	MWAWSubDocument.cxx		\
	MWAWTable.cxx		\
	MWAWCell.hxx			\
	MWAWContentListener.hxx	\
	MWAWDocument.hxx		\
	MWAWEntry.hxx			\
	MWAWHeader.hxx			\
	MWAWList.hxx			\
	MWAWParser.hxx			\
	MWAWSubDocument.hxx		\
	MWAWTable.hxx		\
					\
	MWAWDebug.cxx			\
	MWAWInputStream.cxx		\
	MWAWFontConverter.cxx			\
	MWAWPrinter.cxx			\
	MWAWOLEParser.cxx		\
	MWAWPictBasic.cxx		\
	MWAWPictBitmap.cxx		\
	MWAWPictData.cxx		\
	MWAWPictMac.cxx		\
	MWAWPictOLEContainer.cxx	\
	MWAWPropertyHandler.cxx	\
	MWAWDebug.hxx			\
	MWAWInputStream.hxx		\
	MWAWFontConverter.hxx			\
	MWAWPrinter.hxx			\
	MWAWOLEParser.hxx		\
	MWAWPict.hxx			\
	MWAWPictBasic.hxx		\
	MWAWPictBitmap.hxx		\
	MWAWPictData.hxx		\
	MWAWPictMac.hxx		\
	MWAWPictOLEContainer.hxx	\
	MWAWPosition.hxx		\
	MWAWPropertyHandler.hxx	\
					\
	MWAWFont.cxx			\
	libmwaw.hxx			\
	MWAWFont.hxx			\
					\
	CWDatabase.cxx			\
	CWGraph.cxx			\
	CWParser.cxx			\
	CWSpreadsheet.cxx		\
	CWTable.cxx			\
	CWText.cxx			\
	FWParser.cxx			\
	FWText.cxx			\
	MSKGraph.cxx			\
	MSKParser.cxx			\
	MSKText.cxx			\
	MSWParser.cxx			\
	MSWText.cxx			\
	MWParser.cxx			\
	MWProParser.cxx			\
	MWProStructures.cxx		\
	WNParser.cxx			\
	WNText.cxx			\
	WPParser.cxx			\
	CWDatabase.hxx			\
	CWGraph.hxx			\
	CWParser.hxx			\
	CWSpreadsheet.hxx		\
	CWStruct.hxx			\
	CWTable.hxx			\
	CWText.hxx			\
	FWParser.hxx			\
	FWText.hxx			\
	MSKGraph.hxx			\
	MSKParser.hxx			\
	MSKText.hxx			\
	MSWParser.hxx			\
	MSWText.hxx			\
	MWParser.hxx			\
	MWProParser.hxx			\
	MWProStructures.hxx		\
	WNEntry.hxx			\
	WNParser.hxx			\
	WNText.hxx			\
	WPParser.hxx

if OS_WIN32

@LIBMWAW_WIN32_RESOURCE@ : libmwaw.rc $(libmwaw_@MWAW_MAJOR_VERSION@_@MWAW_MINOR_VERSION@_la_OBJECTS)
	chmod +x $(top_srcdir)/build/win32/*compile-resource
	WINDRES=@WINDRES@ $(top_srcdir)/build/win32/lt-compile-resource libmwaw.rc @LIBMWAW_WIN32_RESOURCE@

endif

EXTRA_DIST = \
	makefile.mk \
	libmwaw.rc.in

# These may be in the builddir too
BUILD_EXTRA_DIST = \
	libmwaw.rc
