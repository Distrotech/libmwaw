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

/* Define some classes used to store a Cell
 */

#ifndef MWAW_CELL_H
#  define MWAW_CELL_H

#include <string>
#include <vector>

#include "libmwaw_internal.hxx"

class WPXPropertyList;

/** a structure used to define a cell and its format */
class MWAWCell
{
public:
  /** the default horizontal alignement.

  \note actually mainly used for table/spreadsheet cell, FULL is not yet implemented */
  enum HorizontalAlignment { HALIGN_LEFT, HALIGN_RIGHT, HALIGN_CENTER,
                             HALIGN_FULL, HALIGN_DEFAULT
                           };

  /** the default vertical alignement.
  \note actually mainly used for table/spreadsheet cell,  not yet implemented */
  enum VerticalAlignment { VALIGN_TOP, VALIGN_CENTER, VALIGN_BOTTOM, VALIGN_DEFAULT };

  //! constructor
  MWAWCell() : m_position(0,0), m_numberCellSpanned(1,1),
    m_hAlign(HALIGN_DEFAULT), m_vAlign(VALIGN_DEFAULT), m_bordersList(),
    m_backgroundColor(MWAWColor::white()), m_protected(false) { }

  //! destructor
  virtual ~MWAWCell() {}

  /** adds to the propList*/
  void addTo(WPXPropertyList &propList) const;

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, MWAWCell const &cell);

  // position

  //! position  accessor
  Vec2i &position() {
    return m_position;
  }
  //! position  accessor
  Vec2i const &position() const {
    return m_position;
  }
  //! set the cell positions :  0,0 -> A1, 0,1 -> A2
  void setPosition(Vec2i posi) {
    m_position = posi;
  }

  //! returns the number of spanned cells
  Vec2i const &numSpannedCells() const {
    return m_numberCellSpanned;
  }
  //! sets the number of spanned cells : Vec2i(1,1) means 1 cellule
  void setNumSpannedCells(Vec2i numSpanned) {
    m_numberCellSpanned=numSpanned;
  }

  //! return the name of a cell (given row and column) : 0,0 -> A1, 0,1 -> A2
  static std::string getCellName(Vec2i const &pos, Vec2b const &absolute);

  //! return the column name
  static std::string getColumnName(int col);

  // format

  //! returns true if the cell is protected
  bool isProtected() const {
    return m_protected;
  }
  //! returns true if the cell is protected
  void setProtected(bool fl) {
    m_protected = fl;
  }

  //! returns the horizontal alignement
  HorizontalAlignment hAlignement() const {
    return m_hAlign;
  }
  //! sets the horizontal alignement
  void setHAlignement(HorizontalAlignment align) {
    m_hAlign = align;
  }

  //! returns the vertical alignement
  VerticalAlignment vAlignement() const {
    return m_vAlign;
  }
  //! sets the vertical alignement
  void setVAlignement(VerticalAlignment align) {
    m_vAlign = align;
  }

  //! return true if the cell has some border
  bool hasBorders() const {
    return m_bordersList.size() != 0;
  }
  //! return the cell border: libmwaw::Left | ...
  std::vector<MWAWBorder> const &borders() const {
    return m_bordersList;
  }

  //! reset the border
  void resetBorders() {
    m_bordersList.resize(0);
  }
  //! sets the cell border: wh=libmwaw::Left|...
  void setBorders(int wh, MWAWBorder const &border);

  //! returns the background color
  MWAWColor backgroundColor() const {
    return m_backgroundColor;
  }
  //! set the background color
  void setBackgroundColor(MWAWColor color) {
    m_backgroundColor = color;
  }

protected:
  //! the cell row and column : 0,0 -> A1, 0,1 -> A2
  Vec2i m_position;
  //! the cell spanned : by default (1,1)
  Vec2i m_numberCellSpanned;

  //! the cell alignement : by default nothing
  HorizontalAlignment m_hAlign;
  //! the vertical cell alignement : by default nothing
  VerticalAlignment m_vAlign;
  //! the cell border MWAWBorder::Pos
  std::vector<MWAWBorder> m_bordersList;
  //! the backgroung color
  MWAWColor m_backgroundColor;
  //! cell protected
  bool m_protected;
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
