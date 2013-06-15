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

#include <libwpd/libwpd.h>

#include "MWAWContentListener.hxx"
#include "MWAWDebug.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWPageSpan.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSection.hxx"
#include "MWAWSubDocument.hxx"

#include "LWParser.hxx"

#include "LWText.hxx"

/** Internal: the structures of a LWText */
namespace LWTextInternal
{
/** the different plc type */
enum PLCType { P_Font, P_Font2, P_Ruler, P_Ruby, P_StyleU, P_StyleV, P_Unknown};

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
  case P_StyleU:
    o << "U";
    break;
  case P_StyleV:
    o << "V";
    break;
  case P_Font2:
    o << "Fa";
    break;
  case P_Ruby:
    o << "Rb";
    break;
  case P_Unknown:
  default:
    o << "#Unkn";
    break;
  }
  if (plc.m_id >= 0) o << plc.m_id;
  else o << "_";
  if (plc.m_extra.length()) o << ":" << plc.m_extra;
  else o << ",";
  return o;
}

////////////////////////////////////////
//! Internal: struct used to store the font of a LWText
struct Font {
  //! constructor
  Font() : m_font(), m_height(0), m_pictId(0) {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Font const &font);

  //! merge extra data to get final font
  void merge(Font const &fExtra) {
    m_font.setFlags(m_font.flags()|fExtra.m_font.flags());
    if (fExtra.m_font.getUnderline().isSet())
      m_font.setUnderline(fExtra.m_font.getUnderline());
    if (fExtra.m_font.getStrikeOut().isSet())
      m_font.setStrikeOut(fExtra.m_font.getStrikeOut());
    if (fExtra.m_font.getOverline().isSet())
      m_font.setOverline(fExtra.m_font.getOverline());
    m_font.set(fExtra.m_font.script());
    MWAWColor backColor;
    fExtra.m_font.getBackgroundColor(backColor);
    m_font.setBackgroundColor(backColor);
    m_font.m_extra += fExtra.m_font.m_extra;
    m_pictId = fExtra.m_pictId;
  }

  //! the font
  MWAWFont m_font;
  //! the line height
  int m_height;
  //! the pict id (if set)
  int m_pictId;
};

std::ostream &operator<<(std::ostream &o, Font const &font)
{
  if (font.m_height > 0)
    o << "h=" << font.m_height << ",";
  if (font.m_pictId > 0)
    o << "pictId=" << font.m_pictId << ",";
  return o;
}

/** Internal: class to store the paragraph properties */
struct Paragraph : public MWAWParagraph {
  //! Constructor
  Paragraph() : MWAWParagraph(), m_deltaSpacing(0) {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Paragraph const &ind) {
    o << reinterpret_cast<MWAWParagraph const &>(ind);
    if (ind.m_deltaSpacing<0 || ind.m_deltaSpacing>0) o << "deltaSpacing=" << ind.m_deltaSpacing << ",";
    return o;
  }
  //! the paragraph delta spacing
  float m_deltaSpacing;
};

////////////////////////////////////////
//! Internal: the header/footer zone of a LWText
struct HFZone {
  //! constructor
  HFZone() : m_numChar(0), m_pos(), m_height(0), m_font(), m_justify(MWAWParagraph::JustificationLeft), m_extra("") {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, HFZone const &hf);
  //! the number of char
  int m_numChar;
  //! the position of the text in the file
  MWAWEntry m_pos;
  //! the height
  int m_height;
  //! the font
  MWAWFont m_font;
  /** the paragraph justification */
  MWAWParagraph::Justification m_justify;
  //! extra data
  std::string m_extra;
};

std::ostream &operator<<(std::ostream &o, HFZone const &hf)
{
  if (hf.m_numChar > 0)
    o << "nC=" << hf.m_numChar << ",";
  if (hf.m_height > 0)
    o << "h=" << hf.m_height << ",";
  switch(hf.m_justify) {
  case MWAWParagraph::JustificationLeft:
    break;
  case MWAWParagraph::JustificationCenter:
    o << "just=centered, ";
    break;
  case MWAWParagraph::JustificationRight:
    o << "just=right, ";
    break;
  case MWAWParagraph::JustificationFull:
    o << "just=full, ";
    break;
  case MWAWParagraph::JustificationFullAllLines:
    o << "just=fullAllLines, ";
    break;
  default:
    o << "#just=" << hf.m_justify << ", ";
  }
  o << hf.m_extra;
  return o;
}

