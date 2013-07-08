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
#include <sstream>

#include <libwpd/libwpd.h>

#include "MWAWCell.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSection.hxx"
#include "MWAWTable.hxx"

#include "WNParser.hxx"
#include "WNEntry.hxx"

#include "WNText.hxx"

/** Internal: the structures of a WNText */
namespace WNTextInternal
{
////////////////////////////////////////
//! Internal: the fonts
struct Font {
  //! the constructor
  Font(): m_font() {
    for (int i = 0; i < 3; i++) m_flags[i] = 0;
    for (int i = 0; i < 2; i++) m_styleId[i] = -1;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Font const &font) {
    for (int i = 0; i < 3; i++) {
      if (!font.m_flags[i]) continue;
      o << "ft" << i << "=";
      if (i == 0) o << std::hex;
      o << font.m_flags[i] << std::dec << ",";
    }
    if (font.m_styleId[0] >= 0) o << "id[charStyle]=" << font.m_styleId[0] << ",";
    if (font.m_styleId[1] >= 0) o << "id[rulerStyle]=" << font.m_styleId[1] << ",";
    return o;
  }

  //! the font
  MWAWFont m_font;
  //! the char/ruler style id
  int m_styleId[2];
  //! some unknown flag
  int m_flags[3];
};

/** Internal: class to store the paragraph properties */
struct Paragraph : public MWAWParagraph {
  //! Constructor with type
  Paragraph() : MWAWParagraph() {
    m_marginsUnit = WPX_POINT;
    for(int i = 0; i < 8; i++)
      m_values[i] = 0;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Paragraph const &ind) {
    o << reinterpret_cast<MWAWParagraph const &>(ind);
    for (int i = 0; i < 8; i++) {
      if (!ind.m_values[i]) continue;
      o << "fR" << i << "=" << ind.m_values[i] << ",";
    }
    return o;
  }
  //! some unknown value
  int m_values[8];
};

/** Internal: class to store a style */
struct Style {
  Style() : m_name(""), m_nextId(-1), m_font(), m_paragraph() {
    for (int i = 0; i < 13; i++) m_values[i] = 0;
    for (int i = 0; i < 6; i++) m_flags[i] = 0;
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Style const &st) {
    if (st.m_name.length()) o << st.m_name << ",";
    if (st.m_nextId >= 0) o << "nextId?=" << st.m_nextId << ",";
    for (int i = 0; i < 13; i++) {
      if (st.m_values[i])
        o << "f" << i << "=" << st.m_values[i] << ",";
    }
    o << "flags=[" << std::hex;
    for (int i = 0; i < 6; i++) {
      if (st.m_flags[i])
        o << st.m_flags[i] << ",";
      else
        o << "_,";
    }
    o << "]," << std::dec;
    return o;
  }

  /** the style name */
  std::string m_name;
  /** the next style ? */
  int m_nextId;
  /** the font properties */
  Font m_font;
  /** the paragraph properties */
  Paragraph m_paragraph;
  /** some unknown value */
  int m_values[13];
  /** some unknown flags */
  int m_flags[6];
};

////////////////////////////////////////
//! Internal: the token of a WNText
struct Token {
  Token() : m_graphicZone(-1), m_box(), m_error("") {
    for (int i = 0; i < 19; i++) m_values[i] = 0;
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Token const &tkn) {
    o << "bdbox=" << tkn.m_box << ",";
    if (tkn.m_graphicZone >= 0) o << "zId=" << tkn.m_graphicZone << ",";
    Vec2i tknSize = tkn.m_box.size();
    for (int i = 0; i < 2; i++) {
      if (tknSize == tkn.m_pos[i]) continue;
      o << "pos" << i << "=" << tkn.m_pos[i] << ",";
    }
    for (int i = 0; i < 19; i++) {
      if (!tkn.m_values[i]) continue;
      if (tkn.m_values[i]>0 && (tkn.m_values[i]&0xF000))
        o << "f" << i << "=0x" << std::hex << tkn.m_values[i] << std::dec << ",";
      else
        o << "f" << i << "=" << tkn.m_values[i] << ",";
    }
    if (tkn.m_error.length()) o << tkn.m_error << ",";
    return o;
  }

  //! the graphic zone id
  int m_graphicZone;

  //! the bdbox
  Box2i m_box;

  //! two positions ( seen relative to the RB point)
  Vec2i m_pos[2];

  //! some unknown values
  int m_values[19];

  /** the parsing errors */
  std::string m_error;
};

////////////////////////////////////////
//! Internal: the table of a WNText
struct TableData {
  //! constructor
  TableData() : m_type(-1), m_box(), m_color(MWAWColor::white()), m_error("") {
    for (int i = 0; i < 4; i++) m_flags[i] = 1;
    for (int i = 0; i < 10; i++) m_values[i] = 0;
  }

  //! update a cell
  void updateCell(MWAWTable::Cell &cell) const {
    // as the cells can overlap a little, we build a new box
    cell.m_box=Box2i(m_box.min(), m_box.max()-Vec2i(1,1));
    cell.setBackgroundColor(m_color);
    for (int i=0; i<4; i++) {
      MWAWBorder border;
      switch(m_flags[i]&0x7f) {
      case 1:
        break;
      case 3:
        border.m_width=2;
        break;
      case 5:
        border.m_type=MWAWBorder::Double;
        break;
      case 0x11:
        border.m_style=MWAWBorder::Dot;
        break;
        // 21: ?
      case 0x61:
        border.m_width=0.5;
        break;
      case 0:
      default:
        border.m_width=0;
        break;
      }

      static int const wh[4]= { libmwaw::TopBit, libmwaw::RightBit, libmwaw::BottomBit, libmwaw::LeftBit };
      if (!border.isEmpty())
        cell.setBorders(wh[i], border);
    }
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, TableData const &table) {
    switch(table.m_type) {
    case -1:
      break;
    case 0:
      o << "end,";
      break;
    case 1:
      o << "def,";
      break;
    case 2:
      o << "cell,";
      break;
    default:
      o << "type=#" << table.m_type << ",";
      break;
    }
    if (table.m_box.size()[0] || table.m_box.size()[1])
      o << table.m_box << ",";
    if (!table.m_color.isWhite())
      o << "color=" << table.m_color << ",";
    for (int i = 0; i < 4; i++) {
      static char const *(wh[4])= {"T", "R", "B", "L"};
      if (table.m_flags[i]&0xFF00)
        o << "#bFlags" << wh[i] << "[high]=" << (table.m_flags[i]>>8) << ",";
      // flags&0x80 : duplicated?
      int fl=table.m_flags[i]&0x7F;
      if (fl==1) continue; // normal
      o << "bFlags" << wh[i] << "=";
      switch(fl) {
      case 0:
        o << "none,";
        break;
      case 3:
        o << "w=2,";
        break;
      case 5:
        o << "double,";
        break;
      case 0x11:
        o << "dots,";
        break;
        // 21: ?
      case 0x61: // in fact always 0xe1?
        o << "w=0.5,";
        break;
      default:
        o << "#" << std::hex << fl << std::dec << ",";
        break;
      }
    }
    if (table.m_values[0]==1) o << "selected,";
    else if (table.m_values[0]) o << "#selected=" << table.m_values[0] << ",";
    for (int i = 1; i < 10; i++) {
      if (!table.m_values[i]) continue;
      o << "f" << i << "=" << table.m_values[i] << ",";
    }
    if (table.m_error.length()) o << table.m_error << ",";
    return o;
  }

  //! the type
  int m_type;

  //! the bdbox
  Box2i m_box;

  //! the background color
  MWAWColor m_color;

  //! some unknown flags : T, R, B, L
  int m_flags[4];
  //! some unknown values
  int m_values[10];

