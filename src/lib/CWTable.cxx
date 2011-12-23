/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libmwaw
 * Copyright (C) 2009, 2011 Alonso Laurent (alonso@loria.fr)
 * Copyright (C) 2006, 2007 Andrew Ziem
 * Copyright (C) 2004-2006 Fridrich Strba (fridrich.strba@bluewin.ch)
 * Copyright (C) 2004 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2003-2005 William Lachance (william.lachance@sympatico.ca)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 * For further information visit http://libmwaw.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>

#include <libwpd/WPXString.h>

#include "TMWAWPictBasic.hxx"
#include "TMWAWPictMac.hxx"
#include "TMWAWPosition.hxx"

#include "IMWAWCell.hxx"

#include "MWAWStruct.hxx"
#include "MWAWTools.hxx"
#include "MWAWContentListener.hxx"

#include "CWTable.hxx"

#include "CWParser.hxx"
#include "CWStruct.hxx"

/** Internal: the structures of a CWTable */
namespace CWTableInternal
{
struct Border {
  Border() : m_flags(0) {}
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Border const &bord) {
    for (int i = 0; i < 2; i++) {
      Vec2f pos = bord.m_position[i];
      pos=1./256.*pos;
      o << pos;
      if (i == 0) o << "<->";
      else o << ",";
    }
    if (bord.m_flags)
      o << "id/flags?=" << bord.m_flags << ",";
    return o;
  }

  /* the origin and the end of edge position : unit WPX_POINT/256 */
  Vec2i m_position[2];
  // some flags : style id ?
  int m_flags;
};

struct Cell {
  Cell() : m_position(-1,-1), m_numSpan(), m_box(), m_size(),
    m_zoneId(0), m_styleId(-1) {
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Cell const &cell) {
    if (cell.m_position.x() >= 0) {
      o << "pos=" << cell.m_position << ",";
      if (cell.m_numSpan[0]!=1 || cell.m_numSpan[1]!=1)
        o << "span=" << cell.m_position << ",";
    }
    Box2f box = cell.m_box;
    box.scale(1./256.);
    o << "box=" << box << ",";
    Vec2f sz = cell.m_size;
    sz = 1./256.*sz;
    o << "size=" << sz << ",";
    if (cell.m_zoneId) o << "zone=" << cell.m_zoneId << ",";
    if (cell.m_styleId >= 0)o << "style=" << cell.m_styleId << ",";
    return o;
  }

  /* the position in the table */
  Vec2i m_position, m_numSpan;
  /* the cell bounding box : unit WPX_POINT/256*/
  Box2i m_box;
  /* the cell size : unit WPX_POINT/256 */
  Vec2i m_size;
  /* the cell zone ( 0 is no content ) */
  int m_zoneId;
  /* the list of border id : Left, Top, Right, Bottom

     Normally, one id but merge cells can have mutiple border
  */
  std::vector<int> m_bordersId[4];
  // the style id ?
  int m_styleId;

};

////////////////////////////////////////
////////////////////////////////////////
struct Table : public CWStruct::DSET {
  //! constructor
  Table(CWStruct::DSET const dset = CWStruct::DSET()) :
    CWStruct::DSET(dset), m_bordersList(),m_cellsList(),
    m_rowsSize(), m_colsSize(), m_parsed(false) {
  }

  //! create the correspondance list, ...
  bool buildStructures();

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Table const &doc) {
    o << static_cast<CWStruct::DSET const &>(doc);
    return o;
  }

  std::vector<Border> m_bordersList;
  std::vector<Cell> m_cellsList;
  std::vector<float> m_rowsSize, m_colsSize; // in inches
  bool m_parsed;

