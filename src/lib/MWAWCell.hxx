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

/** \file MWAWCell.hxx
 * Defines MWAWCell (cell content and format)
 */

#ifndef MWAW_CELL_H
#  define MWAW_CELL_H

#include <string>
#include <vector>

#include "libmwaw_internal.hxx"

class MWAWTable;

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

  //! an enum to defined potential internal line: E_Line1=TL to RB, E_Line2=BL to RT
  enum ExtraLine { E_None, E_Line1, E_Line2, E_Cross };

  //! constructor
  MWAWCell() : m_position(0,0), m_numberCellSpanned(1,1), m_bdBox(),  m_bdSize(),
    m_hAlign(HALIGN_DEFAULT), m_vAlign(VALIGN_DEFAULT), m_bordersList(),
    m_backgroundColor(MWAWColor::white()), m_protected(false),
    m_extraLine(E_None), m_extraLineType() { }

  //! destructor
  virtual ~MWAWCell() {}

  /** adds to the propList*/
  void addTo(librevenge::RVNGPropertyList &propList) const;

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, MWAWCell const &cell);

  // interface with MWAWTable:

  /** function called when a cell is send by MWAWTable to send a cell to a
      listener.

      By default: calls openTableCell(*this), sendContent and then closeTableCell() */
  virtual bool send(MWAWContentListenerPtr listener, MWAWTable &table);
  /** function called when the content of a cell must be send to the listener,
      ie. when MWAWTable::sendTable or MWAWTable::sendAsText is called.

      \note default behavior: does nothing and prints an error in debug mode.*/
  virtual bool sendContent(MWAWContentListenerPtr listener, MWAWTable &table);

  // position

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

  //! bdbox  accessor
  Box2f const &bdBox() const {
    return m_bdBox;
  }
  //! set the bdbox (unit point)
  void setBdBox(Box2f box) {
    m_bdBox = box;
  }

  //! bdbox size accessor
  Vec2f const &bdSize() const {
    return m_bdSize;
  }
  //! set the bdbox size(unit point)
  void setBdSize(Vec2f sz) {
    m_bdSize = sz;
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
  //! sets the background color
  void setBackgroundColor(MWAWColor color) {
    m_backgroundColor = color;
  }
  //! returns true if we have some extra lines
  bool hasExtraLine() const {
    return m_extraLine!=E_None && !m_extraLineType.isEmpty();
  }
  //! returns the extra lines
  ExtraLine extraLine() const {
    return m_extraLine;
  }
  //! returns the extra line border
  MWAWBorder const &extraLineType() const {
    return m_extraLineType;
  }
  //! sets the extraline
  void setExtraLine(ExtraLine extrLine, MWAWBorder const &type=MWAWBorder()) {
    m_extraLine = extrLine;
    m_extraLineType=type;
  }
protected:
  //! the cell row and column : 0,0 -> A1, 0,1 -> A2
  Vec2i m_position;
  //! the cell spanned : by default (1,1)
  Vec2i m_numberCellSpanned;

  /** the cell bounding box (unit in point)*/
  Box2f m_bdBox;

  /** the cell bounding size : unit point */
  Vec2f m_bdSize;

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
  /** extra line */
  ExtraLine m_extraLine;
  /** extra line type */
  MWAWBorder m_extraLineType;
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
