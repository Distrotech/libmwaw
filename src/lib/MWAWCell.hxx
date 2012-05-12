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
 * For further information visit http://libmwaw.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

/* Define some classes used to store a Cell
 */

#ifndef MWAW_CELL_H
#  define MWAW_CELL_H

#include <string>

#include <libwpd/WPXString.h>

#include "libmwaw_internal.hxx"

class WPXPropertyList;

/** a structure used to defined the cell format, .. */
class MWAWCellFormat
{
public:
  /** the default horizontal alignement.

  \note actually mainly used for table/spreadsheet cell, FULL is not yet implemented */
  enum HorizontalAlignment { HALIGN_LEFT, HALIGN_RIGHT, HALIGN_CENTER,
                             HALIGN_FULL, HALIGN_DEFAULT
                           };

  /** the different types of cell's field */
  enum Format { F_TEXT, F_NUMBER, F_DATE, F_TIME, F_UNKNOWN };

  /*   subformat:
            NUMBER             DATE                 TIME       TEXT
    0 :    default           default               default    default
    1 :    decimal            3/2/00             10:03:00 AM  -------
    2 :   exponential      3 Feb, 2000            10:03 AM    -------
    3 :   percent             3, Feb              10:03:00    -------
    4 :    money             Feb, 2000              10:03     -------
    5 :    thousand       Thu, 3 Feb, 2000         -------    -------
    6 :  percent/thou     3 February 2000          -------    -------
    7 :   money/thou  Thursday, February 3, 2000   -------    -------

   */
  //! constructor
  MWAWCellFormat() : m_format(F_UNKNOWN), m_subFormat(0), m_digits(0),
    m_hAlign(HALIGN_DEFAULT), m_bordersList(0), m_protected(false) { }

  virtual ~MWAWCellFormat() {}

  //! returns the format type
  Format format() const {
    return m_format;
  }
  //! returns the subformat type
  int subformat() const {
    return m_subFormat;
  }
  //! sets the format type
  void setFormat(Format format, int subformat=0) {
    m_format = format;
    m_subFormat = subformat;
  }
  //! sets the subformat
  void setSubformat(int subFormat) {
    m_subFormat = subFormat;
  }

  //! returns the number of digits ( for a number)
  int digits() const {
    return m_digits;
  }
  //! set the number of digits ( for a number)
  void setDigits(int newDigit) {
    m_digits = newDigit;
  }

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

  //! return true if the cell has some border
  bool hasBorders() const {
    return m_bordersList != 0;
  }
  //! return the cell border: DMWAW_TABLE_CELL_LEFT_BORDER_OFF | ...
  int borders() const {
    return m_bordersList;
  }
  //! sets the cell border
  void setBorders(int bList) {
    m_bordersList = bList;
  }

  //! a comparison  function
  int compare(MWAWCellFormat const &cell) const;

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, MWAWCellFormat const &cell);

protected:
  //! the cell format : by default unknown
  Format m_format;
  //! the sub format
  int m_subFormat;
  //! the number of digits
  int m_digits;
  //! the cell alignement : by default nothing
  HorizontalAlignment m_hAlign;
  //! the cell border DMWAW_TABLE_CELL_LEFT_BORDER_OFF | ...
  int m_bordersList;
  //! cell protected
  bool m_protected;
};

/** a structure used to defined the cell content */
class MWAWCellContent
{
public:
  /** the different types of cell's field */
  enum Content { C_NONE, C_TEXT, C_NUMBER, C_FORMULA, C_UNKNOWN };

  //! the constructor
  MWAWCellContent() : m_contentType(C_UNKNOWN), m_value(0.0), m_valueSet(false),
    m_textValue(""), m_textValueSet(false), m_formulaValue("") { }
  virtual ~MWAWCellContent() {}

  //! returns the content type
  Content content() const {
    return m_contentType;
  }
  //! set the content type
  void setContent(Content type) {
    m_contentType = type;
  }

  //! sets the double value
  void setValue(double value) {
    m_value = value;
    m_valueSet = true;
  }
  //! return the double value
  double value() const {
    return m_value;
  }
  //! returns true if the value has been setted
  bool isValueSet() const {
    return m_valueSet;
  }

  //! sets the text value
  void setText(std::string const value) {
    m_textValue = value;
    m_textValueSet = true;
  }
  //! returns the text value
  std::string const &text() const {
    return m_textValue;
  }
  //! returns true if the text is set
  bool hasText() const {
    return m_textValue.size() != 0;
  }
  //! returns true if the text has been setted
  bool isTextSet() const {
    return m_textValueSet;
  }

  //! sets the formula value
  void setFormula(std::string const value) {
    m_formulaValue = value;
  }
  //! returns the formula value
  std::string const &formula() const {
    return m_formulaValue;
  }

  //! returns true if the cell has no content
  bool empty() const {
    if (m_contentType == C_NUMBER) return false;
    if (m_contentType == C_TEXT && m_textValue.size()) return false;
    if (m_contentType == C_FORMULA && (m_formulaValue.size() || isValueSet())) return false;
    return true;
  }

  /** If the content is a data cell, filled property and returns in text, a string
      which can be used as text.

      \note - if not, property and text will be empty.
            - if ok, adds in property office:value-type, office:[|date-|time-]value
         and table:formula if neeed
   */
  bool getDataCellProperty(MWAWCellFormat::Format format, WPXPropertyList &property,
                           std::string &text) const;

  /** conversion beetween double days since 1900 and date */
  static bool double2Date(double val, int &Y, int &M, int &D);
  /** conversion beetween double: second since 0:00 and time */
  static bool double2Time(double val, int &H, int &M, int &S);

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, MWAWCellContent const &cell);

protected:
  //! the content type ( by default unknown )
  Content m_contentType;

  //! the cell value
  double m_value;
  //! true if the value has been set
  bool m_valueSet;

  //! the cell string
  std::string m_textValue;
  //! true if the text value has been set
  bool m_textValueSet;
  //! the formula string
  std::string m_formulaValue;
};

/** a structure used to defined the cell position, and a format */
class MWAWCell : public MWAWCellFormat
{
public:
  //! constructor
  MWAWCell() : m_position(0,0), m_numberCellSpanned(1,1) {}

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

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, MWAWCell const &cell);

  //! return the name of a cell (given row and column) : 0,0 -> A1, 0,1 -> A2
  static std::string getCellName(Vec2i const &pos, Vec2b const &absolute);

  //! return the column name
  static std::string getColumnName(int col);

protected:
  //! the cell row and column : 0,0 -> A1, 0,1 -> A2
  Vec2i m_position;
  //! the cell spanned : by default (1,1)
  Vec2i m_numberCellSpanned;
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
