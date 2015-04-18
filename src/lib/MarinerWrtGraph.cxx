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

#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWCell.hxx"
#include "MWAWTextListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWGraphicEncoder.hxx"
#include "MWAWGraphicListener.hxx"
#include "MWAWGraphicShape.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSubDocument.hxx"

#include "MarinerWrtParser.hxx"

#include "MarinerWrtGraph.hxx"

/** Internal: the structures of a MarinerWrtGraph */
namespace MarinerWrtGraphInternal
{
////////////////////////////////////////
//! Internal: the struct use to store a pattern in MarinerWrtGraph
struct Pattern {
  //! constructor with default pattern
  Pattern() : m_uniform(true), m_pattern(), m_percent(1)
  {
  }
  //! constructor ( 4 uint16_t by pattern )
  Pattern(uint16_t const *pat, bool uniform) : m_uniform(uniform), m_pattern(), m_percent(1)
  {
    m_pattern.m_dim=MWAWVec2i(8,8);
    m_pattern.m_colors[0]=MWAWColor::white();
    m_pattern.m_colors[1]=MWAWColor::black();
    for (size_t i=0; i < 4; ++i) {
      uint16_t val=pat[i];
      m_pattern.m_data.push_back((unsigned char)(val>>8));
      m_pattern.m_data.push_back((unsigned char)(val&0xFF));
    }
    int numOnes=0;
    for (size_t j=0; j < 8; ++j) {
      uint8_t val=m_pattern.m_data[j];
      for (int b=0; b < 8; b++) {
        if (val&1) ++numOnes;
        val = uint8_t(val>>1);
      }
    }
    m_percent=float(numOnes)/64.f;
  }
  //! a flag to know if the pattern is uniform or not
  bool m_uniform;
  //! the graphic style pattern
  MWAWGraphicStyle::Pattern m_pattern;
  //! the percent color
  float m_percent;
};

////////////////////////////////////////
//! Internal: the struct use to store a token entry
struct Token {
  //! constructor
  Token() : m_type(-1), m_highType(-1), m_dim(0,0), m_refType(0), m_refId(0), m_fieldType(0), m_value(""),
    m_pictData(), m_pictId(0), m_valPictId(0), m_pictBorderColor(MWAWColor::black()),
    m_ruleType(0), m_rulePattern(0), m_parsed(true), m_extra("")
  {
    for (int i = 0; i < 2; ++i)
      m_id[i] = 0;
    for (int i = 0; i < 4; ++i) {
      m_pictBorderType[i] = 0;
      m_pictBorderWidth[i] = 0;
    }
  }
  //! return true if the picture has some border
  bool hasPictBorders() const
  {
    for (int i = 0; i < 4; ++i)
      if (m_pictBorderType[i]) return true;
    return false;
  }
  //! add border properties
  void addPictBorder(MWAWGraphicStyle &style) const
  {
    if (!hasPictBorders()) return;
    for (int i = 0; i < 4; i++) {
      if (m_pictBorderType[i] <=0)
        continue;
      MWAWBorder border;
      border.m_color=m_pictBorderColor;
      switch (m_pictBorderType[i]) {
      case 1: // single[w=0.5]
        border.m_width = 0.5f;
      // fallthrough intended
      case 2:
        border.m_style = MWAWBorder::Simple;
        break;
      case 3:
        border.m_style = MWAWBorder::Dot;
        break;
      case 4:
        border.m_style = MWAWBorder::Dash;
        break;
      case 5:
        border.m_width = 2;
        break;
      case 6:
        border.m_width = 3;
        break;
      case 7:
        border.m_width = 6;
        break;
      case 8:
        border.m_type = MWAWBorder::Double;
        break;
      case 10:
        border.m_type = MWAWBorder::Double;
        border.m_widthsList.resize(3,1.);
        border.m_widthsList[0]=2.0;
        break;
      case 11:
        border.m_type = MWAWBorder::Double;
        border.m_widthsList.resize(3,1.);
        border.m_widthsList[2]=2.0;
        break;
      case 9:
        border.m_type = MWAWBorder::Double;
        border.m_width = 2;
        break;
      default:
        border.m_style = MWAWBorder::None;
        break;
      }
      static int const wh[] = { libmwaw::LeftBit, libmwaw::TopBit, libmwaw::RightBit, libmwaw::BottomBit };
      style.setBorders(wh[i], border);
    }
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Token const &tkn);
  //! the token id and the graph? id
  long m_id[2];
  //! the field type
  int m_type;
  //! the high byte of the field type
  int m_highType;
  //! the dimension
  MWAWVec2i m_dim;
  // for footnote
  //! the zone to used type
  int m_refType;
  //! the zone to used id
  uint32_t m_refId;
  // for field
  //! the field type
  int m_fieldType;
  //! the token value
  std::string m_value;
  // for picture
  //! the picture data
  MWAWEntry m_pictData;
  //! a picture id
  long m_pictId;
  //! a optional picture id
  long m_valPictId;
  //! the pict border color
  MWAWColor m_pictBorderColor;
  //! the pict border type
  int m_pictBorderType[4];
  //! the pict border width
  float m_pictBorderWidth[4];
  // for rule
  //! the rule type
  int m_ruleType;
  //! the rule pattern
  int m_rulePattern;
  //! true if the token has been send to a listener
  mutable bool m_parsed;
  //! some extra data
  std::string m_extra;
};

std::ostream &operator<<(std::ostream &o, Token const &tkn)
{
  if (tkn.m_id[0]) o << "id=" << std::hex << tkn.m_id[0] << std::dec << ",";
  if (tkn.m_id[1]) o << "id2=" << std::hex << tkn.m_id[1] << std::dec << ",";
  switch (tkn.m_type) {
  case -1:
    break;
  case 0x14:
    o << "graph";
    if (tkn.m_highType) o << "[" << tkn.m_highType << "]";
    o << ",";
    break;
  case 0x17:
    o << "date";
    if (tkn.m_highType!=1) o << "[" << tkn.m_highType << "]";
    o << ",";
    break;
  case 0x18:
    o << "time";
    if (tkn.m_highType!=1) o << "[" << tkn.m_highType << "]";
    o << ",";
    break;
  case 0x19:
    o << "pagenumber";
    if (tkn.m_highType!=1) o << "[" << tkn.m_highType << "]";
    o << ",";
    break;
  case 0x1e:
    o << "footnote[mark]";
    if (tkn.m_highType!=9) o << "[" << tkn.m_highType << "]";
    o << ",";
    break;
  case 0x1f:
    o << "footnote[content]";
    if (tkn.m_highType!=1) o << "[" << tkn.m_highType << "]";
    o << ",";
    break;
  case 0x23:
    o << "rule";
    if (tkn.m_highType!=1) o << "[" << tkn.m_highType << "]";
    o << ",";
    break;
  case 0x24:
    o << "field[formula]";
    if (tkn.m_highType!=9) o << "[" << tkn.m_highType << "]";
    o << ",";
    break;
  default:
    o << "#type=" << tkn.m_type << "[" << tkn.m_highType << "],";
  }
  if (tkn.m_fieldType)
    o << "field[type/val]=" << tkn.m_fieldType << ",";
  if (tkn.m_dim[0] || tkn.m_dim[1])
    o << "dim=" << tkn.m_dim << ",";
  if (tkn.m_value.length())
    o << "val=" << tkn.m_value << ",";
  if (tkn.m_pictId)
    o << "pictId=" << std::hex << tkn.m_pictId << std::dec << ",";
  if (tkn.m_valPictId && tkn.m_valPictId != tkn.m_pictId)
    o << "pictId[inValue]=" << std::hex << tkn.m_valPictId << std::dec << ",";
  if (!tkn.m_pictBorderColor.isBlack())
    o << "pict[color]=" << tkn.m_pictBorderColor << ",";
  if (tkn.hasPictBorders()) {
    o << "pict[borders]=[";
    for (int i=0; i < 4; ++i)
      o << tkn.m_pictBorderType[i] << ":" << tkn.m_pictBorderWidth[i] << ",";
    o << "],";
  }
  if (tkn.m_refId) {
    o << "zone[ref]=";
    if (tkn.m_refType==0xe)
      o << "footnote[" << std::hex << (tkn.m_refId&0xFFFFFFF) << std::dec << "],";
    else
      o << "#type" << tkn.m_refType << "[" << std::hex << (tkn.m_refId&0xFFFFFFF) << std::dec << "],";
  }
  switch (tkn.m_ruleType) {
  case 0:
    break; // no
  case 1:
    o << "rule[hairline],";
    break;
  case 2:
    break; // single
  case 3:
    o << "rule[w=2],";
    break;
  case 4:
    o << "rule[w=3],";
    break;
  case 5:
    o << "rule[w=4],";
    break;
  case 6:
    o << "rule[double],";
    break;
  case 7:
    o << "rule[double,w=2],";
    break;
  default:
    o << "#rule[type=" << tkn.m_ruleType << "],";
    break;
  }
  if (tkn.m_rulePattern)
    o << "rule[pattern]=" << tkn.m_rulePattern << ",";
  o << tkn.m_extra;
  return o;
}

////////////////////////////////////////
//! Internal: the struct use to store a ps zone of a MarinerWrtGraph
struct PSZone {
  //! constructor
  PSZone() : m_pos(), m_type(0), m_id(0), m_parsed(false), m_extra("")
  {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, PSZone const &file)
  {
    if (file.m_type) o << "type=" << file.m_type << ",";
    if (file.m_id > 0) o << "id=" << std::hex << file.m_id << std::dec << ",";
    o << file.m_extra;
    return o;
  }
  //! the file position
  MWAWEntry m_pos;
  //! a local type?
  int m_type;
  //! an id
  long m_id;
  //! a flag to know if the data has been sent
  mutable bool m_parsed;
  //! some extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the struct use to store a zone of a MarinerWrtGraph
struct Zone {
  //! constructor
  Zone() : m_tokenMap(), m_psZoneMap()
  {
  }
  //! the map id->token
  std::map<long, Token> m_tokenMap;
  //! the map id->entry to a psfile
  std::map<long, PSZone> m_psZoneMap;
};

////////////////////////////////////////
//! Internal: the state of a MarinerWrtGraph
struct State {
  //! constructor
  State() : m_zoneMap(), m_patternList(), m_numPages(0) { }

