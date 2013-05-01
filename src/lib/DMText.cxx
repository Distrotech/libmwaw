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

#include "DMParser.hxx"

#include "DMText.hxx"

/** Internal: the structures of a DMText */
namespace DMTextInternal
{
////////////////////////////////////////
//! Internal: structure to store a table of contents of a DMText
struct TOC {
  //! constructor
  TOC() : m_cIdList(), m_textList() {
  }
  //! returns true if the table is empty
  bool empty() const {
    return m_textList.size()==0;
  }
  //! the toc chapter id
  std::vector<int> m_cIdList;
  //! the toc texts
  std::vector<std::string> m_textList;
};

////////////////////////////////////////
//! Internal: structure to store a footer data of a DMText
struct Footer {
  //! constructor
  Footer() : m_font(3,9), m_chapterResetPage(false), m_userInfo(), m_extra("") {
    for (int i = 0; i < 6; i++) m_items[i]=0;
  }
  //! returns true if the footer is empty
  bool empty() const {
    for (int i = 0; i < 6; i++)
      if (m_items[i])
        return false;
    return true;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Footer const &f) {
    static char const *where[]= {"TL", "TC", "TR", "BL", "BC", "BR" };
    static char const *what[]= { "nothing", "unkn1", "unkn2", "time", "date",
                                 "page", "fileName", "chapName", "userText"
                               };
    if (f.m_chapterResetPage)
      o << "pageReset[chapter],";
    for (int i = 0; i < 6; i++) {
      if (!f.m_items[i]) continue;
      o << where[i] << "=";
      if (f.m_items[i] > 0 && f.m_items[i] <= 8)
        o << what[f.m_items[i]] << ",";
      else
        o << "#unkn" << f.m_items[i] << ",";
    }
    o << f.m_extra;
    return o;
  }
  //! the font
  MWAWFont m_font;
  //! true if a chapter reset page
  bool m_chapterResetPage;
  //! the item values
  int m_items[6];
  //! the user information entry
  std::string m_userInfo;
  //! extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: structure to store a the data of a DMText Zone
struct Zone {
  //! constructor
  Zone() : m_pos(), m_justify(MWAWParagraph::JustificationLeft), m_backgroundColor(MWAWColor::white()),
    m_useFooter(true), m_name(""), m_posFontMap(), m_numPages(0), m_parsed(false) {
    for (int i = 0; i < 4; i++)
      m_margins[i]=54;
  }
  //! the text entry
  MWAWEntry m_pos;
  //! the paragraph justification
  MWAWParagraph::Justification m_justify;
  //! the background color
  MWAWColor m_backgroundColor;
  //! print or ignore the footer
  bool m_useFooter;
  //! the margins L,T,R,B in points
  int m_margins[4];
  //! the name
  std::string m_name;
  //! the map of id -> font
  std::map<long, MWAWFont > m_posFontMap;
  //! the number of page
  mutable int m_numPages;
  //! a flag to know if we have send the data to the listener
  mutable bool m_parsed;
};
////////////////////////////////////////
//! Internal: the state of a DMText
struct State {
  //! constructor
  State() : m_version(-1), m_numPages(-1), m_actualPage(0), m_pageWidth(8.5), m_idZoneMap(), m_footer(), m_toc() {
  }
  //! returns the zone corresponding to an id
  Zone &getZone(int id) {
    if (m_idZoneMap.find(id)==m_idZoneMap.end())
      m_idZoneMap[id]=Zone();
    return m_idZoneMap.find(id)->second;
  }

  //! the file version
  mutable int m_version;

  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;
  //! the page width ( not really define so default value 8.5)
  double m_pageWidth;
  //! the map of id -> text zone
  std::map<int, Zone > m_idZoneMap;
  //! the footer
  Footer m_footer;
  //! the table of content
  TOC m_toc;
};

////////////////////////////////////////
//! Internal: the subdocument of a DMText
class SubDocument : public MWAWSubDocument
{
public:
  //! constructor for a footer zone
  SubDocument(DMText &pars, MWAWInputStreamPtr input, int id, libmwaw::SubDocumentType type) :
    MWAWSubDocument(pars.m_mainParser, input, MWAWEntry()), m_textParser(&pars), m_id(id), m_text(""), m_type(type) {}