protected:
  //! a comparaison structure used retrieve the rows and the columns
  struct Compare {
    Compare(int dim) : m_coord(dim) {}
    //! small structure to define a cell point
    struct CellPoint {
      CellPoint(int wh, Cell const *cell) : m_which(wh), m_cell(cell) {}
      int getPos(int coord) const {
        if (m_which)
          return m_cell->m_box.max()[coord];
        return m_cell->m_box.min()[coord];
      }
      int getSize(int coord) const {
        return m_cell->m_box.size()[coord];
      }
      int m_which;
      Cell const *m_cell;
    };

    //! comparaison function
    bool operator()(CellPoint const &c1, CellPoint const &c2) const {
      int diff = c1.getPos(m_coord)-c2.getPos(m_coord);
      if (diff) return (diff < 0);
      diff = c2.m_which - c1.m_which;
      if (diff) return (diff < 0);
      diff = c1.m_cell->m_box.size()[m_coord]
             - c2.m_cell->m_box.size()[m_coord];
      if (diff) return (diff < 0);
      return long(c1.m_cell) < long(c2.m_cell);
    }

    //! the coord to compare
    int m_coord;
  };
};

bool Table::buildStructures()
{
  if (m_colsSize.size())
    return true;

  int numCells = m_cellsList.size();
  std::vector<int> listPositions[2];
  for (int dim = 0; dim < 2; dim++) {
    Compare compareFunction(dim);
    std::set<Compare::CellPoint, Compare> set(compareFunction);
    for (int c = 0; c < numCells; c++) {
      set.insert(Compare::CellPoint(0, &m_cellsList[c]));
      set.insert(Compare::CellPoint(1, &m_cellsList[c]));
    }

    std::vector<int> positions;
    std::set<Compare::CellPoint, Compare>::iterator it = set.begin();
    int prevPos, maxPosiblePos;
    int actCell = -1;
    for ( ; it != set.end(); it++) {
      int pos = it->getPos(dim);
      if (actCell < 0 || pos > maxPosiblePos) {
        actCell++;
        prevPos = pos;
        positions.push_back(pos);
        maxPosiblePos = pos+512; // 2 pixel ok
      }
      if (it->m_which == 0 && it->getPos(1)-512 < maxPosiblePos)
        maxPosiblePos = (it->getPos(dim)+pos)/2;
    }
    listPositions[dim] = positions;
  }
  for (int c = 0; c < numCells; c++) {
    int cellPos[2], spanCell[2];
    for (int dim = 0; dim < 2; dim++) {
      int pt[2] = { m_cellsList[c].m_box.min()[dim],
                    m_cellsList[c].m_box.max()[dim]
                  };
      std::vector<int> &pos = listPositions[dim];
      int numPos = pos.size();
      int i = 0;
      while (i+1 < numPos && pos[i+1] < pt[0])
        i++;
      if (i+1 < numPos && (pos[i]+pos[i+1])/2 < pt[0])
        i++;
      if (i+1 > numPos) {
        MWAW_DEBUG_MSG(("Table::buildStructures: impossible to find cell position !!!\n"));
        return false;
      }
      cellPos[dim] = i;
      while (i+1 < numPos && pos[i+1] < pt[1])
        i++;
      if (i+1 < numPos && (pos[i]+pos[i+1])/2 < pt[1])
        i++;
      spanCell[dim] = i-cellPos[dim];
      if (spanCell[dim]==0 && m_cellsList[c].m_box.size()[dim]) {
        MWAW_DEBUG_MSG(("Table::buildStructures: impossible to find span number !!!\n"));
        return false;
      }
    }
    m_cellsList[c].m_position = Vec2i(cellPos[0], cellPos[1]);
    m_cellsList[c].m_numSpan = Vec2i(spanCell[0], spanCell[1]);
  }
  // finally update the row/col size
  for (int dim = 0; dim < 2; dim++) {
    std::vector<int> const &pos = listPositions[dim];
    int numPos = pos.size();
    if (!numPos) continue;
    std::vector<float> &res = (dim==0) ? m_colsSize : m_rowsSize;
    res.resize(numPos-1);
    for (int i = 0; i < numPos-1; i++)
      res[i] = (pos[i+1]-pos[i])/256.;
  }

  return true;
}

////////////////////////////////////////
//! Internal: the state of a CWTable
struct State {
  //! constructor
  State() : m_tableMap() {
  }

