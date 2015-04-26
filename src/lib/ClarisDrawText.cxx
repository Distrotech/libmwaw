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

#include "ClarisDrawParser.hxx"
#include "ClarisDrawStyleManager.hxx"

#include "ClarisDrawText.hxx"

/** Internal: the structures of a ClarisDrawText */
namespace ClarisDrawTextInternal
{
/** the different plc type */
enum PLCType { P_Font,  P_Ruler, P_Child, P_TextZone, P_Token, P_Unknown};

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

//! paragraph plc
struct ParagraphPLC {
  //! constructor
  ParagraphPLC() : m_rulerId(-1), m_flags(0), m_extra("")
  {
  }
  //! return the label type
  int getLabelType() const
  {
    return int((m_flags>>3)&0xF);
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, ParagraphPLC const &info)
  {
    if (info.m_rulerId >= 0) o << "P" << info.m_rulerId <<",";
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
  /** some flags */
  int m_flags;
  /** extra data */
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
  //! the position
  long m_pos;
  //! the number of character
  int m_N;
  //! extra data
  std::string m_extra;
};

//! token type
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

//! low level internal: main text zone
struct DSET : public ClarisWksStruct::DSET {
  //! constructor
  DSET(ClarisWksStruct::DSET const &dset = ClarisWksStruct::DSET()) :
    ClarisWksStruct::DSET(dset), m_zones(), m_numChar(0), m_numTextZone(0), m_numParagInfo(0),
    m_numFont(0), m_fatherId(0), m_unknown(0),
    m_subSectionPosList(), m_fontList(), m_paragraphList(), m_tokenList(),
    m_textZoneList(), m_plcMap()
  {
  }

  //! true if the zone is outlined
  bool isOutlined() const
  {
    return m_flags[0]==1;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, DSET const &doc)
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

  std::vector<MWAWEntry> m_zones; // the text zones
  int m_numChar /** the number of char in text zone */;
  int m_numTextZone /** the number of text zone ( ie. number of page ? ) */;
  int m_numParagInfo /** the number of paragraph info */;
  int m_numFont /** the number of font */;
  int m_fatherId /** the father id */;
  int m_unknown /** an unknown flags */;

