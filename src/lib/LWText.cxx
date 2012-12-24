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
#include "MWAWDebug.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWPageSpan.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWRSRCParser.hxx"
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
//! Internal: struct used to store the font of a MRWText
struct Font {
  //! constructor
  Font() : m_font(), m_height(0), m_pictId(0), m_extra("") {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Font const &font);

  //! the font
  MWAWFont m_font;
  //! the line height
  int m_height;
  //! the pict id (if set)
  int m_pictId;
  //! extra data
  std::string m_extra;
};

std::ostream &operator<<(std::ostream &o, Font const &font)
{
  if (font.m_height > 0)
    o << "h=" << font.m_height << ",";
  if (font.m_pictId > 0)
    o << "pictId=" << font.m_pictId << ",";
  o << font.m_extra;
  return o;
}

////////////////////////////////////////
//! Internal: the state of a LWText
struct State {
  //! constructor
  State() : m_version(-1), m_numPages(-1), m_actualPage(0), m_fontsList(), m_auxiFontsList(), m_paragraphsList(), m_plcMap() {
  }

  //! the file version
  mutable int m_version;

  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;

  //! the list of fonts
  std::vector<Font> m_fontsList;
  //! the auxiliar list of fonts
  std::vector<Font> m_auxiFontsList;
  //! the list of paragraph
  std::vector<MWAWParagraph> m_paragraphsList;
  std::multimap<long, PLC> m_plcMap /** the plc map */;
};

////////////////////////////////////////
//! Internal: the subdocument of a LWText
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(LWText &pars, MWAWInputStreamPtr input, int id, libmwaw::SubDocumentType type) :
    MWAWSubDocument(pars.m_mainParser, input, MWAWEntry()), m_textParser(&pars), m_id(id), m_type(type) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  /** the text parser */
  LWText *m_textParser;
  //! the subdocument id
  int m_id;
  //! the subdocument type
  libmwaw::SubDocumentType m_type;
private:
  SubDocument(SubDocument const &orig);
  SubDocument &operator=(SubDocument const &orig);
};

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("SubDocument::parse: no listener\n"));
    return;
  }
  LWContentListener *listen = dynamic_cast<LWContentListener *>(listener.get());
  if (!listen) {
    MWAW_DEBUG_MSG(("SubDocument::parse: bad listener\n"));
    return;
  }

  assert(m_textParser);

  long pos = m_input->tell();
  m_input->seek(pos, WPX_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_textParser != sDoc->m_textParser) return true;
  if (m_id != sDoc->m_id) return true;
  if (m_type != sDoc->m_type) return true;
  return false;
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
LWText::LWText(MWAWInputStreamPtr ip, LWParser &parser, MWAWFontConverterPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert),
  m_state(new LWTextInternal::State), m_mainParser(&parser), m_asciiFile(parser.ascii())
{
}

LWText::~LWText()
{ }

int LWText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_mainParser->version();
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

