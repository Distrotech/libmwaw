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

/** a class used to recreate the table structure using cell informations, .... */
class MWAWTable
{
public:
  class Cell;

  //! an enum used to indicate what the list of entries which are filled
  enum DataSet {
    CellPositionBit=1, BoxBit=2, SizeBit=4, TableDimBit=8, TablePosToCellBit=0x10
  };
  //! the constructor
  MWAWTable(uint32_t givenData=BoxBit) :
    m_rowsSize(), m_colsSize(), m_givenData(givenData), m_setData(givenData),
    m_mergeBorders(true), m_cellsList(), m_numRows(0), m_numCols(0), m_posToCellId(), m_hasExtraLines(false) {}

  //! the destructor
  virtual ~MWAWTable();

  //! add a new cells
  void add(shared_ptr<Cell> cell) {
    if (!cell) {
      MWAW_DEBUG_MSG(("MWAWTable::add: must be called with a cell\n"));
      return;
    }
    m_cellsList.push_back(cell);
  }
  //! returns true if we need to merge borders
  bool mergeBorders() const {
    return m_mergeBorders;
  }
  //! sets the merge borders' value
  bool setMergeBorders(bool val) {
    return m_mergeBorders=val;
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
  shared_ptr<Cell> get(int id);

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
  /** do we need to merge cell borders ( default yes) */
  bool m_mergeBorders;
  /** the list of cells */
  std::vector<shared_ptr<Cell> > m_cellsList;
  /** the number of rows ( set by buildPosToCellId ) */
  size_t m_numRows;
  /** the number of cols ( set by buildPosToCellId ) */
  size_t m_numCols;
  /** a vector used to store an id corresponding to each cell */
  std::vector<int> m_posToCellId;
  /** true if we need to send extra lines */
  bool m_hasExtraLines;

public:
  /** a virtual structure used to a cell */
  class Cell : public MWAWCell
  {
  public:
    //! constructor
    Cell() : MWAWCell(), m_box(), m_size() {
    }
    //! destructor
    virtual ~Cell() { }

    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Cell const &cell);

    /** call when a cell must be send.

    \note default: openTableCell(*this), call sendContent and closeTableCell() */
    virtual bool send(MWAWContentListenerPtr listener, MWAWTable &table);
    //! call when the content of a cell must be send
    virtual bool sendContent(MWAWContentListenerPtr listener, MWAWTable &table) = 0;

    /** the cell bounding box (unit in point)*/
    Box2f m_box;
    /** the cell size : unit WPX_POINT */
    Vec2f m_size;
  };

};

#endif
