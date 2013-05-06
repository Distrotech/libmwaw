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

#include <libwpd/libwpd.h>

#include "MWAWCell.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWPictBasic.hxx"
#include "MWAWTable.hxx"

#include "CWParser.hxx"
#include "CWStruct.hxx"
#include "CWStyleManager.hxx"

#include "CWTable.hxx"

/** Internal: the structures of a CWTable */
namespace CWTableInternal
{
/** Internal: the border of a CWTable */
struct Border {
  //! the constructor
  Border() : m_styleId(-1), m_isSent(false) {}
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Border const &bord) {
    for (int i = 0; i < 2; i++) {
      Vec2f pos = bord.m_position[i];
      pos=1./256.*pos;
      o << pos;
      if (i == 0) o << "<->";
      else o << ",";
    }
    if (bord.m_styleId>0)
      o << "m_styleId?=" << bord.m_styleId << ",";
    return o;
  }

  /** the origin and the end of edge position : unit WPX_POINT/256 */
  Vec2i m_position[2];
  /// the style id
  int m_styleId;
  /// a flag to know if the border is already sent
  mutable bool m_isSent;
};

struct Table;
/** Internal: a cell inside a CWTable */
struct Cell : public MWAWTableCell {
  //! constructor
  Cell(Table &table) : MWAWTableCell(), m_table(table), m_size(), m_zoneId(0), m_styleId(-1) {
  }
  //! send the cell to a listener
  virtual bool send(MWAWContentListenerPtr listener);

  //! send the cell content to a listener
  virtual bool sendContent(MWAWContentListenerPtr listener);

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Cell const &cell) {
    o << reinterpret_cast<MWAWTableCell const &>(cell);
    Vec2f sz = cell.m_size;
    o << "size=" << sz << ",";
    if (cell.m_zoneId) o << "zone=" << cell.m_zoneId << ",";
    if (cell.m_styleId >= 0)o << "style=" << cell.m_styleId << ",";
    return o;
  }

  /** the table */
  Table &m_table;
  /** the cell size : unit WPX_POINT */
  Vec2f m_size;
  /** the cell zone ( 0 is no content ) */
  int m_zoneId;
  /** the list of border id : Left, Top, Right, Bottom

     Normally, one id but merge cells can have mutiple border
  */
  std::vector<int> m_bordersId[4];
  /// the style id
  int m_styleId;

private:
  Cell(Cell const &orig);
  Cell &operator=(Cell const &orig);
};

////////////////////////////////////////
////////////////////////////////////////
/** the struct which stores the Table */
struct Table : public CWStruct::DSET, public MWAWTable {
  friend struct Cell;
  //! constructor
  Table(CWStruct::DSET const &dset, CWTable &parser) :
    CWStruct::DSET(dset),MWAWTable(), m_parser(&parser), m_bordersList(), m_hasSomeLinesCell(false), m_mainPtr(-1) {
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Table const &doc) {
    o << static_cast<CWStruct::DSET const &>(doc);
    return o;
  }
  //! check that each child zone are valid
  void checkChildZones() {
    for (size_t i = 0; i < m_cellsList.size(); i++) {
      Cell *cell = reinterpret_cast<Cell *>(m_cellsList[i].get());
      if (!cell) continue;
      if (cell->m_zoneId > 0 && !okChildId(cell->m_zoneId))
        cell->m_zoneId = 0;
    }
  }
  //! a function used to send line cell
  void sendPreTableData(MWAWContentListenerPtr listener);
  //! return a cell corresponding to id
  Cell *get(int id) {
    if (id < 0 || id >= numCells()) {
      MWAW_DEBUG_MSG(("CWTableInteral::Table::get: cell %d does not exists\n",id));
      return 0;
    }
    return reinterpret_cast<Cell *>(MWAWTable::get(id).get());
  }

  /** the main parser */
  CWTable *m_parser;
  /** the list of border */
  std::vector<Border> m_bordersList;
  /** true if some cell have some line */
  bool m_hasSomeLinesCell;
  /** the relative main pointer */
  long m_mainPtr;
private:
  Table(Table const &orig);
  Table &operator=(Table const &orig);
};

