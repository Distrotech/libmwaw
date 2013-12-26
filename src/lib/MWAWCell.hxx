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

#include "MWAWEntry.hxx"

class MWAWTable;

/** a structure used to define a cell and its format */
class MWAWCell
{
public:
  /** the different format of a cell's content */
  enum FormatType { F_TEXT, F_BOOLEAN, F_NUMBER, F_DATE, F_TIME, F_UNKNOWN };
  /** the different number format of a cell's content */
  enum NumberType { F_NUMBER_CURRENCY, F_NUMBER_DECIMAL, F_NUMBER_FRACTION, F_NUMBER_GENERIC, F_NUMBER_SCIENTIFIC, F_NUMBER_PERCENT, F_NUMBER_UNKNOWN };
  /** a structure uses to define the format of a cell content */
  struct Format {
    //! constructor
    Format() : m_format(F_UNKNOWN), m_numberFormat(F_NUMBER_UNKNOWN), m_digits(-1), m_integerDigits(-1), m_numeratorDigits(1), m_denominatorDigits(1), m_thousandHasSeparator(false), m_currencySymbol("$"), m_DTFormat("")
    {
    }
    //! destructor
    virtual ~Format() {}
    //! returns true if this is a basic format style
    bool hasBasicFormat() const
    {
      return m_format==F_TEXT || m_format==F_UNKNOWN;
    }
    //! returns a value type
    std::string getValueType() const;
    //! get the numbering style
    bool getNumberingProperties(librevenge::RVNGPropertyList &propList) const;
    //! convert a DTFormat in a propertyList
    static bool convertDTFormat(std::string const &dtFormat, librevenge::RVNGPropertyListVector &propListVector);
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Format const &format);
    //! a comparison  function
    int compare(Format const &format) const;

    //! the cell format : by default unknown
    FormatType m_format;
    //! the numeric format
    NumberType m_numberFormat;
    //! the number of digits
    int m_digits;
    //! the number of main digits
    int m_integerDigits;
    //! the number of numerator digits
    int m_numeratorDigits;
    //! the number of denominator digits
    int m_denominatorDigits;
    //! true if we must separate the thousand
    bool m_thousandHasSeparator;
    //! the currency symbol ( default '$')
    std::string m_currencySymbol;
    //! a date/time format ( using a subset of strftime format )
    std::string m_DTFormat;
  };
  //! a comparaison structure used to store data
  struct CompareFormat {
    //! constructor
    CompareFormat() {}
    //! comparaison function
    bool operator()(Format const &c1, Format const &c2) const
    {
      return c1.compare(c2) < 0;
    }
  };
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
    m_format(), m_hAlign(HALIGN_DEFAULT), m_vAlign(VALIGN_DEFAULT),
    m_backgroundColor(MWAWColor::white()), m_protected(false),
    m_bordersList(), m_extraLine(E_None), m_extraLineType() { }

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
  virtual bool send(MWAWListenerPtr listener, MWAWTable &table);
  /** function called when the content of a cell must be send to the listener,
      ie. when MWAWTable::sendTable or MWAWTable::sendAsText is called.

      \note default behavior: does nothing and prints an error in debug mode.*/
  virtual bool sendContent(MWAWListenerPtr listener, MWAWTable &table);

  // position

  //! position  accessor
  Vec2i const &position() const
  {
    return m_position;
  }
  //! set the cell positions :  0,0 -> A1, 0,1 -> A2
  void setPosition(Vec2i posi)
  {
    m_position = posi;
  }

  //! returns the number of spanned cells
  Vec2i const &numSpannedCells() const
  {
    return m_numberCellSpanned;
  }
  //! sets the number of spanned cells : Vec2i(1,1) means 1 cellule
  void setNumSpannedCells(Vec2i numSpanned)
  {
    m_numberCellSpanned=numSpanned;
  }

  //! bdbox  accessor
  Box2f const &bdBox() const
  {
    return m_bdBox;
  }
  //! set the bdbox (unit point)
  void setBdBox(Box2f box)
  {
    m_bdBox = box;
  }

  //! bdbox size accessor
  Vec2f const &bdSize() const
  {
    return m_bdSize;
  }
  //! set the bdbox size(unit point)
  void setBdSize(Vec2f sz)
  {
    m_bdSize = sz;
  }

  //! return the name of a cell (given row and column) : 0,0 -> A1, 0,1 -> A2
  static std::string getCellName(Vec2i const &pos, Vec2b const &absolute);

  //! return the column name
  static std::string getColumnName(int col);

  // format

  //! returns the cell format
  Format const &getFormat() const
  {
    return m_format;
  }
  //! set the cell format
  void setFormat(Format const &format)
  {
    m_format=format;
  }
  //! returns true if the cell is protected
  bool isProtected() const
  {
    return m_protected;
  }
  //! sets the cell's protected flag
  void setProtected(bool fl)
  {
    m_protected = fl;
  }

  //! returns the horizontal alignement
  HorizontalAlignment hAlignement() const
  {
    return m_hAlign;
  }
  //! sets the horizontal alignement
  void setHAlignement(HorizontalAlignment align)
  {
    m_hAlign = align;
  }

  //! returns the vertical alignement
  VerticalAlignment vAlignement() const
  {
    return m_vAlign;
  }
  //! sets the vertical alignement
  void setVAlignement(VerticalAlignment align)
  {
    m_vAlign = align;
  }

  //! return true if the cell has some border
  bool hasBorders() const
  {
    return m_bordersList.size() != 0;
  }
  //! return the cell border: libmwaw::Left | ...
  std::vector<MWAWBorder> const &borders() const
  {
    return m_bordersList;
  }

  //! reset the border
  void resetBorders()
  {
    m_bordersList.resize(0);
  }
  //! sets the cell border: wh=libmwaw::LeftBit|...
  void setBorders(int wh, MWAWBorder const &border);

  //! returns the background color
  MWAWColor backgroundColor() const
  {
    return m_backgroundColor;
  }
  //! sets the background color
  void setBackgroundColor(MWAWColor color)
  {
    m_backgroundColor = color;
  }
  //! returns true if we have some extra lines
  bool hasExtraLine() const
  {
    return m_extraLine!=E_None && !m_extraLineType.isEmpty();
  }
  //! returns the extra lines
  ExtraLine extraLine() const
  {
    return m_extraLine;
  }
  //! returns the extra line border
  MWAWBorder const &extraLineType() const
  {
    return m_extraLineType;
  }
  //! sets the extraline
  void setExtraLine(ExtraLine extrLine, MWAWBorder const &type=MWAWBorder())
  {
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

  //! the cell format
  Format m_format;
  //! the cell alignement : by default nothing
  HorizontalAlignment m_hAlign;
  //! the vertical cell alignement : by default nothing
  VerticalAlignment m_vAlign;
  //! the backgroung color
  MWAWColor m_backgroundColor;
  //! cell protected
  bool m_protected;

  //! the cell border MWAWBorder::Pos
  std::vector<MWAWBorder> m_bordersList;
  /** extra line */
  ExtraLine m_extraLine;
  /** extra line type */
  MWAWBorder m_extraLineType;
};

