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

#include <librevenge/librevenge.h>

#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWGraphicListener.hxx"
#include "MWAWListener.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWParser.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSection.hxx"

#include "ClarisWksDocument.hxx"
#include "ClarisWksStruct.hxx"
#include "ClarisWksStyleManager.hxx"

#include "ClarisWksText.hxx"

/** Internal: the structures of a ClarisWksText */
namespace ClarisWksTextInternal
{
/** the different plc type */
enum PLCType { P_Font,  P_Ruler, P_Child, P_Section, P_TextZone, P_Token, P_Unknown};

/** Internal : the different plc types: mainly for debugging */
struct PLC {
  /// the constructor
  PLC() : m_type(P_Unknown), m_id(-1), m_extra("")
  {
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
  switch (plc.m_type) {
  case P_Font:
    o << "F";
    break;
  case P_Ruler:
    o << "R";
    break;
  case P_Child:
    o << "C";
    break;
  case P_Section:
    o << "S";
    break;
  case P_TextZone:
    o << "TZ";
    break;
  case P_Token:
    o << "Tok";
    break;
  case P_Unknown:
  default:
    o << "#Unkn";
    break;
  }
  if (plc.m_id >= 0) o << plc.m_id;
  else o << "_";
  if (plc.m_extra.length()) o << ":" << plc.m_extra;
  return o;
}
/** Internal: class to store the paragraph properties */
struct Paragraph : public MWAWParagraph {
  //! Constructor
  Paragraph() : MWAWParagraph(), m_labelType(0)
  {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Paragraph const &ind)
  {
    o << static_cast<MWAWParagraph const &>(ind) << ",";
    static char const *(labelNames[]) = {
      "none", "diamond", "bullet", "checkbox", "hardvard", "leader", "legal",
      "upperalpha", "alpha", "numeric", "upperroman", "roman"
    };
    if (ind.m_labelType > 0 && ind.m_labelType < 12)
      o << "label=" << labelNames[ind.m_labelType] << ",";
    else if (ind.m_labelType)
      o << "#labelType=" << ind.m_labelType << ",";
    return o;
  }
  //! update the list level
  void updateListLevel();
  //! the label
  int m_labelType;
};

void Paragraph::updateListLevel()
{
  int extraLevel = m_labelType!=0 ? 1 : 0;
  if (*m_listLevelIndex+extraLevel<=0)
    return;
  int lev = *m_listLevelIndex+extraLevel;
  m_listLevelIndex = lev;
  MWAWListLevel theLevel;
  theLevel.m_labelWidth=0.2;
  switch (m_labelType) {
  case 0:
    theLevel.m_type = MWAWListLevel::NONE;
    break;
  case 1: // diamond
    theLevel.m_type = MWAWListLevel::BULLET;
    libmwaw::appendUnicode(0x25c7, theLevel.m_bullet);
    break;
  case 3: // checkbox
    theLevel.m_type = MWAWListLevel::BULLET;
    libmwaw::appendUnicode(0x2610, theLevel.m_bullet);
    break;
  case 4: {
    theLevel.m_suffix = (lev <= 3) ? "." : ")";
    if (lev == 1) theLevel.m_type = MWAWListLevel::UPPER_ROMAN;
    else if (lev == 2) theLevel.m_type = MWAWListLevel::UPPER_ALPHA;
    else if (lev == 3) theLevel.m_type = MWAWListLevel::DECIMAL;
    else if (lev == 4) theLevel.m_type =  MWAWListLevel::LOWER_ALPHA;
    else if ((lev%3)==2) {
      theLevel.m_prefix = "(";
      theLevel.m_type = MWAWListLevel::DECIMAL;
    }
    else if ((lev%3)==0) {
      theLevel.m_prefix = "(";
      theLevel.m_type = MWAWListLevel::LOWER_ALPHA;
    }
    else
      theLevel.m_type = MWAWListLevel::LOWER_ROMAN;
    break;
  }
  case 5: // leader
    theLevel.m_type = MWAWListLevel::BULLET;
    theLevel.m_bullet = "+"; // in fact + + and -
    break;
  case 6: // legal
    theLevel.m_type = MWAWListLevel::DECIMAL;
    theLevel.m_numBeforeLabels = lev-1;
    theLevel.m_suffix = ".";
    theLevel.m_labelWidth = 0.2*lev;
    break;
  case 7:
    theLevel.m_type = MWAWListLevel::UPPER_ALPHA;
    theLevel.m_suffix = ".";
    break;
  case 8:
    theLevel.m_type = MWAWListLevel::LOWER_ALPHA;
    theLevel.m_suffix = ".";
    break;
  case 9:
    theLevel.m_type = MWAWListLevel::DECIMAL;
    theLevel.m_suffix = ".";
    break;
  case 10:
    theLevel.m_type = MWAWListLevel::UPPER_ROMAN;
    theLevel.m_suffix = ".";
    break;
  case 11:
    theLevel.m_type = MWAWListLevel::LOWER_ROMAN;
    theLevel.m_suffix = ".";
    break;
  case 2: // bullet
  default:
    theLevel.m_type = MWAWListLevel::BULLET;
    libmwaw::appendUnicode(0x2022, theLevel.m_bullet);
    break;
  }
  m_margins[1]=m_margins[1].get()-theLevel.m_labelWidth;
  m_listLevel=theLevel;
}

struct ParagraphPLC {
  ParagraphPLC() : m_rulerId(-1), m_styleId(-1), m_flags(0), m_extra("")
  {
  }

  friend std::ostream &operator<<(std::ostream &o, ParagraphPLC const &info)
  {
    if (info.m_rulerId >= 0) o << "P" << info.m_rulerId <<",";
    if (info.m_styleId >= 0) o << "LK" << info.m_styleId <<",";
    switch (info.m_flags&3) {
    case 0: // normal
      break;
    case 1:
      o << "hidden,";
      break;
    case 2:
      o << "collapsed,";
      break;
    default:
      o<< "hidden/collapsed,";
      break;
    }
    if (info.m_flags&4)
      o << "flags4,";
    static char const *(labelNames[]) = {
      "none", "diamond", "bullet", "checkbox", "hardvard", "leader", "legal",
      "upperalpha", "alpha", "numeric", "upperroman", "roman"
    };

    int listType=int((info.m_flags>>3)&0xF);
    if (listType>0 && listType < 12)
      o << labelNames[listType] << ",";
    else if (listType)
      o << "#listType=" << listType << ",";
    if (info.m_flags&0x80) o << "flags80,";
    int listLevel=int((info.m_flags>>8)&0xF);
    if (listLevel) o << "level=" << listLevel+1;
    if (info.m_flags>>12) o << "flags=" << std::hex << (info.m_flags>>12) << std::dec << ",";
    if (info.m_extra.length()) o << info.m_extra;
    return o;
  }

