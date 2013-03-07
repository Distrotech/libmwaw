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
#include <map>
#include <set>
#include <sstream>
#include <string>

#include <libwpd/libwpd.h>

#include "MWAWCell.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWList.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWSubDocument.hxx"

#include "HMWJGraph.hxx"
#include "HMWJText.hxx"

#include "HMWJParser.hxx"

/** Internal: the structures of a HMWJParser */
namespace HMWJParserInternal
{
////////////////////////////////////////
//! Internal: the state of a HMWJParser
struct State {
  //! constructor
  State() : m_zonesListBegin(-1), m_eof(-1), m_zonesMap(),
    m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0) {
  }

  //! the list of zone begin
  long m_zonesListBegin;
  //! end of file
  long m_eof;
  //! a map of entry: zoneId->zone
  std::multimap<long, MWAWEntry> m_zonesMap;

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;
  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a MWParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(HMWJParser &pars, MWAWInputStreamPtr input, int zoneId) :
    MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const {
    if (MWAWSubDocument::operator!=(doc)) return true;
    SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
    if (!sDoc) return true;
    if (m_id != sDoc->m_id) return true;
    return false;
  }

  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }

  //! returns the subdocument \a id
  int getId() const {
    return m_id;
  }
  //! sets the subdocument \a id
  void setId(int vid) {
    m_id = vid;
  }

  //! the parser function
  void parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  //! the subdocument id
  int m_id;
};

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("SubDocument::parse: no listener\n"));
    return;
  }
  if (m_id != 1 && m_id != 2) {
    MWAW_DEBUG_MSG(("SubDocument::parse: unknown zone\n"));
    return;
  }

  assert(m_parser);
  long pos = m_input->tell();
  //reinterpret_cast<HMWJParser *>(m_parser)->sendZone(m_id);
  m_input->seek(pos, WPX_SEEK_SET);
}
}

////////////////////////////////////////////////////////////
// constructor/destructor + basic interface ...
////////////////////////////////////////////////////////////
HMWJParser::HMWJParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWParser(input, rsrcParser, header), m_state(),
  m_pageSpan(), m_graphParser(), m_textParser()
{
  init();
}

HMWJParser::~HMWJParser()
{
}

void HMWJParser::init()
{
  resetListener();
  setAsciiName("main-1");

  m_state.reset(new HMWJParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  m_pageSpan.setMarginTop(0.1);
  m_pageSpan.setMarginBottom(0.1);
  m_pageSpan.setMarginLeft(0.1);
  m_pageSpan.setMarginRight(0.1);

  m_graphParser.reset(new HMWJGraph(*this));
  m_textParser.reset(new HMWJText(*this));
}

bool HMWJParser::sendText(long id, long subId)
{
  return m_textParser->sendText(id, subId);
}
bool HMWJParser::getColor(int colId, int patternId, MWAWColor &color) const
{
  return m_graphParser->getColor(colId, patternId, color);
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
float HMWJParser::pageHeight() const
{
  return float(m_pageSpan.getFormLength()-m_pageSpan.getMarginTop()-m_pageSpan.getMarginBottom()-m_state->m_headerHeight/72.0-m_state->m_footerHeight/72.0);
}

float HMWJParser::pageWidth() const
{
  return float(m_pageSpan.getFormWidth()-m_pageSpan.getMarginLeft()-m_pageSpan.getMarginRight());
}

Vec2f HMWJParser::getPageLeftTop() const
{
  return Vec2f(float(m_pageSpan.getMarginLeft()),
               float(m_pageSpan.getMarginTop()+m_state->m_headerHeight/72.0));
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void HMWJParser::newPage(int number)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!getListener() || m_state->m_actPage == 1)
      continue;
    getListener()->insertBreak(MWAWContentListener::PageBreak);
  }
}

