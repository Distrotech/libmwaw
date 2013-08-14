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

#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>

#include <libwpd/libwpd.h>

#include "MWAWCell.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWSection.hxx"
#include "MWAWTable.hxx"

#include "FWParser.hxx"
#include "FWStruct.hxx"

#include "FWText.hxx"

/** Internal: the structures of a FWText */
namespace FWTextInternal
{
/** Internal: class to store a para modifier with appear in docInfo */
struct ParaModifier {
  //! constructor
  ParaModifier() : m_beforeSpacing(0), m_afterSpacing(0), m_extra("") {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, ParaModifier const &p) {
    if (p.m_beforeSpacing<0)
      o << "befSpacing=" << -p.m_beforeSpacing << "pt,";
    else if (p.m_beforeSpacing>0)
      o << "befSpacing=" << p.m_beforeSpacing << "%,";
    if (p.m_afterSpacing<0)
      o << "aftSpacing=" << -p.m_afterSpacing << "pt,";
    else if (p.m_afterSpacing>0)
      o << "aftSpacing=" << p.m_afterSpacing << "%,";
    o << p.m_extra;
    return o;
  }
  //! the before spacing ( negative in point, positive in percent )
  float m_beforeSpacing;
  //! the after spacing ( negative in point, positive in percent )
  float m_afterSpacing;
  //! some extra data
  std::string m_extra;
};

/** Internal: class to store a font/para modifier with appear in text data */
struct DataModifier {
  //! constructor
  DataModifier() : m_color(MWAWColor::black()), m_extra("") {
    for (int i = 0; i < 4; i++) m_data[i]=0xFFFF;
  }
  //! returns the superscript value ( negative in pt, position in li)
  float getSuper() const {
    return float(int32_t((m_data[0]<<16)|m_data[1]))/65536.f;
  }
  //! returns the sub value ( negative in pt, position in li)
  float getSub() const {
    return float(int32_t((m_data[2]<<16)|m_data[3]))/65536.f;
  }
  //! returns the border id
  int getBorderId() const {
    return m_data[2]==0xFFFF?0:m_data[2];
  }
  //! returns the document extra id
  int getDocParaId() const {
    return m_data[3];
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, DataModifier const &m) {
    if (!m.m_color.isBlack())
      o << "col=" << m.m_color << ",";
    if (m.m_data[1]!=0xFFFF)
      o << "sup=" << m.getSuper() << ",";
    if (m.m_data[3]!=0xFFFF)
      o << "subs=" << m.getSub() << ",";
    for (int i=0; i<2; i++) {
      if (m.m_data[i] != 0xFFFF)
        o << "f" << i << "=" << std::hex << m.m_data[i] << std::dec << ",";
    }
    if (m.m_data[2] && m.m_data[2]<20)
      o << "bordId=" << m.m_data[2] << ",";
    if (m.m_data[3] && m.m_data[3]<20)
      o << "paraId=" << m.m_data[3] << ",";
    o << m.m_extra;
    return o;
  }
  //! the color
  MWAWColor m_color;
  //! the data
  int m_data[4];
  //! extra data
  std::string m_extra;
};

struct Zone;

/** Internal: class to store an item state */
struct Item {
  /** the different type of id */
  enum Type { Father=0, Child, Next, Prev, Main };
  /** constructor */
  Item(): m_level(0), m_index(1), m_collapsed(false), m_hidden(false), m_childList(), m_hiddenZone(), m_extra("") {
    for (int i = 0; i < 5; i++) m_structId[i]=0;
  }
  //! return a value which can be used to represent the label(changme)
  std::string label() const {
    if (m_level <= 0) return "";
    std::stringstream s;
    s << m_index << ". ";
    return s.str();
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Item const &it) {
    if (it.m_hidden) o << "hidden,";
    if (it.m_collapsed) o << "collapsed,";
    if (it.m_level) o << "level=" << it.m_level << ",";
    if (it.m_index!=1) o << "index=" << it.m_index << ",";
    char const *(wh[5])= {"father", "child", "next", "prev", "main"};
    o << "zId=[";
    for (int i = 0; i < 5; i++) {
      if (it.m_structId[i]) o << wh[i] << "=" << it.m_structId[i] << ",";
    }
    o << "],";
    o << it.m_extra;
    return o;
  }
  //! the level
  int m_level;
  //! the actual index
  int m_index;
  //! true if the item is hidden
  bool m_collapsed;
  //! true if the item is hidden
  bool m_hidden;
  //! the list of childlist
  std::vector<int> m_childList;
  //! the hidden item zone
  shared_ptr<Zone> m_hiddenZone;
  //! the item id in text struct zone ( father, child, next, prev, main )
  int m_structId[5];
  //! extra data
  std::string m_extra;
};

/** Internal: class to store a font and it state */
struct Font {
  /** constructor */
  Font() : m_font(), m_modifier(), m_defModifier(true), m_item() {
    for (int i = 0; i < 128; i++) m_state[i]=false;
  }
  /** update the font using the modifier */
  void update();
  /** the font */
  MWAWFont m_font;
  /** the rendering state */
  bool m_state[128];
  /** the modifier data */
  DataModifier m_modifier;
  /** a flag to know if the data modifier is default */
  bool m_defModifier;
  /** the index */
  Item m_item;
};

void Font::update()
{
  if (m_state[9])
    m_font.setColor(m_modifier.m_color);
  else
    m_font.setColor(MWAWColor(0,0,0));
  if (m_state[0xa]) {
    if (m_defModifier)
      m_font.set(MWAWFont::Script::super100());
    else {
      float sup = m_modifier.getSuper();
      if (sup < 0)
        m_font.set(MWAWFont::Script(float(-sup),WPX_POINT));
      else
        m_font.set(MWAWFont::Script(float(sup*100.f),WPX_PERCENT));
    }
  } else if (m_state[0xb]) {
    if (m_defModifier)
      m_font.set(MWAWFont::Script::sub100());
    else {
      float sub = m_modifier.getSub();
      if (sub < 0)
        m_font.set(MWAWFont::Script(float(sub),WPX_POINT));
      else
        m_font.set(MWAWFont::Script(float(-sub*100.f),WPX_PERCENT));
    }
  } else
    m_font.set(MWAWFont::Script());
}

/** Internal: class to store the LineHeader */
struct LineHeader {
  /** Constructor */
  LineHeader() : m_numChar(0), m_font(), m_fontSet(false), m_height(-1.0), m_prevHeight(-1.0), m_textIndent(0), m_extra("") {
  }
  //! try to find the line height using m_height or m_prevHeight
  float height() const {
    if (m_height > 0) return m_height;
    return m_prevHeight;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, LineHeader const &line) {
    o << "numC=" << line.m_numChar << ",";
    if (line.m_fontSet)
      o << "font=[fId=" << line.m_font.id()
        << ",fSize=" << line.m_font.size() << "],";
    if (line.m_height > 0) o << "height=" << line.m_height << ",";
    if (line.m_textIndent.isSet()) o << "textIndent=" << *line.m_textIndent << ",";
    o << line.m_extra;
    return o;
  }
  /** the number of char */
  int m_numChar;
  /** the font */
  MWAWFont m_font;
  /** a flag to know if the font is set */
  bool m_fontSet;
  /** the line height in point ( if known) */
  float m_height;
  /** the previous line height in point ( if known) */
  float m_prevHeight;
  /** the text indent in inches ( if known) */
  Variable<double> m_textIndent;
  /** extra data */
  std::string m_extra;
};
/** Internal: class to store a ColumnInfo */
struct ColumnInfo {
  ColumnInfo() : m_column(0), m_box(), m_beginPos(1) {
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, ColumnInfo const &c) {
    if (c.m_column > 0) o << "col=" << c.m_column+1 << ",";
    o << c.m_box << ",";
    if (c.m_beginPos > 1) o << "textPos=" << c.m_beginPos << ",";
    return o;
  }
  //! the column number
  int m_column;
  //! the bdbox
  Box2i m_box;
  //! the first data
  int m_beginPos;
};

struct PageInfo {
  PageInfo() : m_page(-1), m_columns() {
  }

  //! returns true if the page has same color position
  bool isSimilar(PageInfo const &p) const {
    size_t numColumns = m_columns.size();
    if (numColumns != p.m_columns.size())
      return false;
    for (size_t c = 0; c < numColumns; c++) {
      if (m_columns[c].m_box[0].x() != p.m_columns[c].m_box[0].x())
        return false;
      if (m_columns[c].m_box[1].x() != p.m_columns[c].m_box[1].x())
        return false;
    }
    return true;
  }

