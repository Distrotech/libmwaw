# Microsoft Developer Studio Project File - Name="libmwaw" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libmwaw - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libmwaw.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libmwaw.mak" CFG="libmwaw - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libmwaw - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libmwaw - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libmwaw - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /I "..\..\inc" /D "WIN32" /D "NDEBUG" /D "_LIB" /D "_CRT_SECURE_NO_WARNINGS" /YX /FD " /c
# ADD CPP /nologo /MT /W3 /GR /GX /O2 /I "..\..\inc" /D "NDEBUG" /D "WIN32" /D "_LIB" /D "_CRT_SECURE_NO_WARNINGS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"Release\lib\libmwaw-0.3.lib"

!ELSEIF  "$(CFG)" == "libmwaw - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /I "..\..\inc" /D "WIN32" /D "_DEBUG" /D "DEBUG" /D "_LIB" /D "_CRT_SECURE_NO_WARNINGS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /GR /GX /ZI /Od /I "..\..\inc" /D "_DEBUG" /D "DEBUG" /D "WIN32" /D "_LIB" /D "_CRT_SECURE_NO_WARNINGS" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"Debug\lib\libmwaw-0.3.lib"

!ENDIF 

# Begin Target

# Name "libmwaw - Win32 Release"
# Name "libmwaw - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\src\lib\ActaParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ActaText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\BeagleWksParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\BeagleWksSSParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\BeagleWksStructManager.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\BeagleWksText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWDatabase.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWDbaseContent.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWGraph.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWPresentation.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWSpreadsheet.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWStruct.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWStyleManager.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWTable.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\DMParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\DMText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\EDocParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\FullWrtGraph.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\FullWrtParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\FullWrtStruct.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\FullWrtText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\GWGraph.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\GWParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\GWText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\HMWJGraph.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\HMWJParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\HMWJText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\HMWKGraph.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\HMWKParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\HMWKText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\libmwaw_internal.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\LWGraph.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\LWParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\LWText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MacDocParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MDWParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MORParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MORText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MRWGraph.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MRWParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MRWText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWks3Parser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWks3Text.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWks4Parser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWks4Text.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWks4Zone.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWksGraph.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWksParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWksTable.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MSW1Parser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MSWParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MSWStruct.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MSWText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MSWTextStyles.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWCell.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWChart.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWDebug.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWDocument.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWFont.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWFontConverter.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWFontSJISConverter.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWGraphicInterface.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWGraphicListener.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWGraphicShape.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWGraphicStyle.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWHeader.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWInputStream.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWList.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWOLEParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWPageSpan.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWParagraph.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWPictBitmap.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWPictData.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWPictMac.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWPrinter.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWPropertyHandler.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWRSRCParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWSection.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWSpreadsheetListener.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWSubDocument.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWTable.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWTextListener.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MacWrtParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MacWrtProParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MacWrtProStructures.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\NSGraph.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\NSParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\NSStruct.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\NSText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\TTParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\WNParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\WNText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\WriterPlsParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ZWrtParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ZWrtText.cxx
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\src\lib\ActaParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ActaText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\BeagleWksParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\BeagleWksSSParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\BeagleWksStructManager.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\BeagleWksText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWDatabase.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWDbaseContent.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWGraph.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWPresentation.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWSpreadsheet.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWStruct.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWStyleManager.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWTable.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\DMParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\DMText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\EDocParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\FullWrtGraph.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\FullWrtParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\FullWrtStruct.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\FullWrtText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\GWGraph.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\GWParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\GWText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\HMWJGraph.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\HMWJParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\HMWJText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\HMWKGraph.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\HMWKParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\HMWKText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\inc\libmwaw\libmwaw.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\libmwaw_internal.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\LWGraph.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\LWParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\LWText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MacDocParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MDWParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MORParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MORText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MRWGraph.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MRWParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MRWText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWks3Parser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWks3Text.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWks4Parser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWks4Text.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWks4Zone.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWksGraph.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWksParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWksTable.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MSW1Parser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MSWParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MSWStruct.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MSWText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MSWTextStyles.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWCell.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWChart.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWDebug.hxx
# End Source File
# Begin Source File

SOURCE=..\..\inc\libmwaw\MWAWDocument.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWEntry.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWFont.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWFontConverter.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWFontSJISConverter.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWGraphicInterface.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWGraphicListener.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWGraphicShape.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWGraphicStyle.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWHeader.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWInputStream.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWList.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWListener.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWOLEParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWPageSpan.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWParagraph.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWPict.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWPictBitmap.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWPictData.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWPictMac.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWPosition.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWPrinter.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWPropertyHandler.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWRSRCParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWSection.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWSpreadsheetListener.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWSubDocument.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWTable.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWTextListener.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MacWrtParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MacWrtProParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MacWrtProStructures.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\NSGraph.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\NSParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\NSStruct.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\NSText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\TTParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\WNEntry.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\WNParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\WNText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\WriterPlsParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ZWrtParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ZWrtText.hxx
# End Source File
# End Group
# End Target
# End Project
