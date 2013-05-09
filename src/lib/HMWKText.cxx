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
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSection.hxx"

#include "HMWKParser.hxx"

#include "HMWKText.hxx"

/** Internal: the structures of a HMWKText */
namespace HMWKTextInternal
{
/** Internal: class to store the paragraph properties of a HMWKText */
struct Paragraph : public MWAWParagraph {
  //! Constructor
  Paragraph() : MWAWParagraph(), m_type(0), m_addPageBreak(false) {
    m_tabsRelativeToLeftMargin=false;
  }
  //! destructor
  ~Paragraph() {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Paragraph const &ind) {
    switch(ind.m_type) {
    case 0:
      break;
    case 1:
      o << "header,";
      break;
    case 2:
      o << "footer,";
      break;
    case 5:
      o << "footnote,";
      break;
    default:
      o << "#type=" << ind.m_type << ",";
      break;
    }
    o << reinterpret_cast<MWAWParagraph const &>(ind) << ",";
    if (ind.m_addPageBreak) o << "pageBreakBef,";
    return o;
  }
  //! the type
  int m_type;
  //! flag to store a force page break
  bool m_addPageBreak;
};

/** Internal: class to store a section of a HMWKText */
struct Section {
  //! constructor
  Section() : m_numCols(1), m_colWidth(), m_colSep(), m_id(0), m_extra("") {
  }
  //! returns a MWAWSection
  MWAWSection getSection() const {
    MWAWSection sec;
    if (m_colWidth.size()==0) {
      MWAW_DEBUG_MSG(("HMWKTextInternal::Section:getSection can not find any width\n"));
      return sec;
    }
    if (m_numCols <= 1)
      return sec;
    bool hasSep = m_colWidth.size()==m_colSep.size();
    sec.m_columns.resize(size_t(m_numCols));
    if (m_colWidth.size()==size_t(m_numCols)) {
      for (size_t c=0; c < size_t(m_numCols); c++) {
        sec.m_columns[c].m_width = double(m_colWidth[c]);
        sec.m_columns[c].m_widthUnit = WPX_POINT;
        if (!hasSep) continue;
        sec.m_columns[c].m_margins[libmwaw::Left]=
          sec.m_columns[c].m_margins[libmwaw::Right]=double(m_colSep[c])/2.0/72.;
      }
    } else {
      if (m_colWidth.size()>1) {
        MWAW_DEBUG_MSG(("HMWKTextInternal::Section:getSection colWidth is not coherent with numCols\n"));
      }
      sec.setColumns(m_numCols, double(m_colWidth[0]), WPX_POINT,
                     hasSep ? double(m_colSep[0])/72.0 : 0);
    }
    return sec;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Section const &sec) {
    if (sec.m_numCols!=1)
      o << "numCols=" << sec.m_numCols << ",";
    if (sec.m_colWidth.size()) {
      o << "colWidth=[";
      for (size_t i = 0; i < sec.m_colWidth.size(); i++)
        o << sec.m_colWidth[i] << ":" << sec.m_colSep[i] << ",";
      o << "],";
    }
    if (sec.m_id)
      o << "id=" << std::hex << sec.m_id << std::dec << ",";
    o << sec.m_extra;
    return o;
  }
  //! the number of column
  int m_numCols;
  //! the columns width
  std::vector<double> m_colWidth;
  //! the columns separator width
  std::vector<double> m_colSep;
  //! the id
  long m_id;
  //! extra string string
  std::string m_extra;
};

/** Internal: class to store the token properties of a HMWKText */
struct Token {
  //! Constructor
  Token() : m_type(0), m_id(-1), m_extra("") {
  }
  //! destructor
  ~Token() {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Token const &tkn) {
    switch(tkn.m_type) {
    case 0: // main text
      break;
    case 1:
      o << "header,";
      break;
    case 2:
      o << "footer,";
      break;
    case 3:
      o << "footnote[frame],";
      break;
    case 4:
      o << "textbox,";
      break;
    case 6:
      o << "picture,";
      break;
    case 8:
      o << "basicGraphic,";
      break;
    case 9:
      o << "table,";
      break;
    case 10:
      o << "comments,"; // memo
      break;
    case 11:
      o << "group";
      break;
    default:
      o << "#type=" << tkn.m_type << ",";
      break;
    }
    o << "id=" << std::hex << tkn.m_id << std::dec << ",";
    o << tkn.m_extra;
    return o;
  }
  //! the type
  int m_type;
  //! the identificator
  long m_id;
  //! extra data, mainly for debugging
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the state of a HMWKText
struct State {
  //! constructor
  State() : m_version(-1), m_IdTextMaps(), m_IdTypeMaps(), m_tokenIdList(), m_sectionList(), m_numPages(-1), m_actualPage(0),
    m_headerId(0), m_footerId(0) {
  }