  //! return a section
  MWAWSection getSection() const {
    MWAWSection sec;
    size_t numC = m_columns.size();
    if (numC <= 1)
      return sec;
    sec.m_columns.resize(numC);
    for (size_t c=0; c < numC; c++) {
      MWAWSection::Column &col=sec.m_columns[c];
      int prevPos= c ? (m_columns[c].m_box[0].x()+m_columns[c-1].m_box[1].x())/2 :
                   m_columns[c].m_box[0].x();
      int nextPos= c+1!=numC ? (m_columns[c+1].m_box[0].x()+m_columns[c].m_box[1].x())/2 :
                   m_columns[c].m_box[1].x();
      col.m_width = double(nextPos-prevPos);
      col.m_widthUnit = WPX_POINT;
      col.m_margins[libmwaw::Left]=double(m_columns[c].m_box[0].x()-prevPos)/72.;
      col.m_margins[libmwaw::Right]=double(nextPos-m_columns[c].m_box[1].x())/72.;
    }
    return sec;
  }
  //! the pages
  int m_page;
  //! the columns
  std::vector<ColumnInfo> m_columns;
};

/** Internal: class to store a text zone */
struct Zone {
  //! the zone type
  enum ZoneType { Normal, Main, CollapsedItem };
  //! constructor
  Zone() : m_zone(), m_box(), m_begin(-1), m_end(-1), m_zoneType(Normal), m_pagesInfo(), m_extra("") {
    for (int i = 0; i < 2; i++) m_flags[i] = 0;
    for (int i = 0; i < 2; i++) m_pages[i] = 0;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Zone const &z) {
    switch (z.m_zoneType) {
    case Zone::Normal:
      break;
    case Zone::Main:
      o << "Main,";
      break;
    case Zone::CollapsedItem:
      o << "Collapsed[item],";
      break;
    default:
      o << "#type=" << z.m_zoneType << ",";
      break;
    }
    if (z.m_zone) o << *z.m_zone << ",";
    if (z.m_pages[0]) o << "firstP=" << z.m_pages[0] << ",";
    if (z.m_pages[1] && z.m_pages[1] != z.m_pages[0])
      o << "lastP=" << z.m_pages[1] << ",";
    o << "Box=" << z.m_box << ",";
    for (int i = 0; i < 2; i++) {
      if (z.m_flags[i])
        o << "fl" << i << "=" << z.m_flags[i] << ",";
    }
    if (z.m_end!=z.m_begin)
      o << "sz=" << std::hex << z.m_end-z.m_begin << std::dec << ",";
    if (z.m_extra.length())
      o << "extra=[" << z.m_extra << "],";
    return o;
  }
  //! return the col/page break
  std::vector<int> getBreaksPosition() const {
    size_t numPages = m_pagesInfo.size();
    int prevPos = 0;
    std::vector<int> res;
    for (size_t p = 0; p < numPages; p++) {
      PageInfo const &page = m_pagesInfo[p];
      for (size_t c = 0; c < page.m_columns.size(); c++) {
        int pos = page.m_columns[c].m_beginPos;
        if (pos < prevPos) {
          MWAW_DEBUG_MSG(("FWTextInternal::Zone::getBreaksPosition pos go back\n"));
          p = numPages;
          break;
        }
        res.push_back(pos);
        prevPos = pos;
      }
    }
    return res;
  }
  //! the main zone
  FWStruct::EntryPtr m_zone;
  //! the bdbox
  Box2f m_box;

  //! the beginning of the text data
  long m_begin;
  //! the end of the text data
  long m_end;
  //! the zone type
  ZoneType m_zoneType;
  //! the zone flags, header|footer, normal|extra
  int m_flags[2];
  //! the pages
  int m_pages[2];
  //! the pages info
  std::vector<PageInfo> m_pagesInfo;
  //! the extra data ( for debugging )
  std::string m_extra;
};

/** Internal: class to store the paragraph properties */
struct Paragraph : public MWAWParagraph {
  //! Constructor
  Paragraph() : MWAWParagraph(), m_align(0), m_interSpacing(1.), m_interSpacingUnit(WPX_PERCENT),
    m_dim(0,0), m_border(), m_isTable(false), m_tableBorderId(0), m_tableFlags(), m_actCol(-1), m_isSent(false) {
    m_befAftSpacings[0]=m_befAftSpacings[1]=0;
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Paragraph const &ind) {
    if (ind.m_isTable) o << "table,";
    if (ind.m_tableBorderId) o << "borderId[table]=" << ind.m_tableBorderId << ",";
    if (ind.m_align) o << "align=" << ind.m_align << ",";
    if (ind.m_dim[0]>0 || ind.m_dim[1]>0) o << "dim=" << ind.m_dim << ",";
    o << reinterpret_cast<MWAWParagraph const &>(ind);
    return o;
  }

  //! returns true if this is a table
  bool isTable() const {
    return m_isTable;
  }

  //! set the align type
  void setAlign(int align) {
    m_align = align;
    m_isSent = false;
  }
  //! set the interline spacing
  void setInterlineSpacing(double spacing, WPXUnit unit) {
    m_interSpacing = spacing;
    m_interSpacingUnit = unit;
    m_isSent = false;
  }
  //! set the before/after spacing ( negative in point, positive in percent )
  void setSpacings(double spacing, bool before ) {
    m_befAftSpacings[before ? 0 : 1] = spacing;
    m_isSent = false;
  }
  //! set the border type
  void setBorder(FWStruct::Border border) {
    m_border = border;
    m_isSent = false;
  }

  //! update the paragraph data from a ruler
  void updateFromRuler(Paragraph const &ruler) {
    MWAWParagraph::operator=(ruler);
    m_isTable = ruler.m_isTable;
    m_tableBorderId = ruler.m_tableBorderId;
    m_tableFlags = ruler.m_tableFlags;
    m_dim = ruler.m_dim;
    m_isSent = false;
  }
  //! returns the table dimension in points
  bool getTableDimensions(std::vector<float> &dim) const {
    size_t numTabs = m_tabs->size();
    if ((numTabs%2) != 1 || numTabs != m_tableFlags.size()) {
      MWAW_DEBUG_MSG(("FWTextInternal::Paragraph:getTableDimensions: unexpected number of tabs\n"));
      return false;
    }
    if (m_dim[0] <= 0) {
      MWAW_DEBUG_MSG(("FWTextInternal::Paragraph:getTableDimensions: can not determine line width\n"));
      return false;
    }
    std::vector<double> limits;
    limits.push_back(*m_margins[1]);
    for (size_t i=1; i < numTabs; i+=2) {
      if (m_tableFlags[i] != 4) {
        MWAW_DEBUG_MSG(("FWTextInternal::Paragraph:getTableDimensions: find unexpected tables flags\n"));
        return false;
      }
      limits.push_back((*m_tabs)[i].m_position);
    }
    limits.push_back(double(m_dim[0])-*m_margins[2]);
    dim.resize(limits.size()-1);
    for (size_t i=0; i < dim.size(); i++)
      dim[i] = 72.f*float(limits[i+1]-limits[i]);
    return true;
  }
  //! update the paragraph data to be sent to a listener
  MWAWParagraph updateToSent() const {
    m_isSent = true;
    MWAWParagraph res = *this;
    if ( m_interSpacing>0)
      res.setInterline(m_interSpacing, m_interSpacingUnit);
    for (int i = 0; i < 2; i++) {
      if (m_befAftSpacings[i] <= 0)
        res.m_spacings[i+1]=(-m_befAftSpacings[i])/72.0;
      else // ok suppose line of height 9
        res.m_spacings[i+1]=(m_befAftSpacings[i])*9.0/72.0;
    }
    if (!m_border.m_backColor.isWhite())
      res.m_backgroundColor = m_border.m_backColor;

    if (m_isTable && m_actCol >= 0) {
      if (2*m_actCol < int(m_tableFlags.size())) {
        switch(m_tableFlags[size_t(2*m_actCol)]) {
        case 0:
          res.m_justify = MWAWParagraph::JustificationLeft;
          break;
        case 1:
          res.m_justify = MWAWParagraph::JustificationCenter;
          break;
        case 2:
        case 5:
          res.m_justify = MWAWParagraph::JustificationFull;
          break;
        case 3:
          res.m_justify = MWAWParagraph::JustificationRight;
          break;
        case 6:
          res.m_justify = MWAWParagraph::JustificationFullAllLines;
          break;
        default:
          break;
        }
      }
      res.m_tabs->resize(0);
      m_actCol=-1;
      return res;
    }
    switch(m_align) {
    case 0:
      res.m_justify = MWAWParagraph::JustificationLeft;
      break;
    case 1:
      res.m_justify = MWAWParagraph::JustificationCenter;
      break;
    case 2:
      res.m_justify = MWAWParagraph::JustificationFull;
      break;
    case 3:
      res.m_justify = MWAWParagraph::JustificationRight;
      break;
    default:
      break;
    }
    res.m_borders = m_border.getParagraphBorders();
    return res;
  }

  //! the align value
  int m_align;
  //! the spacing
  double m_interSpacing;
  //! the spacing unit
  WPXUnit m_interSpacingUnit;
  //! the before/after spacing ( negative in point, positive in percent)
  double m_befAftSpacings[2];
  //! the zone dimension
  Vec2f m_dim;
  //! the actual border
  FWStruct::Border m_border;
  //! a flag to know if this is a table
  bool m_isTable;
  //! the table border id
  int m_tableBorderId;
  //! the list of table limit
  std::vector<int> m_tableFlags;
  //! the index of the actual column to send
  mutable int m_actCol;
  //! a flag to know if the parser is send or not
  mutable bool m_isSent;
};

////////////////////////////////////////
//! Internal: the state of a FWText
struct State {
  //! constructor
  State() : m_version(-1), m_entryMap(), m_paragraphMap(), m_itemMap(), m_dataModMap(),
    m_paragraphModList(), m_mainZones(), m_numPages(1), m_actualPage(0) {
  }

  //! the file version
  mutable int m_version;

  //! zoneId -> entry
  std::multimap<int, shared_ptr<Zone> > m_entryMap;
  //! rulerId -> ruler
  std::map<int, Paragraph> m_paragraphMap;
  //! itemId -> item
  std::map<int, Item> m_itemMap;
  //! modId -> font/paragraph modifier
  std::map<int, DataModifier> m_dataModMap;
  //! a list of paragraph modifier
  std::vector<ParaModifier> m_paragraphModList;

  //! the main zone index
  std::vector<int> m_mainZones;

  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;
};
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
FWText::FWText(FWParser &parser) :
  m_parserState(parser.getParserState()), m_state(new FWTextInternal::State),
  m_mainParser(&parser)
{
}

FWText::~FWText()
{ }

int FWText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}

