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
 * Class to read/store the MSW structures
 */

#ifndef MSW_STRUCT
#  define MSW_STRUCT

#include <iostream>
#include <string>
#include <vector>

#include "libmwaw_internal.hxx"

#include "MWAWFont.hxx"
#include "MWAWParagraph.hxx"

class MWAWInputStream;
typedef shared_ptr<MWAWInputStream> MWAWInputStreamPtr;
class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

/** namespace to store the main structure which appears in a Microsoft Word 3.0-5.0 file */
namespace MSWStruct
{
//! generic function use to fill a border using the read data
MWAWBorder getBorder(int val, std::string &extra);

//! the font structure of a Microsoft Word file
struct Font {
  enum { NumFlags /** the number of flags needed to store all datas*/=9 };

  //! the constructor
  Font(): m_font(MWAWFont(-1,0)), m_size(0), m_value(0), m_picturePos(0), m_unknown(0), m_extra("") {
    for (int i = 0; i < NumFlags; i++) m_flags[i]=Variable<int>(0);
  }

  //! insert new font data ( beginning by updating font flags )
  void insert(Font const &font);

  //! returns the font flags
  uint32_t getFlags() const;

  //! returns the underline style
  MWAWBorder::Style getUnderlineStyle() const;

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Font const &font);

  //! the font
  Variable<MWAWFont> m_font;
  //! a second size
  Variable<int> m_size;
  //! a unknown value
  Variable<int> m_value;
  //! a list of flags
  Variable<int> m_flags[NumFlags];
  //! a picture file position (if this corresponds to a picture)
  Variable<long> m_picturePos;
  //! some unknown flag
  Variable<int> m_unknown;
  //! extra data
  std::string m_extra;
};

//! the section structure of a Microsoft Word file
struct Section {
  //! constructor
  Section() : m_id(-1), m_type(0), m_paragraphId(-9999), m_col(1),
    m_colSep(0.5), m_colBreak(false), m_flag(0), m_extra("") {
  }
  //! insert the new values
  void insert(Section const &sec) {
    m_id.insert(sec.m_id);
    m_type.insert(sec.m_type);
    m_paragraphId.insert(sec.m_paragraphId);
    m_col.insert(sec.m_col);
    m_colSep.insert(sec.m_colSep);
    m_colBreak.insert(sec.m_colBreak);
    m_flag.insert(sec.m_flag);
    m_extra+=sec.m_extra;
  }
  //! try to read a data
  bool read(MWAWInputStreamPtr &input, long endPos);
  //! try to read a data ( v3 code )
  bool readV3(MWAWInputStreamPtr &input, long endPos);

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Section const &section);

  //! the identificator
  Variable<int> m_id;
  //! the type
  Variable<int> m_type;
  //! the paragraph id
  Variable<int> m_paragraphId;
  //! the num of columns
  Variable<int> m_col;
  //! the spacing between column
  Variable<float> m_colSep;
  //! only a column break
  Variable<bool> m_colBreak;
  //! some flag ( in the main position)
  Variable<int> m_flag;
  /** the errors */
  std::string m_extra;
};

