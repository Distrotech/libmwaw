EXTERNAL_WARNINGS_NOT_ERRORS := TRUE

PRJ=..$/..$/..$/..$/..$/..

PRJNAME=libmwaw
TARGET=mwawlib
ENABLE_EXCEPTIONS=TRUE
LIBTARGET=NO

.INCLUDE :  settings.mk

.IF "$(GUI)$(COM)"=="WNTMSC"
CFLAGS+=-GR
.ENDIF
.IF "$(COM)"=="GCC"
CFLAGSCXX+=-frtti
.ENDIF

.IF "$(SYSTEM_LIBWPD)" == "YES"
INCPRE+=$(WPD_CFLAGS) -I..
.ELSE
INCPRE+=$(SOLARVER)$/$(UPD)$/$(INPATH)$/inc$/libwpd
.ENDIF

SLOFILES= \
	$(SLO)$/CWDatabase.obj			\
	$(SLO)$/CWGraph.obj			\
	$(SLO)$/CWParser.obj			\
	$(SLO)$/CWSpreadsheet.obj		\
	$(SLO)$/CWTable.obj			\
	$(SLO)$/CWText.obj			\
	$(SLO)$/MWAWOLEStream.obj		\
	$(SLO)$/MWAWPageSpan.obj		\
	$(SLO)$/FWParser.obj			\
	$(SLO)$/FWText.obj			\
	$(SLO)$/MWAWCell.obj			\
	$(SLO)$/MWAWContentListener.obj		\
	$(SLO)$/MWAWDocument.obj		\
	$(SLO)$/MWAWHeader.obj			\
	$(SLO)$/MWAWList.obj			\
	$(SLO)$/MWAWParagraph.obj		\
	$(SLO)$/MWAWTable.obj			\
	$(SLO)$/MWAWFont.obj			\
	$(SLO)$/MWParser.obj			\
	$(SLO)$/MWProParser.obj			\
	$(SLO)$/MWProStructures.obj		\
	$(SLO)$/MSKGraph.obj			\
	$(SLO)$/MSKParser.obj			\
	$(SLO)$/MSKText.obj			\
	$(SLO)$/MSWParser.obj			\
	$(SLO)$/MSWStruct.obj			\
	$(SLO)$/MSWText.obj			\
	$(SLO)$/MSWTextStles.obj		\
	$(SLO)$/MWAWDebug.obj			\
	$(SLO)$/MWAWFontConverter.obj		\
	$(SLO)$/MWAWInputStream.obj		\
	$(SLO)$/MWAWOLEParser.obj		\
	$(SLO)$/MWAWPictBasic.obj		\
	$(SLO)$/MWAWPictBitmap.obj		\
	$(SLO)$/MWAWPictData.obj		\
	$(SLO)$/MWAWPictMac.obj			\
	$(SLO)$/MWAWPictOLEContainer.obj	\
	$(SLO)$/MWAWPrinter.obj			\
	$(SLO)$/MWAWPropertyHandler.obj		\
	$(SLO)$/MWAWSubDocument.obj		\
	$(SLO)$/WNParser.obj			\
	$(SLO)$/WNText.obj			\
	$(SLO)$/WPParser.obj			\
	$(SLO)$/libmwaw_internal.obj

LIB1ARCHIV=$(LB)$/libmwawlib.a
LIB1TARGET=$(SLB)$/$(TARGET).lib
LIB1OBJFILES= $(SLOFILES)

.INCLUDE :  target.mk
