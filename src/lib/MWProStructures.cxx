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
#include <set>
#include <sstream>

#include <libwpd/WPXBinaryData.h>

#include "MWAWPosition.hxx"

#include "MWAWCell.hxx"
#include "MWAWHeader.hxx"
#include "MWAWTable.hxx"

#include "MWAWStruct.hxx"
#include "MWAWTools.hxx"
#include "MWAWContentListener.hxx"

#include "MWProStructures.hxx"
#include "MWProParser.hxx"

/** Internal: the structures of a MWProStructures */
namespace MWProStructuresInternal
{
////////////////////////////////////////
//! Internal: the data block
struct Block {
  enum Type { UNKNOWN, GRAPHIC, TEXT };
  //! the constructor
  Block() :m_type(-1), m_contentType(UNKNOWN), m_fileBlock(0), m_id(-1), m_attachment(false), m_page(-1), m_box(),
    m_baseline(0.), m_lineWidth(1.0), m_lineType(1), m_linePattern(2), m_textPos(0), m_isHeader(false), m_row(0), m_col(0), m_textboxCellType(0),
    m_extra(""), m_send(false) {
    for (int i = 0; i < 3; i++) m_color[i] = m_lineColor[i] = -1;
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Block const &bl) {
    switch(bl.m_contentType) {
    case GRAPHIC:
      o << "graphic,";
      if (bl.m_type != 8) {
        MWAW_DEBUG_MSG(("MWProStructuresInternal::Block::operator<< unknown type\n"));
        o << "#type=" << bl.m_type << ",";
      }
      break;
    case TEXT:
      o << "text";
      switch(bl.m_type) {
      case 3:
        o << "[table]";
        break;
      case 4:
        o << "[textbox/cell]";
        break;
      case 5:
        if (bl.m_textPos) o << "[pageBreak:" << bl.m_textPos << "]";
        break;
      case 6:
        o << "[header/footer]";
        break;
      case 7:
        o << "[footnote]";
        break;
      case 8:
        o << "[empty frame]";
        break;
      default:
        MWAW_DEBUG_MSG(("MWProStructuresInternal::Block::operator<< unknown type\n"));
        o << "[#" << bl.m_type << "]";
        break;
      }
      o << ",";
      break;
    default:
      break;
    }
    if (bl.m_id >= 0) o << "id=" << bl.m_id << ",";
    o << "box=" << bl.m_box << ",";
    for (int i = 0; i < 2; i++) {
      if (bl.m_border[i].x() == 0 && bl.m_border[i].y()==0) continue;
      o << "bord" << i << "?=" << bl.m_border[i] << ",";
    }
    if (bl.m_baseline) o << "baseline=" << bl.m_baseline << ",";
    if (bl.hasColor())
      o << "col=" << bl.m_color[0] << "x" << bl.m_color[1]
        << "x" << bl.m_color[1] << ",";
    if (bl.m_lineWidth != 1.0) o << "lineWidth=" << bl.m_lineWidth << "pt,";
    switch(bl.m_lineType) {
    case 1:
      break; // simple
    case 2:
      o << "double[line],";
      break;
    case 3:
      o << "double[line,externalx2],";
      break;
    case 4:
      o << "double[line,internalx2],";
      break;
    default:
      o << "#lineType=" << bl.m_lineType << ",";
      break;
    }
    switch(bl.m_linePattern) {
    case 1:
      o << "none,";
      break;
    case 2:
      break; // normal
    default:
      o << "linePattern=" << bl.m_linePattern << ",";
      break;
    }
    if (bl.hasLineColor())
      o << "lineCol=" << bl.m_lineColor[0] << "x" << bl.m_lineColor[1]
        << "x" << bl.m_lineColor[1] << ",";
    if (bl.m_fileBlock > 0) o << "block=" << std::hex << bl.m_fileBlock << std::dec << ",";
    if (bl.m_extra.length())
      o << bl.m_extra << ",";
    return o;
  }
  void fillFramePropertyList(WPXPropertyList &extra) const {
    std::stringstream s, s2;
    if (hasColor()) {
      s.str("");
      s << std::hex << std::setfill('0') << "#"
        << std::setw(2) << int(m_color[0])
        << std::setw(2) << int(m_color[1])
        << std::setw(2) << int(m_color[2]);
      extra.insert("fo:background-color", s.str().c_str());
    }
    if (hasBorderLine()) {
      s.str("");
      s2.str("");
      switch(m_lineType) {
      case 2:
        s2 << m_lineWidth << "pt " << m_lineWidth << "pt " << m_lineWidth << "pt";
        extra.insert("style:border-line-width", s2.str().c_str());
        s << 3*m_lineWidth << "pt";
        s << " double";
        break;
      case 3:
        s2 << m_lineWidth << "pt " << m_lineWidth << "pt " << 2*m_lineWidth << "pt";
        extra.insert("style:border-line-width", s2.str().c_str());
        s << 4*m_lineWidth << "pt";
        s << " double";
        break;
      case 4:
        s2 << 2*m_lineWidth << "pt " << m_lineWidth << "pt " << m_lineWidth << "pt";
        extra.insert("style:border-line-width", s2.str().c_str());
        s << 4*m_lineWidth << "pt";
        s << " double";
        break;
      default:
      case 1:
        s << m_lineWidth << "pt";
        s << " solid";
        break;
      }
      if (m_lineColor[0] >= 0) {
        s << std::hex << std::setfill('0') << " #"
          << std::setw(2) << int(m_lineColor[0])
          << std::setw(2) << int(m_lineColor[1])
          << std::setw(2) << int(m_lineColor[2]);
      } else s << " #000000";
      extra.insert("fo:border", s.str().c_str());
    }
  }

  bool isGraphic() const {
    return m_fileBlock > 0 && m_contentType == GRAPHIC;
  }
  bool isText() const {
    return (m_fileBlock > 0 && m_contentType == TEXT);
  }
  bool isTable() const {
    return m_fileBlock <= 0 && m_type == 3;
  }
  bool hasColor() const {
    return m_color[0] >= 0 &&
           (m_color[0] != 255 || m_color[1] != 255 ||  m_color[2] != 255);
  }
  bool hasLineColor() const {
    return m_lineColor[0] >= 0 &&
           (m_lineColor[0] != 0 || m_lineColor[1] != 0 ||  m_lineColor[2] != 0);
  }
  bool hasBorderLine() const {
    return (m_lineWidth !=0.0 && m_linePattern!=1);
  }

  MWAWPosition getPosition() const {
    MWAWPosition res;
    if (m_attachment) {
      res = MWAWPosition(Vec2i(0,0), m_box.size(), WPX_POINT);
      res.setRelativePosition(MWAWPosition::Char, MWAWPosition::XLeft, getRelativeYPos());
    } else {
      res = MWAWPosition(m_box.min(), m_box.size(), WPX_POINT);
      res.setRelativePosition(MWAWPosition::Page);
      res.setPage(m_page);
      res.m_wrapping =  MWAWPosition::WDynamic;
    }
    return res;
  }

  MWAWPosition::YPos getRelativeYPos() const {
    float height = m_box.size()[1];
    if (m_baseline < 0.25*height) return MWAWPosition::YBottom;
    if (m_baseline < 0.75*height) return MWAWPosition::YCenter;
    return MWAWPosition::YTop;
  }
  bool contains(Box2f const &box) const {
    return box[0][0] >= m_box[0][0] && box[0][1] >= m_box[0][1] &&
           box[1][0] <= m_box[1][0] && box[1][1] <= m_box[1][1];
  }
  bool intersects(Box2f const &box) const {
    if (box[0][0] >= m_box[1][0] || box[0][1] >= m_box[1][1] ||
        box[1][0] <= m_box[0][0] || box[1][1] <= m_box[1][1])
      return false;
    if (m_box[0][0] >= box[1][0] || m_box[0][1] >= box[1][1] ||
        m_box[1][0] <= box[0][0] || m_box[1][1] <= box[1][1])
      return false;
    return true;
  }

  //! the type
  int m_type;

  //! the type
  Type m_contentType;

  //! the file block id
  int m_fileBlock;

  //! the block id
  int m_id;

  //! true if this is an attachment
  bool m_attachment;

  //! the page (if absolute)
  int m_page;

  //! the bdbox
  Box2f m_box;

  //! the border or margin?
  Vec2f m_border[2];

  //! the baseline ( in point 0=bottom aligned)
  float m_baseline;

  //! the background color
  int m_color[3];

  //! the line color
  int m_lineColor[3];

  //! the line witdh(in point)
  float m_lineWidth;

  //! the line type
  int m_lineType;

  //! the line pattern
  int m_linePattern;

  /** filled for pagebreak pos */
  int m_textPos;

  /** filled for header/footer */
  bool m_isHeader;

  /** number of row, filled for table */
  int m_row;

  /** number of columns, filled for table */
  int m_col;

  /** filled for textbox : 0: unknown/textbox, 1: cell, 2: textbox(opened)*/
  int m_textboxCellType;

  //! extra data
  std::string m_extra;

  //! true if we have send the data
  bool m_send;
};

////////////////////////////////////////
//! Internal: the fonts
struct Font {
  //! the constructor
  Font(): m_font(), m_flags(0), m_language(-1), m_token(-1),
    m_extra("") {
    for (int i = 0; i < 5; i++) m_values[i] = 0;
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Font const &font) {
    if (font.m_flags) o << "flags=" << std::hex << font.m_flags << std::dec << ",";
    for (int i = 0; i < 5; i++) {
      if (!font.m_values[i]) continue;
      o << "f" << i << "=" << font.m_values[i] << ",";
    }
    switch(font.m_language) {
    case -1:
      break;
    case 0:
      break; // US
    case 2:
      o << "englUK,";
      break;
    case 3:
      o << "german,";
      break;
    default:
      o << "#lang=" << font.m_language << ",";
      break;
    }
    switch(font.m_token) {
    case -1:
      break;
    default:
      o << "token=" << font.m_token << ",";
      break;
    }
    if (font.m_extra.length())
      o << font.m_extra << ",";
    return o;
  }

  //! the font
  MWAWStruct::Font m_font;
  //! some unknown flag
  int m_flags;
  //! the language
  int m_language;
  //! the token type(checkme)
  int m_token;
  //! unknown values
  int m_values[5];

  //! extra data
  std::string m_extra;
};

////////////////////////////////////////
/** Internal: class to store the paragraph properties */
struct Paragraph {
  //! Constructor
  Paragraph() :  m_tabs(), m_justify (DMWAW_PARAGRAPH_JUSTIFICATION_LEFT),
    m_value(0), m_extra("") {
    for(int c = 0; c < 3; c++) m_margins[c] = 0.0;
    for(int i = 0; i < 3; i++) {
      m_spacing[i] = 0.0;
      m_spacingPercent[i]=true;
    }
    m_spacing[0] = 1.0;
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
    if (ind.m_spacing[0] != 1.0) {
      o << "interline=" << ind.m_spacing[0];
      if (ind.m_spacingPercent[0]) o << "%,";
      else o << "inch,";
    }
    for (int i = 1; i < 3; i++) {
      if (ind.m_spacing[i] == 0.0) continue;
      if (i==1) o << "spaceBef=";
      else o << "spaceAft=";
      o << ind.m_spacing[i];
      if (ind.m_spacingPercent[i]) o << "%,";
      else o << "inch,";
    }
    if (ind.m_margins[0]) o << "firstLPos=" << ind.m_margins[0] << ", ";
    if (ind.m_margins[1]) o << "leftPos=" << ind.m_margins[1] << ", ";
    if (ind.m_margins[2]) o << "rightPos=" << ind.m_margins[2] << ", ";
    libmwaw::internal::printTabs(o, ind.m_tabs);
    if (ind.m_value) o << "unkn=" << ind.m_value << ",";
    if (ind.m_extra.length()) o << "extra=[" << ind.m_extra << "],";
    return o;
  }

