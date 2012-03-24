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

#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>

#include <libwpd/WPXString.h>

#include "TMWAWPosition.hxx"

#include "MWAWStruct.hxx"
#include "MWAWTools.hxx"
#include "MWAWContentListener.hxx"

#include "CWText.hxx"

#include "CWParser.hxx"
#include "CWStruct.hxx"

/** Internal: the structures of a CWText */
namespace CWTextInternal
{

/** Internal: class to store the paragraph properties */
struct Ruler {
  //! Constructor
  Ruler() : m_justify (DMWAW_PARAGRAPH_JUSTIFICATION_LEFT),
    m_interlineFixed(-1), m_interlinePercent(0.0), m_tabs(),
    m_error("") {
    for(int c = 0; c < 2; c++) {
      m_margins[c] = 0.0;
      m_spacings[c] = 0;
    }
    m_margins[2] = -1.0;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Ruler const &ind) {
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
    if (ind.m_interlineFixed > 0) o << "interline=" << ind.m_interlineFixed << "pt,";
    if (ind.m_interlinePercent > 0.0) o << "interline=" << 100.*ind.m_interlinePercent << "%,";
    libmwaw::internal::printTabs(o, ind.m_tabs);
    if (ind.m_error.length()) o << "," << ind.m_error;
    return o;
  }

  /** the margins in inches
   *
   * 0: first line left, 1: left, 2: right (from right)
   */
  float m_margins[3];
  //! paragraph justification : DMWAW_PARAGRAPH_JUSTIFICATION*
  int m_justify;
  /** interline (in point)*/
  int m_interlineFixed;
  /** interline */
  float m_interlinePercent;
  /** the spacings ( 0: before, 1: after ) in point*/
  int m_spacings[2];
  //! the tabulations
  std::vector<DMWAWTabStop> m_tabs;
  /** the errors */
  std::string m_error;
};

struct Style {
  //! constructor
  Style() : m_fontId(-1), m_rulerId(-1) {
  }
  //! the char
  int m_fontId;
  //! the ruler
  int m_rulerId;
};

struct ParagraphInfo {
  ParagraphInfo() : m_styleId(-1), m_unknown(0), m_error("") {
  }

  friend std::ostream &operator<<(std::ostream &o, ParagraphInfo const &info) {
    if (info.m_styleId >= 0) o << "style=" << info.m_styleId <<",";
    if (info.m_unknown) o << "unknown=" << info.m_unknown << ",";
    if (info.m_error.length()) o << info.m_error;
    return o;
  }

  int m_styleId;
  int m_unknown;
  std::string m_error;
};

struct TextZoneInfo {
  TextZoneInfo() : m_pos(0), m_N(0), m_error("") {
  }

  friend std::ostream &operator<<(std::ostream &o, TextZoneInfo const &info) {
    o << "pos=" << info.m_pos << ",";
    if (info.m_N >= 0) o << "size=" << info.m_N <<",";
    if (info.m_error.length()) o << info.m_error;
    return o;
  }
  long m_pos;
  int m_N;
  std::string m_error;
};

enum TokenType { TKN_UNKNOWN, TKN_FOOTNOTE, TKN_PAGENUMBER, TKN_GRAPHIC };

/** Internal: class to store field definition: TOKN entry*/
struct Token {
  //! constructor
  Token() : m_type(TKN_UNKNOWN), m_zoneId(-1), m_page(-1), m_error("") {
    for (int i = 0; i < 3; i++) m_unknown[i] = 0;
    for (int i = 0; i < 2; i++) m_size[i] = 0;
  }
  //! operator <<
  friend std::ostream &operator<<(std::ostream &o, Token const &tok);
  //! the type
  TokenType m_type;
  //! the zone id which correspond to this type
  int m_zoneId;
  //! the page
  int m_page;
  //! the size(?)
  int m_size[2];
  //! the unknown zone
  int m_unknown[3];
  //! a string used to store the parsing errors
  std::string m_error;
};
//! operator<< for Token
std::ostream &operator<<(std::ostream &o, Token const &tok)
{
  switch (tok.m_type) {
  case TKN_FOOTNOTE:
    o << "footnoote,";
    break;
  case TKN_PAGENUMBER:
    o << "field[pageNumber],";
    break;
  case TKN_GRAPHIC:
    o << "graphic,";
    break;
  default:
    o << "##field[unknown]" << ",";
    break;
  }
  if (tok.m_zoneId != -1) o << "zoneId=" << tok.m_zoneId << ",";
  if (tok.m_page != -1) o << "page?=" << tok.m_page << ",";
  o << "dim?=" << tok.m_size[0] << "x" << tok.m_size[1] << ",";
  for (int i = 0; i < 3; i++) {
    if (tok.m_unknown[i] == 0)
      continue;
    o << "#unkn" << i << "=" << std::hex << tok.m_unknown[i] << std::dec << ",";
  }
  if (!tok.m_error.empty()) o << "err=[" << tok.m_error << "]";
  return o;
}

struct Zone : public CWStruct::DSET {
  Zone(CWStruct::DSET const dset = CWStruct::DSET()) :
    CWStruct::DSET(dset), m_zones(), m_numChar(0), m_numTextZone(0), m_numParagInfo(0),
    m_numFont(0), m_parsed(false), m_fontMap(), m_styleMap(),
    m_tokenMap(), m_textZoneList() {
    for (int i = 0; i < 2; i++)
      m_unknown[i] = 0;
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Zone const &doc) {
    o << static_cast<CWStruct::DSET const &>(doc);
    if (doc.m_numChar) o << "numChar=" << doc.m_numChar << ",";
    if (doc.m_numTextZone) o << "numTextZone=" << doc.m_numTextZone << ",";
    if (doc.m_numParagInfo) o << "numParag=" << doc.m_numParagInfo << ",";
    if (doc.m_numFont) o << "numFont=" << doc.m_numFont << ",";
    for (int i = 0; i < 2; i++) {
      if (!doc.m_unknown[i]) continue;
      o << "f" << i << "=" << doc.m_unknown[i] << ",";
    }
    return o;
  }

  std::vector<IMWAWEntry> m_zones; // the text zones
  int m_numChar; // the number of char in text zone
  int m_numTextZone; // the number of text zone ( ie. number of page ? )
  int m_numParagInfo; // the number of paragraph info
  int m_numFont; // the number of font
  int m_unknown[2];

