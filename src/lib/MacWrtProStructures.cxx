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

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWCell.hxx"
#include "MWAWTextListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSection.hxx"
#include "MWAWTable.hxx"

#include "MacWrtProStructures.hxx"
#include "MacWrtProParser.hxx"

/** Internal: the structures of a MacWrtProStructures */
namespace MacWrtProStructuresInternal
{
////////////////////////////////////////
//! Internal: the data block
struct Block {
  enum Type { UNKNOWN, GRAPHIC, TEXT, NOTE };
  //! the constructor
  Block() :m_type(-1), m_contentType(UNKNOWN), m_fileBlock(0), m_id(-1), m_attachment(false), m_page(-1), m_box(),
    m_baseline(0.), m_surfaceColor(MWAWColor::white()), m_lineBorder(), m_textPos(0), m_isHeader(false),
    m_row(0), m_col(0), m_textboxCellType(0), m_extra(""), m_send(false)
  {
    for (int i=0; i < 4; ++i) {
      m_borderWList[i]=0;
      m_borderCellList[i]=MWAWBorder();
    }
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Block const &bl)
  {
    switch (bl.m_contentType) {
    case GRAPHIC:
      o << "graphic,";
      if (bl.m_type != 8) {
        MWAW_DEBUG_MSG(("MacWrtProStructuresInternal::Block::operator<< unknown type\n"));
        o << "#type=" << bl.m_type << ",";
      }
      break;
    case NOTE:
      o << "note";
      break;
    case TEXT:
      o << "text";
      switch (bl.m_type) {
      case 3:
        o << "[table]";
        break;
      case 4:
        o << "[textbox/cell/note]";
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
        MWAW_DEBUG_MSG(("MacWrtProStructuresInternal::Block::operator<< unknown type\n"));
        o << "[#" << bl.m_type << "]";
        break;
      }
      o << ",";
      break;
    case UNKNOWN:
    default:
      break;
    }
    if (bl.m_id >= 0) o << "id=" << bl.m_id << ",";
    o << "box=" << bl.m_box << ",";
    static char const *(wh[]) = { "L", "R", "T", "B" };
    if (bl.hasSameBorders()) {
      if (bl.m_borderWList[0] > 0)
        o << "bord[width]=" << bl.m_borderWList[0] << ",";
    }
    else {
      for (int i = 0; i < 4; ++i) {
        if (bl.m_borderWList[i] <= 0)
          continue;
        o << "bord" << wh[i] << "[width]=" << bl.m_borderWList[i] << ",";
      }
    }
    if (bl.m_contentType==TEXT && bl.m_type==4) {
      for (int i = 0; i < 4; ++i)
        o << "bord" << wh[i] << "[cell]=[" << bl.m_borderCellList[i] << "],";
    }
    if (bl.m_baseline < 0 || bl.m_baseline >0) o << "baseline=" << bl.m_baseline << ",";
    if (!bl.m_surfaceColor.isWhite())
      o << "col=" << bl.m_surfaceColor << ",";
    if (!bl.m_lineBorder.isEmpty())
      o << "line=" << bl.m_lineBorder << ",";
    if (bl.m_fileBlock > 0) o << "block=" << std::hex << bl.m_fileBlock << std::dec << ",";
    if (bl.m_extra.length())
      o << bl.m_extra << ",";
    return o;
  }
  void fillFramePropertyList(librevenge::RVNGPropertyList &extra) const
  {
    if (!m_surfaceColor.isWhite())
      extra.insert("fo:background-color", m_surfaceColor.str().c_str());
    if (!hasBorders())
      return;
    bool setAll = hasSameBorders();
    for (int w=0; w < 4; ++w) {
      if (w && setAll) break;
      MWAWBorder border(m_lineBorder);
      border.m_width=m_borderWList[w]; // ok also for setAll
      if (border.isEmpty())
        continue;
      static char const *(wh[]) = { "left", "right", "top", "bottom" };
      if (setAll)
        border.addTo(extra);
      else
        border.addTo(extra,wh[w]);
    }
  }

  bool isGraphic() const
  {
    return m_fileBlock > 0 && m_contentType == GRAPHIC;
  }
  bool isText() const
  {
    return m_fileBlock > 0 && (m_contentType == TEXT || m_contentType == NOTE);
  }
  bool isTable() const
  {
    return m_fileBlock <= 0 && m_type == 3;
  }
  bool hasSameBorders() const
  {
    for (int i=1; i < 4; ++i) {
      if (m_borderWList[i] > m_borderWList[0] ||
          m_borderWList[i] < m_borderWList[0])
        return false;
    }
    return true;
  }
  bool hasBorders() const
  {
    if (m_lineBorder.m_color.isWhite() || m_lineBorder.isEmpty())
      return false;
    for (int i=0; i < 4; ++i) {
      if (m_borderWList[i] > 0)
        return true;
    }
    return false;
  }

  MWAWPosition getPosition() const
  {
    MWAWPosition res;
    if (m_attachment) {
      res = MWAWPosition(Vec2i(0,0), m_box.size(), librevenge::RVNG_POINT);
      res.setRelativePosition(MWAWPosition::Char, MWAWPosition::XLeft, getRelativeYPos());
    }
    else {
      res = MWAWPosition(m_box.min(), m_box.size(), librevenge::RVNG_POINT);
      res.setRelativePosition(MWAWPosition::Page);
      res.setPage(m_page);
      res.m_wrapping = m_contentType==NOTE ? MWAWPosition::WRunThrough :
                       MWAWPosition::WDynamic;
    }
    return res;
  }

  MWAWPosition::YPos getRelativeYPos() const
  {
    float height = m_box.size()[1];
    if (m_baseline < 0.25*height) return MWAWPosition::YBottom;
    if (m_baseline < 0.75*height) return MWAWPosition::YCenter;
    return MWAWPosition::YTop;
  }
  bool contains(Box2f const &box) const
  {
    return box[0][0] >= m_box[0][0] && box[0][1] >= m_box[0][1] &&
           box[1][0] <= m_box[1][0] && box[1][1] <= m_box[1][1];
  }
  bool intersects(Box2f const &box) const
  {
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

  //! the borders width
  double m_borderWList[4];

  //! the cell borders
  MWAWBorder m_borderCellList[4];

  //! the baseline ( in point 0=bottom aligned)
  float m_baseline;

  //! the background color
  MWAWColor m_surfaceColor;

  //! the line border
  MWAWBorder m_lineBorder;

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
  Font(): m_font(), m_flags(0), m_token(-1)
  {
    for (int i = 0; i < 5; ++i) m_values[i] = 0;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Font const &font)
  {
    if (font.m_flags) o << "flags=" << std::hex << font.m_flags << std::dec << ",";
    for (int i = 0; i < 5; ++i) {
      if (!font.m_values[i]) continue;
      o << "f" << i << "=" << font.m_values[i] << ",";
    }
    switch (font.m_token) {
    case -1:
      break;
    default:
      o << "token=" << font.m_token << ",";
      break;
    }
    return o;
  }

  //! the font
  MWAWFont m_font;
  //! some unknown flag
  int m_flags;
  //! the token type(checkme)
  int m_token;
  //! unknown values
  int m_values[5];
};

////////////////////////////////////////
/** Internal: class to store the paragraph properties */
struct Paragraph : public MWAWParagraph {
  //! Constructor
  Paragraph() :  m_value(0)
  {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Paragraph const &ind)
  {
    o << reinterpret_cast<MWAWParagraph const &>(ind);
    if (ind.m_value) o << "unkn=" << ind.m_value << ",";
    return o;
  }
  //! a unknown value
  int m_value;
};

////////////////////////////////////////
//! Internal: the cell of a MacWrtProStructure
struct Cell : public MWAWCell {
  //! constructor
  Cell(MacWrtProStructures &parser, Block *block) : MWAWCell(), m_parser(parser), m_blockId(0)
  {
    if (!block) return;
    setBdBox(Box2f(block->m_box.min(), block->m_box.max()-Vec2f(1,1)));
    setBackgroundColor(block->m_surfaceColor);
    m_blockId = block->m_id;
    for (int b=0; b<4; ++b) {
      int const(wh[4]) = { libmwaw::LeftBit, libmwaw::RightBit,
                           libmwaw::TopBit, libmwaw::BottomBit
                         };
      setBorders(wh[b], block->m_borderCellList[b]);
    }
  }

  //! send the content
  bool sendContent(MWAWListenerPtr listener, MWAWTable &)
  {
    if (m_blockId > 0)
      m_parser.send(m_blockId);
    else if (listener) // try to avoid empty cell
      listener->insertChar(' ');
    return true;
  }

  //! the text parser
  MacWrtProStructures &m_parser;
  //! the block id
  int m_blockId;
};

////////////////////////////////////////
////////////////////////////////////////
struct Table : public MWAWTable {
  //! constructor
  Table() : MWAWTable()
  {
  }

  //! return a cell corresponding to id
  Cell *get(int id)
  {
    if (id < 0 || id >= numCells()) {
      MWAW_DEBUG_MSG(("MacWrtProStructuresInternal::Table::get: cell %d does not exists\n",id));
      return 0;
    }
    return reinterpret_cast<Cell *>(MWAWTable::get(id).get());
  }
};

////////////////////////////////////////
//! Internal: the section of a MacWrtProStructures
struct Section {
  enum StartType { S_Line, S_Page, S_PageLeft, S_PageRight };

