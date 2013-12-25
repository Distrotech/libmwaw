/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */

/* libmwaw
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
* Copyright (C) 2002 William Lachance (wrlach@gmail.com)
* Copyright (C) 2002,2004 Marc Maurer (uwog@uwog.net)
* Copyright (C) 2004-2006 Fridrich Strba (fridrich.strba@bluewin.ch)
* Copyright (C) 2006, 2007 Andrew Ziem
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

#include <iomanip>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWList.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSpreadsheetListener.hxx"
#include "MWAWSubDocument.hxx"

#include "BWStructManager.hxx"

#include "BWSSParser.hxx"

/** Internal: the structures of a BWSSParser */
namespace BWSSParserInternal
{
//! Internal: the spreadsheet of a BWSSParser
struct Spreadsheet {
//! constructor
  Spreadsheet() : m_numRows(0), m_lastReadRow(-1)
  {
  }
//! the number of rows
  int m_numRows;
//! the last read rows
  int m_lastReadRow;
};

////////////////////////////////////////
//! Internal: the state of a BWSSParser
struct State {
  //! constructor
  State() :  m_spreadsheetBegin(-1), m_typeEntryMap(),
    m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0)
  {
  }
  /** the spreadsheet begin position */
  long m_spreadsheetBegin;
  /** the type entry map */
  std::multimap<std::string, MWAWEntry> m_typeEntryMap;

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};
}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
BWSSParser::BWSSParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWSpreadsheetParser(input, rsrcParser, header), m_state(), m_structureManager()
{
  init();
}

BWSSParser::~BWSSParser()
{
}

void BWSSParser::init()
{
  resetSpreadsheetListener();
  setAsciiName("main-1");

  m_state.reset(new BWSSParserInternal::State);
  m_structureManager.reset(new BWStructManager(getParserState()));

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);
}

MWAWInputStreamPtr BWSSParser::rsrcInput()
{
  return getRSRCParser()->getInput();
}

