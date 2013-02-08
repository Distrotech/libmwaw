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

#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWParagraph.hxx"

#include "CWParser.hxx"
#include "CWStruct.hxx"
#include "CWStyleManager.hxx"

#include "CWText.hxx"

/** Internal: the structures of a CWText */
namespace CWTextInternal
{
/** the different plc type */
enum PLCType { P_Font,  P_Ruler, P_Child, P_Section, P_TextZone, P_Token, P_Unknown};

/** Internal : the different plc types: mainly for debugging */
struct PLC {
  /// the constructor
  PLC() : m_type(P_Unknown), m_id(-1), m_extra("") {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, PLC const &plc);
  /** the PLC types */
  PLCType m_type;
  /** the id */
  int m_id;
  /** extra data */
  std::string m_extra;
};

std::ostream &operator<<(std::ostream &o, PLC const &plc)
{
  switch(plc.m_type) {
  case P_Font:
    o << "F";
    break;
  case P_Ruler:
    o << "P";
    break;
  case P_Child:
    o << "C";
    break;
  case P_Section:
    o << "S";
    break;
  case P_TextZone:
    o << "TZ";
    break;
  case P_Token:
    o << "Tok";
    break;
  case P_Unknown:
  default:
    o << "#Unkn";
    break;
  }
  if (plc.m_id >= 0) o << plc.m_id;
  else o << "_";
  if (plc.m_extra.length()) o << ":" << plc.m_extra;
  return o;
}
/** Internal: class to store the paragraph properties */
struct Paragraph : public MWAWParagraph {
  //! Constructor
  Paragraph() : MWAWParagraph(), m_labelType(0) {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Paragraph const &ind) {
    o << reinterpret_cast<MWAWParagraph const &>(ind) << ",";
    static char const *(labelNames[]) = {
      "none", "diamond", "bullet", "checkbox", "hardward", "leader", "legal",
      "upperalpha", "alpha", "numeric", "upperroman", "roman"
    };
    if (ind.m_labelType > 0 && ind.m_labelType < 12)
      o << "label=" << labelNames[ind.m_labelType] << ",";
    else if (ind.m_labelType)
      o << "#labelType=" << ind.m_labelType << ",";
    return o;
  }
  //! update the list level
  void updateListLevel();
  //! the label
  int m_labelType;
};

void Paragraph::updateListLevel()
{
  if (m_labelType==0 && (!m_listLevelIndex.isSet() || !m_listLevelIndex.get()))
    return;
  int lev = m_listLevelIndex.get();
  if (m_labelType) lev++;
  m_listLevelIndex = lev;
  MWAWList::Level theLevel;
  switch(m_labelType) {
  case 0:
    theLevel.m_type = MWAWList::Level::NONE;
    break;
  case 1: // diamond
    theLevel.m_type = MWAWList::Level::BULLET;
    MWAWContentListener::appendUnicode(0x25c7, theLevel.m_bullet);
    break;
  case 3: // checkbox
    theLevel.m_type = MWAWList::Level::BULLET;
    MWAWContentListener::appendUnicode(0x2610, theLevel.m_bullet);
    break;
  case 4: {
    theLevel.m_suffix = (lev <= 3) ? "." : ")";
    if (lev == 1) theLevel.m_type = MWAWList::Level::UPPER_ROMAN;
    else if (lev == 2) theLevel.m_type = MWAWList::Level::UPPER_ALPHA;
    else if (lev == 3) theLevel.m_type = MWAWList::Level::DECIMAL;
    else if (lev == 4) theLevel.m_type =  MWAWList::Level::LOWER_ALPHA;
    else if ((lev%3)==2) {
      theLevel.m_prefix = "(";
      theLevel.m_type = MWAWList::Level::DECIMAL;
    } else if ((lev%3)==0) {
      theLevel.m_prefix = "(";
      theLevel.m_type = MWAWList::Level::LOWER_ALPHA;
    } else
      theLevel.m_type = MWAWList::Level::LOWER_ROMAN;
    break;
  }
  case 5: // leader
    theLevel.m_type = MWAWList::Level::BULLET;
    theLevel.m_bullet = "+"; // in fact + + and -
    break;
  case 6: // legal
    theLevel.m_type = MWAWList::Level::DECIMAL;
    theLevel.m_suffix = "."; // fixme in fact 1.2.2.
    break;
  case 7:
    theLevel.m_type = MWAWList::Level::UPPER_ALPHA;
    break;
  case 8:
    theLevel.m_type = MWAWList::Level::LOWER_ALPHA;
    break;
  case 9:
    theLevel.m_type = MWAWList::Level::DECIMAL;
    break;
  case 10:
    theLevel.m_type = MWAWList::Level::UPPER_ROMAN;
    break;
  case 11:
    theLevel.m_type = MWAWList::Level::LOWER_ROMAN;
    break;
  case 2: // bullet
  default:
    theLevel.m_type = MWAWList::Level::BULLET;
    MWAWContentListener::appendUnicode(0x2022, theLevel.m_bullet);
    break;
  }
  theLevel.m_labelIndent = m_margins[1].get()-0.2f;
  m_listLevel=theLevel;
}

struct ParagraphInfo {
  ParagraphInfo() : m_styleId(-1), m_unknown(0), m_extra("") {
  }

  friend std::ostream &operator<<(std::ostream &o, ParagraphInfo const &info) {
    if (info.m_styleId >= 0) o << "style=" << info.m_styleId <<",";
    if (info.m_unknown) o << "unknown=" << info.m_unknown << ",";
    if (info.m_extra.length()) o << info.m_extra;
    return o;
  }