  /** the parsing errors */
  std::string m_error;
};

////////////////////////////////////////
//! Internal: structure used to store the content structure
struct ContentZone {
  ContentZone() : m_type(-1), m_value(0) {
    for (int i = 0; i < 2; i++) m_pos[i] = -1;
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, ContentZone const &z);

  /** the type 0 : text,
      1<->7 : a char flag
      8<->f : a zone
      10 : used to store a break
  */
  int m_type;
  //! the begin and the end pos
  long m_pos[2];
  //! a value
  int m_value;
};

std::ostream &operator<<(std::ostream &o, ContentZone const &z)
{
  switch(z.m_type) {
  case 0:
    o << "text,";
    break;

  case 3:
    o << "[hyphen],";
    break;
  case 4:
    o << "[footnote],";
    break;
  case 5:
    o << "[header],";
    break;
  case 6:
    o << "[footer],";
    break;
  case 9:
    if (z.m_value<0) o << "sub[fontMod],";
    else if (z.m_value>0) o << "super[fontMod],";
    else o << "normal[fontMod],";
    break;
  case 0xa: {
    switch (z.m_value) {
    case 0:
      o << "table[end],";
      break;
    case 1:
      o << "table[header],";
      break;
    case 2:
      o << "table[zone],";
      break;
    default:
      o << "table[#" << int(z.m_value>>4) << "#],";
      break;
    }
    break;
  }
  case 0xb:
    o << "decimal[" << char(z.m_value) << "],";
    break;
  case 0xc:
    o << "ruler,";
    break;
  case 0xd: {
    switch (z.m_value) {
    case 0:
      o << "page[field],";
      break;
    case 1:
      o << "date[field],";
      break;
    case 2:
      o << "time[field],";
      break;
    case 3:
      o << "note[field],";
      break;
    default:
      o << "#field=" << z.m_value <<",";
      break;
    }
    break;
  }
  case 0xe:
    o << "token,";
    break;
  case 0xf:
    o << "font,";
    break;
  case 0x10:
    o << "break,";
  default:
    o << "type=#" << z.m_type << ",";
    break;
  }
  return o;
}

////////////////////////////////////////
//! Internal: structure used to store the content structure
struct ContentZones {
  ContentZones() : m_entry(), m_id(-1), m_type(0),
    m_zonesList(), m_textCalledTypesList(), m_footnoteList(), m_sent(false) {
  }
  /** returns true if the entry corresponds to a page/column break */
  bool hasPageColumnBreak() const {
    return m_type == 0 && (m_entry.m_val[0] >> 4) == 7;
  }
  int getNumberOfZonesWithType(int type) const {
    int res = 0;
    for (size_t i = 0; i < m_zonesList.size(); i++)
      if (m_zonesList[i].m_type == type) res++;
    return res;
  }
  //! the general zone
  WNEntry m_entry;
  //! the zone id
  int m_id;
  //! the zone type : 0 : main, 1 header/footer, 2 footnote,
  int m_type;
  //! the list of zone
  std::vector<ContentZone> m_zonesList;
  //! the list of type of text zone called by this zone
  std::vector<int> m_textCalledTypesList;
  //! a list to retrieve the footnote content
  std::vector<shared_ptr<ContentZones> > m_footnoteList;
  //! true if this zone was sent to the listener
  mutable bool m_sent;
};


////////////////////////////////////////
//! Internal: the cell of a WNText
struct Cell : public MWAWTable::Cell {
  //! constructor
  Cell(WNText &parser) : MWAWTable::Cell(), m_parser(parser),
    m_zonesList(), m_footnoteList() {}

  //! send the content
  bool sendContent(MWAWContentListenerPtr, MWAWTable &);
  //! the text parser
  WNText &m_parser;
  //! the list of zone
  std::vector<ContentZone> m_zonesList;
  //! a list to retrieve the footnote content
  std::vector<shared_ptr<ContentZones> > m_footnoteList;
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
      MWAW_DEBUG_MSG(("WNTextInternal::Table::get: cell %d does not exists\n",id));
      return 0;
    }
    return reinterpret_cast<Cell *>(MWAWTable::get(id).get());
  }
};

////////////////////////////////////////
//! Internal: structure used to store the content structure
struct Zone {
  //! constructor
  Zone() : m_zones() {}

  //! a zones
  std::vector<shared_ptr<ContentZones> > m_zones;
};

////////////////////////////////////////
//! Internal: the state of a WNText
struct State {
  //! constructor
  State() : m_version(-1), m_numColumns(1), m_numPages(1), m_actualPage(1),
    m_paragraph(), m_header(), m_footer(),
    m_localFIdMap(), m_styleMap(), m_styleList(), m_contentMap() {
  }

  //! return a paragraph corresponding to 0:body, 1: header/footer, 2: footnote
  Paragraph getDefaultParagraph(int type) const {
    int styleId = type == 0 ? 0 : type==1 ? 3 : type==2 ? 2 : -1;
    if (styleId >= 0 && styleId < int(m_styleList.size()))
      return m_styleList[(size_t) styleId].m_paragraph;
    Paragraph res;
    if (m_version>=0 && m_version <= 2 && type==0) {
      res.m_margins[1] = 90.0;
      res.m_margins[2] = 60.0-28.0;
      static double const (defPos[2])= {1.5,5.5};
      for (int i=0; i < 2; i++) {
        MWAWTabStop defTab;
        defTab.m_position=defPos[i];
        res.m_tabs->push_back(defTab);
      }
    }
    if (type==1)
      res.m_justify = MWAWParagraph::JustificationCenter;
    if (type==2) res.m_margins[0] = 10;
    return res;
  }


  //! return a mac font id corresponding to a local id
  int getFontId(int localId) const {
    if (m_localFIdMap.find(localId)==m_localFIdMap.end())
      return localId;
    return m_localFIdMap.find(localId)->second;
  }
  //! return the content corresponding to a pos
  shared_ptr<ContentZones> getContentZone(long pos) const {
    std::map<long, shared_ptr<ContentZones> >::const_iterator it = m_contentMap.find(pos);
    if (it == m_contentMap.end())
      return shared_ptr<ContentZones>();
    return it->second;
  }

  //! the file version
  mutable int m_version;
  //! the number of column
  int m_numColumns;

  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;
  /** the paragraph properties */
  Paragraph m_paragraph;
  //! the header and the footer
  shared_ptr<ContentZones> m_header, m_footer;
  //! a map local fontId->fontId
  std::map<int, int> m_localFIdMap;
  //! the style indirection table
  std::map<int, int> m_styleMap;
  //! the list of styles (body, footer, footnote, header, table )
  std::vector<Style> m_styleList;

  //! the three main zone ( text, footnote, header/footer)
  Zone m_mainZones[3];

  //! the list of contentZones
  std::map<long, shared_ptr<ContentZones> > m_contentMap;
};

bool Cell::sendContent(MWAWContentListenerPtr, MWAWTable &)
{
  /** as a cell can be arbitrary cutted in small part,
      we must retrieve the last ruler */
  Paragraph ruler = m_parser.m_state->m_paragraph;
  m_parser.send(m_zonesList, m_footnoteList, ruler);
  return true;
}

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
WNText::WNText(WNParser &parser) :
  m_parserState(parser.getParserState()), m_state(new WNTextInternal::State),
  m_entryManager(parser.m_entryManager), m_mainParser(&parser)
{
}

WNText::~WNText()
{ }

int WNText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}

int WNText::numPages() const
{
  int nCol, width;
  m_state->m_actualPage = m_state->m_numPages = 1;
  m_mainParser->getColumnInfo(nCol, width);
  m_state->m_numColumns = nCol;
  if (nCol >= 2) return 1;

  if (m_state->m_mainZones[0].m_zones.size()==0 ||
      m_state->m_mainZones[0].m_zones[0]->m_type != 0) {
    m_state->m_numPages = 1;
    return 1;
  }
  shared_ptr<WNTextInternal::ContentZones> mainContent = m_state->m_mainZones[0].m_zones[0];
  int nPages = 1+mainContent->getNumberOfZonesWithType(0x10);
  m_state->m_numPages = nPages;
  return nPages;
}

