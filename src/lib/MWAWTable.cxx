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

#include "MWAWTable.hxx"

#include "MWAWContentListener.hxx"

////////////////////////////////////////////////////////////
// destructor, ...
MWAWTable::~MWAWTable()
{
}

shared_ptr<MWAWTableCell> MWAWTable::get(int id)
{
  if (id < 0 || id >= int(m_cellsList.size())) {
    MWAW_DEBUG_MSG(("MWAWTable::get: cell %d does not exists\n",id));
    return shared_ptr<MWAWTableCell>();
  }
  return m_cellsList[size_t(id)];
}

////////////////////////////////////////////////////////////
// build the table structure
bool MWAWTable::buildStructures()
{
  if (m_colsSize.size())
    return true;

  size_t nCells = m_cellsList.size();
  std::vector<float> listPositions[2];
  for (int dim = 0; dim < 2; dim++) {
    MWAWTableCell::Compare compareFunction(dim);
    std::set<MWAWTableCell::Compare::Point,
        MWAWTableCell::Compare> set(compareFunction);
    for (size_t c = 0; c < nCells; c++) {
      set.insert(MWAWTableCell::Compare::Point(0, m_cellsList[c].get()));
      set.insert(MWAWTableCell::Compare::Point(1, m_cellsList[c].get()));
    }

    std::vector<float> positions;
    std::set<MWAWTableCell::Compare::Point,
        MWAWTableCell::Compare>::iterator it = set.begin();
    float maxPosiblePos=0;
    int actCell = -1;
    for ( ; it != set.end(); it++) {
      float pos = it->getPos(dim);
      if (actCell < 0 || pos > maxPosiblePos) {
        actCell++;
        positions.push_back(pos);
        maxPosiblePos = float(pos+2.0); // 2 pixel ok
      }
      if (it->m_which == 0 && it->getPos(dim)-2.0 < maxPosiblePos)
        maxPosiblePos = float((it->getPos(dim)+pos)/2.);
    }
    listPositions[dim] = positions;
  }
  for (size_t c = 0; c < nCells; c++) {
    int cellPos[2], spanCell[2];
    for (int dim = 0; dim < 2; dim++) {
      float pt[2] = { m_cellsList[c]->box().min()[dim],
                      m_cellsList[c]->box().max()[dim]
                    };
      std::vector<float> &pos = listPositions[dim];
      size_t numPos = pos.size();
      size_t i = 0;
      while (i+1 < numPos && pos[i+1] < pt[0])
        i++;
      if (i+1 < numPos && (pos[i]+pos[i+1])/2 < pt[0])
        i++;
      if (i+1 > numPos) {
        MWAW_DEBUG_MSG(("MWAWTable::buildStructures: impossible to find cell position !!!\n"));
        return false;
      }
      cellPos[dim] = int(i);
      while (i+1 < numPos && pos[i+1] < pt[1])
        i++;
      if (i+1 < numPos && (pos[i]+pos[i+1])/2 < pt[1])
        i++;
      spanCell[dim] = int(i)-cellPos[dim];
      if (spanCell[dim]==0 &&
          (m_cellsList[c]->box().size()[dim] < 0 || m_cellsList[c]->box().size()[dim] > 0)) {
        MWAW_DEBUG_MSG(("MWAWTable::buildStructures: impossible to find span number !!!\n"));
        return false;
      }
      if (spanCell[dim] > 1 &&
          pos[size_t(cellPos[dim])]+2.0f > pos[size_t(cellPos[dim]+1)]) {
        spanCell[dim]--;
        cellPos[dim]++;
      }
    }
    m_cellsList[c]->m_position = Vec2i(cellPos[0], cellPos[1]);
    m_cellsList[c]->m_numberCellSpanned = Vec2i(spanCell[0], spanCell[1]);
  }
  // finally update the row/col size
  for (int dim = 0; dim < 2; dim++) {
    std::vector<float> const &pos = listPositions[dim];
    size_t numPos = pos.size();
    if (!numPos) continue;
    std::vector<float> &res = (dim==0) ? m_colsSize : m_rowsSize;
    res.resize(numPos-1);
    for (size_t i = 0; i < numPos-1; i++)
      res[i] = pos[i+1]-pos[i];
  }

  return true;
}

////////////////////////////////////////////////////////////
// try to send the table
bool MWAWTable::sendTable(MWAWContentListenerPtr listener)
{
  if (!buildStructures())
    return false;
  if (!listener)
    return true;

  size_t nCells = m_cellsList.size();
  size_t numCols = m_colsSize.size();
  size_t numRows = m_rowsSize.size();
  if (!numCols || !numRows)
    return false;
  std::vector<int> cellsId(numCols*numRows, -1);
  for (size_t c = 0; c < nCells; c++) {
    if (!m_cellsList[c]) continue;
    Vec2i const &pos=m_cellsList[c]->m_position;
    Vec2i const &span=m_cellsList[c]->m_numberCellSpanned;

    for (int x = pos[0]; x < pos[0]+span[0]; x++) {
      if (x >= int(numCols)) {
        MWAW_DEBUG_MSG(("MWAWTable::sendTable: x is too big !!!\n"));
        return false;
      }
      for (int y = pos[1]; y < pos[1]+span[1]; y++) {
        if (y >= int(numRows)) {
          MWAW_DEBUG_MSG(("MWAWTable::sendTable: y is too big !!!\n"));
          return false;
        }
        size_t tablePos = size_t(y*int(numCols)+x);
        if (cellsId[tablePos] != -1) {
          MWAW_DEBUG_MSG(("MWAWTable::sendTable: cells is used!!!\n"));
          return false;
        }
        if (x == pos[0] && y == pos[1])
          cellsId[tablePos] = int(c);
        else
          cellsId[tablePos] = -2;
      }
    }
  }

  sendPreTableData(listener);
  listener->openTable(m_colsSize, WPX_POINT);
  for (size_t r = 0; r < numRows; r++) {
    listener->openTableRow(m_rowsSize[r], WPX_POINT);
    for (size_t c = 0; c < numCols; c++) {
      size_t tablePos = r*numCols+c;
      int id = cellsId[tablePos];
      if (id == -1)
        listener->addEmptyTableCell(Vec2i(int(c), int(r)));
      if (id < 0) continue;
      m_cellsList[size_t(id)]->send(listener);
    }
    listener->closeTableRow();
  }

  listener->closeTable();

  return true;
}


////////////////////////////////////////////////////////////
// try to send the table
bool MWAWTable::sendAsText(MWAWContentListenerPtr listener)
{
  if (!listener) return true;

  size_t nCells = m_cellsList.size();
  for (size_t i = 0; i < nCells; i++) {
    if (!m_cellsList[i]) continue;
    m_cellsList[i]->sendContent(listener);
    listener->insertEOL();
  }
  return true;
}