int FWText::numPages() const
{
  return m_state->m_numPages;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////
void FWText::send(shared_ptr<FWTextInternal::Zone> zone, int numChar,
                  FWTextInternal::Font &font, FWTextInternal::Paragraph &ruler,
                  std::string &str)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) return;
  MWAWInputStreamPtr input = zone->m_zone->m_input;
  long pos = input->tell();
  long endPos = pos+numChar;
  bool nextIsChar = false;
  bool fCharSent = false, lastEOL=false, fontSet = false, checkModifier = false;

  libmwaw::DebugStream f;
  for (int i = 0; i < numChar; i++) {
    long actPos = input->tell();
    if (actPos >= endPos)
      break;
    int val = (int)input->readULong(1);
    bool done = false;
    int id=-1;
    if (nextIsChar)
      nextIsChar = false;
    else {
      done = true;
      uint32_t fFlags = font.m_font.flags();
      bool on = false;
      if (val >= 0x80)
        on=font.m_state[val-0x80]=!font.m_state[val-0x80];
      std::string onString(on?"":"/");
      switch(val) {
      case 0:
        val=' ';
        done = false;
        break; // space
      case 0x80:
        f << "[" << onString << "80]";
        break; // often found by pair consecutively : selection ?
      case 0x81:
        f << "[" << onString << "81]";
        break; // often found by pair around a " " a ","...
      case 0x82:
        f << "[" << onString << "82]";
        break; // find one time around a first capital letter I
      case 0x83:
        f << "[" << onString << "b]";
        fFlags ^= MWAWFont::boldBit;
        font.m_font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x84:
        f << "[" << onString << "i]";
        fFlags ^= MWAWFont::italicBit;
        font.m_font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x86:
        f << "[" << onString << "outline]";
        fFlags ^= MWAWFont::outlineBit;
        font.m_font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x87:
        f << "[" << onString << "shadow]";
        fFlags ^= MWAWFont::shadowBit;
        font.m_font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x88:
        f << "[" << onString << "smallcap]";
        fFlags ^= MWAWFont::smallCapsBit;
        font.m_font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x89:
        f << "[" << onString << "color]";
        checkModifier=true;
        break;
      case 0x8a:
        f << "[" << onString << "super]";
        checkModifier=true;
        break;
      case 0x8b: // subs
        f << "[" << onString << "sub]";
        checkModifier=true;
        break;
      case 0x8c:
        f << "[" << onString << "strike]";
        if (on)
          font.m_font.setStrikeOutStyle(MWAWFont::Line::Simple);
        else
          font.m_font.setStrikeOutStyle(MWAWFont::Line::None);
        fontSet=false;
        break;
      case 0x90: // condensed
      case 0x91: // expand
        f << "[" << onString << (val==0x90?"condensed":"expand") << "]";
        font.m_font.setDeltaLetterSpacing
        (float((font.m_state[0x10]?-1:0)+(font.m_state[0x11]?1:0)));
        fontSet=false;
        break;
      case 0x85: // normal underline
      case 0x8e: // word underline
      case 0x8f: // double
      case 0x92: { // dot
        switch(val) {
        case 0x85:
          f << "[" << onString << "underline]";
          break;
        case 0x8e:
          f << "[" << onString << "underline:word]";
          break;
        case 0x8f:
          f << "[" << onString << "underline:double]";
          break;
        default:
        case 0x92:
          f << "[" << onString << "underline:dot]";
          break;
        }
        if (font.m_state[0x12])
          font.m_font.setUnderlineStyle(MWAWFont::Line::Dot);
        else if (font.m_state[0x5]||font.m_state[0xe]|| font.m_state[0xf])
          font.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
        else
          font.m_font.setUnderlineStyle(MWAWFont::Line::None);
        if (font.m_state[0xf])
          font.m_font.setUnderlineType(MWAWFont::Line::Double);
        if (font.m_state[0xe])
          font.m_font.setUnderlineWordFlag(true);
        fontSet=false;
        break;
      }
      case 0x93:
        f << "[" << onString << "overline]";
        if (on)
          font.m_font.setOverlineStyle(MWAWFont::Line::Simple);
        else
          font.m_font.setOverlineStyle(MWAWFont::Line::None);
        fontSet=false;
        break;
      case 0x94:
        f << "[" << onString << "allCaps]";
        fFlags ^= MWAWFont::allCapsBit;
        font.m_font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0x95:
        f << "[" << onString << "lowercase]";
        fFlags ^= MWAWFont::lowercaseBit;
        font.m_font.setFlags(fFlags);
        fontSet=false;
        break;
      case 0xa7: // fixme appear also as tabs separator
        f << "[tab]";
        listener->insertTab();
        break;
      case 0xac:
        // we must wait sending font/ruler then we will send the data
        done = false;
        break;
      case 0xae: // insecable-
        f << "-";
        val=0x2011;
        done = false;
        break;
      case 0xb2:
        if (actPos+1 > endPos) {
          MWAW_DEBUG_MSG(("FWText::send: can not find font size!!!!\n"));
          f << "[##fSz]";
          break;
        }
        font.m_font.setSize((float)input->readULong(1));
        f << "[fSz=" << font.m_font.size() << "]";
        fontSet=false;
        break;
      case 0xb3:
        nextIsChar = true;
        break;
      case 0xc1:
        if (actPos+2 > endPos) {
          MWAW_DEBUG_MSG(("FWText::send: can not find font id!!!!\n"));
          f << "[##fId]";
          break;
        }
        font.m_font.setId((int)input->readULong(2));
        f << "[fId=" << font.m_font.size() << "]";
        fontSet = false;
        break;
      case 0xc7: // item id
        if (actPos+2 > endPos) {
          MWAW_DEBUG_MSG(("FWText::send: can not find item id!!!!\n"));
          f << "[##item]";
          break;
        }
        if (font.m_item.m_collapsed && font.m_item.m_childList.size()) {
          if (!lastEOL)
            listener->insertEOL(false);
          FWTextInternal::Font hFont = font;
          FWTextInternal::Paragraph hRuler = ruler;
          for (size_t c=0; c < font.m_item.m_childList.size(); c++)
            sendHiddenItem(font.m_item.m_childList[c], hFont, hRuler);
          fontSet = false;
          ruler.m_isSent=false;
          lastEOL=false;
        }
        id = (int)input->readULong(2);
        if (m_state->m_itemMap.find(id) == m_state->m_itemMap.end()) {
          MWAW_DEBUG_MSG(("FWText::send: can not find item id!!!!\n"));
          font.m_item = FWTextInternal::Item();
          f << "[#itemId=" << id << "]";
        } else {
          font.m_item =m_state->m_itemMap.find(id)->second;
          f << "[item:" << font.m_item << ",id=" << id << "]";
        }
        break;
      case 0xcb: { // space/and or color
        if (actPos+2 > endPos) {
          MWAW_DEBUG_MSG(("FWText::send: can not read data modifier!!!!\n"));
          f << "[##dMod]";
          break;
        }
        id = (int)input->readULong(2);
        if (m_state->m_dataModMap.find(id) == m_state->m_dataModMap.end()) {
          font.m_modifier=FWTextInternal::DataModifier();
          font.m_defModifier=true;
          f << "[#modifier=" << id << "]";
        } else {
          font.m_modifier=m_state->m_dataModMap.find(id)->second;
          f << "[modifier=" << font.m_modifier << "]";
          font.m_defModifier=false;
        }

        // can be a paragraph modifier
        if (fCharSent) break;
        int pId = font.m_modifier.getDocParaId();
        if (pId >= 0 && pId < int(m_state->m_paragraphModList.size())) {
          FWTextInternal::ParaModifier const &paraMod =
            m_state->m_paragraphModList[size_t(pId)];
          ruler.setSpacings(paraMod.m_beforeSpacing, true);
          ruler.setSpacings(paraMod.m_afterSpacing, false);
          ruler.setBorder(FWStruct::Border());
          checkModifier = true;
        }
        int bId = font.m_modifier.getBorderId();
        FWStruct::Border border;
        if (m_mainParser->getBorder(bId, border)) {
          ruler.setBorder(border);
          checkModifier = true;
        }
        break;
      }
      case 0xd2:
      case 0xd3:
      case 0xd5:
      case 0xdc:
      case 0xe1:
      case 0xe2:
      case 0xe4:
        if (actPos+2 > endPos) {
          MWAW_DEBUG_MSG(("FWText::send: can not an id for %x!!!!\n", val));
          f << "[#" << std::hex << val << std::dec << "]";
          break;
        }
        id = (int)input->readULong(2);
        done = false; // we must wait sending font/ruler then we will send the data
        switch(val) {
        case 0xd2:
          f << "[noteId=" << id << "]";
          break;
        case 0xd3:
          f << "[footnoteId=" << id << "]";
          break;
        case 0xd5:
          f << "[endnoteId=" << id << "]";
          break;
        case 0xdc:
          f << "[graphId=" << id << "]";
          break;
        case 0xe1: // reference field
          done=true;
          f << "[refDefId=" << id << "]";
          break;
        case 0xe2:
          f << "[refId=" << id << "]";
          break;
        case 0xe4:
          f << "[variableId=" << id << "]";
          break;
        default:
          break;
        }
        break;
      case 0xa0: // potential hypen break
        f << "[hyp]";
        if (actPos+1==endPos) {
          val = '-';
          done = false;
        }
        break;
      case 0x98: // soft break
        f << "\\n";
        done = false;
        break;
      case 0x96: // field end
        f << "[FEnd]";
        break;
      case 0x9b: // align
      case 0x9c:
        ruler.setAlign(ruler.m_align^(val-0x9a));
        f << "[" << onString << "align" << (val-0x9b) << "]";
        break;
      case 0x9d: // justication
      case 0x9e:
      case 0x9f: {
        float justify=1.0f+float(val-0x9d)/2.0f;
        ruler.setInterlineSpacing(justify, WPX_PERCENT);
        f << "[just=" << 100*justify << "%]";
        break;
      }
      case 0xa4:
        f << "emptyItem?,";
        break;
      case 0xa8:
        f << "\\n";
        done = false;
        break;
      case 0xab: // begin of line
        f << "\\{";
        break;
      case 0xc6: { // ruler id
        if (actPos+2 > endPos) {
          MWAW_DEBUG_MSG(("FWText::send: can find rulerId!!!!\n"));
          f << "[##rulerId]";
          break;
        }
        id = int(input->readULong(2));
        f << "[P" << id << "]";

        std::map<int,FWTextInternal::Paragraph>::iterator it=
          m_state->m_paragraphMap.find(id);
        if (it==m_state->m_paragraphMap.end()) {
          MWAW_DEBUG_MSG(("FWText::send: can not find paragraph with id=%d\n", id));
          break;
        }
        ruler.updateFromRuler(it->second);
        // do not add a break line for a simple ruler
        if (i<=1)
          lastEOL=true;
        break;
      }
      case 0xe8: // justification
        if (actPos+4 > endPos) {
          MWAW_DEBUG_MSG(("FWText::send: can not find justify!!!!\n"));
          f << "[#justWithType]";
        } else {
          int just = (int)input->readLong(2);
          if (just < 0)
            ruler.setInterlineSpacing(-just, WPX_POINT);
          f << "[just=" << just << ",";
          f << "typ?=" << (int)input->readLong(2) << "]";
        }
        break; // ok
      case 0x8d: // unknown
      case 0x97:
      case 0x9a:
      case 0xa1:
      case 0xa9:
      case 0xaa:
        f << "[" << onString << std::hex << val << std::dec << "]";
        break;
      case 0xca: // unknown
      case 0xcd:
      case 0xd0: // header
      case 0xd1: // footer ?
      case 0xd4: // contents
      case 0xd6: // biblio
      case 0xd7: // entry
      case 0xd9:
      case 0xda:
      case 0xe5: // contents/index data
        if (actPos+2 > endPos) {
          MWAW_DEBUG_MSG(("FWText::send: can find id for %x data!!!!\n", val));
          f << "[##" << std::hex << val << std::dec << "]";
          break;
        }
        id = (int)input->readULong(2);
        if (val==0xd0) f << "[headerId=" << id << "]";
        else if (val==0xd1) f << "[footerId=" << id << "]";
        else f << "[" << std::hex << val << std::dec << "Id=" << val << "]";
        break;
      case 0xe9:
        if (actPos+4 > endPos) {
          MWAW_DEBUG_MSG(("FWText::send: can not find id/val for fe9!!!!\n"));
          f << "[#e9]";
          break;
        }
        id = int(input->readULong(2));
        f << "[e9=" << id << ":" << input->readULong(2) << "]";
        break;
      default:
        done = false;
        break;
      }
    }
    if (checkModifier && (!done || input->tell() == endPos)) {
      font.update();
      fontSet = false;
    }
    if (done)
      continue;

    if (!fCharSent && !ruler.m_isSent)
      listener->setParagraph(ruler.updateToSent());
    if (!fontSet) {
      listener->setFont(font.m_font);
      fontSet = true;
    }
    fCharSent = true;
    lastEOL = false;
    // now, we can try to send the data
    if (id >= 0 || val==0xa8 || val==0x98 || val==0xac) {
      done = true;
      switch(val) {
      case 0x98:
        listener->insertEOL(true);
        break;
      case 0xa8:
        lastEOL = true;
        listener->insertEOL();
        break;
      case 0xac: {
        std::string label=font.m_item.label();
        if (label.length())
          listener->insertUnicodeString(label.c_str());
        break;
      }
      case 0xd2:
        m_mainParser->sendText(id, libmwaw::DOC_COMMENT_ANNOTATION);
        break;
      case 0xd3:
        m_mainParser->sendText(id, libmwaw::DOC_NOTE, MWAWNote::FootNote);
        break;
      case 0xd5:
        m_mainParser->sendText(id, libmwaw::DOC_NOTE, MWAWNote::EndNote);
        break;
      case 0xdc:
        m_mainParser->sendGraphic(id);
        break;
      case 0xe2:
        m_mainParser->sendReference(id);
        break;
      case 0xe4:
        m_mainParser->sendVariable(id);
        break;
      default:
        done = false;
        break;
      }
    }
    if (done)
      continue;
    if (val >= 256)
      listener->insertUnicode((uint32_t) val);
    else {
      i += listener->insertCharacter
           ((unsigned char)val, input, input->tell()+(numChar-1-i));
      if (val <= 0x1f)
        f << "#[" << std::hex << val << "]";
      else
        f << char(val);
    }
  }
  if (!fCharSent && !ruler.m_isSent)
    listener->setParagraph(ruler.updateToSent());
  if (!lastEOL)
    listener->insertEOL(false);
  str=f.str();
}

