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
#include "MWAWListener.hxx"
#include "MWAWParser.hxx"
#include "MWAWPosition.hxx"
#include "MWAWTable.hxx"

#include "ClarisWksDocument.hxx"
#include "ClarisWksStruct.hxx"
#include "ClarisWksStyleManager.hxx"

#include "ClarisWksTable.hxx"

/** Internal: the structures of a ClarisWksTable */
namespace ClarisWksTableInternal
{
/** Internal: the border of a ClarisWksTable */
struct Border {
  //! the constructor
  Border() : m_styleId(-1), m_isSent(false) {}
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Border const &bord)
  {
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

  /** the origin and the end of edge position : unit librevenge::RVNG_POINT/256 */
  Vec2i m_position[2];
  /// the style id
  int m_styleId;
  /// a flag to know if the border is already sent
  mutable bool m_isSent;
};

struct Table;
/** Internal: a cell inside a ClarisWksTable */
struct TableCell : public MWAWCell {
  //! constructor
  TableCell() : MWAWCell(), m_zoneId(0), m_styleId(-1)
  {
  }
  //! use table to finish updating cell
  void update(Table const &table);

  //! send the cell content to a listener
  virtual bool sendContent(MWAWListenerPtr listener, MWAWTable &table);

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, TableCell const &cell)
  {
    o << static_cast<MWAWCell const &>(cell);
    if (cell.m_zoneId) o << "zone=" << cell.m_zoneId << ",";
    if (cell.m_styleId >= 0)o << "style=" << cell.m_styleId << ",";
    return o;
  }

  /** the cell zone ( 0 is no content ) */
  int m_zoneId;
  /** the list of border id : Left, Top, Right, Bottom

     Normally, one id but merge cells can have mutiple border
  */
  std::vector<int> m_bordersId[4];
  /// the style id
  int m_styleId;

private:
  TableCell(TableCell const &orig);
  TableCell &operator=(TableCell const &orig);
};

////////////////////////////////////////
////////////////////////////////////////
/** the struct which stores the Table */
struct Table : public ClarisWksStruct::DSET, public MWAWTable {
  friend struct TableCell;
  //! constructor
  Table(ClarisWksStruct::DSET const &dset, ClarisWksTable &parser,  ClarisWksStyleManager &styleManager) :
    ClarisWksStruct::DSET(dset),MWAWTable(), m_parser(&parser), m_styleManager(&styleManager), m_bordersList(), m_mainPtr(-1)
  {
  }

  //! return a cell corresponding to id
  TableCell *get(int id)
  {
    if (id < 0 || id >= numCells()) {
      MWAW_DEBUG_MSG(("ClarisWksTableInteral::Table::get: cell %d does not exists\n",id));
      return 0;
    }
    return static_cast<TableCell *>(MWAWTable::get(id).get());
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Table const &doc)
  {
    o << static_cast<ClarisWksStruct::DSET const &>(doc);
    return o;
  }
  //! check that each child zone are valid
  void checkChildZones()
  {
    for (size_t i = 0; i < m_cellsList.size(); i++) {
      TableCell *cell = static_cast<TableCell *>(m_cellsList[i].get());
      if (!cell) continue;
      if (cell->m_zoneId > 0 && !okChildId(cell->m_zoneId))
        cell->m_zoneId = 0;
    }
  }
  //! finish updating all cells
  void updateCells()
  {
    for (int c=0; c<numCells(); ++c) {
      if (!get(c)) continue;
      get(c)->update(*this);
    }
  }
  //! ask the main parser to send a zone
  bool askMainToSendZone(int number)
  {
    return m_parser->askMainToSendZone(number);
  }

  /** the main parser */
  ClarisWksTable *m_parser;
  /** the style manager */
  ClarisWksStyleManager *m_styleManager;
  /** the list of border */
  std::vector<Border> m_bordersList;
  /** the relative main pointer */
  long m_mainPtr;
private:
  Table(Table const &orig);
  Table &operator=(Table const &orig);
};

