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
#include "MWAWOLEParser.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWSpreadsheetListener.hxx"
#include "MWAWSubDocument.hxx"

#include "MsWksGraph.hxx"
#include "MsWks3Text.hxx"
#include "MsWksDocument.hxx"

#include "MsWksSSParser.hxx"

/** Internal: the structures of a MsWksSSParser */
namespace MsWksSSParserInternal
{
/** a cell of a MsWksSSParser */
class Cell : public MWAWCell
{
public:
  /// constructor
  Cell() : m_content(), m_noteId(0) { }
  //! returns true if the cell do contain any content
  bool isEmpty() const
  {
    return m_content.empty() && !hasBorders() && !m_noteId;
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Cell const &cell);

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
  std::map<int,MWAWEntry> m_idNoteMap;
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
  State() : m_spreadsheet(), m_actPage(0), m_numPages(0), m_pageLength(-1)
  {
  }

  /** the spreadsheet */
  Spreadsheet m_spreadsheet;
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  //! the page length in point (if known)
  int m_pageLength;
};

////////////////////////////////////////
//! Internal: the subdocument of a MsWksSSParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(MsWksSSParser &pars, MWAWInputStreamPtr input, int zoneId) :
    MWAWSubDocument(&pars, input, MWAWEntry()),m_id(zoneId) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const
  {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  /** the subdocument id*/
  int m_id;
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("MsWksSSParser::SubDocument::parse: no listener\n"));
    return;
  }
  assert(m_parser);

  long pos = m_input->tell();
  MsWksSSParser *parser = static_cast<MsWksSSParser *>(m_parser);
  parser->sendNote(m_id);
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_id != sDoc->m_id) return true;
  return false;
}
}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MsWksSSParser::MsWksSSParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWSpreadsheetParser(input, rsrcParser, header), m_state(), m_listZones(), m_document()
{
  MWAWInputStreamPtr mainInput=input;
  if (!input) return;
  if (input->isStructured()) {
    MWAWInputStreamPtr mainOle = input->getSubStreamByName("MN0");
    if (mainOle)
      mainInput=mainOle;
  }
  m_document.reset(new MsWksDocument(mainInput, *this));
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
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void MsWksSSParser::parse(librevenge::RVNGSpreadsheetInterface *docInterface)
{
  if (!m_document || !m_document->getInput() || !checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = true;
  try {
#ifdef DEBUG
    MWAWInputStreamPtr &input= getInput();
    if (input->isStructured()) {
      shared_ptr<MWAWOLEParser> oleParser(new MWAWOLEParser("MN0"));
      oleParser->parse(input);
    }
#endif
    // create the asciiFile
    m_document->initAsciiFile(asciiName());

    checkHeader(0L);
    createZones();
    ok=!m_state->m_spreadsheet.m_cells.empty();
    if (ok) {
      createDocument(docInterface);
      sendSpreadsheet();
    }
  }
  catch (...) {
    MWAW_DEBUG_MSG(("MsWksSSParser::parse: exception catched when parsing\n"));
    ok = false;
  }
  m_document->ascii().reset();

  resetSpreadsheetListener();
  if (!ok) throw(libmwaw::ParseException());
}

void MsWksSSParser::sendNote(int noteId)
{
  MWAWListenerPtr listener=getSpreadsheetListener();
  if (!listener||
      m_state->m_spreadsheet.m_idNoteMap.find(noteId)==m_state->m_spreadsheet.m_idNoteMap.end()) {
    MWAW_DEBUG_MSG(("MsWksSSParser::sendNote: can not send note %d\n", noteId));
    return;
  }
  MWAWEntry const &entry=m_state->m_spreadsheet.m_idNoteMap.find(noteId)->second;
  int const vers=version();
  if (!entry.valid()) return;
  MWAWInputStreamPtr input = m_document->getInput();
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  while (1) {
    long pos=input->tell();
    if (input->isEnd() || pos>=entry.end()) return;
    int c = (int) input->readULong(1);
    switch (c) {
    case 0x9:
      listener->insertTab();
      break;
    case 0x10: // cursor pos
    case 0x11:
      break;
    default:
      if (c >= 0x15 && c <= 0x19 && vers >= 3) {
        int sz = (c==0x19) ? 0 : (c == 0x18) ? 1 : 2;
        if (sz && pos+1+sz <=  entry.end())
          input->seek(sz, librevenge::RVNG_SEEK_CUR);
        switch (c) {
        case 0x19:
          listener->insertField(MWAWField(MWAWField::Title));
          break;
        case 0x18:
          listener->insertField(MWAWField(MWAWField::PageNumber));
          break;
        case 0x16:
          listener->insertField(MWAWField(MWAWField::Time));
          break;
        case 0x17: // id = 0 : short date ; id=9 : long date
          listener->insertField(MWAWField(MWAWField::Date));
          break;
        case 0x15:
          MWAW_DEBUG_MSG(("MsWksSSParser::sendNote: find unknown field type 0x15\n"));
          break;
        default:
          break;
        }
      }
      else if (c <= 0x1f) {
        MWAW_DEBUG_MSG(("MsWksSSParser::sendNote: find char=%x\n",int(c)));
      }
      else
        listener->insertCharacter((unsigned char)c, input, entry.end());
      break;
    }

  }
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

  std::vector<MWAWPageSpan> pageList;
  m_state->m_actPage = 0;
  m_document->getPageSpanList(pageList, m_state->m_numPages);
  MWAWSpreadsheetListenerPtr listen(new MWAWSpreadsheetListener(*getParserState(), pageList, documentInterface));
  setSpreadsheetListener(listen);
  listen->startDocument();
  // time to send page information the graph parser and the text parser
  m_document->getGraphParser()->setPageLeftTop
  (Vec2f(72.f*float(getPageSpan().getMarginLeft()),
         72.f*float(getPageSpan().getMarginTop())+m_document->getHeaderFooterHeight(true)));
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool MsWksSSParser::createZones()
{
  if (getInput()->isStructured())
    m_document->createOLEZones(getInput());
  MWAWInputStreamPtr input = m_document->getInput();
  const int vers=version();
  long pos;
  if (vers>2) {
    pos = input->tell();
    if (!m_document->readDocumentInfo(0x9a))
      m_document->getInput()->seek(pos, librevenge::RVNG_SEEK_SET);
    pos = input->tell();
    if (m_document->hasHeader() && !m_document->readGroupHeaderFooter(true,99))
      input->seek(pos, librevenge::RVNG_SEEK_SET);
    pos = input->tell();
    if (m_document->hasFooter() && !m_document->readGroupHeaderFooter(false,99))
      input->seek(pos, librevenge::RVNG_SEEK_SET);
  }
  readSSheetZone();

  std::multimap<int, MsWksDocument::Zone> &typeZoneMap=m_document->getTypeZoneMap();
  // fixme: getNewZoneId here
  int mainId=MsWksDocument::Z_MAIN;
  typeZoneMap.insert(std::multimap<int,MsWksDocument::Zone>::value_type
                     (MsWksDocument::Z_MAIN,MsWksDocument::Zone(MsWksDocument::Z_MAIN, mainId)));
  pos = input->tell();
  libmwaw::DebugFile &ascFile = m_document->ascii();
  if (input->isEnd() || input->readLong(2)!=0)
    input->seek(pos, librevenge::RVNG_SEEK_SET);
  else {
    MWAWEntry group;
    group.setId(mainId);
    group.setName("RBDR");
    if (!m_document->m_graphParser->readRB(input,group,1)) {
      MWAW_DEBUG_MSG(("MsWksSSParser::createZones: can not read RBDR group\n"));
      ascFile.addPos(pos+2);
      ascFile.addNote("Entries(RBDR):###");
      input->seek(pos, librevenge::RVNG_SEEK_SET);
    }
  }

  // normally, the file is now parsed, let check for potential remaining zones
  while (!input->isEnd()) {
    MWAW_DEBUG_MSG(("MsWksSSParser::createZones: find some unexpected data\n"));
    pos = input->tell();
    MsWksDocument::Zone unknown;
    if (!m_document->readZone(unknown)) {
      ascFile.addPos(pos);
      ascFile.addNote("Entries(End)");
      ascFile.addPos(pos+100);
      ascFile.addNote("_");
      break;
    }
  }

  // ok, prepare the data
  m_state->m_numPages = 1;
  std::vector<int> linesH, pagesH;
  m_document->getGraphParser()->computePositions(mainId, linesH, pagesH);
  return true;
}

////////////////////////////////////////////////////////////
// read a spreadsheet zone
////////////////////////////////////////////////////////////
bool MsWksSSParser::readSSheetZone()
{
  MsWksSSParserInternal::Spreadsheet &sheet=m_state->m_spreadsheet;
  sheet=MsWksSSParserInternal::Spreadsheet();
  int const vers=version();

  MWAWInputStreamPtr input=m_document->getInput();
  libmwaw::DebugFile &ascFile=m_document->ascii();
  long pos = input->tell(), sheetDebPos=pos;
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

  int zoneSize[3];
  zoneSize[0] = (int) input->readLong(2);
  if (zoneSize[0]) f << "zone[SheetF]=" << std::hex << zoneSize[0] << std::dec << ",";
  int numPageBreak = (int) input->readLong(2);
  if (numPageBreak) f << "nPgBreak=" << numPageBreak << ",";
  int numColBreak = (int) input->readLong(2);
  if (numColBreak) f << "nColBreak=" << numColBreak << ",";
  zoneSize[1]= (int) input->readLong(2);
  if (zoneSize[1]) f << "zone[DocInfo]=" << zoneSize[1] << ",";
  f << "unk1=" << std::hex << input->readULong(4) << std::dec << ","; // big number
  f << std::dec;

  int val;
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
  long endPos=pos+256;
  f << "SSheetC:";
  int numCharts=0;
  for (int i=0; i < 8; ++i) {
    // the chart names in v2, can we have more than 8 charts ?
    long cPos=input->tell();
    val = (int) input->readLong(2);
    int sSz=(int) input->readULong(1);
    if (!sSz || sSz>32-7) {
      input->seek(cPos, librevenge::RVNG_SEEK_SET);
      break;
    }
    ++numCharts;
    f << "Chart" << i << "=[";
    if (val) f << "unkn=" << val << ",";
    std::string name("");
    for (int c=0; c<sSz; ++c)
      name += (char) input->readULong(1);
    f << name << ",";
    if ((sSz%2)==0)   input->seek(1, librevenge::RVNG_SEEK_CUR);
    f << "id=" << std::hex << input->readULong(4) << std::dec << "],";
    input->seek(cPos+32, librevenge::RVNG_SEEK_SET);
  }
  int numRemains=int(endPos-input->tell())/2;
  for (int i = 0; i < numRemains; i++) {
    val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascFile.addNote(f.str().c_str());
  input->seek(endPos, librevenge::RVNG_SEEK_SET);

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
    if (i==3) {
      if (val==44) continue;
      f << "unkn=" << val << ",";
    }
    else if (val)
      f << "f" << i << "=" << val << ",";
  }
  f << "dim?=["; // in general 1,1,1,1 but can be X,X,1,1
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
  int numRemain=int(sheetDebPos+m_document->getLengthOfFileHeader3()-input->tell())/2;
  for (int i = 0; i < numRemain; i++) {
    val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascFile.addNote(f.str().c_str());
  input->seek(sheetDebPos+m_document->getLengthOfFileHeader3(), librevenge::RVNG_SEEK_SET);

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
  if (!input->checkPosition(pos+zoneSize[0])) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("MsWksSSParser::readSSheetZone:Can not read part F\n"));
    return false;
  }

  f.str("");
  f << "SSheetF[A]:";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  input->seek(pos+zoneSize[0]-10, librevenge::RVNG_SEEK_SET);

  pos = input->tell();
  f.str("");
  f << "SSheetF[B]:" << std::dec;
  for (int i = 0; i < 4; i++) {
    val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  val = (int) input->readULong(2);
  if (val) f << std::hex << "fl=" << val << "," << std::dec;
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  if (numPageBreak || numColBreak) {
    pos=input->tell();
    f.str("");
    f << "SSheetG:";
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
    // columns/notes break?
    if (numColBreak) {
      f << "colBreak=[";
      for (int i = 0; i < numColBreak; i++)
        f << input->readULong(2) << ",";
      f << "],";
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  if (zoneSize[1]>0) {
    // only version<=2 seems to contains the print data with size 0x15e
    pos = input->tell();
    MWAWEntry zone;
    if (!m_document->readDocumentInfo(zoneSize[1])) {
      input->seek(pos+zoneSize[1], librevenge::RVNG_SEEK_SET);
      MWAW_DEBUG_MSG(("MsWksSSParser::readSSheetZone:can not read print info\n"));
      ascii().addPos(pos);
      ascii().addNote("SSheet[printInfo]:###");
    }
  }

  if (vers==2) {
    for (int i=0; i < numCharts; ++i) {
      pos=input->tell();
      if (!input->checkPosition(pos+256)) {
        MWAW_DEBUG_MSG(("MsWksSSParser::readSSheetZone:can not find chart %d data\n", i));
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        return false;
      }
      f.str("");
      f << "Entries(Chart)[" << i << "]:";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      input->seek(pos+256, librevenge::RVNG_SEEK_SET);
    }
  }

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
    MWAWEntry note;
    note.setBegin(input->tell());
    note.setLength(pSz);
    std::string str="";
    for (int s = 0; s < pSz; s++)
      str += (char) input->readLong(1);
    if (input->tell() != pos+4+pSz) {
      input->seek(pos, librevenge::RVNG_SEEK_CUR);
      MWAW_DEBUG_MSG(("MsWksSSParser::readSSheetZone:Pb when reading a note\n"));
      return false;
    }
    if (pSz) {
      sheet.m_idNoteMap[idNote] = note;
      f << "Note" << idNote << "=\"" << str << "\",";
    }
  }

  ascFile.addPos(HDebPos);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool MsWksSSParser::readCell(int sz, Vec2i const &cellPos, MsWksSSParserInternal::Cell &cell)
{
  int const vers=version();
  cell = MsWksSSParserInternal::Cell();
  cell.setPosition(cellPos);
  if (sz == 0xFF || sz < 5) return false;

  MWAWInputStreamPtr input=m_document->getInput();
  long debPos = input->tell();

  libmwaw::DebugFile &ascFile=m_document->ascii();
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
  if (vers >= 3) {
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

  MWAWFont font=m_state->m_spreadsheet.m_font;
  MWAWColor col;
  if (vers>=3&&m_document->getColor(fl[0],col,3)) {
    font.setColor(col);
    fl[0]=0;
  }
  uint32_t fflags = 0;
  if (style) {
    if (style & 1) fflags |= MWAWFont::shadowBit;
    if (style & 2) fflags |= MWAWFont::embossBit;
    if (style & 4) fflags |= MWAWFont::italicBit;
    if (style & 8) font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (style & 16) fflags |= MWAWFont::boldBit;
  }
  font.setFlags(fflags);
  cell.setFont(font);

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
      if (!m_document->readDBNumber(endPos, val, isNan, str)) {
        ok=false;
        f << "###number";
      }
      else
        content.setValue(val);
      f << val;
      if (!str.empty()) f << "[" << str << "],";
      break;
    }
    case MWAWCellContent::C_FORMULA: {
      std::string extra("");
      if (!m_document->readFormula(endPos, content, extra)) {
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
  MWAWInputStreamPtr &input= m_document->getInput();
  if (!listener) {
    MWAW_DEBUG_MSG(("MsWksSSParser::sendSpreadsheet: I can not find the listener\n"));
    return false;
  }
  MsWksSSParserInternal::Spreadsheet &sheet = m_state->m_spreadsheet;
  size_t numCell = sheet.m_cells.size();

  int prevRow = -1;
  listener->openSheet(sheet.convertInPoint(sheet.m_widthCols,76), librevenge::RVNG_POINT, sheet.m_name);
  // fixme: this sends the pictures present in the main page, but not the picture present in header/footer
  MsWksDocument::Zone zone=m_document->getZone(MsWksDocument::Z_MAIN);
  m_document->getGraphParser()->sendAll(zone.m_zoneId, true);

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
    listener->openSheetCell(cell, cell.m_content);
    if (cell.m_content.m_textEntry.valid()) {
      listener->setFont(cell.isFontSet() ? cell.getFont() : sheet.m_font);
      input->seek(cell.m_content.m_textEntry.begin(), librevenge::RVNG_SEEK_SET);
      while (!input->isEnd() && input->tell()<cell.m_content.m_textEntry.end()) {
        unsigned char c=(unsigned char) input->readULong(1);
        if (c==0xd)
          listener->insertEOL();
        else
          listener->insertCharacter(c);
      }
    }
    if (cell.m_noteId>0) {
      MWAWSubDocumentPtr subDoc(new MsWksSSParserInternal::SubDocument(*this, input, cell.m_noteId));
      listener->insertComment(subDoc);
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
// read the header
////////////////////////////////////////////////////////////
bool MsWksSSParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = MsWksSSParserInternal::State();
  if (!m_document || !m_document->checkHeader3(header, strict)) return false;
  if (m_document->getKind() != MWAWDocument::MWAW_K_SPREADSHEET)
    return false;
#ifndef DEBUG
  if (version() == 1)
    return false;
#endif
  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