  //! the file version
  mutable int m_version;
  //! the map of id -> text zone
  std::multimap<long, shared_ptr<HMWKZone> > m_IdTextMaps;
  //! the zone frame type if known
  std::map<long,int> m_IdTypeMaps;
  //! the token id list
  std::vector<long> m_tokenIdList;
  //! the list of section
  std::vector<Section> m_sectionList;
  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;
  /** the header text zone id or 0*/
  long m_headerId;
  /** the footer text zone id or 0*/
  long m_footerId;
};
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
HMWKText::HMWKText(HMWKParser &parser) :
  m_parserState(parser.getParserState()), m_state(new HMWKTextInternal::State), m_mainParser(&parser)
{
}

HMWKText::~HMWKText()
{
}

int HMWKText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}

int HMWKText::numPages() const
{
  return m_state->m_numPages;
}

void HMWKText::getHeaderFooterId(long &headerId, long &footerId) const
{
  headerId=m_state->m_headerId;
  footerId=m_state->m_footerId;
}

std::vector<long> const &HMWKText::getTokenIdList() const
{
  return m_state->m_tokenIdList;
}

void HMWKText::updateTextZoneTypes(std::map<long,int> const &idTypeMap)
{
  m_state->m_IdTypeMaps=idTypeMap;
  if (m_state->m_headerId)
    m_state->m_IdTypeMaps[m_state->m_headerId]=1;
  if (m_state->m_footerId)
    m_state->m_IdTypeMaps[m_state->m_footerId]=1;
  std::multimap<long, shared_ptr<HMWKZone> >::iterator tIt;
  int numUnkns=0;
  for (tIt=m_state->m_IdTextMaps.begin(); tIt!=m_state->m_IdTextMaps.end(); ++tIt) {
    if (m_state->m_IdTypeMaps.find(tIt->first)!=m_state->m_IdTypeMaps.end())
      continue;
    m_state->m_IdTypeMaps[tIt->first]=0;
    numUnkns++;
  }
  if (numUnkns!=1) {
    MWAW_DEBUG_MSG(("HMWKText::updateTextZoneTypes: find unexpected number of unknown zone %d\n", numUnkns));
  }
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
//     Text
////////////////////////////////////////////////////////////
bool HMWKText::readTextZone(shared_ptr<HMWKZone> zone)
{
  if (!zone || !zone->valid()) {
    MWAW_DEBUG_MSG(("HMWKText::readTextZone: called without any zone\n"));
    return false;
  }
  m_state->m_IdTextMaps.insert
  (std::multimap<long, shared_ptr<HMWKZone> >::value_type(zone->m_id, zone));
  long dataSz = zone->length();
  MWAWInputStreamPtr input = zone->m_input;
  input->seek(zone->begin(), WPX_SEEK_SET);

  int actPage = 1, actCol = 0, numCol=1;
  long val, pos;
  while (!input->atEOS()) {
    pos = input->tell();
    val = (long) input->readULong(1);
    if (val == 0 && input->atEOS()) break;
    if (val != 1 || input->readLong(1) != 0)
      break;
    int type = (int) input->readLong(2);
    bool done=false;
    switch(type) {
    case 2: { // ruler
      HMWKTextInternal::Paragraph para;
      done=readParagraph(*zone,para);
      if (para.m_addPageBreak)
        actPage++;
      break;
    }
    case 3: { // token
      HMWKTextInternal::Token token;
      done=readToken(*zone,token);
      if (done)
        m_state->m_tokenIdList.push_back(token.m_id);
      break;
    }
    case 4:
      actPage++;
      break;
    default:
      break;
    }

    if (!done) {
      input->seek(pos+4, WPX_SEEK_SET);
      long sz = (long) input->readULong(2);
      if (pos+6+sz > dataSz)
        break;
      input->seek(pos+6+sz, WPX_SEEK_SET);
    }

    bool ok=true;
    while (!input->atEOS()) {
      int c=(int) input->readLong(2);
      if (c==0x100) {
        input->seek(-2, WPX_SEEK_CUR);
        break;
      }
      if (c==0 && input->atEOS())
        break;
      if (c==0) {
        ok = false;
        break;
      }
      if (c==2) {
        if (actCol < numCol-1 && numCol > 1)
          actCol++;
        else {
          actCol = 0;
          actPage++;
        }
      } else if (c==3)
        actPage++;
    }
    if (!ok)
      break;
  }
  if (actPage > m_state->m_numPages)
    m_state->m_numPages = actPage;
  return true;
}

bool HMWKText::sendText(long id, long subId)
{
  std::multimap<long, shared_ptr<HMWKZone> >::iterator tIt
    =m_state->m_IdTextMaps.lower_bound(id);
  if (tIt == m_state->m_IdTextMaps.end() || tIt->first != id) {
    MWAW_DEBUG_MSG(("HMWKText::sendText: can not find the text zone\n"));
    return false;
  }
  while (tIt != m_state->m_IdTextMaps.end() && tIt->first == id) {
    shared_ptr<HMWKZone> zone = (tIt++)->second;
    if (!zone || zone->m_subId != subId) continue;
    sendText(*zone);
    return true;
  }
  MWAW_DEBUG_MSG(("HMWKText::sendText: can not find the text zone\n"));
  return false;
}

bool HMWKText::sendMainText()
{
  std::map<long, int>::iterator tIt=m_state->m_IdTypeMaps.begin();
  for ( ; tIt!=m_state->m_IdTypeMaps.end(); ++tIt) {
    if (tIt->second != 0)
      continue;
    sendText(tIt->first);
    return true;
  }
  MWAW_DEBUG_MSG(("HMWKText::sendText: can not find the main zone\n"));
  return false;
}

bool HMWKText::sendText(HMWKZone &zone)
{
  if (!zone.valid()) {
    MWAW_DEBUG_MSG(("HMWKText::sendText: called without any zone\n"));
    return false;
  }
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("HMWKText::sendText: can not find a listener\n"));
    return false;
  }