  int m_styleId;
  int m_unknown;
  std::string m_extra;
};

/** internal class used to store a section */
struct Section {
  //! the constructor
  Section() : m_pos(0), m_numColumns(1), m_columnsWidth(), m_columnsSep(), m_extra("") {
  }
  //! operator <<
  friend std::ostream &operator<<(std::ostream &o, Section const &sec) {
    o << "pos=" << sec.m_pos << ",";
    if (sec.m_numColumns != 1) o << "numCols=" << sec.m_numColumns << ",";
    o << "colsW=[";
    for (size_t c = 0; c < sec.m_columnsWidth.size(); c++)
      o << sec.m_columnsWidth[c] << ",";
    o << "],";
    if (sec.m_columnsSep.size()) {
      o << "colsW=[";
      for (size_t c = 0; c < sec.m_columnsSep.size(); c++)
        o << sec.m_columnsSep[c] << ",";
      o << "],";
    }
    if (sec.m_extra.length()) o << sec.m_extra;
    return o;
  }
  /** the character position */
  long m_pos;
  /** the number of column */
  int m_numColumns;
  /** the columns width */
  std::vector<int> m_columnsWidth;
  /** the columns separator */
  std::vector<int> m_columnsSep;
  /** a string to store unparsed data */
  std::string m_extra;
};

/** internal class used to store a text zone */
struct TextZoneInfo {
  TextZoneInfo() : m_pos(0), m_N(0), m_extra("") {
  }

  friend std::ostream &operator<<(std::ostream &o, TextZoneInfo const &info) {
    o << "pos=" << info.m_pos << ",";
    if (info.m_N >= 0) o << "size=" << info.m_N <<",";
    if (info.m_extra.length()) o << info.m_extra;
    return o;
  }
  long m_pos;
  int m_N;
  std::string m_extra;
};

enum TokenType { TKN_UNKNOWN, TKN_FOOTNOTE, TKN_PAGENUMBER, TKN_GRAPHIC };

/** Internal: class to store field definition: TOKN entry*/
struct Token {
  //! constructor
  Token() : m_type(TKN_UNKNOWN), m_zoneId(-1), m_page(-1), m_descent(0), m_extra("") {
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
  //! the descent
  int m_descent;
  //! the unknown zone
  int m_unknown[3];
  //! a string used to store the parsing errors
  std::string m_extra;
};
//! operator<< for Token
std::ostream &operator<<(std::ostream &o, Token const &tok)
{
  switch (tok.m_type) {
  case TKN_FOOTNOTE:
    o << "footnoote,";
    break;
  case TKN_PAGENUMBER:
    switch(tok.m_unknown[0]) {
    case 0:
      o << "field[pageNumber],";
      break;
    case 1:
      o << "field[sectionNumber],";
      break;
    case 2:
      o << "field[sectionInPageNumber],";
      break;
    case 3:
      o << "field[pageCount],";
      break;
    default:
      o << "field[pageNumber=#" << tok.m_unknown[0] << "],";
      break;
    }
    break;
  case TKN_GRAPHIC:
    o << "graphic,";
    break;
  case TKN_UNKNOWN:
  default:
    o << "##field[unknown]" << ",";
    break;
  }
  if (tok.m_zoneId != -1) o << "zoneId=" << tok.m_zoneId << ",";
  if (tok.m_page != -1) o << "page?=" << tok.m_page << ",";
  o << "pos?=" << tok.m_size[0] << "x" << tok.m_size[1] << ",";
  if (tok.m_descent) o << "descent=" << tok.m_descent << ",";
  for (int i = 0; i < 3; i++) {
    if (tok.m_unknown[i] == 0 || (i==0 && tok.m_type==TKN_PAGENUMBER))
      continue;
    o << "#unkn" << i << "=" << std::hex << tok.m_unknown[i] << std::dec << ",";
  }
  if (!tok.m_extra.empty()) o << "err=[" << tok.m_extra << "]";
  return o;
}

struct Zone : public CWStruct::DSET {
  Zone(CWStruct::DSET const dset = CWStruct::DSET()) :
    CWStruct::DSET(dset), m_zones(), m_numChar(0), m_numTextZone(0), m_numParagInfo(0),
    m_numFont(0), m_fatherId(0), m_unknown(0), m_fontList(), m_paragraphList(),
    m_sectionList(), m_tokenList(), m_textZoneList(), m_plcMap() {
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Zone const &doc) {
    o << static_cast<CWStruct::DSET const &>(doc);
    if (doc.m_numChar) o << "numChar=" << doc.m_numChar << ",";
    if (doc.m_numTextZone) o << "numTextZone=" << doc.m_numTextZone << ",";
    if (doc.m_numParagInfo) o << "numParag=" << doc.m_numParagInfo << ",";
    if (doc.m_numFont) o << "numFont=" << doc.m_numFont << ",";
    if (doc.m_fatherId) o << "id[father]=" << doc.m_fatherId << ",";
    if (doc.m_unknown) o << "unkn=" << doc.m_unknown << ",";
    return o;
  }

  std::vector<MWAWEntry> m_zones; // the text zones
  int m_numChar /** the number of char in text zone */;
  int m_numTextZone /** the number of text zone ( ie. number of page ? ) */;
  int m_numParagInfo /** the number of paragraph info */;
  int m_numFont /** the number of font */;
  int m_fatherId /** the father id */;
  int m_unknown /** an unknown flags */;

  std::vector<MWAWFont> m_fontList /** the list of fonts */;
  std::vector<ParagraphInfo> m_paragraphList /** the list of paragraph */;
  std::vector<Section> m_sectionList /** the list of section */;
  std::vector<Token> m_tokenList /** the list of token */;
  std::vector<TextZoneInfo> m_textZoneList /** the list of zone */;
  std::multimap<long, PLC> m_plcMap /** the plc map */;
};
////////////////////////////////////////
//! Internal: the state of a CWText
struct State {
  //! constructor
  State() : m_version(-1), m_fontsList(), m_paragraphsList(), m_zoneMap() {
  }

