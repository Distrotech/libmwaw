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

#include "MDWParser.hxx"

/** Internal: the structures of a MDWParser */
namespace MDWParserInternal
{
////////////////////////////////////////
//! Internal: a line information
struct LineInfo {
  LineInfo() : m_entry(), m_type(-1000), m_height(0), m_y(-1), m_page(-1),
    m_listLevel(0), m_listType(0), m_extra("") {
    for (int i = 0; i < 4; i++) m_flags[i] = 0;
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, LineInfo const &line) {
    if (line.m_entry.valid())
      o << "textPos=" << std::hex << line.m_entry.begin()
        << "<->" << line.m_entry.end() << std::dec <<  ",";
    switch (line.m_type) {
    case -1000:
      break;
    case 0:
      o << "text,";
      break;
    case -1:
      o << "graphic,";
      break;
    case -2:
      o << "newpage,";
      break;
      // find also -3 will no data ( nop ? )
    default:
      o << "#type=" << line.m_type << ",";
      break;
    }
    if (line.m_height>0) o << "h=" << line.m_height << ",";
    if (line.m_y>=0) o << "y=" << line.m_y << ",";
    if (line.m_page>0) o << "page?=" << line.m_page << ",";
    if (line.m_flags[1] & 0x40) {
      switch (line.m_flags[1]&0x3) {
      case 0:
        o << "left,";
        break;
      case 1:
        o << "center,";
        break;
      case 2:
        o << "right,";
        break;
      case 3:
        o << "full,";
        break;
      default:
        break;
      }
    }
    if (line.m_flags[1] & 0x4) o << "ruler,"; // checkme maybe show header
    if (line.m_flags[1] & 0x8)
      o << "compressed,";
    if (line.m_listLevel) {
      o << "list=" << line.m_listLevel;
      switch(line.m_listType) {
      case 0:
        o << "Head,";
        break; // Heading
      case 1:
        o << "Unl,";
        break; // unlabelled
      case 2:
        o << "Num,";
        break; // numbered
      case 3:
        o << "Bul,";
        break; // bullet
      default:
        o << "[#type=" << line.m_listType << "],";
      }
    }
    if (line.m_flags[0])
      o << "fl0=" << std::hex << line.m_flags[0] << std::dec << ",";
    if (line.m_flags[1] & 0xb0) {
      if (line.m_flags[1] & 0x90) o << "#";
      o << "fl1=" << std::hex << (line.m_flags[1]& 0xb0) << std::dec << ",";
    }
    for (int i = 2; i < 4; i++) { // [067a][1-4], [0-4]
      if (!line.m_flags[i])
        continue;
      o << "fl" << i << "=" << std::hex << line.m_flags[i] << std::dec << ",";
    }
    if (line.m_extra.length()) o << line.m_extra << ",";
    return o;
  }

  //! the main entry
  MWAWEntry m_entry;
  //! the entry type
  int m_type;
  //! the height
  int m_height;
  //! the y pos
  int m_y;
  //! the page number
  int m_page;
  //! the item level
  int m_listLevel;
  //! the item type
  int m_listType;
  //! two flags
  int m_flags[4];
  //! extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: a zone information
struct ZoneInfo {
  ZoneInfo() : m_linesList() {
  }

  //! the list of line information
  std::vector<LineInfo> m_linesList;
};

////////////////////////////////////////
//! Internal: the state of a MDWParser
struct State {
  //! constructor
  State() : m_compressCorr(""), m_eof(-1), m_entryMap(), m_actPage(0), m_numPages(0),
    m_rulerParagraph(), m_actParagraph(), m_actListType(-1),
    m_headerHeight(0), m_footerHeight(0) {
    m_compressCorr = " etnroaisdlhcfp";
    for (int i = 0; i < 3; i++)
      m_numLinesByZone[i] = 0;
  }

  //! the correspondance between int compressed and char : must be 15 character
  std::string m_compressCorr;

  //! the number of paragraph by zones ( main, header, footer )
  int m_numLinesByZone[3];
  //! the zones
  ZoneInfo m_zones[3];
  //! end of file
  long m_eof;

  //! the zones map
  std::multimap<std::string, MWAWEntry> m_entryMap;

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;
  MWAWParagraph m_rulerParagraph/** the ruler paragraph */,
                m_actParagraph/** the actual paragraph */;
  //! the actual list type
  int m_actListType;
  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a MWParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(MDWParser &pars, MWAWInputStreamPtr input, int zoneId) :
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
  MDWContentListener *listen = dynamic_cast<MDWContentListener *>(listener.get());
  if (!listen) {
    MWAW_DEBUG_MSG(("SubDocument::parse: bad listener\n"));
    return;
  }
  if (m_id != 1 && m_id != 2) {
    MWAW_DEBUG_MSG(("SubDocument::parse: unknown zone\n"));
    return;
  }