  std::vector<long> m_subSectionPosList /** list of end of section position */;
  std::vector<MWAWFont> m_fontList /** the list of fonts */;
  std::vector<ParagraphPLC> m_paragraphList /** the list of paragraph */;
  std::vector<Token> m_tokenList /** the list of token */;
  std::vector<TextZoneInfo> m_textZoneList /** the list of zone */;
  std::multimap<long, PLC> m_plcMap /** the plc map */;
};

////////////////////////////////////////
//! Internal: the state of a ClarisDrawText
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
  std::map<int, shared_ptr<DSET> > m_zoneMap;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
ClarisDrawText::ClarisDrawText(ClarisDrawParser &parser) :
  m_parserState(parser.getParserState()), m_state(new ClarisDrawTextInternal::State), m_mainParser(&parser),
  m_styleManager(parser.m_styleManager)
{
}

ClarisDrawText::~ClarisDrawText()
{ }

int ClarisDrawText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}

int ClarisDrawText::numPages() const
{
  // to do
  return 0;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// a document part
////////////////////////////////////////////////////////////
shared_ptr<ClarisWksStruct::DSET> ClarisDrawText::readDSETZone(ClarisWksStruct::DSET const &zone, MWAWEntry const &entry)
{
  if (!entry.valid() || zone.m_fileType != 1)
    return shared_ptr<ClarisWksStruct::DSET>();
  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos+8+16, librevenge::RVNG_SEEK_SET); // avoid header+8 generic number
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(DSETT):";

  shared_ptr<ClarisDrawTextInternal::DSET> textZone(new ClarisDrawTextInternal::DSET(zone));
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
    MWAW_DEBUG_MSG(("ClarisDrawText::readDSETZone: find unknown position %d\n", (textZone->m_textType >> 4)));
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
  int const data0Length = 28;

  int N = int(zone.m_numData);
  if (long(input->tell())+N*data0Length > entry.end()) {
    MWAW_DEBUG_MSG(("ClarisDrawText::readDSETZone: file is too short\n"));
    return shared_ptr<ClarisWksStruct::DSET>();
  }

  input->seek(entry.end()-N*data0Length, librevenge::RVNG_SEEK_SET);
  ClarisDrawTextInternal::PLC plc;
  plc.m_type = ClarisDrawTextInternal::P_Child;
  int numExtraHId=0;
  textZone->m_subSectionPosList.push_back(0);
  for (int i = 0; i < N; i++) { // normally 1
    /* definition of a list of text zone ( one by column and one by page )*/
    pos = input->tell();
    f.str("");
    f << "DSETT-" << i << ":";
    ClarisWksStruct::DSET::Child child;
    child.m_posC = (long) input->readULong(4);
    if (child.m_posC>textZone->m_subSectionPosList.back())
      textZone->m_subSectionPosList.push_back(child.m_posC);
    child.m_type = ClarisWksStruct::DSET::C_SubText;
    int dim[2];
    for (int j = 0; j < 2; j++)
      dim[j] = (int) input->readLong(2);
    child.m_box = MWAWBox2i(MWAWVec2i(0,0), MWAWVec2i(dim[0], dim[1]));
    textZone->m_childs.push_back(child);
    plc.m_id = i;
    textZone->m_plcMap.insert(std::map<long, ClarisDrawTextInternal::PLC>::value_type(child.m_posC, plc));
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

    long id=(int) input->readULong(4);
    if (id) {
      f << "ID=" << std::hex << id << std::dec << ",";
      ++numExtraHId;
    }

    long actPos = input->tell();
    if (actPos != pos && actPos != pos+data0Length)
      ascFile.addDelimiter(input->tell(),'|');
    input->seek(pos+data0Length, librevenge::RVNG_SEEK_SET);

    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);

  // now normally 4 zones: paragraph, font, token, text zone size + 1 zone for each text zone
  bool ok=true;
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
      MWAW_DEBUG_MSG(("ClarisDrawText::readDSETZone: entry for %d zone is too short\n", z));
      ascFile.addPos(pos);
      ascFile.addNote("###");
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      if (z > 4)
        break;
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
        MWAW_DEBUG_MSG(("ClarisDrawText::readDSETZone: can not find text %d zone\n", z-4));
        if (z > 4) break;
        return textZone;
      }
      f.str("");
      f << "DSETT-Z" << z << "#";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    input->seek(zEntry.end(), librevenge::RVNG_SEEK_SET);
  }
  // never seems
  for (int i=0; ok && i<numExtraHId; ++i) {
    pos=input->tell();
    long sz=(long) input->readULong(4);
    if (sz<10 || !input->checkPosition(pos+4+sz)) {
      MWAW_DEBUG_MSG(("ClarisDrawText::readDSETZone:: can not read an extra block\n"));
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

  if (m_state->m_zoneMap.find(textZone->m_id) != m_state->m_zoneMap.end()) {
    MWAW_DEBUG_MSG(("ClarisDrawText::readDSETZone: zone %d already exists!!!\n", textZone->m_id));
  }
  else
    m_state->m_zoneMap[textZone->m_id] = textZone;

  if (ok) {
    // look for unparsed zone
    pos=input->tell();
    long sz=(long) input->readULong(4);
    if (input->checkPosition(pos+4+sz)) {
      if (sz) {
        MWAW_DEBUG_MSG(("ClarisDrawText::readDSETZone:: find some extra block\n"));
        input->seek(pos+4+sz, librevenge::RVNG_SEEK_SET);
        ascFile.addPos(pos);
        ascFile.addNote("Entries(TextEnd):###");
      }
      else {
        // 2 empty zone is ok, if not probably a problem, but...
        ascFile.addPos(pos);
        ascFile.addNote("_");
      }
    }
    else
      input->seek(pos, librevenge::RVNG_SEEK_SET);
  }

  return textZone;
}
////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////
bool ClarisDrawText::readFonts(MWAWEntry const &entry, ClarisDrawTextInternal::DSET &zone)
{
  long pos = entry.begin();
  if ((entry.length()%12) != 4)
    return false;

  int numElt = int((entry.length()-4)/12);
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos+4, librevenge::RVNG_SEEK_SET); // skip header
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  ascFile.addPos(pos);
  ascFile.addNote("Entries(FontPLC)");

  ClarisDrawTextInternal::PLC plc;
  plc.m_type = ClarisDrawTextInternal::P_Font;
  for (int i = 0; i < numElt; i++) {
    MWAWFont font;
    int posChar;
    if (!readFont(i, posChar, font)) return false;
    zone.m_fontList.push_back(font);
    plc.m_id = i;
    zone.m_plcMap.insert(std::map<long, ClarisDrawTextInternal::PLC>::value_type(posChar, plc));
  }

  return true;
}