////////////////////////////////////////
//! Internal: the state of a LWText
struct State {
  //! constructor
  State() : m_version(-1), m_numPages(-1), m_actualPage(0), m_fontsList(), m_auxiFontsList(), m_paragraphsList(), m_plcMap(), m_header(), m_footer() {
  }

  //! the file version
  mutable int m_version;

  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;

  //! the list of fonts
  std::vector<Font> m_fontsList;
  //! the auxiliar list of fonts
  std::vector<Font> m_auxiFontsList;
  //! the list of paragraph
  std::vector<Paragraph> m_paragraphsList;
  std::multimap<long, PLC> m_plcMap /** the plc map */;

  HFZone m_header /** header */, m_footer /** footer */;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
LWText::LWText(LWParser &parser) :
  m_parserState(parser.getParserState()), m_state(new LWTextInternal::State), m_mainParser(&parser)
{
}

LWText::~LWText()
{ }

int LWText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}

int LWText::numPages() const
{
  if (m_state->m_numPages >= 0)
    return m_state->m_numPages;
  const_cast<LWText *>(this)->computePositions();
  return m_state->m_numPages;
}


void LWText::computePositions()
{
  int nPages = 1;
  m_state->m_actualPage = 1;
  m_state->m_numPages = nPages;
}

bool LWText::hasHeaderFooter(bool header) const
{
  if (header)
    return m_state->m_header.m_pos.valid();
  return m_state->m_footer.m_pos.valid();
}

