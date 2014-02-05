/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */

/* libmwaw: tools
* Version: MPL 2.0 / LGPLv2+
*
* The contents of this file are subject to the Mozilla Public License Version
* 2.0 (the "License"); you may not use this file except in compliance with
* the License or as specified alternatively below. You may obtain a copy of
* the License at http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* Major Contributor(s):
* Copyright (C) 2011, 2012 Alonso Laurent (alonso@loria.fr)
*
*
* All Rights Reserved.
*
* For minor contributions see the git repository.
*
* Alternatively, the contents of this file may be used under the terms of
* the GNU Lesser General Public License Version 2 or later (the "LGPLv2+"),
* in which case the provisions of the LGPLv2+ are applicable
* instead of those above.
*/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iomanip>
#include <iostream>
#include <string>
#include <sstream>

#include "file_internal.h"
#include "input.h"
#include "ole.h"
#include "rsrc.h"
#include "xattr.h"

#include <sys/stat.h>

namespace libmwaw_tools
{
class Exception
{
};

struct File {
  //! the constructor
  File(char const *path) : m_fName(path ? path : ""),
    m_fInfoCreator(""), m_fInfoType(""), m_fInfoResult(""),
    m_fileVersion(), m_appliVersion(), m_rsrcMissingMessage(""), m_rsrcResult(""),
    m_dataResult(), m_printFileName(false)
  {
    if (!m_fName.length()) {
      std::cerr << "File::File: call without path\n";
      throw libmwaw_tools::Exception();
    }

    // check if it is a regular file
    struct stat status;
    if (stat(path, &status) == -1) {
      std::cerr << "File::File: the file " << m_fName << " cannot be read\n";
      throw libmwaw_tools::Exception();
    }
    if (!S_ISREG(status.st_mode)) {
      std::cerr << "File::File: the file " << m_fName << " is a not a regular file\n";
      throw libmwaw_tools::Exception();
    }
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, File const &info)
  {
    if (info.m_printFileName)
      o << info.m_fName << ":\n";
    if (info.m_fInfoCreator.length() || info.m_fInfoType.length()) {
      o << "------- fileInfo -------\n";
      if (info.m_fInfoCreator.length())
        o << "\tcreator=" << info.m_fInfoCreator << "\n";
      if (info.m_fInfoType.length())
        o << "\ttype=" << info.m_fInfoType << "\n";
      if (info.m_fInfoResult.length())
        o << "\t\t=>" << info.m_fInfoResult << "\n";
    }
    if (info.m_fileVersion.ok() || info.m_appliVersion.ok() ||
        info.m_rsrcMissingMessage.length() || info.m_rsrcResult.length()) {
      o << "------- resource fork -------\n";
      if (info.m_fileVersion.ok())
        o << "\tFile" << info.m_fileVersion << "\n";
      if (info.m_appliVersion.ok())
        o << "\tAppli" << info.m_appliVersion << "\n";
      if (info.m_rsrcMissingMessage.length())
        o << "\tmissingString=\"" << info.m_rsrcMissingMessage << "\"\n";
      if (info.m_rsrcResult.length())
        o << "\t\t=>" << info.m_rsrcResult << "\n";
    }
    if (info.m_dataResult.size()) {
      o << "------- data fork -------\n";
      for (size_t i = 0; i < info.m_dataResult.size(); i++)
        o << "\t\t=>" << info.m_dataResult[i] << "\n";
    }
    return o;
  }

  //! try to read the file information
  bool readFileInformation();
  //! try to read the data fork
  bool readDataInformation();
  //! try to read the resource version data
  bool readRSRCInformation();

  //! can type the file
  bool canPrintResult(int verbose) const
  {
    if (m_fInfoResult.length() || m_dataResult.size() || m_rsrcResult.length()) return true;
    if (verbose <= 0) return false;
    if (m_fInfoCreator.length() || m_fInfoType.length()) return true;
    if (verbose <= 1) return false;
    return m_fileVersion.ok() || m_appliVersion.ok();
  }
  //! print the file type
  bool printResult(std::ostream &o, int verbose) const;

  bool checkFInfoType(char const *type, char const *result)
  {
    if (m_fInfoType != type) return false;
    m_fInfoResult=result;
    return true;
  }
  bool checkFInfoType(char const *result)
  {
    m_fInfoResult=result;
    if (m_fInfoType=="AAPL")
      m_fInfoResult+="[Application]";
    else if (m_fInfoType=="AIFF" || m_fInfoType=="AIFC")
      m_fInfoResult+="[sound]";
    else
      m_fInfoResult+="["+m_fInfoType+"]";
    return true;
  }
  bool checkFInfoCreator(char const *result)
  {
    m_fInfoResult=result;
    if (m_fInfoCreator.length())
      m_fInfoResult+="["+m_fInfoCreator+"]";
    return true;
  }

