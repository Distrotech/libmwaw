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

#include "HMWKGraph.hxx"
#include "HMWKText.hxx"

#include "HMWKParser.hxx"

/** Internal: the structures of a HMWKParser */
namespace HMWKParserInternal
{
////////////////////////////////////////
//! Internal: the state of a HMWKParser
struct State {
  //! constructor
  State() : m_zonesListBegin(-1), m_zonesMap(),
    m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0) {
  }

  //! the list of zone begin
  long m_zonesListBegin;
  //! a map of entry: zoneId->zone
  std::multimap<long,shared_ptr<HMWKZone> > m_zonesMap;

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;
  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a MWParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(HMWKParser &pars, MWAWInputStreamPtr input, long zoneId) :
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

  //! the parser function
  void parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  //! the subdocument id
  long m_id;
};

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("HMWKParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  if (type != libmwaw::DOC_HEADER_FOOTER) {
    MWAW_DEBUG_MSG(("HMWKParserInternal::SubDocument::parse: unexpected document type\n"));
    return;
  }

  assert(m_parser);
  long pos = m_input->tell();
  reinterpret_cast<HMWKParser *>(m_parser)->sendText(m_id, 0);
  m_input->seek(pos, WPX_SEEK_SET);
}
}

////////////////////////////////////////////////////////////
// constructor/destructor + basic interface ...
////////////////////////////////////////////////////////////
HMWKParser::HMWKParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWParser(input, rsrcParser, header), m_state(), m_graphParser(), m_textParser()
{
  init();
}

HMWKParser::~HMWKParser()
{
}

