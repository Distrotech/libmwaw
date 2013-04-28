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

#ifndef MWAW_TABLE_HELPER
#  define MWAW_TABLE_HELPER

#include <iostream>
#include <vector>

#include "libmwaw_internal.hxx"

class MWAWContentListener;
typedef shared_ptr<MWAWContentListener> MWAWContentListenerPtr;


class MWAWTable;

/** a virtual structure used to store/send a cell to a listener */
class MWAWTableCell
{
  friend class MWAWTable;
public:
  //! constructor
  MWAWTableCell() : m_box(), m_position(-1,-1), m_numberCellSpanned() {
  }
  //! destructor
  virtual ~MWAWTableCell() { }
  //! set the bounding box (units in point)
  void setBox(Box2f const &dim) {
    m_box = dim;
  }
  //! return the bounding box
  Box2f const &box() const {
    return m_box;
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, MWAWTableCell const &cell) {
    if (cell.m_position.x() >= 0) {
      o << "pos=" << cell.m_position << ",";
      if (cell.m_numberCellSpanned[0]!=1 || cell.m_numberCellSpanned[1]!=1)
        o << "span=" << cell.m_position << ",";
    }
    o << "box=" << cell.m_box << ",";
    return o;
  }

  //! call when a cell must be send
  virtual bool send(MWAWContentListenerPtr listener) = 0;

  //! call when the content of a cell must be send
  virtual bool sendContent(MWAWContentListenerPtr listener) = 0;

protected:
  //! a comparaison structure used retrieve the rows and the columns
  struct Compare {
    Compare(int dim) : m_coord(dim) {}
    //! small structure to define a cell point
    struct Point {
      Point(int wh, MWAWTableCell const *cell) : m_which(wh), m_cell(cell) {}
      float getPos(int coord) const {
        if (m_which)
          return m_cell->box().max()[coord];
        return m_cell->box().min()[coord];
      }
      float getSize(int coord) const {
        return m_cell->box().size()[coord];
      }
      int m_which;
      MWAWTableCell const *m_cell;
    };

    //! comparaison function
    bool operator()(Point const &c1, Point const &c2) const {
      float diffF = c1.getPos(m_coord)-c2.getPos(m_coord);
      if (diffF < 0) return true;
      if (diffF > 0) return false;
      int diff = c2.m_which - c1.m_which;
      if (diff) return (diff < 0);
      diffF = c1.m_cell->box().size()[m_coord]
              - c2.m_cell->box().size()[m_coord];
      if (diffF < 0) return true;
      if (diffF > 0) return false;
      return ssize_t(c1.m_cell) < ssize_t(c2.m_cell);
    }

    //! the coord to compare
    int m_coord;
  };

protected:
  /** the cell bounding box (unit in point)*/
  Box2f m_box;

  /** the final position in the table */
  Vec2i m_position, m_numberCellSpanned /** the number of cell span */;
};

class MWAWTable
{
public:
  //! the constructor
  MWAWTable() : m_cellsList(), m_rowsSize(), m_colsSize() {}

  //! the destructor
  virtual ~MWAWTable();

  //! add a new cells
  void add(shared_ptr<MWAWTableCell> cell) {
    m_cellsList.push_back(cell);
  }

  //! returns the number of cell
  int numCells() const {
    return int(m_cellsList.size());
  }
  //! returns the i^th cell
  shared_ptr<MWAWTableCell> get(int id);

  /** try to send the table

  Note: either send the table ( and returns true ) or do nothing.
   */
  bool sendTable(MWAWContentListenerPtr listener);

  /** a function called just before calling listener->openTable(),
      to insert extra data
   */
  virtual void sendPreTableData(MWAWContentListenerPtr ) {}

  /** try to send the table as basic text */
  bool sendAsText(MWAWContentListenerPtr listener);

protected:
  //! create the correspondance list, ...
  bool buildStructures();

  /** the list of cells */
  std::vector<shared_ptr<MWAWTableCell> > m_cellsList;
  /** the final row and col size (in point) */
  std::vector<float> m_rowsSize, m_colsSize;
};

#endif