libmwaw::DebugFile &BWSSParser::rsrcAscii()
{
  return getRSRCParser()->ascii();
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
Vec2f BWSSParser::getPageLeftTop() const
{
  return Vec2f(float(getPageSpan().getMarginLeft()),
               float(getPageSpan().getMarginTop()+m_state->m_headerHeight/72.0));
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void BWSSParser::newPage(int number)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!getSpreadsheetListener() || m_state->m_actPage == 1)
      continue;
    getSpreadsheetListener()->insertBreak(MWAWSpreadsheetListener::PageBreak);
  }
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void BWSSParser::parse(librevenge::RVNGSpreadsheetInterface */*docInterface*/)
{
  assert(getInput().get() != 0);
  if (!checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = false;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());

    checkHeader(0L);
    ok = createZones();
    if (ok) {
      MWAW_DEBUG_MSG(("BWSSParser::parse: sending result is not implemented\n"));
      ok = false;
    }
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("BWSSParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetSpreadsheetListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void BWSSParser::createDocument(librevenge::RVNGSpreadsheetInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getSpreadsheetListener()) {
    MWAW_DEBUG_MSG(("BWSSParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  int numPages = 1;
  m_state->m_numPages = numPages;

  std::vector<MWAWPageSpan> pageList;
  MWAWPageSpan ps(getPageSpan());
  ps.setPageSpan(numPages);
  pageList.push_back(ps);

  MWAWSpreadsheetListenerPtr listen(new MWAWSpreadsheetListener(*getParserState(), pageList, documentInterface));
  setSpreadsheetListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool BWSSParser::createZones()
{
  readRSRCZones();
  MWAWInputStreamPtr input = getInput();
  if (input->seek(66, librevenge::RVNG_SEEK_SET) || !readPrintInfo())
    return false;
  long pos = input->tell();
  if (!input->checkPosition(pos+70)) {
    MWAW_DEBUG_MSG(("BWSSParser::createZones: the file can not contains Zones\n"));
    return false;
  }

  // now read the list of zones
  libmwaw::DebugStream f;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "Entries(Zones):";
  for (int i=0; i<7; ++i) { // checkme: at least 2 zones, maybe 7
    MWAWEntry entry;
    entry.setBegin(input->readLong(4));
    entry.setLength(input->readLong(4));
    entry.setId((int) input->readLong(2));
    if (entry.length()==0) continue;
    entry.setType(i==1?"Frame":"Unknown");
    f << entry.type() << "[" << entry.id() << "]="
      << std::hex << entry.begin() << "<->" << entry.end() << ",";
    if (!entry.valid() || !input->checkPosition(entry.end())) {
      f << "###";
      if (i<2) {
        MWAW_DEBUG_MSG(("BWSSParser::createZones: can not read the header/footer zone, stop\n"));
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());
        return false;
      }
      MWAW_DEBUG_MSG(("BWSSParser::createZones: can not zones entry %d\n",i));
      continue;
    }
    m_state->m_typeEntryMap.insert
    (std::multimap<std::string, MWAWEntry>::value_type(entry.type(),entry));
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  // now parse the different zones
  std::multimap<std::string, MWAWEntry>::iterator it;
  it=m_state->m_typeEntryMap.find("FontNames");
  if (it!=m_state->m_typeEntryMap.end())
    m_structureManager->readFontNames(it->second);
  it=m_state->m_typeEntryMap.find("Frame");
  if (it!=m_state->m_typeEntryMap.end())
    m_structureManager->readFrame(it->second);

  // now parse the different zones
  for (it=m_state->m_typeEntryMap.begin(); it!=m_state->m_typeEntryMap.end(); ++it) {
    MWAWEntry const &entry=it->second;
    if (entry.isParsed())
      continue;
    f.str("");
    f << "Entries(" << entry.type() << ")[" << entry.id() << "]:";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
    ascii().addPos(entry.end());
    ascii().addNote("_");
  }

  input->seek(m_state->m_spreadsheetBegin, librevenge::RVNG_SEEK_SET);
  pos = input->tell();
  if (!readDocumentInfo())
    input->seek(pos, librevenge::RVNG_SEEK_SET);
  pos = input->tell();
  if (!readChartZone())
    input->seek(pos, librevenge::RVNG_SEEK_SET);
  if (!readSpreadsheet())
    return false;
  if (readZone0() && readZone1() && readZone0())
    readZone2();
  /* normally ends with a zone of size 25
     with looks like 01010000010000000000000000007cffff007d0100007c0000
                or   01010001010000000000000000000000000001000100000000
     some flags + selection?
   */
  ascii().addPos(input->tell());
  ascii().addNote("Entries(ZoneEnd)");
  return true;
}

bool BWSSParser::readRSRCZones()
{
  MWAWRSRCParserPtr rsrcParser = getRSRCParser();
  if (!rsrcParser)
    return true;

  std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::iterator it;
  // the 1 zone
  char const *(zNames[]) = {"wPos", "DMPF", "edtp" };
  for (int z = 0; z < 3; ++z) {
    it = entryMap.lower_bound(zNames[z]);
    while (it != entryMap.end()) {
      if (it->first != zNames[z])
        break;
      MWAWEntry const &entry = it++->second;
      switch (z) {
      case 0: // 1001
        m_structureManager->readwPos(entry);
        break;
      case 1: // find in one file with id=4661 6a1f 4057
        m_structureManager->readFontStyle(entry);
        break;
      case 2: {
        librevenge::RVNGBinaryData data;
        m_structureManager->readPicture(entry.id(), data);
        break;
      }
      /* find also
         - edpt: see sendPicture
         - DMPP: the paragraph style
         - sect and alis: position?, alis=filesystem alias(dir, filename, path...)
      */
      default:
        break;
      }
    }
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool BWSSParser::readPrintInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  if (!input->checkPosition(pos+0x70))
    return false;

  libmwaw::DebugStream f;
  // print info
  libmwaw::PrinterInfo info;
  if (!info.read(input)) return false;
  f << "Entries(PrintInfo):"<< info;

  Vec2i paperSize = info.paper().size();
  Vec2i pageSize = info.page().size();
  if (pageSize.x() <= 0 || pageSize.y() <= 0 ||
      paperSize.x() <= 0 || paperSize.y() <= 0) return false;

  // define margin from print info
  Vec2i lTopMargin= -1 * info.paper().pos(0);
  Vec2i rBotMargin=info.paper().pos(1) - info.page().pos(1);

  // move margin left | top
  int decalX = lTopMargin.x() > 14 ? lTopMargin.x()-14 : 0;
  int decalY = lTopMargin.y() > 14 ? lTopMargin.y()-14 : 0;
  lTopMargin -= Vec2i(decalX, decalY);
  rBotMargin += Vec2i(decalX, decalY);

  // decrease right | bottom
  int rightMarg = rBotMargin.x() -10;
  if (rightMarg < 0) rightMarg=0;
  int botMarg = rBotMargin.y() -50;
  if (botMarg < 0) botMarg=0;

  getPageSpan().setMarginTop(lTopMargin.y()/72.0);
  getPageSpan().setMarginBottom(botMarg/72.0);
  getPageSpan().setMarginLeft(lTopMargin.x()/72.0);
  getPageSpan().setMarginRight(rightMarg/72.0);
  getPageSpan().setFormLength(paperSize.y()/72.);
  getPageSpan().setFormWidth(paperSize.x()/72.);

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(pos+0x78, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != pos+0x78) {
    MWAW_DEBUG_MSG(("BWSSParser::readPrintInfo: file is too short\n"));
    return false;
  }
  ascii().addPos(input->tell());

  return true;
}

////////////////////////////////////////////////////////////
// document header
////////////////////////////////////////////////////////////
bool BWSSParser::readDocumentInfo()
{
  MWAWInputStreamPtr &input= getInput();
  long pos=input->tell();
  if (!input->checkPosition(pos+92+512)) {
    MWAW_DEBUG_MSG(("BWSSParser::readDocumentInfo: can not find the spreadsheet zone\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(DocInfo):";
  // the preferences
  int val=(int) input->readLong(2);
  if (val!=0x2e) f << "f0=" << val << ",";
  val=(int) input->readLong(2);
  if (val!=0xa) f << "f1=" << val << ",";
  std::string what("");
  for (int i=0; i < 4; ++i) // pref
    what+=(char) input->readLong(1);
  f << what << ",";
  for (int i=0; i < 3; ++i) { // always 0
    val=(int) input->readLong(2);
    if (val) f << "f" << i+2 << "=" << val << ",";
  }
  f << "ids=[";
  for (int i=0; i < 2; i++) {
    long id=(long) input->readULong(4);
    f << std::hex << id << std::dec << ",";
  }
  f << "],";
  val=(int) input->readULong(2); // 0|22d8|4ead|e2c8
  if (val)
    f << "fl?=" << std::hex << val << std::dec << ",";
  for (int i=0; i < 8; i++) {
    static int const(expectedValues[])= {1,4/*or 2*/,3,2,2,1,1,1 };
    val=(int) input->readLong(1);
    if (val!=expectedValues[i])
      f << "g" << i << "=" << val << ",";
  }
  for (int i=0; i < 8; ++i) { // 1,a|e, 0, 21, 3|4, 6|7|9, d|13, 3|5: related to font?
    val=(int) input->readLong(2);
    if (val)
      f << "h" << i << "=" << val << ",";
  }
  val=(int) input->readULong(2); //0|10|3e|50|c8|88|98
  if (val)
    f << "h8=" <<  std::hex << val << std::dec << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  // document info
  pos=input->tell();
  f.str("");
  f << "DocInfo[dosu]:";
  val=(int) input->readLong(2);
  if (val!=0x226) f << "f0=" << val << ",";
  val=(int) input->readLong(2);
  if (val!=1) f << "f1=" << val << ",";
  what="";
  for (int i=0; i < 4; ++i) // dosu
    what+=(char) input->readLong(1);
  f << what << ",";
  for (int i=0; i < 3; ++i) { // always 961, 0, 0
    val=(int) input->readLong(2);
    if ((i==0 && val!=0x961) || (i&&val))
      f << "f" << i+2 << "=" << val << ",";
  }
  f << "ids=[";
  for (int i=0; i < 2; ++i) {
    long id=(long) input->readULong(4);
    f << std::hex << id << std::dec << ",";
  }
  f << "],";
  long margins[4];
  f << "margins=[";
  for (int i=0; i < 4; ++i) {
    margins[i]=(int) input->readLong(4);
    f << margins[i] << ",";
  }
  f << "],";
  for (int i=0; i < 4; ++i) { // 1,1,1,0 4 flags ?
    val = (int) input->readLong(1);
    if (val!=1) f << "fl" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int st=0; st<2; ++st) {
    pos=input->tell();
    f.str("");
    if (st==0)
      f << "DocInfo[header]:";
    else
      f << "DocInfo[footer]:";
    int fSz = (int) input->readULong(1);
    std::string name("");
    for (int i=0; i<fSz; ++i)
      name+=(char) input->readULong(1);
    f << name;
    input->seek(pos+256, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// spreadsheet
////////////////////////////////////////////////////////////
bool BWSSParser::readRowSheet(BWSSParserInternal::Spreadsheet &sheet)
{
  MWAWInputStreamPtr &input= getInput();
  long pos=input->tell();
  libmwaw::DebugStream f;
  f << "Entries(Row):";
  int row=(int)input->readLong(2);
  long fSz=(long) input->readULong(4);
  long endPos=pos+6+fSz;
  if (fSz<18 || row <= sheet.m_lastReadRow || row >= sheet.m_numRows || !input->checkPosition(endPos)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  sheet.m_lastReadRow=row;
  f << "row=" << row << ",";
  int val=(int) input->readLong(2);
  if (val && val!=int(fSz))
    f << "#sz=" << val << ",";
  val=(int) input->readLong(2);
  if (val!=-1)
    f << "height=" << val << ",";
  input->seek(10, librevenge::RVNG_SEEK_CUR); // junk
  int N=(int) input->readLong(2)+1;
  f << "N=" << N << ",";
  val=(int) input->readLong(2);
  if (val!=fSz) // size if sz is not set, if not a small number?
    f << "unkn=" << val << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(pos+6+18, librevenge::RVNG_SEEK_SET);
  for (int i=0; i < N; ++i) {
    pos=input->tell();
    if (pos==endPos) break;
    int cSize=(int) input->readULong(1);
    f.str("");
    if (i==0)
      f << "Entries(Cell)[0]:";
    else
      f << "Cell-" << i << ":";
    if (cSize&1) cSize++;
    if (pos+2+cSize>endPos) {
      MWAW_DEBUG_MSG(("BWSSParser::readRowSheet: can not find some cell\n"));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      break;
    }
    ascii().addDelimiter(input->tell(),'|');
    input->seek(pos+2+cSize, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

bool BWSSParser::readChartZone()
{
  MWAWInputStreamPtr &input= getInput();
  long pos=input->tell();
  if (!input->checkPosition(pos+10)) {
    MWAW_DEBUG_MSG(("BWSSParser::readChartZone: can not find the chart zone\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(Chart):";
  pos = input->tell();
  for (int i=0; i < 4; ++i) {
    static int const expectedValues[5]= {1, 0xF, 0x4000, 0x100 };
    int val= (int) input->readULong(2);
    if (val != expectedValues[i])
      f << "f" << i+1 << "=" << std::hex << val << std::dec << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  while (!input->isEnd()) {
    pos=input->tell();
    long sz=(long) input->readULong(2);
    if (sz==0) {
      ascii().addPos(pos);
      ascii().addNote("Chart:_");
      return true;
    }
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    if (!readChart())
      return false;
  }
  return true;
}

bool BWSSParser::readChart()
{
  MWAWInputStreamPtr &input= getInput();
  long pos=input->tell();
  long sz=(long) input->readULong(2);
  if (!sz || !input->checkPosition(pos+sz+0x5f)) {
    MWAW_DEBUG_MSG(("BWSSParser::readChart: can not find the chart zone\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Chart-header:";
  int val=(int) input->readLong(2);
  if (val!=0x12) f << "f0=" << val << ",";
  long endZone1 = pos+sz+2;
  for (int i=0; i<2; ++i) {
    long actPos=input->tell();
    int dSz=(int) input->readULong(2);
    int sSz=(int) input->readULong(1);
    if (actPos+2+dSz > endZone1 || (sSz+1!=dSz && sSz+2!=dSz)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    std::string name("");
    for (int c=0; c<sSz; ++c)
      name += (char) input->readULong(1);
    f << "\"" << name << "\",";
    input->seek(actPos+2+dSz, librevenge::RVNG_SEEK_SET);
  }
  ascii().addDelimiter(input->tell(),'|');
  input->seek(endZone1, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  ascii().addPos(pos);
  ascii().addNote("Chart-B:");
  input->seek(pos+0x5d, librevenge::RVNG_SEEK_SET);
  return true;
}

bool BWSSParser::readSpreadsheet()
{
  MWAWInputStreamPtr &input= getInput();
  long pos=input->tell();
  if (!input->checkPosition(pos+9)) {
    MWAW_DEBUG_MSG(("BWSSParser::readSpreadsheet: can not find the spreadsheet zone\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(Spreadsheet):";
  int val= (int) input->readLong(2);
  if (val!=7)
    f << "f1=" << val << ",";
  BWSSParserInternal::Spreadsheet sheet;
  sheet.m_numRows=(int) input->readLong(2)+1;
  f << "num[row]=" << sheet.m_numRows << ",";
  val= (int) input->readLong(2);
  if (val!=-1)
    f << "f2=" << val << ",";
  for (int i=0; i < 3; ++i) { // g0=0|73|89, other 0
    val=(int) input->readULong(1);
    if (val)
      f << "g" << i << "=" << std::hex << val << std::dec << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  while (readRowSheet(sheet)) {
    if (input->isEnd()) break;
  }
  return true;
}

bool BWSSParser::readZone0()
{
  MWAWInputStreamPtr &input= getInput();
  long pos=input->tell();
  libmwaw::DebugStream f;
  f << "Entries(Zone0):";
  int N=(int) input->readULong(2);
  if (!input->checkPosition(pos+8+3*N)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  f << "unkn=[";
  for (int i=0; i < N; ++i) {
    f << input->readLong(2) << ":" << std::hex << input->readULong(1) << std::dec << ",";
  }
  f << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

bool BWSSParser::readZone1()
{
  MWAWInputStreamPtr &input= getInput();
  long pos=input->tell();
  libmwaw::DebugStream f;
  f << "Entries(Zone1):";
  int N=(int) input->readLong(2);
  f << "N=" << N << ",";
  int val=(int) input->readLong(2);
  if (val) f << "f0=" << val << ",";
  int dSz=(int) input->readULong(2);
  if (N<-1 || (N>=0 && dSz<=0) || !input->checkPosition(pos+6+(N+1)*dSz)) {
    MWAW_DEBUG_MSG(("BWSSParser::Zone&: header seems odd\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (int i=0; i <= N; ++i) {
    pos = input->tell();
    f.str("");
    f << "Zone1-" << i << ":";
    input->seek(pos+dSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool BWSSParser::readZone2()
{
  MWAWInputStreamPtr &input=getInput();
  libmwaw::DebugStream f;
  int id=0;
  while (!input->isEnd()) {
    long pos=input->tell();
    if (!input->checkPosition(pos+6))
      break;
    f.str("");
    if (id==0)
      f << "Entries(Zone2):";
    else
      f << "Zone2-" << id << ":";
    ++id;
    int row=(int) input->readULong(2);
    int col=(int) input->readULong(2);
    if (row==0x4000 && col==0x4000) {
      f << "last,";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return true;
    }
    f << "pos=" << row << "x" << col << ",";
    int dataSz=(int) input->readULong(2);
    if (!dataSz || !input->checkPosition(pos+6+dataSz)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    ascii().addDelimiter(input->tell(),'|');
    input->seek(pos+6+dataSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// resource fork data
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool BWSSParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = BWSSParserInternal::State();
  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork() || !input->checkPosition(66))
    return false;

  libmwaw::DebugStream f;
  f << "FileHeader:";

  input->seek(0, librevenge::RVNG_SEEK_SET);
  if (input->readLong(2)!=0x4257 || input->readLong(2)!=0x6b73 ||
      input->readLong(2)!=0x4257 || input->readLong(2)!=0x7373 ||
      input->readLong(2)!=0x4257 || input->readLong(2)!=0x7373) {
    return false;
  }
  for (int i=0; i < 9; ++i) { // f2=f6=1 other 0
    int val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  setVersion(1);

  if (header)
    header->reset(MWAWDocument::MWAW_T_BEAGLEWORKS, 1, MWAWDocument::MWAW_K_SPREADSHEET);

  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  long pos=input->tell();
  f.str("");
  f << "FileHeader-II:";
  m_state->m_spreadsheetBegin=input->readLong(4);
  if (!input->checkPosition(m_state->m_spreadsheetBegin)) {
    MWAW_DEBUG_MSG(("BWSSParser::checkHeader: can not read the spreadsheet position\n"));
    return false;
  }
  f << "spreadsheet[ptr]=" << std::hex << m_state->m_spreadsheetBegin << std::dec << ",";
  for (int i=0; i < 11; ++i) { // f2=0x50c|58c|5ac f3=f5=9
    long val=input->readLong(2);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  MWAWEntry entry;
  entry.setBegin(input->readLong(4));
  entry.setLength(input->readLong(4));
  entry.setId((int) input->readLong(2)); // in fact nFonts
  entry.setType("FontNames");
  f << "fontNames[ptr]=" << std::hex << entry.begin() << "<->" << entry.end()
    << std::dec << ",nFonts=" << entry.id() << ",";
  if (entry.length() && (!entry.valid() || !input->checkPosition(entry.end()))) {
    MWAW_DEBUG_MSG(("BWSSParser::checkHeader: can not read the font names position\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }

  m_state->m_typeEntryMap.insert
  (std::multimap<std::string, MWAWEntry>::value_type(entry.type(),entry));
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  if (strict && !readPrintInfo())
    return false;
  ascii().addPos(66);
  ascii().addNote("_");

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
