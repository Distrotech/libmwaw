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

#include <librevenge/librevenge.h>

#include "MWAWCell.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSpreadsheetEncoder.hxx"
#include "MWAWSpreadsheetListener.hxx"
#include "MWAWTextListener.hxx"

#include "RagTimeParser.hxx"

#include "RagTimeSpreadsheet.hxx"

/** Internal: the structures of a RagTimeSpreadsheet */
namespace RagTimeSpreadsheetInternal
{
//! Internal: a cell of a RagTimeSpreadsheet
struct Cell : public MWAWCell {
  //! constructor
  Cell(Vec2i pos=Vec2i(0,0)) : MWAWCell(), m_content()
  {
    setPosition(pos);
  }
  //! the cell content
  MWAWCellContent m_content;
};


//! Internal: a spreadsheet's zone of a RagTimeSpreadsheet
struct Spreadsheet {
  //! constructor
  Spreadsheet() : m_widthDefault(74), m_widthCols(), m_heightDefault(12), m_heightRows(),
    m_cells(), m_name("Sheet0"), m_isSent(false)
  {
  }
  //! returns the row size in point
  float getRowHeight(int row) const
  {
    if (row>=0&&row<(int) m_heightRows.size())
      return m_heightRows[size_t(row)];
    return m_heightDefault;
  }
  /** returns the columns dimension in point: TO DO */
  std::vector<float> getColumnsWidth() const
  {
    size_t numCols=size_t(getRightBottomPosition()[0]+1);
    std::vector<float> res(numCols, 72.f);
    if (m_widthCols.size()<numCols)
      numCols=m_widthCols.size();
    for (size_t i = 0; i < numCols; i++) {
      if (m_widthCols[i] > 0)
        res[i] = float(m_widthCols[i]);
    }
    return res;

  }
  /** returns the spreadsheet dimension */
  Vec2i getRightBottomPosition() const
  {
    Vec2i res(0,0);
    for (size_t i=0; i<m_cells.size(); ++i) {
      if (m_cells[i].position()[0] >= res[0])
        res[0]=m_cells[i].position()[0]+1;
      if (m_cells[i].position()[1] >= res[1])
        res[1]=m_cells[i].position()[1]+1;
    }
    return res;
  }
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
  /** the sheet name */
  std::string m_name;
  /** true if the sheet is sent to the listener */
  mutable bool m_isSent;
};

////////////////////////////////////////
//! Internal: the state of a RagTimeSpreadsheet
struct State {
  //! constructor
  State() : m_version(-1), m_idSpreadsheetMap()
  {
  }

