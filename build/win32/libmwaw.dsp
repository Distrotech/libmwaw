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

SOURCE=../../src/libmwaw/DMWAWContentListener.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/DMWAWEncryption.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/DMWAWListener.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/DMWAWMemoryStream.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/DMWAWOLEStream.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/DMWAWPageSpan.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/DMWAWSubDocument.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/DMWAWTable.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/IMWAWCell.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/IMWAWContentListener.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/IMWAWDocument.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/IMWAWExtParser.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/IMWAWExtTextParser.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/IMWAWHeader.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/IMWAWList.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/IMWAWTextParser.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/TMWAWDebug.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/TMWAWInputStream.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/TMWAWMacFont.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/TMWAWMacPrint.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/TMWAWOleParser.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/TMWAWPictBasic.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/TMWAWPictBitmap.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/TMWAWPictData.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/TMWAWPictMac.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/TMWAWPictOleContainer.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/TMWAWPropertyHandler.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/TMWAWWin.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/WDB3MacMNParser.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/WKS3MacMNParser.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/WKS4PcMNParser.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/MWAW3MacGraph.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/MWAW3MacParser.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/MWAW3MacText.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/MWAW4MacMNGraph.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/MWAW4MacMNParser.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/MWAW4MacMNText.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/MWAW4MacParser.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/MWAW4PcMNGraph.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/MWAW4PcMNParser.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/MWAW4PcMNText.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/MWAW4PcParser.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/MWAW8PcMNGraph.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/MWAW8PcMNParser.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/MWAW8PcMNStruct.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/MWAW8PcMNTable.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/MWAW8PcMNText.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/MWAW8PcParser.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/MWAWMacContentListener.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/MWAWMacStruct.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/MWAWMacTools.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/MWAWPcContentListener.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/MWAWPcStruct.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/MWAW_MacGraphParser.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/libmwaw_internal.cxx
# End Source File
# Begin Source File

SOURCE=../../src/libmwaw/libmwaw_libwpd.cxx
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\src\libmwaw\DMWAWContentListener.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\DMWAWEncryption.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\DMWAWFileStructure.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\DMWAWListener.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\DMWAWMemoryStream.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\DMWAWOLEStream.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\DMWAWPageSpan.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\DMWAWSubDocument.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\DMWAWTable.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\IMWAWCell.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\IMWAWContentListener.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\IMWAWDocument.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\IMWAWEntry.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\IMWAWExtEntry.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\IMWAWExtParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\IMWAWExtTextParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\IMWAWHeader.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\IMWAWList.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\IMWAWParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\IMWAWSubDocument.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\IMWAWTextParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\TMWAWDebug.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\TMWAWInputStream.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\TMWAWMacFont.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\TMWAWMacPrint.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\TMWAWOleParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\TMWAWPict.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\TMWAWPictBasic.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\TMWAWPictBitmap.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\TMWAWPictData.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\TMWAWPictMac.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\TMWAWPictOleContainer.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\TMWAWPosition.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\TMWAWPropertyHandler.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\TMWAWWin.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\WDB3MacMNParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\WDBInterface.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\WDBTableInterface.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\WKS3MacMNParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\WKS4PcMNParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\MWAW3MacGraph.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\MWAW3MacParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\MWAW3MacText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\MWAW4MacMNGraph.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\MWAW4MacMNParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\MWAW4MacMNText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\MWAW4MacParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\MWAW4PcMNGraph.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\MWAW4PcMNParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\MWAW4PcMNText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\MWAW4PcParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\MWAW8PcMNGraph.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\MWAW8PcMNParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\MWAW8PcMNStruct.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\MWAW8PcMNTable.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\MWAW8PcMNText.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\MWAW8PcParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\MWAWMacContentListener.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\MWAWMacStruct.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\MWAWMacTools.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\MWAWPcContentListener.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\MWAWPcStruct.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\MWAW_MacGraphParser.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\MWAW_MacGraphStruct.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\libmwaw.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\libmwaw_internal.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\libmwaw_libwpd.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\libmwaw_libwpd_math.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\libmwaw_libwpd_types.hxx
# End Source File
# Begin Source File

SOURCE=..\..\src\libmwaw\libmwaw_tools.hxx
# End Source File
# End Group
# End Target
# End Project
