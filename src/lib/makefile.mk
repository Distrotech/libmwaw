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
	$(SLO)$/DMWAWEncryption.cxx		\
	$(SLO)$/DMWAWListener.cxx		\
	$(SLO)$/DMWAWMemoryStream.cxx		\
	$(SLO)$/DMWAWOLEStream.cxx		\
	$(SLO)$/DMWAWPageSpan.cxx		\
	$(SLO)$/DMWAWSubDocument.cxx		\
	$(SLO)$/DMWAWTable.cxx			\
	$(SLO)$/IMWAWCell.cxx			\
	$(SLO)$/IMWAWContentListener.cxx	\
	$(SLO)$/IMWAWDocument.cxx		\
	$(SLO)$/IMWAWHeader.cxx			\
	$(SLO)$/IMWAWList.cxx			\
	$(SLO)$/IMWAWTableHelper.cxx		\
	$(SLO)$/IMWAWTextParser.cxx		\
	$(SLO)$/MWAWContentListener.cxx		\
	$(SLO)$/MWAWStruct.cxx			\
	$(SLO)$/MWAWTools.cxx			\
	$(SLO)$/MWParser.cxx			\
	$(SLO)$/TMWAWDebug.cxx			\
	$(SLO)$/TMWAWFont.cxx			\
	$(SLO)$/TMWAWInputStream.cxx		\
	$(SLO)$/TMWAWOleParser.cxx		\
	$(SLO)$/TMWAWPictBasic.cxx		\
	$(SLO)$/TMWAWPictBitmap.cxx		\
	$(SLO)$/TMWAWPictData.cxx		\
	$(SLO)$/TMWAWPictMac.cxx		\
	$(SLO)$/TMWAWPictOleContainer.cxx	\
	$(SLO)$/TMWAWPrint.cxx			\
	$(SLO)$/TMWAWPropertyHandler.cxx	\
	$(SLO)$/WNParser.cxx			\
	$(SLO)$/WNText.cxx			\
	$(SLO)$/WPParser.cxx			\
	$(SLO)$/libmwaw_internal.cxx		\
	$(SLO)$/libmwaw_libwpd.cxx

LIB1ARCHIV=$(LB)$/libmwawlib.a
LIB1TARGET=$(SLB)$/$(TARGET).lib
LIB1OBJFILES= $(SLOFILES)

.INCLUDE :  target.mk