//! the table in a Microsoft Word file
struct Table {
  struct Cell;
  //! constructor
  Table() : m_height(0), m_justify(libmwaw::JustificationLeft), m_indent(0),
    m_columns(), m_cells(), m_extra("") {
  }
  //! insert the new values
  void insert(Table const &table) {
    m_height.insert(table.m_height);
    m_justify.insert(table.m_justify);
    m_indent.insert(table.m_indent);
    m_columns.insert(table.m_columns);
    size_t tNumCells = table.m_cells.size();
    if (tNumCells > m_cells.size())
      m_cells.resize(tNumCells, Variable<Cell>());
    for (size_t i=0; i < tNumCells; i++) {
      if (!m_cells[i].isSet())
        m_cells[i] = table.m_cells[i];
      else if (table.m_cells[i].isSet())
        m_cells[i]->insert(*table.m_cells[i]);
    }
    m_extra+=table.m_extra;
  }
  //! try to read a data
  bool read(MWAWInputStreamPtr &input, long endPos);
  //! returns the ith Cell
  Variable<Cell> &getCell(int id);

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Table const &table);

  //! the row height in inches
  Variable<float> m_height;
  //! the justification
  Variable<libmwaw::Justification> m_justify;
  //! the indent
  Variable<float> m_indent;
  //! the table columns
  Variable<std::vector<float> > m_columns;
  //! the table cells
  std::vector<Variable<Cell> > m_cells;
  /** the errors */
  std::string m_extra;

  //! the cells definitions in a Microsoft Word Table
  struct Cell {
    //! constructor
    Cell() : m_borders(), m_backColor(1.0f), m_extra("") {
    }
    //! update the cell data by merging
    void insert(Cell const &cell) {
      size_t cNumBorders = cell.m_borders.size();
      if (cNumBorders > m_borders.size())
        m_borders.resize(cNumBorders);
      for (size_t i=0; i < cNumBorders; i++)
        if (cell.m_borders[i].isSet()) m_borders[i]=*cell.m_borders[i];
      m_backColor.insert(cell.m_backColor);
      m_extra+=cell.m_extra;
    }
    //! returns true if the cell has borders
    bool hasBorders() const {
      for (size_t i = 0; i < m_borders.size(); i++)
        if (m_borders[i].isSet() && m_borders[i]->m_style != MWAWBorder::None)
          return true;
      return false;
    }
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Cell const &cell);
    /** the borders TLBR */
    std::vector<Variable<MWAWBorder> > m_borders;
    /** the background gray color */
    Variable<float> m_backColor;
    /** extra data */
    std::string m_extra;
  };
};

//! the paragraph structure of a Microsoft Word file
struct Paragraph : public MWAWParagraph {
  //! Constructor
  Paragraph(int version) : MWAWParagraph(), m_version(version), m_dim(), m_font(), m_font2(), m_modFont(), m_section(),
    m_bordersStyle(), m_inCell(false), m_tableDef(false), m_table() {
  }
  //! insert the new values
  void insert(Paragraph const &para, bool insertModif=true) {
    MWAWParagraph::insert(para);
    m_dim.insert(para.m_dim);
    if (!m_font.isSet())
      m_font=para.m_font;
    else if (para.m_font.isSet())
      m_font->insert(*para.m_font);
    if (!m_font2.isSet())
      m_font2 = para.m_font2;
    else if (para.m_font2.isSet())
      m_font2->insert(*para.m_font2);
    if (insertModif)
      m_modFont->insert(*para.m_modFont);
    if (!m_section.isSet())
      m_section = para.m_section;
    else if (para.m_section.isSet())
      m_section->insert(*para.m_section);
    if (!m_bordersStyle.isSet() || para.m_bordersStyle.isSet())
      m_bordersStyle = para.m_bordersStyle;
    m_inCell.insert(para.m_inCell);
    if (!m_table.isSet())
      m_table = para.m_table;
    else if (para.m_table.isSet())
      m_table->insert(*para.m_table);
    m_tableDef.insert(para.m_tableDef);
  }
  //! try to read a data
  bool read(MWAWInputStreamPtr &input, long endPos);
  //! returns the font which correspond to the paragraph if possible
  bool getFont(Font &font) const;
  //! returns true if we are in table
  bool inTable() const {
    return m_inCell.get();
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Paragraph const &ind);

  //! operator<<
  void print(std::ostream &o, MWAWFontConverterPtr m_convertissor) const;

  //! the file version
  int m_version;
  //! the dimension
  Variable<Vec2f> m_dim;
  //! the font (simplified)
  Variable<Font> m_font, m_font2 /** font ( not simplified )*/, m_modFont /** font (modifier) */;
  //! the section
  Variable<Section> m_section;
  //! the border style ( old v3)
  Variable<MWAWBorder> m_bordersStyle;
  //! a cell/textbox
  Variable<bool> m_inCell;
  //! a table flag
  Variable<bool> m_tableDef;
  //! the table
  Variable<Table> m_table;
};
}
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