  long dataSz = zone.length();
  MWAWInputStreamPtr input = zone.m_input;
  libmwaw::DebugFile &asciiFile = zone.ascii();
  libmwaw::DebugStream f;
  zone.m_parsed = true;

  long pos = zone.begin();
  input->seek(pos, WPX_SEEK_SET);
  f << "PTR=" << std::hex << zone.fileBeginPos() << std::dec << ",";
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  bool isMain = false;
  if (m_state->m_IdTypeMaps.find(zone.m_id)!= m_state->m_IdTypeMaps.end())
    isMain = m_state->m_IdTypeMaps.find(zone.m_id)->second == 0;
  int actPage = 1, actCol = 0, numCol=1, actSection = 1;
  float width = float(72.0*m_mainParser->getPageWidth());

  if (isMain)
    m_mainParser->newPage(1);
  if (isMain && !m_state->m_sectionList.size()) {
    MWAW_DEBUG_MSG(("HMWKText::sendText: can not find section 0\n"));
  } else if (isMain) {
    HMWKTextInternal::Section sec = m_state->m_sectionList[0];
    if (sec.m_numCols >= 1 && sec.m_colWidth.size() > 0) {
      if (listener->isSectionOpened())
        listener->closeSection();
      listener->openSection(sec.getSection());
      numCol = listener->getSection().numColumns();
    }
  }

  long val;
  while (!input->atEOS()) {
    pos = input->tell();
    f.str("");
    f << zone.name()<< ":";
    val = (long) input->readULong(1);
    if (val == 0 && input->atEOS()) break;
    if (val != 1 || input->readLong(1) != 0) {
      f << "###";
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
      break;
    }
    int type = (int) input->readLong(2);
    bool done=false;
    switch(type) {
    case 1: {
      f << "font,";
      MWAWFont font(-1,-1);
      if (!readFont(zone,font))
        break;
      done = true;
      listener->setFont(font);
      break;
    }
    case 2: { // ruler
      f << "ruler,";
      HMWKTextInternal::Paragraph para;
      if (!readParagraph(zone,para))
        break;
      if (para.m_addPageBreak) {
        m_mainParser->newPage(++actPage);
        actCol = 0;
      }
      setProperty(para, width);
      done=true;
      break;
    }
    case 3: { // footnote, attachment
      f << "token,";
      HMWKTextInternal::Token token;
      done=readToken(zone,token);
      if (!done) break;
      switch(token.m_type) {
      case 3:
      case 4:
      case 6:
      case 8:
      case 9:
      case 10:
        m_mainParser->sendZone(token.m_id);
        break;
      case 11: // group: implement me
        break;
      default:
        MWAW_DEBUG_MSG(("HMWKText::sendText: do not send how to send token with type: %d\n", token.m_type));
        break;
      }
      break;
    }
    case 4:
      f << "section[new],";
      if (!isMain) {
        MWAW_DEBUG_MSG(("HMWKText::sendText: find section in auxilliary block\n"));
        break;
      }
      if (size_t(actSection) >= m_state->m_sectionList.size()) {
        MWAW_DEBUG_MSG(("HMWKText::sendText: can not find section %d\n", actSection));
        break;
      } else {
        HMWKTextInternal::Section sec = m_state->m_sectionList[size_t(actSection++)];
        actCol = 0;
        if (listener->isSectionOpened())
          listener->closeSection();
        m_mainParser->newPage(++actPage);
        listener->openSection(sec.getSection());
        numCol = listener->getSection().numColumns();
      }
      break;
    case 6: // follow by id?
      f << "toc,";
      break;
    case 7:
      f << "bookmark,";
      break;
    case 9: // follow by id?
      f << "rubi,";
      break;
    case 11: // follow by id?
      f << "endToc,";
      break;
    case 12: // follow by id?
      f << "endBookmark,";
      break;
    case 14:
      f << "endRubi,";
      break;
    default:
      break;
    }

    bool ok=true;
    if (!done) {
      input->seek(pos+4, WPX_SEEK_SET);
      long sz = (long) input->readULong(2);
      ok = pos+6+sz <= dataSz;
      if (!ok)
        f << "###";
      else
        input->seek(pos+6+sz, WPX_SEEK_SET);

      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
    }
    if (!ok) {
      MWAW_DEBUG_MSG(("HMWKText::sendText: can not read a zone\n"));
      break;
    }

    pos = input->tell();
    f.str("");
    f << zone.name()<< ":text,";
    while (!input->atEOS()) {
      int c=(int) input->readULong(2);
      if (c==0x100) {
        input->seek(-2, WPX_SEEK_CUR);
        break;
      }
      if (c==0 && input->atEOS())
        break;
      if (c==0) {
        ok = false;
        f << "###";
        break;
      }
      switch(c) {
      case 0x1000:
        f << "[pgNum]";
        listener->insertField(MWAWField(MWAWField::PageNumber));
        break;
      case 0x1001:
        f << "[pgCount]";
        listener->insertField(MWAWField(MWAWField::PageCount));
        break;
      case 0x1002: {
        f << "[date]";
        MWAWField field(MWAWField::Date);
        field.m_DTFormat="%A, %b %d, %Y";
        listener->insertField(field);
        break;
      }
      case 0x1003: {
        f << "[time]";
        MWAWField field(MWAWField::Time);
        field.m_DTFormat="%I:%M %p";
        listener->insertField(field);
        break;
      }
      case 0x1004:
        f << "[title]";
        listener->insertField(MWAWField(MWAWField::Title));
        break;
      case 0x1005: {
        std::stringstream s;
        f << "[section]";
        s << actSection;
        listener->insertUnicodeString(s.str().c_str());
        break;
      }
      case 2:
        f << "[colBreak]";
        if (!isMain) {
          MWAW_DEBUG_MSG(("HMWKText::sendText: find column break in auxilliary block\n"));
          break;
        }
        if (actCol < numCol-1 && numCol > 1) {
          listener->insertBreak(MWAWContentListener::ColumnBreak);
          actCol++;
        } else {
          actCol = 0;
          m_mainParser->newPage(++actPage);
        }
        break;
      case 3:
        f << "[pageBreak]";
        if (isMain) {
          m_mainParser->newPage(++actPage);
          actCol = 0;
        }
        break;
      case 9:
        f << char(c);
        listener->insertTab();
        break;
      case 0xd:
        f << char(c);
        listener->insertEOL();
        break;
      default: {
        if (c <= 0x1f || c >= 0x100) {
          f << "###" << std::hex << c << std::dec;
          MWAW_DEBUG_MSG(("HMWKText::sendText: find a odd char %x\n", c));
          break;
        }
        f << char(c);
        listener->insertCharacter((unsigned char)c, input);
        break;
      }
      }
    }
    if (input->tell() != pos) {
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
    }
    if (!ok) {
      MWAW_DEBUG_MSG(("HMWKText::sendText: can not read a text zone\n"));
      break;
    }
  }
  // FIXME: remove this when the frame/textbox are sent normally
  listener->insertEOL();
  return true;
}

