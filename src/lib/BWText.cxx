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
#include "MWAWInputStream.hxx"
#include "MWAWPageSpan.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSection.hxx"
#include "MWAWSubDocument.hxx"

#include "BWParser.hxx"

#include "BWText.hxx"

/** Internal: the structures of a BWText */
namespace BWTextInternal
{
////////////////////////////////////////
//! Internal: a class used to store the font data of a BWText
struct Font {
  //! constructor
  Font() : m_id(0), m_size(12), m_flags(0), m_color(0), m_extra() {
  }
  /** returns a MWAWFont.
      \note the font id remains filled with the local id */
  MWAWFont getFont() const {
    MWAWFont res(m_id,float(m_size));
    uint32_t flags=0;
    if (m_flags&1) flags |= MWAWFont::boldBit;
    if (m_flags&2) flags |= MWAWFont::italicBit;
    if (m_flags&4) res.setUnderlineStyle(MWAWFont::Line::Simple);
    if (m_flags&8) flags |= MWAWFont::embossBit;
    if (m_flags&0x10) flags |= MWAWFont::shadowBit;
    if (m_flags&0x100) res.set(MWAWFont::Script::super());
    if (m_flags&0x200) res.set(MWAWFont::Script::sub());
    if (m_flags&0x400) flags |= MWAWFont::allCapsBit;
    if (m_flags&0x800) flags |= MWAWFont::lowercaseBit;
    res.setFlags(flags);
    switch(m_color) {
    case 63:
      res.setColor(MWAWColor::white());
      break;
    case 100:
      res.setColor(MWAWColor(0xFF,0xFF,0));
      break;
    case 168:
      res.setColor(MWAWColor(0xFF,0,0xFF));
      break;
    case 236:
      res.setColor(MWAWColor(0xFF,0,0));
      break;
    case 304:
      res.setColor(MWAWColor(0,0xFF,0xFF));
      break;
    case 372:
      res.setColor(MWAWColor(0,0xFF,0));
      break;
    case 440:
      res.setColor(MWAWColor(0,0,0xFF));
      break;
    default:
      break;
    }
    return res;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Font const &fnt) {
    if (fnt.m_id) o << "id=" << fnt.m_id << ",";
    if (fnt.m_size!=12) o << "sz=" << fnt.m_size << ",";
    if (fnt.m_flags&1) o << "b,";
    if (fnt.m_flags&2) o << "it,";
    if (fnt.m_flags&4) o << "underline,";
    if (fnt.m_flags&8) o << "outline,";
    if (fnt.m_flags&0x10) o << "shadow,";
    if (fnt.m_flags&0x100) o << "sup,";
    if (fnt.m_flags&0x200) o << "sub,";
    if (fnt.m_flags&0x400) o << "uppercase,";
    if (fnt.m_flags&0x800) o << "lowercase,";
    if (fnt.m_flags&0xF0E0)
      o << "fl=" << std::hex << (fnt.m_flags&0xF0E0) << std::dec << ",";
    switch(fnt.m_color) {
    case 0: // black
      break;
    case 63:
      o << "white,";
      break;
    case 100:
      o << "yellow,";
      break;
    case 168:
      o << "magenta,";
      break;
    case 236:
      o << "red,";
      break;
    case 304:
      o << "cyan,";
      break;
    case 372:
      o << "green,";
      break;
    case 440:
      o << "blue,";
      break;
    default:
      o << "#color=" << fnt.m_color << ",";
      break;
    }
    o << fnt.m_extra;
    return o;
  }

