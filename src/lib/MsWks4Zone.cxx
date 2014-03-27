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
#include <vector>

#include <librevenge/librevenge.h>

#include "MWAWTextListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWSection.hxx"
#include "MWAWSubDocument.hxx"

#include "MsWksGraph.hxx"
#include "MsWksDocument.hxx"

#include "MsWks4Text.hxx"

#include "MsWks4Zone.hxx"


/** Internal: the structures of a MsWks4Zone */
namespace MsWks4ZoneInternal
{
/**Internal: some data are dupplicated, an enum to know which picture to ignored.
 *
 * For intance, the header text is stored in a separate ole, and we find
 * in the main ole, a picture which represents the headers */
enum { IgnoreFrame = -4 };

//! Internal: a frame ( position, type, ...)
struct Frame {
  //! the type of the frame which can represent header, footer, textbox, ...
  enum Type { Unknown = 0, Header, Footer, Table, Object, Textbox };

  //! constructor
  Frame() : m_type(Unknown), m_position(), m_pictId(), m_error("")
  {
    m_position.setPage(-3);
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Frame const &ft);

  //! the frame type
  Type m_type;

  //! the position of the frame in the document
  MWAWPosition m_position;
  //! some frames are associated with a picture stored in this entry ( name + id)
  MWAWEntry m_pictId;
  //! a string to store unparsed data
  std::string m_error;
};
//! friend operator<< for frame
std::ostream &operator<<(std::ostream &o, Frame const &ft)
{
  switch (ft.m_type) {
  case Frame::Header:
    o<< "header,";
    break;
  case Frame::Footer:
    o<< "footer,";
    break;
  case Frame::Table:
    o<< "table,";
    break;
  case Frame::Textbox:
    o<< "textbox,";
    break;
  case Frame::Object:
    o<< "object,";
    break;
  case Frame::Unknown:
  default:
    break;
  }

  int page = ft.m_position.page();
  switch (page) {
  case -1:
    o << "allpages,";
    break;
  case -2:
    o << "undef,";
    break;
  case -3:
    o << "def,";
    break;
  default:
    if (page <= 0) o << "###page=" << page << ",";
    break;
  }
  if (ft.m_pictId.name().length())
    o << "pict='" << ft.m_pictId.name() << "':" << ft.m_pictId.id() << ",";

  o << ft.m_position;
  if (!ft.m_error.empty()) o << "errors=(" << ft.m_error << ")";
  return o;
}

//!Internal: the state of a MsWks4Zone
struct State {
  //! constructor
  State() : m_mainOle(false), m_numColumns(1), m_hasColumnSep(false), m_parsed(false),
    m_actPage(0), m_numPages(0), m_defFont(20,12), m_framesList(),
    m_headerHeight(0), m_footerHeight(0) { }

  //! true if we parse the main MN0
  bool m_mainOle;

  //! the number of column
  int m_numColumns;
  //! true if a line is added to separated the column
  bool m_hasColumnSep;
  //! a flag to known if the ole is already parsed
  bool m_parsed;

  int m_actPage /** the actual page*/, m_numPages /* the number of pages */;

  //! the default font
  MWAWFont m_defFont;

  //! the list of frames
  std::vector<Frame> m_framesList;

  int m_headerHeight/** the header height if known */, m_footerHeight /** the footer height if known */;
};
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MsWks4Zone::MsWks4Zone(MWAWInputStreamPtr input, MWAWParserStatePtr parserState,
                       MWAWParser &parser, std::string const &oleName) :
  m_mainParser(&parser), m_parserState(parserState), m_state(), m_document()
{
  m_document.reset(new MsWksDocument(input, parser));
  setAscii(oleName);
  m_parserState->m_version=4;
  init();
}

MsWks4Zone::~MsWks4Zone()
{
}

////////////////////////////////////////////////////////////
// small helper to manage interaction between parent and child, ...
////////////////////////////////////////////////////////////
void MsWks4Zone::init()
{
  m_state.reset(new MsWks4ZoneInternal::State);
  m_document->getTextParser4()->setDefault(m_state->m_defFont);
}

MWAWInputStreamPtr MsWks4Zone::getInput()
{
  return m_document->m_input;
}

void MsWks4Zone::setAscii(std::string const &oleName)
{
  std::string fName = libmwaw::Debug::flattenFileName(oleName);
  m_document->initAsciiFile(fName);
}

libmwaw::DebugFile &MsWks4Zone::ascii()
{
  return m_document->ascii();
}

void MsWks4Zone::readFootNote(int id)
{
  m_document->getTextParser4()->readFootNote(m_document->getInput(), id);
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
double MsWks4Zone::getTextHeight() const
{
  return m_parserState->m_pageSpan.getPageLength()-m_state->m_headerHeight/72.0-m_state->m_footerHeight/72.0;
}

////////////////////////////////////////////////////////////
// text positions
////////////////////////////////////////////////////////////
void MsWks4Zone::newPage(int number, bool /*soft*/)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  long pos = m_document->getInput()->tell();
  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!m_parserState->getMainListener() || m_state->m_actPage == 1)
      continue;
    // FIXME: find a way to force the page break to happen
    //    ie. graphParser must add a space to force it :-~
    if (m_state->m_mainOle) m_parserState->getMainListener()->insertBreak(MWAWTextListener::PageBreak);

