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

namespace MSWStruct
{
//! the font structure
struct Font {
  enum { NumFlags=9 };

  //! the constructor
  Font(): m_font(MWAWFont(-1,0)), m_size(0), m_value(0), m_picturePos(0), m_unknown(0), m_extra("") {
    for (int i = 0; i < NumFlags; i++) m_flags[i]=Variable<int>(0);
  }

  //! insert new font data ( beginning by updating font flags )
  void insert(Font const &font);

  //! returns the font flags
  uint32_t getFlags() const;

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

//! the section structure
struct Section {
  //! constructor
  Section() : m_id(-1), m_type(0), m_paragraphId(-9999), m_col(1),
    m_colSep(0.5), m_colBreak(false), m_flag(0), m_error("") {
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
    m_error+=sec.m_error;
  }
  //! try to read a data
  bool read(MWAWInputStreamPtr &input, long endPos);

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
  std::string m_error;
};

//! the paragraph structure
struct Paragraph : public MWAWParagraph {
  struct Cell;

  //! Constructor
  Paragraph() : m_dim(), m_font(), m_font2(), m_modFont(), m_section(),
    m_inCell(false), m_tableDef(false), m_tableColumns(), m_tableCells() {
  }
  //! insert the new values
  void insert(Paragraph const &para, bool insertModif=true) {
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
    m_inCell.insert(para.m_inCell);
    m_tableDef.insert(para.m_tableDef);
    m_tableColumns.insert(para.m_tableColumns);
    m_tableCells.insert(para.m_tableCells);
  }
  //! try to read a data
  bool read(MWAWInputStreamPtr &input, long endPos);
  //! returns the font which correspond to the paragraph if possible
  bool getFont(Font &font) const;
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Paragraph const &ind);

  //! operator<<
  void print(std::ostream &o, MWAWFontConverterPtr m_convertissor) const;

  //! the dimension
  Variable<Vec2f> m_dim;
  //! the font (simplified)
  Variable<Font> m_font, m_font2 /** font ( not simplified )*/, m_modFont /** font (modifier) */;
  //! the section
  Variable<Section> m_section;
  //! a cell/textbox
  Variable<bool> m_inCell;
  //! a table
  Variable<bool> m_tableDef;
  //! the table columns
  Variable<std::vector<float> > m_tableColumns;
  //! the table cells
  Variable<std::vector<Cell> > m_tableCells;

  //! the cells definitions
  struct Cell {
    Cell() : m_extra("") {
      for (int i = 0; i < 4; i++) m_borders[i] = false;
    }
    bool hasBorders() const {
      for (int i = 0; i < 4; i++)
        if (m_borders[i]) return true;
      return false;
    }
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Cell const &cell);
    /* the borders TLBR */
    bool m_borders[4];
    /* extra data */
    std::string m_extra;
  };
};
}
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
