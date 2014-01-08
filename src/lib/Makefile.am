if OS_WIN32
version_info = -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) -avoid-version
else
version_info = -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)
endif

lib_LTLIBRARIES = libmwaw-@MWAW_MAJOR_VERSION@.@MWAW_MINOR_VERSION@.la $(target_libmwaw_stream)
libmwaw_@MWAW_MAJOR_VERSION@_@MWAW_MINOR_VERSION@_includedir = $(includedir)/libmwaw-@MWAW_MAJOR_VERSION@.@MWAW_MINOR_VERSION@/libmwaw
libmwaw_@MWAW_MAJOR_VERSION@_@MWAW_MINOR_VERSION@_include_HEADERS = \
	$(top_srcdir)/inc/libmwaw/libmwaw.hxx \
	$(top_srcdir)/inc/libmwaw/MWAWDocument.hxx

AM_CXXFLAGS = -I$(top_srcdir)/inc $(REVENGE_CFLAGS) $(DEBUG_CXXFLAGS)

libmwaw_@MWAW_MAJOR_VERSION@_@MWAW_MINOR_VERSION@_la_LIBADD  = $(REVENGE_LIBS) @LIBMWAW_WIN32_RESOURCE@
libmwaw_@MWAW_MAJOR_VERSION@_@MWAW_MINOR_VERSION@_la_DEPENDENCIES = @LIBMWAW_WIN32_RESOURCE@  
libmwaw_@MWAW_MAJOR_VERSION@_@MWAW_MINOR_VERSION@_la_LDFLAGS = $(version_info) -export-dynamic  -no-undefined
libmwaw_@MWAW_MAJOR_VERSION@_@MWAW_MINOR_VERSION@_la_SOURCES = \
	libmwaw_internal.cxx		\
	libmwaw_internal.hxx		\
	ActaParser.cxx			\
	ActaParser.hxx			\
	ActaText.cxx			\
	ActaText.hxx			\
	BeagleWksParser.cxx		\
	BeagleWksParser.hxx		\
	BeagleWksSSParser.cxx		\
	BeagleWksSSParser.hxx		\
	BeagleWksStructManager.cxx	\
	BeagleWksStructManager.hxx	\
	BeagleWksText.cxx		\
	BeagleWksText.hxx		\
	CWDatabase.cxx			\
	CWDatabase.hxx			\
	CWDbaseContent.cxx		\
	CWDbaseContent.hxx		\
	CWGraph.cxx			\
	CWGraph.hxx			\
	CWParser.cxx			\
	CWParser.hxx			\
	CWPresentation.cxx		\
	CWPresentation.hxx		\
	CWSpreadsheet.cxx		\
	CWSpreadsheet.hxx		\
	CWStruct.cxx			\
	CWStruct.hxx			\
	CWStyleManager.cxx		\
	CWStyleManager.hxx		\
	CWTable.cxx			\
	CWTable.hxx			\
	CWText.cxx			\
	CWText.hxx			\
	DMParser.cxx			\
	DMParser.hxx			\
	DMText.cxx			\
	DMText.hxx			\
	EDParser.cxx			\
	EDParser.hxx			\
	FullWrtGraph.cxx		\
	FullWrtGraph.hxx		\
	FullWrtParser.cxx		\
	FullWrtParser.hxx		\
	FullWrtStruct.cxx		\
	FullWrtStruct.hxx		\
	FullWrtText.cxx			\
	FullWrtText.hxx			\
	GWGraph.cxx			\
	GWGraph.hxx			\
	GWParser.cxx			\
	GWParser.hxx			\
	GWText.cxx			\
	GWText.hxx			\
	HMWJGraph.cxx			\
	HMWJGraph.hxx			\
	HMWJParser.cxx			\
	HMWJParser.hxx			\
	HMWJText.cxx			\
	HMWJText.hxx			\
	HMWKGraph.cxx			\
	HMWKGraph.hxx			\
	HMWKParser.cxx			\
	HMWKParser.hxx			\
	HMWKText.cxx			\
	HMWKText.hxx			\
	LWGraph.cxx			\
	LWGraph.hxx			\
	LWParser.cxx			\
	LWParser.hxx			\
	LWText.cxx			\
	LWText.hxx			\
	MacWrtParser.cxx		\
	MacWrtParser.hxx		\
	MacWrtProParser.cxx		\
	MacWrtProParser.hxx		\
	MacWrtProStructures.cxx		\
	MacWrtProStructures.hxx		\
	MsWksGraph.cxx			\
	MsWksGraph.hxx			\
	MsWksParser.cxx			\
	MsWksParser.hxx			\
	MsWksTable.cxx			\
	MsWksTable.hxx			\
	MsWks3Parser.cxx		\
	MsWks3Parser.hxx		\
	MsWks3Text.cxx			\
	MsWks3Text.hxx			\
	MsWks4Parser.cxx		\
	MsWks4Parser.hxx		\
	MsWks4Text.cxx			\
	MsWks4Text.hxx			\
	MsWks4Zone.cxx			\
	MsWks4Zone.hxx			\
	MCDParser.cxx			\
	MCDParser.hxx			\
	MDWParser.cxx			\
	MDWParser.hxx			\
	MORParser.cxx			\
	MORParser.hxx			\
	MORText.cxx			\
	MORText.hxx			\
	MRWGraph.cxx			\
	MRWGraph.hxx			\
	MRWParser.cxx			\
	MRWParser.hxx			\
	MRWText.cxx			\
	MRWText.hxx			\
	MSW1Parser.cxx			\
	MSW1Parser.hxx			\
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
	MWAWChart.cxx			\
	MWAWChart.hxx			\
	MWAWDebug.cxx			\
	MWAWDebug.hxx			\
	MWAWDocument.cxx		\
	MWAWEntry.hxx			\
	MWAWFont.cxx			\
	MWAWFont.hxx			\
	MWAWFontConverter.cxx		\
	MWAWFontConverter.hxx		\
	MWAWFontSJISConverter.cxx	\
	MWAWFontSJISConverter.hxx	\
	MWAWGraphicInterface.cxx	\
	MWAWGraphicInterface.hxx	\
	MWAWGraphicListener.cxx		\
	MWAWGraphicListener.hxx		\
	MWAWGraphicShape.cxx		\
	MWAWGraphicShape.hxx		\
	MWAWGraphicStyle.cxx		\
	MWAWGraphicStyle.hxx		\
	MWAWHeader.cxx			\
	MWAWHeader.hxx			\
	MWAWInputStream.cxx		\
	MWAWInputStream.hxx		\
	MWAWList.cxx			\
	MWAWList.hxx			\
	MWAWListener.hxx		\
	MWAWOLEParser.cxx		\
	MWAWOLEParser.hxx		\
	MWAWPageSpan.cxx		\
	MWAWPageSpan.hxx		\
	MWAWParagraph.cxx		\
	MWAWParagraph.hxx		\
	MWAWParser.cxx			\
	MWAWParser.hxx			\
	MWAWPict.hxx			\
	MWAWPictBitmap.cxx		\
	MWAWPictBitmap.hxx		\
	MWAWPictData.cxx		\
	MWAWPictData.hxx		\
	MWAWPictMac.cxx			\
	MWAWPictMac.hxx			\
	MWAWPosition.hxx		\
	MWAWPrinter.cxx			\
	MWAWPrinter.hxx			\
	MWAWPropertyHandler.cxx		\
	MWAWPropertyHandler.hxx		\
	MWAWRSRCParser.cxx		\
	MWAWRSRCParser.hxx		\
	MWAWSection.cxx			\
	MWAWSection.hxx			\
	MWAWSpreadsheetListener.cxx	\
	MWAWSpreadsheetListener.hxx	\
	MWAWSubDocument.cxx		\
	MWAWSubDocument.hxx		\
	MWAWTable.cxx			\
	MWAWTable.hxx			\
	MWAWTextListener.cxx		\
	MWAWTextListener.hxx		\
	NSGraph.cxx			\
	NSGraph.hxx			\
	NSParser.cxx			\
	NSParser.hxx			\
	NSStruct.cxx			\
	NSStruct.hxx			\
	NSText.cxx			\
	NSText.hxx			\
	TTParser.cxx			\
	TTParser.hxx			\
	WNEntry.hxx			\
	WNParser.cxx			\
	WNParser.hxx			\
	WNText.cxx			\
	WNText.hxx			\
	WPParser.cxx			\
	WPParser.hxx			\
	ZWParser.cxx			\
	ZWParser.hxx			\
	ZWText.cxx			\
	ZWText.hxx

if OS_WIN32

@LIBMWAW_WIN32_RESOURCE@ : libmwaw.rc $(libmwaw_@MWAW_MAJOR_VERSION@_@MWAW_MINOR_VERSION@_la_OBJECTS)
	chmod +x $(top_srcdir)/build/win32/*compile-resource
	WINDRES=@WINDRES@ $(top_srcdir)/build/win32/lt-compile-resource libmwaw.rc @LIBMWAW_WIN32_RESOURCE@

endif

EXTRA_DIST = \
	libmwaw.rc.in

# These may be in the builddir too
BUILD_EXTRA_DIST = \
	libmwaw.rc