    MsWksGraph::SendData sendData;
    sendData.m_type = MsWksGraph::SendData::RBDR;
    sendData.m_anchor =  MWAWPosition::Page;
    sendData.m_page = m_state->m_actPage;
    m_document->getGraphParser()->sendObjects(sendData);
  }
  m_document->getInput()->seek(pos, librevenge::RVNG_SEEK_SET);
}

MWAWEntry MsWks4Zone::getTextPosition() const
{
  return m_document->getTextParser4()->m_textPositions;
}

////////////////////////////////////////////////////////////
// create the main listener ( given a header and a footer document)
////////////////////////////////////////////////////////////
MWAWTextListenerPtr MsWks4Zone::createListener(librevenge::RVNGTextInterface *interface)
{
  std::vector<MWAWPageSpan> pageList;
  m_state->m_actPage = 0;
  m_document->getPageSpanList(pageList, m_state->m_numPages);
  MWAWTextListenerPtr res(new MWAWTextListener(*m_parserState, pageList, interface));

  // ok, now we can update the page position
  std::vector<int> linesH, pagesH;
  pagesH.resize(size_t(m_state->m_numPages), int(72.*getTextHeight()));
  m_document->getGraphParser()->computePositions(-1, linesH, pagesH);
  m_document->getGraphParser()->setPageLeftTop
  (Vec2f(72.f*float(m_parserState->m_pageSpan.getMarginLeft()),
         72.f*float(m_parserState->m_pageSpan.getMarginTop())+float(m_state->m_headerHeight)));

  return res;
}

////////////////////////////////////////////////////////////
//
// entry management
//
////////////////////////////////////////////////////////////
bool MsWks4Zone::parseHeaderIndexEntry(MWAWInputStreamPtr &input)
{
  long pos = input->tell();
  m_document->ascii().addPos(pos);

  libmwaw::DebugStream f;

  uint16_t cch = uint16_t(input->readULong(2));

  // check if the entry can be read
  input->seek(pos + cch, librevenge::RVNG_SEEK_SET);
  if (input->tell() != pos+cch) {
    MWAW_DEBUG_MSG(("MsWks4Zone:parseHeaderIndexEntry error: incomplete entry\n"));
    m_document->ascii().addNote("###IndexEntry incomplete (ignored)");
    return false;
  }
  input->seek(pos + 2, librevenge::RVNG_SEEK_SET);

  if (0x18 != cch) {
    if (cch < 0x18) {
      input->seek(pos + cch, librevenge::RVNG_SEEK_SET);
      m_document->ascii().addNote("MsWks4Zone:parseHeaderIndexEntry: ###IndexEntry too short(ignored)");
      if (cch < 10) throw libmwaw::ParseException();
      return true;
    }
  }

  std::string name;

  // sanity check
  for (size_t i =0; i < 4; i++) {
    name.append(1, char(input->readULong(1)));

    if (name[i] != 0 && name[i] != 0x20 &&
        (41 > (uint8_t)name[i] || (uint8_t)name[i] > 90)) {
      MWAW_DEBUG_MSG(("MsWks4Zone:parseHeaderIndexEntry: bad character=%u (0x%02x) in name in header index\n",
                      (uint8_t)name[i], (uint8_t)name[i]));
      m_document->ascii().addNote("###IndexEntry bad name(ignored)");

      input->seek(pos + cch, librevenge::RVNG_SEEK_SET);
      return true;
    }
  }

  f << "Entries("<<name<<")";
  if (cch != 24) f << ", ###size=" << int(cch);
  int id = (int) input->readULong(2);
  f << ", id=" << id << ", (";
  for (int i = 0; i < 2; i ++) {
    int val = (int) input->readLong(2);
    f << val << ",";
  }

  std::string name2;
  for (size_t i =0; i < 4; i++)
    name2.append(1, (char) input->readULong(1));
  f << "), " << name2;

  MWAWEntry hie;
  hie.setName(name);
  hie.setType(name2);
  hie.setId(id);
  hie.setBegin((long) input->readULong(4));
  hie.setLength((long) input->readULong(4));

  f << ", offset=" << std::hex << hie.begin() << ", length=" << hie.length();

  if (cch != 0x18) {
    m_document->ascii().addDelimiter(pos+0x18, '|');
    f << ",#extraData";
  }

  input->seek(hie.end(), librevenge::RVNG_SEEK_SET);
  if (input->tell() != hie.end()) {
    f << ", ###ignored";
    m_document->ascii().addNote(f.str().c_str());
    input->seek(pos + cch, librevenge::RVNG_SEEK_SET);
    return true;
  }


  m_document->getEntryMap().insert(std::multimap<std::string, MWAWEntry>::value_type(name, hie));

  m_document->ascii().addPos(pos);
  m_document->ascii().addNote(f.str().c_str());

  m_document->ascii().addPos(hie.begin());
  f.str("");
  f << name;
  if (name != name2) f << "/" << name2;
  f << ":" << std::dec << id;
  m_document->ascii().addNote(f.str().c_str());

  m_document->ascii().addPos(hie.end());
  m_document->ascii().addNote("_");

  input->seek(pos + cch, librevenge::RVNG_SEEK_SET);
  return true;
}

