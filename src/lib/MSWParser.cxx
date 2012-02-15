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
#include <set>
#include <sstream>

#include <libwpd/WPXString.h>

#include "TMWAWPosition.hxx"
#include "TMWAWPictMac.hxx"
#include "TMWAWPrint.hxx"

#include "IMWAWHeader.hxx"

#include "MWAWStruct.hxx"
#include "MWAWTools.hxx"
#include "MWAWContentListener.hxx"

#include "MSWParser.hxx"

/** Internal: the structures of a MSWParser */
namespace MSWParserInternal
{
////////////////////////////////////////
//! Internal: the entry of MSWParser
struct Entry : public IMWAWEntry {
  Entry() : IMWAWEntry(), m_id(-1) {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Entry const &entry) {
    if (entry.type().length()) {
      o << entry.type();
      if (entry.m_id >= 0) o << "[" << entry.m_id << "]";
      o << "=";
    }
    return o;
  }
  //! the identificator
  int m_id;
};

////////////////////////////////////////
//! Internal: the entry of MSWParser
struct TextEntry : public IMWAWEntry {
  TextEntry() : IMWAWEntry(), m_pos(-1) {
    for (int i = 0; i < 2; i++) m_values[i] = 0;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, TextEntry const &entry) {
    if (entry.m_pos>=0) o << "textPos=" << entry.m_pos << ",";
    o << std::hex;
    if (entry.valid())
      o << "fPos=" << entry.begin() << ":" << entry.end() << ",";
    for (int i = 0; i < 2; i++) {
      if (entry.m_values[i])
        o << "f" << i << "=" << entry.m_values[i] << ",";
    }
    o << std::dec;
    return o;
  }
  //! the text position
  int m_pos;
  //! the identificator
  int m_values[2];
};


////////////////////////////////////////
//! Internal: the plc
struct PLC {
  PLC(int type) : m_type(type), m_extra("") {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, PLC const &plc) {
    o << "[";
    switch(plc.m_type) {
    case 1:
      o << "line,";
      break;
    case 3:
      o << "page,";
      break;
    default:
      o << char('a'+plc.m_type);
    }
    if (plc.m_extra.length()) o << ":" << plc.m_extra;
    o << "]";
    return o;
  }

  //! the plc type
  int m_type;
  //! some extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the font of MSWParser
struct Font {
  //! the constructor
  Font(): m_font(), m_size(0), m_value(0), m_default(true), m_extra("") {
    for (int i = 0; i < 3; i++) m_flags[i] = 0;
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Font const &font) {
    for (int i = 0; i < 3; i++) {
      if (!font.m_flags[i]) continue;
      o << "ft" << i << "=";
      o << std::hex << font.m_flags[i] << std::dec << ",";
    }
    if (font.m_size && font.m_size != font.m_font.size())
      o << "#size2=" << font.m_size << ",";
    if (font.m_value) o << "id?=" << font.m_value << ",";
    if (font.m_extra.length())
      o << font.m_extra << ",";
    return o;
  }

  //! the font
  MWAWStruct::Font m_font;
  //! a second size
  int m_size;
  //! a unknown value
  int m_value;
  //! some unknown flag
  int m_flags[3];
  //! true if is default
  bool m_default;
  //! extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the font of MSWParser
struct Paragraph {
  //! the constructor
  Paragraph(): m_font(), m_font2(), m_extra("") {
  }

  //! operator<<
  void print(std::ostream &o, MWAWTools::ConvertissorPtr m_convertissor) const {
    if (!m_font2.m_default)
      o << "font=[" << m_convertissor->getFontDebugString(m_font2.m_font) << m_font2 << "],";
    else if (!m_font.m_default)
      o << "font=[" << m_convertissor->getFontDebugString(m_font2.m_font) << m_font2 << "],";
    if (m_extra.length())
      o << m_extra << ",";
  }

  //! the font (simplified)
  Font m_font, m_font2 /** font ( not simplified )*/;
  //! extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the object of MSWParser
struct Object {
  Object() : m_textPos(-1), m_pos(), m_name(""), m_id(-1), m_extra("") {
    for (int i = 0; i < 2; i++) {
      m_ids[i] = -1;
      m_idsFlag[i] = 0;
    }
    for (int i = 0; i < 2; i++) m_flags[i] = 0;
  }

  Entry getEntry() const {
    Entry res;
    res.setBegin(m_pos.begin());
    res.setEnd(m_pos.end());
    res.setType("ObjectData");
    res.m_id = m_id;
    return res;
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Object const &obj) {
    if (obj.m_textPos >= 0)
      o << std::hex << "textPos?=" << obj.m_textPos << std::dec << ",";
    if (obj.m_id >= 0) o << "Obj" << obj.m_id << ",";
    if (obj.m_name.length()) o << obj.m_name << ",";
    for (int st = 0; st < 2; st++) {
      if (obj.m_ids[st] == -1 && obj.m_idsFlag[st] == 0) continue;
      o << "id" << st << "=" << obj.m_ids[st];
      if (obj.m_idsFlag[st]) o << ":" << std::hex << obj.m_idsFlag[st] << std::dec << ",";
    }
    for (int st = 0; st < 2; st++) {
      if (obj.m_flags[st])
        o << "fl" << st << "=" << std::hex << obj.m_flags[st] << std::dec << ",";
    }

    if (obj.m_extra.length()) o << "extras=[" << obj.m_extra << "],";
    return o;
  }
  //! the text position
  long m_textPos;

  //! the object entry
  IMWAWEntry m_pos;

  //! the object name
  std::string m_name;

  //! the id
  int m_id;

  //! some others id?
  int m_ids[2];

  //! some flags link to m_ids
  int m_idsFlag[2];

  //! some flags
  int m_flags[2];

  //! some extra data
  std::string m_extra;
};
////////////////////////////////////////
//! Internal: the state of a MSWParser
struct State {
  //! constructor
  State() : m_version(-1), m_eof(-1), m_bot(-1), m_eot(-1),
    m_textLength(0), m_footnoteLength(0), m_headerfooterLength(0),
    m_entryMap(), m_textposEntryMap(), m_textposSet(), m_plcMap(), m_filePlcMap(),
    m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0) {
  }

  //! returns the total text size
  long getTotalTextSize() const {
    return m_textLength + m_footnoteLength + m_headerfooterLength;
  }
  //! create the text pos set
  void createTextposSet() {
    if (m_textposSet.size()) return;
    std::map<long, TextEntry>::const_iterator it = m_textposEntryMap.begin();
    for ( ; it != m_textposEntryMap.end(); it++) m_textposSet.insert(it->first);
  }

  //! return the file text position
  long getFileTextPos(long textPos) {
    createTextposSet();
    long decal = 0;
    std::set<long>::const_iterator it = m_textposSet.find(textPos);
    if (it == m_textposSet.begin()) {
      MWAW_DEBUG_MSG(("MSWParserInternal::State::getFileTextPos: can not find pos %ld\n", textPos));
      return -1;
    }
    if (it == m_textposSet.end() || *it != textPos) {
      it--;
      decal = textPos-*it;
      textPos = *it;
    }
    std::map<long, TextEntry>::const_iterator mIt =  m_textposEntryMap.find(textPos);
    if (mIt == m_textposEntryMap.end()) {
      MWAW_DEBUG_MSG(("MSWParserInternal::State::getFileTextPos: can not find pos %ld, internal problem...\n", textPos));
      return -1;
    }
    return mIt->second.begin()+decal;
  }

  //! the file version
  int m_version;

  //! end of file
  long m_eof;
  //! the begin of the text
  long m_bot;
  //! end of the text
  long m_eot;
  //! the total textlength
  long m_textLength;
  //! the size of the footnote data
  long m_footnoteLength;
  //! the size of the header/footer data
  long m_headerfooterLength;

  //! the list of entries
  std::multimap<std::string, Entry> m_entryMap;

  //! the text correspondance zone ( textpos, textEntry )
  std::map<long, TextEntry> m_textposEntryMap;

  //! the text position which appear in m_textposEntryMap
  std::set<long> m_textposSet;

  //! the text correspondance zone ( textpos, plc )
  std::multimap<long, PLC> m_plcMap;

  //! the text correspondance zone ( filepos, plc )
  std::multimap<long, PLC> m_filePlcMap;

  //! the list of object ( mainZone, other zone)
  std::vector<Object> m_objectList[2];

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;

};

////////////////////////////////////////
//! Internal: the subdocument of a MSWParser
class SubDocument : public IMWAWSubDocument
{
public:
  SubDocument(MSWParser &pars, TMWAWInputStreamPtr input, int id) :
    IMWAWSubDocument(&pars, input, IMWAWEntry()), m_id(id) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(IMWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(IMWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(IMWAWContentListenerPtr &listener, DMWAWSubDocumentType type);

protected:
  //! the subdocument file position
  int m_id;
};

void SubDocument::parse(IMWAWContentListenerPtr &listener, DMWAWSubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("SubDocument::parse: no listener\n"));
    return;
  }
  MSWContentListener *listen = dynamic_cast<MSWContentListener *>(listener.get());
  if (!listen) {
    MWAW_DEBUG_MSG(("SubDocument::parse: bad listener\n"));
    return;
  }

  assert(m_parser);