WNEntry WNText::getHeader() const
{
  if (!m_state->m_header)
    return WNEntry();
  return m_state->m_header->m_entry;
}

WNEntry WNText::getFooter() const
{
  if (!m_state->m_footer)
    return WNEntry();
  return m_state->m_footer->m_entry;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
// try to find the different zone
////////////////////////////////////////////////////////////
bool WNText::createZones()
{
  std::multimap<std::string, WNEntry const *>::const_iterator iter;

  iter = m_entryManager->m_typeMap.find("FontZone");
  if (iter != m_entryManager->m_typeMap.end())
    readFontNames(*iter->second);

  iter = m_entryManager->m_typeMap.find("StylZone");
  if (iter != m_entryManager->m_typeMap.end())
    readStyles(*iter->second);

  // the text zone
  iter = m_entryManager->m_typeMap.find("TextZone");
  int numData = 0;
  std::vector<WNEntry> listData[3];
  while (iter != m_entryManager->m_typeMap.end()) {
    if (iter->first != "TextZone")
      break;
    int id = iter->second->id();
    if (id < 0 || id > 2) {
      MWAW_DEBUG_MSG(("WNText::createZones: find odd zone type:%d\n",id));
      ++iter;
      continue;
    }
    parseZone(*iter->second, listData[id]);
    ++iter;
  }

  // update the number of columns
  int nCol, width;
  m_mainParser->getColumnInfo(nCol, width);
  m_state->m_numColumns = nCol;

  std::map<long, shared_ptr<WNTextInternal::ContentZones> > listDone;
  std::map<long, shared_ptr<WNTextInternal::ContentZones> >::iterator it;
  /** we can now create the content zone and type them */
  for (int z = 2; z >= 0; z--) {
    for (size_t i = 0; i < listData[z].size(); i++) {
      WNEntry data = listData[z][i];
      it = listDone.find(data.begin());
      if (it != listDone.end()) {
        m_state->m_mainZones[z].m_zones.push_back(it->second);
        continue;
      }

      // ok we must create the data
      data.setId(numData++);
      shared_ptr<WNTextInternal::ContentZones> zone;
      if (m_entryManager->add(data))
        zone = parseContent(m_entryManager->m_posMap.find(data.begin())->second);
      if (zone) {
        zone->m_type = z;
        m_state->m_mainZones[z].m_zones.push_back(zone);
        listDone[data.begin()] = zone;
      }
    }
  }
  /* we can now recreate a zone 0 which contains all the main content zone*/
  WNTextInternal::Zone &mainZone = m_state->m_mainZones[0];
  size_t numZones = mainZone.m_zones.size();
  std::vector<shared_ptr<WNTextInternal::ContentZones> > headerFooterList;
  std::vector<int> calledTypesList;

  shared_ptr<WNTextInternal::ContentZones> zone0(new WNTextInternal::ContentZones);
  zone0->m_id = -1;
  zone0->m_type = 0;
  for (size_t z = 0; z < numZones; z++) {
    shared_ptr<WNTextInternal::ContentZones> content =  mainZone.m_zones[z];
    switch (content->m_type) {
    case 0:
      break;
    case 1:
      headerFooterList.push_back(content);
      break;
    case 2:
      zone0->m_footnoteList.push_back(content);
      break;
    default:
      MWAW_DEBUG_MSG(("WNText::createZones: find odd zone type(II):%d\n",content->m_type));
      break;
    }
    if (content->m_type != 0)
      continue;
    if (content->hasPageColumnBreak()) {
      WNTextInternal::ContentZone breakF;
      breakF.m_type=0x10;
      zone0->m_zonesList.push_back(breakF);
    }
    zone0->m_zonesList.insert(zone0->m_zonesList.end(),
                              content->m_zonesList.begin(),
                              content->m_zonesList.end());
    calledTypesList.insert(calledTypesList.end(),
                           content->m_textCalledTypesList.begin(),
                           content->m_textCalledTypesList.end());
  }
  mainZone.m_zones.resize(0);
  mainZone.m_zones.push_back(zone0);
  /* try to create the footnote and the header link */
  int numHeaderFooter = 0; // header+footer

  shared_ptr<WNTextInternal::ContentZones> content =  mainZone.m_zones[0];
  if (content->m_type != 0) return false;

  size_t numCalledZones = calledTypesList.size();
  for (size_t c = 0; c < numCalledZones; c++) {
    int called = calledTypesList[c];
    if (called < 5 || called > 6) {
      MWAW_DEBUG_MSG(("WNText::createZones: unknown content %d\n", called));
      continue;
    }
    int number = numHeaderFooter++;
    if (number >= int(headerFooterList.size())) {
      MWAW_DEBUG_MSG(("WNText::createZones: can not find zone for type:%d\n", called));
      continue;
    }
    if (called == 5) {
      if (m_state->m_header)
        MWAW_DEBUG_MSG(("WNText::createZones: header is already defined\n"));
      else
        m_state->m_header = headerFooterList[(size_t)number];
    } else if (called == 6) {
      if (m_state->m_footer)
        MWAW_DEBUG_MSG(("WNText::createZones: footer is already defined\n"));
      else
        m_state->m_footer = headerFooterList[(size_t)number];
    }
  }

  return true;
}

////////////////////////////////////////////////////////////
// try to parse a text data zone
////////////////////////////////////////////////////////////
shared_ptr<WNTextInternal::ContentZones> WNText::parseContent(WNEntry const &entry)
{
  int vers = version();

  if (m_state->getContentZone(entry.begin())) {
    MWAW_DEBUG_MSG(("WNText::parseContent: textContent is already parsed\n"));
    return m_state->getContentZone(entry.begin());
  }

  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("WNText::parseContent: text zone size is invalid\n"));
    return shared_ptr<WNTextInternal::ContentZones>();
  }

  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(TextData)[" << entry.id() << "]:";
  // print the text zone data
  f << std::hex << "txtZone=[" << std::hex << entry.m_val[0] << std::dec
    <<",h=" << entry.m_val[1] << "],";
  shared_ptr<WNTextInternal::ContentZones> text;
  if (vers >= 3) {
    if (entry.length() < 16) {
      MWAW_DEBUG_MSG(("WNText::parseContent: text zone size is too short\n"));
      return text;
    }
    input->seek(entry.begin(), WPX_SEEK_SET);
    if (input->readLong(4) != entry.length()) {
      MWAW_DEBUG_MSG(("WNText::parseContent: bad begin of last zone\n"));
      return text;
    }

    text.reset(new WNTextInternal::ContentZones);
    text->m_entry = entry;
    text->m_id = entry.id();

    // in 1/3 files, ptr0=begin.pos()+(flag<<15: rev?), but not in other files :-~
    long val;
    for (int i=0; i < 2; i++) {
      val = (long) input->readULong(4);
      if (!val) continue;
      f << "ptr" << i << "=" << std::hex << (val&0x7FFF) << std::dec;
      if (val>>15)
        f << "[" << std::hex << (val>>15) << std::dec << "],";
      else
        f << ",";
    }
    for (int i = 0; i < 2; i++) {
      val = input->readLong(2);
      f << "f" << i << "=" << val << ",";
    }
  } else {
    if (entry.length() < 2) {
      MWAW_DEBUG_MSG(("WNText::parseContent: text zone size is too short\n"));
      return text;
    }
    input->seek(entry.begin(), WPX_SEEK_SET);
    if (int(input->readULong(2))+2 != entry.length()) {
      MWAW_DEBUG_MSG(("WNText::parseContent: bad begin of last zone\n"));
      return text;
    }

    text.reset(new WNTextInternal::ContentZones);
    text->m_entry = entry;
    text->m_id = entry.id();
  }
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());

  while (long(input->tell()) < entry.end()) {
    long pos = input->tell();
    WNTextInternal::ContentZone zone;
    int c = (int) input->readULong(1);
    if (c == 0xf0) {
      MWAW_DEBUG_MSG(("WNText::parseContent: find 0xf0 entry\n"));
      ascFile.addPos(pos);
      ascFile.addNote("TextData:##");
      return text;
    }
    int type = 0;
    if ((c & 0xf0)==0xf0) type = (c & 0xf);
    zone.m_pos[0] = (type < 8) ? pos : pos+1;
    zone.m_type = type;
    if (type == 0) { // text entry
      while (long(input->tell()) != entry.end()) {
        c = (int) input->readULong(1);
        if (c == 0xf0) continue;
        if ((c&0xf0)==0xf0) {
          input->seek(-1, WPX_SEEK_CUR);
          break;
        }
      }
      zone.m_pos[1] = input->tell();
    } else if (type >= 8) {
      bool firstSeen = false;
      int numChar = 0;
      zone.m_pos[1] = entry.end();
      while (long(input->tell()) != entry.end()) {
        c = (int) input->readULong(1);
        if (c==0xf7) {
          zone.m_pos[1] = long(input->tell())-1;
          break;
        }

        if (c==0xf0)
          c = int(0xf0 | (input->readULong(1) & 0xf));
        numChar++;
        if (!firstSeen) {
          zone.m_value = c;
          firstSeen = true;
        }
      }
      if ((type == 0xb || type == 0xd) && numChar != 1) {
        MWAW_DEBUG_MSG(("WNText::parseContent: find odd size for type %x entry\n", type));
        ascFile.addPos(pos);
        ascFile.addNote("TextData:##");

        continue;
      }
    } else
      zone.m_pos[1] = pos+1;

    text->m_zonesList.push_back(zone);
    if (type >= 5 && type <= 6) // header footer
      text->m_textCalledTypesList.push_back(type);

    f.str("");
    f << "TextData-" << text->m_id << ":" << zone;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  entry.setParsed(true);
  ascFile.addPos(entry.end());
  ascFile.addNote("_");

  m_state->m_contentMap[entry.begin()] = text;
  if (long(input->tell()) != entry.end()) {
    MWAW_DEBUG_MSG(("WNText::parseContent: go after entry end\n"));
  }

  return text;
}