  /** the ruler id */
  int m_rulerId;
  /** the style id ( via the style lookup table )*/
  int m_styleId;
  /** some flags */
  int m_flags;
  /** extra data */
  std::string m_extra;
};

/** internal class used to store a section */
struct Section {
  //! the constructor
  Section() : m_pos(0), m_numColumns(1), m_columnsWidth(), m_columnsSep(), m_startOnNewPage(false),
    m_firstPage(0), m_hasTitlePage(false), m_continuousHF(true), m_leftRightHF(false), m_extra("")
  {
    for (int i=0; i<4; ++i) m_HFId[i]=0;
  }
  //! returns a section
  MWAWSection getSection() const
  {
    MWAWSection sec;
    if (m_numColumns <= 1)
      return sec;
    size_t numCols = m_columnsWidth.size();
    if (m_numColumns != int(numCols)) {
      MWAW_DEBUG_MSG(("ClarisWksTextInternal::Section::getSection: unexpected number of columns\n"));
      return sec;
    }
    bool hasSep = numCols==m_columnsSep.size();
    if (!hasSep && m_columnsSep.size()) {
      MWAW_DEBUG_MSG(("ClarisWksTextInternal::Section::getSection: can not used column separator\n"));
      return sec;
    }
    sec.m_columns.resize(size_t(numCols));
    for (size_t c=0; c < numCols; c++) {
      sec.m_columns[c].m_width = double(m_columnsWidth[c]);
      sec.m_columns[c].m_widthUnit = librevenge::RVNG_POINT;
      if (!hasSep) continue;
      sec.m_columns[c].m_margins[libmwaw::Left]=
        double(m_columnsSep[c])/72.*(c==0 ? 1. : 0.5);
      if (c+1!=numCols)
        sec.m_columns[c].m_margins[libmwaw::Right]=double(m_columnsSep[c+1])/2.0/72.;
    }
    return sec;
  }
  //! operator <<
  friend std::ostream &operator<<(std::ostream &o, Section const &sec)
  {
    o << "pos=" << sec.m_pos << ",";
    if (sec.m_numColumns != 1) o << "numCols=" << sec.m_numColumns << ",";
    o << "col[width]=[";
    for (size_t c = 0; c < sec.m_columnsWidth.size(); c++)
      o << sec.m_columnsWidth[c] << ",";
    o << "],";
    if (sec.m_columnsSep.size()) {
      o << "col[sepW]=[";
      for (size_t c = 0; c < sec.m_columnsSep.size(); c++)
        o << sec.m_columnsSep[c] << ",";
      o << "],";
    }
    if (sec.m_firstPage) o << "first[page]=" << sec.m_firstPage << ",";
    if (sec.m_hasTitlePage) o << "title[page],";
    if (sec.m_continuousHF) o << "continuousHF,";
    if (sec.m_leftRightHF) o << "leftRightHF,";
    if (sec.m_HFId[0]) o << "id[header]=" << sec.m_HFId[0] << ",";
    if (sec.m_HFId[1] || sec.m_HFId[0]!=sec.m_HFId[1]) o << "id[header2]=" << sec.m_HFId[1] << ",";
    if (sec.m_HFId[2]) o << "id[footer]=" << sec.m_HFId[2] << ",";
    if (sec.m_HFId[3] || sec.m_HFId[2]!=sec.m_HFId[3]) o << "id[footer2]=" << sec.m_HFId[3] << ",";
    if (sec.m_extra.length()) o << sec.m_extra;
    return o;
  }
  /** the character position */
  long m_pos;
  /** the number of column */
  int m_numColumns;
  /** the columns width */
  std::vector<int> m_columnsWidth;
  /** the columns separator */
  std::vector<int> m_columnsSep;
  /** a new section generates a page break */
  bool m_startOnNewPage;
  /** the first page */
  int m_firstPage;
  /** true if the first page is a title page(ie. no header/footer) */
  bool m_hasTitlePage;
  /** true if the header/footer are shared with previous sections */
  bool m_continuousHF;
  /** true if the left/right header/footer are different */
  bool m_leftRightHF;
  /** the header/footer id*/
  int m_HFId[4];
  /** a string to store unparsed data */
  std::string m_extra;
};

/** internal class used to store a text zone */
struct TextZoneInfo {
  //! constructor
  TextZoneInfo() : m_pos(0), m_N(0), m_extra("")
  {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, TextZoneInfo const &info)
  {
    o << "pos=" << info.m_pos << ",";
    if (info.m_N >= 0) o << "size=" << info.m_N <<",";
    if (info.m_extra.length()) o << info.m_extra;
    return o;
  }
  long m_pos;
  //! the number of character
  int m_N;
  //! extra data
  std::string m_extra;
};

enum TokenType { TKN_UNKNOWN, TKN_FOOTNOTE, TKN_PAGENUMBER, TKN_GRAPHIC, TKN_FIELD };

/** Internal: class to store field definition: TOKN entry*/
struct Token {
  //! constructor
  Token() : m_type(TKN_UNKNOWN), m_zoneId(-1), m_page(-1), m_descent(0), m_fieldEntry(), m_extra("")
  {
    for (int i = 0; i < 3; i++) m_unknown[i] = 0;
    for (int i = 0; i < 2; i++) m_size[i] = 0;
  }
  //! operator <<
  friend std::ostream &operator<<(std::ostream &o, Token const &tok);
  //! the type
  TokenType m_type;
  //! the zone id which correspond to this type
  int m_zoneId;
  //! the page
  int m_page;
  //! the size(?)
  int m_size[2];
  //! the descent
  int m_descent;
  //! the field name entry
  MWAWEntry m_fieldEntry;
  //! the unknown zone
  int m_unknown[3];
  //! a string used to store the parsing errors
  std::string m_extra;
};
//! operator<< for Token
std::ostream &operator<<(std::ostream &o, Token const &tok)
{
  switch (tok.m_type) {
  case TKN_FOOTNOTE:
    o << "footnoote,";
    break;
  case TKN_FIELD:
    o << "field[linked],";
    break;
  case TKN_PAGENUMBER:
    switch (tok.m_unknown[0]) {
    case 0:
      o << "field[pageNumber],";
      break;
    case 1:
      o << "field[sectionNumber],";
      break;
    case 2:
      o << "field[sectionInPageNumber],";
      break;
    case 3:
      o << "field[pageCount],";
      break;
    default:
      o << "field[pageNumber=#" << tok.m_unknown[0] << "],";
      break;
    }
    break;
  case TKN_GRAPHIC:
    o << "graphic,";
    break;
  case TKN_UNKNOWN:
  default:
    o << "##field[unknown]" << ",";
    break;
  }
  if (tok.m_zoneId != -1) o << "zoneId=" << tok.m_zoneId << ",";
  if (tok.m_page != -1) o << "page?=" << tok.m_page << ",";
  o << "pos?=" << tok.m_size[0] << "x" << tok.m_size[1] << ",";
  if (tok.m_descent) o << "descent=" << tok.m_descent << ",";
  for (int i = 0; i < 3; i++) {
    if (tok.m_unknown[i] == 0 || (i==0 && tok.m_type==TKN_PAGENUMBER))
      continue;
    o << "#unkn" << i << "=" << std::hex << tok.m_unknown[i] << std::dec << ",";
  }
  if (!tok.m_extra.empty()) o << "err=[" << tok.m_extra << "]";
  return o;
}

struct Zone : public ClarisWksStruct::DSET {
  Zone(ClarisWksStruct::DSET const &dset = ClarisWksStruct::DSET()) :
    ClarisWksStruct::DSET(dset), m_zones(), m_numChar(0), m_numTextZone(0), m_numParagInfo(0),
    m_numFont(0), m_fatherId(0), m_unknown(0), m_fontList(), m_paragraphList(),
    m_sectionList(), m_tokenList(), m_textZoneList(), m_plcMap()
  {
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Zone const &doc)
  {
    o << static_cast<ClarisWksStruct::DSET const &>(doc);
    if (doc.m_numChar) o << "numChar=" << doc.m_numChar << ",";
    if (doc.m_numTextZone) o << "numTextZone=" << doc.m_numTextZone << ",";
    if (doc.m_numParagInfo) o << "numParag=" << doc.m_numParagInfo << ",";
    if (doc.m_numFont) o << "numFont=" << doc.m_numFont << ",";
    if (doc.m_fatherId) o << "id[father]=" << doc.m_fatherId << ",";
    if (doc.m_unknown) o << "unkn=" << doc.m_unknown << ",";
    return o;
  }

  /** remove a child from a list.

      Normally, this function is not called, so optimizing it is not usefull
   */
  virtual void removeChild(int cId, bool normalChild)
  {
    DSET::removeChild(cId, normalChild);
    for (size_t i=0; i<m_tokenList.size(); ++i) {
      if (m_tokenList[i].m_zoneId!=cId) continue;
      m_tokenList[i].m_zoneId=0;
      return;
    }
    // normally, section list point only to the text zone (ie. the
    // child of the header/footer group), so remove child is not
    // called on it
    MWAW_DEBUG_MSG(("ClarisWksTextInternal::Zone can not detach %d\n", cId));
  }

  std::vector<MWAWEntry> m_zones; // the text zones
  int m_numChar /** the number of char in text zone */;
  int m_numTextZone /** the number of text zone ( ie. number of page ? ) */;
  int m_numParagInfo /** the number of paragraph info */;
  int m_numFont /** the number of font */;
  int m_fatherId /** the father id */;
  int m_unknown /** an unknown flags */;

  std::vector<MWAWFont> m_fontList /** the list of fonts */;
  std::vector<ParagraphPLC> m_paragraphList /** the list of paragraph */;
  std::vector<Section> m_sectionList /** the list of section */;
  std::vector<Token> m_tokenList /** the list of token */;
  std::vector<TextZoneInfo> m_textZoneList /** the list of zone */;
  std::multimap<long, PLC> m_plcMap /** the plc map */;
};
////////////////////////////////////////
//! Internal: the state of a ClarisWksText
struct State {
  //! constructor
  State() : m_version(-1), m_paragraphsList(), m_zoneMap()
  {
  }

  //! the file version
  mutable int m_version;
  //! the list of paragraph
  std::vector<Paragraph> m_paragraphsList;
  //! the list of text zone
  std::map<int, shared_ptr<Zone> > m_zoneMap;
};

////////////////////////////////////////
//! Internal: the subdocument of a ClarisWksDocument
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(ClarisWksText &parser, MWAWInputStreamPtr input, int zoneId) :
    MWAWSubDocument(0, input, MWAWEntry()), m_textParser(parser), m_id(zoneId) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const
  {
    if (MWAWSubDocument::operator!=(doc)) return true;
    SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
    if (!sDoc) return true;
    if (&m_textParser != &sDoc->m_textParser) return true;
    if (m_id != sDoc->m_id) return true;
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
  //! the document manager
  ClarisWksText &m_textParser;
  //! the subdocument id
  int m_id;
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("ClarisWksTextInternal::SubDocument::parse: no listener\n"));
    return;
  }
  if (m_id == -1) { // a number used to send linked frame
    listener->insertChar(' ');
    return;
  }
  if (m_id == 0) {
    MWAW_DEBUG_MSG(("ClarisWksTextInternal::SubDocument::parse: unknown zone\n"));
    return;
  }

