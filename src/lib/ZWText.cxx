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
#include "MWAWSubDocument.hxx"

#include "ZWParser.hxx"

#include "ZWText.hxx"

/** Internal: the structures of a ZWText */
namespace ZWTextInternal
{
////////////////////////////////////////
//! Internal: struct used to store the font of a ZWText
struct Font {
  //! constructor
  Font() : m_font(), m_height(0), m_extra("") {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Font const &font);

  //! the font
  MWAWFont m_font;
  //! the line height
  int m_height;
  //! extra data
  std::string m_extra;
};

std::ostream &operator<<(std::ostream &o, Font const &font)
{
  if (font.m_height)
    o << "h=" << font.m_height << ",";
  o << font.m_extra;
  return o;
}

////////////////////////////////////////
//! Internal: struct used to store a header/footer of a ZWText
struct HFZone {
  //! constructor
  HFZone() : m_pos(), m_font(), m_extra("") {
    m_font.m_font = MWAWFont(3,12);
  }
  //! returns true if the zone is not empty
  bool ok() const {
    return m_pos.valid();
  }
  //! operator<<
  std::string getDebugString(MWAWFontConverterPtr &convert) const {
    std::stringstream s;
    if (!m_pos.valid()) return s.str();
    if (convert)
      s << m_font.m_font.getDebugString(convert) << m_font << ",";
    else
      s << m_font << ",";
    s << m_extra;
    return s.str();
  }
  //! the text position
  MWAWEntry m_pos;
  //! the font
  Font m_font;
  //! extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: struct used to store a section of a ZWText
struct Section {
  //! constructor
  Section() : m_id(), m_pos(), m_name(""), m_idFontMap(), m_parsed(false) {
  }
  //! the section id
  int m_id;
  //! the text position
  MWAWEntry m_pos;
  //! the section name
  std::string m_name;
  //! a map pos -> font
  std::map<long,Font> m_idFontMap;
  //! true if the section is parsed
  mutable bool m_parsed;
};

////////////////////////////////////////
//! Internal: the state of a ZWText
struct State {
  //! constructor
  State() : m_version(-1), m_numPages(-1), m_actualPage(1), m_idSectionMap(), m_header(), m_footer() {
  }
  //! return a section for an id ( if it does not exists, create id )
  Section &getSection(int id) {
    std::map<int,Section>::iterator it = m_idSectionMap.find(id);
    if (it != m_idSectionMap.end())
      return it->second;
    it = m_idSectionMap.insert
         (std::map<int,Section>::value_type(id,Section())).first;
    it->second.m_id = id;
    return it->second;
  }

  //! the file version
  mutable int m_version;
  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;

  //! a map id -> section
  std::map<int, Section> m_idSectionMap;
  //! the header zone
  HFZone m_header;
  //! the footer zone
  HFZone m_footer;
};

////////////////////////////////////////
//! Internal: the subdocument of a ZWText
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(ZWText &pars, MWAWInputStreamPtr input, int id, MWAWEntry entry, ZWText::TextCode type) :
    MWAWSubDocument(pars.m_mainParser, input, MWAWEntry()), m_textParser(&pars), m_id(id), m_type(type), m_pos(entry) {}

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
  ZWText *m_textParser;
  //! the section id
  int m_id;
  //! the type of document
  ZWText::TextCode m_type;
  //! the file pos
  MWAWEntry m_pos;
private:
  SubDocument(SubDocument const &orig);
  SubDocument &operator=(SubDocument const &orig);
};

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_textParser != sDoc->m_textParser) return true;
  if (m_id != sDoc->m_id) return true;
  if (m_type != sDoc->m_type) return true;
  if (m_pos != sDoc->m_pos) return true;
  return false;
}

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("SubDocument::parse: no listener\n"));
    return;
  }
  ZWContentListener *listen = dynamic_cast<ZWContentListener *>(listener.get());
  if (!listen) {
    MWAW_DEBUG_MSG(("SubDocument::parse: bad listener\n"));
    return;
  }

  assert(m_textParser);

  long pos = m_input->tell();
  if (m_type==ZWText::Link)
    listen->insertUnicodeString("link to ");
  else if (m_type==ZWText::Tag)
    listen->insertUnicodeString("ref: ");
  m_textParser->sendText(m_id, m_pos);
  m_input->seek(pos, WPX_SEEK_SET);
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
ZWText::ZWText(MWAWInputStreamPtr ip, ZWParser &parser, MWAWFontConverterPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert),
  m_state(new ZWTextInternal::State), m_mainParser(&parser), m_asciiFile(parser.ascii())
{
}

