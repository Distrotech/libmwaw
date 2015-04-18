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
#include <string>

#include <librevenge/librevenge.h>

#include "MWAWCell.hxx"
#include "MWAWTextListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWList.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSubDocument.hxx"

#include "MindWrtParser.hxx"

/** Internal: the structures of a MindWrtParser */
namespace MindWrtParserInternal
{
////////////////////////////////////////
//! Internal: a field of a MindWrtParser
struct Field {
  //! constructor
  Field(MWAWField::Type type=MWAWField::None) : m_type(type), m_pos(0,-1), m_extra("")
  {
  }
  //! return true is the field is defined
  bool ok() const
  {
    return m_type!=MWAWField::None && m_pos[1] > 0;
  }
  MWAWField getField() const
  {
    MWAWField res(m_type);
    if (m_type==MWAWField::Date)
      res.m_DTFormat="%B %d, %Y";
    else if (m_type==MWAWField::Time)
      res.m_DTFormat="%H:%M";
    return res;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Field const &field)
  {
    if (!field.ok()) {
      o << "undef,";
      return o;
    }
    switch (field.m_type) {
    case MWAWField::Date:
      o << "date,";
      break;
    case MWAWField::Time:
      o << "time,";
      break;
    case MWAWField::PageNumber:
      o << "pagenumber,";
      break;
    case MWAWField::Title:
      o << "title,";
      break;
    case MWAWField::None:
    case MWAWField::PageCount:
    case MWAWField::Database:
    default:
      MWAW_DEBUG_MSG(("MindWrtParserInternal::Field: unexpected field type %d\n", int(field.m_type)));
      break;
    }
    o << "line=" << field.m_pos[1] << ", x=" << field.m_pos[0] << ",";
    o << field.m_extra;
    return o;
  }
  //! the field type
  MWAWField::Type m_type;
  //! the field position: x position in point, y position in line (-1: means no field )
  MWAWVec2i m_pos;
  //! extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the list properties of a MindWrtParser
struct ListProperties {
  //! constructor
  ListProperties() : m_startListIndex(1), m_headingStyle(1), m_headingFullSubLevels(true),
    m_headingListLevels(), m_listLevelsRepeatPos(1), m_showFirstLevel(false), m_useHeadingStyle(false)
  {
  }
  //! updates the heading list
  void updateHeadingList();
  //! returns a list level
  MWAWListLevel getLevel(int level) const;
  //! the first list index
  int m_startListIndex;
  //! the heading labels style
  int m_headingStyle;
  //! true if heading labels show all sub level
  bool m_headingFullSubLevels;
  //! the heading list
  std::vector<MWAWListLevel> m_headingListLevels;
  //! the position used to repeat level
  int m_listLevelsRepeatPos;
  //! true if we show the level 1 label
  bool m_showFirstLevel;
  //! true if we need to use heading style
  bool m_useHeadingStyle;
};

MWAWListLevel ListProperties::getLevel(int level) const
{
  int numLevels=int(m_headingListLevels.size());
  if (level < 0 || numLevels <= 0) {
    MWAW_DEBUG_MSG(("ListProperties::updateHeadingList: can not find any level\n"));
    return MWAWListLevel();
  }

  MWAWListLevel res;
  int numSubLevels = 0;
  if (m_headingFullSubLevels) {
    numSubLevels = level;
    if (!m_showFirstLevel)
      numSubLevels--;
  }
  if (level < numLevels)
    res=m_headingListLevels[size_t(level)];
  else {
    int firstRepeatLevel=m_headingFullSubLevels-1;
    int numLevelsRepeat=numLevels-firstRepeatLevel;
    if (numLevelsRepeat <= 0) {
      numLevelsRepeat = numLevels;
      firstRepeatLevel = 0;
    }
    level = firstRepeatLevel+((level-firstRepeatLevel)%numLevelsRepeat);
    res=m_headingListLevels[size_t(level)];
  }
  if (numSubLevels>0)
    res.m_numBeforeLabels=numSubLevels;
  return res;
}

void  ListProperties::updateHeadingList()
{
  if (m_headingStyle==4) {
    if (!m_headingListLevels.size()) {
      MWAW_DEBUG_MSG(("ListProperties::updateHeadingList: can not find custom list\n"));
    }
    return;
  }
  m_headingListLevels.resize(0);
  if (m_headingStyle < 1 || m_headingStyle > 4) {
    MWAW_DEBUG_MSG(("ListProperties::updateHeadingList: unknown style %d\n", m_headingStyle));
    return;
  }
  MWAWListLevel level;
  if (m_headingStyle==1) { // I. A. 1. a) i)
    m_listLevelsRepeatPos = 3;
    level.m_suffix = ".";
    level.m_type=MWAWListLevel::UPPER_ROMAN;
    m_headingListLevels.push_back(level);
    level.m_type=MWAWListLevel::UPPER_ALPHA;
    m_headingListLevels.push_back(level);
    level.m_type=MWAWListLevel::DECIMAL;
    m_headingListLevels.push_back(level);
    level.m_suffix = ")";
    level.m_type=MWAWListLevel::LOWER_ALPHA;
    m_headingListLevels.push_back(level);
    level.m_type=MWAWListLevel::LOWER_ROMAN;
    m_headingListLevels.push_back(level);
  }
  else if (m_headingStyle==2) {   // I. A. 1. a) (1), (a), i)
    m_listLevelsRepeatPos = 4;
    level.m_suffix = ".";
    level.m_type=MWAWListLevel::UPPER_ROMAN;
    m_headingListLevels.push_back(level);
    level.m_type=MWAWListLevel::UPPER_ALPHA;
    m_headingListLevels.push_back(level);
    level.m_type=MWAWListLevel::DECIMAL;
    m_headingListLevels.push_back(level);
    level.m_suffix = ")";
    level.m_type=MWAWListLevel::LOWER_ALPHA;
    m_headingListLevels.push_back(level);
    level.m_prefix = "(";
    level.m_type=MWAWListLevel::DECIMAL;
    m_headingListLevels.push_back(level);
    level.m_type=MWAWListLevel::LOWER_ALPHA;
    m_headingListLevels.push_back(level);
    level.m_prefix = "";
    level.m_type=MWAWListLevel::LOWER_ROMAN;
    m_headingListLevels.push_back(level);
  }
  else {
    m_listLevelsRepeatPos = 2;
    level.m_suffix = ".";
    level.m_type=MWAWListLevel::DECIMAL;
    m_headingListLevels.push_back(level);
    level.m_suffix = ".";
    level.m_type=MWAWListLevel::DECIMAL;
    m_headingListLevels.push_back(level);
  }
}
////////////////////////////////////////
//! Internal: a line information of a MindWrtParser
struct LineInfo {
  LineInfo() : m_entry(), m_type(-1000), m_height(0), m_y(-1), m_page(-1),
    m_paragraph(),  m_specialHeadingInterface(false), m_paragraphSet(false), m_listLevel(0), m_listType(0), m_extra("")
  {
    for (int i = 0; i < 4; i++) m_flags[i] = 0;
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, LineInfo const &line)
  {
    if (line.m_entry.valid())
      o << "textPos=" << std::hex << line.m_entry.begin()
        << "<->" << line.m_entry.end() << std::dec <<  ",";
    switch (line.m_type) {
    case -1000:
      break;
    case 0:
      o << "text,";
      break;
    case -1:
      o << "graphic,";
      break;
    case -2:
      o << "newpage,";
      break;
    // find also -3 will no data ( nop ? )
    default:
      o << "#type=" << line.m_type << ",";
      break;
    }
    if (line.m_height>0) o << "h=" << line.m_height << ",";
    if (line.m_y>=0) o << "y=" << line.m_y << ",";
    if (line.m_page>0) o << "page?=" << line.m_page << ",";
    if (line.m_flags[1] & 0x40) {
      switch (line.m_flags[1]&0x3) {
      case 0:
        o << "left,";
        break;
      case 1:
        o << "center,";
        break;
      case 2:
        o << "right,";
        break;
      case 3:
        o << "full,";
        break;
      default:
        break;
      }
    }
    if (line.m_flags[1] & 0x4) o << "interline[normal],";
    if (line.m_flags[1] & 0x8)
      o << "compressed,";
    o << "list=" << "[";
    if (line.m_listLevel)
      o << "levl=" << line.m_listLevel << ",";
    switch (line.m_listType) {
    case 0:
      o << "Head,";
      break; // Heading
    case 1:
      o << "Unl,";
      break; // unlabelled
    case 2:
      o << "Num,";
      break; // numbered
    case 3:
      o << "Bul,";
      break; // bullet
    default:
      o << "#type=" << line.m_listType << ",";
    }
    o << "],";
    if (line.m_flags[0])
      o << "fl0=" << std::hex << line.m_flags[0] << std::dec << ",";
    if (line.m_flags[1] & 0xb0) {
      if (line.m_flags[1] & 0x90) o << "#";
      o << "fl1=" << std::hex << (line.m_flags[1]& 0xb0) << std::dec << ",";
    }
    for (int i = 2; i < 4; i++) { // [067a][1-4], [0-4]
      if (!line.m_flags[i])
        continue;
      o << "fl" << i << "=" << std::hex << line.m_flags[i] << std::dec << ",";
    }
    if (line.m_extra.length()) o << line.m_extra << ",";
    return o;
  }

