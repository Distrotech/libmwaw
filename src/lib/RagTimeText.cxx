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

#include <librevenge/librevenge.h>

#include "MWAWTextListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"

#include "RagTimeParser.hxx"

#include "RagTimeText.hxx"

/** Internal: the structures of a RagTimeText */
namespace RagTimeTextInternal
{
//! Internal: a token of a RagTimeText
struct Token {
  //! the token's types
  enum Type { List, Page, PageCount, PageAfter, Date, Time, Unknown };
  //! constructor
  Token() : m_type(Unknown), m_listLevel(0), m_DTFormat(""), m_extra("")
  {
    for (int i=0; i<4; ++i) m_listIndices[i]=0;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Token const &tkn)
  {
    switch (tkn.m_type) {
    case Token::List:
      o << "list[" << tkn.m_listLevel << "]=[";
      for (int i=0; i<4 && i<tkn.m_listLevel; ++i)
        o << tkn.m_listIndices[i] << ",";
      o << "],";
      break;
    case Token::Page:
      o << "page,";
      break;
    case Token::PageAfter:
      o << "page+1,";
      break;
    case Token::PageCount:
      o << "page[num],";
      break;
    case Token::Date:
      o << "date[" << tkn.m_DTFormat << "],";
      break;
    case Token::Time:
      o << "time[" << tkn.m_DTFormat << "],";
      break;
    case Token::Unknown:
    default:
      o << "#type[unkn],";
      break;
    }
    o << tkn.m_extra;
    return o;
  }
  //! returns a field corresponding to the token if possible
  bool getField(MWAWField &field) const
  {
    switch (m_type) {
    case Page:
      field=MWAWField(MWAWField::PageNumber);
      break;
    case PageCount:
      field=MWAWField(MWAWField::PageCount);
      break;
    case Date:
      field=MWAWField(MWAWField::Date);
      field.m_DTFormat=m_DTFormat;
      break;
    case Time:
      field=MWAWField(MWAWField::Time);
      field.m_DTFormat=m_DTFormat;
      break;
    case PageAfter:
    case Unknown:
    case List:
    default:
      return false;
    }
    return true;
  }
  //! returns a string corresponding to the list indices
  bool getIndicesString(std::string &str) const
  {
    if (m_type!=List) {
      MWAW_DEBUG_MSG(("RagTimeTextInternal::Token::getIndicesString: must only be called on list token\n"));
      return false;
    }
    std::stringstream s;
    for (int i=0; i<4 && i<m_listLevel; ++i) {
      s << m_listIndices[i];
      if (i==0 || i+1<m_listLevel) s << ".";
    }
    str=s.str();
    return true;
  }

  //! the token type
  Type m_type;
  //! the list level(for a list)
  int m_listLevel;
  //! the four list indices
  int m_listIndices[4];
  //! the date time format
  std::string m_DTFormat;
  //! extra data
  std::string m_extra;
};

//! Internal: a text's zone of a RagTimeText
struct TextZone {
  //! constructor
  TextZone() : m_textPos(), m_fontPosList(), m_fontList(), m_paragraphPosList(), m_paragraphList(), m_tokenList(), m_isSent(false)
  {
  }
  //! the text zone
  MWAWEntry m_textPos;
  //! the beginning of character properties in the text zone
  std::vector<long> m_fontPosList;
  //! the list of character's properties
  std::vector<MWAWFont> m_fontList;
  //! the beginning of paragraph properties in the text zone
  std::vector<long> m_paragraphPosList;
  //! the list of paragraph's properties
  std::vector<MWAWParagraph> m_paragraphList;
  //! the list of tokens
  std::vector<Token> m_tokenList;
  //! true if the zone is sent to the listener
  mutable bool m_isSent;
};

////////////////////////////////////////
//! Internal: the state of a RagTimeText
struct State {
  //! constructor
  State() : m_version(-1), m_localFIdMap(), m_charPropList(), m_idTextMap()
  {
  }

  //! return a mac font id corresponding to a local id
  int getFontId(int localId) const
  {
    if (m_localFIdMap.find(localId)==m_localFIdMap.end())
      return localId;
    return m_localFIdMap.find(localId)->second;
  }

