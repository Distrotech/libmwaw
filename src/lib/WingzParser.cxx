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

#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWCell.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWSpreadsheetListener.hxx"
#include "MWAWSubDocument.hxx"

#include "WingzParser.hxx"

/** Internal: the structures of a WingzParser */
namespace WingzParserInternal
{

////////////////////////////////////////
//! Internal: the cell style of a WingzParser
struct Style {
  //! constructor
  Style() : m_font(), m_backgroundColor(MWAWColor::white()), m_lineColor(MWAWColor::black()), m_format()
  {
  }
  //! the font
  MWAWFont m_font;
  //! the cell background color
  MWAWColor m_backgroundColor;
  //! the line color (if needed)
  MWAWColor m_lineColor;
  //! the cell custom format (if known)
  std::string m_format;
};

//! Internal: the cell of a WingzParser
struct Cell : public MWAWCell {
  //! constructor
  Cell(Vec2i pos=Vec2i(0,0)) : MWAWCell(), m_content(), m_formula(-1)
  {
    setPosition(pos);
  }
  //! the cell content
  MWAWCellContent m_content;
  //! the formula id
  int m_formula;
};

////////////////////////////////////////
//! Internal: the spreadsheet data of a WingzParser
struct Spreadsheet {
  //! constructor
  Spreadsheet() : m_widthDefault(74), m_widthCols(), m_heightDefault(12), m_heightRows(),
    m_cells(), m_cellIdPosMap(), m_formulaMap(), m_styleMap(), m_name("Sheet0")
  {
  }
  //! returns the row size in point
  float getRowHeight(int row) const
  {
    if (row>=0&&row<(int) m_heightRows.size())
      return m_heightRows[size_t(row)];
    return m_heightDefault;
  }
  //! convert the m_widthCols in a vector of of point size
  std::vector<float> convertInPoint(std::vector<float> const &list) const
  {
    size_t numCols=size_t(getRightBottomPosition()[0]+1);
    std::vector<float> res;
    res.resize(numCols);
    for (size_t i = 0; i < numCols; i++) {
      if (i>=list.size() || list[i] < 0) res[i] = m_widthDefault;
      else res[i] = float(list[i]);
    }
    return res;
  }
  //! update the cell, ie. look if there is a avalaible formula, ...
  void update(Cell &cell) const;
  /** the default column width */
  float m_widthDefault;
  /** the column size in points */
  std::vector<float> m_widthCols;
  /** the default row height */
  float m_heightDefault;
  /** the row height in points */
  std::vector<float> m_heightRows;
  /** the list of not empty cells */
  std::vector<Cell> m_cells;
  //! the map cellId to cellPos
  std::map<int, MWAWCellContent::FormulaInstruction> m_cellIdPosMap;
  //! the list of formula
  std::map<int, std::vector<MWAWCellContent::FormulaInstruction> > m_formulaMap;
  //! the list of style
  std::map<int, Style> m_styleMap;
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

void Spreadsheet::update(Cell &cell) const
{
  if (cell.m_formula < 0 || m_formulaMap.find(cell.m_formula)==m_formulaMap.end())
    return;
  // first, we need to update the relative position
  std::vector<MWAWCellContent::FormulaInstruction> formula=
    m_formulaMap.find(cell.m_formula)->second;
  Vec2i cPos=cell.position();
  for (size_t i=0; i< formula.size(); ++i) {
    MWAWCellContent::FormulaInstruction &instr=formula[i];
    int numToCheck=0;
    if (instr.m_type==MWAWCellContent::FormulaInstruction::F_Cell)
      numToCheck=1;
    else if (instr.m_type==MWAWCellContent::FormulaInstruction::F_CellList)
      numToCheck=2;
    for (int j=0; j<numToCheck; ++j) {
      for (int c=0; c<2; ++c) {
        if (instr.m_positionRelative[j][c])
          instr.m_position[j][c]+=cPos[c];
        if (instr.m_positionRelative[j][c]<0) {
          MWAW_DEBUG_MSG(("WingzParserInternal::Spreadsheet::update: find some bad cell position\n"));
          return;
        }
      }
    }
  }
  cell.m_content.m_contentType=MWAWCellContent::C_FORMULA;
  cell.m_content.m_formula=formula;
}

////////////////////////////////////////
//! Internal: the state of a WingzParser
struct State {
  //! constructor
  State() : m_encrypted(false), m_spreadsheet(), m_numPages(0), m_headerHeight(0), m_footerHeight(0)
  {
  }

  //! returns the pattern percent corresponding to an id and a version
  static bool getPatternPercent(int patId, int vers, float &percent);

