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

#ifndef MWAW_TABLE
#  define MWAW_TABLE

#include <iostream>
#include <vector>

#include "libmwaw_internal.hxx"

#include "MWAWCell.hxx"

class MWAWTable;

/** a virtual structure used to store in a MWAWTable */
class MWAWTableCell : public MWAWCell
{
public:
  //! constructor
  MWAWTableCell() : MWAWCell(), m_box(), m_size() {
  }
  //! destructor
  virtual ~MWAWTableCell() { }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, MWAWTableCell const &cell);

  /** call when a cell must be send.

  \note default: openTableCell(*this), call sendContent and closeTableCell() */
  virtual bool send(MWAWContentListenerPtr listener, MWAWTable &table);
  //! call when the content of a cell must be send
  virtual bool sendContent(MWAWContentListenerPtr listener, MWAWTable &table) = 0;

  /** the cell bounding box (unit in point)*/
  Box2f m_box;
  /** the cell size : unit WPX_POINT */
  Vec2f m_size;

public:
  //! a comparaison structure used retrieve the rows and the columns
  struct Compare {
    Compare(int dim) : m_coord(dim) {}
    //! small structure to define a cell point
    struct Point {
      Point(int wh, MWAWTableCell const *cell, int cellId) : m_which(wh), m_cell(cell), m_cellId(cellId) {}
      float getPos(int coord) const {
        if (m_which)
          return m_cell->m_box.max()[coord];
        return m_cell->m_box.min()[coord];
      }
      /** returns the cells size */
      float getSize(int coord) const {
        return m_cell->m_box.size()[coord];
      }
      /** the position of the point in the cell (0: LT, 1: RB) */
      int m_which;
      /** the cell */
      MWAWTableCell const *m_cell;
      //! the cell id ( used by compare)
      int m_cellId;
    };

    //! comparaison function
    bool operator()(Point const &c1, Point const &c2) const {
      float diffF = c1.getPos(m_coord)-c2.getPos(m_coord);
      if (diffF < 0) return true;
      if (diffF > 0) return false;
      int diff = c2.m_which - c1.m_which;
      if (diff) return (diff < 0);
      diffF = c1.m_cell->m_box.size()[m_coord]
              - c2.m_cell->m_box.size()[m_coord];
      if (diffF < 0) return true;
      if (diffF > 0) return false;
      return c1.m_cellId < c2.m_cellId;
    }

    //! the coord to compare
    int m_coord;
  };
};

/** a class used to recreate the table structure using cell informations, .... */
class MWAWTable
{
public:
  //! an enum used to indicate what the list of entries which are filled
  enum DataSet {
    CellPositionBit=1, BoxBit=2, SizeBit=4, TableDimBit=8, TablePosToCellBit=0x10
  };
  //! the constructor
  MWAWTable(uint32_t givenData=BoxBit) :
    m_rowsSize(), m_colsSize(), m_givenData(givenData), m_setData(givenData),
    m_cellsList(), m_numRows(0), m_numCols(0), m_posToCellId(), m_hasExtraLines(false) {}

  //! the destructor
  virtual ~MWAWTable();

  //! add a new cells
  void add(shared_ptr<MWAWTableCell> cell) {
    if (!cell) {
      MWAW_DEBUG_MSG(("MWAWTable::add: must be called with a cell\n"));
      return;
    }
    m_cellsList.push_back(cell);
  }

  //! returns the number of cell
  int numCells() const {
    return int(m_cellsList.size());
  }
  /** define the row size (in point) */
  void setRowsSize(std::vector<float> const &rSize) {
    m_rowsSize=rSize;
  }
  /** define the columns size (in point) */
  void setColsSize(std::vector<float> const &cSize) {
    m_colsSize=cSize;
  }

  //! returns the i^th cell
  shared_ptr<MWAWTableCell> get(int id);

  /** try to build the table structures */
  bool updateTable();
  /** returns true if the table has extralines */
  bool hasExtraLines() {
    if (!updateTable()) return false;
    return m_hasExtraLines;
  }
  /** try to send the table

  Note: either send the table ( and returns true ) or do nothing.
   */
  bool sendTable(MWAWContentListenerPtr listener, bool inFrame=true);

  /** try to send the table as basic text */
  bool sendAsText(MWAWContentListenerPtr listener);

protected:
  //! convert a cell position in a posToCellId's position
  int getCellIdPos(int col, int row) const {
    if (col<0||col>=int(m_numCols))
      return -1;
    if (row<0||row>=int(m_numRows))
      return -1;
    return col*int(m_numRows)+row;
  }
  //! create the correspondance list, ...
  bool buildStructures();
  /** compute the rows and the cells size */
  bool buildDims();
  /** a function which fills to posToCellId vector using the cell position */
  bool buildPosToCellId();
  //! send extra line
  void sendExtraLines(MWAWContentListenerPtr listener) const;

public:
  /** the final row  size (in point) */
  std::vector<float> m_rowsSize;
  /** the final col size (in point) */
  std::vector<float> m_colsSize;
protected:
  /** a int to indicate what data are given in entries*/
  uint32_t m_givenData;
  /** a int to indicate what data are been reconstruct*/
  uint32_t m_setData;
  /** the list of cells */
  std::vector<shared_ptr<MWAWTableCell> > m_cellsList;
  /** the number of rows ( set by buildPosToCellId ) */
  size_t m_numRows;
  /** the number of cols ( set by buildPosToCellId ) */
  size_t m_numCols;
  /** a vector used to store an id corresponding to each cell */
  std::vector<int> m_posToCellId;
  /** true if we need to send extra lines */
  bool m_hasExtraLines;
};

#endif
