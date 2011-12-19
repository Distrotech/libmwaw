/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libwpd
 * Copyright (C) 2002 William Lachance (wrlach@gmail.com)
 * Copyright (C) 2002 Marc Maurer (uwog@uwog.net)
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

// DMWAWTable: an intermediate representation of a table, designed to be created
// "ahead of time". unlike wordperfect's table definition messages, this representation
// is _consistent_: we can always count on the messages being sent using this representation
// (once it is created and finalized) to be reliable (assuming no bugs in this code!) :-)
//
// example situation where this might be useful: WordPerfect allows two cells,
// side by side, one with border, one without-- creating a false ambiguity (none
// actually exists: if one cell does not have a border, the other doesn't either)

#ifndef _WPXTABLE_H
#define _WPXTABLE_H
#include <vector>
#include "libmwaw_libwpd_types.hxx"

typedef struct _DMWAWTableCell DMWAWTableCell;

struct _DMWAWTableCell {
  _DMWAWTableCell(uint8_t colSpan, uint8_t rowSpan, uint8_t borderBits);
  uint8_t m_colSpan;
  uint8_t m_rowSpan;
  uint8_t m_borderBits;
};

class DMWAWTable
{
public:
  DMWAWTable() : m_tableRows() {}
  ~DMWAWTable();
  void insertRow();
  void insertCell(uint8_t colSpan, uint8_t rowSpan, uint8_t borderBits);
  const DMWAWTableCell  *getCell(int i, int j) {
    return (m_tableRows[i])[j];
  }
  void makeBordersConsistent();
  void _makeCellBordersConsistent(DMWAWTableCell *cell, std::vector<DMWAWTableCell *> &adjacentCells,
                                  int adjacencyBitCell, int adjacencyBitBoundCells);
  std::vector<DMWAWTableCell *>  _getCellsBottomAdjacent(int i, int j);
  std::vector<DMWAWTableCell *>  _getCellsRightAdjacent(int i, int j);

  const std::vector< std::vector<DMWAWTableCell *> >& getRows() const {
    return m_tableRows;
  }
  bool isEmpty() const {
    return m_tableRows.size() == 0;
  }

private:
  std::vector< std::vector<DMWAWTableCell *> > m_tableRows;
};

class DMWAWTableList
{
public:
  DMWAWTableList();
  DMWAWTableList(const DMWAWTableList &);
  DMWAWTableList &operator=(const DMWAWTableList &tableList);
  virtual ~DMWAWTableList();

  DMWAWTable *operator[](unsigned long i) {
    return (*m_tableList)[i];
  }
  void add(DMWAWTable *table) {
    m_tableList->push_back(table);
  }

private:
  void release();
  void acquire(int *refCount, std::vector<DMWAWTable *> *tableList);
  int *getRef() const {
    return m_refCount;
  }
  std::vector<DMWAWTable *> * get() const {
    return m_tableList;
  }

  std::vector<DMWAWTable *> *m_tableList;
  int *m_refCount;
};
#endif /* _WPXTABLE_H */
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
