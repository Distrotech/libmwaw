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
	$(SLO)$/CWDatabase.cxx			\
	$(SLO)$/CWGraph.cxx			\
	$(SLO)$/CWParser.cxx			\
	$(SLO)$/CWSpreadsheet.cxx		\
	$(SLO)$/CWTable.cxx			\
	$(SLO)$/CWText.cxx			\
	$(SLO)$/DMWAWContentListener.cxx	\
	$(SLO)$/MWAWOLEStream.cxx		\
	$(SLO)$/MWAWPageSpan.cxx		\
	$(SLO)$/FWParser.cxx			\
	$(SLO)$/FWText.cxx			\
	$(SLO)$/MWAWCell.cxx			\
	$(SLO)$/MWAWContentListener.cxx		\
	$(SLO)$/MWAWDocument.cxx		\
	$(SLO)$/MWAWHeader.cxx			\
	$(SLO)$/MWAWList.cxx			\
	$(SLO)$/MWAWTable.cxx			\
	$(SLO)$/MWAWFont.cxx			\
	$(SLO)$/MWParser.cxx			\
	$(SLO)$/MWProParser.cxx			\
	$(SLO)$/MWProStructures.cxx		\
	$(SLO)$/MSKGraph.cxx			\
	$(SLO)$/MSKParser.cxx			\
	$(SLO)$/MSKText.cxx			\
	$(SLO)$/MSWParser.cxx			\
	$(SLO)$/MSWText.cxx			\
	$(SLO)$/MWAWDebug.cxx			\
	$(SLO)$/MWAWFontConverter.cxx			\
	$(SLO)$/MWAWInputStream.cxx		\
	$(SLO)$/MWAWOLEParser.cxx		\
	$(SLO)$/MWAWPictBasic.cxx		\
	$(SLO)$/MWAWPictBitmap.cxx		\
	$(SLO)$/MWAWPictData.cxx		\
	$(SLO)$/MWAWPictMac.cxx			\
	$(SLO)$/MWAWPictOLEContainer.cxx	\
	$(SLO)$/MWAWPrinter.cxx			\
	$(SLO)$/MWAWPropertyHandler.cxx		\
	$(SLO)$/MWAWSubDocument.cxx		\
	$(SLO)$/WNParser.cxx			\
	$(SLO)$/WNText.cxx			\
	$(SLO)$/WPParser.cxx			\
	$(SLO)$/libmwaw_internal.cxx

LIB1ARCHIV=$(LB)$/libmwawlib.a
LIB1TARGET=$(SLB)$/$(TARGET).lib
LIB1OBJFILES= $(SLOFILES)

.INCLUDE :  target.mk