////////////////////////////////////////////////////////////
// try to parse a list of content zones
////////////////////////////////////////////////////////////
bool WNText::parseZone(WNEntry const &entry, std::vector<WNEntry> &listData)
{
  listData.resize(0);
  int vers = version();
  int dataSz = 16, headerSz = 16, lengthSz=4;
  if (vers <= 2) {
    dataSz = 6;
    headerSz = lengthSz = 2;
  }
  if (!entry.valid() || entry.length() < headerSz
      || (entry.length()%dataSz) !=(headerSz%dataSz)) {
    MWAW_DEBUG_MSG(("WNText::parseZone: text zone size is invalid\n"));
    return false;
  }

  long endPos = entry.end();

  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(entry.begin(), WPX_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  long sz = (long) input->readULong(lengthSz);
  if (vers>2 && sz != entry.length()) {
    MWAW_DEBUG_MSG(("WNText::parseZone: bad begin of last zone\n"));
    return false;
  }

  libmwaw::DebugStream f;
  f << "Entries(TextZone)[";
  switch(entry.id()) {
  case 0:
    f << "main";
    break;
  case 1:
    f << "header/footer";
    break;
  case 2:
    f << "note";
    break;
  default:
    f << entry.id() << "#";
    break;
  }
  f << "]:";
  long val;
  if (vers > 2) {
    /* seems very simillar that the 2 "ptr" in parseContent:
       sometimes ptr0=begin.pos()+some flag, sometimes not :-~ */
    f << "ptr?=" << std::hex << input->readULong(4) << std::dec << ",";
    f << "ptr2?=" << std::hex << input->readULong(4) << std::dec << ",";
    for (int i = 0; i < 2; i++) {
      val = input->readLong(2);
      f << "f" << i << "=" << val << ",";
    }
  }
  int numElts = int((entry.length()-headerSz)/dataSz);
  for (int elt=0; elt<numElts; elt++) {
    f << "entry" << elt << "=[";
    int flags = (int) input->readULong(1);
    /*  find
        40 44 45 : header ?
        60 61 64 65 : text ?
        70 74 7c : text + new page ?
        c0 c4 e0 e4 : empty

        header: 86: even, a6 even|odd, c6|odd ?
    */
    f << "fl=" << std::hex << flags << std::dec << ",";
    for (int i = 0; i < 3; i++) {
      val = (long) input->readULong(1);
      if (!i && val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
      if (vers <= 2) break;
    }

    WNEntry zEntry;
    zEntry.setBegin((long) input->readULong(vers <= 2 ? 2 : 4));
    if (vers > 2) zEntry.setLength((long) input->readULong(4));
    else if (zEntry.begin() &&
             m_mainParser->checkIfPositionValid(zEntry.begin())) {
      long actPos = input->tell();
      input->seek(zEntry.begin(), WPX_SEEK_SET);
      zEntry.setLength((long) input->readULong(2)+2);
      input->seek(actPos, WPX_SEEK_SET);
    }
    zEntry.setType("TextData");
    zEntry.m_fileType = 4;
    zEntry.m_val[0] = flags;
    zEntry.m_val[1] = (int) input->readLong(lengthSz);

    if (zEntry.begin() == 0 && zEntry.length()==0) f << "_" << ",";
    else {
      bool ok = true;
      if (zEntry.end() > endPos) {
        if (!m_mainParser->checkIfPositionValid(zEntry.end())) {
          f << "#";
          MWAW_DEBUG_MSG(("WNText::parseZone: odd pointer for text zone %d\n", elt));
          ok = false;
        } else
          endPos = zEntry.end();
      }
      if (ok) {
        listData.push_back(zEntry);
        f << "pos=" << std::hex << zEntry.begin() << std::dec << ",";
      }
    }
    f << "h=" << zEntry.m_val[1] << "],";
  }

  entry.setParsed(true);
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(entry.end());
  ascFile.addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// the fonts names
////////////////////////////////////////////////////////////
bool WNText::readFontNames(WNEntry const &entry)
{
  if (!entry.valid() || entry.length() < 0x10) {
    MWAW_DEBUG_MSG(("WNText::readFontNames: zone size is invalid\n"));
    return false;
  }

  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(entry.begin(), WPX_SEEK_SET);
  if (input->readLong(4) != entry.length()) {
    MWAW_DEBUG_MSG(("WNText::readFontNames: bad begin of last zone\n"));
    return false;
  }
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(Fonts):";
  f << "ptr?=" << std::hex << input->readULong(4) << std::dec << ",";
  f << "ptr2?=" << std::hex << input->readULong(4) << std::dec << ",";
  long pos, val;
  for (int i = 0; i < 3; i++) { // 6, 0, 0
    val = input->readLong(2);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  for (int i = 0; i < 2; i++) { // 0
    val = input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  if (long(input->tell())+N*8 > entry.end()) {
    MWAW_DEBUG_MSG(("WNText::readFontNames: the zone is too short\n"));
    return false;
  }
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  std::vector<long> defPos;
  for (int n = 0; n < N; n++) {
    pos = input->tell();
    f.str("");
    f << "Fonts[" << n << "]:";
    int type = (int) input->readULong(1);
    switch(type) {
    case 0:
      f << "def,";
      break;
    default:
      MWAW_DEBUG_MSG(("WNText::readFontNames: find unknown type %d\n", type));
      f << "#type=" << type << ",";
      break;
    }
    for (int i = 0; i < 3; i++) { // always 0
      val = input->readLong(1);
      if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
    }
    val = (long) input->readULong(4);
    defPos.push_back(pos+val);
    // fixme: used this to read the data...
    f << "defPos=" << std::hex << pos+val << std::dec << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  for (int n = 0; n < N; n++) {
    pos = defPos[(size_t) n];
    if (pos == entry.end()) continue;
    if (pos+13 > entry.end()) {
      MWAW_DEBUG_MSG(("WNText::readFontNames: can not read entry : %d\n", n));
      continue;
    }

    input->seek(pos, WPX_SEEK_SET);
    f.str("");
    f << "FontsData[" << n << "]:";
    val = input->readLong(2);
    f << "fId(local)=" << val << ","; // almost always local fid
    val = input->readLong(2);
    if (val) f << "unkn=" << val << ",";
    f << "ptr?=" << std::hex << input->readULong(4) << std::dec << ",";
    for (int i = 0; i < 2; i++) { // always 0
      val = input->readLong(2);
      if (val) f << "f" << i << "=" << val << ",";
    }
    int sz = (int) input->readULong(1);
    if (pos+13+sz > entry.end()) {
      MWAW_DEBUG_MSG(("WNText::readFontNames: can not read entry name : %d\n", n));
      return false;
    }

    bool ok = true;
    std::string name("");
    for (int i = 0; i < sz; i++) {
      char ch = (char) input->readULong(1);
      if (ch == '\0') {
        MWAW_DEBUG_MSG(("WNText::readFontNames: pb with name field %d\n", n));
        ok = false;
        break;
      } else if (ch & 0x80) {
        static bool first = true;
        if (first) {
          MWAW_DEBUG_MSG(("WNText::readFontNames: find odd font\n"));
          first = false;
        }
        ok = false;
      }
      name += ch;
    }
    f << name << ",";
    if (name.length() && ok)
      m_state->m_localFIdMap[n]= m_parserState->m_fontConverter->getId(name);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  entry.setParsed(true);
  ascFile.addPos(entry.end());
  ascFile.addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// the fonts properties
////////////////////////////////////////////////////////////
bool WNText::readFont(MWAWInputStream &input, bool inStyle, WNTextInternal::Font &font)
{
  font = WNTextInternal::Font();
  int vers = version();
  libmwaw::DebugStream f;

  long pos = input.tell();
  int expectedLength = vers <= 2 ? 4 : 14;
  input.seek(expectedLength, WPX_SEEK_CUR);
  if (pos+expectedLength != long(input.tell())) {
    MWAW_DEBUG_MSG(("WNText::readFont: zone is too short \n"));
    return false;
  }
  input.seek(pos, WPX_SEEK_SET);

  font.m_font.setId(m_state->getFontId((int) input.readULong(2)));
  font.m_font.setSize((float) input.readULong(vers <= 2 ? 1 : 2));
  int flag = (int) input.readULong(1);
  uint32_t flags=0;
  if (flag&0x1) flags |= MWAWFont::boldBit;
  if (flag&0x2) flags |= MWAWFont::italicBit;
  if (flag&0x4) font.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
  if (flag&0x8) flags |= MWAWFont::embossBit;
  if (flag&0x10) flags |= MWAWFont::shadowBit;
  if (flag&0x20) font.m_font.setDeltaLetterSpacing(-1);
  if (flag&0x40) font.m_font.setDeltaLetterSpacing(1);
  if (flag&0x80) f << "#flag0[0x80],";

  if (vers <= 2) {
    font.m_font.setFlags(flags);
    font.m_font.m_extra = f.str();
    return true;
  }
  flag = (int) input.readULong(1);
  if (flag&0x80) font.m_font.setStrikeOutStyle(MWAWFont::Line::Simple);
  if (flag&0x7f) f << "#flag1=" << std::hex << (flag&0x7f) << std::dec << ",";

  flag = (int) input.readULong(1);
  if (flag&0x2) {
    font.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
    font.m_font.setUnderlineType(MWAWFont::Line::Double);
  }
  if (flag&0x4) {
    font.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
    font.m_font.setUnderlineWidth(2.0);
  }
  if (flag&0x8) {
    font.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
    font.m_font.setUnderlineColor(MWAWColor(0xb0,0xb0,0xb0));
  }
  if (flag&0x10) {
    font.m_font.setUnderlineStyle(MWAWFont::Line::Wave);
    f << "underline[charcoal],";
  }
  if (flag&0x20) font.m_font.setUnderlineStyle(MWAWFont::Line::Dash);
  if (flag&0x40) font.m_font.setUnderlineStyle(MWAWFont::Line::Dot);
  if (flag&0x81) f << "#flag2=" << std::hex << (flag&0x81) << std::dec << ",";

  int color = (int) input.readULong(1); // fixme find color map
  if (color) {
    MWAWColor col;
    if (m_mainParser->getColor(color,col))
      font.m_font.setColor(col);
    else
      f << "#colorId=" << color << ",";
  }
  int heightDecal = (int) input.readLong(2);
  if (heightDecal)
    font.m_font.set(MWAWFont::Script(float(heightDecal), WPX_POINT));

  font.m_font.setFlags(flags);
  font.m_font.m_extra = f.str();

  int act = 0;
  if (inStyle) {
    font.m_flags[act++] = (int) input.readULong(4);
    font.m_flags[act++] = (int) input.readLong(2);
  } else {
    font.m_flags[act++] = (int) input.readLong(1); // 5: note def, 6: note pos ?
    font.m_styleId[0] = (int) input.readULong(1)-1;
    font.m_styleId[1] = (int) input.readULong(1)-1;
    font.m_flags[act++] = (int) input.readLong(1); // alway 0
  }
  return true;
}

////////////////////////////////////////////////////////////
// the paragraphs properties
////////////////////////////////////////////////////////////
bool WNText::readParagraph(MWAWInputStream &input, WNTextInternal::Paragraph &ruler)
{
  libmwaw::DebugStream f;
  int vers = version();
  ruler = WNTextInternal::Paragraph();
  long pos = input.tell();
  int expectedLength = vers <= 2 ? 8 : 16;
  input.seek(expectedLength, WPX_SEEK_CUR);
  if (pos+expectedLength != long(input.tell())) {
    MWAW_DEBUG_MSG(("WNText::readParagraph: zone is too short: %ld\n",
                    long(input.tell()) - pos));
    return false;
  }
  input.seek(pos, WPX_SEEK_SET);
  int actVal = 0;
  /* small number, 0, small number < 3 */
  if (vers >= 3) {
    for (int i = 0; i < 2; i++)
      ruler.m_values[actVal++] = (int) input.readULong(1);
  }
  ruler.m_margins[1]=(int) input.readLong(2);
  ruler.m_margins[2]=(int) input.readLong(2);
  ruler.m_margins[0]=(int) input.readLong(2);
  *(ruler.m_margins[0])-=ruler.m_margins[1].get();
  int height = 0;
  if (vers >= 3) {
    height=(int) input.readLong(2);
    for (int i = 0; i < 3; i++)
      ruler.m_values[actVal++] = (int) input.readULong(2);
  }
  int flag = (int) input.readULong(1);
  switch (flag & 3) {
  case 0:
    break; // left
  case 1:
    ruler.m_justify = MWAWParagraph::JustificationFull;
    break;
  case 2:
    ruler.m_justify = MWAWParagraph::JustificationCenter;
    break;
  case 3:
    ruler.m_justify = MWAWParagraph::JustificationRight;
    break;
  default:
    break;
  }
  bool interlineFixed = (flag & 0x80);
  ruler.m_values[actVal++] = (flag & 0x7c);
  if (vers <= 2)
    height=(int) input.readULong(1);
  else
    ruler.m_values[actVal++] = (int) input.readULong(1); // always 0
  ruler.m_tabs->resize(0);
  if (!input.atEOS()) {
    int previousVal = 0;
    int tab = 0;
    while (!input.atEOS()) {
      MWAWTabStop newTab;
      int newVal = (int) input.readULong(2);
      if (tab && newVal < previousVal) {
        MWAW_DEBUG_MSG(("WNText::readParagraph: find bad tab pos\n"));
        f << "#tab[" << tab << ",";
        input.seek(-1, WPX_SEEK_CUR);
        break;
      }
      previousVal = newVal;
      ++tab;
      newTab.m_position = (newVal>>2)/72.;
      switch(newVal & 3) {
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
      ruler.m_tabs->push_back(newTab);
    }
  }
  if (version() <= 2) // distance from right
    ruler.m_margins[2] = int(72.0*m_mainParser->getPageWidth())-ruler.m_margins[2].get();
  *(ruler.m_margins[2]) -= 28.;
  if (ruler.m_margins[2].get() < 0) ruler.m_margins[2]=0;
  if (!interlineFixed && height>=0)
    ruler.setInterline(height,WPX_POINT,MWAWParagraph::AtLeast);
  else if (height>0)
    ruler.setInterline(height,WPX_POINT);
  else
    f << "##interline=" << height << "pt,";
  ruler.m_extra = f.str();
  return true;
}

void WNText::setProperty(WNTextInternal::Paragraph const &ruler)
{
  m_state->m_paragraph = ruler;
  if (!m_parserState->m_listener) return;
  m_parserState->m_listener->setParagraph(ruler);
}

////////////////////////////////////////////////////////////
// the style properties ?
////////////////////////////////////////////////////////////
bool WNText::readStyles(WNEntry const &entry)
{
  m_state->m_styleList.resize(0);
  m_state->m_styleMap.clear();

  if (!entry.valid() || entry.length() < 0x10) {
    MWAW_DEBUG_MSG(("WNText::readStyles: zone size is invalid\n"));
    return false;
  }

  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(entry.begin(), WPX_SEEK_SET);
  if (input->readLong(4) != entry.length()) {
    MWAW_DEBUG_MSG(("WNText::readStyles: bad begin of last zone\n"));
    return false;
  }
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(Styles):";
  f << "ptr?=" << std::hex << input->readULong(4) << std::dec << ",";
  f << "ptr2?=" << std::hex << input->readULong(4) << std::dec << ",";
  long pos, val;
  for (int i = 0; i < 3; i++) { // 5, 0|100, 0
    val = input->readLong(2);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  for (int i = 0; i < 2; i++) { // 0
    val = input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  if (long(input->tell())+N*8 > entry.end()) {
    MWAW_DEBUG_MSG(("WNText::readStyles: the zone is too short\n"));
    return false;
  }
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  std::vector<long> defPos;
  std::map<long, int> seenMap;
  std::map<long, int>::iterator seenIt;
  for (int n = 0; n < N; n++) {
    pos = input->tell();
    f.str("");
    f << "Styles[" << n << "]:";
    int type = (int) input->readULong(1);
    switch(type) {
    case 0:
      f << "def[named],";
      break;
    case 2:
      f << "def,";
      break;
    case 4:
      f << "user?[named],";
      break;
    case 0x80:
      f << "none,";
      break;
    default:
      MWAW_DEBUG_MSG(("WNText::readStyles: find unknown type %d\n", type));
      f << "#type=" << type << ",";
      break;
    }
    for (int i = 0; i < 3; i++) { // always 0
      val = input->readLong(1);
      if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
    }
    val = (long) input->readULong(4);
    if (type != 0x80 && pos+val != entry.end()) {
      seenIt = seenMap.find(pos+val);
      int id = 0;
      if (seenIt != seenMap.end())
        id = seenIt->second;
      else {
        id = int(defPos.size());
        defPos.push_back(pos+val);
        seenMap[pos+val] = id;
      }
      m_state->m_styleMap[n] = id;
      f << "style" << id << ",";
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  for (size_t n = 0; n < defPos.size(); n++) {
    pos = defPos[n];
    if (pos+34 > entry.end()) {
      MWAW_DEBUG_MSG(("WNText::readStyles: can not read entry : %ld\n", long(n)));
      return false;
    }

    WNTextInternal::Style style;
    input->seek(pos, WPX_SEEK_SET);
    f.str("");
    int type = (int) input->readULong(1);
    style.m_values[0] = int(type>>4);// find 1, 2 or 3
    style.m_values[1] = int(type&0xF); // always 0 ?

    /* f2: 0 or 0x30, 35, 36, 37, 38, 4d, 53
       f3: small number : maybe a previous id?
     */
    for (int i = 2; i < 4; i++) {
      style.m_values[i] = (int) input->readULong(1);
    }
    style.m_nextId = (int) input->readLong(1)-1; // almost always n+1, so...
    /* f4: almost always 0, but one time f4=nextId
       f5: always=0
     */
    for (int i = 4; i < 6; i++) {
      style.m_values[i] = (int) input->readULong(1);
    }
    for (int i = 7; i < 10; i++) { // always 0
      style.m_values[i] = (int) input->readLong(2);
    }
    for (int i = 0; i < 6; i++) {
      style.m_flags[i] = (int) input->readULong(2);
    }
    for (int i = 10; i < 12; i++) { // always 0
      style.m_values[i] = (int) input->readLong(2);
    }
    int relPos[3];
    for (int i = 0; i < 3; i++)
      relPos[i] = (int) input->readULong(2);
    style.m_values[12] = (int) input->readULong(2); // another pos ?
    if (relPos[0]) { // style name
      input->seek(pos+relPos[0], WPX_SEEK_SET);
      int sz = (int) input->readULong(1);
      if (!sz || pos+relPos[0]+1+sz > entry.end()) {
        MWAW_DEBUG_MSG(("WNText::readStyles: can not read name for entry : %ld\n", long(n)));
        f << "name[length],";
      } else {
        std::string name("");
        for (int i = 0; i < sz; i++) name+=char(input->readLong(1));
        style.m_name = name;
        // maybe followed by an integer
      }
    }
    std::string error = f.str();
    f.str("");
    f << "StylesData[" << n << ":header]:" << style << ",";
    if (error.length()) f << "#errors[" << error << "],";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    if (relPos[1]) { // font ?
      f.str("");
      f << "StylesData[" << n << ":font]:";
      input->seek(pos+relPos[1], WPX_SEEK_SET);
      input->pushLimit(relPos[2] ? pos+relPos[2] : entry.end());

      WNTextInternal::Font font;
      if (readFont(*input, true, font)) {
        style.m_font = font;
        f << font.m_font.getDebugString(m_parserState->m_fontConverter) << font;
      } else
        f << "#";

      input->popLimit();
      ascFile.addPos(pos+relPos[1]);
      ascFile.addNote(f.str().c_str());
    }
    if (relPos[2]) {
      f.str("");
      f << "StylesData[" << n << ":ruler]:";
      input->seek(pos+relPos[2], WPX_SEEK_SET);
      int sz = (int) input->readULong(2);
      input->pushLimit(pos+relPos[2]+sz);
      WNTextInternal::Paragraph ruler;
      if (readParagraph(*input, ruler)) {
        style.m_paragraph = ruler;
        f << ruler;
      } else
        f << "#";
      input->popLimit();
      ascFile.addPos(pos+relPos[2]);
      ascFile.addNote(f.str().c_str());
    }
    m_state->m_styleList.push_back(style);
  }

  entry.setParsed(true);
  ascFile.addPos(entry.end());
  ascFile.addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// zone which corresponds to the token
////////////////////////////////////////////////////////////
bool WNText::readToken(MWAWInputStream &input, WNTextInternal::Token &token)
{
  token=WNTextInternal::Token();

  long pos = input.tell();
  input.seek(pos+54, WPX_SEEK_SET);
  if (pos+54 != long(input.tell())) {
    MWAW_DEBUG_MSG(("WNText::readToken: zone is too short \n"));
    return false;
  }
  input.seek(pos, WPX_SEEK_SET);
  int dim[4];
  for (int i=0; i < 4; i++)
    dim[i] = (int) input.readLong(2);
  token.m_box = Box2i(Vec2i(dim[1], dim[0]), Vec2i(dim[3], dim[2]));
  int actVal = 0;
  for (int st = 0; st < 2; st++) {
    int dim0 = (int) input.readLong(2);
    token.m_values[actVal++] = (int) input.readLong(2); // always 0
    token.m_values[actVal++] = (int) input.readLong(2); // always 0
    int dim1 = (int) input.readLong(2);
    token.m_pos[st] = Vec2i(dim1, -dim0);
  }
  /*
    f0/f2:  0 or 1 ( very similar)
    f1/f3 : 0 or big number
   */
  for (int i = 0; i < 4; i++) {
    token.m_values[actVal++] = (int) input.readULong(2);
  }
  /*
    f8=1, f9=14
    other 0
  */
  for (int i = 0; i < 10; i++) {
    token.m_values[actVal++] = (int) input.readLong(2);
  }
  token.m_graphicZone = (int) input.readLong(2);
  return true;
}

bool WNText::readTokenV2(MWAWInputStream &input, WNTextInternal::Token &token)
{
  token=WNTextInternal::Token();

  long actPos = input.tell();
  int dim[2];
  for (int i=0; i < 2; i++)
    dim[i] = (int) input.readLong(2);
  Vec2i box(dim[1], dim[0]);
  token.m_box=Box2i(Vec2i(0,0), box);
  // we need to get the size, so...
  while (!input.atEOS())
    input.seek(0x100, WPX_SEEK_CUR);
  long endPos = input.tell();
  long sz = endPos-actPos-4;
  if (sz <= 0) return false;
  input.seek(actPos+4, WPX_SEEK_SET);
  MWAWInputStreamPtr ip(&input,MWAW_shared_ptr_noop_deleter<MWAWInputStream>());
  shared_ptr<MWAWPict> pict(MWAWPictData::get(ip, (int) sz));
  if (!pict) {
    MWAW_DEBUG_MSG(("WNParser::readTokenV2: can not read the picture\n"));
    return false;
  }
  if (!m_parserState->m_listener) return true;

  WPXBinaryData data;
  std::string type;
  MWAWPosition pictPos;
  if (box.x() > 0 && box.y() > 0) {
    pictPos=MWAWPosition(Vec2f(0,0),box, WPX_POINT);
    pictPos.setNaturalSize(pict->getBdBox().size());
  } else
    pictPos=MWAWPosition(Vec2f(0,0),pict->getBdBox().size(), WPX_POINT);
  pictPos.setRelativePosition(MWAWPosition::Char);

  if (pict->getBinary(data,type))
    m_parserState->m_listener->insertPicture(pictPos, data, type);

  return true;
}

////////////////////////////////////////////////////////////
// zone which corresponds to the table
////////////////////////////////////////////////////////////
bool WNText::readTable(MWAWInputStream &input, WNTextInternal::TableData &table)
{
  table=WNTextInternal::TableData();
  long pos = input.tell();

  table.m_type = (int) input.readULong(1);
  if (input.atEOS()) {
    if (table.m_type!=0) {
      MWAW_DEBUG_MSG(("WNText::readTable: find a zone will 0 size\n"));
      return false;
    }
    return true;
  }
  input.seek(pos+28, WPX_SEEK_SET);
  if (pos+28 != long(input.tell())) {
    MWAW_DEBUG_MSG(("WNText::readTable: zone is too short \n"));
    return false;
  }
  input.seek(pos+1, WPX_SEEK_SET);
  int actVal = 0;
  table.m_values[actVal++] = (int) input.readLong(1);
  table.m_values[actVal++] = (int) input.readLong(1);
  int backColor = (int) input.readULong(1);
  MWAWColor col;
  if (m_mainParser->getColor(backColor,col))
    table.m_color = col;
  else {
    MWAW_DEBUG_MSG(("WNText::readTable: can not read backgroundColor: %d\n", backColor));
  }
  for (int i = 0; i < 4; i++) {
    table.m_flags[i] = (int) input.readULong(1); // 0x80, 0x81, 5, 0 : border flag ?
    table.m_values[actVal++] = (int) input.readLong(1); //  always 0 ?
  }
  for (int i = 0; i < 3; i++)
    table.m_values[actVal++] = (int) input.readLong(2); // alway 0

  int dim[4];
  for (int i = 0; i < 4; i++)
    dim[i] = (int) input.readLong(2);
  table.m_box = Box2i(Vec2i(dim[1], dim[0]), Vec2i(dim[3], dim[2]));
  table.m_values[actVal++] = (int) input.readLong(2);

  return true;
}

////////////////////////////////////////////////////////////
//! send data to the listener
bool WNText::send(WNEntry const &entry)
{
  shared_ptr<WNTextInternal::ContentZones> text
  (m_state->getContentZone(entry.begin()));
  if (!text) {
    MWAW_DEBUG_MSG(("WNText::send: can not find entry\n"));
    return false;
  }
  WNTextInternal::Paragraph ruler=m_state->getDefaultParagraph(text->m_type);
  text->m_sent = true;
  return send(text->m_zonesList, text->m_footnoteList, ruler);
}

bool WNText::send(std::vector<WNTextInternal::ContentZone> &listZones,
                  std::vector<shared_ptr<WNTextInternal::ContentZones> > &footnoteList,
                  WNTextInternal::Paragraph &ruler)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("WNText::send: can not find the listener\n"));
    return false;
  }
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  int vers = version();
  MWAWFont actFont(3,12); // default geneva 12pt
  listener->setFont(actFont);
  int extraDecal = 0; // for v2
  bool rulerSet = false;
  int numLineTabs = 0, actTabs = 0, numFootnote = 0;

  shared_ptr<WNTextInternal::Table> table;
  shared_ptr<WNTextInternal::Cell> cell;

  MWAWInputStreamPtr &input= m_parserState->m_input;
  for (size_t z = 0; z < listZones.size(); z++) {
    WNTextInternal::ContentZone const &zone = listZones[z];
    if (table && cell) {
      bool done = true;
      switch (zone.m_type) {
      case 0xa:
        done = false;
        break;
      case 4:
        if (numFootnote < int(footnoteList.size())) {
          cell->m_footnoteList.push_back(footnoteList[(size_t) numFootnote]);
          numFootnote++;
        }
        cell->m_zonesList.push_back(zone);
        break;
      case 0x10:
        MWAW_DEBUG_MSG(("WNText::send: find a page/column break in table(ignored)\n"));
        break;
      default:
        cell->m_zonesList.push_back(zone);
        break;
      }
      if (done)
        continue;
    } else if (table && zone.m_type != 0xa) {
      static bool first=true;
      if (first) {
        first = false;
        MWAW_DEBUG_MSG(("WNText::send: find some data in table but outside cell\n"));
      }
    }
    switch(zone.m_type) {
    case 4:
      if (numFootnote < int(footnoteList.size())) {
        m_mainParser->sendFootnote(footnoteList[(size_t) numFootnote]->m_entry);
        numFootnote++;
      } else {
        MWAW_DEBUG_MSG(("WNText::send: can not find footnote:%d\n", numFootnote));
      }
      break;
    case 0xd:
      switch(zone.m_value) {
      case 0:
        listener->insertField(MWAWField(MWAWField::PageNumber));
        break;
      case 1: {
        MWAWField date(MWAWField::Date);
        date.m_DTFormat = "%a, %b %d, %Y";
        listener->insertField(date);
        break;
      }
      case 2: {
        MWAWField time(MWAWField::Time);
        time.m_DTFormat="%H:%M";
        listener->insertField(time);
        break;
      }
      case 3: // note field : ok
      default:
        break;
      }
      break;
    case 0x10:
      if (m_state->m_numColumns <= 1 && ++m_state->m_actualPage <= m_state->m_numPages)
        m_mainParser->newPage(m_state->m_actualPage);
      else if (m_state->m_numColumns > 1 && listener)
        listener->insertBreak(MWAWContentListener::ColumnBreak);
      break;
    default:
      break;
    }

    if ((zone.m_type>0 && zone.m_type < 8) || zone.m_type == 0xd || zone.m_type == 0x10)
      continue;

    WPXBinaryData data;
    input->seek(zone.m_pos[0],WPX_SEEK_SET); //000a2f
    while(int(input->tell()) < zone.m_pos[1]) {
      int ch = (int) input->readULong(1);
      if (ch == 0xf0) {
        ch = (int) input->readULong(1);
        if (ch & 0xf0) {
          MWAW_DEBUG_MSG(("WNText::send: find odd 0xF0 following\n"));
          continue;
        }
        data.append((unsigned char)(0xf0 | ch));
      } else
        data.append((unsigned char)ch);
    }

    f.str("");
    if (zone.m_type == 0) {
      long sz = (long) data.size();
      const unsigned char *buffer = data.getDataBuffer();
      if (!buffer && sz) {
        MWAW_DEBUG_MSG(("WNText::send: can find data buffer\n"));
        sz = 0;
      }
      if (sz && !rulerSet) {
        setProperty(ruler);
        numLineTabs = int(ruler.m_tabs->size());
        rulerSet = true;
      }
      for (int i = 0; i < sz; i++) {
        unsigned char c = *(buffer++);
        f << c;
        switch (c) {
        case 0x9:
          if (actTabs++ < numLineTabs)
            listener->insertTab();
          else
            listener->insertChar(' ');
          break;
        case 0xd:
          // this is marks the end of a paragraph
          listener->insertEOL();
          setProperty(ruler);
          actTabs = 0;
          break;
        default:
          listener->insertCharacter(c);
          break;
        }
      }
      ascFile.addPos(zone.m_pos[0]);
      ascFile.addNote(f.str().c_str());
      continue;
    }
    WPXInputStream *dataStream = const_cast<WPXInputStream *>(data.getDataStream());
    MWAWInputStream dataInput(dataStream, false);
    switch(zone.m_type) {
    case 0x9: { // only in v2
      extraDecal = zone.m_value;
      MWAWFont font(actFont);
      if (extraDecal > 0) font.set(MWAWFont::Script::super100());
      else if (extraDecal < 0) font.set(MWAWFont::Script::sub100());
      listener->setFont(font);
      break;
    }
    case 0xa: {  // only in writenow 4.0 : related to a table ?
      if (!dataStream) break;
      WNTextInternal::TableData tableData;
      if (readTable(dataInput, tableData)) {
        f << tableData;

        bool needSendTable = false;
        bool needCreateCell = false;
        switch(zone.m_value) {
        case 0:
          if (table)
            needSendTable = true;
          else {
            MWAW_DEBUG_MSG(("WNText::send: Find odd end of table\n"));
          }
          break;
        case 1:
          if (!table)
            table.reset(new WNTextInternal::Table);
          else {
            MWAW_DEBUG_MSG(("WNText::send: Find a table in a table\n"));
          }
          break;
        case 2:
          if (table)
            needCreateCell = true;
          else {
            MWAW_DEBUG_MSG(("WNText::send: Find cell outside a table\n"));
          }
          break;
        default:
          break;
        }

        if (needSendTable) {
          if (table) {
            if (!table->sendTable(listener))
              table->sendAsText(listener);
          } else {
            MWAW_DEBUG_MSG(("WNText::send: can not find the cell to send...\n"));
          }
          table.reset();
        }
        if (needCreateCell) {
          cell.reset(new WNTextInternal::Cell(*this));
          tableData.updateCell(*cell);
          table->add(cell);
        }
      } else
        f << "#";
      break;
    }
    case 0xb: {
      // this seems to define the leader than add a tab
      if (actTabs++ < numLineTabs)
        listener->insertTab();
      else
        listener->insertChar(' ');
      break;
    }
    case 0xc: {
      if (!dataStream) break;
      WNTextInternal::Paragraph newParagraph;
      if (readParagraph(dataInput, newParagraph)) {
        ruler = newParagraph;
        numLineTabs = int(ruler.m_tabs->size());
        setProperty(ruler);
        rulerSet = true;
        f << ruler;
      } else
        f << "#";
      break;
    }
    case 0xe: {
      if (!dataStream) break;
      WNTextInternal::Token token;
      if (vers >= 3) {
        if (readToken(dataInput, token)) {
          m_mainParser->sendGraphic(token.m_graphicZone, token.m_box);
          f << token;
        } else
          f << "#";
      } else {
        if (readTokenV2(dataInput, token))
          f << token;
        else
          f << "#";
      }
      break;
    }
    case 0xf: {
      if (!dataStream) break;
      WNTextInternal::Font font;
      if (readFont(dataInput, false, font)) {
        actFont=font.m_font;
        f << font.m_font.getDebugString(m_parserState->m_fontConverter) << font;
        if (extraDecal > 0) font.m_font.set(MWAWFont::Script::super100());
        else if (extraDecal < 0) font.m_font.set(MWAWFont::Script::sub100());
        listener->setFont(font.m_font);
      } else
        f << "#";
      break;
    }
    default:
      MWAW_DEBUG_MSG(("WNText::send: find keyword %x\n",zone.m_type));
      break;
    }

    ascFile.addPos(zone.m_pos[0]-1);
    ascFile.addNote(f.str().c_str());
  }

  if (table) {
    MWAW_DEBUG_MSG(("WNText::send: a table is not closed\n"));
    if (!table->sendTable(listener))
      table->sendAsText(listener);
  }

  return true;
}

void WNText::sendZone(int id)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("WNText::sendZone: can not find the listener\n"));
    return;
  }
  if (id < 0 || id >= 3) {
    MWAW_DEBUG_MSG(("WNText::sendZone: called with id=%d\n",id));
    return;
  }
  if (id == 0) {
    int nCol, width=0;
    m_mainParser->getColumnInfo(nCol, width);
    if (m_state->m_numColumns > 1) {
      if (width <= 0)
        width = int((72.0*m_mainParser->getPageWidth())/m_state->m_numColumns);
      if (listener->isSectionOpened())
        listener->closeSection();
      MWAWSection sec;
      sec.setColumns(m_state->m_numColumns, double(width), WPX_POINT);
      listener->openSection(sec);
    }
  }
  WNTextInternal::Zone &mZone = m_state->m_mainZones[id];
  WNTextInternal::Paragraph ruler=m_state->getDefaultParagraph(id);
  if (id==0)
    listener->setParagraph(ruler);
  for (size_t i = 0; i < mZone.m_zones.size(); i++) {
    if (mZone.m_zones[i]->m_sent) continue;
    if (id == 0 && mZone.m_zones[i]->m_type) continue;
    if (id) ruler=m_state->getDefaultParagraph(id);
    send(mZone.m_zones[i]->m_zonesList, mZone.m_zones[i]->m_footnoteList, ruler);
    mZone.m_zones[i]->m_sent = true;
  }
}

void WNText::flushExtra()
{
  for (int z = 0; z < 3; z++) {
    if (z == 1) continue;
    sendZone(z);
  }
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