  //! the main entry
  MWAWEntry m_entry;
  //! the entry type
  int m_type;
  //! the height
  int m_height;
  //! the y pos
  int m_y;
  //! the page number
  int m_page;
  //! the paragraph
  MWAWParagraph m_paragraph;
  //! true if the paragraph
  bool m_specialHeadingInterface;
  //! true if the paragraph is reset
  bool m_paragraphSet;
  //! the item level
  int m_listLevel;
  //! the item type
  int m_listType;
  //! two flags
  int m_flags[4];
  //! extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: a zone information of a MindWrtParser
struct ZoneInfo {
  ZoneInfo() : m_linesList()
  {
  }

  //! update the line list id
  void updateListId(ListProperties &prop, MWAWListManager &listManager);

  //! the list of line information
  std::vector<LineInfo> m_linesList;
};

void ZoneInfo::updateListId(ListProperties &prop, MWAWListManager &listManager)
{
  size_t numLines=m_linesList.size();
  bool headingStyle=prop.m_useHeadingStyle && prop.m_headingStyle>=1 && prop.m_headingStyle<=4;

  /* we have 4 list, we are almost independent: heading, unlabelled, decimal, bullet */
  MWAWListLevel levelsList[4];
  levelsList[0].m_type = levelsList[1].m_type = MWAWListLevel::NONE;
  levelsList[2].m_type = MWAWListLevel::DECIMAL;
  levelsList[3].m_type = MWAWListLevel::BULLET;
  libmwaw::appendUnicode(0x2022, levelsList[3].m_bullet);

  shared_ptr<MWAWList> listsList[4];
  int maxLevelSet=0;

  for (size_t i = 0; i < numLines; i++) {
    LineInfo &line = m_linesList[i];
    if (line.m_height == 0 ||  // ruler or false graphic
        (line.m_type==-1 && line.m_entry.length()==0)) continue;
    MWAWParagraph &para = line.m_paragraph;
    if (line.m_flags[1] & 0x40) {
      switch (line.m_flags[1]&0x3) {
      case 0:
        para.m_justify = MWAWParagraph::JustificationLeft;
        break;
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
        break;
      }
    }
    int const level = line.m_listLevel+(prop.m_showFirstLevel ? 1 : 0);
    para.m_listLevelIndex=level;
    if (!level || line.m_listType<0 || line.m_listType > 3) continue;

    // create/update the different lists level
    for (int type=0; type < 4; type++) {
      int numLevels = listsList[type] ? listsList[type]->numLevels() : 0;
      int firstL = numLevels+1;
      if (firstL>level && type==line.m_listType) firstL=level;
      for (int l=firstL ; l <= level; l++) {
        bool onlyUpdate = numLevels >= l;
        if (onlyUpdate && type != line.m_listType) continue;
        MWAWListLevel newLevel = (type==0 && headingStyle) ? prop.getLevel(l-1) : levelsList[type];
        if (!onlyUpdate && newLevel.isNumeric())
          newLevel.m_startValue=l==1 ? prop.m_startListIndex : 1;

        if (line.m_specialHeadingInterface && type==0) {
          newLevel.m_labelWidth=0.2;
          newLevel.m_labelBeforeSpace=0.2*l;
        }
        else {
          newLevel.m_labelWidth=0.2;
          newLevel.m_labelBeforeSpace=0.2*(l-1);
          newLevel.m_labelAfterSpace=*(para.m_margins[0])/72.;
        }
        listsList[type]=listManager.getNewList(listsList[type], l, newLevel);
      }
    }
    if (maxLevelSet < level) maxLevelSet=level;

    // update the margins, ...
    if (line.m_specialHeadingInterface && line.m_listType==0)
      // checkme: true on some file but in other files, the display does not change:-~
      *(para.m_margins[0])= *(para.m_margins[1]) = 0;
    else
      *(para.m_margins[0])=-0.2;

    shared_ptr<MWAWList> &actList=listsList[line.m_listType];
    if (!actList) // we have a pb....
      continue;
    for (int type=0; type<4; type++)
      if (listsList[type]) listsList[type]->setLevel(level);
    if (line.m_listType==0)
      listsList[2]->setStartValueForNextElement(level==1 ? prop.m_startListIndex : 1);

    line.m_paragraph.m_listId=actList->getId();
    para.m_listStartValue = actList->getStartValueForNextElement();
    actList->openElement();
    actList->closeElement();
  }
}

////////////////////////////////////////
//! Internal: the state of a MindWrtParser
struct State {
  //! constructor
  State() : m_compressCorr(" etnroaisdlhcfp"), m_entryMap(), m_listProperties(),
    m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0),
    m_headerFieldList(), m_footerFieldList()
  {
    for (int i = 0; i < 3; i++)
      m_numLinesByZone[i] = 0;
  }

  //! the correspondance between int compressed and char : must be 15 character
  std::string m_compressCorr;

  //! the number of paragraph by zones ( main, header, footer )
  int m_numLinesByZone[3];
  //! the zones
  ZoneInfo m_zones[3];

  //! the zones map
  std::multimap<std::string, MWAWEntry> m_entryMap;

