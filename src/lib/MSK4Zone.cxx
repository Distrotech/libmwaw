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

#include <libwpd/libwpd.h>

#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWSubDocument.hxx"

#include "MSK4Parser.hxx"
#include "MSKGraph.hxx"
#include "MSK4Text.hxx"

#include "MSK4Zone.hxx"


/** Internal: the structures of a MSK4Zone */
namespace MSK4ZoneInternal
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
  Frame() : m_type(Unknown), m_position(), m_pictId(), m_error("") {
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
  switch(page) {
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

//!Internal: the state of a MSK4Zone
struct State {
  //! constructor
  State() : m_mainOle(false), m_numColumns(1), m_parsed(false),
    m_actPage(0), m_numPages(0), m_defFont(20,12), m_framesList(),
    m_headerHeight(0), m_footerHeight(0) { }

  //! true if we parse the main MN0
  bool m_mainOle;

  //! the number of column
  int m_numColumns;

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
MSK4Zone::MSK4Zone
(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header, MSK4Parser &parser, MWAWFontConverterPtr &convert, std::string const &oleName)
  : MSKParser(input, rsrcParser, header), m_mainParser(&parser),
    m_state(), m_entryMap(), m_pageSpan(), m_textParser(), m_graphParser()
{
  setFontConverter(convert);
  setAscii(oleName);
  setVersion(4);
  init();
}

MSK4Zone::~MSK4Zone()
{
  resetListener();
}

////////////////////////////////////////////////////////////
// small helper to manage interaction between parent and child, ...
////////////////////////////////////////////////////////////
void MSK4Zone::init()
{
  resetListener();

  m_state.reset(new MSK4ZoneInternal::State);
  m_textParser.reset(new MSK4Text(*this, getFontConverter()));
  m_textParser->setDefault(m_state->m_defFont);
  m_graphParser.reset(new MSKGraph(getInput(), *this, getFontConverter()));
}

void MSK4Zone::setListener(MWAWContentListenerPtr listen)
{
  MSKParser::setListener(listen);
  m_textParser->setListener(listen);
  m_graphParser->setListener(listen);
}

void  MSK4Zone::setAscii(std::string const &oleName)
{
  std::string fName = libmwaw::Debug::flattenFileName(oleName);
  ascii().open(fName);
}

void MSK4Zone::sendFootNote(int id)
{
  m_mainParser->sendFootNote(id);
}
void MSK4Zone::readFootNote(int id)
{
  m_textParser->readFootNote( getInput(), id);
}

void MSK4Zone::sendFrameText(MWAWEntry const &entry, std::string const &frame)
{
  return m_mainParser->sendFrameText(entry, frame);
}

void MSK4Zone::sendRBIL(int id, Vec2i const &sz)
{
  MSKGraph::SendData sendData;
  sendData.m_type = MSKGraph::SendData::RBIL;
  sendData.m_id = id;
  sendData.m_anchor =  MWAWPosition::Char;
  sendData.m_size = sz;
  m_graphParser->sendObjects(sendData);
}

void MSK4Zone::sendOLE(int id, MWAWPosition const &pos, WPXPropertyList frameExtras)
{
  m_mainParser->sendOLE(id, pos, frameExtras);
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
float MSK4Zone::pageHeight() const
{
  return float(m_pageSpan.getFormLength()-m_pageSpan.getMarginTop()-m_pageSpan.getMarginBottom()-m_state->m_headerHeight/72.0-m_state->m_footerHeight/72.0);
}

float MSK4Zone::pageWidth() const
{
  return float(m_pageSpan.getFormWidth()-m_pageSpan.getMarginLeft()-m_pageSpan.getMarginRight());
}

Vec2f MSK4Zone::getPageTopLeft() const
{
  return Vec2f(float(m_pageSpan.getMarginLeft()),
               float(m_pageSpan.getMarginTop()+m_state->m_headerHeight/72.0));
}

////////////////////////////////////////////////////////////
// new page/text positions
////////////////////////////////////////////////////////////
void MSK4Zone::newPage(int number)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  long pos = getInput()->tell();
  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!getListener() || m_state->m_actPage == 1)
      continue;
    // FIXME: find a way to force the page break to happen
    //    ie. graphParser must add a space to force it :-~
    if (m_state->m_mainOle) getListener()->insertBreak(MWAWContentListener::PageBreak);

    MSKGraph::SendData sendData;
    sendData.m_type = MSKGraph::SendData::RBDR;
    sendData.m_anchor =  MWAWPosition::Page;
    sendData.m_page = m_state->m_actPage;
    m_graphParser->sendObjects(sendData);
  }
  getInput()->seek(pos, WPX_SEEK_SET);
}

MWAWEntry MSK4Zone::getTextPosition() const
{
  return m_textParser->m_textPositions;
}

////////////////////////////////////////////////////////////
// create the main listener ( given a header and a footer document)
////////////////////////////////////////////////////////////
MWAWContentListenerPtr MSK4Zone::createListener
(WPXDocumentInterface *interface, MWAWSubDocumentPtr &header, MWAWSubDocumentPtr &footer)
{
  std::vector<MWAWPageSpan> pageList;
  MWAWPageSpan ps(m_pageSpan);

  if (header)
    ps.setHeaderFooter(MWAWPageSpan::HEADER, MWAWPageSpan::ALL, header);
  if (footer)
    ps.setHeaderFooter(MWAWPageSpan::FOOTER, MWAWPageSpan::ALL, footer);

  int numPages = m_textParser->numPages();
  int graphPages = m_graphParser->numPages(-1);
  if (graphPages>numPages) numPages = graphPages;

  // ok, now we can update the page position
  std::vector<int> linesH, pagesH;
  pagesH.resize(size_t(numPages)+1, int(72.*pageHeight()));
  m_graphParser->computePositions(-1, linesH, pagesH);

  // create all the pages + an empty page, if we have some remaining data...
  for (int i = 0; i <= numPages; i++) pageList.push_back(ps);
  m_state->m_numPages=numPages+1;
  MWAWContentListenerPtr res(new MWAWContentListener(getFontConverter(), pageList, interface));
  return res;
}

////////////////////////////////////////////////////////////
//
// entry management
//
////////////////////////////////////////////////////////////
bool MSK4Zone::parseHeaderIndexEntry(MWAWInputStreamPtr &input)
{
  long pos = input->tell();
  ascii().addPos(pos);

  libmwaw::DebugStream f;

  uint16_t cch = uint16_t(input->readULong(2));

  // check if the entry can be read
  input->seek(pos + cch, WPX_SEEK_SET);
  if (input->tell() != pos+cch) {
    MWAW_DEBUG_MSG(("MSK4Zone:parseHeaderIndexEntry error: incomplete entry\n"));
    ascii().addNote("###IndexEntry incomplete (ignored)");
    return false;
  }
  input->seek(pos + 2, WPX_SEEK_SET);

  if (0x18 != cch) {
    if (cch < 0x18) {
      input->seek(pos + cch, WPX_SEEK_SET);
      ascii().addNote("MSK4Zone:parseHeaderIndexEntry: ###IndexEntry too short(ignored)");
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
      MWAW_DEBUG_MSG(("MSK4Zone:parseHeaderIndexEntry: bad character=%u (0x%02x) in name in header index\n",
                      (uint8_t)name[i], (uint8_t)name[i]));
      ascii().addNote("###IndexEntry bad name(ignored)");

      input->seek(pos + cch, WPX_SEEK_SET);
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

  std::string mess;
  if (cch != 0x18) {
    ascii().addDelimiter(pos+0x18, '|');
    f << ",#extraData";
  }

  input->seek(hie.end(), WPX_SEEK_SET);
  if (input->tell() != hie.end()) {
    f << ", ###ignored";
    ascii().addNote(f.str().c_str());
    input->seek(pos + cch, WPX_SEEK_SET);
    return true;
  }


  m_entryMap.insert(std::multimap<std::string, MWAWEntry>::value_type(name, hie));

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  ascii().addPos(hie.begin());
  f.str("");
  f << name;
  if (name != name2) f << "/" << name2;
  f << ":" << std::dec << id;
  ascii().addNote(f.str().c_str());

  ascii().addPos(hie.end());
  ascii().addNote("_");

  input->seek(pos + cch, WPX_SEEK_SET);
  return true;
}

bool MSK4Zone::parseHeaderIndex(MWAWInputStreamPtr &input)
{
  m_entryMap.clear();
  input->seek(0x08, WPX_SEEK_SET);

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

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(0x18, WPX_SEEK_SET);
  bool readSome = false;
  do {
    if (input->atEOS()) return readSome;

    pos = input->tell();
    f.str("");
    uint16_t unknown1 = (uint16_t) input->readULong(2); // function of the size of the entries ?

    uint16_t n_entries_local = (uint16_t) input->readULong(2);
    f << "Header("<<std::hex << unknown1<<"): N=" << std::dec << n_entries_local;

    if (n_entries_local > 0x20) {
      MWAW_DEBUG_MSG(("MSK4Zone::parseHeaderIndex: error: n_entries_local=%i\n", n_entries_local));
      return readSome;
    }

    uint32_t next_index_table = (uint32_t) input->readULong(4);
    f << std::hex << ", nextHeader=" << next_index_table;
    if (next_index_table != 0xFFFFFFFF && long(next_index_table) < pos) {
      MWAW_DEBUG_MSG(("MSK4Zone::parseHeaderIndex: error: next_index_table=%x decreasing !!!\n", next_index_table));
      return readSome;
    }

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());

    do {
      if (!parseHeaderIndexEntry(input)) return readSome;

      readSome=true;
      n_entries--;
      n_entries_local--;
    } while (n_entries > 0 && n_entries_local);

    if (0xFFFFFFFF == next_index_table && n_entries > 0) {
      MWAW_DEBUG_MSG(("MSK4Zone::parseHeaderIndex: error: expected more header index entries\n"));
      return n_entries > 0;
    }

    if (0xFFFFFFFF == next_index_table)	break;

    if (input->seek(long(next_index_table), WPX_SEEK_SET) != 0) return readSome;
  } while (n_entries > 0);

  return true;
}

////////////////////////////////////////////////////////////
// parse the ole part and create the main part
////////////////////////////////////////////////////////////
bool MSK4Zone::createZones(bool mainOle)
{
  if (m_state->m_parsed) return true;

  MWAWInputStreamPtr input = getInput();
  m_state->m_parsed = true;

  m_entryMap.clear();

  /* header index */
  if (!parseHeaderIndex(input)) return false;
  // the text structure
  if (!m_textParser->readStructures(input, mainOle)) return !mainOle;

  std::multimap<std::string, MWAWEntry>::iterator pos;

  pos = m_entryMap.find("PRNT");
  if (m_entryMap.end() != pos) {
    pos->second.setParsed(true);
    MWAWPageSpan page;
    if (readPRNT(input, pos->second, page) && mainOle) m_pageSpan = page;
  }

  // DOP
  pos = m_entryMap.find("DOP ");
  if (m_entryMap.end() != pos) {
    MWAWPageSpan page;
    if (readDOP(input, pos->second, page)) {
      /* do we need to save the page ? */
    }
  }

  // RLRB
  pos = m_entryMap.lower_bound("RLRB");
  while (pos != m_entryMap.end()) {
    MWAWEntry const &entry = pos->second;
    pos++;
    if (!entry.hasName("RLRB")) break;
    if (!entry.hasType("RLRB")) continue;

    readRLRB(input, entry);
  }

  // SELN
  pos = m_entryMap.lower_bound("SELN");
  while (m_entryMap.end() != pos) {
    MWAWEntry const &entry = pos->second;
    pos++;
    if (!entry.hasName("SELN")) break;
    if (!entry.hasType("SELN")) continue;
    readSELN(input, entry);
  }

  // FRAM
  m_state->m_framesList.resize(0);
  pos = m_entryMap.lower_bound("FRAM");
  while (m_entryMap.end() != pos) {
    MWAWEntry const &entry = pos->second;
    pos++;
    if (!entry.hasName("FRAM")) break;
    if (!entry.hasType("FRAM")) continue;
    readFRAM(input, entry);
  }

  /* Graph data */
  pos = m_entryMap.lower_bound("RBDR");
  while (pos != m_entryMap.end()) {
    MWAWEntry const &entry = pos->second;
    pos++;
    if (!entry.hasName("RBDR")) break;
    if (!entry.hasType("RBDR")) continue;

    m_graphParser->readRB(input, entry);
  }
  pos = m_entryMap.lower_bound("RBIL");
  while (pos != m_entryMap.end()) {
    MWAWEntry const &entry = pos->second;
    pos++;
    if (!entry.hasName("RBIL")) break;
    if (!entry.hasType("RBIL")) continue;

    m_graphParser->readRB(input, entry);
  }

  /* read the pictures */
  // in the main block, pict are used to representant the
  // header/footer, so we skip it.
  // In the others block, maybe there can be interesting, so, we read them
  pos = m_entryMap.lower_bound("PICT");
  while (pos != m_entryMap.end()) {
    MWAWEntry const &entry = pos->second;
    pos++;
    if (!entry.hasName("PICT")) break;
    m_graphParser->readPictureV4(input, entry);
  }
  return true;
}

////////////////////////////////////////////////////////////
// send all the data corresponding to a zone
////////////////////////////////////////////////////////////
void MSK4Zone::readContentZones(MWAWEntry const &entry, bool mainOle)
{
  MWAWInputStreamPtr input = getInput();
  bool oldMain = m_state->m_mainOle;
  m_state->m_mainOle = mainOle;

  MSKGraph::SendData sendData;
  sendData.m_type = MSKGraph::SendData::RBDR;
  sendData.m_anchor = mainOle ? MWAWPosition::Page : MWAWPosition::Paragraph;
  sendData.m_page = 0;
  m_graphParser->sendObjects(sendData);

  if (mainOle && getListener() && m_state->m_numColumns > 1) {
    if (getListener()->isSectionOpened())
      getListener()->closeSection();
    int w = int(72.0*pageWidth()/m_state->m_numColumns);
    std::vector<int> colSize(size_t(m_state->m_numColumns), w);
    getListener()->openSection(colSize, WPX_POINT);
  }

  MWAWEntry ent(entry);
  if (!ent.valid()) ent = m_textParser->m_textPositions;
  m_textParser->readText(input, ent, mainOle);

  if (!mainOle) {
    m_state->m_mainOle = oldMain;
    return;
  }

  // send the final data
#ifdef DEBUG
  newPage(m_state->m_numPages);
  m_textParser->flushExtra(input);

  sendData.m_type = MSKGraph::SendData::ALL;
  sendData.m_anchor = MWAWPosition::Char;
  m_graphParser->sendObjects(sendData);
#endif
  m_state->m_mainOle = oldMain;

#ifdef DEBUG
  // check if we have parsed all zones
  std::multimap<std::string, MWAWEntry>::iterator pos;

  pos = m_entryMap.begin();
  while (m_entryMap.end() != pos) {
    MWAWEntry const &zone = pos->second;
    pos++;
    if (zone.isParsed() ||
        zone.hasName("TEXT") || // TEXT entries are managed directly by MSK4Text
        zone.hasName("INK ")) // INK Zone are ignored: always = 2*99
      continue;
    MWAW_DEBUG_MSG(("MSK4Zone::readContentZones: WARNING zone %s ignored\n",
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
bool MSK4Zone::readPRNT(MWAWInputStreamPtr input, MWAWEntry const &entry, MWAWPageSpan &page)
{
  page = MWAWPageSpan();
  if (!entry.hasType("PRR ")) {
    MWAW_DEBUG_MSG(("Works: unknown PRNT type='%s'\n", entry.type().c_str()));
    return false;
  }

  long debPos = entry.begin();
  input->seek(debPos, WPX_SEEK_SET);
  libmwaw::PrinterInfo info;
  if (!info.read(input)) {
    MWAW_DEBUG_MSG(("Works: error: can not read PRNT\n"));
    return false;
  } else {
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
    f << std::dec << info;

    ascii().addPos(debPos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// document
////////////////////////////////////////////////////////////
bool MSK4Zone::readDOP(MWAWInputStreamPtr input, MWAWEntry const &entry, MWAWPageSpan &page)
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

  input->seek(debPage, WPX_SEEK_SET);
  int sz = (int) input->readULong(1);
  if (sz != length-1) f << "###sz=" << sz << ",";

  bool ok = true;
  double dim[6] = {-1., -1., -1., -1., -1., -1. };
  while (input->tell() < endPage) {
    long debPos = input->tell();
    int val = (int) input->readULong(1);

    switch(val) {
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

    case 0x4f: // 1
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
      ascii().addPos(debPos);
      ascii().addNote("DOP ###");

      break;
    }
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
  ascii().addPos(debPage);
  ascii().addNote(f.str().c_str());

  return ok;
}

// the position in the page ? Constant size 0x28c ?
// link to the graphic entry RBDR ?
// does this means RL -> RB while RBDR means RB->DRawing
bool MSK4Zone::readRLRB(MWAWInputStreamPtr input, MWAWEntry const &entry)
{
  if (entry.length() < 13+32) return false;
  entry.setParsed(true);

  long debPos = entry.begin();
  input->seek(debPos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "BDB1=("; // some kind of bdbox: BDB1
  for (int i = 0; i < 4; i++)
    f << input->readLong(2) << ", ";
  f << "), ";
  f << input->readLong(1) << ", ";
  f << input->readLong(2) << ", ";
  for (int i = 0; i < 2; i++)
    f << input->readLong(1) << ", ";

  ascii().addPos(debPos);
  ascii().addNote(f.str().c_str());
  ascii().addPos(input->tell());
  ascii().addNote("RLRB(2)");


  debPos = entry.end()-32;
  input->seek(debPos, WPX_SEEK_SET);
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

  ascii().addPos(debPos);
  ascii().addNote(f.str().c_str());

  return true;
}

// the frame position
bool MSK4Zone::readFRAM(MWAWInputStreamPtr input, MWAWEntry const &entry)
{
  libmwaw::DebugStream f;

  long debPage = entry.begin();
  long endPage = entry.end();

  input->seek(debPage, WPX_SEEK_SET);
  int numFram = (int) input->readULong(2);
  if (numFram <= 0) return false;
  entry.setParsed(true);

  f << "N=" << numFram;

  ascii().addPos(debPage);
  ascii().addNote(f.str().c_str());

  for (int i = 0; i < numFram; i++) {
    long debPos = input->tell();
    int size = (int) input->readULong(1);
    if (size <= 0) break;

    f.str("");

    bool ok = true;
    long endPos = debPos+1+size;
    if (endPos > endPage) break;

    MSK4ZoneInternal::Frame frame;
    Vec2f fOrig, fSiz;
    while (input->tell() < endPos) {
      int val = (int) input->readULong(1);
      long pos = input->tell();

      int sz = 0;
      int szChaine = 0;
      bool done = true;
      switch(val) {
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
          frame.m_type = MSK4ZoneInternal::Frame::Textbox;
          break;
        case 2:
          frame.m_type = MSK4ZoneInternal::Frame::Header;
          break;
        case 4:
          frame.m_type = MSK4ZoneInternal::Frame::Footer;
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
        input->seek(-1, WPX_SEEK_CUR);
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
    ascii().addPos(debPos);
    ascii().addNote(f.str().c_str());
    if (!ok) {
      ascii().addPos(input->tell());
      ascii().addNote("FRAM###");
    }

    input->seek(endPos, WPX_SEEK_SET);
  }

  return true;
}

// the selection
bool MSK4Zone::readSELN(MWAWInputStreamPtr input, MWAWEntry const &entry)
{
  libmwaw::DebugStream f;

  long debPage = entry.begin();
  long endPage = entry.end();

  input->seek(debPage, WPX_SEEK_SET);
  if (endPage-debPage <= 12) {
    MWAW_DEBUG_MSG(("MSK4Zone::readSELN: SELN size=%ld too short\n", endPage-debPage));
    return false;
  }
  entry.setParsed(true);

  int type = (int) input->readLong(1);
  // 2,3
  switch(type) {
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

  ascii().addPos(debPage);
  ascii().addNote(f.str().c_str());

  if (long(input->tell()) != endPage) {
    ascii().addPos(input->tell());
    ascii().addNote("SELN###");
  }
  return true;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