  assert(m_parser);
  long pos = m_input->tell();
  reinterpret_cast<MDWParser *>(m_parser)->sendZone(m_id);
  m_input->seek(pos, WPX_SEEK_SET);
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MDWParser::MDWParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWParser(input, rsrcParser, header), m_listener(), m_convertissor(), m_state(),
  m_pageSpan()
{
  init();
}

MDWParser::~MDWParser()
{
}

void MDWParser::init()
{
  m_convertissor.reset(new MWAWFontConverter);
  m_listener.reset();
  setAsciiName("main-1");

  m_state.reset(new MDWParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  m_pageSpan.setMarginTop(0.1);
  m_pageSpan.setMarginBottom(0.1);
  m_pageSpan.setMarginLeft(0.1);
  m_pageSpan.setMarginRight(0.1);
}

void MDWParser::setListener(MDWContentListenerPtr listen)
{
  m_listener = listen;
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
float MDWParser::pageHeight() const
{
  return float(m_pageSpan.getFormLength()-m_pageSpan.getMarginTop()-m_pageSpan.getMarginBottom()-m_state->m_headerHeight/72.0-m_state->m_footerHeight/72.0);
}

float MDWParser::pageWidth() const
{
  return float(m_pageSpan.getFormWidth()-m_pageSpan.getMarginLeft()-m_pageSpan.getMarginRight());
}


////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void MDWParser::newPage(int number)
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

bool MDWParser::isFilePos(long pos)
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

MWAWEntry MDWParser::readEntry()
{
  MWAWEntry res;
  MWAWInputStreamPtr input = getInput();
  res.setBegin((long) input->readULong(4));
  res.setLength((long) input->readULong(4));
  if (res.length() && !isFilePos(res.end())) {
    MWAW_DEBUG_MSG(("MDWParser::readEntry: find an invalid entry\n"));
    res.setLength(0);
  }
  return res;
}
////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void MDWParser::parse(WPXDocumentInterface *docInterface)
{
  assert(getInput().get() != 0);

  if (!checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = true;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());

    checkHeader(0L);
    if (getRSRCParser()) {
      MWAWEntry corrEntry = getRSRCParser()->getEntry("STR ", 700);
      std::string corrString("");
      if (corrEntry.valid() && getRSRCParser()->parseSTR(corrEntry, corrString)) {
        if (corrString.length() != 15) {
          MWAW_DEBUG_MSG(("MDWParser::parse: resource correspondance string seems bad\n"));
        } else
          m_state->m_compressCorr = corrString;
      }
    }
    ok = createZones();
    if (ok) {
      createDocument(docInterface);
      sendZone(0);
      if (m_listener) m_listener->endDocument();
      m_listener.reset();
    }

    libmwaw::DebugStream f;
    std::multimap<std::string, MWAWEntry>::const_iterator it
      = m_state->m_entryMap.begin();
    while (it != m_state->m_entryMap.end()) {
      MWAWEntry const &entry = (it++)->second;
      if (entry.isParsed())
        continue;
      f.str("");
      f << entry;
      ascii().addPos(entry.begin());
      ascii().addNote(f.str().c_str());
      ascii().addPos(entry.end());
      ascii().addNote("_");
    }
    ascii().reset();
  } catch (...) {
    MWAW_DEBUG_MSG(("MDWParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void MDWParser::createDocument(WPXDocumentInterface *documentInterface)
{
  if (!documentInterface) return;
  if (m_listener) {
    MWAW_DEBUG_MSG(("MDWParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  int numPage = 0, numBreakPage = 0;
  for (size_t i = 0; i < m_state->m_zones[0].m_linesList.size(); i++) {
    MDWParserInternal::LineInfo const &line = m_state->m_zones[0].m_linesList[i];
    if (line.m_type == -2)
      numBreakPage++;
    if (line.m_page > numPage)
      numPage = line.m_page;
  }
  if (numBreakPage > numPage) numPage = numBreakPage;
  m_state->m_numPages = numPage+1;

  // create the page list
  std::vector<MWAWPageSpan> pageList;
  MWAWPageSpan ps(m_pageSpan);

  for (int i = 1; i <= 2; i++) {
    if (!m_state->m_zones[i].m_linesList.size())
      continue;
    shared_ptr<MWAWSubDocument> subdoc(new MDWParserInternal::SubDocument(*this, getInput(), i));
    ps.setHeaderFooter((i==1) ? MWAWPageSpan::HEADER : MWAWPageSpan::FOOTER, MWAWPageSpan::ALL, subdoc);
  }

  for (int i = 0; i <= m_state->m_numPages; i++) pageList.push_back(ps);

  //
  MDWContentListenerPtr listen(new MDWContentListener(pageList, documentInterface));
  setListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool MDWParser::createZones()
{
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  long pos = input->tell();
  if (!isFilePos(pos+128+56)) {
    MWAW_DEBUG_MSG(("MDWParser::createZones: zones Zone is too short\n"));
    return false;
  }

  for (int i = 0; i < 15; i++) {
    pos = input->tell();
    MWAWEntry entry = readEntry();
    if (!entry.valid()) {
      ascii().addPos(pos);
      ascii().addNote("_");
      continue;
    }
    switch(i) {
    case 0:
    case 1:
    case 2:
      entry.setType("LineInfo");
      entry.setId(i);
      break;
    case 6:
      entry.setType("PrintInfo");
      break;
    case 7:
      entry.setType("LastZone");
      break;
    default: {
      std::stringstream s;
      s << "Zone" << i;
      entry.setType(s.str());
      break;
    }
    }
    m_state->m_entryMap.insert(std::multimap<std::string,MWAWEntry>::value_type
                               (entry.type(), entry));
    f.str("");
    f << "Entries(" << entry.type() << "):" << std::hex << entry.begin() << "-" << entry.end() << std::dec << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  pos = input->tell();
  f.str("");
  f << "Entries(ZoneDef):";
  long val;
  for (int i = 0; i < 28; i++) { // always 0 ?
    val = input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  std::multimap<std::string, MWAWEntry>::iterator it;
  it = m_state->m_entryMap.find("PrintInfo");
  if (it !=  m_state->m_entryMap.end())
    readPrintInfo(it->second);
  it = m_state->m_entryMap.find("LineInfo");
  while (it !=  m_state->m_entryMap.end() && it->first == "LineInfo") {
    readLinesInfo(it->second);
    it++;
  }
#ifdef DEBUG
  it = m_state->m_entryMap.find("LastZone");
  if (it !=  m_state->m_entryMap.end())
    readLastZone(it->second);
  it = m_state->m_entryMap.find("Zone12");
  if (it !=  m_state->m_entryMap.end())
    readZone12(it->second);
#endif
  for (int i = 0; i < 3; i++)
    if (m_state->m_zones[i].m_linesList.size())
      return true;
  return false;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// try to send a text zone
////////////////////////////////////////////////////////////
bool MDWParser::sendZone(int id)
{
  // save the actual state
  MWAWParagraph previousRulerPara = m_state->m_rulerParagraph;
#if 0
  MWAWParagraph previousActPara = m_state->m_actParagraph;
  int prevListType = m_state->m_actListType;
#endif

  setProperty(MWAWParagraph());
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  if (id < 0 || id >= 3) {
    MWAW_DEBUG_MSG(("MDWParser::sendZone: find unexpected id %d\n", id));
    return false;
  }
  if (id==0) m_state->m_actPage = 1;
  MDWParserInternal::ZoneInfo &zone = m_state->m_zones[id];
  for (size_t i = 0; i < zone.m_linesList.size(); i++) {
    MDWParserInternal::LineInfo &line = zone.m_linesList[i];
    if (i==0) {
      ascii().addPos(line.m_entry.begin());
      ascii().addNote("Entries(Text)");
    }
    if (id==0 && line.m_page+1 > m_state->m_actPage)
      newPage(line.m_page+1);
    bool done=false;
    switch(line.m_type) {
    case 0:
      if (line.m_height) {
        updateRuler(line);
        if (line.m_flags[1]&8)
          done = readCompressedText(line);
        else
          done = readText(line);
      } else {
        MWAWParagraph para;
        done = readRuler(line, para);
      }
      break;
    case -1:
      updateRuler(line);
      done = readGraphic(line);
      break;
    case -2:
      if (id!=0) {
        MWAW_DEBUG_MSG(("MDWParser::sendZone: find page break on not main zone\n"));
        break;
      }
      newPage(m_state->m_actPage+1);
      done = true;
      break;
    default:
      break;
    }
    if (done)
      continue;
    f.str("");
    f << "Text[" << line << "]";
    ascii().addPos(line.m_entry.begin());
    ascii().addNote(f.str().c_str());
  }
  // reset the actual state
  m_state->m_rulerParagraph=previousRulerPara;
  return true;
}
////////////////////////////////////////////////////////////
// read a graphic zone
////////////////////////////////////////////////////////////
bool MDWParser::readGraphic(MDWParserInternal::LineInfo const &line)
{
  if (!line.m_entry.valid())
    return false;

  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  int sz = (int) line.m_entry.length();
  if (sz < 10) {
    MWAW_DEBUG_MSG(("MDWParser::readGraphic: zone size is two short or odd\n"));
    return false;
  }

  long pos=line.m_entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  int dim[4];
  for (int i = 0; i < 4; i++) dim[i] = (int) input->readLong(2);
  Box2f box(Vec2f((float)dim[1],(float)dim[0]),
            Vec2f((float)dim[3],(float)dim[2]));
  f << "Entries(graphic): bdBox=" << box << ",";

  shared_ptr<MWAWPict> pict(MWAWPictData::get(input, sz-8));
  if (!pict) {
    MWAW_DEBUG_MSG(("MDWParser::readGraphic: can not read the picture\n"));
    return false;
  }
  WPXBinaryData data;
  std::string type;
  if (m_listener && pict->getBinary(data,type)) {
    MWAWPosition pictPos=MWAWPosition(Vec2f(0,0),box.size(), WPX_POINT);
    pictPos.setRelativePosition(MWAWPosition::Char);
    m_listener->insertPicture(pictPos,data, type);
  }
  ascii().skipZone(pos+8, pos+sz-1);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read a ruler zone
////////////////////////////////////////////////////////////
bool MDWParser::readRuler(MDWParserInternal::LineInfo const &line, MWAWParagraph &para)
{
  para = MWAWParagraph();
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;

  if (line.m_entry.length() < 10 || (line.m_entry.length()%2)) {
    MWAW_DEBUG_MSG(("MDWParser::readRuler: zone size is two short or odd\n"));
    return false;
  }
  long pos=line.m_entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  para.m_marginsUnit = WPX_POINT;
  para.m_margins[1] = (double) input->readULong(2);
  para.m_margins[2] = pageWidth()*72.0-(double) input->readULong(2);
  if (*(para.m_margins[2]) < 0) {
    f << "#rightMargin=" << pageWidth()*72.0-*(para.m_margins[2]);
    para.m_margins[2] = 0.0;
  }
  long val = (long) input->readULong(1);
  switch(val) {
  case 0:
    para.m_justify = libmwaw::JustificationLeft;
    break;
  case 1:
    para.m_justify = libmwaw::JustificationCenter;
    break;
  case 2:
    para.m_justify = libmwaw::JustificationRight;
    break;
  case 3:
    para.m_justify = libmwaw::JustificationFull;
    break;
  default:
    f << "#align=" << std::hex << val << std::dec << ",";
    break;
  }
  int N = (int) input->readULong(1);
  if (line.m_entry.length() != 2*N+10) {
    MWAW_DEBUG_MSG(("MDWParser::readRuler: num tabs is incorrect\n"));
    return false;
  }
  val = (long) input->readULong(2);
  para.m_spacingsInterlineUnit = WPX_PERCENT;
  switch (val) {
  case 0:
    para.m_spacings[0] = 1.;
    break;
  case 1:
    para.m_spacings[0] = 1.5;
    break;
  case 2:
    para.m_spacings[0] = 2.;
    break;
  case 0x8000:
    f << "6 lines by ldpi,";
    break;
  default:
    if (val) f << "#interline=" << std::hex << val << std::dec << ",";
  }
  para.m_margins[0] = ((double) input->readULong(2))-*(para.m_margins[1]);
  for (int i = 0; i < N; i++) {
    MWAWTabStop tab;
    val = input->readLong(2);
    if (val > 0)
      tab.m_position = (float(val)-*(para.m_margins[1]))/72.f;
    else {
      tab.m_position = (-float(val)-*(para.m_margins[1]))/72.f;
      tab.m_alignment = MWAWTabStop::CENTER;
    }
    para.m_tabs->push_back(tab);
  }
  para.m_extra = f.str();
  m_state->m_actListType = -1;
  m_state->m_rulerParagraph = para;
  setProperty(para);
  f.str("");
  f << "Text[ruler]:" << para;

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

void MDWParser::updateRuler(MDWParserInternal::LineInfo const &line)
{
  if (!m_listener || !line.m_entry.valid()) return;
  libmwaw::Justification justify = *(m_state->m_rulerParagraph.m_justify);
  if (line.m_flags[1] & 0x40) {
    switch (line.m_flags[1]&0x3) {
    case 0:
      justify = libmwaw::JustificationLeft;
      break;
    case 1:
      justify = libmwaw::JustificationCenter;
      break;
    case 2:
      justify = libmwaw::JustificationRight;
      break;
    case 3:
      justify = libmwaw::JustificationFull;
      break;
    default:
      break;
    }
  }
  bool changeJustification = *(m_state->m_actParagraph.m_justify) != justify;
  bool listChanged = false;
  if (line.m_listLevel != *(m_state->m_actParagraph.m_listLevelIndex))
    listChanged = true;
  if (line.m_listLevel && line.m_listType != m_state->m_actListType)
    listChanged = true;

  if (!changeJustification && !listChanged) {
    if (line.m_listLevel && m_listener)
      m_listener->setCurrentListLevel(line.m_listLevel);
    return;
  }

  MWAWParagraph para = m_state->m_rulerParagraph;
  para.m_justify = justify;

  m_state->m_actListType = line.m_listType;
  int const level = line.m_listLevel;
  double itemPos = level==0 ? 0.0 : level*0.5;
  double textPos = level==0 ? 0.0 : itemPos+0.5;
  if (line.m_listType == 0 && level) itemPos-=0.5;
  if (!level)
    *(para.m_margins[1]) += textPos*72.;
  else {
    MWAWList::Level theLevel;
    switch(line.m_listType) {
    case 0:
      theLevel.m_type = MWAWList::Level::DECIMAL;
      break; // Heading
    case 1:
      theLevel.m_type = MWAWList::Level::NONE;
      break;
    case 2:
      theLevel.m_type = MWAWList::Level::DECIMAL;
      break; // numbered
    case 3:  // bullet
      theLevel.m_type = MWAWList::Level::BULLET;
      MWAWContentListener::appendUnicode(0x2022, theLevel.m_bullet);
      break;
    default:
      theLevel.m_type = MWAWList::Level::NONE;
      break;
    }
    if (listChanged && theLevel.m_type == MWAWList::Level::DECIMAL &&
        line.m_listType != m_state->m_actListType &&
        *(para.m_listLevelIndex)==level)
      theLevel.m_startValue=1;
    theLevel.m_labelIndent = itemPos+*(para.m_margins[1])/72.0;
    *(para.m_margins[1]) += textPos*72.;
    //    theLevel.m_labelWidth = (textPos-itemPos);
    para.m_listLevel=theLevel;
  }
  para.m_listLevelIndex=level;

  setProperty(para);
}

////////////////////////////////////////////////////////////
// read the text zone
////////////////////////////////////////////////////////////
bool MDWParser::readText(MDWParserInternal::LineInfo const &line)
{
  if (!line.m_entry.valid())
    return false;

  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;

  long pos=line.m_entry.begin();
  long endPos=line.m_entry.end();
  input->seek(pos, WPX_SEEK_SET);
  int num = (int) input->readULong(2);
  if (pos+num >= endPos) {
    MWAW_DEBUG_MSG(("MDWParser::readText: numChar is too long\n"));
    return false;
  }

  f << "Text:";
  if (line.m_listLevel) {
    f << "[list=" << line.m_listLevel;
    switch(line.m_listType) {
    case 0:
      f << "Head,";
      break; // Heading
    case 1:
      f << "Unl,";
      break; // unlabelled
    case 2:
      f << "Num,";
      break; // bullet
    case 3:
      f << "Bul,";
      break; // paragraph
    default:
      f << "[#type=" << line.m_listType << "],";
    }
    f << "],";
  }
  std::string text("");
  for (int n = 0; n < num; n++) {
    char c = (char) input->readULong(1);
    if (!c) {
      MWAW_DEBUG_MSG(("MDWParser::readText: find 0 char\n"));
      continue;
    }
    text+=(char) c;
  }
  f << text;

  // realign to 2
  if (input->tell()&1)
    input->seek(1,WPX_SEEK_CUR);

  ascii().addPos(line.m_entry.begin());
  ascii().addNote(f.str().c_str());

  std::vector<int> textPos;
  std::vector<MWAWFont> fonts;
  if (!readFonts(line.m_entry, fonts, textPos))
    return false;
  sendText(text, fonts, textPos);
  if (input->tell()!=endPos) {
    ascii().addPos(input->tell());
    ascii().addNote("Text(II):#");
  }
  return true;
}

bool MDWParser::readCompressedText(MDWParserInternal::LineInfo const &line)
{
  if (!line.m_entry.valid())
    return false;
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;

  long pos=line.m_entry.begin();
  long endPos=line.m_entry.end();
  input->seek(pos, WPX_SEEK_SET);
  int num = (int) input->readULong(2);
  if (pos+num/2 > endPos) {
    MWAW_DEBUG_MSG(("MDWParser::readCompressedText: numChar is too long\n"));
    return false;
  }

  f << "Text:";
  if (line.m_listLevel) {
    f << "[list=" << line.m_listLevel;
    switch(line.m_listType) {
    case 0:
      f << "Head,";
      break; // Heading
    case 1:
      f << "Unl,";
      break; // unlabelled
    case 2:
      f << "Num,";
      break; // bullet
    case 3:
      f << "Bul,";
      break; // paragraph
    default:
      f << "[#type=" << line.m_listType << "],";
    }
    f << "],";
  }
  int actualChar = 0;
  bool actualCharSet = false;
  std::string text("");
  for (int n = 0; n < num; n++) {
    if (input->tell() >= endPos) {
      MWAW_DEBUG_MSG(("MDWParser::readCompressedText: entry is too short\n"));
      return false;
    }
    int highByte = 0;
    for (int st = 0; st < 3; st++) {
      int actVal;
      if (!actualCharSet ) {
        if (input->atEOS()) {
          MWAW_DEBUG_MSG(("MDWParser::readCompressedText: text is too long\n"));
          return false;
        }
        actualChar = (int) input->readULong(1);
        actVal = (actualChar >> 4);
      } else
        actVal = (actualChar & 0xf);
      actualCharSet = !actualCharSet;
      if (st == 0) {
        if (actVal == 0xf) continue;
        text += (char) m_state->m_compressCorr[(size_t) actVal];
        break;
      }
      if (st == 1) { // high bytes
        highByte = (actVal<<4);
        continue;
      }
      if (highByte == 0 && actVal==0) {
        MWAW_DEBUG_MSG(("rser::readCompressedText: find 0 char\n"));
        continue;
      }
      text += (char) (highByte | actVal);
    }
  }
  f << text;

  // realign to 2
  if (input->tell()&1)
    input->seek(1,WPX_SEEK_CUR);

  ascii().addPos(line.m_entry.begin());
  ascii().addNote(f.str().c_str());

  std::vector<int> textPos;
  std::vector<MWAWFont> fonts;
  if (!readFonts(line.m_entry, fonts, textPos))
    return false;
  sendText(text, fonts, textPos);
  if (input->tell()!=endPos) {
    ascii().addPos(input->tell());
    ascii().addNote("Text(II):#");
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the fonts
////////////////////////////////////////////////////////////
bool MDWParser::readFonts(MWAWEntry const &entry, std::vector<MWAWFont> &fonts,
                          std::vector<int> &textPos)
{
  textPos.resize(0);
  fonts.resize(0);

  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;

  long pos = input->tell();
  long endPos= entry.end();
  if (pos+2 > endPos) {
    MWAW_DEBUG_MSG(("MDWParser::readFonts: zone is too short\n"));
    return false;
  }
  int sz = (int) input->readULong(2);
  if (pos+2+sz > endPos || (sz%6)) {
    MWAW_DEBUG_MSG(("MDWParser::readFonts: sz is odd\n"));
    return false;
  }
  int N = sz/6;
  f.str("");
  f << "Text[Font]:N=" << N;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    f << "Text:Font" << i << ":";
    int tPos = (int) input->readULong(2);
    textPos.push_back(tPos);
    f << "pos=" << tPos << ",";
    MWAWFont font;
    font.setSize((int) input->readULong(1));
    int flag = (int) input->readULong(1);
    uint32_t flags = 0;
    // bit 1 = plain
    if (flag&0x1) flags |= MWAW_BOLD_BIT;
    if (flag&0x2) flags |= MWAW_ITALICS_BIT;
    if (flag&0x4) font.setUnderlineStyle(MWAWBorder::Single);
    if (flag&0x8) flags |= MWAW_EMBOSS_BIT;
    if (flag&0x10) flags |= MWAW_SHADOW_BIT;
    if (flag&0x20) flags |= MWAW_SUPERSCRIPT100_BIT;
    if (flag&0x40) flags |= MWAW_SUBSCRIPT100_BIT;
    if (flag&0x80) f << "#fFlags80,";
    font.setFlags(flags);
    font.setId((int) input->readULong(2));
    fonts.push_back(font);
#ifdef DEBUG
    f << font.getDebugString(m_convertissor);
#endif
    input->seek(pos+6, WPX_SEEK_SET);

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool MDWParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = MDWParserInternal::State();

  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;

  int const headerSize=0x50;
  if (!isFilePos(headerSize)) {
    MWAW_DEBUG_MSG(("MDWParser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0,WPX_SEEK_SET);
  if (input->readULong(2) != 0x7704)
    return false;
  if (header)
    header->reset(MWAWDocument::MINDW, 2);

  f << "Entries(Header):";
  for (int i = 0; i < 3; i++)
    m_state->m_numLinesByZone[i] = (int) input->readULong(2);
  f << "nLines=" << m_state->m_numLinesByZone[0] << ",";
  if (m_state->m_numLinesByZone[1])
    f << "nLines[Head]=" << m_state->m_numLinesByZone[1] << ",";
  if (m_state->m_numLinesByZone[2])
    f << "nLines[Foot]=" << m_state->m_numLinesByZone[2] << ",";
  long val;
  for (int i = 0; i < 2; i++) { // find [01] [15]
    val = input->readLong(1);
    if (val) f << "f" << i << "=" << val << ",";
  }
  for (int i = 0; i < 3; i++) { // always 0 ?
    val = input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  val = (long) input->readULong(2); // always 0x7fff
  if (val != 0x7fff) f << "g3=" << std::hex << val << std::dec << ",";
  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  long pos=input->tell();
  f.str("");
  f << "Header(II):";
  val = input->readLong(2); // 0 or 0x80
  if (val) f << "f0=" << val << ",";
  for (int i = 1; i < 5; i++) { // 08, 0, [60|62], [40|60|66|70]
    val = (long) input->readULong(1);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  for (int i = 0; i < 2; i++) { // -1, -1|0
    val = input->readLong(1);
    if (!val) continue;
    f << "g" << i;
    if (val!=-1) f << "=" << val;
    f << ",";
  }
  val = input->readLong(2); // always 1 ?
  if (val != 1) f << "g2=" << val << ",";
  for (int i = 0; i < 18; i++) { // always 0 ?
    val = input->readLong(2);
    if (val) f << "h" << i << "=" << val << ",";
  }
  val = input->readLong(2); // always 0x10 ?
  if (val!=0x10) f << "fl0=" << std::hex << val << std::dec << ",";
  val = input->readLong(2); // always 0x100, maybe textFirstPos ?
  if (val!=0x100) f << "fl1=" << std::hex << val << std::dec << ",";
  val = input->readLong(2);// always 0x0 ?
  if (val) f << "fl2=" << std::hex << val << std::dec << ",";
  val = input->readLong(2);// always 0x1 ?
  if (val != 1) f << "fl3=" << std::hex << val << std::dec << ",";
  for (int i = 0; i < 4; i++) { // always 0 ?
    val = input->readLong(2);
    if (val) f << "j" << i << "=" << val << ",";
  }

  bool ok = true;
  if (strict) {
    // check the line info block size
    input->seek(0x50, WPX_SEEK_SET);
    for (int i = 0; i < 3; i++) {
      input->seek(4, WPX_SEEK_CUR);
      if (input->readLong(4) == 32*m_state->m_numLinesByZone[i])
        continue;
      ok = false;
      break;
    }
    input->seek(0x50, WPX_SEEK_SET);
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  ascii().addPos(input->tell());
  ascii().addNote("_");

  return ok;
}

////////////////////////////////////////////////////////////
// read the line info zones
////////////////////////////////////////////////////////////
bool MDWParser::readLinesInfo(MWAWEntry &entry)
{
  if (!entry.valid())
    return false;
  if (entry.id() < 0 || entry.id() >= 3) {
    MWAW_DEBUG_MSG(("MDWParser::readLinesInfo: bad entry id %d\n", entry.id()));
    return false;
  }
  if (entry.length()%32) {
    MWAW_DEBUG_MSG(("MDWParser::readLinesInfo: the size seems odd\n"));
    return false;
  }
  if (entry.isParsed()) {
    MWAW_DEBUG_MSG(("MDWParser::readLinesInfo: entry is already parsed\n"));
    return true;
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);
  int N = int(entry.length())/32;

  libmwaw::DebugStream f;
  long val;
  MDWParserInternal::ZoneInfo &textZone = m_state->m_zones[entry.id()];
  textZone.m_linesList.clear();
  for (int n = 0; n < N; n++) {
    MDWParserInternal::LineInfo line;

    pos = input->tell();
    f.str("");
    line.m_type = (int) input->readLong(1); // 0, fd, ff
    line.m_height = (int) input->readULong(1);
    line.m_y = (int) input->readLong(2);
    line.m_page = (int)input->readULong(1); // check me
    val = input->readLong(2); // -1 or between 5d7-5de
    f << "f0=" << std::hex << val << std::dec << ",";
    for (int i=0; i < 2; i++)
      line.m_flags[i] = (int) input->readULong(1);
    long ptr = (long) input->readULong(1);
    line.m_entry.setBegin((ptr << 16) | (long) input->readULong(2));
    line.m_entry.setLength((long) input->readULong(2));
    line.m_extra=f.str();
    for (int i=2; i < 4; i++)
      line.m_flags[i] = (int) input->readULong(1);
    line.m_listLevel = (int) input->readLong(2);
    line.m_listType = (int) input->readLong(2); //0: heading, 1: unlabelled 2: paragraph, 3: bullet
    /** 01[45]0 */

    textZone.m_linesList.push_back(line);

    f.str("");
    f << "LineInfo-" << entry.id() << "[" << n << "]:" << line;
    ascii().addDelimiter(input->tell(),'|');
    input->seek(pos+32, WPX_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the last zone ( use ?)
////////////////////////////////////////////////////////////
bool MDWParser::readLastZone(MWAWEntry &entry)
{
  if (!entry.valid())
    return false;
  if (entry.length()!=0x8) {
    MWAW_DEBUG_MSG(("MDWParser::readLastZone: the size seems odd\n"));
    if (entry.length() < 0x8)
      return false;
  }
  if (entry.isParsed()) {
    MWAW_DEBUG_MSG(("MDWParser::readLastZone: entry is already parsed\n"));
    return true;
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "LastZone:";
  long val = (long) input->readULong(4);
  if (val != pos) {
    MWAW_DEBUG_MSG(("MDWParser::readLastZone: the ptr seems odd\n"));
    f << "#ptr=" << std::hex << val << std::dec << ",";
  }
  val = (long) input->readULong(2); // always 0x7fff
  if (val != 0x7fff) f << "f0=" << std::hex << val << std::dec << ",";
  val = input->readLong(2); // always -1
  if (val != -1)  f << "f1=" << val << ",";
  if (entry.length() != 0x8)
    ascii().addDelimiter(input->tell(), '|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read zone12 ( use ?)
////////////////////////////////////////////////////////////
bool MDWParser::readZone12(MWAWEntry &entry)
{
  if (!entry.valid())
    return false;
  if (entry.length()%12) {
    MWAW_DEBUG_MSG(("MDWParser::readZone12: the size seems odd\n"));
    return false;
  }
  if (entry.isParsed()) {
    MWAW_DEBUG_MSG(("MDWParser::readZone12: entry is already parsed\n"));
    return true;
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  int num = int(entry.length()/12);
  libmwaw::DebugStream f;
  for (int i = 0; i < num; i++) {
    pos = input->tell();
    f.str("");
    f << "Zone12[" << i << "]:";

    input->seek(pos+12, WPX_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool MDWParser::readPrintInfo(MWAWEntry &entry)
{
  if (!entry.valid())
    return false;
  if (entry.length()!=0x78) {
    MWAW_DEBUG_MSG(("MDWParser::readPrintInfo: the size seems odd\n"));
    if (entry.length() < 0x78)
      return false;
  }
  if (entry.isParsed()) {
    MWAW_DEBUG_MSG(("MDWParser::readPrintInfo: entry is already parsed\n"));
    return true;
  }
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

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

  m_pageSpan.setMarginTop(lTopMargin.y()/72.0);
  m_pageSpan.setMarginBottom(botMarg/72.0);
  m_pageSpan.setMarginLeft(lTopMargin.x()/72.0);
  m_pageSpan.setMarginRight(rightMarg/72.0);
  m_pageSpan.setFormLength(paperSize.y()/72.);
  m_pageSpan.setFormWidth(paperSize.x()/72.);

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(pos+0x78, WPX_SEEK_SET);
  if (long(input->tell()) != pos+0x78) {
    MWAW_DEBUG_MSG(("MDWParser::readPrintInfo: file is too short\n"));
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////
// send the text, ...
////////////////////////////////////////////////////////////
void MDWParser::sendText(std::string const &text, std::vector<MWAWFont> const &fonts, std::vector<int> const &textPos)
{
  if (!m_listener || !text.length())
    return;
  size_t numFonts = fonts.size();
  if (numFonts != textPos.size()) {
    MWAW_DEBUG_MSG(("MDWParser::sendText: find fonts/textPos incompatibility\n"));
    if (numFonts > textPos.size())
      numFonts = textPos.size();
  }
  size_t actFontId = 0;
  MWAWFont actFont(3,12);
  size_t numChar = text.length();
  for (size_t c = 0; c < numChar; c++) {
    if (actFontId < numFonts && int(c) == textPos[actFontId]) {
      actFont = fonts[actFontId++];
      setProperty(actFont);
    }
    unsigned char ch = (unsigned char)text[c];
    switch(ch) {
    case 0x9:
      m_listener->insertTab();
      break;
    case 0xd:
      m_listener->insertEOL(c!=numChar-1);
      break;
    default: {
      if (ch < 0x20) {
        MWAW_DEBUG_MSG(("MDWParser::sendText: find old character: %x\n", int(ch)));
        break;
      }
      int unicode = m_convertissor->unicode (actFont.id(), (unsigned char) ch);
      if (unicode == -1)
        m_listener->insertCharacter((uint8_t) ch);
      else
        m_listener->insertUnicode((uint32_t) unicode);
      break;
    }
    }
  }
}

void MDWParser::setProperty(MWAWFont const &font)
{
  if (!m_listener) return;
  MWAWFont ft;
  font.sendTo(m_listener.get(), m_convertissor, ft);
}

void MDWParser::setProperty(MWAWParagraph const &para)
{
  if (!m_listener) return;
  m_state->m_actParagraph = para;
  para.send(m_listener);
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