bool MsWks4Zone::parseHeaderIndex(MWAWInputStreamPtr &input)
{
  m_document->getEntryMap().clear();
  input->seek(0x08, librevenge::RVNG_SEEK_SET);

  long pos = input->tell();
  int i0 = (int) input->readLong(2);
  int i1 = (int) input->readLong(2);
  uint16_t n_entries = (uint16_t) input->readULong(2);
  // fixme: sanity check n_entries

  libmwaw::DebugStream f;
  f << "Header: N=" << n_entries << ", " << i0 << ", " << i1 << "(";

  for (int i = 0; i < 4; i++)
    f << std::hex << input->readLong(2) << ",";
  f << "), ";
  f << "end=" << std::hex << input->readLong(2);

  m_document->ascii().addPos(pos);
  m_document->ascii().addNote(f.str().c_str());

  input->seek(0x18, librevenge::RVNG_SEEK_SET);
  bool readSome = false;
  do {
    if (input->isEnd()) return readSome;

    pos = input->tell();
    f.str("");
    uint16_t unknown1 = (uint16_t) input->readULong(2); // function of the size of the entries ?

    uint16_t n_entries_local = (uint16_t) input->readULong(2);
    f << "Header("<<std::hex << unknown1<<"): N=" << std::dec << n_entries_local;

    if (n_entries_local > 0x20) {
      MWAW_DEBUG_MSG(("MsWks4Zone::parseHeaderIndex: error: n_entries_local=%i\n", n_entries_local));
      return readSome;
    }

    uint32_t next_index_table = (uint32_t) input->readULong(4);
    f << std::hex << ", nextHeader=" << next_index_table;
    if (next_index_table != 0xFFFFFFFF && long(next_index_table) < pos) {
      MWAW_DEBUG_MSG(("MsWks4Zone::parseHeaderIndex: error: next_index_table=%x decreasing !!!\n", next_index_table));
      return readSome;
    }

    m_document->ascii().addPos(pos);
    m_document->ascii().addNote(f.str().c_str());

    do {
      if (!parseHeaderIndexEntry(input)) return readSome;

      readSome=true;
      n_entries--;
      n_entries_local--;
    }
    while (n_entries > 0 && n_entries_local);

    if (0xFFFFFFFF == next_index_table && n_entries > 0) {
      MWAW_DEBUG_MSG(("MsWks4Zone::parseHeaderIndex: error: expected more header index entries\n"));
      return n_entries > 0;
    }

    if (0xFFFFFFFF == next_index_table)	break;

    if (input->seek(long(next_index_table), librevenge::RVNG_SEEK_SET) != 0) return readSome;
  }
  while (n_entries > 0);

  return true;
}