  bool m_parsed;
  std::map<long, MWAWStruct::Font> m_fontMap;
  std::map<long, ParagraphInfo> m_styleMap;
  std::map<long, Token> m_tokenMap;
  std::vector<TextZoneInfo> m_textZoneList;

};
////////////////////////////////////////
//! Internal: the state of a CWText
struct State {
  //! constructor
  State() : m_version(-1), m_font(-1, 0, 0), m_rulersList(), m_fontsList(),
    m_stylesList(), m_lookupMap(), m_zoneMap() {
  }

  //! return the ruler corresponding to a styleId
  int getRulerId(int styleId) const {
    if (m_version <= 2)
      return styleId;
    if (m_lookupMap.find(styleId) == m_lookupMap.end())
      return -1;
    int id = m_lookupMap.find(styleId)->second;
    if (id < 0 || int(m_stylesList.size()) <= id)
      return -1;
    return m_stylesList[id].m_rulerId;
  }

  mutable int m_version;
  MWAWStruct::Font m_font; // the actual font
  std::vector<Ruler> m_rulersList;
  std::vector<MWAWStruct::Font> m_fontsList; // used in style
  std::vector<Style> m_stylesList;
  std::map<int, int> m_lookupMap;
  std::map<int, shared_ptr<Zone> > m_zoneMap;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
CWText::CWText
(TMWAWInputStreamPtr ip, CWParser &parser, MWAWTools::ConvertissorPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert), m_state(new CWTextInternal::State),
  m_mainParser(&parser), m_asciiFile(parser.ascii())
{
}

CWText::~CWText()
{ }

int CWText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_mainParser->version();
  return m_state->m_version;
}

int CWText::numPages() const
{
  std::map<int, shared_ptr<CWTextInternal::Zone> >::iterator iter
    = m_state->m_zoneMap.find(1);
  if (iter == m_state->m_zoneMap.end())
    return 1;
  int numPage = 1;
  long pos = m_input->tell();
  bool lastSoftBreak = false;
  for (int i = 0; i < int(iter->second->m_zones.size()); i++) {
    IMWAWEntry const &entry = iter->second->m_zones[i];
    m_input->seek(entry.begin()+4, WPX_SEEK_SET);
    int numC = entry.length()-4;
    for (int ch = 0; ch < numC; ch++) {
      char c = m_input->readULong(1);
      if (c==0xb)
        numPage++;
      else if (c==0x1 && !lastSoftBreak)
        numPage++;
      lastSoftBreak = ( c == 0xb);
    }
  }
  m_input->seek(pos, WPX_SEEK_SET);
  return numPage;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// a document part
////////////////////////////////////////////////////////////
shared_ptr<CWStruct::DSET> CWText::readDSETZone(CWStruct::DSET const &zone, IMWAWEntry const &entry, bool &complete)
{
  complete = false;
  if (!entry.valid() || zone.m_type != 1)
    return shared_ptr<CWStruct::DSET>();
  long pos = entry.begin();
  m_input->seek(pos+8+16, WPX_SEEK_SET); // avoid header+8 generic number
  libmwaw_tools::DebugStream f;
  shared_ptr<CWTextInternal::Zone> textZone(new CWTextInternal::Zone(zone));

  for (int i = 0; i < 2; i++) // N0 alway 0
    textZone->m_unknown[i] = m_input->readULong(2);
  textZone->m_numChar = m_input->readULong(4);
  textZone->m_numTextZone = m_input->readULong(2);
  textZone->m_numParagInfo = m_input->readULong(2);
  textZone->m_numFont = m_input->readULong(2);
  f << "Entries(DSETT):" << *textZone << ",";

  if (long(m_input->tell())%2)
    m_input->seek(1, WPX_SEEK_CUR);
  ascii().addDelimiter(m_input->tell(), '|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  // read the last part
  int data0Length = 0;
  switch(version()) {
  case 1:
    data0Length = 24;
    break;
  case 2:
    data0Length = 28;
    break;
    // case 3: ???
  case 4:
    data0Length = 30;
    break;
  case 5:
    data0Length = 30/* checkme*/;
    break;
  case 6:
    data0Length = 30;
    break;
  default:
    break;
  }

  int N = zone.m_numData;
  if (long(m_input->tell())+N*data0Length > entry.end()) {
    MWAW_DEBUG_MSG(("CWText::readDSETZone: file is too short\n"));
    return shared_ptr<CWStruct::DSET>();
  }

  int val;
  m_input->seek(entry.end()-N*data0Length, WPX_SEEK_SET);
  if (data0Length) {
    for (int i = 0; i < N; i++) {
      /* fixme: definition of a list of text zone
         ( one by column and one by page ) must be
         used by sendText. */
      pos = m_input->tell();
      f.str("");
      f << "DSETT-" << i << ":";
      CWStruct::DSET::Child child;
      child.m_posC = m_input->readULong(4);
      child.m_type = CWStruct::DSET::Child::TEXT;
      int dim[2];
      for (int j = 0; j < 2; j++)
        dim[j] = m_input->readLong(2);
      child.m_box = Box2i(Vec2i(0,0), Vec2i(dim[0], dim[1]));
      textZone->m_childs.push_back(child);

      f << child;
      f << "ptr=" << std::hex << m_input->readULong(4) << std::dec << ",";
      f << "f0=" << m_input->readLong(2) << ","; // a small number ?
      f << "y[real]=" << m_input->readLong(2) << ",";
      for (int j = 1; j < 4; j++) {
        val = m_input->readLong(2);
        if (val)
          f << "f" << j << "=" << val << ",";
      }
      int what = m_input->readLong(2);
      // 0: main text ?, 1 : header/footnote ?, 2: footer
      if (what)
        f << "what=" << what << ",";

      long actPos = m_input->tell();
      if (actPos != pos && actPos != pos+data0Length)
        ascii().addDelimiter(m_input->tell(),'|');
      m_input->seek(pos+data0Length, WPX_SEEK_SET);

      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
    }
  }

  m_input->seek(entry.end(), WPX_SEEK_SET);

  // now normally three zones: paragraph, font, ???
  for (int z = 0; z < 4+textZone->m_numTextZone; z++) {
    pos = m_input->tell();
    long sz = m_input->readULong(4);
    if (!sz) {
      f.str("");
      f << "DSETT-Z" << z;
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      continue;
    }

    IMWAWEntry zEntry;
    zEntry.setBegin(pos);
    zEntry.setLength(sz+4);

    m_input->seek(zEntry.end(), WPX_SEEK_SET);
    if (long(m_input->tell()) !=  zEntry.end()) {
      MWAW_DEBUG_MSG(("CWParser::readDSET: entry for %d zone is too short\n", z));
      return shared_ptr<CWStruct::DSET>();
    }

    bool ok = true;
    switch(z) {
    case 0:
      ok = readParagraphs(zEntry, *textZone);
      break;
    case 1:
      ok = readFonts(zEntry, *textZone);
      break;
    case 2:
      ok = readTokens(zEntry, *textZone);
      break;
    case 3:
      ok = readTextZoneSize(zEntry, *textZone);
      break;
    default:
      textZone->m_zones.push_back(zEntry);
      break;
    }
    if (!ok) {
      if (z >= 4) {
        m_input->seek(pos, WPX_SEEK_SET);
        MWAW_DEBUG_MSG(("CWParser::readDSET: can not find text %d zone\n", z-4));
        if (z > 4)
          return textZone;
        return shared_ptr<CWStruct::DSET>();
      }
      f.str("");
      f << "DSETT-Z" << z << "#";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
    }
    m_input->seek(zEntry.end(), WPX_SEEK_SET);
  }

  std::map<long, CWTextInternal::Token>::const_iterator tIter
    = textZone->m_tokenMap.begin();
  for ( ; tIter != textZone->m_tokenMap.end(); tIter++) {
    CWTextInternal::Token const &token = tIter->second;
    if (token.m_zoneId > 0)
      textZone->m_otherChilds.push_back(token.m_zoneId);
  }

  if (m_state->m_zoneMap.find(textZone->m_id) != m_state->m_zoneMap.end()) {
    MWAW_DEBUG_MSG(("CWParser::readDSET: zone %d already exists!!!\n", textZone->m_id));
  } else
    m_state->m_zoneMap[textZone->m_id] = textZone;

  complete = true;
  return textZone;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

bool CWText::readFont(int id, int &posC, MWAWStruct::Font &font)
{
  long pos = m_input->tell();

  int fontSize = 0;
  switch(version()) {
  case 1:
  case 2:
  case 3:
    fontSize = 10;
    break;
  case 4:
  case 5:
    fontSize = 12;
    break;
  case 6:
    fontSize = 18;
    break;
  default:
    break;
  }
  if (fontSize == 0)
    return false;

  m_input->seek(fontSize, WPX_SEEK_CUR);
  if (long(m_input->tell()) != pos+fontSize) {
    MWAW_DEBUG_MSG(("CWText::readFont: file is too short"));
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  m_input->seek(pos, WPX_SEEK_SET);
  posC = m_input->readULong(4);
  font=MWAWStruct::Font();
  libmwaw_tools::DebugStream f;
  if (id >= 0)
    f << "Font-" << id << ":";
  else
    f << "Font:";

  f << "pos=" << posC << ",";
  font.setId(m_input->readULong(2));
  int flag =m_input->readULong(2), flags=0;
  if (flag&0x1) flags |= DMWAW_BOLD_BIT;
  if (flag&0x2) flags |= DMWAW_ITALICS_BIT;
  if (flag&0x4) flags |= DMWAW_UNDERLINE_BIT;
  if (flag&0x8) flags |= DMWAW_EMBOSS_BIT;
  if (flag&0x10) flags |= DMWAW_SHADOW_BIT;
  /* flags & 0x20: condensed, flags & 0x40: extended */
  if (flag&0x100) flags |= DMWAW_SUPERSCRIPT_BIT;
  if (flag&0x200) flags |= DMWAW_SUBSCRIPT_BIT;
  font.setSize(m_input->readLong(1));

  int colId = m_input->readULong(1);
  int color[3] = { 0, 0, 0};
  if (colId!=1) {
    Vec3uc col;
    if (m_mainParser->getColor(colId, col)) {
      for (int i = 0; i < 3; i++) color[i] = col[i];
    } else if (version() != 1) {
      MWAW_DEBUG_MSG(("CWText::readFont: unknown color %d\n", colId));
    }
    /*V1:
      color  = 1 black, 26 : yellow, 2c: magenta 24 red 29 cyan
      27 green 2a blue 0 white
    */
  }
  if (fontSize >= 12)
    f << "lookupId=" << m_input->readLong(2) << ",";
  if (fontSize >= 14) {
    flag = m_input->readULong(2);
    if (flag & 0x2)
      flags |= DMWAW_DOUBLE_UNDERLINE_BIT;
    if (flag & 0x20)
      flags |= DMWAW_STRIKEOUT_BIT;
    flag &= 0xFFDD;
    if (flag)
      f << "#flag2=" << std::hex << flag << std::dec << ",";
  }
  font.setFlags(flags);
  font.setColor(color);
  f << m_convertissor->getFontDebugString(font);
  if (long(m_input->tell()) != pos+fontSize)
    ascii().addDelimiter(m_input->tell(), '|');
  m_input->seek(pos+fontSize, WPX_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

bool CWText::readChar(int id, int fontSize, MWAWStruct::Font &font)
{
  long pos = m_input->tell();

  m_input->seek(pos, WPX_SEEK_SET);
  font=MWAWStruct::Font();
  libmwaw_tools::DebugStream f;
  if (id == 0)
    f << "Entries(CHAR)-0:";
  else
    f << "CHAR-" << id << ":";

  int val = m_input->readLong(2);
  if (val != -1) f << "f0=" << val << ",";
  f << "flags=[";
  for (int i = 0; i < 6; i++) {
    val  = m_input->readLong(2);
    if (val) {
      if (i == 3)
        f << "f" << i << "=" << std::hex << val << std::dec << ",";
      else
        f << "f" << i << "=" << val << ",";
    }
  }
  font.setId(m_input->readULong(2));
  int flag =m_input->readULong(2), flags=0;
  if (flag&0x1) flags |= DMWAW_BOLD_BIT;
  if (flag&0x2) flags |= DMWAW_ITALICS_BIT;
  if (flag&0x4) flags |= DMWAW_UNDERLINE_BIT;
  if (flag&0x8) flags |= DMWAW_EMBOSS_BIT;
  if (flag&0x10) flags |= DMWAW_SHADOW_BIT;
  /* flags & 0x20: condensed, flags & 0x40: extended */
  if (flag&0x100) flags |= DMWAW_SUPERSCRIPT_BIT;
  if (flag&0x200) flags |= DMWAW_SUBSCRIPT_BIT;
  font.setSize(m_input->readLong(1));

  int colId = m_input->readULong(1);
  int color[3] = { 0, 0, 0};
  if (colId!=1) {
    f << "#col=" << std::hex << colId << std::dec << ",";
  }
  font.setColor(color);
  if (fontSize >= 12 && version()==6) {
    flag = m_input->readULong(2);
    if (flag & 0x2)
      flags |= DMWAW_DOUBLE_UNDERLINE_BIT;
    if (flag & 0x20)
      flags |= DMWAW_STRIKEOUT_BIT;
    flag &= 0xFFDD;
    if (flag)
      f << "#flag2=" << std::hex << flag << std::dec << ",";
  }
  font.setFlags(flags);
  f << m_convertissor->getFontDebugString(font);
  if (long(m_input->tell()) != pos+fontSize)
    ascii().addDelimiter(m_input->tell(), '|');
  m_input->seek(pos+fontSize, WPX_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

void CWText::setProperty(MWAWStruct::Font const &font, bool force)
{
  if (!m_listener) return;
  font.sendTo(m_listener.get(), m_convertissor, m_state->m_font, force);
}

////////////////////////////////////////////////////////////
// the fonts properties
////////////////////////////////////////////////////////////
bool CWText::readFonts(IMWAWEntry const &entry, CWTextInternal::Zone &zone)
{
  long pos = entry.begin();

  int fontSize = 0;
  switch(version()) {
  case 1:
  case 2:
  case 3:
    fontSize = 10;
    break;
  case 4:
  case 5:
    fontSize = 12;
    break;
  case 6:
    fontSize = 18;
    break;
  default:
    break;
  }
  if (fontSize == 0)
    return false;
  if ((entry.length()%fontSize) != 4)
    return false;

  int numElt = (entry.length()-4)/fontSize;
  int actC = -1;

  m_input->seek(pos+4, WPX_SEEK_SET); // skip header
  // first check char pos is ok
  for (int i = 0; i < numElt; i++) {
    pos = m_input->tell();
    long newC = m_input->readULong(4);
    if (newC < actC) return false;
    actC = newC;
    bool ok = m_input->readULong(1)==0;
    m_input->seek(3, WPX_SEEK_CUR);
    // FIXME: remove this when we have another way to find font
    if (!ok && m_input->readULong(1) > 32)
      return false;
    m_input->seek(pos+fontSize, WPX_SEEK_SET);
  }

  pos = entry.begin();
  ascii().addPos(pos);
  ascii().addNote("Entries(Font)");

  m_input->seek(pos+4, WPX_SEEK_SET); // skip header
  for (int i = 0; i < numElt; i++) {
    MWAWStruct::Font font;
    int posChar;
    if (!readFont(i, posChar, font)) return false;
    if (zone.m_fontMap.find(posChar) != zone.m_fontMap.end()) {
      MWAW_DEBUG_MSG(("CWText::readFonts: entry %d already exists\n", posChar));
    }
    zone.m_fontMap[posChar] = font;
  }

  return true;
}

////////////////////////////////////////////////////////////
// the paragraphs properties
////////////////////////////////////////////////////////////
bool CWText::readParagraphs(IMWAWEntry const &entry, CWTextInternal::Zone &zone)
{
  long pos = entry.begin();

  int styleSize = 0;
  switch(version()) {
  case 1:
    styleSize = 6;
    break;
  default:
    styleSize = 8;
    break;
  }
  if (styleSize == 0)
    return false;
  if ((entry.length()%styleSize) != 4)
    return false;

  int numElt = (entry.length()-4)/styleSize;
  int actC = -1;

  m_input->seek(pos+4, WPX_SEEK_SET); // skip header
  // first check char pos is ok
  for (int i = 0; i < numElt; i++) {
    pos = m_input->tell();
    long newC = m_input->readULong(4);
    if (newC < actC) return false;
    actC = newC;
    m_input->seek(pos+styleSize, WPX_SEEK_SET);
  }

  pos = entry.begin();
  ascii().addPos(pos);
  ascii().addNote("Entries(Paragraph)");

  libmwaw_tools::DebugStream f;
  int numRulers = m_state->m_rulersList.size();
  m_input->seek(pos+4, WPX_SEEK_SET); // skip header
  for (int i = 0; i < numElt; i++) {
    pos = m_input->tell();
    CWTextInternal::ParagraphInfo info;

    int posC = m_input->readULong(4);
    f.str("");
    f << "Paragraph-" << i << ": pos=" << posC << ",";
    info.m_styleId = m_input->readLong(2);
    if (styleSize >= 8)
      info.m_unknown = m_input->readLong(2);
    f << info;
    int rulerId = m_state->getRulerId(info.m_styleId);
    if (rulerId >= 0 && rulerId < numRulers)
      f << "ruler"<< rulerId << "[" << m_state->m_rulersList[rulerId] << "]";
    if (long(m_input->tell()) != pos+styleSize)
      ascii().addDelimiter(m_input->tell(), '|');

    if (zone.m_styleMap.find(posC) != zone.m_styleMap.end()) {
      MWAW_DEBUG_MSG(("CWText::readParagraphs: entry %d already exists\n", posC));
    }
    zone.m_styleMap[posC] = info;
    m_input->seek(pos+styleSize, WPX_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  return true;
}

////////////////////////////////////////////////////////////
// zone which corresponds to the token
////////////////////////////////////////////////////////////
bool CWText::readTokens(IMWAWEntry const &entry, CWTextInternal::Zone &zone)
{
  long pos = entry.begin();

  int dataSize = 0;
  switch(version()) {
  case 1:
  case 2:
  case 3:
  case 4:
    dataSize = 32;
    break;
    // case 5: ?
  case 6:
    dataSize = 36;
    break;
  default:
    break;
  }
  if (dataSize && (entry.length()%dataSize) != 4)
    return false;

  ascii().addPos(pos);
  ascii().addNote("Entries(Token)");
  if (dataSize == 0) {
    m_input->seek(entry.end(), WPX_SEEK_SET);
    return true;
  }

  int numElt = (entry.length()-4)/dataSize;
  m_input->seek(pos+4, WPX_SEEK_SET); // skip header

  libmwaw_tools::DebugStream f;
  for (int i = 0; i < numElt; i++) {
    pos = m_input->tell();

    int posC = m_input->readULong(4);
    CWTextInternal::Token token;

    int type = m_input->readLong(2);
    f.str("");
    switch(type) {
    case 0:
      token.m_type = CWTextInternal::TKN_FOOTNOTE;
      break;
    case 1:
      token.m_type = CWTextInternal::TKN_GRAPHIC;
      break;
    case 2:
      /* find in v4-v6, does not seem to exist in v1-v2 */
      token.m_type = CWTextInternal::TKN_PAGENUMBER;
      break;
    default:
      f << "#type=" << type << ",";
      break;
    }

    token.m_unknown[0] =  m_input->readLong(2);
    token.m_zoneId = m_input->readLong(2);
    token.m_unknown[1] =  m_input->readLong(1);
    token.m_page = m_input->readLong(1);
    token.m_unknown[2] =  m_input->readLong(2);
    for (int j = 0; j < 2; j++)
      token.m_size[j] =  m_input->readLong(2);

    token.m_error = f.str();
    f.str("");
    f << "Token-" << i << ": pos=" << posC << "," << token;
    if (zone.m_tokenMap.find(posC) != zone.m_tokenMap.end()) {
      MWAW_DEBUG_MSG(("CWText::readTokens: entry %d already exist.\n", posC));
    }
    zone.m_tokenMap[posC] = token;

    if (long(m_input->tell()) != pos && long(m_input->tell()) != pos+dataSize)
      ascii().addDelimiter(m_input->tell(), '|');
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+dataSize, WPX_SEEK_SET);
  }

  m_input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////
bool CWText::readTextZoneSize(IMWAWEntry const &entry, CWTextInternal::Zone &zone)
{
  long pos = entry.begin();

  int dataSize = 10;
  if ((entry.length()%dataSize) != 4)
    return false;

  ascii().addPos(pos);
  ascii().addNote("Entries(TextZoneSz)");

  int numElt = (entry.length()-4)/dataSize;

  m_input->seek(pos+4, WPX_SEEK_SET); // skip header

  libmwaw_tools::DebugStream f;

  for (int i = 0; i < numElt; i++) {
    pos = m_input->tell();
    f.str("");
    f << "TextZoneSz-" << i << ":";
    CWTextInternal::TextZoneInfo info;
    info.m_pos = m_input->readULong(4);
    info.m_N =  m_input->readULong(2);
    f << info;
    zone.m_textZoneList.push_back(info);
    if (long(m_input->tell()) != pos+dataSize)
      ascii().addDelimiter(m_input->tell(), '|');
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+dataSize, WPX_SEEK_SET);
  }

  m_input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

bool CWText::sendText(CWTextInternal::Zone const &zone)
{
  long actC = 0;
  bool firstFontSend = false;
  int numRulers = m_state->m_rulersList.size();
  MWAWStruct::Font actFont;
  int actPage = 1, numZones = zone.m_zones.size();
  if (zone.m_id == 1 && m_listener)
    m_mainParser->newPage(actPage);
  bool lastSoftBreak = false;
  for (int z = 0; z < numZones; z++) {
    IMWAWEntry const &entry  =  zone.m_zones[z];
    long pos = entry.begin();
    libmwaw_tools::DebugStream f;
    f << "Entries(TextContent):";

    int numC = (entry.length()-4);

    m_input->seek(pos+4, WPX_SEEK_SET); // skip header

    std::string text("");
    for (int i = 0; i < numC; i++) {
      char c = m_input->readULong(1);
      if (c == '\0') {
        if (i == numC-1) break;
        return false;
      }

      text += c;
      if (!m_listener) continue;
      if (zone.m_fontMap.find(actC) != zone.m_fontMap.end()) {
        actFont = zone.m_fontMap.find(actC)->second;
        setProperty(actFont, !firstFontSend);
        firstFontSend = true;
      }
      // we must change the paragraph property before seen the '\n' character
      if (zone.m_styleMap.find(actC) != zone.m_styleMap.end()) {
        CWTextInternal::ParagraphInfo const &parag =
          zone.m_styleMap.find(actC)->second;
        int rulerId = m_state->getRulerId(parag.m_styleId);
        if (rulerId >= 0 && rulerId < numRulers)
          setProperty(m_state->m_rulersList[rulerId]);
      }
      if (zone.m_tokenMap.find(actC) != zone.m_tokenMap.end()) {
        CWTextInternal::Token const &token =
          zone.m_tokenMap.find(actC)->second;
        switch(token.m_type) {
        case CWTextInternal::TKN_FOOTNOTE:
          m_mainParser->sendFootnote(token.m_zoneId);
          m_state->m_font = actFont;
          break;
        case CWTextInternal::TKN_PAGENUMBER:
          m_listener->insertField(IMWAWContentListener::PageNumber);
          break;
        default: // fixme
          break;
        }
        if (c < 30) {
          actC++;
          continue;
        }
      }

      actC++;
      switch (c) {
      case 0xb: // soft page break ?
      case 0x1: // page break
        if (zone.m_id == 1 && !lastSoftBreak)
          m_mainParser->newPage(++actPage);
        break;
      case 0x2: // footnote
        break;
      case 0x4:
        m_listener->insertField(IMWAWContentListener::Date);
        break;
      case 0x5:
        m_listener->insertField(IMWAWContentListener::Time);
        break;
      case 0x6:
        m_listener->insertField(IMWAWContentListener::PageNumber);
        break;
      case 0x7:
        // footnote index
        break;
      case 0x8:
        // potential breaking <<hyphen>>
        break;
      case 0x9:
        m_listener->insertTab();
        break;
      case 0xd:
        // ignore last end of line returns
        if (z != numZones-1 || i != numC-2)
          m_listener->insertEOL();
        break;

      default: {
        int unicode = m_convertissor->getUnicode (actFont,c);
        if (unicode == -1) {
          if (c < 30) {
            MWAW_DEBUG_MSG(("CWText::sendText: Find odd char %x\n", int(c)));
            text+="#";
          } else
            m_listener->insertCharacter(c);
        } else
          m_listener->insertUnicode(unicode);
      }
      }
      lastSoftBreak = (c == 0xb);
    }
    f << "'" << text << "'";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  return true;
}

////////////////////////////////////////////////////////////
// the style definition?
////////////////////////////////////////////////////////////
bool CWText::readSTYL_LKUP(int N, int fSz)
{
  if (fSz == 0 || N== 0) return true;
  libmwaw_tools::DebugStream f;
  for (int i = 0; i < N; i++) {
    long pos = m_input->tell();
    f.str("");
    if (i == 0) f << "Entries(LKUP_STYL): LKUP_STYL-0:";
    else f << "LKUP_STYL-" << i << ":";
    int val = m_input->readLong(2);
    m_state->m_lookupMap[i] = val;
    f << "styleId=" << val;
    if (fSz != 2) {
      ascii().addDelimiter(m_input->tell(), '|');
      m_input->seek(pos+fSz, WPX_SEEK_SET);
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool CWText::readSTYL_CHAR(int N, int fSz)
{
  if (fSz == 0 || N== 0) return true;
  libmwaw_tools::DebugStream f;
  if (m_state->m_fontsList.size()) {
    MWAW_DEBUG_MSG(("CWText::readSTYL_CHAR: font list already exists!!!\n"));
  }
  m_state->m_fontsList.resize(N);
  for (int i = 0; i < N; i++) {
    long pos = m_input->tell();
    MWAWStruct::Font font;
    if (readChar(i, fSz, font))
      m_state->m_fontsList[i] = font;
    else {
      f.str("");
      if (!i)
        f << "Entries(Font)-0:";
      else
        f << "Font-" << i << ":#";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
    }
    m_input->seek(pos+fSz, WPX_SEEK_SET);
  }
  return true;
}

bool CWText::readSTYL_RULR(int N, int fSz)
{
  if (fSz == 0 || N== 0) return true;
  if (fSz != 108) {
    MWAW_DEBUG_MSG(("CWText::readSTYL_RULR: Find old ruler size %d\n", fSz));
  }
  libmwaw_tools::DebugStream f;
  for (int i = 0; i < N; i++) {
    long pos = m_input->tell();
    if (fSz != 108 || !readRuler(i)) {
      f.str("");
      if (!i)
        f << "Entries(RULR)-0:";
      else
        f << "RULR-" << i << ":#";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
    }
    m_input->seek(pos+fSz, WPX_SEEK_SET);
  }
  return true;
}

bool CWText::readSTYL_NAME(int N, int fSz)
{
  if (fSz == 0 || N== 0) return true;
  libmwaw_tools::DebugStream f;
  for (int i = 0; i < N; i++) {
    long pos = m_input->tell();
    f.str("");
    if (i == 0) f << "Entries(NAME_STYL): NAME_STYL-0:";
    else f << "NAME_STYL-" << i << ":";
    f << "id=" << m_input->readLong(2) << ",";
    if (fSz > 4) {
      int nChar = m_input->readULong(1);
      if (3+nChar > fSz) {
        static bool first = true;
        if (first) {
          MWAW_DEBUG_MSG(("CWText::readSTYL_NAME: pb with name field %d", i));
          first = false;
        }
        f << "#";
      } else {
        std::string name("");
        for (int c = 0; c < nChar; c++)
          name += char(m_input->readULong(1));
        f << "'" << name << "'";
      }
    }
    if (long(m_input->tell()) != pos+fSz) {
      ascii().addDelimiter(m_input->tell(), '|');
      m_input->seek(pos+fSz, WPX_SEEK_SET);
    }

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool CWText::readSTYL_FNTM(int N, int fSz)
{
  if (fSz == 0 || N== 0) return true;
  if (fSz < 16) return false;

  libmwaw_tools::DebugStream f;
  for (int i = 0; i < N; i++) {
    long pos = m_input->tell();
    f.str("");
    if (i == 0) f << "Entries(FNTM_STYL): FNTM_STYL-0:";
    else f << "FNTM_STYL-" << i << ":";
    f << "unkn=" << m_input->readLong(2) << ",";
    f << "type?=" << m_input->readLong(2) << ",";

    int nChar = m_input->readULong(1);
    if (5+nChar > fSz) {
      static bool first = true;
      if (first) {
        MWAW_DEBUG_MSG(("CWText::readSTYL_FNTM: pb with name field %d", i));
        first = false;
      }
      f << "#";
    } else {
      std::string name("");
      bool ok = true;
      for (int c = 0; c < nChar; c++) {
        char ch = m_input->readULong(1);
        if (ch == '\0') {
          MWAW_DEBUG_MSG(("CWText::readSTYL_FNTM: pb with name field %d\n", i));
          ok = false;
          break;
        } else if (ch & 0x80) {
          static bool first = true;
          if (first) {
            MWAW_DEBUG_MSG(("CWText::readSTYL_FNTM: find odd font\n"));
            first = false;
          }
          ok = false;
        }
        name += ch;
      }
      f << "'" << name << "'";
      if (name.length() && ok) {
        m_convertissor->setFontCorrespondance(i, name);
      }
    }
    if (long(m_input->tell()) != pos+fSz) {
      ascii().addDelimiter(m_input->tell(), '|');
      m_input->seek(pos+fSz, WPX_SEEK_SET);
    }

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool CWText::readSTYL_STYL(int N, int fSz)
{
  if (fSz == 0 || N== 0) return true;
  if (fSz < 28) {
    MWAW_DEBUG_MSG(("CWText::readSTYL_STYL: Find old ruler size %d\n", fSz));
    return false;
  }
  libmwaw_tools::DebugStream f;
  int val;
  for (int i = 0; i < N; i++) {
    long pos = m_input->tell();
    CWTextInternal::Style style;
    f.str("");
    if (!i)
      f << "Entries(Style)-0:";
    else
      f << "Style-" << i << ":";
    val = m_input->readLong(2);
    if (val != -1) f << "f0=" << val << ",";
    val = m_input->readLong(2);
    if (val) f << "f1=" << val << ",";
    f << "used?=" << m_input->readLong(2) << ",";
    int styleId = m_input->readLong(2);
    if (i != styleId && styleId != -1) f << "#";
    f << "styleId=" << styleId << ",";
    int lookupId = m_input->readLong(2);
    f << "lookupId=" << lookupId << ",";
    for (int i = 0; i < 2; i++) {
      // unknown : hash, dataId ?
      f << "g" << i << "=" << m_input->readLong(1) << ",";
    }
    for (int i = 2; i < 4; i++) {
      // unknown : hash, dataId ?
      f << "g" << i << "=" << m_input->readLong(2) << ",";
    }
    int lookupId2 = m_input->readLong(2);
    f << "lookupId2=" << lookupId2 << ",";
    f << "char=[";
    style.m_fontId = m_input->readLong(2);
    f << "id=" << style.m_fontId << ",";
    f << "hash=" << m_input->readLong(2) << ",";
    f << "],";
    f << "graphId?=" << m_input->readLong(2);
    f << "ruler=[";
    style.m_rulerId = m_input->readLong(2);
    f << "id=" << style.m_rulerId << ",";
    f << "hash=" << m_input->readLong(2) << ",";
    if (fSz >= 30)
      f << "unkn=" << m_input->readLong(2);
    f << "],";
    m_state->m_stylesList.push_back(style);
    if (long(m_input->tell()) != pos+fSz)
      ascii().addDelimiter(m_input->tell(), '|');
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+fSz, WPX_SEEK_SET);
  }
  return true;
}

bool CWText::readSTYL(int id)
{
  long pos = m_input->tell();
  long sz = m_input->readULong(4);
  long endPos = pos+4+sz;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("CWText::readSTYL: pb with sub zone: %d", id));
    return false;
  }
  m_input->seek(pos+4, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "STYL-" << id << ":";
  if (sz < 16) {
    if (sz) f << "#";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(endPos, WPX_SEEK_SET);
    return true;
  }

  std::string name("");
  int N = m_input->readLong(2);
  int type = m_input->readLong(2);
  int val =  m_input->readLong(2);
  int fSz =  m_input->readLong(2);
  f << "N=" << N << ", type?=" << type <<", fSz=" << fSz << ",";
  if (val) f << "unkn=" << val << ",";

  for (int i = 0; i < 2; i++) {
    int val = m_input->readLong(2);
    if (val)  f << "f" << i << "=" << val << ",";
  }
  for (int i = 0; i < 4; i++)
    name += char(m_input->readULong(1));
  f << name;

  long actPos = m_input->tell();
  if (actPos != pos && actPos != endPos - N*fSz)
    ascii().addDelimiter(m_input->tell(), '|');

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  long numRemain = endPos - actPos;
  if (N > 0 && fSz > 0 && numRemain >= N*fSz) {
    m_input->seek(endPos-N*fSz, WPX_SEEK_SET);

    bool ok = false;
    if (name == "LKUP")
      ok = readSTYL_LKUP(N, fSz);
    else if (name == "NAME")
      ok = readSTYL_NAME(N, fSz);
    else if (name == "FNTM")
      ok = readSTYL_FNTM(N, fSz);
    else if (name == "RULR")
      ok = readSTYL_RULR(N, fSz);
    else if (name == "CHAR")
      ok = readSTYL_CHAR(N, fSz);
    else if (name == "STYL")
      ok = readSTYL_STYL(N, fSz);

    if (!ok) {
      m_input->seek(endPos-N*fSz, WPX_SEEK_SET);
      for (int i = 0; i < N; i++) {
        pos = m_input->tell();
        f.str("");
        f << "STYL-" << id << "/" << name << "-" << i << ":";
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());
        m_input->seek(fSz, WPX_SEEK_CUR);
      }
    }
  }

  m_input->seek(endPos, WPX_SEEK_SET);
  return true;
}

bool CWText::readSTYLs(IMWAWEntry const &entry)
{
  if (!entry.valid() || entry.type() != "STYL")
    return false;
  long pos = entry.begin();
  m_input->seek(pos+4, WPX_SEEK_SET); // skip header
  long sz = m_input->readULong(4);
  if (sz > entry.length()) {
    MWAW_DEBUG_MSG(("CWText::readSTYLs: pb with entry length"));
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  libmwaw_tools::DebugStream f;
  f << "Entries(STYL):";
  if (version() <= 3) {
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(entry.end(), WPX_SEEK_SET);
    return true;
  }
  bool limitSet = true;
  if (version() <= 4) {
    // version 4 does not contents total length fields
    m_input->seek(-4, WPX_SEEK_CUR);
    limitSet = false;
  } else
    m_input->pushLimit(entry.end());
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  int id = 0;
  while (long(m_input->tell()) < entry.end()) {
    pos = m_input->tell();
    if (!readSTYL(id)) {
      m_input->seek(pos, WPX_SEEK_SET);
      if (limitSet) m_input->popLimit();
      return false;
    }
    id++;
  }
  if (limitSet) m_input->popLimit();

  return true;
}

////////////////////////////////////////////////////////////
// read a list of rulers
////////////////////////////////////////////////////////////
bool CWText::readRulers()
{
  long pos = m_input->tell();
  long sz = m_input->readULong(4);
  long endPos = pos+4+sz;

  m_input->seek(endPos, WPX_SEEK_SET);
  if (m_input->atEOS()) {
    MWAW_DEBUG_MSG(("CWText::readRulers: ruler zone is too short\n"));
    return false;
  }
  m_input->seek(pos+4, WPX_SEEK_SET);

  int N = m_input->readULong(2);
  int type = m_input->readLong(2);
  int val =  m_input->readLong(2);
  int fSz =  m_input->readLong(2);

  if (sz != 12+fSz*N) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWText::readRulers: find odd ruler size\n"));
    return false;
  }

  libmwaw_tools::DebugStream f;
  f << "Entries(RULR):";
  f << "N=" << N << ", type?=" << type <<", fSz=" << fSz << ",";
  if (val) f << "unkn=" << val << ",";

  for (int i = 0; i < 2; i++) {
    int val = m_input->readLong(2);
    if (val)  f << "f" << i << "=" << val << ",";
  }

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = m_input->tell();
    if (!readRuler(i)) {
      m_input->seek(pos, WPX_SEEK_SET);
      return false;
    }
  }
  return true;
}

////////////////////////////////////////////////////////////
// read a ruler zone
////////////////////////////////////////////////////////////
bool CWText::readRuler(int id)
{
  int dataSize = 0;
  switch (version()) {
  case 1:
    dataSize = 92;
    break;
  case 2:
  case 3:
    dataSize = 96;
    break;
  case 4:
  case 5:
  case 6:
    if (id >= 0) dataSize = 108;
    else dataSize = 96;
    break;
  default:
    MWAW_DEBUG_MSG(("CWText::readRuler: unknown size\n"));
    return false;
  }

  CWTextInternal::Ruler ruler;
  long pos = m_input->tell();
  long endPos = pos+dataSize;
  libmwaw_tools::DebugStream f;

  int val;
  if (version() >= 4 && id >= 0) {
    val = m_input->readLong(2);
    if (val != -1) f << "f0=" << val << ",";
    val = m_input->readLong(4);
    f << "f1=" << val << ",";
    int dim[2];
    for (int i = 0; i < 2; i++)
      dim[i] = m_input->readLong(2);
    f << "dim?=" << dim[0] << "x" << dim[1] << ",";
    val = m_input->readLong(2);
    if (val) f << "unkn=" << val << ",";
  }

  val = m_input->readLong(2);
  f << "num[used]=" << val << ",";
  val = m_input->readULong(2);
  int align = 0;
  switch(version()) {
  case 1:
  case 2:
    align = (val >> 14);
    val &= 0x3FFF;
    break;
  case 4:
  case 5:
  case 6:
    align = (val >> 13) & 3;
    val &= 0x9FFF;
    break;
  default:
    break;
  }
  switch(align) {
  case 0:
    break;
  case 1:
    ruler.m_justify = DMWAW_PARAGRAPH_JUSTIFICATION_CENTER;
    break;
  case 2:
    ruler.m_justify = DMWAW_PARAGRAPH_JUSTIFICATION_RIGHT;
    break;
  case 3:
    ruler.m_justify = DMWAW_PARAGRAPH_JUSTIFICATION_FULL;
    break;
  default:
    break;
  }


  bool inPoint = false;
  int interline = 0;
  switch(version()) {
  case 1:
    inPoint = (val & 0x2000);
    interline = val & 0xFF;
    val &= 0x1F00;
    break;
  case 2:
    interline = val&0xFF;
    if (interline & 0x80) {
      inPoint = true;
      interline &= 0x7F;
      val &= 0x3F00;
    } else {
      interline >>= 3;
      val &= 0x3F07;
    }
    break;
  case 4:
  case 5:
  case 6: {
    interline = (val >> 3);
    bool ok = true;
    switch (val & 7) {
    case 0: // PERCENT
      ok = interline <= 18;
      inPoint = false;
      break;
    case 3: // INCHES but values in POINT
    case 2: // POINT
      ok = interline <= 512;
      inPoint = true;
      break;
    default:
      ok = false;
      break;
    }
    if (ok) val = 0;
    else {
      MWAW_DEBUG_MSG(("CWText::readRuler: can not determine underline\n"));
      interline = 0;
    }
    break;
  }
  default:
    break;
  }
  if (interline) {
    if (inPoint) ruler.m_interlineFixed = interline;
    else ruler.m_interlinePercent = 1.0+interline*0.5;
  }
  if (val) f << "#flags=" << std::hex << val << std::dec << ",";
  for (int i = 0; i < 3; i++)
    ruler.m_margins[i] = m_input->readLong(2)/72.;
  ruler.m_margins[0] +=  ruler.m_margins[1];
  if (version() >= 2) {
    for(int i = 0; i < 2; i++) {
      ruler.m_spacings[i] = m_input->readULong(1);
      m_input->seek(1, WPX_SEEK_CUR); // flags to define the printing unit
    }
  }
  val = m_input->readLong(1);
  if (val) f << "unkn1=" << val << ",";
  int numTabs = m_input->readULong(1);
  if (long(m_input->tell())+numTabs*4 > endPos) {
    if (numTabs != 255) { // 0xFF seems to be used in v1, v2
      MWAW_DEBUG_MSG(("CWText::readRuler: numTabs is too big\n"));
    }
    f << "numTabs*=" << numTabs << ",";
    numTabs = 0;
  }
  for (int i = 0; i < numTabs; i++) {
    DMWAWTabStop tab;
    tab.m_position = m_input->readLong(2)/72.;
    val = m_input->readULong(1);
    int leaderType = 0;
    switch(version()) {
    case 1:
      align = val & 3;
      val &= 0xFC;
      break;
    case 2:
      align = (val >> 6);
      val &= 0x3F;
      break;
    case 4: // checkme
    case 5:
    case 6:
      align = (val >> 5);
      leaderType = (val & 3);
      val &= 0x9C;
      break;
    }
    switch(align&3) {
    case 1:
      tab.m_alignment = CENTER;
      break;
    case 2:
      tab.m_alignment = RIGHT;
      break;
    case 3:
      tab.m_alignment = DECIMAL;
      break;
    case 0: // left
    default:
      break;
    }
    switch(leaderType) {
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
    char decimalChar = m_input->readULong(1);
    if (decimalChar != ',' && decimalChar != '.')
      f << "decimalChar=" << decimalChar << ",";
    ruler.m_tabs.push_back(tab);
    if (val)
      f << "#unkn[tab" << i << "=" << std::hex << val << std::dec << "],";
  }
  ruler.m_error = f.str();
  // save the style
  if (id >= 0) {
    if (int(m_state->m_rulersList.size()) <= id)
      m_state->m_rulersList.resize(id+1);
    m_state->m_rulersList[id]=ruler;
  }
  f.str("");
  if (id == 0)
    f << "Entries(RULR)-0";
  else if (id < 0)
    f << "RULR-_";
  else
    f << "RULR-" << id;
  f << ":" << ruler;

  if (long(m_input->tell()) != pos+dataSize)
    ascii().addDelimiter(m_input->tell(), '|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != pos+dataSize)
    return false;
  return true;
}

void CWText::setProperty(CWTextInternal::Ruler const &ruler)
{
  if (!m_listener) return;

  m_listener->justificationChange(ruler.m_justify);

  double textWidth = m_mainParser->pageWidth();
  m_listener->setParagraphTextIndent(ruler.m_margins[0]);
  m_listener->setParagraphMargin(ruler.m_margins[1], DMWAW_LEFT);
  float rPos = 0;
  if (ruler.m_margins[2] >= 0.0) {
    rPos = ruler.m_margins[2]-28./72.;
    if (rPos < 0) rPos = 0;
  }
  m_listener->setParagraphMargin(rPos, DMWAW_RIGHT);
  m_listener->setParagraphMargin(ruler.m_spacings[0]/72.,DMWAW_TOP);
  m_listener->setParagraphMargin(ruler.m_spacings[1]/72.,DMWAW_BOTTOM);

  if (ruler.m_interlineFixed > 0)
    m_listener->lineSpacingChange(ruler.m_interlineFixed, WPX_POINT);
  else if (ruler.m_interlinePercent > 0.0)
    m_listener->lineSpacingChange(ruler.m_interlinePercent, WPX_PERCENT);
  else
    m_listener->lineSpacingChange(1.0, WPX_PERCENT);

  m_listener->setTabs(ruler.m_tabs,textWidth);
}

bool CWText::sendZone(int number)
{
  std::map<int, shared_ptr<CWTextInternal::Zone> >::iterator iter
    = m_state->m_zoneMap.find(number);
  if (iter == m_state->m_zoneMap.end())
    return false;
  shared_ptr<CWTextInternal::Zone> zone = iter->second;
  sendText(*zone);
  zone->m_parsed = true;
  return true;
}

void CWText::flushExtra()
{
  std::map<int, shared_ptr<CWTextInternal::Zone> >::iterator iter
    = m_state->m_zoneMap.begin();
  for ( ; iter !=  m_state->m_zoneMap.end(); iter++) {
    shared_ptr<CWTextInternal::Zone> zone = iter->second;
    if (zone->m_parsed)
      continue;
    if (m_listener) m_listener->insertEOL();
    sendText(*zone);
    zone->m_parsed = true;
  }
}


// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