bool Cell::send(MWAWContentListenerPtr listener)
{
  if (!listener) return true;
  WPXPropertyList pList;
  MWAWCell cell;
  cell.position() = m_position;
  cell.setNumSpannedCells(m_numberCellSpanned);
  m_table.m_parser->updateCell(*this, cell, pList);
  listener->openTableCell(cell, pList);
  sendContent(listener);
  listener->closeTableCell();
  return true;
}

bool Cell::sendContent(MWAWContentListenerPtr listener)
{
  if (!listener) return true;
  if (m_zoneId <= 0)
    listener->insertChar(' ');
  else
    m_table.m_parser->askMainToSendZone(m_zoneId);
  return true;
}

void Table::sendPreTableData(MWAWContentListenerPtr listener)
{
  if (!listener || !m_hasSomeLinesCell) return;
  CWStyleManager &styleManager= *(m_parser->m_styleManager.get());
  for (int c = 0; c < numCells(); c++) {
    Cell *cell= get(c);
    if (!cell) continue;

    CWStyleManager::Style style;
    if (cell->m_styleId < 0 || !styleManager.get(cell->m_styleId, style))
      continue;
    CWStyleManager::KSEN ksen;
    if (style.m_ksenId < 0 || !styleManager.get(style.m_ksenId, ksen) ||
        !(ksen.m_lines&3))
      continue;
    CWStyleManager::Graphic graph;
    if (style.m_graphicId >= 0)
      styleManager.get(style.m_graphicId, graph);

    Box2i box = cell->box();
    shared_ptr<MWAWPictLine> lines[2];
    if (ksen.m_lines & 1)
      lines[0].reset(new MWAWPictLine(Vec2i(0,0), box.size()));
    if (ksen.m_lines & 2)
      lines[1].reset(new MWAWPictLine(Vec2i(0,box.size()[1]), Vec2i(box.size()[0], 0)));
    MWAWColor lColor = graph.getLineColor();
    for (int i = 0; i < 2; i++) {
      if (!lines[i]) continue;
      lines[i]->setLineWidth((float)graph.m_lineWidth);
      if (!lColor.isBlack())
        lines[i]->setLineColor(lColor);

      WPXBinaryData data;
      std::string type;
      if (!lines[i]->getBinary(data,type)) continue;

      MWAWPosition pos(box[0], box.size(), WPX_POINT);
      pos.setRelativePosition(MWAWPosition::Frame);
      pos.setOrder(-1);
      listener->insertPicture(pos, data, type);
    }
  }
}