void TableCell::update(Table const &table)
{
  ClarisWksStyleManager *styleManager = table.m_styleManager;
  if (!styleManager) {
    MWAW_DEBUG_MSG(("ClarisWksTableInternal::TableCell::update: style manager is not defined\n"));
    return;
  }

  int numTableBorders = (int) table.m_bordersList.size();
  // now look for border id: L,T,R,B
  for (int w = 0; w < 4; w++) {
    size_t numBorders = m_bordersId[w].size();
    if (!numBorders) continue;

    int bId = m_bordersId[w][0];
    bool sameBorders = true;
    for (size_t b = 1; b < numBorders; b++) {
      if (m_bordersId[w][b]!=bId) {
        sameBorders = false;
        break;
      }
    }
    /** fixme: check that the opposite has a border, if not print the first border */
    if (!sameBorders) continue;
    if (bId < 0 || bId >= numTableBorders) {
      MWAW_DEBUG_MSG(("ClarisWksTableInternal::TableCell::get: can not find the border definition\n"));
      continue;
    }
    Border const &border = table.m_bordersList[size_t(bId)];
    ClarisWksStyleManager::Style bStyle;
    if (border.m_isSent || border.m_styleId < 0 || !styleManager->get(border.m_styleId, bStyle))
      continue;
    border.m_isSent = true;
    MWAWGraphicStyle graph;
    bool haveGraph = false;
    if (bStyle.m_graphicId >= 0)
      haveGraph = styleManager->get(bStyle.m_graphicId, graph);
    ClarisWksStyleManager::KSEN ksen;
    bool haveKSEN = false;
    if (bStyle.m_ksenId >= 0)
      haveKSEN = styleManager->get(bStyle.m_ksenId, ksen);

    MWAWBorder bord;
    if (haveGraph && !graph.hasLine())
      bord.m_style = MWAWBorder::None;
    else if (haveKSEN) {
      bord.m_style = ksen.m_lineType;
      bord.m_type = ksen.m_lineRepeat;
      if (bord.m_type == MWAWBorder::Double)
        bord.m_width = 0.5f*float(graph.m_lineWidth);
      else
        bord.m_width = (float) graph.m_lineWidth;
      bord.m_color = graph.m_lineColor;
    }
    static int const wh[] = { libmwaw::LeftBit, libmwaw::TopBit, libmwaw::RightBit, libmwaw::BottomBit};
    setBorders(wh[w], bord);
  }
}

bool TableCell::sendContent(MWAWListenerPtr listener, MWAWTable &table)
{
  if (!listener) return true;
  if (m_zoneId <= 0)
    listener->insertChar(' ');
  else
    static_cast<Table &>(table).askMainToSendZone(m_zoneId);
  return true;
}

////////////////////////////////////////
//! Internal: the state of a ClarisWksTable
struct State {
  //! constructor
  State() : m_tableMap()
  {
  }
  //! map id -> table
  std::map<int, shared_ptr<Table> > m_tableMap;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
ClarisWksTable::ClarisWksTable(ClarisWksDocument &document) :
  m_document(document), m_parserState(document.m_parserState), m_state(new ClarisWksTableInternal::State),
  m_mainParser(&document.getMainParser())
{
}

ClarisWksTable::~ClarisWksTable()
{ }

int ClarisWksTable::version() const
{
  return m_parserState->m_version;
}

bool ClarisWksTable::askMainToSendZone(int number)
{
  return m_document.sendZone(number, false);
}

// fixme
int ClarisWksTable::numPages() const
{
  return 1;
}
////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// a document part
////////////////////////////////////////////////////////////
shared_ptr<ClarisWksStruct::DSET> ClarisWksTable::readTableZone
(ClarisWksStruct::DSET const &zone, MWAWEntry const &entry, bool &complete)
{
  complete = false;
  if (!entry.valid() || zone.m_fileType != 6 || entry.length() < 32)
    return shared_ptr<ClarisWksStruct::DSET>();
  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos+8+16, librevenge::RVNG_SEEK_SET); // avoid header+8 generic number
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  shared_ptr<ClarisWksTableInternal::Table> tableZone(new ClarisWksTableInternal::Table(zone, *this, *m_document.getStyleManager()));

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
      MWAW_DEBUG_MSG(("ClarisWksTable::readTableZone: can not find definition size\n"));
      input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
      return shared_ptr<ClarisWksStruct::DSET>();
    }

    MWAW_DEBUG_MSG(("ClarisWksTable::readTableZone: unexpected size for zone definition, try to continue\n"));
  }

  if (long(input->tell())+N*data0Length > entry.end()) {
    MWAW_DEBUG_MSG(("ClarisWksTable::readTableZone: file is too short\n"));
    return shared_ptr<ClarisWksStruct::DSET>();
  }
  if (N) {
    MWAW_DEBUG_MSG(("ClarisWksTable::readTableZone: find some tabledef !!!\n"));
    input->seek(entry.end()-N*data0Length, librevenge::RVNG_SEEK_SET);

    for (int i = 0; i < N; i++) {
      pos = input->tell();

      f.str("");
      f << "TableDef#";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      input->seek(pos+data0Length, librevenge::RVNG_SEEK_SET);
    }
  }

  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);

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
    ok = m_document.readStructIntZone(s.str().c_str(), false, 2, res);
  }
  if (ok) {
    pos = input->tell();
    ok = readTablePointers(*tableZone);
    if (!ok) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      ok = m_document.readStructZone("TablePointers", false);
    }
  }
  if (ok) {
    pos = input->tell();
    ok = readTableBordersId(*tableZone);
  }

  if (!ok)
    input->seek(pos, librevenge::RVNG_SEEK_SET);

  tableZone->updateCells();
  if (m_state->m_tableMap.find(tableZone->m_id) != m_state->m_tableMap.end()) {
    MWAW_DEBUG_MSG(("ClarisWksTable::readTableZone: zone %d already exists!!!\n", tableZone->m_id));
  }
  else
    m_state->m_tableMap[tableZone->m_id] = tableZone;

  tableZone->m_otherChilds.push_back(tableZone->m_id+1);
  return tableZone;
}

