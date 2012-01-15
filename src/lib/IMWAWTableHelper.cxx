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
 * For further information visit http://libwps.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

/*
 * Structure to store and construct a table from an unstructured list
 * of cell
 *
 */

#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>

#include "IMWAWTableHelper.hxx"

#include "IMWAWContentListener.hxx"

////////////////////////////////////////////////////////////
// destructor, ...
IMWAWTableHelper::~IMWAWTableHelper()
{
}

shared_ptr<IMWAWTableHelperCell> IMWAWTableHelper::get(int id)
{
  if (id < 0 || id >= int(m_cellsList.size())) {
    MWAW_DEBUG_MSG(("IMWAWTableHelper::get: cell %d does not exists\n",id));
    return shared_ptr<IMWAWTableHelperCell>();
  }
  return m_cellsList[id];
}

////////////////////////////////////////////////////////////
// build the table structure
bool IMWAWTableHelper::buildStructures()
{
  if (m_colsSize.size())
    return true;

  int numCells = m_cellsList.size();
  std::vector<float> listPositions[2];
  for (int dim = 0; dim < 2; dim++) {
    IMWAWTableHelperCell::Compare compareFunction(dim);
    std::set<IMWAWTableHelperCell::Compare::Point,
        IMWAWTableHelperCell::Compare> set(compareFunction);
    for (int c = 0; c < numCells; c++) {
      set.insert(IMWAWTableHelperCell::Compare::Point(0, m_cellsList[c].get()));
      set.insert(IMWAWTableHelperCell::Compare::Point(1, m_cellsList[c].get()));
    }

    std::vector<float> positions;
    std::set<IMWAWTableHelperCell::Compare::Point,
        IMWAWTableHelperCell::Compare>::iterator it = set.begin();
    float prevPos, maxPosiblePos=0;
    int actCell = -1;
    for ( ; it != set.end(); it++) {
      float pos = it->getPos(dim);
      if (actCell < 0 || pos > maxPosiblePos) {
        actCell++;
        prevPos = pos;
        positions.push_back(pos);
        maxPosiblePos = pos+2.0; // 2 pixel ok
      }
      if (it->m_which == 0 && it->getPos(1)-2.0 < maxPosiblePos)
        maxPosiblePos = (it->getPos(dim)+pos)/2.;
    }
    listPositions[dim] = positions;
  }
  for (int c = 0; c < numCells; c++) {
    int cellPos[2], spanCell[2];
    for (int dim = 0; dim < 2; dim++) {
      float pt[2] = { m_cellsList[c]->box().min()[dim],
                      m_cellsList[c]->box().max()[dim]
                    };
      std::vector<float> &pos = listPositions[dim];
      int numPos = pos.size();
      int i = 0;
      while (i+1 < numPos && pos[i+1] < pt[0])
        i++;
      if (i+1 < numPos && (pos[i]+pos[i+1])/2 < pt[0])
        i++;
      if (i+1 > numPos) {
        MWAW_DEBUG_MSG(("IMWAWTableHelper::buildStructures: impossible to find cell position !!!\n"));
        return false;
      }
      cellPos[dim] = i;
      while (i+1 < numPos && pos[i+1] < pt[1])
        i++;
      if (i+1 < numPos && (pos[i]+pos[i+1])/2 < pt[1])
        i++;
      spanCell[dim] = i-cellPos[dim];
      if (spanCell[dim]==0 && m_cellsList[c]->box().size()[dim]) {
        MWAW_DEBUG_MSG(("IMWAWTableHelper::buildStructures: impossible to find span number !!!\n"));
        return false;
      }
    }
    m_cellsList[c]->m_position = Vec2i(cellPos[0], cellPos[1]);
    m_cellsList[c]->m_numSpan = Vec2i(spanCell[0], spanCell[1]);
  }
  // finally update the row/col size
  for (int dim = 0; dim < 2; dim++) {
    std::vector<float> const &pos = listPositions[dim];
    int numPos = pos.size();
    if (!numPos) continue;
    std::vector<float> &res = (dim==0) ? m_colsSize : m_rowsSize;
    res.resize(numPos-1);
    for (int i = 0; i < numPos-1; i++)
      res[i] = pos[i+1]-pos[i];
  }

  return true;
}

////////////////////////////////////////////////////////////
// try to send the table
bool IMWAWTableHelper::sendTable(IMWAWContentListenerPtr listener)
{
  if (!buildStructures())
    return false;
  if (!listener)
    return true;

  int numCells = m_cellsList.size();
  int numCols = m_colsSize.size();
  int numRows = m_rowsSize.size();
  if (!numCols || !numRows)
    return false;
  std::vector<int> cellsId(numCols*numRows, -1);
  for (int c = 0; c < numCells; c++) {
    if (!m_cellsList[c]) continue;
    Vec2i const &pos=m_cellsList[c]->m_position;
    Vec2i const &span=m_cellsList[c]->m_numSpan;

    for (int x = pos[0]; x < pos[0]+span[0]; x++) {
      if (x >= numCols) {
        MWAW_DEBUG_MSG(("IMWAWTableHelper::sendTable: x is too big !!!\n"));
        return false;
      }
      for (int y = pos[1]; y < pos[1]+span[1]; y++) {
        if (y >= numRows) {
          MWAW_DEBUG_MSG(("IMWAWTableHelper::sendTable: y is too big !!!\n"));
          return false;
        }
        int tablePos = y*numCols+x;
        if (cellsId[tablePos] != -1) {
          MWAW_DEBUG_MSG(("IMWAWTableHelper::sendTable: cells is used!!!\n"));
          return false;
        }
        if (x == pos[0] && y == pos[1])
          cellsId[tablePos] = c;
        else
          cellsId[tablePos] = -2;
      }
    }
  }
  listener->openTable(m_colsSize, WPX_POINT);
  for (int r = 0; r < numRows; r++) {
    listener->openTableRow(m_rowsSize[r], WPX_POINT);
    for (int c = 0; c < numCols; c++) {
      int tablePos = r*numCols+c;
      int id = cellsId[tablePos];
      if (id < 0) continue;
      m_cellsList[id]->send(listener);
    }
    listener->closeTableRow();
  }

  listener->closeTable();

  return true;
}


////////////////////////////////////////////////////////////
// try to send the table
bool IMWAWTableHelper::sendAsText(IMWAWContentListenerPtr listener)
{
  if (!listener) return true;

  int numCells = m_cellsList.size();
  for (int i = 0; i < numCells; i++) {
    if (!m_cellsList[i]) continue;
    m_cellsList[i]->sendContent(listener);
    listener->insertEOL();
  }
  return true;
}