bool FWText::sendTable(shared_ptr<FWTextInternal::Zone> zone, FWTextInternal::LineHeader const &lHeader,
                       FWTextInternal::Font &font, FWTextInternal::Paragraph &ruler, std::string &str)
{
  std::vector<float> dim;
  if (!ruler.getTableDimensions(dim))
    return false;
  float height=lHeader.height();
  if (height <= 0) {
    MWAW_DEBUG_MSG(("FWText::sendTable: can not find table height\n"));
    return false;
  }

  size_t numCols = dim.size();
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) return false;

  MWAWInputStreamPtr input = zone->m_zone->m_input;
  long pos = input->tell();
  long endPos=pos+lHeader.m_numChar;
  std::vector<long> cellPos;
  cellPos.push_back(pos);
  for (int i = 0; i < lHeader.m_numChar; i++) {
    long actPos = input->tell();
    if (input->atEOS())
      break;
    int c = (int)input->readULong(1);
    if (c==0xa7) {
      cellPos.push_back(actPos);
      cellPos.push_back(actPos+1);
    }
    // special case: an item implies a column shift
    if (c==0xac) {
      cellPos.push_back(actPos+1);
      cellPos.push_back(actPos+1);
    }
  }
  cellPos.push_back(endPos);

  size_t numFind=cellPos.size()/2;
  if (numCols < numFind) {
    if (numCols+1==numFind) {
      // happens one type at the end of file: bug or normal end table?
      MWAW_DEBUG_MSG(("FWText::sendTable: find a spurious column break in last line position\n"));
      cellPos.resize(2*numCols);
    } else {
      MWAW_DEBUG_MSG(("FWText::sendTable: find too many columns\n"));
      return false;
    }
  }

  // ok, we have the limit of the text, let works...
  libmwaw::DebugStream f;
  MWAWTable table(MWAWTable::TableDimBit);
  table.setColsSize(dim);
  listener->openTable(table);
  listener->openTableRow(-height, WPX_POINT);

  MWAWBorder outBorder, vBorder;
  FWStruct::Border border;
  if (m_mainParser->getBorder(ruler.m_tableBorderId, border)) {
    outBorder=FWStruct::Border::getBorder(border.m_type[0]);
    vBorder=FWStruct::Border::getBorder(border.m_type[2]);
    outBorder.m_color=vBorder.m_color=border.m_color[0];
  } else {
    MWAW_DEBUG_MSG(("FWText::sendTable: can not find border=%d\n",ruler.m_tableBorderId));
    outBorder.m_width=vBorder.m_width=0;
  }
  for (size_t col = 0; col < numCols; col++) {
    MWAWCell cell;
    Vec2i cellPosition(Vec2i((int)0,(int)col));
    cell.setPosition(cellPosition);
    if (ruler.m_tableBorderId) {
      cell.setBorders(0xf, outBorder);
      if (col > 0)
        cell.setBorders(libmwaw::LeftBit, vBorder);
      if (col+1 < numCols)
        cell.setBorders(libmwaw::RightBit, vBorder);
    }
    listener->openTableCell(cell);

    if (col < numFind) {
      if (cellPos[2*col+1]>cellPos[2*col]) {
        std::string string;
        input->seek(cellPos[2*col], WPX_SEEK_SET);
        ruler.m_actCol=int(col);
        ruler.m_isSent=false;
        send(zone, int(cellPos[2*col+1]-cellPos[2*col]), font, ruler, string);
        f << string;
      }
    }
    if (col+1 != numCols)
      f << "[col]";
    listener->closeTableCell();
  }
  listener->closeTableRow();
  listener->closeTable();
  input->seek(endPos, WPX_SEEK_SET);
  str=f.str();
  return true;
}

bool FWText::readLineHeader(shared_ptr<FWTextInternal::Zone> zone, FWTextInternal::LineHeader &lHeader)
{
  lHeader = FWTextInternal::LineHeader();

  MWAWInputStreamPtr input = zone->m_zone->m_input;
  libmwaw::DebugStream f;
  long pos = input->tell();

  int type = (int)input->readULong(2);
  int lengthSz = 1;
  if (type & 0x8000)
    lengthSz = 2;

  lHeader.m_numChar = (int)input->readULong(lengthSz);
  if ((lengthSz==1 && (lHeader.m_numChar & 0x80)) ||
      pos+2+lHeader.m_numChar > zone->m_end) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  int val;
  if (type & 0x4000) {
    f << "f0=[";
    lHeader.m_height = float(input->readLong(4))/256.f;
    val = (int)input->readLong(2); // 0 or -1
    if (val == -1) f << "*,";
    else if (val) f << "unkn2=" << val << ",";
    val = (int)input->readLong(2); // small number between 20 and -20?
    if (val) f << "N1=" << float(val)/256.0f << ",";
    val = (int)input->readLong(2);
    if (val)
      lHeader.m_textIndent = (double)val/72.;
    f << "w=" << (int)input->readLong(2) << ",";
    f << "],";
  }
  if (type & 0x2000) {
    // small number between 1 and 4
    f << "f1=" << (int)input->readLong(1) << ",";
  }
  if (type & 0x1000) {
    // small number between 1 and 2
    f << "f2=" << (int)input->readLong(1) << ",";
  }
  if (type & 0x800) {
    // small number between 1 and 2
    f << "f3=" << (int)input->readLong(1) << ",";
  }
  if (type & 0x400) {
    // small number  1
    f << "f4=" << (int)input->readLong(1) << ",";
  }
  if (type & 0x200) {
    // small int between 0 and 0x4f : ident ?
    f << "f5=" << (int)input->readLong(2) << ",";
  }
  if (type & 0x100) {
    // small int between 0 and 0xb0 : ident ?
    f << "f6=" << (int)input->readLong(2) << ",";
  }
  if (type & 0x80) {
    // small int between 0 and 0x5b : ident ?
    f << "f7=" << (int)input->readLong(2) << ",";
  }
  if (type & 0x40) {
    // small int between 0 and 0xcf : link to rulerid in data struct?
    val = (int)input->readLong(2);
    if (val)
      f << "P" << val << ",";
  }
  if (type & 0x20) {
    /* first small number:
       0|10|80|81, 0|1|2|4|10,
       last small number 0|1|...|7|e|10|11|20|21|23|40|41
       &1  line continue ?
       &2  new page ?
    */
    f << "f9=[";
    for (int i = 0; i < 4; i++) {
      val = (int)input->readULong(1);
      if (val) f << std::hex << val << std::dec << ",";
      else f << "_,";
    }
    f << "],";
  }
  if (type & 0x10) {
    int fId = (int)input->readLong(2);
    float fSz = (float)input->readULong(2);
    lHeader.m_fontSet = true;
    lHeader.m_font.setId(fId);
    lHeader.m_font.setSize(fSz);
    f << "id=" << fId << ",";
    f << "sz=" << fSz << ",";
    // negative: point, positive li
    f << "justify=" << (float)(input->readLong(4))/65336.f << ",";
  }
  if (type & 0x8) { // font flag ?
    val = (int)input->readULong(2);
    f << "fa=" << std::hex << val << std::dec << ",";
  }
  if (type & 0x4) {
    MWAW_DEBUG_MSG(("FWText::readLineHeader: find unknown size flags!!!!\n"));
    f << "[#fl&4]";
    // we do not know the size of this field, let try with 2
    input->seek(2, WPX_SEEK_CUR);
  }
  if (type & 0x2) { // 0 or 2
    val = (int)input->readULong(2);
    f << "fb=" << val << ",";
  }
  if (type & 0x1) { // small number between 1 and 1b
    val = (int)input->readLong(2);
    f << "nRows?=" << val << ",";
  }
  lHeader.m_extra = f.str();
  return true;
}