  /** the margins in inches
   *
   * 0: first line left, 1: left, 2: right
   */
  float m_margins[3];
  /** the spacing (interline, before, after) */
  float m_spacing[3];
  /** the spacing unit (percent or point) */
  float m_spacingPercent[3];
  //! the tabulations
  std::vector<DMWAWTabStop> m_tabs;
  //! paragraph justification : DWPS_PARAGRAPH_JUSTIFICATION*
  int m_justify;

  //! a unknown value
  int m_value;

  //! extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the cell of a WNProStructure
struct Cell : public MWAWTableCell {
  //! constructor
  Cell(MWProStructures &parser) : MWAWTableCell(), m_parser(parser),
    m_blockId(0) {
    for (int i = 0; i < 3; i++)
      m_color[i] = -1;
  }
  //! set the background color
  void setBackColor(int const (&col)[3]) {
    for (int i = 0; i < 3; i++)
      m_color[i] = col[i];
  }
  //! send the content
  virtual bool send(MWAWContentListenerPtr listener) {
    if (!listener) return true;
    // fixme
    int border = DMWAW_TABLE_CELL_TOP_BORDER_OFF
                 | DMWAW_TABLE_CELL_RIGHT_BORDER_OFF
                 | DMWAW_TABLE_CELL_BOTTOM_BORDER_OFF
                 | DMWAW_TABLE_CELL_LEFT_BORDER_OFF;

    MWAWCell cell;
    cell.position() = m_position;
    cell.setBorders(border);
    cell.setNumSpannedCells(m_numberCellSpanned);

    WPXPropertyList propList;
    if (m_color[0] >= 0 && m_color[1] >= 0 && m_color[2] >= 0) {
      std::stringstream s;
      s << std::hex << std::setfill('0') << "#"
        << std::setw(2) << int(m_color[0])
        << std::setw(2) << int(m_color[1])
        << std::setw(2) << int(m_color[2]);
      propList.insert("fo:background-color", s.str().c_str());
    }
    listener->openTableCell(cell, propList);
    sendContent(listener);
    listener->closeTableCell();
    return true;
  }

  //! send the content
  bool sendContent(MWAWContentListenerPtr listener) {
    if (m_blockId > 0)
      m_parser.send(m_blockId);
    else if (listener) // try to avoid empty cell
      listener->insertCharacter(' ');
    return true;
  }

  //! the text parser
  MWProStructures &m_parser;
  //! the block id
  int m_blockId;
  //! the background color
  int m_color[3];
};

////////////////////////////////////////
////////////////////////////////////////
struct Table : public MWAWTable {
  //! constructor
  Table() : MWAWTable() {
  }

  //! return a cell corresponding to id
  Cell *get(int id) {
    if (id < 0 || id >= numCells()) {
      MWAW_DEBUG_MSG(("MWProStructuresInternal::Table::get: cell %d does not exists\n",id));
      return 0;
    }
    return reinterpret_cast<Cell *>(MWAWTable::get(id).get());
  }
};

////////////////////////////////////////
//! Internal: the section of a MWProStructures
struct Section {
  enum StartType { S_Line, S_Page, S_PageLeft, S_PageRight };

  //! constructor
  Section() : m_start(S_Page), m_colsWidth(), m_colsBegin(),
    m_textLength(0), m_extra("") {
    for (int i = 0; i < 2; i++) m_headerIds[i] = m_footerIds[i] = 0;
  }
  //! return the number of columns
  int numColumns() const {
    if (m_colsWidth.size())
      return m_colsWidth.size();
    return 1;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Section const &sec) {
    switch (sec.m_start) {
    case S_Line:
      o << "newLine,";
      break;
    case S_Page:
      break;
    case S_PageLeft:
      o << "newPage[left],";
      break;
    case S_PageRight:
      o << "newPage[right],";
      break;
    }
    int numColumns = sec.numColumns();
    if (numColumns != 1) {
      o << "nCols=" << numColumns << ",";
      o << "colsW=[";
      for (int i = 0; i < numColumns; i++)
        o << sec.m_colsWidth[i] << ",";
      o << "], colsPos=[";
      for (int i = 0; i < numColumns; i++)
        o << sec.m_colsBegin[i] << ",";
      o << "],";
    }
    if (sec.m_headerIds[0]) o << "sec.headerId=" << sec.m_headerIds[0] << ",";
    if (sec.m_headerIds[0]!=sec.m_headerIds[1]) o << "sec.headerId1=" << sec.m_headerIds[0] << ",";
    if (sec.m_footerIds[0]) o << "sec.footerId=" << sec.m_footerIds[0] << ",";
    if (sec.m_footerIds[0]!=sec.m_footerIds[1]) o << "sec.footerId1=" << sec.m_footerIds[0] << ",";
    if (sec.m_textLength) o << "nChar=" << sec.m_textLength << ",";
    if (sec.m_extra.length()) o << sec.m_extra;
    return o;
  }
  //! the way to start the new section
  StartType m_start;
  //! the columns size in point
  std::vector<float> m_colsWidth;
  //! the columns float pos in point
  std::vector<float> m_colsBegin;
  //! the header block ids
  int m_headerIds[2];
  //! the footerer block ids
  int m_footerIds[2];
  //! the number of character in the sections
  long m_textLength;
  //! extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the state of a MWProStructures
struct State {
  //! constructor
  State() : m_version(-1), m_numPages(1), m_inputData(), m_fontsList(), m_paragraphsList(),
    m_sectionsList(), m_blocksList(), m_blocksMap(), m_tablesMap(),m_footnotesList(),
    m_headersMap(), m_footersMap() {
  }

  //! the file version
  int m_version;

  //! the number of pages
  int m_numPages;

  //! the input data
  WPXBinaryData m_inputData;