////////////////////////////////////////////////////////////
// parse the ole part and create the main part
////////////////////////////////////////////////////////////
bool MsWks4Zone::createZones(bool mainOle)
{
  if (m_state->m_parsed) return true;

  std::multimap<std::string, MWAWEntry> &entryMap=m_document->getEntryMap();
  MWAWInputStreamPtr input = m_document->getInput();
  m_state->m_parsed = true;

  entryMap.clear();

  m_document->ascii().addPos(0);
  m_document->ascii().addNote("FileHeader");
  /* header index */
  if (!parseHeaderIndex(input)) return false;
  // the text structure
  if (!m_document->getTextParser4()->readStructures(input, mainOle)) return !mainOle;

  std::multimap<std::string, MWAWEntry>::iterator pos;

  // DOP, PRNT
  pos = entryMap.find("PRNT");
  if (entryMap.end() != pos) {
    pos->second.setParsed(true);
    MWAWPageSpan page;
    if (readPRNT(input, pos->second, page) && mainOle) m_parserState->m_pageSpan = page;
  }
  pos = entryMap.find("DOP ");
  if (entryMap.end() != pos) {
    MWAWPageSpan page;
    if (readDOP(input, pos->second, page) && mainOle) m_parserState->m_pageSpan = page;
  }

  // RLRB
  pos = entryMap.lower_bound("RLRB");
  while (pos != entryMap.end()) {
    MWAWEntry const &entry = pos++->second;
    if (!entry.hasName("RLRB")) break;
    if (!entry.hasType("RLRB")) continue;

    readRLRB(input, entry);
  }

  // SELN
  pos = entryMap.lower_bound("SELN");
  while (entryMap.end() != pos) {
    MWAWEntry const &entry = pos++->second;
    if (!entry.hasName("SELN")) break;
    if (!entry.hasType("SELN")) continue;
    readSELN(input, entry);
  }

  // FRAM
  m_state->m_framesList.resize(0);
  pos = entryMap.lower_bound("FRAM");
  while (entryMap.end() != pos) {
    MWAWEntry const &entry = pos++->second;
    if (!entry.hasName("FRAM")) break;
    if (!entry.hasType("FRAM")) continue;
    readFRAM(input, entry);
  }

  /* Graph data */
  pos = entryMap.lower_bound("RBDR");
  while (pos != entryMap.end()) {
    MWAWEntry const &entry = pos++->second;
    if (!entry.hasName("RBDR")) break;
    if (!entry.hasType("RBDR")) continue;

    if (m_document->getGraphParser()->readRB(input, entry, 0))
      entry.setParsed(true);
  }
  pos = entryMap.lower_bound("RBIL");
  while (pos != entryMap.end()) {
    MWAWEntry const &entry = pos++->second;
    if (!entry.hasName("RBIL")) break;
    if (!entry.hasType("RBIL")) continue;

    if (m_document->getGraphParser()->readRB(input, entry, 0))
      entry.setParsed(true);
  }

  /* read the pictures */
  // in the main block, pict are used to representant the
  // header/footer, so we skip it.
  // In the others block, maybe there can be interesting, so, we read them
  pos = entryMap.lower_bound("PICT");
  while (pos != entryMap.end()) {
    MWAWEntry const &entry = pos++->second;
    if (!entry.hasName("PICT")) break;
    m_document->getGraphParser()->readPictureV4(input, entry);
  }
  return true;
}