  //! the font id
  int m_id;
  //! the font size
  int m_size;
  //! the font flags
  int m_flags;
  //! the font color
  int m_color;
  //! extra data
  std::string m_extra;
};
////////////////////////////////////////
//! Internal: a class used to store the section data of a BWText
struct Section : public MWAWSection {
  //! constructor
  Section() : MWAWSection(), m_ruler(), m_hasFirstPage(false), m_hasHeader(false), m_hasFooter(false),
    m_pageNumber(1),  m_usePageNumber(false), m_extra("") {
    for (int i=0; i<5; ++i)
      m_limitPos[i]=0;
    for (int i=0; i<4; ++i)
      m_parsed[i]=false;
    m_heights[0]=m_heights[1]=0;
    m_balanceText=true;
  }
  //! return the i^th entry
  MWAWEntry getEntry(int i) const {
    MWAWEntry res;
    if (i<0||i>=4) {
      MWAW_DEBUG_MSG(("BWTextInternal::getEntry: called with bad id=%d\n",i));
      return res;
    }
    if (m_limitPos[i]<=0)
      return res;
    res.setBegin(m_limitPos[i]);
    res.setEnd(m_limitPos[i+1]-2);
    return res;
  }
  //! return the header entry
  MWAWEntry getHeaderEntry(bool fPage) const {
    return getEntry(fPage?0:2);
  }
  //! return true if we have a header
  MWAWEntry getFooterEntry(bool fPage) const {
    return getEntry(fPage?1:3);
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Section const &sec) {
    o << static_cast<MWAWSection const &>(sec);
    for (int i=0; i<4; ++i) {
      if (sec.m_limitPos[i+1]<=sec.m_limitPos[i]+2)
        continue;
      static char const *(wh[])= {"header[fP]", "footer[fP]", "header", "footer"};
      o << wh[i] << "=" << std::hex << sec.m_limitPos[i]
        << "->" << sec.m_limitPos[i+1] << std::hex << ",";
    }
    if (sec.m_hasFirstPage) o << "firstPage[special],";
    if (!sec.m_hasHeader) o << "hide[header],";
    else if (sec.m_heights[0]) o << "h[header]=" << sec.m_heights[0] << ",";
    if (!sec.m_hasFooter) o << "hide[footer],";
    else if (sec.m_heights[1]) o << "h[footer]=" << sec.m_heights[1] << ",";
    if (sec.m_pageNumber != 1) o << "pagenumber=" << sec.m_pageNumber << ",";
    if (sec.m_usePageNumber) o << "pagenumber[use],";
    o << sec.m_extra;
    return o;
  }
  //! the default section ruler
  MWAWParagraph m_ruler;
  //! a flag to know if the first page is special
  bool m_hasFirstPage;
  //! a flag to know if we need to print the header
  bool m_hasHeader;
  //! a flag to know if we need to print the footer
  bool m_hasFooter;
  //! the data limits ( first page header, first page footer, header, footer, end)
  long m_limitPos[5];
  //! true if the data are send to the listener
  mutable bool m_parsed[4];
  //! the header/footer height
  int m_heights[2];
  //! the page number
  int m_pageNumber;
  //! true if we need to use the page number
  bool m_usePageNumber;
  //! extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the state of a BWText
struct State {
  //! constructor
  State() : m_textEntry(), m_sectionList(), m_numPagesBySectionList(), m_version(-1), m_fileIdFontIdList(), m_numPages(-1), m_actualPage(1) {
  }
  //! returns the font corresponding to a file font
  MWAWFont getFont(Font const &ft) {
    MWAWFont font=ft.getFont();
    int fId=font.id();
    if (fId<0||fId>=int(m_fileIdFontIdList.size())) {
      MWAW_DEBUG_MSG(("BWTextInternal::State:getFont can not find the final font id\n"));
      font.setId(3);
    } else
      font.setId(m_fileIdFontIdList[size_t(fId)]);
    return font;
  }
  //! the main text entry
  MWAWEntry m_textEntry;
  //! the section list
  std::vector<Section> m_sectionList;
  //! the number of page by section
  std::vector<int> m_numPagesBySectionList;
  //! the file version
  mutable int m_version;
  //! a list to get the correspondance between fileId and fontId
  std::vector<int> m_fileIdFontIdList;
  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;
};

////////////////////////////////////////
//! Internal: the subdocument of a BWText
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(BWText &pars, MWAWInputStreamPtr input, int hFId, int sId) :
    MWAWSubDocument(pars.m_mainParser, input, MWAWEntry()), m_textParser(&pars), m_hfId(hFId), m_sectId(sId) {
  }

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
  BWText *m_textParser;
  //! the header/footer id
  int m_hfId;
  //! the section id
  int m_sectId;
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
  if (m_hfId != sDoc->m_hfId) return true;
  if (m_sectId != sDoc->m_sectId) return true;
  return false;
}

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("BWTextInternal::SubDocument::parse: no listener\n"));
    return;
  }
  assert(m_textParser);

  long pos = m_input->tell();
  m_textParser->sendHF(m_hfId, m_sectId);
  m_input->seek(pos, WPX_SEEK_SET);
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
BWText::BWText(BWParser &parser) :
  m_parserState(parser.getParserState()), m_state(new BWTextInternal::State), m_mainParser(&parser)
{
}