////////////////////////////////////////////////////////////
//     Fonts
////////////////////////////////////////////////////////////

// a font in the text zone
bool HMWKText::readFont(HMWKZone &zone, MWAWFont &font)
{
  font = MWAWFont(-1,-1);

  MWAWInputStreamPtr input = zone.m_input;
  long pos = input->tell();
  if (pos+30 > zone.length()) {
    MWAW_DEBUG_MSG(("HMWKText::readFont: the zone is too short\n"));
    return false;
  }

  libmwaw::DebugStream f;
  int val = (int) input->readLong(2);
  if (val!=28) {
    MWAW_DEBUG_MSG(("HMWKText::readFont: the data size seems bad\n"));
    f << "##dataSz=" << val << ",";
  }
  font.setId((int) input->readLong(2));
  val = (int) input->readLong(2);
  if (val) f << "#f1=" << val << ",";
  font.setSize(float(input->readLong(4))/65536.f);
  float expand = float(input->readLong(4))/65536.f;
  if (expand < 0 || expand > 0)
    font.setDeltaLetterSpacing(expand*font.size());
  float xScale = float(input->readLong(4))/65536.f;
  if (xScale < 1.0 || xScale > 1.0)
    font.setTexteWidthScaling(xScale);

  int flag =(int) input->readULong(2);
  uint32_t flags=0;
  if (flag&1) {
    font.setUnderlineStyle(MWAWFont::Line::Simple);
    font.setUnderlineType(MWAWFont::Line::Double);
  }
  if (flag&2)
    font.setUnderlineStyle(MWAWFont::Line::Dot);
  if (flag&4) {
    font.setUnderlineStyle(MWAWFont::Line::Dot);
    font.setUnderlineWidth(2.0);
  }
  if (flag&8)
    font.setUnderlineStyle(MWAWFont::Line::Dash);
  if (flag&0x10)
    font.setStrikeOutStyle(MWAWFont::Line::Simple);
  if (flag&0x20) {
    font.setStrikeOutStyle(MWAWFont::Line::Simple);
    font.setStrikeOutType(MWAWFont::Line::Double);
  }
  if (flag&0xFFC0)
    f << "#flag0=" << std::hex << (flag&0xFFF2) << std::dec << ",";
  flag =(int) input->readULong(2);
  if (flag&1) flags |= MWAWFont::boldBit;
  if (flag&0x2) flags |= MWAWFont::italicBit;
  if (flag&0x4) flags |= MWAWFont::outlineBit;
  if (flag&0x8) flags |= MWAWFont::shadowBit;
  if (flag&0x10) flags |= MWAWFont::reverseVideoBit;
  if (flag&0x20) font.set(MWAWFont::Script::super100());
  if (flag&0x40) font.set(MWAWFont::Script::sub100());
  if (flag&0x80) {
    if (flag&0x20)
      font.set(MWAWFont::Script(48,WPX_PERCENT,58));
    else if (flag&0x40)
      font.set(MWAWFont::Script(16,WPX_PERCENT,58));
    else
      font.set(MWAWFont::Script::super());
  }
  if (flag&0x100) {
    font.setOverlineStyle(MWAWFont::Line::Dot);
    font.setOverlineWidth(2.0);
  }
  if (flag&0x200) flags |= MWAWFont::boxedBit;
  if (flag&0x400) flags |= MWAWFont::boxedRoundedBit;
  if (flag&0x800) {
    font.setUnderlineStyle(MWAWFont::Line::Simple);
    font.setUnderlineWidth(0.5);
  }
  if (flag&0x1000) font.setUnderlineStyle(MWAWFont::Line::Simple);
  if (flag&0x2000) {
    font.setUnderlineStyle(MWAWFont::Line::Simple);
    font.setUnderlineWidth(2.0);
  }
  if (flag&0x4000) {
    font.setUnderlineStyle(MWAWFont::Line::Simple);
    font.setUnderlineWidth(3.0);
  }
  if (flag&0x8000) {
    font.setUnderlineStyle(MWAWFont::Line::Simple);
    font.setUnderlineType(MWAWFont::Line::Double);
    font.setUnderlineWidth(0.5);
  }
  int color = (int) input->readLong(2);
  MWAWColor col;
  if (color && m_mainParser->getColor(color, 1, col))
    font.setColor(col);
  else if (color)
    f << "##fColor=" << color << ",";
  val = (int) input->readLong(2);
  if (val) f << "#unk=" << val << ",";
  color = (int) input->readLong(2);
  int pattern = (int) input->readLong(2);
  if ((color || pattern) && m_mainParser->getColor(color, pattern, col))
    font.setBackgroundColor(col);
  else if (color || pattern)
    f << "#backColor=" << color << ", #pattern=" << pattern << ",";
  font.setFlags(flags);
  font.m_extra = f.str();

  static bool first=true;
  f.str("");
  if (first) {
    f << "Entries(FontDef):";
    first = false;
  } else
    f << "FontDef:";
  f << font.getDebugString(m_parserState->m_fontConverter) << ",";

  zone.ascii().addPos(pos-4);
  zone.ascii().addNote(f.str().c_str());

  input->seek(pos+30, WPX_SEEK_SET);
  return true;
}