bool FWText::send(shared_ptr<FWTextInternal::Zone> zone)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("FWText::send can not find the listener\n"));
    return false;
  }
  MWAWInputStreamPtr input = zone->m_zone->m_input;
  libmwaw::DebugFile &ascii = zone->m_zone->getAsciiFile();
  libmwaw::DebugStream f;

  zone->m_zone->setParsed(true);

  long pos = zone->m_begin;
  input->seek(pos, WPX_SEEK_SET);
  int num=1;
  FWTextInternal::Font font;
  font.m_font=MWAWFont(3,12);
  FWTextInternal::Paragraph ruler;

  std::vector<int> listBreaks = zone->getBreaksPosition();
  int numBreaks = int(listBreaks.size());
  int nPages = int(zone->m_pagesInfo.size());
  int actBreak = numBreaks, actBreakPos = -1;
  if (numBreaks) {
    actBreak = 0;
    actBreakPos = listBreaks[size_t(actBreak)];
  }
  int actPage = 0, actCol = 0, numCol=1;
  listener->setParagraph(ruler);
  float prevHeight = -1;
  while(1) {
    pos = input->tell();
    bool sendData = false;
    f.str("");
    f << "TextData-a[" << num << "]:";
    while (num==actBreakPos) {
      if (num != 1) sendData = true;
      if (actCol < numCol-1 && numCol > 1) {
        listener->insertBreak(MWAWContentListener::ColumnBreak);
        actCol++;
      } else if (actPage >= nPages) {
        MWAW_DEBUG_MSG(("FWText::send can not find the page information\n"));
      } else {
        FWTextInternal::PageInfo const &page = zone->m_pagesInfo[size_t(actPage)];
        if (sendData) {
          if (zone->m_zoneType == FWTextInternal::Zone::Main)
            m_mainParser->newPage(++m_state->m_actualPage);
          else if (numCol > 1)
            listener->insertBreak(MWAWContentListener::ColumnBreak);
        }
        actCol = 0;

        if (!actPage || !page.isSimilar(zone->m_pagesInfo[size_t(actPage-1)])) {
          MWAWSection section = page.getSection();
          libmwaw::SubDocumentType subdocType;
          int numC = section.numColumns();
          if (listener->isSubDocumentOpened(subdocType) && numC <=1
              && subdocType != libmwaw::DOC_TEXT_BOX)
            ;
          else {
            if (listener->isSectionOpened())
              listener->closeSection();

            listener->openSection(section);
            numCol = numC;
          }
        }

        actPage++;
      }

      if (num != 1) f << "break,";
      sendData = true;
      // update numbreaks
      if (++actBreak < numBreaks)
        actBreakPos = listBreaks[(size_t)actBreak];
      else
        actBreakPos = -1;
    }
    num++;
    FWTextInternal::LineHeader lHeader;
    if (!readLineHeader(zone, lHeader)) {
      input->seek(pos, WPX_SEEK_SET);
      break;
    }
    f << lHeader;
    // update prevHeight
    lHeader.m_prevHeight = prevHeight;
    prevHeight = lHeader.height();

    if (lHeader.m_fontSet) {
      font.m_font.setId(lHeader.m_font.id());
      font.m_font.setSize(lHeader.m_font.size());
    }
    long debPos=input->tell();
    if (lHeader.m_numChar)
      ascii.addDelimiter(debPos,'|');
    long lastPos = debPos+lHeader.m_numChar;
    if (listener) {
      std::string str;
      if (!ruler.isTable() || !sendTable(zone, lHeader, font, ruler, str)) {
        str="";
        input->seek(debPos, WPX_SEEK_SET);
        send(zone, lHeader.m_numChar, font, ruler, str);
      }
      f << str;
    }
    input->seek(lastPos, WPX_SEEK_SET);

    ascii.addPos(pos);
    ascii.addNote(f.str().c_str());

    if (long(input->tell()) >= zone->m_end)
      break;
  }
  return true;
}

bool FWText::sendHiddenItem(int id, FWTextInternal::Font &font, FWTextInternal::Paragraph &ruler)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("FWText::sendHiddenItem can not find the listener\n"));
    return false;
  }
  if (m_state->m_itemMap.find(id)==m_state->m_itemMap.end()) {
    MWAW_DEBUG_MSG(("FWText::sendHiddenItem: can not find item %d\n", id));
    return false;
  }
  FWTextInternal::Item &item = m_state->m_itemMap.find(id)->second;
  if (!item.m_hidden) {
    MWAW_DEBUG_MSG(("FWText::sendHiddenItem: item %d is not hidden\n", id));
    return false;
  }
  // avoid loop if pb
  font.m_item=FWTextInternal::Item(); // no previous item
  item.m_hidden = false; // item is send

  shared_ptr<FWTextInternal::Zone> zone=item.m_hiddenZone;
  if (!zone) {
    MWAW_DEBUG_MSG(("FWText::sendHiddenItem can not find the hidden zone\n"));
    return false;
  }
  MWAWInputStreamPtr input = zone->m_zone->m_input;
  libmwaw::DebugFile &ascii = zone->m_zone->getAsciiFile();
  libmwaw::DebugStream f;
  long pos = zone->m_begin;
  input->seek(pos, WPX_SEEK_SET);

  f << "ItemData[Collapsed]:";
  int val = (int) input->readULong(1); // alway 40 ?
  if (val != 0x40)
    f << "#type=" << val << ",";
  val = (int) input->readULong(1); // find number between 3 and 7, a unique id ?
  if (val) f << "id=" << val << ",";
  val = (int) input->readULong(2); // number between 2 and 48
  if (val) f << "f0=" << val << ",";
  for (int i = 0; i < 4; i++) { // f1=3, f2=0|c0, f3=1, f4=0|1
    val = (int) input->readULong(1);
    if (val) f << "f" << i+1 << "=" << std::hex << val << std::dec << ",";
  }
  for (int i = 0; i < 3; i++) { // g0=0, g1=1, g2=0
    val = (int) input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  val = (int) input->readLong(1); // always 0?
  if (val) f << "g3=" << val << ",";
  font.m_font.setSize((float)input->readULong(1));
  font.m_font.setId((int)input->readULong(2));
  for (int i = 0; i < 2; i++) {  //g4=5|6|45, g5=0
    val = (int) input->readULong(1);
    if (val) f << "g" << 4+i << "=" << std::hex << val << std::dec << ",";
  }
  for (int i = 0; i < 3; i++) { // always 0 or color?
    val = (int) input->readLong(2);
    if (val) f << "h" << i << "=" << std::hex << val << std::dec << ",";
  }
  for (int i = 0; i < 5; i++) {
    val = (int) input->readULong(2);
    MWAWColor col;
    if (FWStruct::getColor(val, col)) f << "col" << i << "=" << col << ",";
  }
  for (int i = 0; i < 2; i++) { // h3=id?, h4=id?
    val = (int) input->readLong(2);
    if (val) f << "h" << i+3 << "=" << std::hex << val << std::dec << ",";
  }
  input->seek(4, WPX_SEEK_CUR); // skip size...
  int numChar=int(zone->m_end-(pos+44));
  if (numChar)
    ascii.addDelimiter(pos+44,'|');
  listener->setParagraph(ruler);
  std::string str;
  send(zone, numChar, font, ruler, str);
  f << str;

  ascii.addPos(pos);
  ascii.addNote(f.str().c_str());

  for (size_t c=0; c < item.m_childList.size(); c++)
    sendHiddenItem(item.m_childList[c], font, ruler);
  return true;
}

