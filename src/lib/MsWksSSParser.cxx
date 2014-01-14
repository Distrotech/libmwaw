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
#include <sstream>

#include <librevenge/librevenge.h>


#include "MWAWCell.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWSpreadsheetListener.hxx"
#include "MWAWSubDocument.hxx"

#include "MsWksGraph.hxx"
#include "MsWks3Text.hxx"
#include "MsWksZone.hxx"

#include "MsWksSSParser.hxx"

/** Internal: the structures of a MsWksSSParser */
namespace MsWksSSParserInternal
{
//! Internal: a zone of a MsWksSSParser ( main, header, footer )
struct Zone {
  //! the different type
  enum Type { MAIN, HEADER, FOOTER, NONE };
  //! the constructor
  Zone(Type type=NONE, int zoneId=-1) : m_type(type), m_zoneId(zoneId), m_textId(-1) {}
  //! the zone type
  Type m_type;
  //! the parser zone id
  int m_zoneId;
  //! the text internal id
  int m_textId;
};

/** a cell of a MsWksSSParser */
class Cell : public MWAWCell
{
public:
  /// constructor
  Cell() : m_font(3,12), m_content(), m_noteId(0) { }
  //! returns true if the cell do contain any content
  bool isEmpty() const
  {
    // fixme: if (!MWAWCell::empty()) return false;
    if (hasBorders()) return false;
    if (m_noteId) return false;
    return true;
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Cell const &cell);

  /** the font */
  MWAWFont m_font;
  //! the cell content
  MWAWCellContent m_content;

  /** the note id */
  int m_noteId;
};

//! operator<<
std::ostream &operator<<(std::ostream &o, Cell const &cell)
{
  o << static_cast<MWAWCell const &>(cell);

  if (cell.m_noteId) o << ", noteId=" << cell.m_noteId;
  return o;
}

/** the spreadsheet of a of a MsWksSSParser */
class Spreadsheet
{
public:
  //! constructor
  Spreadsheet() : m_font(3,12), m_widthCols(), m_cells(), m_listPageBreaks(), m_idNoteMap(), m_name("Sheet0") {}
  //! convert the m_widthCols, m_heightRows in a vector of of point size
  std::vector<float> convertInPoint(std::vector<int> const &list, float defSize) const
  {
    size_t numCols=size_t(getRightBottomPosition()[0]+1);
    std::vector<float> res;
    res.resize(numCols);
    for (size_t i = 0; i < numCols; i++) {
      if (i>=list.size() || list[i] < 0) res[i] = defSize;
      else res[i] = float(list[i]);
    }
    return res;
  }

  /** the default font */
  MWAWFont m_font;
  /** the column size in pixels(?) */
  std::vector<int> m_widthCols;
  /** the list of not empty cells */
  std::vector<Cell> m_cells;
  /** the list of page break */
  std::vector<int> m_listPageBreaks;
  /** a map id->note content */
  std::map<int,std::string> m_idNoteMap;
  /** the spreadsheet name */
  std::string m_name;
protected:
  /** returns the last Right Bottom cell position */
  Vec2i getRightBottomPosition() const
  {
    int maxX = 0, maxY = 0;
    size_t numCell = m_cells.size();
    for (size_t i = 0; i < numCell; i++) {
      Vec2i const &p = m_cells[i].position();
      if (p[0] > maxX) maxX = p[0];
      if (p[1] > maxY) maxY = p[1];
    }
    return Vec2i(maxX, maxY);
  }
};

////////////////////////////////////////
//! Internal: the state of a MsWksSSParser
struct State {
  //! constructor
  State() : m_docType(MWAWDocument::MWAW_K_SPREADSHEET), m_spreadsheet(), m_zoneMap(), m_actPage(0), m_numPages(0),
    m_headerText(""), m_footerText(""), m_hasHeader(false), m_hasFooter(false),
    m_pageLength(-1), m_headerHeight(0), m_footerHeight(0)
  {
  }

  //! return a zone
  Zone get(Zone::Type type)
  {
    Zone res;
    if (m_zoneMap.find(int(type)) != m_zoneMap.end())
      res = m_zoneMap[int(type)];
    return res;
  }
  //! the type of document
  MWAWDocument::Kind m_docType;
  /** the spreadsheet */
  Spreadsheet m_spreadsheet;
  //! the list of zone
  std::map<int, Zone> m_zoneMap;
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  std::string m_headerText /**header string v1-2*/, m_footerText /**footer string v1-2*/;
  bool m_hasHeader /** true if there is a header v3*/, m_hasFooter /** true if there is a footer v3*/;
  //! the page length in point (if known)
  int m_pageLength;
  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a MsWksSSParser
class SubDocument : public MWAWSubDocument
{
public:
  enum Type { Zone, Text };
  SubDocument(MsWksSSParser &pars, MWAWInputStreamPtr input, Type type,
              int zoneId, int noteId=-1) :
    MWAWSubDocument(&pars, input, MWAWEntry()), m_type(type), m_id(zoneId), m_noteId(noteId) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const
  {
    return !operator!=(doc);
  }

  //! returns the subdocument \a id
  int getId() const
  {
    return m_id;
  }
  //! sets the subdocument \a id
  void setId(int vid)
  {
    m_id = vid;
  }

  //! the parser function
  void parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  /** the type */
  Type m_type;
  /** the subdocument id*/
  int m_id;
  /** the note id */
  int m_noteId;
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("MsWksSSParser::SubDocument::parse: no listener\n"));
    return;
  }
  assert(m_parser);

  long pos = m_input->tell();
  MsWksSSParser *parser = reinterpret_cast<MsWksSSParser *>(m_parser);
  switch (m_type) {
  case Text:
    parser->sendText(m_id, m_noteId);
    break;
  case Zone:
    parser->sendZone(m_id);
    break;
  default:
    MWAW_DEBUG_MSG(("MsWksSSParser::SubDocument::parse: unexpected zone type\n"));
    break;
  }
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_id != sDoc->m_id) return true;
  if (m_type != sDoc->m_type) return true;
  if (m_noteId != sDoc->m_noteId) return true;
  return false;
}
}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MsWksSSParser::MsWksSSParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWSpreadsheetParser(input, rsrcParser, header), m_state(), m_listZones(), m_graphParser(), m_textParser(), m_zone()
{
  m_zone.reset(new MsWksZone(input, *this));
  if (input->isStructured()) {
    MWAWInputStreamPtr mainOle = input->getSubStreamByName("MN0");
    if (mainOle) {
      MWAW_DEBUG_MSG(("MsWksSSParser::MsWksSSParser: only the MN0 block will be parsed\n"));
      m_zone.reset(new MsWksZone(mainOle, *this));
    }
  }
  init();
}

MsWksSSParser::~MsWksSSParser()
{
}

