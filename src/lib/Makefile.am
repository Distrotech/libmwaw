if OS_WIN32
version_info = -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) -avoid-version
else
version_info = -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)
endif

lib_LTLIBRARIES = libmwaw-@MWAW_MAJOR_VERSION@.@MWAW_MINOR_VERSION@.la $(target_libmwaw_stream)
libmwaw_@MWAW_MAJOR_VERSION@_@MWAW_MINOR_VERSION@_includedir = $(includedir)/libmwaw-@MWAW_MAJOR_VERSION@.@MWAW_MINOR_VERSION@/libmwaw
libmwaw_@MWAW_MAJOR_VERSION@_@MWAW_MINOR_VERSION@_include_HEADERS = libmwaw.hxx IMWAWDocument.hxx 
##TMWAWPropertyHandler.hxx 

INCLUDES = -I$(srcdir) -I..

AM_CXXFLAGS = $(LIBMWAW_CXXFLAGS) $(DEBUG_CXXFLAGS)

libmwaw_@MWAW_MAJOR_VERSION@_@MWAW_MINOR_VERSION@_la_LIBADD  = $(LIBMWAW_LIBS) @LIBMWAW_WIN32_RESOURCE@
libmwaw_@MWAW_MAJOR_VERSION@_@MWAW_MINOR_VERSION@_la_DEPENDENCIES = @LIBMWAW_WIN32_RESOURCE@  
libmwaw_@MWAW_MAJOR_VERSION@_@MWAW_MINOR_VERSION@_la_LDFLAGS = $(version_info) -export-dynamic  -no-undefined
libmwaw_@MWAW_MAJOR_VERSION@_@MWAW_MINOR_VERSION@_la_SOURCES = \
	libmwaw_libwpd.cxx		\
	DMWAWContentListener.cxx		\
	DMWAWEncryption.cxx		\
	DMWAWListener.cxx		\
	DMWAWMemoryStream.cxx		\
	DMWAWOLEStream.cxx		\
	DMWAWPageSpan.cxx		\
	DMWAWSubDocument.cxx		\
	DMWAWTable.cxx			\
	libmwaw_libwpd.hxx		\
	libmwaw_libwpd_math.hxx		\
	libmwaw_libwpd_types.hxx	\
	DMWAWContentListener.hxx	\
	DMWAWEncryption.h		\
	DMWAWListener.hxx		\
	DMWAWMemoryStream.hxx		\
	DMWAWOLEStream.h		\
	DMWAWPageSpan.hxx		\
	DMWAWSubDocument.hxx		\
	DMWAWTable.h			\
	DMWAWFileStructure.hxx		\
	libmwaw_internal.cxx		\
	IMWAWCell.cxx			\
	IMWAWContentListener.cxx	\
	IMWAWDocument.cxx		\
	IMWAWHeader.cxx			\
	IMWAWList.cxx			\
	IMWAWTableHelper.cxx		\
	IMWAWTextParser.cxx		\
	libmwaw_internal.hxx		\
	IMWAWCell.hxx			\
	IMWAWContentListener.hxx	\
	IMWAWDocument.hxx		\
	IMWAWEntry.hxx			\
	IMWAWHeader.hxx			\
	IMWAWList.hxx			\
	IMWAWParser.hxx			\
	IMWAWSubDocument.hxx		\
	IMWAWTableHelper.hxx		\
	IMWAWTextParser.hxx		\
	TMWAWDebug.cxx			\
	TMWAWInputStream.cxx		\
	TMWAWFont.cxx			\
	TMWAWPrint.cxx			\
	TMWAWOleParser.cxx		\
	TMWAWPictBasic.cxx		\
	TMWAWPictBitmap.cxx		\
	TMWAWPictData.cxx		\
	TMWAWPictMac.cxx		\
	TMWAWPictOleContainer.cxx	\
	TMWAWPropertyHandler.cxx	\
	libmwaw_tools.hxx		\
	TMWAWContentListener.hxx	\
	TMWAWDebug.hxx			\
	TMWAWInputStream.hxx		\
	TMWAWFont.hxx			\
	TMWAWPrint.hxx			\
	TMWAWOleParser.hxx		\
	TMWAWPict.hxx			\
	TMWAWPictBasic.hxx		\
	TMWAWPictBitmap.hxx		\
	TMWAWPictData.hxx		\
	TMWAWPictMac.hxx		\
	TMWAWPictOleContainer.hxx	\
	TMWAWPosition.hxx		\
	TMWAWPropertyHandler.hxx	\
	MWAWContentListener.cxx		\
	MWAWStruct.cxx			\
	MWAWTools.cxx			\
	CWDatabase.cxx			\
	CWGraph.cxx			\
	CWParser.cxx			\
	CWSpreadsheet.cxx		\
	CWTable.cxx			\
	CWText.cxx			\
	MWParser.cxx			\
	MWProParser.cxx			\
	MWProStructures.cxx		\
	MSWParser.cxx			\
	MSWText.cxx			\
	WNParser.cxx			\
	WNText.cxx			\
	WPParser.cxx			\
	libmwaw.hxx			\
	MWAWContentListener.hxx		\
	MWAWStruct.hxx			\
	MWAWTools.hxx			\
	CWDatabase.hxx			\
	CWGraph.hxx			\
	CWParser.hxx			\
	CWSpreadsheet.hxx		\
	CWStruct.hxx			\
	CWTable.hxx			\
	CWText.hxx			\
	MWParser.hxx			\
	MWProParser.hxx			\
	MWProStructures.hxx		\
	MSWParser.hxx			\
	MSWText.hxx			\
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
