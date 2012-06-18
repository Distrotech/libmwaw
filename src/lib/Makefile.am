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
	libmwaw.hxx			\
	libmwaw_internal.cxx		\
	libmwaw_internal.hxx		\
	CWDatabase.cxx			\
	CWDatabase.hxx			\
	CWGraph.cxx			\
	CWGraph.hxx			\
	CWParser.cxx			\
	CWParser.hxx			\
	CWSpreadsheet.cxx		\
	CWSpreadsheet.hxx		\
	CWStruct.hxx			\
	CWTable.cxx			\
	CWTable.hxx			\
	CWText.cxx			\
	CWText.hxx			\
	FWParser.cxx			\
	FWParser.hxx			\
	FWText.cxx			\
	FWText.hxx			\
	MSKGraph.cxx			\
	MSKGraph.hxx			\
	MSKParser.cxx			\
	MSKParser.hxx			\
	MSKText.cxx			\
	MSKText.hxx			\
	MSWParser.cxx			\
	MSWParser.hxx			\
	MSWStruct.cxx			\
	MSWStruct.hxx			\
	MSWText.cxx			\
	MSWText.hxx			\
	MSWTextStyles.cxx		\
	MSWTextStyles.hxx		\
	MWAWCell.cxx			\
	MWAWCell.hxx			\
	MWAWContentListener.cxx		\
	MWAWContentListener.hxx		\
	MWAWDebug.cxx			\
	MWAWDebug.hxx			\
	MWAWDocument.cxx		\
	MWAWDocument.hxx		\
	MWAWEntry.hxx			\
	MWAWFont.cxx			\
	MWAWFont.hxx			\
	MWAWFontConverter.cxx		\
	MWAWFontConverter.hxx		\
	MWAWHeader.cxx			\
	MWAWHeader.hxx			\
	MWAWInputStream.cxx		\
	MWAWInputStream.hxx		\
	MWAWList.cxx			\
	MWAWList.hxx			\
	MWAWOLEParser.cxx		\
	MWAWOLEParser.hxx		\
	MWAWOLEStream.cxx		\
	MWAWOLEStream.hxx		\
	MWAWPageSpan.cxx		\
	MWAWPageSpan.hxx		\
	MWAWParagraph.cxx		\
	MWAWParagraph.hxx		\
	MWAWParser.hxx			\
	MWAWPict.hxx			\
	MWAWPictBasic.cxx		\
	MWAWPictBasic.hxx		\
	MWAWPictBitmap.cxx		\
	MWAWPictBitmap.hxx		\
	MWAWPictData.cxx		\
	MWAWPictData.hxx		\
	MWAWPictMac.cxx			\
	MWAWPictMac.hxx			\
	MWAWPictOLEContainer.cxx	\
	MWAWPictOLEContainer.hxx	\
	MWAWPosition.hxx		\
	MWAWPrinter.cxx			\
	MWAWPrinter.hxx			\
	MWAWPropertyHandler.cxx		\
	MWAWPropertyHandler.hxx		\
	MWAWSubDocument.cxx		\
	MWAWSubDocument.hxx		\
	MWAWTable.cxx			\
	MWAWTable.hxx			\
	MWParser.cxx			\
	MWParser.hxx			\
	MWProParser.cxx			\
	MWProParser.hxx			\
	MWProStructures.cxx		\
	MWProStructures.hxx		\
	WNEntry.hxx			\
	WNParser.cxx			\
	WNParser.hxx			\
	WNText.cxx			\
	WNText.hxx			\
	WPParser.cxx			\
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
