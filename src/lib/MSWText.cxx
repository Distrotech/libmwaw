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

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>

#include <libwpd/WPXString.h>

#include "MWAWPosition.hxx"

#include "MWAWStruct.hxx"
#include "MWAWTools.hxx"
#include "MWAWContentListener.hxx"

#include "MSWText.hxx"

#include "MSWParser.hxx"

#define DEBUG_PLC 1

/** Internal: the structures of a MSWText */
namespace MSWTextInternal
{

////////////////////////////////////////
//! Internal: the entry of MSWParser
struct TextEntry : public MWAWEntry {
  TextEntry() : MWAWEntry(), m_pos(-1), m_id(0), m_type(0), m_function(0),
    m_value(0), m_fontId(-2), m_paragraphId(-2) {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, TextEntry const &entry) {
    if (entry.m_pos>=0) o << "textPos=" << entry.m_pos << ",";
    o << "id?=" << entry.m_id << ",";
    switch(entry.m_type) {
    case 0x80: // same line
    case 0: // contains a new line
      break;
    default:
      o << "#type=" << std::hex << entry.m_type << std::dec << ",";
      break;
    }
    if (entry.valid())
      o << std::hex << "fPos=" << entry.begin() << ":" << entry.end() << std::dec << ",";
    switch (entry.m_function) {
    case 0:
      break;
    case 0x80:
      o << "tP" << entry.m_value << ",";
      break;
    default: // see also readParagraph
      o << "f" << std::hex << entry.m_function << "=" <<  entry.m_value << std::dec << ",";
      break;
    }
    return o;
  }

  //! returns the paragraph id ( or -1, if unknown )
  int getParagraphId() const {
    if (m_function != 0x80) return -1;
    return m_value;
  }

  //! a struct used to compare file textpos
  struct CompareFilePos {
    //! comparaison function
    bool operator()(TextEntry const *t1, TextEntry const *t2) const {
      long diff = t1->begin()-t2->begin();
      return (diff < 0);
    }
  };
  //! the text position
  int m_pos;
  //! some identificator
  int m_id;
  //! the type
  int m_type;
  //! the function (if not 0)
  int m_function;
  //! the function paramater
  int m_value;
  //! the font id : -2: not fixed
  int m_fontId;
  //! the paragraph id : -2: not fixed
  int m_paragraphId;
};

////////////////////////////////////////
//! Internal: the plc
struct PLC {
  enum Type { Line, Section, Page, Font, Paragraph, Footnote, FootnoteDef, Field, Object, HeaderFooter, TextPosition };
  PLC(Type type, int id=0) : m_type(type), m_id(id), m_extra("") {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, PLC const &plc) {
    switch(plc.m_type) {
    case Line:
      o << "L";
      break;
    case Section:
      o << "S";
      break;
    case Footnote:
      o << "F";
      break;
    case FootnoteDef:
      o << "valF";
      break;
    case Field:
      o << "Field";
      break;
    case Page:
      o << "Page";
      break;
    case Font:
      o << "F";
      break;
    case Object:
      o << "O";
      break;
    case Paragraph:
      o << "P";
      break;
    case HeaderFooter:
      o << "hfP";
      break;
    case TextPosition:
      o << "textPos";
      break;
    default:
      o << "#type" + char('a'+int(plc.m_type));
    }
    if (plc.m_id < 0) o << "_";
    else o << plc.m_id;
    if (plc.m_extra.length()) o << "[" << plc.m_extra << "]";
    return o;
  }

  //! the plc type
  Type m_type;
  //! the identificator
  int m_id;
  //! some extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the line
struct Line {
  //! constructor
  Line() : m_id(-1), m_type(0), m_height(-1), m_value(0), m_error("") {
    for (int i = 0; i < 2; i++) m_flags[i] = 0;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Line const &line) {
    if (line.m_id >= 0) o << "L" << line.m_id << ":";
    else o << "L_:";
    switch(line.m_type) {
    case 0:
      break; // hard line break
    case 0x20:
      o << "soft,";
      break;
    default:
      if (line.m_type&0xf0) o << "type?=" << (line.m_type>>4) << ",";
      if (line.m_type&0x0f) o << "#unkn=" << (line.m_type&0xf) << ",";
      break;
    }
    if (line.m_height >= 0) o << "height=" << line.m_height << ",";
    if (line.m_value) o << "f0=" << line.m_value << ",";
    for (int i = 0; i < 2; i++)
      o << "fl" << i << "=" << std::hex << line.m_flags[i] << std::dec << ",";
    if (line.m_error.length()) o << line.m_error << ",";
    return o;
  }
  //! the identificator
  int m_id;
  //! the type
  int m_type;
  //! the height
  int m_height;
  //! a value ( between -1 and 9)
  int m_value;
  //! two flags ( first small, second a multiple of 4)
  int m_flags[2];
  /** the errors */
  std::string m_error;
};

////////////////////////////////////////
//! Internal: the section
struct Section {
  //! constructor
  Section() : m_id(-1), m_type(0), m_paragraphId(-9999), m_col(1), m_colSep(0.5), m_colBreak(false),
    m_flag(0), m_default(false), m_error("") {
  }
  //! try to read a data
  bool read(MWAWInputStreamPtr &input, long endPos) {
    long pos = input->tell();
    long dSz = endPos-pos;
    if (dSz < 1) return false;
    libmwaw::DebugStream f;
    int c = input->readULong(1), val;
    switch(c) {
    case 0x75: // column break
      if (dSz < 2) return false;
      val = input->readLong(1);
      switch(val) {
      case 0:
        m_colBreak = false;
        return true;
      case 1:
        m_colBreak = true;
        return true;
      default:
        f << "#f75=" << val << ",";
        break;
      }
      break;
    case 0x77: // num column
      if (dSz<3) return false;
      m_col = input->readLong(2)+1;
      return true;
    case 0x78:
      if (dSz<3) return false;
      m_colSep = input->readULong(2)/1440.;
      return true;
      // FIXME: UNKNOWN
    case 0x80: // a small number
    case 0x76: // always 1 ?
    case 0x79:
    case 0x7d: // always 1
    case 0x7e: // always 0
      if (dSz<2) return false;
      f << "f" << std::hex << c << std::dec << "=" << input->readLong(1) << ",";
      break;
    case 0x7b: // 2e1 3d08
    case 0x7c: // 4da, 6a5, 15ff
      if (dSz<3) return false;
      f << "f" << std::hex << c << std::dec << "=";
      f << std::hex << input->readULong(1) << std::dec << ":";
      f << std::hex << input->readULong(1) << std::dec << ",";
      break;
    case 0x82: // find one time with 168 (related to 7e ?)
      if (dSz<3) return false;
      f << "f" << std::hex << c << std::dec << "=" << input->readLong(2) << ",";
      break;
    case 0x83:
    case 0x84:
      if (dSz < 3) return false;
      val = input->readLong(2);
      if (c == 0x83) f << "header[top]=" << val/1440. << ",";
      else f << "header[bottom]=" << val/1440. << ",";
      break;
    default:
      return false;
    }
    m_error += f.str();
    return true;
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Section const &section) {
    if (section.m_id >= 0) o << "S" << section.m_id << ":";
    else o << "S_:";
    if (section.m_type) o << "type=" << std::hex << section.m_type << std::dec << ",";
    if (section.m_paragraphId > -9999) o << "sP=" << section.m_paragraphId << ",";
    if (section.m_col != 1) o << "cols=" << section.m_col << ",";
    if (section.m_colSep != 0.5) o << "colSep=" << section.m_colSep << "in,";
    if (section.m_colBreak) o << "colBreak,";
    if (section.m_flag)
      o << "fl=" << std::hex << section.m_flag << std::dec << ",";
    if (section.m_error.length()) o << section.m_error << ",";
    return o;
  }
  //! the identificator
  int m_id;
  //! the type
  int m_type;
  //! the paragraph id
  int m_paragraphId;
  //! the num of columns
  int m_col;
  //! the spacing between column
  float m_colSep;
  //! only a column break
  bool m_colBreak;
  //! some flag ( in the main position)
  int m_flag;
  //! true if is default
  bool m_default;
  /** the errors */
  std::string m_error;
};

////////////////////////////////////////
//! Internal: the page
struct Page {
  //! constructor
  Page() : m_id(-1), m_type(0), m_page(-1), m_error("") {
    for (int i = 0; i < 4; i++) m_values[i] = 0;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Page const &page) {
    if (page.m_id >= 0) o << "P" << page.m_id << ":";
    else o << "P_:";
    if (page.m_page != page.m_id+1) o << "page=" << page.m_page << ",";
    if (page.m_type) o << "type=" << std::hex << page.m_type << std::dec << ",";
    for (int i = 0; i < 4; i++) {
      if (page.m_values[i])
        o << "f" << i << "=" << page.m_values[i] << ",";
    }
    if (page.m_error.length()) o << page.m_error << ",";
    return o;
  }
  //! the identificator
  int m_id;
  //! the type
  int m_type;
  //! the page number
  int m_page;
  //! some values ( 0, -1, 0, small number )
  int m_values[4];
  /** the errors */
  std::string m_error;
};

////////////////////////////////////////
//! Internal: the font of MSWParser
struct Font {
  //! the constructor
  Font(): m_font(), m_size(0), m_value(0), m_default(true), m_extra("") {
    for (int i = 0; i < 3; i++) m_flags[i] = 0;
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Font const &font) {
    for (int i = 0; i < 3; i++) {
      if (!font.m_flags[i]) continue;
      o << "ft" << i << "=";
      o << std::hex << font.m_flags[i] << std::dec << ",";
    }
    if (font.m_size && font.m_size != font.m_font.size())
      o << "#size2=" << font.m_size << ",";
    if (font.m_value) o << "id?=" << font.m_value << ",";
    if (font.m_extra.length())
      o << font.m_extra << ",";
    return o;
  }