  m_textParser.m_document.sendZone(m_id, listener);
}

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
ClarisWksText::ClarisWksText(ClarisWksDocument &document) :
  m_document(document), m_parserState(document.m_parserState), m_state(new ClarisWksTextInternal::State),
  m_mainParser(&document.getMainParser())
{
}

ClarisWksText::~ClarisWksText()
{ }

int ClarisWksText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}

int ClarisWksText::numPages() const
{
  std::map<int, shared_ptr<ClarisWksTextInternal::Zone> >::iterator iter
    = m_state->m_zoneMap.find(1);
  if (iter == m_state->m_zoneMap.end())
    return 1;
  int numPage = 1;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  for (size_t i = 0; i < iter->second->m_zones.size(); i++) {
    MWAWEntry const &entry = iter->second->m_zones[i];
    input->seek(entry.begin()+4, librevenge::RVNG_SEEK_SET);
    int numC = int(entry.length()-4);
    for (int ch = 0; ch < numC; ch++) {
      char c = (char) input->readULong(1);
      if (c==0xb || c==0x1)
        numPage++;
    }
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  return numPage;
}

bool ClarisWksText::updatePageSpanList(MWAWPageSpan const &page, std::vector<MWAWPageSpan> &spanList)
{
  if (m_state->m_zoneMap.find(1)==m_state->m_zoneMap.end() || !m_state->m_zoneMap.find(1)->second
      || m_parserState->m_kind==MWAWDocument::MWAW_K_PRESENTATION)
    return false;
  ClarisWksTextInternal::Zone const &zone=*m_state->m_zoneMap.find(1)->second;
  size_t numSection=zone.m_sectionList.size();
  if (!numSection) return false;
  int nPages=m_document.numPages();
  int actPage=0;
  spanList.resize(0);
  for (size_t i=0; i<numSection; ++i) {
    ClarisWksTextInternal::Section const &sec=zone.m_sectionList[i];
    int lastPage=nPages;
    bool ok=true;
    while (i+1<numSection) {
      if (zone.m_sectionList[i+1].m_continuousHF) {
        ++i;
        continue;
      }
      if (zone.m_sectionList[i+1].m_firstPage<actPage) {
        MWAW_DEBUG_MSG(("ClarisWksText::updatePageSpanList: problem with the %d first page\n", int(i+1)));
        ok=false;
        break;
      }
      lastPage=zone.m_sectionList[i+1].m_firstPage;
      break;
    }
    if (!ok)
      break;
    if (lastPage>nPages) {
      MWAW_DEBUG_MSG(("ClarisWksText::updatePageSpanList: some first page seems to big\n"));
      lastPage=nPages;
    }
    if (sec.m_hasTitlePage && actPage<lastPage) {
      // title page have no header/footer
      MWAWPageSpan ps(page);
      ps.setPageSpan(1);
      spanList.push_back(ps);
      ++actPage;
    }
    if (actPage<lastPage) {
      MWAWPageSpan ps(page);
      ps.setPageSpan(lastPage-actPage);
      for (int j=0; j<4; ++j) {
        int zId=sec.m_HFId[j];
        if (!zId) continue;
        if ((j%2)==1 && zId==sec.m_HFId[j-1]) continue;
        /* try to retrieve the father group zone */
        if (m_state->m_zoneMap.find(zId)!=m_state->m_zoneMap.end() &&
            m_state->m_zoneMap.find(zId)->second && m_state->m_zoneMap.find(zId)->second->m_fatherId)
          zId=m_state->m_zoneMap.find(zId)->second->m_fatherId;
        MWAWHeaderFooter hF(j<2 ? MWAWHeaderFooter::HEADER : MWAWHeaderFooter::FOOTER,
                            (j%2) ? MWAWHeaderFooter::EVEN : sec.m_HFId[j]==sec.m_HFId[j+1] ?
                            MWAWHeaderFooter::ALL : MWAWHeaderFooter::ODD);
        hF.m_subDocument.reset(new ClarisWksTextInternal::SubDocument(*this, m_parserState->m_input, zId));
        ps.setHeaderFooter(hF);
      }
      spanList.push_back(ps);
    }
    actPage=lastPage;
  }
  if (actPage<nPages) {
    MWAWPageSpan ps(page);
    ps.setPageSpan(nPages-actPage);
    spanList.push_back(ps);
  }
  return true;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// a document part
////////////////////////////////////////////////////////////
shared_ptr<ClarisWksStruct::DSET> ClarisWksText::readDSETZone(ClarisWksStruct::DSET const &zone, MWAWEntry const &entry, bool &complete)
{
  complete = false;
  if (!entry.valid() || zone.m_fileType != 1)
    return shared_ptr<ClarisWksStruct::DSET>();
  int const vers = version();
  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos+8+16, librevenge::RVNG_SEEK_SET); // avoid header+8 generic number
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(DSETT):";

  shared_ptr<ClarisWksTextInternal::Zone> textZone(new ClarisWksTextInternal::Zone(zone));
  textZone->m_unknown = (int) input->readULong(2); // alway 0 ?
  textZone->m_fatherId = (int) input->readULong(2);
  textZone->m_numChar = (int) input->readULong(4);
  textZone->m_numTextZone = (int) input->readULong(2);
  textZone->m_numParagInfo = (int) input->readULong(2);
  textZone->m_numFont = (int) input->readULong(2);
  switch (textZone->m_textType >> 4) {
  case 2:
    textZone->m_position = ClarisWksStruct::DSET::P_Header;
    break;
  case 4:
    textZone->m_position = ClarisWksStruct::DSET::P_Footer;
    break;
  case 6:
    textZone->m_position = ClarisWksStruct::DSET::P_Footnote;
    break;
  case 8:
    textZone->m_position = ClarisWksStruct::DSET::P_Frame;
    break;
  case 0xe:
    textZone->m_position = ClarisWksStruct::DSET::P_Table;
    break;
  case 0:
    if (zone.m_id==1) {
      textZone->m_position = ClarisWksStruct::DSET::P_Main;
      break;
    }
  // fail through intended
  default:
    MWAW_DEBUG_MSG(("ClarisWksText::readDSETZone: find unknown position %d\n", (textZone->m_textType >> 4)));
    f << "#position="<< (textZone->m_textType >> 4) << ",";
    break;
  }
  // find 2,3,6,a,b,e,f
  if (textZone->m_textType != ClarisWksStruct::DSET::P_Unknown)
    textZone->m_textType &= 0xF;
  f << *textZone << ",";

  if (long(input->tell())%2)
    input->seek(1, librevenge::RVNG_SEEK_CUR);
  ascFile.addDelimiter(input->tell(), '|');
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  // read the last part
  int data0Length = 0;
  switch (vers) {
  case 1:
    data0Length = 24;
    break;
  case 2:
    data0Length = 28;
    break;
  // case 3: ???
  case 4:
  case 5:
  case 6:
    data0Length = 30;
    break;
  default:
    break;
  }

  int N = int(zone.m_numData);
  if (long(input->tell())+N*data0Length > entry.end()) {
    MWAW_DEBUG_MSG(("ClarisWksText::readDSETZone: file is too short\n"));
    return shared_ptr<ClarisWksStruct::DSET>();
  }

  input->seek(entry.end()-N*data0Length, librevenge::RVNG_SEEK_SET);
  ClarisWksTextInternal::PLC plc;
  plc.m_type = ClarisWksTextInternal::P_Child;
  int numExtraHId=0;
  if (data0Length) {
    for (int i = 0; i < N; i++) {
      /* definition of a list of text zone ( one by column and one by page )*/
      pos = input->tell();
      f.str("");
      f << "DSETT-" << i << ":";
      ClarisWksStruct::DSET::Child child;
      child.m_posC = (long) input->readULong(4);
      child.m_type = ClarisWksStruct::DSET::C_SubText;
      int dim[2];
      for (int j = 0; j < 2; j++)
        dim[j] = (int) input->readLong(2);
      child.m_box = MWAWBox2i(MWAWVec2i(0,0), MWAWVec2i(dim[0], dim[1]));
      textZone->m_childs.push_back(child);
      plc.m_id = i;
      textZone->m_plcMap.insert(std::map<long, ClarisWksTextInternal::PLC>::value_type(child.m_posC, plc));

      f << child;
      f << "ptr=" << std::hex << input->readULong(4) << std::dec << ",";
      f << "f0=" << input->readLong(2) << ","; // a small number : number of line ?
      f << "y[real]=" << input->readLong(2) << ",";
      for (int j = 1; j < 4; j++) {
        int val = (int) input->readLong(2);
        if (val)
          f << "f" << j << "=" << val << ",";
      }
      int order = (int) input->readLong(2);
      // simple id or 0: main text ?, 1 : header/footnote ?, 2: footer => id or order?
      if (order)
        f << "order?=" << order << ",";

      if (vers>=2) {
        long id=(int) input->readULong(4);
        if (id) {
          f << "ID=" << std::hex << id << std::dec << ",";
          ++numExtraHId;
        }
      }
      long actPos = input->tell();
      if (actPos != pos && actPos != pos+data0Length)
        ascFile.addDelimiter(input->tell(),'|');
      input->seek(pos+data0Length, librevenge::RVNG_SEEK_SET);

      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
  }

  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);

  // now normally three zones: paragraph, font, token
  bool ok = true;
  for (int z = 0; z < 4+textZone->m_numTextZone; z++) {
    pos = input->tell();
    long sz = (long) input->readULong(4);
    if (!sz) {
      f.str("");
      f << "DSETT-Z" << z;
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      continue;
    }

    MWAWEntry zEntry;
    zEntry.setBegin(pos);
    zEntry.setLength(sz+4);

    if (!input->checkPosition(zEntry.end())) {
      MWAW_DEBUG_MSG(("ClarisWksText::readDSETZone: entry for %d zone is too short\n", z));
      ascFile.addPos(pos);
      ascFile.addNote("###");
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      if (z > 4) {
        ok = false;
        break;
      }
      return textZone;
    }

    switch (z) {
    case 0:
      ok = readParagraphs(zEntry, *textZone);
      break;
    case 1:
      ok = readFonts(zEntry, *textZone);
      break;
    case 2:
      ok = readTokens(zEntry, *textZone);
      break;
    case 3:
      ok = readTextZoneSize(zEntry, *textZone);
      break;
    default:
      textZone->m_zones.push_back(zEntry);
      break;
    }
    if (!ok) {
      if (z >= 4) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        MWAW_DEBUG_MSG(("ClarisWksText::readDSETZone: can not find text %d zone\n", z-4));
        if (z > 4) break;
        return textZone;
      }
      f.str("");
      f << "DSETT-Z" << z << "#";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    if (input->tell() < zEntry.end() || !ok)
      input->seek(zEntry.end(), librevenge::RVNG_SEEK_SET);
  }

  if (ok && vers >= 2) {
    pos = input->tell();
    if (!readTextSection(*textZone))
      input->seek(pos, librevenge::RVNG_SEEK_SET);
  }
  for (int i=0; ok && i<numExtraHId; ++i) {
    pos=input->tell();
    long sz=(long) input->readULong(4);
    if (sz<10 || !input->checkPosition(pos+4+sz)) {
      MWAW_DEBUG_MSG(("ClarisWksText::readDSETZone:: can not read an extra block\n"));
      ascFile.addPos(pos);
      ascFile.addNote("DSETT-extra:###");
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      ok=false;
      break;
    }
    f.str("");
    f << "DSETT-extra:";
    /* Checkme: no sure how to read this unfrequent structures */
    int val=(int) input->readLong(2); // 2 (with size=34 or 4c)|a(with size=a or e)|3c (with size 3c)
    f << "type?=" << val << ",";
    int dim[4];
    for (int j=0; j<4; ++j) dim[j]=(int) input->readLong(2);
    f << "dim=" << dim[1] << "x" << dim[0] << "<->" << dim[3] << "x" << dim[2] << ",";
    if (sz!=10) ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+4+sz, librevenge::RVNG_SEEK_SET);
  }
  for (size_t tok = 0; tok < textZone->m_tokenList.size(); tok++) {
    ClarisWksTextInternal::Token const &token = textZone->m_tokenList[tok];
    if (token.m_zoneId > 0)
      textZone->m_otherChilds.push_back(token.m_zoneId);
  }

  if (m_state->m_zoneMap.find(textZone->m_id) != m_state->m_zoneMap.end()) {
    MWAW_DEBUG_MSG(("ClarisWksText::readDSETZone: zone %d already exists!!!\n", textZone->m_id));
  }
  else
    m_state->m_zoneMap[textZone->m_id] = textZone;

  if (ok) {
    // look for unparsed zone
    pos=input->tell();
    long sz=(long) input->readULong(4);
    if (input->checkPosition(pos+4+sz)) {
      if (sz) {
        MWAW_DEBUG_MSG(("ClarisWksText::readDSETZone:: find some extra block\n"));
        input->seek(pos+4+sz, librevenge::RVNG_SEEK_SET);
        ascFile.addPos(pos);
        ascFile.addNote("Entries(TextEnd):###");
      }
      else {
        // probably a problem, but...
        ascFile.addPos(pos);
        ascFile.addNote("_");
      }
    }
    else
      input->seek(pos, librevenge::RVNG_SEEK_SET);
  }
  complete = ok;
  return textZone;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

bool ClarisWksText::readFont(int id, int &posC, MWAWFont &font)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();

  int fontSize = 0;
  int vers = version();
  switch (vers) {
  case 1:
  case 2:
  case 3:
    fontSize = 10;
    break;
  case 4:
  case 5:
    fontSize = 12;
    break;
  case 6:
    fontSize = 18;
    break;
  default:
    break;
  }
  if (fontSize == 0)
    return false;

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  if (!input->checkPosition(pos+fontSize)) {
    MWAW_DEBUG_MSG(("ClarisWksText::readFont: file is too short"));
    return false;
  }
  posC = int(input->readULong(4));
  font = MWAWFont();
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  if (id >= 0)
    f << "Font-F" << id << ":";
  else
    f << "Font:";

  f << "pos=" << posC << ",";
  font.setId(m_document.getStyleManager()->getFontId((int) input->readULong(2)));
  int flag =(int) input->readULong(2);
  uint32_t flags=0;
  if (flag&0x1) flags |= MWAWFont::boldBit;
  if (flag&0x2) flags |= MWAWFont::italicBit;
  if (flag&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
  if (flag&0x8) flags |= MWAWFont::embossBit;
  if (flag&0x10) flags |= MWAWFont::shadowBit;
  if (flag&0x20) font.setDeltaLetterSpacing(-1);
  if (flag&0x40) font.setDeltaLetterSpacing(1);
  if (flag&0x80) font.setStrikeOutStyle(MWAWFont::Line::Simple);
  if (flag&0x100) font.set(MWAWFont::Script::super100());
  if (flag&0x200) font.set(MWAWFont::Script::sub100());
  if (flag&0x400) font.set(MWAWFont::Script::super());
  if (flag&0x800) font.set(MWAWFont::Script::sub());
  if (flag&0x2000) {
    font.setUnderlineStyle(MWAWFont::Line::Simple);
    font.setUnderlineType(MWAWFont::Line::Double);
  }
  font.setSize((float) input->readULong(1));

  int colId = (int) input->readULong(1);
  MWAWColor color(MWAWColor::black());
  if (colId!=1) {
    MWAWColor col;
    if (m_document.getStyleManager()->getColor(colId, col))
      color = col;
    else if (vers != 1) {
      MWAW_DEBUG_MSG(("ClarisWksText::readFont: unknown color %d\n", colId));
    }
    /*V1:
      color  = 1 black, 26 : yellow, 2c: magenta 24 red 29 cyan
      27 green 2a blue 0 white
    */
  }
  if (fontSize >= 12)
    f << "LK" << input->readLong(2) << ",";
  if (fontSize >= 14) {
    flag = (int) input->readULong(2);
    if (flag & 0x1)
      font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (flag & 0x2) {
      font.setUnderlineStyle(MWAWFont::Line::Simple);
      font.setUnderlineType(MWAWFont::Line::Double);
    }
    if (flag & 0x20)
      font.setStrikeOutStyle(MWAWFont::Line::Simple);
    flag &= 0xFFDC;
    if (flag)
      f << "#flag2=" << std::hex << flag << std::dec << ",";
  }
  font.setFlags(flags);
  font.setColor(color);
  f << font.getDebugString(m_parserState->m_fontConverter);
  if (long(input->tell()) != pos+fontSize)
    ascFile.addDelimiter(input->tell(), '|');
  input->seek(pos+fontSize, librevenge::RVNG_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// the fonts properties
////////////////////////////////////////////////////////////
bool ClarisWksText::readFonts(MWAWEntry const &entry, ClarisWksTextInternal::Zone &zone)
{
  long pos = entry.begin();

  int fontSize = 0;
  switch (version()) {
  case 1:
  case 2:
  case 3:
    fontSize = 10;
    break;
  case 4:
  case 5:
    fontSize = 12;
    break;
  case 6:
    fontSize = 18;
    break;
  default:
    break;
  }
  if (fontSize == 0)
    return false;
  if ((entry.length()%fontSize) != 4)
    return false;

  int numElt = int((entry.length()-4)/fontSize);
  long actC = -1;

  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos+4, librevenge::RVNG_SEEK_SET); // skip header
  // first check char pos is ok
  for (int i = 0; i < numElt; i++) {
    pos = input->tell();
    long newC = (long) input->readULong(4);
    if (newC < actC) return false;
    actC = newC;
    input->seek(pos+fontSize, librevenge::RVNG_SEEK_SET);
  }

  pos = entry.begin();
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  ascFile.addPos(pos);
  ascFile.addNote("Entries(Font)");

  input->seek(pos+4, librevenge::RVNG_SEEK_SET); // skip header
  ClarisWksTextInternal::PLC plc;
  plc.m_type = ClarisWksTextInternal::P_Font;
  for (int i = 0; i < numElt; i++) {
    MWAWFont font;
    int posChar;
    if (!readFont(i, posChar, font)) return false;
    zone.m_fontList.push_back(font);
    plc.m_id = i;
    zone.m_plcMap.insert(std::map<long, ClarisWksTextInternal::PLC>::value_type(posChar, plc));
  }

  return true;
}

////////////////////////////////////////////////////////////
// the paragraphs properties
////////////////////////////////////////////////////////////
bool ClarisWksText::readParagraphs(MWAWEntry const &entry, ClarisWksTextInternal::Zone &zone)
{
  long pos = entry.begin();

  int const vers = version();
  int styleSize = vers==1 ? 6 : 8;
  if ((entry.length()%styleSize) != 4)
    return false;

  int numElt = int((entry.length()-4)/styleSize);
  long actC = -1;

  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos+4, librevenge::RVNG_SEEK_SET); // skip header
  // first check char pos is ok
  for (int i = 0; i < numElt; i++) {
    pos = input->tell();
    long newC = (long) input->readULong(4);
    if (newC < actC) return false;
    actC = newC;
    input->seek(pos+styleSize, librevenge::RVNG_SEEK_SET);
  }

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  pos = entry.begin();
  ascFile.addPos(pos);
  ascFile.addNote("Entries(ParaPLC)");

  libmwaw::DebugStream f;
  input->seek(pos+4, librevenge::RVNG_SEEK_SET); // skip header
  ClarisWksTextInternal::PLC plc;
  plc.m_type = ClarisWksTextInternal::P_Ruler;
  for (int i = 0; i < numElt; i++) {
    pos = input->tell();
    ClarisWksTextInternal::ParagraphPLC info;

    long posC = (long) input->readULong(4);
    f.str("");
    f << "ParaPLC-R" << i << ": pos=" << posC << ",";
    info.m_rulerId = (int) input->readLong(2);
    if (styleSize >= 8)
      info.m_flags = (int) input->readLong(2);

    if (vers > 2) {
      info.m_styleId = info.m_rulerId;
      ClarisWksStyleManager::Style style;
      if (m_document.getStyleManager()->get(info.m_rulerId, style)) {
        info.m_rulerId = style.m_rulerId;
#if 0
        f << "[style=" << style << "]";
#endif
      }
    }
    f << info;

    if (long(input->tell()) != pos+styleSize)
      ascFile.addDelimiter(input->tell(), '|');
    zone.m_paragraphList.push_back(info);
    plc.m_id = i;
    zone.m_plcMap.insert(std::map<long, ClarisWksTextInternal::PLC>::value_type(posC, plc));
    input->seek(pos+styleSize, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  return true;
}

////////////////////////////////////////////////////////////
// zone which corresponds to the token
////////////////////////////////////////////////////////////
bool ClarisWksText::readTokens(MWAWEntry const &entry, ClarisWksTextInternal::Zone &zone)
{
  long pos = entry.begin();

  int dataSize = 0;
  int const vers=version();
  switch (vers) {
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
    dataSize = 32;
    break;
  case 6:
    dataSize = 36;
    break;
  default:
    break;
  }
  if (!dataSize || (entry.length()%dataSize) != 4)
    return false;

  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  ascFile.addPos(pos);
  ascFile.addNote("Entries(Token)");

  int numElt = int((entry.length()-4)/dataSize);
  input->seek(pos+4, librevenge::RVNG_SEEK_SET); // skip header

  libmwaw::DebugStream f;
  ClarisWksTextInternal::PLC plc;
  plc.m_type = ClarisWksTextInternal::P_Token;
  int val;
  std::vector<int> fieldList;
  for (int i = 0; i < numElt; i++) {
    pos = input->tell();

    int posC = (int) input->readULong(4);
    ClarisWksTextInternal::Token token;

    int type = (int) input->readLong(2);
    f.str("");
    switch (type) {
    case 0:
      token.m_type = ClarisWksTextInternal::TKN_FOOTNOTE;
      break;
    case 1:
      token.m_type = ClarisWksTextInternal::TKN_GRAPHIC;
      break;
    case 2:
      /* find in v4-v6, does not seem to exist in v1-v2 */
      token.m_type = ClarisWksTextInternal::TKN_PAGENUMBER;
      break;
    case 3:
      token.m_type = ClarisWksTextInternal::TKN_FIELD;
      fieldList.push_back(i);
      break;
    default:
      f << "#type=" << type << ",";
      break;
    }

    token.m_unknown[0] = (int) input->readLong(2);
    token.m_zoneId = (int) input->readLong(2);
    token.m_unknown[1] = (int) input->readLong(1);
    token.m_page = (int) input->readLong(1);
    token.m_unknown[2] = (int) input->readLong(2);
    for (int j = 0; j < 2; j++)
      token.m_size[1-j] = (int) input->readLong(2);
    for (int j = 0; j < 3; j++) {
      val = (int) input->readLong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }
    val = (int) input->readLong(2);
    if (vers>=6) // checkme: ok for v6 & graphic, not for v2
      token.m_descent = val;
    else if (val)
      f << "f3=" << val << ",";
    token.m_extra = f.str();
    f.str("");
    f << "Token-" << i << ": pos=" << posC << "," << token;
    zone.m_tokenList.push_back(token);
    plc.m_id = i;
    zone.m_plcMap.insert(std::map<long, ClarisWksTextInternal::PLC>::value_type(posC, plc));

    if (long(input->tell()) != pos && long(input->tell()) != pos+dataSize)
      ascFile.addDelimiter(input->tell(), '|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+dataSize, librevenge::RVNG_SEEK_SET);
  }

  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  for (size_t i=0; i < fieldList.size(); ++i) {
    pos=input->tell();
    long sz=(long) input->readULong(4);
    f.str("");
    f << "Token[field-" << i << "]:";
    if (!input->checkPosition(pos+sz+4) || long(input->readULong(1))+1!=sz) {
      MWAW_DEBUG_MSG(("ClarisWksText::readTokens: can find token field name %d\n", int(i)));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      return false;
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    MWAWEntry fieldEntry;
    fieldEntry.setBegin(input->tell());
    fieldEntry.setEnd(pos+sz+4);
    zone.m_tokenList[size_t(fieldList[i])].m_fieldEntry=fieldEntry;
    input->seek(fieldEntry.end(), librevenge::RVNG_SEEK_SET);
  }
  return true;
}

// read the different section definition
bool ClarisWksText::readTextSection(ClarisWksTextInternal::Zone &zone)
{
  int const vers = version();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  if (!input->checkPosition(endPos) || (sz && sz < 12)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksText::readTextSection: unexpected size\n"));
    return false;
  }
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  if (sz == 0) {
    ascFile.addPos(pos);
    ascFile.addNote("Nop");
    return true;
  }
  libmwaw::DebugStream f;
  f << "Entries(TextSection):";

  int N = (int) input->readLong(2);
  f << "N=" << N << ",";
  int type = (int) input->readLong(2);
  if (type != -1)
    f << "#type=" << type << ",";
  long val = input->readLong(2);
  if (val) f << "#unkn=" << val << ",";
  int fSz = (int) input->readULong(2);
  int hSz = (int) input->readULong(2);
  if (!fSz || N *fSz+hSz+12 != sz) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksText::readTextSection: unexpected size\n"));
    return false;
  }
  if ((vers > 3 && fSz != 0x4e) || (vers <= 3 && fSz < 60)) {
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksText::readTextSection: unexpected size\n"));
    return true;
  }
  if (long(input->tell()) != pos+4+hSz)
    ascFile.addDelimiter(input->tell(), '|');
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  input->seek(endPos-N*fSz, librevenge::RVNG_SEEK_SET);
  ClarisWksTextInternal::PLC plc;
  plc.m_type = ClarisWksTextInternal::P_Section;
  for (int i= 0; i < N; i++) {
    ClarisWksTextInternal::Section sec;

    pos = input->tell();
    f.str("");
    sec.m_pos  = input->readLong(4);
    sec.m_firstPage= (int) input->readLong(2);
    for (int j = 0; j < 3; j++) {
      /** find f0=O| (for second section)[1|2|4]
      f1=0| (for second section [2e,4e,5b] , f2=0|2d|4d|5a */
      val = input->readLong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }
    sec.m_numColumns  = (int) input->readULong(2);
    if (!sec.m_numColumns || sec.m_numColumns > 10) {
      MWAW_DEBUG_MSG(("ClarisWksText::readTextSection: num columns seems odd\n"));
      f << "#numColumns=" << sec.m_numColumns << ",";
      sec.m_numColumns = 1;
    }
    for (int c = 0; c < sec.m_numColumns; c++)
      sec.m_columnsWidth.push_back((int)input->readULong(2));
    input->seek(pos+32, librevenge::RVNG_SEEK_SET);
    for (int c = 0; c < sec.m_numColumns; c++)
      sec.m_columnsSep.push_back((int)input->readLong(2));
    input->seek(pos+52, librevenge::RVNG_SEEK_SET);
    val = (int) input->readULong(2);
    switch ((val&3)) {
    case 1:
      f << "newPage[begin],";
      break;
    case 2: // checkme
      f << "leftPage[begin],";
      break;
    case 3: // checkme
      f << "rightPage[begin],";
      break;
    case 0: // begin on new line
    default:
      break;
    }
    sec.m_startOnNewPage=(val&3)!=0;
    val &=0xFFFC;
    if (val) f << "g0=" << std::hex << val << std::dec << ",";
    val = (int) input->readULong(2); // 0|1
    if (val) f << "g1=" << std::hex << val << std::dec << ",";
    val = (int) input->readULong(2); // 0 or 1
    sec.m_hasTitlePage=(val&1);
    val &= 0xFFFE;
    if (val) f << "g2=" << std::hex << val << std::dec << ",";

    val = (int) input->readULong(2); // 0 or 100
    sec.m_continuousHF=(val&0x100);
    sec.m_leftRightHF=(val&1);
    val &= 0xFEFE;
    if (val) f << "g3=" << std::hex << val << std::dec << ",";
    val = (int) input->readULong(2); // 0 ?
    if (val) f << "g4=" << std::hex << val << std::dec << ",";
    int prevHFId=0;
    for (int j=0; j<4; ++j) {
      int hFId=(int) input->readLong(4);
      sec.m_HFId[j]=hFId;
      if (!hFId || prevHFId==hFId) continue;
      zone.m_otherChilds.push_back(hFId);
      prevHFId=hFId;
    }
    sec.m_extra = f.str();
    zone.m_sectionList.push_back(sec);
    plc.m_id = i;
    zone.m_plcMap.insert(std::map<long, ClarisWksTextInternal::PLC>::value_type(sec.m_pos, plc));
    f.str("");
    f << "TextSection-S" << i << ":" << sec;
    if (input->tell() != pos+fSz)
      ascFile.addDelimiter(input->tell(), '|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
  }

  return true;
}

////////////////////////////////////////////////////////////
// read the different size for the text
////////////////////////////////////////////////////////////
bool ClarisWksText::readTextZoneSize(MWAWEntry const &entry, ClarisWksTextInternal::Zone &zone)
{
  long pos = entry.begin();

  int dataSize = 10;
  if ((entry.length()%dataSize) != 4)
    return false;

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  ascFile.addPos(pos);
  ascFile.addNote("Entries(TextZoneSz)");

  int numElt = int((entry.length()-4)/dataSize);

  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos+4, librevenge::RVNG_SEEK_SET); // skip header

  ClarisWksTextInternal::PLC plc;
  plc.m_type = ClarisWksTextInternal::P_TextZone;
  for (int i = 0; i < numElt; i++) {
    pos = input->tell();
    f.str("");
    f << "TextZoneSz-" << i << ":";
    ClarisWksTextInternal::TextZoneInfo info;
    info.m_pos = (long) input->readULong(4);
    info.m_N = (int) input->readULong(2);
    f << info;
    zone.m_textZoneList.push_back(info);
    plc.m_id = i;
    zone.m_plcMap.insert(std::map<long, ClarisWksTextInternal::PLC>::value_type(info.m_pos, plc));

    if (long(input->tell()) != pos+dataSize)
      ascFile.addDelimiter(input->tell(), '|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+dataSize, librevenge::RVNG_SEEK_SET);
  }

  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  return true;
}

bool ClarisWksText::canSendTextAsGraphic(ClarisWksTextInternal::Zone const &zone) const
{
  size_t numSection=zone.m_sectionList.size();
  if (numSection>1) return false;
  if (numSection==1 && zone.m_sectionList[0].m_numColumns>1)
    return false;
  for (size_t t=0; t < zone.m_tokenList.size(); ++t) {
    ClarisWksTextInternal::Token const &tok=zone.m_tokenList[t];
    if (tok.m_type!=ClarisWksTextInternal::TKN_UNKNOWN &&
        tok.m_type!=ClarisWksTextInternal::TKN_PAGENUMBER &&
        tok.m_type!=ClarisWksTextInternal::TKN_FIELD)
      return false;
  }
  return true;
}

bool ClarisWksText::sendText(ClarisWksTextInternal::Zone const &zone, MWAWListenerPtr listener)
{
  zone.m_parsed=true;
  bool localListener=false;
  if (listener)
    localListener=true;
  else
    listener=m_parserState->getMainListener();
  if (!listener || !listener->canWriteText()) {
    MWAW_DEBUG_MSG(("ClarisWksText::sendText: can not find a listener\n"));
    return false;
  }
  // Removeme when all is ok
  if (listener->isParagraphOpened())
    listener->insertEOL();
  long actC = 0;
  bool main = zone.m_id == 1;
  int numParaPLC = int(zone.m_paragraphList.size());
  int numParagraphs = int(m_state->m_paragraphsList.size());
  int actPage = 1;
  size_t numZones = zone.m_zones.size();
  if (main) {
    if (!localListener)
      m_document.newPage(actPage);
    else {
      MWAW_DEBUG_MSG(("ClarisWksText::sendText: try to send main zone as graphic\n"));
      main=false;
    }
  }
  int numCols = 1;
  int numSection = 0, numSectionInPage=0;
  int nextSection = -1;
  long nextSectionPos = main ? 0 : -1;
  if (zone.m_sectionList.size()) {
    nextSection = 0;
    nextSectionPos = zone.m_sectionList[0].m_pos;
  }
  int actListId=-1;
  long actListCPos=-1;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  std::multimap<long, ClarisWksTextInternal::PLC>::const_iterator plcIt;
  for (size_t z = 0; z < numZones; z++) {
    MWAWEntry const &entry  =  zone.m_zones[z];
    long pos = entry.begin();
    libmwaw::DebugStream f, f2;

    int numC = int(entry.length()-4);
    bool lastIsSectionBreak=false;
    input->seek(pos+4, librevenge::RVNG_SEEK_SET); // skip header

    for (int i = 0; i < numC; i++) {
      if (nextSectionPos!=-1 && actC >= nextSectionPos) {
        if (actC != nextSectionPos) {
          MWAW_DEBUG_MSG(("ClarisWksText::sendText: find a section inside a complex char!!!\n"));
          f << "###";
        }
        numSection++;
        numSectionInPage++;
        MWAWSection section;
        if (nextSection>=0) {
          section = zone.m_sectionList[size_t(nextSection)].getSection();
          if (main && lastIsSectionBreak && zone.m_sectionList[size_t(nextSection)].m_startOnNewPage)
            m_document.newPage(++actPage);
          if (size_t(++nextSection) < zone.m_sectionList.size())
            nextSectionPos = zone.m_sectionList[size_t(nextSection)].m_pos;
          else {
            nextSectionPos = -1;
            nextSection = -1;
          }
        }
        else {
          section=m_document.getMainSection();
          nextSectionPos = -1;
          nextSection = -1;
        }
        numCols = section.numColumns();
        int actCols = localListener ? 1 : listener->getSection().numColumns();
        if (numCols > 1  || actCols > 1) {
          if (listener->isSectionOpened())
            listener->closeSection();
          listener->openSection(section);
        }
      }
      else if (numSectionInPage==0)
        numSectionInPage++;
      plcIt = zone.m_plcMap.find(actC);
      bool seeToken = false;
      while (plcIt != zone.m_plcMap.end() && plcIt->first<=actC) {
        if (actC != plcIt->first) {
          MWAW_DEBUG_MSG(("ClarisWksText::sendText: find a plc inside a complex char!!!\n"));
          f << "###";
        }
        ClarisWksTextInternal::PLC const &plc = plcIt++->second;
        f << "[" << plc << "]";
        switch (plc.m_type) {
        case ClarisWksTextInternal::P_Font:
          if (plc.m_id < 0 || plc.m_id >= int(zone.m_fontList.size())) {
            MWAW_DEBUG_MSG(("ClarisWksText::sendText: can not find font %d\n", plc.m_id));
            f << "###";
            break;
          }
          listener->setFont(zone.m_fontList[size_t(plc.m_id)]);
          break;
        case ClarisWksTextInternal::P_Ruler: {
          if (plc.m_id < 0 || plc.m_id >= numParaPLC)
            break;
          ClarisWksTextInternal::ParagraphPLC const &paraPLC = zone.m_paragraphList[(size_t) plc.m_id];
          f << "[" << paraPLC << "]";
          if (paraPLC.m_rulerId < 0 || paraPLC.m_rulerId >= numParagraphs)
            break;
          ClarisWksTextInternal::Paragraph para = m_state->m_paragraphsList[(size_t) paraPLC.m_rulerId];
          if (*para.m_listLevelIndex>0 && actC >= actListCPos)
            actListId=findListId(zone, actListId, actC, actListCPos);
#if 0
          // to use when the style manager is able to retrieve the correct style name
          if (actListId <= 0 && paraPLC.m_styleId >= 0) {
            std::string styleName;
            if (m_document.getStyleManager()->getRulerName(paraPLC.m_styleId, styleName)) {
              librevenge::RVNGString sfinalName("");
              for (size_t c=0; c < styleName.size(); ++c) {
                int unicode= m_parserState->m_fontConverter->unicode(3, (unsigned char) styleName[c]);
                if (unicode==-1)
                  sfinalName.append((char) styleName[c]);
                else
                  libmwaw::appendUnicode((uint32_t) unicode, sfinalName);
              }
              para.m_styleName = librevenge::RVNGString(sfinalName,true).cstr();
            }
          }
#endif
          setProperty(*listener, para, actListId);
          break;
        }
        case ClarisWksTextInternal::P_Token: {
          if (plc.m_id < 0 || plc.m_id >= int(zone.m_tokenList.size())) {
            MWAW_DEBUG_MSG(("ClarisWksText::sendText: can not find the token %d\n", plc.m_id));
            f << "###";
            break;
          }
          ClarisWksTextInternal::Token const &token = zone.m_tokenList[size_t(plc.m_id)];
          switch (token.m_type) {
          case ClarisWksTextInternal::TKN_FOOTNOTE:
            if (m_parserState->m_kind==MWAWDocument::MWAW_K_PAINT) {
              MWAW_DEBUG_MSG(("ClarisWksText::sendText: can not send footnote in a paint file\n"));
              f << "###";
              break;
            }
            if (token.m_zoneId>0)
              m_document.sendFootnote(token.m_zoneId);
            else
              f << "###";
            break;
          case ClarisWksTextInternal::TKN_PAGENUMBER:
            switch (token.m_unknown[0]) {
            case 1:
            case 2: {
              std::stringstream s;
              int num =  token.m_unknown[0]==1 ? numSection : numSectionInPage;
              s << num;
              listener->insertUnicodeString(s.str().c_str());
              break;
            }
            case 3:
              listener->insertField(MWAWField(MWAWField::PageCount));
              break;
            case 0:
            default:
              listener->insertField(MWAWField(MWAWField::PageNumber));
            }
            break;
          case ClarisWksTextInternal::TKN_GRAPHIC:
            if (m_parserState->m_kind==MWAWDocument::MWAW_K_PAINT) {
              MWAW_DEBUG_MSG(("ClarisWksText::sendText: can not send graphic in a paint file\n"));
              f << "###";
              break;
            }
            if (m_parserState->m_kind==MWAWDocument::MWAW_K_PRESENTATION) {
              MWAW_DEBUG_MSG(("ClarisWksText::sendText: find a graphic in text zone, may cause some problem\n"));
              f << "#";
            }
            if (token.m_zoneId>0) {
              // fixme
              MWAWPosition tPos;
              if (token.m_descent != 0) {
                tPos=MWAWPosition(MWAWVec2f(0,float(token.m_descent)), MWAWVec2f(), librevenge::RVNG_POINT);
                tPos.setRelativePosition(MWAWPosition::Char, MWAWPosition::XLeft, MWAWPosition::YBottom);
              }
              m_document.sendZone(token.m_zoneId, MWAWListenerPtr(), tPos);
            }
            else
              f << "###";
            break;
          case ClarisWksTextInternal::TKN_FIELD:
            listener->insertUnicode(0xab);
            if (token.m_fieldEntry.valid() &&
                input->checkPosition(token.m_fieldEntry.end())) {
              long actPos=input->tell();
              input->seek(token.m_fieldEntry.begin(), librevenge::RVNG_SEEK_SET);
              long endFPos=token.m_fieldEntry.end();
              while (!input->isEnd() && input->tell() < token.m_fieldEntry.end())
                listener->insertCharacter((unsigned char)input->readULong(1), input, endFPos);
              input->seek(actPos, librevenge::RVNG_SEEK_SET);
            }
            else {
              MWAW_DEBUG_MSG(("ClarisWksText::sendText: can not find field token data\n"));
              listener->insertCharacter(' ');
            }
            listener->insertUnicode(0xbb);
            break;
          case ClarisWksTextInternal::TKN_UNKNOWN:
          default:
            break;
          }
          seeToken = true;
          break;
        }
        /* checkme: normally, this corresponds to the first
           character following a 0xb/0x1, so we do not need to a
           column/page break here */
        case ClarisWksTextInternal::P_Child:
        case ClarisWksTextInternal::P_Section:
        case ClarisWksTextInternal::P_TextZone:
        case ClarisWksTextInternal::P_Unknown:
        default:
          break;
        }
      }
      char c = (char) input->readULong(1);
      lastIsSectionBreak=(c==0xc);
      actC++;
      if (c == '\0') {
        if (i == numC-1) break;
        MWAW_DEBUG_MSG(("ClarisWksText::sendText: OOPS, find 0 reading the text\n"));
        f << "###0x0";
        continue;
      }
      f << c;
      if (seeToken && static_cast<unsigned char>(c) < 32) continue;
      switch (c) {
      case 0x1: // fixme: column break
        if (numCols) {
          listener->insertBreak(MWAWListener::ColumnBreak);
          break;
        }
        MWAW_DEBUG_MSG(("ClarisWksText::sendText: Find unexpected char 1\n"));
        f << "###";
      case 0xb: // page break
        numSectionInPage = 0;
        if (main)
          m_document.newPage(++actPage);
        break;
      case 0x2: // token footnote ( normally already done)
        break;
      case 0x3: // token graphic
        break;
      case 0x4:
        listener->insertField(MWAWField(MWAWField::Date));
        break;
      case 0x5: {
        MWAWField field(MWAWField::Time);
        field.m_DTFormat="%H:%M";
        listener->insertField(field);
        break;
      }
      case 0x6: // normally already done, but if we do not find the token, ...
        listener->insertField(MWAWField(MWAWField::PageNumber));
        break;
      case 0x7: // footnote index (ok to ignore : index of the footnote )
        break;
      case 0x8: // potential breaking <<hyphen>>
        break;
      case 0x9:
        listener->insertTab();
        break;
      case 0xa:
        listener->insertEOL(true);
        break;
      case 0xc: // new section: this is treated after, at the beginning of the for loop
        break;
      case 0xd:
        f2.str("");
        f2 << "Entries(TextContent):" << f.str();
        ascFile.addPos(pos);
        ascFile.addNote(f2.str().c_str());
        f.str("");
        pos = input->tell();

        // ignore last end of line returns
        if (z != numZones-1 || i != numC-2)
          listener->insertEOL();
        break;

      default: {
        int extraChar = listener->insertCharacter
                        ((unsigned char)c, input, input->tell()+(numC-1-i));
        if (extraChar) {
          i += extraChar;
          actC += extraChar;
        }
      }
      }
    }
    if (f.str().length()) {
      f2.str("");
      f2 << "Entries(TextContent):" << f.str();
      ascFile.addPos(pos);
      ascFile.addNote(f2.str().c_str());
    }
  }

  return true;
}

int ClarisWksText::findListId(ClarisWksTextInternal::Zone const &zone, int actListId, long actC, long &lastPos)
{
  // retrieve the actual list
  shared_ptr<MWAWList> actList;
  if (actListId>0)
    actList = m_parserState->m_listManager->getList(actListId);

  int numParaPLC= int(zone.m_paragraphList.size());
  int numParagraphs = int(m_state->m_paragraphsList.size());
  std::multimap<long, ClarisWksTextInternal::PLC>::const_iterator plcIt;
  plcIt=zone.m_plcMap.find(actC);
  int listId = -1;
  int maxLevelSet = -1;
  // find the last position which can correspond to the actual list
  while (plcIt!=zone.m_plcMap.end()) {
    lastPos = plcIt->first;
    ClarisWksTextInternal::PLC const &plc = plcIt++->second;
    if (plc.m_type != ClarisWksTextInternal::P_Ruler)
      continue;
    if (plc.m_id < 0 || plc.m_id >= numParaPLC)
      break;
    ClarisWksTextInternal::ParagraphPLC const &paraPLC = zone.m_paragraphList[(size_t) plc.m_id];
    if (paraPLC.m_rulerId < 0 || paraPLC.m_rulerId >= numParagraphs)
      break;
    ClarisWksTextInternal::Paragraph const &para=m_state->m_paragraphsList[(size_t) paraPLC.m_rulerId];
    int level = *para.m_listLevelIndex;
    if (level<=0)
      continue;
    shared_ptr<MWAWList> newList =
      m_parserState->m_listManager->getNewList(actList, level, *para.m_listLevel);
    if (!newList)
      break;
    if (level <= maxLevelSet && newList->getId() != listId)
      break;
    if (level > maxLevelSet) maxLevelSet=level;
    listId = newList->getId();
    actList = newList;
  }
  return listId;
}

////////////////////////////////////////////////////////////
// the style definition?
////////////////////////////////////////////////////////////

bool ClarisWksText::readSTYL_RULR(int N, int fSz)
{
  if (fSz == 0 || N== 0) return true;
  if (fSz != 108) {
    MWAW_DEBUG_MSG(("ClarisWksText::readSTYL_RULR: Find odd ruler size %d\n", fSz));
  }
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  for (int i = 0; i < N; i++) {
    long pos = input->tell();
    if (fSz != 108 || !readParagraph(i)) {
      f.str("");
      if (!i)
        f << "Entries(RULR)-P0:#";
      else
        f << "RULR-P" << i << ":#";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// read a list of rulers
////////////////////////////////////////////////////////////
bool ClarisWksText::readParagraphs()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;

  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (input->isEnd()) {
    MWAW_DEBUG_MSG(("ClarisWksText::readParagraphs: ruler zone is too short\n"));
    return false;
  }
  input->seek(pos+4, librevenge::RVNG_SEEK_SET);

  int N = (int) input->readULong(2);
  int type = (int) input->readLong(2);
  int val = (int) input->readLong(2);
  int fSz = (int) input->readLong(2);

  if (sz != 12+fSz*N) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksText::readParagraphs: find odd ruler size\n"));
    return false;
  }

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(RULR):";
  f << "N=" << N << ", type?=" << type <<", fSz=" << fSz << ",";
  if (val) f << "unkn=" << val << ",";

  for (int i = 0; i < 2; i++) {
    val = (int) input->readLong(2);
    if (val)  f << "f" << i << "=" << val << ",";
  }

  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = input->tell();
    if (!readParagraph(i)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
  }
  return true;
}

////////////////////////////////////////////////////////////
// read a ruler zone
////////////////////////////////////////////////////////////
bool ClarisWksText::readParagraph(int id)
{
  int dataSize = 0;
  int vers = version();
  switch (vers) {
  case 1:
    dataSize = 92;
    break;
  case 2:
  case 3:
    dataSize = 96;
    break;
  case 4:
  case 5:
  case 6:
    if (id >= 0) dataSize = 108;
    else dataSize = 96;
    break;
  default:
    MWAW_DEBUG_MSG(("ClarisWksText::readParagraph: unknown size\n"));
    return false;
  }

  ClarisWksTextInternal::Paragraph ruler;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long endPos = pos+dataSize;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  int val;
  if (vers >= 4 && id >= 0) {
    val = (int) input->readLong(2);
    if (val != -1) f << "f0=" << val << ",";
    val = (int) input->readLong(4);
    f << "f1=" << val << ",";
    int dim[2];
    for (int i = 0; i < 2; i++)
      dim[i] = (int) input->readLong(2);
    f << "dim?=" << dim[0] << "x" << dim[1] << ",";
    ruler.m_labelType = (int) input->readLong(1);
    int listLevel = (int) input->readLong(1);
    if (listLevel < 0 || listLevel > 10) {
      MWAW_DEBUG_MSG(("ClarisWksText::readParagraph: can not determine list level\n"));
      f << "##listLevel=" << listLevel << ",";
      listLevel = 0;
    }
    ruler.m_listLevelIndex = listLevel;
  }

  val = (int) input->readLong(2);
  f << "num[used]=" << val << ",";
  val = (int) input->readULong(2);
  int align = 0;
  switch (vers) {
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
    align = (val >> 14);
    val &= 0x3FFF;
    break;
  case 6:
    align = (val >> 13) & 3;
    val &= 0x9FFF;
    break;
  default:
    break;
  }
  switch (align) {
  case 0:
    break;
  case 1:
    ruler.m_justify = MWAWParagraph::JustificationCenter;
    break;
  case 2:
    ruler.m_justify = MWAWParagraph::JustificationRight ;
    break;
  case 3:
    ruler.m_justify = MWAWParagraph::JustificationFull;
    break;
  default:
    break;
  }


  bool inPoint = false;
  int interline = 0;
  switch (vers) {
  case 1:
    inPoint = (val & 0x2000);
    interline = val & 0xFF;
    val &= 0x1F00;
    break;
  case 2:
  case 3:
  case 4:
  case 5:
  case 6: {
    interline = (val >> 3);
    bool ok = true;
    switch (val & 7) {
    case 0: // PERCENT
      ok = interline <= 18;
      inPoint = false;
      break;
    case 6: // display unit pica
    case 5: // display unit cm
    case 4: // display unit mm
    case 3: // display unit Inch
    case 2: // display unit point
      ok = interline <= 512;
      inPoint = true; // data always stored in point
      break;
    default:
      ok = false;
      break;
    }
    if (ok) val = 0;
    else {
      MWAW_DEBUG_MSG(("ClarisWksText::readParagraph: can not determine interline dimension\n"));
      interline = 0;
    }
    break;
  }
  default:
    break;
  }
  if (interline) {
    if (inPoint)
      ruler.setInterline(interline, librevenge::RVNG_POINT);
    else
      ruler.setInterline(1.0+interline*0.5, librevenge::RVNG_PERCENT);
  }
  if (val) f << "#flags=" << std::hex << val << std::dec << ",";
  for (int i = 0; i < 3; i++)
    ruler.m_margins[i] = float(input->readLong(2))/72.f;
  *(ruler.m_margins[2]) -= 28./72.;
  if (ruler.m_margins[2].get() < 0.0) ruler.m_margins[2] = 0.0;
  if (vers >= 2) {
    for (int i = 0; i < 2; i++) {
      ruler.m_spacings[i+1] = float(input->readULong(1))/72.f;
      input->seek(1, librevenge::RVNG_SEEK_CUR); // flags to define the printing unit
    }
  }
  val = (int) input->readLong(1);
  if (val) f << "unkn1=" << val << ",";
  int numTabs = (int) input->readULong(1);
  if (long(input->tell())+numTabs*4 > endPos) {
    if (numTabs != 255) { // 0xFF seems to be used in v1, v2
      MWAW_DEBUG_MSG(("ClarisWksText::readParagraph: numTabs is too big\n"));
    }
    f << "numTabs*=" << numTabs << ",";
    numTabs = 0;
  }
  for (int i = 0; i < numTabs; i++) {
    MWAWTabStop tab;
    tab.m_position = float(input->readLong(2))/72.f;
    val = (int) input->readULong(1);
    int leaderType = 0;
    switch (vers) {
    case 1:
      align = val & 3;
      val &= 0xFC;
      break;
    case 2:
    case 3:
    case 4:
    case 5:
      align = (val >> 6);
      leaderType = (val & 3);
      val &= 0x3C;
      break;
    case 6:
      align = (val >> 5);
      leaderType = (val & 3);
      val &= 0x9C;
      break;
    default:
      break;
    }
    switch (align&3) {
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
    switch (leaderType) {
    case 1:
      tab.m_leaderCharacter = '.';
      break;
    case 2:
      tab.m_leaderCharacter = '-';
      break;
    case 3:
      tab.m_leaderCharacter = '_';
      break;
    case 0:
    default:
      break;
    }
    char decimalChar = (char) input->readULong(1);
    if (decimalChar) {
      int unicode= m_parserState->m_fontConverter->unicode(3, (unsigned char) decimalChar);
      if (unicode==-1)
        tab.m_decimalCharacter = uint16_t(decimalChar);
      else
        tab.m_decimalCharacter = uint16_t(unicode);
    }
    ruler.m_tabs->push_back(tab);
    if (val)
      f << "#unkn[tab" << i << "=" << std::hex << val << std::dec << "],";
  }
  ruler.updateListLevel();
  ruler.m_extra = f.str();
  // save the style
  if (id >= 0) {
    if (int(m_state->m_paragraphsList.size()) <= id)
      m_state->m_paragraphsList.resize((size_t)id+1);
    m_state->m_paragraphsList[(size_t)id]=ruler;
  }
  f.str("");
  if (id == 0)
    f << "Entries(RULR)-P0";
  else if (id < 0)
    f << "RULR-P_";
  else
    f << "RULR-P" << id;
  f << ":" << ruler;

  if (long(input->tell()) != pos+dataSize)
    ascFile.addDelimiter(input->tell(), '|');
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != pos+dataSize)
    return false;
  return true;
}

void ClarisWksText::setProperty(MWAWListener &listener, ClarisWksTextInternal::Paragraph const &ruler, int listId)
{
  if (listId <= 0) {
    listener.setParagraph(ruler);
    return;
  }
  MWAWParagraph para=ruler;
  para.m_listId=listId;
  listener.setParagraph(para);
}

bool ClarisWksText::canSendTextAsGraphic(int number) const
{
  std::map<int, shared_ptr<ClarisWksTextInternal::Zone> >::const_iterator iter
    = m_state->m_zoneMap.find(number);
  if (iter == m_state->m_zoneMap.end() || !iter->second)
    return false;
  return canSendTextAsGraphic(*iter->second);
}

bool ClarisWksText::sendZone(int number, MWAWListenerPtr listener)
{
  std::map<int, shared_ptr<ClarisWksTextInternal::Zone> >::iterator iter
    = m_state->m_zoneMap.find(number);
  if (iter == m_state->m_zoneMap.end())
    return false;
  shared_ptr<ClarisWksTextInternal::Zone> zone = iter->second;
  sendText(*zone, listener);
  return true;
}

void ClarisWksText::flushExtra()
{
  shared_ptr<MWAWListener> listener=m_parserState->getMainListener();
  if (!listener) return;
  std::map<int, shared_ptr<ClarisWksTextInternal::Zone> >::iterator iter
    = m_state->m_zoneMap.begin();
  for (; iter !=  m_state->m_zoneMap.end(); ++iter) {
    shared_ptr<ClarisWksTextInternal::Zone> zone = iter->second;
    if (!zone || zone->m_parsed)
      continue;
    listener->insertEOL();
    if (zone->m_parsed) // can be a header/footer in draw zone
      continue;
    sendText(*zone, listener);
  }
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
