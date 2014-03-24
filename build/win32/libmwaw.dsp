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

SOURCE=..\..\src\lib\BeagleWksBMParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\BeagleWksDRParser.cxx
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

SOURCE=..\..\src\lib\ClarisWksBMParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ClarisWksDatabase.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ClarisWksDbaseContent.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ClarisWksDocument.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ClarisWksGraph.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ClarisWksParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ClarisWksPresentation.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ClarisWksSpreadsheet.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ClarisWksSSParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ClarisWksStruct.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ClarisWksStyleManager.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ClarisWksTable.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ClarisWksText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\DocMkrParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\DocMkrText.cxx
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

SOURCE=..\..\src\lib\GreatWksDocument.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\GreatWksDRParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\GreatWksGraph.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\GreatWksParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\GreatWksBMParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\GreatWksSSParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\GreatWksText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\HanMacWrdJGraph.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\HanMacWrdJParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\HanMacWrdJText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\HanMacWrdKGraph.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\HanMacWrdKParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\HanMacWrdKText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\libmwaw_internal.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\LightWayTxtGraph.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\LightWayTxtParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\LightWayTxtText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MacDocParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MacPaintParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MindWrtParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MoreParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MoreText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MarinerWrtGraph.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MarinerWrtParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MarinerWrtText.cxx
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

SOURCE=..\..\src\lib\MsWksDRParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWksGraph.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWksSSParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWksTable.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWksDocument.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWrd1Parser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWrdParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWrdStruct.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWrdText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWrdTextStyles.cxx
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

SOURCE=..\..\src\lib\MWAWGraphicDecoder.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWGraphicEncoder.cxx
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

SOURCE=..\..\src\lib\NisusWrtGraph.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\NisusWrtParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\NisusWrtStruct.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\NisusWrtText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\SuperPaintParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\TeachTxtParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\WingzParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\WriteNowParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\WriteNowText.cxx
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

SOURCE=..\..\src\lib\BeagleWksBMParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\BeagleWksDRParser.hxx
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

SOURCE=..\..\src\lib\ClarisWksBMParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ClarisWksDatabase.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ClarisWksDbaseContent.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ClarisWksDocument.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ClarisWksGraph.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ClarisWksParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ClarisWksPresentation.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ClarisWksSpreadsheet.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ClarisWksSSParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ClarisWksStruct.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ClarisWksStyleManager.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ClarisWksTable.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\ClarisWksText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\DocMkrParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\DocMkrText.hxx
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

SOURCE=..\..\src\lib\GreatWksDocument.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\GreatWksDRParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\GreatWksGraph.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\GreatWksParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\GreatWksBMParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\GreatWksSSParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\GreatWksText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\HanMacWrdJGraph.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\HanMacWrdJParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\HanMacWrdJText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\HanMacWrdKGraph.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\HanMacWrdKParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\HanMacWrdKText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\inc\libmwaw\libmwaw.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\libmwaw_internal.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\LightWayTxtGraph.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\LightWayTxtParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\LightWayTxtText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MacDocParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MacPaintParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MindWrtParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MoreParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MoreText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MarinerWrtGraph.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MarinerWrtParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MarinerWrtText.hxx
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

SOURCE=..\..\src\lib\MsWksDRParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWksGraph.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWksSSParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWksTable.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWksDocument.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWrd1Parser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWrdParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWrdStruct.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWrdText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MsWrdTextStyles.hxx
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

SOURCE=..\..\src\lib\MWAWGraphicDecoder.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWGraphicEncoder.hxx
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

SOURCE=..\..\src\lib\NisusWrtGraph.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\NisusWrtParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\NisusWrtStruct.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\NisusWrtText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\SuperPaintParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\TeachTxtParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\WingzParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\WriteNowEntry.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\WriteNowParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\WriteNowText.hxx
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
