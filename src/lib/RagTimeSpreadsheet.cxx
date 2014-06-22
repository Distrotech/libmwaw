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
#include "RagTimeStruct.hxx"

#include "RagTimeSpreadsheet.hxx"

/** Internal: the structures of a RagTimeSpreadsheet */
namespace RagTimeSpreadsheetInternal
{
struct Cell;
//! Internal: date/time format of a RagTimeSpreadsheet
struct DateTime {
  //! constructor
  DateTime() : m_isDate(true), m_DTFormat("")
  {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, DateTime const &dt)
  {
    o << dt.m_DTFormat;
    if (!dt.m_isDate) o << "[time],";
    else o << ",";
    return o;
  }
  //! true if this is a date field
  bool m_isDate;
  //! the date time format
  std::string m_DTFormat;
};

//! Internal: cell number format of a RagTimeSpreadsheet (SpVa block)
struct CellFormat {
  //! constructor
  CellFormat() : m_numeric(), m_dateTime(), m_align(MWAWCell::HALIGN_DEFAULT), m_rotation(0), m_flags(0), m_extra("")
  {
  }
  //! update the cell format if needed
  void update(Cell &cell) const;
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, CellFormat const &form)
  {
    if (form.m_rotation) o << "rot=" << form.m_rotation << ",";
    if (form.m_flags&2) o << "protect[all],";
    if (form.m_flags&4) o << "protect[format,formula],";
    if (form.m_flags&0x8) o << "invisible,";
    if (form.m_flags&0x20) o << "zero[dontshow],";
    if (form.m_flags&0x40) o << "prec[variable],"; // precision depend on display
    if (form.m_flags&0x80) o << "print[no],";

    if (form.m_flags&0xFF11) o << "fl=" << std::hex << (form.m_flags&0xFF11) << std::dec << ",";
    o << form.m_extra;
    return o;
  }
  //! the numeric format
  MWAWCell::Format m_numeric;
  //! the date/time format
  DateTime m_dateTime;
  //! the cell's alignment
  MWAWCell::HorizontalAlignment m_align;
  //! the rotation angle
  int m_rotation;
  //! some flags
  int m_flags;
  //! extra data (for debugging)
  std::string m_extra;
};

//! Internal: cell border of a RagTimeSpreadsheet (SpVa block)
struct CellBorder {
  //! constructor
  CellBorder() : m_extra("")
  {
    m_borders[0]=m_borders[1]=MWAWBorder();
  }
  //! returns true if the cell has some border
  bool hasBorders() const
  {
    return !m_borders[0].isEmpty() && !m_borders[1].isEmpty();
  }
  //! update the cell border if need
  void update(Cell &cell) const;
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, CellBorder const &border)
  {
    for (int i=0; i<2; ++i) {
      if (border.m_borders[i]==MWAWBorder()) continue;
      o << (i==0 ? "top=" : "left=") << border.m_borders[i] << ",";
    }
    o << border.m_extra;
    return o;
  }
  //! the top and left border
  MWAWBorder m_borders[2];
  //! extra data
  std::string m_extra;
};

//! Internal: extra cell format of a RagTimeSpreadsheet (SpCe block)
struct CellExtra {
  //! constructor
  CellExtra():m_isTransparent(false), m_color(MWAWColor::white()), m_extra("")
  {
  }
  //! update the cell color if need
  void update(Cell &cell) const;
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, CellExtra const &st)
  {
    if (st.m_isTransparent) o << "noColor,";
    else if (!st.m_color.isWhite()) o << "color[back]=" << st.m_color << ",";
    o << st.m_extra;
    return o;
  }
  //! true if the cell is transparent
  bool m_isTransparent;
  //! the background color
  MWAWColor m_color;
  //! extra data
  std::string m_extra;
};

//! Internal: header of a complex block of a RagTimeSpreadsheet
struct ComplexBlock {
  //! constructor
  ComplexBlock() : m_zones(), m_intList()
  {
  }
  //! a small zone of a ComplexBlock
  struct Zone {
    //! constructor
    Zone() : m_pos(0)
    {
      for (int i=0; i<3; ++i) m_data[i]=0;
    }
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Zone const &z)
    {
      o << "pos=" << std::hex << z.m_pos << std::dec << ",";
      for (int i=0; i<3; ++i) {
        if (!z.m_data[i]) continue;
        char const *(wh[])= {"firstVal","N", "Nsub"};
        o << wh[i] << "=" << z.m_data[i] << ",";
      }
      return o;
    }
    //! the zone position
    long m_pos;
    //! three unknown int
    int m_data[3];
  };
  //! the list of zone
  std::vector<Zone> m_zones;
  //! a list of unknown counter
  std::vector<int> m_intList;
};

//! Internal: a cell of a RagTimeSpreadsheet
struct Cell : public MWAWCell {
  //! constructor
  Cell(Vec2i pos=Vec2i(0,0)) : MWAWCell(), m_content(), m_textEntry(), m_rotation(0)
  {
    setPosition(pos);
  }
  //! test if we can use or not the formula
  bool validateFormula()
  {
    if (m_content.m_formula.empty()) return false;
    for (size_t i=0; i<m_content.m_formula.size(); ++i) {
      bool ok=true;
      MWAWCellContent::FormulaInstruction &instr=m_content.m_formula[i];
      // fixme: cell to other spreadsheet
      if (instr.m_type==MWAWCellContent::FormulaInstruction::F_Cell ||
          instr.m_type==MWAWCellContent::FormulaInstruction::F_CellList)
        ok=instr.m_sheet.empty();
      // fixme: replace operator or by function
      else if (instr.m_type==MWAWCellContent::FormulaInstruction::F_Function)
        ok=(instr.m_content!="Or"&&instr.m_content!="And"&&instr.m_content!="Not");
      if (!ok) {
        m_content.m_formula.resize(0);
        return false;
      }
    }
    return true;
  }
  //! the cell content
  MWAWCellContent m_content;
  //! the text entry if the cell is a zone of text zone
  MWAWEntry m_textEntry;
  //! the content's rotation angle
  int m_rotation;
};

void CellFormat::update(Cell &cell) const
{
  MWAWCell::Format format=cell.getFormat();
  if (format.m_format==MWAWCell::F_NUMBER)
    format=m_numeric;
  else if (format.m_format==MWAWCell::F_DATE) {
    format.m_DTFormat=m_dateTime.m_DTFormat;
    if (!m_dateTime.m_isDate)
      format.m_format=MWAWCell::F_TIME;
  }
  cell.setFormat(format);
  cell.setHAlignment(m_align);
  if (m_flags&6) cell.setProtected(true);
  cell.m_rotation=m_rotation;
}

void CellBorder::update(Cell &cell) const
{
  if (!m_borders[0].isEmpty()) cell.setBorders(libmwaw::Top, m_borders[0]);
  if (!m_borders[1].isEmpty()) cell.setBorders(libmwaw::Left, m_borders[1]);
}

void CellExtra::update(Cell &cell) const
{
  if (!m_isTransparent) cell.setBackgroundColor(m_color);
}

//! Internal: a spreadsheet's zone of a RagTimeSpreadsheet
struct Spreadsheet {
  //! a map a cell sorted by row
  typedef std::map<Vec2i,Cell,Vec2i::PosSizeLtY> Map;
  //! constructor
  Spreadsheet() : m_rows(0), m_columns(0), m_widthDefault(72), m_widthCols(), m_heightDefault(12), m_heightRows(),
    m_cellsBegin(0), m_cellsMap(), m_rowPositionsList(), m_name("Sheet0"), m_isSent(false)
  {
  }
  //! returns the row size in point
  float getRowHeight(int row) const
  {
    if (row>=0&&row<(int) m_heightRows.size()&&m_heightRows[size_t(row)]>0)
      return m_heightRows[size_t(row)];
    return m_heightDefault;
  }
  /** returns the columns dimension in point */
  std::vector<float> getColumnsWidth() const
  {
    size_t numCols=size_t(getRightBottomPosition()[0]+1);
    std::vector<float> res(numCols, float(m_widthDefault));
    if (m_widthCols.size()<numCols)
      numCols=m_widthCols.size();
    for (size_t i=0; i<numCols; ++i) {
      if (m_widthCols[i] > 0)
        res[i] = m_widthCols[i];
    }
    return res;
  }
  /** returns the spreadsheet dimension */
  Vec2i getRightBottomPosition() const
  {
    Vec2i res(0,0);
    for (Map::const_iterator it=m_cellsMap.begin(); it!=m_cellsMap.begin(); ++it) {
      Cell const &cell=it->second;
      if (cell.position()[0] >= res[0])
        res[0]=cell.position()[0]+1;
      if (cell.position()[1] >= res[1])
        res[1]=cell.position()[1]+1;
    }
    return res;
  }
  /** the number of row */
  int m_rows;
  /** the number of col */
  int m_columns;
  /** the default column width */
  float m_widthDefault;
  /** the column size in points */
  std::vector<float> m_widthCols;
  /** the default row height */
  float m_heightDefault;
  /** the row height in points */
  std::vector<float> m_heightRows;
  /** the positions of the cells in the file */
  long m_cellsBegin;
  /** the map cell position to not empty cells */
  Map m_cellsMap;
  /** the positions of row in the file */
  std::vector<long> m_rowPositionsList;
  /** the sheet name */
  std::string m_name;
  /** true if the sheet is sent to the listener */
  mutable bool m_isSent;
};

////////////////////////////////////////
//! Internal: the state of a RagTimeSpreadsheet
struct State {
  //! constructor
  State() : m_version(-1), m_numericFormatList(), m_dateTimeList(), m_cellFontList(), m_cellFormatList(), m_cellBorderList(), m_cellExtraList(), m_cellDEList(), m_idSpreadsheetMap()
  {
  }

  //! the file version
  mutable int m_version;
  //! a list of numeric format
  std::vector<MWAWCell::Format> m_numericFormatList;
  //! a list dateTimeFormatId -> dateTimeFormat;
  std::vector<DateTime> m_dateTimeList;
  //! a list SpTe -> font
  std::vector<MWAWFont> m_cellFontList;
  //! a list SpVaId -> cellFormat
  std::vector<CellFormat> m_cellFormatList;
  //! a list SpBoId -> cellBorder
  std::vector<CellBorder> m_cellBorderList;
  //! a list SpCeId -> cellExtra
  std::vector<CellExtra> m_cellExtraList;
  //! a list SpDEId -> unknown data
  std::vector<std::string> m_cellDEList;
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
// resource
////////////////////////////////////////////////////////////
bool RagTimeSpreadsheet::getDateTimeFormat(int dtId, std::string &dtFormat) const
{
  if (dtId<0||dtId>=int(m_state->m_dateTimeList.size())) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::getDateTimeFormat: can not find the date/format %d\n", dtId));
    return false;
  }
  dtFormat=m_state->m_dateTimeList[size_t(dtId)].m_DTFormat;
  return !dtFormat.empty();
}