  //! the file name
  std::string m_fName;
  //! the file info creator
  std::string m_fInfoCreator;
  //! the file info type
  std::string m_fInfoType;
  //! the result of the finfo
  std::string m_fInfoResult;

  //! the file version (extracted from the resource fork )
  RSRC::Version m_fileVersion;
  //! the application version (extracted from the resource fork )
  RSRC::Version m_appliVersion;
  //! the application missing message
  std::string m_rsrcMissingMessage;
  //! the result of the resource fork
  std::string m_rsrcResult;

  //! the result of the data analysis
  std::vector<std::string> m_dataResult;

  //! print or not the filename
  bool m_printFileName;
};

bool File::readFileInformation()
{
  if (!m_fName.length())
    return false;

  XAttr xattr(m_fName.c_str());
  libmwaw_tools::InputStream *input = xattr.getStream("com.apple.FinderInfo");
  if (!input) return false;

  if (input->length() < 8) {
    delete input;
    return false;
  }

  input->seek(0, libmwaw_tools::InputStream::SK_SET);
  m_fInfoType = "";
  for (int i = 0; i < 4; i++) {
    char c= input->read8();
    if (c==0) break;
    m_fInfoType+= c;
  }
  m_fInfoCreator="";
  for (int i = 0; i < 4; i++) {
    char c= input->read8();
    if (c==0) break;
    m_fInfoCreator+= c;
  }
  delete input;

  if (m_fInfoCreator=="" || m_fInfoType=="")
    return true;
  if (m_fInfoCreator=="AB65") {
    checkFInfoType("AD65", "Pagemaker6.5") || checkFInfoType("Pagemaker6.5");
  }
  else if (m_fInfoCreator=="ACTA") {
    checkFInfoType("OTLN", "Acta") || checkFInfoType("otln", "Acta") || checkFInfoType("Acta");
  }
  else if (m_fInfoCreator=="ALB3") {
    checkFInfoType("ALD3", "Pagemaker3") || checkFInfoType("Pagemaker3");
  }
  else if (m_fInfoCreator=="ALB4") {
    checkFInfoType("ALD4", "Pagemaker4") || checkFInfoType("Pagemaker4");
  }
  else if (m_fInfoCreator=="ALB5") {
    checkFInfoType("ALD5", "Pagemaker5") || checkFInfoType("Pagemaker5");
  }
  else if (m_fInfoCreator=="ALB6") {
    checkFInfoType("ALD6", "Pagemaker6") || checkFInfoType("Pagemaker6");
  }
  else if (m_fInfoCreator=="AOqc") {
    checkFInfoType("TEXT","America Online") || checkFInfoType("ttro","America Online[readOnly]") ||
    checkFInfoType("America Online");
  }
  else if (m_fInfoCreator=="AOS1") {
    checkFInfoType("TEXT","eWorld") || checkFInfoType("ttro","eWorld[readOnly]") ||
    checkFInfoType("eWorld");
  }
  else if (m_fInfoCreator=="BOBO") {
    checkFInfoType("CWDB","ClarisWorks/AppleWorks 1.0[Database]")||
    checkFInfoType("CWD2","ClarisWorks/AppleWorks 2.0-3.0[Database]")||
    checkFInfoType("sWDB","ClarisWorks/AppleWorks 2.0-3.0[Database]")||
    checkFInfoType("CWGR","ClarisWorks/AppleWorks[Draw]")||
    checkFInfoType("sWGR","ClarisWorks/AppleWorks 2.0-3.0[Draw]")||
    checkFInfoType("CWSS","ClarisWorks/AppleWorks[SpreadSheet]")||
    checkFInfoType("CWS2","ClarisWorks/AppleWorks 2.0-3.0[SpreadSheet]")||
    checkFInfoType("sWSS","ClarisWorks/AppleWorks 2.0-3.0[SpreadSheet]")||
    checkFInfoType("CWPR","ClarisWorks/AppleWorks[Presentation]")||
    checkFInfoType("CWWP","ClarisWorks/AppleWorks")||
    checkFInfoType("CWW2","ClarisWorks/AppleWorks 2.0-3.0")||
    checkFInfoType("sWWP","ClarisWorks/AppleWorks 2.0-3.0")||
    checkFInfoType("ClarisWorks/AppleWorks");
  }
  else if (m_fInfoCreator=="BWks") {
    checkFInfoType("BWwp","BeagleWorks/WordPerfect Works") ||
    checkFInfoType("BWdb","BeagleWorks/WordPerfect Works[Database]") ||
    checkFInfoType("BWss","BeagleWorks/WordPerfect Works[SpreadSheet]") ||
    checkFInfoType("BWpt","BeagleWorks/WordPerfect Works[Presentation]") ||
    checkFInfoType("BWdr","BeagleWorks/WordPerfect Works[Draw]") ||
    checkFInfoType("BeagleWorks/WordPerfect Works");
  }
  else if (m_fInfoCreator=="CARO") {
    checkFInfoType("PDF ", "Acrobat PDF");
  }
  else if (m_fInfoCreator=="CDrw") {
    checkFInfoType("dDraw", "ClarisDraw") || checkFInfoType("ClarisDraw");
  }
  else if (m_fInfoCreator=="DkmR") {
    checkFInfoType("TEXT","Basic text(created by DOCMaker)") || checkFInfoType("DOCMaker");
  }
  else if (m_fInfoCreator=="Dk@P") {
    checkFInfoType("APPL","DOCMaker") || checkFInfoType("DOCMaker");
  }
  else if (m_fInfoCreator=="DDAP") {
    checkFInfoType("DDFL+","DiskDoubler") || checkFInfoType("DiskDoubler");
  }
  else if (m_fInfoCreator=="FH50") {
    checkFInfoType("AGD1","FreeHand 5") || checkFInfoType("FreeHand 5");
  }
  else if (m_fInfoCreator=="FHD3") {
    checkFInfoType("FHA3","FreeHand 3") || checkFInfoType("FreeHand 3");
  }
  else if (m_fInfoCreator=="FS03") {
    checkFInfoType("WRT+","WriterPlus") || checkFInfoType("WriterPlus");
  }
  else if (m_fInfoCreator=="Fram") {
    checkFInfoType("FASL","FrameMaker") || checkFInfoType("MIF2","FrameMaker MIF2.0") ||
    checkFInfoType("MIF3","FrameMaker MIF3.0") || checkFInfoType("MIF ","FrameMaker MIF") ||
    checkFInfoType("FrameMaker");
  }
  else if (m_fInfoCreator=="FWRT") {
    checkFInfoType("FWRT","FullWrite 1.0") || checkFInfoType("FWRM","FullWrite 1.0") ||
    checkFInfoType("FWRI","FullWrite 2.0") || checkFInfoType("FullWrite");
  }
  else if (m_fInfoCreator=="JWrt") {
    checkFInfoType("TEXT","JoliWrite") || checkFInfoType("ttro","JoliWrite[readOnly]") ||
    checkFInfoType("JoliWrite");
  }
  else if (m_fInfoCreator=="HMiw") {
    checkFInfoType("IWDC","HanMac Word-J") || checkFInfoType("HanMac Word-J");
  }
  else if (m_fInfoCreator=="HMdr") {
    checkFInfoType("DRD2","HanMac Word-K") || checkFInfoType("HanMac Word-K");
  }
  else if (m_fInfoCreator=="LWTE") {
    checkFInfoType("TEXT","LightWayText") || checkFInfoType("MACR","LightWayText[MACR]") ||
    checkFInfoType("pref","LightWayText[Preferences]") ||
    checkFInfoType("ttro","LightWayText[Tutorial]") || checkFInfoType("LightWayText");
  }
  else if (m_fInfoCreator=="LWTR") {
    checkFInfoType("APPL","LightWayText[appli]");
  }
  else if (m_fInfoCreator=="MACA") {
    checkFInfoType("WORD","MacWrite") || checkFInfoType("MacWrite");
  }
  else if (m_fInfoCreator=="MACD") {   // checkme
    checkFInfoType("DRWG","MacDraw[unsure]");
  }
  else if (m_fInfoCreator=="MDsr") {
    checkFInfoType("APPL","MacDoc(appli)");
  }
  else if (m_fInfoCreator=="MDvr") {
    checkFInfoType("MDdc","MacDoc") || checkFInfoType("MacDoc");
  }
  else if (m_fInfoCreator=="MDRW") {
    checkFInfoType("DRWG","MacDraw") || checkFInfoType("MacDraw");
  }
  else if (m_fInfoCreator=="MDPL") {
    checkFInfoType("DRWG","MacDraw II") || checkFInfoType("MacDraw II");
  }
  else if (m_fInfoCreator=="MMBB") {
    checkFInfoType("MBBT","Mariner Write") || checkFInfoType("Mariner Write");
  }
  else if (m_fInfoCreator=="MORE") {
    checkFInfoType("MORE","More") || checkFInfoType("More");
  }
  else if (m_fInfoCreator=="MOR2") {
    checkFInfoType("MOR2","More 2") || checkFInfoType("MOR3","More 3") ||
    checkFInfoType("More 2-3");
  }
  else if (m_fInfoCreator=="MPNT") {
    checkFInfoType("PNTG","MacPaint") || checkFInfoType("MacPaint");
  }
  else if (m_fInfoCreator=="MWII") {
    checkFInfoType("MW2D","MacWrite II") || checkFInfoType("MacWrite II");
  }
  else if (m_fInfoCreator=="MWPR") {
    checkFInfoType("MWPd","MacWrite Pro") || checkFInfoType("MacWrite Pro");
  }
  else if (m_fInfoCreator=="MSWD") {
    checkFInfoType("WDBN","Microsoft Word 3-5") ||
    checkFInfoType("GLOS","Microsoft Word 3-5[glossary]") ||
    checkFInfoType("W6BN", "Microsoft Word 6") ||
    checkFInfoType("W8BN", "Microsoft Word 8") ||
    checkFInfoType("W8TN", "Microsoft Word 8[W8TN]") || // ?
    checkFInfoType("WXBN", "Microsoft Word 97-2004") || // Office X ?
    checkFInfoType("Microsoft Word");
  }
  else if (m_fInfoCreator=="MSWK") {
    checkFInfoType("AWWP","Microsoft Works 3") ||
    checkFInfoType("AWDB","Microsoft Works 3-4[database]") ||
    checkFInfoType("AWDR","Microsoft Works 3-4[draw]") ||
    checkFInfoType("AWSS","Microsoft Works 3-4[spreadsheet]") ||
    checkFInfoType("RLRB","Microsoft Works 4") ||
    checkFInfoType("sWRB","Microsoft Works 4[template]") ||
    checkFInfoType("Microsoft Works 3-4");
  }
  else if (m_fInfoCreator=="NISI") {
    checkFInfoType("TEXT","Nisus") || checkFInfoType("GLOS","Nisus[glossary]") ||
    checkFInfoType("SMAC","Nisus[macros]") || checkFInfoType("edtt","Nisus[lock]") ||
    checkFInfoType("Nisus");
  }
  else if (m_fInfoCreator=="PPNT") {
    checkFInfoType("SLDS","Microsoft PowerPoint") || checkFInfoType("Microsoft PowerPoint");
  }
  else if (m_fInfoCreator=="PPT3") {
    checkFInfoType("SLD8","Microsoft PowerPoint 97-2004") || checkFInfoType("Microsoft PowerPoint 97-2004");
  }
  else if (m_fInfoCreator=="PSIP") {
    checkFInfoType("AWWP","Microsoft Works 1.0") || checkFInfoType("Microsoft Works 1.0");
  }
  else if (m_fInfoCreator=="PSI2") {
    checkFInfoType("AWWP","Microsoft Works 2.0") || checkFInfoType("AWSS","Microsoft Works 2.0[spreadsheet]") ||
    checkFInfoType("Microsoft Works 2.0");
  }
  else if (m_fInfoCreator=="PWRI") {
    checkFInfoType("OUTL","MindWrite") || checkFInfoType("MindWrite");
  }
  else if (m_fInfoCreator=="R#+A") {
    checkFInfoType("R#+D","RagTime") || checkFInfoType("RagTime");
  }
  else if (m_fInfoCreator=="RTF ") {
    checkFInfoType("RTF ","RTF ") || checkFInfoType("RTF");
  }
  else if (m_fInfoCreator=="SIT!") {
    checkFInfoType("SIT5", "archive SIT") ||
    checkFInfoType("SITD", "archive SIT") ||
    checkFInfoType("SIT!", "archive SIT") || checkFInfoType("SIT");
  }
  else if (m_fInfoCreator=="SSIW") {   // check me
    checkFInfoType("WordPerfect 1.0");
  }
  else if (m_fInfoCreator=="TBB5") {
    checkFInfoType("TEXT","Tex-Edit") || checkFInfoType("ttro","Tex-Edit[readOnly]") ||
    checkFInfoType("Tex-Edit");
  }
  else if (m_fInfoCreator=="WORD") {
    checkFInfoType("WDBN","Microsoft Word 1") || checkFInfoType("Microsoft Word 1");
  }
  else if (m_fInfoCreator=="WPC2") {
    checkFInfoType("WordPerfect");
  }
  else if (m_fInfoCreator=="XCEL") {
    checkFInfoType("XCEL","Microsoft Excel 1") ||
    checkFInfoType("XLS3","Microsoft Excel 3") ||
    checkFInfoType("XLS4","Microsoft Excel 4") ||
    checkFInfoType("XLS5","Microsoft Excel 5") ||
    checkFInfoType("XLS8","Microsoft Excel 97-2004") ||
    checkFInfoType("TEXT","Microsoft Excel[text export]") ||
    checkFInfoType("Microsoft Excel");
  }
  else if (m_fInfoCreator=="XPR3") {
    checkFInfoType("XDOC","QuarkXPress") || checkFInfoType("QuarkXPress");
  }
  else if (m_fInfoCreator=="ZEBR") {
    checkFInfoType("ZWRT","GreatWorks") || checkFInfoType("ZTRM","GreatWorks[comm]") ||
    checkFInfoType("ZDBS","GreatWorks[database]") || checkFInfoType("ZCAL","GreatWorks[spreadsheet]") ||
    checkFInfoType("ZOLN","GreatWorks[outline]") || checkFInfoType("PNTG","GreatWorks[paint]") ||
    checkFInfoType("ZOBJ","GreatWorks[draw]") || checkFInfoType("ZCHT","GreatWorks[chart]") ||
    checkFInfoType("GreatWorks");
  }
  else if (m_fInfoCreator=="ZWRT") {
    checkFInfoType("Zart","Z-Write") || checkFInfoType("Z-Write");
  }
  else if (m_fInfoCreator=="dPro") {
    checkFInfoType("dDoc","MacDraw Pro") || checkFInfoType("MacDraw Pro");
  }
  else if (m_fInfoCreator=="eDcR") {
    checkFInfoType("eDoc","eDOC") || checkFInfoType("eDOC");
  }
  else if (m_fInfoCreator=="eSRD") {
    checkFInfoType("APPL","eDOC(appli)");
  }
  else if (m_fInfoCreator=="nX^n") {
    checkFInfoType("nX^d","WriteNow 2") || checkFInfoType("nX^2","WriteNow 3-4") ||
    checkFInfoType("WriteNow");
  }
  else if (m_fInfoCreator=="ntxt") {
    checkFInfoType("TEXT","Anarcho");
  }
  else if (m_fInfoCreator=="ttxt") {
    if (m_fInfoType=="TEXT") {
      /* a little complex can be Classic MacOS SimpleText/TeachText or
      a << normal >> text file */
      XAttr rsrcAttr(m_fName.c_str());
      libmwaw_tools::InputStream *rsrcStream =
        rsrcAttr.getStream("com.apple.ResourceFork");
      bool ok = false;
      if (rsrcStream && rsrcStream->length()) {
        libmwaw_tools::RSRC rsrcManager(*rsrcStream);
        ok = rsrcManager.hasEntry("styl", 128);
      }
      if (rsrcStream) delete rsrcStream;
      if (ok) checkFInfoType("TEXT","TeachText/SimpleText");
      else checkFInfoType("TEXT","Basic text");
    }
    else
      checkFInfoType("ttro","TeachText/SimpleText[readOnly]");
  }
  // now by type
  else if (m_fInfoType=="AAPL") {
    checkFInfoCreator("Application");
  }
  else if (m_fInfoType=="JFIF") {
    checkFInfoCreator("JPEG");
  }
  if (m_fInfoCreator.length()==0) {
    MWAW_DEBUG_MSG(("File::readFileInformation: Find unknown file info %s[%s]\n", m_fInfoCreator.c_str(), m_fInfoType.c_str()));
  }
  return true;
}

bool File::readDataInformation()
{
  if (!m_fName.length())
    return false;
  libmwaw_tools::FileStream input(m_fName.c_str());
  if (!input.ok()) {
    MWAW_DEBUG_MSG(("File::readDataInformation: can not open the data fork\n"));
    return false;
  }
  if (input.length() < 10)
    return true;
  input.seek(0, InputStream::SK_SET);
  int val[5];
  for (int i = 0; i < 5; i++)
    val[i] = int(input.readU16());
  // ----------- clearly discriminant ------------------
  if (val[2] == 0x424F && val[3] == 0x424F && (val[0]>>8) < 8) {
    m_dataResult.push_back("ClarisWorks/AppleWorks");
    return true;
  }
  if (val[0]==0x4257 && val[1]==0x6b73 && val[2]==0x4257 && val[4]==0x4257) {
    if (val[3]==0x6462)
      m_dataResult.push_back("BeagleWorks/WordPerfect Works[Database]");
    else if (val[3]==0x6472)
      m_dataResult.push_back("BeagleWorks/WordPerfect Works[Draw]");
    else if (val[3]==0x7074)
      m_dataResult.push_back("BeagleWorks/WordPerfect Works[Presentation]");
    else if (val[3]==0x7373)
      m_dataResult.push_back("BeagleWorks/WordPerfect Works[Spreadsheet]");
    else if (val[3]==0x7770)
      m_dataResult.push_back("BeagleWorks/WordPerfect Works");
    else
      m_dataResult.push_back("BeagleWorks/WordPerfect Works[Unknown]");
    return true;
  }
  if (val[0]==0x5772 && val[1]==0x6974 && val[2]==0x654e && val[3]==0x6f77 && val[4]==2) {
    m_dataResult.push_back("WriteNow 3-4");
    return true;
  }
  if (val[0]==3 && val[1]==0x4d52 && val[2]==0x4949 && val[3]==0x80) {
    m_dataResult.push_back("More 2");
    return true;
  }
  if (val[0]==6 && val[1]==0x4d4f && val[2]==0x5233 && val[3]==0x80) {
    m_dataResult.push_back("More 3");
    return true;
  }
  if (val[0]==0x4646 && val[1]==0x4646 && val[2]==0x3030 && val[3]==0x3030) {
    m_dataResult.push_back("Mariner Write");
    return true;
  }
  if (val[0]==0x4859 && val[1]==0x4c53 && val[2]==0x0210) {
    m_dataResult.push_back("HanMac Word-K");
    return true;
  }
  if (val[0]==0x594c && val[1]==0x5953 && val[2]==0x100) {
    m_dataResult.push_back("HanMac Word-J");
    return true;
  }
  if (val[0]==0x2550 && val[1]==0x4446) {
    m_dataResult.push_back("PDF");
    return true;
  }
  if (val[0]==0x2854 && val[1]==0x6869 && val[2]==0x7320 && val[3]==0x6669) {
    m_dataResult.push_back("BinHex");
    return true;
  }
  if (val[0]==0x2521 && val[1]==0x5053 && val[2]==0x2d41 && val[3] == 0x646f && val[4]==0x6265) {
    m_dataResult.push_back("PostScript");
    return true;
  }
  if (val[0]==0xc5d0 && val[1]==0xd3c6) {
    m_dataResult.push_back("Adobe EPS");
    return true;
  }
  if (val[0]==0x7b5c && val[1]==0x7274 && (val[2]>>8)==0x66) {
    m_dataResult.push_back("RTF");
    return true;
  }
  if (val[2]==0x6d6f && val[3]==0x6f76) {
    m_dataResult.push_back("QuickTime movie");
    return true;
  }
  if (val[0]==0 && (val[1]>>8)==0 && val[2]==0x6674 && val[3]==0x7970 && val[4]==0x3367) {
    m_dataResult.push_back("MP4");
    return true;
  }
  if (val[0]==0x4749 && val[1]==0x4638 && (val[2]==0x3761 || val[2]==0x3961)) {
    m_dataResult.push_back("GIF");
    return true;
  }
  if (val[0]==0x8950 && val[1]==0x4e47 && val[2]==0x0d0a && val[3]==0x1a0a) {
    m_dataResult.push_back("PNG");
    return true;
  }
  if (val[0]==0xffd8 &&
      ((val[1]==0xffe0 && val[3]==0x4a46 && val[4] == 0x4946) ||
       (val[1]==0xffe1 && val[3]==0x4578 && val[4] == 0x6966) ||
       (val[1]==0xffe8 && val[3]==0x5350 && val[4] == 0x4946))) {
    m_dataResult.push_back("JPEG");
    return true;
  }
  if (val[0]==0x4949 && val[1]==0x2a00) {
    m_dataResult.push_back("TIF");
    return true;
  }
  if (val[0]==0x4d4d && val[1]==0x002a) {
    m_dataResult.push_back("TIFF");
    return true;
  }
  if (val[0]==0x4f67 && val[1]==0x6753) {
    m_dataResult.push_back("OGG data");
    return true;
  }
  // ----------- less discriminant ------------------
  if (val[0]==0xd0cf && val[1]==0x11e0 && val[2]==0xa1b1 && val[3]==0x1ae1) {
    libmwaw_tools::OLE ole(input);
    for (int step=0; step < 2; step++) {
      std::string res=step==0 ? ole.getCLSIDType() : ole.getCompObjType();
      if (!res.length())
        continue;
      m_dataResult.push_back(res);
      return true;
    }
    m_dataResult.push_back("OLE file: can be DOC, DOT, PPS, PPT, XLA, XLS, WIZ, WPS(4.0), ...");
    return true;
  }
  if (val[0]==0x100 || val[0]==0x200) {
    if (val[1]==0x5a57 && val[2]==0x5254) {
      m_dataResult.push_back("GreatWorks");
      return true;
    }
    if (val[1]==0x5a4f && val[2]==0x4c4e) {
      m_dataResult.push_back("GreatWorks[outline]");
      return true;
    }
    if (val[1]==0x5a44 && val[2]==0x4253) {
      m_dataResult.push_back("GreatWorks[database]");
      return true;
    }
    if (val[1]==0x5a43 && val[2]==0x414c) {
      m_dataResult.push_back("GreatWorks[spreadsheet]");
      return true;
    }
    if (val[1]==0x5a4f && val[2]==0x424a) {
      m_dataResult.push_back("GreatWorks[draw]");
      return true;
    }
    if (val[1]==0x5a43 && val[2]==0x4854) {
      m_dataResult.push_back("GreatWorks[chart]");
      return true;
    }
  }
  // less discriminant
  if ((val[0]==0xfe32 && val[1]==0) || (val[0]==0xfe34 && val[1]==0) ||
      (val[0] == 0xfe37 && (val[1] == 0x23 || val[1] == 0x1c))) {
    switch (val[1]) {
    case 0:
      if (val[0]==0xfe34)
        m_dataResult.push_back("Microsoft Word 3.0");
      else if (val[0]==0xfe32)
        m_dataResult.push_back("Microsoft Word 1.0");
      break;
    case 0x1c:
      m_dataResult.push_back("Microsoft Word 4.0");
      break;
    case 0x23:
      m_dataResult.push_back("Microsoft Word 5.0");
      break;
    default:
      break;
    }
  }
  if (val[0]==0 && val[1]==0 && val[2]==0 && val[3]==0 &&
      ((val[4]>>8)==4 || (val[4]>>8)==0x44))
    m_dataResult.push_back("WriteNow 1-2");
  if (val[0] == 0x2e && val[1] == 0x2e)
    m_dataResult.push_back("MacWrite II");
  if (val[0] == 4 && val[1] == 4)
    m_dataResult.push_back("MacWrite Pro");
  if (val[0] == 0x7704)
    m_dataResult.push_back("MindWrite");
  if (val[0] == 0x110)
    m_dataResult.push_back("WriterPlus");
  if (val[0]==0xdba5 && val[1]==0x2d00) {
    m_dataResult.push_back("Microsoft Word 2.0[pc]");
    return true;
  }
  if (val[0] == 3 || val[0] == 6) {
    int numParaPos = val[0] == 3 ? 2 : 1;
    if (val[numParaPos] < 0x1000 && val[numParaPos+1] < 0x100 && val[numParaPos+2] < 0x100)
      m_dataResult.push_back("MacWrite[unsure]");
  }
  if (val[0]==0) {
    std::stringstream s;
    bool ok = true;
    switch (val[1]) {
    case 4:
      s << "Microsoft Works 1.0";
      break;
    case 8:
      s << "Microsoft Works 2.0";
      break;
    case 9:
      s << "Microsoft Works 3.0";
      break;
    case 11:
      s << "Microsoft Works 4.0";
      break; // all excepted a text file
    default:
      ok = false;
      break;
    }
    input.seek(16, InputStream::SK_SET);
    int type = ok ? (int) input.readU16() : -1;
    switch (type) {
    case 1:
      break;
    case 2:
      s << "[database]";
      break;
    case 3:
      s << "[spreadsheet]";
      break;
    case 12:
      s << "[draw]";
      break;
    default:
      ok = false;
      break;
    }
    if (ok)
      m_dataResult.push_back(s.str());
  }
  input.seek(-4, InputStream::SK_END);
  int lVal[2];
  for (int i = 0; i < 2; i++)
    lVal[i] = int(input.readU16());
  if (lVal[0] == 0x4657 && lVal[1]==0x5254)
    m_dataResult.push_back("FullWrite 2.0");
  else if (lVal[0] == 0x4E4C && lVal[1]==0x544F)
    m_dataResult.push_back("Acta Classic");
  else if (lVal[1]==0 && val[0]==1 && (val[1]==1||val[1]==2))
    m_dataResult.push_back("Acta v2[unsure]");
  else if (lVal[0] == 0 && lVal[1]==1) { // Maybe a FullWrite 1.0 file, limited check
    input.seek(-38, InputStream::SK_END);
    long eof = input.length();
    bool ok = true;
    for (int i = 0; i < 2; i++) {
      long pos = (long) input.readU32();
      long sz = (long) input.read32();
      if (sz <= 0 || pos+sz > eof) {
        ok = false;
        break;
      }
    }
    if (ok)
      m_dataResult.push_back("FullWrite 1.0[unsure]");
  }
#ifdef DEBUG
  if (m_dataResult.size()==0) {
    std::stringstream s;
    s << "Unknown: " << std::hex << std::setfill('0');
    for (int i = 0; i < 5; i++)
      s << std::setw(4) << val[i] << " ";
    m_dataResult.push_back(s.str());
  }
#endif
  return true;
}

bool File::readRSRCInformation()
{
  if (!m_fName.length())
    return false;

  XAttr xattr(m_fName.c_str());
  libmwaw_tools::InputStream *rsrcStream = xattr.getStream("com.apple.ResourceFork");
  if (!rsrcStream) return false;

  if (!rsrcStream->length()) {
    delete rsrcStream;
    return true;
  }
  libmwaw_tools::RSRC rsrcManager(*rsrcStream);
#  if 0
  MWAW_DEBUG_MSG(("File::readRSRCInformation: find a resource fork\n"));
#  endif
  m_rsrcResult = rsrcManager.getString(-16396); // the application missing name
  m_rsrcMissingMessage = rsrcManager.getString(-16397);
  std::vector<RSRC::Version> listVersion = rsrcManager.getVersionList();
  for (size_t i=0; i < listVersion.size(); i++) {
    switch (listVersion[i].m_id) {
    case 1:
      m_fileVersion = listVersion[i];
      break;
    case 2:
      if (!m_appliVersion.ok()) m_appliVersion = listVersion[i];
      break;
    case 2002:
      m_appliVersion = listVersion[i];
      break;
    default:
      break;
    }
  }
  delete rsrcStream;
  return true;
}

bool File::printResult(std::ostream &o, int verbose) const
{
  if (!canPrintResult(verbose)) return false;
  if (m_printFileName)
    o << m_fName << ":";
  if (m_fInfoResult.length())
    o << m_fInfoResult;
  else if (m_rsrcResult.length())
    o << m_rsrcResult;
  else if (m_dataResult.size()) {
    size_t num = m_dataResult.size();
    if (num>1)
      o << "[";
    for (size_t i = 0; i < num; i++) {
      o << m_dataResult[i];
      if (i+1!=num)
        o << ",";
    }
    if (num>1)
      o << "]";
  }
  else
    o << "unknown";
  if (verbose > 0) {
    if (m_fInfoCreator.length() || m_fInfoType.length())
      o << ":type=" << m_fInfoCreator << "["  << m_fInfoType << "]";
  }
  if (verbose > 1) {
    if (m_fileVersion.ok())
      o << "\n\tFile" << m_fileVersion;
    if (m_appliVersion.ok())
      o << "\n\tAppli" << m_appliVersion;
  }
  o << "\n";
  return true;
}
}