  std::map<int, shared_ptr<Table> > m_tableMap;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
CWTable::CWTable
(TMWAWInputStreamPtr ip, CWParser &parser, MWAWTools::ConvertissorPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert), m_state(new CWTableInternal::State),
  m_mainParser(&parser), m_asciiFile(parser.ascii())
{
}

CWTable::~CWTable()
{ }

int CWTable::version() const
{
  return m_mainParser->version();
}

// fixme
int CWTable::numPages() const
{
  return 1;
}
////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// a document part
////////////////////////////////////////////////////////////
shared_ptr<CWStruct::DSET> CWTable::readTableZone
(CWStruct::DSET const &zone, IMWAWEntry const &entry, bool &complete)
{
  complete = false;
  if (!entry.valid() || zone.m_type != 6 || entry.length() < 32)
    return shared_ptr<CWStruct::DSET>();
  long pos = entry.begin();
  m_input->seek(pos+8+16, WPX_SEEK_SET); // avoid header+8 generic number
  libmwaw_tools::DebugStream f;
  shared_ptr<CWTableInternal::Table>
  tableZone(new CWTableInternal::Table(zone));

  f << "Entries(TableDef):" << *tableZone << ",";
  float dim[2];
  for (int i = 0; i < 2; i++) dim[i] = m_input->readLong(4)/256.;
  f << "dim=" << dim[0] << "x" << dim[1] << ",";
  int val;
  for (int i = 0; i < 3; i++) {
    // f1=parentZoneId ?
    val = m_input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascii().addDelimiter(m_input->tell(), '|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  // read the last part
  int data0Length = zone.m_dataSz;
  int N = zone.m_numData;
  if (entry.length() -8-12 != data0Length*N + zone.m_headerSz) {
    if (data0Length == 0 && N) {
      MWAW_DEBUG_MSG(("CWTable::readTableZone: can not find definition size\n"));
      m_input->seek(entry.end(), WPX_SEEK_SET);
      return shared_ptr<CWStruct::DSET>();
    }

    MWAW_DEBUG_MSG(("CWTable::readTableZone: unexpected size for zone definition, try to continue\n"));
  }

  if (long(m_input->tell())+N*data0Length > entry.end()) {
    MWAW_DEBUG_MSG(("CWTable::readTableZone: file is too short\n"));
    return shared_ptr<CWStruct::DSET>();
  }
  if (N) {
    MWAW_DEBUG_MSG(("CWTable::readTableZone: find some tabledef !!!\n"));
    m_input->seek(entry.end()-N*data0Length, WPX_SEEK_SET);

    for (int i = 0; i < N; i++) {
      pos = m_input->tell();

      f.str("");
      f << "TableDef#";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      m_input->seek(pos+data0Length, WPX_SEEK_SET);
    }
  }

  m_input->seek(entry.end(), WPX_SEEK_SET);

  pos = m_input->tell();
  bool ok = readTableBorders(*tableZone);
  if (ok) {
    pos = m_input->tell();
    ok = readTableCells(*tableZone);
  }
  /** three fields which seems to follows the list of cells
      zone 0 : looks like a list of integer : related to last selected border ?
      zone 1 : looks like a list of integer : unknown meaning
      zone 2 : looks like a list of 4 pointers
   */
  for (int i = 0; ok && i < 3; i++) {
    pos = m_input->tell();
    ok = readTableUnknown(i);
  }
  if (ok) {
    pos = m_input->tell();
    ok = readTableBordersId(*tableZone);
  }

  if (!ok)
    m_input->seek(pos, WPX_SEEK_SET);

  if (m_state->m_tableMap.find(tableZone->m_id) != m_state->m_tableMap.end()) {
    MWAW_DEBUG_MSG(("CWTable::readTableZone: zone %d already exists!!!\n", tableZone->m_id));
  } else
    m_state->m_tableMap[tableZone->m_id] = tableZone;

  // fixme: in general followed by another zone
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

  if (!table->buildStructures())
    return false;
  if (!m_listener)
    return true;

  int numCells = table->m_cellsList.size();
  int numCols = table->m_colsSize.size();
  int numRows = table->m_rowsSize.size();
  if (!numCols || !numRows)
    return false;
  std::vector<int> cellsId(numCols*numRows, -1);
  for (int c = 0; c < numCells; c++) {
    Vec2i const &pos=table->m_cellsList[c].m_position;
    Vec2i const &span=table->m_cellsList[c].m_numSpan;

    for (int x = pos[0]; x < pos[0]+span[0]; x++) {
      if (x >= numCols) {
        MWAW_DEBUG_MSG(("CWTable::sendZone: x is too big !!!\n"));
        return false;
      }
      for (int y = pos[1]; y < pos[1]+span[1]; y++) {
        if (y >= numRows) {
          MWAW_DEBUG_MSG(("CWTable::sendZone: y is too big !!!\n"));
          return false;
        }
        int tablePos = y*numCols+x;
        if (cellsId[tablePos] != -1) {
          MWAW_DEBUG_MSG(("CWTable::sendZone: cells is used!!!\n"));
          return false;
        }
        if (x == pos[0] && y == pos[1])
          cellsId[tablePos] = c;
        else
          cellsId[tablePos] = -2;
      }
    }
  }

  m_listener->openTable(table->m_colsSize, WPX_POINT);
  for (int r = 0; r < numRows; r++) {
    m_listener->openTableRow(table->m_rowsSize[r], WPX_POINT);
    for (int c = 0; c < numCols; c++) {
      int tablePos = r*numCols+c;
      int id = cellsId[tablePos];
      if (id < 0) continue;
      IMWAWCell cell;
      cell.position() = Vec2i(c, r);
      Vec2i span = table->m_cellsList[id].m_numSpan;
      cell.setNumSpannedCells(Vec2i(span[1],span[0]));
      m_listener->openTableCell(cell, WPXPropertyList());
      m_listener->insertCharacter(' ');
      m_listener->closeTableCell();
    }
    m_listener->closeTableRow();
  }

  m_listener->closeTable();

  return true;
}

void CWTable::flushExtra()
{
  std::map<int, shared_ptr<CWTableInternal::Table> >::iterator iter
    = m_state->m_tableMap.begin();
  for ( ; iter !=  m_state->m_tableMap.end(); iter++) {
    shared_ptr<CWTableInternal::Table> table = iter->second;
    if (table->m_parsed)
      continue;
    if (m_listener) m_listener->insertEOL();
#ifdef DEBUG
    sendZone(iter->first);
#endif
    table->m_parsed = true;
  }
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool CWTable::readTableBorders(CWTableInternal::Table &table)
{
  long pos = m_input->tell();
  long sz = m_input->readULong(4);
  long endPos = pos+4+sz;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWTable::readTableBorders: file is too short\n"));
    return false;
  }

  m_input->seek(pos+4, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "Entries(TableBorders):";
  int N = m_input->readULong(2);
  f << "N=" << N << ",";
  int val = m_input->readLong(2);
  if (val != -1) f << "f0=" << val << ",";
  val = m_input->readLong(2);
  if (val) f << "f1=" << val << ",";
  int fSz = m_input->readLong(2);
  if (sz != 12+fSz*N || fSz < 18) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWTable::readTableBorders: find odd data size\n"));
    return false;
  }
  for (int i = 2; i < 4; i++) {
    val = m_input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";

  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = m_input->tell();
    CWTableInternal::Border border;
    f.str("");
    f << "TableBorders-" << i << ":";
    int posi[4];
    for (int i = 0; i < 4; i++) posi[i] = m_input->readLong(4);
    border.m_position[0] = Vec2i(posi[1], posi[0]);
    border.m_position[1] = Vec2i(posi[3], posi[2]);
    border.m_flags = m_input->readULong(2);
    table.m_bordersList.push_back(border);
    f << border;
    if (long(m_input->tell()) != pos+fSz)
      ascii().addDelimiter(m_input->tell(), '|');
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+fSz, WPX_SEEK_SET);
  }

  m_input->seek(endPos, WPX_SEEK_SET);
  return true;
}

bool CWTable::readTableCells(CWTableInternal::Table &table)
{
  long pos = m_input->tell();
  long sz = m_input->readULong(4);
  long endPos = pos+4+sz;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWTable::readTableCells: file is too short\n"));
    return false;
  }

  m_input->seek(pos+4, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "Entries(TableCell):";
  int N = m_input->readULong(2);
  f << "N=" << N << ",";
  int val = m_input->readLong(2);
  if (val != -1) f << "f0=" << val << ",";
  val = m_input->readLong(2);
  if (val) f << "f1=" << val << ",";
  int fSz = m_input->readLong(2);
  if (sz != 12+fSz*N || fSz < 32) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWTable::readTableCells: find odd data size\n"));
    return false;
  }
  for (int i = 2; i < 4; i++) {
    val = m_input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";

  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = m_input->tell();
    CWTableInternal::Cell cell;
    int posi[6];
    for (int j = 0; j < 6; j++) posi[j] = m_input->readLong(4);
    cell.m_box = Box2i(Vec2i(posi[1], posi[0]), Vec2i(posi[3], posi[2]));
    cell.m_size = Vec2i(posi[5], posi[4]);
    cell.m_zoneId = m_input->readULong(4);
    cell.m_styleId = m_input->readULong(4);
    table.m_cellsList.push_back(cell);
    f.str("");
    f << "TableCell-" << i << ":" << cell;
    if (long(m_input->tell()) != pos+fSz)
      ascii().addDelimiter(m_input->tell(), '|');
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+fSz, WPX_SEEK_SET);
  }