////////////////////////////////////////////////////////////
// send all the data corresponding to a zone
////////////////////////////////////////////////////////////
void MsWks4Zone::readContentZones(MWAWEntry const &entry, bool mainOle)
{
  MWAWInputStreamPtr input = m_document->getInput();
  bool oldMain = m_state->m_mainOle;
  m_state->m_mainOle = mainOle;

  MsWksGraph::SendData sendData;
  sendData.m_type = MsWksGraph::SendData::RBDR;
  sendData.m_anchor = mainOle ? MWAWPosition::Page : MWAWPosition::Paragraph;
  sendData.m_page = 0;
  m_document->getGraphParser()->sendObjects(sendData);

  if (mainOle && m_parserState->getMainListener() && m_state->m_numColumns > 1) {
    if (m_parserState->getMainListener()->isSectionOpened())
      m_parserState->getMainListener()->closeSection();
    MWAWSection sec;
    sec.setColumns(m_state->m_numColumns, m_mainParser->getPageWidth()/double(m_state->m_numColumns), librevenge::RVNG_INCH);
    if (m_state->m_hasColumnSep)
      sec.m_columnSeparator=MWAWBorder();
    m_parserState->getMainListener()->openSection(sec);
  }

  MWAWEntry ent(entry);
  if (!ent.valid()) ent = m_document->getTextParser4()->m_textPositions;
  m_document->getTextParser4()->readText(input, ent, mainOle);

  if (!mainOle) {
    m_state->m_mainOle = oldMain;
    return;
  }

  // send the final data
#ifdef DEBUG
  newPage(m_state->m_numPages);
  m_document->getTextParser4()->flushExtra(input);

  sendData.m_type = MsWksGraph::SendData::ALL;
  sendData.m_anchor = MWAWPosition::Char;
  m_document->getGraphParser()->sendObjects(sendData);
#endif
  m_state->m_mainOle = oldMain;

#ifdef DEBUG
  // check if we have parsed all zones
  std::multimap<std::string, MWAWEntry> const &entryMap=m_document->getEntryMap();
  std::multimap<std::string, MWAWEntry>::const_iterator pos;

  pos = entryMap.begin();
  while (entryMap.end() != pos) {
    MWAWEntry const &zone = pos++->second;
    if (zone.isParsed() ||
        zone.hasName("TEXT") || // TEXT entries are managed directly by MsWks4Text
        zone.hasName("INK ")) // INK Zone are ignored: always = 2*99
      continue;
    MWAW_DEBUG_MSG(("MsWks4Zone::readContentZones: WARNING zone %s ignored\n",
                    zone.name().c_str()));
  }
#endif
}