////////////////////////////////////////////////////////////
// read the text data
bool FWText::readTextData(FWStruct::EntryPtr zone)
{
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &ascii = zone->getAsciiFile();
  int vers = version();

  long pos = zone->begin();
  input->seek(pos, WPX_SEEK_SET);

  bool knownZone = (zone->m_type >= 0xa && zone->m_type <= 0x14) || (zone->m_type == 0x18);

  int header = (int)input->readULong(2);
  bool hasHeader = false;
  if (!knownZone && (header&0xFC00)) return false;

  shared_ptr<FWTextInternal::Zone> text(new FWTextInternal::Zone);
  text->m_zoneType = (zone->m_type==0xa) ?
                     FWTextInternal::Zone::Main : FWTextInternal::Zone::Normal;
  text->m_zone = zone;

  int val;
  if (!header) { // maybe the main zone
    bool ok = true;
    // find f0 in [1,270], f1 in [1,d2]
    for (int i = 0; i < 2 && ok ; i++) {
      val = (int)input->readLong(2);
      if (val <= 0 || (!i && val >= 0x800) || (i && val >= 0x200))
        ok = false;
    }
    for (int i = 0; i < 2 && ok; i++) {
      val = (int)input->readULong(1);
      if (val>1) ok=false;
    }
    val = ok ? (int)input->readLong(1) : 0;
    if (val && !knownZone)
      ok=false;
    val = ok ? (int)input->readLong(1) : 0;
    if (val < 1 || val > 20)
      ok=false;
    if (!ok) { // force to try as attachment
      header = 1;
      input->seek(pos+2, WPX_SEEK_SET);
    } else if (!knownZone)
      text->m_zoneType = FWTextInternal::Zone::Main;
  }
  if (header) { // attachement
    hasHeader = true;
    for (int i = 0; i < 2; i++) {
      val = (int)input->readLong(2);
      if (val <= 0 || val > 2) return false;
    }
    val = (int)input->readLong(vers==1 ? 1 : 2);
    if (val < 0 || val > 4) return false;
    if (vers==2) {
      if (val== 0) return false;
      val = (int)input->readLong(2);
      if (val) return false;
    }
    if (!knownZone)
      text->m_zoneType = FWTextInternal::Zone::Main;
  }
  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  if (hasHeader) {
    //find 8, 1c, 71, 76, 8a, 99, 9d, c6, c9, ce, f3, f4
    f << "N0=" << (int)input->readLong(2) << ",";
    for (int i = 0; i < 2; i++) { // f0=f1=1 ? f2=1/2
      val = (int)input->readLong(2);
      if (val != 1) f << "f" << i << "=" << val << ",";
    }
    val = (int)input->readLong(vers==1 ? 1 : 2);
    if (val != 1) f << "f2=" << val << ",";
    if (vers == 1) {
      ascii.addDelimiter(input->tell(),'|');
      input->seek(59, WPX_SEEK_CUR);
      ascii.addDelimiter(input->tell(),'|');
    }
  }
  val = (int)input->readLong(2); // always 0 ?
  if (val) f << "f3=" << val << ",";

  /* N[0] related to number line ? N[1] between 1 and 5e */
  int N[2];
  for (int i = 0; i < 2; i++) {
    N[i] = (int)input->readLong(2);
    if (N[i])
      f << "N" << i+1 << "=" << std::hex << N[i] << std::dec << ",";
  }
  // two flag header/footer : 0|1, normal/other: 0|1?
  for (int i = 0; i < 2; i++) {
    text->m_flags[i] = (int)input->readLong(1);
    if (text->m_flags[i])
      f << "fl" << i << "=" << text->m_flags[i] << ",";
  }
  for (int i=0; i < 2; i++) { // f4=0|27, f5=small number 1/2/f
    val = (int)input->readLong(1);
    if (val!=1) f << "f" << i+4 << "=" << val << ",";
  }

  // between 1 and 202 ( very similar to dim[2]
  int dimUnk = (int)input->readLong(2);
  // two flags 0|ce|fa, 0
  for (int i = 3; i < 5; i++) {
    val = (int)input->readLong(1);
    if (val)
      f << "fl" << i << "=" << val << ",";
  }
  // between 8 and 58
  f << "N3=" << (int)input->readLong(2) << ",";
  int dim[4]; // box with almost always y1=y0+1?
  for (int i = 0; i < 4; i++)
    dim[i]=(int)input->readLong(2);
  if (dimUnk != dim[2]) f << "dimUnk=" << dimUnk << ",";

  text->m_box=Box2f(Vec2f(float(dim[0]),float(dim[1])),Vec2f(float(dim[2]),float(dim[3])));
  text->m_pages[1] = (int)input->readLong(2);
  if (text->m_pages[1] == 16000) text->m_pages[1] = 0;
  text->m_pages[0] = (int)input->readLong(2);
  if (text->m_pages[0] == 16000) text->m_pages[0] = text->m_pages[1];

  for (int i = 0; i < 2; i++) { // 1, 2 or 16000=infty?
    val = (int)input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }

  text->m_extra  = f.str();
  f.str("");
  f << "Entries(TextData):";
  if (zone->m_type >= 0)
    f << "zType=" << std::hex << zone->m_type << std::dec << ",";
  f << *text;
  ascii.addPos(pos);
  ascii.addNote(f.str().c_str());
  if (long(input->tell()) >= zone->end())
    return false;

  text->m_begin = input->tell();
#if DEBUGII
  int numLines=0;
#endif
  while(1) {
    pos = input->tell();
    if (pos+2 >= zone->end()) break;
    int type = (int)input->readULong(2);
    if (input->atEOS()) {
      MWAW_DEBUG_MSG(("FWText::readTextData: internal problem, unexpected EOS!!!\n"));
      return false;
    }
    int lengthSz = 1;
    if (type & 0x8000)
      lengthSz = 2;
    int numChar = (int)input->readULong(lengthSz);
    if ((lengthSz==1 && (numChar & 0x80)) ||
        long(input->tell()+numChar) > zone->end()) {
      input->seek(pos, WPX_SEEK_SET);
      break;
    }
#if DEBUGII
    numLines++;
#endif
    int sz = 0;
    if (type & 0x4000) sz+=12;
    if (type & 0x2000) sz+=1;
    if (type & 0x1000) sz+=1;
    if (type & 0x800) sz+=1;
    if (type & 0x400) sz+=1;
    if (type & 0x200) sz+=2;
    if (type & 0x100) sz+=2;
    if (type & 0x80) sz+=2;
    if (type & 0x40) sz+=2;
    if (type & 0x20) sz+=4;
    if (type & 0x10) sz+=8;
    if (type & 0x8) sz+=2;
    if (type & 0x4) {
      MWAW_DEBUG_MSG(("FWText::readTextData: find unknown size flags!!!!\n"));
      sz+=2;
    }
    if (type & 0x2) sz+=2;
    if (type & 0x1) sz+=2;
    if (numChar || sz)
      input->seek(numChar+sz, WPX_SEEK_CUR);
  }
#if DEBUGII
  std::cout << "FIND:N=" << numLines << " [" << N[0] << "]\n";
#endif
  pos = text->m_end = input->tell();
  // ok, we can insert the data
  std::multimap<int, shared_ptr<FWTextInternal::Zone> >::iterator it =
    m_state->m_entryMap.find(zone->id());
  if (it != m_state->m_entryMap.end()) {
    MWAW_DEBUG_MSG(("FWText::readTextData: entry %d already exists\n", zone->id()));
  }
  m_state->m_entryMap.insert
  ( std::multimap<int, shared_ptr<FWTextInternal::Zone> >::value_type(zone->id(), text));

  f.str("");
  f << "TextData-b:";
  val = (int)input->readULong(2);
  if (val || (int)input->readULong(1) != 0xFF) {
    f << "##";
    ascii.addPos(pos);
    ascii.addNote(f.str().c_str());
    return true;
  }

  f << "numCols=" << (int)input->readLong(2) << ",";
  ascii.addPos(pos);
  ascii.addNote(f.str().c_str());
  int actPage = 0;
  while (1) {
    pos = input->tell();
    if (pos >= zone->end()) break;
    val = (int)input->readULong(2);
    int sz = 0;
    if ((val%50)==30) {
      int num = val/50;
      sz = 32+num*52;

      input->seek(9, WPX_SEEK_CUR);

      if (pos+sz > zone->end()) {
        input->seek(pos, WPX_SEEK_SET);
        break;
      }

      FWTextInternal::PageInfo page;
      page.m_page = actPage++;

      ascii.addPos(pos);
      ascii.addNote("TextData-c[ColH]");

      pos+=32;
      input->seek(pos, WPX_SEEK_SET);
      for (int i = 0; i < num; i++) {
        pos = input->tell();
        f.str("");
        f << "TextData-c[ColDef]:";
        FWTextInternal::ColumnInfo col;
        col.m_column = i;
        col.m_beginPos = (int)input->readLong(2);
        for (int j = 0; j < 4; j++) dim[j] = (int)input->readLong(2);
        col.m_box = Box2i(Vec2i(dim[0],dim[2]), Vec2i(dim[1], dim[3]));
        f << col;
        page.m_columns.push_back(col);

        ascii.addDelimiter(input->tell(),'|');
        ascii.addPos(pos);
        ascii.addNote(f.str().c_str());
        input->seek(pos+52, WPX_SEEK_SET);
      }
      text->m_pagesInfo.push_back(page);
      continue;
    }

    // ok, probably link to a col field, but...
    // let try to correct
    int high = (val>>8);
    if (high==0x21)
      sz = 42;
    else if (high==0x61||high==0x63) {
      input->seek(12, WPX_SEEK_CUR);
      int numData = (int)input->readULong(2);
      if (!numData) break;
      sz = 26+9*numData;
    } else if (high==0xe1) {
      input->seek(14, WPX_SEEK_CUR);
      int numData = (int)input->readULong(2);
      if (!numData) break;
      sz = 30+9*numData;
    } else if (high==0 && (val%50)!=30)
      sz=26;
    if (sz == 0 || pos+sz > zone->end()) {
      input->seek(pos, WPX_SEEK_SET);
      break;
    }

    ascii.addPos(pos);
    ascii.addNote("TextData-d:");
    input->seek(pos+sz, WPX_SEEK_SET);
  }

  if (input->tell() < zone->end()) {
    ascii.addPos(input->tell());
    ascii.addNote("TextData-d:");
  }

  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////
bool FWText::readItem(FWStruct::EntryPtr zone, int id, bool hidden)
{
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &ascii = zone->getAsciiFile();
  libmwaw::DebugStream f;
  long pos = input->tell();
  if (pos+25 > zone->end())
    return false;

  FWTextInternal::Item item;
  item.m_hidden = hidden;
  int val;
  int numOk = 0, numBad = 0;
  int numZone = m_mainParser->getNumDocZoneStruct();
  for (int i = 0; i < 4; i++) {
    val = (int)input->readLong(2);
    if (val < 0 || val >= numZone) {
      numBad++;
      f << "#id" << i << "=" << val << ",";
      continue;
    }
    item.m_structId[i]=val;
    if (val) numOk++;
  }
  val = (int)input->readLong(2);
  if (val > 0) {
    item.m_index=val;
    numOk++;
  } else {
    f << "#index=" << val << ",";
    numBad++;
  }
  for (int i = 0; i < 2; i++) { // f1=[1-4]:listId?,f2=0
    val = (int)input->readLong(2);
    if (!val) continue;
    if (val < -100||val > 100) {
      numBad++;
      f << "#";
    } else
      numOk++;
    f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  if (numBad > numOk) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  val = (int)input->readULong(1); // 68 or e8
  if (val!=0x68)
    f << "fl=" << std::hex << val << std::dec << ",";
  item.m_level = (int)input->readLong(2);
  val = (int)input->readULong(1); // always 0
  if (val)
    f << "fl1=" << val << ",";
  item.m_structId[FWTextInternal::Item::Main] = (int)input->readLong(2);
  if (id > 0 && id != item.m_structId[FWTextInternal::Item::Main]) {
    numBad+=3;
    f << "###id,";
  }
  for (int i = 1; i < 3; i++) { // always 0 ?
    val = (int)input->readLong(2);
    if (!val) continue;
    if (i > 1 && (val < -100 || val > 100)) {
      f << "#";
      numBad++;
    } else
      numOk++;
    f << "g" << i << "=" << std::hex << val << std::dec << ",";
  }
  if (numBad >= numOk) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  val = (int)input->readULong(1); // always 0
  if (val)
    f << "fl2=" << val << ",";
  item.m_extra=f.str();
  f.str("");
  f << "Entries(ItemData):" << item;
  ascii.addPos(pos);
  ascii.addNote(f.str().c_str());
  bool set = false;
  if (id < 0) ;
  else if (m_state->m_itemMap.find(id) == m_state->m_itemMap.end()) {
    m_state->m_itemMap.insert
    (std::map<int,FWTextInternal::Item>::value_type(id,item));
    set=true;
  } else {
    MWAW_DEBUG_MSG(("FWText::readItem: id %d already exists\n", id));
  }

  if (!hidden)
    return true;

  pos = input->tell();
  if (pos+44 > zone->end() || input->readULong(1)!=0x40) {
    MWAW_DEBUG_MSG(("FWText::readItem: can not find hidden data\n"));
    input->seek(pos, WPX_SEEK_SET);
    return true;
  }
  input->seek(pos+42, WPX_SEEK_SET);
  int sz = (int) input->readULong(2);
  if (pos+44+sz > zone->end()) {
    MWAW_DEBUG_MSG(("FWText::readItem: find bad data size\n"));
    input->seek(pos, WPX_SEEK_SET);
    return true;
  }

  if (set && m_state->m_itemMap.find(id) != m_state->m_itemMap.end()) {
    FWTextInternal::Item &theItem = m_state->m_itemMap.find(id)->second;
    theItem.m_hiddenZone.reset(new FWTextInternal::Zone);
    theItem.m_hiddenZone->m_zone = zone;
    theItem.m_hiddenZone->m_zoneType = FWTextInternal::Zone::CollapsedItem;
    theItem.m_hiddenZone->m_begin = pos;
    theItem.m_hiddenZone->m_end = pos+44+sz;
  }

  if (sz)
    input->seek(sz, WPX_SEEK_CUR);
  return true;
}

////////////////////////////////////////////////////////////
// read a style
bool FWText::readStyle(FWStruct::EntryPtr zone)
{
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &ascii = zone->getAsciiFile();
  libmwaw::DebugStream f;
  long pos=input->tell();

  int sz = int(input->readLong(2));
  if (sz < 4 || sz >= 0x100) return false;
  if (pos+2+sz> zone->end()) return false;
  f.str("");
  f << "Entries(Style):";
  for (int i=0; i<2; ++i) { //f0: small number, f1: number between 0 and 1c: flag?
    int val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  if (sz==4) {
    ascii.addPos(pos);
    ascii.addNote(f.str().c_str());
    return true;
  }
  if (sz!=0x46) {
    f << "###";
    MWAW_DEBUG_MSG(("FWText::readStyle: style length seems odd\n"));
    input->seek(pos+2+sz, WPX_SEEK_SET);
    ascii.addPos(pos);
    ascii.addNote(f.str().c_str());
    return true;
  }
  int nSz=(int) input->readULong(1);
  if (!nSz || nSz>0x1f) {
    f << "###";
    MWAW_DEBUG_MSG(("FWText::readStyle: style name length seems odd\n"));
    input->seek(pos+2+sz, WPX_SEEK_SET);
    ascii.addPos(pos);
    ascii.addNote(f.str().c_str());
    return true;
  }
  std::string name("");
  for (int c=0; c<nSz; ++c)
    name += (char) input->readLong(1);
  f << name << ",";
  input->seek(pos+38, WPX_SEEK_SET);
  ascii.addDelimiter(input->tell(),'|');
  ascii.addPos(pos);
  ascii.addNote(f.str().c_str());
  input->seek(pos+2+sz, WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// read the paragraph data
bool FWText::readParagraphTabs(FWStruct::EntryPtr zone, int id)
{
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &ascii = zone->getAsciiFile();
  libmwaw::DebugStream f;
  int const vers = version();
  const int dataSz = vers==1 ? 14 : 10;
  const int headerSz = vers==1 ? 24 : 30;
  long pos = input->tell();
  input->seek(pos, WPX_SEEK_SET);
  long sz = (long) input->readULong(4);
  if (sz<24 || pos+4+sz > zone->end()) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  f.str("");
  f << "Entries(Ruler):";
  if (id >= 0) f << "P" << id << ",";
  int val;
  for (int i = 0; i < 2; i++) { // find f0 in 0 c0, f1=0|2
    val = (int)input->readULong(1);
    if (val) f << "fl" << i << "=" << std::hex << val << std::dec << ",";
  }
  for (int i = 0; i < 2; i++) {  // find f0=0|1|2: align ?, f1=0-7
    val = (int)input->readULong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  FWTextInternal::Paragraph para;
  float dim[2];
  for (int i = 0; i < 2; i++) // dim0: height?, dim1:width
    dim[i] = (float)(input->readULong(2))/72.f;
  para.m_dim=Vec2f(dim[1],dim[0]);
  double margins[3];
  char const *(margNames[]) = { "left", "right", "first" };
  for (int i = 0; i < 3; i++) {
    margins[i] = double(input->readLong(4))/65536./72.0;
    if (margins[i] < 0 || margins[i] > 0)
      f << "margins[" << margNames[i] << "]=" << margins[i] << ",";
  }
  para.m_margins[0] = margins[2]-margins[0];
  para.m_margins[1] = margins[0];
  if (margins[1] < dim[1])
    para.m_margins[2]=dim[1]-margins[1];
  ascii.addDelimiter(input->tell(), '|');

  input->seek(vers==1 ? pos+27 : pos+26, WPX_SEEK_SET);
  int N = (int)input->readULong(1);
  if (headerSz+dataSz *N != sz) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  f << "N=" << N << ",";
  if (vers==2) {
    for (int i=0; i < 3; i++) {
      val = (int)input->readULong(1);
      if (val) f << "g" << i << "=" << val << ",";
    }
    if (input->readULong(4)) {
      para.m_isTable = true;
      f << "table,";
    }
    input->seek(pos+4+30, WPX_SEEK_SET);
  }
  ascii.addPos(pos);
  ascii.addNote(f.str().c_str());
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    f << "Ruler:Tabs-" << i << ":";
    MWAWTabStop tab;
    val = (int)input->readULong(1);
    switch(val>>5) {
    case 0:
      break;
    case 1:
      tab.m_alignment = MWAWTabStop::CENTER;
      break;
    case 2:
      tab.m_alignment = MWAWTabStop::RIGHT;
      break;
    case 3:
      tab.m_alignment = MWAWTabStop::DECIMAL;
      break;
    case 4:
      f << "end[table],";
      break;
    case 5:
      f << "justify[table],";
      break;
    case 6:
      f << "justifyAll[table],";
      break;
      // 4: tableend, 5: justify(begintable), 6: justifyall(begin table)
    default:
      f << "##tab[type]=" << (val>>5) << ",";
      break;
    }
    para.m_tableFlags.push_back(val>>5);
    if (val & 0x1F) f << "#type=" << std::hex << (val & 0x1F) << std::dec << ",";
    val = (int)input->readULong(1);
    if (val) {
      int unicode= m_parserState->m_fontConverter->unicode(3, (unsigned char) val);
      if (unicode==-1)
        tab.m_decimalCharacter = uint16_t(val);
      else
        tab.m_decimalCharacter = uint16_t(unicode);
    }
    tab.m_position = double(input->readLong(4))/65536./72.;
    val = (int)input->readLong(2);
    if (val) f << "repeat=" << val/256.;

    val = (int)input->readULong(1);
    switch(val) {
    case 0x1: // fixme: . or dotted
      tab.m_leaderCharacter = '.';
      break;
    case 0x20: // space, ie. none
    case 0:
      break;
    default: {
      int unicode= m_parserState->m_fontConverter->unicode(3, (unsigned char) val);
      if (unicode==-1)
        tab.m_leaderCharacter = uint16_t(val);
      else
        tab.m_leaderCharacter = uint16_t(unicode);
      break;
    }
    }
    val = (int)input->readULong(1);
    if (val) f << "#f2=" <<  std::hex << val << std::dec << ",";
    f << tab;
    para.m_tabs->push_back(tab);
    ascii.addPos(pos);
    ascii.addNote(f.str().c_str());
    input->seek(pos+dataSz, WPX_SEEK_SET);
  }
  if (para.m_isTable) {
    pos = input->tell();
    sz = input->readLong(4);
    f.str("");
    f << "Ruler[Table:end]:";
    if (sz==0x24 && pos+4+sz <= zone->end()) {
      for (int i=0; i<11; ++i) { // always 0
        val=(int)input->readLong(2);
        if (val)
          f << "f" << i << "=" << val << ",";
      }
      para.m_tableBorderId=(int)input->readLong(2);
      if (para.m_tableBorderId)
        f << "B" << para.m_tableBorderId-1 << ",";
      ascii.addDelimiter(input->tell(), '|');
      input->seek(pos+4+0x24, WPX_SEEK_SET);
    } else {
      MWAW_DEBUG_MSG(("FWText::readParagraphTabs: can not find table data\n"));
      f << "###";
      input->seek(pos, WPX_SEEK_SET);
    }
    ascii.addPos(pos);
    ascii.addNote(f.str().c_str());
  }
  if (id < 0) ;
  else if (m_state->m_paragraphMap.find(id) == m_state->m_paragraphMap.end())
    m_state->m_paragraphMap.insert
    (std::map<int,FWTextInternal::Paragraph>::value_type(id,para));
  else {
    MWAW_DEBUG_MSG(("FWText::readParagraphTabs: id %d already exists\n", id));
  }
  return true;
}

/* read font/para modifier in text */
bool FWText::readDataMod(FWStruct::EntryPtr zone, int id)
{
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &ascii = zone->getAsciiFile();
  libmwaw::DebugStream f;

  long pos = input->tell();
  input->seek(pos, WPX_SEEK_SET);
  if (pos+10 > zone->end()) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  FWTextInternal::DataModifier mod;
  int val = int(input->readULong(2));
  MWAWColor col;
  if (FWStruct::getColor(val, col))
    mod.m_color = col;
  else if (val != 0xFFFF)
    f << "#col=" << std::hex << val << std::dec << ",";
  for (int i = 0; i < 4; i++)
    mod.m_data[i] = int(input->readULong(2));
  mod.m_extra = f.str();
  f.str("");
  f << "Entries(DataMod):" << mod;
  if (m_state->m_dataModMap.find(id) == m_state->m_dataModMap.end())
    m_state->m_dataModMap.insert
    (std::map<int, FWTextInternal::DataModifier>::value_type(id, mod));
  else {
    MWAW_DEBUG_MSG(("FWText::readDataMod: Oops id %d already find\n", id));
  }
  ascii.addPos(pos);
  ascii.addNote(f.str().c_str());
  return true;
}

/* read para modifier in doc info */
bool FWText::readParaModDocInfo(FWStruct::EntryPtr zone)
{
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->getAsciiFile();
  libmwaw::DebugStream f;
  long pos = input->tell();
  if (input->readULong(4)!=0x65787472 || input->readULong(1)) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  long blckSz = input->readLong(4);
  long endData = pos+9+blckSz;
  int num = (int) input->readULong(2), val;
  int const fSz = 18;
  f << "Entries(ParaMod):N=" << num << ",";
  if (blckSz < 2 || blckSz != 2 + num*fSz || endData > zone->end()) {
    MWAW_DEBUG_MSG(("FWText::readParaModDocInfo::readCitationDocInfo: problem reading the data block or the number of data\n"));
    f << "###";
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    if (endData <= zone->end()) {
      input->seek(endData, WPX_SEEK_SET);
      return true;
    }
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  m_state->m_paragraphModList.push_back(FWTextInternal::ParaModifier());
  for (int i = 0; i < num; i++) {
    f.str("");
    pos = input->tell();
    FWTextInternal::ParaModifier mod;
    for (int j = 0; j < 2; j++) {
      val = int(input->readLong(2));
      if (val==-1)
        continue;
      if (j==0)
        mod.m_beforeSpacing = float(val)/256.f;
      else
        mod.m_afterSpacing = float(val)/256.f;
    }
    for (int j = 0; j < 3; j++) { // always ffff
      val = (int) input->readLong(2);
      if (val != -1)
        f << "f" << j << "=" << val << ",";
    }
    for (int j = 0; j < 4; j++) { // always 0
      val = (int) input->readLong(2);
      if (val)
        f << "f" << j+3 << "=" << val << ",";
    }
    mod.m_extra = f.str();
    m_state->m_paragraphModList.push_back(mod);
    f.str("");
    f << "ParaMod-" << i << ":" << mod;
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos+fSz, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the column data
bool FWText::readColumns(FWStruct::EntryPtr zone)
{
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &ascii = zone->getAsciiFile();
  libmwaw::DebugStream f;

  long pos = input->tell();
  input->seek(pos, WPX_SEEK_SET);
  long sz = (long) input->readULong(4);
  if (sz<34 || pos+4+sz > zone->end()) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  input->seek(13, WPX_SEEK_CUR);
  int N = (int)input->readULong(1);
  if (24+10*N != sz) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  f.str("");
  f << "Entries(Columns):" << N;
  ascii.addPos(pos);
  ascii.addNote(f.str().c_str());
  input->seek(pos+4+24, WPX_SEEK_SET);
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    f << "Columns-" << i << ":";
    int dim[2];
    dim[0] = (int) input->readLong(2);
    long val = (long) input->readULong(2);
    if (val) f << "f0=" << std::hex << val << std::dec << ",";
    dim[1] = (int) input->readLong(2);
    f << "pos=" << dim[0] << "<->" << dim[1] << ",";
    val = (long) input->readULong(2);
    if (val) f << "f1=" << std::hex << val << std::dec << ",";
    ascii.addDelimiter(input->tell(),'|');
    ascii.addPos(pos);
    ascii.addNote(f.str().c_str());
    input->seek(pos+10, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
//! send data to the listener
bool FWText::sendMainText()
{
  size_t numZones = m_state->m_mainZones.size();
  if (!numZones) {
    MWAW_DEBUG_MSG(("FWText::sendMainText: can not find main zone\n"));
    return false;
  }
  if (!m_parserState->m_listener) return true;
  std::multimap<int, shared_ptr<FWTextInternal::Zone> >::iterator it;
  for (size_t i = 0; i < numZones; i++) {
    it = m_state->m_entryMap.find(m_state->m_mainZones[i]);
    if (it == m_state->m_entryMap.end() || !it->second) {
      MWAW_DEBUG_MSG(("FWText::sendMainText: can not find main zone: internal problem\n"));
      continue;
    }
    m_mainParser->newPage(++m_state->m_actualPage);
    send(it->second);
  }
  return true;
}

bool FWText::send(int zId)
{
  std::multimap<int, shared_ptr<FWTextInternal::Zone> >::iterator it;
  it = m_state->m_entryMap.find(zId);
  if (it == m_state->m_entryMap.end() || !it->second) {
    MWAW_DEBUG_MSG(("FWText::send: can not find zone: %d\n", zId));
    return false;
  }
  send(it->second);
  return true;
}

void FWText::flushExtra()
{
  std::multimap<int, shared_ptr<FWTextInternal::Zone> >::iterator it;
  for (it = m_state->m_entryMap.begin(); it != m_state->m_entryMap.end(); ++it) {
    shared_ptr<FWTextInternal::Zone> zone = it->second;
    if (!zone || !zone->m_zone || zone->m_zone->isParsed())
      continue;
    send(zone);
  }
}

void FWText::sortZones()
{
  std::multimap<int, shared_ptr<FWTextInternal::Zone> >::iterator it;
  int numZones = 0, totalNumPages = 0;
  std::vector<int> pagesLimits;
  for (it = m_state->m_entryMap.begin(); it != m_state->m_entryMap.end(); ++it) {
    shared_ptr<FWTextInternal::Zone> zone = it->second;
    if (!zone || !zone->m_zone || zone->m_zoneType != FWTextInternal::Zone::Main)
      continue;
    int fPage = zone->m_pages[0], lPage = zone->m_pages[1];
    if (lPage < fPage) {
      MWAW_DEBUG_MSG(("FWText::sortZones: last page is inferior to firstPage\n"));
      lPage = fPage;
    }
    int pos = 0;
    while (pos < numZones) {
      if (fPage < pagesLimits[size_t(2*pos)])
        break;
      if (fPage == pagesLimits[size_t(2*pos)] && lPage <= pagesLimits[size_t(2*pos+1)])
        break;
      pos++;
    }
    pagesLimits.resize(size_t(2*numZones+2));
    m_state->m_mainZones.resize(size_t(numZones+1));
    for (int i = numZones-1; i > pos-1; i--) {
      pagesLimits[size_t(2*i+2)]=pagesLimits[size_t(2*i)];
      pagesLimits[size_t(2*i+3)]=pagesLimits[size_t(2*i+1)];
      m_state->m_mainZones[size_t(i+1)]=m_state->m_mainZones[size_t(i)];
    }
    m_state->m_mainZones[size_t(pos)] = zone->m_zone->id();
    pagesLimits[size_t(2*pos)] = fPage;
    pagesLimits[size_t(2*pos+1)] = lPage;
    numZones++;
    int nPages = (lPage-fPage)+1;
    if (nPages < int(zone->m_pagesInfo.size())) {
      MWAW_DEBUG_MSG(("FWText::sortZones: pages limit seems odd!!!\n"));
      nPages = int(zone->m_pagesInfo.size());
    }
    totalNumPages += nPages;
  }
  m_state->m_numPages = totalNumPages;
}

void FWText::createItemStructures()
{
  std::map<int, FWTextInternal::Item>::iterator it=m_state->m_itemMap.begin(), it2;
  for (; it != m_state->m_itemMap.end(); ++it) {
    FWTextInternal::Item &item = it->second;
    int childId=item.m_structId[FWTextInternal::Item::Child];
    int id=item.m_structId[FWTextInternal::Item::Main];
    if (childId<=0)
      continue;
    int prevId=0;
    std::set<int> seens;
    while(childId>0) {
      if (seens.find(childId)!=seens.end()) {
        MWAW_DEBUG_MSG(("FWText::createItemStructures:find loop\n"));
        break;
      }
      seens.insert(childId);
      it2=m_state->m_itemMap.find(childId);
      if (it2==m_state->m_itemMap.end()) {
        MWAW_DEBUG_MSG(("FWText::createItemStructures: can not find child: %d\n", childId));
        break;
      }
      FWTextInternal::Item &child = it2->second;
      if (child.m_structId[FWTextInternal::Item::Father] != id ||
          child.m_structId[FWTextInternal::Item::Prev] != prevId) {
        MWAW_DEBUG_MSG(("FWText::createItemStructures: find unexpected child %d\n", childId));
        break;
      }
      item.m_childList.push_back(childId);
      if (child.m_hidden) item.m_collapsed=true;
      prevId=childId;
      childId=child.m_structId[FWTextInternal::Item::Next];
    }
  }
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