bool ClarisDrawText::readFont(int id, int &posC, MWAWFont &font)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();

  int const fontSize = 12;
  if (!input->checkPosition(pos+fontSize)) {
    MWAW_DEBUG_MSG(("ClarisDrawText::readFont: file is too short"));
    return false;
  }

  font = MWAWFont();
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  if (id >= 0)
    f << "FontPLC-F" << id << ":";
  else
    f << "Entries(FontDef):";
  posC = int(input->readULong(4));
  f << "pos=" << posC << ",";
  font.setId((int) input->readULong(2));
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
  font.setSize((float) input->readULong(2));
  int colId = (int) input->readULong(1);
  if (colId) f << "###backColor=" << colId << ",";
  colId = (int) input->readULong(1);
  if (colId!=1) {
    MWAWColor col;
    if (m_styleManager->getColor(colId, col))
      font.setColor(col);
    else {
      MWAW_DEBUG_MSG(("ClarisDrawText::readFont: unknown color %d\n", colId));
      f << "##colId=" << colId << ",";
    }
  }
  font.setFlags(flags);
  f << font.getDebugString(m_parserState->m_fontConverter);
  if (long(input->tell()) != pos+fontSize)
    ascFile.addDelimiter(input->tell(), '|');
  input->seek(pos+fontSize, librevenge::RVNG_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read a list of rulers
////////////////////////////////////////////////////////////
bool ClarisDrawText::readParagraphs()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;

  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("ClarisDrawText::readParagraphs: ruler zone is too short\n"));
    return false;
  }

  int N = (int) input->readULong(2);
  int type = (int) input->readLong(2);
  int val = (int) input->readLong(2);
  int fSz = (int) input->readLong(2);

  if (sz != 12+fSz*N) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisDrawText::readParagraphs: find odd ruler size\n"));
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