//! small class use to define a sheet cell content
class MWAWCellContent
{
public:
  //! small class use to define a formula instruction
  struct FormulaInstruction {
    enum Type { F_Operator, F_Function, F_Cell, F_CellList, F_Long, F_Double, F_Text };
    //! constructor
    FormulaInstruction() : m_type(F_Text), m_content(""), m_longValue(0), m_doubleValue(0)
    {
      for (int i=0; i<2; ++i) {
        m_position[i]=Vec2i(0,0);
        m_positionRelative[i]=Vec2b(false,false);
      }
    }
    //! return a proplist corresponding to a instruction
    librevenge::RVNGPropertyList getPropertyList() const;
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, FormulaInstruction const &inst);
    //! the type
    Type m_type;
    //! the content ( if type == F_Operator or type = F_Function or type==F_Text)
    std::string m_content;
    //! value ( if type==F_Long )
    double m_longValue;
    //! value ( if type==F_Double )
    double m_doubleValue;
    //! cell position ( if type==F_Cell or F_CellList )
    Vec2i m_position[2];
    //! relative cell position ( if type==F_Cell or F_CellList )
    Vec2b m_positionRelative[2];
  };

  /** the different types of cell's field */
  enum Type { C_NONE, C_TEXT, C_NUMBER, C_FORMULA, C_UNKNOWN };
  /// constructor
  MWAWCellContent() : m_contentType(C_UNKNOWN), m_value(0.0), m_valueSet(false), m_textEntry(), m_formula() { }
  /// destructor
  ~MWAWCellContent() {}
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, MWAWCellContent const &cell);

  //! returns true if the cell has no content
  bool empty() const
  {
    if (m_contentType == C_NUMBER) return false;
    if (m_contentType == C_TEXT && !m_textEntry.valid()) return false;
    if (m_contentType == C_FORMULA && (m_formula.size() || isValueSet())) return false;
    return true;
  }
  //! sets the double value
  void setValue(double value)
  {
    m_value = value;
    m_valueSet = true;
  }
  //! returns true if the value has been setted
  bool isValueSet() const
  {
    return m_valueSet;
  }
  //! returns true if the text is set
  bool hasText() const
  {
    return m_textEntry.valid();
  }
  /** conversion beetween double days since 1900 and date */
  static bool double2Date(double val, int &Y, int &M, int &D);
  /** conversion beetween double: second since 0:00 and time */
  static bool double2Time(double val, int &H, int &M, int &S);

  //! the content type ( by default unknown )
  Type m_contentType;
  //! the cell value
  double m_value;
  //! true if the value has been set
  bool m_valueSet;
  //! the cell string
  MWAWEntry m_textEntry;
  //! the formula list of instruction
  std::vector<FormulaInstruction> m_formula;
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