bool LWText::getColor(int id, MWAWColor &col) const
{
  if (id < 0 || id >= 11) {
    MWAW_DEBUG_MSG(("LWText::createZones: can not find col for id=%d\n", id));
    return false;
  }
  static uint32_t const color[] = {
    0, 0xFF0000, 0x00FF00, 0xFF, 0xFFFF, 0xFF00FF, 0xFFFF00,
    0xFF8040, 0x800000, 0x808080, 0xFFFFFF
  };
  col = color[id];
  return true;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

// find the different zones
bool LWText::createZones()
{
  MWAWRSRCParserPtr rsrcParser = m_mainParser->getRSRCParser();
  if (!rsrcParser) {
    MWAW_DEBUG_MSG(("LWText::createZones: can not find the entry map\n"));
    return false;
  }
  std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::iterator it;

  // the different zones
  it = entryMap.lower_bound("styl");
  while (it != entryMap.end()) {
    if (it->first != "styl")
      break;

    MWAWEntry const &entry = it++->second;
    readFonts(entry);
  }
  it = entryMap.lower_bound("styw");
  while (it != entryMap.end()) {
    if (it->first != "styw")
      break;
    MWAWEntry const &entry = it++->second;
    readFont2(entry);
  }
  it = entryMap.lower_bound("styx");
  while (it != entryMap.end()) {
    if (it->first != "styx")
      break;

    MWAWEntry const &entry = it++->second;
    readRulers(entry);
  }

  it = entryMap.lower_bound("styu"); // pos, flags, 0?
  while (it != entryMap.end()) {
    if (it->first != "styu")
      break;
    MWAWEntry const &entry = it++->second;
    readStyleU(entry);
  }
  it = entryMap.lower_bound("styv"); // never seen
  while (it != entryMap.end()) {
    if (it->first != "styv")
      break;
    MWAWEntry const &entry = it++->second;
    readUnknownStyle(entry);
  }
  it = entryMap.lower_bound("styy"); // pos, 2 flags
  while (it != entryMap.end()) {
    if (it->first != "styy")
      break;
    MWAWEntry const &entry = it++->second;
    readRuby(entry);
  }

  // now update the different data
  computePositions();
  return true;
}

bool LWText::sendMainText()
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("LWText::sendMainText: can not find a listener\n"));
    return false;
  }
  std::multimap<long, LWTextInternal::PLC>::const_iterator plcIt;

  bool useDataFork=m_mainParser->textInDataFork();
  libmwaw::DebugFile &ascFile =
    useDataFork ? m_parserState->m_asciiFile : m_mainParser->rsrcAscii();
  libmwaw::DebugStream f, f2;
  MWAWInputStreamPtr input=
    useDataFork ? m_parserState->m_input :  m_mainParser->rsrcInput();

  long beginPos=0, endPos=-1;
  if (!useDataFork) {
    MWAWEntry entry = m_mainParser->getRSRCParser()->getEntry("TEXT", 128);
    if (!entry.valid()) {
      MWAW_DEBUG_MSG(("LWText::sendMainText: can not find text entry\n"));
      return false;
    }
    beginPos=entry.begin();
    endPos=entry.end();
  }

  long pos = beginPos;
  input->seek(pos, WPX_SEEK_SET);
  LWTextInternal::Font font, auxiFont;

  int numCols, sepWidth;
  if (m_mainParser->getColumnInfo(numCols, sepWidth) && numCols > 1) {
    MWAWSection sec;
    sec.setColumns(numCols, m_mainParser->getPageWidth()/double(numCols), WPX_INCH);
    listener->openSection(sec);
  }

  float deltaSpacing=0;
  while (1) {
    long actPos = input->tell();
    bool done =  input->atEOS() || (endPos>=0&&actPos>=endPos);
    char c = done ? (char) 0 : (char) input->readULong(1);
    if (c==0xd || done) {
      f2.str("");
      f2 << "Entries(TextContent):" << f.str();
      ascFile.addPos(pos);
      ascFile.addNote(f2.str().c_str());
      f.str("");
      pos = actPos+1;
    }
    if (done) break;

    long actC=actPos-beginPos;
    plcIt = m_state->m_plcMap.find(actC);
    bool fontChanged = false;
    while (plcIt != m_state->m_plcMap.end() && plcIt->first<=actC) {
      LWTextInternal::PLC const &plc = plcIt++->second;
      f << "[" << plc << "]";
      switch(plc.m_type) {
      case LWTextInternal::P_Font:
        if (plc.m_id < 0 || plc.m_id >= int(m_state->m_fontsList.size())) {
          MWAW_DEBUG_MSG(("LWText::sendMainText: can not find font %d\n", plc.m_id));
          break;
        }
        font = m_state->m_fontsList[size_t(plc.m_id)];
        fontChanged = true;
        break;
      case LWTextInternal::P_Font2:
        if (plc.m_id < 0 || plc.m_id >= int(m_state->m_auxiFontsList.size())) {
          MWAW_DEBUG_MSG(("LWText::sendMainText: can not find auxi font %d\n", plc.m_id));
          break;
        }
        auxiFont = m_state->m_auxiFontsList[size_t(plc.m_id)];
        fontChanged = true;
        break;
      case LWTextInternal::P_Ruler: {
        float newDeltaSpacing = 0;
        if (plc.m_id < 0 || plc.m_id >= int(m_state->m_paragraphsList.size())) {
          MWAW_DEBUG_MSG(("LWText::sendMainText: can not find paragraph %d\n", plc.m_id));
        } else {
          LWTextInternal::Paragraph const &para=m_state->m_paragraphsList[size_t(plc.m_id)];
          newDeltaSpacing = para.m_deltaSpacing;
          setProperty(para);
        }
        if (deltaSpacing < newDeltaSpacing || deltaSpacing > newDeltaSpacing) {
          deltaSpacing=newDeltaSpacing;
          fontChanged = true;
        }
        break;
      }
      case LWTextInternal::P_Ruby:
      case LWTextInternal::P_StyleU:
      case LWTextInternal::P_StyleV:
      case LWTextInternal::P_Unknown:
      default:
        break;
      }
    }
    f << c;
    if (fontChanged) {
      LWTextInternal::Font final(font);
      final.merge(auxiFont);
      if (deltaSpacing<0||deltaSpacing>0)
        final.m_font.setDeltaLetterSpacing(deltaSpacing+final.m_font.deltaLetterSpacing());
      listener->setFont(final.m_font);
      if (final.m_pictId>0) {
        m_mainParser->sendGraphic(final.m_pictId);
        if (c==(char)0xca)
          continue;
      }
    }
    switch (c) {
    case 0x9:
      listener->insertTab();
      break;
    case 0xa:
      listener->insertEOL(true);
      break;
    case 0xc: // new section (done)
      break;
    case 0xd:
      listener->insertEOL();
      break;

    default:
      listener->insertCharacter((unsigned char)c, input);
      break;
    }
  }

  return true;
}