void HMWKParser::init()
{
  resetListener();
  setAsciiName("main-1");

  m_state.reset(new HMWKParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMarginTop(0.1);
  getPageSpan().setMarginBottom(0.1);
  getPageSpan().setMarginLeft(0.1);
  getPageSpan().setMarginRight(0.1);

  m_graphParser.reset(new HMWKGraph(*this));
  m_textParser.reset(new HMWKText(*this));
}

bool HMWKParser::sendText(long id, long subId)
{
  return m_textParser->sendText(id, subId);
}

bool HMWKParser::sendZone(long zId)
{
  MWAWPosition pos(Vec2i(0,0), Vec2i(0,0), WPX_POINT);
  pos.setRelativePosition(MWAWPosition::Char);
  return m_graphParser->sendFrame(zId, pos);
}

bool HMWKParser::getColor(int colId, int patternId, MWAWColor &color) const
{
  return m_graphParser->getColor(colId, patternId, color);
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
Vec2f HMWKParser::getPageLeftTop() const
{
  return Vec2f(float(getPageSpan().getMarginLeft()),
               float(getPageSpan().getMarginTop()+m_state->m_headerHeight/72.0));
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void HMWKParser::newPage(int number)
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

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void HMWKParser::parse(WPXDocumentInterface *docInterface)
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
      std::vector<long> const &tokenIds = m_textParser->getTokenIdList();
      m_graphParser->sendPageGraphics(tokenIds);
      m_textParser->sendMainText();

      m_textParser->flushExtra();
      m_graphParser->flushExtra();
    }
    ascii().reset();
  } catch (...) {
    MWAW_DEBUG_MSG(("HMWKParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void HMWKParser::createDocument(WPXDocumentInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getListener()) {
    MWAW_DEBUG_MSG(("HMWKParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  int numPage = m_textParser->numPages();
  if (m_graphParser->numPages() > numPage)
    numPage = m_graphParser->numPages();
  m_state->m_numPages = numPage;

  // create the page list
  MWAWPageSpan ps(getPageSpan());
  long headerId, footerId;
  m_textParser->getHeaderFooterId(headerId, footerId);
  if (headerId) {
    MWAWHeaderFooter header(MWAWHeaderFooter::HEADER, MWAWHeaderFooter::ALL);
    header.m_subDocument.reset(new HMWKParserInternal::SubDocument
                               (*this, getInput(), headerId));
    ps.setHeaderFooter(header);
  }
  if (footerId) {
    MWAWHeaderFooter footer(MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
    footer.m_subDocument.reset(new HMWKParserInternal::SubDocument
                               (*this, getInput(), footerId));
    ps.setHeaderFooter(footer);
  }

  ps.setPageSpan(m_state->m_numPages+1);
  std::vector<MWAWPageSpan> pageList(1,ps);
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
bool HMWKParser::createZones()
{
  if (!HMWKParser::readZonesList())
    return false;

  libmwaw::DebugStream f;
  std::multimap<long,shared_ptr<HMWKZone> >::iterator it;
  for (it = m_state->m_zonesMap.begin(); it !=m_state->m_zonesMap.end(); ++it)
    readZone(it->second);
  for (it = m_state->m_zonesMap.begin(); it !=m_state->m_zonesMap.end(); ++it) {
    shared_ptr<HMWKZone> &zone = it->second;
    if (!zone || !zone->valid() || zone->m_parsed)
      continue;
    f.str("");
    f << "Entries(" << std::hex << zone->name() << std::dec << "):";
    zone->ascii().addPos(0);
    zone->ascii().addNote(f.str().c_str());
  }

  // retrieve the text type and pass information to text parser
  std::map<long,int> idTypeMap = m_graphParser->getTextFrameInformations();
  m_textParser->updateTextZoneTypes(idTypeMap);

  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////
bool HMWKParser::readZonesList()
{
  MWAWInputStreamPtr input = getInput();
  if (m_state->m_zonesListBegin <= 0 || !input->checkPosition(m_state->m_zonesListBegin)) {
    MWAW_DEBUG_MSG(("HMWKParser::readZonesList: the list entry is not set \n"));
    return false;
  }
  libmwaw::DebugStream f;

  long debZone = m_state->m_zonesListBegin;
  std::set<long> seeDebZone;
  while (debZone) {
    if (seeDebZone.find(debZone) != seeDebZone.end()) {
      MWAW_DEBUG_MSG(("HMWKParser::readZonesList: oops, we have already see this zone\n"));
      break;
    }
    seeDebZone.insert(debZone);
    long pos = debZone;
    input->seek(pos, WPX_SEEK_SET);
    int numZones = int(input->readULong(1));
    f.str("");
    f << "Entries(Zones):";
    f << "N=" << numZones << ",";
    if (!numZones || !input->checkPosition(pos+16*(numZones+1))) {
      MWAW_DEBUG_MSG(("HMWKParser::readZonesList: can not read the number of zones\n"));
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      break;
    }
    int val;
    for (int i = 0; i < 3; i++) {
      val = int(input->readLong(1));
      if (val) f << "f" << i << "=" << val << ",";
    }
    long ptr = long(input->readULong(4));
    if (ptr != debZone) {
      MWAW_DEBUG_MSG(("HMWKParser::readZonesList: can not read the zone begin ptr\n"));
      f << "#ptr=" << std::hex << ptr << std::dec << ",";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      break;
    }
    long nextPtr = long(input->readULong(4));
    if (nextPtr) {
      f << "nextPtr=" << std::hex << nextPtr << std::dec;
      if (!input->checkPosition(nextPtr)) {
        MWAW_DEBUG_MSG(("HMWKParser::readZonesList: can not read the next zone begin ptr\n"));
        nextPtr = 0;
        f << "###";
      }
      f << ",";
    }
    for (int i = 0; i < 2; i++) { // always 0,0?
      val = int(input->readLong(2));
      if (val) f << "f" << i+3 << "=" << val << ",";
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+16, WPX_SEEK_SET);

    for (int i = 0; i < numZones; i++) {
      pos = input->tell();
      f.str("");
      shared_ptr<HMWKZone> zone(new HMWKZone(shared_ptr<libmwaw::DebugFile>(new libmwaw::DebugFile)));
      zone->m_type = int(input->readLong(2));
      val = int(input->readLong(2));
      if (val) f << "f0=" << val << ",";
      zone->setFileBeginPos(long(input->readULong(4)));
      zone->m_id = long(input->readULong(4));
      zone->m_subId = long(input->readULong(4));
      zone->m_extra = f.str();
      f.str("");
      f << "Zones-" << i << ":" << *zone;
      if (!input->checkPosition(ptr)) {
        MWAW_DEBUG_MSG(("HMWKParser::readZonesList: can not read the %d zone address\n", i));
        f << ",#Ptr";
      } else
        m_state->m_zonesMap.insert
        (std::multimap<long,shared_ptr<HMWKZone> >::value_type(zone->m_id,zone));
      ascii().addDelimiter(input->tell(), '|');
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      input->seek(pos+16, WPX_SEEK_SET);
    }

    ascii().addPos(input->tell());
    ascii().addNote("_");
    if (!nextPtr) break;
    debZone = nextPtr;
  }
  return m_state->m_zonesMap.size();
}

bool HMWKParser::readZone(shared_ptr<HMWKZone> zone)
{
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWKParser::readZone: can not find the zone\n"));
    return false;
  }

  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  long pos = zone->fileBeginPos();
  input->seek(pos, WPX_SEEK_SET);
  f << "Entries(" << zone->name() << "):";
  int n = int(input->readLong(2));
  f << "n?=" << n << ",";
  long val = (long) input->readLong(2);
  if (val) f << "unkn=" << val << ",";

  long totalSz = (long) input->readULong(4);
  long dataSz = (long) input->readULong(4);
  if (totalSz != dataSz+12 || !input->checkPosition(pos+totalSz)) {
    MWAW_DEBUG_MSG(("HMWKParser::readZone: can not read the zone size\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  zone->setFileLength(totalSz);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  decodeZone(zone);
  if (!zone->valid())
    return false;

  switch(zone->m_type) {
  case 1:
    if (m_textParser->readTextZone(zone))
      return true;
    break;
  case 2:
    if (m_graphParser->readFrames(zone))
      return true;
    break;
  case 3:
    if (m_textParser->readStyles(zone))
      return true;
    break;
  case 4:
    if (m_textParser->readSections(zone))
      return true;
    break;
  case 5:
    if (m_textParser->readFontNames(zone))
      return true;
    break;
  case 6:
    if (readZone6(zone))
      return true;
    break;
  case 7:
    if (readPrintInfo(*zone))
      return true;
    break;
  case 8:
    if (readZone8(zone))
      return true;
    break;
  case 9:
    if (readFramesUnkn(zone))
      return true;
    break;
  case 0xa:
    if (readZonea(zone))
      return true;
    break;
  case 0xb:
    if (readZoneb(*zone))
      return true;
    break;
  case 0xc:
    if (readZonec(zone))
      return true;
  case 0xd:
    if (m_graphParser->readPicture(zone))
      return true;
    break;
  default:
    break;
  }
  /** type1: text, type7: printInfo: typed: graphic */
  f.str("");
  f << zone->name() << "[data]:PTR=" << std::hex << zone->fileBeginPos() << std::dec << ",";
  zone->ascii().addPos(0);
  zone->ascii().addNote(f.str().c_str());

  return true;
}

// read the print info data
bool HMWKParser::readPrintInfo(HMWKZone &zone)
{
  long dataSz = zone.length();
  MWAWInputStreamPtr input = zone.m_input;
  libmwaw::DebugFile &asciiFile = zone.ascii();
  long pos = zone.begin();

  if (dataSz < 192 || !input->checkPosition(zone.end())) {
    MWAW_DEBUG_MSG(("HMWKParser::readPrintInfo: the zone seems too short\n"));
    return false;
  }

  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  zone.m_parsed = true;

  f << zone.name() << "(A):PTR=" << std::hex << zone.fileBeginPos() << std::dec << ",";
  float margins[4]; // L, T, R, B
  int dim[4];
  long val;
  f << "margins?=[";
  for (int i= 0; i < 4; i++) {
    margins[i] = float(input->readLong(4))/65536.f;
    f << margins[i] << ",";
  }
  f << "],";
  for (int i = 0; i < 4; i++) dim[i]=int(input->readLong(2));
  f << "paper=[" << dim[1] << "x" << dim[0] << " " << dim[3] << "x" << dim[2] << "],";
  val = (long) input->readULong(2);
  if (val != 1) f << "firstSectNumber=" << val << ",";
  val = (long) input->readULong(2);
  if (val) f << "f0=" << val << ",";

  // after unknown
  asciiFile.addDelimiter(input->tell(),'|');
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  pos += 68;
  input->seek(pos, WPX_SEEK_SET);
  f.str("");
  f << zone.name() << "(B):";
  long sz = (long) input->readULong(4);
  if (sz < 0x78) {
    MWAW_DEBUG_MSG(("HMWKParser::readPrintInfo: the print info data zone seems too short\n"));
    f << "###";
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    return false;
  }

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
    getPageSpan().setMarginTop(lTopMargin.y()/72.0);
    getPageSpan().setMarginBottom(botMarg/72.0);
    getPageSpan().setMarginLeft(lTopMargin.x()/72.0);
    getPageSpan().setMarginRight(rightMarg/72.0);
    getPageSpan().setFormLength(paperSize.y()/72.);
    getPageSpan().setFormWidth(paperSize.x()/72.);

    f << info;
  } else
    f << "###";

  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  if (input->tell()!=zone.end()) {
    asciiFile.addDelimiter(input->tell(),'|');
    input->seek(zone.end(), WPX_SEEK_SET);
  }
  return true;
}

// a small unknown zone: link to table, frame?
bool HMWKParser::readFramesUnkn(shared_ptr<HMWKZone> zone)
{
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWKParser::readFramesUnkn: called without any zone\n"));
    return false;
  }
  long dataSz = zone->length();
  if (dataSz < 2) {
    MWAW_DEBUG_MSG(("HMWKParser::readFramesUnkn: the zone seems too short\n"));
    return false;
  }

  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->ascii();
  libmwaw::DebugStream f;
  zone->m_parsed = true;

  f << zone->name() << ":PTR=" << std::hex << zone->fileBeginPos() << std::dec << ",";
  input->seek(0, WPX_SEEK_SET);
  int N= (int) input->readLong(2); // always find val=0, so :-~
  f << "N?=" << N << ",";
  long expectedSz = N*6+2;
  if (expectedSz != dataSz && expectedSz+1 != dataSz) {
    MWAW_DEBUG_MSG(("HMWKParser::readFramesUnkn: the zone size seems odd\n"));
    return false;
  }
  asciiFile.addPos(0);
  asciiFile.addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    long pos = input->tell();
    f.str("");
    f << zone->name() << "-" << i << ":";
    long id = input->readLong(4);
    f << "id=" << std::hex << id << std::dec << ",";
    int type = (int) input->readLong(2);
    switch(type) {
    case 4:
      f << "textbox,";
      break;
    case 6:
      f << "picture,";
      break;
    case 8:
      f << "basicGraphic,";
      break;
    case 9:
      f << "table,";
      break;
    case 10:
      f << "textbox[withHeader],";
      break;
    case 11:
      f << "group";
      break;
    default:
      f << "#type=" << type << ",";
      break;
    }
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos+6, WPX_SEEK_SET);
  }
  if (!input->atEOS())
    asciiFile.addDelimiter(input->tell(),'|');
  return true;
}

// a unknown zone
bool HMWKParser::readZone6(shared_ptr<HMWKZone> zone)
{
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWKParser::readZone6: called without any zone\n"));
    return false;
  }

  long dataSz = zone->length();
  if (dataSz < 8) {
    MWAW_DEBUG_MSG(("HMWKParser::readZone6: the zone seems too short\n"));
    return false;
  }

  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->ascii();
  libmwaw::DebugStream f;
  zone->m_parsed = true;
  long pos=0;
  input->seek(pos, WPX_SEEK_SET);

  // no sure, checkme
  for (int st = 0; st < 2; st++) {
    pos = input->tell();
    long sz = (long) input->readULong(4);
    if (pos+sz+4 > dataSz) {
      MWAW_DEBUG_MSG(("HMWKParser::readZone6: zone%d ptr seems bad\n", st));
      return false;
    }

    f.str("");
    if (st==0)
      f << zone->name() << "(A):PTR=" << std::hex << zone->fileBeginPos() << std::dec << ",";
    else
      f << zone->name() << "(B):";

    asciiFile.addDelimiter(input->tell(),'|');
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos+4+sz, WPX_SEEK_SET);
  }
  return true;
}

// a small unknown zone
bool HMWKParser::readZone8(shared_ptr<HMWKZone> zone)
{
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWKParser::readZone8: called without any zone\n"));
    return false;
  }
  long dataSz = zone->length();
  if (dataSz < 78) {
    MWAW_DEBUG_MSG(("HMWKParser::readZone8: the zone seems too short\n"));
    return false;
  }

  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->ascii();
  libmwaw::DebugStream f;
  zone->m_parsed = true;

  f << zone->name() << ":PTR=" << std::hex << zone->fileBeginPos() << std::dec << ",";
  input->seek(0, WPX_SEEK_SET);
  // find f0=1 (N?), f3=1, f20=8, f22=6, f24=2, f26=144, f28=1, f30=1
  for (int i = 0; i < 39; i++) {
    long val = input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }

  asciiFile.addPos(0);
  asciiFile.addNote(f.str().c_str());
  if (!input->atEOS())
    asciiFile.addDelimiter(input->tell(),'|');
  return true;
}

// a small unknown zone
bool HMWKParser::readZonea(shared_ptr<HMWKZone> zone)
{
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWKParser::readZonea: called without any zone\n"));
    return false;
  }
  long dataSz = zone->length();
  if (dataSz < 114) {
    MWAW_DEBUG_MSG(("HMWKParser::readZonea: the zone seems too short\n"));
    return false;
  }

  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->ascii();
  libmwaw::DebugStream f;
  zone->m_parsed = true;

  f << zone->name() << ":PTR=" << std::hex << zone->fileBeginPos() << std::dec << ",";
  input->seek(0, WPX_SEEK_SET);
  long val;
  for (int i = 0; i < 40; i++) { // always 0 ?
    val = input->readLong(2);
    if (!val) continue;
    f << "f" << i << "=" << val << ",";
  }
  for (int i = 0; i < 3; i++) { // g0=g1=g2=a665
    val = (long) input->readULong(2);
    if (!val) continue;
    f << "g" << i << "=" << std::hex << val << std::dec << ",";
  }
  for (int i = 0; i < 14; i++) { // h5=h6=h8=h9=-1
    val = input->readLong(2);
    if (!val) continue;
    f << "h" << i << "=" << val << ",";
  }

  asciiFile.addPos(0);
  asciiFile.addNote(f.str().c_str());
  if (!input->atEOS())
    asciiFile.addDelimiter(input->tell(),'|');
  return true;
}

// a small unknown zone
bool HMWKParser::readZoneb(HMWKZone &zone)
{
  long dataSz = zone.length();
  MWAWInputStreamPtr input = zone.m_input;
  libmwaw::DebugFile &asciiFile = zone.ascii();
  long pos=zone.begin();

  if (dataSz < 34 || !input->checkPosition(zone.end())) {
    MWAW_DEBUG_MSG(("HMWKParser::readZoneb: the zone seems too short\n"));
    return false;
  }

  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  zone.m_parsed = true;

  f << zone.name() << ":PTR=" << std::hex << zone.fileBeginPos() << std::dec << ",";

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
  if (dataSz >= 36) {
    val = input->readLong(2);
    if (val) f << "j3=" << val << ",";
  }

  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  if (input->tell()!=zone.end()) {
    asciiFile.addDelimiter(input->tell(),'|');
    input->seek(zone.end(), WPX_SEEK_SET);
  }
  return true;
}

// a small unknown zone
bool HMWKParser::readZonec(shared_ptr<HMWKZone> zone)
{
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWKParser::readZonec: called without any zone\n"));
    return false;
  }
  long dataSz = zone->length();
  if (dataSz < 52) {
    MWAW_DEBUG_MSG(("HMWKParser::readZonec: the zone seems too short\n"));
    return false;
  }

  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->ascii();
  libmwaw::DebugStream f;
  zone->m_parsed = true;

  f << zone->name() << ":PTR=" << std::hex << zone->fileBeginPos() << std::dec << ",";
  input->seek(0, WPX_SEEK_SET);
  long val = input->readLong(2); // 1|4
  if (val != 1) f << "f0=" << val << ",";
  for (int j = 0; j < 5; j++) { // always 0 expect f2=0|800
    val = input->readLong(2);
    if (val) f << "f" << j+1 << "=" << val << ",";
  }
  f << "id=" << std::hex << input->readULong(4) << std::dec << ",";
  for (int j = 0; j < 6; j++) { // always 0
    val = input->readLong(2);
    if (val) f << "g" << j << "=" << val << ",";
  }
  // two similar number: selection?
  long sel[2];
  for (int j = 0; j < 2; j++)
    sel[j] = input->readLong(4);
  if (sel[0] || sel[1]) {
    f << "sel?=" << sel[0];
    if (sel[1] != sel[0]) f << "<->" << sel[1] << ",";
    f << ",";
  }
  for (int j = 0; j < 8; j++) { // always 0
    val = input->readLong(2);
    if (val) f << "h" << j << "=" << val << ",";
  }
  asciiFile.addDelimiter(input->tell(),'|');
  asciiFile.addPos(0);
  asciiFile.addNote(f.str().c_str());

  input->seek(52, WPX_SEEK_SET);
  return true;
}
////////////////////////////////////////////////////////////
// code to uncompress a zone
////////////////////////////////////////////////////////////
/* implementation of a basic splay tree to decode a block
   freely inspired from: ftp://ftp.cs.uiowa.edu/pub/jones/compress/minunsplay.c :

   Author: Douglas Jones, Dept. of Comp. Sci., U. of Iowa, Iowa City, IA 52242.
   Date: Nov. 5, 1990.
         (derived from the Feb. 14 1990 version by stripping out irrelevancies)
         (minor revision of Feb. 20, 1989 to add exit(0) at end of program).
         (minor revision of Nov. 14, 1988 to detect corrupt input better).
         (minor revision of Aug. 8, 1988 to eliminate unused vars, fix -c).
   Copyright:  This material is derived from code Copyrighted 1988 by
         Jeffrey Chilton and Douglas Jones.  That code contained a copyright
         notice allowing copying for personal or research purposes, so long
         as copies of the code were not sold for direct commercial advantage.
         This version of the code has been stripped of most of the material
         added by Jeff Chilton, and this release of the code may be used or
         copied for any purpose, public or private.
   Patents:  The algorithm central to this code is entirely the invention of
         Douglas Jones, and it has not been patented.  Any patents claiming
         to cover this material are invalid.
   Exportability:  Splay-tree based compression algorithms may be used for
         cryptography, and when used as such, they may not be exported from
         the United States without appropriate approval.  All cryptographic
         features of the original version of this code have been removed.
   Language: C
   Purpose: Data uncompression program, a companion to minsplay.c
   Algorithm: Uses a splay-tree based prefix code.  For a full understanding
          of the operation of this data compression scheme, refer to the paper
          "Applications of Splay Trees to Data Compression" by Douglas W. Jones
          in Communications of the ACM, Aug. 1988, pages 996-1007.
*/
shared_ptr<HMWKZone> HMWKParser::decodeZone(shared_ptr<HMWKZone> zone)
{
  if (!zone || zone->fileBeginPos()+12 >= zone->fileEndPos()) {
    MWAW_DEBUG_MSG(("HMWKParser::decodeZone: called with an invalid zone\n"));
    return zone;
  }
  short const maxChar=256;
  short const maxSucc=maxChar+1;
  short const twoMaxChar=2*maxChar+1;
  short const twoMaxSucc=2*maxSucc;

  // first build the tree data
  short left[maxSucc];
  short right[maxSucc];
  short up[twoMaxSucc];
  for (short i = 0; i <= twoMaxChar; ++i)
    up[i] = i/2;
  for (short j = 0; j <= maxChar; ++j) {
    left[j] = short(2 * j);
    right[j] = short(2 * j + 1);
  }

  short const root = 0;
  short const sizeBit = 8;
  short const highBit=128; /* mask for the most sig bit of 8 bit byte */

  short bitbuffer = 0;       /* buffer to hold a byte for unpacking bits */
  short bitcounter = 0;  /* count of remaining bits in buffer */

  MWAWInputStreamPtr input = getInput();
  input->seek(zone->fileBeginPos()+12, WPX_SEEK_SET);
  WPXBinaryData &dt = zone->getBinaryData();
  while (!input->atEOS() && input->tell() < zone->fileEndPos()) {
    short a = root;
    bool ok = true;
    do {  /* once for each bit on path */
      if(bitcounter == 0) {
        if (input->atEOS() || input->tell() >= zone->fileEndPos()) {
          MWAW_DEBUG_MSG(("HMWKParser::decodeZone: find some uncomplete data for zone%lx\n", zone->fileBeginPos()));
          dt.append((unsigned char)a);
          ok = false;
          break;
        }

        bitbuffer = (short) input->readULong(1);
        bitcounter = sizeBit;
      }
      --bitcounter;
      if ((bitbuffer & highBit) != 0)
        a = right[a];
      else
        a = left[a];
      bitbuffer = short(bitbuffer << 1);
    } while (a <= maxChar);
    if (!ok)
      break;
    dt.append((unsigned char)(a - maxSucc));

    /* now splay tree about leaf a */
    do {    /* walk up the tree semi-rotating pairs of nodes */
      short c;
      if ((c = up[a]) != root) {      /* a pair remains */
        short d = up[c];
        short b = left[d];
        if (c == b) {
          b = right[d];
          right[d] = a;
        } else
          left[d] = a;
        if (left[c] == a)
          left[c] = b;
        else
          right[c] = b;
        up[a] = d;
        up[b] = c;
        a = d;
      } else
        a = c;
    } while (a != root);
  }
  if (dt.size()==0) {
    MWAW_DEBUG_MSG(("HMWKParser::decodeZone: oops an empty zone\n"));
    zone.reset();
    return zone;
  }

  zone->m_input=MWAWInputStream::get(zone->getBinaryData(), false);
  if (!zone->m_input) {
    MWAW_DEBUG_MSG(("HMWKParser::decodeZone: can not find my input\n"));
    zone.reset();
    return zone;
  }

  zone->m_input->seek(0,WPX_SEEK_SET);
  zone->ascii().setStream(zone->m_input);
  static int fId = 0;
  std::stringstream s;
  s << zone->name() << "-" << fId++;
  zone->ascii().open(s.str());

  ascii().skipZone(zone->fileBeginPos()+12, zone->fileEndPos()-1);
  return zone;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool HMWKParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = HMWKParserInternal::State();

  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork())
    return false;
  libmwaw::DebugStream f;
  f << "FileHeader:";
  long const headerSize=0x33c;
  if (!input->checkPosition(headerSize)) {
    MWAW_DEBUG_MSG(("HMWKParser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0,WPX_SEEK_SET);
  int head[3];
  for (int i = 0; i < 3; i++)
    head[i] = (int) input->readULong(2);
  if (head[0] != 0x4859 || head[1] != 0x4c53 || head[2] != 0x0210)
    return false;
  int val = (int) input->readLong(1);
  if (val==1) f << "hasPassword,";
  else if (val) {
    if (strict) return false;
    f << "#hasPassword=" << val << ",";
  }
  val = (int) input->readLong(1);
  if (val) {
    if (strict && (val<0||val>2)) return false;
    f << "f0=" << val << ",";
  }

  m_state->m_zonesListBegin = (int) input->readULong(4); // always 0x042c ?
  if (m_state->m_zonesListBegin<0x14 || !input->checkPosition(m_state->m_zonesListBegin))
    return false;
  if (m_state->m_zonesListBegin < 0x33c) {
    MWAW_DEBUG_MSG(("HMWKParser::checkHeader: the header size seems short\n"));
  }
  f << "zonesBeg=" << std::hex << m_state->m_zonesListBegin << std::dec << ",";
  long fLength = long(input->readULong(4));
  if (fLength < m_state->m_zonesListBegin)
    return false;
  if (!input->checkPosition(fLength)) {
    if (!input->checkPosition(fLength/2)) return false;
    MWAW_DEBUG_MSG(("HMWKParser::checkHeader: file seems incomplete, try to continue\n"));
    f << "#len=" << std::hex << fLength << std::dec << ",";
  }
  long tLength = long(input->readULong(4));
  f << "textLength=" << tLength << ",";
  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  long pos;
  // title, subject, author, revision, remark, [2 documents tags], mail:
  int fieldSizes[] = { 128, 128, 32, 32, 256, 40, 64, 64, 64 };
  for (int i = 0; i < 9; i++) {
    pos=input->tell();
    if (i == 5) {
      ascii().addPos(pos);
      ascii().addNote("FileHeader[DocTags]:");
      input->seek(pos+fieldSizes[i], WPX_SEEK_SET);
      continue;
    }
    int fSz = (int) input->readULong(1);
    if (fSz >= fieldSizes[i]) {
      if (strict)
        return false;
      MWAW_DEBUG_MSG(("HMWKParser::checkHeader: can not read field size %i\n", i));
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
  f << "FileHeader(B):"; // 240(K) bytes
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(m_state->m_zonesListBegin, WPX_SEEK_SET);
  if (header)
    header->reset(MWAWDocument::HMAC, 1);

  return true;
}

////////////////////////////////////////////////////////////
// HMWKZone
////////////////////////////////////////////////////////////
HMWKZone::HMWKZone(MWAWInputStreamPtr input, libmwaw::DebugFile &asciiFile) : m_type(-1), m_id(-1), m_subId(-1), m_input(input), m_extra(""), m_parsed(false),
  m_filePos(-1), m_endFilePos(-1), m_data(), m_asciiFile(&asciiFile), m_asciiFilePtr()
{
}

HMWKZone::HMWKZone(shared_ptr<libmwaw::DebugFile> asciiFile) : m_type(-1), m_id(-1), m_subId(-1), m_input(), m_extra(""), m_parsed(false),
  m_filePos(-1), m_endFilePos(-1), m_data(), m_asciiFile(asciiFile.get()), m_asciiFilePtr(asciiFile)
{
}

HMWKZone::~HMWKZone()
{
  if (m_asciiFilePtr)
    ascii().reset();
}

std::ostream &operator<<(std::ostream &o, HMWKZone const &zone)
{
  o << zone.name();
  if (zone.m_id > 0) o << "[" << std::hex << zone.m_id << std::dec << "]";
  if (zone.m_subId > 0) o << "[subId=" << std::hex << zone.m_subId << std::dec << "]";
  if (zone.m_extra.length()) o << "," << zone.m_extra;
  return o;
}

std::string HMWKZone::name(int type)
{
  switch(type) {
  case 1:
    return "TextZone";
  case 2:
    return "FrameDef";
  case 3:
    return "Style";
  case 4:
    return "Section";
  case 5:
    return "FontsName";
  case 7:
    return "PrintInfo";
  case 9:
    return "FrameExt";
  case 0xd:
    return "Picture";
  default:
    break;
  }
  std::stringstream s;
  s << "Zone" << std::hex << type << std::dec;
  return s.str();
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