////////////////////////////////////////
//! Internal: the state of a CWTable
struct State {
  //! constructor
  State() : m_tableMap() {
  }
  //! map id -> table
  std::map<int, shared_ptr<Table> > m_tableMap;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
CWTable::CWTable(CWParser &parser) :
  m_parserState(parser.getParserState()), m_state(new CWTableInternal::State),
  m_mainParser(&parser), m_styleManager(parser.m_styleManager)
{
}

CWTable::~CWTable()
{ }

int CWTable::version() const
{
  return m_parserState->m_version;
}

bool CWTable::askMainToSendZone(int number)
{
  return m_mainParser->sendZone(number);
}

// fixme
int CWTable::numPages() const
{
  return 1;
}
////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////
void CWTable::updateCell(CWTableInternal::Cell const &cell, MWAWCell &rCell, WPXPropertyList &pList)
{
  pList=WPXPropertyList();
  CWStyleManager::Style style;
  if (cell.m_styleId >= 0 && m_styleManager->get(cell.m_styleId, style)) {
    CWStyleManager::Graphic graph;
    if (style.m_graphicId >= 0 && m_styleManager->get(style.m_graphicId, graph)) {
      MWAWColor sColor = graph.getSurfaceColor();
      if (!sColor.isWhite())
        rCell.setBackgroundColor(sColor);
    }
    CWStyleManager::KSEN ksen;
    if (style.m_ksenId >= 0 && m_styleManager->get(style.m_ksenId, ksen)) {
      switch(ksen.m_valign) {
      case 1:
        rCell.setVAlignement(MWAWCell::VALIGN_CENTER);
        break;
      case 2:
        rCell.setVAlignement(MWAWCell::VALIGN_BOTTOM);
        break;
      default:
        break;
      }
    }
  }

  int numTableBorders = (int) cell.m_table.m_bordersList.size();
  // now look for border id: L,T,R,B
  for (int w = 0; w < 4; w++) {
    size_t numBorders = cell.m_bordersId[w].size();
    if (!numBorders) continue;

    int bId = cell.m_bordersId[w][0];
    bool sameBorders = true;
    for (size_t b = 1; b < numBorders; b++) {
      if (cell.m_bordersId[w][b]!=bId) {
        sameBorders = false;
        break;
      }
    }
    /** fixme: check that the opposite has a border, if not print the first border */
    if (!sameBorders) continue;
    if (bId < 0 || bId >= numTableBorders) {
      MWAW_DEBUG_MSG(("CWTable::updateCell: can not find the border definition\n"));
      continue;
    }
    CWTableInternal::Border const &border = cell.m_table.m_bordersList[size_t(bId)];
    CWStyleManager::Style bStyle;
    if (border.m_isSent || border.m_styleId < 0 || !m_styleManager->get(border.m_styleId, bStyle))
      continue;
    border.m_isSent = true;
    CWStyleManager::Graphic graph;
    bool haveGraph = false;
    if (bStyle.m_graphicId >= 0)
      haveGraph = m_styleManager->get(bStyle.m_graphicId, graph);
    CWStyleManager::KSEN ksen;
    bool haveKSEN = false;
    if (bStyle.m_ksenId >= 0)
      haveKSEN = m_styleManager->get(bStyle.m_ksenId, ksen);

    MWAWBorder bord;
    if (haveGraph && graph.m_lineWidth==0)
      bord.m_style = MWAWBorder::None;
    else if (haveKSEN) {
      bord.m_style = ksen.m_lineType;
      bord.m_type = ksen.m_lineRepeat;
      if (bord.m_type == MWAWBorder::Double)
        bord.m_width = 0.5f*float(graph.m_lineWidth);
      else
        bord.m_width = (float) graph.m_lineWidth;
      bord.m_color = graph.getLineColor();
    }
    static int const wh[] = { libmwaw::LeftBit, libmwaw::TopBit, libmwaw::RightBit, libmwaw::BottomBit};
    rCell.setBorders(wh[w], bord);
  }
}

////////////////////////////////////////////////////////////
// a document part
////////////////////////////////////////////////////////////
shared_ptr<CWStruct::DSET> CWTable::readTableZone
(CWStruct::DSET const &zone, MWAWEntry const &entry, bool &complete)
{
  complete = false;
  if (!entry.valid() || zone.m_fileType != 6 || entry.length() < 32)
    return shared_ptr<CWStruct::DSET>();
  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos+8+16, WPX_SEEK_SET); // avoid header+8 generic number
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  shared_ptr<CWTableInternal::Table> tableZone(new CWTableInternal::Table(zone, *this));