  m_input->seek(endPos, WPX_SEEK_SET);
  return true;
}

bool CWTable::readTableUnknown(int id)
{
  long pos = m_input->tell();
  long sz = m_input->readULong(4);
  long endPos = pos+4+sz;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWTable::readTableUnknown: file is too short\n"));
    return false;
  }

  m_input->seek(pos+4, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "Entries(TableUnknown-" << id << "):";
  int N = m_input->readULong(2);
  f << "N=" << N << ",";
  int val = m_input->readLong(2);
  if (val != -1) f << "f0=" << val << ",";
  val = m_input->readLong(2);
  if (val) f << "f1=" << val << ",";
  int fSz = m_input->readLong(2);
  if (sz != 12+fSz*N) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWTable::readTableUnknown: find odd data size\n"));
    return false;
  }
  for (int i = 2; i < 4; i++) {
    val = m_input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";

  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = m_input->tell();
    f.str("");
    f << "TableUnknown-" << id << "[" << i << "]:";

    m_input->seek(pos+fSz, WPX_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool CWTable::readTableBordersId(CWTableInternal::Table &table)
{
  int numCells = table.m_cellsList.size();
  int numBorders = table.m_bordersList.size();
  for (int i = 0; i < 4*numCells; i++) {
    CWTableInternal::Cell &cell = table.m_cellsList[i/4];
    long pos = m_input->tell();
    long sz = m_input->readULong(4);
    long endPos = pos+4+sz;
    m_input->seek(endPos, WPX_SEEK_SET);
    if (long(m_input->tell()) != endPos) {
      m_input->seek(pos, WPX_SEEK_SET);
      MWAW_DEBUG_MSG(("CWTable::readTableBordersId: file is too short\n"));
      return false;
    }

    m_input->seek(pos+4, WPX_SEEK_SET);
    libmwaw_tools::DebugStream f;
    f << "Entries(TableBordersId)[" << i/4 << "," << i%4 << "],";
    int N = m_input->readULong(2);
    f << "N=" << N << ",";
    int val = m_input->readLong(2);
    if (val != -1) f << "f0=" << val << ",";
    val = m_input->readLong(2);
    if (val) f << "f1=" << val << ",";
    int fSz = m_input->readLong(2);
    if (N==0 || sz != 12+fSz*N || fSz < 2) {
      m_input->seek(pos, WPX_SEEK_SET);
      MWAW_DEBUG_MSG(("CWTable::readTableBordersId: find odd data size\n"));
      return false;
    }
    for (int j = 2; j < 4; j++) {
      val = m_input->readLong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }

    std::vector<int> idsList;
    for (int j = 0; j < N; j++) {
      int id = m_input->readLong(2);
      if (id < 0 || id >= numBorders) {
        m_input->seek(pos, WPX_SEEK_SET);
        MWAW_DEBUG_MSG(("CWTable::readTableBordersId: unexpected id\n"));
        return false;
      }
      idsList.push_back(id);
      if (j)
        f << "bordId" << j << "=" << id << ",";
      else
        f << "bordId=" << id << ",";
    }
    cell.m_bordersId[i%4] = idsList;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(endPos, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////


// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