////////////////////////////////////////////////////////////
//
// low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// the printer definition
////////////////////////////////////////////////////////////
bool MsWks4Zone::readPRNT(MWAWInputStreamPtr input, MWAWEntry const &entry, MWAWPageSpan &page)
{
  page = MWAWPageSpan();
  if (!entry.hasType("PRR ")) {
    MWAW_DEBUG_MSG(("Works: unknown PRNT type='%s'\n", entry.type().c_str()));
    return false;
  }

  long debPos = entry.begin();
  input->seek(debPos, librevenge::RVNG_SEEK_SET);
  libmwaw::PrinterInfo info;
  if (!info.read(input)) {
    MWAW_DEBUG_MSG(("Works: error: can not read PRNT\n"));
    return false;
  }
  else {
    Vec2i paperSize = info.paper().size();
    Vec2i pageSize = info.page().size();
    Vec2i margin = paperSize - pageSize;
    margin.set(margin.x()/2, margin.y()/2);

    page.setMarginTop(margin.y()/72.0);
    page.setMarginBottom(margin.y()/72.0);
    page.setMarginLeft(margin.x()/72.0);
    page.setMarginRight(margin.x()/72.0);
    page.setFormLength(pageSize.y()/72.);
    page.setFormWidth(pageSize.x()/72.);

    if (paperSize.y() > paperSize.x())
      page.setFormOrientation(MWAWPageSpan::PORTRAIT);
    else
      page.setFormOrientation(MWAWPageSpan::LANDSCAPE);

    libmwaw::DebugStream f;
    f << info;

    m_document->ascii().addPos(debPos);
    m_document->ascii().addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// document
////////////////////////////////////////////////////////////
bool MsWks4Zone::readDOP(MWAWInputStreamPtr input, MWAWEntry const &entry, MWAWPageSpan &page)
{
  if (!entry.hasType("DOP ")) {
    MWAW_DEBUG_MSG(("Works: unknown DOP type='%s'\n", entry.type().c_str()));
    return false;
  }
  entry.setParsed(true);

  // FIXME: update the page
  page=MWAWPageSpan();

  libmwaw::DebugStream f, f2;

  long debPage = entry.begin();
  long length = entry.length();
  long endPage = entry.end();

  input->seek(debPage, librevenge::RVNG_SEEK_SET);
  int sz = (int) input->readULong(1);
  if (sz != length-1) f << "###sz=" << sz << ",";

  bool ok = true;
  double dim[6] = {-1., -1., -1., -1., -1., -1. };
  while (input->tell() < endPage) {
    long debPos = input->tell();
    int val = (int) input->readULong(1);

    switch (val) {
    case 0x41: // w (or 4000 when frame)
    case 0x42: // h (or 4000 when frame)
    case 0x43: // and margin (y,x)
    case 0x44:
    case 0x45:
    case 0x46: {
      if (debPos+2 > endPage) {
        ok = false;
        break;
      }
      long d = (int) input->readULong(2);
      if (d == 4000) break;
      int pos = val-0x41;
      if (pos > 1) pos += 1-2*(pos%2);
      dim[val-0x41] = double(d)/72.;
      break;
    }

    case 0x4d: { // cols-1 ?
      if (debPos+2 > endPage) {
        ok = false;
        break;
      }
      int v = (int) input->readULong(2);
      m_state->m_numColumns=v+1;
      if (v) f2 << "cols=" << m_state->m_numColumns << ",";
      break;
    }
    case 0x4f: {
      if (debPos+1 > endPage) {
        ok = false;
        break;
      }
      int v = (int) input->readLong(1);
      if (v==1) {
        m_state->m_hasColumnSep=true;
        f2 << "hasColSep,";
      }
      else if (v)
        f2 << "#hasColSeps=" <<  v << ",";
      break;

    }
    case 0x60: // always 1 ?
    case 0x61: {
      if (debPos+2 > endPage) {
        ok = false;
        break;
      }
      int v = (int) input->readULong(2);
      f2 << "f" << val;
      if (v != 1) f2 << "=" << v << "#";
      f2 << ",";
      break;
    }

    case 0x62: { // some flag : 1| 11| 31| 42| 44| 80 | 100 ?
      if (debPos+2 > endPage) {
        ok = false;
        break;
      }
      int v = (int) input->readULong(2);
      f2 << "f" << val << "=" << std::hex << v << std::dec << ",";
      break;
    }

    case 0x6c: // always follows by three 0xFFFF ? white color ?
      if (debPos+6 > endPage) {
        ok = false;
        break;
      }
      f2 << "bkcol?=(";
      for (int i = 0; i < 3; i++) {
        f2 << input->readULong(1) << ",";
        input->readULong(1);
      }
      f2 << "),";
      break;

    case 0x48: // 12
    case 0x4c: { // 36
      if (debPos+2 > endPage) {
        ok = false;
        break;
      }
      int v = (int) input->readLong(2);
      f2 << "f" << val << "=" <<  v << ",";
      break;
    }

    case 0x6a: { // 1
      if (debPos+1 > endPage) {
        ok = false;
        break;
      }
      int v = (int) input->readLong(1);
      f2 << "f" << val << "=" <<  v << ",";
      break;
    }

    case 0x5f: // -1 if header|footer ?
    case 0x63: // -1 if header ?
    case 0x64: // -1 if footer ?
    case 0x66: // 1
    case 0x67: { // 1
      if (debPos+2 > endPage) {
        ok = false;
        break;
      }
      int v = (int) input->readLong(1);
      f2 << "f" << val << "=" <<  v << ",";
      break;
    }

    default:
      ok = false;
      break;
    }

    if (ok) continue;

    if (!ok) {
      m_document->ascii().addPos(debPos);
      m_document->ascii().addNote("DOP ###");

      break;
    }
  }
  bool dimOk=dim[1]>2.*(dim[2]+dim[4]) && dim[0]>2.*(dim[3]+dim[5]);
  for (int i = 2; i < 6; i++)
    dimOk = dimOk && dim[i]>=0;
  if (dimOk) {
    if (dim[1] > dim[0])
      page.setFormOrientation(MWAWPageSpan::PORTRAIT);
    else
      page.setFormOrientation(MWAWPageSpan::LANDSCAPE);
    page.setFormLength(dim[1]);
    page.setFormWidth(dim[0]);
    page.setMarginTop(dim[2]);
    page.setMarginBottom(dim[4]);
    page.setMarginLeft(dim[3]);
    page.setMarginRight(dim[5]);
  }
  if (dim[0] > 0. || dim[1] > 0.)
    f << "sz=" << dim[0] << "x" << dim[1] << ",";
  bool hasMargin = false;
  for (int i = 2; i < 6; i++) if (dim[i] > 0.0) {
      hasMargin = true;
      break;
    }
  if (hasMargin) {
    f << "margin=(";
    for (int i = 2; i < 6; i++) {
      if (dim[i] < 0) f << "_";
      else f << dim[i];
      if (i == 2 || i == 4) f << "x";
      else f << " ";
    }
    f << "),";
  }
  f << f2.str();
  m_document->ascii().addPos(debPage);
  m_document->ascii().addNote(f.str().c_str());

  return dimOk;
}

// the position in the page ? Constant size 0x28c ?
// link to the graphic entry RBDR ?
// does this means RL -> RB while RBDR means RB->DRawing
bool MsWks4Zone::readRLRB(MWAWInputStreamPtr input, MWAWEntry const &entry)
{
  if (entry.length() < 13+32) return false;
  entry.setParsed(true);

  long debPos = entry.begin();
  input->seek(debPos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "BDB1=("; // some kind of bdbox: BDB1
  for (int i = 0; i < 4; i++)
    f << input->readLong(2) << ", ";
  f << "), ";
  f << input->readLong(1) << ", ";
  f << input->readLong(2) << ", ";
  for (int i = 0; i < 2; i++)
    f << input->readLong(1) << ", ";

  m_document->ascii().addPos(debPos);
  m_document->ascii().addNote(f.str().c_str());
  m_document->ascii().addPos(input->tell());
  m_document->ascii().addNote("RLRB(2)");


  debPos = entry.end()-32;
  input->seek(debPos, librevenge::RVNG_SEEK_SET);
  f.str("");
  // v0=[29|72..79|189], v1=[-1|2|4|33], (v2xv3) = dim?=(~700x~1000)
  // seems related to BDB1
  f << "RLRB(3):BDB2(";
  for (int i = 0; i< 4; i++)
    f << input->readLong(2) << ",";
  f << ")," << input->readLong(1) << ","; // always 1 ?
  f << "unk1=(" << std::hex;
  for (int i = 0; i < 9; i++)
    f << std::setw(2) << input->readULong(1) << ",";
  f << ")," << input->readLong(1); // always 1 ?
  f << ",unk2=(" << std::hex;
  for (int i = 0; i < 5; i++)
    f << std::setw(2) << input->readULong(1) << ",";
  // header? : [4f|50|5e|75], 0, 0, 0,[80|8b]
  // footer? : 48, 0, 0, 0,8b
  // qfrm, main : no specific form ?
  f << "),dims=(" << std::dec;
  for (int i = 0; i < 4; i++)
    f << input->readLong(2) << ", ";
  f << "), ";

  m_document->ascii().addPos(debPos);
  m_document->ascii().addNote(f.str().c_str());

  return true;
}

// the frame position
bool MsWks4Zone::readFRAM(MWAWInputStreamPtr input, MWAWEntry const &entry)
{
  libmwaw::DebugStream f;

  long debPage = entry.begin();
  long endPage = entry.end();

  input->seek(debPage, librevenge::RVNG_SEEK_SET);
  int numFram = (int) input->readULong(2);
  if (numFram <= 0) return false;
  entry.setParsed(true);

  f << "N=" << numFram;

  m_document->ascii().addPos(debPage);
  m_document->ascii().addNote(f.str().c_str());

  for (int i = 0; i < numFram; i++) {
    long debPos = input->tell();
    int size = (int) input->readULong(1);
    if (size <= 0) break;

    f.str("");

    bool ok = true;
    long endPos = debPos+1+size;
    if (endPos > endPage) break;

    MsWks4ZoneInternal::Frame frame;
    Vec2f fOrig, fSiz;
    while (input->tell() < endPos) {
      int val = (int) input->readULong(1);
      long pos = input->tell();

      int sz = 0;
      int szChaine = 0;
      bool done = true;
      switch (val) {
      case 0x2e: // x
      case 0x2f: // y
        if (pos+2 > endPos) {
          ok = false;
          break;
        }
        if (val == 0x2e) fOrig.setX(float(input->readLong(2))/72.f);
        else fOrig.setY(float(input->readLong(2))/72.f);
        break;
      case 0x30: // width
      case 0x31: // height
        if (pos+2 > endPos) {
          ok = false;
          break;
        }
        if (val == 0x30) fSiz.setX(float(input->readLong(2))/72.f);
        else fSiz.setY(float(input->readLong(2))/72.f);
        break;
      case 0x3c: { // type : checkme ?
        if (pos+2 > endPos) {
          ok = false;
          break;
        }
        int value = (int) input->readLong(2);
        switch (value) {
        case 1:
          frame.m_type = MsWks4ZoneInternal::Frame::Textbox;
          break;
        case 2:
          frame.m_type = MsWks4ZoneInternal::Frame::Header;
          break;
        case 4:
          frame.m_type = MsWks4ZoneInternal::Frame::Footer;
          break;
        default:
          f << "###type=" << value << ",";
        }
        break;
      }
      case 0x29: { // always -1 ?
        if (pos+2 > endPos) {
          ok = false;
          break;
        }
        int value = (int) input->readLong(2);
        if (value != -1) f << "###29=" << value << ",";
      }
      break;
      case 0x2a: // in main 2a=6, in frame 2a=4, 2b=4
      case 0x2b:
        done = false;
        sz = 1;
        break;
      case 0x3d: // only in textBox, related to the field id of Graphics15
        if (pos+4 > endPos) {
          ok = false;
          break;
        }
        f << "textBoxId=X" << std::hex << input->readLong(4) << "," << std::dec;
        break;
      case 0x3e: { // appear only in QFrm with value ~ 1/2, 2000
        if (pos+4 > endPos) {
          ok = false;
          break;
        }
        f << std::hex << val << "=(" << std::dec;
        for (int j = 0; j < 2; j++)
          f << input->readLong(2) << ",";
        f << "),";
      }
      break;

      case 0x37: {
        if (pos+6 > endPos) {
          ok = false;
          break;
        }
        std::string fName("");
        for (int j = 0; j < 4; j++) {
          char c = (char) input->readULong(1);
          if (c == '\0') continue;
          fName +=  c;
        }
        frame.m_pictId.setName(fName);
        frame.m_pictId.setId((int) input->readULong(2));
        done = true;
      }
      break;
      default:
        done = false;
        break;
      }

      if (!ok) break;
      if (done) continue;

      if ((sz == 0 && szChaine == 0) ||
          (pos + sz + szChaine > endPos)) {
        input->seek(-1, librevenge::RVNG_SEEK_CUR);
        ok = false;
        break;
      }

      f << std::hex << val << "=" << std::dec;

      if (szChaine) {
        std::string s;
        for (int j = 0; j < szChaine; j++)
          s += (char) input->readULong(1);
        f << s << ",";
      }
      if (sz) {
        int v2 = (int) input->readLong(sz);
        f << v2;
      }
      f << ", ";
    }

    frame.m_position=MWAWPosition(fOrig, fSiz);
    frame.m_position.setPage(-3);
    frame.m_error = f.str();
    f.str("");
    m_state->m_framesList.push_back(frame);

    f << "FRAM(" << size << "):" << frame;
    m_document->ascii().addPos(debPos);
    m_document->ascii().addNote(f.str().c_str());
    if (!ok) {
      m_document->ascii().addPos(input->tell());
      m_document->ascii().addNote("FRAM###");
    }

    input->seek(endPos, librevenge::RVNG_SEEK_SET);
  }

  return true;
}

// the selection
bool MsWks4Zone::readSELN(MWAWInputStreamPtr input, MWAWEntry const &entry)
{
  libmwaw::DebugStream f;

  long debPage = entry.begin();
  long endPage = entry.end();

  input->seek(debPage, librevenge::RVNG_SEEK_SET);
  if (endPage-debPage <= 12) {
    MWAW_DEBUG_MSG(("MsWks4Zone::readSELN: SELN size=%ld too short\n", endPage-debPage));
    return false;
  }
  entry.setParsed(true);

  int type = (int) input->readLong(1);
  // 2,3
  switch (type) {
  case 2:
    f << "textPoint, ";
    break;
  case 3:
    f << "textZone, ";
    break;
  default:
    f << "type=###" << type << ",";
    break;
  }
  // 0 0xFF 0
  for (int i = 0; i < 3; i++) {
    int val = (int)  input->readLong(1);
    if ((i%2)+val)
      f << "unk" << i << "=" << val << ",";
  }
  // textBegin, textEnd
  f << "textPos?=(";
  for (int i = 0; i < 2; i++) {
    f << input->readLong(4);
    if (i == 0) f << "<->";
  }
  f << ")";

  // I find f2=-1,f20=-1,f21=-1 and f22=0|1
  // maybe f22=0 -> mainTextZone, f22=1 -> header, ...
  for (int i = 0; i < (endPage-debPage-12)/2; i++) {
    int val = (int) input->readLong(2);
    if (val) f << ",f" << i << "=" << val;
  }

  m_document->ascii().addPos(debPage);
  m_document->ascii().addNote(f.str().c_str());

  if (long(input->tell()) != endPage) {
    m_document->ascii().addPos(input->tell());
    m_document->ascii().addNote("SELN###");
  }
  return true;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