//////////////////////////////////////////////
// Fonts
//////////////////////////////////////////////
bool LWText::readFonts(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() < 2) {
    MWAW_DEBUG_MSG(("LWText::readFonts: the entry is bad\n"));
    return false;
  }
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(Fonts)[" << entry.id() << "]:";
  entry.setParsed(true);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (long(N*20+2) != entry.length()) {
    MWAW_DEBUG_MSG(("LWText::readFonts: the number of entry seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());

  LWTextInternal::PLC plc;
  plc.m_type = LWTextInternal::P_Font;
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    LWTextInternal::Font font;
    f.str("");
    long cPos = input->readLong(4);
    font.m_height = (int) input->readLong(2);
    float sz = (float) input->readLong(2);
    font.m_font.setId((int) input->readULong(2));
    uint32_t flags=0;
    int flag=(int) input->readULong(1);
    if (flag&0x1) flags |= MWAWFont::boldBit;
    if (flag&0x2) flags |= MWAWFont::italicBit;
    if (flag&0x4) font.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (flag&0x8) flags |= MWAWFont::embossBit;
    if (flag&0x10) flags |= MWAWFont::shadowBit;
    if (flag&0x20) font.m_font.setDeltaLetterSpacing(-1);
    if (flag&0x40) font.m_font.setDeltaLetterSpacing(1);
    if (flag&0x80) f << "#fl80,";
    int val = (int) input->readULong(1); // always 0?
    if (val) f << "#f0=" << val << ",";
    font.m_font.setFlags(flags);
    font.m_font.setSize((float) input->readLong(2));
    if (sz < font.m_font.size() || sz > font.m_font.size())
      f << "#sz=" << sz << ",";
    unsigned char col[3];
    for (int j=0; j < 3; j++)
      col[j] = (unsigned char) (input->readULong(2)>>8);
    if (col[0] || col[1] || col[2])
      font.m_font.setColor(MWAWColor(col[0],col[1],col[2]));
    font.m_font.m_extra=f.str();
    f.str("");
    f << "Fonts-" << i << ":cPos=" << std::hex << cPos << std::dec << ","
      << font.m_font.getDebugString(m_parserState->m_fontConverter) << font;

    m_state->m_fontsList.push_back(font);
    plc.m_id = i;
    m_state->m_plcMap.insert(std::map<long, LWTextInternal::PLC>::value_type(cPos, plc));

    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    input->seek(pos+20, WPX_SEEK_SET);
  }
  return true;
}

bool LWText::readFont2(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%10) != 2) {
    MWAW_DEBUG_MSG(("LWText::readFont2: the entry seems bad\n"));
    return false;
  }

  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(Font2)[" << entry.id() << "]:";
  entry.setParsed(true);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (entry.length() != N*10+2) {
    f << "###";
    MWAW_DEBUG_MSG(("LWText::readFont2: N seems bad\n"));
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());

    return false;
  }

  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  LWTextInternal::PLC plc;
  plc.m_type = LWTextInternal::P_Font2;
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    LWTextInternal::Font font;

    long cPos = input->readLong(4);
    uint32_t flag = (uint32_t) input->readULong(2), flags=0;
    switch((flag>>3)&7) {
    case 0:
      break;
    case 1:
      font.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
      break;
    case 2:
      font.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
      font.m_font.setUnderlineType(MWAWFont::Line::Double);
      break;
    case 3:
      font.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
      font.m_font.setUnderlineWidth(2.0);
      break;
    case 4:
      font.m_font.setUnderlineStyle(MWAWFont::Line::Dot);
      break;
    case 5:
      font.m_font.setUnderlineStyle(MWAWFont::Line::Dot);
      font.m_font.setUnderlineWidth(2.0);
      break;
    default:
      f << "#underline=" << ((flag>>3)&7) << ",";
      break;
    }
    switch((flag>>6)&7) {
    case 0:
      break;
    case 1:
      font.m_font.setStrikeOutStyle(MWAWFont::Line::Simple);
      break;
    case 2:
      font.m_font.setStrikeOutStyle(MWAWFont::Line::Simple);
      font.m_font.setStrikeOutType(MWAWFont::Line::Double);
      break;
    case 3:
      font.m_font.setStrikeOutStyle(MWAWFont::Line::Simple);
      font.m_font.setStrikeOutWidth(2.0);
      break;
    case 4:
      font.m_font.setStrikeOutStyle(MWAWFont::Line::Dot);
      break;
    case 5:
      font.m_font.setStrikeOutStyle(MWAWFont::Line::Dot);
      font.m_font.setStrikeOutWidth(2.0);
      break;
    default:
      f << "#strike=" << ((flag>>6)&7) << ",";
      break;
    }
    switch((flag>>9)&7) {
    case 0:
      break;
    case 1:
      font.m_font.setOverlineStyle(MWAWFont::Line::Simple);
      break;
    case 2:
      font.m_font.setOverlineStyle(MWAWFont::Line::Simple);
      font.m_font.setOverlineType(MWAWFont::Line::Double);
      break;
    case 3:
      font.m_font.setOverlineStyle(MWAWFont::Line::Simple);
      font.m_font.setOverlineWidth(2.0);
      break;
    case 4:
      font.m_font.setOverlineStyle(MWAWFont::Line::Dot);
      break;
    case 5:
      font.m_font.setOverlineStyle(MWAWFont::Line::Dot);
      font.m_font.setOverlineWidth(2.0);
      break;
    default:
      f << "#over=" << ((flag>>9)&7) << ",";
      break;
    }
    if (flag >> 12) {
      MWAWColor col=0;
      if (getColor(int(flag >> 12), col)) {
        font.m_font.setOverlineColor(col);
        font.m_font.setStrikeOutColor(col);
        font.m_font.setUnderlineColor(col);
      } else
        f << "#colorId[line]=" << (flag >> 12) << ",";
    }
    flag &= 0x0004;
    if (flag) f << "flags=#" << std::hex << flag << std::dec << ",";
    /* fl0=0|2|b|40|42|..|a0 */
    long val = (long) input->readULong(1);
    MWAWColor backColor(MWAWColor::black());
    if ((val & 0xf0) && !getColor(int(val>>4), backColor))
      f << "#backColorId=" << (val>>4) << ",";
    int percent = (val & 0xf);
    if (percent >= 0 && percent <= 10) {
      float percentVal = 0.1f*float(percent);
      backColor =MWAWColor::barycenter(percentVal,backColor,1.f-percentVal,MWAWColor::white());
      font.m_font.setBackgroundColor(backColor);
    } else if (percent == 11) // fixme: backColor is the border color
      flags |= MWAWFont::boxedBit;
    else {
      MWAW_DEBUG_MSG(("LWText::readFont2: find unknown percent id=%d\n", percent));
    }
    // 0: 0%, .. 10:100%, 11:border
    if (val & 0xf)
      f << "backPatternId=" << (val&0xf) << ",";
    flag = (uint32_t) input->readULong(1);
    switch(flag&7) {
    case 0:
      break;
    case 1:
      font.m_font.set(MWAWFont::Script(33));
      break;
    case 2:
      font.m_font.set(MWAWFont::Script(-33));
      break;
    case 5:
      font.m_font.set(MWAWFont::Script::super());
      break;
    case 6:
      font.m_font.set(MWAWFont::Script::sub());
      break;
    default:
      f << "#pos=" << (flag&7) << ",";
    }
    if (flag&8) f << "write[hor],";
    if (flag & 0xF0)
      f << "#pos2=" << std::hex << (flag&0xF0) << std::dec << ",";
    font.m_pictId = (int) input->readULong(2);
    font.m_font.setFlags(flags);
    font.m_font.m_extra = f.str();
    plc.m_id = i;
    plc.m_extra = f.str();
    m_state->m_auxiFontsList.push_back(font);
    m_state->m_plcMap.insert(std::map<long, LWTextInternal::PLC>::value_type(cPos, plc));

    f.str("");
    f << "Font2-" << i << ":cPos=" << std::hex << cPos << std::dec << ",Fa" << i << ","
      << font.m_font.getDebugString(m_parserState->m_fontConverter) << font;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+10, WPX_SEEK_SET);
  }
  return true;
}