bool ClarisWksTable::sendZone(int number)
{
  std::map<int, shared_ptr<ClarisWksTableInternal::Table> >::iterator iter
    = m_state->m_tableMap.find(number);
  if (iter == m_state->m_tableMap.end())
    return false;
  shared_ptr<ClarisWksTableInternal::Table> table = iter->second;
  table->m_parsed = true;
  if (table->okChildId(number+1))
    m_document.forceParsed(number+1);

  MWAWListenerPtr listener=m_parserState->getMainListener();
  if (!listener)
    return true;

  table->checkChildZones();
  if (table->sendTable(listener))
    return true;
  return table->sendAsText(listener);
}

void ClarisWksTable::flushExtra()
{
  std::map<int, shared_ptr<ClarisWksTableInternal::Table> >::iterator iter
    = m_state->m_tableMap.begin();
  for (; iter !=  m_state->m_tableMap.end(); ++iter) {
    shared_ptr<ClarisWksTableInternal::Table> table = iter->second;
    if (table->m_parsed)
      continue;
    if (m_parserState->getMainListener()) m_parserState->getMainListener()->insertEOL();
    sendZone(iter->first);
  }
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool ClarisWksTable::readTableBorders(ClarisWksTableInternal::Table &table)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != endPos) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksTable::readTableBorders: file is too short\n"));
    return false;
  }

  input->seek(pos+4, librevenge::RVNG_SEEK_SET);
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
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksTable::readTableBorders: find odd data size\n"));
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
    ClarisWksTableInternal::Border border;
    f.str("");
    f << "TableBorders-" << i << ":";
    int posi[4];
    for (int j = 0; j < 4; j++) posi[j] = (int) input->readLong(4);
    border.m_position[0] = Vec2i(posi[1], posi[0]);
    border.m_position[1] = Vec2i(posi[3], posi[2]);
    border.m_styleId = (int) input->readULong(2);
    table.m_bordersList.push_back(border);
    f << border;

    ClarisWksStyleManager::Style style;
    if (border.m_styleId < 0) ;
    else if (!m_document.getStyleManager()->get(border.m_styleId, style)) {
      MWAW_DEBUG_MSG(("ClarisWksTable::readTableBorders: can not find cell style\n"));
      f << "###style";
    }
    else {
      ClarisWksStyleManager::KSEN ksen;
      if (style.m_ksenId >= 0 && m_document.getStyleManager()->get(style.m_ksenId, ksen)) {
        f << "ksen=[" << ksen << "],";
      }
      MWAWGraphicStyle graph;
      if (style.m_graphicId >= 0 && m_document.getStyleManager()->get(style.m_graphicId, graph)) {
        f << "graph=[" << graph << "],";
      }
      //f << "[" << style << "],";
    }

    if (long(input->tell()) != pos+fSz)
      ascFile.addDelimiter(input->tell(), '|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
  }

  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

