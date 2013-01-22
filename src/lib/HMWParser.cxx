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

#include "HMWGraph.hxx"
#include "HMWText.hxx"

#include "HMWParser.hxx"

/** Internal: the structures of a HMWParser */
namespace HMWParserInternal
{
////////////////////////////////////////
//! Internal: the state of a HMWParser
struct State {
  //! constructor
  State() : m_zonesListBegin(-1), m_eof(-1), m_isKoreanFile(true), m_zonesMap(),
    m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0) {
  }

  //! the list of zone begin
  long m_zonesListBegin;
  //! end of file
  long m_eof;
  //! a flag to known if we parse the korean or the japonese version
  bool m_isKoreanFile;
  //! a map of entry: zoneId->zone
  std::multimap<long,shared_ptr<HMWZone> > m_zonesMap;

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;
  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a MWParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(HMWParser &pars, MWAWInputStreamPtr input, int zoneId) :
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
  //reinterpret_cast<HMWParser *>(m_parser)->sendZone(m_id);
  m_input->seek(pos, WPX_SEEK_SET);
}
}

////////////////////////////////////////////////////////////
// constructor/destructor + basic interface ...
////////////////////////////////////////////////////////////
HMWParser::HMWParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWParser(input, rsrcParser, header), m_listener(), m_convertissor(), m_state(),
  m_pageSpan(), m_graphParser(), m_textParser()
{
  init();
}

HMWParser::~HMWParser()
{
}

void HMWParser::init()
{
  m_convertissor.reset(new MWAWFontConverter);
  m_listener.reset();
  setAsciiName("main-1");

  m_state.reset(new HMWParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  m_pageSpan.setMarginTop(0.1);
  m_pageSpan.setMarginBottom(0.1);
  m_pageSpan.setMarginLeft(0.1);
  m_pageSpan.setMarginRight(0.1);

  m_graphParser.reset(new HMWGraph(getInput(), *this, m_convertissor));
  m_textParser.reset(new HMWText(getInput(), *this, m_convertissor));
}

bool HMWParser::isKoreanFile() const
{
  return m_state->m_isKoreanFile;
}

void HMWParser::setListener(MWAWContentListenerPtr listen)
{
  m_listener = listen;
  m_graphParser->setListener(listen);
  m_textParser->setListener(listen);
}

bool HMWParser::sendText(long id, long subId)
{
  return m_textParser->sendText(id, subId);
}
bool HMWParser::getColor(int colId, int patternId, MWAWColor &color) const
{
  return m_graphParser->getColor(colId, patternId, color);
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
float HMWParser::pageHeight() const
{
  return float(m_pageSpan.getFormLength()-m_pageSpan.getMarginTop()-m_pageSpan.getMarginBottom()-m_state->m_headerHeight/72.0-m_state->m_footerHeight/72.0);
}

float HMWParser::pageWidth() const
{
  return float(m_pageSpan.getFormWidth()-m_pageSpan.getMarginLeft()-m_pageSpan.getMarginRight());
}

Vec2f HMWParser::getPageLeftTop() const
{
  return Vec2f(float(m_pageSpan.getMarginLeft()),
               float(m_pageSpan.getMarginTop()+m_state->m_headerHeight/72.0));
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void HMWParser::newPage(int number)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!m_listener || m_state->m_actPage == 1)
      continue;
    m_listener->insertBreak(MWAW_PAGE_BREAK);
  }
}

