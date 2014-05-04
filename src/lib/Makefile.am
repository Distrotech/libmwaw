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
	BeagleWksBMParser.cxx		\
	BeagleWksBMParser.hxx		\
	BeagleWksDBParser.cxx		\
	BeagleWksDBParser.hxx		\
	BeagleWksDRParser.cxx		\
	BeagleWksDRParser.hxx		\
	BeagleWksSSParser.cxx		\
	BeagleWksSSParser.hxx		\
	BeagleWksStructManager.cxx	\
	BeagleWksStructManager.hxx	\
	BeagleWksText.cxx		\
	BeagleWksText.hxx		\
	ClarisWksBMParser.cxx		\
	ClarisWksBMParser.hxx		\
	ClarisWksDatabase.cxx		\
	ClarisWksDatabase.hxx		\
	ClarisWksDbaseContent.cxx	\
	ClarisWksDbaseContent.hxx	\
	ClarisWksDocument.cxx		\
	ClarisWksDocument.hxx		\
	ClarisWksGraph.cxx		\
	ClarisWksGraph.hxx		\
	ClarisWksParser.cxx		\
	ClarisWksParser.hxx		\
	ClarisWksPresentation.cxx	\
	ClarisWksPresentation.hxx	\
	ClarisWksSpreadsheet.cxx	\
	ClarisWksSpreadsheet.hxx	\
	ClarisWksSSParser.cxx		\
	ClarisWksSSParser.hxx		\
	ClarisWksStruct.cxx		\
	ClarisWksStruct.hxx		\
	ClarisWksStyleManager.cxx	\
	ClarisWksStyleManager.hxx	\
	ClarisWksTable.cxx		\
	ClarisWksTable.hxx		\
	ClarisWksText.cxx		\
	ClarisWksText.hxx		\
	DocMkrParser.cxx		\
	DocMkrParser.hxx		\
	DocMkrText.cxx			\
	DocMkrText.hxx			\
	EDocParser.cxx			\
	EDocParser.hxx			\
	FullWrtGraph.cxx		\
	FullWrtGraph.hxx		\
	FullWrtParser.cxx		\
	FullWrtParser.hxx		\
	FullWrtStruct.cxx		\
	FullWrtStruct.hxx		\
	FullWrtText.cxx			\
	FullWrtText.hxx			\
	GreatWksBMParser.cxx		\
	GreatWksBMParser.hxx		\
	GreatWksDBParser.cxx		\
	GreatWksDBParser.hxx		\
	GreatWksDocument.cxx		\
	GreatWksDocument.hxx		\
	GreatWksDRParser.cxx		\
	GreatWksDRParser.hxx		\
	GreatWksGraph.cxx		\
	GreatWksGraph.hxx		\
	GreatWksParser.cxx		\
	GreatWksParser.hxx		\
	GreatWksSSParser.cxx		\
	GreatWksSSParser.hxx		\
	GreatWksText.cxx		\
	GreatWksText.hxx		\
	HanMacWrdJGraph.cxx		\
	HanMacWrdJGraph.hxx		\
	HanMacWrdJParser.cxx		\
	HanMacWrdJParser.hxx		\
	HanMacWrdJText.cxx		\
	HanMacWrdJText.hxx		\
	HanMacWrdKGraph.cxx		\
	HanMacWrdKGraph.hxx		\
	HanMacWrdKParser.cxx		\
	HanMacWrdKParser.hxx		\
	HanMacWrdKText.cxx		\
	HanMacWrdKText.hxx		\
	LightWayTxtGraph.cxx		\
	LightWayTxtGraph.hxx		\
	LightWayTxtParser.cxx		\
	LightWayTxtParser.hxx		\
	LightWayTxtText.cxx		\
	LightWayTxtText.hxx		\
	MacDocParser.cxx		\
	MacDocParser.hxx		\
	MacPaintParser.cxx		\
	MacPaintParser.hxx		\
	MacWrtParser.cxx		\
	MacWrtParser.hxx		\
	MacWrtProParser.cxx		\
	MacWrtProParser.hxx		\
	MacWrtProStructures.cxx		\
	MacWrtProStructures.hxx		\
	MarinerWrtGraph.cxx		\
	MarinerWrtGraph.hxx		\
	MarinerWrtParser.cxx		\
	MarinerWrtParser.hxx		\
	MarinerWrtText.cxx		\
	MarinerWrtText.hxx		\
	MindWrtParser.cxx		\
	MindWrtParser.hxx		\
	MoreParser.cxx			\
	MoreParser.hxx			\
	MoreText.cxx			\
	MoreText.hxx			\
	MsWksDocument.cxx		\
	MsWksDocument.hxx		\
	MsWksDBParser.cxx		\
	MsWksDBParser.hxx		\
	MsWksDRParser.cxx		\
	MsWksDRParser.hxx		\
	MsWksGraph.cxx			\
	MsWksGraph.hxx			\
	MsWksParser.cxx			\
	MsWksParser.hxx			\
	MsWksSSParser.cxx		\
	MsWksSSParser.hxx		\
	MsWksTable.cxx			\
	MsWksTable.hxx			\
	MsWks3Text.cxx			\
	MsWks3Text.hxx			\
	MsWks4Text.cxx			\
	MsWks4Text.hxx			\
	MsWks4Zone.cxx			\
	MsWks4Zone.hxx			\
	MsWrd1Parser.cxx		\
	MsWrd1Parser.hxx		\
	MsWrdParser.cxx			\
	MsWrdParser.hxx			\
	MsWrdStruct.cxx			\
	MsWrdStruct.hxx			\
	MsWrdText.cxx			\
	MsWrdText.hxx			\
	MsWrdTextStyles.cxx		\
	MsWrdTextStyles.hxx		\
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
	MWAWGraphicDecoder.cxx		\
	MWAWGraphicDecoder.hxx		\
	MWAWGraphicEncoder.cxx		\
	MWAWGraphicEncoder.hxx		\
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
	NisusWrtGraph.cxx		\
	NisusWrtGraph.hxx		\
	NisusWrtParser.cxx		\
	NisusWrtParser.hxx		\
	NisusWrtStruct.cxx		\
	NisusWrtStruct.hxx		\
	NisusWrtText.cxx		\
	NisusWrtText.hxx		\
	RagTimeParser.cxx		\
	RagTimeParser.hxx		\
	SuperPaintParser.cxx		\
	SuperPaintParser.hxx		\
	TeachTxtParser.cxx		\
	TeachTxtParser.hxx		\
	WingzParser.cxx			\
	WingzParser.hxx			\
	WriteNowEntry.hxx		\
	WriteNowParser.cxx		\
	WriteNowParser.hxx		\
	WriteNowText.cxx		\
	WriteNowText.hxx		\
	WriterPlsParser.cxx		\
	WriterPlsParser.hxx		\
	ZWrtParser.cxx			\
	ZWrtParser.hxx			\
	ZWrtText.cxx			\
	ZWrtText.hxx

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
