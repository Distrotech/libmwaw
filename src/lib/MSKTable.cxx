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
#include "MWAWTextListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPictData.hxx"
#include "MWAWSubDocument.hxx"
#include "MWAWTable.hxx"

#include "MSKGraph.hxx"
#include "MSKParser.hxx"

#include "MSKTable.hxx"

/** Internal: the structures of a MSKTable */
namespace MSKTableInternal
{
////////////////////////////////////////
//! Internal: the chart of a MSKTable
struct Chart {
  //! constructor
  Chart(MSKGraph::Style const &style) : m_style(style), m_backgroundEntry(), m_zoneId(-1)
  {
    for (int i=0; i < 3; i++)
      m_textZonesId[i]=-1;
  }
  //! empty constructor
  Chart() : m_style(), m_backgroundEntry(), m_zoneId(-1)
  {
    for (int i=0; i < 3; i++)
      m_textZonesId[i]=-1;
  }

  //! the graphic style
  MWAWGraphicStyle m_style;
  //! the three text pictures
  int m_textZonesId[3];
  //! the background entry
  MWAWEntry m_backgroundEntry;
  //! the chart zone id (in the graph parser )
  int m_zoneId;
};


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
    m_rowsDim(), m_colsDim(), m_font(), m_cellsList()
  {
    m_style.m_surfaceColor = style.m_baseSurfaceColor;
  }
  //! empty constructor
  Table() : m_style(), m_numRows(0), m_numCols(0),  m_rowsDim(), m_colsDim(),
    m_font(), m_cellsList() { }