ZWText::~ZWText()
{ }

int ZWText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_mainParser->version();
  return m_state->m_version;
}

int ZWText::numPages() const
{
  if (m_state->m_numPages >= 0)
    return m_state->m_numPages;
  const_cast<ZWText *>(this)->computePositions();
  return m_state->m_numPages;
}

bool ZWText::hasHeaderFooter(bool header) const
{
  if (header) return m_state->m_header.ok();
  return m_state->m_footer.ok();
}

void ZWText::computePositions()
{
  m_state->m_actualPage = 1;

  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  int nPages = 0;
  std::map<int,ZWTextInternal::Section>::iterator it =
    m_state->m_idSectionMap.begin();
  while (it!=m_state->m_idSectionMap.end()) {
    nPages++;
    ZWTextInternal::Section &section=(it++)->second;
    if (!section.m_pos.valid())
      continue;
    long endPos = section.m_pos.end();
    input->seek(section.m_pos.begin(), WPX_SEEK_SET);
    while(!input->atEOS()) {
      if (input->tell()+3 >= endPos)
        break;
      if ((char) input->readLong(1)!='<')
        continue;
      if ((char) input->readLong(1)!='N')
        continue;
      if ((char) input->readLong(1)!='>')
        continue;
      nPages++;
    }
  }
  m_state->m_numPages = nPages;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

// find the different zones
bool ZWText::createZones()
{
  MWAWRSRCParserPtr rsrcParser = m_mainParser->getRSRCParser();
  if (!rsrcParser) {
    MWAW_DEBUG_MSG(("ZWText::createZones: can not find the entry map\n"));
    return false;
  }
  std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::iterator it;
  // the 128 zones
  char const *(zNames[]) = {"HEAD", "FOOT", "STLS"};
  for (int z = 0; z < 3; z++) {
    it = entryMap.lower_bound(zNames[z]);
    while (it != entryMap.end()) {
      if (it->first != zNames[z])
        break;
      MWAWEntry const &entry = it++->second;
      switch(z) {
      case 0:
      case 1:
        readHFZone(entry);
        break;
      case 2: // 128 and following
        readStyles(entry);
        break;
      default:
        break;
      }
    }
  }

  // 1001 and following
  char const *(sNames[]) = {"styl","TEXT"};
  for (int z = 0; z < 2; z++) {
    it = entryMap.lower_bound(sNames[z]);
    while (it != entryMap.end()) {
      if (it->first != sNames[z])
        break;
      MWAWEntry const &entry = it++->second;
      switch(z) {
      case 0:
        readSectionFonts(entry);
        break;
      case 1: {
        ZWTextInternal::Section &sec=m_state->getSection(entry.id());
        sec.m_pos = entry;
        break;
      }
      default:
        break;
      }
    }
  }

  // now update the different data
  computePositions();
  return true;
}

ZWText::TextCode ZWText::isTextCode
(MWAWInputStreamPtr &input, long endPos, MWAWEntry &dPos)
{
  dPos=MWAWEntry();
  long pos = input->tell();
  if (pos+2 > endPos)
    return None;
  char c=(char) input->readLong(1);
  if (c=='C' || c=='N') {
    if (char(input->readLong(1))!='>') {
      input->seek(pos, WPX_SEEK_SET);
      return None;
    }
    return c=='C' ? Center : NewPage;
  }
  std::string expectedString("");
  ZWText::TextCode res = None;
  switch(c) {
  case 'b':
    expectedString="bookmark";
    res = BookMark;
    break;
  case 'i':
    expectedString="insert";
    res = Tag;
    break;
  case 'l':
    expectedString="link";
    res = Link;
    break;
  default:
    input->seek(pos, WPX_SEEK_SET);
    return None;
  }
  expectedString += ' ';
  for (size_t s=1; s < expectedString.size(); s++) {
    if (input->atEOS() || input->tell()  >= endPos ||
        (char) input->readLong(1) != expectedString[s]) {
      input->seek(pos, WPX_SEEK_SET);
      return None;
    }
  }
  dPos.setBegin(input->tell());
  while(1) {
    if (input->atEOS() || input->tell() >= endPos) {
      input->seek(pos, WPX_SEEK_SET);
      return None;
    }
    c=(char)input->readLong(1);
    if (c==0 || c==0xa || c==0xd) {
      input->seek(pos, WPX_SEEK_SET);
      return None;
    }
    if (c=='>') {
      dPos.setEnd(input->tell()-1);
      return res;
    }
  }
  return None;
}

bool ZWText::sendText(ZWTextInternal::Section const &zone, MWAWEntry const &entry)
{
  if (!m_listener) {
    MWAW_DEBUG_MSG(("ZWText::sendText: can not find a listener\n"));
    return false;
  }
  bool main = entry.begin()==zone.m_pos.begin();
  if (main)
    m_mainParser->newPage(m_state->m_actualPage++);
  if (!entry.valid())
    return true;
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  libmwaw::DebugStream f;
  f << "Entries(TextContent)[" << zone.m_name << "]:";
  zone.m_parsed=true;
  long pos = entry.begin(), endPos = entry.end();
  input->seek(pos, WPX_SEEK_SET);

  ZWTextInternal::Font actFont;
  actFont.m_font=MWAWFont(3,12);
  std::multimap<long, ZWTextInternal::Font>::const_iterator fIt=
    zone.m_idFontMap.begin();
  long cPos = pos-zone.m_pos.begin();
  while (fIt != zone.m_idFontMap.end() && fIt->first<cPos)
    actFont = fIt++->second;
  setProperty(actFont.m_font);
  int fId=0;
  bool isCenter = false;
  while (1) {
    long actPos = input->tell();
    bool done = input->atEOS() || actPos==endPos;

    char c = done ? (char) 0 : (char) input->readULong(1);
    if (c==0xd || done) {
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      f.str("");
      f << "TextContent:";
      pos = actPos+1;
    }
    if (done) break;
    while (fIt != zone.m_idFontMap.end() && fIt->first<=cPos) {
      actFont = fIt++->second;
      setProperty(actFont.m_font);
      f << "[F" << fId++ << "]";
    }

    cPos++;
    TextCode textCode;
    MWAWEntry textData;
    if (c=='<' && (textCode=isTextCode(input, endPos, textData))!=None) {
      long newPos = input->tell();
      done = true;
      switch (textCode) {
      case Center:
        isCenter=true;
        m_listener->setParagraphJustification(libmwaw::JustificationCenter);
        break;
      case NewPage:
        if (main)
          m_mainParser->newPage(m_state->m_actualPage++);
        break;
      case Link:
      case Tag:
      case BookMark: {
        if (textCode==Link) {
          MWAW_DEBUG_MSG(("ZWText::sendText: find a link, uses bookmark\n"));
        } else if (textCode==Tag) {
          MWAW_DEBUG_MSG(("ZWText::sendText: find a tag, uses bookmark\n"));
        }
        MWAWSubDocumentPtr subdoc(new ZWTextInternal::SubDocument(*this, input, zone.m_id, textData, textCode));
        m_listener->insertComment(subdoc);
        break;
      }
      case None:
      default:
        break;
      }
      if (done) {
        input->seek(newPos, WPX_SEEK_SET);
        cPos=newPos-zone.m_pos.begin();
        continue;
      }
      input->seek(actPos+1, WPX_SEEK_SET);
    }
    switch(c) {
    case 0x9:
      m_listener->insertTab();
      break;
    case 0xd:
      m_listener->insertEOL();
      if (isCenter) {
        isCenter=false;
        m_listener->setParagraphJustification(libmwaw::JustificationLeft);
      }
      break;
    default: {
      int unicode = m_convertissor->unicode (actFont.m_font.id(), (unsigned char) c);
      if (unicode == -1) {
        if (c >= 0 && c < 0x20) {
          MWAW_DEBUG_MSG(("ZWText::sendText: Find odd char %x\n", int(c)));
          f << "#";
        } else
          m_listener->insertCharacter((uint8_t) c);
      } else
        m_listener->insertUnicode((uint32_t) unicode);

    }
    }
    f << c;
  }
  return true;
}

bool ZWText::sendText(int sectionId, MWAWEntry const &entry)
{
  if (!m_listener) {
    MWAW_DEBUG_MSG(("ZWText::sendText: can not find a listener\n"));
    return false;
  }
  std::map<int,ZWTextInternal::Section>::iterator it =
    m_state->m_idSectionMap.find(sectionId);
  if (it==m_state->m_idSectionMap.end()) {
    MWAW_DEBUG_MSG(("ZWText::sendText: can not find the section\n"));
    return false;
  }
  sendText(it->second, entry);
  return true;
}

bool ZWText::sendMainText()
{
  if (!m_listener) {
    MWAW_DEBUG_MSG(("ZWText::sendMainText: can not find a listener\n"));
    return false;
  }
  std::map<int,ZWTextInternal::Section>::iterator it =
    m_state->m_idSectionMap.begin();
  while (it!=m_state->m_idSectionMap.end()) {
    ZWTextInternal::Section &section=(it++)->second;
    sendText(section, section.m_pos);
  }
  return true;
}

//////////////////////////////////////////////
// Fonts
//////////////////////////////////////////////
void ZWText::setProperty(MWAWFont const &font)
{
  if (!m_listener) return;
  MWAWFont aFont;
  font.sendTo(m_listener.get(), m_convertissor, aFont);
}

bool ZWText::readSectionFonts(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() < 2) {
    MWAW_DEBUG_MSG(("ZWText::readSectionFonts: the entry is bad\n"));
    return false;
  }

  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  libmwaw::DebugStream f;
  f << "Entries(" << entry.type() << ")[" << entry << "]:";
  entry.setParsed(true);
  ZWTextInternal::Section &section = m_state->getSection(entry.id());
  section.m_name = entry.name();

  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);
  int N=(int) input->readLong(2);
  f << "N=" << N << ",";
  if (2+N*20 != int(entry.length())) {
    MWAW_DEBUG_MSG(("ZWText::readSectionFonts: the number N seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = input->tell();
    ZWTextInternal::Font font;
    f.str("");
    long cPos=(long) input->readULong(4);
    font.m_height = (int) input->readLong(2);
    int sz = (int) input->readLong(2);
    font.m_font.setId((int) input->readLong(2));
    int flag = (int) input->readULong(1);
    uint32_t flags = 0;
    if (flag&0x1) flags |= MWAWFont::boldBit;
    if (flag&0x2) flags |= MWAWFont::italicBit;
    if (flag&0x4) font.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (flag&0xF8) f << "fl0=" << std::hex << (flag&0xF8) << std::dec << ",";

    flag = (int) input->readULong(1); // alway 0
    if (flag) f << "#fl1=" << std::hex << flag << std::dec << ",";
    font.m_font.setSize((int) input->readLong(2));
    if (sz != font.m_font.size())
      f << "#sz=" << sz << ",";
    unsigned char col[3];
    for (int j=0; j < 3; j++)
      col[j] = (unsigned char) (input->readULong(2)>>8);
    if (col[0] || col[1] || col[2])
      font.m_font.setColor(MWAWColor(col[0],col[1],col[2]));
    font.m_font.setFlags(flags);
    font.m_extra = f.str();
    section.m_idFontMap.insert(std::map<long, ZWTextInternal::Font>::value_type(cPos, font));

    f.str("");
    f << entry.type() << "-F" << i << ":cPos=" << std::hex << cPos << std::dec << ","
      << font.m_font.getDebugString(m_convertissor) << font;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+20, WPX_SEEK_SET);
  }
  return true;
}