  long pos = m_input->tell();
  //  reinterpret_cast<MSWParser *>(m_parser)->send(m_id);
  m_input->seek(pos, WPX_SEEK_SET);
}

bool SubDocument::operator!=(IMWAWSubDocument const &doc) const
{
  if (IMWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_id != sDoc->m_id) return true;
  return false;
}
}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MSWParser::MSWParser(TMWAWInputStreamPtr input, IMWAWHeader * header) :
  IMWAWParser(input, header), m_listener(), m_convertissor(), m_state(),
  m_pageSpan(), m_listSubDocuments(),
  m_asciiFile(), m_asciiName("")
{
  init();
}

MSWParser::~MSWParser()
{
  if (m_listener.get()) m_listener->endDocument();
}

void MSWParser::init()
{
  m_convertissor.reset(new MWAWTools::Convertissor);
  m_listener.reset();
  m_asciiName = "main-1";

  m_state.reset(new MSWParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  m_pageSpan.setMarginTop(0.1);
  m_pageSpan.setMarginBottom(0.1);
  m_pageSpan.setMarginLeft(0.1);
  m_pageSpan.setMarginRight(0.1);
}

void MSWParser::setListener(MSWContentListenerPtr listen)
{
  m_listener = listen;
}

int MSWParser::version() const
{
  return m_state->m_version;
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
float MSWParser::pageHeight() const
{
  return m_pageSpan.getFormLength()-m_pageSpan.getMarginTop()-m_pageSpan.getMarginBottom()-m_state->m_headerHeight/72.0-m_state->m_footerHeight/72.0;
}

float MSWParser::pageWidth() const
{
  return m_pageSpan.getFormWidth()-m_pageSpan.getMarginLeft()-m_pageSpan.getMarginRight();
}

////////////////////////////////////////////////////////////
// new page and color
////////////////////////////////////////////////////////////
void MSWParser::newPage(int number)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!m_listener || m_state->m_actPage == 1)
      continue;
    m_listener->insertBreak(DMWAW_PAGE_BREAK);
  }
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void MSWParser::parse(WPXDocumentInterface *docInterface)
{
  assert(getInput().get() != 0);

  if (!checkHeader(0L))  throw(libmwaw_libwpd::ParseException());
  bool ok = true;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());

    checkHeader(0L);
    ascii().addPos(getInput()->tell());
    ascii().addNote("_");

    ok = createZones();
    if (ok) {
      createDocument(docInterface);

      std::map<long, MSWParserInternal::TextEntry>::iterator tIt;
      for (tIt = m_state->m_textposEntryMap.begin(); tIt != m_state->m_textposEntryMap.end(); tIt++) {
        readText(tIt->second);
      }
    }