  //! constructor
  Section() : m_start(S_Page), m_colsPos(), m_textLength(0), m_extra("")
  {
    for (int i = 0; i < 2; ++i) m_headerIds[i] = m_footerIds[i] = 0;
  }
  //! returns a MWAWSection
  MWAWSection getSection() const
  {
    MWAWSection sec;
    size_t numCols=m_colsPos.size()/2;
    if (numCols <= 1)
      return sec;
    sec.m_columns.resize(numCols);
    float prevPos=0;
    for (size_t c=0; c < numCols; ++c) {
      sec.m_columns[c].m_width = double(m_colsPos[2*c+1]-prevPos);
      prevPos = m_colsPos[2*c+1];
      sec.m_columns[c].m_widthUnit = librevenge::RVNG_POINT;
      sec.m_columns[c].m_margins[libmwaw::Right] =
        double(m_colsPos[2*c+1]-m_colsPos[2*c])/72.;
    }
    return sec;
  }
  //! return the number of columns
  int numColumns() const
  {
    int numCols=int(m_colsPos.size()/2);
    return numCols ? numCols : 1;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Section const &sec)
  {
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
    default:
      break;
    }
    size_t numColumns = size_t(sec.numColumns());
    if (numColumns != 1) {
      o << "nCols=" << numColumns << ",";
      o << "colsPos=[";
      for (size_t i = 0; i < 2*numColumns; i+=2)
        o << sec.m_colsPos[i] << ":" << sec.m_colsPos[i+1] << ",";
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
  //! the columns position ( series of end columns <-> new column begin)
  std::vector<float> m_colsPos;
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
//! Internal: the state of a MacWrtProStructures
struct State {
  //! constructor
  State() : m_version(-1), m_numPages(1), m_inputData(), m_fontsList(), m_paragraphsList(),
    m_sectionsList(), m_blocksList(), m_blocksMap(), m_tablesMap(),m_footnotesList(),
    m_headersMap(), m_footersMap()
  {
  }

  //! try to set the line properties of a border
  static bool updateLineType(int lineType, MWAWBorder &border)
  {
    switch (lineType) {
    case 2:
      border.m_type=MWAWBorder::Double;
      border.m_widthsList.resize(3,2.);
      border.m_widthsList[1]=1.0;
      break;
    case 3:
      border.m_type=MWAWBorder::Double;
      border.m_widthsList.resize(3,1.);
      border.m_widthsList[2]=2.0;
      break;
    case 4:
      border.m_type=MWAWBorder::Double;
      border.m_widthsList.resize(3,1.);
      border.m_widthsList[0]=2.0;
      break;
    case 1: // solid
      break;
    default:
      return false;
    }
    return true;
  }

  //! the file version
  int m_version;

  //! the number of pages
  int m_numPages;

  //! the input data
  librevenge::RVNGBinaryData m_inputData;

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
MacWrtProStructures::MacWrtProStructures(MacWrtProParser &parser) :
  m_parserState(parser.getParserState()), m_input(), m_mainParser(parser),
  m_state(), m_asciiFile(), m_asciiName("")
{
  init();
}

MacWrtProStructures::~MacWrtProStructures()
{
  ascii().reset();
}

void MacWrtProStructures::init()
{
  m_state.reset(new MacWrtProStructuresInternal::State);
  m_asciiName = "struct";
}

int MacWrtProStructures::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}

MWAWTextListenerPtr &MacWrtProStructures::getTextListener()
{
  return m_parserState->m_textListener;
}

int MacWrtProStructures::numPages() const
{
  return m_state->m_numPages;
}

////////////////////////////////////////////////////////////
// try to return the header/footer block id
int MacWrtProStructures::getHeaderId(int page, int &numSimilar) const
{
  numSimilar = 1;
  if (version()==0) page = 0;
  std::map<int, int>::const_iterator it=m_state->m_headersMap.lower_bound(page);
  if (it==m_state->m_headersMap.end()) {
    if (m_state->m_numPages>page)
      numSimilar=m_state->m_numPages-page+1;
    return 0;
  }
  if (it->first!=page) {
    numSimilar=it->first-page;
    return 0;
  }
  int res = it->second;
  while (++it!=m_state->m_headersMap.end() && it->second==res)
    numSimilar++;
  return res;
}

int MacWrtProStructures::getFooterId(int page, int &numSimilar) const
{
  numSimilar = 1;
  if (version()==0) page = 0;
  std::map<int, int>::const_iterator it=m_state->m_footersMap.lower_bound(page);
  if (it==m_state->m_footersMap.end()) {
    if (m_state->m_numPages>page)
      numSimilar=m_state->m_numPages-page+1;
    return 0;
  }
  if (it->first!=page) {
    numSimilar=it->first-page;
    return 0;
  }
  int res = it->second;
  while (++it!=m_state->m_footersMap.end() && it->second==res)
    numSimilar++;
  return res;
}

////////////////////////////////////////////////////////////
// try to return the color/pattern, set the line properties
bool MacWrtProStructures::getColor(int colId, MWAWColor &color) const
{
  if (version()==0) {
    // MWII: 2:red 4: blue, ..
    switch (colId) {
    case 0:
      color = 0xFFFFFF;
      break;
    case 1:
      color = 0;
      break;
    case 2:
      color = 0xFF0000;
      break;
    case 3:
      color = 0x00FF00;
      break;
    case 4:
      color = 0x0000FF;
      break;
    case 5:
      color = 0x00FFFF;
      break; // cyan
    case 6:
      color = 0xFF00FF;
      break; // magenta
    case 7:
      color = 0xFFFF00;
      break; // yellow
    default:
      MWAW_DEBUG_MSG(("MacWrtProStructures::getColor: unknown color %d\n", colId));
      return false;
    }
  }
  else {
    /* 0: white, 38: yellow, 44: magenta, 36: red, 41: cyan, 39: green, 42: blue
       checkme: this probably corresponds to the following 81 gray/color palette...
    */
    uint32_t const colorMap[] = {
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
      MWAW_DEBUG_MSG(("MacWrtProStructures::getColor: unknown color %d\n", colId));
      return false;
    }
    color = colorMap[colId];
  }
  return true;
}

bool MacWrtProStructures::getPattern(int patId, float &patternPercent) const
{
  patternPercent=1.0f;
  if (version()==0) // not implemented
    return false;
  static float const defPercentPattern[64] = {
    0.0f, 1.0f, 0.968750f, 0.93750f, 0.8750f, 0.750f, 0.50f, 0.250f,
    0.250f, 0.18750f, 0.18750f, 0.1250f, 0.06250f, 0.06250f, 0.031250f, 0.015625f,
    0.750f, 0.50f, 0.250f, 0.3750f, 0.250f, 0.1250f, 0.250f, 0.1250f,
    0.750f, 0.50f, 0.250f, 0.3750f, 0.250f, 0.1250f, 0.250f, 0.1250f,
    0.750f, 0.50f, 0.50f, 0.50f, 0.50f, 0.250f, 0.250f, 0.234375f,
    0.6250f, 0.3750f, 0.1250f, 0.250f, 0.218750f, 0.218750f, 0.1250f, 0.093750f,
    0.50f, 0.56250f, 0.43750f, 0.3750f, 0.218750f, 0.281250f, 0.18750f, 0.093750f,
    0.593750f, 0.56250f, 0.515625f, 0.343750f, 0.31250f, 0.250f, 0.250f, 0.234375f,
  };
  if (patId <= 0 || patId>64) {
    MWAW_DEBUG_MSG(("MacWrtProStructures::getPattern: unknown pattern %d\n", patId));
    return false;
  }
  patternPercent=defPercentPattern[patId-1];
  return true;
}

bool MacWrtProStructures::getColor(int colId, int patId, MWAWColor &color) const
{
  if (!getColor(colId, color))
    return false;
  if (patId==0)
    return true;
  float percent;
  if (!getPattern(patId,percent))
    return false;
  color=MWAWColor::barycenter(percent,color,1.f-percent,MWAWColor::white());
  return true;
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool MacWrtProStructures::createZones()
{
  if (version() == 0)
    return createZonesV2();

  // first we need to create the input
  if (!m_mainParser.getZoneData(m_state->m_inputData, 3))
    return false;
  m_input=MWAWInputStream::get(m_state->m_inputData,false);
  if (!m_input)
    return false;
  ascii().setStream(m_input);
  ascii().open(asciiName());

  long pos = 0;
  m_input->seek(0, librevenge::RVNG_SEEK_SET);

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
      m_input->seek(pos+16, librevenge::RVNG_SEEK_SET);
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
  for (int st = 0; st < 2; ++st) {
    if (!ok) break;
    pos = m_input->tell();
    std::vector<MacWrtProStructuresInternal::Section> sections;
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
    for (int i = 0; i < 2; ++i) {
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

bool MacWrtProStructures::createZonesV2()
{
  if (version()) {
    MWAW_DEBUG_MSG(("MacWrtProStructures::createZonesV2: must be called for a MacWriteII file\n"));
    return false;
  }
  // first we need to create the input
  if (!m_mainParser.getZoneData(m_state->m_inputData, 3))
    return false;

  libmwaw::DebugStream f;
  m_input=MWAWInputStream::get(m_state->m_inputData,false);
  if (!m_input)
    return false;

  ascii().setStream(m_input);
  ascii().open(asciiName());

  long pos = 0;
  m_input->seek(0, librevenge::RVNG_SEEK_SET);

  bool ok = readFontsName();
  long val;
  if (ok) {
    pos = m_input->tell();
    val = (long) m_input->readULong(4);
    if (val) {
      MWAW_DEBUG_MSG(("MacWrtProStructures::createZonesV2: argh!!! find data after the fonts name zone. Trying to continue.\n"));
      f.str("");
      f << "Entries(Styles):#" << std::hex << val << std::dec;

      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
    }
    else {
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
    shared_ptr<MacWrtProStructuresInternal::Block> block;
    while (1) {
      block = readBlockV2(id++);
      if (!block)
        break;
      // temporary fixme...
      block->m_contentType = MacWrtProStructuresInternal::Block::TEXT;
      block->m_id=id;
      m_state->m_blocksMap[block->m_id] = block;
      m_state->m_blocksList.push_back(block);
      if (block->m_fileBlock)
        m_mainParser.parseDataZone(block->m_fileBlock, 0);
      pos =  m_input->tell();
      val = m_input->readLong(1);
      if (val == 2) continue;
      if (val != 3) break;
      m_input->seek(-1, librevenge::RVNG_SEEK_CUR);
    }
  }
  ascii().addPos(pos);
  ascii().addNote("Entries(DataEnd):");

  int nPages = 1;
  for (int i = 0; i < int(m_state->m_blocksList.size()); ++i) {
    shared_ptr<MacWrtProStructuresInternal::Block> &block = m_state->m_blocksList[size_t(i)];
    switch (block->m_type) {
    case 6:
      if (block->m_isHeader)
        m_state->m_headersMap[0] = i;
      else
        m_state->m_footersMap[0] = i;
      break;
    case 5: // soft page break
      nPages++;
      break;
    case 7:
      m_state->m_footnotesList.push_back(i);
      break;
    case -1: // hard page break
      if (block->isText())
        nPages += m_mainParser.findNumHardBreaks(block->m_fileBlock);
      break;
    default:
      break;
    }
  }
  m_state->m_numPages = nPages;

  return true;
}

////////////////////////////////////////////////////////////
// try to find the main text zone and sent it
bool MacWrtProStructures::sendMainZone()
{
  int vers = version();
  for (size_t i = 0; i < m_state->m_blocksList.size(); ++i) {
    if (!m_state->m_blocksList[i]->isText() ||
        m_state->m_blocksList[i]->m_send) continue;
    if (vers == 1 && m_state->m_blocksList[i]->m_type != 5)
      continue;
    // fixme
    if (vers == 0 && m_state->m_blocksList[i]->m_type != -1)
      continue;
    return send(vers==0 ? int(i) : m_state->m_blocksList[i]->m_id, true);
  }

  //ok the main zone can be empty
  for (size_t i = 0; i < m_state->m_blocksList.size(); ++i) {
    if (m_state->m_blocksList[i]->m_type != 5 ||
        m_state->m_blocksList[i]->m_send) continue;

    shared_ptr<MacWrtProStructures> THIS
    (this, MWAW_shared_ptr_noop_deleter<MacWrtProStructures>());
    MacWrtProStructuresListenerState listenerState(THIS, true);
    return true;
  }

  MWAW_DEBUG_MSG(("MacWrtProStructures::sendMainZone: can not find main zone...\n"));
  return false;
}

////////////////////////////////////////////////////////////
// try to find the header and the pages break
void MacWrtProStructures::buildPageStructures()
{
  // first find the pages break
  std::set<long> set;
  int actPage = 0;
  for (size_t i = 0; i < m_state->m_blocksList.size(); ++i) {
    m_state->m_blocksList[i]->m_page = actPage ? actPage : 1; // mainly ok
    if (m_state->m_blocksList[i]->m_type != 5)
      continue;
    actPage++;
    set.insert(m_state->m_blocksList[i]->m_textPos);
  }
  size_t numSections = m_state->m_sectionsList.size();
  long actSectPos = 0;
  for (size_t i = 0; i < numSections; ++i) {
    MacWrtProStructuresInternal::Section &sec = m_state->m_sectionsList[i];
    if (sec.m_start != sec.S_Line) set.insert(actSectPos);
    actSectPos += sec.m_textLength;
  }
  std::vector<int> pagesBreak;
  pagesBreak.assign(set.begin(), set.end());

  // now associates the header/footer to each pages
  int nPages = m_state->m_numPages = int(pagesBreak.size());
  int actPagePos = 0;
  actPage = 0;
  actSectPos = 0;
  for (size_t i = 0; i < numSections; ++i) {
    MacWrtProStructuresInternal::Section &sec = m_state->m_sectionsList[i];
    std::vector<int> listPages;
    actSectPos += sec.m_textLength;
    while (actPagePos < actSectPos) {
      listPages.push_back(actPage);
      if (actPage == nPages-1 || pagesBreak[size_t(actPage+1)] > actSectPos)
        break;
      actPagePos=pagesBreak[(size_t)++actPage];
    }
    int headerId = 0, footerId = 0;
    for (int k = 0; k < 2; ++k) {
      if (sec.m_headerIds[k])
        headerId = sec.m_headerIds[k];
      if (sec.m_footerIds[k])
        footerId = sec.m_footerIds[k];
    }
    if (!headerId && !footerId) continue;
    for (size_t j = 0; j < listPages.size(); ++j) {
      int p = listPages[j]+1;
      if (headerId && m_state->m_headersMap.find(p) == m_state->m_headersMap.end())
        m_state->m_headersMap[p] = headerId;
      if (footerId)
        m_state->m_footersMap[p] = footerId;
    }
  }
  // finally mark the attachment
  std::vector<int> const &listCalled = m_mainParser.getBlocksCalledByToken();
  for (size_t i = 0; i < listCalled.size(); ++i) {
    if (m_state->m_blocksMap.find(listCalled[i]) == m_state->m_blocksMap.end()) {
      MWAW_DEBUG_MSG(("MacWrtProStructures::buildPageStructures: can not find attachment block %d...\n",
                      listCalled[i]));
      continue;
    }
    shared_ptr<MacWrtProStructuresInternal::Block> block =
      m_state->m_blocksMap.find(listCalled[i])->second;
    block->m_attachment = true;
  }
}

////////////////////////////////////////////////////////////
// try to find the main text zone and sent it
void MacWrtProStructures::buildTableStructures()
{
  size_t numBlocks = m_state->m_blocksList.size();
  for (size_t i = 0; i < numBlocks; ++i) {
    if (m_state->m_blocksList[i]->m_type != 3)
      continue;
    shared_ptr<MacWrtProStructuresInternal::Block> table
      = m_state->m_blocksList[i];
    std::vector<shared_ptr<MacWrtProStructuresInternal::Block> > blockList;
    size_t j = i+1;
    for (; j < numBlocks; ++j) {
      shared_ptr<MacWrtProStructuresInternal::Block> cell = m_state->m_blocksList[j];
      if (cell->m_type != 4)
        break;
      if (!table->contains(cell->m_box))
        break;
      bool ok = true;
      for (size_t k = 0; k < blockList.size(); ++k) {
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

    size_t numCells = blockList.size();
    bool ok = numCells > 1;
    if (!ok && numCells == 1)
      ok = table->m_col == 1 && table->m_row == 1;
    if (!ok) {
      MWAW_DEBUG_MSG(("MacWrtProStructures::buildTableStructures: find a table with %ld cells : ignored...\n", long(numCells)));
      continue;
    }

    shared_ptr<MacWrtProStructuresInternal::Table> newTable(new MacWrtProStructuresInternal::Table);
    for (size_t k = 0; k < numCells; ++k) {
      blockList[k]->m_send = true;
      blockList[k]->m_attachment = true;
      blockList[k]->m_textboxCellType=1;
      newTable->add(shared_ptr<MacWrtProStructuresInternal::Cell>
                    (new MacWrtProStructuresInternal::Cell(*this, blockList[k].get())));
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
bool MacWrtProStructures::readFontsName()
{
  long pos = m_input->tell();
  libmwaw::DebugStream f;

  long sz = (long) m_input->readULong(4);
  if (sz == 0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }
  int vers = version();
  long endPos = pos+4+sz;
  m_input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("MacWrtProStructures::readFontsName: file is too short\n"));
    m_input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  m_input->seek(pos+4, librevenge::RVNG_SEEK_SET);
  if (sz == 0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }
  f << "Entries(FontsName):";
  int N=(int) m_input->readULong(2);
  if (3*N+2 > sz) {
    MWAW_DEBUG_MSG(("MacWrtProStructures::readFontsName: can not read the number of fonts\n"));
    m_input->seek(endPos, librevenge::RVNG_SEEK_SET);
    f << "#";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return true;
  }

  for (int ft = 0; ft < N; ++ft) {
    int fId = (int) m_input->readLong(2);
    f << "[id=" << fId << ",";
    for (int st = 0; st < 2; ++st) {
      int sSz = (int) m_input->readULong(1);
      if (long(m_input->tell())+sSz > endPos) {
        MWAW_DEBUG_MSG(("MacWrtProStructures::readFontsName: can not read the %d font\n", ft));
        f << "#";
        break;
      }
      std::string name("");
      for (int i = 0; i < sSz; ++i)
        name += char(m_input->readULong(1));
      if (name.length()) {
        if (st == 0)
          m_parserState->m_fontConverter->setCorrespondance(fId, name);
        f << name << ",";
      }
      if (vers)
        break;
    }
    f << "],";
  }

  if (long(m_input->tell()) != endPos)
    ascii().addDelimiter(m_input->tell(),'|');
  m_input->seek(endPos, librevenge::RVNG_SEEK_SET);

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read the character properties
bool MacWrtProStructures::readFontsDef()
{
  long pos = m_input->tell();
  libmwaw::DebugStream f;

  long sz = (long) m_input->readULong(4);
  if (sz == 0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }
  long endPos = pos+4+sz;
  int expectedSize = version()==0 ? 10 : 20;
  if ((sz%expectedSize) != 0) {
    MWAW_DEBUG_MSG(("MacWrtProStructures::readFontsDef: find an odd value for sz\n"));
    m_input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  m_input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("MacWrtProStructures::readFontsDef: file is too short\n"));
    m_input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  m_input->seek(pos+4, librevenge::RVNG_SEEK_SET);
  f << "Entries(FontsDef):";
  int N = int(sz/expectedSize);
  f << "N=" << N;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  m_state->m_fontsList.resize(0);
  for (int n = 0; n < N; ++n) {
    pos = m_input->tell();
    MacWrtProStructuresInternal::Font font;
    if (!readFont(font)) {
      ascii().addPos(pos);
      ascii().addNote("FontsDef-#");
      m_input->seek(endPos, librevenge::RVNG_SEEK_SET);
      return true;
    }
    m_state->m_fontsList.push_back(font);
    f.str("");
    f << "FontsDef-C" << n << ":";
    f << font.m_font.getDebugString(m_parserState->m_fontConverter) << font << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool MacWrtProStructures::readFont(MacWrtProStructuresInternal::Font &font)
{
  long pos = m_input->tell();
  int vers = version();
  libmwaw::DebugStream f;
  font = MacWrtProStructuresInternal::Font();
  font.m_values[0] = (int) m_input->readLong(2); // 1, 3 or 6
  int val = (int) m_input->readULong(2);
  if (val != 0xFFFF)
    font.m_font.setId(val);
  val = (int) m_input->readULong(2);
  if (val != 0xFFFF)
    font.m_font.setSize(float(val)/4.f);
  if (vers >= 1)
    font.m_values[1] = (int) m_input->readLong(2);
  long flag = (long) m_input->readULong(2);
  uint32_t flags=0;
  if (flag&0x1) flags |= MWAWFont::boldBit;
  if (flag&0x2) flags |= MWAWFont::italicBit;
  if (flag&0x4) font.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
  if (flag&0x8) flags |= MWAWFont::embossBit;
  if (flag&0x10) flags |= MWAWFont::shadowBit;
  if (flag&0x20) font.m_font.set(MWAWFont::Script(40,librevenge::RVNG_PERCENT));
  if (flag&0x40) font.m_font.set(MWAWFont::Script(-40,librevenge::RVNG_PERCENT));
  if (flag&0x100) font.m_font.set(MWAWFont::Script::super());
  if (flag&0x200) font.m_font.setStrikeOutStyle(MWAWFont::Line::Simple);
  if (flag&0x400) flags |= MWAWFont::allCapsBit;
  if (flag&0x800) flags |= MWAWFont::smallCapsBit;
  if (flag&0x1000) font.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
  if (flag&0x2000) {
    font.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
    font.m_font.setUnderlineType(MWAWFont::Line::Double);
  }
  if (flag&0x4000) flags |= MWAWFont::lowercaseBit;
  font.m_flags = (flag&0x8080L);

  int color = (int) m_input->readULong(1);
  MWAWColor col;
  if (color != 1 && getColor(color, col))
    font.m_font.setColor(col);
  else if (color != 1)
    f << "#colId=" << color << ",";
  val = (int) m_input->readULong(1); // always 0x64 (unused?)
  if (val != 0x64) font.m_values[2] = val;
  if (vers == 1) {
    int lang = (int) m_input->readLong(2);
    switch (lang) {
    case 0:
      font.m_font.setLanguage("en_US");
      break;
    case 2:
      font.m_font.setLanguage("en_GB");
      break;
    case 3:
      font.m_font.setLanguage("de");
      break;
    default:
      f << "#lang=" << lang << ",";
      break;
    }
    font.m_token = (int) m_input->readLong(2);
    int spacings = (int) m_input->readLong(2);
    if (spacings) {
      if (spacings < -50 || spacings > 100) {
        MWAW_DEBUG_MSG(("MacWrtProStructures::readFont: character spacings seems odd\n"));
        f << "#spacings=" << spacings << "%,";
        spacings = spacings < 0 ? -50 : 100;
      }
      float fSz = font.m_font.size();
      if (fSz <= 0) {
        MWAW_DEBUG_MSG(("MacWrtProStructures::readFont: expand called without fSize, assume 12pt\n"));
        fSz = 12;
      }
      font.m_font.setDeltaLetterSpacing(fSz*float(spacings)/100.f);
    }
    for (int i = 4; i < 5; ++i)
      font.m_values[i] = (int) m_input->readLong(2);
    m_input->seek(pos+20, librevenge::RVNG_SEEK_SET);
  }
  font.m_font.setFlags(flags);
  font.m_font.m_extra = f.str();

  return true;
}

////////////////////////////////////////////////////////////
// read a paragraph and a list of paragraph
bool MacWrtProStructures::readParagraphs()
{
  long pos = m_input->tell();
  libmwaw::DebugStream f;
  int dataSz = version()==0 ? 202 : 192;

  long sz = (long) m_input->readULong(4);
  if (sz == 0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }
  long endPos = pos+sz;
  if ((sz%dataSz) != 0) {
    MWAW_DEBUG_MSG(("MacWrtProStructures::readParagraphs: find an odd value for sz\n"));
    m_input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  m_input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("MacWrtProStructures::readParagraphs: file is too short\n"));
    m_input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  m_input->seek(pos+4, librevenge::RVNG_SEEK_SET);
  f << "Entries(ParaZone):";
  int N = int(sz/dataSz);
  f << "N=" << N;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  m_state->m_paragraphsList.resize(0);
  for (int n = 0; n < N; ++n) {
    pos = m_input->tell();
    int val = (int) m_input->readLong(2);
    f.str("");
    f << "Entries(Paragraph)[" << n << "]:";
    if (val) f << "numChar?="<<val <<",";
    MacWrtProStructuresInternal::Paragraph para;
    if (!readParagraph(para)) {
      f << "#";
      m_state->m_paragraphsList.push_back(MacWrtProStructuresInternal::Paragraph());
      m_input->seek(pos+dataSz, librevenge::RVNG_SEEK_SET);
    }
    else {
      f << para;
      m_state->m_paragraphsList.push_back(para);
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool MacWrtProStructures::readParagraph(MacWrtProStructuresInternal::Paragraph &para)
{
  libmwaw::DebugStream f;
  int vers = version();
  long pos = m_input->tell(), endPos = pos+(vers == 0 ? 200: 190);
  para = MacWrtProStructuresInternal::Paragraph();

  m_input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("MacWrtProStructures::readParagraph: file is too short\n"));
    m_input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
  int val, just = 0;
  if (vers == 0) {
    just = (int) m_input->readULong(2);
    val = (int) m_input->readLong(2);
    if (val) f << "unkn=" << val << ",";
  }
  para.m_margins[1] = float(m_input->readLong(4))/72.0f/65536.f;
  para.m_margins[0] = float(m_input->readLong(4))/72.0f/65536.f;
  para.m_margins[2] = float(m_input->readLong(4))/72.0f/65536.f;


  float spacings[3];
  for (int i = 0; i < 3; ++i)
    spacings[i] = float(m_input->readLong(4))/65536.f;
  for (int i = 0; i < 3; ++i) {
    int dim = vers==0 ? (int)m_input->readLong(4) : (int)m_input->readULong(1);
    bool inPoint = true;
    bool ok = true;
    switch (dim) {
    case 0: // point
      ok = spacings[i] < 721 && (i || spacings[0] > 0.0);
      spacings[i]/=72.f;
      break;
    case -1:
    case 0xFF: // percent
      ok = (spacings[i] >= 0.0 && spacings[i]<46.0);
      if (i==0) spacings[i]+=1.0f;
      inPoint=false;
      break;
    default:
      f << "#inter[dim]=" << std::hex << dim << std::dec << ",";
      ok = spacings[i] < 721 && (i || spacings[0] > 0.0);
      spacings[i]/=72.f;
      break;
    }
    if (ok) {
      if (i == 0 && inPoint) {
        if (spacings[0] > 0)
          para.setInterline(spacings[0], librevenge::RVNG_INCH, MWAWParagraph::AtLeast);
        else if (spacings[0] < 0) f << "interline=" << spacings[0] << ",";
        continue;
      }
      para.m_spacings[i] = spacings[i];
      if (inPoint && spacings[i] > 1.0) {
        MWAW_DEBUG_MSG(("MacWrtProStructures::readParagraph: spacings looks big decreasing it\n"));
        f << "#prevSpacings" << i << "=" << spacings[i] << ",";
        para.m_spacings[i] = 1.0;
      }
      else if (!inPoint && i && (spacings[i]<0 || spacings[i]>0)) {
        if (i==1) f << "spaceBef";
        else f  << "spaceAft";
        f << "=" << spacings[i] << "%,";
        /** seems difficult to set bottom a percentage of the line unit,
            so do the strict minimum... */
        *(para.m_spacings[i]) *= 10./72.;
      }
    }
    else
      f << "#spacings" << i << ",";
  }

  if (vers==1) {
    just = (int) m_input->readULong(1);
    m_input->seek(pos+28, librevenge::RVNG_SEEK_SET);
  }
  else {
    ascii().addDelimiter(m_input->tell(),'|');
  }
  /* Note: when no extra tab the justification,
           if there is a extra tab, this corresponds to the extra tab alignement :-~ */
  switch (just & 0x3) {
  case 0:
    break;
  case 1:
    para.m_justify = MWAWParagraph::JustificationCenter;
    break;
  case 2:
    para.m_justify = MWAWParagraph::JustificationRight;
    break;
  case 3:
    para.m_justify = MWAWParagraph::JustificationFull;
    break;
  default:
    break;
  }
  if (just & 0x40)
    para.m_breakStatus = MWAWParagraph::NoBreakWithNextBit;
  if (just & 0x80)
    para.m_breakStatus = para.m_breakStatus.get()|MWAWParagraph::NoBreakBit;
  if (just&0x3C) f << "#justify=" << std::hex << (just&0x3C) << std::dec << ",";
  bool emptyTabFound = false;
  for (int i = 0; i < 20; ++i) {
    pos = m_input->tell();
    MWAWTabStop newTab;
    int type = (int) m_input->readULong(1);
    switch (type & 3) {
    case 0:
      break;
    case 1:
      newTab.m_alignment = MWAWTabStop::CENTER;
      break;
    case 2:
      newTab.m_alignment = MWAWTabStop::RIGHT;
      break;
    case 3:
      newTab.m_alignment = MWAWTabStop::DECIMAL;
      break;
    default:
      break;
    }
    if (type & 0xfc) {
      MWAW_DEBUG_MSG(("MacWrtProStructures::readParagraph: tab type is odd\n"));
      f << "tabs" << i << "[#type]=" << std::hex << (type & 0xFc) << std::dec << ",";
    }
    int leader = (int) m_input->readULong(1);
    if (leader != 0x20)
      newTab.m_leaderCharacter = (uint16_t) leader;
    unsigned long tabPos = m_input->readULong(4);
    if (tabPos == 0xFFFFFFFFL) {
      emptyTabFound = true;
      m_input->seek(pos+8, librevenge::RVNG_SEEK_SET);
      continue;
    }
    if (emptyTabFound) {
      MWAW_DEBUG_MSG(("MacWrtProStructures::readParagraph: empty tab already found\n"));
      f << "tab" << i << "#";
    }
    newTab.m_position = float(tabPos)/72./65536.;
    int decimalChar = (int) m_input->readULong(1);
    if (decimalChar && decimalChar != '.')
      newTab.m_decimalCharacter=(uint16_t) decimalChar;
    val = (int) m_input->readLong(1); // always 0?
    if (val)
      f << "tab" << i << "[#unkn=" << std::hex << val << std::dec << "],";
    para.m_tabs->push_back(newTab);
    m_input->seek(pos+8, librevenge::RVNG_SEEK_SET);
  }

  if (vers==1) {
    m_input->seek(endPos-2, librevenge::RVNG_SEEK_SET);
    para.m_value = (int) m_input->readLong(2);
  }
  para.m_extra=f.str();

  m_input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// read the styles ( character )
bool MacWrtProStructures::readCharStyles()
{
  long pos = m_input->tell();
  libmwaw::DebugStream f;
  int vers = version();

  int N;
  int expectedSz = 0x42;
  if (version() == 1) {
    long sz = (long) m_input->readULong(4);
    if ((sz%0x42) != 0) {
      MWAW_DEBUG_MSG(("MacWrtProStructures::readCharStyles: find an odd value for sz=%ld\n",sz));
      m_input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    N = int(sz/0x42);
  }
  else {
    N = (int) m_input->readULong(2);
    expectedSz = 0x2a;
  }

  if (N == 0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }
  long actPos = m_input->tell();
  long endPos = actPos+N*expectedSz;

  m_input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("MacWrtProStructures::readCharStyles: file is too short\n"));
    m_input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  m_input->seek(actPos, librevenge::RVNG_SEEK_SET);
  f << "Entries(CharStyles):N=" << N;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i = 0; i < N; ++i) {
    pos = m_input->tell();
    f.str("");
    f << "CharStyles-" << i << ":";
    int sSz = (int) m_input->readULong(1);
    if (sSz > 31) {
      MWAW_DEBUG_MSG(("MacWrtProStructures::readCharStyles: string size seems odd\n"));
      sSz = 31;
      f << "#";
    }
    std::string name("");
    for (int c = 0; c < sSz; ++c)
      name += char(m_input->readULong(1));
    f << name << ",";
    m_input->seek(pos+32, librevenge::RVNG_SEEK_SET);

    if (vers == 1) {
      int val = (int) m_input->readLong(2);
      if (val) f << "unkn0=" << val << ",";
      val = (int) m_input->readLong(2);
      if (val != -1) f << "unkn1=" << val << ",";
      f << "ptr?=" << std::hex << m_input->readULong(4) << std::dec << ",";
      val = (int) m_input->readLong(2); // small number between 0 and 2 (nextId?)
      if (val) f << "f0=" << val << ",";
      for (int j = 1; j < 5; ++j) { // [-1,0,1], [0,1 or ee], 0, 0
        val = (int) m_input->readLong(1);
        if (val) f << "f" << j <<"=" << val << ",";
      }
    }
    MacWrtProStructuresInternal::Font font;
    if (!readFont(font)) {
      MWAW_DEBUG_MSG(("MacWrtProStructures::readCharStyles: can not read the font\n"));
      f << "###";
    }
    else
      f << font.m_font.getDebugString(m_parserState->m_fontConverter) << font << ",";

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());

    m_input->seek(pos+expectedSz, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the styles ( paragraph + font)
bool MacWrtProStructures::readStyles()
{
  long pos = m_input->tell();
  libmwaw::DebugStream f;
  long sz = (long) m_input->readULong(4);
  if ((sz%0x106) != 0) {
    MWAW_DEBUG_MSG(("MacWrtProStructures::readStyles: find an odd value for sz=%ld\n",sz));
    m_input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  int N = int(sz/0x106);

  if (N==0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }

  f << "Entries(Style):";
  f << "N=" << N;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i = 0; i < N; ++i) {
    pos = m_input->tell();
    if (!readStyle(i)) {
      f.str("");
      f << "#Style-" << i << ":";
      m_input->seek(pos, librevenge::RVNG_SEEK_SET);
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return false;
    }
  }
  ascii().addPos(m_input->tell());
  ascii().addNote("_");

  return true;
}


bool MacWrtProStructures::readStyle(int styleId)
{
  long debPos = m_input->tell(), pos = debPos;
  libmwaw::DebugStream f;
  // checkme something is odd here
  long dataSz = 0x106;
  long endPos = pos+dataSz;
  m_input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("MacWrtProStructures::readStyle: file is too short\n"));
    m_input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "Style-" << styleId << ":";
  int strlen = (int) m_input->readULong(1);
  if (!strlen || strlen > 31) {
    MWAW_DEBUG_MSG(("MacWrtProStructures::readStyle: style name length seems bad!!\n"));
    m_input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  std::string name("");
  for (int i = 0; i < strlen; ++i) // default
    name+=char(m_input->readULong(1));
  f << name << ",";
  m_input->seek(pos+32, librevenge::RVNG_SEEK_SET); // probably end of name

  int val;
  for (int i = 0; i < 3; ++i) { // 0 | [0,1,-1] | numTabs or idStyle?
    val = (int) m_input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  f << "ptr?=" << std::hex << m_input->readULong(4) << std::dec << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos = m_input->tell();

  f.str("");
  f << "Entries(Paragraph)[" << styleId << "]:";
  MacWrtProStructuresInternal::Paragraph para;
  if (!readParagraph(para)) {
    f << "#";
    m_input->seek(pos+190, librevenge::RVNG_SEEK_SET);
  }
  else
    f << para;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos = m_input->tell();
  f.str("");
  f << "Style-" << styleId << "(II):";
  val = (int) m_input->readLong(2);
  if (val != -1) f << "nextId?=" << val << ",";
  val = (int) m_input->readLong(1); // -1 0 or 1
  if (val) f << "f0=" << val << ",";
  for (int i = 1; i < 4; ++i) { // 0, then 0|1
    val = (int) m_input->readLong(i==3?1:2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos = m_input->tell();
  MacWrtProStructuresInternal::Font font;
  if (!readFont(font)) {
    MWAW_DEBUG_MSG(("MacWrtProStructures::readStyle: end of style seems bad\n"));
    ascii().addPos(pos);
    ascii().addNote("Style:end###");
    m_input->seek(endPos, librevenge::RVNG_SEEK_SET);
    return long(m_input->tell()) == endPos;
  }

  f.str("");
  f << "FontsDef:";
  f << font.m_font.getDebugString(m_parserState->m_fontConverter) << font << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos = m_input->tell();

  f.str("");
  f << "Style-" << styleId << "(end):";
  val = (int) m_input->readLong(2);
  if (val!=-1) f << "unkn=" << val << ",";

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  m_input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// read the list of blocks
bool MacWrtProStructures::readBlocksList()
{
  long pos = m_input->tell();
  libmwaw::DebugStream f;

  long endPos = pos+45;
  m_input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("MacWrtProStructures::readBlocksList: file is too short\n"));
    m_input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "Entries(Block):";
  int N= (int) m_input->readLong(4); // 1 or 3
  f << "N?=" << N << ",";
  long val = m_input->readLong(4); // 0 or small number 1|fe 72, 529
  if (val) f << "f0=" << val << ",";
  for (int i = 0; i < 4; ++i) { // [0|81|ff][0|03|33|63|ff][0|ff][0|ff]
    val = (long) m_input->readULong(1);
    if (val) f << "flA" << i << "=" << std::hex << val << std::dec << ",";
  }
  val = m_input->readLong(4); // 0, 2, 46, 1479
  if (val) f << "f1=" << val << ",";
  for (int i = 0; i < 4; ++i) { // [0|1][0|74][0][0|4]
    val = (long) m_input->readULong(1);
    if (val) f << "flB" << i << "=" << std::hex << val << std::dec << ",";
  }
  for (int i = 2; i < 4; ++i) { // [0|72] [0|a]
    val = m_input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  val = (long) m_input->readULong(4);
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
    m_input->seek(sz, librevenge::RVNG_SEEK_CUR);
  }
  shared_ptr<MacWrtProStructuresInternal::Block> block;
  while (1) {
    block = readBlock();
    if (!block) break;
    m_state->m_blocksList.push_back(block);
    if (m_state->m_blocksMap.find(block->m_id) != m_state->m_blocksMap.end()) {
      MWAW_DEBUG_MSG(("MacWrtProStructures::readBlocksList: block %d already exists\n", block->m_id));
    }
    else
      m_state->m_blocksMap[block->m_id] = block;
    if (block->isGraphic() || block->isText())
      m_mainParser.parseDataZone(block->m_fileBlock, block->isGraphic() ? 1 : 0);
  }
  return true;
}

int MacWrtProStructures::getEndBlockSize()
{
  int sz = 8;
  long pos = m_input->tell();
  m_input->seek(6, librevenge::RVNG_SEEK_CUR);
  // CHECKME ( sometimes, we find 0x7FFF here and sometimes not !!!)
  if (m_input->readULong(2) == 0x7fff && m_input->readULong(2) == 1)
    sz += 2;
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
  return sz;
}

shared_ptr<MacWrtProStructuresInternal::Block>  MacWrtProStructures::readBlockV2(int wh)
{
  long pos = m_input->tell();
  long endPos = pos+76;
  libmwaw::DebugStream f;
  shared_ptr<MacWrtProStructuresInternal::Block> res;

  m_input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    m_input->seek(pos, librevenge::RVNG_SEEK_SET);
    return res;
  }
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
  long val;
  int type = (int) m_input->readULong(1);
  res.reset(new MacWrtProStructuresInternal::Block);
  res->m_contentType = MacWrtProStructuresInternal::Block::TEXT;

  if (type == 3) {
    f << "type=3,";
    val = m_input->readLong(1);
    if (val) f<< "unkn=" << val << ",";
    int what = (int) m_input->readULong(1);
    switch (what &0xF0) {
    case 0x40:
      res->m_isHeader = true;
    case 0x80:
      res->m_type = 6;
      break;
    case 0xc0:
      res->m_type = 7;
      break;
    default:
      MWAW_DEBUG_MSG(("MacWrtProStructures::readBlockV2: find unknown block content type\n"));
      f << "#";
      break;
    }
    if (what & 0xf) f << "f0=" << std::hex << int(what & 0xf) << std::dec << ",";
    m_input->seek(23, librevenge::RVNG_SEEK_CUR);
    ascii().addDelimiter(m_input->tell(),'|');
  }
  else {
    endPos = pos+87;
    int bad = 0;
    m_input->seek(-1, librevenge::RVNG_SEEK_CUR);
    for (int i = 0; i < 2; ++i) { // always 0, 0 ?
      val = m_input->readLong(2);
      if (!val) continue;
      f << "f" << i << "=" << val << ",";
      bad++;
    }
    if (bad >= 2) {
      m_input->seek(pos, librevenge::RVNG_SEEK_SET);
      res.reset();
      return res;
    }

    int id = (int) m_input->readLong(2);  // small number, id ?
    if (id) f << "id=" << id << ",";
    val = m_input->readLong(1); // always -1 ?
    if (val != -1) f << "f2=" << val << ",";
    val = m_input->readLong(1); // 0, 1a, 2e, a8, c0
    if (val) f << "f3=" << val << ",";
    for (int i = 0; i < 2; ++i) { // always 0,1 ?
      val = m_input->readLong(2);
      if (val != i) f << "f" << i+4 << "=" << val << ",";
    }
    val = m_input->readLong(1); // 3 or -3
    f << "f6=" << val << ",";
    val = (long) m_input->readULong(1); // 0, 6a, 78, f2, fa : type ?
    if (val) f << "g0=" << val << ",";
    f << "unkn=[";
    for (int i = 0; i < 6; ++i) {
      val = (long) m_input->readULong(2);
      if (val==0) f << "_,";
      else f << std::hex << val << std::dec << ",";
    }
    f << "],";
    f << "unkn2=[";
    for (int i = 0; i < 6; ++i) { // can be 0*12, bb*12 : junk ?
      val = (long) m_input->readULong(2);
      if (val==0) f << "_,";
      else f << std::hex << val << std::dec << ",";
    }
    f << "],";
  }

  // can be fileblock or pageid
  res->m_fileBlock = (int) m_input->readULong(2);

  float dim[4];
  for (int i = 0; i < 4; ++i)
    dim[i] = float(m_input->readLong(2));
  res->m_box = Box2f(Vec2f(dim[1],dim[0]), Vec2f(dim[3],dim[2]));
  for (int i = 0; i < 4; ++i) { // 8000*4 ?
    val = (long) m_input->readULong(i==3 ? 1 : 2);
    if (val != 0x8000) f << "g" << i+1 << "=" << std::hex << val << std::dec << ",";
  }
  res->m_textPos = (int) m_input->readULong(4);
  if (res->m_textPos) {
    // ok this is a soft page break block
    res->m_type = 5;
    res->m_page = res->m_fileBlock;
    res->m_fileBlock = 0;
  }
  val = (long) m_input->readULong(1);
  if (val) f << "g5=" << std::hex << val << std::dec << ",";
  f << "unkn3=[";
  for (int i = 0; i < 6; ++i) { // can be 0*12, cc*12 : junk ?
    val = (long) m_input->readULong(2);
    if (val==0) f << "_,";
    else f << std::hex << val << std::dec << ",";
  }
  f << "],";
  res->m_extra=f.str();
  f.str("");
  f << "Entries(Block)[" << wh << "]:" << *res;

  ascii().addDelimiter(m_input->tell(), '|');
  m_input->seek(endPos, librevenge::RVNG_SEEK_SET);
  while (m_input->readLong(2)==0 && !m_input->isEnd()) {}
  m_input->seek(-2, librevenge::RVNG_SEEK_CUR);
  if (m_input->readLong(1)) m_input->seek(-1, librevenge::RVNG_SEEK_CUR);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return res;
}

shared_ptr<MacWrtProStructuresInternal::Block> MacWrtProStructures::readBlock()
{
  long pos = m_input->tell();
  libmwaw::DebugStream f;

  long sz = (long) m_input->readULong(4);
  // pat2*3?, dim[pt*65536], border[pt*65536], ?, [0|10|1c], 0, block?
  if (sz < 0x40) {
    m_input->seek(pos, librevenge::RVNG_SEEK_SET);
    return shared_ptr<MacWrtProStructuresInternal::Block>();
  }

  long endPos = pos+sz+4;
  m_input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    m_input->seek(pos, librevenge::RVNG_SEEK_SET);
    return shared_ptr<MacWrtProStructuresInternal::Block>();
  }
  m_input->seek(pos+4, librevenge::RVNG_SEEK_SET);

  shared_ptr<MacWrtProStructuresInternal::Block> block(new MacWrtProStructuresInternal::Block);
  long val;
  f << "pat?=[" << std::hex;
  for (int i = 0; i < 2; ++i)
    f << m_input->readULong(2) << ",";
  f << std::dec << "],";
  block->m_type = (int) m_input->readULong(2);
  float dim[4];
  for (int i = 0; i < 4; ++i)
    dim[i] = float(m_input->readLong(4))/65536.f;
  block->m_box = Box2f(Vec2f(dim[1],dim[0]), Vec2f(dim[3],dim[2]));

  static int const(wh[4])= { libmwaw::Top, libmwaw::Left, libmwaw::Bottom, libmwaw::Right };
  for (int i = 0; i < 4; ++i)
    block->m_borderWList[wh[i]]=float(m_input->readLong(4))/65536.f;

  /* 4: pagebreak,
     5: text
     1: floating, 7: none(wrapping/attachment), b: attachment
     0/a: table ?
  */
  for (int i = 0; i < 2; ++i) {
    val = (long) m_input->readULong(2);
    if (val)
      f << "fl" << i << "=" << std::hex << val << std::dec << ",";
  }
  val = m_input->readLong(2);
  if (val) f << "f0=" << val << ",";
  block->m_fileBlock = (int) m_input->readLong(2);
  block->m_id = (int) m_input->readLong(2);
  val = (int) m_input->readLong(2); // almost always 4 ( one time 0)
  if (val!=4)
    f << "bordOffset=" << val << ",";
  for (int i = 2; i < 7; ++i) {
    /* always 0, except f3=-1 (in one file),
       and in other file f4=1,f5=1,f6=1, */
    val = (int) m_input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  block->m_baseline = float(m_input->readLong(4))/65536.f;
  int colorId = (int) m_input->readLong(2);
  int patId=(int) m_input->readLong(2);
  MWAWColor color(MWAWColor::white());
  if (getColor(colorId, patId, color))
    block->m_surfaceColor = color;
  else
    f << "#colorId=" << colorId << ", #patId=" << patId << ",";

  colorId = (int) m_input->readLong(2);
  patId=(int) m_input->readLong(2);
  if (getColor(colorId, patId, color))
    block->m_lineBorder.m_color = color;
  else
    f << "line[#colorId=" << colorId << ", #patId[line]=" << patId << "],";
  val = m_input->readLong(2);
  static float const w[9]= {0,0.5f,1,2,4,6,8,10,12};
  if (val>0&&val<10)
    block->m_lineBorder.m_width = w[val-1];
  else
    f << "#lineWidth=" << val << ",";
  val = m_input->readLong(2);
  if (!m_state->updateLineType((int)val, block->m_lineBorder))
    f << "#line[type]=" << val << ",";
  int contentType = (int) m_input->readULong(1);
  switch (contentType) {
  case 0:
    block->m_contentType = MacWrtProStructuresInternal::Block::TEXT;
    break;
  case 1:
    block->m_contentType = MacWrtProStructuresInternal::Block::GRAPHIC;
    break;
  default:
    MWAW_DEBUG_MSG(("MacWrtProStructures::readBlock: find unknown block content type\n"));
    f << "#contentType=" << contentType << ",";
    break;
  }

  bool isNote = false;
  if (block->m_type==4 && sz == 0xa0) {
    // this can be a note, let check
    isNote=true;
    static double const(expectedWidth[4])= {5,5,19,5};
    for (int i=0; i < 4; ++i) {
      if (block->m_borderWList[i]<expectedWidth[i] ||
          block->m_borderWList[i]>expectedWidth[i]) {
        isNote = false;
        break;
      }
    }
  }
  if (isNote) {
    long actPos = m_input->tell();
    ascii().addDelimiter(pos+116,'|');
    m_input->seek(pos+116, librevenge::RVNG_SEEK_SET);
    val = m_input->readLong(2);
    isNote = val==0 || val==0x100;
    if (isNote) {
      float dim2[4];
      for (int i = 0; i < 4; ++i) {
        dim2[i] = float(m_input->readLong(4))/65536.f;
        if (!val && (dim2[i]<0||dim2[i]>0)) {
          isNote = false;
          break;
        }
      }
      if (isNote && val) {
        // ok, reset the box only if it is bigger
        if (dim2[3]-dim2[1]>dim[3]-dim[1] && dim2[2]-dim2[0]>dim[2]-dim[0])
          block->m_box=Box2f(Vec2f(dim2[1],dim2[0]),Vec2f(dim2[3],dim2[2]));
      }
    }
    if (isNote) {
      block->m_contentType = MacWrtProStructuresInternal::Block::NOTE;
      // ok reset the border and the line color to gray
      for (int i = 0; i < 4; ++i) {
        if (i!=libmwaw::Top)
          block->m_borderWList[i]=1;
      }
      block->m_lineBorder=MWAWBorder();
      block->m_lineBorder.m_color=MWAWColor(128,128,128);

      if (val)
        f << "note[closed],";
      else
        f << "note,";
    }
    m_input->seek(actPos, librevenge::RVNG_SEEK_SET);
  }
  else if (block->m_type==4 && sz == 0x9a) {
    long actPos = m_input->tell();
    ascii().addDelimiter(pos+108,'|');
    m_input->seek(pos+108, librevenge::RVNG_SEEK_SET);
    libmwaw::DebugStream f2;
    for (int i=0; i<4; ++i) {
      MWAWBorder border;
      colorId = (int) m_input->readLong(2);
      patId=(int) m_input->readLong(2);
      f2.str("");
      if (getColor(colorId, patId, color))
        border.m_color=color;
      else
        f2 << "#colorId=" << colorId << ", #patId=" << patId << ",";
      val= m_input->readLong(2);
      if (val > 0 && val < 10)
        border.m_width=w[val-1];
      else
        f2 << "#w[line]=" << val << ",";
      val= m_input->readLong(2);
      if (!m_state->updateLineType((int)val, border))
        f2 << "#border[type]=" << val << ",";
      val=m_input->readLong(2);
      if (int(val)!=i)
        f2 << "#id=" << val << ",";
      border.m_extra = f2.str();
      block->m_borderCellList[wh[i]]=border;
    }
    m_input->seek(actPos, librevenge::RVNG_SEEK_SET);
  }

  block->m_extra = f.str();

  f.str("");
  f << "Block-data(" << m_state->m_blocksList.size() << "):" << *block;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  if (long(m_input->tell()) != endPos)
    ascii().addDelimiter(m_input->tell(), '|');

  m_input->seek(endPos, librevenge::RVNG_SEEK_SET);

  // ok now read the end of the header
  pos = m_input->tell();
  sz = getEndBlockSize();
  if (sz) {
    f.str("");
    f << "Block-end(" << m_state->m_blocksList.size()<< ")[" << block->m_type << "]:";
    switch (block->m_type) {
    case 3: { // table
      block->m_row = (int) m_input->readLong(2);
      block->m_col = (int) m_input->readLong(2);
      f << "numRow=" << block->m_row << ",";
      f << "numCol=" << block->m_col << ",";
      break;
    }
    case 4: { // cell/textbox : no sure it contain data?
      val =  m_input->readLong(2); // always 0 ?
      if (val) f << "f0=" << val << ",";
      val = (long) m_input->readULong(2); // [0|10|1e|10c0|1cc0|a78a|a7a6|d0c0|dcc0]
      if (val) f << "fl?=" << std::hex << val << std::dec << ",";
      break;
    }
    case 5: { // text or ?
      bool emptyBlock = block->m_fileBlock <= 0;
      val = (long) m_input->readULong(2);  // always 0 ?
      if (emptyBlock) {
        if (val & 0xFF00)
          f << "#f0=" << val << ",";
        block->m_textPos=int(((val&0xFF)<<16) | (int) m_input->readULong(2));
        f << "posC=" << block->m_textPos << ",";
      }
      else if (val) f << "f0=" << val << ",";
      val = (long) m_input->readULong(2); // 30c0[normal], 20c0|0[empty]
      f << "fl?=" << std::hex << val << ",";
      break;
    }
    case 6: {
      for (int i = 0; i < 4; ++i) { // [10|d0],40, 0, 0
        val = (long) m_input->readULong(1);
        f << "f" << i << "=" << val << ",";
      }
      val =  m_input->readLong(1);
      switch (val) {
      case 1:
        f << "header,";
        block->m_isHeader = true;
        break;
      case 2:
        f << "footer,";
        block->m_isHeader = false;
        break;
      default:
        MWAW_DEBUG_MSG(("MacWrtProStructures::readBlock: find unknown header/footer type\n"));
        f << "#type=" << val << ",";
        break;
      }
      val =  m_input->readLong(1); // alway 1 ?
      if (val != 1) f << "f4=" << val << ",";
      break;
    }
    case 7: { // footnote: something here ?
      for (int i = 0; i < 3; ++i) { // 0, 0, [0|4000]
        val = (long) m_input->readULong(2);
        f << "f" << i << "=" << std::hex << val << std::dec << ",";
      }
      break;
    }
    case 8:
      break; // graphic: clearly nothing
    default:
      break;
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+sz, librevenge::RVNG_SEEK_SET);
  }

  return block;
}

////////////////////////////////////////////////////////////
// read the column information zone : checkme
bool MacWrtProStructures::readSections(std::vector<MacWrtProStructuresInternal::Section> &sections)
{
  long pos = m_input->tell();
  libmwaw::DebugStream f;

  long sz = (long) m_input->readULong(4);
  if (sz == 0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }
  long endPos = pos+4+sz;
  if ((sz%0xd8)) {
    MWAW_DEBUG_MSG(("MacWrtProStructures::readSections: find an odd value for sz\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(Sections)#");
    m_input->seek(endPos, librevenge::RVNG_SEEK_SET);
    return true;
  }

  int N = int(sz/0xd8);
  f << "Entries(Section):";
  f << "N=" << N;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int n = 0; n < N; ++n) {
    MacWrtProStructuresInternal::Section sec;
    pos = m_input->tell();
    f.str("");
    sec.m_textLength = (long)m_input->readULong(4);
    long val =  m_input->readLong(4); // almost always 0 or a dim?
    if (val) f << "dim?=" << float(val)/65536.f << ",";
    int startWay = (int) m_input->readLong(2);
    switch (startWay) {
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
      MWAW_DEBUG_MSG(("MacWrtProStructures::readSections: find an odd value for start\n"));
      f << "#start=" << startWay << ",";
    }
    val = m_input->readLong(2);
    if (val)
      f << "f0=" << val << ",";
    // a flag ? and noused ?
    for (int i = 0; i < 2; ++i) {
      val = (long) m_input->readULong(1);
      if (val == 0xFF) f << "fl" << i<< "=true,";
      else if (val) f << "fl" << i << "=" << std::hex << val << std::dec << ",";
    }

    for (int st = 0; st < 2; ++st) {
      val = m_input->readLong(2); // alway 1 ?
      if (val != 1) f << "f" << 1+st << "=" << val << ",";
      // another flag ?
      val = (long) m_input->readULong(1);
      if (val) f << "fl" << st+2 << "=" << std::hex << val << std::dec << ",";
    }
    int numColumns = (int) m_input->readLong(2);
    if (numColumns < 1 || numColumns > 20) {
      MWAW_DEBUG_MSG(("MacWrtProStructures::readSections: bad number of columns\n"));
      f << "#nCol=" << numColumns << ",";
      numColumns = 1;
    }
    val = m_input->readLong(2); // find: 3, c, 24
    if (val) f << "f3=" << val << ",";
    for (int i = 4; i < 7; ++i) { // always 0 ?
      val = m_input->readLong(2);
      if (val)
        f << "f" << i << "=" << val << ",";
    }
    long actPos = m_input->tell();
    for (int c = 0; c < 2*numColumns; ++c)
      sec.m_colsPos.push_back(float(m_input->readLong(4))/65536.f);
    m_input->seek(actPos+20*8+4, librevenge::RVNG_SEEK_SET);
    // 5 flags ( 1+unused?)
    for (int i = 0; i < 6; ++i) {
      val = (long) m_input->readULong(1);
      if ((i!=5 && val!=1) || (i==5 && val))
        f << "g" << i << "=" << val << ",";
    }
    for (int st = 0; st < 2; ++st) { // pair, unpair?
      for (int i = 0; i < 2; ++i) { // header/footer
        val = m_input->readLong(2);
        if (val)
          f << "#h" << 2*st+i << "=" << val << ",";

        val = m_input->readLong(2);
        if (i==0) sec.m_headerIds[st] = (int)val;
        else sec.m_footerIds[st] = (int)val;
      }
    }
    sec.m_extra=f.str();
    sections.push_back(sec);

    f.str("");
    f << "Section" << "-" << n << ":" << sec;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+0xd8, librevenge::RVNG_SEEK_SET);
  }

  m_input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// read the selection zone
bool MacWrtProStructures::readSelection()
{
  long pos = m_input->tell();
  libmwaw::DebugStream f;

  long endPos = pos+14;
  m_input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("MacWrtProStructures::readSelection: file is too short\n"));
    m_input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "Entries(Selection):";
  int val = (int) m_input->readLong(2);
  f << "f0=" << val << ","; // zone?
  val = (int) m_input->readLong(4); // -1, 0 or 8 : zone type?
  if (val == -1 || val == 0) { // checkme: none ?
    f << "*";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+6, librevenge::RVNG_SEEK_SET);
    return true;
  }
  if (val!=8) f << "f1=" << val << ",";
  f << "char=";
  for (int i = 0; i < 2; ++i) {
    f << m_input->readULong(4);
    if (i==0) f << "x";
    else f << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  m_input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// read a string
bool MacWrtProStructures::readString(MWAWInputStreamPtr input, std::string &res)
{
  res="";
  long pos = input->tell();
  int sz = (int) input->readLong(2);
  if (sz == 0) return true;
  if (sz < 0) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("MacWrtProStructures::readString: odd value for size\n"));
    return false;
  }
  input->seek(pos+sz+2, librevenge::RVNG_SEEK_SET);
  if (long(input->tell())!=pos+sz+2) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("MacWrtProStructures::readString: file is too short\n"));
    return false;
  }
  input->seek(pos+2, librevenge::RVNG_SEEK_SET);
  for (int i= 0; i < sz; ++i) {
    char c = (char) input->readULong(1);
    if (c) {
      res+=c;
      continue;
    }
    if (i==sz-1) break;

    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("MacWrtProStructures::readString: find odd character in string\n"));
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////
// read an unknown zone
bool MacWrtProStructures::readStructB()
{
  long pos = m_input->tell();
  libmwaw::DebugStream f;

  int N = (int) m_input->readULong(2);
  if (N==0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }
  f << "Entries(StructB):N=" << N << ",";

  // CHECKME: find N=2 only one time ( and across a checksum zone ...)
  long endPos = pos+N*10+6;
  m_input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("MacWrtProStructures::readZonB: file is too short\n"));
    m_input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  m_input->seek(pos+2, librevenge::RVNG_SEEK_SET);
  int val = (int) m_input->readULong(2);
  if (val != 0x2af8)
    f << "f0=" << std::hex << val << std::dec << ",";
  val = (int) m_input->readULong(2);
  if (val) f << "f1=" << val << ",";

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int n = 0; n < N; ++n) {
    pos = m_input->tell();
    f.str("");
    f << "StructB" << "-" << n;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+10, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// check if a block is sent
bool MacWrtProStructures::isSent(int blockId)
{
  if (version()==0) {
    if (blockId < 0 || blockId >= int(m_state->m_blocksList.size())) {
      MWAW_DEBUG_MSG(("MacWrtProStructures::send: can not find the block %d\n", blockId));
      return false;
    }
    return m_state->m_blocksList[(size_t)blockId]->m_send;
  }

  if (m_state->m_blocksMap.find(blockId) == m_state->m_blocksMap.end()) {
    MWAW_DEBUG_MSG(("MacWrtProStructures::isSent: can not find the block %d\n", blockId));
    return true;
  }
  return m_state->m_blocksMap.find(blockId)->second->m_send;
}

////////////////////////////////////////////////////////////
// send a block
bool MacWrtProStructures::send(int blockId, bool mainZone)
{
  MWAWTextListenerPtr listener=m_parserState->m_textListener;
  shared_ptr<MacWrtProStructuresInternal::Block> block;
  if (version()==0) {
    if (blockId < 0) {
      if (-blockId > int(m_state->m_footnotesList.size())) {
        MWAW_DEBUG_MSG(("MacWrtProStructures::send: can not find the footnote %d\n", -blockId));
        return false;
      }
      block = m_state->m_blocksList[(size_t)m_state->m_footnotesList[size_t(-blockId-1)]];
    }
    else {
      if (blockId < 0 || blockId >= int(m_state->m_blocksList.size())) {
        MWAW_DEBUG_MSG(("MacWrtProStructures::send: can not find the block %d\n", blockId));
        return false;
      }
      block = m_state->m_blocksList[(size_t)blockId];
    }
  }
  else {
    if (m_state->m_blocksMap.find(blockId) == m_state->m_blocksMap.end()) {
      MWAW_DEBUG_MSG(("MacWrtProStructures::send: can not find the block %d\n", blockId));
      return false;
    }
    block = m_state->m_blocksMap.find(blockId)->second;
  }
  block->m_send = true;
  if (block->m_type == 4 && block->m_textboxCellType == 0) {
    block->m_textboxCellType = 2;
    librevenge::RVNGPropertyList extras;
    block->fillFramePropertyList(extras);
    m_mainParser.sendTextBoxZone(blockId, block->getPosition(), extras);
    block->m_textboxCellType = 0;
  }
  else if (block->isText())
    m_mainParser.sendTextZone(block->m_fileBlock, mainZone);
  else if (block->isGraphic()) {
    librevenge::RVNGPropertyList extras;
    block->fillFramePropertyList(extras);
    m_mainParser.sendPictureZone(block->m_fileBlock, block->getPosition(), extras);
  }
  else if (block->m_type == 3) {
    if (m_state->m_tablesMap.find(blockId) == m_state->m_tablesMap.end()) {
      MWAW_DEBUG_MSG(("MacWrtProStructures::send: can not find table with id=%d\n", blockId));
    }
    else {
      bool needTextBox = listener && !block->m_attachment && block->m_textboxCellType == 0;
      if (needTextBox) {
        block->m_textboxCellType = 2;
        m_mainParser.sendTextBoxZone(blockId, block->getPosition());
      }
      else {
        shared_ptr<MacWrtProStructuresInternal::Table> table =
          m_state->m_tablesMap.find(blockId)->second;
        if (!table->sendTable(listener))
          table->sendAsText(listener);
        block->m_textboxCellType = 0;
      }
    }
  }
  else if (block->m_type == 4 || block->m_type == 6) {
    // probably ok, can be an empty cell, textbox, header/footer ..
    if (listener) listener->insertChar(' ');
  }
  else if (block->m_type == 8) {   // empty frame
    librevenge::RVNGPropertyList extras;
    block->fillFramePropertyList(extras);
    m_mainParser.sendEmptyFrameZone(block->getPosition(), extras);
  }
  else {
    MWAW_DEBUG_MSG(("MacWrtProStructures::send: can not send block with type=%d\n", block->m_type));
  }
  return true;
}

////////////////////////////////////////////////////////////
// send the not sent data
void MacWrtProStructures::flushExtra()
{
  int vers = version();
  MWAWTextListenerPtr listener=m_parserState->m_textListener;
  if (listener && listener->isSectionOpened()) {
    listener->closeSection();
    listener->openSection(MWAWSection());
  }
  // first send the text
  for (size_t i = 0; i < m_state->m_blocksList.size(); ++i) {
    if (m_state->m_blocksList[i]->m_send)
      continue;
    if (m_state->m_blocksList[i]->m_type == 6) {
      /* Fixme: macwritepro can have one header/footer by page and one by default.
         For the moment, we only print the first one :-~ */
      MWAW_DEBUG_MSG(("MacWrtProStructures::flushExtra: find some header/footer\n"));
      continue;
    }
    int id = vers == 0 ? int(i) : m_state->m_blocksList[i]->m_id;
    if (m_state->m_blocksList[i]->isText()) {
      // force to non floating position
      m_state->m_blocksList[i]->m_attachment = true;
      send(id);
      if (listener) listener->insertEOL();
    }
    else if (m_state->m_blocksList[i]->m_type == 3) {
      // force to non floating position
      m_state->m_blocksList[i]->m_attachment = true;
      send(id);
    }
  }
  // then send graphic
  for (size_t i = 0; i < m_state->m_blocksList.size(); ++i) {
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
MacWrtProStructuresListenerState::MacWrtProStructuresListenerState(shared_ptr<MacWrtProStructures> structures, bool mainZone)
  : m_isMainZone(mainZone), m_actPage(0), m_actTab(0), m_numTab(0),
    m_section(0), m_numCols(1), m_newPageDone(false), m_structures(structures),
    m_font(new MacWrtProStructuresInternal::Font),
    m_paragraph(new MacWrtProStructuresInternal::Paragraph)
{
  if (!m_structures) {
    MWAW_DEBUG_MSG(("MacWrtProStructuresListenerState::MacWrtProStructuresListenerState can not find structures parser\n"));
    return;
  }
  if (mainZone) {
    newPage();
    sendSection(0);
  }
}

MacWrtProStructuresListenerState::~MacWrtProStructuresListenerState()
{
}

bool MacWrtProStructuresListenerState::isSent(int blockId)
{
  if (!m_structures) return false;
  return m_structures->isSent(blockId);
}

bool MacWrtProStructuresListenerState::send(int blockId)
{
  m_newPageDone = false;
  if (!m_structures) return false;
  return m_structures->send(blockId);
}

void MacWrtProStructuresListenerState::insertSoftPageBreak()
{
  if (m_newPageDone) return;
  newPage(true);
}

bool MacWrtProStructuresListenerState::newPage(bool softBreak)
{
  if (!m_structures || !m_isMainZone) {
    MWAW_DEBUG_MSG(("MacWrtProStructuresListenerState::newPage: can not create a new page\n"));
    return false;
  }

  // first send all the floating data
  if (m_actPage == 0) {
    for (size_t i = 0; i < m_structures->m_state->m_blocksList.size(); ++i) {
      shared_ptr<MacWrtProStructuresInternal::Block> block = m_structures->m_state->m_blocksList[i];
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

std::vector<int> MacWrtProStructuresListenerState::getPageBreaksPos() const
{
  std::vector<int> res;
  if (!m_structures || !m_isMainZone) return res;
  for (size_t i = 0; i < m_structures->m_state->m_blocksList.size(); ++i) {
    shared_ptr<MacWrtProStructuresInternal::Block> block = m_structures->m_state->m_blocksList[i];
    if (block->m_type != 5) continue;
    if (block->m_textPos) res.push_back(block->m_textPos);
  }
  return res;
}

// ----------- character function ---------------------
void MacWrtProStructuresListenerState::sendChar(char c)
{
  bool newPageDone = m_newPageDone;
  m_newPageDone = false;
  if (!m_structures) return;
  MWAWTextListenerPtr listener=m_structures->getTextListener();
  if (!listener) return;
  switch (c) {
  case 0:
    break; // ignore
  case 3: // footnote ok
  case 4: // figure ok
  case 5: // hyphen ok
    break;
  case 7:
    if (m_structures->version()==0) {
      m_actTab = 0;
      listener->insertEOL(true);
    }
    else {
      MWAW_DEBUG_MSG(("MacWrtProStructuresListenerState::sendChar: Find odd char 0x7\n"));
    }
    break;
  case 0x9:
    if (m_actTab++ < m_numTab)
      listener->insertTab();
    else
      listener->insertChar(' ');
    break;
  case 0xa:
    m_actTab = 0;
    if (newPageDone) break;
    listener->insertEOL();
    break; // soft break
  case 0xd:
    m_actTab = 0;
    if (newPageDone) break;
    listener->insertEOL();
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
      else if (listener)
        listener->insertBreak(MWAWTextListener::ColumnBreak);
    }
    break;
  case 0xe:
    m_actTab = 0;
    if (!m_isMainZone) break;

    // create a new section here
    if (listener->isSectionOpened())
      listener->closeSection();
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
  default:
    listener->insertCharacter((unsigned char)c);
    break;
  }
}

bool MacWrtProStructuresListenerState::resendAll()
{
  sendParagraph(*m_paragraph);
  sendFont(*m_font);
  return true;
}


// ----------- font function ---------------------
bool MacWrtProStructuresListenerState::sendFont(int id)
{
  if (!m_structures) return false;
  if (!m_structures->getTextListener()) return true;
  if (id < 0 || id >= int(m_structures->m_state->m_fontsList.size())) {
    MWAW_DEBUG_MSG(("MacWrtProStructuresListenerState::sendFont: can not find font %d\n", id));
    return false;
  }
  sendFont(m_structures->m_state->m_fontsList[(size_t)id]);

  return true;
}

void MacWrtProStructuresListenerState::sendFont(MacWrtProStructuresInternal::Font const &font)
{
  if (!m_structures || !m_structures->getTextListener())
    return;

  m_structures->getTextListener()->setFont(font.m_font);
  *m_font = font;
  m_font->m_font =  m_structures->getTextListener()->getFont();
}

std::string MacWrtProStructuresListenerState::getFontDebugString(int fId)
{
  if (!m_structures) {
    MWAW_DEBUG_MSG(("MacWrtProStructuresListenerState::getFontDebugString: can not find structures\n"));
    return "";
  }

  std::stringstream s;
  if (fId < 0 || fId >= int(m_structures->m_state->m_fontsList.size())) {
    MWAW_DEBUG_MSG(("MacWrtProStructuresListenerState::getFontDebugString: can not find font %d\n", fId));
    s << "C" << fId << "(unknown##)";
    return s.str();
  }

  s << "C" << fId << ":";
  s << m_structures->m_state->m_fontsList[(size_t)fId].m_font.getDebugString
    (m_structures->m_parserState->m_fontConverter)
    << m_structures->m_state->m_fontsList[(size_t)fId] << ",";

  return s.str();
}

// ----------- paragraph function ---------------------
bool MacWrtProStructuresListenerState::sendParagraph(int id)
{
  if (!m_structures) return false;
  if (!m_structures->getTextListener()) return true;
  if (id < 0 || id >= int(m_structures->m_state->m_paragraphsList.size())) {
    MWAW_DEBUG_MSG(("MacWrtProStructuresListenerState::sendParagraph: can not find paragraph %d\n", id));
    return false;
  }

  sendParagraph(m_structures->m_state->m_paragraphsList[(size_t)id]);
  return true;
}

void MacWrtProStructuresListenerState::sendParagraph(MacWrtProStructuresInternal::Paragraph const &para)
{
  if (!m_structures || !m_structures->getTextListener())
    return;
  *m_paragraph = para;
  m_structures->getTextListener()->setParagraph(para);
  m_numTab = int(para.m_tabs->size());
}


std::string MacWrtProStructuresListenerState::getParagraphDebugString(int pId)
{
  if (!m_structures) return "";

  std::stringstream s;
  if (pId < 0 || pId >= int(m_structures->m_state->m_paragraphsList.size())) {
    MWAW_DEBUG_MSG(("MacWrtProStructuresListenerState::getParagraphDebugString: can not find paragraph %d\n", pId));
    s << "C" << pId << "(unknown##)";
    return s.str();
  }

  s << "P" << pId << ":";
  s << m_structures->m_state->m_paragraphsList[(size_t)pId] << ",";
  return s.str();
}

// ----------- section function ---------------------
void MacWrtProStructuresListenerState::sendSection(int nSection)
{

  if (!m_structures) return;
  MWAWTextListenerPtr listener=m_structures->getTextListener();
  if (!listener) return;
  if (listener->isSectionOpened()) {
    MWAW_DEBUG_MSG(("MacWrtProStructuresListenerState::sendSection: a section is already opened\n"));
    listener->closeSection();
  }
  if (m_structures->version()==0) {
    m_numCols = m_structures->m_mainParser.numColumns();
    if (m_numCols > 10) {
      MWAW_DEBUG_MSG(("MacWrtProStructuresListenerState::sendSection: num columns is to big, reset to 1\n"));
      m_numCols = 1;
    }
    MWAWSection sec;
    if (m_numCols>1)
      sec.setColumns(m_numCols, m_structures->m_mainParser.getPageWidth()/double(m_numCols), librevenge::RVNG_INCH);
    listener->openSection(sec);
    return;
  }

  if (nSection >= int(m_structures->m_state->m_sectionsList.size())) {
    MWAW_DEBUG_MSG(("MacWrtProStructuresListenerState::sendSection: can not find section %d\n", nSection));
    return;
  }
  MacWrtProStructuresInternal::Section const &section =
    m_structures->m_state->m_sectionsList[(size_t)nSection];
  if (nSection && section.m_start != section.S_Line) newPage();

  listener->openSection(section.getSection());
  m_numCols = listener->getSection().numColumns();
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
