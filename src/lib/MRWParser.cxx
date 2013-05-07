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

#include <string.h>

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
#include "MWAWSection.hxx"
#include "MWAWSubDocument.hxx"

#include "MRWGraph.hxx"
#include "MRWText.hxx"

#include "MRWParser.hxx"

/** Internal: the structures of a MRWParser */
namespace MRWParserInternal
{
////////////////////////////////////////
//! Internal: the struct used to store the zone of a MRWParser
struct Zone {
  //! a enum to define the diffent zone type
  enum Type { Z_Main, Z_Footnote, Z_Header, Z_Footer, Z_Unknown };
  //! constructor
  Zone() : m_id(-1), m_fileId(0), m_type(Z_Unknown), m_endNote(false), m_height(0), m_RBpos(0,0), m_dim(),
    m_pageDim(), m_pageTextDim(), m_section(), m_backgroundColor(MWAWColor::white()), m_extra("") {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Zone const &zone);
  //! the zone id
  int m_id;
  //! the file zone id
  uint32_t m_fileId;
  //! the zone type
  Type m_type;
  //! a flag to know if this an endnote
  bool m_endNote;
  //! height of the zone
  long m_height;
  //! rigth/bottom position
  Vec2l m_RBpos;
  //! the zone total position
  Box2l m_dim;
  //! the page dimension (?)
  Box2i m_pageDim;
  //! the zone of text dimension ( ie page less margins)
  Box2i m_pageTextDim;
  //! the section
  MWAWSection m_section;
  //! the background color
  MWAWColor m_backgroundColor;
  //! extra data
  std::string m_extra;
};

std::ostream &operator<<(std::ostream &o, Zone const &zone)
{
  switch(zone.m_type) {
  case Zone::Z_Main:
    o << "main,";
    break;
  case Zone::Z_Footnote:
    if (zone.m_endNote)
      o <<  "endnote,";
    else
      o << "footnote,";
    break;
  case Zone::Z_Header:
    o << "header,";
    break;
  case Zone::Z_Footer:
    o << "footer,";
    break;
  case Zone::Z_Unknown:
  default:
    break;
  }
  if (zone.m_type==Zone::Z_Header || zone.m_type==Zone::Z_Footer) {
    switch(zone.m_fileId) {
    case 0:
      break; // main
    case 1:
      o << "left,";
      break;
    case 2:
      o << "right,";
      break;
    case 3:
      o << "firstpage,";
      break;
    default:
      o << "#fileId" << zone.m_fileId << ",";
      break;
    }
  } else if (zone.m_fileId & 0xFFFFFF)
    o << "fileId=" << std::hex << (zone.m_fileId&0xFFFFFF) << std::dec << ",";
  if (zone.m_RBpos[0] || zone.m_RBpos[1])
    o << "RBpos=" << zone.m_RBpos << ",";
  if (zone.m_height)
    o << "height=" << zone.m_height << ",";
  if (zone.m_dim.size()[0] || zone.m_dim.size()[1])
    o << "dim=" << zone.m_dim << ",";
  if (!zone.m_backgroundColor.isWhite())
    o << "background=" << zone.m_backgroundColor << ",";
  o << zone.m_extra;
  return o;
}
////////////////////////////////////////
//! Internal: the state of a MRWParser
struct State {
  //! constructor
  State() : m_eof(-1), m_zonesList(), m_fileToZoneMap(),
    m_actPage(0), m_numPages(0), m_firstPageFooter(false), m_hasOddEvenHeaderFooter(false), m_headerHeight(0), m_footerHeight(0) {
  }

  //! end of file
  long m_eof;
  //! the list of zone
  std::vector<Zone> m_zonesList;
  //! a map fileZoneId -> localZoneId
  std::map<uint32_t,int> m_fileToZoneMap;
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;
  /** a flag to know if we have a first page footer */
  bool m_firstPageFooter;
  /** a flag to know if we have odd/even header footer */
  bool m_hasOddEvenHeaderFooter;
  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a MWParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(MRWParser &pars, MWAWInputStreamPtr input, int zoneId) :
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
  int m_id;
};

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("SubDocument::parse: no listener\n"));
    return;
  }
  assert(m_parser);
  long pos = m_input->tell();
  reinterpret_cast<MRWParser *>(m_parser)->sendText(m_id);
  m_input->seek(pos, WPX_SEEK_SET);
}
}

////////////////////////////////////////////////////////////
// constructor/destructor + basic interface ...
////////////////////////////////////////////////////////////
MRWParser::MRWParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWParser(input, rsrcParser, header), m_state(), m_pageMarginsSpanSet(false), m_graphParser(), m_textParser()
{
  init();
}

MRWParser::~MRWParser()
{
}