  //! the font
  MWAWStruct::Font m_font;
  //! a second size
  int m_size;
  //! a unknown value
  int m_value;
  //! some unknown flag
  int m_flags[3];
  //! true if is default
  bool m_default;
  //! extra data
  std::string m_extra;
};

/** Internal: class to store the paragraph properties */
struct Paragraph {
  //! Constructor
  Paragraph() : m_font(), m_font2(), m_section(),
    m_justify (DMWAW_PARAGRAPH_JUSTIFICATION_LEFT),
    m_interline(0), m_tabs(), m_error("") {
    for(int c = 0; c < 2; c++) {
      m_margins[c] = 0.0;
      m_spacings[c] = 0;
    }
    m_margins[2] = 0.0;
    for(int i = 0; i < 5; i++) m_borders[i] = false;
    m_section.m_default = true;
  }
  //! return true if a paragraph has border
  bool hasBorders() const {
    for(int i = 0; i < 4; i++) if (m_borders[i]) return true;
    return false;
  }
  //! try to read a data
  bool read(MWAWInputStreamPtr &input, long endPos) {
    long pos = input->tell();
    if (m_section.read(input,endPos)) {
      m_section.m_default=false;
      return true;
    }

    input->seek(pos, WPX_SEEK_SET);
    long dSz = endPos-pos;
    if (dSz < 1) return false;
    libmwaw::DebugStream f;
    int c = input->readULong(1), val;
    switch(c) {
    case 0x5:
      if (dSz < 2) return false;
      val = input->readLong(1);
      switch (val) {
      case 0:
        return true;
      case 1:
        m_justify = DMWAW_PARAGRAPH_JUSTIFICATION_CENTER;
        return true;
      case 2:
        m_justify = DMWAW_PARAGRAPH_JUSTIFICATION_RIGHT;
        return true;
      case 3:
        m_justify = DMWAW_PARAGRAPH_JUSTIFICATION_FULL;
        return true;
      default:
        MWAW_DEBUG_MSG(("MSWTextInternal::Paragraph::Read: can not read align\n"));
        f << "#align=" << val << ",";
        break;
      }
      break;
    case 0x6: // a small number : always 1 ?
    case 0x7:
    case 0x8:
    case 0x9:
      if (dSz < 2) return false;
      val = input->readLong(1);
      switch(c) {
      case 7:
        f << "keeplineTogether";
        break;
      case 8:
        f << "keepwithnext";
        break;
      case 9:
        f << "pagebreakbefore";
        break;
      default:
        f << "f" << std::hex << c << std::dec;
        break;
      }
      if (val==0) f << "*";
      else if (val != 1) f << "=" << val;
      f << ",";
      break;
    case 0xf: { // tabs
      int sz = input->readULong(1);
      if (sz<2 || 2+sz > dSz) {
        MWAW_DEBUG_MSG(("MSWTextInternal::Paragraph::Read: can not read tab\n"));
        return false;
      }
      int N0 = input->readULong(1);
      if (2*N0 > sz) {
        MWAW_DEBUG_MSG(("MSWTextInternal::Paragraph::Read: num tab0 seems odd\n"));
        return false;
      }
      if (N0) { // CHECKME: an increasing list, but what is it ? Often N0=0 or N ?
        f << "tabs0?=[";
        for (int i = 0; i < N0; i++) {
          f << input->readLong(2)/1440. << ",";
        }
        f << "],";
      }
      int N = input->readULong(1);
      if (N*3+2*N0+2 != sz ) {
        MWAW_DEBUG_MSG(("MSWTextInternal::Paragraph::Read: num tab seems odd\n"));
        f << "#";
        m_error += f.str();
        return false;
      }
      std::vector<float> tabs;
      tabs.resize(N);
      for (int i = 0; i < N; i++) tabs[i] = input->readLong(2)/1440.;
      for (int i = 0; i < N; i++) {
        DMWAWTabStop tab;
        tab.m_position = tabs[i];
        int val = input->readULong(1);
        switch(val>>5) {
        case 0:
          break;
        case 1:
          tab.m_alignment = CENTER;
          break;
        case 2:
          tab.m_alignment = RIGHT;
          break;
        case 3:
          tab.m_alignment = DECIMAL;
          break;
        case 4:
          tab.m_alignment = BAR;
          break;
        default:
          f << "#tabAlign=" << int(val>>5) << ",";
          break;
        }
        switch((val>>2)&3) {
        case 1:
          tab.m_leaderCharacter = '.';
          break;
        case 2:
          tab.m_leaderCharacter = '-';
          break;
        case 3:
          tab.m_leaderCharacter = '_';
          break;
        case 0:
        default:
          break;
        }
        if (val & 0x13)
          f << "#tabsFlags=" << std::hex << (val & 0x13) << ",";
        m_tabs.push_back(tab);
      }
      break;
    }
    case 0x10: // right
    case 0x11: // left
    case 0x13: // first left
      if (dSz < 3) return false;
      val = input->readLong(2);
      if (c == 0x13) m_margins[0] = val/1440.;
      else if (c == 0x11) m_margins[1] = val/1440.;
      else m_margins[2] = val/1440.;
      return true;
    case 0x14: // alignement : 240 normal, 480 : double, ..
    case 0x15: // spacing before DWIPP
    case 0x16: // spacing after DWIPP
      if (dSz < 3) return false;
      val = input->readLong(2);
      if (c == 0x14) m_interline = val/20;
      else if (c == 0x15) m_spacings[0] = val/20;
      else if (c == 0x16) m_spacings[1] = val/20;
      return true;
    case 0x1e:
    case 0x1f:
    case 0x20:
    case 0x21:
    case 0x22:
      if (dSz < 3) return false;
      val = input->readULong(2);
      m_borders[c-0x1e] = true;
      switch(c) {
      case 0x1e:
        f << "border[top]";
        break;
      case 0x1f:
        f << "border[left]";
        break;
      case 0x20:
        f << "border[bottom]";
        break;
      case 0x21:
        f << "border[right]";
        break;
      default:
      case 0x22:
        f << "border[middle]";
        break;
      }
      if (val & 0x7E00) f << ":textSep=" << int((val& 0x8E00)>>9) << "pt";
      if (val & 0x8000) f << "*";
      if (val & 0x1FF) f << ":fl=" << std::hex << (val & 0x1FF) << std::dec << ",";
      break;
    default:
      return false;
    }
    m_error += f.str();
    return true;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Paragraph const &ind) {
    if (ind.m_justify) {
      o << "Just=";
      switch(ind.m_justify) {
      case DMWAW_PARAGRAPH_JUSTIFICATION_LEFT:
        o << "left";
        break;
      case DMWAW_PARAGRAPH_JUSTIFICATION_CENTER:
        o << "centered";
        break;
      case DMWAW_PARAGRAPH_JUSTIFICATION_RIGHT:
        o << "right";
        break;
      case DMWAW_PARAGRAPH_JUSTIFICATION_FULL:
        o << "full";
        break;
      default:
        o << "#just=" << ind.m_justify << ", ";
        break;
      }
      o << ", ";
    }
    if (ind.m_margins[0]) o << "firstLPos=" << ind.m_margins[0] << ", ";
    if (ind.m_margins[1]) o << "leftPos=" << ind.m_margins[1] << ", ";
    if (ind.m_margins[2]) o << "rightPos=" << ind.m_margins[2] << ", ";
    if (ind.m_spacings[0]) o << "beforeSpace=" << ind.m_spacings[0] << "pt, ";
    if (ind.m_spacings[1]) o << "afterSpace=" << ind.m_spacings[1] << "pt, ";
    if (ind.m_interline) {
      if (ind.m_interline > 0.0)
        o << "interline=" << ind.m_interline << "pt,";
      else
        o << "interline=" << -ind.m_interline << "pt[exactly],";
    }
    libmwaw::internal::printTabs(o, ind.m_tabs);
    if (!ind.m_section.m_default) o << ind.m_section << ",";
    if (ind.m_error.length()) o << "," << ind.m_error;
    return o;
  }

  //! operator<<
  void print(std::ostream &o, MWAWTools::ConvertissorPtr m_convertissor) const {
    if (!m_font2.m_default)
      o << "font=[" << m_convertissor->getFontDebugString(m_font2.m_font) << m_font2 << "],";
    else if (!m_font.m_default)
      o << "font=[" << m_convertissor->getFontDebugString(m_font2.m_font) << m_font2 << "],";
    o << *this;
  }

  //! the font (simplified)
  Font m_font, m_font2 /** font ( not simplified )*/;
  //! the section
  Section m_section;

  /** the margins in inches
   *
   * 0: first line left, 1: left, 2: right (from right)
   */
  float m_margins[3];
  //! paragraph justification : DMWAW_PARAGRAPH_JUSTIFICATION*
  int m_justify;
  /** interline (in point)*/
  float m_interline;
  /** the spacings ( 0: before, 1: after ) in point*/
  int m_spacings[2];

  //! the border top, left, bottom, right, middle
  bool m_borders[5];
  //! the tabulations
  std::vector<DMWAWTabStop> m_tabs;

  /** the errors */
  std::string m_error;
};

////////////////////////////////////////
//! Internal: the footnote
struct Footnote {
  //! constructor
  Footnote() : m_pos(), m_id(-1), m_value(0), m_error("") { }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Footnote const &note) {
    if (note.m_id >= 0) o << "F" << note.m_id << ":";
    else o << "F_:";
    if (note.m_pos.valid())
      o << std::hex << note.m_pos.begin() << "-" << note.m_pos.end() << std::dec << ",";
    if (note.m_value) o << "f0=" << note.m_value << ",";
    if (note.m_error.length()) o << note.m_error << ",";
    return o;
  }
  //! the footnote data
  MWAWEntry m_pos;
  //! the id
  int m_id;
  //! a value ( 1, 4)
  int m_value;
  /** the errors */
  std::string m_error;
};

////////////////////////////////////////
//! Internal: the field of MSWParser
struct Field {
  //! constructor
  Field() : m_text(""), m_id(-1), m_error("") { }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Field const &field) {
    o << field.m_text;
    if (field.m_id >= 0) o << "[" << field.m_id << "]";
    if (field.m_error.length()) o << "," << field.m_error << ",";
    return o;
  }
  //! the text
  std::string m_text;
  //! the id
  int m_id;
  /** the errors */
  std::string m_error;
};

////////////////////////////////////////
//! Internal: the state of a MSWParser
struct State {
  //! constructor
  State() : m_version(-1), m_bot(0x100), m_defaultFont(2,12), m_font(m_defaultFont), m_textposList(), m_plcMap(), m_filePlcMap(),
    m_styleFontMap(), m_styleParagraphMap(),m_textstructParagraphList(), m_fontList(), m_paragraphList(),
    m_lineList(), m_sectionList(), m_pageList(),
    m_fieldList(), m_footnoteList(), m_cols(1), m_actPage(0), m_numPages(-1) {
    for (int i = 0; i < 3; i++) m_textLength[i] = 0;
  }
  //! returns the total text size
  long getTotalTextSize() const {
    long res=0;
    for (int i = 0; i < 3; i++) res+=m_textLength[i];
    return res;
  }
  //! returns the file position corresponding to a text entry
  long getFilePos(long textPos) const {
    if (!m_textposList.size() || textPos < m_textposList[0].m_pos)
      return m_bot+textPos;
    int minVal = 0, maxVal = m_textposList.size()-1;
    while (minVal != maxVal) {
      int mid = (minVal+1+maxVal)/2;
      if (m_textposList[mid].m_pos == textPos)
        return m_textposList[mid].begin();
      if (m_textposList[mid].m_pos > textPos)
        maxVal = mid-1;
      else
        minVal = mid;
    }
    return m_textposList[minVal].begin() + (textPos-m_textposList[minVal].m_pos);
  }

  //! the file version
  int m_version;

  //! the default text begin
  long m_bot;

  //! the text length (main, footnote, header+footer)
  long m_textLength[3];

  //! the default font ( NewYork 12pt)
  MWAWStruct::Font m_defaultFont;

  //! the actual font
  MWAWStruct::Font m_font;

  //! the text positions
  std::vector<TextEntry> m_textposList;

  //! the text correspondance zone ( textpos, plc )
  std::multimap<long, PLC> m_plcMap;

  //! the text correspondance zone ( filepos, plc )
  std::multimap<long, PLC> m_filePlcMap;

  //! the list of fonts in style
  std::map<int, Font> m_styleFontMap;

  //! the list of paragraph in style
  std::map<int, Paragraph> m_styleParagraphMap;

