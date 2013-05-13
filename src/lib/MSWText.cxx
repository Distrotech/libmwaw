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

#include <libwpd/libwpd.h>

#include "MWAWCell.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"

#include "MSWParser.hxx"
#include "MSWStruct.hxx"

#include "MSWText.hxx"

#define DEBUG_FONT 1
#define DEBUG_PLC 1
#define DEBUG_PAGE 1
#define DEBUG_PARAGRAPH 1
#define DEBUG_SECTION 1
#define DEBUG_PARAGRAPHINFO 1

/** Internal: the structures of a MSWText */
namespace MSWTextInternal
{

////////////////////////////////////////
//! Internal: the entry of MSWParser
struct TextEntry : public MWAWEntry {
  TextEntry() : MWAWEntry(), m_pos(-1), m_id(0), m_type(0), m_paragraphId(-1), m_complex(false), m_extra("") {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, TextEntry const &entry) {
    if (entry.m_pos>=0) o << "textPos=" << entry.m_pos << ",";
    o << "styleId?=" << entry.m_id << ",";
    if (entry.m_complex) o << "complex,";
    if (entry.m_paragraphId >= 0) o << "tP" << entry.m_paragraphId << ",";
    // checkme
    if (entry.m_type&1)
      o << "noEndPara,";
    if (entry.m_type&2)
      o << "paphNil,";
    if (entry.m_type&4)
      o << "dirty,";
    switch(entry.m_type&0xF8) {
    case 0x80: // reset font ?
    case 0: // keep font ?
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
  //! a flag to know if we read a complex or a simple PRM
  bool m_complex;
  //! extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the page
struct Page {
  //! constructor
  Page() : m_id(-1), m_type(0), m_page(-1), m_paragraphId(-2), m_error("") {
    for (int i = 0; i < 4; i++) m_values[i] = 0;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Page const &page) {
    if (page.m_id >= 0) o << "Pg" << page.m_id << ":";
    else o << "Pg_:";
    if (page.m_paragraphId >= 0) o << "P" << page.m_paragraphId << ",";
    if (page.m_page != page.m_id+1) o << "page=" << page.m_page << ",";
    if (page.m_type&0x10)
      o << "right,";
    // find also page.m_type&0x40 : pageDirty?
    if (page.m_type&0xEF)
      o << "type=" << std::hex << (page.m_type&0xEF) << std::dec << ",";
    for (int i = 0; i < 3; i++) {
      if (page.m_values[i])
        o << "f" << i << "=" << page.m_values[i] << ",";
    }
    if (page.m_values[3])
      o << "f3=" << std::hex << page.m_values[3] << std::dec << ",";
    if (page.m_error.length()) o << page.m_error << ",";
    return o;
  }
  //! the identificator
  int m_id;
  //! the type
  int m_type;
  //! the page number
  int m_page;
  //! the paragraph id
  int m_paragraphId;
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
    if (note.m_id >= 0) o << "Fn" << note.m_id << ":";
    else o << "Fn_:";
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
  Property() : m_pos(-1), m_plcList(), m_debugPrint(false), m_cellsPos() {}
  bool isTable() const {
    return m_cellsPos.size();
  }
  //! the character position
  long m_pos;
  //! the list of plc
  std::vector<MSWText::PLC> m_plcList;
  //! a flag to know if we have print data
  bool m_debugPrint;
  //! the list of beginning of cell positions for the table and the def position
  std::vector<long> m_cellsPos;
};

////////////////////////////////////////
//! Internal: the state of a MSWParser
struct State {
  //! constructor
  State() : m_version(-1), m_bot(0x100), m_headerFooterZones(), m_textposList(),
    m_plcMap(), m_filePlcMap(), m_fontMap(), m_paragraphMap(), m_propertyMap(),
    m_paraInfoList(), m_pageList(), m_fieldList(), m_footnoteList(), m_actPage(0), m_numPages(-1) {
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

  //! the list of paragraph info modifier
  std::vector<MSWStruct::ParagraphInfo> m_paraInfoList;

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
MSWText::MSWText(MSWParser &parser) :
  m_parserState(parser.getParserState()), m_state(new MSWTextInternal::State),
  m_stylesManager(), m_mainParser(&parser)
{
  m_stylesManager.reset(new MSWTextStyles(*this));
}

MSWText::~MSWText()
{ }

int MSWText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}

int MSWText::numPages() const
{
  m_state->m_numPages = int(m_state->m_pageList.size());
  return m_state->m_numPages;
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
  case MSWText::PLC::ParagraphInfo:
    o << "Pi";
    break;
  case MSWText::PLC::Section:
    o << "S";
    break;
  case MSWText::PLC::Footnote:
    o << "Fn";
    break;
  case MSWText::PLC::FootnoteDef:
    o << "vFn";
    break;
  case MSWText::PLC::Field:
    o << "Field";
    break;
  case MSWText::PLC::Page:
    o << "Pg";
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

bool MSWText::readHeaderTextLength()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long endPos = pos+12;
  if (!m_mainParser->isFilePos(endPos))
    return false;
  for (int i = 0; i < 3; i++)
    m_state->m_textLength[i]= (long) input->readULong(4);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "FileHeader(textLength):text="
    << std::hex << m_state->m_textLength[0] << ",";
  if (m_state->m_textLength[1])
    f << "footnote=" << m_state->m_textLength[1] << ",";
  if (m_state->m_textLength[2])
    f << "headerFooter=" << m_state->m_textLength[2] << ",";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(endPos);
  ascFile.addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// try to find the different zone
////////////////////////////////////////////////////////////
bool MSWText::createZones(long bot)
{
  // int const vers=version();
  m_state->m_bot = bot;

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
  it = entryMap.find("ParaInfo");
  if (it != entryMap.end())
    readParagraphInfo(it->second);
  it = entryMap.find("Section");
  if (it != entryMap.end())
    m_stylesManager->readSection(it->second);

  //! read the header footer limit
  it = entryMap.find("HeaderFooter");
  std::vector<long> hfLimits;
  if (it != entryMap.end()) {
    readLongZone(it->second, 4, hfLimits);

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
// read the text structure ( the PieCe Descriptors : plcfpcd )
////////////////////////////////////////////////////////////
bool MSWText::readTextStruct(MSWEntry &entry)
{
  if (entry.length() < 19) {
    MWAW_DEBUG_MSG(("MSWText::readTextStruct: the zone seems to short\n"));
    return false;
  }
  if (!m_stylesManager->readTextStructList(entry))
    return false;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  int type = (int) input->readLong(1);
  if (type != 2) {
    MWAW_DEBUG_MSG(("MSWText::readTextStruct: find odd type %d\n", type));
    return false;
  }
  entry.setParsed(true);
  f << "TextStruct-pos:";
  int sz = (int) input->readULong(2);
  long endPos = pos+3+sz;
  if (endPos > entry.end() || (sz%12) != 4) {
    f << "#";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    MWAW_DEBUG_MSG(("MSWText::readTextStruct: can not read the position zone\n"));
    return false;
  }
  int N=sz/12;
  long textLength=m_state->getTotalTextSize();
  std::vector<long> textPos; // checkme
  textPos.resize((size_t) N+1);
  f << "pos=[" << std::hex;
  for (size_t i = 0; i <= size_t(N); i++) {
    textPos[i] = (int) input->readULong(4);
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
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  PLC plc(PLC::TextPosition);

  for (int i = 0; i < N; i++) {
    pos = input->tell();
    MSWTextInternal::TextEntry tEntry;
    f.str("");
    f<< "TextStruct-pos" << i << ":";
    tEntry.m_pos = (int) textPos[(size_t)i];
    tEntry.m_type = (int) input->readULong(1);
    // fN internal...
    tEntry.m_id = (int) input->readULong(1);
    long ptr = (long) input->readULong(4);
    tEntry.setBegin(ptr);
    tEntry.setLength(textPos[(size_t)i+1]-textPos[(size_t)i]);
    tEntry.m_paragraphId = m_stylesManager->readPropertyModifier(tEntry.m_complex, tEntry.m_extra);
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
    input->seek(pos+8, WPX_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  pos = input->tell();
  if (pos != entry.end()) {
    ascFile.addPos(pos);
    ascFile.addNote("TextStruct-pos#");
  }
  ascFile.addPos(entry.end());
  ascFile.addNote("_");
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
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  int N = (int) input->readULong(2);
  if (N*5+2 > entry.length()) {
    MWAW_DEBUG_MSG(("MSWText::readFontNames: the number of fonts seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  f << "FontNames:" << N;
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    if (pos+5 > entry.end()) {
      input->seek(pos, WPX_SEEK_SET);
      MWAW_DEBUG_MSG(("MSWText::readFontNames: the fonts %d seems bad\n", i));
      break;
    }
    f.str("");
    f << "FontNames-" << i << ":";
    int val = (int) input->readLong(2);
    if (val) f << "f0=" << val << ",";
    int fId = (int) input->readULong(2);
    f << "fId=" << fId << ",";
    int fSz = (int) input->readULong(1);
    if (pos +5 > entry.end()) {
      input->seek(pos, WPX_SEEK_SET);
      MWAW_DEBUG_MSG(("MSWText::readFontNames: the fonts name %d seems bad\n", i));
      break;
    }
    std::string name("");
    for (int j = 0; j < fSz; j++)
      name += char(input->readLong(1));
    if (name.length())
      m_parserState->m_fontConverter->setCorrespondance(fId, name);
    f << name;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  pos = input->tell();
  if (pos != entry.end()) {
    ascFile.addPos(pos);
    ascFile.addNote("FontNames#");
  }

  return true;
}

////////////////////////////////////////////////////////////
// read the zone info zone
////////////////////////////////////////////////////////////
bool MSWText::readParagraphInfo(MSWEntry entry)
{
  int vers=version();
  if (vers<=3) {
    MWAW_DEBUG_MSG(("MSWText::readParagraphInfo: does not know how to read a paragraphInfo in v3 or less\n"));
    return false;
  }
  if (entry.length() < 4 || (entry.length()%10) != 4) {
    MWAW_DEBUG_MSG(("MSWText::readParagraphInfo: the zone size seems odd\n"));
    return false;
  }
  entry.setParsed(true);

  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "ParaInfo:";
  int N=int(entry.length()/10);

  std::vector<long> textPositions;
  f << "[";
  for (int i = 0; i <= N; i++) {
    long textPos = (long) input->readULong(4);
    textPositions.push_back(textPos);
    f << std::hex << textPos << std::dec << ",";
  }
  f << "],";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  PLC plc(PLC::ParagraphInfo);
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    f << "ParaInfo-Pi" << i << ":" << std::hex << textPositions[(size_t) i] << std::dec << ",";
    MSWStruct::ParagraphInfo paraMod;
    if (!paraMod.read(input, pos+6, vers))
      f << "###";
    f << paraMod;
    m_state->m_paraInfoList.push_back(paraMod);

    if (textPositions[(size_t) i] > m_state->m_textLength[0]) {
      MWAW_DEBUG_MSG(("MSWText::readParagraphInfo: text positions is bad...\n"));
      f << "#";
    } else {
      plc.m_id=i;
      m_state->m_plcMap.insert(std::multimap<long,PLC>::value_type
                               (textPositions[(size_t) i],plc));
    }
    input->seek(pos+6, WPX_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  ascFile.addPos(entry.end());
  ascFile.addNote("_");
  return true;

}

////////////////////////////////////////////////////////////
// read the page break
////////////////////////////////////////////////////////////
bool MSWText::readPageBreak(MSWEntry &entry)
{
  int const vers = version();
  int const fSz = vers <= 3 ? 8 : 10;
  if (entry.length() < fSz+8 || (entry.length()%(fSz+4)) != 4) {
    MWAW_DEBUG_MSG(("MSWText::readPageBreak: the zone size seems odd\n"));
    return false;
  }
  long pos = entry.begin();
  entry.setParsed(true);
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "PageBreak:";
  int N=int(entry.length()/(fSz+4));
  std::vector<long> textPos; // checkme
  textPos.resize((size_t) N+1);
  for (int i = 0; i <= N; i++) textPos[(size_t) i] = (long) input->readULong(4);
  PLC plc(PLC::Page);
  for (int i = 0; i < N; i++) {
    MSWTextInternal::Page page;
    page.m_id = i;
    page.m_type = (int) input->readULong(1);
    page.m_values[0] = (int) input->readLong(1); // always 0,1,2
    for (int j = 1; j < 3; j++) // always -1, 0
      page.m_values[j] = (int) input->readLong(2);
    page.m_page = (int) input->readLong(2);
    if (vers > 3)
      page.m_values[3] = (int) input->readLong(2);
    m_state->m_pageList.push_back(page);

    if (textPos[(size_t)i] > m_state->m_textLength[0]) {
      MWAW_DEBUG_MSG(("MSWText::readPageBreak: text positions is bad...\n"));
      f << "#";
    } else {
      plc.m_id = i;
      m_state->m_plcMap.insert(std::multimap<long,PLC>::value_type
                               (textPos[(size_t)i],plc));
    }
    f << "[pos=" << textPos[(size_t)i] << "," << page << "],";
  }
  f << "end=" << std::hex << textPos[(size_t)N] << std::dec << ",";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  ascFile.addPos(entry.end());
  ascFile.addNote("_");
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
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  int N=int(entry.length()/6);
  if (N+2 != int(noteDef.size())) {
    MWAW_DEBUG_MSG(("MSWText::readFootnotesPos: the number N seems odd\n"));
    return false;
  }
  long pos = entry.begin();
  entry.setParsed(true);
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos, WPX_SEEK_SET);
  f << "FootnotePos:";

  std::vector<long> textPos;
  textPos.resize((size_t)N+1);
  for (int i = 0; i <= N; i++)
    textPos[(size_t)i]= (long) input->readULong(4);
  long debFootnote = m_state->m_textLength[0];
  PLC plc(PLC::Footnote);
  PLC defPlc(PLC::FootnoteDef);
  for (int i = 0; i < N; i++) {
    MSWTextInternal::Footnote note;
    note.m_id = i;
    note.m_pos.setBegin(debFootnote+noteDef[(size_t)i]);
    note.m_pos.setEnd(debFootnote+noteDef[(size_t)i+1]);
    note.m_value = (int) input->readLong(2);
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
    f << std::hex << textPos[(size_t)i] << std::dec << ":" << note;
  }
  f << "end=" << std::hex << textPos[(size_t)N] << std::dec << ",";
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(entry.end());
  ascFile.addNote("_");
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
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  int N=int(entry.length()/14);
  long pos = entry.begin();
  entry.setParsed(true);
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos, WPX_SEEK_SET);
  f << "FootnoteData[" << N << "/" << m_state->m_footnoteList.size() << "]:";

  std::vector<long> textPos; // checkme
  textPos.resize((size_t)N+1);
  for (int i = 0; i <= N; i++)
    textPos[(size_t)i]= (long) input->readULong(4);
  for (int i = 0; i < N; i++) {
    if (textPos[(size_t)i] > m_state->m_textLength[1]) {
      MWAW_DEBUG_MSG(("MSWText::readFootnotesData: textPositions seems bad\n"));
      f << "#";
    }
    f << "N" << i << "=[";
    if (textPos[(size_t)i])
      f << "pos=" << std::hex << textPos[(size_t)i] << std::dec << ",";
    for (int j = 0; j < 5; j++) { // always 0|4000, -1, 0, id, 0 ?
      int val=(int) input->readLong(2);
      if (val && j == 0)
        f << std::hex << val << std::dec << ",";
      else if (val)
        f << val << ",";
      else f << "_,";
    }
    f << "],";
  }
  f << "end=" << std::hex << textPos[(size_t)N] << std::dec << ",";
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(entry.end());
  ascFile.addNote("_");
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
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos, WPX_SEEK_SET);

  long sz = (long) input->readULong(2);
  if (entry.length() != sz) {
    MWAW_DEBUG_MSG(("MSWText::readFields: the zone size seems odd\n"));
    return false;
  }
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f, f2;
  f << "FieldName:";
  int const endSize = (version()==5) ? 2 : 1;
  PLC plc(PLC::Field);
  for (int n = 1; n < N; n++) {
    if (input->tell() >= entry.end()) {
      MWAW_DEBUG_MSG(("MSWText::readFields: can not find all field\n"));
      break;
    }
    pos = input->tell();
    int fSz = (int) input->readULong(1);
    if (pos+1+fSz > entry.end()) {
      MWAW_DEBUG_MSG(("MSWText::readFields: can not read a string\n"));
      input->seek(pos, WPX_SEEK_SET);
      f << "#";
      break;
    }
    int endSz = fSz < endSize ? 0 : endSize;

    f2.str("");
    std::string text("");
    for (int i = 0; i < fSz-endSz; i++) {
      char c = (char) input->readULong(1);
      if (c==0) f2 << '#';
      else text+=c;
    }
    MSWTextInternal::Field field;
    if (!endSz) ;
    else if (version()>=5 && input->readULong(1) != 0xc) {
      input->seek(-1, WPX_SEEK_CUR);
      for (int i = 0; i < 2; i++) text+=char(input->readULong(1));
    } else {
      int id = (int) input->readULong(1);
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
  if (long(input->tell()) != entry.end())
    ascFile.addDelimiter(input->tell(), '|');
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(entry.end());
  ascFile.addNote("_");
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
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << entry.type() << ":";
  int N = int(entry.length()/sz);
  for (int i = 0; i < N; i++) {
    int val = (int) input->readLong(sz);
    list.push_back(val);
    f << std::hex << val << std::dec << ",";
  }

  if (long(input->tell()) != entry.end())
    ascFile.addDelimiter(input->tell(), '|');

  entry.setParsed(true);

  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());

  ascFile.addPos(entry.end());
  ascFile.addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// sort/prepare data
////////////////////////////////////////////////////////////
bool MSWText::findParaPosAndStyles(std::map<long, int> &posStyleMap)
{
  posStyleMap.clear();

  int vers = version();
  long cPos = 0;
  long cEnd = 0;
  for (int i = 0; i < 3; i++) cEnd+=m_state->m_textLength[i];
  if (cEnd <= 0) return false;

  long debPos = m_state->getFilePos(cPos), pos=debPos;
  int textposSize = int(m_state->m_textposList.size());

  // first build the paragraph limit
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos, WPX_SEEK_SET);

  int actStyle=-1;
  long lastCPos=0;
  posStyleMap[lastCPos]=actStyle;
  while (!input->atEOS() && cPos < cEnd) {
    std::multimap<long, MSWText::PLC>::const_iterator plcIt =
      m_state->m_plcMap.lower_bound(cPos);

    enum { Paragraph=0, TextStruct, NumEnums=TextStruct+1 };
    int modIdList[NumEnums]= {-2,-2};

    while (plcIt != m_state->m_plcMap.end() && plcIt->first==cPos) {
      MSWText::PLC const &plc = plcIt++->second;
      if (plc.m_type == PLC::Paragraph)
        modIdList[Paragraph]=plc.m_id;
      if (plc.m_type != PLC::TextPosition)
        continue;
      if (plc.m_id < 0 || plc.m_id >= textposSize)
        continue;
      MSWTextInternal::TextEntry const &textEntry=
        m_state->m_textposList[(size_t) plc.m_id];
      modIdList[TextStruct]=textEntry.getParagraphId();
      pos =textEntry.begin();
      input->seek(pos, WPX_SEEK_SET);
    }
    pos = input->tell();
    plcIt = m_state->m_filePlcMap.lower_bound(pos);
    while (plcIt != m_state->m_plcMap.end() && plcIt->first==pos) {
      MSWText::PLC const &plc = plcIt++->second;
      if (plc.m_type == PLC::Paragraph)
        modIdList[Paragraph]=plc.m_id;
    }

    for (int i = 0; i < NumEnums; i++) {
      if (modIdList[i] < 0)
        continue;
      MSWStruct::Paragraph para(vers);
      m_stylesManager->getParagraph
      ((i==TextStruct) ? MSWTextStyles::TextStructZone : MSWTextStyles::TextZone, modIdList[i], para);
      if (para.m_styleId.isSet() && *para.m_styleId>-100) {
        actStyle = *para.m_styleId;
        posStyleMap[lastCPos]=actStyle;
      }
    }

    int c=int(input->readLong(1));
    if (c==0xd || c==0x7) {
      lastCPos = cPos+1;
      posStyleMap[lastCPos]=actStyle;
    }
    cPos++;
  }

  return true;
}

void MSWText::prepareData()
{
  long cPos = 0;
  long cEnd = 0;
  int const vers = version();
  for (int i = 0; i < 3; i++) cEnd+=m_state->m_textLength[i];
  if (cEnd <= 0) return;

  MSWStruct::Font defaultFont;

  long debPos = m_state->getFilePos(cPos), pos=debPos;
  int textposSize = int(m_state->m_textposList.size());

  // first build the paragraph limit
  std::map<long, int> posStyleMap;
  if (!findParaPosAndStyles(posStyleMap))
    return;

  struct LocalState {
    //! constructor
    LocalState(int versi, MSWTextStyles &styleManager) :
      m_version(versi), m_styleManager(styleManager), m_actualStyle(-1),
      m_actualFont(), m_actualPara(versi), m_paraFont(), m_previousFont(),
      m_stylePara(versi), m_styleFont(), m_isLocalFont(false) {
    }

    //! set the current style
    bool setStyle(int style) {
      if (style==m_actualStyle)
        return false;
      m_stylePara=MSWStruct::Paragraph(m_version);
      m_styleManager.getParagraph(MSWTextStyles::StyleZone,style, m_stylePara);
      m_styleFont=MSWStruct::Font();
      m_styleManager.getFont(MSWTextStyles::StyleZone, style, m_styleFont);
      m_actualStyle=style;

      m_actualPara=m_stylePara;
      m_actualPara.getFont(m_paraFont);
      m_previousFont=m_actualFont=m_paraFont;
      m_isLocalFont = false;
      return true;
    }
    //! update the character font
    void updateFont(MSWStruct::Font const &charFont, bool isLocal) {
      m_actualFont = m_paraFont;
      m_actualFont.insert(charFont, &m_styleFont);
      m_isLocalFont=isLocal;
      if (!isLocal)
        m_previousFont=m_actualFont;
    }
    //! update the paragraph
    void setParagraph(MSWStruct::Paragraph const &para) {
      if (para.m_styleId.isSet() && *para.m_styleId!=m_actualStyle) {
        m_previousFont=m_paraFont;
        m_isLocalFont = true;
        return;
      }
      m_actualPara = m_stylePara;
      m_actualPara.insert(para);

      m_paraFont=MSWStruct::Font();
      m_actualPara.getFont(m_paraFont);
      m_previousFont=m_paraFont;
      m_isLocalFont = true;
    }
    //! modify a paragraph
    void addModifier(MSWStruct::Paragraph const &modifier, bool isLocal) {
      m_actualPara.insert(modifier);
      MSWStruct::Font modifierFont;
      if (modifier.getFont(modifierFont))
        m_actualFont.insert(modifierFont, &m_styleFont);
      if (!isLocal)
        m_previousFont=m_actualFont;
    }
    //! modify a paraghaph info
    void addModifier(MSWStruct::ParagraphInfo const &info) {
      m_actualPara.m_info.insert(info);
    }

    //! update the state to new state, returns true if we do some change
    bool updateState(bool local) {
      bool res=m_isLocalFont;
      if (m_isLocalFont)
        m_actualFont=m_previousFont;
      m_isLocalFont = local;
      if (m_isLocalFont)
        m_previousFont=m_actualFont;
      return res;
    }
    //! the actual version
    int m_version;
    //! the style manager
    MSWTextStyles &m_styleManager;

    //! the actual style
    int m_actualStyle;
    //! the actual font
    MSWStruct::Font m_actualFont;
    //! the actual paragraph
    MSWStruct::Paragraph m_actualPara;
    //! the paragraph font
    MSWStruct::Font m_paraFont;
    //! the previous font (before a push)
    MSWStruct::Font m_previousFont;

    //! the style paragraph
    MSWStruct::Paragraph m_stylePara;
    //! the actual style font
    MSWStruct::Font m_styleFont;
    //! a local font
    bool m_isLocalFont;
  };
  LocalState actState(vers, *m_stylesManager.get());
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f, f2;
  PLC::ltstr compare;

  std::multimap<long, PLC>::iterator plcIt;
  cPos = 0;
  pos=debPos;
  long paraPos = cPos;
  while (cPos < cEnd) {
    f.str("");
    // first find the list of the plc
    long cNextPos = cEnd;
    bool fontChanged=false, paraChanged=false;
    enum { Paragraph=0, Font, ParagraphInfo, TextStruct, NumEnums=TextStruct+1 };
    int modIdList[NumEnums]= {-2,-2,-2,-2};
    MSWTextInternal::TextEntry textEntry;

    std::set<PLC, PLC::ltstr> sortedPLC(compare);
    for (int st = 0; st < 2; st++) {
      std::multimap<long, MSWText::PLC> &map = st==0 ? m_state->m_plcMap : m_state->m_filePlcMap;
      long wPos = st==0 ? cPos : pos;
      plcIt = map.upper_bound(wPos);
      if (plcIt != map.end() && plcIt->first != wPos) {
        long sz = plcIt->first-wPos;
        if (sz>0 && cPos+sz < cNextPos) cNextPos = cPos+sz;
      }
      plcIt = map.lower_bound(wPos);
      while (plcIt != map.end() ) {
        if (plcIt->first != wPos) break;
        PLC const &plc = plcIt++->second;
        if (st==0) sortedPLC.insert(plc);
#if DEBUG_PLC
        if (plc.m_type != PLC::TextPosition)
          f << "[" << plc << "],";
#endif

        int pId = plc.m_id;
        switch(plc.m_type) {
        case PLC::TextPosition:
          modIdList[TextStruct] = pId < 0 ? -1 : pId;
          if (pId < 0 || pId > textposSize) {
            MWAW_DEBUG_MSG(("MSWText::prepareData: oops can not find textstruct!!!!\n"));
            modIdList[TextStruct] = -2;
            f << "[###tP" << pId << "]";
          } else {
            textEntry=m_state->m_textposList[(size_t) pId];
            pos = textEntry.begin();
          }
          break;
        case PLC::Section:
#if defined(DEBUG_WITH_FILES) && DEBUG_SECTION
          if (pId >= 0) {
            MSWStruct::Section sec;
            m_stylesManager->getSection(MSWTextStyles::TextZone, pId, sec);
            f << "S" << pId << "=[" << sec << "],";
          } else
            f << "S_,";
#endif
          break;
        case PLC::ParagraphInfo:
          modIdList[ParagraphInfo] = pId < 0 ? -1 : pId;
          break;
        case PLC::Page:
#if defined(DEBUG_WITH_FILES) && DEBUG_PAGE
          if (pId  >= 0 && pId < int(m_state->m_pageList.size()))
            f << "Pg" << pId << "=[" << m_state->m_pageList[(size_t) pId] << "],";
          else
            f << "Pg_,";
#endif
          break;
        case PLC::Paragraph:
          modIdList[Paragraph] = pId < 0 ? -1 : pId;
          break;
        case PLC::Font:
          modIdList[Font] = pId < 0 ? -1 : pId;
          break;
        case PLC::Field:
        case PLC::Footnote:
        case PLC::FootnoteDef:
        case PLC::HeaderFooter:
        case PLC::Object:
        default:
          break;
        }
      }
    }

    MSWTextInternal::Property prop;
    prop.m_pos = pos;
    prop.m_plcList=std::vector<PLC>(sortedPLC.begin(), sortedPLC.end());

    std::map<long,int>::iterator pIt=posStyleMap.find(cPos);
    bool newLine = pIt!=posStyleMap.end();
    if (newLine) {
      paraPos = cPos;
      paraChanged=actState.setStyle(pIt->second);
    }
    // update the paragraph list
    for (size_t i = 0; i < NumEnums; i++) {
      if (i==1 && modIdList[TextStruct]!=-2) {
        /**FIXME: normally, must depend on some textEntry data flags,
           I tested with textEntry.m_type&0x80
           but in average, the results seem slightly worse :-~ */
        fontChanged |= actState.updateState(true);
      }
      int pId = modIdList[i];
      if (pId == -2) continue;

      int paraId=-2;
      switch(i) {
      case TextStruct: {
        if (pId < 0 || pId >= textposSize) {
          MWAW_DEBUG_MSG(("MSWText::prepareData: oops can not find textstruct\n"));
          break;
        }
        paraId=textEntry.getParagraphId();
        break;
      }
      case Paragraph:
        paraId=pId;
        break;
      case Font: {
        MSWStruct::Font newChar;
        if (pId >= 0 && m_stylesManager->getFont(MSWTextStyles::TextZone, pId, newChar)) {
#if defined(DEBUG_WITH_FILES) && DEBUG_FONT
          f << "F" << pId << "=[" << newChar.m_font->getDebugString(m_parserState->m_fontConverter) << newChar << "],";
        } else {
          f << "F_,";
#endif
        }
        actState.updateFont(newChar, modIdList[TextStruct]!=-2);
        fontChanged=true;
        break;
      }
      case ParagraphInfo:
        if (pId >= 0 && pId < int(m_state->m_paraInfoList.size())) {
          MSWStruct::ParagraphInfo info=m_state->m_paraInfoList[(size_t) pId];
          actState.addModifier(info);
#if defined(DEBUG_WITH_FILES) && DEBUG_PARAGRAPHINFO
          f << "Pi" << pId  << "=[" << info << "],";
#endif
        } else {
#if defined(DEBUG_WITH_FILES) && DEBUG_PARAGRAPHINFO
          f << "Pi_,";
#endif
        }
        break;
      default:
        break;
      }
      if (paraId==-2)
        continue;

      MSWStruct::Paragraph para(vers);
      if (paraId >= 0)
        m_stylesManager->getParagraph
        ((i==TextStruct) ? MSWTextStyles::TextStructZone : MSWTextStyles::TextZone, paraId, para);
#if defined(DEBUG_WITH_FILES) && DEBUG_PARAGRAPH
      if (i==TextStruct) f << "tP";
      else f << "P";
      if (paraId < 0)
        f << "_,";
      else {
        f << paraId << "=[";
        para.print(f, m_parserState->m_fontConverter);
        f << "],";
      }
#endif

      if (i==Paragraph)
        actState.setParagraph(para);
      else
        actState.addModifier(para, true);
      paraChanged = true;
    }

    if (paraChanged) {
      if (m_state->m_paragraphMap.find(paraPos)!=m_state->m_paragraphMap.end())
        m_state->m_paragraphMap.erase(paraPos);
      m_state->m_paragraphMap.insert
      (std::map<long, MSWStruct::Paragraph>::value_type
       (paraPos,actState.m_actualPara));
      fontChanged=true;
    }

    if (fontChanged)
      m_state->m_fontMap[cPos] = actState.m_actualFont;
    if (f.str().length()) {
      f2.str("");
      f2 << "TextContent["<<cPos<<"]:" << f.str();
      ascFile.addPos(pos);
      ascFile.addNote(f2.str().c_str());
      prop.m_debugPrint = true;
    }
    m_state->m_propertyMap[cPos] = prop;
    pos+=(cNextPos-cPos);
    cPos = cNextPos;
  }

  prepareTables();
}

void MSWText::prepareTables()
{
  long cEnd = 0;
  for (int i = 0; i < 3; i++) cEnd+=m_state->m_textLength[i];
  if (cEnd <= 0) return;

  std::map<long,MSWTextInternal::Property>::iterator propIt =
    m_state->m_propertyMap.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  while(propIt != m_state->m_propertyMap.end()) {
    long cPos = propIt->first;
    MSWTextInternal::Property &prop = propIt++->second, actProp = prop;

    if (m_state->m_paragraphMap.find(cPos) == m_state->m_paragraphMap.end())
      continue;
    MSWStruct::Paragraph const *para = &m_state->m_paragraphMap.find(cPos)->second;
    if (!para->inTable() || para->m_tableDef.get()) continue;

    std::vector<long> listBeginCells;
    listBeginCells.push_back(cPos);
    while (1) {
      long newEndPos = cEnd;
      input->seek(actProp.m_pos, WPX_SEEK_SET);
      if (propIt != m_state->m_propertyMap.end())
        newEndPos = propIt->first;

      while (cPos++ < newEndPos) {
        int c = (int) input->readULong(1);
        if (c!=0x7) continue;
        listBeginCells.push_back(cPos);
      }

      if (propIt == m_state->m_propertyMap.end()) break;

      cPos = propIt->first;
      actProp = propIt++->second;
      if (m_state->m_paragraphMap.find(cPos) == m_state->m_paragraphMap.end())
        continue;
      para = &m_state->m_paragraphMap.find(cPos)->second;
      if (!para->inTable()) break;
      if (para->m_tableDef.get()) {
        listBeginCells.push_back(cPos);
        break;
      }
    }
    if (!para->m_tableDef.get() || !para->m_table.isSet()
        || !para->m_table->m_columns.isSet()
        || para->m_table->m_columns->size() < 2) {
      MWAW_DEBUG_MSG(("MSWText::prepareTables: can not find table for position %ld pb in %ld\n", listBeginCells[0], cPos));
      ascFile.addPos(m_state->getFilePos(listBeginCells[0]));
      ascFile.addNote("###A");
      ascFile.addPos(m_state->getFilePos(cPos));
      ascFile.addNote("###B");
      continue;
    }

    size_t nTableCells=para->m_table->m_columns->size();
    if (((listBeginCells.size()+nTableCells)%nTableCells) != 1) {
      MWAW_DEBUG_MSG(("MSWText::prepareTables: table num cells is bad for position %ld<->%ld\n", listBeginCells[0], cPos));
      continue;
    }
    prop.m_cellsPos = listBeginCells;
  }
}

////////////////////////////////////////////////////////////
// try to read a text entry
////////////////////////////////////////////////////////////
bool MSWText::sendText(MWAWEntry const &textEntry, bool mainZone, bool tableCell)
{
  if (!textEntry.valid()) return false;
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("MSWText::sendText: can not find a listener!"));
    return true;
  }
  long cPos = textEntry.begin();
  long debPos = m_state->getFilePos(cPos), pos=debPos;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos, WPX_SEEK_SET);
  long cEnd = textEntry.end();

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "TextContent[" << cPos << "]:";
  long pictPos = -1;
  while (!input->atEOS() && cPos < cEnd) {
    bool newTable = false;
    long cEndPos = cEnd;

    MSWTextInternal::Property *prop = 0;
    std::map<long,MSWTextInternal::Property>::iterator propIt = m_state->m_propertyMap.upper_bound(cPos);
    if (propIt != m_state->m_propertyMap.end() && propIt->first < cEndPos && propIt->first > cPos)
      cEndPos = propIt->first;

    size_t numPLC = 0;
    propIt = m_state->m_propertyMap.find(cPos);
    if (propIt != m_state->m_propertyMap.end()) {
      prop = &propIt->second;
      pos = prop->m_pos;
      newTable = !tableCell && prop->isTable();
      input->seek(pos, WPX_SEEK_SET);
      numPLC = prop->m_plcList.size();
    }
    for (size_t i = 0; i < numPLC; i++) {
      PLC const &plc = prop->m_plcList[i];
      if (newTable && int(plc.m_type) >= int(PLC::ParagraphInfo)) continue;
      switch(plc.m_type) {
      case PLC::TextPosition: {
        if (plc.m_id < 0 || plc.m_id >= int(m_state->m_textposList.size()))
          break;
        MSWTextInternal::TextEntry const &entry=m_state->m_textposList[(size_t) plc.m_id];
        if (pos != entry.begin()) {
          MWAW_DEBUG_MSG(("MSWText::sendText: the file position seems bad\n"));
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
      case PLC::ParagraphInfo:
      default:
        break;
      }
    }
    if ((prop && prop->m_debugPrint)  || newTable) {
      ascFile.addPos(debPos);
      ascFile.addNote(f.str().c_str());
      f.str("");
      f << "TextContent["<<cPos<<"]:";
      debPos = pos;
    }
    // time to send the table
    if (newTable) {
      long actCPos = cPos;
      bool ok = sendTable(*prop);
      cPos = ok ? prop->m_cellsPos.back()+1 : actCPos;
      pos=debPos=m_state->getFilePos(cPos);
      input->seek(pos, WPX_SEEK_SET);
      f.str("");
      f << "TextContent["<<cPos<<"]:";
      if (ok)
        continue;
    }
    if (m_state->m_paragraphMap.find(cPos) != m_state->m_paragraphMap.end())
      m_stylesManager->setProperty(m_state->m_paragraphMap.find(cPos)->second);
    if (m_state->m_fontMap.find(cPos) != m_state->m_fontMap.end()) {
      MSWStruct::Font font = m_state->m_fontMap.find(cPos)->second;
      pictPos = font.m_picturePos.get();
      m_stylesManager->setProperty(font);
    }
    for (long p = cPos; p < cEndPos; p++) {
      int c = (int) input->readULong(1);
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
        listener->insertEOL();
        break;
      case 0xc: // end section (ok)
        break;
      case 0x2:
        listener->insertField(MWAWField(MWAWField::PageNumber));
        break;
      case 0x6:
        listener->insertChar('\\');
        break;
      case 0x1e: // unbreaking - ?
        listener->insertChar('-');
        break;
      case 0x1f: // hyphen
        break;
      case 0x13: // month
      case 0x1a: // month abreviated
      case 0x1b: { // checkme month long
        MWAWField field(MWAWField::Date);
        field.m_DTFormat = "%m";
        listener->insertField(field);
        break;
      }
      case 0x10: // day
      case 0x16: // checkme: day abbreviated
      case 0x17: { // checkme: day long
        MWAWField field(MWAWField::Date);
        field.m_DTFormat = "%d";
        listener->insertField(field);
        break;
      }
      case 0x15: { // year
        MWAWField field(MWAWField::Date);
        field.m_DTFormat = "%y";
        listener->insertField(field);
        break;
      }
      case 0x1d: {
        MWAWField field(MWAWField::Date);
        field.m_DTFormat = "%b %d, %Y";
        listener->insertField(field);
        break;
      }
      case 0x18: // checkme hour
      case 0x19: { // checkme hour
        MWAWField field(MWAWField::Time);
        field.m_DTFormat = "%H";
        listener->insertField(field);
        break;
      }
      case 0x3: // v3
        listener->insertField(MWAWField(MWAWField::Date));
        break;
      case 0x4:
        listener->insertField(MWAWField(MWAWField::Time));
        break;
      case 0x5: // footnote mark (ok)
        break;
      case 0x9:
        listener->insertTab();
        break;
      case 0xb: // line break (simple but no a paragraph break ~soft)
        listener->insertEOL(true);
        break;
      case 0xd: // line break hard
        listener->insertEOL();
        break;
      case 0x11: // command key in help
        listener->insertUnicode(0x2318);
        break;
      case 0x14: // apple logo ( note only in private zone)
        listener->insertUnicode(0xf8ff);
        break;
      default:
        p+=listener->insertCharacter((unsigned char)c, input, input->tell()+(cEndPos-1-p));
        break;
      }
      if (c)
        f << char(c);
      else
        f << "###";
    }
  }

  ascFile.addPos(debPos);
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(input->tell());
  ascFile.addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// try to read a table
////////////////////////////////////////////////////////////
bool MSWText::sendTable(MSWTextInternal::Property const &prop)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("MSWText::sendTable: can not find a listener!\n"));
    return true;
  }
  size_t nCells = prop.m_cellsPos.size();
  if (nCells <= 1 ||
      m_state->m_paragraphMap.find(prop.m_cellsPos.back())
      == m_state->m_paragraphMap.end()) {
    MWAW_DEBUG_MSG(("MSWText::sendTable: numcols pos is bad\n"));
    return true;
  }
  nCells--;

  MSWStruct::Paragraph &para =
    m_state->m_paragraphMap.find(prop.m_cellsPos[nCells])->second;
  if (!para.m_table.isSet()) {
    MWAW_DEBUG_MSG(("MSWText::sendTable: can not find the table\n"));
    return false;
  }
  MSWStruct::Table const &table = para.m_table.get();
  if (!table.m_columns.isSet() || table.m_columns->size()==0 ||
      (nCells%table.m_columns->size())) {
    MWAW_DEBUG_MSG(("MSWText::sendTable: numcols para is bad\n"));
    return false;
  }

  size_t numCols = table.m_columns->size();
  size_t numRows = nCells/numCols;

  float height = 0;
  if (table.m_height.isSet()) height=-table.m_height.get();

  std::vector<float> width(numCols-1);
  for (size_t c = 0; c < numCols-1; c++)
    width[c]=table.m_columns.get()[c+1]-table.m_columns.get()[c];
  listener->openTable(width, WPX_POINT);

  size_t numCells = table.m_cells.size();
  for (size_t r = 0; r < numRows; r++) {
    listener->openTableRow(height, WPX_INCH);
    for (size_t c = 0; c < numCols-1; c++) {
      MWAWCell cell;
      size_t cellPos = r*numCols+c;
      if (cellPos < numCells && table.m_cells[cellPos].isSet()) {
        int const wh[] = { libmwaw::TopBit, libmwaw::LeftBit,
                           libmwaw::BottomBit, libmwaw::RightBit
                         };
        MSWStruct::Table::Cell const &tCell = table.m_cells[cellPos].get();
        for (size_t i = 0; i < 4 && i < tCell.m_borders.size(); i++) {
          if (!tCell.m_borders[i].isSet() ||
              tCell.m_borders[i]->m_style==MWAWBorder::None) continue;
          cell.setBorders(wh[i], tCell.m_borders[i].get());
        }
        if (tCell.m_backColor.isSet()) {
          unsigned char col = (unsigned char)(tCell.m_backColor.get()*255.f);
          cell.setBackgroundColor(MWAWColor(col,col,col));
        }
      }
      cell.position() = Vec2i((int)c,(int)r);

      WPXPropertyList extras;
      listener->openTableCell(cell, extras);

      MSWEntry textData;
      textData.setBegin(prop.m_cellsPos[cellPos]);
      long cEndPos = prop.m_cellsPos[cellPos+1]-1;
      textData.setEnd(cEndPos);
      if (textData.length()<=0)
        listener->insertChar(' ');
      else
        sendText(textData, false, true);
      listener->closeTableCell();
    }
    listener->closeTableRow();
  }
  listener->closeTable();
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
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) return true;
  if (id < 0 || id >= int(m_state->m_footnoteList.size())) {
    MWAW_DEBUG_MSG(("MSWText::sendFootnote: can not find footnote %d\n", id));
    listener->insertChar(' ');
    return false;
  }
  MSWTextInternal::Footnote &footnote = m_state->m_footnoteList[(size_t) id];
  if (footnote.m_pos.isParsed())
    listener->insertChar(' ');
  else
    sendText(footnote.m_pos, false);
  footnote.m_pos.setParsed();
  return true;
}

bool MSWText::sendFieldComment(int id)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) return true;
  if (id < 0 || id >= int(m_state->m_fieldList.size())) {
    MWAW_DEBUG_MSG(("MSWText::sendFieldComment: can not find field %d\n", id));
    listener->insertChar(' ');
    return false;
  }
  MSWStruct::Font defFont;
  defFont.m_font = m_stylesManager->getDefaultFont();
  m_stylesManager->setProperty(defFont);
  m_stylesManager->sendDefaultParagraph();
  std::string const &text = m_state->m_fieldList[(size_t) id].m_text;
  if (!text.length()) listener->insertChar(' ');
  for (size_t c = 0; c < text.length(); c++)
    listener->insertCharacter((unsigned char) text[c]);
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
