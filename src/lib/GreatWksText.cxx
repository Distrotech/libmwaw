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

#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWDebug.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWGraphicListener.hxx"
#include "MWAWListener.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSection.hxx"
#include "MWAWRSRCParser.hxx"

#include "GreatWksParser.hxx"

#include "GreatWksText.hxx"

/** Internal: the structures of a GreatWksText */
namespace GreatWksTextInternal
{
/** the different plc type */
enum PLCType { P_Font, P_Page,  P_Ruler, P_Token, P_Unknown};

/** Internal : a PLC: used to store change of properties in GreatWksTextInternal::Zone */
struct PLC {
  /// the constructor
  PLC() : m_type(P_Unknown), m_id(-1), m_extra("")
  {
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
  switch (plc.m_type) {
  case P_Font:
    o << "F";
    break;
  case P_Page:
    o << "Pg";
    break;
  case P_Ruler:
    o << "R";
    break;
  case P_Token:
    o << "Tn";
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

////////////////////////////////////////
//! Internal and low level: structure which stores a token for GreatWksText
struct Token {
  //! constructor
  Token() : m_type(-1), m_format(0), m_pictEntry(), m_dataSize(0), m_dim(0,0), m_date(0xFFFFFFFF), m_extra("")
  {
  }
  //! returns a field format
  std::string getDTFormat() const;
  //! try to send the token to the listener
  bool sendTo(MWAWBasicListener &listener) const;
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Token const &tkn);
  //! the token type
  int m_type;
  //! the token format
  int m_format;
  //! the picture entry
  MWAWEntry m_pictEntry;
  //! the picture data size
  long m_dataSize;
  //! the picture dimension
  Vec2f m_dim;
  //! the token date (0xFFFFFFFF means actual date)
  uint32_t m_date;
  //! extra data
  std::string m_extra;
};

bool Token::sendTo(MWAWBasicListener &listener) const
{
  switch (m_type) {
  case 2:
    switch (m_format) {
    case 1:
    case 3: // must be roman
      listener.insertField(MWAWField(MWAWField::PageNumber));
      break;
    case 2:
    case 4: // must be roman
      listener.insertField(MWAWField(MWAWField::PageNumber));
      listener.insertUnicodeString(" of ");
      listener.insertField(MWAWField(MWAWField::PageCount));
      break;
    default:
      listener.insertField(MWAWField(MWAWField::PageNumber));
      break;
    }
    return true;
  case 0x15:
  case 0x16: {
    MWAWField field(m_type==0x15 ? MWAWField::Date : MWAWField::Time);
    field.m_DTFormat=getDTFormat();
    listener.insertField(field);
    return true;
  }
  default:
    break;
  }
  return false;
}

std::string Token::getDTFormat() const
{
  switch (m_type) {
  case 0x15:
    switch (m_format) {
    case 0xa:
      return "%m/%d/%y";
    case 0xb:
      return "%b %d, %Y";
    case 0xc:
      return "%b %Y";
    case 0xd:
      return "%b %d";
    case 0xe:
      return "%B %d, %Y";
    case 0xf:
      return "%B %Y";
    case 0x10:
      return "%B %d";
    case 0x11:
      return "%a, %b %d, %Y";
    case 0x12:
      return "%A, %B %d, %Y";
    default:
      break;
    }
    break;
  case 0x16:
    switch (m_format) {
    case 0x14:
      return "%I:%M %p";
    case 0x15:
      return "%I:%M:%S %p";
    case 0x16:
      return "%I:%M";
    case 0x17:
      return "%I:%M:%S";
    case 0x18:
      return "%H:%M";
    case 0x19:
      return "%H:%M:%S";
    default:
      break;
    }
    break;
  default:
    break;
  }
  return "";
}

std::ostream &operator<<(std::ostream &o, Token const &token)
{
  switch (token.m_type) {
  case 0: // none
    break;
  case 2:
    switch (token.m_format) {
    case 0:
      o << "page,";
      break;
    case 1:
      o << "page/pagecount,";
      break;
    case 2:
      o << "page[roman],";
      break;
    case 3:
      o << "page/pagecount[roman],";
      break;
    default:
      o << "page[#m_format=" << token.m_format << "],";
      break;
    }
    break;
  case 4:
    o << "pict,dim="<< token.m_dim << ",sz=" << std::hex << token.m_dataSize << std::dec << ",";
    break;
  case 0x15:
  case 0x16: {
    o << (token.m_type==0x15 ? "date" : "time");
    std::string format=token.getDTFormat();
    if (!format.empty())
      o << "[" << format << "]";
    else
      o << "[#format=" << token.m_format << "]";
    if (token.m_date != 0xFFFFFFFF)
      o << ":val=" << std::hex << token.m_date << std::dec;
    o << ",";
    break;
  }
  default:
    MWAW_DEBUG_MSG(("GreatWksTextInternal::Token: unknown type=%d\n", token.m_type));
    o << "#type=" << token.m_type << ",";
    if (token.m_format)
      o << "#format=" << token.m_format << ",";
  }
  o << token.m_extra;
  return o;
}
////////////////////////////////////////
//! Internal and low level: structure which stores a text position for GreatWksText
struct Frame {
  //! constructor
  Frame() : m_pos(), m_page(0), m_extra("")
  {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Frame const &frm)
  {
    o << "dim=" << frm.m_pos << ",";
    if (frm.m_page) o << "page=" << frm.m_page << ",";
    o << frm.m_extra;
    return o;
  }

  //! the frame position
  Box2f m_pos;
  //! the page
  int m_page;
  //! extra data
  std::string m_extra;
};
////////////////////////////////////////
//! Internal and low level: structure which stores a text zone header for GreatWksText
struct Zone {
  //! constructor
  Zone(): m_type(-1), m_numFonts(0), m_numRulers(0), m_numLines(0),
    m_numTokens(0), m_numChar(0), m_numCharPLC(0), m_numFrames(0),
    m_fontList(), m_rulerList(), m_tokenList(), m_frameList(),
    m_textEntry(), m_posPLCMap(), m_parsed(false), m_extra("")
  {
  }
  //! returns true if this is the main zone
  bool isMain() const
  {
    return m_type==3;
  }
  //! check if the data read are or not ok
  bool ok() const
  {
    if (m_type<0 || m_type > 5)
      return false;
    if (m_numFonts<=0 || m_numRulers<=0 || m_numLines<0 || m_numTokens<=0 || m_numChar<0 ||
        m_numCharPLC<=0 || m_numFrames<0)
      return false;
    return true;
  }
  //! returns the data size
  long size() const
  {
    return 22*m_numFonts+192*m_numRulers+6*m_numCharPLC
           +18*m_numTokens+14*m_numLines+22*m_numFrames+m_numChar;
  }
  //! returns true if the data has graphic
  bool hasGraphics() const
  {
    for (size_t t=0; t < m_tokenList.size(); ++t) {
      if (m_tokenList[t].m_type==4)
        return true;
    }
    return false;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Zone const &fr)
  {
    switch (fr.m_type) {
    case 1: // in draw=textbox
      o << "header/footer,";
      break;
    case 2:
      o << "textbox,";
      break;
    case 3:
      o << "main,";
      break;
    default:
      o << "#type=" << fr.m_type << ",";
      break;
    }
    if (fr.m_numFonts)
      o << "nFonts=" << fr.m_numFonts << ",";
    if (fr.m_numRulers)
      o << "nRulers=" << fr.m_numRulers << ",";
    if (fr.m_numChar)
      o << "length[text]=" << fr.m_numChar << ",";
    if (fr.m_numCharPLC)
      o << "char[plc]=" << fr.m_numCharPLC << ",";
    if (fr.m_numLines)
      o << "nLines=" << fr.m_numLines << ",";
    if (fr.m_numTokens)
      o << "nTokens=" << fr.m_numTokens << ",";
    if (fr.m_numFrames)
      o << "nFrames=" << fr.m_numFrames << ",";
    o << fr.m_extra;
    return o;
  }
  //! the main type: 1=auxi, 3=main
  int m_type;
  //! the number of fonts
  int m_numFonts;
  //! the number of rulers
  int m_numRulers;
  //! the number of lines
  int m_numLines;
  //! the number of token
  int m_numTokens;
  //! the number of character
  long m_numChar;
  //! the number of char plc
  int m_numCharPLC;
  //! the number of frames (ie. one by column and one by pages )
  int m_numFrames;
  //! the list of font
  std::vector<MWAWFont> m_fontList;
  //! the list of ruler
  std::vector<MWAWParagraph> m_rulerList;
  //! the list of token
  std::vector<Token> m_tokenList;
  //! the list of frame token
  std::vector<Frame> m_frameList;
  //! the text entry list
  MWAWEntry m_textEntry;
  //! a map text pos -> PLC
  std::multimap<long,PLC> m_posPLCMap;
  //! a bool to know if the data are send to the listener
  mutable bool m_parsed;
  //! extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the state of a GreatWksText
struct State {
  //! constructor
  State() : m_fileIdFontIdMap(), m_zonesList(), m_version(-1), m_numPages(-1)
  {
  }
  //! returns the font id corresponding to a fId
  int getFId(int fileId) const
  {
    if (m_fileIdFontIdMap.find(fileId)==m_fileIdFontIdMap.end())
      return fileId;
    return m_fileIdFontIdMap.find(fileId)->second;
  }
  //! a global map file font id-> fontconverter id
  std::map<int,int> m_fileIdFontIdMap;
  //! a list of text zone
  std::vector<Zone> m_zonesList;
  //! the file version
  mutable int m_version;
  int m_numPages /* the number of pages */;
};
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
GreatWksText::GreatWksText(MWAWParser &parser) :
  m_parserState(parser.getParserState()), m_state(new GreatWksTextInternal::State),
  m_mainParser(&parser), m_callback()
{
}

GreatWksText::~GreatWksText()
{ }

int GreatWksText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}

int GreatWksText::numPages() const
{
  if (m_state->m_numPages >= 0)
    return m_state->m_numPages;

  int nPages=1;
  for (size_t i=0; i < m_state->m_zonesList.size(); ++i) {
    GreatWksTextInternal::Zone const &zone=m_state->m_zonesList[i];
    if (!zone.isMain() || zone.m_frameList.empty())
      continue;
    if (zone.m_frameList.back().m_page>1)
      nPages = zone.m_frameList.back().m_page;
    break;
  }

  return m_state->m_numPages = nPages;
}

int GreatWksText::getFontId(int fileId) const
{
  return m_state->getFId(fileId);
}

int GreatWksText::numHFZones() const
{
  int nHF=0;
  for (size_t i=0; i < m_state->m_zonesList.size(); ++i) {
    GreatWksTextInternal::Zone const &zone=m_state->m_zonesList[i];
    if (zone.isMain())
      break;
    nHF++;
  }
  return nHF;
}

bool GreatWksText::canSendTextBoxAsGraphic(MWAWEntry const &entry)
{
  if (!entry.valid())
    return false;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  GreatWksTextInternal::Zone zone;
  bool ok=!readZone(zone) ||!zone.hasGraphics();
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  return ok;
}

bool GreatWksText::sendTextbox(MWAWEntry const &entry, bool inGraphic)
{
  if (!m_parserState->getMainListener()) {
    MWAW_DEBUG_MSG(("GreatWksText::sendTextbox: can not find a listener\n"));
    return false;
  }
  if (!entry.valid())
    return false;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  GreatWksTextInternal::Zone zone;
  if (readZone(zone)) {
    sendZone(zone, inGraphic);
    return true;
  }

  return sendSimpleTextbox(entry, inGraphic);
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////
bool GreatWksText::createZones(int expectedHF)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  long pos = input->tell();
  f << "Entries(TZoneHeader):";
  int val=(int) input->readULong(2);
  if (val)
    f << "numPages=" << val << ",";
  val=(int) input->readULong(2);
  if (val) // related to number of header/footer?
    f << "f0=" << val << ",";
  f << "height[total]=" << input->readLong(4) << ","; // checkme
  ascFile.addDelimiter(input->tell(),'|');
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  pos += 68;
  input->seek(pos, librevenge::RVNG_SEEK_SET);

  if (!readFontNames()) {
    MWAW_DEBUG_MSG(("GreatWksText::createZones: can not find the font names\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
  }

  bool findMainZone=false;
  int nAuxi=0;
  while (!input->isEnd()) {
    pos=input->tell();
    GreatWksTextInternal::Zone zone;
    if (!readZone(zone)) {
      input->seek(pos,librevenge::RVNG_SEEK_SET);
      if (findMainZone)
        break;

      if (!findNextZone() || !readZone(zone)) {
        input->seek(pos,librevenge::RVNG_SEEK_SET);
        break;
      }
    }
    m_state->m_zonesList.push_back(zone);
    if (zone.isMain())
      findMainZone=true;
    else
      nAuxi++;
  }
  if (nAuxi!=expectedHF) {
    MWAW_DEBUG_MSG(("GreatWksText::createZones: unexpected HF zones: %d/%d\n", nAuxi, expectedHF));
  }
  return findMainZone;
}

bool GreatWksText::findNextZone()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;

  long searchPos=input->tell(), pos=searchPos;
  int const headerSize=24+22+184;
  if (!input->checkPosition(pos+headerSize))
    return false;

  // first look for ruler
  input->seek(pos+headerSize, librevenge::RVNG_SEEK_SET);
  while (true) {
    if (input->isEnd())
      return false;
    pos = input->tell();
    unsigned long val=input->readULong(4);
    if (val==0x20FFFF)
      input->seek(pos, librevenge::RVNG_SEEK_SET);
    else if (val==0x20FFFFFF)
      input->seek(pos-1, librevenge::RVNG_SEEK_SET);
    else if (val==0xFFFFFFFF)
      input->seek(pos-2, librevenge::RVNG_SEEK_SET);
    else if (val==0xFFFFFF2E)
      input->seek(pos-3, librevenge::RVNG_SEEK_SET);
    else
      continue;
    if (input->readULong(4)!=0x20FFFF || input->readULong(4)!=0xFFFF2E00) {
      input->seek(pos+4, librevenge::RVNG_SEEK_SET);
      continue;
    }
    // ok a empty tabs stop
    while (!input->isEnd()) {
      pos = input->tell();
      if (input->readULong(4)!=0x20FFFF || input->readULong(4)!=0xFFFF2E00) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        break;
      }
    }
    break;
  }

  pos=input->tell();
  int nFonts=0;
  GreatWksTextInternal::Zone zone;
  while (true) {
    long hSize=headerSize+22*nFonts++;
    if (pos-hSize < searchPos)
      break;
    input->seek(pos-hSize, librevenge::RVNG_SEEK_SET);
    if (input->readLong(4))
      continue;
    int val=(int)input->readULong(2);
    if (val & 0xFEFE)
      continue;
    input->seek(2,librevenge::RVNG_SEEK_CUR);
    if (input->readLong(2)!=nFonts)
      continue;
    input->seek(pos-hSize, librevenge::RVNG_SEEK_SET);
    if (readZone(zone)) {
      input->seek(pos-hSize, librevenge::RVNG_SEEK_SET);
      return true;
    }
  }

  MWAW_DEBUG_MSG(("GreatWksText::findNextZone: can not find begin of zone for pos=%lx\n", pos));
  input->seek(searchPos, librevenge::RVNG_SEEK_SET);
  return false;
}

bool GreatWksText::readZone(GreatWksTextInternal::Zone &zone)
{
  zone=GreatWksTextInternal::Zone();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  long endPos=pos+24;
  if (!input->checkPosition(endPos))
    return false;

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  if (input->readLong(4)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  for (int i=0; i < 2; ++i) {
    int val=(int) input->readLong(1);
    if (val==0) continue;
    if (val!=1)
      return false;
    if (i==0)
      f << "smartquote,";
    else
      f << "hidepict,";
  }
  zone.m_type=(int) input->readLong(1);
  if (input->readLong(1)) { // simple|complex field
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  zone.m_numFonts=(int) input->readULong(2);
  zone.m_numRulers=(int) input->readULong(2);
  zone.m_numCharPLC=(int) input->readULong(2);
  zone.m_numTokens=(int) input->readULong(2);
  zone.m_numLines=(int) input->readULong(2);
  zone.m_numFrames=(int) input->readULong(2);
  zone.m_numChar=(long) input->readULong(4);
  if (!zone.ok() || !input->checkPosition(endPos+zone.size())) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  zone.m_extra=f.str();
  f.str("");
  f << "Entries(HeaderText):" << zone;
  if (input->tell()!=endPos) {
    ascFile.addDelimiter(input->tell(),'|');
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  for (int i=0; i<zone.m_numFonts; ++i) {
    pos = input->tell();
    f.str("");
    f << "Entries(FontDef)-F" << i << ":";

    MWAWFont font;
    if (!readFont(font)) {
      f << "###";
      font = MWAWFont();
      input->seek(pos+22, librevenge::RVNG_SEEK_SET);
    }
    zone.m_fontList.push_back(font);
    f << font.getDebugString(m_parserState->m_fontConverter) << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  MWAWParagraph para;
  for (int i=0; i<zone.m_numRulers; ++i) {
    pos = input->tell();
    f.str("");
    f << "Entries(RulerDef)-R" << i << ":";
    if (!readRuler(para)) {
      f << "###";
      para = MWAWParagraph();
      input->seek(pos+192, librevenge::RVNG_SEEK_SET);
    }
    zone.m_rulerList.push_back(para);
    f << para;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  pos=input->tell();
  f.str("");
  f << "Entries(CharPLC):";
  GreatWksTextInternal::PLC plc;
  plc.m_type = GreatWksTextInternal::P_Font;
  long textPos = 0;
  for (int i=0; i < zone.m_numCharPLC; ++i) {
    plc.m_id=(int) input->readULong(2);
    zone.m_posPLCMap.insert
    (std::multimap<long,GreatWksTextInternal::PLC>::value_type(textPos, plc));
    f << "F" << plc.m_id << ":" << std::hex << textPos << std::dec << ",";
    textPos+= (int) input->readULong(4);
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  // the token
  plc.m_type = GreatWksTextInternal::P_Token;
  textPos = 0;
  for (int i=0; i < zone.m_numTokens; ++i) {
    pos = input->tell();
    f.str("");
    plc.m_id=i;
    f << "Entries(Token)-Tn" << i << ":";
    GreatWksTextInternal::Token tkn;
    long numC;
    if (!readToken(tkn,numC)) {
      f << "###";
      tkn = GreatWksTextInternal::Token();
      input->seek(pos+2, librevenge::RVNG_SEEK_SET);
      numC=input->readLong(2);
    }
    zone.m_posPLCMap.insert
    (std::multimap<long,GreatWksTextInternal::PLC>::value_type(textPos, plc));
    zone.m_tokenList.push_back(tkn);
    f << tkn << ",pos=" << std::hex << textPos << std::dec;
    textPos+= numC;
    input->seek(pos+18, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  if (!readZonePositions(zone))
    return false;
  // fill the text entry
  pos=input->tell();
  zone.m_textEntry.setBegin(pos);
  zone.m_textEntry.setLength(zone.m_numChar);
  ascFile.addPos(pos);
  ascFile.addNote("_");
  pos += zone.m_numChar;
  ascFile.addPos(pos);
  ascFile.addNote("_");

  for (size_t i=0; i < zone.m_tokenList.size(); ++i) {
    GreatWksTextInternal::Token &tkn=zone.m_tokenList[i];
    if (tkn.m_type != 4)
      continue;
    if (tkn.m_dataSize <= 0 || !input->checkPosition(pos+tkn.m_dataSize)) {
      MWAW_DEBUG_MSG(("GreatWksText::readZone: can not determine the picture size\n"));
      break;
    }
    tkn.m_pictEntry.setBegin(pos);
    tkn.m_pictEntry.setLength(tkn.m_dataSize);
    pos+=tkn.m_dataSize;
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  return true;
}

bool GreatWksText::readZonePositions(GreatWksTextInternal::Zone &zone)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  // the line
  GreatWksTextInternal::PLC plc;
  plc.m_type = GreatWksTextInternal::P_Ruler;
  long textPos = 0;
  std::vector<long> linesPos;
  linesPos.push_back(0);
  for (int i=0; i < zone.m_numLines; ++i) {
    pos=input->tell();
    f.str("");
    plc.m_id=(int) input->readULong(2);
    long numC = (long) input->readULong(4);
    f << "y=" << double(input->readLong(4))/65536.;
    f << "->" << double(input->readLong(4))/65536.;
    plc.m_extra=f.str();
    zone.m_posPLCMap.insert
    (std::multimap<long,GreatWksTextInternal::PLC>::value_type(textPos, plc));
    f.str("");
    f << "Entries(Line)-L" << i << ":" << plc <<  ":"
      << std::hex << textPos << std::dec;
    textPos+=numC;
    linesPos.push_back(textPos);
    input->seek(pos+14, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  // the page dimension
  plc.m_type = GreatWksTextInternal::P_Page;
  for (int i=0; i < zone.m_numFrames; ++i) {
    GreatWksTextInternal::Frame frame;
    pos=input->tell();
    plc.m_id=i;
    f.str("");
    float dim[4];
    for (int j=0; j<4; ++j)
      dim[j]=float(input->readLong(4))/65536.f;
    frame.m_pos=Box2f(Vec2f(dim[1],dim[0]),Vec2f(dim[3],dim[2]));
    int val=(int) input->readLong(2); // always 0
    if (val) f << "#unkn=" << val << ",";
    frame.m_page=(int) input->readLong(2);
    int line=(int) input->readLong(2);
    plc.m_extra=f.str();
    if (line >= 0 && line < int(linesPos.size())) {
      textPos=linesPos[size_t(line)];
      zone.m_posPLCMap.insert
      (std::multimap<long,GreatWksTextInternal::PLC>::value_type(textPos, plc));
      if (textPos)
        f << "pos=" << std::hex << textPos << std::dec;
    }
    else {
      MWAW_DEBUG_MSG(("GreatWksText::readZone: can not find begin pos for page %d\n",line));
      f << "##pos[line]=" << line << ",";
    }
    frame.m_extra=f.str();
    zone.m_frameList.push_back(frame);
    f.str("");
    f << "Entries(TFrames)-" << i << ":" << frame;
    input->seek(pos+22, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

//////////////////////////////////////////////
// Fonts
//////////////////////////////////////////////
bool GreatWksText::readFontNames()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(FontNames):";
  long sz= (long) input->readULong(4);
  long endPos = input->tell()+sz;
  if (sz < 2 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("GreatWksText::readFontNames: can not read field size\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  int N=(int) input->readLong(2);
  f << "N=" << N << ",";
  if (N*5+2 > sz) {
    MWAW_DEBUG_MSG(("GreatWksText::readFontNames: can not read N\n"));
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
    f << "FontNames-" << i << ":";
    int fId=(int) input->readULong(2);
    f << "fId=" << fId << ",";
    int val=(int) input->readLong(2); // always 0 ?
    if (val)
      f << "unkn=" << val << ",";
    int fSz=(int) input->readULong(1);
    if (pos+5+fSz>endPos) {
      MWAW_DEBUG_MSG(("GreatWksText::readFontNames: can not read font %d\n", i));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      input->seek(endPos, librevenge::RVNG_SEEK_SET);
      return i>0;
    }
    std::string name("");
    for (int c=0; c < fSz; ++c)
      name+=(char) input->readULong(1);
    if (!name.empty())
      m_state->m_fileIdFontIdMap[fId]=m_parserState->m_fontConverter->getId(name);
    if ((fSz%2)==0)
      input->seek(1, librevenge::RVNG_SEEK_CUR);

    f << "\"" << name << "\",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  pos = input->tell();
  if (pos!=endPos) {
    MWAW_DEBUG_MSG(("GreatWksText::readFontNames: find extra data\n"));
    ascFile.addPos(pos);
    ascFile.addNote("FontNames:###");
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

bool GreatWksText::readFont(MWAWFont &font)
{
  font=MWAWFont();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  long endPos=pos+22;
  if (!input->checkPosition(endPos))
    return false;

  libmwaw::DebugStream f;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int val=(int) input->readLong(2); // small number between 0 and 5
  if (val==0) f << "unused,";
  else if (val!=1) f << "nbUsed=" << val << ",";
  // some dim dim[0]>dim[1]: minH, maxH ?
  int dim[2];
  for (int i=0; i < 2; ++i)
    dim[i]=(int) input->readLong(2);
  f << "dim?=" << dim[1] << "x" << dim[0] << ",";

  for (int i=0; i < 2; ++i) { // f0=0|4(with italic), f1=0
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  font.setId(m_state->getFId((int) input->readULong(2)));
  int flag =(int) input->readULong(2);
  uint32_t flags=0;
  if (flag&0x1) flags |= MWAWFont::boldBit;
  if (flag&0x2) flags |= MWAWFont::italicBit;
  if (flag&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
  if (flag&0x8) flags |= MWAWFont::embossBit;
  if (flag&0x10) flags |= MWAWFont::shadowBit;
  if (flag&0x20) font.setDeltaLetterSpacing(-1);
  if (flag&0x40) font.setDeltaLetterSpacing(1);
  if (flag&0x100) font.set(MWAWFont::Script::super100());
  if (flag&0x200) font.set(MWAWFont::Script::sub100());
  if (flag&0x800) font.setStrikeOutStyle(MWAWFont::Line::Simple);
  if (flag&0x2000) {
    font.setUnderlineStyle(MWAWFont::Line::Simple);
    font.setUnderlineType(MWAWFont::Line::Double);
  }
  flag &=0xD480;
  if (flag) f << "#fl=" << std::hex << flag << std::dec << ",";
  font.setFlags(flags);
  font.setSize((float) input->readULong(2));
  unsigned char color[3];
  for (int c=0; c<3; ++c)
    color[c] = (unsigned char)(input->readULong(2)>>8);
  font.setColor(MWAWColor(color[0],color[1],color[2]));
  font.m_extra=f.str();

  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}
//////////////////////////////////////////////
// Ruler
//////////////////////////////////////////////
bool GreatWksText::readRuler(MWAWParagraph &para)
{
  para = MWAWParagraph();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  long endPos=pos+192;
  if (!input->checkPosition(endPos))
    return false;

  libmwaw::DebugStream f;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int val=(int) input->readLong(2);
  if (val==0) f << "unused,";
  else if (val!=1) f << "nbUsed=" << val << ",";
  val=(int) input->readLong(2);
  switch (val) {
  case 0:
    break;
  case 1:
    para.m_justify = MWAWParagraph::JustificationCenter;
    break;
  case 2:
    para.m_justify = MWAWParagraph::JustificationRight ;
    break;
  case 3:
    para.m_justify = MWAWParagraph::JustificationFull;
    break;
  default:
    f << "#align" << val << ",";
    break;
  }
  para.m_marginsUnit=librevenge::RVNG_POINT;
  for (int i=0; i < 3; ++i)
    para.m_margins[i] = double(input->readLong(4))/65536.;
  para.m_margins[0]=*para.m_margins[0]-*para.m_margins[1];
  double spacing[3];
  for (int i=0; i < 3; ++i)
    spacing[i] = double(input->readLong(4))/65536.;
  int unit[3];
  for (int i=0; i < 3; ++i)
    unit[i] = (int) input->readLong(1);
  switch (unit[0]) {
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
    para.setInterline(spacing[0], librevenge::RVNG_POINT);
    break;
  case 6:
    para.setInterline(spacing[0], librevenge::RVNG_PERCENT);
    break;
  default:
    f << "#interline=" << spacing[0] << "[unit=" << unit[0] << "],";
  }
  for (int w=1; w < 3; ++w) {
    if (unit[w]==6)
      para.m_spacings[w]=spacing[w]*12./72.;
    else if (unit[w]>0 && unit[w]<6)
      para.m_spacings[w]=spacing[w]/72.;
    else
      f << "#spac" << w << "=" << spacing[w] << "[unit=" << unit[w] << "],";
  }
  val = (int) input->readLong(1);
  if (val) f << "#f0=" << val << ",";

  for (int i=0; i < 20; ++i) {
    MWAWTabStop tab;
    val = (int) input->readLong(1);
    switch (val) {
    case 0: // left
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
    default:
      f << "#tab" << i << "[align]=" << val << ",";
      break;
    }
    char leaderChar = (char) input->readULong(1);
    if (leaderChar) {
      int unicode= m_parserState->m_fontConverter->unicode(3, (unsigned char) leaderChar);
      if (unicode==-1)
        tab.m_leaderCharacter = uint16_t(leaderChar);
      else
        tab.m_leaderCharacter = uint16_t(unicode);
    }
    long tPos=input->readLong(4);
    if (tPos==-1) {
      input->seek(2, librevenge::RVNG_SEEK_CUR);
      continue;
    }
    tab.m_position=double(tPos)/72./65536.;
    char decimalChar = (char) input->readULong(1);
    if (decimalChar) {
      int unicode= m_parserState->m_fontConverter->unicode(3, (unsigned char) decimalChar);
      if (unicode==-1)
        tab.m_decimalCharacter = uint16_t(decimalChar);
      else
        tab.m_decimalCharacter = uint16_t(unicode);
    }
    val = (int) input->readLong(1);
    if (val) f << "#tab" << i << "[f0=" << val << ",";
    para.m_tabs->push_back(tab);
  }
  para.m_extra=f.str();

  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

//////////////////////////////////////////////
// Token
//////////////////////////////////////////////
bool GreatWksText::readToken(GreatWksTextInternal::Token &token, long &nChar)
{
  token = GreatWksTextInternal::Token();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  long endPos=pos+18;
  if (!input->checkPosition(endPos))
    return false;

  libmwaw::DebugStream f;
  token.m_type=(int) input->readULong(1);
  token.m_format=(int) input->readULong(1);
  nChar=input->readLong(4);
  if (token.m_type==0x15||token.m_type==0x16)
    token.m_date=(uint32_t) input->readULong(4);
  else if (token.m_type==4) {
    token.m_dataSize = input->readLong(4);
    float dim[2];
    for (int i=0; i < 2; i++)
      dim[i] = float(input->readLong(4))/65536.f;
    token.m_dim=Vec2f(dim[0],dim[1]); // checkme
  }
  int nUnread=int(endPos-input->tell())/2;
  for (int i=0; i < nUnread; i++) {
    int val=(int)input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  token.m_extra=f.str();

  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

//////////////////////////////////////////////
// send data
//////////////////////////////////////////////
bool GreatWksText::sendMainText()
{
  for (size_t i=0; i < m_state->m_zonesList.size(); ++i) {
    GreatWksTextInternal::Zone const &zone=m_state->m_zonesList[i];
    if (!zone.isMain())
      continue;
    return sendZone(zone);
  }
  return false;
}

bool GreatWksText::sendHF(int id)
{
  for (size_t i=0; i < m_state->m_zonesList.size(); ++i) {
    GreatWksTextInternal::Zone const &zone=m_state->m_zonesList[i];
    if (zone.isMain())
      continue;
    if (id--==0)
      return sendZone(zone);
  }
  MWAW_DEBUG_MSG(("GreatWksText::sendHF: can not find a header/footer\n"));
  return false;
}

void GreatWksText::flushExtra()
{
  MWAWListenerPtr listener=m_parserState->getMainListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("GreatWksText::flushExtra: can not find a listener\n"));
    return;
  }
  for (size_t i=0; i < m_state->m_zonesList.size(); ++i) {
    GreatWksTextInternal::Zone const &zone=m_state->m_zonesList[i];
    if (zone.m_parsed)
      continue;
    sendZone(zone);
  }
}

bool GreatWksText::sendSimpleTextbox(MWAWEntry const &entry, bool inGraphic)
{
  MWAWBasicListenerPtr listener;
  if (inGraphic)
    listener=m_parserState->m_graphicListener;
  else
    listener=m_parserState->getMainListener();
  if (!listener || !listener->canWriteText()) {
    MWAW_DEBUG_MSG(("GreatWksText::sendSimpleTextbox: can not find a listener\n"));
    return false;
  }
  if (entry.length()<51) {
    MWAW_DEBUG_MSG(("GreatWksText::sendSimpleTextbox: the entry seems to short\n"));
    return false;
  }
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=entry.begin();
  input->seek(pos, librevenge::RVNG_SEEK_SET);

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(Texbox):";
  bool ok = input->readLong(4)==0;
  for (int i=0; ok && i < 2; ++i) {
    int val=(int) input->readLong(1);
    if (val==0) continue;
    if (val!=1) {
      f << "#fl" << i << "=" << val << ",";
      ok = false;
      break;
    }
    if (i==0)
      f << "smartquote,";
    else
      f << "hidepict,";
  }
  int type=(int) input->readLong(1);
  if (ok) {
    if (type<0 || type>5) {
      f << "#type=" << type << ",";
      ok = false;
    }
    else  if (type==1)
      f << "textbox[draw],";
    else if (type!=2)
      f << "type=" << type << ",";
  }
  if (ok) ok=input->readLong(1)==1;

  if (!ok) {
    MWAW_DEBUG_MSG(("GreatWksText::sendSimpleTextbox: the header seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  MWAWFont font;
  font.setId(m_state->getFId((int) input->readULong(2)));
  int flag =(int) input->readULong(2);
  uint32_t flags=0;
  if (flag&0x1) flags |= MWAWFont::boldBit;
  if (flag&0x2) flags |= MWAWFont::italicBit;
  if (flag&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
  if (flag&0x8) flags |= MWAWFont::embossBit;
  if (flag&0x10) flags |= MWAWFont::shadowBit;
  if (flag&0x20) font.setDeltaLetterSpacing(-1);
  if (flag&0x40) font.setDeltaLetterSpacing(1);
  if (flag&0x100) font.set(MWAWFont::Script::super100());
  if (flag&0x200) font.set(MWAWFont::Script::sub100());
  if (flag&0x800) font.setStrikeOutStyle(MWAWFont::Line::Simple);
  if (flag&0x2000) {
    font.setUnderlineStyle(MWAWFont::Line::Simple);
    font.setUnderlineType(MWAWFont::Line::Double);
  }
  flag &=0xD480;
  if (flag) f << "font[#fl]=" << std::hex << flag << std::dec << ",";
  font.setFlags(flags);
  font.setSize((float) input->readULong(2));
  unsigned char color[3];
  for (int c=0; c<3; ++c)
    color[c] = (unsigned char)(input->readULong(2)>>8);
  font.setColor(MWAWColor(color[0],color[1],color[2]));
  f << "font=[" << font.getDebugString(m_parserState->m_fontConverter) << "],";
  listener->setFont(font);

  int val;
  f << "unkn=[" << std::hex;
  for (int i=0; i<6; i++) { // junk ?
    val=(int) input->readULong(2);
    if (val)
      f << val << ",";
    else
      f << "_,";
  }
  f << std::dec << "],";
  MWAWParagraph para;
  val=(int) input->readLong(2);
  switch (val) {
  case 0:
    break;
  case 1:
    para.m_justify = MWAWParagraph::JustificationCenter;
    break;
  case 2:
    para.m_justify = MWAWParagraph::JustificationRight ;
    break;
  case 3:
    para.m_justify = MWAWParagraph::JustificationFull;
    break;
  default:
    f << "#align" << val << ",";
    break;
  }
  f << para <<",";
  listener->setParagraph(para);
  double dim[4];
  for (int i=0; i<4; ++i)
    dim[i]=double(input->readLong(4))/65536.;
  f << "dim=" << dim[1] << "x" << dim[0] << "<->" << dim[3] << "x" << dim[2] << "," ;
  int fSz=(int) input->readULong(1);
  if (50+fSz > entry.length()) {
    MWAW_DEBUG_MSG(("GreatWksText::sendSimpleTextbox: the text size seems too big\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "Entries(Text):";
  for (int i=0; i < fSz; ++i) {
    char c = (char) input->readULong(1);
    f << c;
    switch (c) {
    case 0x9:
      listener->insertTab();
      break;
    case 0xd:
      listener->insertEOL();
      break;
    default:
      listener->insertCharacter((unsigned char) c);
      break;
    }
  }
  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool GreatWksText::sendZone(GreatWksTextInternal::Zone const &zone, bool inGraphic)
{
  MWAWBasicListenerPtr listener;
  if (inGraphic)
    listener=m_parserState->m_graphicListener;
  else
    listener=m_parserState->getMainListener();
  if (!listener || !listener->canWriteText()) {
    MWAW_DEBUG_MSG(("GreatWksText::sendZone: can not find a listener\n"));
    return false;
  }
  bool isMain = zone.isMain();
  int actPage = 1, actCol = 0, numCol=1;
  if (isMain && !listener->canOpenSectionAddBreak()) {
    MWAW_DEBUG_MSG(("GreatWksText::sendZone: in a main zone, but can not open section\n"));
    isMain = false;
  }
  else if (isMain) {
    if (m_callback.m_newPage)
      (m_mainParser->*m_callback.m_newPage)(1);
    MWAWSection sec;
    if (m_callback.m_mainSection)
      sec=(m_mainParser->*m_callback.m_mainSection)();
    numCol = sec.numColumns();
    if (numCol>1) {
      if (listener->isSectionOpened())
        listener->closeSection();
      listener->openSection(sec);
    }
  }
  if (!zone.m_textEntry.valid())
    return true;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(Text):";
  zone.m_parsed=true;
  long pos = zone.m_textEntry.begin(), endPos = zone.m_textEntry.end();
  input->seek(pos, librevenge::RVNG_SEEK_SET);

  long cPos=0;
  while (1) {
    long actPos = input->tell();
    bool done = input->isEnd() || actPos==endPos;

    char c = done ? (char) 0 : (char) input->readULong(1);
    if (pos!=actPos && (c==0xd || done)) {
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      f.str("");
      f << "Text:";
      pos = actPos+1;
    }
    if (done) break;

    std::multimap<long, GreatWksTextInternal::PLC>::const_iterator pIt=
      zone.m_posPLCMap.find(cPos);
    GreatWksTextInternal::Token token;
    while (pIt!=zone.m_posPLCMap.end() && pIt->first==cPos) {
      GreatWksTextInternal::PLC const &plc=(pIt++)->second;
      f << "[" << plc << "]";
      switch (plc.m_type) {
      case GreatWksTextInternal::P_Font:
        if (plc.m_id < 0 || plc.m_id >= (int) zone.m_fontList.size()) {
          MWAW_DEBUG_MSG(("GreatWksText::sendZone: can not find font %d\n", plc.m_id));
          break;
        }
        listener->setFont(zone.m_fontList[size_t(plc.m_id)]);
        break;
      case GreatWksTextInternal::P_Ruler:
        if (plc.m_id < 0 || plc.m_id >= (int) zone.m_rulerList.size()) {
          MWAW_DEBUG_MSG(("GreatWksText::sendZone: can not find ruler %d\n", plc.m_id));
          break;
        }
        listener->setParagraph(zone.m_rulerList[size_t(plc.m_id)]);
        break;
      case GreatWksTextInternal::P_Token:
        if (plc.m_id < 0 || plc.m_id >= (int) zone.m_tokenList.size()) {
          MWAW_DEBUG_MSG(("GreatWksText::sendZone: can not find token %d\n", plc.m_id));
          break;
        }
        token=zone.m_tokenList[size_t(plc.m_id)];
        break;
      case GreatWksTextInternal::P_Page:
      case GreatWksTextInternal::P_Unknown:
      default:
        break;
      }
    }
    if (c!=0xd) f << c;
    switch (c) {
    case 0x4: {
      f << "[pict]";
      if (token.m_type!=4 || !token.m_pictEntry.valid()) {
        MWAW_DEBUG_MSG(("GreatWksText::sendZone: can not find a picture\n"));
        f << "###";
        break;
      }
      if (inGraphic) {
        MWAW_DEBUG_MSG(("GreatWksText::sendZone: oops, can not send a picture in a graphic zone\n"));
        break;
      }
      MWAWPosition pictPos(Vec2f(0,0), token.m_dim, librevenge::RVNG_POINT);
      pictPos.setRelativePosition(MWAWPosition::Char, MWAWPosition::XLeft, MWAWPosition::YBottom);
      if (m_callback.m_sendPicture)
        (m_mainParser->*m_callback.m_sendPicture)(token.m_pictEntry, pictPos);
      else {
        MWAW_DEBUG_MSG(("GreatWksText::sendZone: oops, can not send the send picture callback\n"));
        break;
      }
      break;
    }
    case 0x9:
      listener->insertTab();
      break;
    case 0xb:
      f << "[colbreak]";
      if (!isMain) {
        MWAW_DEBUG_MSG(("GreatWksText::sendZone: find column break in auxilliary block\n"));
        break;
      }
      if (actCol < numCol-1 && numCol > 1) {
        listener->insertBreak(MWAWBasicListener::ColumnBreak);
        actCol++;
      }
      else {
        actCol = 0;
        ++actPage;
        if (m_callback.m_newPage)
          (m_mainParser->*m_callback.m_newPage)(actPage);
      }
      break;
    case 0xc:
      f << "[pagebreak]";
      if (!isMain) {
        f << "###";
        MWAW_DEBUG_MSG(("GreatWksText::sendZone: find page break in auxilliary zone\n"));
        break;
      }
      ++actPage;
      if (m_callback.m_newPage)
        (m_mainParser->*m_callback.m_newPage)(actPage);
      actCol = 0;
      break;
    case 0xd:
      listener->insertEOL();
      break;
    case 2: // page number
    case 0x15: // date
    case 0x16: // time
      if (token.sendTo(*listener))
        break;
      f << "###";
      MWAW_DEBUG_MSG(("GreatWksText::sendZone: can not send token\n"));
      break;
    default:
      listener->insertCharacter((unsigned char) c);
      break;
    }
    cPos++;
  }
  return true;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