  f << "Entries(TableDef):" << *tableZone << ",";
  float dim[2];
  for (int i = 0; i < 2; i++) dim[i] = float(input->readLong(4))/256.f;
  f << "dim=" << dim[0] << "x" << dim[1] << ",";
  long val;
  for (int i = 0; i < 3; i++) {
    // f1=parentZoneId ?
    val = input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  tableZone->m_mainPtr = (long) input->readULong(4);
  f << "PTR=X" << std::hex << tableZone->m_mainPtr << std::dec << ",";
  for (int i = 0; i < 2; i++) { // find 0,0 or -1,-1 here
    val = input->readLong(2);
    if (val) f << "f" << i+3 << "=" << val << ",";
  }
  f << "relPtr=PTR+[" << std::hex;
  for (int i = 0; i < 3; i++) {
    /** find 3 ptr here in general >= PTR, very often PTR+4,PTR+8,PTR+c,
     but can be more complex for instance PTR+354,PTR-6924,PTR+7fc, */
    val = (long) input->readULong(4);
    if (val > tableZone->m_mainPtr)
      f << val-tableZone->m_mainPtr << ",";
    else
      f << "-" << tableZone->m_mainPtr-val << ",";
  }
  f << std::dec << "],";
  ascFile.addDelimiter(input->tell(), '|');
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  // read the last part
  long data0Length = zone.m_dataSz;
  long N = zone.m_numData;
  if (entry.length() -8-12 != data0Length*N + zone.m_headerSz) {
    if (data0Length == 0 && N) {
      MWAW_DEBUG_MSG(("CWTable::readTableZone: can not find definition size\n"));
      input->seek(entry.end(), WPX_SEEK_SET);
      return shared_ptr<CWStruct::DSET>();
    }

    MWAW_DEBUG_MSG(("CWTable::readTableZone: unexpected size for zone definition, try to continue\n"));
  }

  if (long(input->tell())+N*data0Length > entry.end()) {
    MWAW_DEBUG_MSG(("CWTable::readTableZone: file is too short\n"));
    return shared_ptr<CWStruct::DSET>();
  }
  if (N) {
    MWAW_DEBUG_MSG(("CWTable::readTableZone: find some tabledef !!!\n"));
    input->seek(entry.end()-N*data0Length, WPX_SEEK_SET);

    for (int i = 0; i < N; i++) {
      pos = input->tell();

      f.str("");
      f << "TableDef#";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      input->seek(pos+data0Length, WPX_SEEK_SET);
    }
  }

  input->seek(entry.end(), WPX_SEEK_SET);

  pos = input->tell();
  bool ok = readTableBorders(*tableZone);
  if (ok) {
    pos = input->tell();
    ok = readTableCells(*tableZone);
  }
  /** three fields which seems to follows the list of cells
      zone 0 : looks like a list of integer : related to last selected border ?
      zone 1 : looks like a list of integer : unknown meaning
   */
  for (int i = 0; ok && i < 2; i++) {
    std::stringstream s;
    s << "TableUnknown-" << i;
    std::vector<int> res;
    pos = input->tell();
    ok = m_mainParser->readStructIntZone(s.str().c_str(), false, 2, res);
  }
  if (ok) {
    pos = input->tell();
    ok = readTablePointers(*tableZone);
    if (!ok) {
      input->seek(pos, WPX_SEEK_SET);
      ok = m_mainParser->readStructZone("TablePointers", false);
    }
  }
  if (ok) {
    pos = input->tell();
    ok = readTableBordersId(*tableZone);
  }

  if (!ok)
    input->seek(pos, WPX_SEEK_SET);

  if (m_state->m_tableMap.find(tableZone->m_id) != m_state->m_tableMap.end()) {
    MWAW_DEBUG_MSG(("CWTable::readTableZone: zone %d already exists!!!\n", tableZone->m_id));
  } else
    m_state->m_tableMap[tableZone->m_id] = tableZone;

  tableZone->m_otherChilds.push_back(tableZone->m_id+1);
  return tableZone;
}

bool CWTable::sendZone(int number)
{
  std::map<int, shared_ptr<CWTableInternal::Table> >::iterator iter
    = m_state->m_tableMap.find(number);
  if (iter == m_state->m_tableMap.end())
    return false;
  shared_ptr<CWTableInternal::Table> table = iter->second;
  table->m_parsed = true;
  if (table->okChildId(number+1))
    m_mainParser->forceParsed(number+1);

  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener)
    return true;

  table->checkChildZones();
  if (table->sendTable(listener))
    return true;
  return table->sendAsText(listener);
}

