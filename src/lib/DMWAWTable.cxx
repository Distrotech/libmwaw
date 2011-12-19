/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libwpd
 * Copyright (C) 2002 William Lachance (wrlach@gmail.com)
 * Copyright (C) 2002 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2006 Fridrich Strba (fridrich.strba@bluewin.ch)
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
 * For further information visit http://libwpd.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

#include "DMWAWTable.hxx"
#include "libmwaw_libwpd.hxx"

_DMWAWTableCell::_DMWAWTableCell(uint8_t colSpan, uint8_t rowSpan, uint8_t borderBits) :
  m_colSpan(colSpan),
  m_rowSpan(rowSpan),
  m_borderBits(borderBits)
{
}

DMWAWTable::~DMWAWTable()
{
  typedef std::vector<DMWAWTableCell *>::iterator VTCIter;
  typedef std::vector< std::vector<DMWAWTableCell *> >::iterator VVTCIter;
  for (VVTCIter iter1 = m_tableRows.begin(); iter1 != m_tableRows.end(); iter1++) {
    for (VTCIter iter2 = (*iter1).begin(); iter2 != (*iter1).end(); iter2++) {
      delete(*iter2);
    }
  }
}

void DMWAWTable::insertRow()
{
  m_tableRows.push_back(std::vector<DMWAWTableCell *>());
}

void DMWAWTable::insertCell(uint8_t colSpan, uint8_t rowSpan, uint8_t borderBits)
{
  if (m_tableRows.size() < 1)
    throw libmwaw_libwpd::ParseException();
  m_tableRows[(m_tableRows.size()-1)].push_back(new DMWAWTableCell(colSpan, rowSpan, borderBits));
}

// makeConsistent: make the table border specification (defined per-cell) consistent, with no
// duplicated borders
// pre-condition: the table must be completely built/read before this function is called
void DMWAWTable::makeBordersConsistent()
{
  // make the top/bottom table borders consistent
  for(unsigned i=0; i<m_tableRows.size(); i++) {
    for (unsigned j=0; j<m_tableRows[i].size(); j++) {
      if (i < (m_tableRows.size()-1)) {
        std::vector<DMWAWTableCell *> cellsBottomAdjacent = _getCellsBottomAdjacent(i, j);
        _makeCellBordersConsistent((m_tableRows[i])[j], cellsBottomAdjacent,
                                   DMWAW_TABLE_CELL_BOTTOM_BORDER_OFF, DMWAW_TABLE_CELL_TOP_BORDER_OFF);
      }
      if (j < (m_tableRows[i].size()-1)) {
        std::vector<DMWAWTableCell *> cellsRightAdjacent = _getCellsRightAdjacent(i, j);
        _makeCellBordersConsistent((m_tableRows[i])[j], cellsRightAdjacent,
                                   DMWAW_TABLE_CELL_RIGHT_BORDER_OFF, DMWAW_TABLE_CELL_LEFT_BORDER_OFF);
      }
    }
  }
}

void DMWAWTable::_makeCellBordersConsistent(DMWAWTableCell *cell, std::vector<DMWAWTableCell *> &adjacentCells,
    int adjacencyBitCell, int adjacencyBitBoundCells)
{
  typedef std::vector<DMWAWTableCell *>::iterator VTCIter;
  if (adjacentCells.size() > 0) {
    // if this cell is adjacent to > 1 cell, and it has no border
    // make the cells below have no border
    // NB: there is a corner case where this will not work but it
    // is not resolvable given how WP/OOo define table borders. see BUGS
    if (cell->m_borderBits & adjacencyBitCell) {
      for (VTCIter iter = adjacentCells.begin(); iter != adjacentCells.end(); iter++) {
        (*iter)->m_borderBits |= (uint8_t)(adjacencyBitBoundCells & 0xff);
      }
    }
    // otherwise we can get the same effect by bottom border from
    // this cell-- if the adjacent cells have/don't have borders, this will be
    // picked up automatically
    else
      cell->m_borderBits |= (uint8_t)(adjacencyBitCell & 0xff);
  }
}

std::vector<DMWAWTableCell *> DMWAWTable::_getCellsBottomAdjacent(int i, int j)
{
  int bottomAdjacentRow = i + (m_tableRows[i])[j]->m_rowSpan;
  std::vector<DMWAWTableCell *>  cellsBottomAdjacent = std::vector<DMWAWTableCell *>();

  if ((long)bottomAdjacentRow >= (long)m_tableRows.size())
    return cellsBottomAdjacent;

  for (int j1=0; j1<(int)m_tableRows[bottomAdjacentRow].size(); j1++) {
    if (((j1 + (m_tableRows[bottomAdjacentRow])[j1]->m_colSpan) > j) &&
        (j1 < (j + (m_tableRows[i])[j]->m_colSpan))) {
      cellsBottomAdjacent.push_back((m_tableRows[bottomAdjacentRow])[j1]);
    }
  }

  return cellsBottomAdjacent;
}

std::vector<DMWAWTableCell *> DMWAWTable::_getCellsRightAdjacent(int i, int j)
{
  int rightAdjacentCol = j + 1;
  std::vector<DMWAWTableCell *> cellsRightAdjacent = std::vector<DMWAWTableCell *>();

  if ((long)rightAdjacentCol >= (long)m_tableRows[i].size()) // num cols is uniform across table: this comparison is valid
    return cellsRightAdjacent;

  for(int i1=0; i1<(int)m_tableRows.size(); i1++) {
    if ((long)(m_tableRows[i1]).size() > (long)rightAdjacentCol) { // ignore cases where the right adjacent column
      // pushes us beyond table borders (FIXME: good idea?)
      if (((i1 + (m_tableRows[i1])[rightAdjacentCol]->m_rowSpan) > i) &&
          (i1 < (i + (m_tableRows[i])[j]->m_rowSpan))) {
        cellsRightAdjacent.push_back((m_tableRows[i1])[rightAdjacentCol]);
      }
    }
  }

  return cellsRightAdjacent;
}

DMWAWTableList::DMWAWTableList() :
  m_tableList(new std::vector<DMWAWTable *>),
  m_refCount(new int)
{
  (*m_refCount) = 1;
}

DMWAWTableList::DMWAWTableList(const DMWAWTableList &tableList) :
  m_tableList(tableList.get()),
  m_refCount(tableList.getRef())
{
  if (m_refCount)
    (*m_refCount)++;
}

DMWAWTableList &DMWAWTableList::operator=(const DMWAWTableList &tableList)
{
  if (this != &tableList) {
    release();
    acquire(tableList.getRef(), tableList.get());
  }

  return (*this);
}

void DMWAWTableList::acquire(int *refCount, std::vector<DMWAWTable *> *tableList)
{
  m_refCount = refCount;
  m_tableList = tableList;
  if (m_refCount)
    (*m_refCount)++;
}

void DMWAWTableList::release()
{
  if (m_refCount) {
    if (--(*m_refCount) == 0) {
      for (std::vector<DMWAWTable *>::iterator iter = (*m_tableList).begin(); iter != (*m_tableList).end(); iter++)
        delete (*iter);
      delete m_tableList;
      delete m_refCount;
    }
    m_refCount = 0;
    m_tableList = 0;
  }
}

DMWAWTableList::~DMWAWTableList()
{
  release();
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