  //! the file version
  mutable int m_version;
  //! a map local fontId->fontId
  std::map<int, int> m_localFIdMap;
  //! the character properties
  std::vector<MWAWFont> m_charPropList;
  //! a map entry id to text zone
  std::map<int, shared_ptr<TextZone> > m_idTextMap;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
RagTimeText::RagTimeText(RagTimeParser &parser) :
  m_parserState(parser.getParserState()), m_state(new RagTimeTextInternal::State), m_mainParser(&parser)
{
}

RagTimeText::~RagTimeText()
{ }

int RagTimeText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}

int RagTimeText::getFontId(int localId) const
{
  return m_state->getFontId(localId);
}

bool RagTimeText::getCharStyle(int charId, MWAWFont &font) const
{
  if (charId<0 || charId>=int(m_state->m_charPropList.size())) {
    MWAW_DEBUG_MSG(("RagTimeText::readFontNames: can not find char style %d\n", charId));
    return false;
  }
  font=m_state->m_charPropList[size_t(charId)];
  return true;
}

////////////////////////////////////////////////////////////
// rsrc zone: fonts/character properties
////////////////////////////////////////////////////////////
bool RagTimeText::readFontNames(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+0x26)) {
    MWAW_DEBUG_MSG(("RagTimeText::readFontNames: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(" << entry.type() << ")[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  int headerSz=(int) input->readULong(2);
  int fSz=(int) input->readULong(2);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (headerSz<0x20 || fSz<0x10 || dSz<headerSz+(N+1)*fSz || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeText::readFontNames: the size seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  input->seek(pos+2+headerSz, librevenge::RVNG_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  std::set<long> posSet;
  std::map<int, long> fontIdPosMap;
  posSet.insert(endPos);
  for (int i=0; i<=N; ++i) {
    pos=input->tell();
    f.str("");
    f << entry.type() << "-" << i << ":";
    int val=(int) input->readLong(2); // small number
    if (val) f << "f0=" << val << ",";
    val=(int) input->readLong(2); // always 0?
    if (val) f << "f1=" << val << ",";
    int fPos=(int) input->readULong(2);
    f << "pos[name]=" << std::hex << entry.begin()+2+fPos << std::dec << ",";
    posSet.insert(entry.begin()+2+fPos);
    int fId=(int) input->readLong(2);
    if (fId) f << "fId=" << fId << ",";
    fontIdPosMap[i]=entry.begin()+2+fPos;
    ascFile.addDelimiter(input->tell(),'|');
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  for (std::map<int, long>::const_iterator it=fontIdPosMap.begin(); it!=fontIdPosMap.end(); ++it) {
    pos=it->second;
    int fId=it->first;
    if (pos>=endPos) continue;
    std::set<long>::const_iterator pIt=posSet.find(pos);
    f.str("");
    f << entry.type() << "[name]:id=" << fId << ",";
    if (pIt==posSet.end()|| ++pIt==posSet.end()) {
      MWAW_DEBUG_MSG(("RagTimeText::readFontNames: can not find the end name position\n"));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    std::string name("");
    long nextPos=*pIt;
    while (!input->isEnd() && input->tell()<nextPos) {
      char c=(char) input->readULong(1);
      if (c=='\0') break;
      name+=c;
    }
    f << name;
    // ok, let update the conversion map
    m_state->m_localFIdMap[fId]=m_parserState->m_fontConverter->getId(name);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool RagTimeText::readCharProperties(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+0x26)) {
    MWAW_DEBUG_MSG(("RagTimeText::readCharProperties: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(CharProp)[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  int headerSz=(int) input->readULong(2);
  int fSz=(int) input->readULong(2);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (headerSz<0x2c || fSz<42 || dSz!=headerSz+(N+1)*fSz || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeText::readCharProperties: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  if (fSz>42) {
    MWAW_DEBUG_MSG(("RagTimeText::readCharProperties: the data size seems odds\n"));
    f << "###";
  }
  input->seek(pos+2+headerSz, librevenge::RVNG_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  for (int i=0; i<=N; ++i) {
    pos=input->tell();
    if (i==N) {
      input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
      ascFile.addPos(pos);
      ascFile.addNote("CharProp[end]:");
      break;
    }
    f.str("");
    f << "CharProp-S" << i << ":";


    int val=(int) input->readLong(2); // always 0 or a small negative number
    if (val) f << "f0=" << val;
    val=(int) input->readLong(2);
    if (val) f << "used=" << val << ",";

    MWAWFont font;
    font.setId(getFontId((int) input->readULong(2)-1));
    int size= (int) input->readULong(2);
    if (size>1000) {
      MWAW_DEBUG_MSG(("RagTimeText::readCharProperties: the font size seems bad\n"));
      f << "###sz=" << size << ",";
    }
    font.setSize((float)size);
    val=(int) input->readLong(2); // always 0?
    if (val) f << "f1=" << val;

    int flag = (int) input->readULong(2);
    uint32_t flags=0;
    if (flag&0x1) flags |= MWAWFont::boldBit;
    if (flag&0x2) flags |= MWAWFont::italicBit;
    if (flag&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (flag&0x8) flags |= MWAWFont::embossBit;
    if (flag&0x10) flags |= MWAWFont::shadowBit;
    if (flag&0x20) font.setDeltaLetterSpacing(-1);
    if (flag&0x40) font.setDeltaLetterSpacing(1);
    if (flag&0x80) font.set(MWAWFont::Script::super100());
    if (flag&0x100) font.set(MWAWFont::Script::sub100());
    font.setFlags(flags);
    // checkme: does the following contains interesting data ?
    ascFile.addDelimiter(input->tell(), '|');
    f << font.getDebugString(m_parserState->m_fontConverter);
    m_state->m_charPropList.push_back(font);
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read a zone of text
////////////////////////////////////////////////////////////
bool RagTimeText::readTextZone(MWAWEntry &entry, int width, MWAWColor const &color)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  int const vers=version();
  int dataFieldSize=(vers==1||entry.valid()) ? 2 : m_mainParser->getZoneDataFieldSize(entry.id());
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+5+dataFieldSize+2+6)) {
    MWAW_DEBUG_MSG(("RagTimeText::readTextZone: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(TextZone):";
  long endPos=entry.end();
  if (!entry.valid()) {
    int dSz=(int) input->readULong(dataFieldSize);
    endPos=pos+dataFieldSize+dSz;
  }
  long begTextZonePos=input->tell();
  int numChar=(int) input->readULong(2);
  f << "N=" << numChar << ",";
  if (!input->checkPosition(endPos) || begTextZonePos+numChar>endPos) {
    MWAW_DEBUG_MSG(("RagTimeText::readTextZone: the numChar seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(endPos);
  ascFile.addNote("_");

  shared_ptr<RagTimeTextInternal::TextZone> zone(new RagTimeTextInternal::TextZone);
  pos = input->tell();
  zone->m_textPos.setBegin(pos);
  zone->m_textPos.setLength(numChar);
  if (vers>=2 && (numChar%2)==1)
    ++numChar;
  input->seek(pos+numChar, librevenge::RVNG_SEEK_SET);

  if (!readFonts(*zone, color, endPos))
    return false;

  if (m_state->m_idTextMap.find(entry.id())!=m_state->m_idTextMap.end()) {
    MWAW_DEBUG_MSG(("RagTimeText::readTextZone: a zone with id=%d already exists\n", entry.id()));
  }
  else
    m_state->m_idTextMap[entry.id()]=zone;
  if (input->tell()==endPos)
    return true;

  if (!readParagraphs(*zone, width, endPos))
    return false;
  pos=input->tell();
  if (vers==1) {
    if (pos!=endPos) {
      MWAW_DEBUG_MSG(("RagTimeText::readTextZone: find some extra data\n"));
      ascFile.addPos(pos);
      ascFile.addNote("TextZone[end]:###");
    }
    return true;
  }
  // checkme: can this size be a uint32 ?
  int dSz=(int) input->readULong(2);
  f.str("");
  f << "TextZone[A]:";
  if (pos+2+dSz>endPos) {
    MWAW_DEBUG_MSG(("RagTimeText::readTextZone: the zoneA size seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  if (dSz==0) {
    ascFile.addPos(pos);
    ascFile.addNote("_");
  }
  else {
    // never seems
    MWAW_DEBUG_MSG(("RagTimeText::readTextZone: find a zoneA zone!!!\n"));
    f << "#";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+2+dSz, librevenge::RVNG_SEEK_SET);
  }

  // now the token?
  if (!readTokens(*zone, endPos))
    return true;

  if (input->tell()!=endPos) {
    MWAW_DEBUG_MSG(("RagTimeText::readTextZone: find extra data\n"));
    ascFile.addPos(pos);
    ascFile.addNote("TextZone[extra]:###");
  }
  return true;
}

bool RagTimeText::readFonts(RagTimeTextInternal::TextZone &zone, MWAWColor const &color, long endPos)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  int const vers=version();
  long pos=input->tell();

  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(TextChar):";
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  int const fSz=vers>=2 ? 10:8;
  if (pos+2+fSz*N>endPos+2+4) {
    MWAW_DEBUG_MSG(("RagTimeText::readFonts: the number of styles seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  for (int i=0; i < N; ++i) {
    pos = input->tell();
    f.str("");
    f << "TextChar-C" << i << ":";
    long textPos=(long) input->readULong(2);
    f << "pos=" << textPos << ",";
    MWAWFont font;
    if (vers <=1)  {
      font.setColor(color);
      int size= (int) input->readULong(1);
      int flag = (int) input->readULong(1);
      uint32_t flags=0;
      if (flag&0x1) flags |= MWAWFont::boldBit;
      if (flag&0x2) flags |= MWAWFont::italicBit;
      if (flag&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
      if (flag&0x8) flags |= MWAWFont::embossBit;
      if (flag&0x10) flags |= MWAWFont::shadowBit;
      if (flag&0x20) font.setDeltaLetterSpacing(-1);
      if (flag&0x40) font.setDeltaLetterSpacing(1);
      if (flag&0x80) font.set(MWAWFont::Script::super100());
      if (size&0x80) {
        font.set(MWAWFont::Script::sub100());
        size&=0x7f;
      }
      font.setSize((float)size);
      font.setFlags(flags);
      font.setId(getFontId((int) input->readULong(2)));
      int val=(int) input->readLong(1);
      if (val) font.setDeltaLetterSpacing(-float(val)/16.0f);
      val=(int) input->readLong(1);
      if (val) font.set(MWAWFont::Script(-float(val),librevenge::RVNG_POINT));
    }
    else {
      int id=(int) input->readULong(2)-1;
      if (id<0 || id>=(int) m_state->m_charPropList.size()) {
        MWAW_DEBUG_MSG(("RagTimeText::readFonts: the character id seems bad\n"));
        f << "###";
      }
      else
        font=m_state->m_charPropList[size_t(id)];
      f << "S" << id << ",";
      int val=(int) input->readLong(1);
      if (val) font.setDeltaLetterSpacing(float(val));
      val=(int) input->readLong(1);
      if (val) f << "f0=" << val << ",";
      val=(int) input->readLong(1);
      if (val) font.set(MWAWFont::Script(-float(val),librevenge::RVNG_POINT));
      val=(int) input->readULong(1);
      switch (val) {
      case 0:
        font.setLanguage("en_US");
        break;
      case 1:
        font.setLanguage("fr_FR");
        break;
      case 2:
        font.setLanguage("en_UK");
        break;
      case 3:
        font.setLanguage("de_DE");
        break;
      case 4:
        font.setLanguage("it_IT");
        break;
      case 5:
        font.setLanguage("nl_NL");
        break;
      case 7:
        font.setLanguage("sv_SE");
        break;
      case 8:
        font.setLanguage("es_ES");
        break;
      case 9:
        font.setLanguage("da_DK");
        break;
      case 10:
        font.setLanguage("pt_PT");
        break;
      case 12:
        font.setLanguage("nb_NO");
        break;
      case 19:
        font.setLanguage("de_CH");
        break;
      case 20:
        font.setLanguage("el_GR");
        break;
      case 24:
        font.setLanguage("tr_TR");
        break;
      case 25:
        font.setLanguage("hr_HR");
        break;
      case 49:
        font.setLanguage("ru_RU");
        break;
      case 0xF4:
        font.setLanguage("nn_NO");
        break;
      default: {
        static bool first = true;
        if (first) {
          first=false;
          MWAW_DEBUG_MSG(("RagTimeText::readFonts: find some unknown language\n"));
        }
        f << "#lang=" << val << ",";
        break;
      }
      }
      val=(int) input->readULong(2)-1;
      MWAWColor col;
      if (val && m_mainParser->getColor(val, col))
        font.setColor(col);
      else if (val)
        f << "#col=" << val << ",";
    }
    zone.m_fontPosList.push_back(textPos);
    zone.m_fontList.push_back(font);
    f << font.getDebugString(m_parserState->m_fontConverter);

    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool RagTimeText::readParagraphs(RagTimeTextInternal::TextZone &zone, int width, long endPos)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  int const vers=version();
  long pos=input->tell();

  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  f << "Entries(TextPara):";
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  int const paraSz=vers>=2 ? 48 : 34;
  if (pos+2+paraSz*N>endPos) {
    MWAW_DEBUG_MSG(("RagTimeText::readParagraphs: the number of paragrphs seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  int maxNumTabs=vers==1 ? 10 : 16;
  for (int i=0; i < N; ++i) {
    pos = input->tell();
    f.str("");
    f << "TextPara-P" << i << ":";
    long textPos=(long) input->readULong(2);
    f << "pos=" << textPos << ",";
    MWAWParagraph para;
    para.m_marginsUnit=librevenge::RVNG_POINT;
    // add a default border to mimick frame distance to text
    double const borderSize=4;
    para.m_margins[1]=borderSize+(double)input->readLong(2);
    para.m_margins[2]=(double)(width-(int)input->readULong(2))-2*borderSize;
    if (*para.m_margins[2]<-borderSize) {
      if (*para.m_margins[2]<-borderSize*2) {
        MWAW_DEBUG_MSG(("RagTimeText::readParagraphs: the right margins seems bad\n"));
        f << "##";
      }
      f << "margins[right]=" << *para.m_margins[2] << ",";
      para.m_margins[2]=0;
    }
    int align=(int) input->readULong(1);
    switch (align) {
    case 0: // left
      break;
    case 1:
      para.m_justify = MWAWParagraph::JustificationCenter;
      break;
    case 2:
      para.m_justify = MWAWParagraph::JustificationRight;
      break;
    case 3: // in pratical, look like basic left justification
      f << "justify,";
      break;
    default:
      MWAW_DEBUG_MSG(("RagTimeText::readParagraphs: find unknown align value\n"));
      f << "###align=" << align << ",";
      break;
    }
    int numTabs=(int) input->readULong(1);
    if (numTabs>maxNumTabs) {
      MWAW_DEBUG_MSG(("RagTimeText::readParagraphs: the number of tabs seems odd\n"));
      f << "###tabs[num]=" << numTabs << ",";
      numTabs=0;
    }
    int interlinePoint=(int) input->readLong(1);
    int interline=(int) input->readULong(1);
    if (interline & 0xF8)
      f << "interline[high]=" << std::hex << (interline & 0xFC) << std::dec << ",";
    interline &= 0x7;
    switch (interline) {
    case 0:
    case 1:
    case 2:
      para.setInterline(1.+interline*0.5, librevenge::RVNG_PERCENT);
      break;
    case 3: // 1line +/- nbPt
      para.setInterline(1.+interlinePoint/12., librevenge::RVNG_PERCENT, MWAWParagraph::AtLeast);
      break;
    case 4:
      para.setInterline(interlinePoint, librevenge::RVNG_POINT);
      break;
    default:
      MWAW_DEBUG_MSG(("RagTimeText::readParagraphs: unknown interline type\n"));
      f << "#interline=" << interline << ",";
      break;
    }

    para.m_margins[0]=(double)input->readLong(2)-*para.m_margins[1];
    for (int j=0; j<numTabs; ++j) {
      int tabPos=(int) input->readLong(2);
      MWAWTabStop tab;
      if (tabPos<0) {
        tab.m_alignment=MWAWTabStop::DECIMAL;
        tabPos*=-1;
      }
      else if (tabPos&0x4000) {
        tab.m_alignment=MWAWTabStop::CENTER;
        tabPos &= 0x1FFF;
      }
      else if (tabPos&0x2000) {
        tab.m_alignment=MWAWTabStop::RIGHT;
        tabPos &= 0x1FFF;
      }
      tab.m_position=double(tabPos)/72.;
      para.m_tabs->push_back(tab);
    }
    input->seek(pos+12+2*maxNumTabs, librevenge::RVNG_SEEK_SET);
    int prev=(int) input->readULong(1);
    int next=(int) input->readULong(1);
    int wh=0;
    if (prev&0x80) {
      wh=2;
      prev&=0x7f;
    }
    if (next&0x80) {
      wh|=1;
      next&=0x7f;
    }
    switch (wh) {
    default:
    case 0: // normal;
      break;
    case 1: // +0.5 interline
    case 2: // +1 interline
      para.m_spacings[1]=(wh-1)*0.5*12./72.;
      break;
    case 3:
      para.m_spacings[1]=prev/72.;
      para.m_spacings[2]=next/72.;
      break;
    }
    if (vers>=2) {
      char tabSep=(char) input->readULong(1);
      if (tabSep!='.') // fixme: we need to update the decimal tab
        f << "tab[sep]=" << tabSep << ",";
      int val=(int) input->readLong(1); // always 0?
      if (val) f << "g0=" << val << ",";
    }
    f << para;
    zone.m_paragraphPosList.push_back(textPos);
    zone.m_paragraphList.push_back(para);

    input->seek(pos+paraSz, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}


bool RagTimeText::readTokens(RagTimeTextInternal::TextZone &zone, long endPos)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  int const vers=version();
  long pos=input->tell();
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(TextToken):";
  int dSz=(int) input->readULong(2);
  if (vers <= 1 || pos+2+dSz>endPos) {
    MWAW_DEBUG_MSG(("RagTimeText::readTokens: the tokens size seems bad (or unexpected version)\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  endPos=pos+2+dSz;
  if (dSz==0) {
    ascFile.addPos(pos);
    ascFile.addNote("_");
    return true;
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  int n=0;
  while (!input->isEnd()) {
    pos=input->tell();
    if (pos>=endPos) break;
    f.str("");
    dSz=(int) input->readULong(2);
    long fEndPos=pos+dSz;
    if (dSz<3 || fEndPos>endPos) {
      MWAW_DEBUG_MSG(("RagTimeText::readTokens: the token zone size seems bad\n"));
      f << "###TextToken";
      input->seek(endPos, librevenge::RVNG_SEEK_SET);
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      return true;
    }
    int val;
    RagTimeTextInternal::Token token;
    if (dSz==4) {
      val=(int) input->readLong(1);
      if (val!=1) f << "f0=" << 1 << ",";
      val=(int) input->readLong(1);
      switch (val) {
      case 0x2c:
        token.m_type=RagTimeTextInternal::Token::Page;
        break;
      case 0x2d:
        token.m_type=RagTimeTextInternal::Token::PageAfter;
        break;
      case 0x2e:
        token.m_type=RagTimeTextInternal::Token::PageCount;
        break;
      default:
        MWAW_DEBUG_MSG(("RagTimeText::readTokens: find unknown field\n"));
        f << "#f1=" << val << ",";
      }
    }
    else if (dSz==6) {
      val=(int) input->readLong(2);
      if (val!=100) f << "f0=" << val << ",";
      int format=(int) input->readLong(2)-1;
      // using default, fixme: use file format type here,
      token.m_type=(format==4 || format==5) ? RagTimeTextInternal::Token::Time :
                   RagTimeTextInternal::Token::Date;
      if (!m_mainParser->getDateTimeFormat(format, token.m_DTFormat))
        f << "#";
      f << "F" << format << ",";
    }
    else if (dSz>14) {
      f << "id?=" << input->readLong(2) << ",";
      token.m_type=RagTimeTextInternal::Token::List;
      token.m_listLevel=0;
      for (int i=0; i< 4; ++i) { // small number
        token.m_listIndices[i]=(int) input->readLong(2);
        if (token.m_listIndices[i])
          token.m_listLevel=i+1;
      }
      int sSz=(int) input->readULong(1);
      if (sSz+13>dSz) {
        MWAW_DEBUG_MSG(("RagTimeText::readTokens: can not find the item format name\n"));
        f << "###";
      }
      else {
        std::string text("");
        for (int i=0; i<sSz; ++i)
          text+=(char) input->readULong(1);
        f << "\"" << text << "\",";
      }
      // in 3.2 the size field seems constant
      if (input->tell()!=fEndPos)
        ascFile.addDelimiter(input->tell(),'|');
    }
    else {
      MWAW_DEBUG_MSG(("RagTimeText::readTokens: can not determine the token type\n"));
      f << "###";
    }
    token.m_extra=f.str();
    zone.m_tokenList.push_back(token);
    f.str("");
    f << "TextToken-" << n++ << ":" << token;
    input->seek(fEndPos, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// send data to the listener
bool RagTimeText::send(int zId, MWAWListenerPtr listener)
{
  if (m_state->m_idTextMap.find(zId)==m_state->m_idTextMap.end() ||
      !m_state->m_idTextMap.find(zId)->second) {
    MWAW_DEBUG_MSG(("RagTimeText::send: can not find the text zone %d\n", zId));
    return false;
  }
  return send(*m_state->m_idTextMap.find(zId)->second, listener);
}

bool RagTimeText::send(RagTimeTextInternal::TextZone const &zone, MWAWListenerPtr listener)
{
  if (!listener)
    listener=m_parserState->getMainListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("RagTimeText::send: can not find the listener\n"));
    return false;
  }
  zone.m_isSent=true;
  MWAWEntry entry=zone.m_textPos;
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("RagTimeText::send: the text zone is empty\n"));
    return false;
  }

  MWAWInputStreamPtr input = m_parserState->m_input;
  int const vers=version();
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  long pos=entry.begin(), lPos=pos;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  size_t actFont=0, numFont=zone.m_fontPosList.size();
  if (numFont>zone.m_fontList.size()) numFont=zone.m_fontList.size();
  size_t actPara=0, numPara=zone.m_paragraphPosList.size();
  if (numPara>zone.m_paragraphList.size()) numPara=zone.m_paragraphList.size();

  f << "TextZone:";
  int actToken=0;
  for (long tPos=0; tPos<entry.length(); ++tPos, ++pos) {
    if (input->isEnd()) {
      MWAW_DEBUG_MSG(("RagTimeText::send: oops, find end of file\n"));
      break;
    }
    if (actPara<numPara && zone.m_paragraphPosList[actPara]==tPos) {
      if (pos!=lPos) {
        ascFile.addPos(lPos);
        ascFile.addNote(f.str().c_str());
        lPos=pos;
        f.str("");
        f << "TextZone:";
      }
      f << "[P" << actPara << "]";
      listener->setParagraph(zone.m_paragraphList[actPara++]);
    }
    if (actFont<numFont && zone.m_fontPosList[actFont]==tPos) {
      if (pos!=lPos) {
        ascFile.addPos(lPos);
        ascFile.addNote(f.str().c_str());
        lPos=pos;
        f.str("");
        f << "TextZone:";
      }
      f << "[C" << actFont << "]";
      listener->setFont(zone.m_fontList[actFont++]);
    }
    unsigned char c = (unsigned char) input->readULong(1);
    switch (c) {
    case 0: // at the beginning of a zone of text: related to section?
      break;
    case 1: {
      if (vers>=2) {
        if (actToken>=int(zone.m_tokenList.size())) {
          MWAW_DEBUG_MSG(("RagTimeText::send: can not find token %d\n", actToken));
          f << "[#token]";
          break;
        }
        RagTimeTextInternal::Token const &token=zone.m_tokenList[size_t(actToken++)];
        f << "[" << token << "]";
        MWAWField field(MWAWField::None);
        if (token.getField(field))
          listener->insertField(field);
        else if (token.m_type==RagTimeTextInternal::Token::PageAfter)
          listener->insertUnicodeString("#P+1#");
        else if (token.m_type==RagTimeTextInternal::Token::List) {
          std::string indices;
          if (token.getIndicesString(indices))
            listener->insertUnicodeString(indices.c_str());
        }
        else {
          MWAW_DEBUG_MSG(("RagTimeText::send: does not know how to send a token\n"));
          f << "##";
        }
        break;
      }
      f << "[date]";
      MWAWField date(MWAWField::Date);
      date.m_DTFormat = "%d/%m/%y";
      listener->insertField(date);
      break;
    }
    case 2: {
      if (vers>=2) {
        MWAW_DEBUG_MSG(("RagTimeText::send:  find unexpected char 2\n"));
        f << "[#2]";
        break;
      }
      f << "[time]";
      MWAWField time(MWAWField::Time);
      time.m_DTFormat="%H:%M";
      listener->insertField(time);
      break;
    }
    case 3:
      if (vers>=2) {
        MWAW_DEBUG_MSG(("RagTimeText::send:  find unexpected char 3\n"));
        f << "[#3]";
        break;
      }
      f << "[page]";
      listener->insertField(MWAWField(MWAWField::PageNumber));
      break;
    case 4:
      if (vers>=2) {
        MWAW_DEBUG_MSG(("RagTimeText::send:  find unexpected char 4\n"));
        f << "[#4]";
        break;
      }
      f << "[page+1]";
      listener->insertUnicodeString("#P+1#");
      break;
    case 5:
      if (vers>=2) {
        MWAW_DEBUG_MSG(("RagTimeText::send:  find unexpected char 5\n"));
        f << "[#5]";
        break;
      }
      f << "[section]";
      listener->insertUnicodeString("#S#");
      break;
    case 6: // ok, must be the end of the zone
      if (vers>=2) {
        MWAW_DEBUG_MSG(("RagTimeText::send:  find unexpected char 6\n"));
        f << "[#6]";
        break;
      }
      f << "[pagebreak]";
      break;
    case 9:
      listener->insertTab();
      f << c;
      break;
    case 0xb:
    case 0xd:
      listener->insertEOL(c==0xb);
      ascFile.addPos(lPos);
      ascFile.addNote(f.str().c_str());
      lPos=pos+1;
      f.str("");
      f << "TextZone:";
      break;
    case 0x1f: // soft hyphen
      break;
    default:
      if (c<=0x1f) {
        MWAW_DEBUG_MSG(("RagTimeText::send:  find an odd char %x\n", int(c)));
        f << "[#" << std::hex << int(c) << std::dec << "]";
        break;
      }
      listener->insertCharacter(c);
      f << c;
      break;
    }

  }

  if (lPos!=entry.end()) {
    ascFile.addPos(lPos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

void RagTimeText::flushExtra()
{
  MWAWListenerPtr listener=m_parserState->getMainListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("RagTimeText::flushExtra: can not find the listener\n"));
    return;
  }
  std::map<int, shared_ptr<RagTimeTextInternal::TextZone> >::const_iterator it;
  for (it=m_state->m_idTextMap.begin(); it!=m_state->m_idTextMap.end(); ++it) {
    if (!it->second) continue;
    RagTimeTextInternal::TextZone const &zone=*it->second;
    if (zone.m_isSent) continue;
    static bool first=true;
    if (first) {
      MWAW_DEBUG_MSG(("RagTimeText::flushExtra: find some unsend zone\n"));
      first=false;
    }
    send(zone, listener);
    listener->insertEOL();
  }
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