  //! try to find a cell
  Cell const *getCell(Vec2i const &pos) const
  {
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
  State() : m_version(-1), m_idChartMap(), m_idTableMap() { }

  //! the version
  int m_version;

  //! the map id->chart
  std::map<int, Chart> m_idChartMap;
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
bool MSKTable::sendTable(int zoneId)
{
  MWAWTextListenerPtr listener=m_parserState->m_textListener;
  if (!listener) return false;

  if (m_state->m_idTableMap.find(zoneId)==m_state->m_idTableMap.end()) {
    MWAW_DEBUG_MSG(("MSKTable::sendTable: can not find textbox %d\n", zoneId));
    return false;
  }
  MSKTableInternal::Table &table = m_state->m_idTableMap.find(zoneId)->second;

  // open the table
  size_t nCols = table.m_colsDim.size();
  size_t nRows = table.m_rowsDim.size();
  if (!nCols || !nRows) {
    MWAW_DEBUG_MSG(("MSKTable::sendTable: problem with dimensions\n"));
    return false;
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
    listener->openTableRow(float(table.m_rowsDim[row]), librevenge::RVNG_POINT);

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
          switch (c) {
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
  return true;
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

    input->seek(actPos+34, librevenge::RVNG_SEEK_SET);
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
        cell.m_font.set(MWAWFont::Script(20,librevenge::RVNG_PERCENT,80));
      else
        cell.m_font.set(MWAWFont::Script::super100());
    }
    if (fFlags & 0x40) {
      if (vers==1)
        cell.m_font.set(MWAWFont::Script(-20,librevenge::RVNG_PERCENT,80));
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
  }
  else
    m_state->m_idTableMap[zoneId]=table;
  return true;
}

////////////////////////////////////////////////////////////
// chart
////////////////////////////////////////////////////////////

void MSKTable::setChartZoneId(int chartId, int zoneId)
{
  if (m_state->m_idChartMap.find(chartId)==m_state->m_idChartMap.end()) {
    MWAW_DEBUG_MSG(("MSKTable::setChartZoneId: can not find chart %d\n", chartId));
    return;
  }
  MSKTableInternal::Chart &chart = m_state->m_idChartMap.find(chartId)->second;
  chart.m_zoneId = zoneId;
}

bool MSKTable::sendChart(int chartId)
{
  MWAWTextListenerPtr listener=m_parserState->m_textListener;
  if (!listener) {
    MWAW_DEBUG_MSG(("MSKTable::sendChart: can not find a listener\n"));
    return false;
  }
  if (m_state->m_idChartMap.find(chartId)==m_state->m_idChartMap.end()) {
    MWAW_DEBUG_MSG(("MSKTable::sendChart: can not find chart %d\n", chartId));
    return false;
  }
  MSKTableInternal::Chart &chart = m_state->m_idChartMap.find(chartId)->second;

  MWAWInputStreamPtr input=m_mainParser->getInput();
  MWAWPosition chartPos;
  if (chart.m_zoneId < 0 || !m_graphParser->getZonePosition(chart.m_zoneId, MWAWPosition::Frame, chartPos)) {
    MWAW_DEBUG_MSG(("MSKTable::sendChart: oops can not find chart bdbox %d[%d]\n", chartId, chart.m_zoneId));
    return false;
  }
  MWAWPosition pictPos(Vec2f(0,0), chartPos.size(),librevenge::RVNG_POINT);
  pictPos.setRelativePosition(MWAWPosition::Frame, MWAWPosition::XLeft, MWAWPosition::YTop);
  if (chart.m_backgroundEntry.valid()) {
    long actPos = input->tell();
#ifdef DEBUG_WITH_FILES
    if (1) {
      librevenge::RVNGBinaryData file;
      input->seek(chart.m_backgroundEntry.begin(), librevenge::RVNG_SEEK_SET);
      input->readDataBlock(chart.m_backgroundEntry.length(), file);
      static int volatile pictName = 0;
      libmwaw::DebugStream f;
      f << "Pict-" << ++pictName << ".pct";
      libmwaw::Debug::dumpFile(file, f.str().c_str());
    }
#endif

    input->seek(chart.m_backgroundEntry.begin(), librevenge::RVNG_SEEK_SET);
    Box2f naturalBox;
    MWAWPict::ReadResult res = MWAWPictData::check(input, (int)chart.m_backgroundEntry.length(), naturalBox);
    if (res == MWAWPict::MWAW_R_BAD) {
      MWAW_DEBUG_MSG(("MSKTable::sendChart: can not find the picture\n"));
    }
    else {
      input->seek(chart.m_backgroundEntry.begin(), librevenge::RVNG_SEEK_SET);
      shared_ptr<MWAWPict> pict(MWAWPictData::get(input, (int)chart.m_backgroundEntry.length()));

      librevenge::RVNGBinaryData data;
      std::string type;
      if (pict && pict->getBinary(data,type))
        listener->insertPicture(pictPos, data, type);
    }
    input->seek(actPos, librevenge::RVNG_SEEK_SET);
  }
  for (int i=0; i < 3; i++) {
    int cId=chart.m_textZonesId[i];
    MWAWPosition childPos;
    if (!m_graphParser->getZonePosition(cId, MWAWPosition::Frame, childPos)) {
      MWAW_DEBUG_MSG(("MSKTable::sendChart: oops can not find chart bdbox for child %d[%d]\n", i, cId));
      continue;
    }
    MWAWPosition textPos(pictPos);
    textPos.setOrigin(childPos.origin()-chartPos.origin());
    textPos.setSize(childPos.size());
    m_graphParser->send(cId, textPos);
  }

  return true;
}

bool MSKTable::readChart(int chartId, MSKGraph::Style const &style)
{
  MWAWInputStreamPtr input=m_mainParser->getInput();
  long pos = input->tell();
  if (version() <= 3 || !input->checkPosition(pos+306))
    return false;

  libmwaw::DebugFile &ascFile = m_mainParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(Chart):";

  MSKTableInternal::Chart chart(style);
  int val = (int) input->readLong(2);
  switch (val) {
  case 1:
    f << "bar,";
    break;
  case 2:
    f << "stacked,";
    break;
  case 3:
    f << "line,";
    break; // checkme
  case 4:
    f << "combo,";
    break; // checkme
  case 5:
    f << "pie,";
    break; // checkme
  case 6:
    f << "hi-lo-choose,";
    break; // checkme
  default:
    f << "#type=val";
    break;
  }
  for (int i = 0; i < 4; i++) {
    val = (int) input->readLong(2);
    if (val) f << "col" << i << "=" << val << ",";
  }
  f << "rows=";
  for (int i = 0; i < 2; i++) {
    val = (int) input->readLong(2);
    f << val;
    if (i==0) f << "-";
    else f << ",";
  }
  val = (int) input->readLong(2);
  if (val) f << "colLabels=" << val << ",";
  val = (int) input->readLong(2);
  if (val) f << "rowLabels=" << val << ",";
  std::string name("");
  int sz = (int) input->readULong(1);
  if (sz > 31) {
    MWAW_DEBUG_MSG(("MSKTable::readChart: string size is too long\n"));
    return false;
  }
  for (int i = 0; i < sz; i++) {
    char c = (char) input->readLong(1);
    if (!c) break;
    name+=c;
  }
  f << name << ",";
  input->seek(pos+50, librevenge::RVNG_SEEK_SET);
  for (int i = 0; i < 128; i++) { // always 0 ?
    val = (int) input->readLong(2);
    if (val) f << "g" << i << "=" << val << std::dec << ",";
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  pos = input->tell();
  ascFile.addPos(pos);
  ascFile.addNote("Chart(II)");
  input->seek(2428, librevenge::RVNG_SEEK_CUR);

  // three textbox
  for (int i = 0; i < 3; i++) {
    pos = input->tell();
    MWAWEntry childZone;
    chart.m_textZonesId[i] = m_graphParser->getEntryPicture(-9999, childZone, false, i+2);
    if (chart.m_textZonesId[i]<0) {
      MWAW_DEBUG_MSG(("MSKTable::readChart: can not find textbox\n"));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
  }
  // the background picture
  pos = input->tell();
  long dataSz = (long) input->readULong(4);
  long smDataSz = (long) input->readULong(2);
  if (!dataSz || (dataSz&0xFFFF) != smDataSz || !input->checkPosition(pos+4+dataSz)) {
    MWAW_DEBUG_MSG(("MSKTable::readChart: last pict size seems odd\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  MWAWEntry &background=chart.m_backgroundEntry;
  background.setBegin(pos+4);
  background.setLength(dataSz);
  ascFile.skipZone(background.begin(), background.end()-1);

  ascFile.addPos(pos);
  ascFile.addNote("Chart(picture)");
  input->seek(background.end(), librevenge::RVNG_SEEK_SET);

  // last the value ( by columns ? )
  for (int i = 0; i < 4; i++) {
    pos = input->tell();
    dataSz = (long) input->readULong(4);
    if (dataSz%0x10) {
      MWAW_DEBUG_MSG(("MSKTable::readChart: can not read end last zone\n"));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    f.str("");
    f << "Chart(A" << i << ")";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    int numLine = int(dataSz/0x10);
    for (int l = 0; l < numLine; l++) {
      f.str("");
      f << "Chart(A" << i << "-" << l << ")";
      ascFile.addPos(pos+4+0x10*l);
      ascFile.addNote(f.str().c_str());
    }
    input->seek(pos+4+dataSz, librevenge::RVNG_SEEK_SET);
  }
  if (m_state->m_idChartMap.find(chartId)!=m_state->m_idChartMap.end()) {
    MWAW_DEBUG_MSG(("MSKTable::readChart: oops a chart with id=%d already exists\n", chartId));
  }
  else
    m_state->m_idChartMap[chartId]=chart;
  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
