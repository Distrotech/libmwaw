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

SOURCE=..\..\src\lib\DMWAWEncryption.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\DMWAWListener.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\DMWAWMemoryStream.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\DMWAWOLEStream.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\DMWAWPageSpan.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\DMWAWSubDocument.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\DMWAWTable.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\IMWAWCell.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\IMWAWContentListener.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\IMWAWDocument.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\IMWAWHeader.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\IMWAWList.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\IMWAWTableHelper.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\IMWAWTextParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWContentListener.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWStruct.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWTools.cxx
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

SOURCE=..\..\src\lib\TMWAWDebug.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\TMWAWFont.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\TMWAWInputStream.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\TMWAWOleParser.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\TMWAWPictBasic.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\TMWAWPictBitmap.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\TMWAWPictData.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\TMWAWPictMac.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\TMWAWPictOleContainer.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\TMWAWPrint.cxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\TMWAWPropertyHandler.cxx
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

SOURCE=..\..\src\lib\DMWAWEncryption.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\DMWAWFileStructure.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\DMWAWListener.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\DMWAWMemoryStream.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\DMWAWOLEStream.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\DMWAWPageSpan.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\DMWAWSubDocument.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\DMWAWTable.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\IMWAWCell.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\IMWAWContentListener.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\IMWAWDocument.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\IMWAWEntry.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\IMWAWHeader.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\IMWAWList.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\IMWAWParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\IMWAWSubDocument.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\IMWAWTableHelper.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\IMWAWTextParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWContentListener.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWStruct.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\MWAWTools.hxx
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

SOURCE=..\..\src\lib\TMWAWDebug.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\TMWAWFont.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\TMWAWInputStream.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\TMWAWOleParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\TMWAWPict.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\TMWAWPictBasic.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\TMWAWPictBitmap.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\TMWAWPictData.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\TMWAWPictMac.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\TMWAWPictOleContainer.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\TMWAWPosition.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\TMWAWPrint.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\TMWAWPropertyHandler.hxx
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

SOURCE=..\..\src\lib\libmwaw_libwpd_math.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\libmwaw_libwpd_types.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\lib\libmwaw_tools.hxx
# End Source File
# End Group
# End Target
# End Project