bool LWText::sendText(MWAWEntry &entry)
{
#if 0
  if (!m_listener) {
    MWAW_DEBUG_MSG(("LWText::sendText: can not find a listener\n"));
    return false;
  }
#endif
  std::multimap<long, LWTextInternal::PLC>::const_iterator plcIt;

  long pos = entry.begin(), endPos = entry.end();
  libmwaw::DebugStream f, f2;
  m_input->seek(pos, WPX_SEEK_SET);

  while (1) {
    long actPos = m_input->tell();
    bool done = m_input->atEOS() || actPos ==endPos;
    char c = done ? 0 : (char) m_input->readULong(1);
    if (c==0xd || done) {
      f2.str("");
      f2 << "Entries(TextContent):" << f.str();
      ascii().addPos(pos);
      ascii().addNote(f2.str().c_str());
      f.str("");
      pos = actPos+1;
    }
    if (done) break;

    plcIt = m_state->m_plcMap.find(actPos);
    while (plcIt != m_state->m_plcMap.end() && plcIt->first==actPos) {
      LWTextInternal::PLC const &plc = plcIt++->second;
      f << "[" << plc << "]";
    }
    if (c != 0xd)
      f << c;
    else if (c==0)
      f << "#[0]";
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
  long val;
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    LWTextInternal::Font font;
    f.str("");
    long cPos = input->readLong(4);
    font.m_height = (int) input->readLong(2);
    int sz = (int) input->readLong(2);
    font.m_font.setId((int) input->readLong(2));
    uint32_t flags=0;
    int flag=(int) input->readULong(1);
    if (flag&0x1) flags |= MWAW_BOLD_BIT;
    if (flag&0x2) flags |= MWAW_ITALICS_BIT;
    if (flag&0x4) font.m_font.setUnderlineStyle(MWAWBorder::Single);
    if (flag&0x8) flags |= MWAW_EMBOSS_BIT;
    if (flag&0x10) flags |= MWAW_SHADOW_BIT;
    if (flag&0x20) f << "expand,";
    if (flag&0x40) f << "condense,";
    if (flag&0x80) f << "#fl80,";
    val = (int) input->readULong(1); // always 0?
    if (val) f << "#f0=" << val << ",";
    font.m_font.setFlags(flags);
    font.m_font.setSize((int) input->readLong(2));
    if (sz!=font.m_font.size())
      f << "#sz=" << sz << ",";
    int col[3];
    for (int j=0; j < 3; j++)
      col[j] = (int) input->readULong(2);
    if (col[0] || col[1] || col[2])
      font.m_font.setColor(uint32_t(((col[0]>>8)<<16)|((col[1]>>8)<<8)|(col[2]>>8)));
    font.m_extra=f.str();
    f.str("");
    f << "Fonts-" << i << ":cPos=" << std::hex << cPos << std::dec << ","
      << font.m_font.getDebugString(m_convertissor) << font;

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
  long val;
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
      font.m_font.setUnderlineStyle(MWAWBorder::Single);
      break;
    case 2:
      font.m_font.setUnderlineStyle(MWAWBorder::Double);
      break;
    case 3:
      font.m_font.setUnderlineStyle(MWAWBorder::Single);
      f << "underline[w=2],";
      break;
    case 4:
      font.m_font.setUnderlineStyle(MWAWBorder::Dot);
      break;
    case 5:
      font.m_font.setUnderlineStyle(MWAWBorder::Dot);
      f << "underline[dot2],";
      break;
    default:
      f << "#underline=" << ((flag>>3)&7) << ",";
      break;
    }
    switch((flag>>6)&7) {
    case 0:
      break;
    case 1:
      flags |= MWAW_STRIKEOUT_BIT;
      break;
    case 2:
      flags |= MWAW_STRIKEOUT_BIT;
      f << "strike[double],";
      break;
    case 3:
      flags |= MWAW_STRIKEOUT_BIT;
      f << "strike[w=2],";
      break;
    case 4:
      flags |= MWAW_STRIKEOUT_BIT;
      f << "strike[dot],";
      break;
    case 5:
      flags |= MWAW_STRIKEOUT_BIT;
      f << "strike[dot2],";
      break;
    default:
      f << "#strike=" << ((flag>>6)&7) << ",";
      break;
    }
    switch((flag>>9)&7) {
    case 0:
      break;
    case 1:
      flags |= MWAW_OVERLINE_BIT;
      break;
    case 2:
      flags |= MWAW_OVERLINE_BIT;
      f << "over[double],";
      break;
    case 3:
      flags |= MWAW_OVERLINE_BIT;
      f << "over[w=2],";
      break;
    case 4:
      flags |= MWAW_OVERLINE_BIT;
      f << "over[dot],";
      break;
    case 5:
      flags |= MWAW_OVERLINE_BIT;
      f << "over[dot2],";
      break;
    default:
      f << "#over=" << ((flag>>9)&7) << ",";
      break;
    }
    if (flag >> 12)
      f << "colorId[line]=" << (flag >> 12) << ",";
    flag &= 0x0004;
    if (flag) f << "flags=#" << std::hex << flag << std::dec << ",";
    /* fl0=0|2|b|40|42|..|a0 */
    val = (long) input->readULong(1);
    if (val & 0xf0)
      f << "backColorId=" << (val>>4) << ",";
    if (val & 0xf)
      f << "backPatternId=" << (val&0xf) << ",";
    flag = (uint32_t) input->readULong(1);
    switch(flag&7) {
    case 0:
      break;
    case 1:
      flags |= MWAW_SUPERSCRIPT100_BIT;
      break;
    case 2:
      flags |= MWAW_SUBSCRIPT100_BIT;
      break;
    case 5:
      flags |= MWAW_SUPERSCRIPT_BIT;
      break;
    case 6:
      flags |= MWAW_SUBSCRIPT_BIT;
      break;
    default:
      f << "#pos=" << (flag&7) << ",";
    }
    if (flag&8) f << "write[hor],";
    if (flag & 0xF0)
      f << "#pos2=" << std::hex << (flag&0xF0) << std::dec << ",";
    font.m_pictId = (int) input->readULong(2);
    font.m_font.setFlags(flags);
    font.m_extra = f.str();
    plc.m_id = i;
    plc.m_extra = f.str();
    m_state->m_auxiFontsList.push_back(font);
    m_state->m_plcMap.insert(std::map<long, LWTextInternal::PLC>::value_type(cPos, plc));

    f.str("");
    f << "Font2-" << i << ":cPos=" << std::hex << cPos << std::dec << ",Fa" << i << ","
      << font.m_font.getDebugString(m_convertissor) << font;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+10, WPX_SEEK_SET);
  }
  return true;
}

//////////////////////////////////////////////
// Ruler
//////////////////////////////////////////////
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
  long val;
  LWTextInternal::PLC plc;
  plc.m_type = LWTextInternal::P_Ruler;
  for (int i = 0; i < N; i++) {
    MWAWParagraph para;

    pos = input->tell();
    f.str("");
    long cPos = input->readLong(4);
    para.m_margins[0] = (int) input->readLong(2);
    para.m_margins[1] = (int) input->readLong(2);
    para.m_margins[2] = (int) input->readLong(2);
    val = (int) input->readLong(2);
    if (val)
      para.m_spacings[1]=para.m_spacings[2]=float(val)/72.f;
    val = (int) input->readLong(2);
    if (val)
      f << "expand=" << val << ",";
    long flag = (long) input->readULong(1);
    switch(flag) {
    case 0:
      break; // left
    case 1:
      para.m_justify = libmwaw::JustificationCenter;
      break;
    case 2:
      para.m_justify = libmwaw::JustificationRight;
      break;
    case 3:
      para.m_justify = libmwaw::JustificationFull;
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
  long val;
  LWTextInternal::PLC plc;
  plc.m_type = LWTextInternal::P_StyleU;
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    long cPos = input->readLong(4);
    long flag = (long) input->readULong(2); // 2022
    if (flag)
      f << "flag=" << std::hex << flag << std::dec << ",";
    val = input->readLong(2); // always 0
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
//     Fonts
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
//     Paragraphs
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

//! send data to the listener
bool LWText::sendMainText()
{
  // if (!m_listener) return true;
  m_input->seek(0, WPX_SEEK_SET);
  while (!m_input->atEOS())
    m_input->seek(2048, WPX_SEEK_CUR);
  MWAWEntry entry;
  entry.setBegin(0);
  entry.setEnd(m_input->tell());
  sendText(entry);
  return true;
}


void LWText::flushExtra()
{
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