bool ClarisWksTable::readTableCells(ClarisWksTableInternal::Table &table)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != endPos) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksTable::readTableCells: file is too short\n"));
    return false;
  }

  input->seek(pos+4, librevenge::RVNG_SEEK_SET);
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
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksTable::readTableCells: find odd data size\n"));
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
    shared_ptr<ClarisWksTableInternal::TableCell> cell(new ClarisWksTableInternal::TableCell());
    float posi[6];
    for (int j = 0; j < 6; j++) posi[j] = float(input->readLong(4))/256.f;
    cell->setBdBox(Box2f(Vec2f(posi[1], posi[0]), Vec2f(posi[3], posi[2])));
    cell->setBdSize(Vec2f(float(posi[5]), float(posi[4])));
    cell->m_zoneId = (int) input->readULong(4);
    val = (int) input->readLong(2);
    if (val) // find one time a number here, another id?...
      f << "#unkn=" << val << ",";
    cell->m_styleId = (int) input->readULong(2);
    table.add(cell);
    if (cell->m_zoneId)
      table.m_otherChilds.push_back(cell->m_zoneId);
    f.str("");
    f << "TableCell-" << i << ":";
    ClarisWksStyleManager::Style style;
    if (cell->m_styleId < 0) ;
    else if (!m_document.getStyleManager()->get(cell->m_styleId, style)) {
      MWAW_DEBUG_MSG(("ClarisWksTable::readTableCells: can not find cell style\n"));
      f  << *cell << "###style";
    }
    else {
      ClarisWksStyleManager::KSEN ksen;
      bool hasExtraLines=false;
      if (style.m_ksenId >= 0 && m_document.getStyleManager()->get(style.m_ksenId, ksen)) {
        switch (ksen.m_valign) {
        case 1:
          cell->setVAlignement(MWAWCell::VALIGN_CENTER);
          break;
        case 2:
          cell->setVAlignement(MWAWCell::VALIGN_BOTTOM);
          break;
        default:
          break;
        }
        hasExtraLines=(ksen.m_lines & 3);
        switch (ksen.m_lines & 3) {
        case 1: // TL->BR
          cell->setExtraLine(MWAWCell::E_Line1);
          break;
        case 2: // BL->TR
          cell->setExtraLine(MWAWCell::E_Line2);
          break;
        case 3:
          cell->setExtraLine(MWAWCell::E_Cross);
          break;
        default:
        case 0: // None
          break;
        }
        f << "ksen=[" << ksen << "],";
      }
      MWAWGraphicStyle graph;
      if (style.m_graphicId >= 0 && m_document.getStyleManager()->get(style.m_graphicId, graph)) {
        // checkme: no always there
        if (graph.hasSurfaceColor())
          cell->setBackgroundColor(graph.m_surfaceColor);
        if (hasExtraLines) {
          MWAWBorder border;
          border.m_width=(float)graph.m_lineWidth;
          border.m_color=graph.m_lineColor;
          cell->setExtraLine(cell->extraLine(), border);
        }
        f << "graph=[" << graph << "],";
      }
      f << *cell;
      //f << "[" << style << "],";
    }

    if (long(input->tell()) != pos+fSz)
      ascFile.addDelimiter(input->tell(), '|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
  }

  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

bool ClarisWksTable::readTableBordersId(ClarisWksTableInternal::Table &table)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  int numCells = table.numCells();
  int numBorders = int(table.m_bordersList.size());
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  for (int i = 0; i < 4*numCells; i++) {
    ClarisWksTableInternal::TableCell *cell = table.get(i/4);
    long pos = input->tell();
    long sz = (long) input->readULong(4);
    long endPos = pos+4+sz;
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    if (long(input->tell()) != endPos) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      MWAW_DEBUG_MSG(("ClarisWksTable::readTableBordersId: file is too short\n"));
      return false;
    }

    input->seek(pos+4, librevenge::RVNG_SEEK_SET);
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
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      MWAW_DEBUG_MSG(("ClarisWksTable::readTableBordersId: find odd data size\n"));
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
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        MWAW_DEBUG_MSG(("ClarisWksTable::readTableBordersId: unexpected id\n"));
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
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

bool ClarisWksTable::readTablePointers(ClarisWksTableInternal::Table &table)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != endPos) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksTable::readTablePointers: file is too short\n"));
    return false;
  }

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  if (!sz) {
    // find this one time as the last entry which ends the table
    ascFile.addPos(pos);
    ascFile.addNote("NOP");
    return true;
  }
  input->seek(pos+4, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(TablePointers):";
  int N = (int) input->readULong(2);
  if (N != table.numCells()) {
    MWAW_DEBUG_MSG(("ClarisWksTable::readTablePointers: the number of pointers seems odd\n"));
    f << "###";
  }
  f << "N=" << N << ",";
  long val = input->readLong(2);
  if (val != -1) f << "f0=" << val << ",";
  val = input->readLong(2);
  if (val) f << "f1=" << val << ",";
  int fSz = (int) input->readLong(2);
  if (sz != 12+fSz*N || fSz < 16) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksTable::readTablePointers: find odd data size\n"));
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
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
  }

  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////


// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