bool HMWJParser::isFilePos(long pos)
{
  if (pos <= m_state->m_eof)
    return true;

  MWAWInputStreamPtr input = getInput();
  long actPos = input->tell();
  input->seek(pos, WPX_SEEK_SET);
  bool ok = long(input->tell()) == pos;
  if (ok) m_state->m_eof = pos;
  input->seek(actPos, WPX_SEEK_SET);
  return ok;
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void HMWJParser::parse(WPXDocumentInterface *docInterface)
{
  assert(getInput().get() != 0);

  if (!checkHeader(0L)) throw(libmwaw::ParseException());
  bool ok = true;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());

    checkHeader(0L);
    ok = createZones();
    if (ok) {
      createDocument(docInterface);
      m_graphParser->sendPageGraphics();

      m_textParser->flushExtra();
      m_graphParser->flushExtra();
    }
    ascii().reset();
  } catch (...) {
    MWAW_DEBUG_MSG(("HMWJParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void HMWJParser::createDocument(WPXDocumentInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getListener()) {
    MWAW_DEBUG_MSG(("HMWJParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  int numPage = m_textParser->numPages();
  if (m_graphParser->numPages() > numPage)
    numPage = m_graphParser->numPages();
  m_state->m_numPages = numPage;

  // create the page list
  std::vector<MWAWPageSpan> pageList;
  MWAWPageSpan ps(m_pageSpan);

  for (int i = 0; i <= m_state->m_numPages; i++) pageList.push_back(ps);

  //
  MWAWContentListenerPtr listen(new MWAWContentListener(*getParserState(), pageList, documentInterface));
  setListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool HMWJParser::createZones()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  if (!readZoneb())
    input->seek(pos+34, WPX_SEEK_SET);
  if (!readZonesList())
    return false;

  libmwaw::DebugStream f;
  std::multimap<long,MWAWEntry>::iterator it =
    m_state->m_zonesMap.begin();
  for ( ; it != m_state->m_zonesMap.end(); it++) {
    if (it->second.begin()<=0) continue;
    readZone(it->second);
  }
  for (it = m_state->m_zonesMap.begin(); it !=m_state->m_zonesMap.end(); it++) {
    if (it->second.begin()<=0) continue;
    MWAWEntry const &zone = it->second;
    if (zone.isParsed()) continue;
    f.str("");
    f << "Entries(" << zone.name() << "):";
    ascii().addPos(zone.begin());
    ascii().addNote(f.str().c_str());
  }

  return false;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////
bool HMWJParser::readZonesList()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  if (!isFilePos(pos+82))
    return false;

  libmwaw::DebugStream f;
  f << "Entries(Zones):";
  long val;
  for (int i = 0; i < 7; i++) { // f0=a000
    val = (long) input->readULong(2);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "Zones(A):";

  for (int i = 0; i < 20; i++) {
    // Zones0: styleZone, Zones2: Ruler, Zones5: Picture+?, sure up to Zonesb=FontNames Zones19=EOF
    long ptr = (long) input->readULong(4);
    if (!ptr) continue;
    bool ok = isFilePos(ptr);
    if (!ok)
      f << "###";
    else if (i != 19) { // i==19: is end of file
      std::stringstream name;
      if (i==0) name << "Style";
      else if (i==5) name << "FrameDef";
      else if (i==11) name << "FontsName";
      else
        name << "JZone" << std::hex << i << std::dec;
      MWAWEntry zone;
      zone.setId(i);
      zone.setBegin(ptr);
      zone.setName(name.str());
      m_state->m_zonesMap.insert
      (std::multimap<long,MWAWEntry>::value_type(zone.id(),zone));
    }
    f << "Zone" << i << "=" << std::hex << ptr << std::dec << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  ascii().addPos(input->tell());
  ascii().addNote("_");

  return m_state->m_zonesMap.size();
}

bool HMWJParser::readZone(MWAWEntry &entry)
{
  if (entry.begin()<=0) {
    MWAW_DEBUG_MSG(("HMWJParser::readZone: can not find the zone\n"));
    return false;
  }

  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  f << "Entries(" << entry.name() << "):";
  int jType = (int) input->readULong(2); // number between 0 and f
  f << "type=" << jType << ",";
  long val = input->readLong(2);
  if (val) f << "f0=" << val << ",";
  entry.setLength((long) input->readULong(4));
  if (entry.length() < 12 || !isFilePos(entry.end())) {
    MWAW_DEBUG_MSG(("HMWJParser::readZone: header seems to short\n"));
    return false;
  }
  entry.setParsed(true);

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  bool done = false;
  switch(entry.id()) {
  case 0:
    done = m_textParser->readStyles(entry);
    break;
  case 1:
  case 2:
  case 3:
  case 4:
  case 9:
  case 10:
    done = readZone4(entry);
    break;
  case 5:
    done = m_graphParser->readFrames(entry);
    break;
  case 11:
    done = m_textParser->readFontNames(entry);
    break;
  default:
    break;
  }

  if (done) return true;

  f.str("");
  f << entry.name() << "[data]:";
  ascii().addPos(pos+8);
  ascii().addNote(f.str().c_str());

  return true;
}


// read the print info data
bool HMWJParser::readPrintInfo(MWAWEntry const &entry)
{
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugFile &asciiFile = ascii();
  long pos = entry.begin();

  if (!isFilePos(entry.end())) {
    MWAW_DEBUG_MSG(("HMWJParser::readPrintInfo: the zone seems too short\n"));
    return false;
  }

  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  entry.setParsed(true);

  f << "Entries(PrintInfo):";
  float margins[4] = {0,0,0,0}; // L, T, R, B
  int dim[4] = {0,0,0,0};
  long val;

  val = (long) input->readULong(2);
  if (val != 1) f << "firstSectNumber=" << val << ",";
  val = (long) input->readULong(2);
  if (val) f << "f0=" << val << ",";
  for (int i = 0; i < 4; i++) dim[i]=int(input->readLong(2));
  f << "paper=[" << dim[1] << "x" << dim[0] << " " << dim[3] << "x" << dim[2] << "],";
  f << "margins?=[";
  for (int i= 0; i < 4; i++) {
    margins[i] = float(input->readLong(4))/65536.f;
    f << margins[i] << ",";
  }
  f << "],";

  // after unknown
  asciiFile.addDelimiter(input->tell(),'|');
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  pos += 44;
  input->seek(pos, WPX_SEEK_SET);
  f.str("");
  f << "PrintInfo(B):";

  // print info
  libmwaw::PrinterInfo info;
  if (!info.read(input)) {
    f << "###";
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    return false;
  }

  Vec2i paperSize = info.paper().size();
  Vec2i pageSize = info.page().size();

  bool useDocInfo = (dim[3]-dim[1]>margins[2]+margins[0]) &&
                    (dim[2]-dim[0]>margins[2]+margins[0]);
  bool usePrintInfo = pageSize.x() > 0 && pageSize.y() > 0 &&
                      paperSize.x() > 0 && paperSize.y() > 0;

  Vec2f lTopMargin(margins[0],margins[1]), rBotMargin(margins[2],margins[3]);
  // define margin from print info
  if (useDocInfo)
    paperSize = Vec2i(dim[3]-dim[1],dim[2]-dim[0]);
  else if (usePrintInfo) {
    lTopMargin= Vec2f(-float(info.paper().pos(0)[0]), -float(info.paper().pos(0)[1]));
    rBotMargin=info.paper().pos(1) - info.page().pos(1);

    // move margin left | top
    float decalX = lTopMargin.x() > 14 ? 14 : 0;
    float decalY = lTopMargin.y() > 14 ? 14 : 0;
    lTopMargin -= Vec2f(decalX, decalY);
    rBotMargin += Vec2f(decalX, decalY);
  }

  // decrease right | bottom
  float rightMarg = rBotMargin.x() -10;
  if (rightMarg < 0) rightMarg=0;
  float botMarg = rBotMargin.y() -50;
  if (botMarg < 0) botMarg=0;

  if (useDocInfo || usePrintInfo) {
    m_pageSpan.setMarginTop(lTopMargin.y()/72.0);
    m_pageSpan.setMarginBottom(botMarg/72.0);
    m_pageSpan.setMarginLeft(lTopMargin.x()/72.0);
    m_pageSpan.setMarginRight(rightMarg/72.0);
    m_pageSpan.setFormLength(paperSize.y()/72.);
    m_pageSpan.setFormWidth(paperSize.x()/72.);

    f << info;
  } else
    f << "###";

  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  if (input->tell()!=entry.end()) {
    asciiFile.addDelimiter(input->tell(),'|');
    input->seek(entry.end(), WPX_SEEK_SET);
  }
  return true;
}


// a small unknown zone
bool HMWJParser::readZoneb()
{
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugFile &asciiFile = ascii();
  long pos=input->tell();
  long endPos=pos+34;

  if (!isFilePos(endPos)) {
    MWAW_DEBUG_MSG(("HMWJParser::readZoneb: the zone seems too short\n"));
    return false;
  }

  libmwaw::DebugStream f;
  f << "Entries(Zoneb):";

  long val = input->readLong(4); // 1c58b4
  f << "dim?=" << float(val)/65536.f << ",";

  for (int i = 0; i < 4; i++) { // always 7,7,0,0
    val = input->readLong(2);
    if (!val) continue;
    f << "f" << i << "=" << val << ",";
  }
  val = input->readLong(4); // 2d5ab ~dim/10.
  f << "dim2?=" << float(val)/65536.f << ",";
  for (int i = 0; i < 4; i++) { // 0,4,0, 0
    val = (long) input->readULong(2);
    if (!val) continue;
    f << "g" << i << "=" << val << ",";
  }
  for (int i = 0; i < 4; i++) { // 1,1,1,0
    val = input->readLong(1);
    if (!val) continue;
    f << "h" << i << "=" << val << ",";
  }
  for (int i = 0; i < 3; i++) { // always 6,0,0
    val = input->readLong(2);
    if (!val) continue;
    f << "j" << i << "=" << val << ",";
  }

  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  if (input->tell()!=endPos) {
    asciiFile.addDelimiter(input->tell(),'|');
    input->seek(endPos, WPX_SEEK_SET);
  }
  return true;
}


bool HMWJParser::readZone4(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("HMWJParser::readZone4: called without any entry\n"));
    return false;
  }
  if (entry.length() < 8) {
    MWAW_DEBUG_MSG(("HMWJParser::readZone4: the entry seems too short\n"));
    return false;
  }

  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugFile &asciiFile = ascii();
  libmwaw::DebugStream f;
  entry.setParsed(true);

  long pos = entry.begin()+8; // skip header
  long endPos = entry.end();
  input->seek(pos, WPX_SEEK_SET);
  int frame=0;
  while (!input->atEOS()) {
    pos = input->tell();
    if (pos==endPos) {
      asciiFile.addPos(endPos);
      asciiFile.addNote("_");
      return true;
    }
    f.str("");
    f << entry.name() << "-" << frame++ << ":";
    long dataSz = (long) input->readULong(4);
    if (pos+4+dataSz>endPos) {
      MWAW_DEBUG_MSG(("HMWJParser::readZone4: can not read an entry\n"));
      f << "###sz=" << dataSz;
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
      return false;
    }
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    if (dataSz) input->seek(dataSz, WPX_SEEK_CUR);
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool HMWJParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = HMWJParserInternal::State();

  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork())
    return false;
  libmwaw::DebugStream f;
  f << "FileHeader:";
  long const headerSize=0x33c;
  if (!isFilePos(headerSize)) {
    MWAW_DEBUG_MSG(("HMWJParser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0,WPX_SEEK_SET);
  int head[3];
  for (int i = 0; i < 3; i++)
    head[i] = (int) input->readULong(2);
  if (head[0] == 0x594c && head[1] == 0x5953 && head[2] == 0x100) {
    MWAW_DEBUG_MSG(("HMWJParser::checkHeader: oops a Japanese file, no parsing\n"));
  } else
    return false;
  int val = (int) input->readLong(1);
  if (val) {
    if (strict) return false;
    if (val==1) f << "hasPassword,";
    else f << "#hasPassword=" << val << ",";
  }
  val = (int) input->readLong(1);
  if (val) {
    if (strict) return false;
    f << "f0=" << val << ",";
  }

  m_state->m_zonesListBegin = 0x460;
  for (int i = 0; i < 4; i++) { // always 0?
    val = (int) input->readLong(2);
    if (val) f << "f" << i+1 << "=" << val << ",";
  }

  long pos;
  // title, subject, author, revision, remark, [2 documents tags], mail:
  int fieldSizes[] = { 128, 128, 32, 32, 256, 36, 64, 64, 64 };
  for (int i = 0; i < 9; i++) {
    pos=input->tell();
    if (i == 5) {
      ascii().addPos(pos);
      ascii().addNote("FileHeader[DocTags]:");
      input->seek(pos+fieldSizes[i], WPX_SEEK_SET);
      pos=input->tell();
      MWAWEntry printInfo;
      printInfo.setBegin(pos);
      printInfo.setLength(164);
      if (!readPrintInfo(printInfo))
        input->seek(pos+164, WPX_SEEK_SET);

      pos=input->tell();
      ascii().addPos(pos);
      ascii().addNote("FileHeader[DocEnd]");
      input->seek(pos+60, WPX_SEEK_SET);
      continue;
    }
    int fSz = (int) input->readULong(1);
    if (fSz >= fieldSizes[i]) {
      if (strict)
        return false;
      MWAW_DEBUG_MSG(("HMWJParser::checkHeader: can not read field size %i\n", i));
      ascii().addPos(pos);
      ascii().addNote("FileHeader#");
      input->seek(pos+fieldSizes[i], WPX_SEEK_SET);
      continue;
    }
    f.str("");
    if (fSz == 0)
      f << "_";
    else {
      std::string name("");
      for (int c = 0; c < fSz; c++)
        name+=(char) input->readULong(1);
      f.str("");
      f << "FileHeader[field"<<i<< "]:" << name;
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+fieldSizes[i], WPX_SEEK_SET);
  }
  pos=input->tell();
  f.str("");
  f << "FileHeader(B):"; // unknown 76 bytes
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(m_state->m_zonesListBegin, WPX_SEEK_SET);
  if (header)
    header->reset(MWAWDocument::HMACJ, 1);

  return true;
}



// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