  //! the file version
  mutable int m_version;
  //! map id -> spreadsheet
  std::map<int, shared_ptr<Spreadsheet> > m_idSpreadsheetMap;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
RagTimeSpreadsheet::RagTimeSpreadsheet(RagTimeParser &parser) :
  m_parserState(parser.getParserState()), m_state(new RagTimeSpreadsheetInternal::State), m_mainParser(&parser)
{
}

RagTimeSpreadsheet::~RagTimeSpreadsheet()
{ }

int RagTimeSpreadsheet::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}


////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// spreadsheet zone
////////////////////////////////////////////////////////////
bool RagTimeSpreadsheet::readSpreadsheet(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+0x66)) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheet: the position seems bad\n"));
    return false;
  }
  if (version()<2) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheet: must not be called for v1-2... file\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(SpreadsheetZone):";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  if (dSz<0x62 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheet: the size seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  int val;
  for (int i=0; i<6; ++i) { // f0=0, f1=4|6|44, f2=1-8, f3=1-f, f4=1-5, f5=1-3
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  for (int i=0; i<6; ++i) // g0~40, g1=g2=2, g3~16, g4=[b-10], g5=[8-c]|800d
    f << "g" << i << "=" << double(input->readLong(4))/65536. << ",";
  long zoneBegin[11];
  zoneBegin[10]=endPos;
  for (int i=0; i<10; ++i) {
    zoneBegin[i]=(long) input->readULong(4);
    if (!zoneBegin[i]) continue;
    f << "zone" << i << "=" << std::hex << pos+2+zoneBegin[i] << std::dec << ",";
    if (pos+2+zoneBegin[i]>endPos) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheet: the zone %d seems bad\n",i));
      zoneBegin[i]=0;
      f << "###";
      continue;
    }
    zoneBegin[i]+=pos+2;
  }
  f << "fl?=["; // or some big number
  for (int i=0; i<8; ++i) {
    val=(int) input->readULong(2);
    if (val)
      f << std::hex << val << std::dec << ",";
    else
      f << "_,";
  }
  f << "],";
  for (int i=0; i<3; ++i) { // h0=0-4, h1=h2=0
    val=(int) input->readULong(2);
    if (val)
      f << "h" << i << "=" << val << ",";
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  // now read the different zone, first set the endPosition
  for (int i=9; i>=0; --i) {
    if (zoneBegin[i]==0)
      zoneBegin[i]=zoneBegin[i+1];
    else if (zoneBegin[i]>zoneBegin[i+1]) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheet: the zone %d seems bad(II)\n",i));
      zoneBegin[i]=zoneBegin[i+1];
    }
  }
  for (int i=0; i<10; ++i) {
    if (zoneBegin[i+1]<=zoneBegin[i]) continue;
    f.str("");
    f << "SpreadsheetZone-" << i << ":";
    // SpreadsheetZone-3: sz+[32bytes]+(N+1)*12
    // SpreadsheetZone-9: sz+N+N*14
    ascFile.addPos(zoneBegin[i]);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool RagTimeSpreadsheet::readSpreadsheetV2(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+6)) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetV2: the position seems bad\n"));
    return false;
  }
  if (version()>=2) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetV2: must not be called for v3... file\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  f << "Entries(SpreadsheetZone):";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  if (dSz<4 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetV2: the size seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  ascFile.addPos(endPos);
  ascFile.addNote("_");
  long zonesList[2]= {0,endPos};
  for (int i=0; i<2; ++i) {
    long ptr=pos+6+(long) input->readULong(2);
    f << "ptr[" << i << "]=" << std::hex << ptr << std::dec << ",";
    if (ptr>=endPos) {
      f << "###";
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetV2: the zone begin seems bad%d\n", i));
      continue;
    }
    zonesList[i]=ptr;
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  shared_ptr<RagTimeSpreadsheetInternal::Spreadsheet> sheet(new RagTimeSpreadsheetInternal::Spreadsheet);

  MWAWEntry cells;
  cells.setBegin(zonesList[0]);
  cells.setEnd(zonesList[1]);
  if (readSpreadsheetCellsV2(cells, *sheet))
    m_state->m_idSpreadsheetMap[entry.id()]=sheet;

  MWAWEntry extra;
  extra.setBegin(zonesList[1]);
  extra.setEnd(endPos);
  return readSpreadsheetExtraV2(extra, *sheet);
}

bool RagTimeSpreadsheet::readSpreadsheetCellsV2(MWAWEntry &entry, RagTimeSpreadsheetInternal::Spreadsheet &sheet)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=entry.begin();
  long endPos=entry.end();
  if (pos<=0 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellsV2: the position seems bad\n"));
    return false;
  }

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  ascFile.addPos(endPos);
  ascFile.addNote("_");

  int n=0;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  Vec2i cellPos(-1,0);
  while (!input->isEnd()) {
    pos=input->tell();
    if (pos+2>endPos) break;
    int col=int(input->readLong(1))-1;
    if (col<=cellPos[0]) ++cellPos[1];
    cellPos[0]=col;
    int dSz=(int) input->readULong(1);
    long zEndPos=pos+6+dSz;
    RagTimeSpreadsheetInternal::Cell cell;
    cell.setPosition(cellPos);
    f.str("");
    f << "Entries(SpreadsheetCell)[" << n++ << "]:";
    if (zEndPos>endPos) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellsV2: problem reading some cells\n"));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      return false;
    }
    else if (!readSpreadsheetCellV2(cell, zEndPos) || cellPos[0]<0||cellPos[1]<0) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellsV2: small pb reading a cell\n"));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    else
      sheet.m_cells.push_back(cell);
    if ((dSz%2)==1) ++zEndPos;
    input->seek(zEndPos, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

bool RagTimeSpreadsheet::readSpreadsheetCellV2(RagTimeSpreadsheetInternal::Cell &cell, long endPos)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=input->tell();
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;

  f << "Entries(SpreadsheetCell)[C" << cell.position() << "]:";
  if (pos+4>endPos) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellV2: the zone seems too short\n"));
    f << "###";
    ascFile.addPos(pos-2);
    ascFile.addNote(f.str().c_str());
    return false;
  }

  int val=(int) input->readULong(1);
  int type=(val>>4);
  MWAWCell::Format format;
  switch (type) {
  case 0:
    format.m_format=MWAWCell::F_NUMBER;
    f << "empty,";
    break;
  case 3:
    format.m_format=MWAWCell::F_NUMBER;
    format.m_numberFormat=MWAWCell::F_NUMBER_GENERIC;
    f << "number,";
    break;
  case 9:
    format.m_format=MWAWCell::F_TEXT;
    f << "text,";
    break;
  default:
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellV2: find unknown type %d\n", type));
    f << "##type=" << type << ",";
    break;
  }
  bool hasFont=(val&1);
  bool hasAlign=(val&2);
  bool hasFormula=(val&4);
  bool hasFlag80=(val&8);
  if (hasFormula)
    f << "formula,";
  if (val&8)
    f << "flag80,";

  val=(int) input->readULong(1);
  bool hasNumberFormat=false;
  if (val&0x80) {
    f << "digits[set],";
    val&=0x7F;
    hasNumberFormat=true;
  }
  // fl0&30: no change
  if (val) f << "fl0=" << std::hex << val << std::dec << ",";
  val=(int) input->readULong(1);
  if (val&0xF0) { // checkme
    int borders=0;
    f << "bord=";
    if (val&0x10) {
      borders|=libmwaw::LeftBit;
      f << "L";
    }
    if (val&0x20) {
      borders|=libmwaw::RightBit;
      f << "R";
    }
    if (val&0x40) {
      borders|=libmwaw::TopBit;
      f << "T";
    }
    if (val&0x80) {
      borders|=libmwaw::BottomBit;
      f << "B";
    }
    f << ",";
    cell.setBorders(borders, MWAWBorder());
  }
  if (val&0xF) f << "fl1=" << std::hex << (val&0xf) << std::dec << ",";
  val=(int) input->readULong(1);
  if (val) f << "fl2=" << std::hex << val << std::dec << ",";
  long actPos;
  if (hasNumberFormat) {
    val=(int) input->readULong(1);
    actPos=input->tell();
    bool ok=true, hasDigits=true;
    switch (val>>5) {
    case 1: // unknown
      f << "type1,";
      break;
    case 3:
      f << "currency,";
      format.m_numberFormat=MWAWCell::F_NUMBER_CURRENCY;
      break;
    case 6:
      f << "percent,";
      format.m_numberFormat=MWAWCell::F_NUMBER_PERCENT;
      break;
    case 4:
      f << "scientific,";
      format.m_numberFormat=MWAWCell::F_NUMBER_SCIENTIFIC;
      break;
    case 2:
      f << "decimal,";
      format.m_numberFormat=MWAWCell::F_NUMBER_DECIMAL;
      break;
      break;
    case 0:
      hasDigits=false;
      break;
    default:
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellV2: problem read numbering flags\n"));
      f << "##type=" << (val>>5) << ",";
      hasDigits=ok=false;
      break;
    }
    if (ok) {
      val &= 0x1F;
      if (hasDigits && actPos+1>endPos) {
        MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellV2: can not read a digit\n"));
        f << "##digits,";
        ok=false;
      }
      else if (hasDigits) {
        int digits=(int) input->readULong(1);
        if (digits&0xC0) {
          f << "digits[high]=" << (digits>>6) << ",";
          digits &= 0x3f;
        }
        format.m_digits=digits;
        f << "digits=" << digits << ",";
      }
    }
    else
      f << "##";
    if (val)
      f << "fl3=" << std::hex << val << std::dec << ",";
    if (!ok) {
      ascFile.addPos(pos-2);
      ascFile.addNote(f.str().c_str());
      return true;
    }
  }
  if (hasFont) {
    if (input->tell()+4>endPos) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellV2: problem reading font format\n"));
      f << "##font,";
      ascFile.addPos(pos-2);
      ascFile.addNote(f.str().c_str());
      return true;
    }
    MWAWFont font;
    int size=(int) input->readULong(1);
    int flag = (int) input->readULong(1);
    uint32_t flags=0;
    if (flag&0x1) flags |= MWAWFont::boldBit;
    if (flag&0x2) flags |= MWAWFont::italicBit;
    if (flag&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (flag&0x8) flags |= MWAWFont::embossBit;
    if (flag&0x10) flags |= MWAWFont::shadowBit;
    if (flag&0x20) font.setDeltaLetterSpacing(-1);
    if (flag&0x40) font.setDeltaLetterSpacing(1);
    if (flag&0x80) font.set(MWAWFont::Script::super100());
    if (size&0x80) {
      font.set(MWAWFont::Script::sub100());
      size&=0x7f;
    }
    font.setSize((float)size);
    font.setFlags(flags);
    font.setId(m_mainParser->getFontId((int) input->readULong(2)));
    cell.setFont(font);
    f << "font=[" << font.getDebugString(m_parserState->m_fontConverter) << "],";
  }
  if (hasFlag80) {
    actPos=input->tell();
    int fSz=(int) input->readULong(1);
    if (!fSz||actPos+1+fSz>endPos) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellV2: can not read a flag 80\n"));
      f << "###flag80,";
      ascFile.addPos(pos-2);
      ascFile.addNote(f.str().c_str());
      return true;
    }
    // find some zone with size 22 as 59cb050e030304058380fd04050e86050d030104030d
    ascFile.addDelimiter(input->tell(),'|');
    input->seek(actPos+1+fSz, librevenge::RVNG_SEEK_SET);
    if (actPos+1+fSz!=endPos)
      ascFile.addDelimiter(input->tell(),'|');
  }
  val= hasAlign ? (int) input->readULong(1) : 0;
  int align= val&7;
  switch (align) {
  case 0:
    break;
  case 2:
    cell.setHAlignement(MWAWCell::HALIGN_LEFT);
    f << "left,";
    break;
  case 3:
    cell.setHAlignement(MWAWCell::HALIGN_CENTER);
    f << "center,";
    break;
  case 4:
    cell.setHAlignement(MWAWCell::HALIGN_RIGHT);
    f << "right,";
    break;
  case 5: // full(repeat)
    cell.setHAlignement(MWAWCell::HALIGN_LEFT);
    f << "repeat,";
    break;
  default:
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellV2: find unknown alignment\n"));
    f << "##align=" << align << ",";
    break;
  }
  val&=0xF8;
  if (hasFormula) {
    // TODO: parse the formula
    actPos=input->tell();
    int fSz=(int) input->readULong(1);
    if (!fSz||actPos+1+fSz>endPos) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellV2: can not read a formula\n"));
      f << "###formula,";
      ascFile.addPos(pos-2);
      ascFile.addNote(f.str().c_str());
      return true;
    }
    ascFile.addDelimiter(input->tell(),'|');
    input->seek(actPos+1+fSz, librevenge::RVNG_SEEK_SET);
    if (actPos+1+fSz!=endPos)
      ascFile.addDelimiter(input->tell(),'|');
  }

  actPos=input->tell();
  MWAWCellContent &content=cell.m_content;
  switch (type) {
  case 0:
    if (actPos!=endPos) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellV2: something look bad\n"));
      f << "###data";
      break;
    }
    break;
  case 3: {
    if (actPos+10!=endPos) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellV2: can not read a number\n"));
      f << "###number";
      break;
    }
    if (format.m_format==MWAWCell::F_UNKNOWN)
      format.m_format=MWAWCell::F_NUMBER;
    if (content.m_contentType!=MWAWCellContent::C_FORMULA)
      content.m_contentType=MWAWCellContent::C_NUMBER;
    double res;
    bool isNan;
    if (!input->readDouble10(res, isNan))
      f << "#value,";
    else {
      content.setValue(res);
      f << res << ",";
    }
    break;
  }
  case 9: {
    int sSz=(int) input->readULong(1);
    if (actPos+1+sSz!=endPos) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellV2: can not read a text\n"));
      f << "###text";
      break;
    }
    if (format.m_format==MWAWCell::F_UNKNOWN)
      format.m_format=MWAWCell::F_TEXT;
    if (content.m_contentType!=MWAWCellContent::C_FORMULA)
      content.m_contentType=MWAWCellContent::C_TEXT;
    content.m_textEntry.setBegin(input->tell());
    content.m_textEntry.setLength(sSz);
    std::string text("");
    for (int i=0; i<sSz; ++i) text+=(char) input->readULong(1);
    f << text << ",";
    break;
  }
  default:
    break;
  }
  cell.setFormat(format);
  actPos=input->tell();
  if (actPos!=endPos)
    ascFile.addDelimiter(actPos,'|');
  ascFile.addPos(pos-2);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool RagTimeSpreadsheet::readSpreadsheetExtraV2(MWAWEntry &entry, RagTimeSpreadsheetInternal::Spreadsheet &sheet)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=entry.begin();
  long endPos=entry.end();
  if (pos<=0 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetExtraV2: the position seems bad\n"));
    return false;
  }

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  ascFile.addPos(endPos);
  ascFile.addNote("_");

  for (int i=0; i<2; ++i) {
    pos=input->tell();
    f.str("");
    static char const *(what[])= {"SpreadsheetRow", "SpreadsheetCol"};
    f << "Entries(" << what[i] << "):";
    int n=(int) input->readULong(2);
    f << "N=" << n << ",";
    static int const dataSize[]= {20,14};
    if (pos+2+dataSize[i]*n>endPos) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetExtraV2: problem reading some spreadsheetZone Col/Row field\n"));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      return false;
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    int prevDim=0;
    std::vector<float> &dims=i==0 ? sheet.m_heightRows : sheet.m_widthCols;
    for (int j=0; j<n; ++j) {
      pos=input->tell();
      f.str("");
      f << what[i] << "-" << j << ":";
      int val;
      for (int k=0; k<7; ++k) { // f0=0|80, f1=0|8|10, f4=0|20|60|61, f5=2|42|82,f6=1|3
        val=(int) input->readULong(1);
        if (val) f << "f" << k << "=" << std::hex << val << std::dec << ",";
      }
      f << "font[";
      f << "sz=" << input->readLong(2) << ",";
      val=(int) input->readULong(1);
      if (val) f << "fl=" << std::hex << val << std::dec << ",";
      f << "id=" << input->readULong(2) << ",";
      f << "],";
      int dim=(int) input->readULong(2);
      if (dim<prevDim) {
        f << "###dim, ";
        MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetExtraV2: problem reading some position\n"));
      }
      else {
        dims.push_back(float(dim-prevDim));
        prevDim=dim;
      }
      if (i==0) {
        f << "height=" << input->readULong(2) << ",";
        // an increasing number, maybe related to the first cell position
        f << "pos?=" << std::hex << input->readULong(4) << std::dec << ",";
      }
      input->seek(pos+dataSize[i], librevenge::RVNG_SEEK_SET);
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
  }

  /* finally something like
     86000000200201000c000014003c00030c090000
     or
     86000000204201000a000003004000060d0a000000000ce5410000010000000000000000688f688f688f0000000000000000688f688f688f000000000000
     font + ?
  */
  ascFile.addPos(input->tell());
  ascFile.addNote("SpreadsheetZone[end]:");
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// read a zone of spreadsheet
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// send data to the listener
////////////////////////////////////////////////////////////
bool RagTimeSpreadsheet::send(RagTimeSpreadsheetInternal::Spreadsheet &sheet, MWAWSpreadsheetListenerPtr listener)
{
  if (!listener) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::send: I can not find the listener\n"));
    return false;
  }
  sheet.m_isSent=true;
  size_t numCell = sheet.m_cells.size();
  listener->openSheet(sheet.getColumnsWidth(), librevenge::RVNG_POINT, sheet.m_name);

  MWAWInputStreamPtr &input=m_parserState->m_input;
  int prevRow = -1;
  for (size_t i = 0; i < numCell; i++) {
    RagTimeSpreadsheetInternal::Cell cell= sheet.m_cells[i];
    if (cell.position()[1] != prevRow) {
      while (cell.position()[1] > prevRow) {
        if (prevRow != -1)
          listener->closeSheetRow();
        listener->openSheetRow(sheet.getRowHeight(++prevRow), librevenge::RVNG_POINT);
      }
    }
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

bool RagTimeSpreadsheet::send(int zId, MWAWPosition const &pos)
{
  MWAWListenerPtr listener=m_parserState->getMainListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::send: I can not find the listener\n"));
    return false;
  }
  if (m_state->m_idSpreadsheetMap.find(zId)==m_state->m_idSpreadsheetMap.end() ||
      !m_state->m_idSpreadsheetMap.find(zId)->second) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::send: can not find the spreadsheet %d\n", zId));
    return false;
  }
  RagTimeSpreadsheetInternal::Spreadsheet &sheet=*m_state->m_idSpreadsheetMap.find(zId)->second;
  Box2f box=Box2f(Vec2f(0,0), pos.size());
  MWAWSpreadsheetEncoder spreadsheetEncoder;
  MWAWSpreadsheetListenerPtr spreadsheetListener(new MWAWSpreadsheetListener(*m_parserState, box, &spreadsheetEncoder));
  spreadsheetListener->startDocument();
  send(sheet, spreadsheetListener);
  spreadsheetListener->endDocument();
  librevenge::RVNGBinaryData data;
  std::string mime;
  if (spreadsheetEncoder.getBinaryResult(data,mime))
    listener->insertPicture(pos, data, mime);
  return true;
}

void RagTimeSpreadsheet::flushExtra()
{
  MWAWListenerPtr listener=m_parserState->getMainListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::flushExtra: can not find the listener\n"));
    return;
  }
  std::map<int, shared_ptr<RagTimeSpreadsheetInternal::Spreadsheet> >::const_iterator it;
  for (it=m_state->m_idSpreadsheetMap.begin(); it!=m_state->m_idSpreadsheetMap.end(); ++it) {
    if (!it->second) continue;
    RagTimeSpreadsheetInternal::Spreadsheet const &zone=*it->second;
    if (zone.m_isSent) continue;
    static bool first=true;
    if (first) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::flushExtra: find some unsend zone\n"));
      first=false;
    }
    MWAWPosition pos(Vec2f(0,0), Vec2f(200,200), librevenge::RVNG_POINT);
    pos.m_anchorTo=MWAWPosition::Char;
    send(it->first, pos);
    listener->insertEOL();
  }
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