bool HMWParser::isFilePos(long pos)
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
void HMWParser::parse(WPXDocumentInterface *docInterface)
{
  assert(getInput().get() != 0);

  if (!checkHeader(0L)) throw(libmwaw::ParseException());
  bool ok = true;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());

    checkHeader(0L);
    if (m_state->m_isKoreanFile)
      ok = createKZones();
    else
      ok = createJZones();
    if (ok) {
      createDocument(docInterface);
      m_graphParser->sendPageGraphics();

      m_textParser->flushExtra();
      m_graphParser->flushExtra();
      if (m_listener) m_listener->endDocument();
      m_listener.reset();
    }
    ascii().reset();
  } catch (...) {
    MWAW_DEBUG_MSG(("HMWParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void HMWParser::createDocument(WPXDocumentInterface *documentInterface)
{
  if (!documentInterface) return;
  if (m_listener) {
    MWAW_DEBUG_MSG(("HMWParser::createDocument: listener already exist\n"));
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
  MWAWContentListenerPtr listen(new MWAWContentListener(m_convertissor, pageList, documentInterface));
  setListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool HMWParser::createJZones()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  HMWZone mainZone(input, ascii());
  mainZone.m_type = 11;
  mainZone.setFilePositions(pos, pos+34);
  if (!readZoneb(mainZone))
    input->seek(pos+34, WPX_SEEK_SET);
  if (!readJZonesList())
    return false;

  libmwaw::DebugStream f;
  std::multimap<long,shared_ptr<HMWZone> >::const_iterator it =
    m_state->m_zonesMap.begin();
  for ( ; it != m_state->m_zonesMap.end(); it++) {
    if (!it->second) continue;
    readJZone(it->second);
  }
  for (it = m_state->m_zonesMap.begin(); it !=m_state->m_zonesMap.end(); it++) {
    if (!it->second) continue;
    HMWZone const &zone = *it->second;
    if (zone.m_parsed) continue;
    f.str("");
    f << "Entries(" << zone.name() << "):";
    ascii().addPos(zone.begin());
    ascii().addNote(f.str().c_str());
  }

  return false;
}

bool HMWParser::createKZones()
{
  if (!HMWParser::readKZonesList())
    return false;

  libmwaw::DebugStream f;
  std::multimap<long,shared_ptr<HMWZone> >::iterator it;
  for (it = m_state->m_zonesMap.begin(); it !=m_state->m_zonesMap.end(); it++)
    readKZone(it->second);
  for (it = m_state->m_zonesMap.begin(); it !=m_state->m_zonesMap.end(); it++) {
    shared_ptr<HMWZone> &zone = it->second;
    if (!zone || !zone->valid() || zone->m_parsed)
      continue;
    f.str("");
    f << "Entries(" << std::hex << zone->name() << std::dec << "):";
    zone->ascii().addPos(0);
    zone->ascii().addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////
bool HMWParser::readJZonesList()
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
      shared_ptr<HMWZone> zone(new HMWZone(input, ascii()));
      zone->m_type = -i;
      zone->setFileBeginPos(ptr);
      m_state->m_zonesMap.insert
      (std::multimap<long,shared_ptr<HMWZone> >::value_type(zone->m_id,zone));
    }
    f << "Zone" << i << "=" << std::hex << ptr << std::dec << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  ascii().addPos(input->tell());
  ascii().addNote("_");

  return m_state->m_zonesMap.size();
}

bool HMWParser::readJZone(shared_ptr<HMWZone> zone)
{
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWParser::readJZone: can not find the zone\n"));
    return false;
  }

  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  long pos = zone->begin();
  input->seek(pos, WPX_SEEK_SET);

  zone->m_JType = (int) input->readULong(2); // number between 0 and f
  long val = input->readLong(2);
  if (val) f << "f0=" << val << ",";
  zone->setFileLength((long) input->readULong(4));
  if (zone->length() < 12 || !isFilePos(zone->end())) {
    MWAW_DEBUG_MSG(("HMWParser::readJZone: header seems to short\n"));
    return false;
  }
  zone->m_parsed = true;
  zone->m_extra = f.str();

  bool done = false;
  switch(zone->m_type) {
  case 0:
    zone->m_type = 3;
    done = m_textParser->readStyles(zone);
    break;
  case -11:
    zone->m_type = 5;
    done = m_textParser->readFontNames(zone);
    break;
  default:
    break;
  }

  f.str("");
  f << "Entries(" << zone->name() << "):" << *zone;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  if (done) return true;
  f.str("");
  f << zone->name() << "[data]:";
  ascii().addPos(pos+8);
  ascii().addNote(f.str().c_str());
  return true;
}

bool HMWParser::readKZonesList()
{
  if (m_state->m_zonesListBegin <= 0 || !isFilePos(m_state->m_zonesListBegin)) {
    MWAW_DEBUG_MSG(("HMWParser::readKZonesList: the list entry is not set \n"));
    return false;
  }
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;

  long debZone = m_state->m_zonesListBegin;
  std::set<long> seeDebZone;
  while (debZone) {
    if (seeDebZone.find(debZone) != seeDebZone.end()) {
      MWAW_DEBUG_MSG(("HMWParser::readKZonesList: oops, we have already see this zone\n"));
      break;
    }
    seeDebZone.insert(debZone);
    long pos = debZone;
    input->seek(pos, WPX_SEEK_SET);
    int numZones = int(input->readULong(1));
    f.str("");
    f << "Entries(Zones):";
    f << "N=" << numZones << ",";
    if (!numZones || !isFilePos(pos+16*(numZones+1))) {
      MWAW_DEBUG_MSG(("HMWParser::readKZonesList: can not read the number of zones\n"));
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
      MWAW_DEBUG_MSG(("HMWParser::readKZonesList: can not read the zone begin ptr\n"));
      f << "#ptr=" << std::hex << ptr << std::dec << ",";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      break;
    }
    long nextPtr = long(input->readULong(4));
    if (nextPtr) {
      f << "nextPtr=" << std::hex << nextPtr << std::dec;
      if (!isFilePos(nextPtr)) {
        MWAW_DEBUG_MSG(("HMWParser::readKZonesList: can not read the next zone begin ptr\n"));
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
      shared_ptr<HMWZone> zone(new HMWZone(shared_ptr<libmwaw::DebugFile>(new libmwaw::DebugFile)));
      zone->m_type = int(input->readLong(2));
      val = int(input->readLong(2));
      if (val) f << "f0=" << val << ",";
      zone->setFileBeginPos(long(input->readULong(4)));
      zone->m_id = long(input->readULong(4));
      zone->m_subId = long(input->readULong(4));
      zone->m_extra = f.str();
      f.str("");
      f << "Zones-" << i << ":" << *zone;
      if (!isFilePos(ptr)) {
        MWAW_DEBUG_MSG(("HMWParser::readKZonesList: can not read the %d zone address\n", i));
        f << ",#Ptr";
      } else
        m_state->m_zonesMap.insert
        (std::multimap<long,shared_ptr<HMWZone> >::value_type(zone->m_id,zone));
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

bool HMWParser::readKZone(shared_ptr<HMWZone> zone)
{
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWParser::readKZone: can not find the zone\n"));
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
  if (totalSz != dataSz+12 || !isFilePos(pos+totalSz)) {
    MWAW_DEBUG_MSG(("HMWParser::readKZone: can not read the zone size\n"));
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
bool HMWParser::readPrintInfo(HMWZone &zone)
{
  bool isKorean = isKoreanFile();
  long dataSz = zone.length();
  MWAWInputStreamPtr input = zone.m_input;
  libmwaw::DebugFile &asciiFile = zone.ascii();
  long pos = zone.begin();

  if ((isKorean && dataSz < 192) || !isFilePos(zone.end())) {
    MWAW_DEBUG_MSG(("HMWParser::readPrintInfo: the zone seems too short\n"));
    return false;
  }

  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  zone.m_parsed = true;

  if (isKorean)
    f << zone.name() << "(A):PTR=" << std::hex << zone.fileBeginPos() << std::dec << ",";
  else
    f << "Entries(" << zone.name() << "):";
  float margins[4]; // L, T, R, B
  int dim[4];
  long val;
  if (isKorean) {
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
  } else {
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
  }

  // after unknown
  asciiFile.addDelimiter(input->tell(),'|');
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  pos += isKorean ? 68 : 44;
  input->seek(pos, WPX_SEEK_SET);
  f.str("");
  f << zone.name() << "(B):";
  if (isKorean) {
    long sz = (long) input->readULong(4);
    if (sz < 0x78) {
      MWAW_DEBUG_MSG(("HMWParser::readPrintInfo: the print info data zone seems too short\n"));
      f << "###";
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
      return false;
    }
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
  if (input->tell()!=zone.end()) {
    asciiFile.addDelimiter(input->tell(),'|');
    input->seek(zone.end(), WPX_SEEK_SET);
  }
  return true;
}

// a small unknown zone: link to table, frame?
bool HMWParser::readFramesUnkn(shared_ptr<HMWZone> zone)
{
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWParser::readFramesUnkn: called without any zone\n"));
    return false;
  }
  long dataSz = zone->length();
  if (dataSz < 2) {
    MWAW_DEBUG_MSG(("HMWParser::readFramesUnkn: the zone seems too short\n"));
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
    MWAW_DEBUG_MSG(("HMWParser::readFramesUnkn: the zone size seems odd\n"));
    return false;
  }
  asciiFile.addPos(0);
  asciiFile.addNote(f.str().c_str());

  long pos;
  for (int i = 0; i < N; i++) {
    pos = input->tell();
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
bool HMWParser::readZone6(shared_ptr<HMWZone> zone)
{
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWParser::readZone6: called without any zone\n"));
    return false;
  }

  long dataSz = zone->length();
  if (dataSz < 8) {
    MWAW_DEBUG_MSG(("HMWParser::readZone6: the zone seems too short\n"));
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
      MWAW_DEBUG_MSG(("HMWParser::readZone6: zone%d ptr seems bad\n", st));
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
bool HMWParser::readZone8(shared_ptr<HMWZone> zone)
{
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWParser::readZone8: called without any zone\n"));
    return false;
  }
  long dataSz = zone->length();
  if (dataSz < 78) {
    MWAW_DEBUG_MSG(("HMWParser::readZone8: the zone seems too short\n"));
    return false;
  }

  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->ascii();
  libmwaw::DebugStream f;
  zone->m_parsed = true;

  f << zone->name() << ":PTR=" << std::hex << zone->fileBeginPos() << std::dec << ",";
  input->seek(0, WPX_SEEK_SET);
  long val;
  // find f0=1 (N?), f3=1, f20=8, f22=6, f24=2, f26=144, f28=1, f30=1
  for (int i = 0; i < 39; i++) {
    val = input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }

  asciiFile.addPos(0);
  asciiFile.addNote(f.str().c_str());
  if (!input->atEOS())
    asciiFile.addDelimiter(input->tell(),'|');
  return true;
}

// a small unknown zone
bool HMWParser::readZonea(shared_ptr<HMWZone> zone)
{
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWParser::readZonea: called without any zone\n"));
    return false;
  }
  long dataSz = zone->length();
  if (dataSz < 114) {
    MWAW_DEBUG_MSG(("HMWParser::readZonea: the zone seems too short\n"));
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
bool HMWParser::readZoneb(HMWZone &zone)
{
  bool isKorean = isKoreanFile();
  long dataSz = zone.length();
  MWAWInputStreamPtr input = zone.m_input;
  libmwaw::DebugFile &asciiFile = zone.ascii();
  long pos=zone.begin();

  if ((isKorean && dataSz < 34) || !isFilePos(zone.end())) {
    MWAW_DEBUG_MSG(("HMWParser::readZoneb: the zone seems too short\n"));
    return false;
  }

  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  zone.m_parsed = true;

  if (isKorean)
    f << zone.name() << ":PTR=" << std::hex << zone.fileBeginPos() << std::dec << ",";
  else
    f << "Entries(" << zone.name() << "):";

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
bool HMWParser::readZonec(shared_ptr<HMWZone> zone)
{
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWParser::readZonec: called without any zone\n"));
    return false;
  }
  long dataSz = zone->length();
  if (dataSz < 52) {
    MWAW_DEBUG_MSG(("HMWParser::readZonec: the zone seems too short\n"));
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
shared_ptr<HMWZone> HMWParser::decodeZone(shared_ptr<HMWZone> zone)
{
  if (!zone || zone->fileBeginPos()+12 >= zone->fileEndPos()) {
    MWAW_DEBUG_MSG(("HMWParser::decodeZone: called with an invalid zone\n"));
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
          MWAW_DEBUG_MSG(("HMWParser::decodeZone: find some uncomplete data for zone%lx\n", zone->fileBeginPos()));
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
      short b,c,d;
      if ((c = up[a]) != root) {      /* a pair remains */
        d = up[c];
        b = left[d];
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
    MWAW_DEBUG_MSG(("HMWParser::decodeZone: oops an empty zone\n"));
    zone.reset();
    return zone;
  }

  WPXInputStream *dataInput =
    const_cast<WPXInputStream *>(zone->getBinaryData().getDataStream());
  if (!dataInput) {
    MWAW_DEBUG_MSG(("HMWParser::decodeZone: can not find my input\n"));
    zone.reset();
    return zone;
  }

  zone->m_input.reset(new MWAWInputStream(dataInput, false));
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
bool HMWParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = HMWParserInternal::State();

  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork())
    return false;
  libmwaw::DebugStream f;
  f << "FileHeader:";
  long const headerSize=0x33c;
  if (!isFilePos(headerSize)) {
    MWAW_DEBUG_MSG(("HMWParser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0,WPX_SEEK_SET);
  int head[3];
  for (int i = 0; i < 3; i++)
    head[i] = (int) input->readULong(2);
  if (head[0] == 0x4859 && head[1] == 0x4c53 && head[2] == 0x0210)
    m_state->m_isKoreanFile = true;
  else if (head[0] == 0x594c && head[1] == 0x5953 && head[2] == 0x100) {
    MWAW_DEBUG_MSG(("HMWParser::checkHeader: oops a Japanese file, no parsing\n"));
    m_state->m_isKoreanFile = false; // Japanese
#ifndef DEBUG
    return false;
#endif
  } else
    return false;
  bool isKorean = m_state->m_isKoreanFile;
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

  if (isKorean) {
    m_state->m_zonesListBegin = (int) input->readULong(4); // always 0x042c ?
    if (m_state->m_zonesListBegin<0x14 || !isFilePos(m_state->m_zonesListBegin))
      return false;
    if (m_state->m_zonesListBegin < 0x33c) {
      MWAW_DEBUG_MSG(("HMWParser::checkHeader: the header size seems short\n"));
    }
    f << "zonesBeg=" << std::hex << m_state->m_zonesListBegin << std::dec << ",";
    long fLength = long(input->readULong(4));
    if (fLength < m_state->m_zonesListBegin)
      return false;
    if (!isFilePos(fLength)) {
      if (!isFilePos(fLength/2)) return false;
      MWAW_DEBUG_MSG(("HMWParser::checkHeader: file seems incomplete, try to continue\n"));
      f << "#len=" << std::hex << fLength << std::dec << ",";
    }
    long tLength = long(input->readULong(4));
    f << "textLength=" << tLength << ",";
  } else {
    m_state->m_zonesListBegin = 0x460;
    for (int i = 0; i < 4; i++) { // always 0?
      val = (int) input->readLong(2);
      if (val) f << "f" << i+1 << "=" << val << ",";
    }
  }

  long pos;
  // title, subject, author, revision, remark, [2 documents tags], mail:
  int fieldSizes[] = { 128, 128, 32, 32, 256, isKorean ? 40 : 36, 64, 64, 64 };
  for (int i = 0; i < 9; i++) {
    pos=input->tell();
    if (i == 5) {
      ascii().addPos(pos);
      ascii().addNote("FileHeader[DocTags]:");
      input->seek(pos+fieldSizes[i], WPX_SEEK_SET);
      if (!isKorean) {
        pos=input->tell();
        HMWZone printZone(input, ascii());
        printZone.m_type = 7;
        printZone.setFilePositions(pos, pos+164);
        if (!readPrintInfo(printZone))
          input->seek(pos+164, WPX_SEEK_SET);

        pos=input->tell();
        ascii().addPos(pos);
        ascii().addNote("FileHeader[DocEnd]");
        input->seek(pos+60, WPX_SEEK_SET);
      }
      continue;
    }
    int fSz = (int) input->readULong(1);
    if (fSz >= fieldSizes[i]) {
      if (strict)
        return false;
      MWAW_DEBUG_MSG(("HMWParser::checkHeader: can not read field size %i\n", i));
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
  f << "FileHeader(B):"; // unknown 76(J) or 240(K) bytes
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(m_state->m_zonesListBegin, WPX_SEEK_SET);
  if (header)
    header->reset(MWAWDocument::HMAC, 1);

  return true;
}

////////////////////////////////////////////////////////////
// HMWZone
////////////////////////////////////////////////////////////
HMWZone::HMWZone(MWAWInputStreamPtr input, libmwaw::DebugFile &asciiFile) : m_type(-1), m_JType(-1), m_id(-1), m_subId(-1), m_input(input), m_extra(""), m_parsed(false),
  m_filePos(-1), m_endFilePos(-1), m_data(), m_asciiFile(&asciiFile), m_asciiFilePtr()
{
}

HMWZone::HMWZone(shared_ptr<libmwaw::DebugFile> asciiFile) : m_type(-1), m_JType(-1), m_id(-1), m_subId(-1), m_input(), m_extra(""), m_parsed(false),
  m_filePos(-1), m_endFilePos(-1), m_data(), m_asciiFile(asciiFile.get()), m_asciiFilePtr(asciiFile)
{
}

HMWZone::~HMWZone()
{
  if (m_asciiFilePtr)
    ascii().reset();
}

std::ostream &operator<<(std::ostream &o, HMWZone const &zone)
{
  o << zone.name();
  if (zone.m_id > 0) o << "[" << std::hex << zone.m_id << "]";
  if (zone.m_subId > 0) o << "[subId=" << std::hex << zone.m_subId << "]";
  if (zone.m_extra.length()) o << "," << zone.m_extra;
  return o;
}

std::string HMWZone::name(int type)
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
  if (type > 0)
    s << "Zone" << std::hex << type << std::dec;
  else
    s << "JZone" << std::hex << -type << std::dec;
  return s.str();
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
