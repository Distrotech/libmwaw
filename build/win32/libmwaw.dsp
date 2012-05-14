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
# ADD BASE CPP /nologo /W3 /GX /O2 /I "libwpd-0.8" /D "WIN32" /D "NDEBUG" /D "_LIB" /D "_CRT_SECURE_NO_WARNINGS" /YX /FD " /c
# ADD CPP /nologo /MT /W3 /GR /GX /O2 /I "libwpd-0.8" /D "NDEBUG" /D "WIN32" /D "_LIB" /D "_CRT_SECURE_NO_WARNINGS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"Release\lib\libmwaw-0.2.lib"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /I "libwpd-0.8" /D "WIN32" /D "_DEBUG" /D "DEBUG" /D "_LIB" /D "_CRT_SECURE_NO_WARNINGS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /GR /GX /ZI /Od /I "libwpd-0.8" /D "_DEBUG" /D "DEBUG" /D "WIN32" /D "_LIB" /D "_CRT_SECURE_NO_WARNINGS" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"Debug\lib\libmwaw-0.2.lib"

!ENDIF 

# Begin Target

# Name "libmwaw - Win32 Release"
# Name "libmwaw - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\src\lib\CWDatabase.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWGraph.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWSpreadsheet.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWTable.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\DMWAWContentListener.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWOLEStream.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\DMWAWPageSpan.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\FWParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\FWText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWCell.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWContentListener.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWDocument.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWHeader.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWList.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWSubDocument.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWTable.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWFont.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWProParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWProStructures.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MSKGraph.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MSKParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MSKText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MSWParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MSWText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWDebug.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWFontConverter.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWInputStream.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWOLEParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWPictBasic.cxx
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

SOURCE=..\..\src\lib\MWAWPictOLEContainer.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWPrinter.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWPropertyHandler.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\WNParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\WNText.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\WPParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\libmwaw_internal.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\libmwaw_libwpd.cxx
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\src\lib\CWDatabase.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWGraph.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWSpreadsheet.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWStruct.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWTable.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\CWText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\DMWAWContentListener.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWOLEStream.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\DMWAWPageSpan.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\FWParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\FWText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWCell.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWContentListener.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWDocument.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWEntry.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWHeader.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWList.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWSubDocument.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWTable.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWFont.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWProParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWProStructures.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MSKGraph.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MSKParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MSKText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MSWParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MSWText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWDebug.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWFontConverter.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWInputStream.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWOLEParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWPict.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWPictBasic.hxx
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

SOURCE=..\..\src\lib\MWAWPictOLEContainer.hxx
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

SOURCE=..\..\src\lib\WNEntry.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\WNParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\WNText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\WPParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\libmwaw.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\libmwaw_internal.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\libmwaw_libwpd.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\libmwaw_tools.hxx
# End Source File
# End Group
# End Target
# End Project