//////////////////////////////////////////////
// Ruler
//////////////////////////////////////////////
void LWText::setProperty(MWAWParagraph const &ruler)
{
  if (!m_parserState->m_listener) return;
  m_parserState->m_listener->setParagraph(ruler);
}

bool LWText::readRulers(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%84) != 2) {
    MWAW_DEBUG_MSG(("LWText::readRulers: the entry seems bad\n"));
    return false;
  }

  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(Ruler)[" << entry.id() << "]:";
  entry.setParsed(true);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (entry.length() != N*84+2) {
    f << "###";
    MWAW_DEBUG_MSG(("LWText::readRulers: N seems bad\n"));
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());

    return false;
  }

  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  LWTextInternal::PLC plc;
  plc.m_type = LWTextInternal::P_Ruler;
  for (int i = 0; i < N; i++) {
    LWTextInternal::Paragraph para;

    pos = input->tell();
    f.str("");
    long cPos = input->readLong(4);
    para.m_marginsUnit = WPX_POINT;
    para.m_margins[0] = (int) input->readLong(2);
    para.m_margins[1] = (int) input->readLong(2);
    para.m_margins[2] = (int) input->readLong(2);
    para.m_margins[0] = para.m_margins[0].get()-para.m_margins[1].get();

    long val = (int) input->readLong(2);
    if (val)
      para.m_spacings[1]=para.m_spacings[2]=float(val)/72.f;
    para.m_deltaSpacing = (float) input->readLong(2);
    long flag = (long) input->readULong(1);
    switch(flag) {
    case 0:
      break; // left
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
      f << "#justify=" << flag << ",";
    }
    int numTabs = (int) input->readULong(1);
    if (numTabs > 16) {
      MWAW_DEBUG_MSG(("LWText::readRulers: the numbers of tabs seems bad\n"));
      f << "###nTabs=" << numTabs << ",";
      numTabs=0;
    }
    uint32_t tabsType = (uint32_t) input->readULong(4);
    for (int j = 0; j < numTabs; j++) {
      MWAWTabStop tab;
      tab.m_position = float(input->readLong(2))/72.f;
      switch(tabsType&3) {
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
      tabsType = tabsType>>2;
      para.m_tabs->push_back(tab);
    }
    input->seek(pos+52, WPX_SEEK_SET);
    uint32_t tabsLeader = (uint32_t) input->readULong(4);
    for (int j = 0; j < numTabs; j++) {
      uint16_t leader=0;
      switch(tabsLeader&3) {
      case 1:
        leader='.';
        break;
      case 2:
        leader='-';
        break;
      case 3:
        leader='_';
        break;
      case 0: // none
      default:
        break;
      }
      tabsLeader = tabsLeader>>2;
      if (!leader) continue;
      (*para.m_tabs)[size_t(j)].m_leaderCharacter = leader;
    }
    for (int j = 0; j < 3; j++) { // g0=0|0b13
      val = (int) input->readLong(2);
      if (val)
        f << "g" << j << "=" << std::hex << val << std::dec << ",";
    }
    for (int j = 0; j < 2; j++) { // g5=5|14
      val = (long) input->readULong(1);
      if (val) f << "g" << j+3 << "=" << val << ",";
    }
    for (int j = 0; j < 10; j++) { // always 0
      val = (int) input->readLong(2);
      if (val)
        f << "h" << j << "=" << std::hex << val << std::dec << ",";
    }
    para.m_extra = f.str();
    m_state->m_paragraphsList.push_back(para);
    plc.m_id = i;
    m_state->m_plcMap.insert(std::map<long, LWTextInternal::PLC>::value_type(cPos, plc));

    f.str("");
    f << "Ruler-" << i << ":cPos=" << std::hex << cPos << std::dec << "," << para;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+84, WPX_SEEK_SET);
  }
  return true;
}