  //! constructor for a comment zone
  SubDocument(DMText &pars, MWAWInputStreamPtr input, std::string const &text, libmwaw::SubDocumentType type) :
    MWAWSubDocument(pars.m_mainParser, input, MWAWEntry()), m_textParser(&pars), m_id(-1), m_text(text), m_type(type) {}

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
  DMText *m_textParser;
  //! the subdocument id
  int m_id;
  //! the string text
  std::string m_text;
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
  assert(m_textParser);

  long pos = m_input->tell();
  if (m_type == libmwaw::DOC_HEADER_FOOTER)
    m_textParser->sendFooter(m_id);
  else if (m_type == libmwaw::DOC_COMMENT_ANNOTATION) {
    listener->setFont(MWAWFont(3,10));
    m_textParser->sendString(m_text);
  } else {
    MWAW_DEBUG_MSG(("SubDocument::parse: oops do not know how to send this kind of document\n"));
  }
  m_input->seek(pos, WPX_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_textParser != sDoc->m_textParser) return true;
  if (m_id != sDoc->m_id) return true;
  if (m_text != sDoc->m_text) return true;
  if (m_type != sDoc->m_type) return true;
  return false;
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
DMText::DMText(DMParser &parser) :
  m_parserState(parser.getParserState()), m_state(new DMTextInternal::State), m_mainParser(&parser)
{
}

DMText::~DMText()
{ }

int DMText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}

int DMText::numPages() const
{
  if (m_state->m_numPages >= 0)
    return m_state->m_numPages;
  m_state->m_actualPage = 1;

  int nPages = 0;
  std::map<int, DMTextInternal::Zone >::const_iterator it =
    m_state->m_idZoneMap.begin();
  for ( ; it != m_state->m_idZoneMap.end(); it++) {
    DMTextInternal::Zone const &zone= it->second;
    computeNumPages(zone);
    nPages += zone.m_numPages;
  }
  m_state->m_numPages = nPages;
  return m_state->m_numPages;
}

int DMText::numChapters() const
{
  return int(m_state->m_idZoneMap.size());
}

void DMText::sendComment(std::string const &str)
{
  if (!m_parserState->m_listener) {
    MWAW_DEBUG_MSG(("DMText::sendComment: called without listener\n"));
    return;
  }
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  shared_ptr<MWAWSubDocument> comment
  (new DMTextInternal::SubDocument(*this, input, str, libmwaw::DOC_COMMENT_ANNOTATION));
  m_parserState->m_listener->insertComment(comment);
}

////////////////////////////////////////////////////////////
// pages/...
////////////////////////////////////////////////////////////
void DMText::computeNumPages(DMTextInternal::Zone const &zone) const
{
  if (zone.m_numPages || !zone.m_pos.valid())
    return;
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  input->seek(zone.m_pos.begin(), WPX_SEEK_SET);
  long len = zone.m_pos.length();
  int nPages=1;
  for (long i=0; i < len; i++) {
    if (input->atEOS())
      break;
    if (input->readLong(1)==0)
      nPages++;
  }
  zone.m_numPages = nPages;
}

void DMText::updatePageSpanList(std::vector<MWAWPageSpan> &spanList)
{
  numPages();
  spanList.resize(0);
  MWAWPageSpan ps;
  ps.setMarginTop(0.1);
  ps.setMarginBottom(0.015);
  ps.setMarginLeft(0.1);
  ps.setMarginRight(0.1);
  bool hasFooter = !m_state->m_footer.empty();
  bool needResetPage = m_state->m_footer.m_chapterResetPage;
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  std::map<int, DMTextInternal::Zone >::const_iterator it =
    m_state->m_idZoneMap.begin();
  for ( ; it != m_state->m_idZoneMap.end(); it++) {
    int zId = it->first;
    DMTextInternal::Zone const &zone= it->second;
    if (zone.m_numPages <= 0)
      continue;
    MWAWPageSpan span(ps);
    if (needResetPage)
      span.setPageNumber(1);
    if (zone.m_margins[0]>=0)
      span.setMarginLeft(double(zone.m_margins[0])/72.);
    if (zone.m_margins[1]>=0)
      span.setMarginTop(double(zone.m_margins[1])/72.);
    if (zone.m_margins[2]>=0)
      span.setMarginRight(double(zone.m_margins[2])/72.);
    if (zone.m_margins[3]>=0)
      span.setMarginBottom(double(zone.m_margins[3])/72.);
    span.setBackgroundColor(zone.m_backgroundColor);
    if (hasFooter && zone.m_useFooter) {
      MWAWHeaderFooter footer(MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
      footer.m_subDocument.reset
      (new DMTextInternal::SubDocument(*this, input, zId, libmwaw::DOC_HEADER_FOOTER));
      span.setHeaderFooter(footer);
    }
    for (int i = 0; i < zone.m_numPages; i++) {
      spanList.push_back(span);
      span.setPageNumber(-1);
    }
  }
  if (spanList.size()==0 || !m_state->m_toc.empty())
    spanList.push_back(ps);
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

// find the different zones
bool DMText::createZones()
{
  if (!m_mainParser->getRSRCParser()) {
    MWAW_DEBUG_MSG(("DMText::createZones: can not find the entry map\n"));
    return false;
  }
  MWAWRSRCParserPtr rsrcParser = m_mainParser->getRSRCParser();
  std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::iterator it;

  // entry 128: font name and size
  it = entryMap.lower_bound("rQDF");
  while (it != entryMap.end()) {
    if (it->first != "rQDF")
      break;
    MWAWEntry const &entry = it++->second;
    readFontNames(entry);
  }
  // Footer: foot:128
  it = entryMap.lower_bound("foot");
  while (it != entryMap.end()) {
    if (it->first != "foot")
      break;
    MWAWEntry const &entry = it++->second;
    readFooter(entry);
  }
  // entry 128: ?
  it = entryMap.lower_bound("cnt#");
  while (it != entryMap.end()) {
    if (it->first != "cnt#")
      break;
    MWAWEntry const &entry = it++->second;
    readTOC(entry);
  }

  // the chapter zone zone
  it = entryMap.lower_bound("styl");
  while (it != entryMap.end()) {
    if (it->first != "styl")
      break;
    MWAWEntry const &entry = it++->second;
    readStyles(entry);
  }
  it = entryMap.lower_bound("TEXT");
  while (it != entryMap.end()) {
    if (it->first != "TEXT")
      break;
    MWAWEntry const &entry = it++->second;
    m_state->getZone(entry.id()).m_pos = entry;
  }

  it = entryMap.lower_bound("Wndo");
  while (it != entryMap.end()) {
    if (it->first != "Wndo")
      break;
    MWAWEntry const &entry = it++->second;
    readWindows(entry);
  }
  // font color
  it = entryMap.lower_bound("clut");
  while (it != entryMap.end()) {
    if (it->first != "clut")
      break;
    MWAWEntry const &entry = it++->second;
    std::vector<MWAWColor> cmap;
    rsrcParser->parseClut(entry, cmap);
    if (entry.id() != 128)
      continue;
    for (size_t i = 0; i < cmap.size(); i++) {
      if (m_state->m_idZoneMap.find(int(i)+128)==m_state->m_idZoneMap.end())
        continue;
      m_state->m_idZoneMap.find(int(i)+128)->second.m_backgroundColor=cmap[i];
    }
  }

  it = entryMap.lower_bound("STR ");
  while (it != entryMap.end()) {
    if (it->first != "STR ")
      break;
    MWAWEntry const &entry = it++->second;
    // 1000: footer (user information)
    if (entry.id()==1000 && entry.length()>0) {
      std::string userInfo("");
      rsrcParser->parseSTR(entry,userInfo);
      m_state->m_footer.m_userInfo = userInfo;
    }
    // 200?: chapter name
    else if (entry.id()>2000 &&
             m_state->m_idZoneMap.find(entry.id()-2001+128)!=m_state->m_idZoneMap.end()) {
      std::string name("");
      rsrcParser->parseSTR(entry,name);
      m_state->getZone(entry.id()-2001+128).m_name=name;
    }
  }
  return m_state->m_idZoneMap.size();
}

////////////////////////////////////////////////////////////
//    Text
////////////////////////////////////////////////////////////
bool DMText::sendText(DMTextInternal::Zone const &zone)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("DMText::sendText: can not find the listener\n"));
    return false;
  }
  if (!zone.m_pos.valid()) {
    MWAW_DEBUG_MSG(("DMText::sendText: the entry is bad\n"));
    return false;
  }
  zone.m_parsed = true;
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  long pos = zone.m_pos.begin(), debPos=pos-4;
  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(TEXT)[" << zone.m_pos.id() << "]:";
  listener->setFont(MWAWFont(3,12));
  MWAWParagraph para;
  para.m_justify=zone.m_justify;
  listener->setParagraph(para);
  std::map<long, MWAWFont >::const_iterator fontIt;
  int nPict=0, zId=zone.m_pos.id()-128;
  double w = m_state->m_pageWidth-double(zone.m_margins[0]+zone.m_margins[2])/72.;
  for (long i = 0; i <= zone.m_pos.length(); i++) {
    bool endPos = (i==zone.m_pos.length());
    unsigned char c=endPos ? (unsigned char)0 : (unsigned char) input->readULong(1);
    if (endPos || c==0xd || c==0) {
      ascFile.addPos(debPos);
      ascFile.addNote(f.str().c_str());
      debPos = input->tell();
      if (endPos) break;
      f.str("");
      f << "TEXT:";
    }
    fontIt=zone.m_posFontMap.find(i);
    if (fontIt != zone.m_posFontMap.end())
      listener->setFont(fontIt->second);
    if (c)
      f << c;
    switch(c) {
    case 0:
      m_mainParser->newPage(++m_state->m_actualPage);
      break;
    case 0x9:
      listener->insertTab();
      break;
    case 0xd:
      listener->insertEOL();
      break;
    case 0x11: // command key
      listener->insertUnicode(0x2318);
      break;
    case 0x14: // apple logo: check me
      listener->insertUnicode(0xf8ff);
      break;
    case 0xca:
      m_mainParser->sendPicture(zId, ++nPict, w);
      break;
    default:
      i += listener->insertCharacter(c, input, zone.m_pos.end());
      break;
    }
  }
  return true;
}

////////////////////////////////////////////////////////////
//     Fonts
////////////////////////////////////////////////////////////
bool DMText::readFontNames(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<2) {
    MWAW_DEBUG_MSG(("DMText::readFontNames: the entry is bad\n"));
    return false;
  }
  entry.setParsed(true);
  long pos = entry.begin();
  long endPos = entry.end();
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  int N=(int) input->readULong(2);
  f << "Entries(FontName)[" << entry.type() << "-" << entry.id() << "]:N="<<N;
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());

  int val;
  for (int i = 0; i < N; i++) {
    f.str("");
    f << "FontName-" << i << ":";
    pos = input->tell();
    if (pos+1 > endPos) {
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      MWAW_DEBUG_MSG(("DMText::readFontNames: can not read fontname %d\n", i));
      return false;
    }
    int sz = (int)input->readULong(1);
    if (pos+1+sz+2 > endPos) {
      f.str("");
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      MWAW_DEBUG_MSG(("DMText::readFontNames: fontname size %d is bad\n", i));
      return false;
    }

    std::string str("");
    for (int c=0; c < sz; c++)
      str += (char) input->readULong(1);
    f << str << ",";

    val=(int) input->readULong(1);
    if (val) f << "unkn=" << val << ",";
    int N1=(int) input->readULong(1);
    if (pos+1+sz+2+N1 > endPos) {
      f.str("");
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      MWAW_DEBUG_MSG(("DMText::readFontNames: fontname size %d is bad\n", i));
      return false;
    }
    f << "fontSz=[";
    for (int j = 0; j < N1; j++)
      f << input->readULong(1) << ",";
    f << "],";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// the styles
////////////////////////////////////////////////////////////
bool DMText::readStyles(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<2) {
    MWAW_DEBUG_MSG(("DMText::readStyles: the entry is bad\n"));
    return false;
  }
  entry.setParsed(true);
  long pos = entry.begin();
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  int N=(int) input->readULong(2);
  f << "Entries(Style)[" << entry.type() << "-" << entry.id() << "]:N="<<N;
  if (20*N+2 != entry.length()) {
    MWAW_DEBUG_MSG(("DMText::readStyles: the number of values seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  DMTextInternal::Zone &zone = m_state->getZone(entry.id());
  for (int i = 0; i < N; i++) {
    MWAWFont font;
    f.str("");
    pos = input->tell();
    long cPos = input->readLong(4);
    int dim[2];
    for (int j = 0; j < 2; j++)
      dim[j] = (int) input->readLong(2);
    f << "height?=" << dim[0] << ":" << dim[1] << ",";
    font.setId((int) input->readULong(2));
    int flag=(int) input->readULong(1);
    uint32_t flags = 0;
    // bit 1 = plain
    if (flag&0x1) flags |= MWAWFont::boldBit;
    if (flag&0x2) flags |= MWAWFont::italicBit;
    if (flag&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (flag&0x8) flags |= MWAWFont::embossBit;
    if (flag&0x10) flags |= MWAWFont::shadowBit;
    if (flag&0xe0) f << "#flags=" << std::hex << (flag&0xe0) << std::dec << ",";
    flag=(int) input->readULong(1);
    if (flag) f << "#flags1=" << std::hex << flag << std::dec << ",";
    font.setSize((float)input->readULong(2));
    font.setFlags(flags);
    unsigned char col[3];
    for (int j=0; j < 3; j++)
      col[j] = (unsigned char) (input->readULong(2)>>8);
    font.setColor(MWAWColor(col[0],col[1],col[2]));
    font.m_extra=f.str();
    if (zone.m_posFontMap.find(cPos) != zone.m_posFontMap.end()) {
      MWAW_DEBUG_MSG(("DMText::readStyles: a style for pos=%lx already exist\n", cPos));
    } else
      zone.m_posFontMap[cPos] = font;
    f.str("");
    f << "Style-" << i << ":" << "cPos=" << std::hex << cPos << std::dec << ",";
#ifdef DEBUG
    f << ",font=[" << font.getDebugString(m_parserState->m_fontConverter) << "]";
#endif
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

//     Table of Content information
////////////////////////////////////////////////////////////
bool DMText::sendTOC()
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("DMText::sendTOC: can not find the listener\n"));
    return false;
  }
  DMTextInternal::TOC const &toc=m_state->m_toc;
  if (toc.empty())
    return true;
  if (toc.m_cIdList.size() != toc.m_textList.size()) {
    MWAW_DEBUG_MSG(("DMText::sendTOC: the TOC is bad\n"));
    return false;
  }

  MWAWFont cFont(3,12);
  cFont.setFlags(MWAWFont::boldBit);
  MWAWFont actFont(3,10);
  listener->setFont(actFont);
  double w = m_state->m_pageWidth;
  MWAWParagraph para;
  MWAWTabStop tab;
  tab.m_alignment = MWAWTabStop::RIGHT;
  tab.m_leaderCharacter='.';
  tab.m_position = w;
  para.m_tabs->push_back(tab);
  listener->setParagraph(para);

  std::stringstream ss;
  int prevId=-1;
  for (size_t i = 0; i < toc.m_textList.size(); i++) {
    int zId=toc.m_cIdList[i];
    ss.str("");
    ss << "C" << zId;

    if (zId!=prevId) {
      prevId=zId;
      listener->setFont(cFont);

      listener->insertUnicodeString(ss.str().c_str());
      listener->insertChar(' ');
      std::string str("");
      if (m_state->m_idZoneMap.find(127+zId)!=m_state->m_idZoneMap.end())
        sendString(m_state->m_idZoneMap.find(127+zId)->second.m_name);
      listener->insertEOL();
      listener->setFont(actFont);
    }
    sendString(toc.m_textList[i]);
    listener->insertTab();
    listener->insertUnicodeString(ss.str().c_str());
    listener->insertEOL();
  }
  return true;
}

bool DMText::readTOC(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<2) {
    MWAW_DEBUG_MSG(("DMText::readTOC: the entry is bad\n"));
    return false;
  }
  entry.setParsed(true);
  long pos = entry.begin();
  long endPos = entry.end();
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(TOC)[" << entry.type() << ":" << entry.id() << "]:";
  int N=(int) input->readULong(2);
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    f.str("");
    f << "TOC-" << i << ":";
    pos = input->tell();
    if (pos+7 > endPos) {
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      MWAW_DEBUG_MSG(("DMText::readTOC: can not read string %d\n", i));
      return false;
    }
    int zId = (int) input->readLong(2);
    if (zId) f << "zId=" << zId+127 << ",";
    int cPos[2];
    for (int j=0; j < 2; j++)
      cPos[j] = (int) input->readULong(2);
    f << "cPos=" << std::hex << cPos[0] << "<->" << cPos[1] << std::dec << ",";
    int sz = (int)input->readULong(1);
    if (pos+7+sz > endPos) {
      f.str("");
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      MWAW_DEBUG_MSG(("DMText::readTOC: string size %d is bad\n", i));
      return false;
    }

    std::string str("");
    for (int c=0; c < sz; c++)
      str += (char) input->readULong(1);
    f << str << ",";
    m_state->m_toc.m_cIdList.push_back(zId);
    m_state->m_toc.m_textList.push_back(str);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

//     Windows information
////////////////////////////////////////////////////////////
bool DMText::readWindows(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<20) {
    MWAW_DEBUG_MSG(("DMText::readWindows: the entry seems very short\n"));
    return false;
  }

  entry.setParsed(true);
  long pos = entry.begin();
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  input->seek(pos, WPX_SEEK_SET);

  DMTextInternal::Zone &zone = m_state->getZone(entry.id());
  libmwaw::DebugStream f;
  f << "Entries(Windows)[" << entry.type() << "-" << entry.id() << "]:";
  int val=(int) input->readLong(2); // always 0?
  if (val) f << "unkn=" << val << ",";
  int wDim[3];
  for (int i = 0; i < 3; i++)
    wDim[i]=(int) input->readLong(2);
  f << "windows=[left=" << wDim[0] << ",right=" << wDim[2]
    << ",bottom=" << wDim[1] << "],";

  for (int i = 0; i < 4; i++)
    zone.m_margins[i]=(int) input->readLong(2);
  f << "margins=[" << zone.m_margins[1] << "x" << zone.m_margins[0]
    << "<->" << zone.m_margins[3] << "x" << zone.m_margins[2] << ",";
  int flag = (int) input->readULong(1);
  if (flag==1) {
    zone.m_useFooter = false;
    f << "noFooter,";
  } else if (flag) f << "#footer=" << flag << ",";
  flag =(int) input->readULong(1); //9|3e|6d|a8|
  if (flag)
    f << "fl=" << std::hex << flag << std::dec << ",";
  val = (int) input->readLong(2); // always 0?
  switch(val) {
  case 0:
    break;
  case 1:
    zone.m_justify = MWAWParagraph::JustificationCenter;
    f << "just=center,";
    break;
  case -1:
    zone.m_justify = MWAWParagraph::JustificationRight;
    f << "just=right,";
    break;
  default:
    f << "#justify=" << val << ",";
    break;
  }

  if (input->tell()!=entry.end())
    ascFile.addDelimiter(input->tell(),'|');
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

//     Footer
////////////////////////////////////////////////////////////
bool DMText::sendFooter(int zId)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("DMText::sendFooter: can not find my listener\n"));
    return false;
  }
  DMTextInternal::Footer const &ft=m_state->m_footer;
  if (ft.empty()) {
    MWAW_DEBUG_MSG(("DMText::sendFooter: oops, the footer is empty\n"));
    return false;
  }
  if (m_state->m_idZoneMap.find(zId)==m_state->m_idZoneMap.end()) {
    MWAW_DEBUG_MSG(("DMText::sendFooter: oops, can not find the zone\n"));
    return false;
  }
  listener->setFont(ft.m_font);

  DMTextInternal::Zone const &zone=m_state->getZone(zId);
  double w = m_state->m_pageWidth-double(zone.m_margins[0]+zone.m_margins[2])/72.;
  MWAWParagraph para;
  MWAWTabStop tab;
  tab.m_alignment = MWAWTabStop::CENTER;
  tab.m_position = w/2.0;
  para.m_tabs->push_back(tab);
  tab.m_alignment = MWAWTabStop::RIGHT;
  tab.m_position = w;
  para.m_tabs->push_back(tab);
  listener->setParagraph(para);

  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  for (int st=0; st<2; st++) {
    int id=3*st;
    if (ft.m_items[id]<=0&&ft.m_items[id+1]<=0&&ft.m_items[id+2]<=0)
      continue;
    for (int f = 0; f < 3; f++, id++) {
      switch (ft.m_items[id]) {
      case 3: {
        MWAWField field(MWAWField::Time);
        field.m_DTFormat="%H:%M";
        listener->insertField(field);
        break;
      }
      case 4: {
        MWAWField field(MWAWField::Date);
        field.m_DTFormat="%a, %b %d, %Y";
        listener->insertField(field);
        break;
      }
      case 5:
        listener->insertUnicodeString("Page ");
        listener->insertField(MWAWField(MWAWField::PageNumber));
        break;
      case 6:
        listener->insertField(MWAWField(MWAWField::Title));
        break;
      case 7:
        sendString(zone.m_name);
        break;
      case 8:
        sendString(ft.m_userInfo);
        break;
      default:
        break;
      }
      if (f!=2)
        listener->insertTab();
    }
    if (st==0)
      listener->insertEOL();
  }
  return true;
}