void CWTable::flushExtra()
{
  std::map<int, shared_ptr<CWTableInternal::Table> >::iterator iter
    = m_state->m_tableMap.begin();
  for ( ; iter !=  m_state->m_tableMap.end(); iter++) {
    shared_ptr<CWTableInternal::Table> table = iter->second;
    if (table->m_parsed)
      continue;
    if (m_parserState->m_listener) m_parserState->m_listener->insertEOL();
    sendZone(iter->first);
  }
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool CWTable::readTableBorders(CWTableInternal::Table &table)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos) {
    input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWTable::readTableBorders: file is too short\n"));
    return false;
  }

  input->seek(pos+4, WPX_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(TableBorders):";
  int N = (int) input->readULong(2);
  f << "N=" << N << ",";
  int val =(int)  input->readLong(2);
  if (val != -1) f << "f0=" << val << ",";
  val = (int) input->readLong(2);
  if (val) f << "f1=" << val << ",";
  int fSz = (int) input->readLong(2);
  if (sz != 12+fSz*N || fSz < 18) {
    input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWTable::readTableBorders: find odd data size\n"));
    return false;
  }
  for (int i = 2; i < 4; i++) {
    val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";

  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = input->tell();
    CWTableInternal::Border border;
    f.str("");
    f << "TableBorders-" << i << ":";
    int posi[4];
    for (int j = 0; j < 4; j++) posi[j] = (int) input->readLong(4);
    border.m_position[0] = Vec2i(posi[1], posi[0]);
    border.m_position[1] = Vec2i(posi[3], posi[2]);
    border.m_styleId = (int) input->readULong(2);
    table.m_bordersList.push_back(border);
    f << border;

    CWStyleManager::Style style;
    if (border.m_styleId < 0) ;
    else if (!m_styleManager->get(border.m_styleId, style)) {
      MWAW_DEBUG_MSG(("CWTable::readTableBorders: can not find cell style\n"));
      f << "###style";
    } else {
      CWStyleManager::KSEN ksen;
      if (style.m_ksenId >= 0 && m_styleManager->get(style.m_ksenId, ksen)) {
        f << "ksen=[" << ksen << "],";
      }
      CWStyleManager::Graphic graph;
      if (style.m_graphicId >= 0 && m_styleManager->get(style.m_graphicId, graph)) {
        f << "graph=[" << graph << "],";
      }
      //f << "[" << style << "],";
    }

    if (long(input->tell()) != pos+fSz)
      ascFile.addDelimiter(input->tell(), '|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+fSz, WPX_SEEK_SET);
  }

  input->seek(endPos, WPX_SEEK_SET);
  return true;
}

bool CWTable::readTableCells(CWTableInternal::Table &table)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos) {
    input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWTable::readTableCells: file is too short\n"));
    return false;
  }

  input->seek(pos+4, WPX_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(TableCell):";
  int N = (int) input->readULong(2);
  f << "N=" << N << ",";
  int val = (int) input->readLong(2);
  if (val != -1) f << "f0=" << val << ",";
  val = (int) input->readLong(2);
  if (val) f << "f1=" << val << ",";
  int fSz = (int) input->readLong(2);
  if (sz != 12+fSz*N || fSz < 32) {
    input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWTable::readTableCells: find odd data size\n"));
    return false;
  }
  for (int i = 2; i < 4; i++) {
    val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";

  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = input->tell();
    shared_ptr<CWTableInternal::Cell> cell(new CWTableInternal::Cell(table));
    int posi[6];
    for (int j = 0; j < 6; j++) posi[j] = (int) input->readLong(4);
    Box2f box = Box2f(Vec2f(float(posi[1]), float(posi[0])), Vec2f(float(posi[3]), float(posi[2])));
    box.scale(1./256.);
    cell->setBox(box);
    cell->m_size = 1./256.*Vec2f(float(posi[5]), float(posi[4]));
    cell->m_zoneId = (int) input->readULong(4);
    val = (int) input->readLong(2);
    if (val) // find one time a number here, another id?...
      f << "#unkn=" << val << ",";
    cell->m_styleId = (int) input->readULong(2);
    table.add(cell);
    if (cell->m_zoneId)
      table.m_otherChilds.push_back(cell->m_zoneId);
    f.str("");
    f << "TableCell-" << i << ":" << *cell;
    CWStyleManager::Style style;
    if (cell->m_styleId < 0) ;
    else if (!m_styleManager->get(cell->m_styleId, style)) {
      MWAW_DEBUG_MSG(("CWTable::readTableCells: can not find cell style\n"));
      f << "###style";
    } else {
      CWStyleManager::KSEN ksen;
      if (style.m_ksenId >= 0 && m_styleManager->get(style.m_ksenId, ksen)) {
        if (ksen.m_lines & 3) table.m_hasSomeLinesCell = true;
        f << "ksen=[" << ksen << "],";
      }
      CWStyleManager::Graphic graph;
      if (style.m_graphicId >= 0 && m_styleManager->get(style.m_graphicId, graph)) {
        f << "graph=[" << graph << "],";
      }
      //f << "[" << style << "],";
    }

    if (long(input->tell()) != pos+fSz)
      ascFile.addDelimiter(input->tell(), '|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+fSz, WPX_SEEK_SET);
  }

  input->seek(endPos, WPX_SEEK_SET);
  return true;
}

bool CWTable::readTableBordersId(CWTableInternal::Table &table)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  int numCells = table.numCells();
  int numBorders = int(table.m_bordersList.size());
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  for (int i = 0; i < 4*numCells; i++) {
    CWTableInternal::Cell *cell = table.get(i/4);
    long pos = input->tell();
    long sz = (long) input->readULong(4);
    long endPos = pos+4+sz;
    input->seek(endPos, WPX_SEEK_SET);
    if (long(input->tell()) != endPos) {
      input->seek(pos, WPX_SEEK_SET);
      MWAW_DEBUG_MSG(("CWTable::readTableBordersId: file is too short\n"));
      return false;
    }

    input->seek(pos+4, WPX_SEEK_SET);
    libmwaw::DebugStream f;
    f << "Entries(TableBordersId)[" << i/4 << "," << i%4 << "],";
    int N = (int)input->readULong(2);
    f << "N=" << N << ",";
    int val = (int)input->readLong(2);
    if (val != -1) f << "f0=" << val << ",";
    val = (int)input->readLong(2);
    if (val) f << "f1=" << val << ",";
    int fSz = (int)input->readLong(2);
    if (N==0 || sz != 12+fSz*N || fSz < 2) {
      input->seek(pos, WPX_SEEK_SET);
      MWAW_DEBUG_MSG(("CWTable::readTableBordersId: find odd data size\n"));
      return false;
    }
    for (int j = 2; j < 4; j++) {
      val = (int)input->readLong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }

    std::vector<int> idsList;
    for (int j = 0; j < N; j++) {
      int id = (int)input->readLong(2);
      if (id < 0 || id >= numBorders) {
        input->seek(pos, WPX_SEEK_SET);
        MWAW_DEBUG_MSG(("CWTable::readTableBordersId: unexpected id\n"));
        return false;
      }
      idsList.push_back(id);
      if (j)
        f << "bordId" << j << "=" << id << ",";
      else
        f << "bordId=" << id << ",";
    }
    if (cell)
      cell->m_bordersId[i%4] = idsList;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(endPos, WPX_SEEK_SET);
  }
  return true;
}