bool ClarisDrawText::readParagraphs(MWAWEntry const &entry, ClarisDrawTextInternal::DSET &zone)
{
  long pos = entry.begin();
  if ((entry.length()%8) != 4)
    return false;

  int numElt = int((entry.length()-4)/8);

  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos+4, librevenge::RVNG_SEEK_SET); // skip header
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  pos = entry.begin();
  ascFile.addPos(pos);
  ascFile.addNote("Entries(ParaPLC)");

  libmwaw::DebugStream f;
  input->seek(pos+4, librevenge::RVNG_SEEK_SET); // skip header
  ClarisDrawTextInternal::PLC plc;
  plc.m_type = ClarisDrawTextInternal::P_Ruler;
  for (int i = 0; i < numElt; i++) {
    pos = input->tell();
    ClarisDrawTextInternal::ParagraphPLC info;

    long posC = (long) input->readULong(4);
    f.str("");
    f << "ParaPLC-R" << i << ": pos=" << posC << ",";
    info.m_rulerId = (int) input->readLong(2);
    info.m_flags = (int) input->readLong(2);
    f << info;

    zone.m_paragraphList.push_back(info);
    plc.m_id = i;
    zone.m_plcMap.insert(std::map<long, ClarisDrawTextInternal::PLC>::value_type(posC, plc));
    input->seek(pos+8, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  return true;
}

////////////////////////////////////////////////////////////
// read a ruler zone
////////////////////////////////////////////////////////////
bool ClarisDrawText::readParagraph(int id)
{
  int const dataSize=96;
  ClarisDrawTextInternal::Paragraph ruler;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long endPos = pos+dataSize;
  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("ClarisDrawText::readParagraph: file is too short\n"));
    return false;
  }
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  int val;
  val = (int) input->readLong(2);
  f << "num[used]=" << val << ",";
  val = (int) input->readULong(2);
  int align = 0;
  align = (val >> 14);
  val &= 0x3FFF;
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

  bool inPoint = (val & 0x2000);
  int interline = val & 0xFF;
  if (interline) {
    if (inPoint)
      ruler.setInterline(interline, librevenge::RVNG_POINT);
    else
      ruler.setInterline(1.0+double(interline)/8, librevenge::RVNG_PERCENT);
  }
  if (val) f << "#flags=" << std::hex << val << std::dec << ",";
  for (int i = 0; i < 3; i++)
    ruler.m_margins[i] = float(input->readLong(2))/72.f;
  *(ruler.m_margins[2]) -= 28./72.;
  if (ruler.m_margins[2].get() < 0.0) ruler.m_margins[2] = 0.0;
  for (int i = 0; i < 2; i++) {
    ruler.m_spacings[i+1] = float(input->readULong(1))/72.f;
    input->seek(1, librevenge::RVNG_SEEK_CUR); // flags to define the printing unit
  }
  val = (int) input->readLong(1);
  if (val) f << "unkn1=" << val << ",";
  int numTabs = (int) input->readULong(1);
  if (long(input->tell())+numTabs*4 > endPos) {
    if (numTabs != 255) { // 0xFF seems to be used in v1, v2
      MWAW_DEBUG_MSG(("ClarisDrawText::readParagraph: numTabs is too big\n"));
    }
    f << "numTabs*=" << numTabs << ",";
    numTabs = 0;
  }
  for (int i = 0; i < numTabs; i++) {
    MWAWTabStop tab;
    tab.m_position = float(input->readLong(2))/72.f;
    val = (int) input->readULong(1);
    switch ((val>>6)&3) {
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
    switch (val&3) {
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
    val &= 0x3C;
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
    f << "Entries(RULR)-P_";
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

////////////////////////////////////////////////////////////
// zone which corresponds to the token (never seens data, so suppose that this is simillar to ClarisWorks)
////////////////////////////////////////////////////////////
bool ClarisDrawText::readTokens(MWAWEntry const &entry, ClarisDrawTextInternal::DSET &zone)
{
  long pos = entry.begin();

  if ((entry.length()%32) != 4)
    return false;

  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  ascFile.addPos(pos);
  ascFile.addNote("Entries(Token)");
  int numElt = int((entry.length()-4)/32);
  input->seek(pos+4, librevenge::RVNG_SEEK_SET); // skip header

  libmwaw::DebugStream f;
  ClarisDrawTextInternal::PLC plc;
  plc.m_type = ClarisDrawTextInternal::P_Token;
  int val;
  std::vector<int> fieldList;
  for (int i = 0; i < numElt; i++) {
    pos = input->tell();

    int posC = (int) input->readULong(4);
    ClarisDrawTextInternal::Token token;

    int type = (int) input->readLong(2);
    f.str("");
    switch (type) {
    case 0:
      token.m_type = ClarisDrawTextInternal::TKN_FOOTNOTE;
      break;
    case 1:
      token.m_type = ClarisDrawTextInternal::TKN_GRAPHIC;
      break;
    case 2:
      token.m_type = ClarisDrawTextInternal::TKN_PAGENUMBER;
      break;
    case 3:
      token.m_type = ClarisDrawTextInternal::TKN_FIELD;
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
    if (val)
      f << "f3=" << val << ",";
    token.m_extra = f.str();
    f.str("");
    f << "Token-" << i << ": pos=" << posC << "," << token;
    zone.m_tokenList.push_back(token);
    plc.m_id = i;
    zone.m_plcMap.insert(std::map<long, ClarisDrawTextInternal::PLC>::value_type(posC, plc));

    if (input->tell() != pos+32)
      ascFile.addDelimiter(input->tell(), '|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+32, librevenge::RVNG_SEEK_SET);
  }

  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  for (size_t i=0; i < fieldList.size(); ++i) {
    pos=input->tell();
    long sz=(long) input->readULong(4);
    f.str("");
    f << "Token[field-" << i << "]:";
    if (!input->checkPosition(pos+sz+4) || long(input->readULong(1))+1!=sz) {
      MWAW_DEBUG_MSG(("ClarisDrawText::readTokens: can find token field name %d\n", int(i)));
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

////////////////////////////////////////////////////////////
// read the different size for the text
////////////////////////////////////////////////////////////
bool ClarisDrawText::readTextZoneSize(MWAWEntry const &entry, ClarisDrawTextInternal::DSET &zone)
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

  ClarisDrawTextInternal::PLC plc;
  plc.m_type = ClarisDrawTextInternal::P_TextZone;
  for (int i = 0; i < numElt; i++) {
    pos = input->tell();
    f.str("");
    f << "TextZoneSz-" << i << ":";
    ClarisDrawTextInternal::TextZoneInfo info;
    info.m_pos = (long) input->readULong(4);
    info.m_N = (int) input->readULong(2);
    f << info;
    zone.m_textZoneList.push_back(info);
    plc.m_id = i;
    zone.m_plcMap.insert(std::map<long, ClarisDrawTextInternal::PLC>::value_type(info.m_pos, plc));

    if (long(input->tell()) != pos+dataSize)
      ascFile.addDelimiter(input->tell(), '|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+dataSize, librevenge::RVNG_SEEK_SET);
  }

  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// send data
////////////////////////////////////////////////////////////
bool ClarisDrawText::sendText(ClarisDrawTextInternal::DSET const &zone, int subZone)
{
  zone.m_parsed=true;
  MWAWListenerPtr listener=m_parserState->getMainListener();
  if (!listener || !listener->canWriteText()) {
    MWAW_DEBUG_MSG(("ClarisDrawText::sendText: can not find a listener\n"));
    return false;
  }
  int numParaPLC = int(zone.m_paragraphList.size());
  int numParagraphs = int(m_state->m_paragraphsList.size());
  size_t numZones = zone.m_zones.size();
  int numCols = 1;
  shared_ptr<MWAWList> actList;
  bool isOutline=zone.isOutlined();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  std::multimap<long, ClarisDrawTextInternal::PLC>::const_iterator plcIt;

  long firstChar=0, lastChar=zone.m_numChar;
  if (subZone>=0) {
    if (subZone>=(int) zone.m_subSectionPosList.size())
      return true;
    firstChar=zone.m_subSectionPosList[size_t(subZone)];
    if (subZone+1<(int) zone.m_subSectionPosList.size())
      lastChar=zone.m_subSectionPosList[size_t(subZone+1)];
  }

  long actC = 0;
  for (size_t z = 0; z < numZones; z++) {
    MWAWEntry const &entry  =  zone.m_zones[z];
    long pos = entry.begin();
    libmwaw::DebugStream f, f2;

    int numC = int(entry.length()-4);
    input->seek(pos+4, librevenge::RVNG_SEEK_SET); // skip header

    for (int i = 0; i < numC; i++) {
      plcIt = zone.m_plcMap.find(actC);
      bool seeToken = false;
      while (actC>=firstChar && plcIt != zone.m_plcMap.end() && plcIt->first<=actC) {
        if (actC != plcIt->first) {
          MWAW_DEBUG_MSG(("ClarisDrawText::sendText: find a plc inside a complex char!!!\n"));
          f << "###";
        }
        ClarisDrawTextInternal::PLC const &plc = plcIt++->second;
        f << "[" << plc << "]";
        switch (plc.m_type) {
        case ClarisDrawTextInternal::P_Font:
          if (plc.m_id < 0 || plc.m_id >= int(zone.m_fontList.size())) {
            MWAW_DEBUG_MSG(("ClarisDrawText::sendText: can not find font %d\n", plc.m_id));
            f << "###";
            break;
          }
          listener->setFont(zone.m_fontList[size_t(plc.m_id)]);
          break;
        case ClarisDrawTextInternal::P_Ruler: {
          if (plc.m_id < 0 || plc.m_id >= numParaPLC)
            break;
          ClarisDrawTextInternal::ParagraphPLC const &paraPLC = zone.m_paragraphList[(size_t) plc.m_id];
          f << "[" << paraPLC << "]";
          if (paraPLC.m_rulerId < 0 || paraPLC.m_rulerId >= numParagraphs)
            break;
          ClarisDrawTextInternal::Paragraph para = m_state->m_paragraphsList[(size_t) paraPLC.m_rulerId];
          if (isOutline) {
            para.m_labelType=paraPLC.getLabelType();
            para.m_listLevelIndex=0;
            para.updateListLevel();

            actList = m_parserState->m_listManager->getNewList(actList, 1, *para.m_listLevel);
            para.m_listId=actList->getId();
          }
          listener->setParagraph(para);
          break;
        }
        case ClarisDrawTextInternal::P_Token: {
          if (plc.m_id < 0 || plc.m_id >= int(zone.m_tokenList.size())) {
            MWAW_DEBUG_MSG(("ClarisDrawText::sendText: can not find the token %d\n", plc.m_id));
            f << "###";
            break;
          }
          ClarisDrawTextInternal::Token const &token = zone.m_tokenList[size_t(plc.m_id)];
          switch (token.m_type) {
          case ClarisDrawTextInternal::TKN_FOOTNOTE:
            MWAW_DEBUG_MSG(("ClarisDrawText::sendText: can not send footnote in a paint file\n"));
            f << "###ftnote";
            break;
          case ClarisDrawTextInternal::TKN_PAGENUMBER:
            switch (token.m_unknown[0]) {
            case 1:
            case 2: {
              MWAW_DEBUG_MSG(("ClarisDrawText::sendText: find section token\n"));
              f << "##";
              listener->insertUnicodeString("1");
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
          case ClarisDrawTextInternal::TKN_GRAPHIC:
            MWAW_DEBUG_MSG(("ClarisDrawText::sendText: can not send graphic\n"));
            f << "###";
            break;
          case ClarisDrawTextInternal::TKN_FIELD:
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
              MWAW_DEBUG_MSG(("ClarisDrawText::sendText: can not find field token data\n"));
              listener->insertCharacter(' ');
            }
            listener->insertUnicode(0xbb);
            break;
          case ClarisDrawTextInternal::TKN_UNKNOWN:
          default:
            break;
          }
          seeToken = true;
          break;
        }
        /* checkme: normally, this corresponds to the first
           character following a 0xb/0x1, so we do not need to a
           column/page break here */
        case ClarisDrawTextInternal::P_Child:
        case ClarisDrawTextInternal::P_TextZone:
        case ClarisDrawTextInternal::P_Unknown:
        default:
          break;
        }
      }
      char c = (char) input->readULong(1);
      if (actC++<firstChar)
        continue;
      if (actC>lastChar) break;
      if (c == '\0') {
        if (i == numC-1) break;
        MWAW_DEBUG_MSG(("ClarisDrawText::sendText: OOPS, find 0 reading the text\n"));
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
        MWAW_DEBUG_MSG(("ClarisDrawText::sendText: Find unexpected char 1\n"));
        f << "###";
      case 0xb: // page break
        MWAW_DEBUG_MSG(("ClarisDrawText::sendText: Find unexpected page break\n"));
        f << "###pb";
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

bool ClarisDrawText::sendZone(int number, int subZone)
{
  std::map<int, shared_ptr<ClarisDrawTextInternal::DSET> >::iterator iter
    = m_state->m_zoneMap.find(number);
  if (iter == m_state->m_zoneMap.end())
    return false;
  shared_ptr<ClarisDrawTextInternal::DSET> zone = iter->second;
  if (zone)
    sendText(*zone, subZone);
  return true;
}

void ClarisDrawText::flushExtra()
{
  MWAW_DEBUG_MSG(("ClarisDrawText::flushExtra: not implemented\n"));
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