bool DMText::readFooter(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<22) {
    MWAW_DEBUG_MSG(("DMText::readFooter: the entry seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  long pos = entry.begin();
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  DMTextInternal::Footer &footer=m_state->m_footer;
  for (int i=0; i < 6; i++)
    footer.m_items[i]=(int) input->readLong(2);
  int val;
  for (int i = 0; i < 6; i++) {
    val =(int) input->readLong(1);
    if (!val) continue;
    if (val!=1) {
      f << "#fl" << i << "=" << val << ",";
      continue;
    }
    switch(i) {
    case 0:
      footer.m_chapterResetPage = true;
      break;
    case 2:
      f << "hasSep,";
      break;
    case 4:
      f << "graySep,";
      break;
    default:
      f << "#fl" << i << "=" << 1 << ",";
      break;
    }
  }
  footer.m_font.setId((int) input->readULong(2));
  footer.m_font.setSize((float) input->readULong(2));

  footer.m_extra=f.str();

  f.str("");
  f << "Entries(Footer)[" << entry.type() << "-" << entry.id() << "]:"<<footer;
#ifdef DEBUG
  f << ",font=[" << footer.m_font.getDebugString(m_parserState->m_fontConverter) << "]";
#endif

  if (input->tell()!=entry.end())
    ascFile.addDelimiter(input->tell(),'|');
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

void DMText::sendString(std::string const &str) const
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) return;

  for (size_t s=0; s < str.size(); s++)
    listener->insertCharacter((unsigned char)str[s]);
}

bool DMText::sendMainText()
{
  if (!m_parserState->m_listener) return true;

  std::map<int, DMTextInternal::Zone >::const_iterator it = m_state->m_idZoneMap.begin();
  for ( ; it != m_state->m_idZoneMap.end(); it++) {
    DMTextInternal::Zone const &zone= it->second;
    if (zone.m_parsed)
      continue;
    if (sendText(zone))
      m_mainParser->newPage(++m_state->m_actualPage);
  }
  return true;
}


void DMText::flushExtra()
{
  return;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
