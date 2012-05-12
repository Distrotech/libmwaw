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
	libmwaw_libwpd.cxx		\
	DMWAWContentListener.cxx	\
	DMWAWOLEStream.cxx		\
	DMWAWPageSpan.cxx		\
	libmwaw_libwpd.hxx		\
	libmwaw_libwpd_math.hxx		\
	libmwaw_libwpd_types.hxx	\
	DMWAWContentListener.hxx	\
	DMWAWOLEStream.hxx		\
	DMWAWPageSpan.hxx		\
					\
	libmwaw_internal.cxx		\
	MWAWCell.cxx			\
	MWAWContentListener.cxx	\
	MWAWDocument.cxx		\
	MWAWHeader.cxx			\
	MWAWList.cxx			\
	MWAWTableHelper.cxx		\
	libmwaw_internal.hxx		\
	MWAWCell.hxx			\
	MWAWContentListener.hxx	\
	MWAWDocument.hxx		\
	MWAWEntry.hxx			\
	MWAWHeader.hxx			\
	MWAWList.hxx			\
	MWAWParser.hxx			\
	MWAWSubDocument.hxx		\
	MWAWTableHelper.hxx		\
					\
	MWAWDebug.cxx			\
	MWAWInputStream.cxx		\
	TMWAWFont.cxx			\
	MWAWPrinter.cxx			\
	TMWAWOleParser.cxx		\
	MWAWPictBasic.cxx		\
	MWAWPictBitmap.cxx		\
	MWAWPictData.cxx		\
	MWAWPictMac.cxx		\
	MWAWPictOLEContainer.cxx	\
	MWAWPropertyHandler.cxx	\
	libmwaw_tools.hxx		\
	MWAWDebug.hxx			\
	MWAWInputStream.hxx		\
	TMWAWFont.hxx			\
	MWAWPrinter.hxx			\
	TMWAWOleParser.hxx		\
	MWAWPict.hxx			\
	MWAWPictBasic.hxx		\
	MWAWPictBitmap.hxx		\
	MWAWPictData.hxx		\
	MWAWPictMac.hxx		\
	MWAWPictOLEContainer.hxx	\
	MWAWPosition.hxx		\
	MWAWPropertyHandler.hxx	\
					\
	MWAWStruct.cxx			\
	MWAWTools.cxx			\
	libmwaw.hxx			\
	MWAWStruct.hxx			\
	MWAWTools.hxx			\
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