  //! set the default pattern map
  void setDefaultPatternList(int version);
  //! return a reference to a textzone ( if zone not exists, created it )
  Zone &getZone(int id)
  {
    std::map<int,Zone>::iterator it = m_zoneMap.find(id);
    if (it != m_zoneMap.end())
      return it->second;
    it = m_zoneMap.insert
         (std::map<int,Zone>::value_type(id,Zone())).first;
    return it->second;
  }

  //! a map id -> textZone
  std::map<int,Zone> m_zoneMap;
  //! a list patternId -> percent
  std::vector<Pattern> m_patternList;

  int m_numPages /* the number of pages */;
};

void State::setDefaultPatternList(int /*version*/)
{
  if (m_patternList.size()) return;
  static uint16_t const(dataV1[4*29])= {
    0x0000,0x0000,0x0000,0x0000,0x8800,0x0200,0x8800,0x2000,
    0x8800,0x2200,0x8800,0x2200,0x8800,0xaa00,0x8800,0xaa00,
    0x8822,0x8822,0x8822,0x8822,0x10aa,0x00aa,0x01aa,0x00aa,
    0xaa11,0xaa44,0xaa11,0xaa44,0xaa11,0xaa44,0xaa11,0xaa44,
    0xaa55,0xaa55,0xaa55,0xaa55,0x55aa,0x55bb,0x55aa,0x55bb,
    0x55ee,0x55bb,0x55ee,0x55bb,0x55ee,0x55ff,0x55ee,0x55ff,
    0xdd77,0xdd77,0xdd77,0xdd77,0xdd7f,0xddf7,0xdd7f,0xddf7,
    0xddff,0x77ff,0xddff,0x77ff,0xf7ff,0xddff,0x7fff,0xddff,
    0xffff,0xffff,0xffff,0xffff,
    0xffff,0x0000,0xffff,0x0000,0xcccc,0xcccc,0xcccc,0xcccc,
    0xcc66,0x3399,0xcc66,0x3399,0x9933,0x66cc,0x9933,0x66cc,
    0xcccc,0x3333,0xcccc,0x3333,0x33ff,0xccff,0x33ff,0xccff,
    0xff00,0x0000,0xff00,0x0000,0x8888,0x8888,0x8888,0x8888,
    0x8844,0x2211,0x8844,0x2211,0x1122,0x4488,0x1122,0x4488,
    0xff88,0x8888,0xff88,0x8888,0x5522,0x5588,0x5522,0x5588
  };
  for (size_t i=0; i < 29; ++i)
    m_patternList.push_back(Pattern(dataV1+4*i, i<17));
}

////////////////////////////////////////
//! Internal: the subdocument of a MarinerWrtGraph
class SubDocument : public MWAWSubDocument
{
public:
  //! constructor
  SubDocument(MarinerWrtGraph &pars, MWAWInputStreamPtr input, int id) :
    MWAWSubDocument(pars.m_mainParser, input, MWAWEntry()), m_graphParser(&pars), m_id(id) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const
  {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  /** the graph parser */
  MarinerWrtGraph *m_graphParser;
  //! the zone id
  int m_id;

private:
  SubDocument(SubDocument const &orig);
  SubDocument &operator=(SubDocument const &orig);
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("MarinerWrtGraphInternal::SubDocument::parse: no listener\n"));
    return;
  }
  if (!m_graphParser) {
    MWAW_DEBUG_MSG(("MarinerWrtGraphInternal::SubDocument::parse: no parser\n"));
    return;
  }