  //! the list of paragraph in textstruct
  std::vector<Paragraph> m_textstructParagraphList;

  //! the list of fonts
  std::vector<Font> m_fontList;

  //! the list of paragraph
  std::vector<Paragraph> m_paragraphList;

  //! the list of lines
  std::vector<Line> m_lineList;

  //! the list of section
  std::vector<Section> m_sectionList;

  //! the list of pages
  std::vector<Page> m_pageList;

  //! the list of fields
  std::vector<Field> m_fieldList;

  //! the list of footnotes
  std::vector<Footnote> m_footnoteList;

  /** the actual number of columns */
  int m_cols;
  int m_actPage/** the actual page*/, m_numPages /** the number of page of the final document */;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MSWText::MSWText
(MWAWInputStreamPtr ip, MSWParser &parser, MWAWTools::ConvertissorPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert), m_state(new MSWTextInternal::State),
  m_mainParser(&parser), m_asciiFile(parser.ascii())
{
}

MSWText::~MSWText()
{ }

int MSWText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_mainParser->version();
  return m_state->m_version;
}

int MSWText::numPages() const
{
  m_state->m_numPages = m_state->m_pageList.size();
  return m_state->m_numPages;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
// try to find the different zone
////////////////////////////////////////////////////////////
bool MSWText::createZones(long bot, long (&textLength)[3])
{
  m_state->m_bot = bot;
  for (int i = 0; i < 3; i++)
    m_state->m_textLength[i]=textLength[i];

  std::multimap<std::string, MSWEntry> &entryMap
    = m_mainParser->m_entryMap;
  std::multimap<std::string, MSWEntry>::iterator it;
  // the fonts
  it = entryMap.find("FontIds");
  if (it != entryMap.end()) {
    std::vector<long> list;
    readLongZone(it->second, 2, list);
  }
  it = entryMap.find("FontNames");
  if (it != entryMap.end())
    readFontNames(it->second);
  // the styles
  it = entryMap.find("Styles");
  long prevDeb = 0;
  while (it != entryMap.end()) {
    if (!it->second.hasType("Styles")) break;
    MSWEntry &entry=it++->second;
#ifndef DEBUG
    // first entry is often bad or share the same data than the second
    if (entry.m_id == 0)
      continue;
#endif
    if (entry.begin() == prevDeb) continue;
    prevDeb = entry.begin();
    readStyles(entry);
  }
  // read the text structure
  it = entryMap.find("TextStruct");
  if (it != entryMap.end())
    readTextStruct(it->second);

  //! the break position
  it = entryMap.find("PageBreak");
  if (it != entryMap.end())
    readPageBreak(it->second);
  it = entryMap.find("LineInfo");
  if (it != entryMap.end())
    readLineInfo(it->second);
  it = entryMap.find("Section");
  if (it != entryMap.end())
    readSection(it->second);

  //! read the header footer limit
  it = entryMap.find("HeaderFooter");
  std::vector<long> hfLimits;
  if (it != entryMap.end()) { // list of header/footer size
    readLongZone(it->second, 4, hfLimits);
    int N = hfLimits.size();
    if (N) {
      if (version() <= 3) {
        // we must update the different size
        m_state->m_textLength[0] -= (hfLimits[N-1]+1);
        m_state->m_textLength[2] = (hfLimits[N-1]+1);
      }
    }
  }

  //! read the note
  std::vector<long> fieldPos;
  it = entryMap.find("FieldPos");
  if (it != entryMap.end()) { // a list of text pos ( or a size from ? )
    readLongZone(it->second, 4, fieldPos);
  }
  it = entryMap.find("FieldName");
  if (it != entryMap.end())
    readFields(it->second, fieldPos);

  //! read the footenote
  std::vector<long> footnoteDef;
  it = entryMap.find("FootnoteDef");
  if (it != entryMap.end()) { // list of pos in footnote data
    readLongZone(it->second, 4, footnoteDef);
  }
  it = entryMap.find("FootnotePos");
  if (it != entryMap.end()) { // a list of text pos
    readFootnotesPos(it->second, footnoteDef);
  }
  /* CHECKME: this zone seems presents only when FootnoteDef and FootnotePos,
     but what does it means ?
   */
  it = entryMap.find("FootnoteData");
  if (it != entryMap.end()) { // a list of text pos
    readFootnotesData(it->second);
  }
  // we can now update the header/footer limits
  long debHeader = m_state->m_textLength[0]+m_state->m_textLength[1];
  MSWTextInternal::PLC plc(MSWTextInternal::PLC::HeaderFooter);
  for (int i = 0; i < int(hfLimits.size()); i++) {
    plc.m_id = i;
    m_state->m_plcMap.insert(std::multimap<long,MSWTextInternal::PLC>::value_type
                             (hfLimits[i]+debHeader, plc));
  }

  it = entryMap.find("ParagList");
  if (it != entryMap.end())
    readPLCList(it->second);
  it = entryMap.find("CharList");
  if (it != entryMap.end())
    readPLCList(it->second);

  updateTextEntryStyle();

  return true;
}

////////////////////////////////////////////////////////////
// finds the style which must be used for each style
////////////////////////////////////////////////////////////
void MSWText::updateTextEntryStyle()
{
  int N = m_state->m_textposList.size();
  if (N == 0) return;
  std::vector<MSWTextInternal::TextEntry *> list;
  list.resize(N);
  for (int i = 0; i < N; i++) list[i] = & m_state->m_textposList[i];
  MSWTextInternal::TextEntry::CompareFilePos compare;
  std::sort(list.begin(), list.end(), compare);

  std::multimap<long, MSWTextInternal::PLC>::iterator it
    = m_state->m_filePlcMap.begin();
  int actFont = -1, actParag = -1;
  for (int i = 0; i < N; i++) {
    MSWTextInternal::TextEntry &entry = *list[i];
    long pos = entry.begin();
    while (it != m_state->m_filePlcMap.end()) {
      if (it->first > pos) break;
      MSWTextInternal::PLC &plc = it++->second;
      switch(plc.m_type) {
      case MSWTextInternal::PLC::Font:
        actFont = plc.m_id;
        break;
      case MSWTextInternal::PLC::Paragraph:
        actParag = plc.m_id;
        break;
      default:
        break;
      }
    }
    entry.m_fontId = actFont;
    entry.m_paragraphId = actParag;
  }
}

////////////////////////////////////////////////////////////
// read the text structure
////////////////////////////////////////////////////////////
bool MSWText::readTextStruct(MSWEntry &entry)
{
  if (entry.length() < 19) {
    MWAW_DEBUG_MSG(("MSWText::readTextStruct: the zone seems to short\n"));
    return false;
  }
  long pos = entry.begin();
  m_input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  int type = m_input->readLong(1);
  if (type != 1 && type != 2) {
    MWAW_DEBUG_MSG(("MSWText::readTextStruct: find odd type %d\n", type));
    return false;
  }
  entry.setParsed(true);
  int num = 0;
  while (type == 1) {
    /* probably a paragraph definition. Fixme: create a function */
    int length = m_input->readULong(2);
    long endPos = pos+3+length;
    if (endPos> entry.end()) {
      ascii().addPos(pos);
      ascii().addNote("TextStruct[paragraph]#");
      MWAW_DEBUG_MSG(("MSWText::readTextStruct: zone(paragraph) is too big\n"));
      return false;
    }
    f.str("");
    f << "ParagPLC:tP" << num++<< "]:";
    MSWTextInternal::Paragraph para;
    m_input->seek(-2,WPX_SEEK_CUR);
    if (readParagraph(para) && long(m_input->tell()) <= endPos) {
#ifdef DEBUG_WITH_FILES
      para.print(f, m_convertissor);
#endif
    } else {
      para = MSWTextInternal::Paragraph();
      f << "#";
    }
    m_state->m_textstructParagraphList.push_back(para);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(endPos, WPX_SEEK_SET);

    pos = m_input->tell();
    type = m_input->readULong(1);
    if (type == 2) break;
    if (type != 1) {
      MWAW_DEBUG_MSG(("MSWText::readTextStruct: find odd type %d\n", type));
      return false;
    }
  }

  f.str("");
  f << "TextStruct-pos:";
  int sz = m_input->readULong(2);
  long endPos = pos+3+sz;
  if (endPos > entry.end() || (sz%12) != 4) {
    f << "#";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    MWAW_DEBUG_MSG(("MSWText::readTextStruct: can not read the position zone\n"));
    return false;
  }
  int N=sz/12;
  long textLength=m_state->getTotalTextSize();
  std::vector<long> textPos; // checkme
  textPos.resize(N+1);
  f << "pos=[" << std::hex;
  for (int i = 0; i <= N; i++) {
    textPos[i] = m_input->readULong(4);
    if (i && textPos[i] <= textPos[i-1]) {
      MWAW_DEBUG_MSG(("MSWText::readTextStruct: find backward text pos\n"));
      f << "#" << textPos[i] << ",";
      textPos[i]=textPos[i-1];
    } else {
      if (i != N && textPos[i] > textLength) {
        MWAW_DEBUG_MSG(("MSWText::readTextStruct: find a text position which is too big\n"));
        f << "#";
      }
      f << textPos[i] << ",";
    }
  }
  f << std::dec << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  MSWTextInternal::PLC plc(MSWTextInternal::PLC::TextPosition);

  for (int i = 0; i < N; i++) {
    pos = m_input->tell();
    MSWTextInternal::TextEntry tEntry;
    f.str("");
    f<< "TextStruct-pos" << i << ":";
    tEntry.m_pos = textPos[i];
    tEntry.m_type = m_input->readULong(1);
    tEntry.m_id = m_input->readULong(1);
    long ptr = m_input->readULong(4);
    tEntry.setBegin(ptr);
    tEntry.setLength(textPos[i+1]-textPos[i]);
    tEntry.m_function = m_input->readULong(1);
    tEntry.m_value  = m_input->readULong(1);
    m_state->m_textposList.push_back(tEntry);
    if (!m_mainParser->isFilePos(ptr)) {
      MWAW_DEBUG_MSG(("MSWText::readTextStruct: find a bad file position \n"));
      f << "#";
    } else {
      plc.m_id = i;
      m_state->m_plcMap.insert(std::multimap<long,MSWTextInternal::PLC>::value_type
                               (textPos[i],plc));
    }
    f << tEntry;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  pos = m_input->tell();
  if (pos != entry.end()) {
    ascii().addPos(pos);
    ascii().addNote("TextStruct-pos#");
  }
  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the font name
////////////////////////////////////////////////////////////
bool MSWText::readFontNames(MSWEntry &entry)
{
  if (entry.length() < 2) {
    MWAW_DEBUG_MSG(("MSWText::readFontNames: the zone seems to short\n"));
    return false;
  }

  long pos = entry.begin();
  m_input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  int N = m_input->readULong(2);
  if (N*5+2 > entry.length()) {
    MWAW_DEBUG_MSG(("MSWText::readFontNames: the number of fonts seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  f << "FontNames:" << N;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (int i = 0; i < N; i++) {
    pos = m_input->tell();
    if (pos+5 > entry.end()) {
      m_input->seek(pos, WPX_SEEK_SET);
      MWAW_DEBUG_MSG(("MSWText::readFontNames: the fonts %d seems bad\n", i));
      break;
    }
    f.str("");
    f << "FontNames-" << i << ":";
    int val = m_input->readLong(2);
    if (val) f << "f0=" << val << ",";
    int fId = m_input->readULong(2);
    f << "fId=" << fId << ",";
    int fSz = m_input->readULong(1);
    if (pos +5 > entry.end()) {
      m_input->seek(pos, WPX_SEEK_SET);
      MWAW_DEBUG_MSG(("MSWText::readFontNames: the fonts name %d seems bad\n", i));
      break;
    }
    std::string name("");
    for (int j = 0; j < fSz; j++)
      name += char(m_input->readLong(1));
    if (name.length())
      m_convertissor->setFontCorrespondance(fId, name);
    f << name;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  pos = m_input->tell();
  if (pos != entry.end()) {
    ascii().addPos(pos);
    ascii().addNote("FontNames#");
  }

  return true;
}

////////////////////////////////////////////////////////////
// read the line info zone
////////////////////////////////////////////////////////////
bool MSWText::readLineInfo(MSWEntry &entry)
{
  if (entry.length() < 4 || (entry.length()%10) != 4) {
    MWAW_DEBUG_MSG(("MSWText::readLineInfo: the zone size seems odd\n"));
    return false;
  }
  long pos = entry.begin();
  entry.setParsed(true);
  m_input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "LineInfo:";
  int N=entry.length()/10;

  std::vector<long> textPositions;
  f << "[";
  for (int i = 0; i <= N; i++) {
    long textPos = m_input->readULong(4);
    textPositions.push_back(textPos);
    f << std::hex << textPos << std::dec << ",";
  }
  f << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  MSWTextInternal::PLC plc(MSWTextInternal::PLC::Line);
  for (int i = 0; i < N; i++) {
    pos = m_input->tell();
    f.str("");
    f << "LineInfo-" << i << ":" << std::hex << textPositions[i] << std::dec << ",";
    MSWTextInternal::Line line;
    line.m_id = i;
    line.m_type = m_input->readULong(1); // 0, 20, 40, 60
    line.m_value = m_input->readLong(1); // f0: -1 up to 9
    for (int j = 0; j < 2; j++) { // fl0: 0 to 3b, fl1 always a multiple of 4?
      line.m_flags[j] = m_input->readULong(1);
    }
    line.m_height = m_input->readLong(2);
    f << line;
    m_state->m_lineList.push_back(line);

    if (textPositions[i] > m_state->m_textLength[0]) {
      MWAW_DEBUG_MSG(("MSWText::readLineInfo: text positions is bad...\n"));
      f << "#";
    } else {
      plc.m_id=i;
      m_state->m_plcMap.insert(std::multimap<long,MSWTextInternal::PLC>::value_type
                               (textPositions[i],plc));
    }

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;

}

////////////////////////////////////////////////////////////
// read the page break
////////////////////////////////////////////////////////////
bool MSWText::readPageBreak(MSWEntry &entry)
{
  if (entry.length() < 18 || (entry.length()%14) != 4) {
    MWAW_DEBUG_MSG(("MSWText::readPageBreak: the zone size seems odd\n"));
    return false;
  }
  long pos = entry.begin();
  entry.setParsed(true);
  m_input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "PageBreak:";
  int N=entry.length()/14;
  std::vector<long> textPos; // checkme
  textPos.resize(N+1);
  for (int i = 0; i <= N; i++) textPos[i] = m_input->readULong(4);
  MSWTextInternal::PLC plc(MSWTextInternal::PLC::Page);
  for (int i = 0; i < N; i++) {
    MSWTextInternal::Page page;
    page.m_id = i;
    page.m_type = m_input->readULong(1);
    page.m_values[0] = m_input->readLong(1); // always 0?
    for (int j = 1; j < 3; j++) // always -1, 0
      page.m_values[j] = m_input->readLong(2);
    page.m_page = m_input->readLong(2);
    page.m_values[3] = m_input->readLong(2);
    m_state->m_pageList.push_back(page);

    if (textPos[i] > m_state->m_textLength[0]) {
      MWAW_DEBUG_MSG(("MSWText::readPageBreak: text positions is bad...\n"));
      f << "#";
    } else {
      plc.m_id = i;
      m_state->m_plcMap.insert(std::multimap<long,MSWTextInternal::PLC>::value_type
                               (textPos[i],plc));
    }
    f << std::hex << "[pos?=" << textPos[i] << std::dec << "," << page << "],";
  }
  f << "end=" << std::hex << textPos[N] << std::dec << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the section zone
////////////////////////////////////////////////////////////
bool MSWText::readSection(MSWEntry &entry)
{
  if (entry.length() < 14 || (entry.length()%10) != 4) {
    MWAW_DEBUG_MSG(("MSWText::readSection: the zone size seems odd\n"));
    return false;
  }
  long pos = entry.begin();
  entry.setParsed(true);
  m_input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Section:";
  int N=entry.length()/10;
  std::vector<long> textPos; // checkme
  textPos.resize(N+1);
  for (int i = 0; i <= N; i++) textPos[i] = m_input->readULong(4);
  MSWTextInternal::PLC plc(MSWTextInternal::PLC::Section);
  for (int i = 0; i < N; i++) {
    MSWTextInternal::Section sec;
    sec.m_type = m_input->readULong(1);
    sec.m_flag = m_input->readULong(1);
    sec.m_id = i;
    unsigned long filePos = m_input->readULong(4);
    if (textPos[i] > m_state->m_textLength[0]) {
      MWAW_DEBUG_MSG(("MSWText::readSection: text positions is bad...\n"));
      f << "#";
    } else {
      plc.m_id = i;
      m_state->m_plcMap.insert(std::multimap<long,MSWTextInternal::PLC>::value_type
                               (textPos[i],plc));
    }
    f << std::hex << "pos?=" << textPos[i] << ":[" << sec << ",";
    if (filePos != 0xFFFFFFFFL) {
      f << "pos=" << std::hex << filePos << std::dec << ",";
      long actPos = m_input->tell();
      readSection(sec,filePos);
      m_input->seek(actPos, WPX_SEEK_SET);
    }
    f << "],";

    m_state->m_sectionList.push_back(sec);
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the char/parag plc list
////////////////////////////////////////////////////////////
bool MSWText::readPLCList(MSWEntry &entry)
{
  if (entry.length() < 10 || (entry.length()%6) != 4) {
    MWAW_DEBUG_MSG(("MSWText::readPLCList: the zone size seems odd\n"));
    return false;
  }
  long pos = entry.begin();
  entry.setParsed(true);
  m_input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << entry.type() << ":";
  int N=entry.length()/6;
  std::vector<long> textPos; // limit of the text in the file
  textPos.resize(N+1);
  for (int i = 0; i <= N; i++) textPos[i] = m_input->readULong(4);
  for (int i = 0; i < N; i++) {
    if (!m_mainParser->isFilePos(textPos[i])) f << "#";

    long defPos = m_input->readULong(2);
    f << std::hex << "[filePos?=" << textPos[i] << ",dPos=" << defPos << std::dec << ",";
    f << "],";
    int expectedSize = (version() <= 3) ? 0x80 : 0x200;

    MSWEntry plc;
    plc.setType(entry.m_id ? "ParagPLC" : "CharPLC");
    plc.m_id = i;
    plc.setBegin(defPos*expectedSize);
    plc.setLength(expectedSize);
    if (!m_mainParser->isFilePos(plc.end())) {
      f << "#PLC,";
      MWAW_DEBUG_MSG(("MSWText::readPLCList: plc def is outside the file\n"));
    } else {
      long actPos = m_input->tell();
      readPLC(plc, entry.m_id);
      m_input->seek(actPos, WPX_SEEK_SET);
    }
  }
  f << std::hex << "end?=" << textPos[N] << ",";

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the char/parag plc
////////////////////////////////////////////////////////////
bool MSWText::readPLC(MSWEntry &entry, int type)
{
  int expectedSize = (version() <= 3) ? 0x80 : 0x200;
  int posFactor = (version() <= 3) ? 1 : 2;
  if (entry.length() != expectedSize) {
    MWAW_DEBUG_MSG(("MSWText::readPLC: the zone size seems odd\n"));
    return false;
  }
  m_input->seek(entry.end()-1, WPX_SEEK_SET);
  int N=m_input->readULong(1);
  if (5*(N+1) > entry.length()) {
    MWAW_DEBUG_MSG(("MSWText::readPLC: the number of plc seems odd\n"));
    return false;
  }

  long pos = entry.begin();
  entry.setParsed(true);
  libmwaw::DebugStream f;
  f << "Entries("<< entry.type() << ")[" << entry.m_id << "]:N=" << N << ",";

  m_input->seek(pos, WPX_SEEK_SET);
  std::vector<long> filePos;
  filePos.resize(N+1);
  for (int i = 0; i <= N; i++)
    filePos[i] = m_input->readULong(4);
  std::map<int, int> mapPosId;
  std::vector<int> decal;
  decal.resize(N);
  int numData = type == 0 ? m_state->m_fontList.size() :
                m_state->m_paragraphList.size();
  MSWTextInternal::PLC::Type plcType =
    type == 0 ? MSWTextInternal::PLC::Font : MSWTextInternal::PLC::Paragraph;

  for (int i = 0; i < N; i++) {
    decal[i] = m_input->readULong(1);
    int id = -1;
    if (decal[i]) {
      if (mapPosId.find(decal[i]) != mapPosId.end())
        id = mapPosId.find(decal[i])->second;
      else {
        id = numData++;
        mapPosId[decal[i]] = id;

        long actPos = m_input->tell();
        libmwaw::DebugStream f2;
        f2 << entry.type() << "-";

        long dataPos = entry.begin()+posFactor*decal[i];
        if (type == 0) {
          m_input->seek(dataPos, WPX_SEEK_SET);
          f2 << "F" << id << ":";
          MSWTextInternal::Font font;
          if (!readFont(font, true)) {
            font = MSWTextInternal::Font();
            f2 << "#";
          } else
            f2 << m_convertissor->getFontDebugString(font.m_font) << font << ",";
          m_state->m_fontList.push_back(font);
        } else {
          MSWTextInternal::Paragraph para;
          f2 << "P" << id << ":";

          m_input->seek(dataPos, WPX_SEEK_SET);
          int sz = m_input->readLong(1);
          if (sz < 4 || dataPos+2*sz > entry.end()-1) {
            MWAW_DEBUG_MSG(("MSWText::readPLC: can not read plcSz\n"));
            f2 << "#";
          } else {
            int pId = m_input->readLong(1);
            if (m_state->m_styleParagraphMap.find(pId)==m_state->m_styleParagraphMap.end()) {
              MWAW_DEBUG_MSG(("MSWText::readPLC: can not find parent paragraph\n"));
              f2 << "#";
            } else
              para = m_state->m_styleParagraphMap.find(pId)->second;
            f2 << "sP" << pId << ",";
            int val = m_input->readLong(1);
            if (val) // some flag ?
              f2 << "g0=" << std::hex << val << std::dec << ",";
            val = m_input->readLong(1);
            if (val) // a small number
              f2 << "g1=" << val << ",";
            for (int j = 2; j < 4; j++) {
              val = m_input->readULong(2);
              if (val) f2 << "g" << j << "=" << std::hex << val << std::dec << ",";
            }
            if (sz > 4) {
              ascii().addDelimiter(dataPos+8,'|');
              m_input->seek(dataPos+8, WPX_SEEK_SET);
              if (readParagraph(para, sz*2+1-8)) {
#ifdef DEBUG_WITH_FILES
                para.print(f2, m_convertissor);
#endif
              } else {
                para = MSWTextInternal::Paragraph();
                f2 << "#";
              }
            }
          }
          m_state->m_paragraphList.push_back(para);
        }
        m_input->seek(actPos, WPX_SEEK_SET);
        ascii().addPos(dataPos);
        ascii().addNote(f2.str().c_str());
      }
    }
    f << std::hex << filePos[i] << std::dec;
    MSWTextInternal::PLC plc(plcType, id);
    m_state->m_filePlcMap.insert
    (std::multimap<long,MSWTextInternal::PLC>::value_type(filePos[i], plc));
    if (id >= 0) {
      if (type==0) f << ":F" << id;
      else f << ":P" << id;
    }
    f << ",";
  }

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the footnotes pos + val
////////////////////////////////////////////////////////////
bool MSWText::readFootnotesPos(MSWEntry &entry, std::vector<long> const &noteDef)
{
  if (entry.length() < 4 || (entry.length()%6) != 4) {
    MWAW_DEBUG_MSG(("MSWText::readFootnotesPos: the zone size seems odd\n"));
    return false;
  }
  libmwaw::DebugStream f;
  int N=entry.length()/6;
  if (N+2 != int(noteDef.size())) {
    MWAW_DEBUG_MSG(("MSWText::readFootnotesPos: the number N seems odd\n"));
    return false;
  }
  if (version() <= 3) {
    // we must update the different size
    m_state->m_textLength[0] -= (noteDef[N+1]+1);
    m_state->m_textLength[1] = (noteDef[N+1]+1);
  }
  long pos = entry.begin();
  entry.setParsed(true);
  m_input->seek(pos, WPX_SEEK_SET);
  f << "FootnotePos:";

  std::vector<long> textPos;
  textPos.resize(N+1);
  for (int i = 0; i <= N; i++)
    textPos[i]= m_input->readULong(4);
  long debFootnote = m_state->m_textLength[0];
  MSWTextInternal::PLC plc(MSWTextInternal::PLC::Footnote);
  MSWTextInternal::PLC defPlc(MSWTextInternal::PLC::FootnoteDef);
  for (int i = 0; i < N; i++) {
    MSWTextInternal::Footnote note;
    note.m_id = i;
    note.m_pos.setBegin(debFootnote+noteDef[i]);
    note.m_pos.setEnd(debFootnote+noteDef[i+1]);
    note.m_value = m_input->readLong(2);
    m_state->m_footnoteList.push_back(note);

    if (textPos[i] > m_state->getTotalTextSize()) {
      MWAW_DEBUG_MSG(("MSWText::readFootnotesPos: can not find text position\n"));
      f << "#";
    } else if (noteDef[i+1] > m_state->m_textLength[1]) {
      MWAW_DEBUG_MSG(("MSWText::readFootnotesPos: can not find definition position\n"));
      f << "#";
    } else {
      defPlc.m_id = plc.m_id = i;
      m_state->m_plcMap.insert(std::multimap<long,MSWTextInternal::PLC>::value_type
                               (textPos[i], plc));
      m_state->m_plcMap.insert(std::multimap<long,MSWTextInternal::PLC>::value_type
                               (note.m_pos.begin(), defPlc));
    }
    f << std::hex << textPos[i] << std::dec << ":" << note << ",";
  }
  f << "end=" << std::hex << textPos[N] << std::dec << ",";
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the footnotes pos?
////////////////////////////////////////////////////////////
bool MSWText::readFootnotesData(MSWEntry &entry)
{
  if (entry.length() < 4 || (entry.length()%14) != 4) {
    MWAW_DEBUG_MSG(("MSWText::readFootnotesData: the zone size seems odd\n"));
    return false;
  }
  libmwaw::DebugStream f;
  int N=entry.length()/14;
  long pos = entry.begin();
  entry.setParsed(true);
  m_input->seek(pos, WPX_SEEK_SET);
  f << "FootnoteData[" << N << "/" << m_state->m_footnoteList.size() << "]:";

  std::vector<long> textPos; // checkme
  textPos.resize(N+1);
  for (int i = 0; i <= N; i++)
    textPos[i]= m_input->readULong(4);
  for (int i = 0; i < N; i++) {
    if (textPos[i] > m_state->m_textLength[1]) {
      MWAW_DEBUG_MSG(("MSWText::readFootnotesData: textPositions seems bad\n"));
      f << "#";
    }
    f << "N" << i << "=[";
    if (textPos[i])
      f << "pos=" << std::hex << textPos[i] << std::dec << ",";
    for (int i = 0; i < 5; i++) { // always 0|4000, -1, 0, id, 0 ?
      int val=m_input->readLong(2);
      if (val && i == 0)
        f << std::hex << val << std::dec << ",";
      else if (val)
        f << val << ",";
      else f << "_,";
    }
    f << "],";
  }
  f << "end=" << std::hex << textPos[N] << std::dec << ",";
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the note
////////////////////////////////////////////////////////////
bool MSWText::readFields(MSWEntry &entry, std::vector<long> const &fieldPos)
{
  long pos = entry.begin();
  int N = fieldPos.size();
  long textLength = m_state->getTotalTextSize();
  if (N==0) {
    MWAW_DEBUG_MSG(("MSWText::readFields: number of fields is 0\n"));
    return false;
  }
  N--;
  entry.setParsed(true);
  m_input->seek(pos, WPX_SEEK_SET);

  long sz = m_input->readULong(2);
  if (entry.length() != sz) {
    MWAW_DEBUG_MSG(("MSWText::readFields: the zone size seems odd\n"));
    return false;
  }
  libmwaw::DebugStream f, f2;
  f << "FieldName:";
  int const endSize = (version()==5) ? 2 : 1;
  MSWTextInternal::PLC plc(MSWTextInternal::PLC::Field);
  for (int n = 1; n < N; n++) {
    if (m_input->tell() >= entry.end()) {
      MWAW_DEBUG_MSG(("MSWText::readFields: can not find all field\n"));
      break;
    }
    pos = m_input->tell();
    int fSz = m_input->readULong(1);
    if (pos+1+fSz > entry.end()) {
      MWAW_DEBUG_MSG(("MSWText::readFields: can not read a string\n"));
      m_input->seek(pos, WPX_SEEK_SET);
      f << "#";
      break;
    }
    int endSz = fSz < endSize ? 0 : endSize;

    f2.str("");
    std::string text("");
    for (int i = 0; i < fSz-endSz; i++) {
      char c = m_input->readULong(1);
      if (c==0) f2 << '#';
      else text+=c;
    }
    MSWTextInternal::Field field;
    if (!endSz) ;
    else if (version()>=5 && m_input->readULong(1) != 0xc) {
      m_input->seek(-1, WPX_SEEK_CUR);
      for (int i = 0; i < 2; i++) text+=char(m_input->readULong(1));
    } else {
      int id = m_input->readULong(1);
      if (id >= N) {
        if (version()>=5) {
          MWAW_DEBUG_MSG(("MSWText::readFields: find a strange id\n"));
          f2 << "#";
        } else
          text+=char(id);
      } else
        field.m_id = id;
    }
    field.m_text = text;
    field.m_error = f2.str();
    m_state->m_fieldList.push_back(field);

    f << "N" << n << "=" << field << ",";
    if ( fieldPos[n] >= textLength) {
      MWAW_DEBUG_MSG(("MSWText::readFields: text positions is bad...\n"));
      f << "#";
    } else {
      plc.m_id = n-1;
      m_state->m_plcMap.insert(std::multimap<long,MSWTextInternal::PLC>::value_type
                               (fieldPos[n], plc));
    }
  }
  if (long(m_input->tell()) != entry.end())
    ascii().addDelimiter(m_input->tell(), '|');
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the styles
////////////////////////////////////////////////////////////
bool MSWText::readStyles(MSWEntry &entry)
{
  if (entry.length() < 6) {
    MWAW_DEBUG_MSG(("MSWText::readStyles: zone seems to short...\n"));
    return false;
  }
  m_state->m_styleFontMap.clear();
  m_state->m_styleParagraphMap.clear();
  entry.setParsed(true);
  ascii().addPos(entry.end());
  ascii().addNote("_");

  long pos = entry.begin();
  libmwaw::DebugStream f;
  m_input->seek(pos, WPX_SEEK_SET);
  f << entry << ":";
  int N = m_input->readLong(2);
  if (N) f << "N?=" << N;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos = m_input->tell();

  int dataSz = m_input->readULong(2);
  long endPos = pos+dataSz;
  if (dataSz < 2+N || endPos > entry.end()) {
    ascii().addPos(pos);
    ascii().addNote("###Styles(names)");
    MWAW_DEBUG_MSG(("MSWText::readStyles: can not read styles(names)...\n"));
    return false;
  }

  ascii().addPos(pos);

  f.str("");
  f << "Styles(names):";
  int actN=0;
  while (long(m_input->tell()) < endPos) {
    int sz = m_input->readULong(1);
    if (sz == 0) {
      f << "*";
      actN++;
      continue;
    }
    if (sz == 0xFF) {
      f << "_";
      actN++;
      continue;
    }
    pos = m_input->tell();
    if (pos+sz > endPos) {
      MWAW_DEBUG_MSG(("MSWText::readStyles: zone(names) seems to short...\n"));
      f << "#";
      ascii().addNote(f.str().c_str());
      m_input->seek(pos-1, WPX_SEEK_SET);
      break;
    }
    std::string s("");
    for (int i = 0; i < sz; i++) s += char(m_input->readULong(1));
    f << "N" << actN-N << "=" ;
    f << s << ",";
    actN++;
  }
  int N1=actN-N;
  if (N1 < 0) {
    MWAW_DEBUG_MSG(("MSWText::readStyles: zone(names) seems to short: stop...\n"));
    f << "#";
    ascii().addNote(f.str().c_str());
    return false;
  }
  if (long(m_input->tell()) != endPos) {
    ascii().addDelimiter(m_input->tell(),'|');
    m_input->seek(endPos, WPX_SEEK_SET);
  }
  ascii().addNote(f.str().c_str());

  pos = m_input->tell();
  dataSz = m_input->readULong(2);
  endPos = pos+dataSz;
  if (dataSz < N+2 || endPos > entry.end()) {
    if (dataSz >= N+2 && endPos < entry.end()+30) {
      MWAW_DEBUG_MSG(("MSWText::readStyles: must increase font zone...\n"));
      entry.setEnd(endPos+1);
    } else {
      ascii().addPos(pos);
      ascii().addNote("###Styles(font)");
      MWAW_DEBUG_MSG(("MSWText::readStyles: can not read styles(font)...\n"));
      return false;
    }
  }
  f.str("");
  f << "Styles(font):";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i = 0; i < N+N1; i++) {
    pos = m_input->tell();
    if (pos >= endPos)
      break;

    f.str("");
    f << "CharPLC(mF" << i-N << "):";

    MSWTextInternal::Font font;
    if (!readFont(font, false) || long(m_input->tell()) > endPos) {
      m_input->seek(pos, WPX_SEEK_SET);
      int sz = m_input->readULong(1);
      if (sz == 0xFF) f << "_";
      else if (pos+1+sz <= endPos) {
        f << "#";
        m_input->seek(pos+1+sz, WPX_SEEK_SET);
        MWAW_DEBUG_MSG(("MSWText::readStyles: can not read a font, continue\n"));
      } else {
        f << "#";
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());

        MWAW_DEBUG_MSG(("MSWText::readStyles: can not read a font, stop\n"));
        break;
      }
      font = MSWTextInternal::Font();
    } else
      f << "font=[" << m_convertissor->getFontDebugString(font.m_font) << font << "],";

    m_state->m_styleFontMap.insert
    (std::multimap<int,MSWTextInternal::Font>::value_type(i-N,font));

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  m_input->seek(endPos, WPX_SEEK_SET);
  pos = m_input->tell();
  dataSz = m_input->readULong(2);
  endPos = pos+dataSz;

  ascii().addPos(pos);
  f.str("");
  f << "Styles(paragraph):";
  bool szOk = true;
  if (endPos > entry.end()) {
    // sometimes entry.end() seems a little to short :-~
    if (endPos > entry.end()+100) {
      ascii().addNote("###Styles(paragraph)");
      MWAW_DEBUG_MSG(("MSWText::readStyles: can not read styles(paragraph)...\n"));
      return false;
    }
    szOk = false;
    MWAW_DEBUG_MSG(("MSWText::readStyles: styles(paragraph) size seems incoherent...\n"));
    f << "#sz=" << dataSz << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  int val;
  for (int i = 0; i < N+N1; i++) {
    pos = m_input->tell();
    if (pos >= endPos)
      break;

    if (long(m_input->tell()) >= endPos)
      break;

    f.str("");
    f << "ParagPLC(sP" << i-N << "):";
    int sz = m_input->readULong(1);
    MSWTextInternal::Paragraph para;
    if (m_state->m_styleFontMap.find(i-N) != m_state->m_styleFontMap.end())
      para.m_font = m_state->m_styleFontMap.find(i-N)->second;
    if (sz == 0xFF)
      f << "_";
    else if (sz < 7) {
      MWAW_DEBUG_MSG(("MSWText::readStyles: zone(paragraph) seems to short...\n"));
      f << "#";
    } else {
      int id = m_input->readLong(1);
      if (i >= N && id != i-N) {
        MWAW_DEBUG_MSG(("MSWText::readStyles: zone(paragraph) the id seems bad...\n"));
        f << "#id=" << id << ",";
      }
      for (int i = 0; i < 3; i++) { // 0, 0|c,0|1
        val = m_input->readLong(2);
        if (val) f << "g" << i << "=" << val << ",";
      }
      if (sz > 7) {
        if (readParagraph(para, sz-7)) {
#ifdef DEBUG_WITH_FILES
          para.print(f, m_convertissor);
#endif
        } else {
          f << "#";
          para = MSWTextInternal::Paragraph();
        }
      }
    }
    m_state->m_styleParagraphMap.insert
    (std::multimap<int,MSWTextInternal::Paragraph>::value_type(i-N,para));
    if (sz != 0xFF)
      m_input->seek(pos+1+sz, WPX_SEEK_SET);

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  m_input->seek(endPos, WPX_SEEK_SET);

  pos = m_input->tell();
  int N2 = m_input->readULong(2);
  f.str("");
  f << "Styles(IV):";
  if (N2 != N+N1) {
    MWAW_DEBUG_MSG(("MSWText::readStyles: read zone(IV): N seems odd...\n"));
    f << "#N=" << N2 << ",";
  }
  if (pos+(N2+1)*2 > entry.end()) {
    if (N2>40) {
      MWAW_DEBUG_MSG(("MSWText::readStyles: can not read zone(IV)...\n"));
      ascii().addPos(pos);
      ascii().addNote("Styles(IV):#"); // big problem
    }
    f << "#";
  }
  for (int i = 0; i < N2; i++) {
    int v0 = m_input->readLong(1);
    int v1 = m_input->readULong(1);
    if (!v0 && !v1) f << "_,";
    else if (!v1)
      f << v0 << ",";
    else
      f << v0 << ":" << std::hex << v1 << std::dec << ",";
  }

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos = m_input->tell();
  if (pos != entry.end()) {
    ascii().addPos(pos);
    ascii().addNote("_");
  }

  return true;
}

////////////////////////////////////////////////////////////
// try to read a font
////////////////////////////////////////////////////////////
bool MSWText::readFont(MSWTextInternal::Font &font, bool mainZone)
{
  font = MSWTextInternal::Font();

  libmwaw::DebugStream f;

  long pos = m_input->tell();
  int sz = m_input->readULong(1);
  if (sz > 20 || sz == 3) {
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  if (sz == 0) return true;
  font.m_default = false;

  int flag = m_input->readULong(1);
  int flags = 0;
  if (flag&0x80) flags |= DMWAW_BOLD_BIT;
  if (flag&0x40) flags |= DMWAW_ITALICS_BIT;
  if (flag&0x20) flags |= DMWAW_STRIKEOUT_BIT;
  if (flag&0x10) flags |= DMWAW_OUTLINE_BIT;
  if (flag&0x8) flags |= DMWAW_SHADOW_BIT;
  if (flag&0x4) flags |= DMWAW_SMALL_CAPS_BIT;
  if (flag&0x2) flags |= DMWAW_ALL_CAPS_BIT;
  if (flag&0x1) flags |= DMWAW_HIDDEN_BIT;

  int what = 0;
  if (sz >= 2) what = m_input->readULong(1);

  /*  01: horizontal decal, 2: vertical decal, 4; underline, 08: fSize,  10: set font, 20: font color, 40: ???(maybe reset)
  */
  font.m_font.setId(m_state->m_defaultFont.id());
  if (sz >= 4) {
    int fId = m_input->readULong(2);
    if (fId) {
      if (mainZone && (what & 0x50)==0) f << "#fId,";
      font.m_font.setId(fId);
    } else if (what & 0x10) {
    }
    what &= 0xEF;
  } else if (what & 0x10) {
  }
  font.m_font.setSize(0);
  if (sz >= 5) {
    int fSz = m_input->readULong(1)/2;
    if (fSz) {
      if (mainZone && (what & 0x48)==0) f << "#fSz,";
      font.m_font.setSize(fSz);
    }
    what &= 0xF7;
  } else // reset to default
    font.m_font.setSize(m_state->m_defaultFont.size());

  if (sz >= 6) {
    int decal = m_input->readLong(1); // unit point
    if (decal) {
      if (what & 0x2) {
        if (decal > 0)
          flags |= DMWAW_SUPERSCRIPT100_BIT;
        else
          flags |= DMWAW_SUBSCRIPT100_BIT;
      } else
        f << "#vDecal=" << decal;
    }
    what &= 0xFD;
  }
  if (sz >= 7) {
    int decal = m_input->readLong(1); // unit point > 0 -> expand < 0: condensed
    if (decal) {
      if ((what & 0x1) == 0) f << "#";
      f << "hDecal=" << decal <<",";
    }
    what &= 0xFE;
  }

  if (sz >= 8) {
    int val = m_input->readULong(1);
    if (val & 0xF0) {
      if (what & 0x20) {
        Vec3uc col;
        if (m_mainParser->getColor((val>>4),col)) {
          int colors[3] = {col[0], col[1], col[2]};
          font.m_font.setColor(colors);
        } else
          f << "#fColor=" << (val>>4) << ",";
      } else
        f << "#fColor=" << (val>>4) << ",";
    }
    what &= 0xDF;

    if (val && (what & 0x4)) {
      flags |= DMWAW_UNDERLINE_BIT;
      if (val != 2) f << "#underline=" << (val &0xf) << ",";
      what &= 0xFB;
    } else if (val & 0xf)
      f << "#underline?=" << (val &0xf) << ",";
  }

  font.m_flags[0] =what;
  font.m_font.setFlags(flags);

  bool ok = false;
  if (mainZone && sz >= 10 && sz <= 12) {
    int wh = m_input->readULong(1);
    long pictPos = 0;
    for (int i = 10; i < 13; i++) {
      pictPos <<= 8;
      if (i <= sz) pictPos += m_input->readULong(1);
    }
    long actPos = m_input->tell();
    if (m_mainParser->checkPicturePos(pictPos, wh)) {
      ok = true;
      m_input->seek(actPos, WPX_SEEK_SET);
      f << "pict=" << std::hex << pictPos << std::dec << "[" << wh << "],";
    } else
      m_input->seek(pos+1+8, WPX_SEEK_SET);
  }
  if (!ok && sz >= 9) {
    int wh = m_input->readLong(1);
    switch(wh) {
    case -1:
      ok = true;
      break;
    case 0: // line height ?
      if (sz < 10) break;
      font.m_size=m_input->readULong(1)/2;
      ok = true;
      break;
    default:
      break;
    }
  }
  if (!ok && sz >= 9) {
    m_input->seek(pos+1+8, WPX_SEEK_SET);
    f << "#";
  }
  if (long(m_input->tell()) != pos+1+sz)
    ascii().addDelimiter(m_input->tell(), '|');

  m_input->seek(pos+1+sz, WPX_SEEK_SET);
  font.m_extra = f.str();
  return true;
}

////////////////////////////////////////////////////////////
// try to read a paragraph
////////////////////////////////////////////////////////////
bool MSWText::readParagraph(MSWTextInternal::Paragraph &para, int dataSz)
{
  int sz;
  if (dataSz >= 0)
    sz = dataSz;
  else
    sz = m_input->readULong(2);

  long pos = m_input->tell();
  long endPos = pos+sz;

  if (sz == 0) return true;
  if (!m_mainParser->isFilePos(pos+sz)) return false;

  libmwaw::DebugStream f;
  while (long(m_input->tell()) < endPos) {
    long actPos = m_input->tell();
    /* 5-16: basic paragraph properties
       75-84: basic section properties
       other
     */
    if (para.read(m_input,endPos)) continue;
    m_input->seek(actPos, WPX_SEEK_SET);

    int wh = m_input->readULong(1), val;
    bool done = true;
    switch(wh) {
    case 0:
      done = actPos+1==endPos;
      break;
    case 0x3a:
      f << "f" << std::hex << wh << std::dec << ",";
      break;
    case 0x2: // a small number between 0 and 4
    case 0x18: // in cell ?
    case 0x19: // new table
    case 0x1d: // a small number 1, 6
    case 0x34: // 0 ( one time)
    case 0x45: // a small number between 0 or 1
    case 0x47: // 0 one time
    case 0x49: // 0 ( one time)
    case 0x4c: // 0, 6, -12
    case 0x4d: // a small number between -4 and 14
    case 0x5e: // 0
      if (actPos+2 > endPos) {
        done = false;
        f << "#";
        break;
      }
      val = m_input->readLong(1);
      switch(wh) {
      case 0x18:
        f << "table,";
        break;
      case 0x19:
        f << "textbox?,";
        break;
      default:
        f << "f" << std::hex << wh << std::dec << "=" << val << ",";
      }
      break;
    case 0x3c: // always 0x80 | 0x81 ?
    case 0x3d: // always 0x80 | 0x81 ?
    case 0x3e:
    case 0x3f:
    case 0x40:
    case 0x41:
    case 0x42:
    case 0x43:
    case 0x4a: // in general a small number
      if (actPos+2 > endPos) {
        done = false;
        f << "#";
        break;
      }
      val = m_input->readULong(1);
      f << "f" << std::hex << wh << "=" << val << std::dec << ",";
      break;
    case 0x1a: // a small negative number
    case 0x1b: // a small negative number
    case 0x1c: // dim: related to caps ?
    case 0x23: // alway 0 ?
    case 0x24: // always b4 ?
    case 0x92: // alway 0 ?
    case 0x93: // a dim ?
    case 0x99: // alway 0 ?
      if (actPos+3 > endPos) {
        done = false;
        f << "#";
        break;
      }
      val = m_input->readLong(2);
      f << "f" << std::hex << wh << std::dec << "=" << val << ",";
      break;
    case 0x9f: // two small number
    case 0x44: // two flag?
      if (actPos+3 > endPos) {
        done = false;
        f << "#";
        break;
      }
      f << "f" << std::hex << wh << "=[";
      for (int i = 0; i < 2; i++)
        f << m_input->readULong(1) << ",";
      f << std::dec << "],";
      break;
    case 3: // four small number
      if (actPos+5 > endPos) {
        done = false;
        f << "#";
        break;
      }
      f << "f" << std::hex << wh << std::dec << "=[";
      for (int i = 0; i < 4; i++)
        f << m_input->readLong(1) << ",";
      f << "],";
      break;
    case 0x50: // two small number
      if (actPos+4 > endPos) {
        done = false;
        f << "#";
        break;
      }
      f << "f" << std::hex << wh << std::dec << "=[";
      f << m_input->readLong(1) << ",";
      f << m_input->readLong(2) << ",";
      f << "],";
      break;
    case 0x38:
    case 0x4f: // a small int and a pos?
      if (actPos+4 > endPos) {
        done = false;
        f << "#";
        break;
      }
      f << "f" << std::hex << wh << std::dec << "=[";
      f << m_input->readLong(1) << ",";
      f << std::hex << m_input->readULong(2) << std::dec << "],";
      break;
    case 0x9e:
    case 0xa0: // two small int and a pos?
      if (actPos+5 > endPos) {
        done = false;
        f << "#";
        break;
      }
      f << "f" << std::hex << wh << std::dec << "=[";
      for (int i = 0; i < 2; i++)
        f << m_input->readLong(1) << ",";
      f << std::hex << m_input->readULong(2) << std::dec << "],";
      break;
    case 0x9d: // three small int and a pos?
    case 0xa3: // three small int and a pos?
      if (actPos+6 > endPos) {
        done = false;
        f << "#";
        break;
      }
      f << "f" << std::hex << wh << std::dec << "=[";
      for (int i = 0; i < 3; i++)
        f << m_input->readLong(1) << ",";
      f << std::hex << m_input->readULong(2) << std::dec << "],";
      break;
    case 0x4e:
    case 0x53: { // same as 4e but with size=0xa
      MSWTextInternal::Font font;
      if (!readFont(font, false) || long(m_input->tell()) > endPos) {
        done = false;
        f << "#";
        break;
      }
      if (wh == 0x4e) para.m_font = font;
      else para.m_font2 = font;
      break;
    }
    case 0x5f: { // 4 index
      if (actPos+10 > endPos) {
        done = false;
        f << "#";
        break;
      }
      int sz = m_input->readULong(1);
      if (sz != 8) f << "#sz=" << sz << ",";
      f << "f5f=[";
      for (int i = 0; i < 4; i++) f << m_input->readLong(2) << ",";
      f << "],";
      break;
    }
    case 0x17: {
      int sz = m_input->readULong(1);
      if (!sz || actPos+2+sz > endPos) {
        done = false;
        f << "#";
        break;
      }
      f << "f" << std::hex << wh << "=[";
      for (int i = 0; i < sz; i++) {
        val= m_input->readULong(1);
        if (val) f << val << ",";
        else f << "_,";
      }
      f << std::dec << "],";
      break;
    }
    case 0x94: // checkme space between column divided by 2 (in table) ?
      if (actPos+3 > endPos) {
        done = false;
        f << "#";
        break;
      }
      val = m_input->readLong(2);
      f << "colsSep?=" << 2*val/1440. << ",";
      break;
    case 0x98: { // tabs columns
      int sz = m_input->readULong(2);
      if (!sz || actPos+2+sz > endPos) {
        done = false;
        f << "#";
        break;
      }
      int N = m_input->readULong(1);
      if (1+(N+1)*2 > sz) {
        done = false;
        f << "#";
        break;
      }
      f << "table[N=" << N << ",posCol=[";
      for (int i=0; i <= N; i++) {
        f << m_input->readLong(2)/20.0 << ",";
      }
      f << "],";
      int N1 = (sz-(N+1)*2-2)/2; // 0 or 5*N ?
      if (N1) {
        f << "unk=[N1=" << N1 << ",";
        for (int i = 0; i < N1; i++) {
          val = m_input->readULong(2);
          if (val) f << std::hex << val << std::dec << ",";
          else f << "_";
        }
        f << "]";
      }
      f << "],";
      if (long(m_input->tell()) != actPos+2+sz) {
        ascii().addDelimiter(m_input->tell(),'#');
        m_input->seek(actPos+2+sz, WPX_SEEK_SET);
      }
      break;
    }
    default:
      done = false;
      break;
    }
    if (!done) {
      m_input->seek(actPos, WPX_SEEK_SET);
      break;
    }
  }

  if (long(m_input->tell()) != endPos) {
    static bool first = true;
    if (first) {
      MWAW_DEBUG_MSG(("MSWText::readParagraph: can not read end of paragraph\n"));
      first = false;
    }
    ascii().addDelimiter(m_input->tell(),'|');
    f << "#";
    m_input->seek(endPos, WPX_SEEK_SET);
  }
  para.m_error += f.str();

  return true;
}

////////////////////////////////////////////////////////////
// read the section data
////////////////////////////////////////////////////////////
bool MSWText::readSection(MSWTextInternal::Section &sec, long debPos)
{
  if (!m_mainParser->isFilePos(debPos)) {
    MWAW_DEBUG_MSG(("MSWText::readSection: can not find section data...\n"));
    return false;
  }
  m_input->seek(debPos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  int sz = m_input->readULong(1);
  long endPos = debPos+sz+1;
  if (sz < 1 || sz >= 255) {
    MWAW_DEBUG_MSG(("MSWText::readSection: data section size seems bad...\n"));
    f << "Section-" << sec << ":#";
    ascii().addPos(debPos);
    ascii().addNote(f.str().c_str());
    return false;
  }

  while (m_input->tell() < endPos) {
    long pos = m_input->tell();
    if (sec.read(m_input, endPos)) continue;
    f << "#";
    ascii().addDelimiter(pos,'|');
    break;
  }
  f << "Section-" << sec;
  ascii().addPos(debPos);
  ascii().addNote(f.str().c_str());

  ascii().addPos(endPos);
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read  a list of ints zone
////////////////////////////////////////////////////////////
bool MSWText::readLongZone(MSWEntry &entry, int sz, std::vector<long> &list)
{
  list.resize(0);
  if (entry.length() < sz || (entry.length()%sz)) {
    MWAW_DEBUG_MSG(("MSWText::readIntsZone: the size of zone %s seems to odd\n", entry.type().c_str()));
    return false;
  }

  long pos = entry.begin();
  m_input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << entry.type() << ":";
  int N = entry.length()/sz;
  for (int i = 0; i < N; i++) {
    int val = m_input->readLong(sz);
    list.push_back(val);
    f << std::hex << val << std::dec << ",";
  }

  if (long(m_input->tell()) != entry.end())
    ascii().addDelimiter(m_input->tell(), '|');

  entry.setParsed(true);

  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// try to read a text entry
////////////////////////////////////////////////////////////
bool MSWText::sendText(MWAWEntry const &textEntry, bool mainZone)
{
  if (!textEntry.valid()) return false;
  if (!m_listener) {
    MWAW_DEBUG_MSG(("MSWText::sendText: can not find a listener!"));
    return true;
  }
  long cDebPos = textEntry.begin(), cPos = cDebPos;
  long debPos = m_state->getFilePos(cPos), pos=debPos;
  m_input->seek(pos, WPX_SEEK_SET);
  long cEnd = textEntry.end();

  MSWTextInternal::Paragraph actPara;
  MSWTextInternal::Font actFont;
  actFont.m_font = m_state->m_defaultFont;
  bool fontSent = false;

  libmwaw::DebugStream f;
  f << "TextContent:";
  std::multimap<long, MSWTextInternal::PLC>::iterator plcIt;
  while (!m_input->atEOS() && cPos < cEnd) {
    plcIt = m_state->m_plcMap.find(cPos);
    while (plcIt != m_state->m_plcMap.end() && plcIt->first == cPos) {
      MSWTextInternal::PLC &plc = plcIt++->second;
#if DEBUG_PLC
      if (plc.m_type != plc.TextPosition)
        f << "@[" << plc << "]";
#endif
      switch (plc.m_type) {
      case MSWTextInternal::PLC::Page: {
        if (mainZone) m_mainParser->newPage(++m_state->m_actPage);
        actFont = MSWTextInternal::Font();
        actFont.m_font = m_state->m_defaultFont;
        fontSent = false;
        break;
      }
      case MSWTextInternal::PLC::Section: {
        if (plc.m_id < 0 || plc.m_id >= int(m_state->m_sectionList.size())) {
          MWAW_DEBUG_MSG(("MSWText::sendText: can not find new section\n"));
          break;
        }
        actFont = MSWTextInternal::Font();
        actFont.m_font = m_state->m_defaultFont;
        fontSent = false;
        setProperty(m_state->m_sectionList[plc.m_id], actFont);
        break;
      }
      case MSWTextInternal::PLC::TextPosition: {
        if (plc.m_id < 0 || plc.m_id >= int(m_state->m_textposList.size())) {
          MWAW_DEBUG_MSG(("MSWText::sendText: can not find new text position\n"));
          break;
        }
        MSWTextInternal::TextEntry const &entry=m_state->m_textposList[plc.m_id];
        int pId = entry.getParagraphId();
        if (pId >= 0) {
          if (pId < int(m_state->m_textstructParagraphList.size()))
            setProperty(m_state->m_textstructParagraphList[pId],actFont);
          else {
            MWAW_DEBUG_MSG(("MSWText::sendText: can not find a paragraph in textstruct...\n"));
          }
        }
        if (entry.m_fontId <= 0) { // CHECKME
          int fId = actFont.m_font.id(), fSz =actFont.m_font.size();
          actFont = MSWTextInternal::Font();
          actFont.m_font = MWAWStruct::Font(fId, fSz);
        } else
          actFont = m_state->m_fontList[entry.m_fontId];
        fontSent = false;
        if (entry.begin() == pos)
          break;
        if (cPos != cDebPos) {
          ascii().addPos(debPos);
          ascii().addNote(f.str().c_str());
          f.str("");
          f << "TextContent:";
          cDebPos = cPos;
        }
        debPos = pos = m_state->m_textposList[plc.m_id].begin();
        m_input->seek(debPos, WPX_SEEK_SET);
        break;
      }
      case MSWTextInternal::PLC::Field: // some fields ?
#ifdef DEBUG
        m_mainParser->sendFieldComment(plc.m_id);
#endif
        break;
      case MSWTextInternal::PLC::Footnote: {
        int numCols = m_state->m_cols;
        m_state->m_cols=1;
        m_mainParser->sendFootnote(plc.m_id);
        m_state->m_cols=numCols;
        break;
      }
      case MSWTextInternal::PLC::FootnoteDef:
        break;
      default:
        break;
      }
    }
    cPos++;
    plcIt = m_state->m_filePlcMap.find(pos);
    while (plcIt != m_state->m_filePlcMap.end() && plcIt->first == pos) {
      MSWTextInternal::PLC &plc = plcIt++->second;
      switch (plc.m_type) {
      case MSWTextInternal::PLC::Font:
        if (plc.m_id < -1 || plc.m_id >= int(m_state->m_fontList.size())) {
          MWAW_DEBUG_MSG(("MSWText::sendText: can not find new font\n"));
          break;
        }
        if (plc.m_id == -1) {
          // CHECKME: clearly
          int fSz =actFont.m_font.size();
          actFont = MSWTextInternal::Font();
          actFont.m_font = MWAWStruct::Font(m_state->m_defaultFont.id(), fSz);
        } else {
          int fId = actFont.m_font.id();
          actFont = m_state->m_fontList[plc.m_id];
          if (actFont.m_font.id() == -1) actFont.m_font.setId(fId);
        }
        fontSent = false;
        break;
      case MSWTextInternal::PLC::Paragraph:
        if (plc.m_id < -1 || plc.m_id >= int(m_state->m_paragraphList.size())) {
          MWAW_DEBUG_MSG(("MSWText::sendText: can not find new paragraph\n"));
          break;
        }
        if (plc.m_id == -1)
          actPara = MSWTextInternal::Paragraph();
        else {
          actPara = m_state->m_paragraphList[plc.m_id];
        }
        setProperty(actPara, actFont);
        break;
      default:
        break;
      }
#if DEBUG_PLC
      f << "@[" << plc << "]";
#endif
    }
    if (!fontSent) {
      setProperty(actFont);
      fontSent = true;
    }
    int c = m_input->readULong(1);
    pos++;
    switch (c) {
    case 0x1: // FIXME: object insertion
      break;
    case 0x7: // FIXME: cell end ?
      m_listener->insertEOL();
      break;
    case 0xc: // end section (ok)
      break;
    case 0x2:
      m_listener->insertField(MWAWContentListener::PageNumber);
      break;
    case 0x6:
      m_listener->insertCharacter('\\');
      break;
    case 0x1e: // unbreaking - ?
      m_listener->insertCharacter('-');
      break;
    case 0x1f: // hyphen
      break;
    case 0x13: // month
    case 0x1a: // month abreviated
    case 0x1b: // checkme month long
      m_listener->insertDateTimeField("%m");
      break;
    case 0x10: // day
    case 0x16: // checkme: day abbreviated
    case 0x17: // checkme: day long
      m_listener->insertDateTimeField("%d");
      break;
    case 0x15: // year
      m_listener->insertDateTimeField("%y");
      break;
    case 0x1d:
      m_listener->insertField(MWAWContentListener::Date);
      break;
    case 0x18: // checkme hour
    case 0x19: // checkme hour
      m_listener->insertDateTimeField("%H");
      break;
    case 0x4:
      m_listener->insertField(MWAWContentListener::Time);
      break;
    case 0x5: // footnote mark (ok)
      break;
    case 0x9:
      m_listener->insertTab();
      break;
    case 0xb: // line break (simple but no a paragraph break ~soft)
      m_listener->insertEOL(true);
      break;
    case 0xd: // line break hard
      m_listener->insertEOL();
      break;
    case 0x11: // command key in help
      m_listener->insertUnicode(0x2318);
      break;
    case 0x14: // apple logo ( note only in private zone)
      m_listener->insertUnicode(0xf8ff);
      break;
    default: {
      int unicode = m_convertissor->getUnicode (actFont.m_font, c);
      if (unicode == -1) {
        if (c < 32) {
          MWAW_DEBUG_MSG(("MSWText::sendText: Find odd char %x\n", int(c)));
          f << "#";
        } else
          m_listener->insertCharacter(c);
      } else
        m_listener->insertUnicode(unicode);
      break;
    }
    }
    if (c)
      f << char(c);
    else
      f << "###";
  }

  ascii().addPos(debPos);
  ascii().addNote(f.str().c_str());
  ascii().addPos(m_input->tell());
  ascii().addNote("_");
  return true;
}

bool MSWText::sendMainText()
{
  MWAWEntry entry;
  entry.setBegin(0);
  entry.setLength(m_state->m_textLength[0]);
  sendText(entry, true);
  return true;
}

bool MSWText::sendFootnote(int id)
{
  if (!m_listener) return true;
  if (id < 0 || id >= int(m_state->m_footnoteList.size())) {
    MWAW_DEBUG_MSG(("MSWText::sendFootnote: can not find footnote %d\n", id));
    m_listener->insertCharacter(' ');
    return false;
  }
  MSWTextInternal::Footnote &footnote = m_state->m_footnoteList[id];
  if (footnote.m_pos.isParsed())
    m_listener->insertCharacter(' ');
  else
    sendText(footnote.m_pos, false);
  footnote.m_pos.setParsed();
  return true;
}

bool MSWText::sendFieldComment(int id)
{
  if (!m_listener) return true;
  if (id < 0 || id >= int(m_state->m_fieldList.size())) {
    MWAW_DEBUG_MSG(("MSWText::sendFieldComment: can not find field %d\n", id));
    m_listener->insertCharacter(' ');
    return false;
  }
  MSWTextInternal::Font defFont;
  defFont.m_font = m_state->m_defaultFont;
  setProperty(defFont);
  MSWTextInternal::Paragraph defParag;
  setProperty(defParag, defFont);
  std::string const &text = m_state->m_fieldList[id].m_text;
  if (!text.length()) m_listener->insertCharacter(' ');
  for (int c = 0; c < int(text.length()); c++) {
    int unicode = m_convertissor->getUnicode (defFont.m_font, text[c]);
    if (unicode == -1) {
      if (text[c] < 32) {
        MWAW_DEBUG_MSG(("MSWText::sendFieldComment: Find odd char %x\n", int(text[c])));
        m_listener->insertCharacter(' ');
      } else
        m_listener->insertCharacter(unicode);
    } else
      m_listener->insertUnicode(unicode);
  }
  return true;
}

void MSWText::setProperty(MSWTextInternal::Font const &font)
{
  if (!m_listener) return;
  font.m_font.sendTo(m_listener.get(), m_convertissor, m_state->m_font, true);
}

void MSWText::setProperty(MSWTextInternal::Section const &sec,
                          MSWTextInternal::Font &actFont, bool recursive)
{
  if (!m_listener) return;
  int numCols = sec.m_col;
  if (numCols >= 1 && m_state->m_cols > 1 && sec.m_colBreak) {
    if (!m_listener->isSectionOpened()) {
      MWAW_DEBUG_MSG(("MSWText::setProperty: section is not opened\n"));
    } else
      m_listener->insertBreak(DMWAW_COLUMN_BREAK);
  } else {
    if (m_listener->isSectionOpened())
      m_listener->closeSection();
    if (numCols<=1) m_listener->openSection();
    else {
      // column seems to have equal size
      int colWidth = int((72.0*m_mainParser->pageWidth())/numCols);
      std::vector<int> colSize;
      colSize.resize(numCols);
      for (int i = 0; i < numCols; i++) colSize[i] = colWidth;
      m_listener->openSection(colSize, WPX_POINT);
    }
    m_state->m_cols = numCols;
  }
  if (sec.m_paragraphId > -9999 && !recursive) {
    if (m_state->m_styleParagraphMap.find(sec.m_paragraphId) ==
        m_state->m_styleParagraphMap.end()) {
      MWAW_DEBUG_MSG(("MSWText::setProperty: can not find paragraph in section\n"));
    } else
      setProperty(m_state->m_styleParagraphMap.find(sec.m_paragraphId)->second,
                  actFont, true);
  }
}

void MSWText::setProperty(MSWTextInternal::Paragraph const &para,
                          MSWTextInternal::Font &actFont, bool recursive)
{
  if (!m_listener) return;
  if (!para.m_section.m_default && !recursive)
    setProperty(para.m_section, actFont, true);
  m_listener->justificationChange(para.m_justify);

  if (para.m_interline < 0.0)
    m_listener->lineSpacingChange(-para.m_interline, WPX_POINT);
  else if (para.m_interline) // fixme: in fact, this mean at least...
    m_listener->lineSpacingChange(para.m_interline, WPX_POINT);
  else
    m_listener->lineSpacingChange(1.0, WPX_PERCENT);

  m_listener->setParagraphTextIndent(para.m_margins[0]+para.m_margins[1]);
  m_listener->setParagraphMargin(para.m_margins[1], DMWAW_LEFT);
  m_listener->setParagraphMargin(para.m_margins[2], DMWAW_RIGHT);

  m_listener->setParagraphMargin(para.m_spacings[0]/72., DMWAW_TOP);
  m_listener->setParagraphMargin(para.m_spacings[1]/72., DMWAW_BOTTOM);
  m_listener->setTabs(para.m_tabs);

  if (!para.hasBorders())
    m_listener->setParagraphBorder(0);
  else {
    int w = 0;
    if (para.m_borders[0]) w |= DMWAW_TABLE_CELL_TOP_BORDER_OFF;
    if (para.m_borders[1]) w |= DMWAW_TABLE_CELL_LEFT_BORDER_OFF;
    if (para.m_borders[2]) w |= DMWAW_TABLE_CELL_BOTTOM_BORDER_OFF;
    if (para.m_borders[3]) w |= DMWAW_TABLE_CELL_RIGHT_BORDER_OFF;
    m_listener->setParagraphBorder(w);
  }
  if (!para.m_font2.m_default) {
    setProperty(para.m_font2);
    actFont = para.m_font2;
  } else if (!para.m_font.m_default) {
    setProperty(para.m_font);
    actFont = para.m_font;
  }
}

void MSWText::flushExtra()
{
#ifdef DEBUG
  if (m_state->m_textLength[1]) {
    for (int i = 0; i < int(m_state->m_footnoteList.size()); i++) {
      MSWTextInternal::Footnote &footnote = m_state->m_footnoteList[i];
      if (footnote.m_pos.isParsed()) continue;
      sendText(footnote.m_pos, false);
      footnote.m_pos.setParsed();
    }
  }
#endif
  if (m_state->m_textLength[2]) {
    MWAWEntry entry;
    entry.setBegin(m_state->m_textLength[0]+m_state->m_textLength[1]);
    entry.setLength(m_state->m_textLength[2]);
    sendText(entry, false);
  }
}


// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
