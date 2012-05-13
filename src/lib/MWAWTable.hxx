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
  void setBox(Box2f const &box) {
    m_box = box;
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
      if (diffF) return (diffF < 0);
      int diff = c2.m_which - c1.m_which;
      if (diff) return (diff < 0);
      diffF = c1.m_cell->box().size()[m_coord]
              - c2.m_cell->box().size()[m_coord];
      if (diffF) return (diffF < 0);
      return long(c1.m_cell) < long(c2.m_cell);
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
    return m_cellsList.size();
  }
  //! returns the i^th cell
  shared_ptr<MWAWTableCell> get(int id);

  /** try to send the table

  Note: either send the table ( and returns true ) or do nothing.
   */
  bool sendTable(MWAWContentListenerPtr listener);

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
