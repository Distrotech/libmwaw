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

#include <libwpd/WPXString.h>

#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"

#include "MSWParser.hxx"
#include "MSWStruct.hxx"

#include "MSWText.hxx"

#define DEBUG_PLC 1
#define DEBUG_PARAGRAPH 1
#define DEBUG_SECTION 1
#define DEBUG_ZONEINFO 1

/** Internal: the structures of a MSWText */
namespace MSWTextInternal
{

////////////////////////////////////////
//! Internal: the entry of MSWParser
struct TextEntry : public MWAWEntry {
  TextEntry() : MWAWEntry(), m_pos(-1), m_id(0), m_type(0), m_paragraphId(-1),  m_extra("") {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, TextEntry const &entry) {
    if (entry.m_pos>=0) o << "textPos=" << entry.m_pos << ",";
    o << "id?=" << entry.m_id << ",";
    if (entry.m_paragraphId >= 0) o << "tP" << entry.m_paragraphId << ",";
    switch(entry.m_type) {
    case 0x80: // keep font ?
    case 0: // reset font ?
      o << "type=" << std::hex << entry.m_type << std::dec << ",";
      break;
    default:
      o << "#type=" << std::hex << entry.m_type << std::dec << ",";
      break;
    }
    if (entry.valid())
      o << std::hex << "fPos=" << entry.begin() << ":" << entry.end() << std::dec << ",";
    if (entry.m_extra.length())
      o << entry.m_extra << ",";
    return o;
  }

  //! returns the paragraph id ( or -1, if unknown )
  int getParagraphId() const {
    return m_paragraphId;
  }
  //! a struct used to compare file textpos
  struct CompareFilePos {
    //! comparaison function
    bool operator()(TextEntry const *t1, TextEntry const *t2) const {
      long diff = t1->begin()-t2->begin();
      return (diff < 0);
    }
  };
  //! the text position
  int m_pos;
  //! some identificator
  int m_id;
  //! the type
  int m_type;
  //! the paragraph id
  int m_paragraphId;
  //! extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the zone
struct ZoneInfo {
  //! constructor
  ZoneInfo() : m_id(-1), m_type(0), m_dim(), m_value(0), m_error("") {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, ZoneInfo const &zone) {
    switch(zone.m_type) {
    case 0:
      break; // hard line break
    case 0x20:
      o << "soft,";
      break;
    default:
      if (zone.m_type&0xf0) o << "type?=" << (zone.m_type>>4) << ",";
      if (zone.m_type&0x0f) o << "#unkn=" << (zone.m_type&0xf) << ",";
      break;
    }
    if (zone.m_dim[0] > 0) o << "width=" << zone.m_dim[0] << ",";
    if (zone.m_dim[1] > 0) o << "height=" << zone.m_dim[1] << ",";
    if (zone.m_value) o << "f0=" << zone.m_value << ",";
    if (zone.m_error.length()) o << zone.m_error << ",";
    return o;
  }
  //! the identificator
  int m_id;
  //! the type
  int m_type;
  //! the zone dimension
  Vec2f m_dim;
  //! a value ( between -1 and 9)
  int m_value;
  /** the errors */
  std::string m_error;
};

////////////////////////////////////////
//! Internal: the page
struct Page {
  //! constructor
  Page() : m_id(-1), m_type(0), m_page(-1), m_error("") {
    for (int i = 0; i < 4; i++) m_values[i] = 0;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Page const &page) {
    if (page.m_id >= 0) o << "P" << page.m_id << ":";
    else o << "P_:";
    if (page.m_page != page.m_id+1) o << "page=" << page.m_page << ",";
    if (page.m_type) o << "type=" << std::hex << page.m_type << std::dec << ",";
    for (int i = 0; i < 4; i++) {
      if (page.m_values[i])
        o << "f" << i << "=" << page.m_values[i] << ",";
    }
    if (page.m_error.length()) o << page.m_error << ",";
    return o;
  }
  //! the identificator
  int m_id;
  //! the type
  int m_type;
  //! the page number
  int m_page;
  //! some values ( 0, -1, 0, small number )
  int m_values[4];
  /** the errors */
  std::string m_error;
};

////////////////////////////////////////
//! Internal: the footnote
struct Footnote {
  //! constructor
  Footnote() : m_pos(), m_id(-1), m_value(0), m_error("") { }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Footnote const &note) {
    if (note.m_id >= 0) o << "F" << note.m_id << ":";
    else o << "F_:";
    if (note.m_pos.valid())
      o << std::hex << note.m_pos.begin() << "-" << note.m_pos.end() << std::dec << ",";
    if (note.m_value) o << "f0=" << note.m_value << ",";
    if (note.m_error.length()) o << note.m_error << ",";
    return o;
  }
  //! the footnote data
  MWAWEntry m_pos;
  //! the id
  int m_id;
  //! a value ( 1, 4)
  int m_value;
  /** the errors */
  std::string m_error;
};

////////////////////////////////////////
//! Internal: the field of MSWParser
struct Field {
  //! constructor
  Field() : m_text(""), m_id(-1), m_error("") { }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Field const &field) {
    o << field.m_text;
    if (field.m_id >= 0) o << "[" << field.m_id << "]";
    if (field.m_error.length()) o << "," << field.m_error << ",";
    return o;
  }
  //! the text
  std::string m_text;
  //! the id
  int m_id;
  /** the errors */
  std::string m_error;
};

////////////////////////////////////////
//! Internal: a list of plc
struct Property {
  Property() : m_plcList(), m_debugPrint(false) {}
  //! the list of plc
  std::vector<MSWText::PLC> m_plcList;
  //! a flag to know if we have print data
  bool m_debugPrint;
};

////////////////////////////////////////
//! Internal: the state of a MSWParser
struct State {
  //! constructor
  State() : m_version(-1), m_bot(0x100), m_headerFooterZones(), m_textposList(),
    m_plcMap(), m_filePlcMap(), m_fontMap(), m_paragraphMap(), m_propertyMap(),
    m_zoneList(), m_pageList(), m_fieldList(), m_footnoteList(), m_actPage(0), m_numPages(-1) {
    for (int i = 0; i < 3; i++) m_textLength[i] = 0;
  }
  //! returns the total text size
  long getTotalTextSize() const {
    long res=0;
    for (int i = 0; i < 3; i++) res+=m_textLength[i];
    return res;
  }
  //! returns the file position corresponding to a text entry
  long getFilePos(long textPos) const {
    if (!m_textposList.size() || textPos < m_textposList[0].m_pos)
      return m_bot+textPos;
    int minVal = 0, maxVal = int(m_textposList.size())-1;
    while (minVal != maxVal) {
      int mid = (minVal+1+maxVal)/2;
      if (m_textposList[(size_t)mid].m_pos == textPos)
        return m_textposList[(size_t)mid].begin();
      if (m_textposList[(size_t)mid].m_pos > textPos)
        maxVal = mid-1;
      else
        minVal = mid;
    }
    return m_textposList[(size_t)minVal].begin() + (textPos-m_textposList[(size_t)minVal].m_pos);
  }