    ascii().reset();
  } catch (...) {
    MWAW_DEBUG_MSG(("MSWParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  if (!ok) throw(libmwaw_libwpd::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void MSWParser::createDocument(WPXDocumentInterface *documentInterface)
{
  if (!documentInterface) return;
  if (m_listener) {
    MWAW_DEBUG_MSG(("MSWParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  std::list<DMWAWPageSpan> pageList;
  DMWAWPageSpan ps(m_pageSpan);

  int numPage = 1;
  m_state->m_numPages = numPage;

  for (int i = 0; i <= m_state->m_numPages; i++) pageList.push_back(ps);

  //
  MSWContentListenerPtr listen =
    MSWContentListener::create(pageList, documentInterface, m_convertissor);
  setListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// try to find the different zone
////////////////////////////////////////////////////////////
bool MSWParser::createZones()
{
  if (!readZoneList()) return false;
  TMWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  if (pos != m_state->m_bot) {
    ascii().addPos(pos);
    ascii().addNote("_");
  }
  libmwaw_tools::DebugStream f;
  ascii().addPos(m_state->m_eot);
  ascii().addNote("_");

  std::multimap<std::string, MSWParserInternal::Entry>::iterator it;
  it = m_state->m_entryMap.find("PrintInfo");
  if (it != m_state->m_entryMap.end())
    readPrintInfo(it->second);

  it = m_state->m_entryMap.find("DocSum");
  if (it != m_state->m_entryMap.end())
    readDocSum(it->second);

  it = m_state->m_entryMap.find("Printer");
  if (it != m_state->m_entryMap.end())
    readPrinter(it->second);

  it = m_state->m_entryMap.find("FontNames");
  if (it != m_state->m_entryMap.end())
    readFontNames(it->second);

  readObjects();

  it = m_state->m_entryMap.find("Zone18");
  if (it != m_state->m_entryMap.end())
    readZone18(it->second);

  it = m_state->m_entryMap.find("LineInfo");
  if (it != m_state->m_entryMap.end())
    readLineInfo(it->second);

  it = m_state->m_entryMap.find("Zone17");
  if (it != m_state->m_entryMap.end())
    readZone17(it->second);

  it = m_state->m_entryMap.find("Glossary");
  if (it != m_state->m_entryMap.end())
    readGlossary(it->second);
  it = m_state->m_entryMap.find("GlossPos");
  if (it != m_state->m_entryMap.end()) { // a list of text pos ( or a size from ? )
    std::vector<int> list;
    readIntsZone(it->second, 4, list);
  }

  it = m_state->m_entryMap.find("HeaderFooter");
  if (it != m_state->m_entryMap.end()) { // list of header/footer size
    std::vector<int> list;
    readIntsZone(it->second, 4, list);
  }
  it = m_state->m_entryMap.find("FontIds");
  if (it != m_state->m_entryMap.end()) {
    std::vector<int> list;
    readIntsZone(it->second, 2, list);
  }

  it = m_state->m_entryMap.find("Section");
  if (it != m_state->m_entryMap.end())
    readSection(it->second);

  it = m_state->m_entryMap.find("PageBreak");
  if (it != m_state->m_entryMap.end())
    readPageBreak(it->second);

  it = m_state->m_entryMap.find("TextData2");
  while (it != m_state->m_entryMap.end()) {
    if (!it->second.hasType("TextData2")) break;
    MSWParserInternal::Entry &entry=it++->second;
    std::vector<std::string> list;
    readTextData2(entry);
  }

  it = m_state->m_entryMap.find("Styles");
  long prevDeb = 0;
  while (it != m_state->m_entryMap.end()) {
    if (!it->second.hasType("Styles")) break;
    MSWParserInternal::Entry &entry=it++->second;
    if (entry.begin() == prevDeb) continue;
    prevDeb = entry.begin();
    readStyles(entry);
  }

  it = m_state->m_entryMap.find("SectData");
  while (it != m_state->m_entryMap.end()) {
    if (!it->second.hasType("SectData")) break;
    readSectionData(it++->second);
  }

  searchPictures();
  it = m_state->m_entryMap.find("Picture");
  while (it != m_state->m_entryMap.end()) {
    if (!it->second.hasType("Picture")) break;
    MSWParserInternal::Entry &entry=it++->second;
    readPicture(entry);
  }

  if (m_state->m_textposEntryMap.size() == 0) {
    if (m_state->m_eot >= m_state->m_bot+m_state->getTotalTextSize()) {
      MSWParserInternal::TextEntry tEntry;
      tEntry.m_pos = 0;
      tEntry.setBegin(m_state->m_bot);
      tEntry.setLength(m_state->m_textLength);
      if (tEntry.valid())
        m_state->m_textposEntryMap[0] = tEntry;
    } else {
      MWAW_DEBUG_MSG(("MSWParser::createZones: can not find all text zones...\n"));
      MSWParserInternal::TextEntry tEntry;
      tEntry.m_pos = 0;
      tEntry.setBegin(m_state->m_bot);
      tEntry.setEnd(m_state->m_eot);
      if (tEntry.valid())
        m_state->m_textposEntryMap[0] = tEntry;
    }
  }

  for (it=m_state->m_entryMap.begin(); it!=m_state->m_entryMap.end(); it++) {
    MSWParserInternal::Entry const &entry = it->second;
    if (entry.isParsed()) continue;
    ascii().addPos(entry.begin());
    f.str("");
    f << entry;
    ascii().addNote(f.str().c_str());
    ascii().addPos(entry.end());
    ascii().addNote("_");

  }
  return m_state->m_textposEntryMap.size() != 0;
}

////////////////////////////////////////////////////////////
// read the zone list
////////////////////////////////////////////////////////////
bool MSWParser::readZoneList()
{
  TMWAWInputStreamPtr input = getInput();

  int numData = version() <= 3 ? 15: 20;
  std::stringstream s;
  for (int i = 0; i < numData; i++) {
    switch(i) {
      // the first two zone are often simillar : even/odd header/footer ?
    case 0:
      readEntry("Styles", 0);
      break; // checkme: size is often invalid
    case 1:
      readEntry("Styles", 1);
      break;
    case 2:
      readEntry("FootnotePos");
      break;
    case 3:
      readEntry("FootnoteSize");
      break; // size
    case 4:
      readEntry("Section");
      break;
    case 5:
      readEntry("PageBreak");
      break;
    case 6:
      readEntry("Glossary");
      break;
    case 7:
      readEntry("GlossPos");
      break; // size ?
    case 8:
      readEntry("HeaderFooter");
      break; // size
    case 9:
      readEntry("TextData2",0);
      break;
    case 10:
      readEntry("TextData2",1);
      break;
    case 12:
      readEntry("FontIds");
      break;
    case 13:
      readEntry("PrintInfo");
      break;
    case 14:
      readEntry("LineInfo");
      break;
    case 16:
      readEntry("Printer");
      break;
    default:
      s.str("");
      s << "Zone" << i;
      if (i < 4) s << "_";
      readEntry(s.str());
      break;
    }
  }

  long pos = input->tell();
  libmwaw_tools::DebugStream f;
  f << "Entries(ListZoneData)[0]:";
  for (int i = 0; i < 2; i++) // two small int
    f << "f" << i << "=" << input->readLong(2) << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  if (version() <= 4) return true;

  // main
  readEntry("ObjectName",0);
  readEntry("FontNames");
  readEntry("ObjectList",0);
  readEntry("ObjectFlags",0);
  readEntry("DocSum",0);
  for (int i = 25; i < 31; i++) {
    /* check me: Zone25, Zone26, Zone27: also some object name, list, flags ? */
    // header/footer
    if (i==28) readEntry("ObjectName",1);
    else if (i==29) readEntry("ObjectList",1);
    else if (i==30) readEntry("ObjectFlags",1);
    else {
      s.str("");
      s << "Zone" << i;
      readEntry(s.str());
    }
  }

  pos = input->tell();
  f.str("");
  f << "ListZoneData[1]:";

  long val = input->readLong(2);
  if (val) f << "unkn=" << val << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  if (input->atEOS()) {
    MWAW_DEBUG_MSG(("MSWParser::readZoneList: can not read list zone\n"));
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool MSWParser::checkHeader(IMWAWHeader *header, bool strict)
{
  *m_state = MSWParserInternal::State();

  TMWAWInputStreamPtr input = getInput();

  libmwaw_tools::DebugStream f;
  int headerSize=64;
  input->seek(headerSize,WPX_SEEK_SET);
  if (int(input->tell()) != headerSize) {
    MWAW_DEBUG_MSG(("MSWParser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0, WPX_SEEK_SET);
  int val = input->readULong(2);
  switch (val) {
  case 0xfe34:
    switch (input->readULong(2)) {
    case 0x0:
      headerSize = 30;
      m_state->m_version = 3;
#ifndef DEBUG
      return false;
#else
      break;
#endif
    default:
      return false;
    }
    break;
  case 0xfe37:
    switch (input->readULong(2)) {
    case 0x1c:
      m_state->m_version = 4;
      break;
    case 0x23:
      m_state->m_version = 5;
      break;
    default:
      return false;
    }
    break;
  default:
    return false;
  }

  f << "FileHeader:";
  val = input->readLong(1);
  if (val) f << "f0=" << val << ",";
  for (int i = 1; i < 3; i++) { // always 0
    val = input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  val = input->readLong(2); // v4-v5: find 4, 8, c, 24, 2c
  if (val)
    f << "unkn=" << std::hex << val << std::dec << ",";
  val = input->readLong(1); // always 0 ?
  if (val) f << "f4=" << val << ",";
  val = input->readLong(2); // always 0x19: for version 4, 5
  if (val!=0x19) f << "f5=" << val << ",";

  if (version() <= 3) {
    val = input->readLong(2);
    if (val) f << "f6=" << val << ",";
    m_state->m_bot = 0x100;
    m_state->m_eot = input->readULong(2);
    m_state->m_textLength = m_state->m_eot-0x100;

    for (int i = 0; i < 6; i++) { // always 0?
      val = input->readLong(2);
      if (val) f << "h" << i << "=" << val << ",";
    }
    input->seek(headerSize, WPX_SEEK_SET);
    ascii().addPos(0);
    ascii().addNote(f.str().c_str());
    return true;
  }

  for (int i = 0; i < 6; i++) { // always 0 ?
    val = input->readLong(1);
    if (val) f << "g" << i << "=" << val << ",";
  }
  if (val) f << "g7=" << val << ","; // always 0 ?
  m_state->m_bot =  input->readULong(4);
  m_state->m_eot = input->readULong(4);

  if (m_state->m_bot > m_state->m_eot) {
    f << "#text:" << std::hex << m_state->m_bot << "<->" << m_state->m_eot << ",";
    if (0x100 <= m_state->m_eot) {
      MWAW_DEBUG_MSG(("MSWParser::checkHeader: problem with text position: reset begin to default\n"));
      m_state->m_bot = 0x100;
    } else {
      MWAW_DEBUG_MSG(("MSWParser::checkHeader: problem with text position: reset to empty\n"));
      m_state->m_bot = m_state->m_eot = 0x100;
    }
  }

  long endOfData = input->readULong(4);
  if (endOfData < 100) {
    MWAW_DEBUG_MSG(("MSWParser::checkHeader: end of file pos is too small\n"));
    return false;
  }

  long actPos = input->tell();
  input->seek(endOfData, WPX_SEEK_SET);
  if (long(input->tell()) != endOfData) {
    if (strict) return false;
    endOfData = input->tell();
    if (endOfData < m_state->m_eot) {
      MWAW_DEBUG_MSG(("MSWParser::checkHeader: file seems too short, break...\n"));
      return false;
    }
    MWAW_DEBUG_MSG(("MSWParser::checkHeader: file seems too short, continue...\n"));
  }
  m_state->m_eof = endOfData;
  ascii().addPos(endOfData);
  ascii().addNote("Entries(End)");
  input->seek(actPos, WPX_SEEK_SET);
  val = input->readLong(4); // always 0 ?
  if (val) f << "unkn2=" << val << ",";

  // seen to regroup main textZone + ?
  m_state->m_textLength= input->readULong(4);
  f << "textLength=" << std::hex << m_state->m_textLength << std::dec << ",";
  m_state->m_footnoteLength = input->readULong(4);
  if (m_state->m_footnoteLength)
    f << "footnoteLength=" << std::hex << m_state->m_footnoteLength << std::dec << ",";
  m_state->m_headerfooterLength  = input->readULong(4);
  if (m_state->m_headerfooterLength)
    f << "headerFooterLength=" << std::hex << m_state->m_headerfooterLength << std::dec << ",";

  for (int i = 0; i < 8; i++) { // always 0 ?
    val = input->readLong(2);
    if (val) f << "h" << i << "=" << val << ",";
  }
  // ok, we can finish initialization
  if (header) {
    header->setMajorVersion(m_state->m_version);
    header->setType(IMWAWDocument::MSWORD);
  }

  if (long(input->tell()) != headerSize) {
    ascii().addDelimiter(input->tell(), '|');
    input->seek(headerSize, WPX_SEEK_SET);
  }

  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  return true;
}

////////////////////////////////////////////////////////////
// try to read the first zone
////////////////////////////////////////////////////////////
namespace MSWParserInternal
{
struct Data {
  Data() : m_bool(false), m_values() {}
  bool isBool() const {
    return m_values.size() == 0;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Data const &data) {
    if (data.isBool()) {
      if (data.m_bool) o << "*";
      else o << "_";
    } else {
      o << "[";
      for (int i = 0; i < int(data.m_values.size()); i++)
        o << std::hex << (int(data.m_values[i])&0xFF) << std::dec << ",";
      o << "]";
    }
    return o;
  }
  bool read(TMWAWInputStreamPtr input, long endPos=-1) {
    m_values.resize(0);
    long pos = input->tell();
    int val = input->readULong(1);
    if (val == 0) {
      m_bool = true;
      return true;
    }
    if (val == 0xFF) {
      m_bool = false;
      return true;
    }
    if (endPos>0 && pos+1+val > endPos) {
      MWAW_DEBUG_MSG(("MMSWParserInternal::data: can not read the zone...\n"));
      input->seek(pos, WPX_SEEK_SET);
      return false;
    }
    m_values.resize(val);
    for (int i = 0; i < val; i++) m_values[i] = input->readULong(1);
    return true;
  }
  std::string getString() const {
    std::string res("");
    for (int i = 0; i < int(m_values.size()); i++) res += m_values[i];
    return res;
  }
  bool isString() const {
    if (m_values.size() == 0) return false;
    for (int i = 0; i < int(m_values.size()); i++) {
      if (m_values[i] == 0) return false;
    }
    return true;
  }
  bool m_bool;
  std::vector<char> m_values;
};
}

bool MSWParser::readStyles(MSWParserInternal::Entry &entry)
{
  if (entry.length() < 6) {
    MWAW_DEBUG_MSG(("MSWParser::readStyles: zone seems to short...\n"));
    return false;
  }
  TMWAWInputStreamPtr input = getInput();
  entry.setParsed(true);
  ascii().addPos(entry.end());
  ascii().addNote("_");

  long pos = entry.begin();
  libmwaw_tools::DebugStream f;
  input->seek(pos, WPX_SEEK_SET);
  f << entry << ":";
  int N = input->readLong(2);
  if (N) f << "N?=" << N;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos = input->tell();

  int dataSz = input->readULong(2);
  long endPos = pos+dataSz;
  if (dataSz < 2+N || endPos > entry.end()) {
    ascii().addPos(pos);
    ascii().addNote("###Styles(names)");
    MWAW_DEBUG_MSG(("MSWParser::readStyles: can not read styles(names)...\n"));
    return false;
  }

  ascii().addPos(pos);

  f.str("");
  f << "Styles(names):";
  int actN=0;

  MSWParserInternal::Data data;
  while (long(input->tell()) < endPos) {
    if (!data.read(input, endPos)) {
      MWAW_DEBUG_MSG(("MSWParser::readStyles: zone(names) seems to short...\n"));
      f << "#";
      ascii().addNote(f.str().c_str());
      break;
    }
    if (data.isBool()) f << data << ",";
    else {
      if (actN < N) f << "defN" << actN << "=" ;
      else f << "N" << actN-N << "=" ;
      f << data.getString() << ",";
    }
    actN++;
  }
  int N1=actN-N;
  if (N1 < 0) {
    MWAW_DEBUG_MSG(("MSWParser::readStyles: zone(names) seems to short: stop...\n"));
    f << "#";
    ascii().addNote(f.str().c_str());
    return false;
  }
  if (long(input->tell()) != endPos) {
    ascii().addDelimiter(input->tell(),'|');
    input->seek(endPos, WPX_SEEK_SET);
  }
  ascii().addNote(f.str().c_str());

  pos = input->tell();
  dataSz = input->readULong(2);
  endPos = pos+dataSz;
  if (dataSz < N+2 || endPos > entry.end()) {
    if (dataSz >= N+2 && endPos < entry.end()+30) {
      MWAW_DEBUG_MSG(("MSWParser::readStyles: must increase font zone...\n"));
      entry.setEnd(endPos+1);
    } else {
      ascii().addPos(pos);
      ascii().addNote("###Styles(font)");
      MWAW_DEBUG_MSG(("MSWParser::readStyles: can not read styles(font)...\n"));
      return false;
    }
  }
  f.str("");
  f << "Styles(font):";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i = 0; i < N+N1; i++) {
    pos = input->tell();
    if (pos >= endPos)
      break;

    f.str("");
    if (i < N)
      f << "Styles(FDef" << i << "):";
    else
      f << "Styles(F" << i-N << "):";

    MSWParserInternal::Font font;
    if (!readFont(font) || long(input->tell()) > endPos) {
      input->seek(pos, WPX_SEEK_SET);
      int sz = input->readULong(1);
      if (sz == 0xFF) f << "_";
      else if (pos+1+sz <= endPos) {
        f << "#";
        input->seek(pos+1+sz, WPX_SEEK_SET);
        MWAW_DEBUG_MSG(("MSWParser::readStyles: can not read a font, continue\n"));
      } else {
        f << "#";
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());

        MWAW_DEBUG_MSG(("MSWParser::readStyles: can not read a font, stop\n"));
        break;
      }
    } else
      f << "font=[" << m_convertissor->getFontDebugString(font.m_font) << font << "],";

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  input->seek(endPos, WPX_SEEK_SET);
  pos = input->tell();
  dataSz = input->readULong(2);
  endPos = pos+dataSz;

  ascii().addPos(pos);
  f.str("");
  f << "Styles(paragraph):";
  bool szOk = true;
  if (endPos > entry.end()) {
    // sometimes entry.end() seems a little to short :-~
    if (endPos > entry.end()+100) {
      ascii().addNote("###Styles(paragraph)");
      MWAW_DEBUG_MSG(("MSWParser::readStyles: can not read styles(paragraph)...\n"));
      return false;
    }
    szOk = false;
    MWAW_DEBUG_MSG(("MSWParser::readStyles: styles(paragraph) size seems incoherent...\n"));
    f << "#sz=" << dataSz << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  int val;
  for (int i = 0; i < N+N1; i++) {
    pos = input->tell();
    if (pos >= endPos)
      break;

    if (long(input->tell()) >= endPos)
      break;

    f.str("");
    if (i < N)
      f << "Styles(PDef" << i << "):";
    else
      f << "Styles(P" << i-N << "):";
    int sz = input->readULong(1);
    if (sz == 0xFF) f << "_";
    else if (sz < 7) {
      MWAW_DEBUG_MSG(("MSWParser::readStyles: zone(paragraph) seems to short...\n"));
      f << "#";
    } else {
      f << "id=" << input->readLong(1) << ",";
      for (int i = 0; i < 3; i++) { // 0, 0|c,0|1
        val = input->readLong(2);
        if (val) f << "f" << i << "=" << val << ",";
      }
      if (sz > 7) {
        MSWParserInternal::Paragraph para;
        if (readParagraph(para, sz-7)) {
#ifdef DEBUG_WITH_FILES
          para.print(f, m_convertissor);
#endif
        } else
          f << "#";
      }
    }
    if (sz != 0xFF)
      input->seek(pos+1+sz, WPX_SEEK_SET);

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  input->seek(endPos, WPX_SEEK_SET);

  pos = input->tell();
  int N2 = input->readULong(2);
  f.str("");
  f << "Styles(IV):";
  if (N2 != N+N1) {
    MWAW_DEBUG_MSG(("MSWParser::readStyles: read zone(IV): N seems odd...\n"));
    f << "#N=" << N2 << ",";
  }
  if (pos+(N2+1)*2 > entry.end()) {
    if (N2>40) {
      MWAW_DEBUG_MSG(("MSWParser::readStyles: can not read zone(IV)...\n"));
      ascii().addPos(pos);
      ascii().addNote("Styles(IV):#"); // big problem
    }
    f << "#";
  }
  for (int i = 0; i < N2; i++) {
    int v0 = input->readLong(1);
    int v1 = input->readULong(1);
    if (!v0 && !v1) f << "_,";
    else if (!v1)
      f << v0 << ",";
    else
      f << v0 << ":" << std::hex << v1 << std::dec << ",";
  }

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos = input->tell();
  if (pos != entry.end()) {
    ascii().addPos(pos);
    ascii().addNote("_");
  }

  return true;
}

////////////////////////////////////////////////////////////
// try to read an entry
////////////////////////////////////////////////////////////
MSWParserInternal::Entry MSWParser::readEntry(std::string type, int id)
{
  TMWAWInputStreamPtr input = getInput();
  MSWParserInternal::Entry entry;
  entry.setType(type);
  entry.m_id = id;
  long pos = input->tell();
  libmwaw_tools::DebugStream f;

  long debPos = input->readULong(4);
  long sz =  input->readULong(2);
  if (id >= 0) f << "Entries(" << type << ")[" << id << "]:";
  else f << "Entries(" << type << "):";
  if (sz == 0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return entry;
  }
  if (m_state->m_eof <= 0) {
    input->seek(debPos+sz, WPX_SEEK_SET);
    bool ok=long(input->tell()) == debPos+sz;
    input->seek(pos+6, WPX_SEEK_SET);
    if (!ok) {
      MWAW_DEBUG_MSG(("MSWParser::readEntry: problem reading entry: %s\n", type.c_str()));
      f << "#";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return entry;
    }
  } else if (debPos+sz > m_state->m_eof) {
    MWAW_DEBUG_MSG(("MSWParser::readEntry: problem reading entry: %s\n", type.c_str()));
    f << "#";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return entry;
  }
  entry.setBegin(debPos);
  entry.setLength(sz);
  m_state->m_entryMap.insert
  (std::multimap<std::string, MSWParserInternal::Entry>::value_type(type, entry));

  f << std::hex << debPos << "[" << sz << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  return entry;
}

////////////////////////////////////////////////////////////
// read the printer name
////////////////////////////////////////////////////////////
bool MSWParser::readFontNames(MSWParserInternal::Entry &entry)
{
  if (entry.length() < 2) {
    MWAW_DEBUG_MSG(("MSWParser::readFontNames: the zone seems to short\n"));
    return false;
  }

  TMWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  int N = input->readULong(2);
  if (N*5+2 > entry.length()) {
    MWAW_DEBUG_MSG(("MSWParser::readFontNames: the number of fonts seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  f << "FontNames:" << N;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    if (pos+5 > entry.end()) {
      input->seek(pos, WPX_SEEK_SET);
      MWAW_DEBUG_MSG(("MSWParser::readFontNames: the fonts %d seems bad\n", i));
      break;
    }
    f.str("");
    f << "FontNames-" << i << ":";
    int val = input->readLong(2);
    if (val) f << "f0=" << val << ",";
    int fId = input->readULong(2);
    f << "fId=" << fId << ",";
    int fSz = input->readULong(1);
    if (pos +5 > entry.end()) {
      input->seek(pos, WPX_SEEK_SET);
      MWAW_DEBUG_MSG(("MSWParser::readFontNames: the fonts name %d seems bad\n", i));
      break;
    }
    std::string name("");
    for (int j = 0; j < fSz; j++)
      name += char(input->readLong(1));
    if (name.length())
      m_convertissor->setFontCorrespondance(fId, name);
    f << name;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  pos = input->tell();
  if (pos != entry.end()) {
    ascii().addPos(pos);
    ascii().addNote("FontNames#");
  }

  return true;
}

////////////////////////////////////////////////////////////
// read the line info zone
////////////////////////////////////////////////////////////
bool MSWParser::readLineInfo(MSWParserInternal::Entry &entry)
{
  if (entry.length() < 4 || (entry.length()%10) != 4) {
    MWAW_DEBUG_MSG(("MSWParser::readLineInfo: the zone size seems odd\n"));
    return false;
  }
  TMWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  entry.setParsed(true);
  input->seek(pos, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "LineInfo:";
  int N=entry.length()/10;

  std::vector<long> textPositions;
  f << "[";
  for (int i = 0; i <= N; i++) {
    long textPos = input->readULong(4);
    textPositions.push_back(textPos);
    f << std::hex << textPos << std::dec << ",";
  }
  f << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  long val;
  for (int i = 0; i < N; i++) {
    MSWParserInternal::PLC plc(1);
    pos = input->tell();
    f.str("");
    f << "LineInfo-" << i << ":" << std::hex << textPositions[i] << std::dec << ",";
    int type = input->readULong(1); // 0, 20, 40, 60
    switch(type) {
    case 0:
      break; // line break
    case 0x20:
      f << "soft,";
      break;
    default:
      if (type&0xf0) f << "type?=" << (type>>4) << ",";
      if (type&0x0f) f << "#unkn=" << (type&0xf) << ",";
    }
    for (int j = 0; j < 2; j++) { // f0: -1 up to 9, f1: 0 to 3b
      val = input->readLong(1);
      f << "f"<< j << "=" << val << ",";
    }
    val = input->readULong(1); // always a multiple of 4?
    f << "fl=" << std::hex << val << std::dec << ",";

    int h = input->readLong(2); // 0 to 23a
    f << "heigth=" << h << ",";

    if (type) {
      std::stringstream s;
      s << std::hex << type << std::dec;
      plc.m_extra=s.str();
    }
    m_state->m_plcMap.insert(std::multimap<long,MSWParserInternal::PLC>::value_type
                             (textPositions[i],plc));
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;

}

////////////////////////////////////////////////////////////
// read the glossary
////////////////////////////////////////////////////////////
bool MSWParser::readGlossary(MSWParserInternal::Entry &entry)
{
  TMWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  entry.setParsed(true);
  input->seek(pos, WPX_SEEK_SET);

  long sz = input->readULong(2);
  if (entry.length() != sz) {
    MWAW_DEBUG_MSG(("MSWParser::readGlossary: the zone size seems odd\n"));
    return false;
  }
  libmwaw_tools::DebugStream f;
  f << "Glossary:";
  while (input->tell()<entry.end()) {
    /** note:
        version 4: string can be followed by a number (byte)
        version 5: string can be followed by 0x12 and a number (byte)
    */
    pos = input->tell();
    int fSz = input->readULong(1);
    if (pos+1+fSz > entry.end()) {
      MWAW_DEBUG_MSG(("MSWParser::readGlossary: can not read a string\n"));
      f << "#";
      ascii().addDelimiter(pos, '|');
      break;
    }
    std::string text("");
    for (int i = 0; i < fSz; i++) {
      char c = input->readULong(1);
      if (c==0) text+='#';
      else text+=c;
    }
    f << text << ",";
  }
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the zone 17
////////////////////////////////////////////////////////////
bool MSWParser::readZone17(MSWParserInternal::Entry &entry)
{
  if (entry.length() != 0x2a) {
    MWAW_DEBUG_MSG(("MSWParser::readZone17: the zone size seems odd\n"));
    return false;
  }
  TMWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  entry.setParsed(true);
  input->seek(pos, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "Zone17:";
  if (version() < 5) {
    f << "bdbox?=[";
    for (int i = 0; i < 4; i++)
      f << input->readLong(2) << ",";
    f << "],";
    f << "bdbox2?=[";
    for (int i = 0; i < 4; i++)
      f << input->readLong(2) << ",";
    f << "],";
  }

  /*
    f0=0, 80, 82, 84, b0, b4, c2, c4, f0, f2 : type and ?
    f1=0|1|8|34|88 */
  int val;
  for (int i = 0; i < 2; i++) {
    val = input->readULong(1);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  // 0 or 1, followed by 0
  for (int i = 2; i < 4; i++) {
    val = input->readLong(1);
    if (val) f << "f" << i << "=" << val << ",";
  }
  long ptr = input->readULong(4); // a text ptr ( often near to textLength )
  if (ptr > m_state->m_textLength) f << "#";
  f << "textPos=" << std::hex << ptr << std::dec << ",";
  val  = input->readULong(4); // almost always ptr
  if (val != ptr)
    f << "textPos1=" << std::hex << val << std::dec << ",";
  // a small int between 6 and b
  val = input->readLong(2);
  if (val) f << "f4=" << val << ",";

  for (int i = 5; i < 7; i++) { // 0,0 or 3,5 or 8000, 8000
    val = input->readULong(2);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  val  = input->readULong(4); // almost always ptr
  if (val != ptr)
    f << "textPos2=" << std::hex << val << std::dec << ",";
  /* g0=[0,1,5,c], g1=[0,1,3,4] */
  for (int i = 0; i < 2; i++) {
    val = input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  if (version() == 5) {
    f << "bdbox?=[";
    for (int i = 0; i < 4; i++)
      f << input->readLong(2) << ",";
    f << "],";
    f << "bdbox2?=[";
    for (int i = 0; i < 4; i++)
      f << input->readLong(2) << ",";
    f << "],";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}
////////////////////////////////////////////////////////////
// read the zone 18
////////////////////////////////////////////////////////////
bool MSWParser::readZone18(MSWParserInternal::Entry &entry)
{
  if (entry.length() < 19) {
    MWAW_DEBUG_MSG(("MSWParser::readZone18: the zone seems to short\n"));
    return false;
  }
  TMWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  int type = input->readLong(1);
  if (type != 1 && type != 2) {
    MWAW_DEBUG_MSG(("MSWParser::readZone18: find odd type %d\n", type));
    return false;
  }
  entry.setParsed(true);
  int num = 0;
  while (type == 1) {
    /* probably a paragraph definition. Fixme: create a function */
    int length = input->readULong(2);
    long endPos = pos+3+length;
    if (endPos> entry.end()) {
      ascii().addPos(pos);
      ascii().addNote("Zone18[paragraph]#");
      MWAW_DEBUG_MSG(("MSWParser::readZone18: zone(paragraph) is too big\n"));
      return false;
    }
    f.str("");
    f << "Zone18:P" << num++<< "]:";
    MSWParserInternal::Paragraph para;
    input->seek(-2,WPX_SEEK_CUR);
    if (readParagraph(para) && long(input->tell()) <= endPos) {
#ifdef DEBUG_WITH_FILES
      para.print(f, m_convertissor);
#endif
    } else
      f << "#";

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(endPos, WPX_SEEK_SET);

    pos = input->tell();
    type = input->readULong(1);
    if (type == 2) break;
    if (type != 1) {
      MWAW_DEBUG_MSG(("MSWParser::readZone18: find odd type %d\n", type));
      return false;
    }
  }

  f.str("");
  f << "Entries(Corresp):";
  int sz = input->readULong(2);
  long endPos = pos+3+sz;
  if (endPos > entry.end() || (sz%12) != 4) {
    f << "#";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    MWAW_DEBUG_MSG(("MSWParser::readZone18: can not read the correspondance zone\n"));
    return false;
  }
  int N=sz/12;
  std::vector<long> textPos; // checkme
  textPos.resize(N+1);
  f << "pos=[" << std::hex;
  for (int i = 0; i <= N; i++) {
    textPos[i] = input->readULong(4);
    if (i && textPos[i] <= textPos[i-1]) {
      MWAW_DEBUG_MSG(("MSWParser::readZone18: find backward text pos\n"));
      f << "#" << textPos[i] << ",";
      textPos[i]=textPos[i-1];
    } else
      f << textPos[i] << ",";
  }
  f << std::dec << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = input->tell();
    MSWParserInternal::TextEntry tEntry;
    f.str("");
    f<< "Corresp-" << i << ":";
    tEntry.m_pos = textPos[i];
    tEntry.m_values[0] = input->readULong(2);
    long ptr = input->readULong(4);
    tEntry.setBegin(ptr);
    tEntry.setLength(textPos[i+1]-textPos[i]);
    tEntry.m_values[1]  = input->readULong(2); // always 0?
    if (tEntry.valid()) {
      m_state->m_textposEntryMap[textPos[i]] = tEntry;
      m_state->m_plcMap.insert(std::multimap<long,MSWParserInternal::PLC>::value_type
                               (textPos[i],MSWParserInternal::PLC(0)));
    }
    f << tEntry;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  pos = input->tell();
  if (pos != entry.end()) {
    ascii().addPos(pos);
    ascii().addNote("Corresp#");
  }
  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the printer name
////////////////////////////////////////////////////////////
bool MSWParser::readPrinter(MSWParserInternal::Entry &entry)
{
  if (entry.length() < 2) {
    MWAW_DEBUG_MSG(("MSWParser::readPrinter: the zone seems to short\n"));
    return false;
  }

  TMWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "Printer:";
  int sz = input->readULong(2);
  if (sz > entry.length()) {
    MWAW_DEBUG_MSG(("MSWParser::readPrinter: the zone seems to short\n"));
    return false;
  }
  int strSz = input->readULong(1);
  if (strSz+2> sz) {
    MWAW_DEBUG_MSG(("MSWParser::readPrinter: name seems to big\n"));
    return false;
  }
  std::string name("");
  for (int i = 0; i < strSz; i++)
    name+=char(input->readLong(1));
  f << name << ",";
  int i= 0;
  while (long(input->tell())+2 <= entry.end()) { // almost always a,0,0
    int val = input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
    i++;
  }
  if (long(input->tell()) != entry.end())
    ascii().addDelimiter(input->tell(), '|');

  entry.setParsed(true);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the document summary
////////////////////////////////////////////////////////////
bool MSWParser::readDocSum(MSWParserInternal::Entry &entry)
{
  if (entry.length() < 8) {
    MWAW_DEBUG_MSG(("MSWParser::readDocSum: the zone seems to short\n"));
    return false;
  }

  TMWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "DocSum:";
  int sz = input->readULong(2);
  if (sz > entry.length()) {
    MWAW_DEBUG_MSG(("MSWParser::readDocSum: the zone seems to short\n"));
    return false;
  }
  entry.setParsed(true);

  if (sz != entry.length()) f << "#";
  MSWParserInternal::Data data;
  char const *(what[]) = { "title", "subject","author","version","keyword",
                           "author", "#unknown", "#unknown2"
                         };
  for (int i = 0; i < 8; i++) {
    long actPos = input->tell();
    if (actPos == entry.end()) break;
    if (!data.read(input, entry.end())) {
      // try with a slighter size
      input->seek(actPos, WPX_SEEK_SET);
      if (data.read(input, entry.end()+32) && (data.isBool() || data.isString())) {
        MWAW_DEBUG_MSG(("MSWParser::readDocSum: string %d to long, try to continue...\n", i));
        entry.setEnd(input->tell());
      } else {
        MWAW_DEBUG_MSG(("MSWParser::readDocSum: string %d to short...\n", i));
        f << "#";
        input->seek(actPos, WPX_SEEK_SET);
        break;
      }
    }
    if (data.isBool()) continue;
    f << what[i] << "=" <<  data.getString() << ",";
  }

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  if (long(input->tell()) != entry.end())
    ascii().addDelimiter(input->tell(), '|');

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read  a list of ints zone
////////////////////////////////////////////////////////////
bool MSWParser::readIntsZone(MSWParserInternal::Entry &entry, int sz, std::vector<int> &list)
{
  list.resize(0);
  if (entry.length() < sz || (entry.length()%sz)) {
    MWAW_DEBUG_MSG(("MSWParser::readIntsZone: the size of zone %s seems to odd\n", entry.type().c_str()));
    return false;
  }

  TMWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << entry.type() << ":";
  int N = entry.length()/sz;
  for (int i = 0; i < N; i++) {
    int val = input->readLong(sz);
    list.push_back(val);
    f << std::hex << val << std::dec << ",";
  }

  if (long(input->tell()) != entry.end())
    ascii().addDelimiter(input->tell(), '|');

  entry.setParsed(true);

  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read  a list of strings zone
////////////////////////////////////////////////////////////
bool MSWParser::readStringsZone(MSWParserInternal::Entry &entry, std::vector<std::string> &list)
{
  list.resize(0);
  if (entry.length() < 2) {
    MWAW_DEBUG_MSG(("MSWParser::readStringsZone: the zone seems to short\n"));
    return false;
  }

  TMWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << entry;
  int sz = input->readULong(2);
  if (sz > entry.length()) {
    MWAW_DEBUG_MSG(("MSWParser::readStringsZone: the zone seems to short\n"));
    return false;
  }
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());

  int id = 0;
  while (long(input->tell()) != entry.end()) {
    pos = input->tell();
    int strSz = input->readULong(1);
    if (pos+strSz+1> entry.end()) {
      MWAW_DEBUG_MSG(("MSWParser::readStringsZone: a string seems to big\n"));
      f << "#";
      break;
    }
    std::string name("");
    for (int i = 0; i < strSz; i++)
      name+=char(input->readLong(1));
    list.push_back(name);
    f.str("");
    f << entry << "id" << id++ << "," << name << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  if (long(input->tell()) != entry.end()) {
    ascii().addPos(input->tell());
    f.str("");
    f << entry << "#";
    ascii().addNote(f.str().c_str());
  }

  entry.setParsed(true);

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the objects
////////////////////////////////////////////////////////////
bool MSWParser::readObjects()
{
  TMWAWInputStreamPtr input = getInput();

  std::multimap<std::string, MSWParserInternal::Entry>::iterator it;

  it = m_state->m_entryMap.find("ObjectList");
  while (it != m_state->m_entryMap.end()) {
    if (!it->second.hasType("ObjectList")) break;
    MSWParserInternal::Entry &entry=it++->second;
    readObjectList(entry);
  }

  it = m_state->m_entryMap.find("ObjectFlags");
  while (it != m_state->m_entryMap.end()) {
    if (!it->second.hasType("ObjectFlags")) break;
    MSWParserInternal::Entry &entry=it++->second;
    readObjectFlags(entry);
  }

  it = m_state->m_entryMap.find("ObjectName");
  while (it != m_state->m_entryMap.end()) {
    if (!it->second.hasType("ObjectName")) break;
    MSWParserInternal::Entry &entry=it++->second;
    std::vector<std::string> list;
    readStringsZone(entry, list);

    if (entry.m_id < 0 || entry.m_id > 1) {
      MWAW_DEBUG_MSG(("MSWParser::readObjects: unexpected entry id: %d\n", entry.m_id));
      continue;
    }
    std::vector<MSWParserInternal::Object> &listObject = m_state->m_objectList[entry.m_id];
    int numObjects = listObject.size();
    if (int(list.size()) != numObjects) {
      MWAW_DEBUG_MSG(("MSWParser::readObjects: unexpected number of name\n"));
      if (int(list.size()) < numObjects) numObjects = list.size();
    }
    for (int i = 0; i < numObjects; i++)
      listObject[i].m_name = list[i];
  }

  for (int st = 0; st < 2; st++) {
    std::vector<MSWParserInternal::Object> &listObject = m_state->m_objectList[st];

    for (int i = 0; i < int(listObject.size()); i++)
      readObject(listObject[i]);
  }
  return true;
}

bool MSWParser::readObjectList(MSWParserInternal::Entry &entry)
{
  if (entry.m_id < 0 || entry.m_id > 1) {
    MWAW_DEBUG_MSG(("MSWParser::readObjectList: unexpected entry id: %d\n", entry.m_id));
    return false;
  }
  std::vector<MSWParserInternal::Object> &listObject = m_state->m_objectList[entry.m_id];
  listObject.resize(0);
  if (entry.length() < 4 || (entry.length()%18) != 4) {
    MWAW_DEBUG_MSG(("MSWParser::readObjectList: the zone size seems odd\n"));
    return false;
  }
  TMWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  entry.setParsed(true);
  input->seek(pos, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "ObjectList[" << entry.m_id << "]:";
  int N=entry.length()/18;
  std::vector<long> textPos; // checkme
  textPos.resize(N+1);
  f << "[";
  for (int i = 0; i < N+1; i++) {
    textPos[i] = input->readULong(4);
    f << std::hex << textPos[i] << std::dec << ",";
  }
  f << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  int val;

  for (int i = 0; i < N; i++) {
    MSWParserInternal::Object object;
    object.m_textPos = textPos[i];
    pos = input->tell();
    f.str("");
    object.m_id = input->readLong(2);
    for (int st = 0; st < 2; st++) {
      object.m_ids[st] = input->readLong(2); // small number -1, 0, 2, 3, 4
      object.m_idsFlag[st] = input->readULong(1); // 0, 48 .
    }

    object.m_pos.setBegin(input->readULong(4));
    val = input->readLong(2); // always 0 ?
    if (val) f << "#f1=" << val << ",";
    object.m_extra = f.str();
    f.str("");
    f << "ObjectList-" << i << ":" << object;
    if (object.m_pos.begin() >= m_state->m_eof) {
      MWAW_DEBUG_MSG(("MSWParser::readObjectList: pb with ptr\n"));
      f << "#ptr=" << std::hex << object.m_pos.begin() << std::dec << ",";
      object.m_pos.setBegin(0);
    }

    listObject.push_back(object);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;

}

bool MSWParser::readObjectFlags(MSWParserInternal::Entry &entry)
{
  if (entry.m_id < 0 || entry.m_id > 1) {
    MWAW_DEBUG_MSG(("MSWParser::readObjectFlags: unexpected entry id: %d\n", entry.m_id));
    return false;
  }
  std::vector<MSWParserInternal::Object> &listObject = m_state->m_objectList[entry.m_id];
  int numObject = listObject.size();
  if (entry.length() < 4 || (entry.length()%6) != 4) {
    MWAW_DEBUG_MSG(("MSWParser::readObjectFlags: the zone size seems odd\n"));
    return false;
  }
  TMWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  entry.setParsed(true);
  input->seek(pos, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "ObjectFlags[" << entry.m_id << "]:";
  int N=entry.length()/6;
  if (N != numObject) {
    MWAW_DEBUG_MSG(("MSWParser::readObjectFlags: unexpected number of object\n"));
  }

  f << "[";
  for (int i = 0; i < N+1; i++) {
    long textPos = input->readULong(4);
    if (i < numObject && textPos != listObject[i].m_textPos && textPos != listObject[i].m_textPos+1)
      f << "#";
    f << std::hex << textPos << std::dec << ",";
  }
  f << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = input->tell();
    int fl[2];
    for (int st = 0; st < 2; st++) fl[st] = input->readULong(1);
    f.str("");
    f << "ObjectFlags-" << i << ":";
    if (i < numObject) {
      for (int st = 0; st < 2; st++) listObject[i].m_flags[st] = fl[st];
      f << "Obj" << listObject[i].m_id << ",";
    }
    if (fl[0] != 0x48) f << "fl0="  << std::hex << fl[0] << std::dec << ",";
    if (fl[1]) f << "fl1="  << std::hex << fl[1] << std::dec << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;

}

bool MSWParser::readObject(MSWParserInternal::Object &obj)
{
  TMWAWInputStreamPtr input = getInput();
  libmwaw_tools::DebugStream f;

  long pos = obj.m_pos.begin();
  if (!pos) return false;

  input->seek(pos, WPX_SEEK_SET);
  int sz = input->readULong(4);

  f << "Entries(ObjectData):Obj" << obj.m_id << ",";
  if (pos+sz >= m_state->m_eof || sz < 6) {
    MWAW_DEBUG_MSG(("MSWParser::readObjects: pb finding object data sz\n"));
    f << "#";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  int fSz = input->readULong(2);
  if (sz < 2 || fSz+4 > sz) {
    MWAW_DEBUG_MSG(("MSWParser::readObjects: pb reading the name\n"));
    f << "#";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  obj.m_pos.setLength(sz);
  MSWParserInternal::Entry fileEntry = obj.getEntry();
  fileEntry.setParsed(true);
  m_state->m_entryMap.insert
  (std::multimap<std::string, MSWParserInternal::Entry>::value_type
   (fileEntry.type(), fileEntry));

  long endPos = pos+4+fSz;
  std::string name(""); // first equation, second "" or Equation Word?
  while(long(input->tell()) != endPos) {
    int c = input->readULong(1);
    if (c == 0) {
      if (name.length()) f << name << ",";
      name = "";
      continue;
    }
    name += char(c);
  }
  if (name.length()) f << name << ",";

  pos = input->tell();
  endPos = obj.m_pos.end();
  int val;
  if (pos+2 <= endPos) {
    val = input->readLong(2);
    if (val) f << "#unkn=" << val <<",";
  }
  // Equation Word? : often contains not other data
  long dataSz = 0;
  if (pos+9 <= endPos) {
    for (int i = 0; i < 3; i++) { // always 0
      val = input->readLong(1);
      if (val) f << "f" << i << "=" << val <<",";
    }
    dataSz = input->readULong(4);
  }
  pos = input->tell();
  if (dataSz && pos+dataSz != endPos) f << "#";
  ascii().addPos(obj.m_pos.begin());
  ascii().addNote(f.str().c_str());
  if (long(input->tell()) != obj.m_pos.end())
    ascii().addDelimiter(input->tell(), '|');

  ascii().addPos(obj.m_pos.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// try to read a font
////////////////////////////////////////////////////////////
bool MSWParser::readFont(MSWParserInternal::Font &font)
{
  font = MSWParserInternal::Font();

  TMWAWInputStreamPtr input = getInput();
  libmwaw_tools::DebugStream f;

  long pos = input->tell();
  int sz = input->readULong(1);
  if ((sz > 10 && sz != 14) || sz == 3) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  if (sz == 0) return true;
  font.m_default = false;

  // [00|04|08|18|40|80|c0], 0
  for (int i = 0; i < 2; i++) {
    if (sz >= i+1)
      font.m_flags[i] = input->readULong(1);
  }
  if (sz >= 4)
    font.m_font.setId(input->readULong(2));
  if (sz >= 5)
    font.m_font.setSize(input->readULong(1)/2);
  else
    font.m_font.setSize(0);
  if (sz >= 6)
    font.m_flags[2] = input->readULong(1); // 0, ee, fc, fa
  if (sz >= 8)
    font.m_value = input->readLong(2); // 0 or 2
  if (sz >= 10)
    font.m_size=input->readULong(2)/2;
  if (sz >= 14) {
    f.str("");
    int val = input->readLong(2); // 0
    if (val) f << "f0=" << val << ",";
    val  = input->readULong(2)/2;
    if (val != font.m_size) f << "#size3=" << val << ",";
    if (f.str().length()) font.m_extra = f.str();
  }
  return true;
}

////////////////////////////////////////////////////////////
// try to read a paragraph
////////////////////////////////////////////////////////////
bool MSWParser::readParagraph(MSWParserInternal::Paragraph &para, int dataSz)
{
  para = MSWParserInternal::Paragraph();

  TMWAWInputStreamPtr input = getInput();
  int sz;
  if (dataSz >= 0)
    sz = dataSz;
  else
    sz = input->readULong(2);

  long pos = input->tell();
  long endPos = pos+sz;

  if (sz == 0) return true;
  if (pos+sz > m_state->m_eof) return false;

  libmwaw_tools::DebugStream f;
  while (long(input->tell()) < endPos) {
    long actPos = input->tell();
    int wh = input->readULong(1), val;
    bool done = true;
    switch(wh) {
    case 0x3a:
      f << "f" << std::hex << wh << std::dec << ",";
      break;
    case 0x2: // a small number between 0 and 4
    case 0x5: // a small number between 0 or 1
    case 0x8: // a small number : always 1 ?
    case 0x18: // a small number between 0 or 1
    case 0x1d: // a small number 1, 6
    case 0x34: // 0 ( one time)
    case 0x45: // a small number between 0 or 1
    case 0x47: // 0 one time
    case 0x49: // 0 ( one time)
    case 0x4c: // 0, 6, -12
    case 0x4d: // a small number between -4 and 14
    case 0x5e: // 0
    case 0x80: // a small number 8 or a
      if (actPos+2 > endPos) {
        done = false;
        f << "#";
        break;
      }
      val = input->readLong(1);
      f << "f" << std::hex << wh << std::dec << "=" << val << ",";
      break;
    case 0x3c: // always 0x80 | 0x81 ?
    case 0x3d: // always 0x80 | 0x81 ?
    case 0x3e:
    case 0x3f:
    case 0x40:
    case 0x41:
    case 0x42:
    case 0x43:
    case 0x4a: // in general a small number
      if (actPos+2 > endPos) {
        done = false;
        f << "#";
        break;
      }
      val = input->readULong(1);
      f << "f" << std::hex << wh << "=" << val << std::dec << ",";
      break;
    case 0x10: // a number which is often negative
    case 0x11: // a dim a file pos ?
    case 0x13: // a number which can be negative
    case 0x14: // always a multiple of 10?
    case 0x15: // always a multiple of 10?
    case 0x16: // always a multiple of 10?
    case 0x1a: // a small negative number
    case 0x1b: // a small negative number
    case 0x1c: // a dim ?
    case 0x22: // alway 0 ?
    case 0x23: // alway 0 ?
    case 0x24: // always b4 ?
    case 0x77: // 0 or 1
    case 0x78: // 008e
    case 0x92: // alway 0 ?
    case 0x93: // a dim ?
    case 0x94: // always followed by 50 ?
    case 0x99: // alway 0 ?
      if (actPos+3 > endPos) {
        done = false;
        f << "#";
        break;
      }
      val = input->readLong(2);
      f << "f" << std::hex << wh << std::dec << "=" << val << ",";
      break;
    case 0x9f: // two small number
    case 0x1e:
    case 0x1f:
    case 0x20:
    case 0x21: // two flag 4040?
    case 0x44: // two flag?
      if (actPos+3 > endPos) {
        done = false;
        f << "#";
        break;
      }
      f << "f" << std::hex << wh << "=[";
      for (int i = 0; i < 2; i++)
        f << input->readULong(1) << ",";
      f << std::dec << "],";
      break;
    case 3: // four small number
      if (actPos+5 > endPos) {
        done = false;
        f << "#";
        break;
      }
      f << "f" << std::hex << wh << std::dec << "=[";
      for (int i = 0; i < 4; i++)
        f << input->readLong(1) << ",";
      f << "],";
      break;
    case 0x50: // two small number
      if (actPos+4 > endPos) {
        done = false;
        f << "#";
        break;
      }
      f << "f" << std::hex << wh << std::dec << "=[";
      f << input->readLong(1) << ",";
      f << input->readLong(2) << ",";
      f << "],";
      break;
    case 0x38:
    case 0x4f: // a small int and a pos?
      if (actPos+4 > endPos) {
        done = false;
        f << "#";
        break;
      }
      f << "f" << std::hex << wh << std::dec << "=[";
      f << input->readLong(1) << ",";
      f << std::hex << input->readULong(2) << std::dec << "],";
      break;
    case 0x9e:
    case 0xa0: // two small int and a pos?
      if (actPos+5 > endPos) {
        done = false;
        f << "#";
        break;
      }
      f << "f" << std::hex << wh << std::dec << "=[";
      for (int i = 0; i < 2; i++)
        f << input->readLong(1) << ",";
      f << std::hex << input->readULong(2) << std::dec << "],";
      break;
    case 0x9d: // three small int and a pos?
    case 0xa3: // three small int and a pos?
      if (actPos+6 > endPos) {
        done = false;
        f << "#";
        break;
      }
      f << "f" << std::hex << wh << std::dec << "=[";
      for (int i = 0; i < 3; i++)
        f << input->readLong(1) << ",";
      f << std::hex << input->readULong(2) << std::dec << "],";
      break;
    case 0x4e:
    case 0x53: { // same as 4e but with size=0xa
      MSWParserInternal::Font font;
      if (!readFont(font) || long(input->tell()) > endPos) {
        done = false;
        f << "#";
        break;
      }
      if (wh == 0x4e) para.m_font = font;
      else para.m_font2 = font;
      break;
    }
    case 0x5f: { // 4 index
      if (actPos+10 > endPos) {
        done = false;
        f << "#";
        break;
      }
      int sz = input->readULong(1);
      if (sz != 8) f << "#sz=" << sz << ",";
      f << "f5f=[";
      for (int i = 0; i < 4; i++) f << input->readLong(2) << ",";
      f << "],";
      break;
    }
    case 0xf: // variable field, find with sz=4,6,9,e,11,15,21
    case 0x17: { // variable field : find with sz=5,9,c,d,11 :-~
      int sz = input->readULong(1);
      if (!sz || actPos+2+sz > endPos) {
        done = false;
        f << "#";
        break;
      }
      f << "f" << std::hex << wh << "=[";
      for (int i = 0; i < sz; i++) {
        val= input->readULong(1);
        if (val) f << val << ",";
        else f << "_,";
      }
      f << std::dec << "],";
      break;
    }
    default:
      done = false;
      break;
    }
    if (!done) {
      input->seek(actPos, WPX_SEEK_SET);
      break;
    }
  }

  if (long(input->tell()) != endPos) {
    static bool first = true;
    if (first) {
      MWAW_DEBUG_MSG(("MSWParser::readParagraph: can not read end of paragraph\n"));
      first = false;
    }
    ascii().addDelimiter(input->tell(),'|');
    f << "#";
    input->seek(endPos, WPX_SEEK_SET);
  }
  para.m_extra = f.str();

  return true;
}

////////////////////////////////////////////////////////////
// read the text zone 0/1 (checkme)
////////////////////////////////////////////////////////////
bool MSWParser::readSection(MSWParserInternal::Entry &entry)
{
  if (entry.length() < 14 || (entry.length()%10) != 4) {
    MWAW_DEBUG_MSG(("MSWParser::readSection: the zone size seems odd\n"));
    return false;
  }
  TMWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  entry.setParsed(true);
  input->seek(pos, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "Section:";
  int N=entry.length()/10;
  std::vector<long> textPos; // checkme
  textPos.resize(N+1);
  for (int i = 0; i < N+1; i++) textPos[i] = input->readULong(4);
  for (int i = 0; i < N; i++) {
    int fl = input->readULong(2);
    long filePos = input->readULong(4);
    m_state->m_plcMap.insert(std::multimap<long,MSWParserInternal::PLC>::value_type
                             (textPos[i+1],MSWParserInternal::PLC(2)));
    f << std::hex << "pos?=" << textPos[i] << "-" << textPos[i+1];
    if (textPos[i+1] > m_state->getTotalTextSize()+1)
      f << "#[" << textPos[i+1] - m_state->getTotalTextSize() << "]";
    if (filePos == 0xFFFFFFFFL) // can be 0xFFFFFFFFL
      f << ", def=_";
    else
      f << ", def=" << filePos;
    f << "[" << fl << "]," << std::dec;
    MSWParserInternal::Entry ent;
    if (filePos != 0xFFFFFFFFL) {
      ent.setBegin(filePos);
      ent.setType("SectData");
      ent.m_id = i;
      m_state->m_entryMap.insert
      (std::multimap<std::string, MSWParserInternal::Entry>::value_type(ent.type(), ent));
    }
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

bool MSWParser::readSectionData(MSWParserInternal::Entry &entry)
{
  TMWAWInputStreamPtr input = getInput();
  input->seek(entry.begin(), WPX_SEEK_SET);
  int sz = input->readULong(1);
  if (sz < 1 || sz >= 255) return false;
  entry.setLength(sz+1);
  entry.setParsed(true);

  libmwaw_tools::DebugStream f;
  f << "Entries(SectData)-" << entry.m_id << ":";

  int val;
  while (input->tell() < entry.end()) {
    long pos = input->tell();
    int c = input->readULong(1);
    bool done = true;
    switch(c) { // code commun with readParagraph ?
    case 0x75: // always 0 ?
    case 0x76: // always 1 ?
    case 0x79: // find one time with 1 (related to column ?)
    case 0x7d: // always 1
    case 0x7e: // always 0
    case 0x80: // small number between 2 and 11
      if (pos+2 > entry.end()) {
        done = false;
        break;
      }
      f << "f" << std::hex << c << std::dec << "=" << input->readLong(1) << ",";
      break;
    case 0x82: // find one time with 168 (related to 7e ?)
    case 0x84: // 0, 2f1, 3f2, 3f3, 435 ( related to 83 ?)
      if (pos+3 > entry.end()) {
        done = false;
        break;
      }
      val = input->readLong(2);
      f << "f" << std::hex << c << std::dec << "=" << val << ",";
      break;
    case 0x77: // num column
      if (pos+3 > entry.end()) {
        done = false;
        break;
      }
      val = input->readLong(2);
      if (val) f << "numColumns=" << val+1 << ",";
      break;
    case 0x78: // height? 11c or 2c5
      if (pos+3 > entry.end()) {
        done = false;
        break;
      }
      val = input->readLong(2);
      if (val) f << "height?=" << val << ",";
      break;
    case 0x83: // a dim? total height? 0188 or 0435
      if (pos+3 > entry.end()) {
        done = false;
        break;
      }
      val = input->readLong(2);
      if (val) f << "dim?=" << val << ",";
      break;
    case 0x7b: // 2e1 3d08
    case 0x7c: // 4da, 6a5, 15ff
      if (pos+3 > entry.end()) {
        done = false;
        break;
      }
      f << "f" << std::hex << c << std::dec << "=";
      f << std::hex << input->readULong(1) << std::dec << ":";
      f << std::hex << input->readULong(1) << std::dec << ",";
      break;
    default:
      done = false;
      break;
    }
    if (!done) {
      f << "#";
      ascii().addDelimiter(pos,'|');
      break;
    }
  }
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the text zone 1 (checkme)
////////////////////////////////////////////////////////////
bool MSWParser::readPageBreak(MSWParserInternal::Entry &entry)
{
  if (entry.length() < 18 || (entry.length()%14) != 4) {
    MWAW_DEBUG_MSG(("MSWParser::readPageBreak: the zone size seems odd\n"));
    return false;
  }
  TMWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  entry.setParsed(true);
  input->seek(pos, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "PageBreak:";
  int val = input->readULong(4);
  if (val) f << "unkn=" << val << ",";
  int N=entry.length()/14;
  std::vector<long> textPos; // checkme
  textPos.resize(N);
  for (int i = 0; i < N; i++) textPos[i] = input->readULong(4);
  for (int i = 0; i < N; i++) {
    m_state->m_plcMap.insert(std::multimap<long,MSWParserInternal::PLC>::value_type
                             (textPos[i],MSWParserInternal::PLC(3)));
    int fl = input->readULong(2); // break type?
    f << std::hex << "[pos?=" << textPos[i];
    if (textPos[i] > m_state->getTotalTextSize()+1)
      f << "#[" << std::dec << textPos[i] - m_state->getTotalTextSize() << std::hex << "]";
    f << ",fl=" << fl << std::dec << ",";
    for (int j = 0; j < 2; j++) { // always -1, 0
      val = input->readLong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }
    f << "page=" <<  input->readLong(2) << ",";
    val = input->readLong(2);
    if (val != -1) f << "f2=" << val << ",";
    f << "],";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the text zone 2 (checkme)
////////////////////////////////////////////////////////////
bool MSWParser::readTextData2(MSWParserInternal::Entry &entry)
{
  if (entry.length() < 10 || (entry.length()%6) != 4) {
    MWAW_DEBUG_MSG(("MSWParser::readTextData2: the zone size seems odd\n"));
    return false;
  }
  TMWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  entry.setParsed(true);
  input->seek(pos, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "TextData2-" << entry.m_id << ":";
  int N=entry.length()/6;
  std::vector<long> textPos; // checkme
  textPos.resize(N+1);
  for (int i = 0; i <= N; i++) textPos[i] = input->readULong(4);
  for (int i = 0; i < N; i++) {
    if (textPos[i] >= m_state->m_eof) f << "#";

    m_state->m_filePlcMap.insert(std::multimap<long,MSWParserInternal::PLC>::value_type
                                 (textPos[i],MSWParserInternal::PLC(4)));
    int id = input->readLong(2);
    // FIXME: this point to a file position
    f << std::hex << "[filePos?=" << textPos[i] << ",id=" << id << std::dec << ",";
    f << "],";
  }
  f << std::hex << "end?=" << textPos[N] << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read a picture
////////////////////////////////////////////////////////////
bool MSWParser::readPicture(MSWParserInternal::Entry &entry)
{
  if (entry.length() < 30 ) {
    MWAW_DEBUG_MSG(("MSWParser::readPicture: the zone seems too short\n"));
    return false;
  }
  TMWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  entry.setParsed(true);
  input->seek(pos, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "Entries(Picture)[" << entry.m_id << "]:";
  long sz = input->readULong(4);
  if (sz > entry.length()) {
    MWAW_DEBUG_MSG(("MSWParser::readPicture: the zone size seems too big\n"));
    return false;
  }
  int N = input->readULong(1);
  f << "N=" << N << ",";
  int val = input->readULong(1); // find 0 or 0x80
  if (val) f << "fl=" << std::hex << val << std::dec << ",";
  int dim[4];
  for (int i = 0; i < 4; i++)
    dim[i] = input->readLong(2);
  f << "dim=[" << dim[1] << "x" << dim[0] << "," << dim[3] << "x" << dim[2] << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int n=0; n < N; n++) {
    pos = input->tell();
    f.str("");
    f << "Picture-" << n << "[" << entry.m_id << "]:";
    sz = input->readULong(4);
    if (sz < 16 || sz+pos > entry.end()) {
      MWAW_DEBUG_MSG(("MSWParser::readPicture: pb with the picture size\n"));
      f << "#";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return false;
    }
    val = input->readULong(1); // always 8?
    if (val) f << "type?=" << val << ",";
    val = input->readLong(1); // always 0 ?
    if (val) f << "unkn=" << val << ",";
    val = input->readLong(2); // almost always 1 : two time 0?
    if (val) f << "id?=" << val << ",";
    for (int i = 0; i < 4; i++)
      dim[i] = input->readLong(2);
    f << "dim=[" << dim[1] << "x" << dim[0] << "," << dim[3] << "x" << dim[2] << "],";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
#ifdef DEBUG_WITH_FILES
    if (sz > 16) {
      ascii().skipZone(pos+16, pos+sz-1);
      WPXBinaryData file;
      input->seek(pos+16, WPX_SEEK_SET);
      input->readDataBlock(sz-16, file);
      static int volatile pictName = 0;
      libmwaw_tools::DebugStream f;
      f << "PICT-" << ++pictName << ".pct";
      libmwaw_tools::Debug::dumpFile(file, f.str().c_str());
    }
#endif
    input->seek(pos+sz, WPX_SEEK_SET);
  }

  pos = input->tell();
  if (pos != entry.end())
    ascii().addDelimiter(pos, '|');
  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// try to read a text zone
////////////////////////////////////////////////////////////
bool MSWParser::readText(MSWParserInternal::TextEntry &entry)
{
  if (!entry.valid())
    return false;
  TMWAWInputStreamPtr input = getInput();
  input->seek(entry.begin(), WPX_SEEK_SET);
  long endPos = entry.end();
  MSWParserInternal::Font actFont;
  libmwaw_tools::DebugStream f;
  f << "Entries(TextContent)[";
  for (int i = 0; i < 2; i++) {
    if (entry.m_values[i])
      f << std::hex << entry.m_values[i] << std::dec << ",";
    else
      f << "_,";
  }
  f << "]:";
  long textPos = entry.m_pos;
  std::multimap<long, MSWParserInternal::PLC>::iterator plcIt;
  while (!input->atEOS() && long(input->tell()) < endPos) {
    plcIt = m_state->m_plcMap.find(textPos);
    while (plcIt != m_state->m_plcMap.end() && plcIt->first == textPos) {
      MSWParserInternal::PLC &plc = plcIt++->second;
      if (plc.m_type <= 1) continue;
#ifdef DEBUG_PLC
      std::stringstream s;
      s << plc;
      if (m_listener) m_listener->insertUnicodeString(s.str().c_str());
#endif
    }
    textPos++;

    long pos = input->tell();
    plcIt = m_state->m_filePlcMap.find(pos);
    while (plcIt != m_state->m_filePlcMap.end() && plcIt->first == pos) {
      MSWParserInternal::PLC &plc = plcIt++->second;
      if (plc.m_type <= 1) continue;
#ifdef DEBUG_PLC
      std::stringstream s;
      s << plc;
      if (m_listener) m_listener->insertUnicodeString(s.str().c_str());
#endif
    }
    int c = input->readULong(1);
    switch (c) {
    case 0x9:
      if (m_listener) m_listener->insertTab();
      break;
    case 0xd:
      if (m_listener) m_listener->insertEOL();
      break;
    default: {
      if (!m_listener) break;
      int unicode = m_convertissor->getUnicode (actFont.m_font, c);
      if (unicode == -1) {
        static bool first = true;
        if (c < 32) {
          if (first) {
            MWAW_DEBUG_MSG(("MSWParser::readText: Find odd char %x\n", int(c)));
            first = false;
          }
          f << "#";
        } else
          m_listener->insertCharacter(c);
      } else
        m_listener->insertUnicode(unicode);
      break;
    }
    }
    if (c)
      f << char(c);
    else
      f << "###";
  }
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// search for picture
////////////////////////////////////////////////////////////
void MSWParser::searchPictures()
{
  TMWAWInputStreamPtr input = getInput();
#if 1
  std::multimap<std::string, MSWParserInternal::Entry>::iterator it;
  it = m_state->m_entryMap.find("SectData");
  long lastPos = -1;
  while (it != m_state->m_entryMap.end()) {
    if (!it->second.hasType("SectData")) break;
    if (it->second.end() > lastPos) lastPos = it->second.end();
    it++;
  }
  input->seek(lastPos, WPX_SEEK_SET);
  long pos;
#else
  long pos = m_state->m_eot;
  // go to the next 0x100 block
  pos = ((pos>>8)+1)*0x100;
  input->seek(pos, WPX_SEEK_SET);
#endif

  int id = 0;
  while(1) {
    pos = input->tell();
    // align to 4
    pos = 4*((pos+3)/4);
    input->seek(pos, WPX_SEEK_SET);

    long sz = input->readULong(4);
    long endPos = pos+sz;
    if (sz < 14 || sz+pos > m_state->m_eof) return;
    int num = input->readLong(1);
    if (num <= 0 || num > 4) break;
    input->seek(pos+14, WPX_SEEK_SET);
    for (int n = 0; n < num; n++) {
      long actPos = input->tell();
      long pSz = input->readULong(4);
      if (pSz+actPos > endPos) break;
      input->seek(pSz+actPos, WPX_SEEK_SET);
    }
    if (input->tell() != endPos)
      break;
    MSWParserInternal::Entry entry;
    entry.setBegin(pos);
    entry.setEnd(endPos);
    entry.setType("Picture");
    entry.m_id = id++;
    m_state->m_entryMap.insert
    (std::multimap<std::string, MSWParserInternal::Entry>::value_type(entry.type(), entry));
  }
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool MSWParser::readPrintInfo(MSWParserInternal::Entry &entry)
{
  if (entry.length() < 0x78) {
    MWAW_DEBUG_MSG(("MSWParser::readPrintInfo: the zone seems to short\n"));
    return false;
  }
  TMWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  entry.setParsed(true);
  input->seek(pos, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  // print info
  libmwaw_tools_mac::PrintInfo info;
  if (!info.read(input)) return false;
  f << "PrintInfo:"<< info;

  Vec2i paperSize = info.paper().size();
  Vec2i pageSize = info.page().size();
  if (pageSize.x() <= 0 || pageSize.y() <= 0 ||
      paperSize.x() <= 0 || paperSize.y() <= 0) return false;

  // define margin from print info
  Vec2i lTopMargin= -1 * info.paper().pos(0);
  Vec2i rBotMargin=info.paper().size() - info.page().size();

  // move margin left | top
  int decalX = lTopMargin.x() > 14 ? lTopMargin.x()-14 : 0;
  int decalY = lTopMargin.y() > 14 ? lTopMargin.y()-14 : 0;
  lTopMargin -= Vec2i(decalX, decalY);
  rBotMargin += Vec2i(decalX, decalY);

  // decrease right | bottom
  int rightMarg = rBotMargin.x() -50;
  if (rightMarg < 0) rightMarg=0;
  int botMarg = rBotMargin.y() -50;
  if (botMarg < 0) botMarg=0;

  m_pageSpan.setMarginTop(lTopMargin.y()/72.0);
  m_pageSpan.setMarginBottom(botMarg/72.0);
  m_pageSpan.setMarginLeft(lTopMargin.x()/72.0);
  m_pageSpan.setMarginRight(rightMarg/72.0);
  m_pageSpan.setFormLength(paperSize.y()/72.);
  m_pageSpan.setFormWidth(paperSize.x()/72.);

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  if (long(input->tell()) != entry.end())
    ascii().addDelimiter(input->tell(), '|');

  ascii().addPos(entry.end());
  ascii().addNote("_");

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