  //! the file version
  mutable int m_version;
  //! the list of fonts
  std::vector<MWAWFont> m_fontsList;
  //! the list of paragraph
  std::vector<Paragraph> m_paragraphsList;
  //! the list of text zone
  std::map<int, shared_ptr<Zone> > m_zoneMap;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
CWText::CWText
(MWAWInputStreamPtr ip, CWParser &parser, MWAWFontConverterPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert), m_state(new CWTextInternal::State),
  m_mainParser(&parser), m_styleManager(parser.m_styleManager), m_asciiFile(parser.ascii())
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
  for (size_t i = 0; i < iter->second->m_zones.size(); i++) {
    MWAWEntry const &entry = iter->second->m_zones[i];
    m_input->seek(entry.begin()+4, WPX_SEEK_SET);
    int numC = int(entry.length()-4);
    for (int ch = 0; ch < numC; ch++) {
      char c = (char) m_input->readULong(1);
      if (c==0xb || c==0x1)
        numPage++;
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
shared_ptr<CWStruct::DSET> CWText::readDSETZone(CWStruct::DSET const &zone, MWAWEntry const &entry, bool &complete)
{
  complete = false;
  if (!entry.valid() || zone.m_fileType != 1)
    return shared_ptr<CWStruct::DSET>();
  int const vers = version();
  long pos = entry.begin();
  m_input->seek(pos+8+16, WPX_SEEK_SET); // avoid header+8 generic number
  libmwaw::DebugStream f;
  shared_ptr<CWTextInternal::Zone> textZone(new CWTextInternal::Zone(zone));

  textZone->m_unknown = (int) m_input->readULong(2); // alway 0 ?
  textZone->m_fatherId = (int) m_input->readULong(2);
  textZone->m_numChar = (int) m_input->readULong(4);
  textZone->m_numTextZone = (int) m_input->readULong(2);
  textZone->m_numParagInfo = (int) m_input->readULong(2);
  textZone->m_numFont = (int) m_input->readULong(2);
  switch(textZone->m_textType >> 4) {
  case 2:
    textZone->m_type = CWStruct::DSET::T_Header;
    break;
  case 4:
    textZone->m_type = CWStruct::DSET::T_Footer;
    break;
  case 6:
    textZone->m_type = CWStruct::DSET::T_Footnote;
    break;
  case 8:
    textZone->m_type = CWStruct::DSET::T_Frame;
    break;
  case 0xe:
    textZone->m_type = CWStruct::DSET::T_Table;
    break;
  default:
    break;
  }
  if (textZone->m_textType != CWStruct::DSET::T_Unknown)
    textZone->m_textType &= 0xF;

  f << "Entries(DSETT):" << *textZone << ",";

  if (long(m_input->tell())%2)
    m_input->seek(1, WPX_SEEK_CUR);
  ascii().addDelimiter(m_input->tell(), '|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  // read the last part
  int data0Length = 0;
  switch(vers) {
  case 1:
    data0Length = 24;
    break;
  case 2:
    data0Length = 28;
    break;
    // case 3: ???
  case 4:
  case 5:
  case 6:
    data0Length = 30;
    break;
  default:
    break;
  }

  int N = int(zone.m_numData);
  if (long(m_input->tell())+N*data0Length > entry.end()) {
    MWAW_DEBUG_MSG(("CWText::readDSETZone: file is too short\n"));
    return shared_ptr<CWStruct::DSET>();
  }

  int val;
  m_input->seek(entry.end()-N*data0Length, WPX_SEEK_SET);
  CWTextInternal::PLC plc;
  plc.m_type = CWTextInternal::P_Child;
  if (data0Length) {
    for (int i = 0; i < N; i++) {
      /* definition of a list of text zone ( one by column and one by page )*/
      pos = m_input->tell();
      f.str("");
      f << "DSETT-" << i << ":";
      CWStruct::DSET::Child child;
      child.m_posC = (long) m_input->readULong(4);
      child.m_type = CWStruct::DSET::Child::TEXT;
      int dim[2];
      for (int j = 0; j < 2; j++)
        dim[j] = (int) m_input->readLong(2);
      child.m_box = Box2i(Vec2i(0,0), Vec2i(dim[0], dim[1]));
      textZone->m_childs.push_back(child);
      plc.m_id = i;
      textZone->m_plcMap.insert(std::map<long, CWTextInternal::PLC>::value_type(child.m_posC, plc));

      f << child;
      f << "ptr=" << std::hex << m_input->readULong(4) << std::dec << ",";
      f << "f0=" << m_input->readLong(2) << ","; // a small number : number of line ?
      f << "y[real]=" << m_input->readLong(2) << ",";
      for (int j = 1; j < 4; j++) {
        val = (int) m_input->readLong(2);
        if (val)
          f << "f" << j << "=" << val << ",";
      }
      int what = (int) m_input->readLong(2);
      // simple id or 0: main text ?, 1 : header/footnote ?, 2: footer
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
  bool ok = true;
  for (int z = 0; z < 4+textZone->m_numTextZone; z++) {
    pos = m_input->tell();
    long sz = (long) m_input->readULong(4);
    if (!sz) {
      f.str("");
      f << "DSETT-Z" << z;
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      continue;
    }

    MWAWEntry zEntry;
    zEntry.setBegin(pos);
    zEntry.setLength(sz+4);

    m_input->seek(zEntry.end(), WPX_SEEK_SET);
    if (long(m_input->tell()) !=  zEntry.end()) {
      MWAW_DEBUG_MSG(("CWText::readDSETZone: entry for %d zone is too short\n", z));
      ascii().addPos(pos);
      ascii().addNote("###");
      m_input->seek(pos, WPX_SEEK_SET);
      if (z > 4) {
        ok = false;
        break;
      }
      return textZone;
    }

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
        MWAW_DEBUG_MSG(("CWText::readDSETZone: can not find text %d zone\n", z-4));
        if (z > 4) break;
        return textZone;
      }
      f.str("");
      f << "DSETT-Z" << z << "#";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
    }
    m_input->seek(zEntry.end(), WPX_SEEK_SET);
  }

  if (ok && vers >= 2) {
    pos = m_input->tell();
    if (!readTextSection(*textZone))
      m_input->seek(pos, WPX_SEEK_SET);
  }

  for ( size_t tok = 0; tok < textZone->m_tokenList.size(); tok++) {
    CWTextInternal::Token const &token = textZone->m_tokenList[tok];
    if (token.m_zoneId > 0)
      textZone->m_otherChilds.push_back(token.m_zoneId);
  }

  if (m_state->m_zoneMap.find(textZone->m_id) != m_state->m_zoneMap.end()) {
    MWAW_DEBUG_MSG(("CWText::readDSETZone: zone %d already exists!!!\n", textZone->m_id));
  } else
    m_state->m_zoneMap[textZone->m_id] = textZone;

  complete = ok;
  return textZone;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

bool CWText::readFont(int id, int &posC, MWAWFont &font)
{
  long pos = m_input->tell();

  int fontSize = 0;
  int vers = version();
  switch(vers) {
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
  posC = int(m_input->readULong(4));
  font = MWAWFont();
  libmwaw::DebugStream f;
  if (id >= 0)
    f << "Font-" << id << ":";
  else
    f << "Font:";

  f << "pos=" << posC << ",";
  font.setId(m_styleManager->getFontId((int) m_input->readULong(2)));
  int flag =(int) m_input->readULong(2);
  uint32_t flags=0;
  if (flag&0x1) flags |= MWAWFont::boldBit;
  if (flag&0x2) flags |= MWAWFont::italicBit;
  if (flag&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
  if (flag&0x8) flags |= MWAWFont::embossBit;
  if (flag&0x10) flags |= MWAWFont::shadowBit;
  if (flag&0x20) font.setDeltaLetterSpacing(-1);
  if (flag&0x40) font.setDeltaLetterSpacing(1);
  if (flag&0x80) font.setStrikeOutStyle(MWAWFont::Line::Simple);
  if (flag&0x100) font.set(MWAWFont::Script::super100());
  if (flag&0x200) font.set(MWAWFont::Script::sub100());
  if (flag&0x400) font.set(MWAWFont::Script::super());
  if (flag&0x800) font.set(MWAWFont::Script::sub());
  if (flag&0x2000) {
    font.setUnderlineStyle(MWAWFont::Line::Simple);
    font.setUnderlineType(MWAWFont::Line::Double);
  }
  font.setSize((float) m_input->readLong(1));

  int colId = (int) m_input->readULong(1);
  MWAWColor color(MWAWColor::black());
  if (colId!=1) {
    MWAWColor col;
    if (m_mainParser->getColor(colId, col))
      color = col;
    else if (vers != 1) {
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
    flag = (int) m_input->readULong(2);
    if (flag & 0x1)
      font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (flag & 0x2) {
      font.setUnderlineStyle(MWAWFont::Line::Simple);
      font.setUnderlineType(MWAWFont::Line::Double);
    }
    if (flag & 0x20)
      font.setStrikeOutStyle(MWAWFont::Line::Simple);
    flag &= 0xFFDC;
    if (flag)
      f << "#flag2=" << std::hex << flag << std::dec << ",";
  }
  font.setFlags(flags);
  font.setColor(color);
  f << font.getDebugString(m_convertissor);
  if (long(m_input->tell()) != pos+fontSize)
    ascii().addDelimiter(m_input->tell(), '|');
  m_input->seek(pos+fontSize, WPX_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

bool CWText::readChar(int id, int fontSize, MWAWFont &font)
{
  long pos = m_input->tell();

  m_input->seek(pos, WPX_SEEK_SET);
  font = MWAWFont();
  libmwaw::DebugStream f;
  if (id == 0)
    f << "Entries(CHAR)-0:";
  else
    f << "CHAR-" << id << ":";

  int val = (int) m_input->readLong(2);
  if (val != -1) f << "f0=" << val << ",";
  f << "flags=[";
  for (int i = 0; i < 6; i++) {
    val  = (int) m_input->readLong(2);
    if (val) {
      if (i == 3)
        f << "f" << i << "=" << std::hex << val << std::dec << ",";
      else
        f << "f" << i << "=" << val << ",";
    }
  }
  font.setId(m_styleManager->getFontId((int) m_input->readULong(2)));
  int flag =(int) m_input->readULong(2);
  uint32_t flags=0;
  if (flag&0x1) flags |= MWAWFont::boldBit;
  if (flag&0x2) flags |= MWAWFont::italicBit;
  if (flag&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
  if (flag&0x8) flags |= MWAWFont::embossBit;
  if (flag&0x10) flags |= MWAWFont::shadowBit;
  if (flag&0x20) font.setDeltaLetterSpacing(-1);
  if (flag&0x40) font.setDeltaLetterSpacing(1);
  if (flag&0x80) font.setStrikeOutStyle(MWAWFont::Line::Simple);
  if (flag&0x100) font.set(MWAWFont::Script::super100());
  if (flag&0x200) font.set(MWAWFont::Script::sub100());
  if (flag&0x400) font.set(MWAWFont::Script::super());
  if (flag&0x800) font.set(MWAWFont::Script::sub());
  if (flag&0x2000) {
    font.setUnderlineStyle(MWAWFont::Line::Simple);
    font.setUnderlineType(MWAWFont::Line::Double);
  }
  font.setSize((float) m_input->readLong(1));

  int colId = (int) m_input->readULong(1);
  MWAWColor color(MWAWColor::black());
  if (colId!=1) {
    f << "#col=" << std::hex << colId << std::dec << ",";
  }
  font.setColor(color);
  if (fontSize >= 12 && version()==6) {
    flag = (int) m_input->readULong(2);
    if (flag & 0x1)
      font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (flag & 0x2) {
      font.setUnderlineStyle(MWAWFont::Line::Simple);
      font.setUnderlineType(MWAWFont::Line::Double);
    }
    if (flag & 0x20)
      font.setStrikeOutStyle(MWAWFont::Line::Simple);
    flag &= 0xFFDC;
    if (flag)
      f << "#flag2=" << std::hex << flag << std::dec << ",";
  }
  font.setFlags(flags);
  f << font.getDebugString(m_convertissor);
  if (long(m_input->tell()) != pos+fontSize)
    ascii().addDelimiter(m_input->tell(), '|');
  m_input->seek(pos+fontSize, WPX_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// the fonts properties
////////////////////////////////////////////////////////////
bool CWText::readFonts(MWAWEntry const &entry, CWTextInternal::Zone &zone)
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

  int numElt = int((entry.length()-4)/fontSize);
  long actC = -1;

  m_input->seek(pos+4, WPX_SEEK_SET); // skip header
  // first check char pos is ok
  for (int i = 0; i < numElt; i++) {
    pos = m_input->tell();
    long newC = (long) m_input->readULong(4);
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
  CWTextInternal::PLC plc;
  plc.m_type = CWTextInternal::P_Font;
  for (int i = 0; i < numElt; i++) {
    MWAWFont font;
    int posChar;
    if (!readFont(i, posChar, font)) return false;
    zone.m_fontList.push_back(font);
    plc.m_id = i;
    zone.m_plcMap.insert(std::map<long, CWTextInternal::PLC>::value_type(posChar, plc));
  }

  return true;
}

////////////////////////////////////////////////////////////
// the paragraphs properties
////////////////////////////////////////////////////////////
bool CWText::readParagraphs(MWAWEntry const &entry, CWTextInternal::Zone &zone)
{
  long pos = entry.begin();

  int styleSize = 0;
  int const vers = version();
  switch(vers) {
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

  int numElt = int((entry.length()-4)/styleSize);
  long actC = -1;

  m_input->seek(pos+4, WPX_SEEK_SET); // skip header
  // first check char pos is ok
  for (int i = 0; i < numElt; i++) {
    pos = m_input->tell();
    long newC = (long) m_input->readULong(4);
    if (newC < actC) return false;
    actC = newC;
    m_input->seek(pos+styleSize, WPX_SEEK_SET);
  }

  pos = entry.begin();
  ascii().addPos(pos);
  ascii().addNote("Entries(Paragraph)");

  libmwaw::DebugStream f;
  int numParagraphs = int(m_state->m_paragraphsList.size());
  m_input->seek(pos+4, WPX_SEEK_SET); // skip header
  CWTextInternal::PLC plc;
  plc.m_type = CWTextInternal::P_Ruler;
  for (int i = 0; i < numElt; i++) {
    pos = m_input->tell();
    CWTextInternal::ParagraphInfo info;

    long posC = (long) m_input->readULong(4);
    f.str("");
    f << "Paragraph-" << i << ": pos=" << posC << ",";
    info.m_styleId = (int) m_input->readLong(2);
    if (styleSize >= 8)
      info.m_unknown = (int) m_input->readLong(2);
    f << info;

    int rulerId = info.m_styleId;
    if (vers > 2) {
      CWStyleManager::Style style;
      m_styleManager->get(rulerId, style);
      rulerId = style.m_rulerId;
    }
    if (rulerId >= 0 && rulerId < numParagraphs)
      f << "ruler"<< rulerId << "[" << m_state->m_paragraphsList[(size_t) rulerId] << "]";
    if (long(m_input->tell()) != pos+styleSize)
      ascii().addDelimiter(m_input->tell(), '|');
    zone.m_paragraphList.push_back(info);
    plc.m_id = rulerId;
    zone.m_plcMap.insert(std::map<long, CWTextInternal::PLC>::value_type(posC, plc));
    m_input->seek(pos+styleSize, WPX_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  return true;
}

////////////////////////////////////////////////////////////
// zone which corresponds to the token
////////////////////////////////////////////////////////////
bool CWText::readTokens(MWAWEntry const &entry, CWTextInternal::Zone &zone)
{
  long pos = entry.begin();

  int dataSize = 0;
  int const vers=version();
  switch(vers) {
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
    dataSize = 32;
    break;
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

  int numElt = int((entry.length()-4)/dataSize);
  m_input->seek(pos+4, WPX_SEEK_SET); // skip header

  libmwaw::DebugStream f;
  CWTextInternal::PLC plc;
  plc.m_type = CWTextInternal::P_Token;
  int val;
  for (int i = 0; i < numElt; i++) {
    pos = m_input->tell();

    int posC = (int) m_input->readULong(4);
    CWTextInternal::Token token;

    int type = (int) m_input->readLong(2);
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

    token.m_unknown[0] =  (int) m_input->readLong(2);
    token.m_zoneId = (int) m_input->readLong(2);
    token.m_unknown[1] =  (int) m_input->readLong(1);
    token.m_page = (int) m_input->readLong(1);
    token.m_unknown[2] =  (int) m_input->readLong(2);
    for (int j = 0; j < 2; j++)
      token.m_size[1-j] =  (int) m_input->readLong(2);
    for (int j = 0; j < 3; j++) {
      val = (int) m_input->readLong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }
    val = (int) m_input->readLong(2);
    if (vers>=6) // checkme: ok for v6 & graphic, not for v2
      token.m_descent = val;
    else if (val)
      f << "f3=" << val << ",";
    token.m_extra = f.str();
    f.str("");
    f << "Token-" << i << ": pos=" << posC << "," << token;
    zone.m_tokenList.push_back(token);
    plc.m_id = i;
    zone.m_plcMap.insert(std::map<long, CWTextInternal::PLC>::value_type(posC, plc));

    if (long(m_input->tell()) != pos && long(m_input->tell()) != pos+dataSize)
      ascii().addDelimiter(m_input->tell(), '|');
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+dataSize, WPX_SEEK_SET);
  }

  m_input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

// read the different section definition
bool CWText::readTextSection(CWTextInternal::Zone &zone)
{
  int const vers = version();
  long pos = m_input->tell();
  long sz = (long) m_input->readULong(4);
  long endPos = pos+4+sz;
  m_input->seek(endPos,WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos || (sz && sz < 12)) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWText::readTextSection: unexpected size\n"));
    return false;
  }
  if (sz == 0) {
    ascii().addPos(pos);
    ascii().addNote("Nop");
    return true;
  }
  libmwaw::DebugStream f;
  f << "Entries(TextSection):";

  m_input->seek(pos+4, WPX_SEEK_SET);
  int N = (int) m_input->readLong(2);
  f << "N=" << N << ",";
  int type = (int) m_input->readLong(2);
  if (type != -1)
    f << "#type=" << type << ",";
  long val = m_input->readLong(2);
  if (val) f << "#unkn=" << val << ",";
  int fSz = (int) m_input->readULong(2);
  int hSz = (int) m_input->readULong(2);
  if (!fSz || N *fSz+hSz+12 != sz) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWText::readTextSection: unexpected size\n"));
    return false;
  }
  if ((vers > 3 && fSz != 0x4e) || (vers <= 3 && fSz < 60)) {
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(endPos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWText::readTextSection: unexpected size\n"));
    return true;
  }
  if (long(m_input->tell()) != pos+4+hSz)
    ascii().addDelimiter(m_input->tell(), '|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  m_input->seek(endPos-N*fSz, WPX_SEEK_SET);
  CWTextInternal::PLC plc;
  plc.m_type = CWTextInternal::P_Section;
  for (int i= 0; i < N; i++) {
    CWTextInternal::Section sec;

    pos = m_input->tell();
    f.str("");
    sec.m_pos  = m_input->readLong(4);
    for (int j = 0; j < 4; j++) {
      /** find f0=0|1, f1=O| (for second section)[1|2|4]
      f2=0| (for second section [2e,4e,5b] , f3=0|2d|4d|5a */
      val = m_input->readLong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }
    sec.m_numColumns  = (int) m_input->readULong(2);
    if (!sec.m_numColumns || sec.m_numColumns > 10) {
      MWAW_DEBUG_MSG(("CWText::readTextSection: num columns seems odd\n"));
      f << "#numColumns=" << sec.m_numColumns << ",";
      sec.m_numColumns = 1;
    }
    for (int c = 0; c < sec.m_numColumns; c++)
      sec.m_columnsWidth.push_back((int)m_input->readULong(2));
    m_input->seek(pos+34, WPX_SEEK_SET);
    for (int c = 0; c < sec.m_numColumns-1; c++)
      sec.m_columnsSep.push_back((int)m_input->readLong(2));
    m_input->seek(pos+52, WPX_SEEK_SET);
    for (int j = 0; j < 4; j++) {
      // find g0=0|1, g1=0|1, g2=100, g3=0|e0|7b
      val = (int) m_input->readULong(2);
      if (val) f << "g" << j << "=" << std::hex << val << std::dec << ",";
    }
    sec.m_extra = f.str();
    zone.m_sectionList.push_back(sec);
    plc.m_id = i;
    zone.m_plcMap.insert(std::map<long, CWTextInternal::PLC>::value_type(sec.m_pos, plc));
    f.str("");
    f << "TextSection-" << i << ":" << sec;
    if (m_input->tell() != pos+fSz)
      ascii().addDelimiter(m_input->tell(), '|');
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+fSz, WPX_SEEK_SET);
  }

  return true;
}

////////////////////////////////////////////////////////////
// read the different size for the text
////////////////////////////////////////////////////////////
bool CWText::readTextZoneSize(MWAWEntry const &entry, CWTextInternal::Zone &zone)
{
  long pos = entry.begin();

  int dataSize = 10;
  if ((entry.length()%dataSize) != 4)
    return false;

  ascii().addPos(pos);
  ascii().addNote("Entries(TextZoneSz)");

  int numElt = int((entry.length()-4)/dataSize);

  m_input->seek(pos+4, WPX_SEEK_SET); // skip header

  libmwaw::DebugStream f;

  CWTextInternal::PLC plc;
  plc.m_type = CWTextInternal::P_TextZone;
  for (int i = 0; i < numElt; i++) {
    pos = m_input->tell();
    f.str("");
    f << "TextZoneSz-" << i << ":";
    CWTextInternal::TextZoneInfo info;
    info.m_pos = (long) m_input->readULong(4);
    info.m_N =  (int) m_input->readULong(2);
    f << info;
    zone.m_textZoneList.push_back(info);
    plc.m_id = i;
    zone.m_plcMap.insert(std::map<long, CWTextInternal::PLC>::value_type(info.m_pos, plc));

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
  if (!m_listener) {
    MWAW_DEBUG_MSG(("CWText::sendText: can not find a listener\n"));
    return false;
  }
  // Removeme when all is ok
  if (m_listener->isParagraphOpened())
    m_listener->insertEOL();
  long actC = 0;
  bool main = zone.m_id == 1;
  int numParagraphs = int(m_state->m_paragraphsList.size());
  int actPage = 1;
  size_t numZones = zone.m_zones.size();
  if (main)
    m_mainParser->newPage(actPage);
  int numCols = 1;
  int numSection = 0, numSectionInPage=0;
  int nextSection = -1;
  long nextSectionPos = main ? 0 : -1;
  if (zone.m_sectionList.size()) {
    nextSection = 0;
    nextSectionPos = zone.m_sectionList[0].m_pos;
  }
  std::multimap<long, CWTextInternal::PLC>::const_iterator plcIt;
  for (size_t z = 0; z < numZones; z++) {
    MWAWEntry const &entry  =  zone.m_zones[z];
    long pos = entry.begin();
    libmwaw::DebugStream f, f2;

    int numC = int(entry.length()-4);
    m_input->seek(pos+4, WPX_SEEK_SET); // skip header

    for (int i = 0; i < numC; i++) {
      if (nextSectionPos!=-1 && actC >= nextSectionPos) {
        if (actC != nextSectionPos) {
          MWAW_DEBUG_MSG(("CWText::sendText: find a section inside a complex char!!!\n"));
          f << "###";
        }
        std::vector<int> width, sepWidth;
        numSection++;
        numSectionInPage++;
        if (nextSection>=0) {
          CWTextInternal::Section const &sec = zone.m_sectionList[size_t(nextSection)];
          numCols = sec.m_numColumns;
          width = sec.m_columnsWidth;
          sepWidth = sec.m_columnsSep;
          if (size_t(++nextSection) < zone.m_sectionList.size())
            nextSectionPos = zone.m_sectionList[size_t(nextSection)].m_pos;
          else {
            nextSectionPos = -1;
            nextSection = -1;
          }
        } else {
          m_mainParser->getColumnInfo(numCols, width, sepWidth);
          nextSectionPos = -1;
          nextSection = -1;
        }
        int actCols = m_listener->getSectionNumColumns();
        if (numCols > 1  || actCols > 1) {
          if (m_listener->isSectionOpened())
            m_listener->closeSection();
          if (numCols<=1) m_listener->openSection();
          else {
            if (width.size() == 0) {
              int colWidth = int((72.0*m_mainParser->pageWidth())/numCols);
              width.resize((size_t) numCols, colWidth);
            }
            m_listener->openSection(width, WPX_POINT);
          }
        }
      } else if (numSectionInPage==0)
        numSectionInPage++;
      plcIt = zone.m_plcMap.find(actC);
      bool seeToken = false;
      while (plcIt != zone.m_plcMap.end() && plcIt->first<=actC) {
        if (actC != plcIt->first) {
          MWAW_DEBUG_MSG(("CWText::sendText: find a plc inside a complex char!!!\n"));
          f << "###";
        }
        CWTextInternal::PLC const &plc = plcIt++->second;
        f << "[" << plc << "]";
        switch(plc.m_type) {
        case CWTextInternal::P_Font:
          if (plc.m_id < 0 || plc.m_id >= int(zone.m_fontList.size())) {
            MWAW_DEBUG_MSG(("CWText::sendText: can not find font %d\n", plc.m_id));
            f << "###";
            break;
          }
          m_listener->setFont(zone.m_fontList[size_t(plc.m_id)]);
          break;
        case CWTextInternal::P_Ruler:
          if (plc.m_id >= 0 && plc.m_id < numParagraphs)
            setProperty(m_state->m_paragraphsList[(size_t) plc.m_id]);
          break;
        case CWTextInternal::P_Token: {
          if (plc.m_id < 0 || plc.m_id >= int(zone.m_tokenList.size())) {
            MWAW_DEBUG_MSG(("CWText::sendText: can not find the token %d\n", plc.m_id));
            f << "###";
            break;
          }
          CWTextInternal::Token const &token = zone.m_tokenList[size_t(plc.m_id)];
          switch(token.m_type) {
          case CWTextInternal::TKN_FOOTNOTE:
            if (zone.okChildId(token.m_zoneId))
              m_mainParser->sendFootnote(token.m_zoneId);
            else
              f << "###";
            break;
          case CWTextInternal::TKN_PAGENUMBER:
            switch(token.m_unknown[0]) {
            case 1:
            case 2: {
              std::stringstream s;
              int num =  token.m_unknown[0]==1 ? numSection : numSectionInPage;
              s << num;
              m_listener->insertUnicodeString(s.str().c_str());
              break;
            }
            case 3:
              m_listener->insertField(MWAWContentListener::PageCount);
              break;
            case 0:
            default:
              m_listener->insertField(MWAWContentListener::PageNumber);
            }
            break;
          case CWTextInternal::TKN_GRAPHIC:
            if (zone.okChildId(token.m_zoneId)) {
              // fixme
              if (token.m_descent != 0) {
                MWAWPosition tPos(Vec2f(0,float(token.m_descent)), Vec2f(), WPX_POINT);
                tPos.setRelativePosition(MWAWPosition::Char, MWAWPosition::XLeft, MWAWPosition::YBottom);
                m_mainParser->sendZone(token.m_zoneId, tPos);
              } else
                m_mainParser->sendZone(token.m_zoneId);
            } else
              f << "###";
            break;
          case CWTextInternal::TKN_UNKNOWN:
          default:
            break;
          }
          seeToken = true;
          break;
        }
        case CWTextInternal::P_Child:
        case CWTextInternal::P_Section:
        case CWTextInternal::P_TextZone:
        case CWTextInternal::P_Unknown:
        default:
          break;
        }
      }
      char c = (char) m_input->readULong(1);
      actC++;
      if (c == '\0') {
        if (i == numC-1) break;
        MWAW_DEBUG_MSG(("CWText::sendText: OOPS, find 0 reading the text\n"));
        f << "###0x0";
        continue;
      }
      f << c;
      if (seeToken && c >= 0 && c < 32) continue;
      switch (c) {
      case 0x1: // fixme: column break
        if (numCols) {
          m_listener->insertBreak(MWAWContentListener::ColumnBreak);
          break;
        }
        MWAW_DEBUG_MSG(("CWText::sendText: Find unexpected char 1\n"));
        f << "###";
      case 0xb: // page break
        numSectionInPage = 0;
        if (main)
          m_mainParser->newPage(++actPage);
        break;
      case 0x2: // token footnote ( normally already done)
        break;
      case 0x3: // token graphic
        break;
      case 0x4:
        m_listener->insertField(MWAWContentListener::Date);
        break;
      case 0x5:
        m_listener->insertField(MWAWContentListener::Time);
        break;
      case 0x6: // normally already done, but if we do not find the token, ...
        m_listener->insertField(MWAWContentListener::PageNumber);
        break;
      case 0x7: // footnote index (ok to ignore : index of the footnote )
        break;
      case 0x8: // potential breaking <<hyphen>>
        break;
      case 0x9:
        m_listener->insertTab();
        break;
      case 0xa:
        m_listener->insertEOL(true);
        break;
      case 0xc: // new section (done)
        break;
      case 0xd:
        f2.str("");
        f2 << "Entries(TextContent):" << f.str();
        ascii().addPos(pos);
        ascii().addNote(f2.str().c_str());
        f.str("");
        pos = m_input->tell();

        // ignore last end of line returns
        if (z != numZones-1 || i != numC-2)
          m_listener->insertEOL();
        break;

      default: {
        int extraChar = m_listener->insertCharacter
                        ((unsigned char)c, m_input, m_input->tell()+(numC-1-i));
        if (extraChar) {
          i += extraChar;
          actC += extraChar;
        }
      }
      }
    }
    if (f.str().length()) {
      f2.str("");
      f2 << "Entries(TextContent):" << f.str();
      ascii().addPos(pos);
      ascii().addNote(f2.str().c_str());
    }
  }

  return true;
}

////////////////////////////////////////////////////////////
// the style definition?
////////////////////////////////////////////////////////////
bool CWText::readSTYL_CHAR(int N, int fSz)
{
  if (fSz == 0 || N== 0) return true;
  libmwaw::DebugStream f;
  if (m_state->m_fontsList.size()) {
    MWAW_DEBUG_MSG(("CWText::readSTYL_CHAR: font list already exists!!!\n"));
  }
  m_state->m_fontsList.resize((size_t)N);
  for (int i = 0; i < N; i++) {
    long pos = m_input->tell();
    MWAWFont font;
    if (readChar(i, fSz, font))
      m_state->m_fontsList[(size_t) i] = font;
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
    MWAW_DEBUG_MSG(("CWText::readSTYL_RULR: Find odd ruler size %d\n", fSz));
  }
  libmwaw::DebugStream f;
  for (int i = 0; i < N; i++) {
    long pos = m_input->tell();
    if (fSz != 108 || !readParagraph(i)) {
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

////////////////////////////////////////////////////////////
// read a list of rulers
////////////////////////////////////////////////////////////
bool CWText::readParagraphs()
{
  long pos = m_input->tell();
  long sz = (long) m_input->readULong(4);
  long endPos = pos+4+sz;

  m_input->seek(endPos, WPX_SEEK_SET);
  if (m_input->atEOS()) {
    MWAW_DEBUG_MSG(("CWText::readParagraphs: ruler zone is too short\n"));
    return false;
  }
  m_input->seek(pos+4, WPX_SEEK_SET);

  int N = (int) m_input->readULong(2);
  int type = (int) m_input->readLong(2);
  int val =  (int) m_input->readLong(2);
  int fSz =  (int) m_input->readLong(2);

  if (sz != 12+fSz*N) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWText::readParagraphs: find odd ruler size\n"));
    return false;
  }

  libmwaw::DebugStream f;
  f << "Entries(RULR):";
  f << "N=" << N << ", type?=" << type <<", fSz=" << fSz << ",";
  if (val) f << "unkn=" << val << ",";

  for (int i = 0; i < 2; i++) {
    val = (int) m_input->readLong(2);
    if (val)  f << "f" << i << "=" << val << ",";
  }

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = m_input->tell();
    if (!readParagraph(i)) {
      m_input->seek(pos, WPX_SEEK_SET);
      return false;
    }
  }
  return true;
}

////////////////////////////////////////////////////////////
// read a ruler zone
////////////////////////////////////////////////////////////
bool CWText::readParagraph(int id)
{
  int dataSize = 0;
  int vers = version();
  switch (vers) {
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
    MWAW_DEBUG_MSG(("CWText::readParagraph: unknown size\n"));
    return false;
  }

  CWTextInternal::Paragraph ruler;
  long pos = m_input->tell();
  long endPos = pos+dataSize;
  libmwaw::DebugStream f;

  int val;
  if (vers >= 4 && id >= 0) {
    val = (int) m_input->readLong(2);
    if (val != -1) f << "f0=" << val << ",";
    val = (int) m_input->readLong(4);
    f << "f1=" << val << ",";
    int dim[2];
    for (int i = 0; i < 2; i++)
      dim[i] = (int) m_input->readLong(2);
    f << "dim?=" << dim[0] << "x" << dim[1] << ",";
    ruler.m_labelType = (int) m_input->readLong(1);
    int listLevel = (int) m_input->readLong(1);
    if (listLevel < 0 || listLevel > 10) {
      MWAW_DEBUG_MSG(("CWText::readParagraph: can not determine list level\n"));
      f << "##listLevel=" << listLevel << ",";
      listLevel = 0;
    }
    ruler.m_listLevelIndex = listLevel;
  }

  val = (int) m_input->readLong(2);
  f << "num[used]=" << val << ",";
  val = (int) m_input->readULong(2);
  int align = 0;
  switch(vers) {
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
    align = (val >> 14);
    val &= 0x3FFF;
    break;
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
    ruler.m_justify = MWAWParagraph::JustificationCenter;
    break;
  case 2:
    ruler.m_justify = MWAWParagraph::JustificationRight ;
    break;
  case 3:
    ruler.m_justify = MWAWParagraph::JustificationFull;
    break;
  default:
    break;
  }


  bool inPoint = false;
  int interline = 0;
  switch(vers) {
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
    case 6: // display unit pica
    case 5: // display unit cm
    case 4: // display unit mm
    case 3: // display unit Inch
    case 2: // display unit point
      ok = interline <= 512;
      inPoint = true; // data always stored in point
      break;
    default:
      ok = false;
      break;
    }
    if (ok) val = 0;
    else {
      MWAW_DEBUG_MSG(("CWText::readParagraph: can not determine interline dimension\n"));
      interline = 0;
    }
    break;
  }
  default:
    break;
  }
  if (interline) {
    if (inPoint)
      ruler.setInterline(interline, WPX_POINT);
    else
      ruler.setInterline(1.0+interline*0.5, WPX_PERCENT);
  }
  if (val) f << "#flags=" << std::hex << val << std::dec << ",";
  for (int i = 0; i < 3; i++)
    ruler.m_margins[i] = float(m_input->readLong(2))/72.f;
  *(ruler.m_margins[2]) -= 28./72.;
  if (ruler.m_margins[2].get() < 0.0) ruler.m_margins[2] = 0.0;
  if (vers >= 2) {
    for(int i = 0; i < 2; i++) {
      ruler.m_spacings[i+1] = float(m_input->readULong(1))/72.f;
      m_input->seek(1, WPX_SEEK_CUR); // flags to define the printing unit
    }
  }
  val = (int) m_input->readLong(1);
  if (val) f << "unkn1=" << val << ",";
  int numTabs = (int) m_input->readULong(1);
  if (long(m_input->tell())+numTabs*4 > endPos) {
    if (numTabs != 255) { // 0xFF seems to be used in v1, v2
      MWAW_DEBUG_MSG(("CWText::readParagraph: numTabs is too big\n"));
    }
    f << "numTabs*=" << numTabs << ",";
    numTabs = 0;
  }
  for (int i = 0; i < numTabs; i++) {
    MWAWTabStop tab;
    tab.m_position = float(m_input->readLong(2))/72.f;
    val = (int) m_input->readULong(1);
    int leaderType = 0;
    switch(vers) {
    case 1:
      align = val & 3;
      val &= 0xFC;
      break;
    case 2:
    case 3:
    case 4:
    case 5:
      align = (val >> 6);
      leaderType = (val & 3);
      val &= 0x3C;
      break;
    case 6:
      align = (val >> 5);
      leaderType = (val & 3);
      val &= 0x9C;
      break;
    default:
      break;
    }
    switch(align&3) {
    case 1:
      tab.m_alignment = MWAWTabStop::CENTER;
      break;
    case 2:
      tab.m_alignment = MWAWTabStop::RIGHT;
      break;
    case 3:
      tab.m_alignment = MWAWTabStop::DECIMAL;
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
    char decimalChar = (char) m_input->readULong(1);
    if (decimalChar != ',' && decimalChar != '.')
      f << "decimalChar=" << decimalChar << ",";
    ruler.m_tabs->push_back(tab);
    if (val)
      f << "#unkn[tab" << i << "=" << std::hex << val << std::dec << "],";
  }
  ruler.updateListLevel();
  ruler.m_extra = f.str();
  // save the style
  if (id >= 0) {
    if (int(m_state->m_paragraphsList.size()) <= id)
      m_state->m_paragraphsList.resize((size_t)id+1);
    m_state->m_paragraphsList[(size_t)id]=ruler;
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

void CWText::setProperty(CWTextInternal::Paragraph const &ruler)
{
  if (!m_listener) return;
  m_listener->setParagraph(ruler);
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