//////////////////////////////////////////////
// HF
//////////////////////////////////////////////
bool LWText::sendHeaderFooter(bool header)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("LWText::sendHeaderFooter: can not find the listener\n"));
    return false;
  }
  LWTextInternal::HFZone const &zone =
    header ? m_state->m_header : m_state->m_footer;
  if (!zone.m_pos.valid()) {
    MWAW_DEBUG_MSG(("LWText::sendHeaderFooter: can not find the text\n"));
    return false;
  }
  MWAWParagraph para;
  para.m_justify=zone.m_justify;
  listener->setParagraph(para);
  listener->setFont(zone.m_font);
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  input->seek(zone.m_pos.begin(), WPX_SEEK_SET);

  int numChar = (int) zone.m_pos.length();
  std::string str("");
  for (int c = 0; c < numChar; c++) {
    if(input->atEOS()) {
      MWAW_DEBUG_MSG(("LWText::sendHeaderFooter: can not read end of text\n"));
      break;
    }
    str += (char) input->readULong(1);
  }
  std::string::const_iterator it = str.begin();
  while (it!=str.end()) {
    char c=*(it++);
    if (c=='<' && it!= str.end()) {
      char const *strPtr = &(*it);
      bool done = true;
      if (strncmp(strPtr, "PAGE>", 5)==0)
        listener->insertField(MWAWField(MWAWField::PageNumber));
      else if (strncmp(strPtr, "DATE>", 5)==0)
        listener->insertField(MWAWField(MWAWField::Date));
      else if (strncmp(strPtr, "TIME>", 5)==0)
        listener->insertField(MWAWField(MWAWField::Time));
      else if (strncmp(strPtr, "PMAX>", 5)==0)
        listener->insertField(MWAWField(MWAWField::PageCount));
      else if (strncmp(strPtr, "NAME>", 5)==0)
        listener->insertField(MWAWField(MWAWField::Title));
      else
        done=false;
      if (done) {
        it += 5;
        continue;
      }
    }
    if (c == 0xd) {
      listener->insertEOL();
      continue;
    }
    listener->insertCharacter((unsigned char) c);
  }
  return true;
}