// the list of fonts
bool HMWKText::readFontNames(shared_ptr<HMWKZone> zone)
{
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWKText::readFontNames: called without any zone\n"));
    return false;
  }
  long dataSz = zone->length();
  if (dataSz < 2) {
    MWAW_DEBUG_MSG(("HMWKText::readFontNames: the zone seems too short\n"));
    return false;
  }
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->ascii();
  libmwaw::DebugStream f;
  zone->m_parsed = true;

  f << zone->name() << ":PTR=" << std::hex << zone->fileBeginPos() << std::dec << ",";

  long pos = zone->begin();
  input->seek(pos, WPX_SEEK_SET);
  int val;
  int N = (int) input->readLong(2);
  f << "N=" << N << ",";
  long expectedSz = N*68+2;
  if (expectedSz != dataSz && expectedSz+1 != dataSz) {
    MWAW_DEBUG_MSG(("HMWKText::readFontNames: the zone size seems odd\n"));
    return false;
  }
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    f << zone->name() << "-" << i << ":";
    int fId = (int) input->readLong(2);
    f << "fId=" << fId << ",";
    val = (int) input->readLong(2);
    if (val != fId)
      f << "#fId2=" << val << ",";
    int fSz = (int) input->readULong(1);
    if (fSz+5 > 68) {
      f << "###fSz";
      MWAW_DEBUG_MSG(("HMWKText::readFontNames: can not read a font\n"));
    } else {
      std::string name("");
      for (int c = 0; c < fSz; c++)
        name += (char) input->readULong(1);
      f << name;
      m_parserState->m_fontConverter->setCorrespondance(fId, name);
    }
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos+68, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
//     Style
////////////////////////////////////////////////////////////
bool HMWKText::readStyles(shared_ptr<HMWKZone> zone)
{
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWKText::readStyles: called without any zone\n"));
    return false;
  }

  long dataSz = zone->length();
  if (dataSz < 24) {
    MWAW_DEBUG_MSG(("HMWKText::readStyles: the zone seems too short\n"));
    return false;
  }
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->ascii();
  libmwaw::DebugStream f;
  zone->m_parsed = true;

  f << zone->name() << ":PTR=" << std::hex << zone->fileBeginPos() << std::dec << ",";

  long pos = zone->begin();
  input->seek(pos, WPX_SEEK_SET);
  int val;
  long fieldSz = 636;
  int N = (int) input->readULong(2);
  f << "N=" << N << ",";
  long expectedSz = N*fieldSz+2;
  if (expectedSz != dataSz && expectedSz+1 != dataSz) {
    MWAW_DEBUG_MSG(("HMWKText::readStyles: the zone size seems odd\n"));
    return false;
  }
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    f.str("");
    f << zone->name() << "-" << i << ":";
    pos = input->tell();
    val = (int) input->readULong(2);
    if (val != i) f << "#id=" << val << ",";

    // f0=c2|c6, f2=0|44, f3=1|14|15|16: fontId?
    for (int j=0; j < 4; j++) {
      val = (int) input->readULong(1);
      if (val)
        f << "f" << j << "=" << std::hex << val << std::dec << ",";
    }
    /* g1=9|a|c|e|12|18: size ?, g5=1, g8=0|1, g25=0|18|30|48, g31=1, g35=0|1 */
    for (int j=0; j < 37; j++) {
      val = (int) input->readULong(2);
      if (val)
        f << "g" << j << "=" << val  << ",";
    }
    for (int j=0; j < 4; j++) { // b,b,b,0
      val = (int) input->readULong(1);
      if ((j < 3 && val != 0xb) || (j==3 && val))
        f << "h" << j << "=" << val  << ",";
    }

    for (int j=0; j < 17; j++) { // always 0
      val = (int) input->readULong(2);
      if (val)
        f << "l" << j << "=" << val  << ",";
    }
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    long pos2 = input->tell();
    f.str("");
    f << "Style-" << i << "[B]:";
    // checkme probably f15=numTabs  ..
    for (int j = 0; j < 50; j++) {
      val = (int) input->readULong(2);
      if ((j < 5 && val != 1) || (j >= 5 && val))
        f << "f" << j << "=" << val  << ",";
    }
    for (int j = 0; j < 50; j++) {
      val = (int) input->readULong(2);
      if (val)
        f << "g" << j << "=" << val  << ",";
    }
    for (int j = 0; j < 100; j++) {
      val = (int) input->readULong(2);
      if (val)
        f << "h" << j << "=" << val  << ",";
    }
    for (int j = 0; j < 41; j++) {
      val = (int) input->readULong(2);
      if (val)
        f << "l" << j << "=" << val  << ",";
    }
    asciiFile.addPos(pos2);
    asciiFile.addNote(f.str().c_str());

    pos2 = input->tell();
    f.str("");
    f << "Style-" << i << "[C]:";
    val = (int) input->readLong(2);
    if (val != -1) f << "unkn=" << val << ",";
    val  = (int) input->readLong(2);
    if (val != i) f << "#id" << val << ",";
    int fSz = (int) input->readULong(1);
    if (input->tell()+fSz > pos+fieldSz) {
      MWAW_DEBUG_MSG(("HMWKText::readStyles: can not read styleName\n"));
      f << "###";
    } else {
      std::string name("");
      for (int j = 0; j < fSz; j++)
        name +=(char) input->readULong(1);
      f << name;
    }
    asciiFile.addPos(pos2);
    asciiFile.addNote(f.str().c_str());
    if (input->tell() != pos+fieldSz)
      asciiFile.addDelimiter(input->tell(),'|');

    input->seek(pos+fieldSz, WPX_SEEK_SET);
  }

  if (!input->atEOS()) {
    asciiFile.addPos(input->tell());
    asciiFile.addNote("_");
  }
  return true;
}