BWText::~BWText()
{ }

int BWText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}

int BWText::numPages() const
{
  if (m_state->m_numPages <= 0)
    const_cast<BWText *>(this)->countPages();
  return m_state->m_numPages;
}

shared_ptr<MWAWSubDocument> BWText::getHeader(int page, int &numSimillar)
{
  numSimillar=1;
  shared_ptr<MWAWSubDocument> res;
  int actPage=0, newSectionPage=0;
  size_t s=0;
  for ( ; s < m_state->m_numPagesBySectionList.size(); s++) {
    newSectionPage+=m_state->m_numPagesBySectionList[s];
    if (newSectionPage>page)
      break;
    actPage=newSectionPage;
  }
  if (s >= m_state->m_sectionList.size())
    return res;
  BWTextInternal::Section const &sec=m_state->m_sectionList[s];
  bool useFPage=page==actPage && sec.m_hasFirstPage;
  if (!useFPage)
    numSimillar=newSectionPage-page;
  if (sec.getHeaderEntry(useFPage).valid())
    res.reset(new BWTextInternal::SubDocument
              (*this, m_parserState->m_input, useFPage?0:2, int(s)));
  return res;
}

shared_ptr<MWAWSubDocument> BWText::getFooter(int page, int &numSimillar)
{
  numSimillar=1;
  shared_ptr<MWAWSubDocument> res;
  int actPage=0, newSectionPage=0;
  size_t s=0;
  for ( ; s < m_state->m_numPagesBySectionList.size(); s++) {
    newSectionPage+=m_state->m_numPagesBySectionList[s];
    if (newSectionPage>page)
      break;
    actPage=newSectionPage;
  }
  if (s >= m_state->m_sectionList.size())
    return res;
  BWTextInternal::Section const &sec=m_state->m_sectionList[s];
  bool useFPage=page==actPage && sec.m_hasFirstPage;
  if (!useFPage)
    numSimillar=newSectionPage-page;
  if (sec.getFooterEntry(useFPage).valid())
    res.reset(new BWTextInternal::SubDocument
              (*this, m_parserState->m_input, useFPage?1:3, int(s)));
  return res;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

//
// find the different zones
//
bool BWText::createZones(MWAWEntry &entry)
{
  if (!entry.valid() || entry.length()<22) {
    MWAW_DEBUG_MSG(("BWText::createZones: the entry seems bad\n"));
    return false;
  }

  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=entry.begin(), endPos=entry.end();
  input->seek(pos+4, WPX_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(THeader):";
  long val=input->readLong(4); // always 0x238
  if (val!=0x238)
    f << "f0=" << val << ",";
  val=input->readLong(2);
  if (val!=1)
    f << "f1=" << val << ",";
  val=(long) input->readULong(4);
  int nSections=int(entry.length()-val);
  if (val<22|| nSections<6 || (nSections%6)) {
    f << "###";
    MWAW_DEBUG_MSG(("BWText::createZones: the data size seems bad\n"));
    return false;
  }
  endPos = pos+val;
  nSections/=6;
  for (int i=0; i<2; i++) { // f2=0, f3=6
    val=input->readLong(2);
    if (val)
      f << "f" << i+2 << "=" << val << ",";
  }
  val=input->readLong(2);
  f << "nSect=" << val << ",";
  if (val!=nSections) {
    f << "###";
    MWAW_DEBUG_MSG(("BWText::createZones: the number of sections/pages seems bad\n"));
  }
  // checkme: after junk ?
  ascFile.addDelimiter(input->tell(),'|');
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  input->seek(endPos, WPX_SEEK_SET);
  std::vector<MWAWEntry> listEntries;
  f.str("");
  f << "Entries(Text):";
  for (int i=0; i <nSections; ++i) {
    pos=input->tell();
    MWAWEntry pEntry;
    pEntry.setBegin(entry.begin()+(long)input->readULong(4));
    pEntry.setLength((long)input->readULong(2));
    f << std::hex << pEntry.begin() << "<->" << pEntry.end() << std::dec << ",";
    if (!pEntry.valid() || pEntry.begin()+16 < entry.begin()
        || pEntry.end()>endPos) {
      pEntry=MWAWEntry();
      f << "###";
      MWAW_DEBUG_MSG(("BWText::createZones: the page entry %d seems bad\n", i));
    }
    listEntries.push_back(pEntry);
    input->seek(pos+6, WPX_SEEK_SET);
  }
  ascFile.addPos(endPos);
  ascFile.addNote(f.str().c_str());

  size_t p=0;
  m_state->m_textEntry.setBegin(listEntries[0].begin());

  for (p=0; p+1 < listEntries.size(); ++p) {
    if (!listEntries[p].valid())
      continue;
    if (p) {
      // use the section signature to diffentiate text/section (changeme)
      input->seek(listEntries[p].begin(), WPX_SEEK_SET);
      if (input->readLong(2)==0xdc)
        break;
    }
    m_state->m_textEntry.setEnd(listEntries[p].end());
  }
  for ( ; p < listEntries.size(); ++p) {
    BWTextInternal::Section sec;
    if (listEntries[p].valid() && !readSection(listEntries[p], sec))
      sec = BWTextInternal::Section();
    m_state->m_sectionList.push_back(sec);
  }
  input->seek(entry.end(), WPX_SEEK_SET);
  return m_state->m_textEntry.valid();
}

void BWText::countPages()
{
  if (!m_state->m_textEntry.valid()) {
    MWAW_DEBUG_MSG(("BWText::countPages: can not find the main entry\n"));
    return;
  }
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=m_state->m_textEntry.begin(), endPos=m_state->m_textEntry.end();
  input->seek(pos, WPX_SEEK_SET);
  int nSectPages=0, nPages=1;
  while (!input->atEOS()) {
    pos=input->tell();
    if (pos>=endPos) break;
    unsigned char c = (unsigned char) input->readULong(1);
    if (c) continue;
    c=(unsigned char) input->readULong(1);
    bool done=false;
    input->seek(pos, WPX_SEEK_SET);
    switch(c) {
    case 0: {
      BWTextInternal::Font font;
      done=readFont(font,endPos);
      break;
    }
    case 1: {
      MWAWParagraph para;
      done=readParagraph(para,endPos);
      break;
    }
    case 2:
      if (pos+6 > endPos)
        break;
      input->seek(4, WPX_SEEK_CUR);
      done = input->readLong(2)==0x200;
      break;
    case 3: { // type 3:page 4:section
      if (pos+6 > endPos)
        break;
      input->seek(2, WPX_SEEK_CUR);
      int type=(int) input->readLong(2);
      if (input->readLong(2)!=0x300)
        break;
      if (type==3) {
        nSectPages++;
        nPages++;
      } else if (type==4) {
        m_state->m_numPagesBySectionList.push_back(nSectPages);
        nSectPages=0;
      }
      done=true;
      break;
    }
    case 4: // picture
      if (pos+8 > endPos)
        break;
      input->seek(6, WPX_SEEK_CUR);
      done = input->readLong(2)==0x400;
      break;
    case 5: // a field
      if (pos+36 > endPos)
        break;
      input->seek(34, WPX_SEEK_CUR);
      done=input->readLong(2)==0x500;
      break;
    default:
      break;
    }
    if (!done)
      break;
  }
}

//
// send the text
//
bool BWText::sendMainText()
{
  return sendText(m_state->m_textEntry);
}

bool BWText::sendHF(int hfId, int sectId)
{
  if (hfId<0||hfId>=4) {
    MWAW_DEBUG_MSG(("BWText::sendHF: hfId=%d is bad\n", hfId));
    return false;
  }
  if (sectId<0||sectId>=(int) m_state->m_sectionList.size()) {
    MWAW_DEBUG_MSG(("BWText::sendHF: can not find section %d\n", sectId));
    return false;
  }

  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  BWTextInternal::Section const &sec=m_state->m_sectionList[size_t(sectId)];
  sec.m_parsed[hfId]=true;
  bool ok=sendText(sec.getEntry(hfId));
  input->seek(pos,WPX_SEEK_SET);
  return ok;
}

void BWText::flushExtra()
{
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  for (size_t i=0; i<m_state->m_sectionList.size(); ++i) {
    BWTextInternal::Section const &sec=m_state->m_sectionList[i];

    for (int j=0; j < 4; ++j) {
      if (sec.m_parsed[j])
        continue;
      MWAWEntry hfEntry=sec.getEntry(j);
      if (!hfEntry.valid()) {
        if (hfEntry.begin()>0) {
          ascFile.addPos(hfEntry.begin());
          ascFile.addNote("_");
        }
        continue;
      }
      sendText(hfEntry);
    }
  }
}

bool BWText::sendText(MWAWEntry entry)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("BWText::sendText: can not find the listener\n"));
    return false;
  }
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("BWText::sendText: can not find the entry\n"));
    return false;
  }

  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=entry.begin(), debPos=pos, endPos=entry.end();
  bool isMain=entry.begin()==m_state->m_textEntry.begin();
  size_t actSection=0, numSection=isMain ? m_state->m_sectionList.size() : 0;
  if (actSection<numSection) {
    if (listener->isSectionOpened())
      listener->closeSection();
    listener->openSection(m_state->m_sectionList[actSection++]);
  }

  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Text:";
  BWTextInternal::Font font;
  listener->setFont(m_state->getFont(font));
  int actPage = 1, sectPage=1;
  while (!input->atEOS()) {
    pos=input->tell();
    bool last=pos==endPos;
    unsigned char c = last ? (unsigned char) 0 :
                      (unsigned char) input->readULong(1);
    if ((c==0 || c==0xd) && pos!=debPos) {
      ascFile.addPos(debPos);
      ascFile.addNote(f.str().c_str());
      debPos=(c==0xd) ? pos+1:pos;
      f.str("");
      f << "Text:";
    }
    if (last) break;
    if (c) {
      f << c;
      switch(c) {
      case 0x1: // end zone marker, probably save to ignore
        break;
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
      continue;
    }
    c=(unsigned char) input->readULong(1);
    bool done=false;
    input->seek(pos, WPX_SEEK_SET);
    switch(c) {
    case 0:
      if (!readFont(font,endPos))
        break;
      done=true;
      listener->setFont(m_state->getFont(font));
      break;
    case 1: {
      MWAWParagraph para;
      if (!readParagraph(para,endPos))
        break;
      done=true;
      listener->setParagraph(para);
      break;
    }
    case 2: {
      if (pos+6 > endPos)
        break;
      input->seek(2, WPX_SEEK_CUR);
      int type=(int) input->readLong(2);
      if (input->readLong(2)!=0x200)
        break;
      f.str("");
      f << "Entries(Field):";
      switch(type) {
      case 0:
      case 1: {
        std::stringstream s;
        if (type==0) {
          f << "pagenumber[section]";
          s << sectPage;
        } else {
          f << "section";
          s << actSection;
        }
        listener->insertUnicodeString(s.str().c_str());
        break;
      }
      case 2:
        listener->insertField(MWAWField(MWAWField::PageNumber));
        f << "pagenumber";
        break;
      case 3:
        listener->insertField(MWAWField(MWAWField::Date));
        f << "date";
        break;
      case 4: {
        MWAWField field(MWAWField::Time);
        field.m_DTFormat="%H:%M";
        listener->insertField(field);
        f << "time";
        break;
      }
      default:
        MWAW_DEBUG_MSG(("BWText::sendText: find unknown field type=%d\n", type));
        f << "#type=" << type << ",";
        break;
      }
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      done=true;
      break;
    }
    case 3: {
      if (pos+6 > endPos)
        break;
      input->seek(2, WPX_SEEK_CUR);
      int type=(int) input->readLong(2);
      if (input->readLong(2)!=0x300)
        break;
      f.str("");
      f << "Entries(Break):";
      switch(type) {
      case 3:
        f << "pagebreak";
        sectPage++;
        if (!isMain) break;
        m_mainParser->newPage(++actPage);
        break;
      case 4:
        f << "sectionbreak";
        sectPage=1;
        if (!isMain) break;
        if (actSection<numSection) {
          if (listener->isSectionOpened())
            listener->closeSection();
          listener->openSection(m_state->m_sectionList[actSection++]);
        } else {
          MWAW_DEBUG_MSG(("BWText::sendText: can not find the new section\n"));
        }
        break;
      default:
        MWAW_DEBUG_MSG(("BWText::sendText: find unknown break type=%d\n", type));
        f << "#type=" << type << ",";
        break;
      }
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      done=true;
      break;
    }
    case 4: { // picture
      if (pos+8 > endPos)
        break;
      input->seek(2, WPX_SEEK_CUR);
      int val=(int) input->readLong(2);
      int id=(int) input->readULong(2);
      if (input->readLong(2)!=0x400)
        break;
      f.str("");
      f << "Entries(Picture):id?=" << id << ",";
      if (val) f << "f0=" << val << ",";
      m_mainParser->sendFrame(id);
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      done=true;
      break;
    }
    case 5: { // a field
      if (pos+36 > endPos)
        break;
      input->seek(2, WPX_SEEK_CUR);
      f.str("");
      f << "Entries(Database):";
      int fl=(int) input->readULong(1); // find 40
      if (fl) f << "fl=" << std::hex << fl << std::dec << ",";
      int fSz=(int) input->readULong(1);
      if (fSz>30) {
        MWAW_DEBUG_MSG(("BWText::sendText: field name size seems bad\n"));
        fSz=0;
        f << "###";
      }
      std::string name("");
      listener->insertUnicode(0xab);
      for (int i=0; i < fSz; ++i) {
        unsigned char ch=(unsigned char) input->readULong(1);
        listener->insertCharacter(ch);
        name+=(char)ch;
      }
      listener->insertUnicode(0xbb);
      f << name;
      input->seek(pos+34, WPX_SEEK_SET);
      if (input->readLong(2)!=0x500)
        break;
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      done=true;
      break;

    }
    default:
      break;
    }
    if (done) {
      debPos=input->tell();
      f.str("");
      f << "Text:";
      continue;
    }
    input->seek(pos, WPX_SEEK_SET);
    break;
  }
  if (input->tell()!=endPos) {
    ascFile.addPos(input->tell());
    ascFile.addNote("Text:###");

    MWAW_DEBUG_MSG(("BWText::sendText: find extra data\n"));
    input->seek(endPos, WPX_SEEK_SET);
  }
  ascFile.addPos(endPos);
  ascFile.addNote("_");
  return true;
}