bool LWText::readDocumentHF(MWAWEntry const &entry)
{
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  long pos = input->tell();

  libmwaw::DebugStream f, f2;
  f << "Document(HF):";
  if (entry.length()<0x50) {
    f << "###";
    MWAW_DEBUG_MSG(("LWText::readDocumentHF: HF seems too short\n"));
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }

  for (int s=0; s < 2; s++) {
    LWTextInternal::HFZone zone;
    zone.m_height = (int) input->readLong(2);
    zone.m_numChar = (int) input->readLong(2);
    // ruler align
    int flag = (int) input->readULong(1); // [012]
    f2.str("");
    switch(flag) {
    case 0:
      break; // left
    case 1:
      zone.m_justify = MWAWParagraph::JustificationCenter;
      break;
    case 2:
      zone.m_justify = MWAWParagraph::JustificationRight;
      break;
    case 3:
      zone.m_justify = MWAWParagraph::JustificationFull;
      break;
    default:
      f2 << "#justify=" << flag << ",";
    }
    uint32_t flags=0;
    flag=(int) input->readULong(1);
    if (flag&0x1) flags |= MWAWFont::boldBit;
    if (flag&0x2) flags |= MWAWFont::italicBit;
    if (flag&0x4) zone.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (flag&0x8) flags |= MWAWFont::embossBit;
    if (flag&0x10) flags |= MWAWFont::shadowBit;
    if (flag&0x20) zone.m_font.setDeltaLetterSpacing(-1);
    if (flag&0x40) zone.m_font.setDeltaLetterSpacing(1);
    if (flag&0x80) f2 << "#fl80,";
    zone.m_font.setFlags(flags);
    zone.m_font.setId((int) input->readULong(2));
    zone.m_font.setSize((float) input->readULong(2));
    unsigned char col[3];
    for (int j=0; j < 3; j++)
      col[j] = (unsigned char) (input->readULong(2)>>8);
    if (col[0] || col[1] || col[2])
      zone.m_font.setColor(MWAWColor(col[0],col[1],col[2]));
    long val = input->readLong(2); // can be 0x200
    if (val) f2 << "f0=" << std::hex << val << std::dec << ",";
    zone.m_extra = f2.str();

    if (s==0)
      m_state->m_header = zone;
    else
      m_state->m_footer = zone;
    f << (s==0 ? "header" :  "footer") << "=[" << zone  << ","
      << zone.m_font.getDebugString(m_parserState->m_fontConverter) << "],";

    val = input->readLong(2);
    if (val) {
      if (s==0) f << "fPage=" << val+1 << ",";
      else
        f << "#f1=" << std::hex << val << std::dec << ",";
    }
  }
  long debText = input->tell();
  int numRemains=int(entry.end()-debText);
  bool ok = numRemains == m_state->m_header.m_numChar+m_state->m_footer.m_numChar;
  if (ok) {
    if (m_state->m_header.m_numChar) {
      m_state->m_header.m_pos.setBegin(debText);
      m_state->m_header.m_pos.setLength(m_state->m_header.m_numChar);
      debText+=m_state->m_header.m_numChar;
    }
    if (m_state->m_footer.m_numChar) {
      m_state->m_footer.m_pos.setBegin(debText);
      m_state->m_footer.m_pos.setLength(m_state->m_footer.m_numChar);
    }
  } else {
    MWAW_DEBUG_MSG(("LWText::readDocumentHF: HF can not determine header/footer text\n"));
    f << "###";
  }
  std::string name(""); // something like <NAME>(<PAGE>)
  for (int i = 0; i < numRemains; i++)
    name += (char) input->readULong(1);
  f << name << ",";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}

