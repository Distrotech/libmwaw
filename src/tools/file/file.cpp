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

#if defined(__MACH__)
#  include <Carbon/Carbon.h>
#else
#  include <Carbon.h>
#endif

#include <string.h>
#include <unistd.h>

#include <iomanip>
#include <iostream>
#include <string>
#include <sstream>

#include "file_internal.h"
#include "input.h"
#include "rsrc.h"

/**
   Can be compile with
   g++ -o mwawFile file.cpp -framework Carbon
*/
namespace libmwaw_tools
{
class Exception
{
};

void checkError(OSStatus result, char const *err, bool fatal)
{
  if (result == noErr)
    return;
  if (err) {
    MWAW_DEBUG_MSG(("%s: get error %s\n", err, GetMacOSStatusErrorString(result)));
  }
  if (fatal) throw Exception();
}

struct File {
  //! the constructor
  File(char const *path) : m_fName(path), m_fRef(),
    m_fInfoCreator(""), m_fInfoType(""), m_fInfoResult(""),
    m_fileVersion(), m_appliVersion(), m_dataResult() {
    OSStatus result;
    Boolean isDirectory;
    /* convert the POSIX path to an FSRef */
    result = FSPathMakeRef((const UInt8*)path, &m_fRef, &isDirectory);
    libmwaw_tools::checkError(result, "FSPathMakeRef", true);
    if (isDirectory) {
      std::cerr << "File::File: the file " << m_fName << " is a directory\n";
      throw libmwaw_tools::Exception();
    }
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, File const &info) {
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
    if (info.m_fileVersion.ok() || info.m_appliVersion.ok()) {
      o << "------- resource fork -------\n";
      if (info.m_fileVersion.ok())
        o << "\tFile" << info.m_fileVersion << "\n";
      if (info.m_appliVersion.ok())
        o << "\tAppli" << info.m_appliVersion << "\n";
    }
    if (info.m_dataResult.size()) {
      o << "------- data fork -------\n";
      for (size_t i = 0; i < info.m_dataResult.size(); i++)
        o << "\t" << info.m_dataResult[i] << "\n";
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
  bool canPrintResult(int verbose) const {
    if (m_fInfoResult.length() || m_dataResult.size()) return true;
    if (verbose <= 0) return false;
    if (m_fInfoCreator.length() || m_fInfoType.length()) return true;
    if (verbose <= 1) return false;
    return m_fileVersion.ok() || m_appliVersion.ok();
  }
  //! print the file type
  bool printResult(std::ostream &o, int verbose) const;

  bool checkFInfoType(char const *type, char const *result) {
    if (m_fInfoType != type) return false;
    m_fInfoResult=result;
    return true;
  }
  bool checkFInfoType(char const *result) {
    m_fInfoResult=result;
    m_fInfoResult+="["+m_fInfoType+"]";
    return true;
  }

  //! the file name
  std::string m_fName;
  //! the file reference
  FSRef m_fRef;
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

  //! the result of the data analysis
  std::vector<std::string> m_dataResult;
};

bool File::readFileInformation()
{
  FSCatalogInfo cInfo;
  OSStatus result;
  result=FSGetCatalogInfo (&m_fRef, kFSCatInfoFinderInfo, &cInfo, NULL, NULL, NULL);
  if (result != noErr) {
    libmwaw_tools::checkError(result, "FSGetCatalogInfo", false);
    return false;
  }

  FileInfo &fInfo = reinterpret_cast< FileInfo &>(cInfo.finderInfo);
  m_fInfoCreator="";
  if (fInfo.fileCreator) {
    char const *str=reinterpret_cast<char *>(&fInfo.fileCreator);
    for (int i = 0; i < 4; i++)
      m_fInfoCreator+=str[3-i];
  }
  m_fInfoType = "";
  if (fInfo.fileType) {
    char const *str=reinterpret_cast<char *>(&fInfo.fileType);
    for (int i = 0; i < 4; i++)
      m_fInfoType+=str[3-i];
  }
  if (m_fInfoCreator=="" || m_fInfoType=="")
    return true;
  if (m_fInfoType=="AAPL")
    m_fInfoResult="Application["+m_fInfoCreator+"]";
  else if (m_fInfoCreator=="BOBO") {
    checkFInfoType("CWDB","ClarisWorks/AppleWorks[Database]")||
    checkFInfoType("CWGR","ClarisWorks/AppleWorks[Draw]")||
    checkFInfoType("CWSS","ClarisWorks/AppleWorks[SpreadSheet]")||
    checkFInfoType("CWWP","ClarisWorks/AppleWorks")||
    checkFInfoType("ClarisWorks/AppleWorks");
  } else if (m_fInfoCreator=="CARO") {
    checkFInfoType("PDF ", "Acrobat PDF");
  } else if (m_fInfoCreator=="FS03") {
    checkFInfoType("WRT+","WriterPlus") || checkFInfoType("WriterPlus");
  } else if (m_fInfoCreator=="FWRT") {
    checkFInfoType("FWRM","FullWrite 1.0") || checkFInfoType("FWRI","FullWrite 2.0") ||
    checkFInfoType("FWRI","FullWrite");
  } else if (m_fInfoCreator=="MACA") {
    checkFInfoType("WORD","MacWrite") || checkFInfoType("MacWrite");
  } else if (m_fInfoCreator=="MACD") { // checkme
    checkFInfoType("DRWG","MacDraw[unsure]");
  } else if (m_fInfoCreator=="MDRW") {
    checkFInfoType("DRWG","MacDraw") || checkFInfoType("MacDraw");
  } else if (m_fInfoCreator=="MDPL") {
    checkFInfoType("DRWG","MacDraw II") || checkFInfoType("MacDraw II");
  } else if (m_fInfoCreator=="MWII") {
    checkFInfoType("MW2D","MacWrite II") || checkFInfoType("MacWrite II");
  } else if (m_fInfoCreator=="MWPR") {
    checkFInfoType("MWPd","MacWrite Pro") || checkFInfoType("MacWrite Pro");
  } else if (m_fInfoCreator=="MSWD") {
    checkFInfoType("WDBN","Microsoft Word 3-5") ||
    checkFInfoType("GLOS","Microsoft Word 3-5[glossary]") ||
    checkFInfoType("Microsoft Word 3-5");
  } else if (m_fInfoCreator=="MSWK") {
    checkFInfoType("AWWP","Microsoft Works 3") ||
    checkFInfoType("RLRB","Microsoft Works 4") ||
    checkFInfoType("Microsoft Works 3-4");
  } else if (m_fInfoCreator=="NISI") {
    checkFInfoType("TEXT","Nisus") || checkFInfoType("GLOS","Nisus[glossary]") ||
    checkFInfoType("SMAC","Nisus[macros]") || checkFInfoType("edtt","Nisus[lock]") ||
    checkFInfoType("Nisus");
  } else if (m_fInfoCreator=="PSIP") {
    checkFInfoType("AWWP","Microsoft Works 1.0") || checkFInfoType("Microsoft Works 1.0");
  } else if (m_fInfoCreator=="PSI2") {
    checkFInfoType("AWWP","Microsoft Works 2.0") || checkFInfoType("Microsoft Works 2.0");
  } else if (m_fInfoCreator=="PWRI") {
    checkFInfoType("OUTL","MindWrite") || checkFInfoType("MindWrite");
  } else if (m_fInfoCreator=="R#+A") {
    checkFInfoType("R#+D","RagTime") || checkFInfoType("RagTime");
  } else if (m_fInfoCreator=="WORD") {
    checkFInfoType("WDBN","Microsoft Word 1") || checkFInfoType("Microsoft Word 1");
  } else if (m_fInfoCreator=="XCEL") {
    checkFInfoType("XCEL","Microsoft Excel 1") ||
    checkFInfoType("XLS4","Microsoft Excel 4") ||
    checkFInfoType("XLS5","Microsoft Excel 5") ||
    checkFInfoType("Microsoft Excel");
  } else if (m_fInfoCreator=="dPro") {
    checkFInfoType("dDoc","MacDraw Pro") || checkFInfoType("MacDraw Pro");
  } else if (m_fInfoCreator=="nX^n") {
    checkFInfoType("nX^d","WriteNow 2") || checkFInfoType("nX^2","WriteNow 3-4") ||
    checkFInfoType("WriteNow");
  } else if (m_fInfoCreator=="ttxt") {
    checkFInfoType("TEXT","Text(basic)");
  }
  if (m_fInfoCreator.length()==0) {
    MWAW_DEBUG_MSG(("File::readFileInformation: Find unknown file info %s[%s]\n", m_fInfoCreator.c_str(), m_fInfoType.c_str()));
  }
  return true;
}

bool File::readDataInformation()
{
  HFSUniStr255 dataFName;
  FSGetDataForkName(&dataFName);
  libmwaw_tools::FileStream input(m_fRef, dataFName);
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
  if (val[0]==0x5772 && val[1]==0x6974 && val[2]==0x654e && val[3]==0x6f77 && val[4]==2) {
    m_dataResult.push_back("WriteNow 3-4");
    return true;
  }
  if (val[0]==0x2550 && val[1]==0x4446) {
    m_dataResult.push_back("PDF");
    return true;
  }
  // ----------- less discriminant ------------------
  if (val[0]==0xd0cf && val[1]==0x11e0 && val[2]==0xa1b1 && val[3]==0x1ae1) {
    m_dataResult.push_back("OLE file: can be DOC, DOT, PPS, PPT, XLA, XLS, WIZ, WPS(4.0), ...");
    return true;
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
  if (val[0]==0 && val[1]==0 && val[2]==0 && val[3]==0 && (val[4]>>8)==4)
    m_dataResult.push_back("WriteNow 1-2");
  if (val[0] == 0x2e && val[1] == 0x2e)
    m_dataResult.push_back("MacWrite II");
  if (val[0] == 4 && val[1] == 4)
    m_dataResult.push_back("MacWritePro");
  if (val[0] == 0x7704)
    m_dataResult.push_back("MindWrite");
  if (val[0] == 0x110)
    m_dataResult.push_back("WriterPlus");
  if (val[0] == 3 || val[0] == 6) {
    int numParaPos = val[0] == 3 ? 2 : 1;
    if (val[numParaPos] < 0x1000 && val[numParaPos+1] < 0x100 && val[numParaPos+2] < 0x100)
      m_dataResult.push_back("MacWrite[unsure]");
  }
  if (val[0]==0) {
    std::stringstream s;
    bool ok = true;
    switch(val[1]) {
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
    switch(type) {
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
  if (lVal[0] == 0 && lVal[1]==1) { // Maybe a FullWrite 1.0 file, limited check
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
  HFSUniStr255 rsrcFName;
  FSGetResourceForkName(&rsrcFName);
  libmwaw_tools::FileStream rsrcStream(m_fRef, rsrcFName);
  if (!rsrcStream.ok())
    return false;
  if (!rsrcStream.length())
    return true;
  libmwaw_tools::RSRC rsrcManager(rsrcStream);
  std::vector<RSRC::Version> listVersion = rsrcManager.getVersionList();
#if 0
  MWAW_DEBUG_MSG(("File::readRSRCInformation: find a resource fork\n"));
#endif
  for (size_t i=0; i < listVersion.size(); i++) {
    switch(listVersion[i].m_id) {
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
  return true;
}

bool File::printResult(std::ostream &o, int verbose) const
{
  if (!canPrintResult(verbose)) return false;
  o << m_fName << ":";
  if (m_fInfoResult.length())
    o << m_fInfoResult;
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
  } else
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

std::string UniStr255ToStr(HFSUniStr255 const &data)
{
  CFStringRef input = CFStringCreateWithCharacters( kCFAllocatorDefault,
                      data.unicode, data.length );
  std::string res("");
  res.reserve(size_t(CFStringGetLength(input))+1);
  CFStringGetCString(input, &res[0], CFStringGetLength(input)+1,
                     kCFStringEncodingMacRoman);

  CFRelease( input );
  return res;
}
}

void usage(char const *fName)
{
  std::cerr << "Syntax error, expect:\n";
  std::cerr << "\t " << fName << " [-vNum] filename\n";
  std::cerr << "\t where filename is the file path\n";
  std::cerr << "\t and Num is the verbose flag\n";
}

int main(int argc, char *const argv[])
{
  int ch, verbose=0;

  while ((ch = getopt(argc, argv, "v:")) != -1) {
    switch (ch) {
    case 'v':
      verbose=atoi(optarg);
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
  } catch(...) {
    std::cerr << argv[0] << ": can not open file " << argv[optind] << "\n";
    if (file) delete file;
    return -1;
  }
  if (!file)
    return -1;
  try {
    file->readDataInformation();
  } catch(...) {
  }
  try {
    file->readRSRCInformation();
  } catch(...) {
  }

  if (verbose >= 4)
    std::cout << *file;
  else if (file->canPrintResult(verbose))
    file->printResult(std::cout, verbose);
  delete file;
  return 0;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