  //! the file version
  int m_version;

  //! the default text begin
  long m_bot;

  //! the text length (main, footnote, header+footer)
  long m_textLength[3];

  //! the header/footer zones
  std::vector<MWAWEntry> m_headerFooterZones;

  //! the text positions
  std::vector<TextEntry> m_textposList;

  //! the text correspondance zone ( textpos, plc )
  std::multimap<long, MSWText::PLC> m_plcMap;

  //! the file correspondance zone ( filepos, plc )
  std::multimap<long, MSWText::PLC> m_filePlcMap;

  //! the final correspondance font zone ( textpos, font)
  std::map<long, MSWStruct::Font> m_fontMap;

  //! the final correspondance paragraph zone ( textpos, paragraph)
  std::map<long, MSWStruct::Paragraph> m_paragraphMap;

  //! the position where we have new data ( textpos -> [ we have done debug printing ])
  std::map<long, Property> m_propertyMap;

  //! the list of zones
  std::vector<ZoneInfo> m_zoneList;

  //! the list of pages
  std::vector<Page> m_pageList;

  //! the list of fields
  std::vector<Field> m_fieldList;

  //! the list of footnotes
  std::vector<Footnote> m_footnoteList;

  int m_actPage/** the actual page*/, m_numPages /** the number of page of the final document */;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MSWText::MSWText
(MWAWInputStreamPtr ip, MSWParser &parser, MWAWFontConverterPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert), m_state(new MSWTextInternal::State),
  m_stylesManager(), m_mainParser(&parser), m_asciiFile(parser.ascii())
{
  m_stylesManager.reset(new MSWTextStyles(ip, *this, convert));
}

MSWText::~MSWText()
{ }

int MSWText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_mainParser->version();
  return m_state->m_version;
}

int MSWText::numPages() const
{
  m_state->m_numPages = int(m_state->m_pageList.size());
  return m_state->m_numPages;
}

void MSWText::setListener(MSWContentListenerPtr listen)
{
  m_listener = listen;
  m_stylesManager->setListener(listen);
}

long MSWText::getMainTextLength() const
{
  return m_state->m_textLength[0];
}

MWAWEntry MSWText::getHeader() const
{
  if (m_state->m_headerFooterZones.size() == 0)
    return MWAWEntry();
  return m_state->m_headerFooterZones[0];
}

MWAWEntry MSWText::getFooter() const
{
  if (m_state->m_headerFooterZones.size() < 2)
    return MWAWEntry();
  return m_state->m_headerFooterZones[1];
}

std::multimap<long, MSWText::PLC> &MSWText::getTextPLCMap()
{
  return m_state->m_plcMap;
}

