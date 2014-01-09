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

#if defined(DEBUG_WITH_FILES)
# include <fstream>
#endif

#include <librevenge/librevenge.h>

#include "MWAWCell.hxx"
#include "MWAWTextListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWTable.hxx"

#include "MsWrdParser.hxx"
#include "MsWrdStruct.hxx"

#include "MsWrdText.hxx"

#define DEBUG_FONT 1
#define DEBUG_PLC 1
#define DEBUG_PAGE 1
#define DEBUG_PARAGRAPH 1
#define DEBUG_SECTION 1
#define DEBUG_PARAGRAPHINFO 1

/** Internal: the structures of a MsWrdText */
namespace MsWrdTextInternal
{
#if defined(DEBUG_WITH_FILES)
// use cut -c13- main-2.data|sort -n to retrieve the data
//! internal and low level: defined a second debug file
static std::fstream &debugFile2()
{
  static std::fstream s_file("main-2.data", std::ios_base::out | std::ios_base::trunc);
  return s_file;
}
#endif

////////////////////////////////////////
//! Internal: the entry of MsWrdParser
struct TextStruct : public MWAWEntry {
  TextStruct() : MWAWEntry(), m_pos(-1), m_id(0), m_type(0), m_paragraphId(-1), m_complex(false), m_extra("")
  {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, TextStruct const &entry)
  {
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
    switch (entry.m_type&0xF8) { // fNoParaLast
    case 0x80: // sameline
      break;
    case 0:
      o << "newline,";
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
  int getParagraphId() const
  {
    return m_paragraphId;
  }
  //! a struct used to compare file textpos
  struct CompareFilePos {
    //! comparaison function
    bool operator()(TextStruct const *t1, TextStruct const *t2) const
    {
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
  Page() : m_id(-1), m_type(0), m_page(-1), m_paragraphId(-2), m_error("")
  {
    for (int i = 0; i < 4; i++) m_values[i] = 0;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Page const &page)
  {
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
  friend std::ostream &operator<<(std::ostream &o, Footnote const &note)
  {
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
//! Internal: the field of MsWrdParser
struct Field {
  //! constructor
  Field() : m_text(""), m_id(-1), m_error("") { }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Field const &field)
  {
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
  Property() : m_fPos(-1), m_plcList(), m_debugPrint(false) {}
  //! the character position in the file
  long m_fPos;
  //! the list of plc
  std::vector<MsWrdText::PLC> m_plcList;
  //! a flag to know if we have print data
  bool m_debugPrint;
};

////////////////////////////////////////
//! Internal and low level: a structure to store a line or a cell of a MsWrdText
struct Line {
  //! an enum used to differentiate line and cell
  enum Type { L_Line, L_Cell, L_LastLineCell, L_LastRowCell };
  //! constructor
  Line() : m_type(L_Line), m_cPos()
  {
  }
  //! the line type
  Type m_type;
  //! the caracter position
  Vec2l m_cPos;
};

////////////////////////////////////////
//! Internal and low level: a structure to store a table of a MsWrdText
struct Table : public MWAWTable {
  //! constructor
  Table() : MWAWTable(MWAWTable::TableDimBit), m_cellPos(), m_delimiterPos(), m_height(0), m_backgroundColor(MWAWColor::white()), m_cells()
  {
  }
  //! the list of cPos corresponding to cells limits
  std::vector<long> m_cellPos;
  //! the list of the delimiter cPos (ie. end of each cell)
  std::vector<long> m_delimiterPos;
  //! the row height
  float m_height;
  //! the background color
  MWAWColor m_backgroundColor;
  //! the table cells
  std::vector<Variable<MsWrdStruct::Table::Cell> > m_cells;
};

////////////////////////////////////////
//! Internal: the state of a MsWrdParser
struct State {
  //! constructor
  State() : m_version(-1), m_bot(0x100), m_headerFooterZones(), m_textposList(),
    m_plcMap(), m_filePlcMap(), m_lineList(), m_paragraphLimitMap(), m_sectionLimitList(),
    m_fontMap(), m_paragraphMap(), m_propertyMap(), m_tableCellPosSet(), m_tableMap(),
    m_paraInfoList(), m_pageList(), m_fieldList(), m_footnoteList(), m_actPage(0), m_numPages(-1)
  {
    for (int i = 0; i < 3; i++) m_textLength[i] = 0;
  }
  //! returns the total text size
  long getTotalTextSize() const
  {
    long res=0;
    for (int i = 0; i < 3; i++) res+=m_textLength[i];
    return res;
  }
  //! returns the id of textpos corresponding to a cPos or -1
  int getTextStructId(long textPos) const
  {
    if (m_textposList.empty() || textPos < m_textposList[0].m_pos)
      return -1;
    int minVal = 0, maxVal = int(m_textposList.size())-1;
    while (minVal != maxVal) {
      int mid = (minVal+1+maxVal)/2;
      if (m_textposList[(size_t)mid].m_pos == textPos)
        return mid;
      if (m_textposList[(size_t)mid].m_pos > textPos)
        maxVal = mid-1;
      else
        minVal = mid;
    }
    return minVal;
  }
  //! returns the file position corresponding to a text entry
  long getFilePos(long textPos) const
  {
    int tId=getTextStructId(textPos);
    if (tId==-1)
      return m_bot+textPos;
    return m_textposList[(size_t)tId].begin() + (textPos-m_textposList[(size_t)tId].m_pos);
  }
  //! try to return a table which begins at a character position
  shared_ptr<Table> getTable(long cPos) const
  {
    shared_ptr<Table> empty;
    std::map<long,shared_ptr<Table> >::const_iterator tableIt=m_tableMap.find(cPos);
    if (tableIt==m_tableMap.end()||!tableIt->second) return empty;
    shared_ptr<Table> table=tableIt->second;
    if (table->m_cellPos.empty()||table->m_cellPos[0]!=cPos)
      return empty;
    return table;
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
  std::vector<TextStruct> m_textposList;

  //! the text correspondance zone ( textpos, plc )
  std::multimap<long, MsWrdText::PLC> m_plcMap;
  //! the file correspondance zone ( filepos, plc )
  std::multimap<long, MsWrdText::PLC> m_filePlcMap;

  //! the list of lines
  std::vector<Line> m_lineList;
  //! the paragraph limit -> textposition (or -1)
  std::map<long, int> m_paragraphLimitMap;
  //! the section cPos limit
  std::vector<long> m_sectionLimitList;
  //! the final correspondance font zone ( textpos, font)
  std::map<long, MsWrdStruct::Font> m_fontMap;

  //! the final correspondance paragraph zone ( textpos, paragraph)
  std::map<long, MsWrdStruct::Paragraph> m_paragraphMap;
  //! the position where we have new data ( textpos -> [ we have done debug printing ])
  std::map<long, Property> m_propertyMap;
  //! a set of all begin cell position
  std::set<long> m_tableCellPosSet;
  //! the final correspondance table zone ( textpos, font)
  std::map<long, shared_ptr<Table> > m_tableMap;
  //! the list of paragraph info modifier
  std::vector<MsWrdStruct::ParagraphInfo> m_paraInfoList;

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
MsWrdText::MsWrdText(MsWrdParser &parser) :
  m_parserState(parser.getParserState()), m_state(new MsWrdTextInternal::State),
  m_stylesManager(), m_mainParser(&parser)
{
  m_stylesManager.reset(new MsWrdTextStyles(*this));
}

MsWrdText::~MsWrdText()
{ }

int MsWrdText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}

int MsWrdText::numPages() const
{
  m_state->m_numPages = int(m_state->m_pageList.size());
  return m_state->m_numPages;
}

long MsWrdText::getMainTextLength() const
{
  return m_state->m_textLength[0];
}

MWAWEntry MsWrdText::getHeader() const
{
  if (m_state->m_headerFooterZones.size() == 0)
    return MWAWEntry();
  MWAWEntry entry=m_state->m_headerFooterZones[0];
  bool ok=entry.valid();
  if (ok && entry.length()<=2)  {
    // small header, check if contains data
    MWAWInputStreamPtr &input= m_parserState->m_input;
    long pos = input->tell();
    ok=false;
    for (long cPos=entry.begin(); cPos<entry.end(); ++cPos) {
      input->seek(m_state->getFilePos(cPos), librevenge::RVNG_SEEK_SET);
      if (input->readLong(1)==0xd)
        continue;
      ok=true;
      break;
    }
    input->seek(pos, librevenge::RVNG_SEEK_SET);
  }
  return ok ? entry : MWAWEntry();
}

MWAWEntry MsWrdText::getFooter() const
{
  if (m_state->m_headerFooterZones.size() < 2)
    return MWAWEntry();
  MWAWEntry entry=m_state->m_headerFooterZones[1];
  bool ok=entry.valid();
  if (ok && entry.length()<=2)  {
    // check if it contains data
    MWAWInputStreamPtr &input= m_parserState->m_input;
    long pos = input->tell();
    ok=false;
    for (long cPos=entry.begin(); cPos<entry.end(); ++cPos) {
      input->seek(m_state->getFilePos(cPos), librevenge::RVNG_SEEK_SET);
      if (input->readLong(1)==0xd)
        continue;
      ok=true;
      break;
    }
    input->seek(pos, librevenge::RVNG_SEEK_SET);
  }
  return ok ? entry : MWAWEntry();
}

std::multimap<long, MsWrdText::PLC> &MsWrdText::getTextPLCMap()
{
  return m_state->m_plcMap;
}

std::multimap<long, MsWrdText::PLC> &MsWrdText::getFilePLCMap()
{
  return m_state->m_filePlcMap;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////
// PLC
std::ostream &operator<<(std::ostream &o, MsWrdText::PLC const &plc)
{
  switch (plc.m_type) {
  case MsWrdText::PLC::ParagraphInfo:
    o << "Pi";
    break;
  case MsWrdText::PLC::Section:
    o << "S";
    break;
  case MsWrdText::PLC::Footnote:
    o << "Fn";
    break;
  case MsWrdText::PLC::FootnoteDef:
    o << "vFn";
    break;
  case MsWrdText::PLC::Field:
    o << "Field";
    break;
  case MsWrdText::PLC::Page:
    o << "Pg";
    break;
  case MsWrdText::PLC::Font:
    o << "F";
    break;
  case MsWrdText::PLC::Object:
    o << "O";
    break;
  case MsWrdText::PLC::Paragraph:
    o << "P";
    break;
  case MsWrdText::PLC::HeaderFooter:
    o << "hfP";
    break;
  case MsWrdText::PLC::TextPosition:
    o << "textPos";
    break;
  default:
    o << "#type" << char('a'+int(plc.m_type));
  }
  if (plc.m_id < 0) o << "_";
  else o << plc.m_id;
  if (plc.m_extra.length()) o << "[" << plc.m_extra << "]";
  return o;
}

bool MsWrdText::readHeaderTextLength()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long endPos = pos+12;
  if (!input->checkPosition(endPos))
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
bool MsWrdText::createZones(long bot)
{
  // int const vers=version();
  m_state->m_bot = bot;

  std::multimap<std::string, MsWrdEntry> &entryMap
    = m_mainParser->m_entryMap;
  std::multimap<std::string, MsWrdEntry>::iterator it;
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
    MsWrdEntry &entry=it++->second;
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
  if (it != entryMap.end() &&
      !m_stylesManager->readSection(it->second, m_state->m_sectionLimitList))
    m_state->m_sectionLimitList.resize(0);

  //! read the header footer limit
  it = entryMap.find("HeaderFooter");
  std::vector<long> hfLimits;
  if (it != entryMap.end()) {
    readLongZone(it->second, 4, hfLimits);

    long debHeader = m_state->m_textLength[0]+m_state->m_textLength[1];
    MsWrdText::PLC plc(MsWrdText::PLC::HeaderFooter);
    // list Header0,Footer0,Header1,Footer1,...,Footern, 3
    for (size_t i = 0; i+2 < hfLimits.size(); i++) {
      plc.m_id = int(i);
      m_state->m_plcMap.insert(std::multimap<long,MsWrdText::PLC>::value_type
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
bool MsWrdText::readTextStruct(MsWrdEntry &entry)
{
  if (entry.length() < 19) {
    MWAW_DEBUG_MSG(("MsWrdText::readTextStruct: the zone seems to short\n"));
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
    MWAW_DEBUG_MSG(("MsWrdText::readTextStruct: find odd type %d\n", type));
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
    MWAW_DEBUG_MSG(("MsWrdText::readTextStruct: can not read the position zone\n"));
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
      MWAW_DEBUG_MSG(("MsWrdText::readTextStruct: find backward text pos\n"));
      f << "#" << textPos[i] << ",";
      textPos[i]=textPos[i-1];
    }
    else {
      if (i != (size_t) N && textPos[i] > textLength) {
        MWAW_DEBUG_MSG(("MsWrdText::readTextStruct: find a text position which is too big\n"));
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
    MsWrdTextInternal::TextStruct tEntry;
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
    if (!input->checkPosition(ptr)) {
      MWAW_DEBUG_MSG(("MsWrdText::readTextStruct: find a bad file position \n"));
      f << "#";
    }
    else {
      plc.m_id = i;
      m_state->m_plcMap.insert(std::multimap<long,PLC>::value_type
                               (textPos[(size_t)i],plc));
    }
    f << tEntry;
    input->seek(pos+8, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
#if defined(DEBUG_WITH_FILES)
    f.str("");
    f<< "TextContent[" << tEntry.m_pos << "]:" << tEntry << ",";
    MsWrdTextInternal::debugFile2() << f.str() << "\n";
#endif
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
bool MsWrdText::readFontNames(MsWrdEntry &entry)
{
  if (entry.length() < 2) {
    MWAW_DEBUG_MSG(("MsWrdText::readFontNames: the zone seems to short\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  int N = (int) input->readULong(2);
  if (N*5+2 > entry.length()) {
    MWAW_DEBUG_MSG(("MsWrdText::readFontNames: the number of fonts seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  f << "FontNames:" << N;
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    if (pos+5 > entry.end()) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      MWAW_DEBUG_MSG(("MsWrdText::readFontNames: the fonts %d seems bad\n", i));
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
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      MWAW_DEBUG_MSG(("MsWrdText::readFontNames: the fonts name %d seems bad\n", i));
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
bool MsWrdText::readParagraphInfo(MsWrdEntry entry)
{
  int vers=version();
  if (vers<=3) {
    MWAW_DEBUG_MSG(("MsWrdText::readParagraphInfo: does not know how to read a paragraphInfo in v3 or less\n"));
    return false;
  }
  if (entry.length() < 4 || (entry.length()%10) != 4) {
    MWAW_DEBUG_MSG(("MsWrdText::readParagraphInfo: the zone size seems odd\n"));
    return false;
  }
  entry.setParsed(true);

  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
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
    MsWrdStruct::ParagraphInfo paraMod;
    if (!paraMod.read(input, pos+6, vers))
      f << "###";
    f << paraMod;
    m_state->m_paraInfoList.push_back(paraMod);

    if (textPositions[(size_t) i] > m_state->m_textLength[0]) {
      MWAW_DEBUG_MSG(("MsWrdText::readParagraphInfo: text positions is bad...\n"));
      f << "#";
    }
    else {
      plc.m_id=i;
      m_state->m_plcMap.insert(std::multimap<long,PLC>::value_type
                               (textPositions[(size_t) i],plc));
    }
    input->seek(pos+6, librevenge::RVNG_SEEK_SET);
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
bool MsWrdText::readPageBreak(MsWrdEntry &entry)
{
  int const vers = version();
  int const fSz = vers <= 3 ? 8 : 10;
  if (entry.length() < fSz+8 || (entry.length()%(fSz+4)) != 4) {
    MWAW_DEBUG_MSG(("MsWrdText::readPageBreak: the zone size seems odd\n"));
    return false;
  }
  long pos = entry.begin();
  entry.setParsed(true);
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "PageBreak:";
  int N=int(entry.length()/(fSz+4));
  std::vector<long> textPos; // checkme
  textPos.resize((size_t) N+1);
  for (int i = 0; i <= N; i++) textPos[(size_t) i] = (long) input->readULong(4);
  PLC plc(PLC::Page);
  int prevPage=-1;
  for (int i = 0; i < N; i++) {
    MsWrdTextInternal::Page page;
    page.m_id = i;
    page.m_type = (int) input->readULong(1);
    page.m_values[0] = (int) input->readLong(1); // always 0,1,2
    for (int j = 1; j < 3; j++) // always -1, 0
      page.m_values[j] = (int) input->readLong(2);
    page.m_page = (int) input->readLong(2);
    if (vers > 3)
      page.m_values[3] = (int) input->readLong(2);
    if (i && textPos[(size_t)i]==textPos[(size_t)i-1] && page.m_page==prevPage) {
      // find this one time in v3...
      MWAW_DEBUG_MSG(("MsWrdText::readPageBreak: page %d is duplicated...\n", i));
      f << "#dup,";
      continue;
    }
    prevPage=page.m_page;
    m_state->m_pageList.push_back(page);

    if (textPos[(size_t)i] > m_state->m_textLength[0]) {
      MWAW_DEBUG_MSG(("MsWrdText::readPageBreak: text positions is bad...\n"));
      f << "#";
    }
    else {
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
bool MsWrdText::readFootnotesPos(MsWrdEntry &entry, std::vector<long> const &noteDef)
{
  if (entry.length() < 4 || (entry.length()%6) != 4) {
    MWAW_DEBUG_MSG(("MsWrdText::readFootnotesPos: the zone size seems odd\n"));
    return false;
  }
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  int N=int(entry.length()/6);
  if (N+2 != int(noteDef.size())) {
    MWAW_DEBUG_MSG(("MsWrdText::readFootnotesPos: the number N seems odd\n"));
    return false;
  }
  long pos = entry.begin();
  entry.setParsed(true);
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "FootnotePos:";

  std::vector<long> textPos;
  textPos.resize((size_t)N+1);
  for (int i = 0; i <= N; i++)
    textPos[(size_t)i]= (long) input->readULong(4);
  long debFootnote = m_state->m_textLength[0];
  PLC plc(PLC::Footnote);
  PLC defPlc(PLC::FootnoteDef);
  for (int i = 0; i < N; i++) {
    MsWrdTextInternal::Footnote note;
    note.m_id = i;
    note.m_pos.setBegin(debFootnote+noteDef[(size_t)i]);
    note.m_pos.setEnd(debFootnote+noteDef[(size_t)i+1]);
    note.m_value = (int) input->readLong(2);
    m_state->m_footnoteList.push_back(note);

    if (textPos[(size_t)i] > m_state->getTotalTextSize()) {
      MWAW_DEBUG_MSG(("MsWrdText::readFootnotesPos: can not find text position\n"));
      f << "#";
    }
    else if (noteDef[(size_t)i+1] > m_state->m_textLength[1]) {
      MWAW_DEBUG_MSG(("MsWrdText::readFootnotesPos: can not find definition position\n"));
      f << "#";
    }
    else {
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
bool MsWrdText::readFootnotesData(MsWrdEntry &entry)
{
  if (entry.length() < 4 || (entry.length()%14) != 4) {
    MWAW_DEBUG_MSG(("MsWrdText::readFootnotesData: the zone size seems odd\n"));
    return false;
  }
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  int N=int(entry.length()/14);
  long pos = entry.begin();
  entry.setParsed(true);
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "FootnoteData[" << N << "/" << m_state->m_footnoteList.size() << "]:";

  std::vector<long> textPos; // checkme
  textPos.resize((size_t)N+1);
  for (int i = 0; i <= N; i++)
    textPos[(size_t)i]= (long) input->readULong(4);
  for (int i = 0; i < N; i++) {
    if (textPos[(size_t)i] > m_state->m_textLength[1]) {
      MWAW_DEBUG_MSG(("MsWrdText::readFootnotesData: textPositions seems bad\n"));
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
bool MsWrdText::readFields(MsWrdEntry &entry, std::vector<long> const &fieldPos)
{
  long pos = entry.begin();
  int N = int(fieldPos.size());
  long textLength = m_state->getTotalTextSize();
  if (N==0) {
    MWAW_DEBUG_MSG(("MsWrdText::readFields: number of fields is 0\n"));
    return false;
  }
  N--;
  entry.setParsed(true);
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos, librevenge::RVNG_SEEK_SET);

  long sz = (long) input->readULong(2);
  if (entry.length() != sz) {
    MWAW_DEBUG_MSG(("MsWrdText::readFields: the zone size seems odd\n"));
    return false;
  }
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f, f2;
  f << "FieldName:";
  int const endSize = (version()==5) ? 2 : 1;
  PLC plc(PLC::Field);
  for (int n = 1; n < N; n++) {
    if (input->tell() >= entry.end()) {
      MWAW_DEBUG_MSG(("MsWrdText::readFields: can not find all field\n"));
      break;
    }
    pos = input->tell();
    int fSz = (int) input->readULong(1);
    if (pos+1+fSz > entry.end()) {
      MWAW_DEBUG_MSG(("MsWrdText::readFields: can not read a string\n"));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
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
    MsWrdTextInternal::Field field;
    if (!endSz) ;
    else if (version()>=5 && input->readULong(1) != 0xc) {
      input->seek(-1, librevenge::RVNG_SEEK_CUR);
      for (int i = 0; i < 2; i++) text+=char(input->readULong(1));
    }
    else {
      int id = (int) input->readULong(1);
      if (id >= N) {
        if (version()>=5) {
          MWAW_DEBUG_MSG(("MsWrdText::readFields: find a strange id\n"));
          f2 << "#";
        }
        else
          text+=char(id);
      }
      else
        field.m_id = id;
    }
    field.m_text = text;
    field.m_error = f2.str();
    m_state->m_fieldList.push_back(field);

    f << "N" << n << "=" << field << ",";
    if (fieldPos[(size_t)n] >= textLength) {
      MWAW_DEBUG_MSG(("MsWrdText::readFields: text positions is bad...\n"));
      f << "#";
    }
    else {
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
bool MsWrdText::readLongZone(MsWrdEntry &entry, int sz, std::vector<long> &list)
{
  list.resize(0);
  if (entry.length() < sz || (entry.length()%sz)) {
    MWAW_DEBUG_MSG(("MsWrdText::readIntsZone: the size of zone %s seems to odd\n", entry.type().c_str()));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
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
void MsWrdText::prepareLines()
{
  m_state->m_lineList.clear();
  long cPos = 0, cEnd = m_state->getTotalTextSize();
  if (cEnd <= 0) return;

  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(m_state->getFilePos(0), librevenge::RVNG_SEEK_SET);

  MsWrdTextInternal::Line line;
  line.m_cPos[0]=0;
  size_t numTextPos = m_state->m_textposList.size();
  while (!input->isEnd() && cPos < cEnd) {
    std::multimap<long, MsWrdText::PLC>::const_iterator plcIt =
      m_state->m_plcMap.lower_bound(cPos);
    while (plcIt != m_state->m_plcMap.end() && plcIt->first==cPos) {
      MsWrdText::PLC const &plc = plcIt++->second;
      if (plc.m_type != PLC::TextPosition)
        continue;
      if (plc.m_id < 0 || plc.m_id >= (int)numTextPos)
        continue;
      MsWrdTextInternal::TextStruct const &textEntry=
        m_state->m_textposList[(size_t) plc.m_id];
      input->seek(textEntry.begin(), librevenge::RVNG_SEEK_SET);
    }
    char c=(char) input->readLong(1);
    ++cPos;
    if (c!=0x7 && c!=0xd && cPos!=cEnd)
      continue;
    line.m_cPos[1]=cPos;
    if (c==0x7)
      line.m_type=MsWrdTextInternal::Line::L_LastLineCell;
    else
      line.m_type=MsWrdTextInternal::Line::L_Line;
    m_state->m_lineList.push_back(line);

    line.m_cPos[0]=cPos;
  }
}

void MsWrdText::convertFilePLCPos()
{
  size_t numTextPos = m_state->m_textposList.size();
  std::multimap<long, MsWrdText::PLC>::const_iterator it;
  std::multimap<long, MsWrdText::PLC> &cMap=m_state->m_plcMap;

  // create the list of table delimiters
  std::set<long> tableSet;
  for (size_t l=0; l<m_state->m_lineList.size(); ++l) {
    MsWrdTextInternal::Line const &line=m_state->m_lineList[l];
    if (line.m_type==MsWrdTextInternal::Line::L_Line)
      tableSet.insert(line.m_cPos[1]);
  }

  std::set<long>::const_iterator tableIt=tableSet.begin();
  MsWrdText::PLC resetParaPLC(PLC::Paragraph,-1);
  // simplest case
  if (!numTextPos) {
    long const bottom = m_state->m_bot;
    long pPos=bottom;
    it= m_state->m_filePlcMap.begin();
    while (it != m_state->m_filePlcMap.end()) {
      long pos=it->first, prevPos=0;
      MsWrdText::PLC const &plc=it++->second;
      if (plc.m_type==PLC::Paragraph) {
        while (tableIt!=tableSet.end() && *tableIt<=pos-bottom) {
          long resPos=*(tableIt++);
          if (resPos<pos-bottom) {
            m_state->m_paragraphLimitMap[pPos-bottom]=-1;
            cMap.insert(std::map<long, MsWrdText::PLC>::value_type(pPos-bottom, resetParaPLC));
            pPos=resPos;
          }
        }
        m_state->m_paragraphLimitMap[pPos-bottom]=-1;
        prevPos=pPos;
        pPos=pos;
      }
      else if (plc.m_type==PLC::Font)
        prevPos=pos;
      else {
        MWAW_DEBUG_MSG(("MsWrdText::convertFilePLCPos: unexpected plc type: %d\n", plc.m_type));
        continue;
      }
      cMap.insert(std::map<long, MsWrdText::PLC>::value_type(prevPos-bottom, plc));
    }
    return;
  }

  long cPos=0, pPos=0;
  int fontId=-1;
  for (size_t i=0; i < numTextPos; ++i) {
    MsWrdTextInternal::TextStruct const &tPos=m_state->m_textposList[i];
    long const begPos= tPos.begin();
    long const endPos=tPos.end();
    it=m_state->m_filePlcMap.lower_bound(begPos);
    bool fontCheck=false;
    while (it!=m_state->m_filePlcMap.end()) {
      long pos=it->first;
      if (!fontCheck && pos!=begPos) {
        // time to check if the font has changed
        std::multimap<long, MsWrdText::PLC>::const_iterator fIt=
          m_state->m_filePlcMap.lower_bound(begPos);
        while (fIt!=m_state->m_filePlcMap.begin()) {
          if (fIt==m_state->m_filePlcMap.end()||fIt->first>=begPos)
            --fIt;
          else
            break;
        }
        while (fIt!=m_state->m_filePlcMap.end()) {
          if (fIt->first >= begPos)
            break;
          MsWrdText::PLC const &plc=fIt->second;
          if (plc.m_type==PLC::Font) {
            if (fontId!=plc.m_id) {
              fontId=plc.m_id;
              cMap.insert(std::map<long, MsWrdText::PLC>::value_type(cPos, plc));
            }
            break;
          }
          if (fIt==m_state->m_filePlcMap.begin())
            break;
          --fIt;
        }
        fontCheck=true;
      }
      if (pos>endPos)
        break;
      MsWrdText::PLC const &plc=it++->second;
      long newCPos=cPos+(pos-begPos), prevPos=0;
      if (plc.m_type==PLC::Paragraph) {
        if (pos==begPos)
          continue;
        while (tableIt!=tableSet.end() && *tableIt<=newCPos) {
          long resPos=*(tableIt++);
          if (resPos<newCPos) {
            m_state->m_paragraphLimitMap[pPos]=-1;
            cMap.insert(std::map<long, MsWrdText::PLC>::value_type(pPos, resetParaPLC));
            pPos=resPos;
          }
        }
        m_state->m_paragraphLimitMap[pPos]=int(i);
        prevPos=pPos;
        pPos=newCPos;
      }
      else if (plc.m_type==PLC::Font) {
        if (pos==endPos)
          continue;
        fontCheck=true;
        fontId=plc.m_id;
        prevPos=newCPos;
      }
      else {
        MWAW_DEBUG_MSG(("MsWrdText::convertFilePLCPos: unexpected plc type: %d\n", plc.m_type));
        continue;
      }
      cMap.insert(std::map<long, MsWrdText::PLC>::value_type(prevPos, plc));
    }
    cPos+=tPos.length();
  }
}

void MsWrdText::prepareParagraphProperties()
{
  int const vers=version();
  size_t numLines=m_state->m_lineList.size();
  int textposSize = int(m_state->m_textposList.size());
  MsWrdTextInternal::Line::Type lineType=MsWrdTextInternal::Line::L_Line;
  MsWrdStruct::Paragraph paragraph(vers), tablePara(vers);
  long cTableEndPos=-1;
  bool inTable=false;
  for (int i=0; i<int(numLines); ++i) {
    MsWrdTextInternal::Line &line = m_state->m_lineList[size_t(i)];

    std::map<long, int>::const_iterator pIt;
    long cPos=line.m_cPos[0];
    if (inTable && cPos>=cTableEndPos) {
      inTable=false;
      lineType=MsWrdTextInternal::Line::L_Line;
    }
    pIt=m_state->m_paragraphLimitMap.lower_bound(cPos);
    if (pIt==m_state->m_paragraphLimitMap.end() || pIt->first!=cPos) {
      line.m_type=lineType;
      continue;
    }
    int textId=pIt->second;

    // first retrieve the paragraph
    std::multimap<long, MsWrdText::PLC>::const_iterator plcIt;
    plcIt=m_state->m_plcMap.lower_bound(cPos);
    while (plcIt != m_state->m_plcMap.end() && plcIt->first==cPos) {
      MsWrdText::PLC const &plc = plcIt++->second;
      if (plc.m_type != PLC::Paragraph)
        continue;
      if (plc.m_id>=0)
        m_stylesManager->getParagraph(MsWrdTextStyles::TextZone,
                                      plc.m_id, paragraph);
      else
        paragraph=MsWrdStruct::Paragraph(vers);
      if (inTable) {
        MsWrdStruct::Paragraph tmpPara=tablePara;
        tmpPara.insert(paragraph);
        paragraph=tmpPara;
      }
    }

    MsWrdStruct::Paragraph finalPara(paragraph);
    if (textId>=0 && textId < textposSize) {
      MsWrdTextInternal::TextStruct const &textEntry=
        m_state->m_textposList[(size_t) textId];
      int id=textEntry.getParagraphId();
      // checkme do we need to test (textEntry.m_type&0x80)==0 here
      if (id>=0) {
        MsWrdStruct::Paragraph modifier(vers);
        m_stylesManager->getParagraph(MsWrdTextStyles::TextStructZone, id, modifier);
        finalPara.insert(modifier);
      }
    }

    if (finalPara.m_styleId.isSet()) {
      MsWrdStruct::Paragraph style(vers);
      m_stylesManager->getParagraph(MsWrdTextStyles::StyleZone,*finalPara.m_styleId, style);
      MsWrdStruct::Paragraph tmpPara(style);
      tmpPara.insert(finalPara);
      tmpPara.updateParagraphToFinalState(&style);
      finalPara=tmpPara;
    }
    else
      finalPara.updateParagraphToFinalState();

    if (!inTable && (finalPara.inTable()||line.m_type==MsWrdTextInternal::Line::L_LastLineCell) &&
        updateTableBeginnningAt(cPos, cTableEndPos) && cPos<cTableEndPos) {
      inTable=true;
      // ok, find the main table paragraph and loop
      tablePara=MsWrdStruct::Paragraph(vers);
      plcIt=m_state->m_plcMap.lower_bound(cTableEndPos-1);
      while (plcIt != m_state->m_plcMap.end() && plcIt->first==cTableEndPos-1) {
        MsWrdText::PLC const &plc = plcIt++->second;
        if (plc.m_type != PLC::Paragraph)
          continue;
        if (plc.m_id>=0)
          m_stylesManager->getParagraph(MsWrdTextStyles::TextZone, plc.m_id, tablePara);
      }
      paragraph=tablePara;
      --i;
      continue;
    }
    if (inTable && line.m_type==MsWrdTextInternal::Line::L_Line)
      line.m_type=MsWrdTextInternal::Line::L_Cell;

    // store the result
    m_state->m_paragraphMap.insert
    (std::map<long, MsWrdStruct::Paragraph>::value_type(cPos,finalPara));
    lineType=line.m_type;
  }
}

void MsWrdText::prepareFontProperties()
{
  int const vers = version();
  long cPos = 0, cEnd = m_state->getTotalTextSize();
  if (cEnd <= 0) return;

  std::multimap<long, PLC>::iterator plcIt;
  std::multimap<long, MsWrdText::PLC> &map = m_state->m_plcMap;
  int textposSize = int(m_state->m_textposList.size());
  MsWrdStruct::Font font, modifier, paraFont, styleFont;
  int actStyle=-1;
  while (cPos < cEnd) {
    bool fontChanged=false;
    if (m_state->m_paragraphMap.find(cPos)!=m_state->m_paragraphMap.end()) {
      MsWrdStruct::Paragraph const &para= m_state->m_paragraphMap.find(cPos)->second;
      para.getFont(paraFont);
      if (para.m_styleId.isSet() && actStyle!=*para.m_styleId) {
        actStyle=*para.m_styleId;
        styleFont=MsWrdStruct::Font();
        m_stylesManager->getFont(MsWrdTextStyles::StyleZone, *para.m_styleId, styleFont);
      }
      fontChanged=true; // force a font change (even if no needed)
    }

    long cNextPos = cEnd;
    plcIt = map.lower_bound(cPos);
    int textPId=-2;
    while (plcIt != map.end()) {
      if (plcIt->first != cPos) {
        cNextPos=plcIt->first;
        break;
      }
      PLC const &plc = plcIt++->second;
      int pId = plc.m_id;
      switch (plc.m_type) {
      case PLC::TextPosition: {
        if (pId < 0 || pId > textposSize) {
          MWAW_DEBUG_MSG(("MsWrdText::prepareFontProperties: oops can not find textstruct!!!!\n"));
          break;
        }
        MsWrdTextInternal::TextStruct const &textEntry=m_state->m_textposList[(size_t) pId];
        textPId=textEntry.getParagraphId();
        break;
      }
      case PLC::Font:
        fontChanged=true;
        modifier=font=MsWrdStruct::Font();
        if (pId >= 0)
          m_stylesManager->getFont(MsWrdTextStyles::TextZone, pId, font);
        break;
      case PLC::Field:
      case PLC::Footnote:
      case PLC::FootnoteDef:
      case PLC::HeaderFooter:
      case PLC::Object:
      case PLC::Page:
      case PLC::Paragraph:
      case PLC::ParagraphInfo:
      case PLC::Section:
      default:
        break;
      }
    }
    if (textPId>=0) {
      MsWrdStruct::Paragraph para(vers);
      m_stylesManager->getParagraph(MsWrdTextStyles::TextStructZone, textPId, para);
      modifier=MsWrdStruct::Font();
      para.getFont(modifier);
      fontChanged=true;
    }
    else if (textPId==-1) {
      modifier=MsWrdStruct::Font();
      fontChanged=true;
    }
    if (fontChanged) {
      MsWrdStruct::Font final(paraFont); // or stylefont
      final.insert(font, &styleFont);
      final.insert(modifier, &styleFont);
      m_state->m_fontMap[cPos] = final;
    }
    cPos = cNextPos;
  }
}

void MsWrdText::prepareTableLimits()
{
  int const vers=version();
  size_t numLines=m_state->m_lineList.size();
  // first find the table delimiters
  std::map<long,size_t> cposToLineMap;
  for (size_t l=0; l < numLines; ++l) {
    MsWrdTextInternal::Line const &line = m_state->m_lineList[l];
    if (line.m_type != MsWrdTextInternal::Line::L_LastLineCell)
      continue;
    cposToLineMap[line.m_cPos[1]-1]=l;
  }

  size_t numTextpos=m_state->m_textposList.size();
  std::map<long,size_t>::const_iterator tPosIt=cposToLineMap.begin();
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  while (tPosIt!=cposToLineMap.end()) {
    size_t lId=tPosIt->second;
    if (lId>=numLines) {
      MWAW_DEBUG_MSG(("MsWrdText::prepareTableLimits: lId is bad\n"));
      ++tPosIt;
      continue;
    }
    MsWrdTextInternal::Line line = m_state->m_lineList[lId];
    std::vector<long> listDelimiterCells;
    bool ok=false;
    std::map<long,size_t>::const_iterator actTPosIt=tPosIt;
    while (tPosIt!=cposToLineMap.end()) {
      long cPos=tPosIt->first;
      lId=tPosIt++->second;
      listDelimiterCells.push_back(cPos);
      if (lId>=numLines) {
        MWAW_DEBUG_MSG(("MsWrdText::prepareTableLimits: lId is bad(II)\n"));
        break;
      }
      line=m_state->m_lineList[lId];
      MsWrdStruct::Paragraph para(vers);
      // try to retrieve the paragraph attributes
      std::multimap<long, MsWrdText::PLC>::const_iterator plcIt;
      plcIt=m_state->m_plcMap.lower_bound(cPos);
      while (plcIt != m_state->m_plcMap.end() && plcIt->first==cPos) {
        MsWrdText::PLC const &plc = plcIt++->second;
        if (plc.m_type != PLC::Paragraph)
          continue;
        if (plc.m_id>=0)
          m_stylesManager->getParagraph(MsWrdTextStyles::TextZone, plc.m_id, para);
        if (para.m_styleId.isSet()) {
          MsWrdStruct::Paragraph style(vers);
          m_stylesManager->getParagraph(MsWrdTextStyles::StyleZone,*para.m_styleId, style);
          style.insert(para);
          para=style;
        }
      }
      std::map<long, int>::const_iterator pIt;
      pIt=m_state->m_paragraphLimitMap.find(line.m_cPos[0]);
      if (pIt!=m_state->m_paragraphLimitMap.end() && pIt->second>0 && pIt->second<(int)numTextpos) {
        MsWrdTextInternal::TextStruct const &textEntry=m_state->m_textposList[(size_t) pIt->second];
        int id=textEntry.getParagraphId();
        if (id>=0) {
          MsWrdStruct::Paragraph modifier(vers);
          m_stylesManager->getParagraph(MsWrdTextStyles::TextStructZone, id, modifier);
          para.insert(modifier);
        }
      }
      if (!para.m_tableDef.get() || !para.m_table.isSet() || !para.m_table->m_columns.isSet())
        continue;
      m_state->m_lineList[lId].m_type=MsWrdTextInternal::Line::L_LastRowCell;

      // ok, we have find the end of the table
      MsWrdStruct::Table const &table = para.m_table.get();
      size_t numCols=table.m_columns->size();
      if (!numCols || listDelimiterCells.size()!=numCols) {
        MWAW_DEBUG_MSG(("MsWrdText::prepareTableLimits: can not find the number of row for position %ld(%d,%d)\n", line.m_cPos[0], int(listDelimiterCells.size()), (int) numCols));
        break;
      }

      shared_ptr<MsWrdTextInternal::Table> finalTable(new MsWrdTextInternal::Table);
      finalTable->m_delimiterPos = listDelimiterCells;
      finalTable->m_cells = table.m_cells;
      if (table.m_height.isSet())
        finalTable->m_height=*table.m_height;
      std::vector<float> width(numCols-1);
      for (size_t c = 0; c < numCols-1; c++)
        width[c]=table.m_columns.get()[c+1]-table.m_columns.get()[c];
      finalTable->setColsSize(width);
      for (size_t i=0; i < listDelimiterCells.size(); ++i)
        m_state->m_tableMap[listDelimiterCells[i]]=finalTable;
      listDelimiterCells.clear();
      ok=true;
      break;
    }
    if (ok)
      continue;

    ascFile.addPos(m_state->getFilePos(listDelimiterCells[0]));
    ascFile.addNote("###table");
    m_state->m_tableMap[listDelimiterCells[0]]=shared_ptr<MsWrdTextInternal::Table>();
    tPosIt=++actTPosIt;
    MWAW_DEBUG_MSG(("MsWrdText::prepareTableLimits: problem finding some table limits\n"));
  }
}

bool MsWrdText::updateTableBeginnningAt(long cPos, long &nextCPos)
{
  std::map<long,shared_ptr<MsWrdTextInternal::Table> >::iterator tableIt=m_state->m_tableMap.lower_bound(cPos);
  if (tableIt==m_state->m_tableMap.end() || !tableIt->second ||
      tableIt->second->m_delimiterPos.empty() ||
      tableIt->second->m_delimiterPos[0] < cPos) {
    MWAW_DEBUG_MSG(("MsWrdText::updateTableBeginnningAt: can find no table at position %ld\n", cPos));
    return false;
  }
  shared_ptr<MsWrdTextInternal::Table> table=tableIt->second;
  size_t numDelim=table->m_delimiterPos.size();
  table->m_cellPos.resize(numDelim);
  table->m_cellPos[0]=cPos;
  for (size_t c=0; c+1<numDelim; ++c)
    table->m_cellPos[c+1]=table->m_delimiterPos[c]+1;
  for (size_t c=0; c+1<table->m_cellPos.size(); ++c)
    m_state->m_tableCellPosSet.insert(table->m_cellPos[c]);
  if (table->m_delimiterPos[0]!=cPos)
    m_state->m_tableMap[cPos]=table;
  nextCPos=table->m_delimiterPos[numDelim-1]+1;
  return true;
}

void MsWrdText::prepareData()
{
#if defined(DEBUG_WITH_FILES) && DEBUG_PARAGRAPH
  int const vers = version();
#endif
  long cPos = 0, cEnd = m_state->getTotalTextSize();
  if (cEnd <= 0) return;
  prepareLines();
  convertFilePLCPos();
  prepareTableLimits();

  prepareParagraphProperties();
  prepareFontProperties();

  MsWrdStruct::Font defaultFont;
  long pos = m_state->getFilePos(cPos);
  int textposSize = int(m_state->m_textposList.size());

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f, f2;
  PLC::ltstr compare;

  std::multimap<long, PLC> &map = m_state->m_plcMap;
  std::multimap<long, PLC>::iterator plcIt;
  while (cPos < cEnd) {
    f.str("");
    // first find the list of the plc
    long cNextPos = cEnd;

    std::set<PLC, PLC::ltstr> sortedPLC(compare);
    plcIt = map.lower_bound(cPos);
    while (plcIt != map.end()) {
      if (plcIt->first != cPos) {
        cNextPos=plcIt->first;
        break;
      }
      PLC const &plc = plcIt++->second;
      if (plc.m_type!=PLC::Paragraph&&plc.m_type!=PLC::Font)
        sortedPLC.insert(plc);
#if DEBUG_PLC
      if (plc.m_type != PLC::TextPosition)
        f << "[" << plc << "],";
#endif

      int pId = plc.m_id;
      switch (plc.m_type) {
      case PLC::TextPosition:
        if (pId < 0 || pId > textposSize) {
          MWAW_DEBUG_MSG(("MsWrdText::prepareData: oops can not find textstruct!!!!\n"));
          f << "[###tP" << pId << "]";
        }
        else {
          MsWrdTextInternal::TextStruct const &textEntry=m_state->m_textposList[(size_t) pId];
          pos = textEntry.begin();
#if defined(DEBUG_WITH_FILES) && DEBUG_PARAGRAPH
          int paraId=textEntry.getParagraphId();
          if (paraId < 0)
            f << "tP_,";
          else {
            MsWrdStruct::Paragraph para(vers);
            m_stylesManager->getParagraph(MsWrdTextStyles::TextStructZone, paraId, para);
            f << "tP" << paraId << "=[";
            para.print(f, m_parserState->m_fontConverter);
            f << "],";
          }
#endif
        }
        break;
      case PLC::Section:
#if defined(DEBUG_WITH_FILES) && DEBUG_SECTION
        if (pId >= 0) {
          MsWrdStruct::Section sec;
          m_stylesManager->getSection(MsWrdTextStyles::TextZone, pId, sec);
          f << "S" << pId << "=[" << sec << "],";
        }
        else
          f << "S_,";
#endif
        break;
      case PLC::ParagraphInfo:
#if defined(DEBUG_WITH_FILES) && DEBUG_PARAGRAPHINFO
        if (pId >= 0 && pId < int(m_state->m_paraInfoList.size())) {
          MsWrdStruct::ParagraphInfo info=m_state->m_paraInfoList[(size_t) pId];
          f << "Pi" << pId  << "=[" << info << "],";
        }
        else
          f << "Pi_,";
#endif
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
#if defined(DEBUG_WITH_FILES) && DEBUG_PARAGRAPH
        if (pId >= 0) {
          MsWrdStruct::Paragraph para(vers);
          m_stylesManager->getParagraph(MsWrdTextStyles::TextZone, pId, para);
          f << "P" << pId << "=[";
          para.print(f, m_parserState->m_fontConverter);
          f << "],";
        }
        else f << "P_,";
#endif
        break;
      case PLC::Font: {
#if defined(DEBUG_WITH_FILES) && DEBUG_FONT
        if (pId >= 0) {
          MsWrdStruct::Font font;
          m_stylesManager->getFont(MsWrdTextStyles::TextZone, pId, font);
          f << "F" << pId << "=[" << font.m_font->getDebugString(m_parserState->m_fontConverter) << font << "],";
        }
        else
          f << "F_,";
#endif
        break;
      }
      case PLC::Field:
      case PLC::Footnote:
      case PLC::FootnoteDef:
      case PLC::HeaderFooter:
      case PLC::Object:
      default:
        break;
      }
    }
    MsWrdTextInternal::Property prop;
    prop.m_fPos = pos;
    prop.m_plcList=std::vector<PLC>(sortedPLC.begin(), sortedPLC.end());

    if (f.str().length()) {
      f2.str("");
      f2 << "TextContent["<<cPos<<"]:" << f.str();
      ascFile.addPos(pos);
      ascFile.addNote(f2.str().c_str());
#if defined(DEBUG_WITH_FILES)
      MsWrdTextInternal::debugFile2() << f2.str() << "\n";
#endif
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
bool MsWrdText::sendText(MWAWEntry const &textEntry, bool mainZone, bool tableCell)
{
  if (!textEntry.valid()) return false;
  MWAWTextListenerPtr listener=m_parserState->m_textListener;
  if (!listener) {
    MWAW_DEBUG_MSG(("MsWrdText::sendText: can not find a listener!"));
    return true;
  }
  long cPos = textEntry.begin();
  long debPos = m_state->getFilePos(cPos), pos=debPos;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  long cEnd = textEntry.end();

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "TextContent[" << cPos << "]:";
  long pictPos = -1;
  while (!input->isEnd() && cPos < cEnd) {
    bool newTable = false;
    long cEndPos = cEnd;

    MsWrdTextInternal::Property *prop = 0;
    std::map<long,MsWrdTextInternal::Property>::iterator propIt = m_state->m_propertyMap.upper_bound(cPos);
    if (propIt != m_state->m_propertyMap.end() && propIt->first < cEndPos && propIt->first > cPos)
      cEndPos = propIt->first;

    size_t numPLC = 0;
    propIt = m_state->m_propertyMap.find(cPos);
    if (propIt != m_state->m_propertyMap.end()) {
      prop = &propIt->second;
      pos = prop->m_fPos;
      newTable = !tableCell && m_state->getTable(cPos);
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      numPLC = prop->m_plcList.size();
    }
    int newSectionId=-1;
    for (size_t i = 0; i < numPLC; i++) {
      PLC const &plc = prop->m_plcList[i];
      if (newTable && int(plc.m_type) >= int(PLC::ParagraphInfo)) continue;
      switch (plc.m_type) {
      case PLC::Page: {
        if (tableCell) break;
        if (mainZone) m_mainParser->newPage(++m_state->m_actPage);
        break;
      }
      case PLC::Section:
        if (tableCell) break;
        newSectionId=plc.m_id;
        break;
      case PLC::Field: // some fields ?
#ifdef DEBUG
        m_mainParser->sendFieldComment(plc.m_id);
#endif
        break;
      case PLC::Footnote:
        m_mainParser->sendFootnote(plc.m_id);
        break;
      case PLC::TextPosition:
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
    if (newSectionId >= 0)
      sendSection(newSectionId);
    if ((prop && prop->m_debugPrint)  || newTable) {
      ascFile.addPos(debPos);
      ascFile.addNote(f.str().c_str());
#if defined(DEBUG_WITH_FILES)
      MsWrdTextInternal::debugFile2() << f.str() << "\n";
#endif
      f.str("");
      f << "TextContent["<<cPos<<"]:";
      debPos = pos;
    }
    // time to send the table
    shared_ptr<MsWrdTextInternal::Table> table;
    if (newTable && (table=m_state->getTable(cPos))) {
      long actCPos = cPos;
      bool ok = sendTable(*table);
      cPos = ok ? table->m_cellPos.back()+1 : actCPos;
      pos=debPos=m_state->getFilePos(cPos);
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      f.str("");
      f << "TextContent["<<cPos<<"]:";
      if (ok)
        continue;
    }
    if (m_state->m_paragraphMap.find(cPos) != m_state->m_paragraphMap.end())
      listener->setParagraph(m_state->m_paragraphMap.find(cPos)->second);
    if (m_state->m_fontMap.find(cPos) != m_state->m_fontMap.end()) {
      MsWrdStruct::Font font = m_state->m_fontMap.find(cPos)->second;
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
          MWAW_DEBUG_MSG(("MsWrdText::sendText: can not find picture\n"));
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
        if (cPos!=cEnd)
          listener->insertEOL(true);
        break;
      case 0xd: // line break hard
        if (cPos!=cEnd)
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

bool MsWrdText::sendSection(int secId)
{
  int textStructId=-1;
  if (!m_state->m_textposList.empty() &&
      secId>=0 && secId+1<(int)m_state->m_sectionLimitList.size()) {
    int tId=m_state->getTextStructId
            (m_state->m_sectionLimitList[(size_t)secId+1]-1);
    if (tId>=0 && tId<(int)m_state->m_textposList.size())
      textStructId=m_state->m_textposList[(size_t)tId].getParagraphId();
  }
  return m_stylesManager->sendSection(secId, textStructId);
}

////////////////////////////////////////////////////////////
// try to read a table
////////////////////////////////////////////////////////////
bool MsWrdText::sendTable(MsWrdTextInternal::Table const &table)
{
  MWAWTextListenerPtr listener=m_parserState->m_textListener;
  if (!listener) {
    MWAW_DEBUG_MSG(("MsWrdText::sendTable: can not find a listener!\n"));
    return true;
  }
  size_t nCells = table.m_cellPos.size();
  if (nCells < 1) {
    MWAW_DEBUG_MSG(("MsWrdText::sendTable: numcols pos is bad\n"));
    return true;
  }

  size_t numCols = table.getColsSize().size()+1;
  size_t numRows = nCells/numCols;

  float height = table.m_height;
  if (height > 0) height*=-1;

  listener->openTable(table);
  size_t numCells = table.m_cells.size();
  for (size_t r = 0; r < numRows; r++) {
    listener->openTableRow(height, librevenge::RVNG_INCH);
    for (size_t c = 0; c < numCols-1; c++) {
      MWAWCell cell;
      size_t cellPos = r*numCols+c;
      if (cellPos < numCells && table.m_cells[cellPos].isSet()) {
        int const wh[] = { libmwaw::TopBit, libmwaw::LeftBit,
                           libmwaw::BottomBit, libmwaw::RightBit
                         };
        MsWrdStruct::Table::Cell const &tCell = table.m_cells[cellPos].get();
        for (size_t i = 0; i < 4 && i < tCell.m_borders.size(); i++) {
          if (!tCell.m_borders[i].isSet() ||
              tCell.m_borders[i]->m_style==MWAWBorder::None) continue;
          cell.setBorders(wh[i], tCell.m_borders[i].get());
        }
        if (tCell.m_backColor.isSet()) {
          unsigned char col = (unsigned char)(tCell.m_backColor.get()*255.f);
          cell.setBackgroundColor(MWAWColor(col,col,col));
        }
        else if (!table.m_backgroundColor.isWhite())
          cell.setBackgroundColor(table.m_backgroundColor);
      }
      cell.setPosition(Vec2i((int)c,(int)r));

      listener->openTableCell(cell);

      MsWrdEntry textData;
      textData.setBegin(table.m_cellPos[cellPos]);
      long cEndPos = table.m_cellPos[cellPos+1]-1;
      textData.setEnd(cEndPos);
      if (textData.length()<=0)
        listener->insertChar(' ');
      else
        sendText(textData, false, true);
#if defined(DEBUG_WITH_FILES)
      MsWrdTextInternal::debugFile2() << "TextContent["<<cEndPos<<"]:" << char(7) << "\n";
#endif
      listener->closeTableCell();
    }
    listener->closeTableRow();
  }
  listener->closeTable();
  return true;
}

bool MsWrdText::sendMainText()
{
  MWAWEntry entry;
  entry.setBegin(0);
  entry.setLength(m_state->m_textLength[0]);
  sendText(entry, true);
  return true;
}

bool MsWrdText::sendFootnote(int id)
{
  MWAWTextListenerPtr listener=m_parserState->m_textListener;
  if (!listener) return true;
  if (id < 0 || id >= int(m_state->m_footnoteList.size())) {
    MWAW_DEBUG_MSG(("MsWrdText::sendFootnote: can not find footnote %d\n", id));
    listener->insertChar(' ');
    return false;
  }
  MsWrdTextInternal::Footnote &footnote = m_state->m_footnoteList[(size_t) id];
  if (footnote.m_pos.isParsed())
    listener->insertChar(' ');
  else
    sendText(footnote.m_pos, false);
  footnote.m_pos.setParsed();
  return true;
}

bool MsWrdText::sendFieldComment(int id)
{
  MWAWTextListenerPtr listener=m_parserState->m_textListener;
  if (!listener) return true;
  if (id < 0 || id >= int(m_state->m_fieldList.size())) {
    MWAW_DEBUG_MSG(("MsWrdText::sendFieldComment: can not find field %d\n", id));
    listener->insertChar(' ');
    return false;
  }
  MsWrdStruct::Font defFont;
  defFont.m_font = m_stylesManager->getDefaultFont();
  m_stylesManager->setProperty(defFont);
  m_stylesManager->sendDefaultParagraph();
  std::string const &text = m_state->m_fieldList[(size_t) id].m_text;
  if (!text.length()) listener->insertChar(' ');
  for (size_t c = 0; c < text.length(); c++)
    listener->insertCharacter((unsigned char) text[c]);
  return true;
}

void MsWrdText::flushExtra()
{
#ifdef DEBUG
  if (m_state->m_textLength[1]) {
    for (size_t i = 0; i < m_state->m_footnoteList.size(); i++) {
      MsWrdTextInternal::Footnote &footnote = m_state->m_footnoteList[i];
      if (footnote.m_pos.isParsed()) continue;
      sendText(footnote.m_pos, false);
      footnote.m_pos.setParsed();
    }
  }
#endif
}


// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