  //! a flag to know if the data is encrypted
  bool m_encrypted;
  //! the spreadsheet
  Spreadsheet m_spreadsheet;
  int m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

bool State::getPatternPercent(int patId, int vers, float &perc)
{
  if (vers==2) {
    if (patId<0 || patId>38) {
      MWAW_DEBUG_MSG(("WingzParserInternal::State::getPatternPercent: can not find patId=%d\n", patId));
      return false;
    }
    static float const percent[]= { 0/*none*/, 1, 0.9f, 0.7f, 0.5f, 0.7f, 0.5f, 0.7f, 0.2f, 0.3f, 0.1f, 0.3f, 0.3f,
                                    0.04f, 0.1f, 0.2f, 0.5f, 0.2f, 0.2f, 0.4f, 0, 0.1f, 0.2f, 0.3f, 0.3f, 0.5f,
                                    0.3f, 0.3f, 0.2f, 0.2f, 0.2f, 0.3f, 0.3f, 0.2f, 0.3f, 0.4f, 0.4f, 0.5f, 0.4f
                                  };
    perc=percent[patId];
    return true;
  }

  if (patId<0 || patId>=64) {
    MWAW_DEBUG_MSG(("WingzParserInternal::State::getPatternPercent: can not find patId=%d\n", patId));
    return false;
  }
  static float const percent[]= {
    0.0f, 1.0f, 0.968750f, 0.93750f, 0.875f, 0.750f, 0.5f, 0.250f,
    0.250f, 0.18750f, 0.1875f, 0.1250f, 0.0625f, 0.0625f, 0.031250f, 0.0f,
    0.75f, 0.5f, 0.25f, 0.3750f, 0.25f, 0.1250f, 0.25f, 0.1250f,
    0.75f, 0.5f, 0.25f, 0.3750f, 0.25f, 0.1250f, 0.25f, 0.1250f,
    0.75f, 0.5f, 0.5f, 0.5f, 0.5f, 0.25f, 0.25f, 0.234375f,
    0.6250f, 0.3750f, 0.1250f, 0.25f, 0.218750f, 0.218750f, 0.1250f, 0.093750f,
    0.5f, 0.5625f, 0.4375f, 0.3750f, 0.218750f, 0.281250f, 0.1875f, 0.093750f,
    0.593750f, 0.5625f, 0.515625f, 0.343750f, 0.3125f, 0.25f, 0.25f, 0.234375f
  };
  perc=percent[patId];
  return true;
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
WingzParser::WingzParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWSpreadsheetParser(input, rsrcParser, header), m_state()
{
  init();
}

WingzParser::~WingzParser()
{
}

void WingzParser::init()
{
  resetSpreadsheetListener();
  setAsciiName("main-1");

  m_state.reset(new WingzParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);
}


////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void WingzParser::parse(librevenge::RVNGSpreadsheetInterface *docInterface)
{
  assert(getInput().get() != 0);

  if (!checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = true;
  try {
    if (m_state->m_encrypted)
      ok = decodeEncrypted();
    if (ok) {
      // create the asciiFile
      ascii().setStream(getInput());
      ascii().open(asciiName());
      checkHeader(0L);
      ok = createZones();
    }
    if (ok) {
      createDocument(docInterface);
      sendSpreadsheet();
    }
  }
  catch (...) {
    MWAW_DEBUG_MSG(("WingzParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  ascii().reset();
  resetSpreadsheetListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void WingzParser::createDocument(librevenge::RVNGSpreadsheetInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getSpreadsheetListener()) {
    MWAW_DEBUG_MSG(("WingzParser::createDocument: listener already exist\n"));
    return;
  }

  // create the page list
  m_state->m_numPages = 1;
  MWAWPageSpan ps(getPageSpan());
  ps.setPageSpan(1);
  std::vector<MWAWPageSpan> pageList(1,ps);
  //
  MWAWSpreadsheetListenerPtr listen(new MWAWSpreadsheetListener(*getParserState(), pageList, documentInterface));
  setSpreadsheetListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool WingzParser::createZones()
{
  MWAWInputStreamPtr input = getInput();
  input->setReadInverted(true);
  input->seek(13, librevenge::RVNG_SEEK_SET);
  if (!readPreferences()) {
    ascii().addPos(input->tell());
    ascii().addNote("Entries(Loose)");
    if (!findNextZone())
      return false;
  }
  if (!readSpreadsheet()) return false;
  if (!input->isEnd()) {
    ascii().addPos(input->tell());
    ascii().addNote("Entries(Loose)");
  }
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read the preferences zone
////////////////////////////////////////////////////////////

bool WingzParser::readPreferences()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  int const vers=version();
  if (!input->checkPosition(pos+172+2*vers)) {
    MWAW_DEBUG_MSG(("WingzParser::readPreferences: the zone seems to short\n"));
    return false;
  }

  libmwaw::DebugStream f;
  f << "Entries(Preferences):";
  int type=(int) input->readULong(1);
  int val=(int) input->readULong(1);
  int dSz=(int) input->readULong(2);
  int id=(int) input->readULong(2);
  long endPos=pos+4+dSz;
  if (type!=0 || !input->checkPosition(endPos)) return false;
  if (val!=0x80) f << "f0=" << val << ",";
  if (id) f << "id=" << id << ",";
  ascii().addDelimiter(input->tell(), '|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(pos+18+2*vers, librevenge::RVNG_SEEK_SET);

  for (int i=0; i < 4; ++i) {
    pos=input->tell();
    f.str("");
    f << "Preferences-" << i << ":";
    int const(sz[]) = { 42, 42, 30, 51};
    input->seek(pos+sz[i], librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  // now a font (or maybe a font list)
  pos=input->tell();
  f.str("");
  f << "Preferences-Fonts:";
  val=(int) input->readULong(1);
  if (val) f << "f0=" << val << ",";
  long sz=(long) input->readULong(1);
  long fontEndPos=pos+sz;
  if (!input->checkPosition(pos+sz)) {
    MWAW_DEBUG_MSG(("WingzParser::readPreferences: the fonts zone seems to short\n"));
    return false;
  }
  int N=(int) input->readULong(1);
  f << "N=" << N << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (int i=0; i < N; ++i) {
    f.str("");
    f << "Preferences-Font" << i << ":";
    pos=input->tell();
    int fSz=(int) input->readULong(1);
    if (pos+1+fSz>fontEndPos) {
      MWAW_DEBUG_MSG(("WingzParser::readPreferences: the %d font size seems bad\n", i));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      input->seek(fontEndPos, librevenge::RVNG_SEEK_SET);
      return true;
    }
    std::string name("");
    for (int c=0; c<fSz; ++c) name+=(char) input->readULong(1);
    f << name;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  if (input->tell()!=fontEndPos) {
    ascii().addPos(input->tell());
    ascii().addNote("Preferences-Fontsend");
    MWAW_DEBUG_MSG(("WingzParser::readPreferences: find extra data\n"));
    input->seek(fontEndPos, librevenge::RVNG_SEEK_SET);
  }

  // last unknown
  pos=input->tell();
  if (!input->checkPosition(pos+237)) {
    MWAW_DEBUG_MSG(("WingzParser::readPreferences: the last zone seems to short\n"));
    return false;
  }

  f.str("");
  f << "Preferences-B0:";
  val=(int) input->readLong(2);
  if (val) f << "f0=" << val << ",";
  val=(int) input->readLong(2);
  if (val) f << "graph[num]=" << val << ",";
  for (int i=0; i < 10; ++i) { // f7,f8 related to page size?
    val=(int) input->readLong(2);
    if (val)
      f << "f" << i+1 << "=" << val << ",";
  }
  int sSz=(int) input->readULong(1);
  if (!input->checkPosition(pos+25+sSz)) {
    MWAW_DEBUG_MSG(("WingzParser::readPreferences: auto save name seems bad\n"));
    f << "####";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  std::string name("");
  for (int i=0; i < sSz; ++i)
    name += (char) input->readULong(1);
  f << name << ",";
  for (int i=0; i < 8; ++i) {
    val=(int) input->readLong(2);
    if (val)
      f << "g" << i << "=" << val << ",";
  }
  int dim[4];
  for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
  f << "select?=" << dim[0] << "x" << dim[1] << "<->" << dim[2] << "x" << dim[3] << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  ascii().addPos(pos);
  ascii().addNote("Preferences-B1");
  input->seek(pos+96, librevenge::RVNG_SEEK_SET);

  pos=input->tell();
  ascii().addPos(pos);
  ascii().addNote("Preferences-B2");
  input->seek(pos+58, librevenge::RVNG_SEEK_SET);

  pos=input->tell();
  f.str("");
  f << "Preferences[passwd]";
  for (int i=0; i<2; ++i) {
    input->seek(pos+i*17, librevenge::RVNG_SEEK_SET);
    int len=(int) input->readULong(1);
    if (!len) continue;
    if (len>16) {
      MWAW_DEBUG_MSG(("WingzParser::readPreferences: passwd size seems bad\n"));
      f << "###len" << i << "=" << len << ",";
      break;
    }
    std::string passwd("");
    for (int c=0; c<len; ++c) passwd += (char) input->readULong(1);
    f << passwd << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  if (vers==1)
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
  else
    input->seek(pos+34, librevenge::RVNG_SEEK_SET);

  return true;
}


////////////////////////////////////////////////////////////
// spreadsheet
////////////////////////////////////////////////////////////
bool WingzParser::readSpreadsheet()
{
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  int const vers=version();
  int const headerSize=vers==1?4:6;
  while (!input->isEnd()) {
    long pos=input->tell();
    int type=(int) input->readULong(1);
    int val=(int) input->readULong(1);
    int dSz=(int) input->readULong(2);
    if (type!=0xFF && input->isEnd()) {
      MWAW_DEBUG_MSG(("WingzParser::readSpreadsheet: can not read some zone\n"));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    std::string name("");
    if (type<=0x10) {
      static char const *(wh[])= {
        "", "SheetSize", "SheetSize", "", "", "", "", "CellName",
        "Formula", "Style", "SheetErr", "Sheet2Err", "", "SheetMcro", "Graphic", "",
        "PrintInfo"
      };
      name=wh[type];
    }
    if (name.empty()) {
      std::stringstream s;
      s << "ZSheet" << type;
      name = s.str();
    }
    f.str("");
    f << "Entries(" << name << "):";
    if (val!=0x80) f << "fl=" << std::hex << val << std::dec << ",";

    bool ok=true;
    switch (type) {
    case 1: // col size
    case 2: // row size
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      ok=readSpreadsheetSize();
      break;
    case 18:
    case 19:
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      ok=readSpreadsheetPBreak();
      break;
    case 3: // a list of int?
      ok=input->checkPosition(pos+headerSize+dSz);
      if (!ok) break;
      if (vers>1) {
        val=(int) input->readLong(2);
        if (val) f << "id=" << val << ",";
      }
      if ((dSz%2)) {
        MWAW_DEBUG_MSG(("WingzParser::readSpreadsheet: find some data in zone3\n"));
        f << "###";
        ascii().addDelimiter(pos+headerSize,'|');
      }
      else if (dSz) {
        f << "val=[";
        for (int i=0; i<dSz/2; ++i) f << input->readLong(2) << ",";
        f << "],";
      }
      input->seek(pos+headerSize+dSz, librevenge::RVNG_SEEK_SET);
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      break;
    case 4: // never find this block with data ...
      ok=input->checkPosition(pos+headerSize+dSz);
      if (!ok) break;
      if (vers>1) {
        val=(int) input->readLong(2);
        if (val) f << "id=" << val << ",";
      }
      if (dSz) {
        MWAW_DEBUG_MSG(("WingzParser::readSpreadsheet: find some data in zone4\n"));
        f << "###";
        ascii().addDelimiter(pos+headerSize,'|');
      }
      input->seek(pos+headerSize+dSz, librevenge::RVNG_SEEK_SET);
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      break;
    case 5:
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      ok=readSpreadsheetZone5();
      break;
    case 6: { // only in v1?
      ok=input->checkPosition(pos+headerSize+(vers==1 ? 2 : 0) +dSz);
      if (!ok) break;
      val=(int) input->readLong(2);
      if (val) f << "id=" << val << ",";
      int fSz=(int) input->readULong(1);
      if (dSz<1 || dSz!=1+fSz) {
        MWAW_DEBUG_MSG(("WingzParser::readSpreadsheet: zone 6 size seems bad\n"));
        f << "###";
      }
      else {
        std::string text("");
        for (int i=0; i<fSz; ++i) text += (char) input->readULong(1);
        f << text << ",";
      }
      input->seek(pos+headerSize+(vers==1 ? 2 : 0)+dSz, librevenge::RVNG_SEEK_SET);
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      break;
    }
    case 7:
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      ok=readSpreadsheetCellName();
      break;
    case 8:
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      ok=readFormula();
      break;
    case 9:
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      ok=readSpreadsheetStyle();
      break;
    case 0xa: { // seems related to error in formula 0x1a
      ok=input->checkPosition(pos+6+dSz);
      if (!ok) break;
      val=(int) input->readLong(2);
      if (val) f << "id=" << val << ",";
      int fSz=(int) input->readULong(1);
      if (dSz<1 || dSz!=1+fSz) {
        MWAW_DEBUG_MSG(("WingzParser::readSpreadsheet: SheetErr size seems bad\n"));
        f << "###";
      }
      else {
        std::string text("");
        for (int i=0; i<fSz; ++i) text += (char) input->readULong(1);
        f << text << ",";
      }
      input->seek(pos+6+dSz, librevenge::RVNG_SEEK_SET);
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      break;
    }
    case 0xb: { // seems related to error in formula 0x1a
      ok=input->checkPosition(pos+6+dSz);
      if (!ok) break;
      val=(int) input->readLong(2);
      if (val) f << "id=" << val << ",";
      if ((vers==1&&dSz<2) || (vers==2&&dSz<4)) {
        MWAW_DEBUG_MSG(("WingzParser::readSpreadsheet: Sheet2Err size seems bad\n"));
        f << "###";
      }
      else {
        f << "pos=" << input->readULong(1) << "x" << input->readULong(1) << ",";
        val = (int) input->readLong(1);
        if (val) f << "#g0=" << val << ",";
        std::string text("");
        for (int i=0; i<dSz-3; ++i) text += (char) input->readULong(1);
        f << text << ",";
      }
      input->seek(pos+6+dSz, librevenge::RVNG_SEEK_SET);
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      break;
    }
    case 0xc:
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      ok=readSpreadsheetCellList();
      break;
    case 0xd:
      val=(int) input->readLong(2);
      if (val) f << "id=" << val << ",";
      ok=readMacro();
      if (!ok) break;
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      break;
    case 0xe:
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      ok=readGraphic();
      if (!ok) {
        ascii().addPos(pos);
        ascii().addNote("Entries(Graphic):###");
        ok=findNextZone(0xe) && input->tell()>pos+46;
      }
      break;
    case 0x10:
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      ok=readPrintInfo();
      break;
    case 0xff: // end
      if (val==0xf && dSz==0) {
        ascii().addPos(pos);
        ascii().addNote("_");
        return true;
      }
      ok=false;
      break;
    default:
      ok=false;
    }
    if (ok) continue;
    input->seek(pos+4, librevenge::RVNG_SEEK_SET);
    if (vers==1) {
      if (type==6 || type==7 || type==0x11) dSz+=2;
    }
    else {
      if (type==0xc) dSz+=4;
      else if (type==0xe) dSz+=2;
      else if (type==0x10) dSz+=14;
    }
    if (!type || (vers==2 && !val) || (type>24 && !(vers==1&&type>100 && type<104)) ||
        (val&0x3F) || !input->checkPosition(pos+headerSize+dSz)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    int id=(int) input->readLong(2);
    if (id) f << "id=" << id << ",";
    if (input->tell() != pos+headerSize+dSz)
      ascii().addDelimiter(input->tell(), '|');
    input->seek(pos+headerSize+dSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool WingzParser::readSpreadsheetCellList()
{
  MWAWInputStreamPtr input = getInput();
  int const vers=version();
  long pos=input->tell();
  int type=(int) input->readULong(1);
  if (type!=12) return false;
  int val=(int) input->readULong(1);
  int dSz= (int) input->readULong(2);
  int row=(int) input->readLong(2);
  int firstCol=(int) input->readLong(2);
  long endPos=pos+(vers==1 ? 6 : 10)+dSz;
  libmwaw::DebugStream f;
  f << "Entries(SheetCell)[row=" << row << "]:";
  if (firstCol) f << "first[col]=" << firstCol << ",";
  if (val!=0x40) f << "fl=" << std::hex << val << std::dec << ",";
  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("WingzParser::readSpreadsheetCellList: find bad size for data\n"));
    return false;
  }
  val=(int) input->readLong(2);
  if (val) f << "f0=" << val << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  while (!input->isEnd()) {
    Vec2i cellPos(firstCol++, row);
    pos=input->tell();
    if (pos>=endPos) break;
    type=(int) input->readULong(1);
    f.str("");
    f << "SheetCell[" << cellPos << "]:type=" << (type&0xf);
    if (type&0xf0) {
      f << "[high=" << (type>>4) << "]";
      type &=0xf;
    }
    f << ",";
    if (type==0) { // empty cell
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      continue;
    }
    if (pos+(vers==1 ? 4 : 6)>endPos) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    WingzParserInternal::Cell cell(cellPos);
    val=(int) input->readULong(1);
    if (val&0xF) {
      int borders=0;
      f << "bord=";
      if (val&1) {
        borders|=libmwaw::LeftBit;
        f << "L";
      }
      if (val&2) {
        borders|=libmwaw::RightBit;
        f << "R";
      }
      if (val&4) {
        borders|=libmwaw::TopBit;
        f << "T";
      }
      if (val&8) {
        borders|=libmwaw::BottomBit;
        f << "B";
      }
      f << ",";
      cell.setBorders(borders, MWAWBorder());
    }
    if (val&0xF0) f << "f0=" << (val>>4) << ",";

    MWAWCell::Format format;
    val=(int) input->readULong(1);
    format.m_digits= val&0xf;
    if (format.m_digits!=2) f << "digits=" << format.m_digits << ",";
    switch (val>>4) {
    case 0: // general
      break;
    case 1:
      format.m_format=MWAWCell::F_NUMBER;
      format.m_numberFormat=MWAWCell::F_NUMBER_DECIMAL;
      break;
    case 2:
      format.m_format=MWAWCell::F_NUMBER;
      format.m_numberFormat=MWAWCell::F_NUMBER_CURRENCY;
      break;
    case 3:
      format.m_format=MWAWCell::F_NUMBER;
      format.m_numberFormat=MWAWCell::F_NUMBER_PERCENT;
      break;
    case 4:
      format.m_format=MWAWCell::F_NUMBER;
      format.m_numberFormat=MWAWCell::F_NUMBER_SCIENTIFIC;
      break;
    case 5:
      format.m_format=MWAWCell::F_DATE;
      format.m_DTFormat="%b %d %y:";
      break;
    case 6:
      format.m_format=MWAWCell::F_DATE;
      format.m_DTFormat="%b %d";
      break;
    case 7:
      format.m_format=MWAWCell::F_DATE;
      format.m_DTFormat="%b %y";
      break;
    case 8:
      format.m_format=MWAWCell::F_DATE;
      format.m_DTFormat="%m/%d/%y";
      break;
    case 9:
      format.m_format=MWAWCell::F_DATE;
      format.m_DTFormat="%m/%d";
      break;
    case 10:
      format.m_format=MWAWCell::F_TIME;
      format.m_DTFormat="%I:%M:%S %p";
      break;
    case 11:
      format.m_format=MWAWCell::F_TIME;
      format.m_DTFormat="%I:%M %p";
      break;
    case 12:
      format.m_format=MWAWCell::F_TIME;
      format.m_DTFormat="%H:%M:%S";
      break;
    case 13:
      format.m_format=MWAWCell::F_TIME;
      format.m_DTFormat="%H:%M";
      break;
    case 14:
      MWAW_DEBUG_MSG(("WingzParser::readSpreadsheetCellList: find cell with custom format\n"));
      f << "format=custom,";
      break;
    default:
      MWAW_DEBUG_MSG(("WingzParser::readSpreadsheetCellList: find cell with format=15\n"));
      f << "##format=15,";
      break;
    }
    val=(int) input->readULong(1);
    bool ok=true;
    switch ((val>>4)&7) {
    case 0: // general
      break;
    case 1:
      cell.setHAlignement(MWAWCell::HALIGN_LEFT);
      f << "align=left,";
      break;
    case 2:
      cell.setHAlignement(MWAWCell::HALIGN_CENTER);
      f << "align=center,";
      break;
    case 3:
      cell.setHAlignement(MWAWCell::HALIGN_RIGHT);
      f << "align=right,";
      break;
    default:
      break;
    }
    if (val&0x8F) f << "f1=" << std::hex << (val&0x8F) << std::dec << ",";
    val=(int) input->readLong(2);
    f << "style=" << val << ",";
    if (m_state->m_spreadsheet.m_styleMap.find(val)==m_state->m_spreadsheet.m_styleMap.end()) {
      f << "#style,";
      MWAW_DEBUG_MSG(("WingzParser::readSpreadsheetStyle: can not find a style\n"));
    }
    else {
      WingzParserInternal::Style const &style=m_state->m_spreadsheet.m_styleMap.find(val)->second;
      cell.setFont(style.m_font);
      if (!style.m_backgroundColor.isWhite())
        cell.setBackgroundColor(style.m_backgroundColor);
    }
    WingzParserInternal::Style style;
    f << "format=[" << format << "],";
    if (type!=1) {
      cell.m_formula=(int) input->readLong(2);
      if (cell.m_formula!=-1)
        f << "formula=" << cell.m_formula << ",";
    }
    MWAWCellContent &content=cell.m_content;
    long dPos=input->tell();
    switch (type) {
    case 1: // only style
      break;
    case 2:
    case 3: {
      ok=false;
      if (dPos+1>endPos)
        break;
      int fSz=(int) input->readULong(1);
      if (dPos+1+fSz>endPos)
        break;
      if (format.m_format==MWAWCell::F_UNKNOWN)
        format.m_format=MWAWCell::F_TEXT;
      if (content.m_contentType!=MWAWCellContent::C_FORMULA)
        content.m_contentType=MWAWCellContent::C_TEXT;
      content.m_textEntry.setBegin(input->tell());
      content.m_textEntry.setLength(fSz);
      std::string text("");
      for (int i=0; i<fSz; ++i) text += (char) input->readULong(1);
      f << text;
      input->seek(dPos+1+fSz, librevenge::RVNG_SEEK_SET);
      ok=true;
      break;
    }
    case 4:
      if (dPos+2>endPos) {
        ok=false;
        break;
      }
      if (format.m_format==MWAWCell::F_UNKNOWN)
        format.m_format=MWAWCell::F_NUMBER;
      if (content.m_contentType!=MWAWCellContent::C_FORMULA)
        content.m_contentType=MWAWCellContent::C_NUMBER;
      content.setValue(std::numeric_limits<double>::quiet_NaN());
      f << "nan" << input->readLong(2) << ",";
      break;
    case 5: { // style + double
      if (dPos+8>endPos) {
        ok=false;
        break;
      }
      double value;
      bool isNAN;
      if (format.m_format==MWAWCell::F_UNKNOWN)
        format.m_format=MWAWCell::F_NUMBER;
      if (content.m_contentType!=MWAWCellContent::C_FORMULA)
        content.m_contentType=MWAWCellContent::C_NUMBER;
      if (input->readDoubleReverted8(value, isNAN))
        content.setValue(value);
      f << value;
      input->seek(dPos+8, librevenge::RVNG_SEEK_SET);
      break;
    }
    default:
      ok=false;
      break;
    }
    cell.setFormat(format);
    // change the reference date from 1/1/1904 to 1/1/1900
    if (format.m_format==MWAWCell::F_DATE && content.isValueSet())
      content.setValue(content.m_value+1462.);

    m_state->m_spreadsheet.m_cells.push_back(cell);
    if (!ok) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  pos=input->tell();
  if (pos==endPos) return true;
  MWAW_DEBUG_MSG(("WingzParser::readSpreadsheetCellList: find some extra data\n"));
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote("SheetCell-end:");
  return true;
}

// style
bool WingzParser::readSpreadsheetCellName()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  int type=(int) input->readULong(1);
  if (type!=7) return false;
  int val=(int) input->readULong(1);
  int dSz= (int) input->readULong(2);
  int id=(int) input->readLong(2);
  long endPos=pos+6+dSz;
  libmwaw::DebugStream f;
  f << "Entries(CellName)[" << id << "]:";
  if (val!=0x40) f << "fl=" << std::hex << val << std::dec << ",";
  if (dSz<10 || !input->checkPosition(pos+6+dSz)) {
    MWAW_DEBUG_MSG(("WingzParser::readSpreadsheetCellName: find bad size for data\n"));
    return false;
  }
  val=(int) input->readLong(2);
  if (val!=-1) f << "f0=" << val << ",";
  int sSz=(int) input->readULong(1);
  if ((sSz!=7&&sSz!=12) || input->tell()+sSz>endPos) {
    MWAW_DEBUG_MSG(("WingzParser::readSpreadsheetCellName: can not determine the block type\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    return true;
  }
  MWAWCellContent::FormulaInstruction instr;
  if (sSz==7) { // a cell
    val=(int) input->readLong(1);
    if (val!=7) f << "f1=" << val << ",";
    int cell[2];
    for (int i=0; i<2; ++i) cell[i]=(int) input->readLong(2);
    instr.m_type=MWAWCellContent::FormulaInstruction::F_Cell;
    instr.m_position[0]=Vec2i(cell[1],cell[0]);
    instr.m_positionRelative[0]=Vec2b(false,false);
    f << "cell=" << instr.m_position[0] << ",";
  }
  else { // a cell list
    for (int i=0; i< 2; ++i) {
      val=(int) input->readLong(1);
      if (val!=7) f << "f" << i+1 << "=" << val << ",";
    }
    int cell[4];
    for (int i=0; i<4; ++i) cell[i]=(int) input->readLong(2);
    instr.m_type=MWAWCellContent::FormulaInstruction::F_CellList;
    instr.m_position[0]=Vec2i(cell[2], cell[0]);
    instr.m_position[1]=Vec2i(cell[3], cell[1]);
    instr.m_positionRelative[0]=instr.m_positionRelative[1]=Vec2b(false,false);
    f << "cell=" << instr.m_position[0] << "<->" << instr.m_position[1] << ",";
  }
  val=(int) input->readLong(1);
  if (val!=-1) f << "g0=" << val << ",";

  sSz=(int) input->readULong(1);
  if (input->tell()+sSz>endPos) {
    MWAW_DEBUG_MSG(("WingzParser::readSpreadsheetCellName: style name seems bad\n"));
    f << "###";
  }
  else {
    std::string name("");
    for (int i=0; i<sSz; ++i) name+=(char) input->readULong(1);
    f << name << ",";
    if (input->tell()!=endPos) {
      MWAW_DEBUG_MSG(("WingzParser::readSpreadsheetCellName: find some extra data\n"));
      ascii().addDelimiter(input->tell(), '|');
    }
  }
  m_state->m_spreadsheet.m_cellIdPosMap[id]=instr;
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

bool WingzParser::readSpreadsheetStyle()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  int type=(int) input->readULong(1);
  if (type!=9) return false;
  int val=(int) input->readULong(1);
  int dSz= (int) input->readULong(2);
  int id=(int) input->readLong(2);
  long endPos=pos+6+dSz;

  libmwaw::DebugStream f;
  f << "Entries(Style)[" << id << "]:";
  if (val!=0x40) f << "fl=" << std::hex << val << std::dec << ",";
  if (dSz<26 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("WingzParser::readSpreadsheetStyle: find bad size for data\n"));
    return false;
  }
  WingzParserInternal::Style style;
  val=(int) input->readLong(2);
  if (val!=1) f << "used=" << val << ",";
  val=(int) input->readLong(2); // always 0?
  if (val) f << "f0=" << val << ",";
  MWAWFont &font=style.m_font;
  font.setSize((float) input->readULong(2));
  int flag=(int) input->readULong(2);
  uint32_t flags=0;
  if (flag&0x1) flags |= MWAWFont::boldBit;
  if (flag&0x2) flags |= MWAWFont::italicBit;
  if (flag&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
  if (flag&0x8) flags |= MWAWFont::embossBit;
  if (flag&0x10) flags |= MWAWFont::shadowBit;
  if (flag&0xFF60)
    f << "#font[flag]=" << std::hex << (flag&0xFF60) << std::dec << ",";
  font.setFlags(flags);

  val=(int) input->readLong(1);
  if (val==1)
    f << "hasCustomFmt,";
  else if (val) f << "#hasCustomFmt=" << val << ",";
  bool hasCustomFormat=val==1;

  int patId=0;
  MWAWColor colors[4];
  for (int i=0; i<4; ++i) { // font, back, unknown,font color
    val=(int) input->readULong(4);
    int col=((val>>16)&0xFF)|(val&0xFF00)|((val<<16)&0xFFFFFF);
    int high=(val>>24);
    colors[i]=MWAWColor(uint32_t(col));
    switch (i) {
    case 0:
      patId=high;
      if (patId) f << "patId=" << patId << ",";
      if (col) f << "backColor=" << std::hex << col << std::dec << ",";
      break;
    case 1:
      if (col!=0xFFFFFF) f << "frontColor=" << std::hex << col << std::dec << ",";
      if (high) f << "g0=" << high << ",";
      break;
    case 2:
      style.m_lineColor=colors[i];
      if (col) f << "lineColor=" << std::hex << col << std::dec << ",";
      if (high!=1) f << "g1=" << high << ",";
      break;
    case 3:
      font.setColor(uint32_t(col));
      if (high) f << "g2=" << high << ",";
      break;
    default:
      break;
    }
  }
  float percent;
  if (patId && m_state->getPatternPercent(patId, version(), percent)) {
    style.m_backgroundColor=MWAWColor::barycenter(percent, colors[0], 1.f-percent, colors[1]);
    if (!style.m_backgroundColor.isWhite())
      f << "cellColor=" << style.m_backgroundColor << ",";
  }
  int nSz=(int) input->readULong(1);
  if (26+nSz>dSz) {
    MWAW_DEBUG_MSG(("WingzParser::readSpreadsheetStyle: the name size seems bad\n"));
    f << "###";
  }
  else {
    std::string name("");
    for (int i=0; i < nSz; ++i) name+=(char) input->readULong(1);
    font.setId(getParserState()->m_fontConverter->getId(name));
  }
  f << font.getDebugString(getParserState()->m_fontConverter) << ",";

  if (hasCustomFormat && input->tell()!=endPos) {
    long actPos=input->tell();
    int fSz=(int) input->readULong(1);
    if (actPos+1+fSz<=endPos) {
      std::stringstream form("");
      for (int i=0; i<fSz; ++i) {
        // list of code defining either a variable, special char or normal char
        int c=(int) input->readULong(1);
        switch (c) {
        case 1:
          form << "\\";
          break;
        case 8:
          form << "[day]";
          break;
        case 0x1a:
          form << "%";
          break;
        case 0x2d:
          form << ":";
          break;
        default:
          if (c>0x30) form << (char) c;
          else form << "[0x" << std::hex << c << std::dec << "]";
        }
      }
      f << "form=\"" << form.str() << "\",";
    }
    else {
      MWAW_DEBUG_MSG(("WingzParser::readSpreadsheetStyle: can not read custom format\n"));
      f << "##format,";
      input->seek(actPos, librevenge::RVNG_SEEK_SET);
    }
  }

  if (m_state->m_spreadsheet.m_styleMap.find(id)!=m_state->m_spreadsheet.m_styleMap.end()) {
    f << "#id,";
    MWAW_DEBUG_MSG(("WingzParser::readSpreadsheetStyle: the style %d already exists\n", id));
  }
  else
    m_state->m_spreadsheet.m_styleMap[id]=style;
  if (input->tell()!=endPos) {
    MWAW_DEBUG_MSG(("WingzParser::readSpreadsheetStyle: find some extra data\n"));
    ascii().addDelimiter(input->tell(), '|');
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// formula
////////////////////////////////////////////////////////////
namespace WingzParserInternal
{
struct Functions {
  char const *m_name;
  int m_arity;
};

static Functions const s_listFunctions[] = {
  { "", -2} /*cell*/,{ "", -2} /*cell*/,{ "", -2} /*cell*/,{ "", -2} /*cell*/,
  { "", -2} /*cell*/,{ "", -2} /*cell*/,{ "", -2} /*cell*/,{ "", -2} /*cell*/,
  { "", -2} /*list cell*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  // 10
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  { "", -2} /*double*/,{ "", -2} /* text*/,{ "", -2} /*1a entry error */,{ "", -2} /*UNKN*/,
  { "", -2}/*if separator?*/,{ "", -2} /*if separator?*/,{ "", -2} /* convert to?*/,{ "", -2} /*convert to bool*/,
  // 20
  { "", -2} /*UNKN*/, { "", -2},{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "(", 1} /*spec()*/,{ "", -2}/*UNKN*/ ,{ "", -2} /*UNKN*/,
  { "", -2} /*convert to?*/,{ "", -2} /*UNKN*/,{ "", -2} /*+number*/,{ "", -2} /*-number*/,
  { "", -2} /* *number*/,{ "", -2} /*/number*/,{ "+", 2},{ "-", 2},
  // 30
  { "*", 2},{ "/", 2}, { "-", 1},{ "", -2} /*UNKN*/,
  { "^", 2},{ "Concatenate", 2},{ "And", 2},{ "Or", 2},
  { "Not", 1},{ "=", 2} ,{ "<", 2} ,{ "<=", 2} ,
  { ">", 2} ,{ ">=", 2} ,{ "<>", 2} ,{ "", -2} /*UNKN*/,
  // 40
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "False", 0},{ "True", 0}, { "", -2} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "E", 0}/*exp 1*/,{ "Pi", 0},{ "IsErr", 1},
  // 50
  { "IsNA", 1},{ "IsNumber", 1},{ "IsString", 1},{ "IsBlank", 1},
  { "", -2} /*UNKN*/,{ "DAverage", 3},{ "DCount", 3},{ "DMax", 3},
  { "DMin", 3},{ "DStDev", 3},{ "DStDevP", 3}/*checkme*/,{ "DSum", 3},
  { "DSumSq", 3},{ "DVar", 3},{ "DVarP", 3}/*checkme*/,{ "Now", 0},
  // 60
  { "CMonth", 1},{ "CWeekday", 1},{ "DateValue", 1},{ "Day", 1},
  { "DayName", 1},{ "Month", 1},{ "MonthName", 1},{ "Year", 1},
  { "ADate", 2},{ "AddDays", 2},{ "AddMonths", 2},{ "AddYears", 2},
  { "Date", 3},{ "Hour", 1},{ "Minute", 1},{ "Second", 1},
  // 70
  { "TimeValue", 1},{ "AddHours", 2},{ "AddMinutes",2},{ "AddSeconds", 2},
  { "Atime", 2},{ "Time", 3},{ "CTERM", 3},{ "FV", 3},
  { "FVL", 3},{ "Interest", 3},{ "LoanTerm", 3},{ "PMT", 3},
  { "Principal", 3},{ "PV", 3},{ "PVL", 3},{ "Rate", 3},
  // 80
  { "SLN", 3},{ "", -2} /*UNKN*/,{ "DDB", 4},{ "SYD", 4},
  { "BondPrice", 5},{ "BondYTM", 5},{ "IRR", -1},{ "NPV", -1},
  { "Acosh", 1},{ "Asinh", 1} /*UNKN*/,{ "Atanh", 1},{ "Cosh", 1},
  { "Sinh", 1},{ "Tanh", 1},{ "If", 3},{ "Choose", -1},
  // 90
  { "NA", 0} /*Err*/,{ "NA", 0},{ "Guess", 0},{ "Abs", 1},
  { "Factorial", 1},{ "Int", 1},{ "Sign", 1},{ "", -2} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "Mod", 2},{ "Round", 2},
  { "Goal", 3},{ "Rand", 0},{ "Exponential", 1} /*odd: maybe set*/,{ "Normal", 1},
  // a0
  { "Uniform", 1},{ "Average", -1},{ "Count", -1},{ "Max", -1},
  { "Min", -1},{ "StD", -1},{ "StDev", -1},{ "Sum", -1},
  { "SumSq", -1},{ "Var", -1},{ "VarP", -1}/*checkme*/,{ "Char", 1},
  { "Code", 1},{ "Length", 1},{ "Lower", 1},{ "", -2} /*UNKN*/,
  // b0
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "Currency", 1},{ "Proper", 1},
  { "", -2} /*UNKN*/,{ "Exact", 2},{ "NFormat", 2},{ "Left", 2},
  { "Right", 2},{ "", -2} /*UNKN*/,{ "Collate", 2},{ "Rept", 2},
  { "Find", 3},{ "Match", 3},{ "MID", 3},{ "Replace", 4},
  // c0
  { "Exp", 1},{ "Ln", 1},{ "Log", 1},{ "Logn", 2},
  { "Sqrt", 1},{ "Acos", 1},{ "Asin", 1},{ "Atan", 1},
  { "Cos", 1},{ "Degrees", 1},{ "Radians", 1},{ "Sin", 1},
  { "Tan", 1},{ "Atan2", 2},{ "Col", 0},{ "Row", 0},
  // d0
  { "Cols", 1},{ "", -2} /*UNKN*/,{ "Indirect", 1},{ "Range", 1},
  { "MakeCell", 2},{ "HLookUp", 3},{ "Index", 3},{ "", -2} /*UNKN*/,
  { "MakeRange", 4},{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  // e0
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  { "FunctE4", 2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  { "N", 1},{ "Cell", 0},{ "Contains", 2},{ "", -2} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  // f0
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
};
}

bool WingzParser::readFormula()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell(), debPos=pos;
  int type=(int) input->readULong(1);
  if (type!=8) return false;
  int val=(int) input->readULong(1);
  int dSz= (int) input->readULong(2);
  long endPos=pos+6+dSz;
  if (dSz<7 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("WingzParser::readFormula: find bad size for data\n"));
    return false;
  }
  int id=(int) input->readLong(2);
  libmwaw::DebugStream f;
  f << "Entries(Formula)[" << id << "]:";
  if (val!=0x40) f << "fl=" << std::hex << val << std::dec << ",";
  for (int i=0; i<2; ++i) { // f0=1|2, f1=0
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  val=(int) input->readLong(1); // always 0, maybe related to the equal sign
  if (val) f << "f2=" << val << ",";
  bool ok = true;
  std::vector<std::vector<MWAWCellContent::FormulaInstruction> > stack;
  std::string error("");
  while (long(input->tell()) != endPos) {
    pos = input->tell();
    if (pos > endPos) return false;
    int wh = (int) input->readULong(1);
    if (wh==0xFF) break; // operator=
    double value;
    int arity = 0;
    bool isNan;
    MWAWCellContent::FormulaInstruction instr;
    bool noneInstr=false;
    switch (wh) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7: { // difference with 0-3 ?
      if (pos+1+4>endPos) {
        error="#cell";
        ok=false;
        break;
      }
      int cPos[2];
      for (int i=0; i<2; ++i) cPos[i]=(int) input->readLong(2);
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Cell;
      instr.m_position[0]=Vec2i(cPos[1], cPos[0]);
      instr.m_positionRelative[0]=Vec2b((wh&1)==0, (wh&2)==0);
      break;
    }
    case 8: {
      int typ=(int) input->readULong(1);
      if (typ>0xF || pos+1+9>endPos) {
        error="#listCell";
        ok=false;
        break;
      }
      int cPos[4];
      for (int i=0; i<4; ++i) cPos[i]=(int) input->readLong(2);
      instr.m_type=MWAWCellContent::FormulaInstruction::F_CellList;
      instr.m_position[0]=Vec2i(cPos[2], cPos[0]);
      instr.m_position[1]=Vec2i(cPos[3], cPos[1]);
      instr.m_positionRelative[0]=Vec2b((typ&2)==0, (typ&1)==0);
      instr.m_positionRelative[1]=Vec2b((typ&8)==0, (typ&4)==0);
      break;
    }
    case 0x9:
    case 0xa: {
      if (pos+1+2>endPos) {
        ok=false;
        break;
      }
      int cId=(int) input->readLong(2);
      if (m_state->m_spreadsheet.m_cellIdPosMap.find(cId)==m_state->m_spreadsheet.m_cellIdPosMap.end()) {
        MWAW_DEBUG_MSG(("WingzParser::readFormula: can not find cell with id\n"));
        std::stringstream s;
        s << "##cellId=" << cId << ",";
        error=s.str();
        ok=false;
        break;
      }
      instr=m_state->m_spreadsheet.m_cellIdPosMap.find(cId)->second;
      break;
    }
    case 0x1a: // error related to Sheet2Err
      if (pos+1+2>endPos) {
        ok=false;
        break;
      }
      f << "f1a=" << input->readLong(1) << "x" << input->readLong(1) <<",";
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Function;
      instr.m_content="NA";
      break;
    case 0x1c:
    case 0x1d: // appear with if, maybe to separate condition to other ?
      if (pos+1+2>endPos) {
        ok=false;
        break;
      }
      noneInstr=true;
      f << "f" << std::hex << wh << std::dec << "=" << input->readLong(2) << ",";
      break;
    case 0x1e: // convert to ?
    case 0x1f: // convert to bool
    case 0x26:
    case 0x27:
    case 0x28: // convert to ?
      noneInstr=true;
      break;
    case 0x18:
    case 0x2a:
    case 0x2b:
    case 0x2c:
    case 0x2d:
      if (endPos-pos<9 || !input->readDoubleReverted8(value, isNan)) {
        error="#number";
        ok = false;
        break;
      }
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Double;
      instr.m_doubleValue=value;
      if (wh>=0x2a&&wh<=0x2d) { // mixed operator + number
        stack.push_back(std::vector<MWAWCellContent::FormulaInstruction>(1,instr));
        static char const *(what[])= {"+","-","*","/"};

        instr=MWAWCellContent::FormulaInstruction();
        instr.m_type=MWAWCellContent::FormulaInstruction::F_Function;
        instr.m_content=what[wh-0x2a];
        arity=2;
      }
      break;
    case 0x19: {
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Text;
      int fSz=(int) input->readULong(1);
      if (pos+1+fSz > endPos) {
        ok=false;
        break;
      }
      for (int i=0; i<fSz; ++i) {
        char c = (char) input->readULong(1);
        if (c==0) {
          ok = i+1==fSz;
          break;
        }
        instr.m_content += c;
      }
      break;
    }
    case 0xfe: { // checkme
      if (pos+1+1 > endPos) {
        ok=false;
        break;
      }
      wh=(int) input->readULong(1);
      switch (wh) {
      case 0x25:
        instr.m_type=MWAWCellContent::FormulaInstruction::F_Function;
        instr.m_content="CellText";
        arity=1;
        break;
      case 0x27:
        instr.m_type=MWAWCellContent::FormulaInstruction::F_Function;
        instr.m_content="IsRange";
        arity=1;
        break;
      case 0x92:
        instr.m_type=MWAWCellContent::FormulaInstruction::F_Function;
        instr.m_content="ColOf";
        arity=1;
        break;
      case 0x93:
        instr.m_type=MWAWCellContent::FormulaInstruction::F_Function;
        instr.m_content="RowOf";
        arity=1;
        break;
      default: {
        std::stringstream s;
        s << "##FunctExtra" << std::hex << wh << std::dec << ",";
        error=s.str();
        ok = false;
        break;
      }
      }
      break;
    }
    default:
      if (wh < 0xf0 && WingzParserInternal::s_listFunctions[wh].m_arity > -2) {
        instr.m_content=WingzParserInternal::s_listFunctions[wh].m_name;
        instr.m_type=MWAWCellContent::FormulaInstruction::F_Function;
        arity=WingzParserInternal::s_listFunctions[wh].m_arity;
      }
      if (instr.m_content.empty()) {
        std::stringstream s;
        s << "##Funct" << std::hex << wh << std::dec << ",";
        error=s.str();
        ok = false;
      }
      if (arity==-1) arity=(int)input->readULong(1);
      break;
    }
    if (!ok) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    if (noneInstr) continue;
    std::vector<MWAWCellContent::FormulaInstruction> child;
    if (instr.m_type!=MWAWCellContent::FormulaInstruction::F_Function) {
      child.push_back(instr);
      stack.push_back(child);
      continue;
    }
    size_t numElt = stack.size();
    if ((int) numElt < arity) {
      std::stringstream s;
      s << instr.m_content << "[##" << arity << "]";
      error=s.str();
      ok = false;
      break;
    }
    if ((instr.m_content[0] >= 'A' && instr.m_content[0] <= 'Z') || instr.m_content[0] == '(') {
      if (instr.m_content[0] != '(')
        child.push_back(instr);

      instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
      instr.m_content="(";
      child.push_back(instr);
      for (int i = 0; i < arity; i++) {
        if (i) {
          instr.m_content=";";
          child.push_back(instr);
        }
        std::vector<MWAWCellContent::FormulaInstruction> const &node=
          stack[size_t((int)numElt-arity+i)];
        child.insert(child.end(), node.begin(), node.end());
      }
      instr.m_content=")";
      child.push_back(instr);

      stack.resize(size_t((int) numElt-arity+1));
      stack[size_t((int)numElt-arity)] = child;
      continue;
    }
    if (arity==1) {
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
      stack[numElt-1].insert(stack[numElt-1].begin(), instr);
      if (wh==0x3 && pos+2==endPos)
        break;
      continue;
    }
    if (arity==2) {
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
      stack[numElt-2].push_back(instr);
      stack[numElt-2].insert(stack[numElt-2].end(), stack[numElt-1].begin(), stack[numElt-1].end());
      stack.resize(numElt-1);
      continue;
    }
    ok=false;
    error = "### unexpected arity";
  }
  pos=input->tell();
  if (pos!=endPos || !ok || stack.size()!=1 || stack[0].empty()) {
    MWAW_DEBUG_MSG(("WingzParser::readFormula: can not read a formula\n"));
    ascii().addDelimiter(pos, '|');
    input->seek(endPos, librevenge::RVNG_SEEK_SET);

    for (size_t i = 0; i < stack.size(); ++i) {
      for (size_t j=0; j < stack[i].size(); ++j)
        f << stack[i][j] << ",";
    }
    if (!error.empty())
      f << error;
    else
      f << "##unknownError,";
    ascii().addPos(debPos);
    ascii().addNote(f.str().c_str());
    return true;
  }

  m_state->m_spreadsheet.m_formulaMap[id]=stack[0];
  for (size_t i=0; i < stack[0].size(); ++i)
    f << stack[0][i];
  f << ",";
  ascii().addPos(debPos);
  ascii().addNote(f.str().c_str());
  return true;
}

// column/row dim
bool WingzParser::readSpreadsheetSize()
{
  MWAWInputStreamPtr input = getInput();
  int const vers=version();
  long pos=input->tell();
  int type=(int) input->readULong(1);
  if (type!=1 && type!=2) return false;
  libmwaw::DebugStream f;
  f << "Entries(SheetSize)[" << (type==1 ? "col" : "row") << "]:";
  int val=(int) input->readULong(1);
  if (val!=0x80) f << "fl=" << std::hex << val << std::dec << ",";
  int dSz= (int) input->readULong(2);
  if (dSz%4 || !input->checkPosition(pos+(vers==1 ? 4 : 6)+dSz)) {
    MWAW_DEBUG_MSG(("WingzParser::readSpreadsheetSize: find bad size for data\n"));
    return false;
  }
  if (vers>1) {
    int id=(int) input->readLong(2);
    if (id) f << "id=" << id << ",";
  }
  f << "pos=[";
  float &defaultDim= type==1 ? m_state->m_spreadsheet.m_widthDefault :
                     m_state->m_spreadsheet.m_heightDefault;
  std::vector<float> &dimList=type==1 ? m_state->m_spreadsheet.m_widthCols :
                              m_state->m_spreadsheet.m_heightRows;
  for (int i=0; i<dSz/4; ++i) {
    int cell=(int) input->readULong(2); // the row/col number
    float dim=float(input->readULong(2))/20.f;  // in TWIP
    if (cell==0xFFFF) f << "-inf";
    else if (cell==0x7FFF) {
      defaultDim =dim;
      f << "inf";
    }
    else {
      if (cell < int(dimList.size()) || cell > int(dimList.size())+1000) {
        MWAW_DEBUG_MSG(("WingzParser::readSpreadsheetSize: the cell seems bad\n"));
        f << "###";
      }
      else
        dimList.resize(size_t(cell)+1, dim);
      f << cell;
    }
    f << ":" << dim << "pt,";
  }
  f << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  return true;
}

// page break pos
bool WingzParser::readSpreadsheetPBreak()
{
  MWAWInputStreamPtr input = getInput();
  int const vers=version();
  long pos=input->tell();
  int type=(int) input->readULong(1);
  if (type!=18 && type!=19) return false;
  libmwaw::DebugStream f;
  f << "Entries(SheetPbrk)[" << (type==18 ? "col" : "row") << "]:";
  int val=(int) input->readULong(1);
  if (val!=0x80) f << "fl=" << std::hex << val << std::dec << ",";
  int dSz= (int) input->readULong(2);
  if (dSz%4 || !input->checkPosition(pos+(vers==1 ? 4 : 6)+dSz)) {
    MWAW_DEBUG_MSG(("WingzParser::readSpreadsheetPBreak: find bad size for data\n"));
    return false;
  }
  if (vers==2) {
    int id=(int) input->readLong(2);
    if (id) f << "id=" << id << ",";
  }
  f << "pos=[";
  for (int i=0; i<dSz/4; ++i) {
    int cell=(int) input->readULong(2); // the row/col number
    if (cell==0xFFFF) f << "-inf";
    else if (cell==0x7FFF) f << "inf";
    else f << cell;
    f << "[sz=" << input->readULong(2) << "],"; // num row/column
  }
  f << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  return true;
}

// unknown zone
bool WingzParser::readSpreadsheetZone5()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  int type=(int) input->readULong(1);
  if (type!=5) return false;
  int val=(int) input->readULong(1);
  int dSz= (int) input->readULong(2);
  long endPos=pos+6+dSz;
  int id=(int) input->readLong(2);

  libmwaw::DebugStream f;
  f << "Entries(ZSheet5)[" << id << "]:";
  if (val!=0x40) f << "fl=" << std::hex << val << std::dec << ",";
  if (dSz<2 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("WingzParser::readSpreadsheetZone5: find bad size for data\n"));
    return false;
  }
  val=(int) input->readULong(2);
  if (val!=dSz) f << "#dSz=" << val << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  while (!input->isEnd()) {
    pos=input->tell();
    if (pos >= endPos) break;
    type=(int) input->readLong(1);
    bool ok=true;
    f.str("");
    f << "ZSheet5-" << std::hex << type << std::dec << ":";
    switch (type) {
    case 0: // nothing
    case 0x4:
      break;
    case 0x3:
      if (pos+4>endPos) {
        ok=false;
        break;
      }
      input->seek(pos+4, librevenge::RVNG_SEEK_SET);
      break;
    case 0x5: // the row?
      if (pos+5>endPos) {
        ok=false;
        break;
      }
      input->seek(pos+5, librevenge::RVNG_SEEK_SET);
      break;
    case 0x1:
    case 0x2:
      if (pos+3>endPos) {
        ok=false;
        break;
      }
      input->seek(pos+3, librevenge::RVNG_SEEK_SET);
      break;
    default:
      ok=false;
      break;
    }
    if (!ok) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  pos=input->tell();
  if (pos==endPos)
    return true;
  MWAW_DEBUG_MSG(("WingzParser::readSpreadsheetZone5: find some extra data\n"));
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote("ZSheet5-end:###");
  return true;
}

// retrieve a next spreadheet zone (used when parsing stop for some problem )
bool WingzParser::findNextZone(int lastType)
{
  MWAWInputStreamPtr input = getInput();
  bool lastCheck=true;
  while (!input->isEnd()) {
    long pos=input->tell();
    int val=(int)input->readULong(2);
    int type=val&0xFF;
    if (type==0x80) {
      if (!lastCheck) {
        input->seek(-3, librevenge::RVNG_SEEK_CUR);
        lastCheck=true;
      }
      continue;
    }
    lastCheck=false;
    if ((val&0xff00)!=0x8000 || (lastType==0 && type!=1) || type>=0x14 || type<lastType)
      continue;
    long dSz=(long) input->readULong(2);
    if (type==0xc) dSz+=4;
    else if (type==0x10) dSz+=4;
    else if (type==0xe) {
      if (dSz < 0x80) dSz += 2;
      else if (input->checkPosition(pos+0x40)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        return true;
      }
      else {
        input->seek(pos+2, librevenge::RVNG_SEEK_SET);
        continue;
      }
    }
    if (input->checkPosition(pos+6+dSz+2)) {
      input->seek(pos+6+dSz+1,librevenge::RVNG_SEEK_SET);
      val=(int) input->readULong(1);
      if ((val&0xC0) && !(val&0x3F)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        return true;
      }
    }
    input->seek(pos+2, librevenge::RVNG_SEEK_SET);
  }
  return false;
}

////////////////////////////////////////////////////////////
// read a graphic zone
////////////////////////////////////////////////////////////
bool WingzParser::readGraphic()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  int type=(int) input->readULong(1);
  if (type!=0xe) return false;
  int fl=(int) input->readULong(1);
  int dSz=(int) input->readULong(2);
  int id= (fl==0) ? 0 : (int) input->readULong(2);
  libmwaw::DebugStream f;
  f << "Entries(Graphic):";
  if (fl!=0x80) f << "fl=" << std::hex << fl << std::dec << ",";
  if (id) f << "id=" << id << ",";
  if (!input->checkPosition(pos+60)) {
    MWAW_DEBUG_MSG(("WingzParser::readGraphic: the header seems bad\n"));
    return false;
  }
  long actPos=input->tell();
  int nSz=(int) input->readULong(1);
  if (nSz>15) {
    MWAW_DEBUG_MSG(("WingzParser::readGraphic: the graphic title seems bad\n"));
    f << "#nSz=" << nSz << ",";
  }
  else if (nSz) {
    std::string name("");
    for (int i=0; i<nSz; ++i) name+=(char)input->readULong(1);
    f << name << ",";
  }
  input->seek(actPos+16, librevenge::RVNG_SEEK_SET);
  int order=(int) input->readULong(2);
  f << "order=" << order << ",";
  int val=(int) input->readULong(2); // always 0
  if (val) f << "f1=" << val << ",";
  // the position seem to be stored as cell + % of the cell width...
  int decal[4];
  for (int i=0; i<4; ++i) decal[i]=(int) input->readULong(1);
  int dim[4]; // the cells which included the picture
  for (int i=0; i<4; ++i) dim[i]=(int) input->readULong(2);
  f << "dim=" << dim[0] << ":" << double(decal[2])/255. << "x"
    << dim[1] << ":" << double(decal[0])/255. << "<->"
    << dim[2] << ":" << double(decal[3])/255. << "x"
    << dim[3] << ":" << double(decal[1])/255. << ",";
  type=(int) input->readULong(2);
  val=(int) input->readULong(2); // always 0
  if (val) f << "f2=" << val << ",";

  long endPos=pos+(version()==1 ? 4 : 8)+dSz;
  long dataPos=input->tell();
  switch (type) {
  case 0:
  case 2: { // button, textbox
    f << "TextZone,g0=" << std::hex << dSz << std::dec << ",";
    for (int i=0; i<2; ++i) { // name, title
      int sSz=(int)input->readULong(1);
      if (!input->checkPosition(input->tell()+sSz+1)) {
        MWAW_DEBUG_MSG(("WingzParser::readGraphic: can not find the textbox name%d\n", i));
        return false;
      }
      std::string name("");
      for (int c=0; c<sSz; ++c) name+=(char) input->readULong(1);
      if (sSz) f << name << ",";
    }
    int hasMacro=(int)input->readLong(1);
    if (hasMacro==1) {
      f << "macro,";
      if (!readMacro()) return false;
    }
    else if (hasMacro) {
      f << "###macro=" << hasMacro << ",";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      MWAW_DEBUG_MSG(("WingzParser::readGraphic: can not find the textbox type\n"));
      return false;
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return readTextZone();
  }
  case 4:
    f << "Chart,";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return readChartData();
  case 5:
  case 6:
  case 7:
  case 8:
  case 9: {
    static int const(expectedSize[])= {0x38, 0x3c, 0x34, 0x40, 0x40 };
    if (!input->checkPosition(endPos) || dSz < expectedSize[type-5]) {
      MWAW_DEBUG_MSG(("WingzParser::readGraphic: find bad size for shape\n"));
      return false;
    }
    static char const *(what[])= {"line", "arc", "circle", "rectangle", "poly" };
    f << what[type-5] << ",";
    for (int i=0; i<4; ++i) { // g0=g3=0, g2=0|-1, g4=0|19|202
      val=(int) input->readLong(1);
      if (val) f << "g" << i << "=" << val << ",";
    }
    int numColors=type==8?5:3;
    for (int i=0; i<numColors; ++i) { // back, unknown,front color
      val=(int) input->readULong(4);
      int col=int(val&0xFFFFFF);
      int high=(val>>24);
      if (((i%2)==0&&col!=0xFFFFFF) || ((i%2)&&col))
        f << "col" << i << "=" << std::hex << col << std::dec << ",";
      if (high!=1) f << "col" << i << "[high]=" << high << ","; // 0|1
    }
    val=(int) input->readLong(2);
    if (val!=5) f << "g4=" << val << ",";
    switch (type) {
    case 5:
      // checkme, look like the polygon
      for (int i=0; i<4; ++i) {
        static int const(expected[])= {0x1c,2,0/*|1|2*/,0};
        val=(int) input->readLong(1);
        if (val != expected[i]) f << "h" << i << "=" << val << ",";
      }
      break;
    case 6:
      for (int i=0; i<2; ++i) { // cell position?
        int delta=(int) input->readULong(1);
        int cell=(int) input->readLong(1);
        if (val) f << "h" << i << "=" << double(cell)+double(delta)/256. << ",";
      }
      for (int i=0; i<2; ++i) { // h2=0|384|a8c, h3=384
        val=(int) input->readULong(2);
        if (val) f << "h" << i+2 << "=" << val << ",";
      }
      break;
    case 7:
      break;
    case 8: // always 20x20
      f << "corner?=" << input->readLong(2) << "x" <<  input->readLong(2) << ",";
      break;
    case 9: {
      val=(int) input->readULong(2);
      if (val) f << "h0=" << val << ",";
      int nbPt=0;
      for (int i=0; i<4; ++i) {
        static int const(expected[])= {0x1c,2,0/*|1|2*/,0};
        val=(int) input->readLong(1);
        if (i==2) {
          nbPt=val;
          continue;
        }
        if (val != expected[i]) f << "h" << i << "=" << val << ",";
      }
      f << "nbPt=" << nbPt << ",";
      if (input->tell()+nbPt*4>endPos) {
        f << "###";
        break;
      }
      f << "pts=[";
      for (int i=0; i<nbPt; ++i) {
        for (int j=0; j<2; ++j) { // checkme
          int delta=(int) input->readULong(1);
          int cell=(int) input->readLong(1);
          f << double(cell)+double(delta)/256. << (j ? "," : "x");
        }
      }
      f << "],";
      break;
    }
    default:
      break;
    }
    break;
  }
  case 0xb: {
    if (!input->checkPosition(endPos)) {
      MWAW_DEBUG_MSG(("WingzParser::readGraphic: find bad size for group\n"));
      return false;
    }
    f << "group,";
    break;
  }
  case 10: {
    if (!input->checkPosition(endPos)) {
      MWAW_DEBUG_MSG(("WingzParser::readGraphic: find bad size for picture\n"));
      return false;
    }
    f << "picture,";
    long pSz=(long)input->readULong(2);
    for (int i=0; i<2; ++i) { // g0=0, g1=2
      val=(int) input->readULong(2);
      if (val) f << "g" << i << "=" << val << ",";
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    if (!pSz || !input->checkPosition(dataPos+6+pSz)) {
      MWAW_DEBUG_MSG(("WingzParser::readGraphic: can not find the picture data\n"));
      return false;
    }
#ifdef DEBUG_WITH_FILES
    ascii().skipZone(dataPos, dataPos+6+pSz-1);
    librevenge::RVNGBinaryData file;
    input->seek(dataPos+6, librevenge::RVNG_SEEK_SET);
    input->readDataBlock(pSz, file);
    static int volatile pictName = 0;
    f.str("");
    f << "PICT-" << ++pictName;
    libmwaw::Debug::dumpFile(file, f.str().c_str());
#endif
    input->seek(dataPos+6+pSz, librevenge::RVNG_SEEK_SET);
    return true;
  }
  default:
    MWAW_DEBUG_MSG(("WingzParser::readGraphic: find some unknown type %d\n", type));
    f << "#typ=" << type << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  if (input->tell()!=pos && input->tell()!=endPos)
    ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// text box
////////////////////////////////////////////////////////////
bool WingzParser::readTextZone()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  if (!input->checkPosition(pos+0x30)) {
    MWAW_DEBUG_MSG(("WingzParser::readTextZone: the zone seems too short\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  libmwaw::DebugStream f;
  f << "Entries(TextZone):";
  int val;
  for (int i=0; i<6; ++i) { //  color ?
    val=(int) input->readULong(4);
    int col=int(val&0xFFFFFF);
    int high=(val>>24);
    if (((i%2)&&col!=0xFFFFFF) || ((i%2)==0&&col))
      f << "col" << i << "=" << std::hex << col << std::dec << ",";
    if (high!=1) f << "col" << i << "[high]=" << high << ","; // 0|1
  }
  for (int i=0; i<3; ++i) { // f0=5/20, f1=f2=20
    val=(int) input->readLong(2);
    if (val!=20) f << "f" << i << "=" << val << ",";
  }
  for (int i=0; i<3; ++i) { //  color ?
    val=(int) input->readULong(4);
    int col=int(val&0xFFFFFF);
    int high=(val>>24);
    if (((i%2)&&col!=0xFFFFFF) || ((i%2)==0&&col))
      f << "colA" << i << "=" << std::hex << col << std::dec << ",";
    if (high!=1) f << "colA" << i << "[high]=" << high << ","; // 0|1
  }
  for (int i=0; i<2; ++i) { // text and title font
    f << "font" << i << "=[";
    int fSz=(int) input->readULong(1);
    f << "sz=" << fSz << ",";
    int flag=(int) input->readULong(1);
    if (flag)
      f << "flags=" << std::hex << flag << std::dec << ",";
    int sSz=(int) input->readULong(1);
    if (!sSz || !input->checkPosition(input->tell()+4+sSz)) {
      MWAW_DEBUG_MSG(("WingzParser::readTextZone: can not determine the string zone %d\n", i));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    std::string font("");
    for (int j=0; j<sSz; ++j)
      font+=(char) input->readLong(1);
    f << font << ",";
    val=(int) input->readULong(4);
    int col=int(val&0xFFFFFF);
    int high=(val>>24);
    if (col)
      f << "col=#" << std::hex << col << std::dec << ",";
    if (high) f << "col" << i << "[high]=" << high << ","; // 0
    f << "],";
  }
  val=(int) input->readLong(2);
  if (val) f << "g0=" << val;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "TextZone-A:";
  int type=(int) input->readLong(1);
  bool ok=true;
  switch (type) {
  case 0:
    f << "button,";
    val=(int) input->readLong(1);
    if (val!=3) f << "f0=" << val << ",";
    val=(int) input->readLong(1);
    if (val==0) f << "noContent,";
    else if (val!=1) f << "#content=" << val << ",";
    val=(int) input->readLong(1);
    if (val==1) f << "title,";
    else if (val) f << "#title=" << val << ",";
    val=(int) input->readULong(1);
    if (val) f << "h[content]=" << val << ",";
    val=(int) input->readULong(1);
    if (val) f << "h[title]=" << val << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return true;
  case 1:
    ok=input->checkPosition(pos+60);
    if (!ok) break;
    f << "text,";
    break;
  case 5: // double result ?
    ok=input->checkPosition(pos+53);
    if (!ok) break;
    f << "double,";
    input->seek(pos+53, librevenge::RVNG_SEEK_SET);
    break;
  case 6: // nan result ?
    ok=input->checkPosition(pos+40);
    if (!ok) break;
    f << "nan,";
    input->seek(pos+40, librevenge::RVNG_SEEK_SET);
    break;
  default:
    MWAW_DEBUG_MSG(("WingzParser::readTextZone: find unknown type %d\n", type));
    f << "###type=type";
    ok=false;
    break;
  }
  if (!ok || type!=1) {
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    if (ok) return true;
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return findNextZone(0xe) && input->tell()>pos;
  }
  val=(int) input->readLong(1);
  if (val!=3) f<<"f0=" << val << ",";
  for (int i=0; i<5; ++i) {
    val=(int) input->readULong(2);
    if (i==2 && (val>>12)) { // 1 left, 2 center, 3 right
      f << "align=" << (val>>12) << ",";
      val &= 0xFFF;
    }
    if (val) f<<"f" << i+1 << "=" << std::hex << val << std::dec << ",";
  }
  val=(int) input->readULong(4);
  int textSize=(int) input->readLong(4);
  if (val!=textSize)
    f << "selection="<<val << ",";
  val=(int) input->readLong(2); // 1|7
  if (val!=1) f << "g0=" << val << ",";
  val=(int) input->readLong(2);
  if (val) f << "g1=" << val << ",";
  for (int i=0; i< 2; ++i) { // g2=0|1, g3=4|6, g3=64 -> scroll bar, g3&1 -> cell note?
    val=(int) input->readULong(1);
    static int const expected[]= {0,0x40};
    if (val!=expected[i])
      f << "g" << i+2 << "=" << std::hex << val << std::dec << ",";
  }
  int numFonts=(int) input->readLong(2);
  if (numFonts!=1) f << "numFonts=" << numFonts << ",";
  val=(int) input->readLong(2);
  if (val) f << "h0=" << val << ",";
  int numPos=(int) input->readULong(2);
  if (numPos!=1) f << "numPos=" << numPos << ",";
  for (int i=0; i<14; ++i) {
    val=(int) input->readLong(2);
    if (!val) continue;
    if (i==3) f << "marg[top]="  << double(val)/20. << ",";
    else if (i==4) f << "marg[bottom]="  << double(val)/20. << ",";
    else if (i==7) f << "tabs[repeat]=" << double(val)/20. << ",";
    else
      f << "h" << i+1 << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  if (!input->checkPosition(pos+textSize)) {
    MWAW_DEBUG_MSG(("WingzParser::readTextZone: the text zone seems bad\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  f.str("");
  f << "TextZone[text]:";
  std::string text("");
  for (int i=0; i< textSize; ++i) text+=(char) input->readULong(1);
  f << text;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  if (!input->checkPosition(pos+numFonts*7)) {
    MWAW_DEBUG_MSG(("WingzParser::readTextZone: the fonts zone seems bad\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  f.str("");
  f << "TextZone[fonts]:";
  for (int i=0; i<numFonts; ++i) {
    f << "font" << i << "=[";
    val=(int) input->readULong(4);
    int col=int(val&0xFFFFFF);
    int high=(val>>24);
    if (col)
      f << "col=#" << std::hex << col << std::dec << ",";
    if (high) f << "col" << i << "[high]=" << high << ","; // 0
    int fSz=(int) input->readULong(1);
    f << "sz=" << fSz << ",";
    int flag=(int) input->readULong(1);
    if (flag)
      f << "flags=" << std::hex << flag << std::dec << ",";
    int sSz=(int) input->readULong(1);
    if (!sSz || !input->checkPosition(input->tell()+sSz)) {
      MWAW_DEBUG_MSG(("WingzParser::readTextZone: can not determine the font %d\n", i));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    std::string font("");
    for (int j=0; j<sSz; ++j)
      font+=(char) input->readLong(1);
    f << font << ",";
    f << "],";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  if (!input->checkPosition(pos+16+numPos*6)) {
    MWAW_DEBUG_MSG(("WingzParser::readTextZone: the last zone seems bad\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  f.str("");
  f << "TextZone-B:";
  for (int i=0; i<7; ++i) {
    val=(int) input->readLong(2);
    if (val==0) continue;
    switch (i) {
    case 2:
      f << "marg[left]=" << double(val)/20. << ",";
      break;
    case 3:
      f << "marg[right]=" << double(val)/20. << ",";
      break;
    case 4:
      f << "para[indent]=" << double(val)/20. << ",";
      break;
    case 5:
      f << "height[leading]=" << double(val)/20. << ",";
      break;
    default:
      f << "f" << i << "=" << val << ",";
      break;
    }
  }
  val=(int) input->readLong(1);
  switch (val) {
  case 1: // normal
    break;
  case 2:
    f << "interline=200%,";
    break;
  case 3:
    f << "interline=150%,";
    break;
  case 4:
    f << "interline=fixed,";
    break;
  case 5:
    f << "interline=extra[leading],";
    break;
  default:
    f << "#interline=" << val << ",";
    break;
  }
  val=(int) input->readLong(1); // 1|2
  if (val!=1) f << "f8=" << val << ",";
  int lastPos=0;
  f << "pos=[";
  for (int i=0; i<numPos; ++i) {
    int newPos=(int) input->readULong(4);
    int ft=(int) input->readULong(2);
    if ((i==0 && newPos!=0) || (i && (newPos<lastPos || newPos>textSize)) || (ft>numFonts)) {
      MWAW_DEBUG_MSG(("WingzParser::readTextZone: the position zone seems bad\n"));
      f << "##";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    f << std::hex << newPos << std::dec << ":" << ft << ",";
  }
  f << "],";

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

bool WingzParser::readMacro()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  if (!input->checkPosition(pos+76)) {
    MWAW_DEBUG_MSG(("WingzParser::readMacro: the zone seems too short\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  libmwaw::DebugStream f;
  f << "Entries(Macro):";
  long textSize=(long) input->readULong(4);
  f << "textSize=" << std::hex << textSize << std::dec << ",";
  long scriptSize=(long) input->readULong(4);
  f << "scriptSize=" << std::hex << scriptSize << std::dec << ",";
  for (int i=0; i<3; ++i) {
    long sz=(long) input->readULong(4);
    if (sz!=scriptSize)
      f << "sel" << i << "=" << std::hex << sz << std::dec << ",";
  }
  for (int i=0; i<28; ++i) { // f22=f24=0|1|2
    int val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  if (!scriptSize || !input->checkPosition(pos+scriptSize)) {
    MWAW_DEBUG_MSG(("WingzParser::readMacro: the script size seems bad\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  f.str("");
  f << "Macro[script]:";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(pos+scriptSize, librevenge::RVNG_SEEK_SET);

  pos=input->tell();
  if (!input->checkPosition(pos+textSize)) {
    MWAW_DEBUG_MSG(("WingzParser::readMacro: the text size seems bad\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  f.str("");
  f << "Macro[text]:";
  std::string text("");
  for (long i=0; i<textSize; ++i) text+=(char) input->readULong(1);
  f << text;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(pos+textSize, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// read the chart (end data)
////////////////////////////////////////////////////////////
bool WingzParser::readChartData()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell(), debPos=pos;
  libmwaw::DebugStream f;
  f << "Entries(Chart):";
  int val=(int) input->readLong(2);
  f << "f0=" << val << ",";
  val=(int) input->readLong(2);
  f << "f1=" << val << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  if (val>0)
    return true;
  if (!input->checkPosition(pos+866)) {
    MWAW_DEBUG_MSG(("WingzParser::readChartData: the zone seems to short\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  pos+=193;
  f.str("");
  f << "Chart-header:";
  input->seek(pos+73, librevenge::RVNG_SEEK_SET);
  ascii().addDelimiter(input->tell(),'|');
  int numZoneB=(int) input->readULong(2);
  f << "numZoneB=" << numZoneB << ",";
  long endPos=debPos+866+73*numZoneB;
  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("WingzParser::readChartData: the zone seems to short\n"));
    if (!findNextZone(0xe) || input->tell()<debPos+866) {
      MWAW_DEBUG_MSG(("WingzParser::readChartData: can not find the next zone\n"));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    numZoneB=0;
    endPos=input->tell();
    input->seek(pos+75, librevenge::RVNG_SEEK_SET);
  }
  ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos+=153;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<4; ++i) {
    pos=input->tell();
    f.str("");
    f << "Chart-A" << i+1 << ":";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());

    input->seek(pos+113, librevenge::RVNG_SEEK_SET);
  }
  pos=input->tell();
  ascii().addPos(pos);
  ascii().addNote("Chart-A5:");

  pos+=68;
  input->seek(pos, librevenge::RVNG_SEEK_SET);

  for (int i=0; i<numZoneB; ++i) {
    pos=input->tell();
    f.str("");
    f << "Chart-B" << i << ":";

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+73, librevenge::RVNG_SEEK_SET);
  }
  if (input->tell()!=endPos) {
    MWAW_DEBUG_MSG(("WingzParser::readChartData: find some extra data\n"));
    ascii().addPos(input->tell());
    ascii().addNote("Chart-end:###");
  }
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// decode an encrypted file
////////////////////////////////////////////////////////////
bool WingzParser::decodeEncrypted()
{
  MWAWInputStreamPtr input = getInput();
  long length=input->size();
  if (length<=13) {
    MWAW_DEBUG_MSG(("WingzParser::decodeEncrypted: the file seems too short\n"));
    return false;
  }
  input->seek(0, librevenge::RVNG_SEEK_SET);
  unsigned long read;
  uint8_t const *data=input->read(size_t(length), read);
  if (!data || length!=long(read)) {
    MWAW_DEBUG_MSG(("WingzParser::decodeEncrypted: can not read the buffer\n"));
    return false;
  }
  uint8_t *buffer=new uint8_t[length];
  if (!buffer) {
    MWAW_DEBUG_MSG(("WingzParser::decodeEncrypted: can not allocate a buffer\n"));
    return false;
  }

  for (int i=0; i<12; i++) buffer[i]=data[i];
  buffer[12]=0;
  int delta=0;
  for (long i=13; i<length; ++i) {
    uint8_t const codeString[]= { 0x53, 0x66, 0xA5, 0x35, 0x5A, 0xAA, 0x55, 0xE3 };
    uint8_t v=uint8_t((codeString[(delta&7)]+delta)&0xFF);
    buffer[i]=(data[i]^v);
    delta++;
  }

  shared_ptr<librevenge::RVNGInputStream> newInput
  (new librevenge::RVNGStringStream(buffer, (unsigned int)length));
  getParserState()->m_input.reset(new MWAWInputStream(newInput, false));
  delete [] buffer;
  return true;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool WingzParser::checkHeader(MWAWHeader *header, bool /*strict*/)
{
  *m_state = WingzParserInternal::State();

  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork())
    return false;

  int const headerSize=13;
  if (!input->checkPosition(headerSize)) {
    MWAW_DEBUG_MSG(("WingzParser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0,librevenge::RVNG_SEEK_SET);
  int values[4];
  for (int i=0; i<4; ++i)
    values[i]=(int) input->readULong(2);
  bool isWingz=true;
  if (values[0]==0x574e && values[1]==0x475a && values[2]==0x575a && values[3]==0x5353) // WNGZWZSS
    isWingz=true;
  else if (values[0]==0x4241 && values[1]==0x545F && values[2]==0x4254 && values[3]==0x5353) // BAT_BTSS
    isWingz=false;
  else
    return false;
  setVersion(isWingz ? 2 : 1);
  input->setReadInverted(true);
  libmwaw::DebugStream f;
  f << "FileHeader:";
  std::string name(""); // 0110: version number
  for (int i=0; i<4; ++i)
    name += (char) input->readULong(1);
  f << "vers=" << name << ",";
  int val=(int) input->readLong(1);
  if (val==1) { // SgB
    MWAW_DEBUG_MSG(("WingzParser::checkHeader: Find an encrypted file...\n"));
    m_state->m_encrypted=true;
  }
  else if (val) {
    MWAW_DEBUG_MSG(("WingzParser::checkHeader: Find unknown encryped flag...\n"));
    return false;
  }
  ascii().addPos(0);
  ascii().addNote(f.str().c_str());
  if (header)
    header->reset(isWingz ? MWAWDocument::MWAW_T_WINGZ : MWAWDocument::MWAW_T_CLARISRESOLVE, 1, MWAWDocument::MWAW_K_SPREADSHEET);
  input->seek(12,librevenge::RVNG_SEEK_SET);
  input->setReadInverted(false);
  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool WingzParser::readPrintInfo()
{
  MWAWInputStreamPtr input = getInput();
  int const vers=version();
  long pos = input->tell();
  int type=(int) input->readULong(1);
  if (type!=0x10) return false;
  int val=(int) input->readULong(1);
  int dSz=(int) input->readULong(2);
  int id=vers==1 ? 0 : (int) input->readULong(2);
  int expectedSize=vers==1 ? 0x8a : 0x7c;
  long endPos=pos+(vers==1 ? 4+0x8a : 20+0x7c);
  if (dSz!=expectedSize || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("WingzParser::readPrintInfo: the header seem bad\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(PrintInfo):";
  if (val!=0x80) f << "fl=" << std::hex << val << std::dec << ",";
  if (id) f << "id=" << id << ",";
  for (int i=0; i<3; ++i) {
    int dim[2];
    for (int j=0; j<2; ++j) dim[j]=(int) input->readULong(2);
    if (i==2)
      f << "unit=" << dim[0] << "x" << dim[1] << ",";
    else
      f << "dim" << i << "=" << dim[0] << "x" << dim[1] << ",";
  }
  // 3 small number 0x78,4,6|7
  for (int i=0; i<3; ++i) {
    val=(int) input->readULong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  // print info
  libmwaw::PrinterInfo info;
  input->setReadInverted(false);
  bool ok=info.read(input);
  input->setReadInverted(true);
  if (!ok) return false;
  f << info;

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

  getPageSpan().setMarginTop(lTopMargin.y()/72.0);
  getPageSpan().setMarginBottom(botMarg/72.0);
  getPageSpan().setMarginLeft(lTopMargin.x()/72.0);
  getPageSpan().setMarginRight(rightMarg/72.0);
  getPageSpan().setFormLength(paperSize.y()/72.);
  getPageSpan().setFormWidth(paperSize.x()/72.);

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// send spreadsheet
////////////////////////////////////////////////////////////
bool WingzParser::sendSpreadsheet()
{
  MWAWSpreadsheetListenerPtr listener=getSpreadsheetListener();
  MWAWInputStreamPtr &input= getInput();
  if (!listener) {
    MWAW_DEBUG_MSG(("WingzParser::sendSpreadsheet: I can not find the listener\n"));
    return false;
  }
  WingzParserInternal::Spreadsheet &sheet = m_state->m_spreadsheet;
  size_t numCell = sheet.m_cells.size();
  listener->openSheet(sheet.convertInPoint(sheet.m_widthCols), librevenge::RVNG_POINT, sheet.m_name);
  // DOME: sendPageGraphics();

  int prevRow = -1;
  for (size_t i = 0; i < numCell; i++) {
    WingzParserInternal::Cell cell= sheet.m_cells[i];
    if (cell.position()[1] != prevRow) {
      while (cell.position()[1] > prevRow) {
        if (prevRow != -1)
          listener->closeSheetRow();
        prevRow++;
        listener->openSheetRow(sheet.getRowHeight(prevRow), librevenge::RVNG_POINT);
      }
    }
    sheet.update(cell);
    listener->openSheetCell(cell, cell.m_content);
    if (cell.m_content.m_textEntry.valid()) {
      listener->setFont(cell.getFont());
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

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