void MsWksSSParser::init()
{
  resetSpreadsheetListener();
  setAsciiName("main-1");

  m_state.reset(new MsWksSSParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);

  m_graphParser.reset(new MsWksGraph(*this, *m_zone));
  m_textParser.reset(new MsWks3Text(*this, *m_zone));
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void MsWksSSParser::parse(librevenge::RVNGSpreadsheetInterface *docInterface)
{
  assert(m_zone && m_zone->getInput());

  if (!checkHeader(0L) || !m_zone)  throw(libmwaw::ParseException());
  bool ok = true;
  try {
    // create the asciiFile
    m_zone->initAsciiFile(asciiName());

    checkHeader(0L);
    long pos=m_zone->getInput()->tell();
    if (!readDocumentInfo(0x9a))
      m_zone->getInput()->seek(pos, librevenge::RVNG_SEEK_SET);
    ok=readSSheetZone();
    createZones();
    if (ok) {
      createDocument(docInterface);
      sendSpreadsheet();
    }
    m_zone->ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("MsWksSSParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetSpreadsheetListener();
  if (!ok) throw(libmwaw::ParseException());
}

void MsWksSSParser::sendText(int id, int noteId)
{
  if (noteId < 0)
    m_textParser->sendZone(id);
  else
    m_textParser->sendNote(id, noteId);
}

void MsWksSSParser::sendZone(int zoneType)
{
  if (!getSpreadsheetListener()) return;
  MsWksSSParserInternal::Zone zone=m_state->get(MsWksSSParserInternal::Zone::Type(zoneType));
  if (zone.m_zoneId >= 0)
    m_graphParser->sendAll(zone.m_zoneId, zoneType==MsWksSSParserInternal::Zone::MAIN);
  if (zone.m_textId >= 0)
    m_textParser->sendZone(zone.m_textId);
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void MsWksSSParser::createDocument(librevenge::RVNGSpreadsheetInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getSpreadsheetListener()) {
    MWAW_DEBUG_MSG(("MsWksSSParser::createDocument: listener already exist\n"));
    return;
  }

  int vers = version();

  MsWksSSParserInternal::Zone mainZone=m_state->get(MsWksSSParserInternal::Zone::MAIN);
  // update the page
  int numPage = 1;
  if (mainZone.m_textId >= 0 && m_textParser->numPages(mainZone.m_textId) > numPage)
    numPage = m_textParser->numPages(mainZone.m_textId);
  if (mainZone.m_zoneId >= 0 && m_graphParser->numPages(mainZone.m_zoneId) > numPage)
    numPage = m_graphParser->numPages(mainZone.m_zoneId);
  m_state->m_numPages = numPage;
  m_state->m_actPage = 0;

  // create the page list
  MWAWPageSpan ps(getPageSpan());
  int id = m_textParser->getHeader();
  if (id >= 0) {
    if (vers <= 2) m_state->m_headerHeight = 12;
    MWAWHeaderFooter header(MWAWHeaderFooter::HEADER, MWAWHeaderFooter::ALL);
    header.m_subDocument.reset
    (new MsWksSSParserInternal::SubDocument
     (*this, m_zone->getInput(), MsWksSSParserInternal::SubDocument::Text, id));
    ps.setHeaderFooter(header);
  }
  else if (m_state->get(MsWksSSParserInternal::Zone::HEADER).m_zoneId >= 0) {
    MWAWHeaderFooter header(MWAWHeaderFooter::HEADER, MWAWHeaderFooter::ALL);
    header.m_subDocument.reset
    (new MsWksSSParserInternal::SubDocument
     (*this, m_zone->getInput(), MsWksSSParserInternal::SubDocument::Zone, int(MsWksSSParserInternal::Zone::HEADER)));
    ps.setHeaderFooter(header);
  }
  id = m_textParser->getFooter();
  if (id >= 0) {
    if (vers <= 2) m_state->m_footerHeight = 12;
    MWAWHeaderFooter footer(MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
    footer.m_subDocument.reset
    (new MsWksSSParserInternal::SubDocument
     (*this, m_zone->getInput(), MsWksSSParserInternal::SubDocument::Text, id));
    ps.setHeaderFooter(footer);
  }
  else if (m_state->get(MsWksSSParserInternal::Zone::FOOTER).m_zoneId >= 0) {
    MWAWHeaderFooter footer(MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
    footer.m_subDocument.reset
    (new MsWksSSParserInternal::SubDocument
     (*this, m_zone->getInput(), MsWksSSParserInternal::SubDocument::Zone, int(MsWksSSParserInternal::Zone::FOOTER)));
    ps.setHeaderFooter(footer);
  }
  ps.setPageSpan(m_state->m_numPages+1);
  std::vector<MWAWPageSpan> pageList(1,ps);
  //
  MWAWSpreadsheetListenerPtr listen(new MWAWSpreadsheetListener(*getParserState(), pageList, documentInterface));
  setSpreadsheetListener(listen);
  listen->startDocument();
  // time to send page information the graph parser and the text parser
  m_graphParser->setPageLeftTop
  (Vec2f(72.f*float(getPageSpan().getMarginLeft()),
         72.f*float(getPageSpan().getMarginTop())+float(m_state->m_headerHeight)));
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool MsWksSSParser::createZones()
{
  MWAWInputStreamPtr input = m_zone->getInput();
  long pos = input->tell();

  MsWksSSParserInternal::Zone::Type const type = MsWksSSParserInternal::Zone::MAIN;
  MsWksSSParserInternal::Zone newZone(type, int(m_state->m_zoneMap.size()));
  m_state->m_zoneMap.insert(std::map<int,MsWksSSParserInternal::Zone>::value_type(int(type),newZone));
  MsWksSSParserInternal::Zone &mainZone = m_state->m_zoneMap.find(int(type))->second;

  MWAWEntry group;
  readGroup(mainZone, group, 2);

  while (!input->isEnd()) {
    pos = input->tell();
    if (!readZone(mainZone)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
  }

  pos = input->tell();

  if (!input->isEnd())
    m_zone->ascii().addPos(input->tell());
  m_zone->ascii().addNote("Entries(End)");
  m_zone->ascii().addPos(pos+100);
  m_zone->ascii().addNote("_");

  // ok, prepare the data
  m_state->m_numPages = 1;
  return true;
}

////////////////////////////////////////////////////////////
// read a spreadsheet zone
////////////////////////////////////////////////////////////
bool MsWksSSParser::readSSheetZone()
{
  MsWksSSParserInternal::Spreadsheet &sheet=m_state->m_spreadsheet;
  sheet=MsWksSSParserInternal::Spreadsheet();

  MWAWInputStreamPtr input=m_zone->getInput();
  libmwaw::DebugFile &ascFile=m_zone->ascii();
  long pos = input->tell();
  if (!input->checkPosition(pos+0x30+0x30+0x408)) {
    MWAW_DEBUG_MSG(("MsWksSSParser::readSSheetZone:Can not read part A/B\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  ascFile.addPos(pos);
  libmwaw::DebugStream f, f2;
  f << "Entries(SSheet): v0=["; // always 8, -1 ?
  for (int i = 0; i < 2; i++)
    f << input->readLong(1) << ",";
  f << "],";

  int N[2]; // max col, max row
  for (int i = 0; i < 2; i++)
    N[i] = (int) input->readLong(2);
  f << "maxCol=" << N[0] << ","  << "maxRow=" << N[1] << ",";

  f << "unk0=" << std::hex << input->readULong(4) << std::dec << ","; // big number

  int val = (int) input->readLong(2);
  if (val) f << "unkn0.1=" << val << ","; // 0e|7e
  int numPageBreak = (int) input->readLong(2);
  if (numPageBreak) f << "nPgBreak=" << numPageBreak << ",";
  int numPrinter = (int) input->readLong(2);
  if (numPrinter) f << "nPrinters=" << numPrinter << ",";
  val = (int) input->readLong(2);
  if (val) f << "unkn0.2=" << val << ",";
  f << "unk1=" << std::hex << input->readULong(4) << std::dec << ","; // big number
  f << std::dec;

  for (int i = 0; i < 2; i++) {
    val = (int) input->readLong(2);
    if (val)
      f << "w" << i << "=" << val << ",";
  }
  f << "v2=["; // [_,_,_,_,_,_,] or [_,1,3,_,1,3,] or [_,5,5,_,4,2,]
  for (int i = 0; i < 6; i++) {
    val = (int) input->readLong(1);
    if (val)
      f << val << ",";
    else
      f << "_,";
  }
  f << "]";
  f << ", v3=["; // [_,_,_,_,] or [_,_,_,1,] or [_,_,_,11,]
  for (int i = 0; i < 4; i++) {
    val = (int) input->readLong(2);
    if (val)
      f << val << ",";
    else
      f << "_,";
  }
  f << "]";

  f << ", v4=[" << std::hex;
  for (int i = 0; i < 4; i++) {
    val = (int) input->readLong(2);
    if (val)
      f << val << ",";
    else
      f << "_,";
  }
  f << "]" << std::dec;

  ascFile.addNote(f.str().c_str());

  pos = input->tell();
  f.str("");
  ascFile.addPos(pos);
  f << "SSheetB: ";

  std::vector<int> colPos;

  int prevW = -1;
  bool ok = true;
  for (int i = 0; i < 257; i++) {
    int col = (int) input->readULong(2);
    if ((col & 0xFFF) != i+1) {
      ok = false;
      break;
    }
    int w = (int) input->readLong(2);
    if (w > 0) {
      if (prevW > w && i < 200) {
        ok = false;
        break;
      }
      if (prevW > w) w = prevW;
      else prevW = w;
    }
    colPos.push_back(w);
  }

  if (!ok) {
    MWAW_DEBUG_MSG(("MsWksSSParser::readSSheetZone:Can not read part B\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  f << "lastN=" << input->readULong(2);
  f << ", lastV=" << input->readULong(2);
  f << ", width=[";
  std::vector<int> &colSize = sheet.m_widthCols;
  colSize.resize(256);
  for (size_t i = 0; i < 257; i++) {
    int cPos = colPos[i];
    int lastPos = (i == 0) ? 0 : colPos[i-1];
    if (cPos == 0) f << "_";
    else if (cPos == -1) {
      f << "Inf";
      cPos = 0;
    }
    else f << cPos-lastPos;

    if (i< 256) {
      if (cPos) colSize[i]=cPos-lastPos;
      else colSize[i]=0;
    }

    f << ",";
  }
  f << "]" << std::dec;
  ascFile.addNote(f.str().c_str());

  pos = input->tell();
  f.str("");
  ascFile.addPos(pos);
  f << "SSheetC:";
  for (int i = 0; i < 128; i++) {
    val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascFile.addNote(f.str().c_str());

  pos = input->tell();
  f.str("");
  ascFile.addPos(pos);

  if (!input->checkPosition(pos+52)) {
    MWAW_DEBUG_MSG(("MsWksSSParser::readSSheetZone:Can not read part D\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  f << "SSheetD:";
  for (int i = 0; i < 5; i++) {
    val = (int) input->readLong(2);
    if (val) {
      if (i == 3)
        f << "Y=" << val << ",";
      else
        f << "f" << i << "=" << val << ",";
    }
  }
  f << "dim?=["; // some dim rowm == 44 * Y, 44 * Y, 1, 1
  for (int i = 0; i < 4; i++)
    f << input->readLong(2) << ",";
  f << "]";

  // always g0=3ffe, g1=100 ?
  for (int i = 0; i < 2; i++) {
    f << ", g" << i << "="  << std::hex << input->readLong(2) << std::dec;
  }
  int fId = (int) input->readLong(2);
  int fSize = (int) input->readLong(2);
  sheet.m_font = MWAWFont(fId, (float) fSize);
  f << ", [" <<  sheet.m_font.getDebugString(getParserState()->m_fontConverter) << "]";
  // , [0,{33|255},] ?
  f << ", unk=[" << std::dec;
  for (int i = 0; i < 2; i++)
    f << input->readLong(2) << ",";
  f << "]";
  for (int i = 0; i < 11; i++) {
    val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascFile.addNote(f.str().c_str());

  // part E
  Vec2i cellPos(0,0);
  while (1) {
    if (cellPos[1] == N[1]) break;

    long posRow = input->tell();
    unsigned long length = input->readULong(4);
    int fl = 0;
    if (length & 0xF0000000L) {
      if ((length >> 28) != 8) {
        input->seek(posRow, librevenge::RVNG_SEEK_SET);
        break;
      }
      f.str("");
      int nRow = length & 0x0FFFFFFFL;
      f << "SSheetE:skipRow=" << std::hex << nRow;
      cellPos[1]+=nRow;
      ascFile.addPos(posRow);
      ascFile.addNote(f.str().c_str());
      continue;
    }
    long endRow = posRow+(long)length+4;

    if (!input->checkPosition(endRow)) {
      MWAW_DEBUG_MSG(("MsWksSSParser::readSSheetZone:Can not read part E(row)\n"));
      input->seek(posRow, librevenge::RVNG_SEEK_SET);
      return false;
    }

    f.str("");
    cellPos[0]=0;
    f << "SSheetE(Row" << std::dec << cellPos[1]+1 << "): ";
    if (fl) f << std::hex << "fl=" << fl << ",";
    while (input->tell() < endRow-1) {
      pos = input->tell();
      int sz = (int) input->readULong(1);

      if (sz == 0xFE) { // skip next cell
        int numC = (int) input->readULong(1);
        f2.str("");
        f2 << "SSheetE:skipC=" << numC;
        cellPos[0]+=numC;
        ascFile.addPos(pos);
        ascFile.addNote(f2.str().c_str());
        continue;
      }

      if (sz == 0) {
        ascFile.addPos(pos);
        ascFile.addNote("SSheetE:empty");
        ++cellPos[0];
        continue;
      }
      MsWksSSParserInternal::Cell cell;
      if (!readCell(sz, cellPos, cell)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        break;
      }
      if (!cell.isEmpty())
        sheet.m_cells.push_back(cell);
      ++cellPos[0];
    }

    ++cellPos[1];
    if (input->tell() != endRow-1) {
      MWAW_DEBUG_MSG(("MsWksSSParser::readSSheetZone:Can not read part E(cells)\n"));
      input->seek(posRow, librevenge::RVNG_SEEK_SET);
      return false;
    }
    ascFile.addDelimiter(endRow-1,'|');
    val = (int) input->readULong(1);
    if (val == 0) f << "*";
    else if (val != 0xFF)
      f << "##end=" << std::hex << val << std::dec;

    ascFile.addPos(posRow);
    ascFile.addNote(f.str().c_str());
    if (val == 0)
      break;
  }

  pos = input->tell();
  f.str("");
  ascFile.addPos(pos);

  long sz = (long) input->readULong(4);
  f << "SSheetF:sz=" << sz;
  ascFile.addNote(f.str().c_str());

  if (!input->checkPosition(pos+4+sz)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("MsWksSSParser::readSSheetZone:Can not read part F\n"));
    return false;
  }

  input->seek(pos+4+sz, librevenge::RVNG_SEEK_SET);
  long GDebPos = input->tell();
  f.str("");
  f << "SSheetG:" << std::dec;

  for (int i = 0; i < 4; i++) {
    val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  val = (int) input->readULong(2);
  if (val) f << std::hex << "fl=" << val << "," << std::dec;

  if (numPageBreak > 0) {
    sheet.m_listPageBreaks.resize((size_t) numPageBreak);
    f << "pgBreak=[";
    for (size_t pgBreak = 0; pgBreak < (size_t) numPageBreak; pgBreak++) {
      val = (int) input->readULong(2);
      sheet.m_listPageBreaks[pgBreak] = val;
      f << val << ",";
    }
    f << "],";
  }
  if (numPrinter>0) {
    for (int nPrint = 0; nPrint < numPrinter; nPrint++) {
      f << "printer" << nPrint << "=" << input->readULong(2) << ",";
      if (version() <= 2) {
        // only version<=2 seems to contains the print data with size 0x15e
        pos = input->tell();
        MWAWEntry zone;
        if (!readDocumentInfo(0x15e))
          input->seek(pos, librevenge::RVNG_SEEK_SET);
      }
    }
  }

  ascFile.addPos(GDebPos);
  ascFile.addNote(f.str().c_str());

  long HDebPos = input->tell();
  f.str("");
  f << "SSheetH:" << std::dec;
  int numNote = 0;
  while (1) {
    pos = input->tell();
    int idNote = (int) input->readLong(2);
    if (idNote == 0) break;
    if (idNote != ++numNote) {
      input->seek(pos, librevenge::RVNG_SEEK_CUR);
      MWAW_DEBUG_MSG(("MsWksSSParser::readSSheetZone:Pb when reading a note\n"));
      return false;
    }
    // a pascal string
    int pSz = (int) input->readLong(2);
    std::string str="";
    for (int s = 0; s < pSz; s++)
      str += (char) input->readLong(1);
    if (input->tell() != pos+4+pSz) {
      input->seek(pos, librevenge::RVNG_SEEK_CUR);
      MWAW_DEBUG_MSG(("MsWksSSParser::readSSheetZone:Pb when reading a note\n"));
      return false;
    }
    if (pSz) {
      sheet.m_idNoteMap[idNote] = str;
      f << "Note" << idNote << "=\"" << str << "\",";
    }
  }

  ascFile.addPos(HDebPos);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool MsWksSSParser::readCell(int sz, Vec2i const &cellPos, MsWksSSParserInternal::Cell &cell)
{
  cell = MsWksSSParserInternal::Cell();
  cell.setPosition(cellPos);
  if (sz == 0xFF || sz < 5) return false;

  MWAWInputStreamPtr input=m_zone->getInput();
  long debPos = input->tell();

  libmwaw::DebugFile &ascFile=m_zone->ascii();
  libmwaw::DebugStream f;
  long endPos = debPos+sz-5;
  input->seek(endPos+5, librevenge::RVNG_SEEK_SET);
  if (input->tell() !=endPos+5) return false;

  // read the flag
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  int fl[5];
  for (int i  = 0; i < 5; i++)
    fl[i] = (int) input->readULong(1);
  MWAWCell::Format format;
  MWAWCellContent &content=cell.m_content;
  int style = 0;
  int subformat = 0;
  if (version() >= 3) {
    style = (fl[4]>>1) & 0x1f;
    switch (fl[4] >> 6) {
    case 0:
      cell.setHAlignement(MWAWCell::HALIGN_LEFT);
      break;
    case 1:
      cell.setHAlignement(MWAWCell::HALIGN_CENTER);
      break;
    case 2:
      cell.setHAlignement(MWAWCell::HALIGN_RIGHT);
      break;
    default:
    case 3:
      break; // default
    }
    fl[4] &= 1;
    subformat = fl[2] >> 4;
    cell.setBorders(fl[2] & 0xF, MWAWBorder()); // checkme
    fl[2] = 0;
  }
  cell.m_noteId = fl[1] & 0xf;
  fl[1] >>= 4;
  int type = fl[3] >> 4;
  format.m_digits=(fl[3] & 0xf);
  fl[3] = 0;

  cell.m_font=m_state->m_spreadsheet.m_font;
  uint32_t fflags = 0;
  if (style) {
    if (style & 1) fflags |= MWAWFont::shadowBit;
    if (style & 2) fflags |= MWAWFont::embossBit;
    if (style & 4) fflags |= MWAWFont::italicBit;
    if (style & 8) cell.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (style & 16) fflags |= MWAWFont::boldBit;
  }
  cell.m_font.setFlags(fflags);

  content.m_contentType=MWAWCellContent::C_NONE;
  if (type & 2) {
    cell.setProtected(true);
    type &= 0xFD;
  }
  switch (type) {
  case 0:
    content.m_contentType=MWAWCellContent::C_TEXT;
    format.m_format=MWAWCell::F_TEXT;
    break;
  case 1:
    if (subformat < 4)
      format.m_format=MWAWCell::F_TIME;
    else
      format.m_format=MWAWCell::F_DATE;
    content.m_contentType=MWAWCellContent::C_TEXT;
    break;
  case 4:
    format.m_format=MWAWCell::F_NUMBER;
    content.m_contentType=MWAWCellContent::C_NUMBER;
    break;
  case 5:
    if (subformat < 4)
      format.m_format=MWAWCell::F_TIME;
    else
      format.m_format=MWAWCell::F_DATE;
    content.m_contentType=MWAWCellContent::C_NUMBER;
    break;
  case 12:
    f << "type" << type << ",";
  case 8: //number general
    format.m_format=MWAWCell::F_NUMBER;
    content.m_contentType=MWAWCellContent::C_FORMULA;
    break;
  case 13: // date or time
    f << "type" << type << ",";
  case 9: // date or time
    if (subformat < 4)
      format.m_format=MWAWCell::F_TIME;
    else
      format.m_format=MWAWCell::F_DATE;
    content.m_contentType=MWAWCellContent::C_FORMULA;
    break;
  default:
    MWAW_DEBUG_MSG(("MsWksSSParser::readCell: unknown type:%d for a cell\n", type));
    f << "typ[##unk"<<type << "],";
    content.m_contentType=MWAWCellContent::C_UNKNOWN;
    break;
  }
  switch (format.m_format) {
  case  MWAWCell::F_NUMBER:
    format.m_numberFormat=MWAWCell::F_NUMBER_GENERIC;
    switch (subformat) {
    case 0:
      break;
    case 1:
      format.m_numberFormat=MWAWCell::F_NUMBER_DECIMAL;
      break;
    case 2:
      format.m_numberFormat=MWAWCell::F_NUMBER_CURRENCY;
      break;
    case 3:
      format.m_thousandHasSeparator=true;
      break;
    case 4:
      format.m_numberFormat=MWAWCell::F_NUMBER_CURRENCY;
      format.m_thousandHasSeparator=true;
      break;
    case 5:
      format.m_numberFormat=MWAWCell::F_NUMBER_SCIENTIFIC;
      break;
    case 6:
      format.m_numberFormat=MWAWCell::F_NUMBER_PERCENT;
      break;
    case 7:
      format.m_numberFormat=MWAWCell::F_NUMBER_PERCENT;
      format.m_thousandHasSeparator=true;
      break;
    default:
      f << ",subform==##unkn" << subformat;
      break;
    }
    break;
  case MWAWCell::F_TIME:
    if (subformat >= 0 && subformat < 4) {
      static char const *(wh[])= {"%I:%M:%S %p", "%I:%M %p", "%H:%M:%S", "%H:%M"};
      format.m_DTFormat=wh[subformat];
    }
    else
      f << ",subform==##unkn" << subformat;
    break;
  case MWAWCell::F_DATE:
    switch (subformat) {
    case 4:
    case 5:
    case 6:
    case 7:
    case 8: {
      static char const *(wh[])= {"%m/%d/%y", "%b %d, %Y", "%b, %d", "%b, %Y", "%a, %d %b, %Y" };
      format.m_DTFormat=wh[subformat-4];
      break;
    }
    case 10:
    case 11:
      format.m_DTFormat="%B %d %Y";
      break;
    case 12:
    case 13:
      format.m_DTFormat="%A, %B %d, %Y";
      break;
    default:
      f << ",subform==##unkn" << subformat;
      break;
    }
    break;
  case MWAWCell::F_TEXT:
    break;
  case MWAWCell::F_BOOLEAN:
  case MWAWCell::F_UNKNOWN:
    MWAW_DEBUG_MSG(("MsWksSSParser::readCell: unexpected format\n"));
    break;
  default:
    f << ",subform==##unkn" << subformat;
    break;
  }

  long pos = debPos;
  input->seek(pos, librevenge::RVNG_SEEK_SET);

  if (pos != endPos) {
    bool ok = true;
    switch (content.m_contentType) {
    case MWAWCellContent::C_TEXT: {
      content.m_textEntry.setBegin(pos);
      content.m_textEntry.setEnd(endPos);
      if (content.m_textEntry.length()>=0) {
        std::string text("");
        for (long i=0; i <content.m_textEntry.length(); ++i)
          text += (char) input->readULong(1);
        f << text << ",";
      }
      else
        f << "###text,";
      break;
    }
    case MWAWCellContent::C_NUMBER: {
      double val;
      bool isNan;
      std::string str;
      if (!readNumber(endPos, val, isNan, str)) {
        ok=false;
        f << "###number";
      }
      else
        content.setValue(val);
      f << val;
      if (!str.empty())
        f << "[" << str << "],";
      break;
    }
    case MWAWCellContent::C_FORMULA: {
      std::string extra("");
      if (!readFormula(endPos, content, extra)) {
        ok=false;
        f << "###";
      }
      f << extra;
      break;
    }
    case MWAWCellContent::C_NONE:
      break;
    case MWAWCellContent::C_UNKNOWN:
    default:
      ok = false;
    }
    if (!ok)
      f << ",###pb";
  }
  cell.setFormat(format);
  // change the reference date from 1/1/1904 to 1/1/1900
  if (format.m_format==MWAWCell::F_DATE && content.isValueSet())
    content.setValue(content.m_value+1462.);

  std::string extra = f.str();
  f.str("");
  f << "SSheetE:" << cell << ",unk=[" << std::hex;
  for (int i = 0; i < 5; i++) {
    if (!fl[i]) f << "_,";
    else f << fl[i] << ",";
  }
  f << "]" << std::dec;
  if (extra.size()) f << ", extra=" << extra;


  pos = input->tell();
  if (pos != endPos) {
    ascFile.addDelimiter(pos,'[');
    ascFile.addDelimiter(endPos,']');
  }

  ascFile.addPos(debPos-1);
  ascFile.addNote(f.str().c_str());
  input->seek(endPos+5, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
bool MsWksSSParser::sendSpreadsheet()
{
  MWAWSpreadsheetListenerPtr listener=getSpreadsheetListener();
  MWAWInputStreamPtr &input= m_zone->getInput();
  if (!listener) {
    MWAW_DEBUG_MSG(("MsWksSSParser::sendSpreadsheet: I can not find the listener\n"));
    return false;
  }
  MsWksSSParserInternal::Spreadsheet &sheet = m_state->m_spreadsheet;
  size_t numCell = sheet.m_cells.size();

  int prevRow = -1;
  listener->openSheet(sheet.convertInPoint(sheet.m_widthCols,76), librevenge::RVNG_POINT, sheet.m_name);
  MsWksSSParserInternal::Zone zone=m_state->get(MsWksSSParserInternal::Zone::MAIN);
  m_graphParser->sendAll(zone.m_zoneId, true);

  for (size_t i = 0; i < numCell; i++) {
    MsWksSSParserInternal::Cell const &cell= sheet.m_cells[i];
    // FIXME: openSheetRow/openSheetCell must do that
    if (cell.position()[1] != prevRow) {
      while (cell.position()[1] > prevRow) {
        if (prevRow != -1)
          listener->closeSheetRow();
        prevRow++;
        listener->openSheetRow(16, librevenge::RVNG_POINT);
      }
    }
    listener->setFont(cell.m_font);
    listener->openSheetCell(cell, cell.m_content);
    if (cell.m_content.m_textEntry.valid()) {
      input->seek(cell.m_content.m_textEntry.begin(), librevenge::RVNG_SEEK_SET);
      while (!input->isEnd() && input->tell()<cell.m_content.m_textEntry.end()) {
        unsigned char c=(unsigned char) input->readULong(1);
        if (c==0xd)
          listener->insertEOL();
        else
          listener->insertCharacter(c);
      }
    }
    listener->closeSheetCell();
  }
  if (prevRow!=-1) listener->closeSheetRow();
  listener->closeSheet();
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read a generic zone
////////////////////////////////////////////////////////////
bool MsWksSSParser::readZone(MsWksSSParserInternal::Zone &zone)
{
  MWAWInputStreamPtr input = m_zone->getInput();
  if (input->isEnd()) return false;
  long pos = input->tell();
  MWAWEntry pict;
  int val = (int) input->readLong(1);
  input->seek(-1, librevenge::RVNG_SEEK_CUR);
  switch (val) {
  case 0: {
    if (m_graphParser->getEntryPicture(zone.m_zoneId, pict)>=0) {
      input->seek(pict.end(), librevenge::RVNG_SEEK_SET);
      return true;
    }
    break;
  }
  case 1: {
    if (m_graphParser->getEntryPictureV1(zone.m_zoneId, pict)>=0) {
      input->seek(pict.end(), librevenge::RVNG_SEEK_SET);
      return true;
    }
    break;
  }
  case 2:
    if (readDocumentInfo())
      return true;
    break;
  // case 3: can we also find a group header here ?
  default:
    break;
  }

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  return false;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool MsWksSSParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = MsWksSSParserInternal::State();
  MWAWInputStreamPtr input = m_zone->getInput();
  if (!input || !input->hasDataFork())
    return false;

  int numError = 0, val;

  const int headerSize = 0x20;

  libmwaw::DebugFile &ascFile=m_zone->ascii();
  libmwaw::DebugStream f;

  input->seek(0,librevenge::RVNG_SEEK_SET);

  m_state->m_hasHeader = m_state->m_hasFooter = false;
  int vers = (int) input->readULong(4);
  switch (vers) {
  case 11:
    setVersion(4);
    break;
  case 9:
    setVersion(3);
    break;
  case 8:
    setVersion(2);
    break;
  case 4:
    // fixme: never see a version 1: does it exists ?
    setVersion(1);
    break;
  default:
    if (strict) return false;

    MWAW_DEBUG_MSG(("MsWksSSParser::checkHeader: find unknown version 0x%x\n", vers));
    // must we stop in this case, or can we continue ?
    if (vers < 0 || vers > 14) {
      MWAW_DEBUG_MSG(("MsWksSSParser::checkHeader: version too big, we stop\n"));
      return false;
    }
    setVersion((vers < 4) ? 1 : (vers < 8) ? 2 : (vers < 11) ? 3 : 4);
  }
  if (input->seek(headerSize,librevenge::RVNG_SEEK_SET) != 0 || input->isEnd())
    return false;

  if (input->seek(12,librevenge::RVNG_SEEK_SET) != 0) return false;

  for (int i = 0; i < 3; i++) {
    val = (int)(int) input->readLong(1);
    if (val < -10 || val > 10) {
      MWAW_DEBUG_MSG(("MsWksSSParser::checkHeader: find odd val%d=0x%x: not implemented\n", i, val));
      numError++;
    }
  }
  input->seek(1,librevenge::RVNG_SEEK_CUR);
  int type = (int) input->readLong(2);
  switch (type) {
  // Text document
  case 1:
    break;
  case 2:
    m_state->m_docType = MWAWDocument::MWAW_K_DATABASE;
    break;
  case 3:
    m_state->m_docType = MWAWDocument::MWAW_K_SPREADSHEET;
    break;
  case 12:
    m_state->m_docType = MWAWDocument::MWAW_K_DRAW;
    break;
  default:
    MWAW_DEBUG_MSG(("MsWksSSParser::checkHeader: find odd type=%d: not implemented\n", type));
    return false;
  }

  if (m_state->m_docType != MWAWDocument::MWAW_K_SPREADSHEET)
    return false;
#ifndef DEBUG
  if (version() == 1)
    return false;
#endif

  // ok, we can finish initialization
  MWAWEntry headerZone;
  headerZone.setBegin(0);
  headerZone.setEnd(headerSize);
  headerZone.setType("FileHeader");
  m_listZones.push_back(headerZone);

  //
  input->seek(0,librevenge::RVNG_SEEK_SET);
  f << "FileHeader: ";
  f << "version= " << input->readULong(4);
  long dim[4];
  for (int i = 0; i < 4; i++) dim[i] = input->readLong(2);
  if (dim[2] <= dim[0] || dim[3] <= dim[1]) {
    MWAW_DEBUG_MSG(("MsWksSSParser::checkHeader: find odd bdbox\n"));
    numError++;
  }
  f << ", windowdbdbox?=(";
  for (int i = 0; i < 4; i++) f << dim[i]<<",";
  f << "),";
  for (int i = 0; i < 4; i++) {
    static int const expectedVal[]= {0,0,5,0x6c};
    val = (int) input->readULong(1);
    if (val==expectedVal[i]) continue;
    f << "##v" << i << "=" << std::hex << val <<",";
  }
  type = (int) input->readULong(2);
  f << std::dec;
  switch (type) {
  case 1:
    f << "doc,";
    break;
  case 2:
    f << "database,";
    break; // with ##v3=50
  case 3:
    f << "spreadsheet,";
    break; // with ##v2=5,##v3=6c
  case 12:
    f << "draw,";
    break;
  default:
    f << "###type=" << type << ",";
    break;
  }
  f << "numlines?=" << input->readLong(2) << ",";
  val = (int) input->readLong(1); // 0, v2: 0, 4 or -4
  if (val)  f << "f0=" << val << ",";
  val = (int) input->readLong(1); // almost always 1
  if (val != 1) f << "f1=" << val << ",";
  for (int i = 11; i < headerSize/2; i++) { // v1: 0, 0, v2: 0, 0|1
    val = (int) input->readULong(2);
    if (!val) continue;
    f << "f" << i << "=" << std::hex << val << std::dec;
    if (version() >= 3 && i == 12) {
      if (val & 0x100) {
        m_state->m_hasHeader = true;
        f << "(Head)";
      }
      if (val & 0x200) {
        m_state->m_hasFooter = true;
        f << "(Foot)";
      }
    }
    f << ",";
  }

  if (header)
    header->reset(MWAWDocument::MWAW_T_MICROSOFTWORKS, version(), m_state->m_docType);

  ascFile.addPos(0);
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(headerSize);

  input->seek(headerSize,librevenge::RVNG_SEEK_SET);
  return strict ? (numError==0) : (numError < 3);
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool MsWksSSParser::readDocumentInfo(long sz)
{
  MWAWInputStreamPtr input = m_zone->getInput();
  long pos = input->tell();
  libmwaw::DebugFile &ascFile=m_zone->ascii();
  libmwaw::DebugStream f;

  int vers = version();
  int docId = 0;
  int docExtra = 0;
  int flag = 0;
  int expectedSz = 0x80;
  if (sz<=0) {
    if (input->readLong(1) != 2)
      return false;
    docId = (int) input->readULong(1);
    docExtra = (int) input->readULong(1);
    flag = (int) input->readULong(1);
    sz = (long) input->readULong(2);
    expectedSz = vers<=2 ? 0x15e : 0x9a;
  }
  long endPos = input->tell()+sz;
  if (!input->checkPosition(endPos))
    return false;

  if (sz < expectedSz) {
    if (sz < 0x78+8) {
      MWAW_DEBUG_MSG(("MsWksSSParser::readDocumentInfo: size is too short\n"));
      return false;
    }
    MWAW_DEBUG_MSG(("MsWksSSParser::readDocumentInfo: size is too short: try to continue\n"));
  }

  f << "Entries(DocInfo):";
  if (docId) f << "id=0x"<< std::hex << docId << ",";
  if (docExtra) f << "unk=" << docExtra << ","; // in v3: find 3, 7, 1x
  if (flag) f << "fl=" << flag << ","; // in v3: find 80, 84, e0
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  if (!readPrintInfo()) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return true;
  }

  pos = input->tell();
  if (sz < 0x9a) {
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    return true;
  }
  pos = input->tell();
  f.str("");
  f << "DocInfo-1:";
  int val = (int) input->readLong(2);
  if ((val & 0x0400) && vers >= 3) {
    f << "titlepage,";
    val &= 0xFBFF;
  }
  if (val) f << "unkn=" << val << ",";
  if (vers <= 2) {
    for (int wh = 0; wh < 2; wh++) {
      long debPos = input->tell();
      std::string name(wh==0 ? "header" : "footer");
      std::string text = m_textParser->readHeaderFooterString(wh==0);
      if (text.size()) f << name << "="<< text << ",";

      long remain = debPos+100 - input->tell();
      for (long i = 0; i < remain; i++) {
        unsigned char c = (unsigned char) input->readULong(1);
        if (c == 0) continue;
        f << std::dec << "f"<< i << "=" << (int) c << ",";
      }
    }
    f << "defFid=" << input->readULong(2) << ",";
    f << "defFsz=" << input->readULong(2)/2 << ",";
    val = (int) input->readULong(2); // 0 or 8
    if (val) f << "#unkn=" << val << ",";
    int dim[2];
    for (int i = 0; i < 2; i++) dim[i] = (int) input->readULong(2);
    f << "dim=" << dim[0] << "x" << dim[1] << ",";
    /* followed by 0 (v1) or 0|0x21|0* (v2)*/
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    pos = input->tell();
    f.str("");
    f << "DocInfo-2:";
  }

  // last data ( normally 26)
  int numData = int((endPos - input->tell())/2);
  for (int i = 0; i < numData; i++) {
    val = (int) input->readLong(2);
    switch (i) {
    case 2:
      if (val!=1) f << "firstPageNumber=" << val << ",";
      break;
    case 3:
      if (val!=1) f << "firstNoteNumber=" << val << ",";
      break;
    default:
      if (val)
        f << "g" << i << "=" << val << ",";
      break;
    }
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  input->seek(endPos, librevenge::RVNG_SEEK_SET);

  return true;
}

////////////////////////////////////////////////////////////
// read a basic zone info
////////////////////////////////////////////////////////////
bool MsWksSSParser::readGroup(MsWksSSParserInternal::Zone &zone, MWAWEntry &entry, int check)
{
  entry = MWAWEntry();
  MWAWInputStreamPtr input=m_zone->getInput();
  if (input->isEnd()) return false;

  long pos = input->tell();
  int val=(int) input->readULong(1);
  // checkme: maybe we can also find 3 here, for a group of shape ?
  if (val != 0) return false;

  libmwaw::DebugFile &ascFile=m_zone->ascii();
  libmwaw::DebugStream f;
  int docId = (int) input->readULong(1);
  int docExtra = (int) input->readULong(1);
  int flag = (int) input->readULong(1);
  long size = (long) input->readULong(2)+6;

  int blockSize = version() <= 2 ? 340 : 360;
  if (size < blockSize) return false;

  f << "Entries(GroupHeader):";
  if (docId) f << "id=0x"<< std::hex << docId << std::dec << ",";
  if (docExtra) f << "unk=" << docExtra << ",";
  if (flag) f << "fl=" << flag << ",";
  if (size != blockSize)
    f << "end=" << std::hex << pos+size << std::dec << ",";

  entry.setBegin(pos);
  entry.setLength(size);
  entry.setType("GroupHeader");

  if (!input->checkPosition(entry.end())) {
    if (!input->checkPosition(pos+blockSize)) {
      MWAW_DEBUG_MSG(("MsWksSSParser::readGroup: can not determine group %d size \n", docId));
      return false;
    }
    entry.setLength(blockSize);
  }

  if (check <= 0) return true;
  input->seek(pos+8, librevenge::RVNG_SEEK_SET);
  for (int i = 0; i < 52; i++) {
    int v = (int) input->readLong(2);
    if (i < 8 && (v < -100 || v > 100)) return false;
    if (v) {
      f << "f" << i << "=";
      if (v > 0 && v < 1000)
        f << v;
      else
        f << std::hex << "X" << v << std::dec;
      f << ",";
    }
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  pos = pos+blockSize;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int N=(int) input->readLong(2);

  f.str("");
  f << "GroupHeader:N=" << N << ",";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  MWAWEntry pictZone;
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    if (m_graphParser->getEntryPicture(zone.m_zoneId, pictZone)>=0)
      continue;
    MWAW_DEBUG_MSG(("MsWksSSParser::readGroup: can not find the end of group \n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    break;
  }
  if (input->tell() < entry.end()) {
    ascFile.addPos(input->tell());
    ascFile.addNote("Entries(GroupData)");
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  }

  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool MsWksSSParser::readPrintInfo()
{
  MWAWInputStreamPtr input = m_zone->getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_zone->ascii();
  // print info
  libmwaw::PrinterInfo info;
  long endPos=pos+0x80;
  if (!input->checkPosition(endPos) || !info.read(input)) return false;
  f << "Entries(PrintInfo):"<< info;

  Vec2i paperSize = info.paper().size();
  Vec2i pageSize = info.page().size();
  if (pageSize.x() <= 0 || pageSize.y() <= 0 ||
      paperSize.x() <= 0 || paperSize.y() <= 0) return false;

  // now read the margin
  int margin[4];
  int maxSize = paperSize.x() > paperSize.y() ? paperSize.x() : paperSize.y();
  f << ", margin=(";
  for (int i = 0; i < 4; i++) {
    margin[i] = int(72.f/120.f*(float)input->readLong(2));
    if (margin[i] <= -maxSize || margin[i] >= maxSize) return false;
    f << margin[i];
    if (i != 3) f << ", ";
  }
  f << ")";

  // fixme: compute the real page length here...
  // define margin from print info
  Vec2i lTopMargin(margin[0],margin[1]), rBotMargin(margin[2],margin[3]);
  lTopMargin += paperSize - pageSize;

  int leftMargin = lTopMargin.x();
  int topMargin = lTopMargin.y();

  // decrease a little right and bottom margins Margin
  int rightMarg = rBotMargin.x()-50;
  if (rightMarg < 0) {
    leftMargin -= (-rightMarg);
    if (leftMargin < 0) leftMargin=0;
    rightMarg=0;
  }
  int botMarg = rBotMargin.y()-50;
  if (botMarg < 0) {
    topMargin -= (-botMarg);
    if (topMargin < 0) topMargin=0;
    botMarg=0;
  }

  getPageSpan().setMarginTop(topMargin/72.0);
  getPageSpan().setMarginBottom(botMarg/72.0);
  getPageSpan().setMarginLeft(leftMargin/72.0);
  getPageSpan().setMarginRight(rightMarg/72.0);
  getPageSpan().setFormLength(paperSize.y()/72.);
  getPageSpan().setFormWidth(paperSize.x()/72.);

  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// formula data
////////////////////////////////////////////////////////////
bool MsWksSSParser::readString(long endPos, std::string &res)
{
  MWAWInputStreamPtr input=m_zone->getInput();
  res = "";
  int error = 0;
  int ok = 0;
  while (input->tell() != endPos) {
    char c = (char) input->readLong(1);
    if (c < 27 && c != '\t' && c != '\n') error++;
    else ok++;
    res += c;
  }
  return ok >= error;
}

bool MsWksSSParser::readNumber(long endPos, double &res, bool &isNan, std::string &str)
{
  MWAWInputStreamPtr input=m_zone->getInput();
  res = 0;
  str="";
  long pos = input->tell();
  if (endPos > pos+10 && !readString(endPos-10,str)) return false;
  return input->tell() == endPos-10 && input->readDouble10(res,isNan);
}

bool MsWksSSParser::readCellInFormula(MWAWCellContent::FormulaInstruction &instr)
{
  MWAWInputStreamPtr input=m_zone->getInput();
  instr=MWAWCellContent::FormulaInstruction();
  instr.m_type=MWAWCellContent::FormulaInstruction::F_Cell;
  bool ok = true;
  bool absolute[2] = { false, false};
  int type = (int) input->readULong(1);
  if (type & 0x80) {
    absolute[0] = true;
    type &= 0x7F;
  }
  if (type & 0x40) {
    absolute[1] = true;
    type &= 0xBF;
  }
  if (type) {
    MWAW_DEBUG_MSG(("MSWksSSParser::readCellInFormula:Pb find fl=%d when reading a cell\n", type));
    ok = false;
  }

  int pos[2];
  for (int i=0; i<2; ++i)
    pos[i] = (int) input->readULong(1);

  if (pos[0] < 1 || pos[1] < 0) {
    if (ok) {
      MWAW_DEBUG_MSG(("MSWksSSParser::readCell: can not read cell position\n"));
    }
    return false;
  }
  instr.m_position[0]=Vec2i(pos[1],pos[0]-1);
  instr.m_positionRelative[0]=Vec2b(!absolute[1],!absolute[0]);
  return ok;
}

bool MsWksSSParser::readFormula(long endPos, MWAWCellContent &content, std::string &extra)
{
  MWAWInputStreamPtr input=m_zone->getInput();
  long pos = input->tell();
  extra="";
  if (pos == endPos) return false;

  std::stringstream f;
  std::vector<MWAWCellContent::FormulaInstruction> &formula=content.m_formula;
  bool ok=true;
  while (input->tell() !=endPos) {
    pos = input->tell();
    int code = (int) input->readLong(1);
    MWAWCellContent::FormulaInstruction instr;
    bool findEnd=false;
    switch (code) {
    case 0x0:
    case 0x2:
    case 0x4:
    case 0x6: {
      static char const *wh[]= {"+","-","*","/"};
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
      instr.m_content=wh[code/2];
      break;
    }
    case 0x8: { // a number
      int sz = (int) input->readULong(1);
      std::string s;
      double val;
      bool isNan;
      if (pos+sz+12 <= endPos && readNumber((pos+2)+sz+10, val, isNan, s)) {
        instr.m_type=MWAWCellContent::FormulaInstruction::F_Double;
        instr.m_doubleValue=val;
        break;
      }
      f << "###number" << s;
      ok=false;
      break;
    }
    case 0x0a: { // a cell
      if (!readCellInFormula(instr))
        f << "#";
      break;
    }
    case 0x0c: { // function
      int v = (int) input->readULong(1);
      static char const *(listFunc) [0x41] = {
        "Abs", "Sum", "Na", "Error", "ACos", "And", "ASin", "ATan",
        "ATan2", "Average", "Choose", "Cos", "Count", "Exp", "False", "FV",
        "HLookup", "If", "Index", "Int", "IRR", "IsBlank", "IsError", "IsNa",
        "##Funct[30]", "Ln", "Lookup", "Log10", "Max", "Min", "Mod", "Not",
        "NPer", "NPV", "Or", "Pi", "Pmt", "PV", "Rand", "Round",
        "Sign", "Sin", "Sqrt", "StDev", "Tan", "True", "Var", "VLookup",
        "Match", "MIRR", "Rate", "Type", "Radians", "Degrees", "Sum" /*"SSum: checkme"*/, "Date",
        "Day", "Hour", "Minute", "Month", "Now", "Second", "Time", "Weekday",
        "Year"
      };
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Function;
      if ((v%2) == 0 && v >= 0 && v/2 <= 0x40)
        instr.m_content=listFunc[v/2];
      else {
        f << "###";
        MWAW_DEBUG_MSG(("MSWksSSParser::readFormula: find unknown function %x\n", v));
        std::stringstream s;
        s << "Funct" << std::hex << v << std::dec;
        instr.m_content=s.str();
      }
      break;
    }
    case 0x0e: { // list of cell
      MWAWCellContent::FormulaInstruction instr2;
      if (endPos-pos< 9) {
        f << "###list cell short";
        ok = false;
        break;
      }
      if (!readCellInFormula(instr) || !readCellInFormula(instr2))
        f << "#";
      instr.m_type=MWAWCellContent::FormulaInstruction::F_CellList;
      instr.m_position[1]=instr2.m_position[0];
      instr.m_positionRelative[1]=instr2.m_positionRelative[0];
      break;
    }
    case 0x16:
      findEnd=true;
      input->seek(-1, librevenge::RVNG_SEEK_CUR);
      f << ",";
      break;
    default:
      if ((code%2)==0 && code>=0x10 && code<=0x22) {
        static char const *wh[]= {"(", ")", ";", "end", "<", ">", "=", "<=", ">=", "<>" };
        instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
        instr.m_content=wh[(code-0x10)/2];
        break;
      }
      f << "###" << std::hex << code << std::dec;
      ok = false;
      break;
    }
    if (!ok || findEnd)
      break;
    f << instr;
    formula.push_back(instr);
  }
  if (ok)
    content.m_formula=formula;
  extra=f.str();
  pos = input->tell();
  if (endPos - pos < 21)
    return ok;
  // test if we have the value
  if (input->readLong(1) != 0x16) {
    input->seek(-1, librevenge::RVNG_SEEK_CUR);
    return true;
  }

  f.str("");
  f << std::dec << "unk1=[";
  // looks a little as a zone of cell ?? but this seems eroneous
  for (int i = 0; i < 2; i++) {
    int v = (int) input->readULong(1);

    int n0 = (int) input->readULong(1);
    int n1 = (int) input->readULong(1);
    if (i == 1) f << ":";
    f << n0 << "x" << n1;
    if (v) f << "##v";
  }
  f << std::hex << "],unk2=["; // 0, followed by a small number between 1 and 140
  for (int i = 0; i < 2; i++)
    f << input->readULong(2) << ",";
  f << "]";

  // the value
  double value;
  bool isNan;
  std::string res;
  if (!readNumber(endPos, value, isNan, res)) {
    MWAW_DEBUG_MSG(("MsWksSSParser::readFormula: can not read val number\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    f << ",###";
  }
  else {
    content.setValue(value);
    f << ":" << value << ",";
  }
  extra += f.str();
  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