  //! the list properties
  ListProperties m_listProperties;
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;
  //! the actual list id
  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
  //! the header field list
  std::vector<Field> m_headerFieldList;
  //! the footer field list
  std::vector<Field> m_footerFieldList;
};

////////////////////////////////////////
//! Internal: the subdocument of a MindWrtParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(MindWrtParser &pars, MWAWInputStreamPtr input, int zoneId, int step) :
    MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId), m_step(step) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const
  {
    if (MWAWSubDocument::operator!=(doc)) return true;
    SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
    if (!sDoc) return true;
    if (m_id != sDoc->m_id) return true;
    if (m_step != sDoc->m_step) return true;
    return false;
  }

  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const
  {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  //! the subdocument id
  int m_id;
  //! the current step: 0 mean send header, 1 mean send header fields
  int m_step;
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("MindWrtParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  if (m_id != 1 && m_id != 2) {
    MWAW_DEBUG_MSG(("MindWrtParserInternal::SubDocument::parse: unknown zone\n"));
    return;
  }
  if (m_step < 0 || m_step > 1) {
    MWAW_DEBUG_MSG(("MindWrtParserInternal::SubDocument::parse: unknown step\n"));
    return;
  }
  MindWrtParser *parser=dynamic_cast<MindWrtParser *>(m_parser);
  if (!parser) {
    MWAW_DEBUG_MSG(("MindWrtParserInternal::SubDocument::parse: no parser\n"));
    return;
  }
  long pos = m_input->tell();
  if (m_step==0)
    parser->sendHeaderFooter(m_id==1);
  else
    parser->sendHeaderFooterFields(m_id==1);
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MindWrtParser::MindWrtParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWTextParser(input, rsrcParser, header), m_state()
{
  init();
}

MindWrtParser::~MindWrtParser()
{
}

void MindWrtParser::init()
{
  resetTextListener();
  setAsciiName("main-1");

  m_state.reset(new MindWrtParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void MindWrtParser::newPage(int number)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!getTextListener() || m_state->m_actPage == 1)
      continue;
    getTextListener()->insertBreak(MWAWTextListener::PageBreak);
  }
}

MWAWEntry MindWrtParser::readEntry()
{
  MWAWEntry res;
  MWAWInputStreamPtr input = getInput();
  res.setBegin((long) input->readULong(4));
  res.setLength((long) input->readULong(4));
  if (res.length() && !input->checkPosition(res.end())) {
    MWAW_DEBUG_MSG(("MindWrtParser::readEntry: find an invalid entry\n"));
    res.setLength(0);
  }
  return res;
}
////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void MindWrtParser::parse(librevenge::RVNGTextInterface *docInterface)
{
  if (!getInput().get() || !checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = true;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());

    checkHeader(0L);
    if (getRSRCParser()) {
      MWAWEntry corrEntry = getRSRCParser()->getEntry("STR ", 700);
      std::string corrString("");
      if (corrEntry.valid() && getRSRCParser()->parseSTR(corrEntry, corrString)) {
        if (corrString.length() != 15) {
          MWAW_DEBUG_MSG(("MindWrtParser::parse: resource correspondance string seems bad\n"));
        }
        else
          m_state->m_compressCorr = corrString;
      }
    }
    ok = createZones();
    if (ok) {
      createDocument(docInterface);
      sendZone(0);
    }

    libmwaw::DebugStream f;
    std::multimap<std::string, MWAWEntry>::const_iterator it
      = m_state->m_entryMap.begin();
    while (it != m_state->m_entryMap.end()) {
      MWAWEntry const &entry = (it++)->second;
      if (entry.isParsed())
        continue;
      f.str("");
      f << entry;
      ascii().addPos(entry.begin());
      ascii().addNote(f.str().c_str());
      ascii().addPos(entry.end());
      ascii().addNote("_");
    }
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("MindWrtParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetTextListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void MindWrtParser::createDocument(librevenge::RVNGTextInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getTextListener()) {
    MWAW_DEBUG_MSG(("MindWrtParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  int numPage = 0, numBreakPage = 0;
  for (size_t i = 0; i < m_state->m_zones[0].m_linesList.size(); i++) {
    MindWrtParserInternal::LineInfo const &line = m_state->m_zones[0].m_linesList[i];
    if (line.m_type == -2)
      numBreakPage++;
    if (line.m_page > numPage)
      numPage = line.m_page;
  }
  if (numBreakPage > numPage) numPage = numBreakPage;
  m_state->m_numPages = numPage+1;

  // create the page list
  MWAWPageSpan ps(getPageSpan());
  for (int i = 1; i <= 2; i++) {
    if (!m_state->m_zones[i].m_linesList.size())
      continue;
    MWAWHeaderFooter hF((i==1) ? MWAWHeaderFooter::HEADER : MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
    hF.m_subDocument.reset(new MindWrtParserInternal::SubDocument(*this, getInput(), i, 0));
    ps.setHeaderFooter(hF);
  }
  ps.setPageSpan(m_state->m_numPages+1);
  std::vector<MWAWPageSpan> pageList(1,ps);
  //
  MWAWTextListenerPtr listen(new MWAWTextListener(*getParserState(), pageList, documentInterface));
  setTextListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool MindWrtParser::createZones()
{
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  long pos = input->tell();
  if (!input->checkPosition(pos+128+56)) {
    MWAW_DEBUG_MSG(("MindWrtParser::createZones: zones Zone is too short\n"));
    return false;
  }

  for (int i = 0; i < 16; i++) {
    pos = input->tell();
    MWAWEntry entry = readEntry();
    if (!entry.valid()) {
      ascii().addPos(pos);
      ascii().addNote("_");
      continue;
    }
    switch (i) {
    case 0:
    case 1:
    case 2:
      entry.setType("LineInfo");
      entry.setId(i);
      break;
    case 6:
      entry.setType("PrintInfo");
      break;
    case 7:
      entry.setType("LastZone");
      break;
    case 10:
      entry.setType("HeadState");
      break;
    case 12:
      entry.setType("HeadField");
      break;
    case 14:
      entry.setType("HeadProp");
      break;
    case 15:
      entry.setType("HeadCust");
      break;
    default: {
      std::stringstream s;
      s << "Zone" << i;
      entry.setType(s.str());
      break;
    }
    }
    m_state->m_entryMap.insert(std::multimap<std::string,MWAWEntry>::value_type
                               (entry.type(), entry));
    f.str("");
    f << "Entries(" << entry.type() << "):" << std::hex << entry.begin() << "-" << entry.end() << std::dec << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  pos = input->tell();
  f.str("");
  f << "Entries(ZoneDef):";
  for (int i = 0; i < 24; i++) { // always 0 ?
    long val = input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  std::multimap<std::string, MWAWEntry>::iterator it;
  // first read print info
  it = m_state->m_entryMap.find("PrintInfo");
  if (it !=  m_state->m_entryMap.end())
    readPrintInfo(it->second);
  // now the heading properties
  it = m_state->m_entryMap.find("HeadProp");
  if (it !=  m_state->m_entryMap.end())
    readHeadingProperties(it->second);
  it = m_state->m_entryMap.find("HeadState");
  if (it != m_state->m_entryMap.end())
    readHeadingStates(it->second);
  it = m_state->m_entryMap.find("HeadCust");
  if (it != m_state->m_entryMap.end())
    readHeadingCustom(it->second);
  it = m_state->m_entryMap.find("HeadField");
  if (it !=  m_state->m_entryMap.end())
    readHeadingFields(it->second);
  it = m_state->m_entryMap.find("Zone8");
  if (it !=  m_state->m_entryMap.end())
    readZone8(it->second);
#ifdef DEBUG
  // some unknown zone
  it = m_state->m_entryMap.find("LastZone");
  if (it !=  m_state->m_entryMap.end())
    readLastZone(it->second);
#endif
  m_state->m_listProperties.updateHeadingList();
  // finally, we can read the line info
  it = m_state->m_entryMap.find("LineInfo");
  while (it !=  m_state->m_entryMap.end() && it->first == "LineInfo")
    readLinesInfo(it++->second);

  for (int i = 0; i < 3; i++)
    if (m_state->m_zones[i].m_linesList.size())
      return true;
  return false;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// try to send a text zone
////////////////////////////////////////////////////////////
bool MindWrtParser::sendZone(int id)
{
  if (id < 0 || id >= 3) {
    MWAW_DEBUG_MSG(("MindWrtParser::sendZone: find unexpected id %d\n", id));
    return false;
  }
  MWAWTextListenerPtr listener=getTextListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MindWrtParser::sendZone: can not find a listener\n"));
    return false;
  }

  MWAWParagraph para;
  // FIXME: writerperfect or libreoffice seems to loose page dimensions and header/footer if we begin by a list
  if (id==0) {
    para.setInterline(1,librevenge::RVNG_POINT);
    setProperty(para);
    getTextListener()->insertEOL();
  }
  else
    setProperty(para);

  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  if (id==0) m_state->m_actPage = 1;
  MindWrtParserInternal::ZoneInfo &zone = m_state->m_zones[id];
  for (size_t i = 0; i < zone.m_linesList.size(); i++) {
    MindWrtParserInternal::LineInfo &line = zone.m_linesList[i];
    if (i==0) {
      ascii().addPos(line.m_entry.begin());
      ascii().addNote("Entries(Text)");
    }
    if (id==0 && line.m_page+1 > m_state->m_actPage)
      newPage(line.m_page+1);
    bool done=false;
    switch (line.m_type) {
    case 0:
      if (line.m_height) {
        listener->setParagraph(line.m_paragraph);
        if (line.m_flags[1]&8)
          done = readCompressedText(line);
        else
          done = readText(line);
      }
      else if (line.m_paragraphSet) {
        done = true;
        setProperty(line.m_paragraph);
      }
      break;
    case -1:
      if (line.m_entry.length()==0) {
        done = true;
        break;
      }
      listener->setParagraph(line.m_paragraph);
      if (!readGraphic(line))
        break;
      done = true;
      listener->insertEOL();
      break;
    case -2:
      if (id!=0) {
        MWAW_DEBUG_MSG(("MindWrtParser::sendZone: find page break on not main zone\n"));
        break;
      }
      newPage(m_state->m_actPage+1);
      done = true;
      break;
    case -3: // a nop ?
      done = true;
      break;
    default:
      break;
    }
    if (done)
      continue;
    f.str("");
    f << "Text[" << line << "]";
    ascii().addPos(line.m_entry.begin());
    ascii().addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// read a graphic zone
////////////////////////////////////////////////////////////
bool MindWrtParser::readGraphic(MindWrtParserInternal::LineInfo const &line)
{
  if (!line.m_entry.valid())
    return false;

  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  int sz = (int) line.m_entry.length();
  if (sz < 10) {
    MWAW_DEBUG_MSG(("MindWrtParser::readGraphic: zone size is two short or odd\n"));
    return false;
  }

  long pos=line.m_entry.begin();
  input->seek(pos, librevenge::RVNG_SEEK_SET);

  int dim[4];
  for (int i = 0; i < 4; i++) dim[i] = (int) input->readLong(2);
  MWAWBox2f box(MWAWVec2f((float)dim[1],(float)dim[0]),
                MWAWVec2f((float)dim[3],(float)dim[2]));
  f << "Entries(graphic): bdBox=" << box << ",";

  shared_ptr<MWAWPict> pict(MWAWPictData::get(input, sz-8));
  if (!pict) {
    MWAW_DEBUG_MSG(("MindWrtParser::readGraphic: can not read the picture\n"));
    return false;
  }
  librevenge::RVNGBinaryData data;
  std::string type;
  if (getTextListener() && pict->getBinary(data,type)) {
    MWAWPosition pictPos=MWAWPosition(MWAWVec2f(0,0),box.size(), librevenge::RVNG_POINT);
    pictPos.setRelativePosition(MWAWPosition::Char);
    getTextListener()->insertPicture(pictPos,data, type);
  }
  ascii().skipZone(pos+8, pos+sz-1);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read a ruler zone
////////////////////////////////////////////////////////////
bool MindWrtParser::readRuler(MindWrtParserInternal::LineInfo &line)
{
  line.m_paragraph = MWAWParagraph();
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;

  if (line.m_entry.length() < 10 || (line.m_entry.length()%2)) {
    MWAW_DEBUG_MSG(("MindWrtParser::readRuler: zone size is two short or odd\n"));
    return false;
  }
  line.m_specialHeadingInterface = (line.m_flags[1] & 0x4)==0;

  long pos=line.m_entry.begin();
  input->seek(pos, librevenge::RVNG_SEEK_SET);

  MWAWParagraph para;
  para.m_marginsUnit = librevenge::RVNG_POINT;
  para.m_margins[1] = (double) input->readULong(2);
  para.m_margins[2] = getPageWidth()*72.0-(double) input->readULong(2);
  if (*(para.m_margins[2]) < 0) {
    f << "#rightMargin=" << getPageWidth()*72.0-*(para.m_margins[2]);
    para.m_margins[2] = 0.0;
  }
  long val = (long) input->readULong(1);
  switch (val) {
  case 0:
    para.m_justify = MWAWParagraph::JustificationLeft;
    break;
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
    f << "#align=" << std::hex << val << std::dec << ",";
    break;
  }
  int N = (int) input->readULong(1);
  if (line.m_entry.length() != 2*N+10) {
    MWAW_DEBUG_MSG(("MindWrtParser::readRuler: num tabs is incorrect\n"));
    line.m_paragraph = para;
    line.m_paragraphSet = true;
    return false;
  }
  val = (long) input->readULong(2);
  double factor = 1.0;
  switch (val & 0x7FFF) {
  case 0:
    break;
  case 1:
    factor = 1.5;
    break;
  case 2:
    factor = 2.;
    break;
  default:
    if (val) f << "#interline=" << std::hex << (val&0x7FFF) << std::dec << ",";
  }
  if (val & 0x8000) { // 6 inches by line + potential before space
    para.m_spacings[1] = (factor-1.)/6;
    para.setInterline(12, librevenge::RVNG_POINT);
  }
  else
    para.setInterline(factor, librevenge::RVNG_PERCENT);
  para.m_margins[0] = ((double) input->readULong(2))-*(para.m_margins[1]);
  for (int i = 0; i < N; i++) {
    MWAWTabStop tab;
    val = input->readLong(2);
    if (val > 0)
      tab.m_position = double(val)/72.;
    else {
      tab.m_position = -double(val)/72.;
      tab.m_alignment = MWAWTabStop::CENTER;
    }
    para.m_tabs->push_back(tab);
  }
  para.m_extra = f.str();
  line.m_paragraph = para;
  line.m_paragraphSet = true;

  f.str("");
  f << "Text[ruler]:" << para;

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// send the header data
////////////////////////////////////////////////////////////
void MindWrtParser::sendHeaderFooter(bool header)
{
  MWAWTextListenerPtr listener=getTextListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MindWrtParser::sendHeaderFooter: can note find the listener\n"));
    return;
  }
  int zId=header ? 1 : 2;
  std::vector<MindWrtParserInternal::Field> const &fieldList=
    header ? m_state->m_headerFieldList : m_state->m_footerFieldList;
  if (fieldList.size()) {
    /** field are separated from the main text
        -> we need to use an intermediate frame */
    MWAWPosition fPos(MWAWVec2f(0,0), MWAWVec2f(float(getPageWidth()),0), librevenge::RVNG_INCH);
    fPos.m_anchorTo = MWAWPosition::Paragraph;
    fPos.m_wrapping =  MWAWPosition::WBackground;
    shared_ptr<MindWrtParserInternal::SubDocument> subDoc
    (new MindWrtParserInternal::SubDocument(*this, getInput(), zId, 1));
    listener->insertTextBox(fPos, subDoc);
  }
  sendZone(zId);
}

void MindWrtParser::sendHeaderFooterFields(bool header)
{
  MWAWTextListenerPtr listener=getTextListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MindWrtParser::sendHeaderFooterFields: can note find the listener\n"));
    return;
  }
  std::vector<MindWrtParserInternal::Field> fieldList=
    header ? m_state->m_headerFieldList : m_state->m_footerFieldList;
  size_t numFields=fieldList.size();
  if (!numFields) {
    MWAW_DEBUG_MSG(("MindWrtParser::sendHeaderFooterFields: called with empty field list\n"));
    return;
  }
  // sort the field list by position ; at most 4 elements, so...
  for (size_t i=0; i+1 < numFields; i++) {
    for (size_t j=i+1; j < numFields; j++) {
      int diffLine = fieldList[j].m_pos[1]-fieldList[i].m_pos[1];
      if (diffLine > 0 ||
          (diffLine==0 && fieldList[j].m_pos[0]>fieldList[i].m_pos[0]))
        continue;
      MindWrtParserInternal::Field tmp=fieldList[j];
      fieldList[j]=fieldList[i];
      fieldList[i]=tmp;
    }
  }
  if (fieldList[numFields-1].m_pos[1]>10) {
    MWAW_DEBUG_MSG(("MindWrtParser::sendHeaderFooterFields: the line position seems too big\n"));
    return;
  }
  int actLine = 0;
  MWAWParagraph defPara;
  defPara.setInterline(16, librevenge::RVNG_POINT);
  listener->setParagraph(defPara);
  size_t f=0;
  while (f < numFields) {
    while (fieldList[f].m_pos[1]>++actLine)
      listener->insertEOL();
    // first prepare the tabs
    MWAWParagraph newPara(defPara);
    int actPos=0;
    for (size_t f1=f; f1 < numFields; f1++) {
      if (fieldList[f1].m_pos[1]!=actLine)
        break;
      if (fieldList[f1].m_pos[0]==actPos)
        continue;
      actPos=fieldList[f1].m_pos[0];
      MWAWTabStop tab;
      tab.m_position = double(actPos)/72.;
      newPara.m_tabs->push_back(tab);
    }
    listener->setParagraph(newPara);

    actPos=0;
    while (f < numFields) {
      if (fieldList[f].m_pos[1]!=actLine)
        break;
      if (fieldList[f].m_pos[0]!=actPos) {
        actPos=fieldList[f].m_pos[0];
        listener->insertTab();
      }
      listener->insertField(fieldList[f].getField());
      f++;
    }
    if (f != numFields)
      listener->insertEOL();
  }
}
////////////////////////////////////////////////////////////
// read the text zone
////////////////////////////////////////////////////////////
bool MindWrtParser::readText(MindWrtParserInternal::LineInfo const &line)
{
  if (!line.m_entry.valid())
    return false;

  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;

  long pos=line.m_entry.begin();
  long endPos=line.m_entry.end();
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int num = (int) input->readULong(2);
  if (pos+num >= endPos) {
    MWAW_DEBUG_MSG(("MindWrtParser::readText: numChar is too long\n"));
    return false;
  }

  f << "Text:";
  if (line.m_listType!=1) {
    f << "[list=" << line.m_listLevel;
    switch (line.m_listType) {
    case 0:
      f << "Head,";
      break; // Heading
    case 1:
      f << "Unl,";
      break; // unlabelled
    case 2:
      f << "Num,";
      break; // bullet
    case 3:
      f << "Bul,";
      break; // paragraph
    default:
      f << "[#type=" << line.m_listType << "],";
    }
    f << "],";
  }
  std::string text("");
  for (int n = 0; n < num; n++) {
    char c = (char) input->readULong(1);
    if (!c) {
      MWAW_DEBUG_MSG(("MindWrtParser::readText: find 0 char\n"));
      continue;
    }
    text+=(char) c;
  }
  f << text;

  // realign to 2
  if (input->tell()&1)
    input->seek(1,librevenge::RVNG_SEEK_CUR);

  ascii().addPos(line.m_entry.begin());
  ascii().addNote(f.str().c_str());

  std::vector<int> textPos;
  std::vector<MWAWFont> fonts;
  if (!readFonts(line.m_entry, fonts, textPos))
    return false;
  sendText(text, fonts, textPos);
  if (input->tell()!=endPos) {
    ascii().addPos(input->tell());
    ascii().addNote("Text(II):#");
  }
  return true;
}

bool MindWrtParser::readCompressedText(MindWrtParserInternal::LineInfo const &line)
{
  if (!line.m_entry.valid())
    return false;
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;

  long pos=line.m_entry.begin();
  long endPos=line.m_entry.end();
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int num = (int) input->readULong(2);
  if (pos+num/2 > endPos) {
    MWAW_DEBUG_MSG(("MindWrtParser::readCompressedText: numChar is too long\n"));
    return false;
  }

  f << "Text:";
  if (line.m_listType!=1) {
    f << "[list=" << line.m_listLevel;
    switch (line.m_listType) {
    case 0:
      f << "Head,";
      break; // Heading
    case 1:
      f << "Unl,";
      break; // unlabelled
    case 2:
      f << "Num,";
      break; // bullet
    case 3:
      f << "Bul,";
      break; // paragraph
    default:
      f << "[#type=" << line.m_listType << "],";
    }
    f << "],";
  }
  int actualChar = 0;
  bool actualCharSet = false;
  std::string text("");
  for (int n = 0; n < num; n++) {
    if (input->tell() >= endPos) {
      MWAW_DEBUG_MSG(("MindWrtParser::readCompressedText: entry is too short\n"));
      return false;
    }
    int highByte = 0;
    for (int st = 0; st < 3; st++) {
      int actVal;
      if (!actualCharSet) {
        if (input->isEnd()) {
          MWAW_DEBUG_MSG(("MindWrtParser::readCompressedText: text is too long\n"));
          return false;
        }
        actualChar = (int) input->readULong(1);
        actVal = (actualChar >> 4);
      }
      else
        actVal = (actualChar & 0xf);
      actualCharSet = !actualCharSet;
      if (st == 0) {
        if (actVal == 0xf) continue;
        text += (char) m_state->m_compressCorr[(size_t) actVal];
        break;
      }
      if (st == 1) { // high bytes
        highByte = (actVal<<4);
        continue;
      }
      if (highByte == 0 && actVal==0) {
        MWAW_DEBUG_MSG(("rser::readCompressedText: find 0 char\n"));
        continue;
      }
      text += (char)(highByte | actVal);
    }
  }
  f << text;

  // realign to 2
  if (input->tell()&1)
    input->seek(1,librevenge::RVNG_SEEK_CUR);

  ascii().addPos(line.m_entry.begin());
  ascii().addNote(f.str().c_str());

  std::vector<int> textPos;
  std::vector<MWAWFont> fonts;
  if (!readFonts(line.m_entry, fonts, textPos))
    return false;
  sendText(text, fonts, textPos);
  if (input->tell()!=endPos) {
    ascii().addPos(input->tell());
    ascii().addNote("Text(II):#");
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the fonts
////////////////////////////////////////////////////////////
bool MindWrtParser::readFonts(MWAWEntry const &entry, std::vector<MWAWFont> &fonts,
                              std::vector<int> &textPos)
{
  textPos.resize(0);
  fonts.resize(0);

  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;

  long pos = input->tell();
  long endPos= entry.end();
  if (pos+2 > endPos) {
    MWAW_DEBUG_MSG(("MindWrtParser::readFonts: zone is too short\n"));
    return false;
  }
  int sz = (int) input->readULong(2);
  if (pos+2+sz > endPos || (sz%6)) {
    MWAW_DEBUG_MSG(("MindWrtParser::readFonts: sz is odd\n"));
    return false;
  }
  int N = sz/6;
  f.str("");
  f << "Text[Font]:N=" << N;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    f << "Text:Font" << i << ":";
    int tPos = (int) input->readULong(2);
    textPos.push_back(tPos);
    f << "pos=" << tPos << ",";
    MWAWFont font;
    font.setSize((float) input->readULong(1));
    int flag = (int) input->readULong(1);
    uint32_t flags = 0;
    // bit 1 = plain
    if (flag&0x1) flags |= MWAWFont::boldBit;
    if (flag&0x2) flags |= MWAWFont::italicBit;
    if (flag&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (flag&0x8) flags |= MWAWFont::embossBit;
    if (flag&0x10) flags |= MWAWFont::shadowBit;
    if (flag&0x20) font.set(MWAWFont::Script::super100());
    if (flag&0x40) font.set(MWAWFont::Script::sub100());
    if (flag&0x80) f << "#fFlags80,";
    font.setFlags(flags);
    font.setId((int) input->readULong(2));
    fonts.push_back(font);
#ifdef DEBUG
    f << font.getDebugString(getFontConverter());
#endif
    input->seek(pos+6, librevenge::RVNG_SEEK_SET);

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool MindWrtParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = MindWrtParserInternal::State();

  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork())
    return false;
  libmwaw::DebugStream f;

  int const headerSize=0x50;
  if (!input->checkPosition(headerSize)) {
    MWAW_DEBUG_MSG(("MindWrtParser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0,librevenge::RVNG_SEEK_SET);
  if (input->readULong(2) != 0x7704)
    return false;
  if (header)
    header->reset(MWAWDocument::MWAW_T_MINDWRITE, 2);

  f << "Entries(Header):";
  for (int i = 0; i < 3; i++)
    m_state->m_numLinesByZone[i] = (int) input->readULong(2);
  f << "nLines=" << m_state->m_numLinesByZone[0] << ",";
  if (m_state->m_numLinesByZone[1])
    f << "nLines[Head]=" << m_state->m_numLinesByZone[1] << ",";
  if (m_state->m_numLinesByZone[2])
    f << "nLines[Foot]=" << m_state->m_numLinesByZone[2] << ",";
  long val;
  for (int i = 0; i < 2; i++) { // find [01] [15]
    val = input->readLong(1);
    if (val) f << "f" << i << "=" << val << ",";
  }
  for (int i = 0; i < 3; i++) { // always 0 ?
    val = input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  val = (long) input->readULong(2); // always 0x7fff
  if (val != 0x7fff) f << "g3=" << std::hex << val << std::dec << ",";
  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  long pos=input->tell();
  f.str("");
  f << "Header(II):";
  val = input->readLong(2); // 0 or 0x80
  if (val) f << "f0=" << val << ",";
  for (int i = 1; i < 5; i++) { // 08, 0, [60|62], [40|60|66|70]
    val = (long) input->readULong(1);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  for (int i = 0; i < 2; i++) { // -1, -1|0
    val = input->readLong(1);
    if (!val) continue;
    f << "g" << i;
    if (val!=-1) f << "=" << val;
    f << ",";
  }
  val = input->readLong(2); // always 1 ?
  if (val != 1) f << "g2=" << val << ",";
  m_state->m_listProperties.m_startListIndex = (int) input->readULong(1)+1;
  if (m_state->m_listProperties.m_startListIndex!=1)
    f << "list[start]=" << m_state->m_listProperties.m_startListIndex << ",";
  val = (long) input->readULong(1);
  if (val)
    f << "g3=" << val << ",";

  for (int i = 0; i < 17; i++) { // always 0 ?
    val = input->readLong(2);
    if (val) f << "h" << i << "=" << val << ",";
  }
  val = input->readLong(2); // always 0x10 ?
  if (val!=0x10) f << "fl0=" << std::hex << val << std::dec << ",";
  val = input->readLong(2); // always 0x100, maybe textFirstPos ?
  if (val!=0x100) f << "fl1=" << std::hex << val << std::dec << ",";
  val = input->readLong(2);// always 0x0 ?
  if (val) f << "fl2=" << std::hex << val << std::dec << ",";
  val = input->readLong(2);// always 0x1 ?
  if (val != 1) f << "fl3=" << std::hex << val << std::dec << ",";
  for (int i = 0; i < 4; i++) { // always 0 ?
    val = input->readLong(2);
    if (val) f << "j" << i << "=" << val << ",";
  }

  bool ok = true;
  if (strict) {
    // check the line info block size
    input->seek(0x50, librevenge::RVNG_SEEK_SET);
    for (int i = 0; i < 3; i++) {
      input->seek(4, librevenge::RVNG_SEEK_CUR);
      if (input->readLong(4) == 32*m_state->m_numLinesByZone[i])
        continue;
      ok = false;
      break;
    }
    input->seek(0x50, librevenge::RVNG_SEEK_SET);
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  ascii().addPos(input->tell());
  ascii().addNote("_");

  return ok;
}

////////////////////////////////////////////////////////////
// read the line info zones
////////////////////////////////////////////////////////////
bool MindWrtParser::readLinesInfo(MWAWEntry &entry)
{
  if (!entry.valid())
    return false;
  if (entry.id() < 0 || entry.id() >= 3) {
    MWAW_DEBUG_MSG(("MindWrtParser::readLinesInfo: bad entry id %d\n", entry.id()));
    return false;
  }
  if (entry.length()%32) {
    MWAW_DEBUG_MSG(("MindWrtParser::readLinesInfo: the size seems odd\n"));
    return false;
  }
  if (entry.isParsed()) {
    MWAW_DEBUG_MSG(("MindWrtParser::readLinesInfo: entry is already parsed\n"));
    return true;
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int N = int(entry.length())/32;

  libmwaw::DebugStream f;
  MindWrtParserInternal::ZoneInfo &textZone = m_state->m_zones[entry.id()];
  textZone.m_linesList.clear();
  for (int n = 0; n < N; n++) {
    MindWrtParserInternal::LineInfo line;

    pos = input->tell();
    f.str("");
    line.m_type = (int) input->readLong(1); // 0, fd, ff
    line.m_height = (int) input->readULong(1);
    line.m_y = (int) input->readLong(2);
    line.m_page = (int)input->readULong(1); // check me
    long val = input->readLong(2); // -1 or between 5d7-5de
    f << "f0=" << std::hex << val << std::dec << ",";
    for (int i=0; i < 2; i++)
      line.m_flags[i] = (int) input->readULong(1);
    long ptr = (long) input->readULong(1);
    line.m_entry.setBegin((ptr << 16) | (long) input->readULong(2));
    line.m_entry.setLength((long) input->readULong(2));
    line.m_extra=f.str();
    for (int i=2; i < 4; i++)
      line.m_flags[i] = (int) input->readULong(1);
    line.m_listLevel = (int) input->readLong(2);
    line.m_listType = (int) input->readLong(2); //0: heading, 1: unlabelled 2: paragraph, 3: bullet
    /** 01[45]0 */

    textZone.m_linesList.push_back(line);

    f.str("");
    f << "LineInfo-" << entry.id() << "[" << n << "]:" << line;
    ascii().addDelimiter(input->tell(),'|');
    input->seek(pos+32, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());

  }
  // first, we must read and update the paragraph
  MWAWParagraph para;
  bool hasSpecialHeadInterface = false;
  for (size_t n = 0; n < size_t(N); n++) {
    MindWrtParserInternal::LineInfo &line=textZone.m_linesList[n];
    if (line.m_height || line.m_type) {
      line.m_paragraph=para;
      line.m_specialHeadingInterface = hasSpecialHeadInterface;
      continue;
    }
    readRuler(line);
    para = line.m_paragraph;
    hasSpecialHeadInterface = line.m_specialHeadingInterface;
  }
  if (entry.id()==0) // only main zone has list, ouff...
    textZone.updateListId(m_state->m_listProperties, *getParserState()->m_listManager);
  return true;
}

////////////////////////////////////////////////////////////
// read the last zone ( use ?)
////////////////////////////////////////////////////////////
bool MindWrtParser::readLastZone(MWAWEntry &entry)
{
  if (!entry.valid())
    return false;
  if (entry.length()<0x8 || (entry.length())%4!=0) {
    MWAW_DEBUG_MSG(("MindWrtParser::readLastZone: the size seems odd\n"));
    return false;
  }
  if (entry.isParsed()) {
    MWAW_DEBUG_MSG(("MindWrtParser::readLastZone: entry is already parsed\n"));
    return true;
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos, librevenge::RVNG_SEEK_SET);

  libmwaw::DebugStream f;
  f << "LastZone:";
  int N=int(entry.length()>>2)-2;
  long val;
  for (int i = 0; i < N; i++) { // find one time two value 0x154, 0xc ?
    val = (long) input->readULong(4);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  val = (long) input->readULong(4);
  if (val != pos) {
    MWAW_DEBUG_MSG(("MindWrtParser::readLastZone: the ptr seems odd\n"));
    f << "#ptr=" << std::hex << val << std::dec << ",";
  }
  val = (long) input->readULong(2); // always 0x7fff
  if (val != 0x7fff) f << "g0=" << std::hex << val << std::dec << ",";
  val = input->readLong(2); // always -1
  if (val != -1)  f << "g1=" << val << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read the heading state/properties/custom
////////////////////////////////////////////////////////////
bool MindWrtParser::readHeadingStates(MWAWEntry &entry)
{
  if (!entry.valid())
    return false;
  if (!entry.length() || (entry.length()%2)) {
    MWAW_DEBUG_MSG(("MindWrtParser::readHeadingStates: the size seems odd\n"));
    return false;
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "HeadState:";
  long unkn = input->readLong(2); // always 2?
  if (unkn!=2) f << "unkn=" << unkn << ",";
  int N=int(entry.length()/2)-1;
  for (int i = 0; i < N; i++) {
    int fl=(int) input->readULong(1);
    int wh=(int) input->readULong(1);
    if (fl==0 && wh==4)
      continue;
    f << "L" << i << "=[";
    switch (fl) {
    case 0: // visible
      break;
    case 1:
      f << "hidden,";
      break;
    default:
      f << "#state=" << std::hex << fl << std::dec << ",";
      break;
    }
    switch (wh) {
    // find also 0 and 1
    case 4: // standard
      break;
    default:
      f << "#wh=" << std::hex << fl << std::dec << ",";
      break;
    }
    f << "],";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

bool MindWrtParser::readHeadingCustom(MWAWEntry &entry)
{
  if (!entry.valid())
    return false;
  int len=int(entry.length());
  if (len<16) {
    MWAW_DEBUG_MSG(("MindWrtParser::readHeadingCustom: the size seems odd\n"));
    return false;
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin(), beginPos=pos;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "HeadCust:";
  if ((int) input->readULong(2)!= len) {
    MWAW_DEBUG_MSG(("MindWrtParser::readHeadingCustom: the size field seems odd\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (len < 16+2*N) {
    MWAW_DEBUG_MSG(("MindWrtParser::readHeadingCustom: N seems bads\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }

  int val = (int) input->readLong(2);
  m_state->m_listProperties.m_listLevelsRepeatPos = val;
  if (val) f << "repeatPos=" << val << ",";
  int debDeplPos = (int) input->readLong(2);
  int debStringPos = (int) input->readLong(2);
  if (debDeplPos+2*N >= len || debStringPos > len) {
    f << "##deplPos=" << debDeplPos << ","
      << "##stringPos=" << debStringPos << ",";
    MWAW_DEBUG_MSG(("MindWrtParser::readHeadingCustom: can not read first pos\n"));
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  for (int i = 0; i < 3; i++) { // always 0 ?
    val = (int) input->readLong(2);
    if (!val) continue;
    f << "f" << i << "=" << val << ",";
  }
  input->seek(beginPos+(long) debDeplPos, librevenge::RVNG_SEEK_SET);

  std::vector<int> listPos;
  for (int i = 0; i < N; i++)
    listPos.push_back((int) input->readLong(2));
  listPos.push_back(len);

  std::string str("");
  for (size_t i = 0; i < size_t(N); i++) {
    input->seek(beginPos+(long) listPos[i], librevenge::RVNG_SEEK_SET);
    int sSz=listPos[i+1]-listPos[i];
    if (sSz < 0) {
      f << "###len"<<i<<"="<< sSz << ",";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return false;
    }
    MWAWListLevel level;
    bool seeIndex=false;
    for (int s = 0; s < sSz; s++) {
      char c = (char) input->readULong(1);
      int unicode;
      str+=c;
      if (c==0)
        break;
      // check for end of field
      if (seeIndex && (s >= sSz-2) && (c==',' || c==';' || c==' ')) {
        if (s==sSz-1)
          break;
        if ((char) input->readULong(1)==' ')
          break;
        input->seek(-1, librevenge::RVNG_SEEK_CUR);
      }
      if (!seeIndex) {
        seeIndex = true;
        switch (c) {
        case '1':
          level.m_type=MWAWListLevel::DECIMAL;
          break;
        case 'a':
          level.m_type=MWAWListLevel::LOWER_ALPHA;
          break;
        case 'A':
          level.m_type=MWAWListLevel::UPPER_ALPHA;
          break;
        case 'i':
          level.m_type=MWAWListLevel::LOWER_ROMAN;
          break;
        case 'I':
          level.m_type=MWAWListLevel::UPPER_ROMAN;
          break;
        default:
          seeIndex=false;
          unicode = getParserState()->m_fontConverter->unicode(3, (unsigned char)c);
          if (unicode==-1)
            libmwaw::appendUnicode((unsigned char)c, level.m_prefix);
          else
            libmwaw::appendUnicode(uint32_t(unicode), level.m_prefix);
          break;
        }
        continue;
      }
      unicode = getParserState()->m_fontConverter->unicode(3, (unsigned char)c);
      if (unicode==-1)
        libmwaw::appendUnicode((unsigned char)c, level.m_suffix);
      else
        libmwaw::appendUnicode(uint32_t(unicode), level.m_suffix);
    }
    m_state->m_listProperties.m_headingListLevels.push_back(level);
    str+='|';
  }
  f << "\"" << str << "\"";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

bool MindWrtParser::readHeadingProperties(MWAWEntry &entry)
{
  if (!entry.valid())
    return false;
  if (entry.length()!=32) {
    MWAW_DEBUG_MSG(("MindWrtParser::readHeadingProperties: the size seems odd\n"));
    return false;
  }
  if (entry.isParsed()) {
    MWAW_DEBUG_MSG(("MindWrtParser::readHeadingProperties: entry is already parsed\n"));
    return true;
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "HeadProp:";
  long val=input->readLong(1); // always 0 ?
  if (val) f << "f0=" << val << ",";
  m_state->m_listProperties.m_headingStyle=(int) input->readULong(1);
  switch (m_state->m_listProperties.m_headingStyle) {
  case 1:
    f << "list[type]=Hardvard,"; // I. A. 1. a) i)
    break;
  case 2:
    f << "list[type]=Chicago,"; // I. A. 1. a) (1), (a), i)
    break;
  case 3:
    f << "list[type]=Section,"; // 1. 1.
    break;
  case 4:
    f << "list[type]=custom,";
    break;
  default:
    f << "#list[type]=" << m_state->m_listProperties.m_headingStyle << ",";
    break;
  }
  val = input->readLong(1);
  switch (val) {
  case 1: // show all level
    break;
  case 2:
    m_state->m_listProperties.m_headingFullSubLevels = false;
    f << "list[showOneLevel],";
    break;
  default:
    f << "#list[showOneLevel]=" << val << ",";
    break;
  }
  val = (long) input->readULong(1); // short, long?
  if (val)
    f << "f1=" << val << ",";
  long const expectedValues[]= {0,0x7ffe,0xf,0xc};
  for (int i = 0; i < 4; i++) {
    val = input->readLong(2);
    if (val!=expectedValues[i])
      f << "f" << i+4 << "=" << val << ",";
  }
  for (int i = 0; i < 6; i++) { // always 0?
    val = input->readLong(2);
    if (val)
      f << "g" << i << "=" << val << ",";
  }
  val = (long) input->readULong(4); // 5e3be|5e3c2|5e49c|2975a4|69dce0|69e10c
  f << "ptr?=" << std::hex << val << std::dec << ",";
  val = (long) input->readULong(4); // always FFFFFF?
  if (val != 0xFFFFFF)
    f << "unkn?=" << std::hex << val << std::dec << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read zone8
////////////////////////////////////////////////////////////
bool MindWrtParser::readZone8(MWAWEntry &entry)
{
  if (!entry.valid())
    return false;
  if (entry.length()!=48) {
    MWAW_DEBUG_MSG(("MindWrtParser::readZone8: the size seems odd\n"));
    return false;
  }
  if (entry.isParsed()) {
    MWAW_DEBUG_MSG(("MindWrtParser::readZone8: entry is already parsed\n"));
    return true;
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Zone8:";
  long val = (long) input->readULong(1);// fl0=0|1|2c|2d,
  if (val)
    f << "fl0=" << std::hex << val << std::dec << ",";
  val = (long) input->readULong(1); // fl1=2b|6b|ab
  if (val & 0x80) { // checkme
    f << "showFirstLevel?,";
    m_state->m_listProperties.m_showFirstLevel = true;
    val &= 0x7F;
  }
  if (val)
    f << "fl1=" << std::hex << val << std::dec << ",";
  /* f0=0|9|a, f1=4|10|5c, f2=0|1|1c,f3=1|3|9|c|24|29,f4=1|2,4|6|c|24,f5=0|17|63,
     f6=0, f7=26|c5, f8=0
     f3=f4 related to visible heading?
  */
  for (int i = 0; i < 9; i++) {
    val = input->readLong(2);
    if (val)
      f << "f" << i << "=" << val << ",";
  }
  int dim[2];
  for (int i = 0; i < 2; i++)
    dim[i] = (int) input->readLong(2);
  f << "dim=" << dim[0] << "x" << dim[1] << ",";
  for (int i = 0; i < 12; i++) { // always 0...
    val = input->readLong(2);
    if (val)
      f << "g" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// header/footer fields position
////////////////////////////////////////////////////////////
bool MindWrtParser::readHeadingFields(MWAWEntry &entry)
{
  if (!entry.valid())
    return false;
  if (entry.length()%12) {
    MWAW_DEBUG_MSG(("MindWrtParser::readHeadingFields: the size seems odd\n"));
    return false;
  }
  if (entry.isParsed()) {
    MWAW_DEBUG_MSG(("MindWrtParser::readHeadingFields: entry is already parsed\n"));
    return true;
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos, librevenge::RVNG_SEEK_SET);

  int num = int(entry.length()/12);
  if (num != 8) {
    MWAW_DEBUG_MSG(("MindWrtParser::readHeadingFields: the number of fields seems odd\n"));
  }
  libmwaw::DebugStream f;
  long val;
  static MWAWField::Type const(listType[4])= {
    MWAWField::Date, MWAWField::Time, MWAWField::PageNumber, MWAWField::Title
  };
  for (int i = 0; i < num; i++) {
    pos = input->tell();
    MindWrtParserInternal::Field field(listType[i%4]);
    f.str("");
    int dim[2];
    for (int j=0; j < 2; j++)
      dim[j] = (int) input->readLong(j==0 ? 2:4);
    field.m_pos=MWAWVec2i(dim[1], dim[0]);
    static int const(expectedVal[3])= {3,12,0};
    for (int j=0; j < 3; j++) {
      val = input->readLong(2);
      if (val!=expectedVal[j])
        f << "f" << j << "=" << val << ",";
    }
    field.m_extra=f.str();
    f.str("");
    f << "HeadField[" << i << "]:" << field;
    if (num==8 && field.ok()) {
      if (i < 4)
        m_state->m_headerFieldList.push_back(field);
      else
        m_state->m_footerFieldList.push_back(field);
    }
    input->seek(pos+12, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool MindWrtParser::readPrintInfo(MWAWEntry &entry)
{
  if (!entry.valid())
    return false;
  if (entry.length()!=0x78) {
    MWAW_DEBUG_MSG(("MindWrtParser::readPrintInfo: the size seems odd\n"));
    if (entry.length() < 0x78)
      return false;
  }
  if (entry.isParsed()) {
    MWAW_DEBUG_MSG(("MindWrtParser::readPrintInfo: entry is already parsed\n"));
    return true;
  }
  MWAWInputStreamPtr input = getInput();
  long pos = entry.begin();
  input->seek(pos, librevenge::RVNG_SEEK_SET);

  libmwaw::DebugStream f;
  // print info
  libmwaw::PrinterInfo info;
  if (!info.read(input)) return false;
  f << "Entries(PrintInfo):"<< info;

  MWAWVec2i paperSize = info.paper().size();
  MWAWVec2i pageSize = info.page().size();
  if (pageSize.x() <= 0 || pageSize.y() <= 0 ||
      paperSize.x() <= 0 || paperSize.y() <= 0) return false;

  // define margin from print info
  MWAWVec2i lTopMargin= -1 * info.paper().pos(0);
  MWAWVec2i rBotMargin=info.paper().size() - info.page().size();

  // move margin left | top
  int decalX = lTopMargin.x() > 14 ? lTopMargin.x()-14 : 0;
  int decalY = lTopMargin.y() > 14 ? lTopMargin.y()-14 : 0;
  lTopMargin -= MWAWVec2i(decalX, decalY);
  rBotMargin += MWAWVec2i(decalX, decalY);

  // decrease right | bottom
  int rightMarg = rBotMargin.x() -50;
  if (rightMarg < 0) rightMarg=0;
  int botMarg = rBotMargin.y() -50;
  if (botMarg < 0) botMarg=0;

  getPageSpan().setMarginTop(lTopMargin.y()/72.0);
  getPageSpan().setMarginBottom(botMarg/72.0);
  getPageSpan().setMarginLeft(lTopMargin.x()/72.0);
  getPageSpan().setMarginRight(rightMarg/72.0);
  getPageSpan().setFormLength(paperSize.y()/72.);
  getPageSpan().setFormWidth(paperSize.x()/72.);

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(pos+0x78, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != pos+0x78) {
    MWAW_DEBUG_MSG(("MindWrtParser::readPrintInfo: file is too short\n"));
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////
// send the text, ...
////////////////////////////////////////////////////////////
void MindWrtParser::sendText(std::string const &text, std::vector<MWAWFont> const &fonts, std::vector<int> const &textPos)
{
  if (!getTextListener() || !text.length())
    return;
  size_t numFonts = fonts.size();
  if (numFonts != textPos.size()) {
    MWAW_DEBUG_MSG(("MindWrtParser::sendText: find fonts/textPos incompatibility\n"));
    if (numFonts > textPos.size())
      numFonts = textPos.size();
  }
  size_t actFontId = 0;
  size_t numChar = text.length();
  for (size_t c = 0; c < numChar; c++) {
    if (actFontId < numFonts && int(c) == textPos[actFontId])
      getTextListener()->setFont(fonts[actFontId++]);
    unsigned char ch = (unsigned char)text[c];
    switch (ch) {
    case 0x9:
      getTextListener()->insertTab();
      break;
    case 0xd:
      getTextListener()->insertEOL(c!=numChar-1);
      break;
    default:
      getTextListener()->insertCharacter((unsigned char) ch);
      break;
    }
  }
}

void MindWrtParser::setProperty(MWAWParagraph const &para)
{
  if (!getTextListener()) return;
  getTextListener()->setParagraph(para);
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