//////////////////////////////////////////////
// Styles
//////////////////////////////////////////////
bool ZWText::readStyles(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("ZWText::readStyles: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  libmwaw::DebugStream f;
  f << "Entries(" << entry.type() << ")[" << entry << "]:";
  entry.setParsed(true);

  std::vector<ZWField> fields;
  if (!m_mainParser->getFieldList(entry, fields)) {
    MWAW_DEBUG_MSG(("ZWText::readStyles: can not get fields list\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return false;
  }

  size_t numFields = fields.size();
  if (numFields < 9) {
    MWAW_DEBUG_MSG(("ZWText::readStyles: the fields list seems very short\n"));
  }
  std::string strVal;
  int intVal;
  bool boolVal;
  std::vector<int> intList;
  for (size_t ff = 0; ff < numFields; ff++) {
    ZWField const &field = fields[ff];
    bool done = false;
    unsigned char color[3];
    switch(ff) {
    case 0:
      done = field.getString(input, strVal);
      if (!done||!strVal.length())
        break;
      f << "font=" << strVal << ",";
      break;
    case 1:
      done = field.getInt(input, intVal);
      if (!done||!intVal)
        break;
      f << "fSz=" << intVal << ",";
      break;
    case 2:
    case 3:
    case 4: {
      color[0]=color[1]=color[2]=0;
      done = field.getInt(input, intVal);
      if (!done)
        break;
      color[ff-2]=(unsigned char) intVal;
      while (ff < 4) {
        fields[++ff].getInt(input, intVal);
        color[ff-2]=(unsigned char) intVal;
      }
      if (color[0]||color[1]||color[2])
        f << "col=" << MWAWColor(color[0],color[1],color[2]) << ",";
      break;
    }
    case 5:
    case 6: // italic?
    case 7: // always false ?
      done = field.getBool(input, boolVal);
      if (!done)
        break;
      if (boolVal)
        f << "f" << ff << "Set,";
      break;
    case 8: // 0|1|2
      done = field.getInt(input, intVal);
      if (!done||!intVal)
        break;
      f << "id?=" << intVal << ",";
      break;
    default:
      break;
    }
    if (done)
      continue;
    if (fields[ff].getDebugString(input, strVal))
      f << "#f" << ff << "=\"" << strVal << "\",";
    else
      f << "#f" << ff << ",";
  }

  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read the header/footer zone
////////////////////////////////////////////////////////////
bool ZWText::sendHeaderFooter(bool header)
{
  if (!m_listener) {
    MWAW_DEBUG_MSG(("ZWText::sendHeaderFooter: can not find a listener\n"));
    return false;
  }
  ZWTextInternal::HFZone const &zone = header ?
                                       m_state->m_header : m_state->m_footer;
  if (!zone.ok()) {
    MWAW_DEBUG_MSG(("ZWText::sendHeaderFooter: zone is not valid\n"));
    return false;
  }
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  input->seek(zone.m_pos.begin(), WPX_SEEK_SET);
  MWAWFont const &actFont=zone.m_font.m_font;
  setProperty(actFont);
  long endPos = zone.m_pos.end();
  while(!input->atEOS()) {
    long actPos = input->tell();
    if (actPos >= endPos)
      break;
    char c = (char) input->readULong(1);
    switch(c) {
    case 0xa:
      m_listener->insertTab();
      break;
    case 0xd:
      m_listener->insertEOL();
      break;
    case '#':
      if (actPos+1 < endPos) {
        char nextC = (char) input->readULong(1);
        bool done = true;
        switch(nextC) {
        case 'd':
          m_listener->insertField(MWAWContentListener::Date);
          break;
        case 'p':
          m_listener->insertField(MWAWContentListener::PageNumber);
          break;
        case 's':
          m_listener->insertUnicodeString("#section#");
          break;
        case 't':
          m_listener->insertField(MWAWContentListener::Time);
          break;
        case '#':
          m_listener->insertField(MWAWContentListener::PageCount);
          break;
        default:
          done=false;
          break;
        }
        if (done)
          break;
      }
      input->seek(actPos+1, WPX_SEEK_SET);
    default: {
      int unicode = m_convertissor->unicode (actFont.id(), (unsigned char) c);
      if (unicode == -1) {
        if (c >= 0 && c < 0x20) {
          MWAW_DEBUG_MSG(("ZWText::sendHeaderFooter: Find odd char %x\n", int(c)));
        } else
          m_listener->insertCharacter((uint8_t) c);
      } else
        m_listener->insertUnicode((uint32_t) unicode);
    }
    }
  }
  return true;
}

bool ZWText::readHFZone(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("ZWText::readHFZone: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=128) {
    MWAW_DEBUG_MSG(("ZWText::readHFZone: the entry id is odd\n"));
  }
  long pos = entry.begin();
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  libmwaw::DebugStream f;
  entry.setParsed(true);

  std::vector<ZWField> fields;
  if (!m_mainParser->getFieldList(entry, fields)) {
    MWAW_DEBUG_MSG(("ZWText::readHFZone: can not get fields list\n"));
    f << "Entries(" << entry.type() << ")[" << entry << "]:";
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return false;
  }

  size_t numFields = fields.size();
  if (numFields < 9) {
    MWAW_DEBUG_MSG(("ZWText::readHFZone: the fields list seems very short\n"));
  }
  std::string strVal;
  int intVal;
  bool boolVal;
  std::vector<int> intList;
  ZWTextInternal::HFZone &zone =
    entry.type()=="HEAD" ? m_state->m_header : m_state->m_footer;
  ZWTextInternal::Font &font = zone.m_font;
  uint32_t flags=0;
  for (size_t ff = 0; ff < numFields; ff++) {
    ZWField const &field = fields[ff];
    bool done = false;
    switch(ff) {
    case 0:
    case 2:
    case 5:
    case 7:
      done = field.getBool(input, boolVal);
      if (!done)
        break;
      if (!boolVal)
        continue;
      switch(ff) {
      case 0:
        flags |= MWAWFont::boldBit;
        break;
      case 2:
        flags |= MWAWFont::italicBit;
        break;
      case 5:
        font.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
        break;
      case 7:
        f << "addDelimiter,";
        break;
      default:
        f << "f" << ff << "Set,";
        break;
      }
      break;
    case 4:
    case 8:
      done = field.getInt(input, intVal);
      if (!done||!intVal)
        break;
      if (ff==4)
        font.m_font.setSize(intVal);
      else
        f << "delimiterSize=" << intVal << ",";
      break;
    case 3:
      done = field.getString(input, strVal);
      if (!done||!strVal.length())
        break;
      font.m_font.setId(m_convertissor->getId(strVal));
      break;
    case 6:
      done = field.getDebugString(input, strVal);
      if (!done||!strVal.length())
        break;
      zone.m_pos = field.m_pos;
      f << "text=\"" << strVal << "\",";
      break;
    case 1: {
      done = field.getIntList(input, intList);
      if (!done|| intList.size() != 3)
        break;
      uint32_t col = uint32_t((intList[0]<<16)|(intList[1]<<8)|intList[2]);
      if (col)
        font.m_font.setColor(MWAWColor(col));
      break;
    }
    default:
      break;
    }
    if (done)
      continue;
    if (fields[ff].getDebugString(input, strVal))
      f << "#f" << ff << "=\"" << strVal << "\",";
    else
      f << "#f" << ff << ",";
  }
  font.m_font.setFlags(flags);

  zone.m_extra=f.str();
  f.str("");
  f << "Entries(" << entry.type() << ")[" << entry << "]:"
    << zone.getDebugString(m_convertissor);

  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

void ZWText::flushExtra()
{
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
