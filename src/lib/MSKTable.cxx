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
#include <map>
#include <sstream>

#include "libmwaw_internal.hxx"

#include "MWAWCell.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWSubDocument.hxx"
#include "MWAWTable.hxx"

#include "MSKGraph.hxx"
#include "MSKParser.hxx"

#include "MSKTable.hxx"

/** Internal: the structures of a MSKTable */
namespace MSKTableInternal
{
////////////////////////////////////////
//! Internal: the table of a MSKTable
struct Table {
  //! the cell content
  struct Cell {
    Cell() : m_pos(-1,-1), m_font(), m_text("") {}
    //! the cell position
    Vec2i m_pos;
    //! the font
    MWAWFont m_font;
    //! the text
    std::string m_text;
  };
  //! constructor
  Table(MSKGraph::Style const &style) : m_style(style), m_numRows(0), m_numCols(0),
    m_rowsDim(), m_colsDim(), m_font(), m_cellsList() {
    m_style.m_surfaceColor = style.m_baseSurfaceColor;
  }
  //! empty constructor
  Table() : m_style(), m_numRows(0), m_numCols(0),  m_rowsDim(), m_colsDim(),
    m_font(), m_cellsList() { }

  //! try to find a cell
  Cell const *getCell(Vec2i const &pos) const {
    for (size_t i = 0; i < m_cellsList.size(); i++) {
      if (m_cellsList[i].m_pos == pos)
        return &m_cellsList[i];
    }
    return 0;
  }

  //! the graphic style
  MWAWGraphicStyle m_style;
  int m_numRows /** the number of rows*/, m_numCols/** the number of columns*/;
  std::vector<int> m_rowsDim/**the rows dimensions*/, m_colsDim/*the columns dimensions*/;
  //! the default font
  MWAWFont m_font;
  //! the list of cell
  std::vector<Cell> m_cellsList;
};

////////////////////////////////////////
//! Internal: the state of a MSKTable
struct State {
  //! constructor
  State() : m_version(-1), m_idTableMap() { }

  //! the version
  int m_version;

  //! the map id->table
  std::map<int, Table> m_idTableMap;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MSKTable::MSKTable(MSKParser &parser, MSKGraph &graph) :
  m_parserState(parser.getParserState()), m_state(new MSKTableInternal::State),
  m_mainParser(&parser), m_graphParser(&graph)
{
}

MSKTable::~MSKTable()
{ }

int MSKTable::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}

////////////////////////////////////////////////////////////
// table
////////////////////////////////////////////////////////////
void MSKTable::sendTable(int zoneId)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) return;

  if (m_state->m_idTableMap.find(zoneId)==m_state->m_idTableMap.end()) {
    MWAW_DEBUG_MSG(("MSKTable::sendTable: can not find textbox %d\n", zoneId));
    return;
  }
  MSKTableInternal::Table &table = m_state->m_idTableMap.find(zoneId)->second;

  // open the table
  size_t nCols = table.m_colsDim.size();
  size_t nRows = table.m_rowsDim.size();
  if (!nCols || !nRows) {
    MWAW_DEBUG_MSG(("MSKTable::sendTable: problem with dimensions\n"));
    return;
  }
  std::vector<float> colsDims(nCols);
  for (size_t c = 0; c < nCols; c++) colsDims[c] = float(table.m_colsDim[c]);
  MWAWTable theTable(MWAWTable::TableDimBit);
  theTable.setColsSize(colsDims);
  listener->openTable(theTable);

  int const borderPos = libmwaw::TopBit | libmwaw::RightBit |
                        libmwaw::BottomBit | libmwaw::LeftBit;
  MWAWBorder border, internBorder;
  internBorder.m_width=0.5;
  internBorder.m_color=MWAWColor(0xC0,0xC0,0xC0);
  MWAWParagraph para;
  para.m_justify=MWAWParagraph::JustificationCenter;
  for (size_t row = 0; row < nRows; row++) {
    listener->openTableRow(float(table.m_rowsDim[row]), WPX_POINT);

    for (size_t col = 0; col < nCols; col++) {
      MWAWCell cell;
      Vec2i cellPosition(Vec2i((int)col,(int)row));
      cell.setPosition(cellPosition);
      cell.setBorders(borderPos, border);
      int internWhat=0;
      if (col!=0) internWhat|=libmwaw::LeftBit;
      if (col+1!=nCols) internWhat|=libmwaw::RightBit;
      if (row!=0) internWhat|=libmwaw::TopBit;
      if (row+1!=nRows) internWhat|=libmwaw::BottomBit;
      cell.setBorders(internWhat, internBorder);
      if (!table.m_style.m_surfaceColor.isWhite())
        cell.setBackgroundColor(table.m_style.m_surfaceColor);
      listener->setParagraph(para);
      listener->openTableCell(cell);

      MSKTableInternal::Table::Cell const *tCell=table.getCell(cellPosition);
      if (tCell) {
        listener->setFont(tCell->m_font);
        size_t nChar = tCell->m_text.size();
        for (size_t ch = 0; ch < nChar; ch++) {
          unsigned char c = (unsigned char) tCell->m_text[ch];
          switch(c) {
          case 0x9:
            MWAW_DEBUG_MSG(("MSKTable::sendTable: find a tab\n"));
            listener->insertChar(' ');
            break;
          case 0xd:
            listener->insertEOL();
            break;
          default:
            listener->insertCharacter(c);
            break;
          }
        }
      }

      listener->closeTableCell();
    }
    listener->closeTableRow();
  }

  // close the table
  listener->closeTable();
}