void usage(char const *fName)
{
  std::cerr << "Syntax error, expect:\n";
  std::cerr << "\t " << fName << " [-h][-H][-vNum] filename\n";
  std::cerr << "\t where\t filename is the file path,\n";
  std::cerr << "\t\t -h: does not print the filename,\n";
  std::cerr << "\t\t -H: prints the filename[default],\n";
  std::cerr << "\t\t -vNum: define the verbose level.\n";
}

int main(int argc, char *const argv[])
{
  int ch, verbose=0;
  bool printFileName=true;

  while ((ch = getopt(argc, argv, "hHv:")) != -1) {
    switch (ch) {
    case 'v':
      verbose=atoi(optarg);
      break;
    case 'h':
      printFileName = false;
      break;
    case 'H':
      printFileName = true;
      break;
    case '?':
    default:
      verbose=-1;
      break;
    }
  }
  if (argc != 1+optind || verbose < 0) {
    usage(argv[0]);
    return -1;
  }
  libmwaw_tools::File *file = 0;
  try {
    file=new libmwaw_tools::File(argv[optind]);
    file->readFileInformation();
  }
  catch (...) {
    std::cerr << argv[0] << ": can not open file " << argv[optind] << "\n";
    if (file) delete file;
    return -1;
  }
  if (!file)
    return -1;
  try {
    file->readDataInformation();
  }
  catch (...) {
  }
  try {
    file->readRSRCInformation();
  }
  catch (...) {
  }

  file->m_printFileName = printFileName;
  if (verbose >= 4)
    std::cout << *file;
  else if (file->canPrintResult(verbose))
    file->printResult(std::cout, verbose);
  delete file;
  return 0;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