//////////////////////////////////////////////
// Fonts
//////////////////////////////////////////////
bool BWText::readFont(BWTextInternal::Font &font, long endPos)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  if (pos+12 > endPos || input->readLong(2)) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  font.m_size ^= (int) input->readLong(2);
  font.m_flags ^= (int)input->readULong(2);
  font.m_color ^= (int) input->readLong(2);
  int val=(int)input->readULong(1);
  if (val) // find b1 and 20
    f << "#f0=" << std::hex << val << std::dec << ",";
  font.m_id ^= (int) input->readULong(1);
  font.m_extra=f.str();
  f.str("");
  f << "Entries(FontDef):" << font;
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  // now the reverse header
  if (input->readLong(2)) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  input->seek(pos+12, WPX_SEEK_SET);
  return true;
}

bool BWText::readFontsName(MWAWEntry &entry)
{
  if (!entry.valid())
    return (entry.length()==0&&entry.id()==0);

  entry.setParsed(true);
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=entry.begin(), endPos=entry.end();
  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  m_state->m_fileIdFontIdList.resize(0);
  for (int i=0; i < entry.id(); ++i) {
    pos = input->tell();
    f.str("");
    f << "Entries(FontNames)[" << i << "]:";
    int fSz=(int) input->readULong(1);
    if (pos+1+fSz>endPos) {
      MWAW_DEBUG_MSG(("GWText::readFontNames: can not read font %d\n", i));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      input->seek(endPos, WPX_SEEK_SET);
      return i>0;
    }
    std::string name("");
    for (int c=0; c < fSz; ++c)
      name+=(char) input->readULong(1);
    if (!name.empty())
      m_state->m_fileIdFontIdList.push_back(m_parserState->m_fontConverter->getId(name));

    f << "\"" << name << "\",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  pos = input->tell();
  if (pos!=endPos) {
    MWAW_DEBUG_MSG(("BWText::readFontNames: find extra data\n"));
    ascFile.addPos(pos);
    ascFile.addNote("FontNames:###");
    input->seek(endPos, WPX_SEEK_SET);
  }

  return true;
}

//////////////////////////////////////////////
// Paragraph
//////////////////////////////////////////////
bool BWText::readParagraph(MWAWParagraph &para, long endPos, bool inSection)
{
  para=MWAWParagraph();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  if (pos+23 > endPos) return false;

  int fSz=0;
  if (!inSection) {
    bool ok= input->readLong(2)==1;
    fSz=ok ? (int)input->readULong(1) : 0;
    if (!ok || fSz < 19 || pos+4+fSz > endPos) {
      input->seek(pos, WPX_SEEK_SET);
      return false;
    }
  }

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  para.setInterline(1.+double(input->readULong(1))/10., WPX_PERCENT);
  // para spacing, before/after
  para.m_spacings[1] = para.m_spacings[2] =
                         (double(input->readULong(1))/10.)*6./72.;
  int fl=(int)input->readULong(1);
  switch(fl&0xf) {
  case 1: // left
    break;
  case 2:
    para.m_justify=MWAWParagraph::JustificationRight;
    break;
  case 4:
    para.m_justify=MWAWParagraph::JustificationCenter;
    break;
  case 8:
    para.m_justify=MWAWParagraph::JustificationFull;
    break;
  default:
    f << "#align=" << (fl&0xf) << ",";
    break;
  }
  fl &=0xFFF0; // find 60 or 70
  if (fl) f << "flags=" << std::hex << fl << std::dec << ",";
  para.m_marginsUnit = WPX_POINT;
  for (int i=0; i<3; ++i) // left, right, indent
    para.m_margins[i==2 ? 0 : i+1]=double(input->readLong(4))/65536.;
  int nTabs=(int) input->readLong(2);
  if ((inSection && (nTabs < 0 || nTabs>20)) ||
      (!inSection && 19+nTabs*6!=fSz)) {
    MWAW_DEBUG_MSG(("BWText::readParagraph: the number of tabs seems bad\n"));
    f << "###numTabs=" << nTabs << ",";
    nTabs=0;
  }
  for (int i=0; i<nTabs; ++i) {
    MWAWTabStop tab;
    tab.m_position=double(input->readLong(4))/65536./72;
    int val=(int) input->readLong(1);
    switch(val) {
    case 1: // left
      break;
    case 2:
      tab.m_alignment=MWAWTabStop::RIGHT;
      break;
    case 3:
      tab.m_alignment=MWAWTabStop::CENTER;
      break;
    case 4:
      tab.m_alignment=MWAWTabStop::DECIMAL;
      break;
    case 5:
      tab.m_alignment=MWAWTabStop::BAR;
      break;
    default:
      MWAW_DEBUG_MSG(("BWText::readParagraph: find unknown tab align=%d\n", val));
      f << "tabs" << i << "[#align=" << tab.m_alignment << "],";
      break;
    }
    unsigned char leader=(unsigned char)input->readULong(1);
    if (leader) {
      int unicode= m_parserState->m_fontConverter->unicode(3, leader);
      if (unicode==-1)
        tab.m_leaderCharacter =(unsigned short) leader;
      else
        tab.m_leaderCharacter =(unsigned short) unicode;
    }
    para.m_tabs->push_back(tab);
  }
  para.m_extra=f.str();
  f.str("");
  f << "Entries(Ruler):" << para;
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  if (inSection)
    return true;
  // now the reverse header
  if ((int) input->readULong(1)!=fSz || input->readLong(2)!=0x100) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  input->seek(pos+4+fSz, WPX_SEEK_SET);
  return true;
}

//////////////////////////////////////////////
// Section
//////////////////////////////////////////////
bool BWText::readSection(MWAWEntry const &entry, BWTextInternal::Section &sec)
{
  sec=BWTextInternal::Section();
  if (entry.length()<0xdc) {
    MWAW_DEBUG_MSG(("BWText::readSection: the entry seems bad\n"));
    return false;
  }
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  long pos=entry.begin();
  input->seek(pos, WPX_SEEK_SET);
  if (input->readULong(2)!=0xdc) {
    MWAW_DEBUG_MSG(("BWText::readSection: the section header seems bad\n"));
    ascFile.addPos(pos);
    ascFile.addNote("Entries(Section):###");
    return false;
  }

  sec.m_limitPos[0]=pos+0xdc;
  for (int i=1; i < 5; ++i) {
    sec.m_limitPos[i]=pos+(long) input->readULong(2);
    if (sec.m_limitPos[i]>entry.end()) {
      MWAW_DEBUG_MSG(("BWText::readSection: some limits seem too big\n"));
      f << "###limit-" << i << "=" << std::hex << sec.m_limitPos[i-1] << std::dec << ",";
      sec.m_limitPos[i]=0;
    }
    if (sec.m_limitPos[i]<=sec.m_limitPos[i-1]) {
      MWAW_DEBUG_MSG(("BWText::readSection: some limits seem incoherent\n"));
      f << "###limit-" << i << "=" << std::hex << sec.m_limitPos[i-1] << "x"
        << sec.m_limitPos[i]  << std::dec << ",";
    }
  }
  int nCols=(int) input->readULong(1);
  if (nCols<0 || nCols>16) {
    MWAW_DEBUG_MSG(("BWText::readSection: the number of columns seems bad\n"));
    f << "###nCols=" << nCols << ",";
    nCols=1;
  }
  long val=(long)input->readULong(1); // 0|1|6|1e
  if (val) f << "f0=" << std::hex << val << std::dec << ",";
  double colSep=double(input->readLong(4))/65536;
  if (colSep<48 || colSep>48)
    f << "colSep=" << colSep << ",";
  if (nCols>1)
    sec.setColumns(nCols, m_mainParser->getPageWidth()/double(nCols), WPX_INCH, colSep/72.);
  for (int st=0; st<2; ++st) {
    f << ((st==0) ? "header=[" : "footer=[");
    sec.m_heights[st]=(int) input->readLong(2);
    val = input->readLong(2);
    if (val) f << "fl=" << val << ",";
    val = input->readLong(2); // right/left page ?
    if (val!=sec.m_heights[st]) f << "dim2=" << val << ",";
    f << "],";
  }
  sec.m_pageNumber=(int) input->readLong(2);
  unsigned long flags= input->readULong(4);
  sec.m_hasFirstPage = (flags & 0x10000);
  if (flags & 0x20000) f << "newPage,";
  sec.m_hasHeader = (flags & 0x40000);
  sec.m_hasFooter = (flags & 0x80000);
  sec.m_usePageNumber = (flags & 0x100000);
  if (flags & 0x400000)
    sec.m_columnSeparator=MWAWBorder();
  flags &= 0xFFA0FFFF;
  if (val) f << "flags=" << std::hex << flags << std::dec << ",";
  val=input->readLong(2);
  if (val!=1) f << "page=" << val << ",";
  val=input->readLong(2);
  if (val) f << "yPos=" << val << ",";
  sec.m_extra=f.str();
  f.str("");
  f << "Entries(Section):" << sec;
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  pos=input->tell();
  ascFile.addPos(pos);
  ascFile.addNote("Section-II:");

  input->seek(entry.begin()+81,WPX_SEEK_SET);
  if (!readParagraph(sec.m_ruler, pos+0xda, true)) {
    sec.m_ruler=MWAWParagraph();
    MWAW_DEBUG_MSG(("BWText::readSection: can not read the section ruler\n"));
    ascFile.addPos(pos+81);
    ascFile.addNote("Section(Ruler):###");
  }

  input->seek(entry.begin()+0xda,WPX_SEEK_SET);
  pos=input->tell();
  f.str("");
  f << "Section-III:";
  val=(long) input->readULong(2); // find 3007, 4fef, 7006, fff9 ?
  if (val) f << "f0=" << std::hex << val << std::dec << ",";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  input->seek(entry.end(),WPX_SEEK_SET);
  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