bool CWTable::readTablePointers(CWTableInternal::Table &table)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos) {
    input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWTable::readTablePointers: file is too short\n"));
    return false;
  }

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  if (!sz) {
    // find this one time as the last entry which ends the table
    ascFile.addPos(pos);
    ascFile.addNote("NOP");
    return true;
  }
  input->seek(pos+4, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(TablePointers):";
  int N = (int) input->readULong(2);
  if (N != table.numCells()) {
    MWAW_DEBUG_MSG(("CWTable::readTablePointers: the number of pointers seems odd\n"));
    f << "###";
  }
  f << "N=" << N << ",";
  long val = input->readLong(2);
  if (val != -1) f << "f0=" << val << ",";
  val = input->readLong(2);
  if (val) f << "f1=" << val << ",";
  int fSz = (int) input->readLong(2);
  if (sz != 12+fSz*N || fSz < 16) {
    input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWTable::readTablePointers: find odd data size\n"));
    return false;
  }
  for (int i = 2; i < 4; i++) { // normally 0, 1
    val = input->readLong(2);
    if (val != i-2) f << "f" << i << "=" << val << ",";

  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    f << "TablePointers-" << i << ":PTR+[" << std::hex;
    for (int j = 0; j < 4; j++) {
      val = (long) input->readULong(4);
      if (val > table.m_mainPtr) f << val-table.m_mainPtr << ",";
      else f << "-" << table.m_mainPtr-val << ",";
    }
    f << "]" << std::dec;
    if (long(input->tell()) != pos+fSz)
      ascFile.addDelimiter(input->tell(), '|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+fSz, WPX_SEEK_SET);
  }

  input->seek(endPos, WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////


// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