////////////////////////////////////////////////////////////
//     Paragraph
////////////////////////////////////////////////////////////
void HMWKText::setProperty(HMWKTextInternal::Paragraph const &para, float)
{
  if (!m_parserState->m_listener) return;
  m_parserState->m_listener->setParagraph(para);
}

bool HMWKText::readParagraph(HMWKZone &zone, HMWKTextInternal::Paragraph &para)
{
  para = HMWKTextInternal::Paragraph();

  MWAWInputStreamPtr input = zone.m_input;
  long pos = input->tell();
  long dataSz = zone.length();
  if (pos+102 > dataSz) {
    MWAW_DEBUG_MSG(("HMWKText::readParagraph: the zone is too short\n"));
    return false;
  }

  libmwaw::DebugStream f;
  libmwaw::DebugFile &asciiFile = zone.ascii();
  int val = (int) input->readLong(2);
  if (val!=100) {
    MWAW_DEBUG_MSG(("HMWKText::readParagraph: the data size seems bad\n"));
    f << "##dataSz=" << val << ",";
  }
  int flags = (int) input->readULong(1);
  if (flags&0x80)
    para.m_breakStatus = para.m_breakStatus.get()|MWAWParagraph::NoBreakWithNextBit;
  if (flags&0x40)
    para.m_breakStatus = para.m_breakStatus.get()|MWAWParagraph::NoBreakBit;
  if (flags&0x2)
    para.m_addPageBreak = true;
  if (flags&0x4)
    f << "linebreakByWord,";

  if (flags & 0x39) f << "#fl=" << std::hex << (flags&0x39) << std::dec << ",";

  val = (int) input->readLong(2);
  if (val) f << "#f0=" << val << ",";
  val = (int) input->readULong(2);
  switch(val&3) {
  case 0:
    para.m_justify = MWAWParagraph::JustificationLeft;
    break;
  case 1:
    para.m_justify = MWAWParagraph::JustificationRight;
    break;
  case 2:
    para.m_justify = MWAWParagraph::JustificationCenter;
    break;
  case 3:
    para.m_justify = MWAWParagraph::JustificationFull;
    break;
  default:
    break;
  }
  if (val&0xFFFC) f << "#f1=" << val << ",";
  val = (int) input->readLong(1);
  if (val) f << "#f2=" << val << ",";
  para.m_type = (int) input->readLong(2);

  float dim[3];
  for (int i = 0; i < 3; i++)
    dim[i] = float(input->readLong(4))/65536.0f;
  para.m_marginsUnit = WPX_POINT;
  para.m_margins[0]=dim[1];
  para.m_margins[1]=dim[0];
  para.m_margins[2]=dim[2]; // ie. distance to rigth border - ...

  for (int i = 0; i < 3; i++)
    para.m_spacings[i] = float(input->readLong(4))/65536.0f;
  int spacingsUnit[3]; // 1=mm, ..., 4=pt, b=line
  for (int i = 0; i < 3; i++)
    spacingsUnit[i] = (int) input->readULong(1);
  if (spacingsUnit[0]==0xb)
    para.m_spacingsInterlineUnit = WPX_PERCENT;
  else
    para.m_spacingsInterlineUnit = WPX_POINT;
  for (int i = 1; i < 3; i++) // convert point|line -> inches
    para.m_spacings[i]= ((spacingsUnit[i]==0xb) ? 12.0 : 1.0)*(para.m_spacings[i].get())/72.0;

  val = (int) input->readLong(1);
  if (val) f << "#f3=" << val << ",";
  for (int i = 0;  i< 2; i++) { // one time f4=8000
    val = (int) input->readULong(2);
    if (val) f << "#f" << i+4 << "=" << std::hex << val << std::dec << ",";
  }
  // the borders
  char const *(wh[5]) = { "T", "L", "B", "R", "VSep" };
  MWAWBorder borders[5];
  for (int d=0; d < 5; d++)
    borders[d].m_width = float(input->readLong(4))/65536.f;
  for (int d=0; d < 5; d++) {
    val = (int) input->readULong(1);
    switch (val) {
    case 0: // normal
      break;
    case 1:
      borders[d].m_type = MWAWBorder::Double;
      break;
    case 2:
      borders[d].m_type = MWAWBorder::Double;
      f << "bord" << wh[d] << "[ext=2],";
      break;
    case 3:
      borders[d].m_type = MWAWBorder::Double;
      f << "bord" << wh[d] << "[int=2],";
      break;
    default:
      f << "#bord" << wh[d] << "[style=" << val << "],";
      break;
    }
  }
  int color[5], pattern[5];
  for (int d=0; d < 5; d++)
    color[d] = (int) input->readULong(1);
  for (int d=0; d < 5; d++)
    pattern[d] = (int) input->readULong(2);
  for (int d=0; d < 5; d++) {
    if (!color[d] && !pattern[d])
      continue;
    MWAWColor col;
    if (m_mainParser->getColor(color[d], pattern[d], col))
      borders[d].m_color = col;
    else
      f << "#bord" << wh[d] << "[col=" << color[d] << ",pat=" << pattern[d] << "],";
  }
  // update the paragraph
  para.resizeBorders(6);
  libmwaw::Position const (which[5]) = {
    libmwaw::Top, libmwaw::Left, libmwaw::Bottom, libmwaw::Right,
    libmwaw::VMiddle
  };
  for (int d=0; d < 5; d++) {
    if (borders[d].m_width <= 0)
      continue;
    para.m_borders[which[d]]=borders[d];
  }
  val = (int) input->readLong(1);
  if (val) f << "#f6=" << val << ",";
  double bMargins[4]= {0,0,0,0};
  for (int d = 0; d < 4; d++) {
    bMargins[d] =  double(input->readLong(4))/256./65536./72.;
    if (bMargins[d] > 0 || bMargins[d] < 0)
      f << "bordMarg" << wh[d] << "=" << bMargins[d] << ",";
  }
  int nTabs = (int) input->readULong(1);
  if (pos+102+nTabs*12 > dataSz) {
    MWAW_DEBUG_MSG(("HMWKText::readParagraph: can not read numbers of tab\n"));
    return false;
  }
  val = (int) input->readULong(2); // always 0
  if (val) f << "#h0=" << val << ",";
  para.m_extra=f.str();
  static bool first=true;
  f.str("");
  if (first) {
    f << "Entries(Ruler):";
    first = false;
  } else
    f << "Ruler:";
  f << para;

  asciiFile.addPos(pos-4);
  asciiFile.addNote(f.str().c_str());
  for (int i = 0; i < nTabs; i++) {
    pos = input->tell();
    f.str("");
    f << "Ruler[Tabs-" << i << "]:";

    MWAWTabStop tab;
    val = (int)input->readULong(1);
    switch(val) {
    case 0:
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
    case 4:
      tab.m_alignment = MWAWTabStop::BAR;
      break;
    default:
      f << "#type=" << val << ",";
      break;
    }
    val = (int)input->readULong(1);
    if (val) f << "barType=" << val << ",";
    val = (int)input->readULong(2);
    if (val) {
      int unicode= m_parserState->m_fontConverter->unicode(3, (unsigned char) val);
      if (unicode==-1)
        tab.m_decimalCharacter = uint16_t(val);
      else
        tab.m_decimalCharacter = uint16_t(unicode);
    }
    val = (int) input->readULong(2);
    if (val) {
      int unicode= m_parserState->m_fontConverter->unicode(3, (unsigned char) val);
      if (unicode==-1)
        tab.m_leaderCharacter = uint16_t(val);
      else
        tab.m_leaderCharacter = uint16_t(unicode);
    }
    val = (int)input->readULong(2); // 0|73|74|a044|f170|f1e0|f590
    if (val) f << "f0=" << std::hex << val << std::dec << ",";

    tab.m_position = float(input->readLong(4))/65536.f/72.f;
    para.m_tabs->push_back(tab);
    f << tab;

    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos+12, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
//     the token
////////////////////////////////////////////////////////////
bool HMWKText::readToken(HMWKZone &zone, HMWKTextInternal::Token &token)
{
  token = HMWKTextInternal::Token();

  MWAWInputStreamPtr input = zone.m_input;
  long pos = input->tell();
  long dataSz = zone.length();
  if (pos+10 > dataSz) {
    MWAW_DEBUG_MSG(("HMWKText::readToken: the zone is too short\n"));
    return false;
  }

  libmwaw::DebugStream f;
  libmwaw::DebugFile &asciiFile = zone.ascii();
  int val = (int) input->readLong(2);
  if (val!=8) {
    MWAW_DEBUG_MSG(("HMWKText::readToken: the data size seems bad\n"));
    f << "##dataSz=" << val << ",";
  }

  token.m_type = (int) input->readLong(1);
  val = (int) input->readLong(1);
  if (val) f << "f0=" << val << ",";
  val = (int) input->readLong(2);
  if (val) f << "f1=" << val << ",";
  token.m_id = (long)  input->readULong(4);
  token.m_extra = f.str();
  f.str("");
  static bool first=true;
  f.str("");
  if (first) {
    f << "Entries(Token):";
    first = false;
  } else
    f << "Token:";
  f << token;
  asciiFile.addPos(pos-4);
  asciiFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
//     the sections
////////////////////////////////////////////////////////////
bool HMWKText::readSections(shared_ptr<HMWKZone> zone)
{
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWKText::readSections: called without any zone\n"));
    return false;
  }
  long dataSz = zone->length();
  if (dataSz < 160) {
    MWAW_DEBUG_MSG(("HMWKText::readSections: the zone seems too short\n"));
    return false;
  }

  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->ascii();
  libmwaw::DebugStream f;
  zone->m_parsed = true;

  HMWKTextInternal::Section sec;
  long pos=0;
  input->seek(pos, WPX_SEEK_SET);
  long val = input->readLong(2);
  if (val != 1) // always 1
    f << "f0=" << val << ",";
  int numColumns = (int) input->readLong(2);
  if (numColumns <= 0 || numColumns > 8) {
    MWAW_DEBUG_MSG(("HMWKText::readSections: numColumns seems bad\n"));
    f << "###nCols=" << numColumns << ",";
    numColumns = 1;
  } else
    sec.m_numCols=numColumns;
  val = (long) input->readLong(1);
  bool diffWidth=val==0;
  if (val==1)
    f << "sameWidth,";
  else if (val)
    f << "#width=" << val << ",";
  val = (long) input->readLong(1); // always 0
  if (val) f << "f1=" << val << ",";
  for (int i = 0; i < 19; i++) { // always 0
    val = (long) input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }

  if (diffWidth) {
    for (int i = 0; i < numColumns; i++) {
      sec.m_colWidth.push_back(double(input->readLong(4))/65536);
      sec.m_colSep.push_back(double(input->readLong(4))/65536);
    }
  } else {
    sec.m_colWidth.push_back(double(input->readLong(4))/65536);
    sec.m_colSep.push_back(double(input->readLong(4))/65536);
  }
  sec.m_extra = f.str();

  f.str("");
  f << zone->name() << "(A):PTR=" << std::hex << zone->fileBeginPos() << std::dec << "," << sec;

  asciiFile.addDelimiter(input->tell(),'|');
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  input->seek(pos+0x6c, WPX_SEEK_SET);

  pos = input->tell();
  f.str("");
  f << zone->name() << "(B):";
  for (int i = 0; i < 4; i++) {
    long id = input->readLong(4);
    if (!id) continue;
    if (i < 2) {
      if (!m_state->m_headerId)
        m_state->m_headerId = id;
      else if (m_state->m_headerId != id) {
        MWAW_DEBUG_MSG(("HMWKText::readSections: headerid is already defined\n"));
        f << "###";
      }
      f << "headerId=" << std::hex << id << std::dec << ",";
    } else {
      if (!m_state->m_footerId)
        m_state->m_footerId = id;
      else if (m_state->m_footerId != id) {
        MWAW_DEBUG_MSG(("HMWKText::readSections: footerid is already defined\n"));
        f << "###";
      }
      f << "footerId=" << std::hex << id << std::dec << ",";
    }
  }
  for (int i = 0; i < 8; i++) {
    val = input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  asciiFile.addDelimiter(input->tell(),'|');
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  if (zone->m_id >= 0) {
    if (zone->m_id <= int(m_state->m_sectionList.size()))
      m_state->m_sectionList.resize(size_t(zone->m_id)+1);
    m_state->m_sectionList[size_t(zone->m_id)]=sec;
  }
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

//! send data to the listener

void HMWKText::flushExtra()
{
  if (!m_parserState->m_listener) return;
  std::multimap<long, shared_ptr<HMWKZone> >::iterator tIt
    =m_state->m_IdTextMaps.begin();
  for ( ; tIt!=m_state->m_IdTextMaps.end(); ++tIt) {
    if (!tIt->second) continue;
    HMWKZone &zone = *tIt->second;
    if (zone.m_parsed) continue;
    sendText(zone);
  }
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