//////////////////////////////////////////////
// Unknown
//////////////////////////////////////////////
bool LWText::readStyleU(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%8) != 4) {
    MWAW_DEBUG_MSG(("LWText::readStyleU: the entry seems bad\n"));
    return false;
  }

  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(StyleU)[" << entry.id() << "]:";
  entry.setParsed(true);
  int N=(int) input->readULong(4);
  f << "N=" << N << ",";
  if (entry.length() != N*8+4) {
    f << "###";
    MWAW_DEBUG_MSG(("LWText::readStyleU: N seems bad\n"));
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());

    return false;
  }

  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  LWTextInternal::PLC plc;
  plc.m_type = LWTextInternal::P_StyleU;
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    long cPos = input->readLong(4);
    long flag = (long) input->readULong(2); // 2022
    if (flag)
      f << "flag=" << std::hex << flag << std::dec << ",";
    long val = input->readLong(2); // always 0
    if (val) f << "f0=" << val << ",";
    plc.m_id = i;
    plc.m_extra = f.str();
    m_state->m_plcMap.insert(std::map<long, LWTextInternal::PLC>::value_type(cPos, plc));

    f.str("");
    f << "StyleU-" << i << ":cPos=" << std::hex << cPos << std::dec << "," << plc;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+8, WPX_SEEK_SET);
  }
  return true;
}

bool LWText::readRuby(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%6) != 4) {
    MWAW_DEBUG_MSG(("LWText::readRuby: the entry seems bad\n"));
    return false;
  }

  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(Ruby)[" << entry.id() << "]:";
  entry.setParsed(true);
  int N=(int) input->readULong(4);
  f << "N=" << N << ",";
  if (entry.length() != N*6+4) {
    f << "###";
    MWAW_DEBUG_MSG(("LWText::readRuby: N seems bad\n"));
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());

    return false;
  }

  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  LWTextInternal::PLC plc;
  plc.m_type = LWTextInternal::P_Ruby;
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    long cPos = input->readLong(4);
    f << "n[text]=" << (int) input->readULong(1) << ",";
    f << "n[ruby]=" << (int) input->readULong(1) << ",";
    plc.m_id = i;
    plc.m_extra = f.str();
    m_state->m_plcMap.insert(std::map<long, LWTextInternal::PLC>::value_type(cPos, plc));

    f.str("");
    f << "Ruby-" << i << ":cPos=" << std::hex << cPos << std::dec << "," << plc;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+6, WPX_SEEK_SET);
  }
  return true;
}

bool LWText::readUnknownStyle(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() < 4) {
    MWAW_DEBUG_MSG(("LWText::readUnknownStyle: the entry is bad\n"));
    return false;
  }
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(" << entry.type() << ")[" << entry.id() << "]:";
  entry.setParsed(true);
  int numSz=2;
  int N=(int) input->readULong(2);
  if (!N) {
    N=(int) input->readULong(2);
    numSz+=2;
  }
  f << "N=" << N << ",";
  int fSz = N ? int((entry.length()-numSz)/N) : 0;
  if (long(N*fSz+numSz) != entry.length()) {
    MWAW_DEBUG_MSG(("LWText::readUnknownStyle: the number of entry seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    f << entry.type() << "-" << i << ":";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    input->seek(pos+fSz, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

void LWText::flushExtra()
{
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