void MRWParser::init()
{
  resetListener();
  setAsciiName("main-1");

  m_state.reset(new MRWParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);

  m_graphParser.reset(new MRWGraph(*this));
  m_textParser.reset(new MRWText(*this));
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
Vec2f MRWParser::getPageLeftTop() const
{
  return Vec2f(float(getPageSpan().getMarginLeft()),
               float(getPageSpan().getMarginTop()+m_state->m_headerHeight/72.0));
}

MWAWSection MRWParser::getSection(int zId) const
{
  if (zId >= 0 && zId < int(m_state->m_zonesList.size()))
    return m_state->m_zonesList[size_t(zId)].m_section;
  return MWAWSection();
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void MRWParser::newPage(int number)
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

bool MRWParser::isFilePos(long pos)
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
// interface
////////////////////////////////////////////////////////////
int MRWParser::getZoneId(uint32_t fileId, bool &endNote)
{
  if (m_state->m_fileToZoneMap.find(fileId)==m_state->m_fileToZoneMap.end()) {
    MWAW_DEBUG_MSG(("MRWParser::getZoneId: can not find zone for %x\n", fileId));
    return -1;
  }
  int id=m_state->m_fileToZoneMap.find(fileId)->second;
  endNote=false;
  if (id>=0 && id < int(m_state->m_zonesList.size()))
    endNote = m_state->m_zonesList[size_t(id)].m_endNote;
  return id;
}

void MRWParser::sendText(int zoneId)
{
  MWAWInputStreamPtr input = getInput();
  long actPos = input->tell();
  m_textParser->send(zoneId);
  input->seek(actPos, WPX_SEEK_SET);
}

float MRWParser::getPatternPercent(int id) const
{
  return m_graphParser->getPatternPercent(id);
}

void MRWParser::sendToken(int zoneId, long tokenId, MWAWFont const &actFont)
{
  MWAWInputStreamPtr input = getInput();
  long actPos = input->tell();
  m_graphParser->sendToken(zoneId, tokenId, actFont);
  input->seek(actPos, WPX_SEEK_SET);
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void MRWParser::parse(WPXDocumentInterface *docInterface)
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
      m_textParser->send(0);

#ifdef DEBUG
      m_textParser->flushExtra();
      m_graphParser->flushExtra();
#endif
    }
    ascii().reset();
  } catch (...) {
    MWAW_DEBUG_MSG(("MRWParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void MRWParser::createDocument(WPXDocumentInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getListener()) {
    MWAW_DEBUG_MSG(("MRWParser::createDocument: listener already exist\n"));
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
  if (m_state->m_zonesList.size())
    ps.setBackgroundColor(m_state->m_zonesList[0].m_backgroundColor);

  // look for an header/footer
  int headerId[4] = { -1, -1, -1, -1}, footerId[4] = { -1, -1, -1, -1};
  for (size_t z = 0; z < m_state->m_zonesList.size(); z++) {
    MRWParserInternal::Zone const &zone=m_state->m_zonesList[z];
    if (zone.m_type==MRWParserInternal::Zone::Z_Header) {
      if (zone.m_fileId < 4)
        headerId[zone.m_fileId]=int(z);
    } else if (zone.m_type==MRWParserInternal::Zone::Z_Footer) {
      if (zone.m_fileId < 4)
        footerId[zone.m_fileId]=int(z);
    }
  }
  MWAWPageSpan firstPs(ps);
  if (m_state->m_firstPageFooter) {
    if (headerId[3]>0) {
      MWAWHeaderFooter header(MWAWHeaderFooter::HEADER, MWAWHeaderFooter::ALL);
      header.m_subDocument.reset(new MRWParserInternal::SubDocument(*this, getInput(), int(headerId[3])));
      firstPs.setHeaderFooter(header);
    }
    if (footerId[3]>0) {
      MWAWHeaderFooter footer(MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
      footer.m_subDocument.reset(new MRWParserInternal::SubDocument(*this, getInput(), int(footerId[3])));
      firstPs.setHeaderFooter(footer);
    }
  }
  for (int st = 0; st < 2; st++) {
    MWAWHeaderFooter::Occurence what=
      !m_state->m_hasOddEvenHeaderFooter ? MWAWHeaderFooter::ALL : st==0 ? MWAWHeaderFooter::ODD : MWAWHeaderFooter::EVEN;
    int which= !m_state->m_hasOddEvenHeaderFooter ? 0 : 1+st;
    if (headerId[which]>0) {
      MWAWHeaderFooter header(MWAWHeaderFooter::HEADER, what);
      header.m_subDocument.reset(new MRWParserInternal::SubDocument(*this, getInput(), int(headerId[which])));
      ps.setHeaderFooter(header);
    }
    if (footerId[which]>0) {
      MWAWHeaderFooter footer(MWAWHeaderFooter::FOOTER, what);
      footer.m_subDocument.reset(new MRWParserInternal::SubDocument(*this, getInput(), int(footerId[which])));
      ps.setHeaderFooter(footer);
    }
    if (!m_state->m_hasOddEvenHeaderFooter)
      break;
  }

  std::vector<MWAWPageSpan> pageList;
  if (m_state->m_firstPageFooter) {
    pageList.push_back(firstPs);
    ps.setPageSpan(m_state->m_numPages);
  } else
    ps.setPageSpan(m_state->m_numPages+1);
  if (ps.getPageSpan())
    pageList.push_back(ps);
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

// ------ read the different zones ---------
bool MRWParser::createZones()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  int actZone=-1;
  while (readZone(actZone))
    pos = input->tell();
  ascii().addPos(pos);
  ascii().addNote("Entries(Loose)");
  return m_state->m_zonesList.size();
}

bool MRWParser::readZone(int &actZone, bool onlyTest)
{
  MWAWInputStreamPtr input = getInput();
  if (input->atEOS())
    return false;
  long pos = input->tell();
  MRWEntry zone;
  if (!readEntryHeader(zone)) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  libmwaw::DebugStream f;
  f << "Entries(" << zone.name() << "):" << zone;

  bool done = false;
  switch(zone.m_fileType) {
  case -1: // separator
  case -2: // last file
    done = readSeparator(zone);
    if (onlyTest)
      break;
    actZone++;
    break;
  case 0:
    done = readZoneHeader(zone, actZone, onlyTest);
    break;
  case 1:
    done = m_textParser->readTextStruct(zone, actZone);
    break;
  case 2:
    done = m_textParser->readZone(zone, actZone);
    break;
  case 4:
  case 5:
    done = m_textParser->readPLCZone(zone, actZone);
    break;
  case 6:
    done = m_textParser->readFonts(zone, actZone);
    break;
  case 7:
    done = m_textParser->readRulers(zone, actZone);
    break;
  case 8:
    done = m_textParser->readFontNames(zone, actZone);
    break;
  case 9: // zone size
    done = readZoneDim(zone, actZone);
    break;
  case 0xa: // zone size with margins removal
    done = readZoneDim(zone, actZone);
    break;
  case 0xb: // border dim?
    done = readZoneb(zone, actZone);
    break;
  case 0xc:
    done = readZonec(zone, actZone);
    break;
  case 0xf:
    done = readDocInfo(zone, actZone);
    break;
  case 0x13:
    done = readZone13(zone, actZone);
    break;
  case 0x14:
    done = m_graphParser->readToken(zone, actZone);
    break;
  case 0x1a:
    done = m_textParser->readStyleNames(zone, actZone);
    break;
  case 0x1f:
    done = readPrintInfo(zone);
    break;
  case 0x24:
    done = readCPRT(zone);
    break;
    /* 0x41a: docInfo */
  case 0x420:
    done = m_graphParser->readPostscript(zone, actZone);
    break;
  default:
    break;
  }
  if (done) {
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(zone.end(), WPX_SEEK_SET);
    return true;
  }
  if (onlyTest)
    return false;
  input->seek(zone.begin(), WPX_SEEK_SET);
  input->pushLimit(zone.end());
  std::vector<MRWStruct> dataList;
  decodeZone(dataList);
  input->popLimit();

  size_t numData = dataList.size();
  f << "numData=" << numData << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  int numDataByField = zone.m_fileType==1 ? 22 : 10;
  for (size_t d = 0; d < numData; d++) {
    MRWStruct const &dt = dataList[d];
    if ((int(d)%numDataByField)==0) {
      if (d)
        ascii().addNote(f.str().c_str());
      f.str("");
      f << zone.name() << "-" << d << ":";
      ascii().addPos(dt.m_filePos);
    }
    f << dt << ",";
  }
  if (numData)
    ascii().addNote(f.str().c_str());

  if (input->tell() != zone.end()) {
    f.str("");
    if (input->tell() == zone.end()-1)
      f << "_";
    else
      f << zone.name() << ":###";
    ascii().addPos(input->tell());
    ascii().addNote(f.str().c_str());
  }
  input->seek(zone.end(), WPX_SEEK_SET);
  return true;
}

// --------- zone separator ----------

// read the zone separator
bool MRWParser::readSeparator(MRWEntry const &entry)
{
  if (entry.length() < 0x3) {
    MWAW_DEBUG_MSG(("MRWParser::readSeparator: data seems to short\n"));
    return false;
  }
  MWAWInputStreamPtr input = getInput();
  input->seek(entry.begin(), WPX_SEEK_SET);
  input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  decodeZone(dataList);
  input->popLimit();

  if (dataList.size() != 1) {
    MWAW_DEBUG_MSG(("MRWParser::readSeparator: can find my data\n"));
    return false;
  }

  MRWStruct const &data = dataList[0]; // always 0x77aa
  libmwaw::DebugStream f;
  f << entry.name() << "[data]:";
  if (data.m_data.size() != 1 || data.m_data[0] != 0x77aa)
    f << "#" << data;
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());

  return true;
}

bool MRWParser::readZoneHeader(MRWEntry const &entry, int actId, bool onlyTest)
{
  if (entry.length() < 3) {
    MWAW_DEBUG_MSG(("MRWParser::readZoneHeader: data seems to short\n"));
    return false;
  }
  MWAWInputStreamPtr input = getInput();
  input->seek(entry.begin(), WPX_SEEK_SET);
  input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  decodeZone(dataList);
  input->popLimit();

  size_t numData = dataList.size();
  if (numData < 47) {
    MWAW_DEBUG_MSG(("MRWParser::readZoneHeader: find unexpected number of data\n"));
    return false;
  }
  if (onlyTest)
    return true;
  size_t d = 0;
  long val;
  libmwaw::DebugStream f;
  MRWParserInternal::Zone zone;
  for (int j = 0; j < 47; j++) {
    MRWStruct const &data = dataList[d++];
    if (!data.isBasic()) {
      f << "###f" << j << "=" << data << ",";
      continue;
    }
    long sel[4]= {0,0,0,0};
    int dim[4]= {0,0,0,0};
    unsigned char color[3];
    switch(j) {
    case 0: // version?
      f << "vers?=" << (data.value(0)>>16) << "[" << (data.value(0)&0xFFFF) << "],";
      break;
    case 1:
      val = data.value(0);
      if (val>>16) // 0 or 1000
        f << "f1[high]=" << std::hex << (val>>16) << std::dec << ",";
      if ((val&0xFFFF)!=1)
        f << "f1[low]=" << (val& 0xFFFF) << ",";
      break;
    case 2: { // find 8[01]0[0145]8120 or a0808120
      uint32_t high = uint32_t(data.value(0))>>16;
      // high&4: no odd/even header/footer ?
      if (high) f << "fl2[high]=" << std::hex << high << std::dec << ",";
      uint32_t low = uint32_t(data.value(0))&0xFFFF;
      if (low) f << "fl2[low]=" << std::hex << low << std::dec << ",";
      break;
    }
    case 3: { // 80000[01] or 800000[01]
      uint32_t value=uint32_t(data.value(0));
      if (value&0x2000000) {
        zone.m_endNote=true;
        value &= ~uint32_t(0x2000000);
      }
      // value&1: firstPage[header/footer] ?
      if (value)
        f << "fl3=" << std::hex << value << std::dec << ",";
      break;
    }
    case 4: {
      uint32_t v = uint32_t(data.value(0));
      switch(v>>28) {
      case 0: // main
        zone.m_type = MRWParserInternal::Zone::Z_Main;
        break;
      case 0xc:
        zone.m_type = MRWParserInternal::Zone::Z_Header;
        break;
      case 0xd:
        zone.m_type = MRWParserInternal::Zone::Z_Footer;
        break;
      case 0xe:
        zone.m_type = MRWParserInternal::Zone::Z_Footnote;
        break;
      default:
        f << "#type=" << (v>>28) << ",";
        break;
      }
      zone.m_fileId=(v&0xFFFFFFF);
      m_state->m_fileToZoneMap[v]=actId;
      break;
    }
    case 9: // real bottom margin
      if (data.value(0))
        f << "y[bottom+footer?]=" << data.value(0) << ",";
      break;
    case 10:
      zone.m_height = data.value(0);
      break;
    case 13: // right margin
    case 14: // bottom margin
      zone.m_RBpos[j-13] = data.value(0);
      break;
    case 21:
      if (data.value(0))
        f << "h[act]=" << data.value(0) << ",";
      break;
    case 5: // 0|14|90
    case 6: // 0|11-14
    case 15: // -2: main?|-1
    case 16: // -2: main?|-1
    case 25: // 0|43
    case 41: // 0|5|26
      if (data.value(0))
        f << "f" << j << "=" << data.value(0) << ",";
      break;
    case 7: // two units ?
      if (data.value(0) != 0x480048)
        f << "f" << j << "=" << std::hex << data.value(0) << std::dec << ",";
      break;
    case 8: // always -2?
      if (data.value(0) != -2)
        f << "#" << f << j << "=" << data.value(0) << ",";
      break;
    case 12: // always 16
      if (data.value(0) != 16)
        f << "#" << f << j << "=" << data.value(0) << ",";
      break;
    case 17:
    case 18:
    case 19:
    case 20:
      sel[j-17] = data.value(0);
      while (j<20)
        sel[++j-17] = dataList[d++].value(0);
      if (sel[0]||sel[1])
        f << "sel0=" << std::hex << sel[0] << "x" << sel[1] << std::dec << ",";
      if (sel[2]||sel[3])
        f << "sel1=" << std::hex << sel[2] << "x" << sel[3] << std::dec << ",";
      break;
    case 28: // left margin
    case 29: // top margin
    case 30:
    case 31:
      dim[j-28] = (int) data.value(0);
      while (j<31)
        dim[++j-28] = (int) dataList[d++].value(0);
      zone.m_dim = Box2l(Vec2l(dim[1],dim[0]), Vec2l(dim[3],dim[2]));
      break;
    case 32:
    case 33:
    case 34: { // 35,36,37: front color?
      color[0]=color[1]=color[2]=0xFF;
      color[j-32]=(unsigned char) (data.value(0)>>8);
      while (j < 34)
        color[++j-32] = (unsigned char) (dataList[d++].value(0)>>8);
      zone.m_backgroundColor = MWAWColor(color[0],color[1],color[2]);
      break;
    }
    case 38:
      if (data.value(0)!=1)
        f << "#f" << j << "=" << data.value(0) << ",";
      break;
    case 39:
      if (data.value(0) && data.value(0)!=0x4d4d4242) // MMBB: creator
        f << "#creator=" << std::hex << uint32_t(data.value(0)) << std::dec << ",";
      break;
    case 40: // normally 0 or pretty small, but can also be very very big
      if (data.value(0))
        f << "f" << j << "=" << std::hex << data.value(0) << std::dec << ",";
      break;
    case 42:
      if (data.value(0)!=43)
        f << "#f" << j << "=" << data.value(0) << ",";
      break;
    case 44:
      if (data.value(0))
        f << "sel[pt]=" << std::hex << data.value(0) << std::dec << ",";
      break;
    case 45:
      if (data.value(0)!=72)
        f << "#f" << j << "=" << data.value(0) << ",";
      break;
    default:
      if (data.value(0))
        f << "#f" << j << "=" << data.value(0) << ",";
      break;
    }
  }
  while (d < numData) {
    f << "#f" << d << "=" << dataList[d] << ",";
    d++;
  }
  zone.m_extra = f.str();
  if (actId < 0) {
    MWAW_DEBUG_MSG(("MRWParser::readZoneHeader: called with negative id\n"));
  } else {
    if (actId >= int(m_state->m_zonesList.size()))
      m_state->m_zonesList.resize(size_t(actId)+1);
    m_state->m_zonesList[size_t(actId)] = zone;
  }
  f.str("");
  f << entry.name() << ":" << zone;

  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  return true;
}

bool MRWParser::readZoneDim(MRWEntry const &entry, int zoneId)
{
  if (entry.length() < entry.m_N) {
    MWAW_DEBUG_MSG(("MRWParser::readZoneDim: data seems to short\n"));
    return false;
  }
  MWAWInputStreamPtr input = getInput();
  input->seek(entry.begin(), WPX_SEEK_SET);
  input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  decodeZone(dataList, 1+4*entry.m_N);
  input->popLimit();

  if (int(dataList.size()) != 4*entry.m_N) {
    MWAW_DEBUG_MSG(("MRWParser::readZoneDim: find unexpected number of data\n"));
    return false;
  }

  libmwaw::DebugStream f;
  size_t d = 0;
  std::vector<int> colPos;
  for (int i = 0; i < entry.m_N; i++) {
    f.str("");
    f << entry.name() << "-" << i << ":";
    ascii().addPos(dataList[d].m_filePos);

    int dim[4] = { 0, 0, 0, 0 };
    for (int j = 0; j < 4; j++) {
      MRWStruct const &data = dataList[d++];
      if (!data.isBasic()) {
        MWAW_DEBUG_MSG(("MRWParser::readZoneDim: find unexpected dim data type\n"));
        f << "###dim" << j << "=" << data << ",";
      } else
        dim[j] = (int) data.value(0);
    }
    // checkme
    Box2i dimension(Vec2i(dim[1],dim[0]), Vec2i(dim[3],dim[2]));
    f << "pos=" << dimension << ",";
    bool dimOk=dim[0] >= 0 && dim[0] < dim[2] && dim[1] >= 0 && dim[1] < dim[3];
    if (i==0 && dimOk) {
      if (zoneId < 0 || zoneId >= int(m_state->m_zonesList.size())) {
        MWAW_DEBUG_MSG(("MRWParser::readZoneDim: can not find the zone storage\n"));
      } else if (entry.m_fileType == 9)
        m_state->m_zonesList[size_t(zoneId)].m_pageDim = dimension;
      else if (entry.m_fileType == 0xa)
        m_state->m_zonesList[size_t(zoneId)].m_pageTextDim = dimension;
      else {
        MWAW_DEBUG_MSG(("MRWParser::readZoneDim: unknown zone type\n"));
      }
    } else if (i && dimOk) {
      if (!colPos.size() || colPos.back() <= dim[1]) {
        colPos.push_back(dim[1]);
        colPos.push_back(dim[3]);
      } else
        f << "###";
    }
    ascii().addNote(f.str().c_str());
  }
  if (entry.m_fileType == 0xa && zoneId >= 0 &&
      zoneId < int(m_state->m_zonesList.size()) &&
      colPos.size() > 2 && int(colPos.size())==2*(entry.m_N-1)) {
    size_t numCols=size_t(entry.m_N-1);
    MWAWSection &sec=m_state->m_zonesList[size_t(zoneId)].m_section;
    sec.m_columns.resize(numCols);
    for (size_t c=0; c < numCols; c++) {
      MWAWSection::Column &col = sec.m_columns[c];
      int prevPos= c==0 ? colPos[0] : (colPos[2*c-1]+colPos[2*c])/2;
      int nextPos= c+1==numCols ? colPos[2*c+1] :
                   (colPos[2*c+1]+colPos[2*c+2])/2;
      col.m_width=double(nextPos-prevPos);
      col.m_widthUnit=WPX_POINT;
      col.m_margins[libmwaw::Left]=double(colPos[2*c]-prevPos)/72.;
      col.m_margins[libmwaw::Right]=double(nextPos-colPos[2*c+1])/72.;
    }
  }
  input->seek(entry.end(), WPX_SEEK_SET);

  return true;
}

bool MRWParser::readDocInfo(MRWEntry const &entry, int zoneId)
{
  if (entry.length() < 3) {
    MWAW_DEBUG_MSG(("MRWParser::readDocInfo: data seems to short\n"));
    return false;
  }
  MWAWInputStreamPtr input = getInput();
  input->seek(entry.begin(), WPX_SEEK_SET);
  input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  decodeZone(dataList);
  input->popLimit();

  int numDatas = int(dataList.size());
  if (numDatas < 60) {
    MWAW_DEBUG_MSG(("MRWParser::readDocInfo: find unexpected number of data\n"));
    return false;
  }

  libmwaw::DebugStream f;
  f << entry.name() << ":";

  int dim[2], margins[4]= {0,0,0,0};
  unsigned char color[3];
  size_t d=0;
  for (int j=0; j < numDatas; j++, d++) {
    MRWStruct const &dt = dataList[d];
    if (!dt.isBasic()) {
      f << "#f" << d << "=" << dt << ",";
      static bool first=true;
      if (first) {
        MWAW_DEBUG_MSG(("MRWParser::readDocInfo: find some struct block\n"));
        first = false;
      }
      continue;
    }
    switch(j) {
    case 0: { // 15|40
      uint32_t val=uint32_t(dt.value(0));
      if (val&0x4000) {
        if (zoneId==0)
          m_state->m_hasOddEvenHeaderFooter=true;
        f << "hasOddEven[header/footer],";
        val &= ~uint32_t(0x4000);
      }
      if (val&0x20000) {
        if (zoneId==0 && m_state->m_zonesList.size()) {
          MWAWSection &sec=m_state->m_zonesList[0].m_section;
          sec.m_columnSeparator = MWAWBorder();
        }
        f << "colSep,";
        val &= ~uint32_t(0x20000);
      }
      if (val)
        f << "f0=" << std::hex << val << std::dec << ",";
      break;
    }
    case 2: // small number between 108 and 297
    case 3: // f3=f2-24?
    case 5: // small number between -18 and 57
    case 6: // f6~=f5 ?
    case 13: // a small number 1|2|3|4|58|61|283
    case 35: // 1|2|31|60|283
    case 44: // 0 or 1
    case 46: // 0 or 1
    case 47: // 0 or 1
    case 48: // 0 or 1
    case 51: // always 1
      if (dt.value(0))
        f << "f" << j << "=" << dt.value(0) << ",";
      break;
    case 1: // 1
      if (dt.value(0)!=1)
        f << "f" << j << "=" << dt.value(0) << ",";
      break;
    case 7:
    case 8: // a dim?
      dim[0] = dim[1] = 0;
      dim[j-7]= (int) dt.value(0);
      if (j!=8)
        dim[++j-7] = (int) dataList[++d].value(0);
      f << "dim?=" << dim[1] << "x" << dim[0] << ",";
      break;
    case 9:
    case 10:
    case 11:
    case 12:
      margins[j-9]=(int) dt.value(0);
      while(j<12)
        margins[++j-9]= (int) dataList[++d].value(0);
      f << "margins=" << margins[1] << "x" << margins[0]
        << "<->"  << margins[3] << "x" << margins[2] << ",";
      break;
    case 14: // a very big number
      if (dt.value(0))
        f << "id?=" << std::hex << dt.value(0) << std::dec << ",";
      break;
    case 15: { // border type?
      long val = dt.value(0);
      if (!val) break;
      int depl = 24;
      f << "border?=[";
      for (int b = 0; b < 4; b++) {
        if ((val>>depl)&0xFF)
          f << ((val>>depl)&0xFF) << ",";
        else
          f << "_";
        depl -= 8;
      }
      f << "],";
      break;
    }
    case 16:
    case 17:
    case 18: {
      color[0]=color[1]=color[2]=0;
      color[j-16]=(unsigned char) (dt.value(0)>>8);
      while (j < 18)
        color[++j-16] = (unsigned char) (dataList[++d].value(0)>>8);
      MWAWColor col(color[0],color[1],color[2]);
      if (!col.isBlack()) f << "color=" << col << ",";
      break;
    }
    case 76: // a very big number
    case 79:
      if (dt.value(0))
        f << "f" << j << "=" << std::hex << uint32_t(dt.value(0)) << std::dec << ",";
      break;
    default:
      if (dt.value(0))
        f << "#f" << j << "=" << dt.value(0) << ",";
      break;
    }
  }
  if (zoneId==0 && margins[0] > 0 && margins[1] > 0 &&
      margins[2] > 0 && margins[3] > 0) {
    m_pageMarginsSpanSet= true;
    getPageSpan().setMarginTop(double(margins[0])/72.0);
    if (margins[2]>80)
      getPageSpan().setMarginBottom(double(margins[2]-40)/72.0);
    else
      getPageSpan().setMarginBottom(double(margins[2]/2)/72.0);
    getPageSpan().setMarginLeft(double(margins[1])/72.0);
    getPageSpan().setMarginRight(double(margins[3])/72.0);

  }
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  return true;
}
bool MRWParser::readZoneb(MRWEntry const &entry, int)
{
  if (entry.length() < entry.m_N) {
    MWAW_DEBUG_MSG(("MRWParser::readZoneb: data seems to short\n"));
    return false;
  }
  MWAWInputStreamPtr input = getInput();
  input->seek(entry.begin(), WPX_SEEK_SET);
  input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  decodeZone(dataList, 1+4*entry.m_N);
  input->popLimit();

  if (int(dataList.size()) != 4*entry.m_N) {
    MWAW_DEBUG_MSG(("MRWParser::readZoneb: find unexpected number of data\n"));
    return false;
  }

  libmwaw::DebugStream f;
  size_t d = 0;
  for (int i = 0; i < entry.m_N; i++) {
    f.str("");
    f << entry.name() << "-" << i << ":";
    ascii().addPos(dataList[d].m_filePos);

    for (int j = 0; j < 4; j++) { // always 0, 0, 0, 0?
      MRWStruct const &data = dataList[d++];
      if (!data.isBasic()) {
        MWAW_DEBUG_MSG(("MRWParser::readZoneb: find unexpected dim data type\n"));
        f << "###dim" << j << "=" << data << ",";
      } else if (data.value(0))
        f << "f" << j << "=" << data.value(0) << ",";
    }
    ascii().addNote(f.str().c_str());
  }
  input->seek(entry.end(), WPX_SEEK_SET);

  return true;
}

bool MRWParser::readZonec(MRWEntry const &entry, int zoneId)
{
  if (entry.length() < entry.m_N) {
    MWAW_DEBUG_MSG(("MRWParser::readZonec: data seems to short\n"));
    return false;
  }
  MWAWInputStreamPtr input = getInput();
  input->seek(entry.begin(), WPX_SEEK_SET);
  input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  decodeZone(dataList, 1+9*entry.m_N);
  input->popLimit();

  if (int(dataList.size()) != 9*entry.m_N) {
    MWAW_DEBUG_MSG(("MRWParser::readZonec: find unexpected number of data\n"));
    return false;
  }

  libmwaw::DebugStream f;
  size_t d = 0;
  for (int i = 0; i < entry.m_N; i++) {
    f.str("");
    f << entry.name() << "-" << i << ":";
    ascii().addPos(dataList[d].m_filePos);

    for (int j = 0; j < 9; j++) {
      MRWStruct const &data = dataList[d++];
      if (!data.isBasic()) {
        MWAW_DEBUG_MSG(("MRWParser::readZonec: find unexpected dim data type\n"));
        f << "###dim" << j << "=" << data << ",";
      } else if (j==8) {
        if (!data.value(0)) {
          f << "firstPage[header/footer],";
          if (zoneId==0)
            m_state->m_firstPageFooter=true;
        } else if (data.value(0)!=1)
          f << "#f8=" << "=" << data.value(0) << ",";
      } else if (data.value(0))
        f << "f" << j << "=" << data.value(0) << ",";
    }
    ascii().addNote(f.str().c_str());
  }
  input->seek(entry.end(), WPX_SEEK_SET);

  return true;
}

bool MRWParser::readZone13(MRWEntry const &entry, int)
{
  if (entry.length() < 3) {
    MWAW_DEBUG_MSG(("MRWParser::readZone13: data seems to short\n"));
    return false;
  }
  MWAWInputStreamPtr input = getInput();
  input->seek(entry.begin(), WPX_SEEK_SET);
  input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  decodeZone(dataList, 1+23);
  input->popLimit();

  if (int(dataList.size()) != 23) {
    MWAW_DEBUG_MSG(("MRWParser::readZone13: find unexpected number of data\n"));
    return false;
  }

  libmwaw::DebugStream f;
  size_t d = 0;
  f << entry.name() << ":";
  ascii().addPos(dataList[d].m_filePos);

  int val;
  for (int j = 0; j < 23; j++) {
    MRWStruct const &data = dataList[d++];
    if ((j!=14 && !data.isBasic()) || (j==14 && data.m_type)) {
      MWAW_DEBUG_MSG(("MRWParser::readZone13: find unexpected struct data type\n"));
      f << "#f" << j << "=" << data << ",";
      continue;
    }
    if (j < 14) {
      int const expectedValues[]= {13,10,9,31,8,12,14,28,29,30,31,0x7f,27,0};
      if ((int) data.value(0) != expectedValues[j])
        f << "f" << j << "=" << data.value(0) << ",";
      continue;
    }
    if (j == 14) {
      if (!data.m_pos.valid())
        f << "#f" << j << "=" << data << ",";
      else {
        /* find v_{2i}=0 and v_{2i+1}=
           {301,302,422,450,454,422,421,418,302,295,290,468,469,466,467,457,448}
        */
        f << "bl=[";
        input->seek(data.m_pos.begin(), WPX_SEEK_SET);
        int N = int(data.m_pos.length()/2);
        for (int k = 0; k < N; k++) {
          val = (int) input->readLong(2);
          if (val) f << val << ",";
          else f << "_,";
        }
        f << "],";
      }
      continue;
    }
    unsigned char color[3];
    MWAWColor col;
    switch(j) {
    case 15:
    case 16:
    case 17:
      color[0]=color[1]=color[2]=0xFF;
      color[j-15]=(unsigned char) (data.value(0)>>8);
      while (j < 17)
        color[++j-15] = (unsigned char) (dataList[d++].value(0)>>8);
      col = MWAWColor(color[0],color[1],color[2]);
      if (!col.isWhite())
        f << "col0=" << col << ",";
      break;
    case 19:
    case 20:
    case 21:
      color[0]=color[1]=color[2]=0xFF;
      color[j-19]=(unsigned char) (data.value(0)>>8);
      while (j < 21)
        color[++j-19] = (unsigned char) (dataList[d++].value(0)>>8);
      col = MWAWColor(color[0],color[1],color[2]);
      if (!col.isWhite())
        f << "col1=" << col << ",";
      break;
    default:
      if (data.value(0))
        f << "#f" << j << "=" << data.value(0) << ",";
    }
  }
  ascii().addNote(f.str().c_str());
  input->seek(entry.end(), WPX_SEEK_SET);

  return true;
}

// --------- print info ----------

// read the print info xml data
bool MRWParser::readCPRT(MRWEntry const &entry)
{
  if (entry.length() < 0x10) {
    MWAW_DEBUG_MSG(("MRWParser::readCPRT: data seems to short\n"));
    return false;
  }

  MWAWInputStreamPtr input = getInput();
  input->seek(entry.begin(), WPX_SEEK_SET);
#ifdef DEBUG_WITH_FILES
  WPXBinaryData file;
  input->readDataBlock(entry.length(), file);

  static int volatile cprtName = 0;
  libmwaw::DebugStream f;
  f << "CPRT" << ++cprtName << ".plist";
  libmwaw::Debug::dumpFile(file, f.str().c_str());

  ascii().skipZone(entry.begin(),entry.end()-1);
#endif

  input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

// read the print info data
bool MRWParser::readPrintInfo(MRWEntry const &entry)
{
  if (entry.length() < 0x77) {
    MWAW_DEBUG_MSG(("MRWParser::readPrintInfo: data seems to short\n"));
    return false;
  }

  MWAWInputStreamPtr input = getInput();
  input->seek(entry.begin(), WPX_SEEK_SET);

  libmwaw::PrinterInfo info;
  if (!info.read(input))
    return false;

  libmwaw::DebugStream f;
  f << "PrintInfo:"<< info;
  Vec2i paperSize = info.paper().size();
  Vec2i pageSize = info.page().size();
  if (pageSize.x() <= 0 || pageSize.y() <= 0 ||
      paperSize.x() <= 0 || paperSize.y() <= 0) return false;

  if (!m_pageMarginsSpanSet) {
    // define margin from print info
    Vec2i lTopMargin= -1 * info.paper().pos(0);
    Vec2i rBotMargin=info.paper().size() - info.page().size();

    // move margin left | top
    int decalX = lTopMargin.x() > 14 ? lTopMargin.x()-14 : 0;
    int decalY = lTopMargin.y() > 14 ? lTopMargin.y()-14 : 0;
    lTopMargin -= Vec2i(decalX, decalY);
    rBotMargin += Vec2i(decalX, decalY);

    // decrease right | bottom
    int rightMarg = rBotMargin.x() -50;
    if (rightMarg < 0) rightMarg=0;
    int botMarg = rBotMargin.y() -50;
    if (botMarg < 0) botMarg=0;

    getPageSpan().setMarginTop(lTopMargin.y()/72.0);
    getPageSpan().setMarginBottom(botMarg/72.0);
    getPageSpan().setMarginLeft(lTopMargin.x()/72.0);
    getPageSpan().setMarginRight(rightMarg/72.0);
  }
  getPageSpan().setFormLength(paperSize.y()/72.);
  getPageSpan().setFormWidth(paperSize.x()/72.);
  getPageSpan().checkMargins();

  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());

  input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// low level
////////////////////////////////////////////////////////////

// ---- field decoder ---------
bool MRWParser::readEntryHeader(MRWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  std::vector<long> dataList;
  if (!readNumbersString(4,dataList)||dataList.size()<5) {
    MWAW_DEBUG_MSG(("MRWParser::readEntryHeader: oops can not find header entry\n"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  long length = (dataList[1]<<16)+dataList[2];
  if (length < 0 || !isFilePos(input->tell()+length)) {
    MWAW_DEBUG_MSG(("MRWParser::readEntryHeader: the header data seems to short\n"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  entry.setBegin(input->tell());
  entry.setLength(length);
  entry.m_fileType = (int) int16_t(dataList[0]);
  entry.m_N = (int) dataList[4];
  entry.m_value = (int) dataList[3];

  return true;
}

bool MRWParser::readNumbersString(int num, std::vector<long> &res)
{
  res.resize(0);
  // first read the string
  MWAWInputStreamPtr input = getInput();
  std::string str("");
  while (!input->atEOS()) {
    int ch = int(input->readULong(1));
    if (ch=='-' || (ch >= 'A' && ch <= 'F') || (ch >= '0' && ch <= '9')) {
      str += char(ch);
      continue;
    }
    input->seek(-1, WPX_SEEK_CUR);
    break;
  }
  if (!str.length()) return false;

  // ok we have the string, let decodes it
  size_t sz = str.length(), i = sz;
  int nBytes = 0;
  long val=0;
  while(1) {
    if (i==0) {
      if (nBytes)
        res.insert(res.begin(),val);
      break;
    }
    char c = str[--i];
    if (c=='-') {
      if (!nBytes) {
        MWAW_DEBUG_MSG(("MRWParser::readNumbersString find '-' with no val\n"));
        break;
      }
      res.insert(res.begin(),-val);
      val = 0;
      nBytes = 0;
      continue;
    }

    if (nBytes==num) {
      res.insert(res.begin(),val);
      val = 0;
      nBytes = 0;
    }

    if (c >= '0' && c <= '9')
      val += (long(c-'0')<<(4*nBytes));
    else if (c >= 'A' && c <= 'F')
      val += (long(c+10-'A')<<(4*nBytes));
    else {
      MWAW_DEBUG_MSG(("MRWParser::readNumbersString find odd char %x\n", int(c)));
      break;
    }
    nBytes++;
  }
  return true;
}

bool MRWParser::decodeZone(std::vector<MRWStruct> &dataList, long numData)
{
  dataList.clear();

  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  while (!input->atEOS()) {
    size_t numVal = dataList.size();
    if (numVal >= size_t(numData))
      break;
    MRWStruct data;
    data.m_filePos = pos;
    int type = int(input->readULong(1));
    data.m_type = (type&3);
    if (type == 3)
      return true;
    if ((type & 0x3c) || (type && !(type&0x3)))
      break;
    if ((type>>4)==0xc) {
      if (input->atEOS()) break;
      int num = int(input->readULong(1));
      if (!num) break;
      if (numVal==0) {
        MWAW_DEBUG_MSG(("MRWParser::decodeZone: no previous data to copy\n"));
      } else // checkme
        data = dataList[numVal-1];

      for (int j = 0; j < num; j++)
        dataList.push_back(data);
      pos = input->tell();
      continue;
    }
    if ((type>>4)==0x8) {
      if (numVal==0) {
        MWAW_DEBUG_MSG(("MRWParser::decodeZone: no previous data to copy(II)\n"));
        dataList.push_back(data);
      } else
        dataList.push_back(dataList[numVal-1]);
      pos = input->tell();
      continue;
    }
    std::vector<long> &numbers = data.m_data;
    if (!readNumbersString(data.m_type==1 ? 4: 8, numbers))
      break;
    if (type==0) {
      if (numbers.size() != 1 || numbers[0] < 0 || input->readULong(1) != 0x2c)
        break;
      data.m_pos.setBegin(input->tell());
      data.m_pos.setLength(numbers[0]);
      if (!isFilePos(data.m_pos.end()))
        break;
      input->seek(data.m_pos.end(), WPX_SEEK_SET);
      numbers.resize(0);
    }

    dataList.push_back(data);
    pos = input->tell();
  }
  input->seek(pos, WPX_SEEK_SET);
  return dataList.size();
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool MRWParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = MRWParserInternal::State();
  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork())
    return false;

  long const headerSize=0x2e;
  if (!isFilePos(headerSize)) {
    MWAW_DEBUG_MSG(("MRWParser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0,WPX_SEEK_SET);

  int actZone = -1;
  if (!readZone(actZone, true))
    return false;
  if (strict && !readZone(actZone, true))
    return false;

  input->seek(0, WPX_SEEK_SET);
  if (header)
    header->reset(MWAWDocument::MARIW, 1);

  return true;
}

////////////////////////////////////////////////////////////
// MRWEntry/MRWStruct function
////////////////////////////////////////////////////////////
std::string MRWEntry::name() const
{
  switch(m_fileType) {
  case -1:
    return "Separator";
  case -2:
    return "EndZone";
  case 0:
    return "ZoneHeader";
  case 1:
    return "TextStruct";
  case 2:
    return "TEXT";
  case 4:
    return "CharPLC";
  case 5:
    return "ParagPLC";
  case 6:
    return "Fonts";
  case 7:
    return "Paragraphs";
  case 8:
    return "FontNames";
  case 9:
    return "PaperSize";
  case 0xa:
    return "ColDim";
  case 0xf:
    return "DocInfo";
  case 0x14: // token, picture, ...
    return "Token";
  case 0x1a:
    return "StyleNames";
  case 0x1f:
    return "PrintInfo";
  case 0x24:
    return "CPRT";
  case 0x41a:
    return "DocInf2";
  case 0x420:
    return "PSFile";
  default:
    break;
  }
  std::stringstream s;
  if (m_fileType >= 0)
    s << "Zone" << std::hex << std::setfill('0') << std::setw(2) << m_fileType << std::dec;
  else
    s << "Zone-" << std::hex << std::setfill('0') << std::setw(2) << -m_fileType << std::dec;

  return s.str();
}

long MRWStruct::value(int i) const
{
  if (i < 0 || i >= int(m_data.size())) {
    if (i) {
      MWAW_DEBUG_MSG(("MRWStruct::value: can not find value %d\n", i));
    }
    return 0;
  }
  return m_data[size_t(i)];
}
std::ostream &operator<<(std::ostream &o, MRWStruct const &dt)
{
  switch(dt.m_type) {
  case 0: // data
    o << "sz=" << std::hex << dt.m_pos.length() << std::dec;
    return o;
  case 3: // end of data
    return o;
  case 1: // int?
  case 2: // long?
    break;
  default:
    if (dt.m_type) o << ":" << dt.m_type;
    break;
  }
  size_t numData = dt.m_data.size();
  if (!numData) {
    o << "_";
    return o;
  }
  if (numData > 1) o << "[";
  for (size_t d = 0; d < numData; d++) {
    long val = dt.m_data[d];
    if (val > -100 && val < 100)
      o << val;
    else if (val > 0)
      o << "0x" << std::hex << val << std::dec;
    else
      o << "-0x" << std::hex << -val << std::dec;
    if (d+1 != numData) o << ",";
  }
  if (numData > 1) o << "]";
  return o;
}


// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