  long pos = m_input->tell();
  m_graphParser->sendText(m_id);
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_graphParser != sDoc->m_graphParser) return true;
  if (m_id != sDoc->m_id) return true;
  return false;
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MarinerWrtGraph::MarinerWrtGraph(MarinerWrtParser &parser) :
  m_parserState(parser.getParserState()), m_state(new MarinerWrtGraphInternal::State),
  m_mainParser(&parser)
{
}

MarinerWrtGraph::~MarinerWrtGraph()
{ }

int MarinerWrtGraph::version() const
{
  return m_parserState->m_version;
}

int MarinerWrtGraph::numPages() const
{
  if (m_state->m_numPages)
    return m_state->m_numPages;
  int nPages = 0;
  m_state->m_numPages = nPages;
  return nPages;
}

float MarinerWrtGraph::getPatternPercent(int id) const
{
  int numPattern = (int) m_state->m_patternList.size();
  if (!numPattern) {
    m_state->setDefaultPatternList(version());
    numPattern = int(m_state->m_patternList.size());
  }
  if (id < 0 || id >= numPattern)
    return -1.;
  return m_state->m_patternList[size_t(id)].m_percent;
}

void MarinerWrtGraph::sendText(int zoneId)
{
  if (zoneId)
    m_mainParser->sendText(zoneId);
}

void MarinerWrtGraph::sendToken(int zoneId, long tokenId)
{
  MWAWTextListenerPtr listener=m_parserState->m_textListener;
  if (!listener) {
    MWAW_DEBUG_MSG(("MarinerWrtGraph::sendToken: can not the listener\n"));
    return;
  }
  if (m_state->m_zoneMap.find(zoneId)==m_state->m_zoneMap.end()) {
    MWAW_DEBUG_MSG(("MarinerWrtGraph::sendToken: can not find zone %d\n", zoneId));
    return;
  }
  MarinerWrtGraphInternal::Zone &zone = m_state->getZone(zoneId);
  if (zone.m_tokenMap.find(tokenId)== zone.m_tokenMap.end()) {
    MWAW_DEBUG_MSG(("MarinerWrtGraph::sendToken: can not find token id %ld\n", tokenId));
    return;
  }
  MarinerWrtGraphInternal::Token const &token = zone.m_tokenMap.find(tokenId)->second;
  token.m_parsed = true;
  switch (token.m_type) {
  case 0x14:
    sendPicture(token);
    return;
  case 0x17:
    if (token.m_value.length()) {
      for (size_t c=0; c < token.m_value.length(); ++c)
        listener->insertCharacter((unsigned char) token.m_value[c]);
    }
    else
      listener->insertField(MWAWField(MWAWField::Date));
    return;
  case 0x18:
    if (token.m_value.length()) {
      for (size_t c=0; c < token.m_value.length(); ++c)
        listener->insertCharacter((unsigned char) token.m_value[c]);
    }
    else
      listener->insertField(MWAWField(MWAWField::Time));
    return;
  case 0x19: // fixme this can also be page count
    switch (token.m_fieldType) {
    case 0:
    case 4: // big roman
    case 6: // small roman
      listener->insertField(MWAWField(MWAWField::PageNumber));
      break;
    case 1:
    case 5: // big roman
    case 7: // small roman
      listener->insertField(MWAWField(MWAWField::PageCount));
      break;
    case 2:
      listener->insertField(MWAWField(MWAWField::PageNumber));
      listener->insertUnicodeString(" of ");
      listener->insertField(MWAWField(MWAWField::PageCount));
      break;
    case 3:
      listener->insertField(MWAWField(MWAWField::PageNumber));
      listener->insertChar('/');
      listener->insertField(MWAWField(MWAWField::PageCount));
      break;
    default:
      MWAW_DEBUG_MSG(("MarinerWrtGraph::sendToken: find unknown pagenumber style\n"));
      listener->insertField(MWAWField(MWAWField::PageNumber));
      break;
    }
    return;
  case 0x1e: {
    bool endNote=true;
    int fZoneId = m_mainParser->getZoneId(token.m_refId, endNote);
    MWAWSubDocumentPtr subdoc(new MarinerWrtGraphInternal::SubDocument(*this, m_parserState->m_input, fZoneId));
    listener->insertNote(MWAWNote(endNote ? MWAWNote::EndNote : MWAWNote::FootNote), subdoc);
    return;
  }
  case 0x1f: // footnote content, ok to ignore
    return;
  case 0x23:
    sendRule(token);
    return;
  case 0x24: // field
    listener->insertChar('[');
    if (token.m_value.length()) {
      for (size_t c=0; c < token.m_value.length(); ++c)
        listener->insertCharacter((unsigned char) token.m_value[c]);
    }
    else
      listener->insertUnicodeString("Merge Field");
    listener->insertChar(']');
    return;
  default:
    break;
  }

  MWAW_DEBUG_MSG(("MarinerWrtGraph::sendToken: sending type %x is not unplemented\n", (unsigned int) token.m_type));
}

void MarinerWrtGraph::sendRule(MarinerWrtGraphInternal::Token const &tkn)
{
  MWAWListenerPtr listener=m_parserState->m_textListener;
  if (!listener) {
    MWAW_DEBUG_MSG(("MarinerWrtGraph::sendRule: can not find the listener\n"));
    return;
  }
  MWAWVec2i const &sz=tkn.m_dim;
  if (sz[0] < 0 || sz[1] < 0 || (sz[0]==0 && sz[1]==0)) {
    MWAW_DEBUG_MSG(("MarinerWrtGraph::sendRule: the rule size seems bad\n"));
    return;
  }
  std::vector<float> listW;
  switch (tkn.m_ruleType) {
  case 0: // no width
    return;
  case 1:
    listW.resize(1,0.5f);
    break;
  default:
  case 2:
    listW.resize(1,1);
    break;
  case 6:
    listW.resize(3,1);
    break;
  case 3:
    listW.resize(1,2);
    break;
  case 7:
    listW.resize(3,2);
    break;
  case 4:
    listW.resize(1,3);
    break;
  case 5:
    listW.resize(1,4);
    break;
  }

  MarinerWrtGraphInternal::Pattern pat;
  if (tkn.m_rulePattern >= 0 && tkn.m_rulePattern < (int) m_state->m_patternList.size())
    pat = m_state->m_patternList[size_t(tkn.m_rulePattern)];
  else {
    MWAW_DEBUG_MSG(("MarinerWrtGraph::sendRule: can not find pattern\n"));
  }
  // retrieve the actual font to get the ruler color + a basic estimation of the line height
  MWAWFont actFont=listener->getFont();
  MWAWColor col;
  actFont.getColor(col);
  float lineH = actFont.size() > 0 ? actFont.size() : 12.f;
  float totalWidth=0;
  for (size_t l=0; l < listW.size(); ++l) totalWidth += listW[l];
  if (lineH<totalWidth) lineH=totalWidth;
  MWAWBox2f box(MWAWVec2f(0,0), MWAWVec2f(sz)+MWAWVec2f(0,lineH));
  MWAWPosition pos(box[0], box[1], librevenge::RVNG_POINT);
  pos.setRelativePosition(MWAWPosition::Char);
  MWAWGraphicStyle pStyle;
  MWAWGraphicShape shape;
  if (pat.m_uniform) {
    pStyle.m_lineWidth=listW[0];
    pStyle.m_lineColor=MWAWColor::barycenter(pat.m_percent,col,1.f-pat.m_percent,MWAWColor::white());
    shape=MWAWGraphicShape::line(MWAWVec2f(0,0), MWAWVec2f(sz));
  }
  else {
    pStyle.m_lineWidth=0;
    pat.m_pattern.m_colors[1]=col;
    pStyle.setPattern(pat.m_pattern);
    shape=MWAWGraphicShape::rectangle(MWAWBox2f(MWAWVec2f(0,0), MWAWVec2f(float(sz[0]),listW[0])));
  }

  if (listW.size()==1) {
    shape.m_bdBox=box;
    listener->insertPicture(pos,shape, pStyle);
    return;
  }
  MWAWGraphicEncoder graphicEncoder;
  MWAWGraphicListener graphicListener(*m_parserState, MWAWBox2f(MWAWVec2f(0,0), MWAWVec2f(sz)+MWAWVec2f(0,lineH)), &graphicEncoder);
  graphicListener.startDocument();
  float actH = (lineH-totalWidth)/2.f;
  for (size_t l=0; l < listW.size(); ++l) {
    if ((l%2)==0) {
      MWAWPosition linePos(MWAWVec2f(0,actH), MWAWVec2f(sz)+MWAWVec2f(0,listW[l]),librevenge::RVNG_POINT);
      linePos.m_anchorTo=MWAWPosition::Page;
      graphicListener.insertPicture(linePos, shape, pStyle);
    }
    actH += listW[l];
  }
  graphicListener.endDocument();
  librevenge::RVNGBinaryData data;
  std::string mime;
  if (graphicEncoder.getBinaryResult(data,mime))
    listener->insertPicture(pos,data,mime);
}

void MarinerWrtGraph::sendPicture(MarinerWrtGraphInternal::Token const &tkn)
{
  if (!tkn.m_pictData.valid()) {
    MWAW_DEBUG_MSG(("MarinerWrtGraph::sendPicture: can not find the graph\n"));
    return;
  }

  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  input->seek(tkn.m_pictData.begin(), librevenge::RVNG_SEEK_SET);
  librevenge::RVNGBinaryData data;
  input->readDataBlock(tkn.m_pictData.length(), data);

#ifdef DEBUG_WITH_FILES
  static int volatile pictName = 0;
  std::stringstream fName;
  fName << "Pict" << ++pictName << ".pct";
  libmwaw::Debug::dumpFile(data, fName.str().c_str());

  m_parserState->m_asciiFile.skipZone(tkn.m_pictData.begin(),tkn.m_pictData.end()-1);
#endif
  MWAWVec2i dim(tkn.m_dim);
  if (dim[0] <= 0 || dim[1] <= 0) {
    MWAW_DEBUG_MSG(("MarinerWrtGraph::sendPicture: can not find the picture dim\n"));
    dim = MWAWVec2i(100,100);
  }
  MWAWPosition posi(MWAWVec2i(0,0),dim,librevenge::RVNG_POINT);
  posi.setRelativePosition(MWAWPosition::Char);
  MWAWGraphicStyle style;
  tkn.addPictBorder(style);
  if (m_parserState->m_textListener)
    m_parserState->m_textListener->insertPicture(posi, data, "image/pict", style);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
}

void MarinerWrtGraph::sendPSZone(MarinerWrtGraphInternal::PSZone const &ps, MWAWPosition const &pos)
{
  ps.m_parsed = true;

  if (!ps.m_pos.valid()) {
    MWAW_DEBUG_MSG(("MarinerWrtGraph::sendPicture: can not find the graph\n"));
    return;
  }

  MWAWInputStreamPtr &input= m_parserState->m_input;
  long actPos = input->tell();
  input->seek(ps.m_pos.begin(), librevenge::RVNG_SEEK_SET);
  librevenge::RVNGBinaryData data;
  input->readDataBlock(ps.m_pos.length(), data);

#ifdef DEBUG_WITH_FILES
  static int volatile pictName = 0;
  std::stringstream fName;
  fName << "PS" << ++pictName << ".ps";
  libmwaw::Debug::dumpFile(data, fName.str().c_str());

  m_parserState->m_asciiFile.skipZone(ps.m_pos.begin(),ps.m_pos.end()-1);
#endif
  MWAWPosition pictPos(pos);
  if (pos.size()[0] <= 0 || pos.size()[1] <= 0)
    pictPos.setSize(MWAWVec2f(100,100));
  if (m_parserState->m_textListener)
    m_parserState->m_textListener->insertPicture(pictPos, data, "image/ps");
  input->seek(actPos, librevenge::RVNG_SEEK_SET);
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////

bool MarinerWrtGraph::readToken(MarinerWrtEntry const &entry, int zoneId)
{
  if (entry.length() < 3) {
    MWAW_DEBUG_MSG(("MarinerWrtGraph::readToken: data seems to short\n"));
    return false;
  }
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  input->pushLimit(entry.end());
  std::vector<MarinerWrtStruct> dataList;
  m_mainParser->decodeZone(dataList, 100);
  input->popLimit();

  size_t numData = dataList.size();
  if (numData < 16) {
    MWAW_DEBUG_MSG(("MarinerWrtGraph::readToken: find unexpected number of data\n"));
    return false;
  }

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  size_t d = 0;
  long val;

  MarinerWrtGraphInternal::Zone &zone = m_state->getZone(zoneId);
  MarinerWrtGraphInternal::Token tkn;
  for (int j = 0; j < 14; j++) {
    MarinerWrtStruct const &dt = dataList[d++];
    if (!dt.isBasic()) {
      f << "#f" << j << "=" << dt << ",";
      static bool first=true;
      if (first) {
        MWAW_DEBUG_MSG(("MarinerWrtGraph::readToken: find some struct block\n"));
        first = false;
      }
      continue;
    }
    int dim[2];
    switch (j) {
    case 0:
      tkn.m_id[0] = dt.value(0);
      break;
    case 1: // always 5
      if (dt.value(0) != 5)
        f << "f0=" << dt.value(0) << ",";
      break;
    case 2:
      val = dt.value(0);
      tkn.m_highType = int(val >> 16);
      tkn.m_type = int(val&0xFFFF);
      break;
    case 3:
    case 4:
      dim[0]= dim[1]= 0;
      dim[j-3]=(int) dt.value(0);
      if (j!=4)
        dim[++j-3]=(int) dataList[d++].value(0);
      tkn.m_dim = MWAWVec2i(dim[0],dim[1]);
      break;
    case 6:
      if (dt.value(0)) {
        uint32_t v = uint32_t(dt.value(0));
        tkn.m_refType = int(v>>28);
        tkn.m_refId = v;
      }
      break;
    case 8:
      tkn.m_fieldType = (int) dt.value(0);
      break;
    case 9:
      tkn.m_id[1] = dt.value(0);
      break;
    case 10:
    case 11:
    case 12:
    case 13: // 0 or 1 for graph : link to border ?
      tkn.m_pictBorderWidth[j-10]=(float) dt.value(0);
      while (j!=13)
        tkn.m_pictBorderWidth[++j-10]=(float) dataList[d++].value(0);
      break;
    default:
      if (dt.value(0))
        f << "#f" << j << "=" << dt.value(0) << ",";
      break;
    }
  }
  tkn.m_extra = f.str();
  f.str("");
  f << entry.name() << ":" << tkn;
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());

  ascFile.addPos(dataList[d].m_filePos);
  f.str("");
  f << entry.name() << "(II):type=" << std::hex << tkn.m_type << std::dec << ",";
  for (int i = 0; i < 2; i++) {
    MarinerWrtStruct const &data = dataList[d++];
    std::string str;
    if (i==0 && readTokenBlock0(data, tkn, str)) {
      if (str.length())
        f << "block0=[" << str << "],";
      continue;
    }
    if (data.m_type != 0) {
      MWAW_DEBUG_MSG(("MarinerWrtGraph::readToken(II): can not read block%d\n", i));
      f << "###bl" << i << "=" << data << ",";
    }
    else {
      f << "bl" << i << "=[";
      input->seek(data.m_pos.begin(), librevenge::RVNG_SEEK_SET);
      for (int j = 0; j < int(data.m_pos.length()/2); j++) {
        if (i==1 && j == 1 && data.m_pos.length() >= 12) {
          for (int c=0; c<4; ++c)
            tkn.m_pictBorderType[c]=(int) input->readULong(1);
          j+=2;
          // checkme: only for picture or always ?
          unsigned char col[]= {0,0,0};
          for (int c=0; c<3; ++c, ++j)
            col[c]=(unsigned char)(input->readULong(2)>>8);
          tkn.m_pictBorderColor = MWAWColor(col[0],col[1],col[2]);
          if (!tkn.m_pictBorderColor.isBlack()) f << "bordColor=" << tkn.m_pictBorderColor << ",";
          continue;
        }
        val = (long) input->readULong(2);
        if (val) f << "f" << j << "=" << std::hex << val << std::dec << ",";
      }
      f << "],";
    }
  }
  if (tkn.m_type==0x24 && d < numData && tkn.m_value.empty()) {
    std::string str;
    if (readTokenBlock0(dataList[d], tkn, str)) {
      d++;
      f << str;
    }
  }
  if (tkn.m_type != 0x14 || numData < 32) {
    for (; d < numData; d++) {
      MarinerWrtStruct const &data = dataList[d];
      f << "#" << data << ",";
      static bool first = true;
      if (first) {
        first = false;
        MWAW_DEBUG_MSG(("MarinerWrtGraph::readToken(II): find some extra data \n"));
      }
    }
    zone.m_tokenMap[tkn.m_id[0]] = tkn;
    ascFile.addNote(f.str().c_str());

    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }

  // ok now read the picture data
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(dataList[d].m_filePos);
  f.str("");
  for (int j = 0; j < 15; j++) {
    MarinerWrtStruct const &dt = dataList[d++];
    if (!dt.isBasic()) {
      f << "#f" << j << "=" << dt << ",";
      MWAW_DEBUG_MSG(("MarinerWrtGraph::readToken(III): find some struct block\n"));
      continue;
    }
    switch (j) {
    case 8:
      if (!dt.value(0))
        break;
      tkn.m_pictId = dt.value(0);
      f << "pictId=" <<  std::hex << tkn.m_pictId << std::dec << ",";
      break;
    default:
      if (dt.value(0))
        f << "#f" << j << "=" << dt.value(0) << ",";
    }
  }

  MarinerWrtStruct dt = dataList[d++];
  if (dt.m_type != 0 || !dt.m_pos.length()) {
    MWAW_DEBUG_MSG(("MarinerWrtGraph::readToken: can not find the picture data\n"));
    f << "###pictData=" << dt << ",";
  }
  else
    tkn.m_pictData = dt.m_pos;

  for (; d < numData; d++) {
    MarinerWrtStruct const &data = dataList[d];
    f << "#" << data << ",";
    static bool first = true;
    if (first) {
      first = false;
      MWAW_DEBUG_MSG(("MarinerWrtGraph::readToken(III): find some extra data \n"));
    }
  }
  tkn.m_extra += f.str();
  zone.m_tokenMap[tkn.m_id[0]] = tkn;

  std::stringstream f2;
  f2 << entry.name() << "(III):" << f.str();
  ascFile.addNote(f2.str().c_str());

  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  return true;
}

bool MarinerWrtGraph::readTokenBlock0(MarinerWrtStruct const &data, MarinerWrtGraphInternal::Token &tkn, std::string &res)
{
  res = "";
  if (data.m_type != 0 || !data.m_pos.valid()) {
    MWAW_DEBUG_MSG(("MarinerWrtGraph::readTokenBlock0: called without data\n"));
    return false;
  }
  if (data.m_pos.length()<0x2c) {
    MWAW_DEBUG_MSG(("MarinerWrtGraph::readTokenBlock0: the data seems very short\n"));
    return false;
  }

  std::stringstream f;

  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = data.m_pos.begin(), endPos = data.m_pos.end();
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  // fixme: this depends on the token type
  long val;
  int firstExpectedVal= (tkn.m_type==0x14) ? 28 :
                        (tkn.m_type==0x17||tkn.m_type==0x18) ? 6 : 0;
  for (int i = 0; i < firstExpectedVal/2; i++) {
    val = (long) input->readLong(2);
    if (val) f << "#f" << i << "=" << val << ",";
  }
  input->seek(pos+firstExpectedVal, librevenge::RVNG_SEEK_SET);
  std::string fValue("");
  switch (tkn.m_type) {
  case 0x14:
    tkn.m_valPictId = input->readLong(4);
    if (tkn.m_valPictId) f << "pId=" << std::hex << tkn.m_valPictId << ",";
    break;
  case 0x17:
  case 0x18:
    val = input->readLong(2);
    if (val) f << "f0=" << val << ","; // fieldType?
  case 0x19:
  case 0x1e:
  case 0x1f:
  case 0x24:
    while (!input->isEnd()) {
      if (input->tell() >= endPos)
        break;
      val = (long) input->readULong(1);
      if (!val) {
        input->seek(-1, librevenge::RVNG_SEEK_CUR);
        break;
      }
      fValue += (char) val;
    }
    break;
  case 0x23:
    // either big or small endian
    tkn.m_ruleType = (int) input->readULong(2);
    if ((tkn.m_ruleType&0xFF)==0) tkn.m_ruleType>>=8;
    tkn.m_rulePattern = (int) input->readULong(2);
    if ((tkn.m_rulePattern&0xFF)==0) tkn.m_rulePattern>>=8;
    switch (tkn.m_ruleType) {
    case 0:
      break; // no
    case 1:
      f << "rule[hairline],";
      break;
    case 2:
      f << "rule[single],";
      break;
    case 3:
      f << "rule[w=2],";
      break;
    case 4:
      f << "rule[w=3],";
      break;
    case 5:
      f << "rule[w=4],";
      break;
    case 6:
      f << "rule[double],";
      break;
    case 7:
      f << "rule[double,w=2],";
      break;
    default:
      f << "#rule[type=" << tkn.m_ruleType << "],";
      break;
    }
    if (tkn.m_rulePattern)
      f << "rule[pattern]=" << tkn.m_rulePattern << ",";
    break;
  default:
    break;
  }
  if (fValue.length()) {
    tkn.m_value = fValue;
    f << "val=" << fValue << ",";
  }
  int numRemains = int(endPos-input->tell())/2;
  for (int i = 0; i < numRemains; i++) { // always 0
    val = input->readLong(2);
    if (val) f << "#g" << i << "=" << val << ",";
  }
  res = f.str();
  tkn.m_extra += res;
  return true;
}

bool MarinerWrtGraph::readPostscript(MarinerWrtEntry const &entry, int zoneId)
{
  if (entry.length() < 3) {
    MWAW_DEBUG_MSG(("MarinerWrtGraph::readPostscript: data seems to short\n"));
    return false;
  }
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  input->pushLimit(entry.end());
  std::vector<MarinerWrtStruct> dataList;
  m_mainParser->decodeZone(dataList, 1+3);
  input->popLimit();

  if (int(dataList.size()) != 3) {
    MWAW_DEBUG_MSG(("MarinerWrtGraph::readPostscript: find unexpected number of data\n"));
    return false;
  }

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  size_t d = 0;
  MarinerWrtGraphInternal::Zone &zone = m_state->getZone(zoneId);
  MarinerWrtGraphInternal::PSZone psFile;
  for (int i = 0; i < 2; i++) {
    MarinerWrtStruct const &data = dataList[d++];
    if (!data.isBasic()) {
      MWAW_DEBUG_MSG(("MarinerWrtGraph::readPostscript: find unexpected type for f0\n"));
      f << "###f" << i << "=" << data << ",";
    }
    else if (i==0)
      psFile.m_type = (int) data.value(0);
    else
      psFile.m_id = data.value(0);
  }
  MarinerWrtStruct const &data = dataList[d++];
  if (data.m_type != 0) {
    MWAW_DEBUG_MSG(("MarinerWrtGraph::readPostscript: can not find my file\n"));
    f << "###";
    psFile.m_extra=f.str();
  }
  else if (data.m_pos.valid()) {
    psFile.m_extra=f.str();
    psFile.m_pos = data.m_pos;
    zone.m_psZoneMap[psFile.m_id] = psFile;
#ifdef DEBUG_WITH_FILES
    input->seek(data.m_pos.begin(), librevenge::RVNG_SEEK_SET);
    librevenge::RVNGBinaryData file;
    input->readDataBlock(data.m_pos.length(), file);

    static int volatile psName = 0;
    std::stringstream fName;
    fName << "PS" << ++psName << ".ps";
    libmwaw::Debug::dumpFile(file, fName.str().c_str());

    ascFile.skipZone(data.m_pos.begin(),data.m_pos.end()-1);
#endif
  }
  f.str("");
  f << entry.name() << ":" << psFile;
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// send data to a listener
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// low level
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// send data
////////////////////////////////////////////////////////////
bool MarinerWrtGraph::sendPageGraphics()
{
  return true;
}

void MarinerWrtGraph::flushExtra()
{
#ifdef DEBUG
  std::map<int,MarinerWrtGraphInternal::Zone>::const_iterator it = m_state->m_zoneMap.begin();
  std::map<long,MarinerWrtGraphInternal::Token>::const_iterator tIt;
  std::map<long,MarinerWrtGraphInternal::PSZone>::const_iterator psIt;
  MWAWPosition pictPos(MWAWVec2i(0,0),MWAWVec2i(0,0),librevenge::RVNG_POINT);
  pictPos.setRelativePosition(MWAWPosition::Char);

  while (it != m_state->m_zoneMap.end()) {
    int zId = it->first;
    MarinerWrtGraphInternal::Zone const &zone = it++->second;

    tIt = zone.m_tokenMap.begin();
    while (tIt != zone.m_tokenMap.end()) {
      long tId = it->first;
      MarinerWrtGraphInternal::Token const &tkn = tIt++->second;
      if (tkn.m_parsed) continue;
      sendToken(zId, tId);
    }
    psIt = zone.m_psZoneMap.begin();
    while (psIt != zone.m_psZoneMap.end()) {
      MarinerWrtGraphInternal::PSZone const &psZone = psIt++->second;
      if (psZone.m_parsed) continue;
      sendPSZone(psZone, pictPos);
    }
  }
#endif
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