std::multimap<long, MSWText::PLC> &MSWText::getFilePLCMap()
{
  return m_state->m_filePlcMap;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////
// PLC
std::ostream &operator<<(std::ostream &o, MSWText::PLC const &plc)
{
  switch(plc.m_type) {
  case MSWText::PLC::ZoneInfo:
    o << "Z";
    break;
  case MSWText::PLC::Section:
    o << "S";
    break;
  case MSWText::PLC::Footnote:
    o << "F";
    break;
  case MSWText::PLC::FootnoteDef:
    o << "valF";
    break;
  case MSWText::PLC::Field:
    o << "Field";
    break;
  case MSWText::PLC::Page:
    o << "Page";
    break;
  case MSWText::PLC::Font:
    o << "F";
    break;
  case MSWText::PLC::Object:
    o << "O";
    break;
  case MSWText::PLC::Paragraph:
    o << "P";
    break;
  case MSWText::PLC::HeaderFooter:
    o << "hfP";
    break;
  case MSWText::PLC::TextPosition:
    o << "textPos";
    break;
  default:
    o << "#type" + char('a'+int(plc.m_type));
  }
  if (plc.m_id < 0) o << "_";
  else o << plc.m_id;
  if (plc.m_extra.length()) o << "[" << plc.m_extra << "]";
  return o;
}

////////////////////////////////////////////////////////////
// try to find the different zone
////////////////////////////////////////////////////////////
bool MSWText::createZones(long bot, long (&textLength)[3])
{
  m_state->m_bot = bot;
  for (int i = 0; i < 3; i++)
    m_state->m_textLength[i]=textLength[i];

  std::multimap<std::string, MSWEntry> &entryMap
    = m_mainParser->m_entryMap;
  std::multimap<std::string, MSWEntry>::iterator it;
  // the fonts
  it = entryMap.find("FontIds");
  if (it != entryMap.end()) {
    std::vector<long> list;
    readLongZone(it->second, 2, list);
  }
  it = entryMap.find("FontNames");
  if (it != entryMap.end())
    readFontNames(it->second);
  // the styles
  it = entryMap.find("Styles");
  long prevDeb = 0;
  while (it != entryMap.end()) {
    if (!it->second.hasType("Styles")) break;
    MSWEntry &entry=it++->second;
#ifndef DEBUG
    // first entry is often bad or share the same data than the second
    if (entry.id() == 0)
      continue;
#endif
    if (entry.begin() == prevDeb) continue;
    prevDeb = entry.begin();
    m_stylesManager->readStyles(entry);
  }
  // read the text structure
  it = entryMap.find("TextStruct");
  if (it != entryMap.end())
    readTextStruct(it->second);

  //! the break position
  it = entryMap.find("PageBreak");
  if (it != entryMap.end())
    readPageBreak(it->second);
  it = entryMap.find("ZoneInfo");
  if (it != entryMap.end())
    readZoneInfo(it->second);
  it = entryMap.find("Section");
  if (it != entryMap.end())
    m_stylesManager->readSection(it->second);

  //! read the header footer limit
  it = entryMap.find("HeaderFooter");
  std::vector<long> hfLimits;
  if (it != entryMap.end()) { // list of header/footer size
    readLongZone(it->second, 4, hfLimits);
    size_t N = hfLimits.size();
    if (N) {
      if (version() <= 3) {
        // we must update the different size
        m_state->m_textLength[0] -= (hfLimits[N-1]+1);
        m_state->m_textLength[2] = (hfLimits[N-1]+1);
      } else if (N >= 2 && hfLimits[N-2] > m_state->m_textLength[2])
        m_state->m_textLength[2] = hfLimits[N-2];
    }
  }

  //! read the note
  std::vector<long> fieldPos;
  it = entryMap.find("FieldPos");
  if (it != entryMap.end()) { // a list of text pos ( or a size from ? )
    readLongZone(it->second, 4, fieldPos);
  }
  it = entryMap.find("FieldName");
  if (it != entryMap.end())
    readFields(it->second, fieldPos);

  //! read the footenote
  std::vector<long> footnoteDef;
  it = entryMap.find("FootnoteDef");
  if (it != entryMap.end()) { // list of pos in footnote data
    readLongZone(it->second, 4, footnoteDef);
  }
  it = entryMap.find("FootnotePos");
  if (it != entryMap.end()) { // a list of text pos
    readFootnotesPos(it->second, footnoteDef);
  }
  /* CHECKME: this zone seems presents only when FootnoteDef and FootnotePos,
     but what does it means ?
   */
  it = entryMap.find("FootnoteData");
  if (it != entryMap.end()) { // a list of text pos
    readFootnotesData(it->second);
  }
  // we can now update the header/footer limits
  long debHeader = m_state->m_textLength[0]+m_state->m_textLength[1];
  MSWText::PLC plc(MSWText::PLC::HeaderFooter);
  // list Header0,Footer0,Header1,Footer1,...,Footern, 3
  for (size_t i = 0; i+2 < hfLimits.size(); i++) {
    plc.m_id = int(i);
    m_state->m_plcMap.insert(std::multimap<long,MSWText::PLC>::value_type
                             (hfLimits[i]+debHeader, plc));

    MWAWEntry entry;
    entry.setBegin(debHeader+hfLimits[i]);
    entry.setEnd(debHeader+hfLimits[i+1]);
    m_state->m_headerFooterZones.push_back(entry);
  }

  it = entryMap.find("ParagList");
  if (it != entryMap.end())
    m_stylesManager->readPLCList(it->second);
  it = entryMap.find("CharList");
  if (it != entryMap.end())
    m_stylesManager->readPLCList(it->second);

  prepareData();
  return true;
}

////////////////////////////////////////////////////////////
// read the text structure
////////////////////////////////////////////////////////////
bool MSWText::readTextStruct(MSWEntry &entry)
{
  if (entry.length() < 19) {
    MWAW_DEBUG_MSG(("MSWText::readTextStruct: the zone seems to short\n"));
    return false;
  }
  if (!m_stylesManager->readTextStructList(entry))
    return false;
  long pos = m_input->tell();
  libmwaw::DebugStream f;
  int type = (int) m_input->readLong(1);
  if (type != 2) {
    MWAW_DEBUG_MSG(("MSWText::readTextStruct: find odd type %d\n", type));
    return false;
  }
  entry.setParsed(true);
  f << "TextStruct-pos:";
  int sz = (int) m_input->readULong(2);
  long endPos = pos+3+sz;
  if (endPos > entry.end() || (sz%12) != 4) {
    f << "#";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    MWAW_DEBUG_MSG(("MSWText::readTextStruct: can not read the position zone\n"));
    return false;
  }
  int N=sz/12;
  long textLength=m_state->getTotalTextSize();
  std::vector<long> textPos; // checkme
  textPos.resize((size_t) N+1);
  f << "pos=[" << std::hex;
  for (size_t i = 0; i <= size_t(N); i++) {
    textPos[i] = (int) m_input->readULong(4);
    if (i && textPos[i] <= textPos[i-1]) {
      MWAW_DEBUG_MSG(("MSWText::readTextStruct: find backward text pos\n"));
      f << "#" << textPos[i] << ",";
      textPos[i]=textPos[i-1];
    } else {
      if (i != (size_t) N && textPos[i] > textLength) {
        MWAW_DEBUG_MSG(("MSWText::readTextStruct: find a text position which is too big\n"));
        f << "#";
      }
      f << textPos[i] << ",";
    }
  }
  f << std::dec << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  PLC plc(PLC::TextPosition);

  for (int i = 0; i < N; i++) {
    pos = m_input->tell();
    MSWTextInternal::TextEntry tEntry;
    f.str("");
    f<< "TextStruct-pos" << i << ":";
    tEntry.m_pos = (int) textPos[(size_t)i];
    tEntry.m_type = (int) m_input->readULong(1);
    // same value for all entries ?
    tEntry.m_id = (int) m_input->readULong(1);
    long ptr = (long) m_input->readULong(4);
    tEntry.setBegin(ptr);
    tEntry.setLength(textPos[(size_t)i+1]-textPos[(size_t)i]);
    tEntry.m_paragraphId = m_stylesManager->readTextStructParaZone(tEntry.m_extra);
    m_state->m_textposList.push_back(tEntry);
    if (!m_mainParser->isFilePos(ptr)) {
      MWAW_DEBUG_MSG(("MSWText::readTextStruct: find a bad file position \n"));
      f << "#";
    } else {
      plc.m_id = i;
      m_state->m_plcMap.insert(std::multimap<long,PLC>::value_type
                               (textPos[(size_t)i],plc));
    }
    f << tEntry;
    m_input->seek(pos+8, WPX_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  pos = m_input->tell();
  if (pos != entry.end()) {
    ascii().addPos(pos);
    ascii().addNote("TextStruct-pos#");
  }
  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the font name
////////////////////////////////////////////////////////////
bool MSWText::readFontNames(MSWEntry &entry)
{
  if (entry.length() < 2) {
    MWAW_DEBUG_MSG(("MSWText::readFontNames: the zone seems to short\n"));
    return false;
  }

  long pos = entry.begin();
  m_input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  int N = (int) m_input->readULong(2);
  if (N*5+2 > entry.length()) {
    MWAW_DEBUG_MSG(("MSWText::readFontNames: the number of fonts seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  f << "FontNames:" << N;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (int i = 0; i < N; i++) {
    pos = m_input->tell();
    if (pos+5 > entry.end()) {
      m_input->seek(pos, WPX_SEEK_SET);
      MWAW_DEBUG_MSG(("MSWText::readFontNames: the fonts %d seems bad\n", i));
      break;
    }
    f.str("");
    f << "FontNames-" << i << ":";
    int val = (int) m_input->readLong(2);
    if (val) f << "f0=" << val << ",";
    int fId = (int) m_input->readULong(2);
    f << "fId=" << fId << ",";
    int fSz = (int) m_input->readULong(1);
    if (pos +5 > entry.end()) {
      m_input->seek(pos, WPX_SEEK_SET);
      MWAW_DEBUG_MSG(("MSWText::readFontNames: the fonts name %d seems bad\n", i));
      break;
    }
    std::string name("");
    for (int j = 0; j < fSz; j++)
      name += char(m_input->readLong(1));
    if (name.length())
      m_convertissor->setCorrespondance(fId, name);
    f << name;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  pos = m_input->tell();
  if (pos != entry.end()) {
    ascii().addPos(pos);
    ascii().addNote("FontNames#");
  }

  return true;
}

////////////////////////////////////////////////////////////
// read the zone info zone
////////////////////////////////////////////////////////////
bool MSWText::readZoneInfo(MSWEntry &entry)
{
  if (entry.length() < 4 || (entry.length()%10) != 4) {
    MWAW_DEBUG_MSG(("MSWText::readZoneInfo: the zone size seems odd\n"));
    return false;
  }
  long pos = entry.begin();
  entry.setParsed(true);
  m_input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "ZoneInfo:";
  int N=int(entry.length()/10);

  std::vector<long> textPositions;
  f << "[";
  for (int i = 0; i <= N; i++) {
    long textPos = (long) m_input->readULong(4);
    textPositions.push_back(textPos);
    f << std::hex << textPos << std::dec << ",";
  }
  f << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  PLC plc(PLC::ZoneInfo);
  for (int i = 0; i < N; i++) {
    pos = m_input->tell();
    f.str("");
    f << "ZoneInfo-Z" << i << ":" << std::hex << textPositions[(size_t) i] << std::dec << ",";
    MSWTextInternal::ZoneInfo zone;
    zone.m_id = i;
    zone.m_type = (int) m_input->readULong(1); // 0, 20, 40, 60
    zone.m_value = (int) m_input->readLong(1); // f0: -1 up to 9
    zone.m_dim.setX(float(m_input->readULong(2)/1440.));
    zone.m_dim.setY(float(m_input->readLong(2)/72.));
    f << zone;
    m_state->m_zoneList.push_back(zone);

    if (textPositions[(size_t) i] > m_state->m_textLength[0]) {
      MWAW_DEBUG_MSG(("MSWText::readZoneInfo: text positions is bad...\n"));
      f << "#";
    } else {
      plc.m_id=i;
      m_state->m_plcMap.insert(std::multimap<long,PLC>::value_type
                               (textPositions[(size_t) i],plc));
    }

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;

}

////////////////////////////////////////////////////////////
// read the page break
////////////////////////////////////////////////////////////
bool MSWText::readPageBreak(MSWEntry &entry)
{
  if (entry.length() < 18 || (entry.length()%14) != 4) {
    MWAW_DEBUG_MSG(("MSWText::readPageBreak: the zone size seems odd\n"));
    return false;
  }
  long pos = entry.begin();
  entry.setParsed(true);
  m_input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "PageBreak:";
  int N=int(entry.length()/14);
  std::vector<long> textPos; // checkme
  textPos.resize((size_t) N+1);
  for (int i = 0; i <= N; i++) textPos[(size_t) i] = (long) m_input->readULong(4);
  PLC plc(PLC::Page);
  for (int i = 0; i < N; i++) {
    MSWTextInternal::Page page;
    page.m_id = i;
    page.m_type = (int) m_input->readULong(1);
    page.m_values[0] = (int) m_input->readLong(1); // always 0?
    for (int j = 1; j < 3; j++) // always -1, 0
      page.m_values[j] = (int) m_input->readLong(2);
    page.m_page = (int) m_input->readLong(2);
    page.m_values[3] = (int) m_input->readLong(2);
    m_state->m_pageList.push_back(page);

    if (textPos[(size_t)i] > m_state->m_textLength[0]) {
      MWAW_DEBUG_MSG(("MSWText::readPageBreak: text positions is bad...\n"));
      f << "#";
    } else {
      plc.m_id = i;
      m_state->m_plcMap.insert(std::multimap<long,PLC>::value_type
                               (textPos[(size_t)i],plc));
    }
    f << std::hex << "[pos?=" << textPos[(size_t)i] << std::dec << "," << page << "],";
  }
  f << "end=" << std::hex << textPos[(size_t)N] << std::dec << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the footnotes pos + val
////////////////////////////////////////////////////////////
bool MSWText::readFootnotesPos(MSWEntry &entry, std::vector<long> const &noteDef)
{
  if (entry.length() < 4 || (entry.length()%6) != 4) {
    MWAW_DEBUG_MSG(("MSWText::readFootnotesPos: the zone size seems odd\n"));
    return false;
  }
  libmwaw::DebugStream f;
  int N=int(entry.length()/6);
  if (N+2 != int(noteDef.size())) {
    MWAW_DEBUG_MSG(("MSWText::readFootnotesPos: the number N seems odd\n"));
    return false;
  }
  if (version() <= 3) {
    // we must update the different size
    m_state->m_textLength[0] -= (noteDef[(size_t)N+1]+1);
    m_state->m_textLength[1] = (noteDef[(size_t)N+1]+1);
  }
  long pos = entry.begin();
  entry.setParsed(true);
  m_input->seek(pos, WPX_SEEK_SET);
  f << "FootnotePos:";

  std::vector<long> textPos;
  textPos.resize((size_t)N+1);
  for (int i = 0; i <= N; i++)
    textPos[(size_t)i]= (long) m_input->readULong(4);
  long debFootnote = m_state->m_textLength[0];
  PLC plc(PLC::Footnote);
  PLC defPlc(PLC::FootnoteDef);
  for (int i = 0; i < N; i++) {
    MSWTextInternal::Footnote note;
    note.m_id = i;
    note.m_pos.setBegin(debFootnote+noteDef[(size_t)i]);
    note.m_pos.setEnd(debFootnote+noteDef[(size_t)i+1]);
    note.m_value = (int) m_input->readLong(2);
    m_state->m_footnoteList.push_back(note);

    if (textPos[(size_t)i] > m_state->getTotalTextSize()) {
      MWAW_DEBUG_MSG(("MSWText::readFootnotesPos: can not find text position\n"));
      f << "#";
    } else if (noteDef[(size_t)i+1] > m_state->m_textLength[1]) {
      MWAW_DEBUG_MSG(("MSWText::readFootnotesPos: can not find definition position\n"));
      f << "#";
    } else {
      defPlc.m_id = plc.m_id = i;
      m_state->m_plcMap.insert(std::multimap<long,PLC>::value_type
                               (textPos[(size_t)i], plc));
      m_state->m_plcMap.insert(std::multimap<long,PLC>::value_type
                               (note.m_pos.begin(), defPlc));
    }
    f << std::hex << textPos[(size_t)i] << std::dec << ":" << note << ",";
  }
  f << "end=" << std::hex << textPos[(size_t)N] << std::dec << ",";
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the footnotes pos?
////////////////////////////////////////////////////////////
bool MSWText::readFootnotesData(MSWEntry &entry)
{
  if (entry.length() < 4 || (entry.length()%14) != 4) {
    MWAW_DEBUG_MSG(("MSWText::readFootnotesData: the zone size seems odd\n"));
    return false;
  }
  libmwaw::DebugStream f;
  int N=int(entry.length()/14);
  long pos = entry.begin();
  entry.setParsed(true);
  m_input->seek(pos, WPX_SEEK_SET);
  f << "FootnoteData[" << N << "/" << m_state->m_footnoteList.size() << "]:";

  std::vector<long> textPos; // checkme
  textPos.resize((size_t)N+1);
  for (int i = 0; i <= N; i++)
    textPos[(size_t)i]= (long) m_input->readULong(4);
  for (int i = 0; i < N; i++) {
    if (textPos[(size_t)i] > m_state->m_textLength[1]) {
      MWAW_DEBUG_MSG(("MSWText::readFootnotesData: textPositions seems bad\n"));
      f << "#";
    }
    f << "N" << i << "=[";
    if (textPos[(size_t)i])
      f << "pos=" << std::hex << textPos[(size_t)i] << std::dec << ",";
    for (int j = 0; j < 5; j++) { // always 0|4000, -1, 0, id, 0 ?
      int val=(int) m_input->readLong(2);
      if (val && j == 0)
        f << std::hex << val << std::dec << ",";
      else if (val)
        f << val << ",";
      else f << "_,";
    }
    f << "],";
  }
  f << "end=" << std::hex << textPos[(size_t)N] << std::dec << ",";
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the note
////////////////////////////////////////////////////////////
bool MSWText::readFields(MSWEntry &entry, std::vector<long> const &fieldPos)
{
  long pos = entry.begin();
  int N = int(fieldPos.size());
  long textLength = m_state->getTotalTextSize();
  if (N==0) {
    MWAW_DEBUG_MSG(("MSWText::readFields: number of fields is 0\n"));
    return false;
  }
  N--;
  entry.setParsed(true);
  m_input->seek(pos, WPX_SEEK_SET);

  long sz = (long) m_input->readULong(2);
  if (entry.length() != sz) {
    MWAW_DEBUG_MSG(("MSWText::readFields: the zone size seems odd\n"));
    return false;
  }
  libmwaw::DebugStream f, f2;
  f << "FieldName:";
  int const endSize = (version()==5) ? 2 : 1;
  PLC plc(PLC::Field);
  for (int n = 1; n < N; n++) {
    if (m_input->tell() >= entry.end()) {
      MWAW_DEBUG_MSG(("MSWText::readFields: can not find all field\n"));
      break;
    }
    pos = m_input->tell();
    int fSz = (int) m_input->readULong(1);
    if (pos+1+fSz > entry.end()) {
      MWAW_DEBUG_MSG(("MSWText::readFields: can not read a string\n"));
      m_input->seek(pos, WPX_SEEK_SET);
      f << "#";
      break;
    }
    int endSz = fSz < endSize ? 0 : endSize;

    f2.str("");
    std::string text("");
    for (int i = 0; i < fSz-endSz; i++) {
      char c = (char) m_input->readULong(1);
      if (c==0) f2 << '#';
      else text+=c;
    }
    MSWTextInternal::Field field;
    if (!endSz) ;
    else if (version()>=5 && m_input->readULong(1) != 0xc) {
      m_input->seek(-1, WPX_SEEK_CUR);
      for (int i = 0; i < 2; i++) text+=char(m_input->readULong(1));
    } else {
      int id = (int) m_input->readULong(1);
      if (id >= N) {
        if (version()>=5) {
          MWAW_DEBUG_MSG(("MSWText::readFields: find a strange id\n"));
          f2 << "#";
        } else
          text+=char(id);
      } else
        field.m_id = id;
    }
    field.m_text = text;
    field.m_error = f2.str();
    m_state->m_fieldList.push_back(field);

    f << "N" << n << "=" << field << ",";
    if ( fieldPos[(size_t)n] >= textLength) {
      MWAW_DEBUG_MSG(("MSWText::readFields: text positions is bad...\n"));
      f << "#";
    } else {
      plc.m_id = n-1;
      m_state->m_plcMap.insert(std::multimap<long,PLC>::value_type
                               (fieldPos[(size_t)n], plc));
    }
  }
  if (long(m_input->tell()) != entry.end())
    ascii().addDelimiter(m_input->tell(), '|');
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read  a list of ints zone
////////////////////////////////////////////////////////////
bool MSWText::readLongZone(MSWEntry &entry, int sz, std::vector<long> &list)
{
  list.resize(0);
  if (entry.length() < sz || (entry.length()%sz)) {
    MWAW_DEBUG_MSG(("MSWText::readIntsZone: the size of zone %s seems to odd\n", entry.type().c_str()));
    return false;
  }

  long pos = entry.begin();
  m_input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << entry.type() << ":";
  int N = int(entry.length()/sz);
  for (int i = 0; i < N; i++) {
    int val = (int) m_input->readLong(sz);
    list.push_back(val);
    f << std::hex << val << std::dec << ",";
  }

  if (long(m_input->tell()) != entry.end())
    ascii().addDelimiter(m_input->tell(), '|');

  entry.setParsed(true);

  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// sort/prepare data
////////////////////////////////////////////////////////////
void MSWText::prepareData()
{
  long cPos = 0;
  long cEnd = 0;
  for (int i = 0; i < 3; i++) cEnd+=m_state->m_textLength[i];
  if (cEnd <= 0) return;

  MSWStruct::Font actFont, defaultFont, paraFont;
  defaultFont.m_font = m_stylesManager->getDefaultFont();

  long pos = m_state->getFilePos(cPos);
  int textposSize = int(m_state->m_textposList.size());
  std::multimap<long, PLC>::iterator plcIt;

  enum { Page=0, Section, Normal, TextStruct };
  int paragraphId[TextStruct+1]= {-1,-2,-2,-2};
  MSWStruct::Paragraph paragraphList[TextStruct+1];
  libmwaw::DebugStream f, f2;
  PLC::ltstr compare;

  int fontId = -1;
  while (cPos < cEnd) {
    f.str("");
    // first find the list of the plc
    long cNextPos = cEnd;
    std::set<PLC, PLC::ltstr> sortedPLC(compare);
    for (int st = 0; st < 2; st++) {
      std::multimap<long, MSWText::PLC> &map = st==0 ? m_state->m_plcMap : m_state->m_filePlcMap;
      long wPos = st==0 ? cPos : pos;
      plcIt = map.upper_bound(wPos);
      if (plcIt != map.end() && plcIt->first != wPos) {
        long sz = plcIt->first-wPos;
        if (sz>0 && cPos+sz < cNextPos) cNextPos = cPos+sz;
      }
      plcIt = map.find(wPos);
      while (plcIt != map.end() ) {
        if (plcIt->first != wPos) break;
        PLC const &plc = plcIt++->second;
        if (st==0) sortedPLC.insert(plc);
#if DEBUG_PLC
        if (plc.m_type != PLC::TextPosition)
          f << "[" << plc << "],";
#endif
        switch(plc.m_type) {
        case PLC::TextPosition:
          if (plc.m_id >= 0 && plc.m_id < textposSize) {
            MSWTextInternal::TextEntry const &entry=m_state->m_textposList[(size_t) plc.m_id];
            pos = entry.begin();
            if ((paragraphId[TextStruct]=entry.getParagraphId()) < 0)
              paragraphId[TextStruct] = -1;
          } else {
            MWAW_DEBUG_MSG(("MSWText::prepareData: oops can not find textstruct\n"));
            paragraphId[TextStruct] = -1;
          }
          break;
        case PLC::Section:
          paragraphId[Section] = plc.m_id < 0 ? -1 : plc.m_id;
          break;
        case PLC::Paragraph:
          paragraphId[Normal] = plc.m_id < 0 ? -1 : plc.m_id;
          break;
        case PLC::Font:
          fontId = plc.m_id < 0 ? -1 : plc.m_id;
          break;
        case PLC::ZoneInfo:
#if DEBUG_ZONEINFO
          if (plc.m_id >= 0 && plc.m_id < int(m_state->m_zoneList.size())) {
            MSWTextInternal::ZoneInfo const &zone=m_state->m_zoneList[(size_t) plc.m_id];
            f << "Z" << plc.m_id  << "=[" << zone << "],";
          }
#endif
          break;
        case PLC::Field:
        case PLC::Footnote:
        case PLC::FootnoteDef:
        case PLC::HeaderFooter:
        case PLC::Object:
        case PLC::Page:
        default:
          break;
        }
      }
    }
    MSWTextInternal::Property prop;
    prop.m_plcList=std::vector<PLC>(sortedPLC.begin(), sortedPLC.end());
    // update the paragraph list
    int lastModPos = -1;
    for (int i = 0; i <= TextStruct; i++) {
      int pId = paragraphId[i];
      if (pId == -2) continue;
      lastModPos = i;

#if defined(DEBUG_WITH_FILES)
      switch(i) {
      case Section:
#if DEBUG_SECTION
        f << "S" << pId;
        if (pId >= 0) {
          MSWStruct::Section sec;
          m_stylesManager->getSection(MSWTextStyles::TextZone, pId, sec);
          f << "=[" << sec << "],";
        } else
          f << ",";
#endif
        break;
      case TextStruct:
      case Normal:
#if DEBUG_PARAGRAPH
        if (i==TextStruct) f << "t";
        if (pId < 0) f << "P" << "_,";
        else {
          f <<  "P" << pId;
          MSWStruct::Paragraph para;
          m_stylesManager->getParagraph
          ((i==TextStruct) ? MSWTextStyles::TextStructZone : MSWTextStyles::TextZone, pId, para);
          f << "=[";
          para.print(f, m_convertissor);
          f << "],";
        }
#endif
        break;
      case Page:
      default:
        break;
      }
#endif
      if (pId == -1) {
        paragraphList[i]=MSWStruct::Paragraph();
        continue;
      }

      MSWStruct::Paragraph newPara;
      bool ok = true;
      switch(i) {
      case Section:
        m_stylesManager->getSectionParagraph(MSWTextStyles::TextZone, pId,newPara);
        break;
      case Normal:
        ok = m_stylesManager->getParagraph(MSWTextStyles::TextZone, pId, newPara);
        break;
      case TextStruct:
        m_stylesManager->getParagraph(MSWTextStyles::TextStructZone, pId, newPara);
        break;
      case Page:
      default:
        MWAW_DEBUG_MSG(("MSWText::prepareData: find unexpected changed\n"));
      }
      paragraphList[i] = newPara;
      if (!ok) {
        MWAW_DEBUG_MSG(("MSWText::prepareData: can not find paragraph for pos: %lx\n", pos));
      }
    }

    if (lastModPos != -1) {
      MSWStruct::Paragraph newParagraph;
      for (int i = 0; i <= lastModPos; i++) {
        newParagraph.insert(paragraphList[i]);
        paragraphId[i] = -2;
      }
      paraFont = defaultFont;
      newParagraph.getFont(paraFont);
      actFont = paraFont;
      m_state->m_paragraphMap[cPos]=newParagraph;
    }
    if (fontId == -1)
      actFont = paraFont;
    else if (fontId >= 0) {
      MSWStruct::Font newFont;
      m_stylesManager->getFont(MSWTextStyles::TextZone, fontId, newFont);
      actFont = paraFont;
      actFont.insert(newFont);
    }
    if (lastModPos != -1 || fontId != -2) {
      fontId = -2;
      m_state->m_fontMap[cPos] = actFont;
    }
    if (f.str().length()) {
      f2.str("");
      f2 << "TextContent["<<cPos<<"]:" << f.str();
      ascii().addPos(pos);
      ascii().addNote(f2.str().c_str());
      prop.m_debugPrint = true;
    }
    m_state->m_propertyMap[cPos] = prop;
    pos+=(cNextPos-cPos);
    cPos = cNextPos;
  }
}

////////////////////////////////////////////////////////////
// try to read a text entry
////////////////////////////////////////////////////////////
bool MSWText::sendText(MWAWEntry const &textEntry, bool mainZone, bool tableCell)
{
  if (!textEntry.valid()) return false;
  if (!m_listener) {
    MWAW_DEBUG_MSG(("MSWText::sendText: can not find a listener!"));
    return true;
  }
  long cPos = textEntry.begin();
  long debPos = m_state->getFilePos(cPos), pos=debPos;
  m_input->seek(pos, WPX_SEEK_SET);
  long cEnd = textEntry.end();

  MSWStruct::Font actFont;
  actFont.m_font = m_stylesManager->getDefaultFont();

  libmwaw::DebugStream f;
  f << "TextContent[" << cPos << "]:";
  long pictPos = -1;
  while (!m_input->atEOS() && cPos < cEnd) {
    bool newTable = false;
    long cEndPos = cEnd;

    MSWTextInternal::Property *prop = 0;
    std::map<long,MSWTextInternal::Property>::iterator propIt = m_state->m_propertyMap.find(cPos);
    if (propIt != m_state->m_propertyMap.end()) prop = &propIt->second;
    propIt = m_state->m_propertyMap.upper_bound(cPos);
    if (propIt != m_state->m_propertyMap.end() && propIt->first < cEndPos && propIt->first > cPos)
      cEndPos = propIt->first;
    size_t numPLC = prop ? prop->m_plcList.size() : 0;
    for (size_t i = 0; i < numPLC; i++) {
      PLC const &plc = prop->m_plcList[i];
      if (newTable && int(plc.m_type) >= int(PLC::ZoneInfo)) continue;
      switch(plc.m_type) {
      case PLC::TextPosition: {
        if (plc.m_id < 0 || plc.m_id >= int(m_state->m_textposList.size()))
          break;
        MSWTextInternal::TextEntry const &entry=m_state->m_textposList[(size_t) plc.m_id];
        if (pos != entry.begin()) {
          pos = entry.begin();
          m_input->seek(pos, WPX_SEEK_SET);
        }
        break;
      }
      case PLC::Page: {
        if (tableCell) break;
        if (mainZone) m_mainParser->newPage(++m_state->m_actPage);
        break;
      }
      case PLC::Section:
        if (tableCell) break;
        m_stylesManager->sendSection(plc.m_id);
        break;
      case PLC::Field: // some fields ?
#ifdef DEBUG
        m_mainParser->sendFieldComment(plc.m_id);
#endif
        break;
      case PLC::Footnote:
        m_mainParser->sendFootnote(plc.m_id);
        break;
      case PLC::Font:
      case PLC::FootnoteDef:
      case PLC::HeaderFooter:
      case PLC::Object:
      case PLC::Paragraph:
      case PLC::ZoneInfo:
      default:
        break;
      }
    }
    // fixme: add new table here
    if (prop && prop->m_debugPrint) {
      ascii().addPos(debPos);
      ascii().addNote(f.str().c_str());
      f.str("");
      f << "TextContent["<<cPos<<"]:";
      debPos = pos;
    }
    // time to send the table
    if (newTable) {
      MSWEntry tableEntry;
      tableEntry.setBegin(cPos);
      tableEntry.setEnd(cEnd);
      newTable = false;
      if (!sendTable(tableEntry)) {
        //f << "###table,";
        m_input->seek(pos, WPX_SEEK_SET);
      } else {
        cPos = tableEntry.end();
        pos=debPos=m_state->getFilePos(cPos);
        m_input->seek(pos, WPX_SEEK_SET);
        continue;
      }
    }
    if (m_state->m_paragraphMap.find(cPos) != m_state->m_paragraphMap.end())
      m_stylesManager->setProperty(m_state->m_paragraphMap.find(cPos)->second);
    if (m_state->m_fontMap.find(cPos) != m_state->m_fontMap.end()) {
      actFont = m_state->m_fontMap.find(cPos)->second;
      pictPos = actFont.m_picturePos.get();
      m_stylesManager->setProperty(actFont);
    }
    for (long p = cPos; p < cEndPos; p++) {
      int c = (int) m_input->readULong(1);
      cPos++;
      pos++;
      switch (c) {
      case 0x1:
        if (pictPos <= 0) {
          MWAW_DEBUG_MSG(("MSWText::sendText: can not find picture\n"));
          f << "###";
          break;
        }
        m_mainParser->sendPicture(pictPos, int(cPos), MWAWPosition::Char);
        break;
      case 0x7: // FIXME: cell end ?
        m_listener->insertEOL();
        break;
      case 0xc: // end section (ok)
        break;
      case 0x2:
        m_listener->insertField(MWAWContentListener::PageNumber);
        break;
      case 0x6:
        m_listener->insertCharacter('\\');
        break;
      case 0x1e: // unbreaking - ?
        m_listener->insertCharacter('-');
        break;
      case 0x1f: // hyphen
        break;
      case 0x13: // month
      case 0x1a: // month abreviated
      case 0x1b: // checkme month long
        m_listener->insertDateTimeField("%m");
        break;
      case 0x10: // day
      case 0x16: // checkme: day abbreviated
      case 0x17: // checkme: day long
        m_listener->insertDateTimeField("%d");
        break;
      case 0x15: // year
        m_listener->insertDateTimeField("%y");
        break;
      case 0x1d:
        m_listener->insertField(MWAWContentListener::Date);
        break;
      case 0x18: // checkme hour
      case 0x19: // checkme hour
        m_listener->insertDateTimeField("%H");
        break;
      case 0x4:
        m_listener->insertField(MWAWContentListener::Time);
        break;
      case 0x5: // footnote mark (ok)
        break;
      case 0x9:
        m_listener->insertTab();
        break;
      case 0xb: // line break (simple but no a paragraph break ~soft)
        m_listener->insertEOL(true);
        break;
      case 0xd: // line break hard
        m_listener->insertEOL();
        break;
      case 0x11: // command key in help
        m_listener->insertUnicode(0x2318);
        break;
      case 0x14: // apple logo ( note only in private zone)
        m_listener->insertUnicode(0xf8ff);
        break;
      default: {
        int unicode = m_convertissor->unicode(actFont.m_font->id(), (unsigned char)c);
        if (unicode == -1) {
          if (c < 32) {
            MWAW_DEBUG_MSG(("MSWText::sendText: Find odd char %x\n", int(c)));
            f << "#";
          } else
            m_listener->insertCharacter((uint8_t)c);
        } else
          m_listener->insertUnicode((uint32_t) unicode);
        break;
      }
      }
      if (c)
        f << char(c);
      else
        f << "###";
    }
  }

  ascii().addPos(debPos);
  ascii().addNote(f.str().c_str());
  ascii().addPos(m_input->tell());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// try to read a table
////////////////////////////////////////////////////////////
bool MSWText::sendTable(MSWEntry &tableEntry)
{
  return false;
  if (!m_listener) {
    MWAW_DEBUG_MSG(("MSWText::sendTable: can not find a listener!\n"));
    return true;
  }
  long actPos = m_input->tell();
  long cPos = tableEntry.begin();
  long cEnd = tableEntry.end();
  long pos = m_state->getFilePos(cPos);
  bool firstChar = true;
  m_input->seek(pos, WPX_SEEK_SET);
  /* first try to find the limit */
  std::vector<long> limits;
  limits.push_back(cPos);
  std::multimap<long, PLC>::iterator plcIt;
  MSWStruct::Paragraph para;
  float height = 0.0, maxHeight=0.0;
  while (cPos < cEnd && !m_input->atEOS()) {
    bool newPara = false, cellChecked=false;
    plcIt = m_state->m_plcMap.find(cPos);
    while (plcIt != m_state->m_plcMap.end() && plcIt->first == cPos) {
      PLC const &plc = plcIt++->second;
      if (plc.m_type == PLC::TextPosition) {
        MSWTextInternal::TextEntry const &entry=m_state->m_textposList[(size_t) plc.m_id];
        if (pos != entry.begin()) {
          pos = entry.begin();
          m_input->seek(pos, WPX_SEEK_SET);
        }
        cellChecked = false;
        int pId = entry.getParagraphId();
        newPara = pId >=0 &&
                  m_stylesManager->getParagraph(MSWTextStyles::TextStructZone, pId, para);
        if (newPara && para.m_inCell.get())
          cellChecked=true;
      } else if (!firstChar && (plc.m_type == PLC::Page || plc.m_type == PLC::Section || plc.m_type == PLC::FootnoteDef)) {
        MWAW_DEBUG_MSG(("MSWText::sendTable: find a page/section/footnote in a table!\n"));
        m_input->seek(actPos, WPX_SEEK_SET);
        return false;
      }
    }
    plcIt = m_state->m_filePlcMap.find(pos);
    while (plcIt != m_state->m_filePlcMap.end() && plcIt->first == pos) {
      PLC const &plc = plcIt++->second;
      if (plc.m_type != PLC::Paragraph || plc.m_id < 0)
        continue;
      newPara=m_stylesManager->getParagraph(MSWTextStyles::TextZone, plc.m_id, para);
      if (newPara && !para.m_inCell.get() && !cellChecked) {
        MWAW_DEBUG_MSG(("MSWText::sendTable: find a not cell paragraph!\n"));
        m_input->seek(actPos, WPX_SEEK_SET);
        return false;
      }
    }
    if (newPara)
      height += para.m_dim.get()[1];
    else if (!cellChecked && firstChar) {
      MWAW_DEBUG_MSG(("MSWText::sendTable: can not find first cell begin...\n"));
      m_input->seek(actPos, WPX_SEEK_SET);
      return false;
    }
    cPos++;
    pos++;
    firstChar = false;
    int c = (int) m_input->readULong(1);
    if (c!=0x7) continue;
    if (height > maxHeight) maxHeight=height;
    limits.push_back(cPos);
    if (para.m_tableDef.get()) break;
  }
  size_t numCols = para.m_tableColumns->size();
  size_t numLimits = limits.size();
  if (!numCols || numLimits < numCols+1 || (numLimits%numCols)!=1) {
    MWAW_DEBUG_MSG(("MSWText::sendTable: problem with the number of columns: find %ld %ld\n", long(limits.size())-1, long(para.m_tableColumns->size())));
    m_input->seek(actPos, WPX_SEEK_SET);
    return false;
  }
  // checkme can we really have many row ( or we read something bad)
  size_t numRows = limits.size()/numCols;
  for (size_t r = 0; r < numRows; r++) {
    for (size_t c = r*numCols; c < (r+1)*numCols; c++) {
      MSWEntry textData;
      textData.setBegin(limits[c]);
      textData.setEnd(limits[c+1]-1);
      if (textData.length()<=0) {
        m_listener->insertEOL();
        continue;
      }
      sendText(textData, false, true);
      m_listener->insertEOL();
    }
  }
  tableEntry.setEnd(limits[numLimits-1]);
  return true;
}

bool MSWText::sendMainText()
{
  MWAWEntry entry;
  entry.setBegin(0);
  entry.setLength(m_state->m_textLength[0]);
  sendText(entry, true);
  return true;
}

bool MSWText::sendFootnote(int id)
{
  if (!m_listener) return true;
  if (id < 0 || id >= int(m_state->m_footnoteList.size())) {
    MWAW_DEBUG_MSG(("MSWText::sendFootnote: can not find footnote %d\n", id));
    m_listener->insertCharacter(' ');
    return false;
  }
  MSWTextInternal::Footnote &footnote = m_state->m_footnoteList[(size_t) id];
  if (footnote.m_pos.isParsed())
    m_listener->insertCharacter(' ');
  else
    sendText(footnote.m_pos, false);
  footnote.m_pos.setParsed();
  return true;
}

bool MSWText::sendFieldComment(int id)
{
  if (!m_listener) return true;
  if (id < 0 || id >= int(m_state->m_fieldList.size())) {
    MWAW_DEBUG_MSG(("MSWText::sendFieldComment: can not find field %d\n", id));
    m_listener->insertCharacter(' ');
    return false;
  }
  MSWStruct::Font defFont;
  defFont.m_font = m_stylesManager->getDefaultFont();
  m_stylesManager->setProperty(defFont);
  m_stylesManager->sendDefaultParagraph();
  std::string const &text = m_state->m_fieldList[(size_t) id].m_text;
  if (!text.length()) m_listener->insertCharacter(' ');
  for (size_t c = 0; c < text.length(); c++) {
    int unicode = m_convertissor->unicode(defFont.m_font->id(), (unsigned char) text[c]);
    if (unicode == -1) {
      if (text[c] < 32) {
        MWAW_DEBUG_MSG(("MSWText::sendFieldComment: Find odd char %x\n", int(text[c])));
        m_listener->insertCharacter(' ');
      } else
        m_listener->insertCharacter((uint8_t) text[c]);
    } else
      m_listener->insertUnicode((uint32_t) unicode);
  }
  return true;
}

void MSWText::flushExtra()
{
#ifdef DEBUG
  if (m_state->m_textLength[1]) {
    for (size_t i = 0; i < m_state->m_footnoteList.size(); i++) {
      MSWTextInternal::Footnote &footnote = m_state->m_footnoteList[i];
      if (footnote.m_pos.isParsed()) continue;
      sendText(footnote.m_pos, false);
      footnote.m_pos.setParsed();
    }
  }
#endif
}


// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