bool MSKTable::readTable(int numCol, int numRow, int zoneId, MSKGraph::Style const &style)
{
  int vers=version();
  MWAWInputStreamPtr input=m_mainParser->getInput();
  long actPos = input->tell();
  libmwaw::DebugFile &ascFile = m_mainParser->ascii();
  libmwaw::DebugStream f, f2;
  f << "Entries(Table): ";

  MSKTableInternal::Table table(style);
  table.m_numRows=numRow;
  table.m_numCols=numCol;
  // first we read the dim
  for (int i = 0; i < 2; i++) {
    std::vector<int> &dim = i==0 ? table.m_rowsDim : table.m_colsDim;
    dim.resize(0);
    int sz = (int) input->readLong(4);
    if (i == 0 && sz != 2*table.m_numRows) return false;
    if (i == 1 && sz != 2*table.m_numCols) return false;

    if (i == 0) f << "rowS=(";
    else f << "colS=(";

    for (int j = 0; j < sz/2; j++) {
      int val = (int) input->readLong(2);

      if (val < -10) return false;

      dim.push_back(val);
      f << val << ",";
    }
    f << "), ";
  }

  long sz = input->readLong(4);
  f << "szOfCells=" << sz;
  ascFile.addPos(actPos);
  ascFile.addNote(f.str().c_str());

  actPos = input->tell();
  long endPos = actPos+sz;
  // now we read the data for each size
  while (input->tell() != endPos) {
    f.str("");
    actPos = input->tell();
    MSKTableInternal::Table::Cell cell;
    int y = (int) input->readLong(2);
    int x = (int) input->readLong(2);
    cell.m_pos = Vec2i(x,y);
    if (x < 0 || y < 0 ||
        x >= table.m_numCols || y >= table.m_numRows) return false;

    f << "Table:("<< cell.m_pos << "):";
    int nbChar = (int) input->readLong(1);
    if (nbChar < 0 || actPos+5+nbChar > endPos) return false;

    std::string fName("");
    for (int c = 0; c < nbChar; c++)
      fName +=(char) input->readLong(1);

    input->seek(actPos+34, WPX_SEEK_SET);
    f << std::hex << "unk=" << input->readLong(2) << ", "; // 0|827
    int v = (int) input->readLong(2);
    if (v) f << "f0=" << v << ", ";
    int fSize = (int) input->readLong(2);
    v = (int) input->readLong(2);
    if (v) f2 << "unkn0=" << v << ", ";
    int fFlags = (int) input->readLong(2);

    nbChar = (int) input->readLong(4);
    if (nbChar <= 0 || input->tell()+nbChar > endPos) return false;

    v = (int) input->readLong(2);
    if (v) f << "f1=" << v << ", ";
    int fColors = (int) input->readLong(2);
    v = (int) input->readLong(2);
    if (v) f << "f2=" << v << ", ";
    int bgColors = (int) input->readLong(2);
    if (bgColors)
      f2 << std::dec << "bgColorId(?)=" << bgColors << ", "; // indexed

    cell.m_font=MWAWFont(m_parserState->m_fontConverter->getId(fName), float(fSize));
    uint32_t flags = 0;
    if (fFlags & 0x1) flags |= MWAWFont::boldBit;
    if (fFlags & 0x2) flags |= MWAWFont::italicBit;
    if (fFlags & 0x4) cell.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (fFlags & 0x8) flags |= MWAWFont::embossBit;
    if (fFlags & 0x10) flags |= MWAWFont::shadowBit;
    if (fFlags & 0x20) {
      if (vers==1)
        cell.m_font.set(MWAWFont::Script(20,WPX_PERCENT,80));
      else
        cell.m_font.set(MWAWFont::Script::super100());
    }
    if (fFlags & 0x40) {
      if (vers==1)
        cell.m_font.set(MWAWFont::Script(-20,WPX_PERCENT,80));
      else
        cell.m_font.set(MWAWFont::Script::sub100());
    }
    cell.m_font.setFlags(flags);

    if (fColors != 0xFF) {
      MWAWColor col;
      if (m_mainParser->getColor(fColors,col,3))
        cell.m_font.setColor(col);
      else
        f << "#colId=" << fColors << ",";
    }
    f << "[" << cell.m_font.getDebugString(m_parserState->m_fontConverter) << "," << f2.str()<< "],";
    // check what happens, if the size of text is greater than 4
    for (int c = 0; c < nbChar; c++)
      cell.m_text+=(char) input->readLong(1);
    f << cell.m_text;

    table.m_cellsList.push_back(cell);

    ascFile.addPos(actPos);
    ascFile.addNote(f.str().c_str());
  }
  if (m_state->m_idTableMap.find(zoneId)!=m_state->m_idTableMap.end()) {
    MWAW_DEBUG_MSG(("MSKTable::readTable: oops a table with id=%d already exists\n", zoneId));
  } else
    m_state->m_idTableMap[zoneId]=table;
  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