  //! the list of fonts
  std::vector<Font> m_fontsList;
  //! the list of paragraph
  std::vector<Paragraph> m_paragraphsList;
  //! the list of section
  std::vector<Section> m_sectionsList;
  //! the list of block
  std::vector<shared_ptr<Block> > m_blocksList;
  //! a map block id -> block
  std::map<int, shared_ptr<Block> > m_blocksMap;
  //! a map block id -> table
  std::map<int, shared_ptr<Table> > m_tablesMap;
  //! the foonote list (for MWII)
  std::vector<int> m_footnotesList;
  //! a map page -> header id
  std::map<int, int> m_headersMap;
  //! a map page -> footer id
  std::map<int, int> m_footersMap;
};

}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MWProStructures::MWProStructures(MWProParser &parser) :
  m_input(), m_mainParser(parser), m_listener(),
  m_convertissor(m_mainParser.m_convertissor),
  m_state(), m_asciiFile(), m_asciiName("")
{
  init();
}

MWProStructures::~MWProStructures()
{
  ascii().reset();
}

void MWProStructures::init()
{
  m_state.reset(new MWProStructuresInternal::State);
  m_listener.reset();
  m_asciiName = "struct";
}

void MWProStructures::setListener(MWProContentListenerPtr listen)
{
  m_listener = listen;
}

int MWProStructures::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_mainParser.version();
  return m_state->m_version;
}

int MWProStructures::numPages() const
{
  return m_state->m_numPages;
}

////////////////////////////////////////////////////////////
// try to return the header/footer block id
int MWProStructures::getHeaderId(int page)
{
  if (version()==0) page = 0;
  if (m_state->m_headersMap.find(page) != m_state->m_headersMap.end())
    return m_state->m_headersMap.find(page)->second;
  return 0;
}

int MWProStructures::getFooterId(int page)
{
  if (version()==0) page = 0;
  if (m_state->m_footersMap.find(page) != m_state->m_footersMap.end())
    return m_state->m_footersMap.find(page)->second;
  return 0;
}

////////////////////////////////////////////////////////////
// try to return the color
bool MWProStructures::getColor(int colId, Vec3uc &color) const
{
  int col;
  if (version()==0) {
    // MWII: 2:red 4: blue, ..
    switch(colId) {
    case 0:
      col = 0xFFFFFF;
      break;
    case 1:
      col = 0;
      break;
    case 2:
      col = 0xFF0000;
      break;
    case 3:
      col = 0x00FF00;
      break;
    case 4:
      col = 0x0000FF;
      break;
    case 5:
      col = 0x00FFFF;
      break; // cyan
    case 6:
      col = 0xFF00FF;
      break; // magenta
    case 7:
      col = 0xFFFF00;
      break; // yellow
    default:
      MWAW_DEBUG_MSG(("MWProStructures::getColor: unknown color %d\n", colId));
      return false;
    }
  } else {
    /* 0: white, 38: yellow, 44: magenta, 36: red, 41: cyan, 39: green, 42: blue
       checkme: this probably corresponds to the following 81 gray/color palette...
    */
    int const colorMap[] = {
      0xFFFFFF, 0x0, 0x222222, 0x444444, 0x666666, 0x888888, 0xaaaaaa, 0xcccccc, 0xeeeeee,
      0x440000, 0x663300, 0x996600, 0x002200, 0x003333, 0x003399, 0x000055, 0x330066, 0x660066,
      0x770000, 0x993300, 0xcc9900, 0x004400, 0x336666, 0x0033ff, 0x000077, 0x660099, 0x990066,
      0xaa0000, 0xcc3300, 0xffcc00, 0x006600, 0x006666, 0x0066ff, 0x0000aa, 0x663399, 0xcc0099,
      0xdd0000, 0xff3300, 0xffff00, 0x008800, 0x009999, 0x0099ff, 0x0000dd, 0x9900cc, 0xff0099,
      0xff3333, 0xff6600, 0xffff33, 0x00ee00, 0x00cccc, 0x00ccff, 0x3366ff, 0x9933ff, 0xff33cc,
      0xff6666, 0xff6633, 0xffff66, 0x66ff66, 0x66cccc, 0x66ffff, 0x3399ff, 0x9966ff, 0xff66ff,
      0xff9999, 0xff9966, 0xffff99, 0x99ff99, 0x66ffcc, 0x99ffff, 0x66ccff, 0x9999ff, 0xff99ff,
      0xffcccc, 0xffcc99, 0xffffcc, 0xccffcc, 0x99ffcc, 0xccffff, 0x99ccff, 0xccccff, 0xffccff
    };
    if (colId < 0 || colId >= 81) {
      MWAW_DEBUG_MSG(("MWProStructures::getColor: unknown color %d\n", colId));
      return false;
    }
    col = colorMap[colId];
  }

  color = Vec3uc((col>>16)&0xff, (col>>8)&0xff,col&0xff);
  return true;
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool MWProStructures::createZones()
{
  if (version() == 0)
    return createZonesV2();

  // first we need to create the input
  if (!m_mainParser.getZoneData(m_state->m_inputData, 3))
    return false;
  WPXInputStream *dataInput =
    const_cast<WPXInputStream *>(m_state->m_inputData.getDataStream());
  if (!dataInput) {
    MWAW_DEBUG_MSG(("MWProStructures::createZones: can not find my input\n"));
    return false;
  }
  m_input.reset(new MWAWInputStream(dataInput, false));
  m_input->setResponsable(false);

  ascii().setStream(m_input);
  ascii().open(asciiName());

  long pos = 0;
  m_input->seek(0, WPX_SEEK_SET);

  if (version() == 0) {
    bool ok = readFontsName();
    if (ok) pos = m_input->tell();
    ascii().addPos(pos);
    ascii().addNote("Entries(Data1):");
    ascii().addPos(pos+100);
    ascii().addNote("_");
    return true;
  }
  bool ok = readStyles() && readCharStyles();
  if (ok) {
    pos = m_input->tell();
    if (!readSelection()) {
      ascii().addPos(pos);
      ascii().addNote("Entries(Selection):#");
      m_input->seek(pos+16, WPX_SEEK_SET);
    }
  }

  if (ok) {
    pos = m_input->tell();
    ok = readFontsName();
    if (!ok) {
      ascii().addPos(pos);
      ascii().addNote("Entries(FontsName):#");
    }
  }
  if (ok) {
    pos = m_input->tell();
    ok = readStructB();
    if (!ok) {
      ascii().addPos(pos);
      ascii().addNote("Entries(StructB):#");
    }
  }
  if (ok) {
    pos = m_input->tell();
    ok = readFontsDef();
    if (!ok) {
      ascii().addPos(pos);
      ascii().addNote("Entries(FontsDef):#");
    }
  }
  if (ok) {
    pos = m_input->tell();
    ok = readParagraphs();
    if (!ok) {
      ascii().addPos(pos);
      ascii().addNote("Entries(ParaZone):#");
    }
  }
  for (int st = 0; st < 2; st++) {
    if (!ok) break;
    pos = m_input->tell();
    std::vector<MWProStructuresInternal::Section> sections;
    ok = readSections(sections);
    if (!ok) {
      ascii().addPos(pos);
      ascii().addNote("Entries(Sections):#");
      break;
    }
    if (st == 0) continue;
    m_state->m_sectionsList = sections;
  }
  if (ok) {
    pos = m_input->tell();
    libmwaw::DebugStream f;
    f << "Entries(UserName):";
    // username,
    std::string res;
    for (int i = 0; i < 2; i++) {
      ok = readString(m_input, res);
      if (!ok) {
        f << "#" ;
        break;
      }
      f << "'" << res << "',";
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  if (ok) {
    pos = m_input->tell();
    ok = readBlocksList();
    if (!ok) {
      ascii().addPos(pos);
      ascii().addNote("Entries(Block):#");
    }
  }

  pos = m_input->tell();
  ascii().addPos(pos);
  ascii().addNote("Entries(End)");

  // ok, now we can build the structures
  buildPageStructures();
  buildTableStructures();

  return true;
}

bool MWProStructures::createZonesV2()
{
  if (version()) {
    MWAW_DEBUG_MSG(("MWProStructures::createZonesV2: must be called for a MacWriteII file\n"));
    return false;
  }
  // first we need to create the input
  if (!m_mainParser.getZoneData(m_state->m_inputData, 3))
    return false;

  libmwaw::DebugStream f;
  WPXInputStream *dataInput =
    const_cast<WPXInputStream *>(m_state->m_inputData.getDataStream());
  if (!dataInput) {
    MWAW_DEBUG_MSG(("MWProStructures::createZonesV2: can not find my input\n"));
    return false;
  }
  m_input.reset(new MWAWInputStream(dataInput, false));
  m_input->setResponsable(false);

  ascii().setStream(m_input);
  ascii().open(asciiName());

  long pos = 0;
  m_input->seek(0, WPX_SEEK_SET);

  bool ok = readFontsName();
  long val;
  if (ok) {
    pos = m_input->tell();
    val = m_input->readULong(4);
    if (val) {
      MWAW_DEBUG_MSG(("MWProStructures::createZonesV2: argh!!! find data after the fonts name zone. Trying to continue.\n"));
      f.str("");
      f << "Entries(Styles):#" << std::hex << val << std::dec;

      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
    } else {
      ascii().addPos(pos);
      ascii().addNote("_");
    }

    pos = m_input->tell();
    ok = readCharStyles();
    if (ok) pos = m_input->tell();
  }
  if (ok) {
    pos = m_input->tell();
    ok = readFontsDef();
    if (ok)
      pos =  m_input->tell();
  }
  if (ok) {
    pos = m_input->tell();
    ok = readParagraphs();
    if (ok)
      pos =  m_input->tell();
  }
  if (ok) {
    pos = m_input->tell();
    int id = 0;
    bool first = true;
    shared_ptr<MWProStructuresInternal::Block> block;
    while (1) {
      block = readBlockV2(id++);
      if (!block) {
        ok = false;
        break;
      }
      // temporary fixme...
      block->m_contentType = MWProStructuresInternal::Block::TEXT;
      block->m_id=id;
      m_state->m_blocksMap[block->m_id] = block;
      m_state->m_blocksList.push_back(block);
      if (block->m_fileBlock) {
        m_mainParser.parseDataZone(block->m_fileBlock, 0);
        first = true;
      }
      pos =  m_input->tell();
      int val = m_input->readLong(1);
      if (val == 2) continue;
      if (val != 3) break;
      m_input->seek(-1, WPX_SEEK_CUR);
    }
  }
  ascii().addPos(pos);
  ascii().addNote("Entries(DataEnd):");

  int numPages = 1;
  for (int i = 0; i < int(m_state->m_blocksList.size()); i++) {
    shared_ptr<MWProStructuresInternal::Block> &block = m_state->m_blocksList[i];
    switch (block->m_type) {
    case 6:
      if (block->m_isHeader)
        m_state->m_headersMap[0] = i;
      else
        m_state->m_footersMap[0] = i;
      break;
    case 5: // soft page break
      numPages++;
      break;
    case 7:
      m_state->m_footnotesList.push_back(i);
      break;
    case -1: // hard page break
      if (block->isText())
        numPages += m_mainParser.findNumHardBreaks(block->m_fileBlock);
      break;
    default:
      break;
    }
  }
  m_state->m_numPages = numPages;

  return true;
}

////////////////////////////////////////////////////////////
// try to find the main text zone and sent it
bool MWProStructures::sendMainZone()
{
  int vers = version();
  for (int i = 0; i < int(m_state->m_blocksList.size()); i++) {
    if (!m_state->m_blocksList[i]->isText() ||
        m_state->m_blocksList[i]->m_send) continue;
    if (vers == 1 && m_state->m_blocksList[i]->m_type != 5)
      continue;
    // fixme
    if (vers == 0 && m_state->m_blocksList[i]->m_type != -1)
      continue;
    return send(vers==0 ? i : m_state->m_blocksList[i]->m_id, true);
  }

  //ok the main zone can be empty
  for (int i = 0; i < int(m_state->m_blocksList.size()); i++) {
    if (m_state->m_blocksList[i]->m_type != 5 ||
        m_state->m_blocksList[i]->m_send) continue;

    shared_ptr<MWProStructures> THIS
    (this, MWAW_shared_ptr_noop_deleter<MWProStructures>());
    MWProStructuresListenerState listenerState(THIS, true);
    return true;
  }

  MWAW_DEBUG_MSG(("MWProStructures::sendMainZone: can not find main zone...\n"));
  return false;
}

////////////////////////////////////////////////////////////
// try to find the header and the pages break
void MWProStructures::buildPageStructures()
{
  // first find the pages break
  std::set<int> set;
  int actPage = 0;
  for (int i = 0; i < int(m_state->m_blocksList.size()); i++) {
    m_state->m_blocksList[i]->m_page = actPage ? actPage : 1; // mainly ok
    if (m_state->m_blocksList[i]->m_type != 5)
      continue;
    actPage++;
    set.insert(m_state->m_blocksList[i]->m_textPos);
  }
  int numSections = m_state->m_sectionsList.size();
  int actSectPos = 0;
  for (int i = 0; i < numSections; i++) {
    MWProStructuresInternal::Section &sec = m_state->m_sectionsList[i];
    if (sec.m_start != sec.S_Line) set.insert(actSectPos);
    actSectPos += sec.m_textLength;
  }
  std::vector<int> pagesBreak;
  pagesBreak.assign(set.begin(), set.end());

  // now associates the header/footer to each pages
  int numPages = m_state->m_numPages = pagesBreak.size();
  int actPagePos = 0;
  actPage = 0;
  actSectPos = 0;
  for (int i = 0; i < numSections; i++) {
    MWProStructuresInternal::Section &sec = m_state->m_sectionsList[i];
    std::vector<int> listPages;
    actSectPos += sec.m_textLength;
    while (actPagePos < actSectPos) {
      listPages.push_back(actPage);
      if (actPage == numPages-1 || pagesBreak[actPage+1] > actSectPos)
        break;
      actPagePos=pagesBreak[++actPage];
    }
    int headerId = 0, footerId = 0;
    for (int k = 0; k < 2; k++) {
      if (sec.m_headerIds[k])
        headerId = sec.m_headerIds[k];
      if (sec.m_footerIds[k])
        footerId = sec.m_footerIds[k];
    }
    if (!headerId && !footerId) continue;
    for (int j = 0; j < int(listPages.size()); j++) {
      int p = listPages[j]+1;
      if (headerId && m_state->m_headersMap.find(p) == m_state->m_headersMap.end())
        m_state->m_headersMap[p] = headerId;
      if (footerId)
        m_state->m_footersMap[p] = footerId;
    }
  }
  // finally mark the attachment
  std::vector<int> const &listCalled = m_mainParser.getBlocksCalledByToken();
  for (int i = 0; i < int(listCalled.size()); i++) {
    if (m_state->m_blocksMap.find(listCalled[i]) == m_state->m_blocksMap.end()) {
      MWAW_DEBUG_MSG(("MWProStructures::buildPageStructures: can not find attachment block %d...\n",
                      listCalled[i]));
      continue;
    }
    shared_ptr<MWProStructuresInternal::Block> block =
      m_state->m_blocksMap.find(listCalled[i])->second;
    block->m_attachment = true;
  }
}

////////////////////////////////////////////////////////////
// try to find the main text zone and sent it
void MWProStructures::buildTableStructures()
{
  int numBlocks = m_state->m_blocksList.size();
  for (int i = 0; i < numBlocks; i++) {
    if (m_state->m_blocksList[i]->m_type != 3)
      continue;
    shared_ptr<MWProStructuresInternal::Block> table
      = m_state->m_blocksList[i];
    std::vector<shared_ptr<MWProStructuresInternal::Block> > blockList;
    int j = i+1;
    for ( ; j < numBlocks; j++) {
      shared_ptr<MWProStructuresInternal::Block> cell = m_state->m_blocksList[j];
      if (cell->m_type != 4)
        break;
      if (!table->contains(cell->m_box))
        break;
      bool ok = true;
      for (int k = 0; k < int(blockList.size()); k++) {
        if (cell->intersects(blockList[k]->m_box)) {
          ok = false;
          break;
        }
      }
      if (!ok)
        break;
      blockList.push_back(cell);
    }
    if (j-1 >= i) i = j-1;

    int numCells = blockList.size();
    bool ok = numCells > 1;
    if (!ok && numCells == 1)
      ok = table->m_col == 1 && table->m_row == 1;
    if (!ok) {
      MWAW_DEBUG_MSG(("MWProStructures::buildTableStructures: find a table with %d cells : ignored...\n", numCells));
      continue;
    }

    shared_ptr<MWProStructuresInternal::Table> newTable(new MWProStructuresInternal::Table);
    for (int j = 0; j < numCells; j++) {
      blockList[j]->m_send = true;
      blockList[j]->m_attachment = true;
      Box2f box(blockList[j]->m_box.min(), blockList[j]->m_box.max()-Vec2f(1,1));
      shared_ptr<MWProStructuresInternal::Cell> newCell(new MWProStructuresInternal::Cell(*this));
      newCell->setBox(box);
      newCell->setBackColor(blockList[j]->m_color);
      newCell->m_blockId = blockList[j]->m_id;
      blockList[j]->m_textboxCellType=1;
      newTable->add(newCell);
    }
    m_state->m_tablesMap[table->m_id]=newTable;
  }
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read the font names
bool MWProStructures::readFontsName()
{
  long pos = m_input->tell();
  libmwaw::DebugStream f;

  long sz = m_input->readULong(4);
  if (sz == 0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }
  int vers = version();
  long endPos = pos+4+sz;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("MWProStructures::readFontsName: file is too short\n"));
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  m_input->seek(pos+4, WPX_SEEK_SET);
  if (sz == 0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }
  f << "Entries(FontsName):";
  int N=m_input->readULong(2);
  if (3*N+4 > sz) {
    MWAW_DEBUG_MSG(("MWProStructures::readFontsName: can not read the number of fonts\n"));
    m_input->seek(endPos, WPX_SEEK_SET);
    f << "#";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return true;
  }

  for (int ft = 0; ft < N; ft++) {
    int fId = m_input->readLong(2);
    f << "[id=" << fId << ",";
    for (int st = 0; st < 2; st++) {
      int sSz = m_input->readULong(1);
      if (long(m_input->tell())+sSz > endPos) {
        MWAW_DEBUG_MSG(("MWProStructures::readFontsName: can not read the %d font\n", ft));
        f << "#";
        break;
      }
      std::string name("");
      for (int i = 0; i < sSz; i++)
        name += char(m_input->readULong(1));
      if (name.length()) {
        if (st == 0)
          m_convertissor->setFontCorrespondance(fId, name);
        f << name << ",";
      }
      if (vers)
        break;
    }
    f << "],";
  }

  if (long(m_input->tell()) != endPos)
    ascii().addDelimiter(m_input->tell(),'|');
  m_input->seek(endPos, WPX_SEEK_SET);

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read the character properties
bool MWProStructures::readFontsDef()
{
  long pos = m_input->tell();
  libmwaw::DebugStream f;

  long sz = m_input->readULong(4);
  if (sz == 0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }
  long endPos = pos+4+sz;
  int expectedSize = version()==0 ? 10 : 20;
  if ((sz%expectedSize) != 0) {
    MWAW_DEBUG_MSG(("MWProStructures::readFontsDef: find an odd value for sz\n"));
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("MWProStructures::readFontsDef: file is too short\n"));
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  m_input->seek(pos+4, WPX_SEEK_SET);
  f << "Entries(FontsDef):";
  int N = sz/expectedSize;
  f << "N=" << N;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  m_state->m_fontsList.resize(0);
  for (int n = 0; n < N; n++) {
    pos = m_input->tell();
    MWProStructuresInternal::Font font;
    if (!readFont(font)) {
      ascii().addPos(pos);
      ascii().addNote("FontsDef-#");
      m_input->seek(endPos, WPX_SEEK_SET);
      return true;
    }
    m_state->m_fontsList.push_back(font);
    f.str("");
    f << "FontsDef-C" << n << ":";
    f << m_convertissor->getFontDebugString(font.m_font) << font << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool MWProStructures::readFont(MWProStructuresInternal::Font &font)
{
  long pos = m_input->tell();
  int vers = version();
  libmwaw::DebugStream f;
  font = MWProStructuresInternal::Font();
  font.m_values[0] = m_input->readLong(2); // 1, 3 or 6
  int val = m_input->readULong(2);
  if (val != 0xFFFF)
    font.m_font.setId(val);
  val = m_input->readULong(2);
  if (val != 0xFFFF)
    font.m_font.setSize((val+3)/4);
  if (vers >= 1)
    font.m_values[1] = m_input->readLong(2);
  long flag = m_input->readULong(2);
  int flags=0;
  if (flag&0x1) flags |= DMWAW_BOLD_BIT;
  if (flag&0x2) flags |= DMWAW_ITALICS_BIT;
  if (flag&0x4) flags |= DMWAW_UNDERLINE_BIT;
  if (flag&0x8) flags |= DMWAW_EMBOSS_BIT;
  if (flag&0x10) flags |= DMWAW_SHADOW_BIT;
  if (flag&0x20) flags |= DMWAW_SUPERSCRIPT100_BIT;
  if (flag&0x40) flags |= DMWAW_SUBSCRIPT100_BIT;
  if (flag&0x100) flags |= DMWAW_SUPERSCRIPT_BIT;
  if (flag&0x200) flags |= DMWAW_STRIKEOUT_BIT;
  if (flag&0x400) flags |= DMWAW_ALL_CAPS_BIT;
  if (flag&0x800) flags |= DMWAW_SMALL_CAPS_BIT;
  if (flag&0x1000) flags |= DMWAW_UNDERLINE_BIT;
  if (flag&0x2000) flags |= DMWAW_DOUBLE_UNDERLINE_BIT;
  if (flag&0x4000) f << "lowercase,";
  font.m_flags = (flag&0x8080L);

  int color = m_input->readULong(1);
  Vec3uc col;
  if (color != 1 && getColor(color, col)) {
    int colVal[] = { col[0], col[1], col[2] };
    font.m_font.setColor(colVal);
  } else if (color != 1)
    f << "#colId=" << color << ",";
  val = m_input->readULong(1); // always 0x64 (unused?)
  if (val != 0x64) font.m_values[2] = val;
  if (vers == 1) {
    font.m_language =  m_input->readLong(2);
    font.m_token = m_input->readLong(2);
    /* f3=1 spacing 1, f3=3 spacing 3 */
    for (int i = 3; i < 5; i++)
      font.m_values[i] = m_input->readLong(2);
    m_input->seek(pos+20, WPX_SEEK_SET);
  }
  font.m_font.setFlags(flags);
  font.m_extra = f.str();

  return true;
}

////////////////////////////////////////////////////////////
// read a paragraph and a list of paragraph
bool MWProStructures::readParagraphs()
{
  long pos = m_input->tell();
  libmwaw::DebugStream f;
  int dataSz = version()==0 ? 202 : 192;

  long sz = m_input->readULong(4);
  if (sz == 0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }
  long endPos = pos+sz;
  if ((sz%dataSz) != 0) {
    MWAW_DEBUG_MSG(("MWProStructures::readParagraphs: find an odd value for sz\n"));
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("MWProStructures::readParagraphs: file is too short\n"));
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  m_input->seek(pos+4, WPX_SEEK_SET);
  f << "Entries(ParaZone):";
  int N = sz/dataSz;
  f << "N=" << N;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  int val;
  m_state->m_paragraphsList.resize(0);
  for (int n = 0; n < N; n++) {
    pos = m_input->tell();
    val = m_input->readLong(2);
    f.str("");
    f << "Entries(Paragraph)[" << n << "]:";
    if (val) f << "numChar?="<<val <<",";
    MWProStructuresInternal::Paragraph para;
    if (!readParagraph(para)) {
      f << "#";
      m_state->m_paragraphsList.push_back(MWProStructuresInternal::Paragraph());
      m_input->seek(pos+dataSz, WPX_SEEK_SET);
    } else {
      f << para;
      m_state->m_paragraphsList.push_back(para);
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool MWProStructures::readParagraph(MWProStructuresInternal::Paragraph &para)
{
  libmwaw::DebugStream f;
  int vers = version();
  long pos = m_input->tell(), endPos = pos+(vers == 0 ? 200: 190);
  para = MWProStructuresInternal::Paragraph();

  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("MWProStructures::readParagraph: file is too short\n"));
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  m_input->seek(pos, WPX_SEEK_SET);
  int val, just = 0;
  if (vers == 0) {
    just = m_input->readULong(2);
    val = m_input->readLong(2);
    if (val) f << "unkn=" << val << ",";
  }
  para.m_margins[1] = m_input->readLong(4)/72.0/65536.;
  para.m_margins[0] = m_input->readLong(4)/72.0/65536.+para.m_margins[1];
  para.m_margins[2] = m_input->readLong(4)/72.0/65536.;


  float spacing[3];
  for (int i = 0; i < 3; i++)
    spacing[i] = m_input->readLong(4)/65536.;
  for (int i = 0; i < 3; i++) {
    int dim = vers==0 ? m_input->readLong(4) : m_input->readULong(1);
    bool inPoint = true;
    bool ok = true;
    switch (dim) {
    case 0: // point
      ok = spacing[i] < 721 && (i || spacing[0] > 0.0);
      spacing[i]/=72.;
      break;
    case -1:
    case 0xFF: // percent
      ok = (spacing[i] >= 0.0 && spacing[i]<46.0);
      if (i==0) spacing[i]+=1.0;
      inPoint=false;
      break;
    default:
      f << "#inter[dim]=" << std::hex << dim << std::dec << ",";
      ok = spacing[i] < 721 && (i || spacing[0] > 0.0);
      spacing[i]/=72.;
      break;
    }
    if (ok) {
      // the interline spacing seems ignored when the dimension is point...
      if (i == 0 && inPoint)
        continue;
      para.m_spacing[i] = spacing[i];
      if (inPoint && spacing[i] > 1.0) {
        MWAW_DEBUG_MSG(("MWProStructures::readParagraph: spacing looks big decreasing it\n"));
        f << "#prevSpacing" << i << "=" << spacing[i] << ",";
        para.m_spacing[i] = 1.0;
      }
      para.m_spacingPercent[i] = !inPoint;
    } else
      f << "#spacing" << i << ",";
  }

  if (vers==1) {
    just = m_input->readULong(1);
    m_input->seek(pos+28, WPX_SEEK_SET);
  } else {
    ascii().addDelimiter(m_input->tell(),'|');
  }
  /* Note: when no extra tab the justification,
           if there is a extra tab, this corresponds to the extra tab alignement :-~ */
  switch(just & 0x3) {
  case 0:
    break;
  case 1:
    para.m_justify = DMWAW_PARAGRAPH_JUSTIFICATION_CENTER;
    break;
  case 2:
    para.m_justify = DMWAW_PARAGRAPH_JUSTIFICATION_RIGHT;
    break;
  case 3:
    para.m_justify = DMWAW_PARAGRAPH_JUSTIFICATION_FULL;
    break;
  }
  if (just&0xFC) f << "#justify=" << std::hex << just << std::dec << ",";
  bool emptyTabFound = false;
  for (int i = 0; i < 20; i++) {
    pos = m_input->tell();
    DMWAWTabStop newTab;
    int type = m_input->readULong(1);
    switch(type & 3) {
    case 0:
      break;
    case 1:
      newTab.m_alignment = CENTER;
      break;
    case 2:
      newTab.m_alignment = RIGHT;
      break;
    case 3:
      newTab.m_alignment = DECIMAL;
      break;
    default:
      break;
    }
    if (type & 0xfc) {
      MWAW_DEBUG_MSG(("MWProStructures::readParagraph: tab type is odd\n"));
      f << "tabs" << i << "[#type]=" << std::hex << (type & 0xFc) << std::dec << ",";
    }
    int leader = m_input->readULong(1);
    if (leader != 0x20)
      newTab.m_leaderCharacter = leader;
    unsigned long tabPos = m_input->readULong(4);
    if (tabPos == 0xFFFFFFFFL) {
      emptyTabFound = true;
      m_input->seek(pos+8, WPX_SEEK_SET);
      continue;
    }
    if (emptyTabFound) {
      MWAW_DEBUG_MSG(("MWProStructures::readParagraph: empty tab already found\n"));
      f << "tab" << i << "#";
    }
    newTab.m_position = float(tabPos)/72./65536.-para.m_margins[1];
    int decimalChar = m_input->readULong(1);
    if (decimalChar != '.' && decimalChar != ',')
      f << "tab" << i << "[decimalChar]=" << char(decimalChar) << ",";
    val = m_input->readLong(1); // always 0?
    if (val)
      f << "tab" << i << "[#unkn=" << std::hex << val << std::dec << "],";
    para.m_tabs.push_back(newTab);
    m_input->seek(pos+8, WPX_SEEK_SET);
  }

  if (vers==1) {
    m_input->seek(endPos-2, WPX_SEEK_SET);
    para.m_value = m_input->readLong(2);
  }
  para.m_extra=f.str();

  m_input->seek(endPos, WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// read the styles ( character )
bool MWProStructures::readCharStyles()
{
  long pos = m_input->tell();
  libmwaw::DebugStream f;
  int vers = version();

  int N;
  int expectedSz = 0x42;
  if (version() == 1) {
    long sz = m_input->readULong(4);
    if ((sz%0x42) != 0) {
      MWAW_DEBUG_MSG(("MWProStructures::readCharStyles: find an odd value for sz=%ld\n",sz));
      m_input->seek(pos, WPX_SEEK_SET);
      return false;
    }
    N = sz/0x42;
  } else {
    N = m_input->readULong(2);
    expectedSz = 0x2a;
  }

  if (N == 0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }
  long actPos = m_input->tell();
  long endPos = actPos+N*expectedSz;

  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("MWProStructures::readCharStyles: file is too short\n"));
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  m_input->seek(actPos, WPX_SEEK_SET);
  f << "Entries(CharStyles):N=" << N;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = m_input->tell();
    f.str("");
    f << "CharStyles-" << i << ":";
    int sSz = m_input->readULong(1);
    if (sSz > 31) {
      MWAW_DEBUG_MSG(("MWProStructures::readCharStyles: string size seems odd\n"));
      sSz = 31;
      f << "#";
    }
    std::string name("");
    for (int c = 0; c < sSz; c++)
      name += char(m_input->readULong(1));
    f << name << ",";
    m_input->seek(pos+32, WPX_SEEK_SET);

    if (vers == 1) {
      int val = m_input->readLong(2);
      if (val) f << "unkn0=" << val << ",";
      val = m_input->readLong(2);
      if (val != -1) f << "unkn1=" << val << ",";
      f << "ptr?=" << std::hex << m_input->readULong(4) << std::dec << ",";
      val = m_input->readLong(2); // small number between 0 and 2 (nextId?)
      if (val) f << "f0=" << val << ",";
      for (int j = 1; j < 5; j++) { // [-1,0,1], [0,1 or ee], 0, 0
        val = m_input->readLong(1);
        if (val) f << "f" << j <<"=" << val << ",";
      }
    }
    MWProStructuresInternal::Font font;
    if (!readFont(font)) {
      MWAW_DEBUG_MSG(("MWProStructures::readCharStyles: can not read the font\n"));
      f << "###";
    } else
      f << m_convertissor->getFontDebugString(font.m_font) << font << ",";

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());

    m_input->seek(pos+expectedSz, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the styles ( paragraph + font)
bool MWProStructures::readStyles()
{
  long pos = m_input->tell();
  libmwaw::DebugStream f;
  long sz = m_input->readULong(4);
  if ((sz%0x106) != 0) {
    MWAW_DEBUG_MSG(("MWProStructures::readStyles: find an odd value for sz=%ld\n",sz));
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  int N = sz/0x106;

  if (N==0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }

  f << "Entries(Style):";
  f << "N=" << N;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = m_input->tell();
    if (!readStyle(i)) {
      f.str("");
      f << "#Style-" << i << ":";
      m_input->seek(pos, WPX_SEEK_SET);
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return false;
    }
  }
  ascii().addPos(m_input->tell());
  ascii().addNote("_");

  return true;
}


bool MWProStructures::readStyle(int styleId)
{
  long debPos = m_input->tell(), pos = debPos;
  libmwaw::DebugStream f;
  // checkme something is odd here
  long dataSz = 0x106;
  long endPos = pos+dataSz;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("MWProStructures::readStyle: file is too short\n"));
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  m_input->seek(pos, WPX_SEEK_SET);
  f << "Style-" << styleId << ":";
  int strlen = m_input->readULong(1);
  if (!strlen || strlen > 31) {
    MWAW_DEBUG_MSG(("MWProStructures::readStyle: style name length seems bad!!\n"));
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  std::string name("");
  for (int i = 0; i < strlen; i++) // default
    name+=char(m_input->readULong(1));
  f << name << ",";
  m_input->seek(pos+32, WPX_SEEK_SET); // probably end of name

  int val;
  for (int i = 0; i < 3; i++) { // 0 | [0,1,-1] | numTabs or idStyle?
    val = m_input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  f << "ptr?=" << std::hex << m_input->readULong(4) << std::dec << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos = m_input->tell();

  f.str("");
  f << "Entries(Paragraph)[" << styleId << "]:";
  MWProStructuresInternal::Paragraph para;
  if (!readParagraph(para)) {
    f << "#";
    m_input->seek(pos+190, WPX_SEEK_SET);
  } else
    f << para;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos = m_input->tell();
  f.str("");
  f << "Style-" << styleId << "(II):";
  val = m_input->readLong(2);
  if (val != -1) f << "nextId?=" << val << ",";
  val = m_input->readLong(1); // -1 0 or 1
  if (val) f << "f0=" << val << ",";
  for (int i = 1; i < 4; i++) { // 0, then 0|1
    val = m_input->readLong(i==3?1:2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos = m_input->tell();
  MWProStructuresInternal::Font font;
  if (!readFont(font)) {
    MWAW_DEBUG_MSG(("MWProStructures::readStyle: end of style seems bad\n"));
    ascii().addPos(pos);
    ascii().addNote("Style:end###");
    m_input->seek(endPos, WPX_SEEK_SET);
    return long(m_input->tell()) == endPos;
  }

  f.str("");
  f << "FontsDef:";
  f << m_convertissor->getFontDebugString(font.m_font) << font << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos = m_input->tell();

  f.str("");
  f << "Style-" << styleId << "(end):";
  val = m_input->readLong(2);
  if (val!=-1) f << "unkn=" << val << ",";

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  m_input->seek(endPos, WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// read the list of blocks
bool MWProStructures::readBlocksList()
{
  long pos = m_input->tell();
  libmwaw::DebugStream f;

  long endPos = pos+45;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("MWProStructures::readBlocksList: file is too short\n"));
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  m_input->seek(pos, WPX_SEEK_SET);
  f << "Entries(Block):";
  int N= m_input->readLong(4); // 1 or 3
  f << "N?=" << N << ",";
  long val = m_input->readLong(4); // 0 or small number 1|fe 72, 529
  if (val) f << "f0=" << val << ",";
  for (int i = 0; i < 4; i++) { // [0|81|ff][0|03|33|63|ff][0|ff][0|ff]
    val = m_input->readULong(1);
    if (val) f << "flA" << i << "=" << std::hex << val << std::dec << ",";
  }
  val = m_input->readLong(4); // 0, 2, 46, 1479
  if (val) f << "f1=" << val << ",";
  for (int i = 0; i < 4; i++) { // [0|1][0|74][0][0|4]
    val = m_input->readULong(1);
    if (val) f << "flB" << i << "=" << std::hex << val << std::dec << ",";
  }
  for (int i = 2; i < 4; i++) { // [0|72] [0|a]
    val = m_input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  val = m_input->readULong(4);
  if (val) f << "ptr?=" << std::hex << val << std::dec << ",";

  std::string str;
  if (!readString(m_input, str))
    return false;
  if (str.length()) f << "dir='" << str << "',";
  val = m_input->readLong(2);
  if (val) f << "f4=" << val << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos = m_input->tell();
  long sz = getEndBlockSize();
  if (sz) {
    f.str("");
    f << "Block-end:";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(sz, WPX_SEEK_CUR);
  }
  shared_ptr<MWProStructuresInternal::Block> block;
  while (1) {
    block = readBlock();
    if (!block) break;
    m_state->m_blocksList.push_back(block);
    if (m_state->m_blocksMap.find(block->m_id) != m_state->m_blocksMap.end()) {
      MWAW_DEBUG_MSG(("MWProStructures::readBlocksList: block %d already exists\n", block->m_id));
    } else
      m_state->m_blocksMap[block->m_id] = block;
    if (block->isGraphic() || block->isText())
      m_mainParser.parseDataZone(block->m_fileBlock, block->isGraphic() ? 1 : 0);
  }
  return true;
}

int MWProStructures::getEndBlockSize()
{
  int sz = 8;
  long pos = m_input->tell();
  m_input->seek(6, WPX_SEEK_CUR);
  // CHECKME ( sometimes, we find 0x7FFF here and sometimes not !!!)
  if (m_input->readULong(2) == 0x7fff && m_input->readULong(2) == 1)
    sz += 2;
  m_input->seek(pos, WPX_SEEK_SET);
  return sz;
}

shared_ptr<MWProStructuresInternal::Block>  MWProStructures::readBlockV2(int wh)
{
  long pos = m_input->tell();
  long endPos = pos+76;
  libmwaw::DebugStream f;
  shared_ptr<MWProStructuresInternal::Block> res;

  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    m_input->seek(pos, WPX_SEEK_SET);
    return res;
  }
  m_input->seek(pos, WPX_SEEK_SET);
  long val;
  int type = m_input->readULong(1);
  res.reset(new MWProStructuresInternal::Block);
  res->m_contentType = MWProStructuresInternal::Block::TEXT;

  if (type == 3) {
    f << "type=3,";
    val = m_input->readLong(1);
    if (val) f<< "unkn=" << val << ",";
    int wh = m_input->readULong(1);
    switch(wh &0xF0) {
    case 0x40:
      res->m_isHeader = true;
    case 0x80:
      res->m_type = 6;
      break;
    case 0xc0:
      res->m_type = 7;
      break;
    default:
      MWAW_DEBUG_MSG(("MWProStructures::readBlockV2: find unknown block content type\n"));
      f << "#";
      break;
    }
    if (wh & 0xf) f << "f0=" << std::hex << int(wh & 0xf) << std::dec << ",";
    m_input->seek(23, WPX_SEEK_CUR);
    ascii().addDelimiter(m_input->tell(),'|');
  } else {
    endPos = pos+87;
    int bad = 0;
    m_input->seek(-1, WPX_SEEK_CUR);
    for (int i = 0; i < 2; i++) { // always 0, 0 ?
      val = m_input->readLong(2);
      if (!val) continue;
      f << "f" << i << "=" << val << ",";
      bad++;
    }
    if (bad >= 2) {
      m_input->seek(pos, WPX_SEEK_SET);
      res.reset();
      return res;
    }

    int id =  m_input->readLong(2); // small number, id ?
    if (id) f << "id=" << id << ",";
    val = m_input->readLong(1); // always -1 ?
    if (val != -1) f << "f2=" << val << ",";
    val = m_input->readLong(1); // 0, 1a, 2e, a8, c0
    if (val) f << "f3=" << val << ",";
    for (int i = 0; i < 2; i++) { // always 0,1 ?
      val = m_input->readLong(2);
      if (val != i) f << "f" << i+4 << "=" << val << ",";
    }
    val = m_input->readLong(1); // 3 or -3
    f << "f6=" << val << ",";
    val = m_input->readULong(1); // 0, 6a, 78, f2, fa : type ?
    if (val) f << "g0=" << val << ",";
    f << "unkn=[";
    for (int i = 0; i < 6; i++) {
      val = m_input->readULong(2);
      if (val==0) f << "_,";
      else f << std::hex << val << std::dec << ",";
    }
    f << "],";
    f << "unkn2=[";
    for (int i = 0; i < 6; i++) { // can be 0*12, bb*12 : junk ?
      val = m_input->readULong(2);
      if (val==0) f << "_,";
      else f << std::hex << val << std::dec << ",";
    }
    f << "],";
  }

  // can be fileblock or pageid
  res->m_fileBlock = m_input->readULong(2);

  float dim[4];
  for (int i = 0; i < 4; i++)
    dim[i] = m_input->readLong(2);
  res->m_box = Box2f(Vec2f(dim[1],dim[0]), Vec2f(dim[3],dim[2]));
  for (int i = 0; i < 4; i++) { // 8000*4 ?
    val = m_input->readULong(i==3 ? 1 : 2);
    if (val != 0x8000) f << "g" << i+1 << "=" << std::hex << val << std::dec << ",";
  }
  res->m_textPos =  m_input->readULong(4);
  if (res->m_textPos) {
    // ok this is a soft page break block
    res->m_type = 5;
    res->m_page = res->m_fileBlock;
    res->m_fileBlock = 0;
  }
  val =  m_input->readULong(1);
  if (val) f << "g5=" << std::hex << val << std::dec << ",";
  f << "unkn3=[";
  for (int i = 0; i < 6; i++) { // can be 0*12, cc*12 : junk ?
    val = m_input->readULong(2);
    if (val==0) f << "_,";
    else f << std::hex << val << std::dec << ",";
  }
  f << "],";
  res->m_extra=f.str();
  f.str("");
  f << "Entries(Block)[" << wh << "]:" << *res;

  ascii().addDelimiter(m_input->tell(), '|');
  m_input->seek(endPos, WPX_SEEK_SET);
  while (m_input->readLong(2)==0 && !m_input->atEOS()) {}
  m_input->seek(-2, WPX_SEEK_CUR);
  if (m_input->readLong(1)) m_input->seek(-1, WPX_SEEK_CUR);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return res;
}

shared_ptr<MWProStructuresInternal::Block> MWProStructures::readBlock()
{
  long pos = m_input->tell();
  libmwaw::DebugStream f;

  long sz = m_input->readULong(4);
  // pat2*3?, dim[pt*65536], border[pt*65536], ?, [0|10|1c], 0, block?
  if (sz < 0x40) {
    m_input->seek(pos, WPX_SEEK_SET);
    return shared_ptr<MWProStructuresInternal::Block>();
  }

  long endPos = pos+sz+4;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    m_input->seek(pos, WPX_SEEK_SET);
    return shared_ptr<MWProStructuresInternal::Block>();
  }
  m_input->seek(pos+4, WPX_SEEK_SET);

  shared_ptr<MWProStructuresInternal::Block> block(new MWProStructuresInternal::Block);
  long val;
  /* [4-8]*3 or 271027100003, 277427740004, 2af82af80004 */
  f << "pat?=[" << std::hex;
  for (int i = 0; i < 2; i++)
    f << m_input->readULong(2) << ",";
  f << std::dec << "],";
  block->m_type = m_input->readULong(2);
  float dim[4];
  for (int i = 0; i < 4; i++)
    dim[i] = m_input->readLong(4)/65536.;
  block->m_box = Box2f(Vec2f(dim[1],dim[0]), Vec2f(dim[3],dim[2]));
  float border[4];
  for (int i = 0; i < 4; i++)
    border[i]=m_input->readLong(4)/65536.;
  // check me
  block->m_border[0] = Vec2f(border[1], border[0]);
  block->m_border[1] = Vec2f(border[3], border[2]);
  /* 4: pagebreak,
     5: text
     1: floating, 7: none(wrapping/attachment), b: attachment
     0/a: table ?
  */
  for (int i = 0; i < 2; i++) {
    val = m_input->readULong(2);
    if (val)
      f << "fl" << i << "=" << std::hex << val << std::dec << ",";
  }
  val = m_input->readLong(2);
  if (val) f << "f0=" << val << ",";
  block->m_fileBlock = m_input->readLong(2);
  block->m_id = m_input->readLong(2);
  val = m_input->readLong(2); // almost always 4 ( one time 0)
  if (val!=4)
    f << "f1=" << val << ",";
  for (int i = 2; i < 7; i++) {
    /* always 0, except f3=-1 (in one file),
       and in other file f4=1,f5=1,f6=1, */
    val = m_input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  block->m_baseline = m_input->readLong(4)/65536.;
  int colorId = m_input->readLong(2);
  if (colorId) {
    Vec3uc col;
    if (getColor(colorId, col)) {
      for (int i = 0; i < 3; i++) block->m_color[i] = col[i];
    } else
      f << "#colorId=" << colorId << ",";
  }

  val = m_input->readLong(2);/* 2: normal, 1 transparent  ?*/
  if (val) f << "g0=" << val << ",";
  colorId = m_input->readLong(2);
  if (colorId!=1) {
    Vec3uc col;
    if (getColor(colorId, col)) {
      for (int i = 0; i < 3; i++) block->m_lineColor[i] = col[i];
    } else
      f << "#lineColorId=" << colorId << ",";
  }
  block->m_linePattern = m_input->readLong(2);
  val =  m_input->readLong(2);
  switch(val) {
  case 1:
    block->m_lineWidth = 0.0;
    break;
  case 2:
    block->m_lineWidth = 0.25;
    break; // hairline
  case 3:
    block->m_lineWidth = 1.;
    break;
  case 4:
    block->m_lineWidth = 2.;
    break;
  case 5:
    block->m_lineWidth = 4.;
    break;
  case 6:
    block->m_lineWidth = 6.;
    break;
  case 7:
    block->m_lineWidth = 8.;
    break;
  case 8:
    block->m_lineWidth = 10.;
    break;
  case 9:
    block->m_lineWidth = 12.;
    break;
  default:
    f << "#lineWidth=" << val << ",";
    break;
  }
  block->m_lineType = m_input->readLong(2);
  int contentType = m_input->readULong(1);
  switch(contentType) {
  case 0:
    block->m_contentType = MWProStructuresInternal::Block::TEXT;
    break;
  case 1:
    block->m_contentType = MWProStructuresInternal::Block::GRAPHIC;
    break;
  default:
    MWAW_DEBUG_MSG(("MWProStructures::readBlock: find unknown block content type\n"));
    f << "#contentType=" << contentType << ",";
    break;
  }
  block->m_extra = f.str();

  f.str("");
  f << "Block-data(" << m_state->m_blocksList.size() << "):" << *block;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  if (long(m_input->tell()) != endPos)
    ascii().addDelimiter(m_input->tell(), '|');

  m_input->seek(endPos, WPX_SEEK_SET);

  // ok now read the end of the header
  pos = m_input->tell();
  sz = getEndBlockSize();
  if (sz) {
    f.str("");
    f << "Block-end(" << m_state->m_blocksList.size()<< ")[" << block->m_type << "]:";
    switch(block->m_type) {
    case 3: { // table
      block->m_row = m_input->readLong(2);
      block->m_col = m_input->readLong(2);
      f << "numRow=" << block->m_row << ",";
      f << "numCol=" << block->m_col << ",";
      break;
    }
    case 4: { // cell/textbox : no sure it contain data?
      val =  m_input->readLong(2); // always 0 ?
      if (val) f << "f0=" << val << ",";
      val = m_input->readULong(2); // [0|10|1e|10c0|1cc0|a78a|a7a6|d0c0|dcc0]
      if (val) f << "fl?=" << std::hex << val << std::dec << ",";
      break;
    }
    case 5: { // text or ?
      bool emptyBlock = block->m_fileBlock <= 0;
      val =  m_input->readULong(2); // always 0 ?
      if (emptyBlock) {
        if (val & 0xFF00)
          f << "#f0=" << val << ",";
        block->m_textPos=((val&0xFF)<<16) | m_input->readULong(2);
        f << "posC=" << block->m_textPos << ",";
      } else if (val) f << "f0=" << val << ",";
      val = m_input->readULong(2); // 30c0[normal], 20c0|0[empty]
      f << "fl?=" << std::hex << val << ",";
      break;
    }
    case 6: {
      for (int i = 0; i < 4; i++) { // [10|d0],40, 0, 0
        val =  m_input->readULong(1);
        f << "f" << i << "=" << val << ",";
      }
      val =  m_input->readLong(1);
      switch(val) {
      case 1:
        f << "header,";
        block->m_isHeader = true;
        break;
      case 2:
        f << "footer,";
        block->m_isHeader = false;
        break;
      default:
        MWAW_DEBUG_MSG(("MWProStructures::readBlock: find unknown header/footer type\n"));
        f << "#type=" << val << ",";
        break;
      }
      val =  m_input->readLong(1); // alway 1 ?
      if (val != 1) f << "f4=" << val << ",";
      break;
    }
    case 7: { // footnote: something here ?
      for (int i = 0; i < 3; i++) { // 0, 0, [0|4000]
        val =  m_input->readULong(2);
        f << "f" << i << "=" << std::hex << val << std::dec << ",";
      }
      break;
    }
    case 8:
      break; // graphic: clearly nothing
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+sz, WPX_SEEK_SET);
  }

  return block;
}

////////////////////////////////////////////////////////////
// read the column information zone : checkme
bool MWProStructures::readSections(std::vector<MWProStructuresInternal::Section> &sections)
{
  long pos = m_input->tell();
  libmwaw::DebugStream f;

  long sz = m_input->readULong(4);
  if (sz == 0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }
  long endPos = pos+4+sz;
  if ((sz%0xd8)) {
    MWAW_DEBUG_MSG(("MWProStructures::readSections: find an odd value for sz\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(Sections)#");
    m_input->seek(endPos, WPX_SEEK_SET);
    return true;
  }

  int N = sz/0xd8;
  f << "Entries(Section):";
  f << "N=" << N;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  long val;
  for (int n = 0; n < N; n++) {
    MWProStructuresInternal::Section sec;
    pos = m_input->tell();
    f.str("");
    sec.m_textLength = m_input->readULong(4);
    val =  m_input->readLong(4); // almost always 0 or a dim?
    if (val) f << "dim?=" << val/65536. << ",";
    int startWay = m_input->readLong(2);
    switch(startWay) {
    case 1:
      sec.m_start = sec.S_Line;
      break;
    case 2:
      sec.m_start = sec.S_Page;
      break;
    case 3:
      sec.m_start = sec.S_PageLeft;
      break;
    case 4:
      sec.m_start = sec.S_PageRight;
      break;
    default:
      MWAW_DEBUG_MSG(("MWProStructures::readSections: find an odd value for start\n"));
      f << "#start=" << startWay << ",";
    }
    val = m_input->readLong(2);
    if (val)
      f << "f0=" << val << ",";
    // a flag ? and noused ?
    for (int i = 0; i < 2; i++) {
      val = m_input->readULong(1);
      if (val == 0xFF) f << "fl" << i<< "=true,";
      else if (val) f << "fl" << i << "=" << std::hex << val << std::dec << ",";
    }

    for (int st = 0; st < 2; st++) {
      val = m_input->readLong(2); // alway 1 ?
      if (val != 1) f << "f" << 1+st << "=" << val << ",";
      // another flag ?
      val = m_input->readULong(1);
      if (val) f << "fl" << st+2 << "=" << std::hex << val << std::dec << ",";
    }
    int numColumns = m_input->readLong(2);
    if (numColumns < 1 || numColumns > 20) {
      MWAW_DEBUG_MSG(("MWProStructures::readSections: bad number of columns\n"));
      f << "#nCol=" << numColumns << ",";
      numColumns = 1;
    }
    val = m_input->readLong(2); // find: 3, c, 24
    if (val) f << "f3=" << val << ",";
    for (int i = 4; i < 7; i++) { // always 0 ?
      val = m_input->readLong(2);
      if (val)
        f << "f" << i << "=" << val << ",";
    }
    long actPos = m_input->tell();
    float prevPos = 0;
    f << "rightPos=[";
    for (int c = 0; c < numColumns; c++) {
      float rightPos = m_input->readLong(4)/65536.;
      f << rightPos << ",";
      sec.m_colsBegin.push_back(prevPos);
      float leftPos = m_input->readLong(4)/65536.;
      sec.m_colsWidth.push_back(leftPos-prevPos);
      prevPos = leftPos;
    }
    f << "],";
    m_input->seek(actPos+20*8+4, WPX_SEEK_SET);
    // 5 flags ( 1+unused?)
    for (int i = 0; i < 6; i++) {
      val = m_input->readULong(1);
      if ((i!=5 && val!=1) || (i==5 && val))
        f << "g" << i << "=" << val << ",";
    }
    for (int st = 0; st < 2; st++) { // pair, unpair?
      for (int i = 0; i < 2; i++) { // header/footer
        val = m_input->readLong(2);
        if (val)
          f << "#h" << 2*st+i << "=" << val << ",";

        val = m_input->readLong(2);
        if (i==0) sec.m_headerIds[st] = val;
        else sec.m_footerIds[st] = val;
      }
    }
    sec.m_extra=f.str();
    sections.push_back(sec);

    f.str("");
    f << "Section" << "-" << n << ":" << sec;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+0xd8, WPX_SEEK_SET);
  }

  m_input->seek(endPos, WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// read the selection zone
bool MWProStructures::readSelection()
{
  long pos = m_input->tell();
  libmwaw::DebugStream f;

  long endPos = pos+14;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("MWProStructures::readSelection: file is too short\n"));
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  m_input->seek(pos, WPX_SEEK_SET);
  f << "Entries(Selection):";
  int val = m_input->readLong(2);
  f << "f0=" << val << ","; // zone?
  val = m_input->readLong(4); // -1, 0 or 8 : zone type?
  if (val == -1 || val == 0) { // checkme: none ?
    f << "*";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+6, WPX_SEEK_SET);
    return true;
  }
  if (val!=8) f << "f1=" << val << ",";
  f << "char=";
  for (int i = 0; i < 2; i++) {
    f << m_input->readULong(4);
    if (i==0) f << "x";
    else f << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  m_input->seek(endPos, WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// read a string
bool MWProStructures::readString(MWAWInputStreamPtr input, std::string &res)
{
  res="";
  long pos = input->tell();
  int sz = input->readLong(2);
  if (sz == 0) return true;
  if (sz < 0) {
    input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("MWProStructures::readString: odd value for size\n"));
    return false;
  }
  input->seek(pos+sz+2, WPX_SEEK_SET);
  if (long(input->tell())!=pos+sz+2) {
    input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("MWProStructures::readString: file is too short\n"));
    return false;
  }
  input->seek(pos+2, WPX_SEEK_SET);
  for (int i= 0; i < sz; i++) {
    char c = input->readULong(1);
    if (c) {
      res+=c;
      continue;
    }
    if (i==sz-1) break;

    input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("MWProStructures::readString: find odd character in string\n"));
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////
// read an unknown zone
bool MWProStructures::readStructB()
{
  long pos = m_input->tell();
  libmwaw::DebugStream f;

  int N = m_input->readULong(2);
  if (N==0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }
  f << "Entries(StructB):N=" << N << ",";

  // CHECKME: find N=2 only one time ( and across a checksum zone ...)
  long endPos = pos+N*10+6;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("MWProStructures::readZonB: file is too short\n"));
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  m_input->seek(pos+2, WPX_SEEK_SET);
  int val = m_input->readULong(2);
  if (val != 0x2af8)
    f << "f0=" << std::hex << val << std::dec << ",";
  val = m_input->readULong(2);
  if (val) f << "f1=" << val << ",";

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int n = 0; n < N; n++) {
    pos = m_input->tell();
    f.str("");
    f << "StructB" << "-" << n;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+10, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// check if a block is sent
bool MWProStructures::isSent(int blockId)
{
  if (version()==0) {
    if (blockId < 0 || blockId >= int(m_state->m_blocksList.size())) {
      MWAW_DEBUG_MSG(("MWProStructures::send: can not find the block %d\n", blockId));
      return false;
    }
    return m_state->m_blocksList[blockId]->m_send;
  }

  if (m_state->m_blocksMap.find(blockId) == m_state->m_blocksMap.end()) {
    MWAW_DEBUG_MSG(("MWProStructures::isSent: can not find the block %d\n", blockId));
    return true;
  }
  return m_state->m_blocksMap.find(blockId)->second->m_send;
}

////////////////////////////////////////////////////////////
// send a block
bool MWProStructures::send(int blockId, bool mainZone)
{
  shared_ptr<MWProStructuresInternal::Block> block;
  if (version()==0) {
    if (blockId < 0) {
      if (-blockId > int(m_state->m_footnotesList.size())) {
        MWAW_DEBUG_MSG(("MWProStructures::send: can not find the footnote %d\n", -blockId));
        return false;
      }
      block = m_state->m_blocksList[m_state->m_footnotesList[-blockId-1]];
    } else {
      if (blockId < 0 || blockId >= int(m_state->m_blocksList.size())) {
        MWAW_DEBUG_MSG(("MWProStructures::send: can not find the block %d\n", blockId));
        return false;
      }
      block = m_state->m_blocksList[blockId];
    }
  } else {
    if (m_state->m_blocksMap.find(blockId) == m_state->m_blocksMap.end()) {
      MWAW_DEBUG_MSG(("MWProStructures::send: can not find the block %d\n", blockId));
      return false;
    }
    block = m_state->m_blocksMap.find(blockId)->second;
  }
  block->m_send = true;
  if (block->m_type == 4 && block->m_textboxCellType == 0) {
    block->m_textboxCellType = 2;
    WPXPropertyList extras;
    block->fillFramePropertyList(extras);
    m_mainParser.sendTextBoxZone(blockId, block->getPosition(), extras);
    block->m_textboxCellType = 0;
  } else if (block->isText())
    m_mainParser.sendTextZone(block->m_fileBlock, mainZone);
  else if (block->isGraphic()) {
    WPXPropertyList extras;
    block->fillFramePropertyList(extras);
    m_mainParser.sendPictureZone(block->m_fileBlock, block->getPosition(), extras);
  } else if (block->m_type == 3) {
    if (m_state->m_tablesMap.find(blockId) == m_state->m_tablesMap.end()) {
      MWAW_DEBUG_MSG(("MWProStructures::send: can not find table with id=%d\n", blockId));
    } else {
      bool needTextBox = m_listener && !block->m_attachment && block->m_textboxCellType == 0;
      if (needTextBox) {
        block->m_textboxCellType = 2;
        m_mainParser.sendTextBoxZone(blockId, block->getPosition());
      } else {
        shared_ptr<MWProStructuresInternal::Table> table =
          m_state->m_tablesMap.find(blockId)->second;
        if (!table->sendTable(m_listener))
          table->sendAsText(m_listener);
        block->m_textboxCellType = 0;
      }
    }
  } else if (block->m_type == 4 || block->m_type == 6) {
    // probably ok, can be an empty cell, textbox, header/footer ..
    if (m_listener) m_listener->insertCharacter(' ');
  } else if (block->m_type == 8) { // empty frame
    WPXPropertyList extras;
    block->fillFramePropertyList(extras);
    m_mainParser.sendEmptyFrameZone(block->getPosition(), extras);
  } else {
    MWAW_DEBUG_MSG(("MWProStructures::send: can not send block with type=%d\n", block->m_type));
  }
  return true;
}

////////////////////////////////////////////////////////////
// send the not sent data
void MWProStructures::flushExtra()
{
  int vers = version();
  if (m_listener && m_listener->isSectionOpened()) {
    m_listener->closeSection();
    m_listener->openSection();
  }
  // first send the text
  for (int i = 0; i < int(m_state->m_blocksList.size()); i++) {
    if (m_state->m_blocksList[i]->m_send)
      continue;
    if (m_state->m_blocksList[i]->m_type == 6) {
      /* Fixme: macwritepro can have one header/footer by page and one by default.
         For the moment, we only print the first one :-~ */
      MWAW_DEBUG_MSG(("MWProStructures::flushExtra: find some header/footer\n"));
      continue;
    }
    int id = vers == 0 ? i : m_state->m_blocksList[i]->m_id;
    if (m_state->m_blocksList[i]->isText()) {
      // force to non floating position
      m_state->m_blocksList[i]->m_attachment = true;
      send(id);
      if (m_listener) m_listener->insertEOL();
    } else if (m_state->m_blocksList[i]->m_type == 3) {
      // force to non floating position
      m_state->m_blocksList[i]->m_attachment = true;
      send(id);
    }
  }
  // then send graphic
  for (int i = 0; i < int(m_state->m_blocksList.size()); i++) {
    if (m_state->m_blocksList[i]->m_send)
      continue;
    if (m_state->m_blocksList[i]->isGraphic()) {
      // force to non floating position
      m_state->m_blocksList[i]->m_attachment = true;
      send(m_state->m_blocksList[i]->m_id);
    }
  }
}

////////////////////////////////////////////////////////////
// interface with the listener
MWProStructuresListenerState::MWProStructuresListenerState(shared_ptr<MWProStructures> structures, bool mainZone)
  : m_isMainZone(mainZone), m_actPage(0), m_actTab(0), m_numTab(0),
    m_section(0), m_numCols(1), m_newPageDone(false), m_structures(structures),
    m_font(new MWProStructuresInternal::Font),
    m_paragraph(new MWProStructuresInternal::Paragraph)
{
  if (!m_structures) {
    MWAW_DEBUG_MSG(("MWProStructuresListenerState::MWProStructuresListenerState can not find structures parser\n"));
    return;
  }
  if (mainZone) {
    newPage();
    sendSection(0);
  }
}

MWProStructuresListenerState::~MWProStructuresListenerState()
{
}

bool MWProStructuresListenerState::isSent(int blockId)
{
  if (!m_structures) return false;
  return m_structures->isSent(blockId);
}

bool MWProStructuresListenerState::send(int blockId)
{
  m_newPageDone = false;
  if (!m_structures) return false;
  return m_structures->send(blockId);
}

void MWProStructuresListenerState::insertSoftPageBreak()
{
  if (m_newPageDone) return;
  newPage(true);
}

bool MWProStructuresListenerState::newPage(bool softBreak)
{
  if (!m_structures || !m_isMainZone) {
    MWAW_DEBUG_MSG(("MWProStructuresListenerState::newPage: can not create a new page\n"));
    return false;
  }

  // first send all the floating data
  if (m_actPage == 0) {
    for (int i = 0; i < int(m_structures->m_state->m_blocksList.size()); i++) {
      shared_ptr<MWProStructuresInternal::Block> block = m_structures->m_state->m_blocksList[i];
      if (block->m_send || block->m_attachment) continue;
      if (block->m_type != 3 && block->m_type != 4 && block->m_type != 8) continue;
      m_structures->send(block->m_id);
    }
  }

  m_structures->m_mainParser.newPage(++m_actPage, softBreak);
  m_actTab=0;
  m_newPageDone = true;

  return true;
}

std::vector<int> MWProStructuresListenerState::getPageBreaksPos() const
{
  std::vector<int> res;
  if (!m_structures || !m_isMainZone) return res;
  for (int i = 0; i < int(m_structures->m_state->m_blocksList.size()); i++) {
    shared_ptr<MWProStructuresInternal::Block> block = m_structures->m_state->m_blocksList[i];
    if (block->m_type != 5) continue;
    if (block->m_textPos) res.push_back(block->m_textPos);
  }
  return res;
}

// ----------- character function ---------------------
void MWProStructuresListenerState::sendChar(char c)
{
  bool newPageDone = m_newPageDone;
  m_newPageDone = false;
  if (!m_structures || !m_structures->m_listener)
    return;
  switch(c) {
  case 0:
    break; // ignore
  case 3: // footnote ok
  case 4: // figure ok
  case 5: // hyphen ok
    break;
  case 7:
    if (m_structures->version()==0) {
      m_actTab = 0;
      m_structures->m_listener->insertEOL(true);
    } else {
      MWAW_DEBUG_MSG(("MWProStructuresListenerState::sendChar: Find odd char 0x7\n"));
    }
    break;
  case 0x9:
    if (m_actTab++ < m_numTab)
      m_structures->m_listener->insertTab();
    else
      m_structures->m_listener->insertCharacter(' ');
    break;
  case 0xa:
    m_actTab = 0;
    if (newPageDone) break;
    m_structures->m_listener->insertEOL();
    break; // soft break
  case 0xd:
    m_actTab = 0;
    if (newPageDone) break;
    m_structures->m_listener->insertEOL();
    sendParagraph(*m_paragraph);
    break;
  case 0xc:
    m_actTab = 0;
    if (m_isMainZone) newPage();
    break;
  case 0xb: // add a columnbreak
    m_actTab = 0;
    if (m_isMainZone) {
      if (m_numCols <= 1) newPage();
      else if (m_structures->m_listener)
        m_structures->m_listener->insertBreak(DMWAW_COLUMN_BREAK);
    }
    break;
  case 0xe:
    m_actTab = 0;
    if (!m_isMainZone) break;

    // create a new section here
    if (m_structures->m_listener->isSectionOpened())
      m_structures->m_listener->closeSection();
    sendSection(++m_section);
    break;
  case 2: // for MWII
  case 0x15:
  case 0x17:
  case 0x1a:
    break;
  case 0x1f: // some hyphen
    break;
    /* 0x10 and 0x13 : seems also to have some meaning ( replaced by 1 in on field )*/
  default: {
    int unicode = m_structures->m_convertissor->getUnicode (m_font->m_font,c);
    if (unicode == -1) {
      if (c < 32) {
        MWAW_DEBUG_MSG(("MWProStructuresListenerState::sendChar: Find odd char %x\n", int(c)));
      } else
        m_structures->m_listener->insertCharacter(c);
    } else
      m_structures->m_listener->insertUnicode(unicode);
    break;
  }
  }
}

bool MWProStructuresListenerState::resendAll()
{
  sendParagraph(*m_paragraph);
  sendFont(*m_font, true);
  return true;
}


// ----------- font function ---------------------
bool MWProStructuresListenerState::sendFont(int id, bool force)
{
  if (!m_structures) return false;
  if (!m_structures->m_listener) return true;
  if (id < 0 || id >= int(m_structures->m_state->m_fontsList.size())) {
    MWAW_DEBUG_MSG(("MWProStructuresListenerState::sendFont: can not find font %d\n", id));
    return false;
  }
  sendFont(m_structures->m_state->m_fontsList[id], force);

  return true;
}

void MWProStructuresListenerState::sendFont(MWProStructuresInternal::Font const &font, bool force)
{
  if (!m_structures || !m_structures->m_listener)
    return;

  font.m_font.sendTo(m_structures->m_listener.get(), m_structures->m_convertissor, m_font->m_font, force);
  *m_font = font;
  switch(m_font->m_language) {
  case -1:
    break;
  case 0:
    m_structures->m_listener->setTextLanguage("en_US");
    break;
  case 2:
    m_structures->m_listener->setTextLanguage("en_GB");
    break;
  case 3:
    m_structures->m_listener->setTextLanguage("de");
    break;
  default:
    break;
  }
}

std::string MWProStructuresListenerState::getFontDebugString(int fId)
{
  if (!m_structures) {
    MWAW_DEBUG_MSG(("MWProStructuresListenerState::getFontDebugString: can not find structures\n"));
    return "";
  }

  std::stringstream s;
  if (fId < 0 || fId >= int(m_structures->m_state->m_fontsList.size())) {
    MWAW_DEBUG_MSG(("MWProStructuresListenerState::getFontDebugString: can not find font %d\n", fId));
    s << "C" << fId << "(unknown##)";
    return s.str();
  }

  s << "C" << fId << ":";
  s << m_structures->m_convertissor->getFontDebugString
    (m_structures->m_state->m_fontsList[fId].m_font)
    << m_structures->m_state->m_fontsList[fId] << ",";

  return s.str();
}

// ----------- paragraph function ---------------------
bool MWProStructuresListenerState::sendParagraph(int id)
{
  if (!m_structures) return false;
  if (!m_structures->m_listener) return true;
  if (id < 0 || id >= int(m_structures->m_state->m_paragraphsList.size())) {
    MWAW_DEBUG_MSG(("MWProStructuresListenerState::sendParagraph: can not find paragraph %d\n", id));
    return false;
  }

  sendParagraph(m_structures->m_state->m_paragraphsList[id]);
  return true;
}

void MWProStructuresListenerState::sendParagraph(MWProStructuresInternal::Paragraph const &para)
{
  if (!m_structures || !m_structures->m_listener)
    return;
  *m_paragraph = para;

  m_structures->m_listener->justificationChange(para.m_justify);

  m_structures->m_listener->setParagraphTextIndent(para.m_margins[0]);
  m_structures->m_listener->setParagraphMargin(para.m_margins[1], DMWAW_LEFT);
  m_structures->m_listener->setParagraphMargin(para.m_margins[2], DMWAW_RIGHT);

  if (para.m_spacing[0] < 1)
    m_structures->m_listener->lineSpacingChange(1.0, WPX_PERCENT);
  else
    m_structures->m_listener->lineSpacingChange
    (para.m_spacing[0], para.m_spacingPercent[0] ? WPX_PERCENT: WPX_INCH);

  for (int sp = 1; sp < 3; sp++) {
    double val = para.m_spacing[sp];
    if (val < 0)
      val = 0;
    /** seems difficult to set bottom a percentage of the line unit,
        so do the strict minimum... */
    else if (para.m_spacingPercent[sp])
      val = (val*7.)/72.;
    m_structures->m_listener->setParagraphMargin
    (val, sp==1 ? DMWAW_TOP : DMWAW_BOTTOM, WPX_INCH);
  }
  m_numTab = para.m_tabs.size();
  m_structures->m_listener->setTabs(para.m_tabs);
}


std::string MWProStructuresListenerState::getParagraphDebugString(int pId)
{
  if (!m_structures) return "";

  std::stringstream s;
  if (pId < 0 || pId >= int(m_structures->m_state->m_paragraphsList.size())) {
    MWAW_DEBUG_MSG(("MWProStructuresListenerState::getParagraphDebugString: can not find paragraph %d\n", pId));
    s << "C" << pId << "(unknown##)";
    return s.str();
  }

  s << "P" << pId << ":";
  s << m_structures->m_state->m_paragraphsList[pId] << ",";
  return s.str();
}

// ----------- section function ---------------------
void MWProStructuresListenerState::sendSection(int numSection)
{
  if (!m_structures || !m_structures->m_listener) return;
  if (m_structures->m_listener->isSectionOpened()) {
    MWAW_DEBUG_MSG(("MWProStructuresListenerState::sendSection: a section is already opened\n"));
    m_structures->m_listener->closeSection();
  }
  if (m_structures->version()==0) {
    m_numCols = m_structures->m_mainParser.numColumns();
    if (m_numCols > 10) {
      MWAW_DEBUG_MSG(("MWProStructuresListenerState::sendSection: num columns is to big, reset to 1\n"));
      m_numCols = 1;
    }
    if (m_numCols==1) m_structures->m_listener->openSection();
    else {
      std::vector<int> colSize;
      float colWidth =  72.*m_structures->m_mainParser.pageWidth()/m_numCols;
      colSize.resize(m_numCols);
      for (int i = 0; i < m_numCols; i++) colSize[i] = int(colWidth);
      m_structures->m_listener->openSection(colSize, WPX_POINT);
    }
    return;
  }

  if (numSection >= int(m_structures->m_state->m_sectionsList.size())) {
    MWAW_DEBUG_MSG(("MWProStructuresListenerState::sendSection: can not find section %d\n", numSection));
    return;
  }
  MWProStructuresInternal::Section const &section =
    m_structures->m_state->m_sectionsList[numSection];
  if (numSection && section.m_start != section.S_Line) newPage();
  m_numCols = section.numColumns();
  if (m_numCols==1) m_structures->m_listener->openSection();
  else {
    std::vector<int> colSize;
    colSize.resize(m_numCols);
    for (int i = 0; i < m_numCols; i++) colSize[i] = int(section. m_colsWidth[i]);
    m_structures->m_listener->openSection(colSize, WPX_POINT);
  }
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