bool RagTimeSpreadsheet::readNumericFormat(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+0x26)) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readNumericFormat: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  f << "Entries(" << entry.type() << ")[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  int headerSz=(int) input->readULong(2);
  int fSz=(int) input->readULong(2);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (headerSz<0x20 || fSz<6 || dSz<headerSz+(N+1)*fSz || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readNumericFormat: the size seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  input->seek(pos+2+headerSz, librevenge::RVNG_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  std::set<long> posSet;
  posSet.insert(endPos);
  for (int i=0; i<=N; ++i) {
    pos=input->tell();
    f.str("");
    f << entry.type() << "-" << i << ":";
    int val=(int) input->readLong(2); // 0 (except last)
    if (val) f << "f0=" << val << ",";
    val=(int) input->readLong(2); // always 1 ?
    if (val!=1) f << "f1=" << val << ",";
    int fPos=(int) input->readULong(2);
    f << "pos[def]=" << std::hex << entry.begin()+2+fPos << std::dec << ",";
    posSet.insert(entry.begin()+2+fPos);
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  for (std::set<long>::const_iterator it=posSet.begin(); it!=posSet.end();) {
    pos=*(it++);
    if (pos>=endPos) break;
    long nextPos=it==posSet.end() ? endPos : *it;
    f.str("");
    f << entry.type() << "[def]:";
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    int depl[4];
    f << "depl=[";
    for (int i=0; i<4; ++i) {
      depl[i]=(int) input->readULong(1);
      if (depl[i]) f << depl[i] << ",";
      else f << "_,";
    }
    f << "],";
    if (entry.id()==0) {
      MWAWCell::Format format;
      format.m_format=MWAWCell::F_NUMBER;
      format.m_numberFormat=MWAWCell::F_NUMBER_DECIMAL;
      if (depl[0]<16 || pos+depl[0]>endPos) {
        MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readNumericFormat: the first zone seems bad\n"));
        f << "#header,";
      }
      else {
        int val=(int) input->readULong(1);
        if (val==1) {
          format.m_numberFormat=MWAWCell::F_NUMBER_GENERIC;
          f << "standart,";
        }
        else if (val) f << "#standart=" << val << ",";
        format.m_digits=(int) input->readULong(1);
        for (int i=0; i<10; ++i) {
          val=(int) input->readULong(1);
          if (val) f << "f" << i << "=" << val << ",";
        }
      }
      if (depl[0]<depl[1] && pos+depl[1]<endPos) {
        input->seek(pos+depl[0], librevenge::RVNG_SEEK_SET);
        ascFile.addDelimiter(input->tell(),'|');
        f << "format=[";
        for (int i=depl[0]; i<depl[1]; ++i) {
          int c=(int) input->readULong(1);
          switch (c) {
          case 1: // end
            break;
          case 6:
            f << "#";
            break;
          case 7: // int
            f << "0";
            break;
          case 9: // fix
            f << ",";
            break;
          case 0xa:
            format.m_thousandHasSeparator=true;
            f << " ";
            break;
          case 0xc:
            format.m_numberFormat=MWAWCell::F_NUMBER_SCIENTIFIC;
            f << "E+";
            break;
          case 0xe:
            format.m_numberFormat=MWAWCell::F_NUMBER_PERCENT;
            f << "%";
            break;
          case 0xf:
            format.m_numberFormat=MWAWCell::F_NUMBER_GENERIC;
            f << "[standart]";
            break;
          default:
            if (c<0x1f) {
              f << "#[" << std::hex << c << std::dec << "]";
            }
            else {
              format.m_numberFormat=MWAWCell::F_NUMBER_CURRENCY; // only probable
              f << char(c);
            }
          }
        }
        f << "],";
      }
      else {
        MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readNumericFormat: the compress format zone seems bad\n"));
        f << "#format,";
      }
      f << format;
      m_state->m_numericFormatList.push_back(format);
    }
    else if (depl[3]<8 || pos+depl[3]>endPos) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readNumericFormat: the first zone seems bad\n"));
      f << "#header,";
      m_state->m_dateTimeList.push_back(RagTimeSpreadsheetInternal::DateTime());
    }
    else {
      f << "depl2=[";
      for (int i=0; i<4; ++i) { // f0=small number, f1=f2=f3=0
        int val=(int) input->readULong(1);
        if (val) f << val << ",";
        else f << "_,";
      }
      f << "],";
      bool ok=true, isDate=false;;
      std::string dtFormat("");
      for (int i=8; i<depl[3]; ++i) {
        int c=(int) input->readULong(1);
        switch (c) {
        case 0: // probably next field...
          break;
        case 1: // end
          break;
        case 7:
          dtFormat+="%S";
          break;
        case 9:
          dtFormat+="%M";
          break;
        case 0xa:
          dtFormat+="%H";
          break;
        case 0xc:
        case 0xd: // with 2 digits
          dtFormat+="%d";
          isDate=true;
          break;
        case 0xe: // checkme
          dtFormat+="%a";
          isDate=true;
          break;
        case 0xf:
          dtFormat+="%A";
          isDate=true;
          break;
        case 0x10:
        case 0x11: // with 2 digits
          dtFormat+="%m";
          isDate=true;
          break;
        case 0x12:
          dtFormat+="%b";
          isDate=true;
          break;
        case 0x13:
          dtFormat+="%B";
          isDate=true;
          break;
        case 0x14:
          dtFormat+="%y";
          isDate=true;
          break;
        case 0x15:
          dtFormat+="%Y";
          isDate=true;
          break;
        default:
          if (c<0x1f) {
            std::stringstream s;
            s << "#[" << std::hex << c << std::dec << "]";
            dtFormat+=s.str();
            ok=false;
          }
          else
            dtFormat+=char(c);
        }
      }
      f << "format=[" << dtFormat << "],";
      if (ok) {
        RagTimeSpreadsheetInternal::DateTime dt;
        dt.m_DTFormat=dtFormat;
        dt.m_isDate=isDate;
        m_state->m_dateTimeList.push_back(dt);
      }
      else
        m_state->m_dateTimeList.push_back(RagTimeSpreadsheetInternal::DateTime());
    }
    // fixme: read the 3 other potential header zones
    headerSz=depl[3];
    if ((headerSz&1)) ++headerSz;
    input->seek(pos+headerSz, librevenge::RVNG_SEEK_SET);
    int cSz=(int) input->readULong(1);
    if (headerSz<4||cSz<=0||pos+headerSz+1+cSz!=nextPos) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readNumericFormat: can not read a format\n"));
      f << "###";
    }
    else {
      ascFile.addDelimiter(input->tell()-1,'|');
      std::string name("");
      for (int i=0; i<cSz; ++i) name += (char) input->readULong(1);
      f << name << ",";
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool RagTimeSpreadsheet::readResource(MWAWEntry &entry)
{
  if (entry.begin()<=0) return false;

  std::string const &type=entry.type();
  if (type.length()!=8 || type.compare(0,6,"rsrcSp")!=0)
    return false;
  if (entry.type()=="rsrcSpDI")
    return readRsrcSpDI(entry);
  if (entry.type()=="rsrcSpDo")
    return readRsrcSpDo(entry);

  RagTimeStruct::ResourceList::Type resType=RagTimeStruct::ResourceList::Undef;
  for (int i=RagTimeStruct::ResourceList::SpBo; i<=RagTimeStruct::ResourceList::SpVa; ++i) {
    std::string name("rsrc");
    name+=RagTimeStruct::ResourceList::getName(RagTimeStruct::ResourceList::Type(i));
    if (entry.type()!=name) continue;
    resType=RagTimeStruct::ResourceList::Type(i);
    break;
  }
  if (resType==RagTimeStruct::ResourceList::Undef)
    return false;

  entry.setParsed(true);
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=entry.begin();
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(" << entry.type() << ")[" << entry.id() << "]:";
  RagTimeStruct::ResourceList zone;
  if (!zone.read(input, entry)) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readResource: the size seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  f << zone;
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  input->seek(zone.m_dataPos, librevenge::RVNG_SEEK_SET);
  if (zone.m_type!=resType && zone.m_dataNumber) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readResource: find unexpected data size\n"));
    for (int i=0; i<zone.m_dataNumber; ++i) {
      pos=input->tell();
      f.str("");
      f << entry.type() << "-" << i << ":##";
      input->seek(pos+zone.m_dataSize, librevenge::RVNG_SEEK_SET);
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
  }
  else {
    int val;
    for (int i=0; i<zone.m_dataNumber; ++i) {
      pos=input->tell();
      f.str("");
      switch (resType) {
      case RagTimeStruct::ResourceList::SpBo: {
        RagTimeSpreadsheetInternal::CellBorder borders;
        libmwaw::DebugStream f2;
        val=(int) input->readLong(2); // always 0
        if (val) f << "f0=" << val << ",";
        val=(int) input->readLong(2);
        if (val!=1) f << "used=" << val << ",";
        for (int j=0; j<2; ++j) {
          MWAWBorder border;
          val=(int) input->readULong(2);
          f2.str("");
          // val&0x8000: def in cell
          if (val&0x4000) if (j) f2 << "third[bottom],";
          switch (val&3) {
          case 0:
            f2 << "pos[exterior],";
            break;
          case 1:
            f2 << "pos[interior],";
            break;
          case 2: // normal
            break;
          default:
            f2 << "##pos=3,";
            break;
          }
          val &= 0x3FFC;
          if (val) f2 << "fl=" << std::hex << val << std::dec << ",";
          border.m_width=float(input->readLong(4))/65536.f;
          val=(int) input->readLong(2);
          if (val && !m_mainParser->getColor(val-1, border.m_color)) {
            MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readResource: can not find border color %d\n", val-1));
            f2 << "#color=" << val-1 << ",";
          }
          border.m_extra=f2.str();
          borders.m_borders[j]=border;
        }
        borders.m_extra=f.str();
        m_state->m_cellBorderList.push_back(borders);
        f.str("");
        f << borders;
        break;
      }
      case RagTimeStruct::ResourceList::SpCe: {
        RagTimeSpreadsheetInternal::CellExtra extra;
        val=(int) input->readLong(2); // always 0
        if (val) f << "f0=" << val << ",";
        val=(int) input->readLong(2);
        if (val!=1) f << "used=" << val << ",";
        val=(int) input->readULong(2);
        if (val&0x8000) extra.m_isTransparent=true;
        if (val&0x4000) f << "bottom[color],";
        if (val&0x2000) f << "top[color],";
        val &= 0x1FFF;
        if (val) f << "fl=" << std::hex << val << std::dec << ",";
        val=(int) input->readLong(2);
        if (val && !m_mainParser->getColor(val-1, extra.m_color)) {
          MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readResource: can not find color %d\n", val-1));
          f << "#color=" << val-1 << ",";
        }
        extra.m_extra=f.str();
        m_state->m_cellExtraList.push_back(extra);
        f.str("");
        f << extra;
        break;
      }
      case RagTimeStruct::ResourceList::SpDE:
        for (int j=0; j<2; ++j) { // f0=2, f1=1-b
          val=(int) input->readLong(2);
          if (val) f << "f" << j << "=" << val << ",";
        }
        for (int j=0; j<2; ++j) { // two big number ?
          val=(int) input->readULong(2);
          if (val) f << "g" << j << "=" << std::hex << val << std::dec << ",";
        }
        val=(int) input->readLong(2);
        if (val) f << "f2=" << val << ",";
        m_state->m_cellDEList.push_back(f.str());
        break;
      case RagTimeStruct::ResourceList::SpTe: {
        MWAWFont font;
        val=(int) input->readLong(2); // always 0
        if (val) f << "f0=" << val << ",";
        val=(int) input->readLong(2);
        if (val!=1) f << "used=" << val << ",";
        val=(int) input->readLong(2);
        if (val>0 && !m_mainParser->getCharStyle(val-1, font)) {
          MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readResource: can not find a char format\n"));
          f << "##id[CharId]=" << val << ",";
        }
        val=(int) input->readLong(1);
        if (val) font.setDeltaLetterSpacing(float(val));
        val=(int) input->readLong(1); // 1|5
        if (val!=1) f << "f1=" << val << ",";
        val=(int) input->readLong(1);
        if (val) font.set(MWAWFont::Script(-float(val),librevenge::RVNG_POINT));
        val=(int) input->readULong(1);
        f << "lang=" << val << ",";
        font.m_extra=f.str();
        m_state->m_cellFontList.push_back(font);
        f.str("");
        f << font.getDebugString(m_parserState->m_fontConverter);
        break;
      }
      case RagTimeStruct::ResourceList::SpVa: {
        RagTimeSpreadsheetInternal::CellFormat format;
        val=(int) input->readLong(2); // always 0
        if (val) f << "f0=" << val << ",";
        val=(int) input->readLong(2);
        if (val!=1) f << "used=" << val << ",";
        format.m_flags=(int) input->readULong(2); // small number
        val=(int) input->readLong(2);
        if (val<=0 || val>int(m_state->m_numericFormatList.size())) {
          MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readResource: can not find a numeric format\n"));
          f << "##id[NumFormat]=" << val << ",";
        }
        else {
          format.m_numeric=m_state->m_numericFormatList[size_t(val-1)];
          if (val!=1) // default
            f << format.m_numeric << ",";
        }
        val=(int) input->readLong(2);
        if (val<=0 || val>int(m_state->m_dateTimeList.size())) {
          MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readResource: can not find a date/time format\n"));
          f << "##id[DTFormat]=" << val << ",";
        }
        else {
          format.m_dateTime=m_state->m_dateTimeList[size_t(val-1)];
          if (val!=1)
            f << format.m_dateTime << ",";
        }
        val=(int) input->readLong(1);
        switch (val) {
        case 1: // normal
          break;
        case 2:
          format.m_align=MWAWCell::HALIGN_LEFT;
          f << "left,";
          break;
        case 3:
          format.m_align=MWAWCell::HALIGN_CENTER;
          f << "center,";
          break;
        case 4:
          format.m_align=MWAWCell::HALIGN_RIGHT;
          f << "right,";
          break;
        case 5:
          f << "comma[justify],";
          break;
        case 6:
          f << "repeat,";
          break;
        default:
          f << "#align=" << val << ",";
          break;
        }
        val=(int) input->readLong(1);
        if (val>=1&&val<=4)
          format.m_rotation=90*(val-1);
        else
          f << "#rotation=" << val << ",";

        format.m_extra=f.str();
        m_state->m_cellFormatList.push_back(format);
        f.str("");
        f << format;
        break;
      }
      case RagTimeStruct::ResourceList::BuSl:
      case RagTimeStruct::ResourceList::BuGr:
      case RagTimeStruct::ResourceList::gray:
      case RagTimeStruct::ResourceList::colr:
      case RagTimeStruct::ResourceList::res_:
      case RagTimeStruct::ResourceList::Undef:
      default:
        break;
      }
      std::string data=f.str();
      f.str("");
      f << entry.type() << "-" << i << ":" << data;
      input->seek(pos+zone.m_dataSize, librevenge::RVNG_SEEK_SET);
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
  }
  // last field is always empty
  pos=input->tell();
  ascFile.addPos(pos);
  ascFile.addNote("_");
  input->seek(pos+zone.m_dataSize, librevenge::RVNG_SEEK_SET);

  if (input->tell()!=zone.m_endPos) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readResource: find some extra data\n"));
    f.str("");
    f << entry.type() << "-end:";
    ascFile.addPos(input->tell());
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool RagTimeSpreadsheet::readRsrcSpDI(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+0x20)) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readRsrcSpDI: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(" << entry.type() << ")[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  int headerSz=(int) input->readULong(2);
  int fSz=(int) input->readULong(2);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (headerSz<0x20 || fSz<0x8 || dSz<headerSz+(N+1)*fSz || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readRsrcSpDI: the size seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  input->seek(pos+2+headerSz, librevenge::RVNG_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  std::set<long> posSet;
  posSet.insert(endPos);
  for (int i=0; i<=N; ++i) {
    pos=input->tell();
    f.str("");
    f << entry.type() << "-" << i << ":";
    int val=(int) input->readLong(2); // 0 (except last)
    if (val) f << "f0=" << val << ",";
    int fPos=(int) input->readULong(2);
    if (fPos) {
      f << "pos[def]=" << std::hex << entry.begin()+2+fPos << std::dec << ",";
      posSet.insert(entry.begin()+2+fPos);
    }
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  for (std::set<long>::const_iterator it=posSet.begin(); it!=posSet.end();) {
    pos=*(it++);
    if (pos>=endPos) break;
    f.str("");
    f << entry.type() << "[data]:";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool RagTimeSpreadsheet::readRsrcSpDo(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+0x4a)) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readRsrcSpDo: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(rsrcSpDo)[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  if (dSz<0x4a || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readRsrcSpDo: the size seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  for (int i=0; i<2; ++i) { // f1=0|80
    int val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  int val0;
  for (int i=0; i<10; ++i) { // find g0=3+10*k, g1=10+g0, g2=10+g1, g3=10+g2, other 0|3+10*k1
    int val = (int) input->readLong(4);
    if (i==0) {
      val0=val;
      f << "g0=" << val << ",";
    }
    else if (i<4 && val!=val0+10*i)
      f << "g" << i << "=" << val << ",";
    else if (i>=4 && val)
      f << "g" << i << "=" << val << ",";
  }
  for (int i=0; i<9; ++i) {
    int val = (int) input->readULong(2);
    static int const expected[]= {0,0,0,0x64,0x3ff5,0x8312,0x6e97,0x8d4f,0xdf3b};
    if (val!=expected[i])
      f << "h" << i << "=" << std::hex << val << std::dec << ",";
  }
  int const numVal= int(endPos-4-input->tell())/2;
  for (int i=0; i<numVal; ++i) { // k0=small int, k1=k0+1, k2=k3=k4=0, k5=0|b7|e9|f3
    int val = (int) input->readLong(2);
    if (val) f << "k" << i << "=" << val << ",";
  }
  input->seek(endPos-4, librevenge::RVNG_SEEK_SET);
  f << "id?=" << std::hex << input->readULong(4) << std::dec << ","; // a big number
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}


////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////
bool RagTimeSpreadsheet::readBlockHeader(MWAWEntry const &entry, RagTimeSpreadsheetInternal::ComplexBlock &block)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=entry.begin();
  long endPos=entry.end();
  if (pos<=0 || entry.length()<6 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readBlockHeader: the position seems bad\n"));
    return false;
  }

  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  ascFile.addPos(endPos);
  ascFile.addNote("_");

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "Entries(" << entry.type() << "):";
  int N=(int) input->readLong(2);
  f << "N=" << N << ",";
  int dSz=(int) input->readULong(4);
  if (dSz<N*2+10 || pos+2+dSz>endPos) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readBlockHeader: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  int val=(int) input->readLong(2);
  f << "f0=" << val << ",";
  f << "flags=[";
  for (int i=0; i<=N; ++i) {
    val=(int) input->readLong(2);
    block.m_intList.push_back(val);
    if (val) f << val << ",";
    else f << "_,";
  }
  f << "],";
  if (dSz!=N*2+10) {
    ascFile.addDelimiter(input->tell(),'|');
    input->seek(pos+dSz, librevenge::RVNG_SEEK_SET);
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << entry.type() << ":";
  N=(int) input->readULong(2);
  f << "N[zones]=" << N << ",";
  if (pos+2+10*N>endPos) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readBlockHeader: the zone block seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  for (int i=0; i<N; ++i) {
    RagTimeSpreadsheetInternal::ComplexBlock::Zone zone;
    pos=input->tell();
    f.str("");
    f << entry.type() << "-Z" << i << "[def]:";
    for (int j=0; j<3; ++j) zone.m_data[j]=(int) input->readLong(2);
    long dataPos=(long) input->readULong(4);
    if (dataPos<N*2+10 || dataPos>entry.length()) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readBlockHeader: the zone position seems bad\n"));
      f << "###dataPos" << std::hex << entry.begin()+dataPos << std::dec << ",";
    }
    else
      zone.m_pos=entry.begin()+dataPos;
    block.m_zones.push_back(zone);
    f << zone;
    input->seek(pos+10, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  return true;
}

bool RagTimeSpreadsheet::readPositionsList(MWAWEntry const &entry, std::vector<long> &posList, long &lastDataPos)
{
  posList.resize(0);
  if (version()<2) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readPositionsList: must not be called for v1-2... file\n"));
    return false;
  }

  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=input->tell();
  long endPos=entry.end();
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << entry.type() << "[PosList]:";
  int dSz=(int) input->readULong(4);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (dSz<10+2*N || pos+dSz>endPos) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readPositionsList: can not find the second block size\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  long dataSize=(long) input->readULong(2);
  f << "lPos=" << std::hex << pos+dSz+dataSize << std::dec << ",";
  if (dataSize&1) ++dataSize;
  lastDataPos=pos+dSz+dataSize;
  if (lastDataPos>endPos) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readPositionsList: the last position seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  f << "pos=[";
  for (int i=0; i<N; ++i) {
    long newPos=pos+dSz+(long) input->readULong(2);
    f << std::hex << newPos << std::dec << ",";
    if (newPos>lastDataPos)  {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readPositionsList: find some bad position\n"));
      f << "###]";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      input->seek(pos+dSz, librevenge::RVNG_SEEK_SET);
      return true;
    }
    posList.push_back(newPos);
  }
  f << "],";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// spreadsheet zone v3...
////////////////////////////////////////////////////////////
bool RagTimeSpreadsheet::readSpreadsheet(MWAWEntry &entry)
{
  if (version()<2) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheet: must not be called for v1-2... file\n"));
    return false;
  }
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=entry.begin();
  int dataFieldSize=m_mainParser->getZoneDataFieldSize(entry.id());
  if (pos<=0 || !input->checkPosition(pos+dataFieldSize+0x64)) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheet: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(SpreadsheetZone):";
  int dSz=(int) input->readULong(dataFieldSize);
  long endPos=pos+dataFieldSize+dSz;
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
    f << "zone" << i << "=" << std::hex << pos+dataFieldSize+zoneBegin[i] << std::dec << ",";
    if (pos+2+zoneBegin[i]>endPos) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheet: the zone %d seems bad\n",i));
      zoneBegin[i]=0;
      f << "###";
      continue;
    }
    zoneBegin[i]+=pos+dataFieldSize;
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
  shared_ptr<RagTimeSpreadsheetInternal::Spreadsheet> sheet(new RagTimeSpreadsheetInternal::Spreadsheet);
  for (int i=0; i<10; ++i) {
    if (zoneBegin[i+1]<=zoneBegin[i]) continue;
    MWAWEntry zone;
    zone.setBegin(zoneBegin[i]);
    zone.setEnd(zoneBegin[i+1]);
    zone.setId(i);

    char const *(what[])= {
      "SpreadsheetContent", "SpreadsheetCFormat", "SpreadsheetFormula", "SpreadsheetCondition",
      "SpreadsheetCUseIn", "SpreadsheetUnknown5", "SpreadsheetUnknown6", "SpreadsheetCExtraUseIn",
      "SpreadsheetCRef", "SpreadsheetUnknown9",
    };
    zone.setType(what[i]);

    bool ok=true;
    switch (i) {
    case 0: // content
    case 1: // format
    case 2: // formula
    case 3: // condition
    case 4: // list of cell which used which have a ref the current cell
      ok=readSpreadsheetComplexStructure(zone, *sheet);
      if (ok && i<=2)
        m_state->m_idSpreadsheetMap[entry.id()]=sheet;
      break;
    case 5:
    // case 6: never seens
    case 7: // list of extra spreadsheet which have a ref to the current cell
    case 8: // link to cell in other extra spreadsheet(we must probably read it)
      ok=readSpreadsheetSimpleStructure(zone, *sheet);
      break;
    case 9:
      ok=readSpreadsheetZone9(zone, *sheet);
      break;
    default:
      ok=false;
      break;
    }
    if (ok) continue;
    f.str("");
    f << "Entries(" << zone.type() << "):";
    ascFile.addPos(zoneBegin[i]);
    ascFile.addNote(f.str().c_str());
    // SpreadsheetZone-9: sz+N+N*14
  }
  return true;
}

bool RagTimeSpreadsheet::readSpreadsheetCellDimension(Vec2i const &cellPos, long endPos, RagTimeSpreadsheetInternal::Spreadsheet &sheet)
{
  if (cellPos[0] && cellPos[1]) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellDimension: expect cellPos[0] or cellPos[1] = 0\n"));
    return false;
  }
  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;

  long pos=input->tell();
  f << "SpreadsheetContent-";
  if (cellPos[1]==0) {
    f << "colDim[" << cellPos[0] << "]:";
    if (pos+16>endPos || cellPos[0]<=0) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellDimension: can not read a row height\n"));
      f << "##";
    }
    else {
      f << "dim=[";
      // col dim, followed by some margins?, no sure if v3~15/16 is or not a dim
      for (int j=0; j<4; ++j) {
        long dim=(long) input->readULong(4);
        f << float(dim&0x7FFFFFFF)/65536.f;
        if (dim&0x80000000) f << "/h";
        f << ",";
        if (j) continue;
        if (cellPos[0]>int(sheet.m_widthCols.size()))
          sheet.m_widthCols.resize(size_t(cellPos[0]),0);
        sheet.m_widthCols[size_t(cellPos[0]-1)]=float(dim&0x7FFFFFFF)/65536.f;
      }
      f << "],";
    }
    if ((input->tell()+1==endPos && input->readLong(1)!=0) || input->tell()!=endPos)
      f<<"#";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return true;
  }

  f << "-rowDim[" << cellPos[1] << "]:";
  if (pos+8>endPos||cellPos[1]<=0) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellDimension: can not read a row height\n"));
    f << "##";
  }
  else {
    f << "dim=[";
    for (int j=0; j<2; ++j) { // row dim, followed by cell height
      long dim=(long) input->readULong(4);
      f << float(dim&0x7FFFFFFF)/65536.f;
      if (dim&0x80000000) f << "/h";
      f << ",";
      if (j) continue;
      if (cellPos[1]>int(sheet.m_heightRows.size()))
        sheet.m_heightRows.resize(size_t(cellPos[1]),0);
      sheet.m_heightRows[size_t(cellPos[1]-1)]=float(dim&0x7FFFFFFF)/65536.f;
    }
    f << "],";
  }
  if ((input->tell()+1==endPos && input->readLong(1)!=0) || input->tell()!=endPos)
    f<<"#";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool RagTimeSpreadsheet::readSpreadsheetCellContent(RagTimeSpreadsheetInternal::Cell &cell, long endPos)
{
  Vec2i const &cellPos=cell.position();
  if (cellPos[0]<0 || cellPos[1]<0) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellContent: expect cellPos[0] and cellPos[1] >= 0\n"));
    return false;
  }

  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;

  long pos=input->tell();

  f << "SpreadsheetContent-C" << cellPos<< "]:";
  bool ok=true;
  MWAWCell::Format format=cell.getFormat();
  MWAWCellContent &content=cell.m_content;
  int val, type=(int) input->readULong(1);
  switch (type) {
  case 0: // not frequent, but can happens
    break;
  case 0x40:
  case 0x44: // find with 44244131
  case 0x80:
    if (type==0x80)
      f << "Nan[eval],";
    else
      f << "Nan[circ],";
    format.m_format=MWAWCell::F_NUMBER;
    format.m_numberFormat=MWAWCell::F_NUMBER_GENERIC;
    content.m_contentType=MWAWCellContent::C_NUMBER;
    content.setValue(std::numeric_limits<double>::quiet_NaN());
    break;
  case 0x81:
    f << "float81,";
  // fall through intended
  case 3:
  // fall through intended
  case 1: {
    if (type==3) {
      format.m_format=MWAWCell::F_DATE; // we need the format to choose between date and time
      f << "date/time,";
    }
    else {
      format.m_format=MWAWCell::F_NUMBER;
      format.m_numberFormat=MWAWCell::F_NUMBER_GENERIC;
    }
    content.m_contentType=MWAWCellContent::C_NUMBER;
    if (pos+11>endPos) {
      ok=false;
      break;
    }
    double res;
    bool isNan;
    if (input->readDouble10(res, isNan)) {
      content.setValue(res);
      f << res << ",";
    }
    else
      f << "#value,";
    break;
  }
  case 0x24:
    f << "text2,";
  // fall through intended
  case 4: {
    format.m_format=MWAWCell::F_TEXT;
    content.m_textEntry.setBegin(input->tell());
    content.m_textEntry.setEnd(endPos);

    std::string text("");
    for (int j=0; j<endPos-1-pos; ++j) {
      char c=(char) input->readLong(1);
      if (c==0) {
        content.m_textEntry.setEnd(input->tell()-1);
        break;
      }
      text+=c;
    }
    f << text << ",";
    break;
  }
  case 5:
    if (pos+2>endPos) {
      ok=false;
      break;
    }
    val=(int)input->readLong(1);
    f << val << ",";
    break;
  case 6:
  case 0x14: {
    f << "textZone,";
    MWAWEntry textEntry;
    textEntry.setBegin(input->tell());
    textEntry.setEnd(endPos);
    textEntry.setId(m_mainParser->getNewZoneId());
    textEntry.setType("SpreadsheetText");
    format.m_format=MWAWCell::F_TEXT;
    cell.m_textEntry=textEntry;
    // fixme: set the column width here
    ok=m_mainParser->readTextZone(textEntry, 0);
    break;
  }
  case 0x51:
    f << "long51,";
  // fall through intended
  case 0x11: // or 2 int?
    if (pos+5>endPos) {
      ok=false;
      break;
    }
    val=(int)input->readLong(4);
    format.m_format=MWAWCell::F_NUMBER;
    format.m_numberFormat=MWAWCell::F_NUMBER_GENERIC;
    content.m_contentType=MWAWCellContent::C_NUMBER;
    content.setValue(double(val));
    f << val << ",";
    break;
  default:
    ok=false;
    break;
  }
  cell.setFormat(format);
  if (!ok) f<< "##";
  else if ((input->tell()+1==endPos && input->readLong(1)!=0) || input->tell()!=endPos)
    f<<"#";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return ok;
}

bool RagTimeSpreadsheet::readSpreadsheetCellFormat(Vec2i const &cellPos, long endPos, RagTimeSpreadsheetInternal::Cell &cell)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=input->tell();
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;

  f << "SpreadsheetCFormat-C" << cellPos << "]:";
  if (cellPos[1]==0) f << "col,";
  else if (cellPos[0]==0) f << "row,";
  if (pos+8>endPos) {
    f << "###";
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellFormat: a block definition seems bad\n"));
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  MWAWCell::Format format;
  for (int j=0; j<4; ++j) {
    int val=(int) input->readULong(2);
    if (val==0) {
      if (cellPos[0] && cellPos[1]) {
        char const(*wh[])= {"SpTe","SpVa", "SpBo", "SpCe"};
        f << wh[j] << "=#undef,";
      }
      continue;
    }
    switch (j) {
    case 0: {
      if (val<=0 || val >(int) m_state->m_cellFontList.size()) {
        MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellFormat: find unexpected SpTe index\n"));
        f << "id[SpTe]=##" << val-1 << ",";
        break;
      }

      MWAWFont const &font=m_state->m_cellFontList[size_t(val-1)];
      cell.setFont(font);
      if (val==1) break;
      f << "te=[" << font.getDebugString(m_parserState->m_fontConverter) << "],";
      break;
    }
    case 1: {
      if (val<=0 || val>(int) m_state->m_cellFormatList.size()) {
        MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellFormat: find unexpected SpVa index\n"));
        f << "id[SpVa]=##" << val-1 << ",";
        break;
      }
      RagTimeSpreadsheetInternal::CellFormat const &cFormat=m_state->m_cellFormatList[size_t(val-1)];
      cFormat.update(cell);
      if (val==1) break;
      f << "va=[" << cFormat << "],";
      break;
    }
    case 2: {
      if (val <= 0 || val>(int) m_state->m_cellBorderList.size()) {
        MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellFormat: find unexpected SpBoindex\n"));
        f << "id[SpBo]=##" << val-1 << ",";
        break;
      }
      RagTimeSpreadsheetInternal::CellBorder const &borders=m_state->m_cellBorderList[size_t(val-1)];
      if (val==1) break;
      borders.update(cell);
      f << "bo=[" << borders <<  "],";
      break;
    }
    case 3: {
      if (val<=0 || val>(int) m_state->m_cellExtraList.size()) {
        MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellFormat: find unexpected SpCe index\n"));
        f << "id[SpCe]=##" << val-1 << ",";
        break;
      }
      RagTimeSpreadsheetInternal::CellExtra const &extras=m_state->m_cellExtraList[size_t(val-1)];
      extras.update(cell);
      if (val==1) break;
      f << "ce=[" << extras << "],";
      break;
    }
    default:
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellFormat: find unexpected type\n"));
      f << "###";
      break;
    }

  }
  cell.setFormat(format);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  return true;
}

bool RagTimeSpreadsheet::readSpreadsheetZone9(MWAWEntry const &entry, RagTimeSpreadsheetInternal::Spreadsheet &/*sheet*/)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=entry.begin();
  long endPos=entry.end();
  if (pos<=0 || entry.length()<3 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetZone9: the position seems bad\n"));
    return false;
  }

  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  ascFile.addPos(endPos);
  ascFile.addNote("_");

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "Entries(" << entry.type() << "):";
  int dSz=(int) input->readULong(4);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (pos+4+dSz>endPos||dSz!=2+14*N) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetZone9: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << entry.type() << "-A" << i << ":";
    input->seek(pos+14, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  pos=input->tell();
  if (pos!=endPos) {
    f.str("");
    f << entry.type() << "-extra:";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool RagTimeSpreadsheet::readSpreadsheetSimpleStructure(MWAWEntry const &entry, RagTimeSpreadsheetInternal::Spreadsheet &/*sheet*/)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=entry.begin();
  long endPos=entry.end();
  if (pos<=0 || entry.length()<8 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetSimpleStructure: the position seems bad\n"));
    return false;
  }

  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  ascFile.addPos(endPos);
  ascFile.addNote("_");

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "Entries(" << entry.type() << "):";
  int dSz=(int) input->readULong(4);
  int headerSz=(int) input->readULong(2);
  if (pos+4+dSz>endPos||headerSz<18||headerSz>dSz) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetSimpleStructure: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  int fSz=(int) input->readULong(2);
  int N=(int) input->readULong(2);
  f << "N=" << N << "[" << fSz << "],";
  int val;
  for (int i=0; i<2; ++i) { // f0=4|c
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  int ptrSz=(int) input->readLong(2);
  if (ptrSz==2||ptrSz==4) f << "ptr[sz]=" << ptrSz << ",";
  else if (ptrSz) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetSimpleStructure: the ptr size seems bad\n"));
    f << "###ptrSz" << ptrSz << ",";
  }
  int dataSz=(int) input->readLong(4);
  if (headerSz<18 || headerSz+(N+1)*fSz+dataSz>dSz || fSz < 0) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetSimpleStructure: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  ascFile.addDelimiter(input->tell(),'|');
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  input->seek(pos+4+headerSz, librevenge::RVNG_SEEK_SET);
  std::set<long> dataPosSet;
  for (int i=0; i<=N; ++i) {
    pos=input->tell();
    f.str("");
    f << entry.type() << "-A" << i << ":";
    if (ptrSz) {
      long dPos=entry.begin()+4+(long) input->readULong(ptrSz);
      f << "pos=" << std::hex << dPos << std::dec << ",";
      if (dPos>endPos) {
        f << "###";
        MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetSimpleStructure: the data pos seems bad\n"));
      }
      else if (dPos<endPos)
        dataPosSet.insert(dPos);
    }
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  f.str("");
  f << entry.type() << "-data:";
  for (std::set<long>::const_iterator it=dataPosSet.begin(); it!=dataPosSet.end(); ++it) {
    ascFile.addPos(*it);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool RagTimeSpreadsheet::readSpreadsheetComplexStructure(MWAWEntry const &entry, RagTimeSpreadsheetInternal::Spreadsheet &sheet)
{
  RagTimeSpreadsheetInternal::ComplexBlock block;
  if (!readBlockHeader(entry, block)) return false;

  MWAWInputStreamPtr input = m_parserState->m_input;
  long endPos=entry.end();

  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  ascFile.addPos(endPos);
  ascFile.addNote("_");

  /* first sort data by first value ( as we need to parse first the
     column definition before parsing a cell content ) */
  std::map<int,size_t> sortZones;
  for (size_t z=0; z< block.m_zones.size(); ++z) {
    int const fValue=block.m_zones[z].m_data[0];
    if (sortZones.find(fValue)!=sortZones.end()) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetComplexStructure: oops, a zone with firstVal=%d already exists\n", fValue));
      f.str("");
      f << entry.type() << "-unparsed:###";
      ascFile.addPos(block.m_zones[z].m_pos);
      ascFile.addNote(f.str().c_str());
    }
    else
      sortZones[fValue]=z;
  }

  long lastEndPos=input->tell(); // end of last block to check for unparsed data
  for (std::map<int,size_t>::iterator it=sortZones.begin(); it!=sortZones.end(); ++it) {
    size_t const z=it->second;
    RagTimeSpreadsheetInternal::ComplexBlock::Zone &contentZone=block.m_zones[z];
    long contentEndPos=(block.m_zones.size()>z+1 && block.m_zones[z+1].m_pos<entry.begin()) ?
                       block.m_zones[z+1].m_pos : endPos;

    long pos=contentZone.m_pos;
    if (pos<entry.begin() || pos>=contentEndPos) continue;
    input->seek(pos, librevenge::RVNG_SEEK_SET);

    std::vector<long> posList;
    long lastDataPos;
    if (!readPositionsList(entry, posList, lastDataPos))
      return true;

    int const numCellsByRows=contentZone.m_data[2];
    if (!posList.empty() && numCellsByRows<=0) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetComplexStructure: can not find determine the cell's positions\n"));
      continue;
    }

    int row=contentZone.m_data[0]-1, prevActiveRow=-1, rowType=1;
    for (size_t i=0; i<posList.size(); ++i) {
      f.str("");
      int col=int(i)%numCellsByRows;
      if (int(i)/numCellsByRows!=prevActiveRow) {
        while (1) {
          if (++row >= int(block.m_intList.size()) || row<0) {
            MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetComplexStructure: can not find determine a cell's row\n"));
            f << "###";
            break;
          }
          if (!block.m_intList[size_t(row)]) continue;
          if (rowType==2) --row;
          if (rowType!=1 && rowType!=2) {
            MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetComplexStructure: find unknown row type\n"));
            f << "###";
          }
          break;
        }
        prevActiveRow=int(i)/numCellsByRows;
      }

      long zEndPos=(i+1==posList.size()) ? lastDataPos:posList[i+1];
      if (!posList[i] || posList[i]>=zEndPos) continue;

      input->seek(posList[i], librevenge::RVNG_SEEK_SET);
      Vec2i cellPos(col,row);
      bool ok=true;
      RagTimeSpreadsheetInternal::Cell emptyCell;
      RagTimeSpreadsheetInternal::Cell *cell = 0;
      if (col>0 && row>0 && entry.id()<=2) {
        if (sheet.m_cellsMap.find(cellPos-Vec2i(1,1))==sheet.m_cellsMap.end())
          sheet.m_cellsMap[cellPos-Vec2i(1,1)]=RagTimeSpreadsheetInternal::Cell();
        cell=&sheet.m_cellsMap.find(cellPos-Vec2i(1,1))->second;
      }
      if (!cell) cell=&emptyCell;
      cell->setPosition(cellPos-Vec2i(1,1));
      switch (entry.id()) {
      case 0:
        if (cellPos[0]<=0 || cellPos[1]<=0)
          ok=readSpreadsheetCellDimension(cellPos, zEndPos, sheet);
        else
          ok=readSpreadsheetCellContent(*cell, zEndPos);
        break;
      case 1:
        ok=readSpreadsheetCellFormat(cellPos, zEndPos, *cell);
        break;
      case 2: // formula
      case 3: { // condition
        f << entry.type() << "-C" << col << "x" << row << "]:";
        std::string extra("");
        std::vector<MWAWCellContent::FormulaInstruction> formula;
        int val=(int) input->readULong(1);
        if (val) f << "f0=" << std::hex << val << std::dec << ",";
        ok=readFormula(cellPos-Vec2i(1,1), formula, zEndPos, extra);
        if (ok && entry.id()==2) {
          MWAWCellContent &content=cell->m_content;
          content.m_formula=formula;
          if (cell && cell->validateFormula())
            content.m_contentType=MWAWCellContent::C_FORMULA;
        }
        if (!ok) f << "###";
        f << "formula=[";
        for (size_t j=0; j<formula.size(); ++j)
          f << formula[j];
        f << "]";
        if (!extra.empty()) f << ":" << extra;
        f << ",";
        ascFile.addPos(posList[i]);
        ascFile.addNote(f.str().c_str());
        ok=true;
        break;
      }
      // case 4: list of cell which reference this cell, ok to ignore
      default:
        ok=false;
        break;
      }
      if (!ok) {
        f << entry.type() << "-C" << col << "x" << row << "]:";
        if (row==0) f << "col,";
        else if (col==0) f << "row,";

        ascFile.addPos(posList[i]);
        ascFile.addNote(f.str().c_str());
      }
    }
    if (lastDataPos>lastEndPos) lastEndPos=lastDataPos;
  }

  if (lastEndPos<endPos) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetComplexStructure: find some extra data\n"));
    f.str("");
    f << entry.type() << "-extra:###";
    ascFile.addPos(lastEndPos);
    ascFile.addNote(f.str().c_str());
  }
  input->seek(lastEndPos, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// spreadsheet zone v2...
////////////////////////////////////////////////////////////
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
  std::stringstream s;
  s << "Sheet" << entry.id();
  sheet->m_name=s.str();
  // first read the last data, which contains the begin of row positions
  MWAWEntry extra;
  extra.setBegin(zonesList[1]);
  extra.setEnd(endPos);
  sheet->m_cellsBegin=zonesList[0];
  if (!readSpreadsheetExtraV2(extra, *sheet))
    return false;

  MWAWEntry cells;
  cells.setBegin(zonesList[0]);
  cells.setEnd(zonesList[1]);
  if (!readSpreadsheetCellsV2(cells, *sheet))
    return false;
  m_state->m_idSpreadsheetMap[entry.id()]=sheet;
  return true;
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

  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  ascFile.addPos(endPos);
  ascFile.addNote("_");

  int n=0;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (size_t row=0; row<sheet.m_rowPositionsList.size(); ++row) {
    pos=sheet.m_rowPositionsList[row];
    long rEndPos=row+1==sheet.m_rowPositionsList.size() ? endPos : sheet.m_rowPositionsList[row+1];
    if (pos<entry.begin() || rEndPos>entry.end()) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellsV2: the position of the row cells %d is odd\n", int(row)));
      continue;
    }
    if (pos+2>=rEndPos) continue;
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    while (1) {
      pos=input->tell();
      if (pos+2>rEndPos) break;
      int col=int(input->readULong(1))-1;
      Vec2i cellPos(col,int(row));
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
        break;
      }
      else if (!readSpreadsheetCellV2(cell, zEndPos) || cellPos[0]<0||cellPos[1]<0) {
        MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellsV2: small pb reading a cell\n"));
        f << "###";
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
      }
      else if (sheet.m_cellsMap.find(cellPos) != sheet.m_cellsMap.end()) {
        MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellsV2: already find a cell in (%d,%d)\n", cellPos[0],cellPos[1]));
        ascFile.addPos(pos);
        ascFile.addNote("###duplicated");
      }
      else
        sheet.m_cellsMap[cellPos]=cell;
      if ((dSz%2)==1) ++zEndPos;
      input->seek(zEndPos, librevenge::RVNG_SEEK_SET);
    }
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
  case 7:
    format.m_format=MWAWCell::F_DATE;
    f << "date,";
    break;
  case 9:
    format.m_format=MWAWCell::F_TEXT;
    f << "text,";
    break;
  case 11:
    format.m_format=MWAWCell::F_NUMBER;
    format.m_numberFormat=MWAWCell::F_NUMBER_GENERIC;
    f << "nan,";
    break;
  default:
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellV2: find unknown type %d\n", type));
    f << "##type=" << type << ",";
    break;
  }
  bool hasFont=(val&1);
  bool hasAlign=(val&2);
  bool hasFormula=(val&4);
  bool hasPreFormula=(val&8);
  if (hasFormula)
    f << "formula,";
  if (hasPreFormula)
    f << "preFormula,";

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
  MWAWCellContent &content=cell.m_content;
  if (hasPreFormula) {
    std::string extra("");
    std::vector<MWAWCellContent::FormulaInstruction> &formula=content.m_formula;
    bool ok=readFormulaV2(cell.position(), formula, endPos, extra);
    f << "formula=[";
    for (size_t i=0; i<formula.size(); ++i)
      f << formula[i];
    f << "]";
    if (!extra.empty()) f << ":" << extra;
    f << ",";
    if (!ok) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellV2: can not read a preFormula\n"));
      f << "###formula,";
      ascFile.addPos(pos-2);
      ascFile.addNote(f.str().c_str());
      return true;
    }
    if (cell.validateFormula())
      content.m_contentType=MWAWCellContent::C_FORMULA;
    if (input->tell()!=endPos) ascFile.addDelimiter(input->tell(),'|');
  }
  val= hasAlign ? (int) input->readULong(1) : 0;
  int align= val&7;
  switch (align) {
  case 0:
    break;
  case 2:
    cell.setHAlignment(MWAWCell::HALIGN_LEFT);
    f << "left,";
    break;
  case 3:
    cell.setHAlignment(MWAWCell::HALIGN_CENTER);
    f << "center,";
    break;
  case 4:
    cell.setHAlignment(MWAWCell::HALIGN_RIGHT);
    f << "right,";
    break;
  case 5: // full(repeat)
    cell.setHAlignment(MWAWCell::HALIGN_LEFT);
    f << "repeat,";
    break;
  default:
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellV2: find unknown alignment\n"));
    f << "##align=" << align << ",";
    break;
  }
  val&=0xF8;
  if (hasFormula) {
    std::string extra("");
    std::vector<MWAWCellContent::FormulaInstruction> condition;
    std::vector<MWAWCellContent::FormulaInstruction> &formula=hasPreFormula ? condition : content.m_formula;
    bool ok=readFormulaV2(cell.position(), formula, endPos, extra);
    if (hasPreFormula)
      f << "condition=[";
    else
      f << "formula=[";
    for (size_t i=0; i<formula.size(); ++i)
      f << formula[i];
    f << "]";
    if (!extra.empty()) f << ":" << extra;
    f << ",";
    if (!ok) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellV2: can not read a formula\n"));
      f << "###formula,";
      ascFile.addPos(pos-2);
      ascFile.addNote(f.str().c_str());
      return true;
    }
    if (!hasPreFormula && cell.validateFormula())
      content.m_contentType=MWAWCellContent::C_FORMULA;
    if (input->tell()!=endPos) ascFile.addDelimiter(input->tell(),'|');
  }

  actPos=input->tell();
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
  case 7: {
    if (actPos+4!=endPos) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellV2: can not read a date\n"));
      f << "###number";
      break;
    }
    if (format.m_format==MWAWCell::F_UNKNOWN)
      format.m_format=MWAWCell::F_DATE;
    if (content.m_contentType!=MWAWCellContent::C_FORMULA)
      content.m_contentType=MWAWCellContent::C_NUMBER;
    int Y=(int) input->readULong(2);
    int M=(int) input->readULong(1);
    int D=(int) input->readULong(1);
    f << M << "/" << D << "/" << Y << ",";
    double res;
    if (!MWAWCellContent::date2Double(Y,M,D,res))
      f << "#date,";
    else
      content.setValue(res);
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
  case 11: {
    if (actPos+1!=endPos) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellV2: can not read a nan number\n"));
      f << "###nan";
      break;
    }
    if (format.m_format==MWAWCell::F_UNKNOWN)
      format.m_format=MWAWCell::F_NUMBER;
    if (content.m_contentType!=MWAWCellContent::C_FORMULA)
      content.m_contentType=MWAWCellContent::C_NUMBER;
    cell.m_content.setValue(std::numeric_limits<double>::quiet_NaN());
    val=(int) input->readULong(1);
    f << "nan=" << val << ",";
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
        dims.push_back(0);
      }
      else {
        dims.push_back(float(dim-prevDim));
        prevDim=dim;
      }
      f << "dim=" << float(dim-prevDim) << ",";
      if (i==0) {
        f << "height=" << input->readULong(2) << ",";
        long rowPos=sheet.m_cellsBegin+(long)input->readULong(4);
        sheet.m_rowPositionsList.push_back(rowPos);
        f << "pos?=" << std::hex << rowPos << std::dec << ",";
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
  listener->openSheet(sheet.getColumnsWidth(), librevenge::RVNG_POINT, sheet.m_name);

  MWAWInputStreamPtr &input=m_parserState->m_input;
  int prevRow = -1;
  RagTimeSpreadsheetInternal::Spreadsheet::Map::const_iterator cIt;
  for (cIt=sheet.m_cellsMap.begin(); cIt!=sheet.m_cellsMap.end(); ++cIt) {
    RagTimeSpreadsheetInternal::Cell cell= cIt->second;
    if (cell.position()[1] != prevRow) {
      while (cell.position()[1] > prevRow) {
        if (prevRow != -1)
          listener->closeSheetRow();
        prevRow++;
        listener->openSheetRow(sheet.getRowHeight(prevRow), librevenge::RVNG_POINT);
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

////////////////////////////////////////////////////////////
// formula
////////////////////////////////////////////////////////////
bool RagTimeSpreadsheet::readCellInFormula(Vec2i const &cellPos, bool canBeList, MWAWCellContent::FormulaInstruction &instr, long endPos, std::string &extra)
{
  bool isList=canBeList;
  MWAWInputStreamPtr input=m_parserState->m_input;
  libmwaw::DebugStream f;

  instr.m_type= isList ? MWAWCellContent::FormulaInstruction::F_CellList :
                MWAWCellContent::FormulaInstruction::F_Cell;
  f << "Cell=";
  long pos=input->tell();
  if (pos+2>endPos) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readCellInFormula: the size seems bad\n"));
    f << "#size,";
    extra=f.str();
    return false;
  }
  int val=(int) input->readULong(1);
  if (val&0x30) { // does not seem to have any sens
    f << "fl=" << std::hex << (val&0x30) << std::dec << ",";
    val &= 0xCF;
  }
  if (isList) {
    if ((val&0x40)==0)
      f << "noFlList,";
    val &= 0xBF;
  }
  if (val<0x80 || (!isList && val>0x83) || (isList && val>0x8f))  {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readCellInFormula: find unknown begin\n"));
    f << "#f0=" << std::hex << val << std::dec << ",";
    extra=f.str();
    return false;
  }
  instr.m_positionRelative[0][0]=(val&2);
  instr.m_positionRelative[0][1]=(val&1);
  instr.m_positionRelative[1][0]=(val&8);
  instr.m_positionRelative[1][1]=(val&4);
  val=(int) input->readULong(1);
  int type0=(val>>5)&7;
  int type1=(val>>2)&7;
  if (type0&4) {
    type0&=3;
    instr.m_type=MWAWCellContent::FormulaInstruction::F_CellList;
    isList=true;
  }
  else { // no cell 0, ie a simple cell list reduced to a cell
    instr.m_type=MWAWCellContent::FormulaInstruction::F_Cell;
    isList=false;
  }
  for (int c=0; c<2; ++c) {
    switch (type0) {
    case 0:
      if (instr.m_positionRelative[c][1]==false) {
        MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readCellInFormula: find unexpected type0 cell\n"));
        f << "#type0[abs],";
        extra=f.str();
        return false;
      }
      instr.m_position[c][1]=cellPos[1];
      if (instr.m_positionRelative[c][0]==true) {
        if (val<0x10)
          instr.m_position[c][0]=cellPos[0]+val;
        else if (val<0x20)
          instr.m_position[c][0]=cellPos[0]+val-0x20;
      }
      else
        instr.m_position[c][0]=val-5;
      break;
    case 1:
      if (instr.m_positionRelative[c][0]==false) {
        MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readCellInFormula: find unexpected type0 cell\n"));
        f << "#type0=1[abs],";
        extra=f.str();
        return false;
      }
      val &= 0x1F;
      instr.m_position[c][0]=cellPos[0];
      if (instr.m_positionRelative[c][1]==true) {
        if (val<0x10)
          instr.m_position[c][1]=cellPos[1]+val;
        else if (val<0x20)
          instr.m_position[c][1]=cellPos[1]+val-0x20;
      }
      else
        instr.m_position[c][1]=val-5;
      break;
    case 2: {
      int firstValue=((val>>2)&0x7);
      int secondValue=(val&0x3);
      if (instr.m_positionRelative[c][0]==true) {
        if (instr.m_positionRelative[c][1]==false) {
          if (firstValue==0) {
            MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readCellInFormula: can not determine the row\n"));
            f << "#type0=2[row],";
            extra=f.str();
            return false;
          }
          instr.m_position[c][1]=firstValue-1;
        }
        else if (firstValue&4)
          instr.m_position[c][1]=cellPos[1]+firstValue-8;
        else
          instr.m_position[c][1]=cellPos[1]+firstValue;
        if (secondValue&2)
          instr.m_position[c][0]=cellPos[0]+secondValue-4;
        else
          instr.m_position[c][0]=cellPos[0]+secondValue;
      }
      else if (instr.m_positionRelative[c][1]==true) {
        MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readCellInFormula: find unexpected type0 cell\n"));
        f << "#type0=2[rel/abs],";
        extra=f.str();
        return false;
      }
      else {
        std::stringstream s;
        s << "Ref" << firstValue << ",";
        instr.m_sheet=s.str();
        return true;
      }
      break;
    }
    case 3: {
      if (type1>5) {
        f << "type0=" << type0 << ",type1=" << type1 << ",";
        MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readCellInFormula: unexpected number of bytes for type1\n"));
        extra=f.str();
        return false;
      }
      int nExpectedBits=2*type1+3+(type1>=4?3:0); // between 3 and 16
      int nBytesToRead=(nExpectedBits-2+7)/8; // between 1 and 2
      if (input->tell()+nBytesToRead>endPos) {
        MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readCellInFormula: unexpected end\n"));
        f << "#end[row],";
        extra=f.str();
        return false;
      }
      int readValue=((val&3)<<(nBytesToRead*8))+(int) input->readULong(nBytesToRead);
      int nBitRemain=2+8*nBytesToRead-nExpectedBits;
      int secondValue=(readValue&((1<<nBitRemain)-1));

      int firstValue=(readValue>>nBitRemain);
      if (instr.m_positionRelative[c][1]==true) {
        if (firstValue>=(1<<(nExpectedBits-1)))
          firstValue-=(1<<nExpectedBits);
        firstValue+=cellPos[1];
      }
      else
        --firstValue;
      instr.m_position[c][1]=firstValue;

      if ((instr.m_positionRelative[c][0]==false && secondValue==0) ||
          (instr.m_positionRelative[c][0]==true && nBitRemain<=2)) {
        if (input->tell()>=endPos) {
          MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readCellInFormula: unexpected end\n"));
          f << "#end[col],";
          extra=f.str();
          return false;
        }
        secondValue=(int) input->readULong(1);
        nBitRemain=8;
      }
      if (instr.m_positionRelative[c][0]==false) {
        if (secondValue<5) {
          std::stringstream s;
          s << "Ref" << firstValue+1 << ",";
          instr.m_position[c][1]=0;
          instr.m_sheet=s.str();
          return true;
        }
        else
          instr.m_position[c][0]=secondValue-5;
      }
      // checkme: seems to work, but I am very unsure of this code
      else if (nBitRemain!=8 && (secondValue&(1<<(nBitRemain-1))))
        instr.m_position[c][0]=(256+secondValue+cellPos[0]-(1<<nBitRemain))%256;
      else
        instr.m_position[c][0]=(secondValue+cellPos[0])%256;
      break;
    }
    default:
      f << "type0=" << type0 << ",type1=" << type1 << ",";
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readCellInFormula: I only know how to read type0 field\n"));
      extra=f.str();
      return false;
    }

    if (!isList || c==1) break;
    if (input->tell()>=endPos) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readCellInFormula: can not read the second cell\n"));
      f << "#end[cell]=" << instr.m_position[0] << ",";
      extra=f.str();
      return false;
    }

    val=(int) input->readULong(1);
    type0=(val>>5)&7;
    type1=(val>>2)&7;
  }
  if (instr.m_position[0][0]<0||instr.m_position[0][1]<0||
      (instr.m_type==MWAWCellContent::FormulaInstruction::F_CellList &&
       (instr.m_position[1][0]<0||instr.m_position[1][1]<0))) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readCellInFormula: bads cell\n"));
    f << "#cell=" << instr << ",";
    extra=f.str();
    return false;
  }
  return true;
}

bool RagTimeSpreadsheet::readCellInFormulaV2(Vec2i const &cellPos, bool canBeList, MWAWCellContent::FormulaInstruction &instr, long endPos, std::string &extra)
{
  MWAWInputStreamPtr input=m_parserState->m_input;
  libmwaw::DebugStream f;

  instr.m_type=MWAWCellContent::FormulaInstruction::F_Cell;
  f << "Cell=";
  bool ok=true;
  int which=0;
  int page=-1, frame=-1;
  while (!input->isEnd()) {
    long pos=input->tell();
    if (pos+2>endPos) break;
    int what=(int) input->readULong(1);
    if (canBeList && (what==0x83 || what==0x84) && which==0) {
      instr.m_type=MWAWCellContent::FormulaInstruction::F_CellList;
      which=1;
      what&=0xF;
      instr.m_position[1]=instr.m_position[0];
      instr.m_positionRelative[1]=instr.m_positionRelative[0];
    }
    if (what < 3 || what > 6) {
      ok=false;
      f << "##marker=" << what << ",";
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readCellInFormulaV2: find unknown marker %d\n", what));
      break;
    }
    int val = (int) input->readULong(1);
    int flag=0;
    if (val==0x80 || val==0xc0 || val==0xFF) {
      flag=val;
      val = (int) input->readULong(1);
    }
    if (input->tell()>endPos) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readCellInFormulaV2: the %d marker data seems odd\n", what));
      f << "###market[data]=" << what << ",";
      ok=false;
      break;
    }
    bool absolute=false;
    if ((flag==0&&!(val&0xC0)) || (flag==0x80 && (val&0xC0)))
      absolute=true;
    else if (flag==0&&(val&0xe0)==0x60) {
      if (what>=5) {
        MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readCellInFormulaV2: find relative position for sheet\n"));
        f << "###";
      }
      else
        val += 1-0x80+cellPos[4-what];
    }
    else if (flag==0&&(val&0xe0)==0x40) {
      if (what>=5) {
        MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readCellInFormulaV2: find relative position for sheet\n"));
        f << "###";
      }
      else
        val += 1-0x40+cellPos[4-what];
    }
    else if (flag==0xc0) {
      if (what>=5) {
        MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readCellInFormulaV2: find relative position for sheet\n"));
        f << "###";
      }
      else
        val += 1+cellPos[4-what];
    }
    else if (flag==0xff) {
      if (what>=5) {
        MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readCellInFormulaV2: find relative position for sheet\n"));
        f << "###";
      }
      else
        val += -0xff+cellPos[4-what];
    }
    else {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readCellInFormulaV2: can not read a cell position position for sheet\n"));
      f << "###v" << what << "=" << std::hex << val << "[" << flag << "]" << std::dec;
      ok=false;
      break;
    }
    if (what==3||what==4) {
      instr.m_position[which][4-what]=(val-1);
      instr.m_positionRelative[which][4-what]=!absolute;
    }
    else if (what==5 || what==6) {
      if (what==5) frame=val;
      else if (what==6) page=val;
      std::stringstream s;
      s << "Sheet";
      if (page>=0) s << "P" << page;
      if (frame>=0) s << "F" << frame;
      instr.m_sheet=s.str();
    }
    else {
      f << "##marker=" << what << ",";
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readCellInFormulaV2: find unexpected marker %d\n", what));
      break;
    }
  }
  if (ok && input->tell()!=endPos) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readCellInFormulaV2: find extra data at the end of a cell\n"));
    f << "###cell[extra],";
    ok=false;
  }
  if (ok && (instr.m_position[0][0]<0||instr.m_position[0][0]>255 ||
             instr.m_position[0][1]<0||instr.m_position[0][1]>255)) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readCellInFormulaV2: something go wrong\n"));
    f << "###cell[position],";
    ok=false;
  }
  if (ok && instr.m_type==MWAWCellContent::FormulaInstruction::F_CellList &&
      (instr.m_position[1][0]<0||instr.m_position[1][0]>255 ||
       instr.m_position[1][1]<0||instr.m_position[1][1]>255)) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readCellInFormulaV2: something go wrong\n"));
    f << "###cell[position2],";
    ok=false;
  }
  extra=f.str();
  return ok;
}

bool RagTimeSpreadsheet::readFormula(Vec2i const &cellPos, std::vector<MWAWCellContent::FormulaInstruction> &formula, long endPos, std::string &extra)
{
  formula.resize(0);

  MWAWInputStreamPtr input=m_parserState->m_input;
  long pos=input->tell();
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  bool ok=true, lastIsClosePara=false;
  while (!input->isEnd()) {
    pos=input->tell();
    if (pos >= endPos)
      break;
    int val, type=(int) input->readULong(1);
    MWAWCellContent::FormulaInstruction instr;
    if (type==0 && pos+1==endPos) // rare but seems ok
      break;
    switch (type) {
    case 5:
      if (pos+1+2 > endPos) {
        ok = false;
        break;
      }
      val = (int) input->readLong(2);
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Long;
      instr.m_longValue=val;
      break;
    case 7:
    case 8: { // date
      if (pos+1+10 > endPos) {
        ok = false;
        break;
      }
      double value;
      bool isNan;
      input->readDouble10(value, isNan);
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Double;
      instr.m_doubleValue=value;
      break;
    }
    case 9: {
      int sSz=(int) input->readULong(1);
      if (pos+1+2+sSz > endPos) {
        ok = false;
        break;
      }
      val=(char) input->readULong(1);
      if (val!='"' && val!='\'') {
        MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readFormula: the first string char seems odd\n"));
        f << "##fChar=" << val << ",";
      }
      std::string text("");
      for (int i=0; i<sSz; ++i) text+=(char) input->readULong(1);
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Text;
      instr.m_content=text;
      break;
    }
    case 0xf: // if delimiter with unknown value
      if (pos+1+1 > endPos) {
        ok = false;
        break;
      }
      val = (int) input->readLong(1);
      f << "unkn[sep]=" << val << ",";
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
      instr.m_content=";";
      break;
    case 0x32: {
      if (pos+1+1 > endPos) {
        ok = false;
        break;
      }
      val=(int) input->readULong(1);
      std::string funct("");
      if (val>=0 && val<0x60) {
        static char const* (s_functions[]) = {
          // 0
          "Abs(", "Sign(", "Rand()", "Sqrt(", "Sum(", "SumSq(", "Max(", "Min(",
          "Average(", "StDev(", "Pi()", "Sin(", "ASin(", "Cos(", "ACos(", "Tan(",
          // 1
          "ATan(", "Exp(", "Exp1(" /* fixme: exp(x+1)*/, "Ln(", "Ln1(" /* fixme: ln(n+1) */, "Log10(", "Annuity(", "Rate(",
          "PV(", "If(", "True()", "False()", "Len(", "Mid(", "Rept(", "Int(",
          // 2
          "Round(", "Text("/* add a format*/, "Dollar(", "Value(", "Number("/* of cell in list*/, "Row()", "Column()", "Index(",
          "Find(", "", "Page()", "Frame()" /* frame?*/, "IsError(", "IsNA(","NA()", "Day(",
          // 3
          "Month(", "Year("/* or diffyear*/, "DayOfYear("/* checkme*/, "SetDay(", "SetMonth(", "SetYear(",
          "AddMonth(", "AddYear(", "Today()", "PrintCyle()"/*print ?*/, "PrintStop("/*print stop*/, "Choose("/*checkme or select */, "Type(", "Find(", "SetFileName(", "Today()"/*hour*/,
          // 4
          "Today()"/*Minute*/, "", "Second(", "Minute(", "Hour(", "DayOfWeek(", "WeekOf(", "Slope(",
          "Intersect(", "LogSlope(", "LogIntersect(", "Mailing(", "Cell(", "Button(", "Today()"/*Now*/, "TotalPage()",

          // 5
          "", "DayOfWeekUS(", "WeekOfUS(", "", "", "", "", "",
          "", "", "", "", "", "", "", "",
        };
        funct=s_functions[val];
      }
      if (funct.empty()) {
        std::stringstream s;
        s << "Funct" << std::hex << val << std::dec << "(";
        funct=s.str();
      }
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Function;
      instr.m_content=funct;
      size_t functionLength=funct.length();
      if (functionLength>2 && instr.m_content[functionLength-1]==')') {
        instr.m_content.resize(functionLength-2);
        formula.push_back(instr);
        instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
        instr.m_content="(";
        formula.push_back(instr);
        instr.m_content=")";
      }
      else if (functionLength>1 && instr.m_content[functionLength-1]=='(') {
        instr.m_content.resize(functionLength-1);
        formula.push_back(instr);
        instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
        instr.m_content="(";
      }

      break;
    }
    default: {
      if ((type&0xCC)==0x80) { // checkme: maybe (type&0xC0)==0x80
        std::string error("");
        input->seek(-1, librevenge::RVNG_SEEK_CUR);
        ok=readCellInFormula(cellPos, false, instr, endPos, error);
        if (!ok) f << "cell=[" << error << "],";
        break;
      }
      if ((type&0x80)==0x80) {
        std::string error("");
        input->seek(-1, librevenge::RVNG_SEEK_CUR);
        ok=readCellInFormula(cellPos, true, instr, endPos, error);
        if (!ok) f << "cell[list]=[" << error << "],";
        break;
      }
      static char const* (s_operators[]) = {
        // 0
        "", "", "", "", "", "", "", "",
        "", "", "", "", "(", ")", ";", "",
        // 1
        /* fixme: we need to reconstruct the formula for operator or, and, ... */        "", "", "^", "", "", "*", "/", "",
        "And", "", "", "", "", "+", "-", "Or" ,
        // 2
        "&", "", "", "", "=", "<", ">", "<=",
        ">=", "<>", "", "", "", "", "", "",
      };
      std::string op("");
      if (type>=0 && type< 0x30)
        op=s_operators[type];
      if (op.empty()) {
        ok=false;
        break;
      }
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
      instr.m_content=op;
      break;
    }
    }
    if (!ok) {
      ascFile.addDelimiter(pos,'#');
      f << "###";
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readFormula: can not read a formula\n"));
      break;
    }
    // a cell ref followed Today(), Page(), ... so we must ignored it
    if (lastIsClosePara && (instr.m_type==MWAWCellContent::FormulaInstruction::F_Cell ||
                            instr.m_type==MWAWCellContent::FormulaInstruction::F_CellList)) {
      lastIsClosePara=false;
      continue;
    }
    lastIsClosePara= instr.m_type==MWAWCellContent::FormulaInstruction::F_Operator &&
                     instr.m_content==")";
    formula.push_back(instr);
  }
  extra=f.str();
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (ok) return true;

  f.str("");
  for (size_t i=0; i<formula.size(); ++i)
    f << formula[i];
  f << "," << extra;
  extra=f.str();
  formula.resize(0);
  return true;
}

bool RagTimeSpreadsheet::readFormulaV2(Vec2i const &cellPos, std::vector<MWAWCellContent::FormulaInstruction> &formula, long endPos, std::string &extra)
{
  formula.resize(0);

  MWAWInputStreamPtr input=m_parserState->m_input;
  long pos=input->tell();
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  long formulaEndPos=pos+1+(long) input->readULong(1);
  if (formulaEndPos>endPos) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readFormulaV2: the formula size seems bad\n"));
    return false;
  }
  ascFile.addDelimiter(pos,'|');
  bool ok=true;
  while (!input->isEnd()) {
    pos=input->tell();
    if (pos >= formulaEndPos)
      break;
    int val, type=(int) input->readULong(1);
    MWAWCellContent::FormulaInstruction instr;
    switch (type) {
    case 5:
      if (pos+1+2 > formulaEndPos) {
        ok = false;
        break;
      }
      val = (int) input->readLong(2);
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Long;
      instr.m_longValue=val;
      break;
    case 6: {
      if (pos+1+10 > formulaEndPos) {
        ok = false;
        break;
      }
      double value;
      bool isNan;
      input->readDouble10(value, isNan);
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Double;
      instr.m_doubleValue=value;
      break;
    }
    case 8: {
      if (pos+1+4 > formulaEndPos) {
        ok = false;
        break;
      }
      int Y=(int) input->readULong(2);
      int M=(int) input->readULong(1);
      int D=(int) input->readULong(1);
      double value;
      if (!MWAWCellContent::date2Double(Y,M,D,value)) {
        f << "#date,";
        break;
      }
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Double;
      instr.m_doubleValue=value;
      break;
    }
    case 9: {
      int sSz=(int) input->readULong(1);
      if (pos+1+2+sSz > formulaEndPos) {
        ok = false;
        break;
      }
      val=(char) input->readULong(1);
      if (val!='"' && val!='\'') {
        MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readFormulaV2: the first string char seems odd\n"));
        f << "##fChar=" << val << ",";
      }
      std::string text("");
      for (int i=0; i<sSz; ++i) text+=(char) input->readULong(1);
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Text;
      instr.m_content=text;
      break;
    }
    case 0xf: // if delimiter with unknown value
      if (pos+1+1 > formulaEndPos) {
        ok = false;
        break;
      }
      val = (int) input->readLong(1);
      f << "unkn[sep]=" << val << ",";
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
      instr.m_content=";";
      break;
    default: {
      if ((type&0xF0)==0x80) {
        long endCellPos=pos+1+(type-0x80);
        if (endCellPos > formulaEndPos) {
          ok = false;
          break;
        }
        std::string error("");
        ok=readCellInFormulaV2(cellPos, false, instr, endCellPos, error);
        if (!ok) f << "cell=[" << error << "],";
        break;
      }
      if ((type&0xF0)==0xc0 || (type&0xF0)==0xd0) {
        long endCellPos=pos+1+(type-0xc0);
        if (endCellPos > formulaEndPos) {
          ok = false;
          break;
        }
        std::string error("");
        ok=readCellInFormulaV2(cellPos, true, instr, endCellPos, error);
        if (!ok) f << "cell=[" << error << "],";
        break;
      }
      static char const* (s_functions[]) = {
        // 0
        "", "", "", "", "", "", "", "",
        "", "", "", "", "(", ")", ";", "",
        // 1
        /* fixme: we need to reconstruct the formula for operator or, and, ... */
        "", "", "^", "", "", "*", "/", "",
        "And", "", "", "", "", "+", "-", "Or" ,
        // 2
        "", "", "", "", "=", "<", ">", "<=",
        ">=", "<>", "", "", "", "", "", "",
        // 3
        "", "", "Abs(", "Sign(", "Rand()", "Sqrt(", "Sum(", "SumSq(",
        "Max(", "Min(", "Average(", "StDev(", "Pi()", "Sin(", "ASin(", "Cos(",
        // 4
        "ACos(", "Tan(", "ATan(", "Exp(", "Exp1(" /* fixme: exp(x+1)*/, "Ln(", "Ln1(" /* fixme: ln(n+1) */, "Log10(",
        "Annuity(", "Rate(", "PV(", "If(", "True()", "False()", "Len(", "Mid(",
        // 5
        "Rept(", "Int(", "Round(", "Text("/* add a format*/, "Dollar(", "Value(", "Number(", "Row()",
        "Column()", "Index(", "Find(", "", "Page()", "Frame()" /* frame?*/, "IsError(", "IsNA(",
        // 6
        "NA()", "Day(", "Month(", "Year(", "DayOfYear("/* checkme*/, "SetDay(", "SetMonth(", "SetYear(",
        "AddMonth(", "AddYear(", "Today()", "Funct6b()"/*print ?*/, "Funct6c("/*print stop*/, "Choose("/*checkme or select */, "Type(", "Find(",
        // 70
        "SetFileName(", "", "", "", "", "", "", "",
        "", "", "", "", "", "", "", "",
      };
      if (type>=0 && type < 0x80)
        instr.m_content=s_functions[type];
      size_t functionLength=instr.m_content.length();
      if (functionLength==0) {
        ok=false;
        break;
      }
      if (instr.m_content[0] >= 'A' && instr.m_content[0] <= 'Z') {
        instr.m_type=MWAWCellContent::FormulaInstruction::F_Function;
        if (functionLength>2 && instr.m_content[functionLength-1]==')') {
          instr.m_content.resize(functionLength-2);
          formula.push_back(instr);
          instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
          instr.m_content="(";
          formula.push_back(instr);
          instr.m_content=")";
        }
        else if (instr.m_content[functionLength-1]=='(') {
          instr.m_content.resize(functionLength-1);
          formula.push_back(instr);
          instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
          instr.m_content="(";
        }
      }
      else
        instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
      break;
    }
    }
    if (!ok) {
      ascFile.addDelimiter(pos,'#');
      f << "###";
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readFormulaV2: can not read a formula\n"));
      break;
    }
    formula.push_back(instr);
  }
  extra=f.str();
  input->seek(formulaEndPos, librevenge::RVNG_SEEK_SET);
  if (ok) return true;

  f.str("");
  for (size_t i=0; i<formula.size(); ++i)
    f << formula[i];
  f << "," << extra;
  extra=f.str();
  formula.resize(0);
  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
