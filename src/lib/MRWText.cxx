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

#include "MWAWCell.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWDebug.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSection.hxx"
#include "MWAWSubDocument.hxx"

#include "MRWParser.hxx"

#include "MRWText.hxx"

/** Internal: the structures of a MRWText */
namespace MRWTextInternal
{
////////////////////////////////////////
//! Internal: struct used to store the font of a MRWText
struct Font {
  //! constructor
  Font() : m_font(), m_localId(-1), m_tokenId(0), m_extra("") {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Font const &font);

  //! the font
  MWAWFont m_font;
  //! the local id
  int m_localId;
  //! the token id
  long m_tokenId;
  //! extra data
  std::string m_extra;
};

std::ostream &operator<<(std::ostream &o, Font const &font)
{
  if (font.m_localId >= 0)
    o << "FN" << font.m_localId << ",";
  if (font.m_tokenId > 0)
    o << "tokId=" << std::hex << font.m_tokenId << std::dec << ",";
  o << font.m_extra;
  return o;
}

////////////////////////////////////////
//! Internal: struct used to store the paragraph of a MRWText
struct Paragraph : public MWAWParagraph {
  //! constructor
  Paragraph() : MWAWParagraph(), m_paraFill(),
    m_cellWidth(0), m_cellHeight(0), m_cellSep(0), m_cellFill() {
  }
  //! updates the paragraph knowing the paragraph pattern percent
  void update(float percent) {
    if (m_paraFill.hasBackgroundColor())
      m_backgroundColor=m_paraFill.getBackgroundColor(percent);
    if (!m_paraFill.hasBorders())
      return;
    static int const wh[] = { libmwaw::Left, libmwaw::Top, libmwaw::Right, libmwaw::Bottom };
    m_borders.resize(4);
    for (int i = 0; i < 4; i++) {
      if (m_paraFill.m_borderTypes[i] <=0)
        continue;
      m_borders[size_t(wh[i])]=m_paraFill.getBorder(i);
    }
  }
  //! updates the paragraph knowing the paragraph pattern percent
  void update(float percent, MWAWCell &cell) const {
    if (m_cellFill.hasBackgroundColor())
      cell.setBackgroundColor(m_cellFill.getBackgroundColor(percent));
    if (!m_cellFill.hasBorders())
      return;
    static int const wh[] = { libmwaw::LeftBit, libmwaw::TopBit, libmwaw::RightBit, libmwaw::BottomBit };
    for (int i = 0; i < 4; i++) {
      if (m_cellFill.m_borderTypes[i] <=0)
        continue;
      cell.setBorders(wh[i], m_cellFill.getBorder(i));
    }
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Paragraph const &para);
  //! small structure to store border/fills properties in MRWText
  struct BorderFill {
    //! constructor
    BorderFill() : m_foreColor(MWAWColor::black()), m_backColor(MWAWColor::white()), m_patternId(0),
      m_borderColor(MWAWColor::black()) {
      for (int i=0; i < 4; i++)
        m_borderTypes[i]=0;
    }
    //! return true if the properties are default properties
    bool isDefault() const {
      return !hasBorders() && !hasBackgroundColor();
    }
    //! reset the background color
    void resetBackgroundColor() {
      m_foreColor=MWAWColor::black();
      m_backColor=MWAWColor::white();
      m_patternId=0;
    }
    //! return true if we have a not white background color
    bool hasBackgroundColor() const {
      return !m_foreColor.isBlack()||!m_backColor.isWhite()||m_patternId;
    }
    //! returns the background color knowing the pattern percent
    MWAWColor getBackgroundColor(float percent) const {
      if (percent < 0.0)
        return m_backColor;
      return MWAWColor::barycenter(percent,m_foreColor,1.f-percent,m_backColor);
    }
    //! reset the borders
    void resetBorders() {
      for (int i=0; i < 4; i++)
        m_borderTypes[i]=0;
    }
    //! return true if we have border
    bool hasBorders() const {
      for (int i = 0; i < 4; i++)
        if (m_borderTypes[i]) return true;
      return false;
    }
    //! return a border corresponding to a pos
    MWAWBorder getBorder(int pos) const;
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, BorderFill const &fill);

    //! the foreground color
    MWAWColor m_foreColor;
    //! the background color
    MWAWColor m_backColor;
    //! the pattern id
    int m_patternId;
    //! the border color
    MWAWColor m_borderColor;
    //! the border type L T R B
    int m_borderTypes[4];
  };
  //! the paragraph fill properties
  BorderFill m_paraFill;
  //! a cell width
  int m_cellWidth;
  //! a cell height
  int m_cellHeight;
  //! a cell separator
  int m_cellSep;
  //! the cell fill properties
  BorderFill m_cellFill;
};

std::ostream &operator<<(std::ostream &o, Paragraph const &para)
{
  o << reinterpret_cast<MWAWParagraph const &>(para);
  if (para.m_cellWidth)
    o << "cellWidth=" << para.m_cellWidth << ",";
  if (para.m_cellHeight > 0)
    o << "cellHeight[atLeast]=" << para.m_cellHeight << ",";
  else if (para.m_cellHeight < 0)
    o << "cellHeight=" << -para.m_cellHeight << ",";
  if (para.m_cellSep)
    o << "cellSep=" << para.m_cellSep << ",";
  if (!para.m_paraFill.isDefault())
    o << para.m_paraFill;
  if (!para.m_cellFill.isDefault())
    o << "cell=[" << para.m_cellFill << "]";
  return o;
}

MWAWBorder Paragraph::BorderFill::getBorder(int i) const
{
  MWAWBorder res;
  switch(m_borderTypes[i]) {
  case 0:
    res.m_style = MWAWBorder::None;
    break;
  case 1: // single[w=0.5]
    res.m_width = 0.5f;
  case 2:
    res.m_style = MWAWBorder::Simple;
    break;
  case 3:
    res.m_style = MWAWBorder::Dot;
    break;
  case 4:
    res.m_style = MWAWBorder::Dash;
    break;
  case 5:
    res.m_width = 2;
    break;
  case 6:
    res.m_width = 3;
    break;
  case 7:
    res.m_width = 6;
    break;
  case 8:
  case 10: // 1 then 2
  case 11: // 2 then 1
    res.m_type = MWAWBorder::Double;
    break;
  case 9:
    res.m_type = MWAWBorder::Double;
    res.m_width = 2;
    break;
  default:
    res.m_style = MWAWBorder::None;
    break;
  }
  res.m_color=m_borderColor;
  return res;
}

std::ostream &operator<<(std::ostream &o, Paragraph::BorderFill const &fill)
{
  if (fill.hasBackgroundColor()) {
    o << "fill=[";
    if (!fill.m_foreColor.isBlack()) o << "foreColor=" << fill.m_foreColor << ",";
    if (!fill.m_backColor.isWhite()) o << "backColor=" << fill.m_backColor << ",";
    if (fill.m_patternId) o << "patId=" << fill.m_patternId << ",";
    o << "],";
  }
  static char const *(wh[]) = {"bordL", "bordT", "bordR", "bordB" };
  if (!fill.m_borderColor.isBlack() && fill.hasBorders())
    o << "borderColor=" << fill.m_borderColor << ",";
  for (int i = 0; i < 4; i++) {
    if (!fill.m_borderTypes[i]) continue;
    o << wh[i] << "=";
    switch(fill.m_borderTypes[i]) {
    case 0:
      break;
    case 1:
      o << "single[w=0.5],";
      break;
    case 2:
      o << "single,";
      break;
    case 3:
      o << "dot,";
      break;
    case 4:
      o << "dash,";
      break;
    case 5:
      o << "single[w=2],";
      break;
    case 6:
      o << "single[w=3],";
      break;
    case 7:
      o << "single[w=6],";
      break;
    case 8:
      o << "double,";
      break;
    case 9:
      o << "double[w=2],";
      break;
    case 10:
      o << "double[w=1|2],";
      break;
    case 11:
      o << "double[w=2|1],";
      break;
    default:
      o << "#" << fill.m_borderTypes[i] << ",";
      break;
    }
  }
  return o;
}

////////////////////////////////////////
//! Internal: struct used to store zone data of a MRWText
struct Zone {
  struct Information;

  //! constructor
  Zone(int zId) : m_id(zId), m_infoList(), m_fontList(),m_rulerList(), m_idFontMap(), m_posFontMap(), m_posRulerMap(), m_actZone(0), m_parsed(false) {
  }

  //! returns the file position and the number of the sub zone
  bool getPosition(long cPos, long &fPos, size_t &subZone) const {
    if (cPos < 0) return false;
    long nChar= cPos;
    for (size_t z = 0; z < m_infoList.size(); z++) {
      if (m_infoList[z].m_pos.length() > nChar) {
        fPos = m_infoList[z].m_pos.begin()+nChar;
        subZone = z;
        return true;
      }
      nChar -= m_infoList[z].m_pos.length();
    }
    return false;
  }
  //! returns the zone length
  long length() const {
    long res=0;
    for (size_t z = 0; z < m_infoList.size(); z++)
      res += m_infoList[z].m_pos.length();
    return res;
  }
  //! returns a fonts corresponding to an id (if possible)
  bool getFont(int id, Font &ft) const {
    if (id < 0 || id >= int(m_fontList.size())) {
      MWAW_DEBUG_MSG(("MRWTextInternal::Zone::getFont: can not find font %d\n", id));
      return false;
    }
    ft = m_fontList[size_t(id)];
    if (m_idFontMap.find(ft.m_localId) == m_idFontMap.end()) {
      MWAW_DEBUG_MSG(("MRWTextInternal::Zone::getFont: can not find font id %d\n", id));
    } else
      ft.m_font.setId(m_idFontMap.find(ft.m_localId)->second);
    return true;
  }
  //! returns a ruler corresponding to an id (if possible)
  bool getRuler(int id, Paragraph &ruler) const {
    if (id < 0 || id >= int(m_rulerList.size())) {
      MWAW_DEBUG_MSG(("MRWTextInternal::Zone::getParagraph: can not find paragraph %d\n", id));
      return false;
    }
    ruler = m_rulerList[size_t(id)];
    return true;
  }
  //! the zone id
  int m_id;
  //! the list of information of the text in the file
  std::vector<Information> m_infoList;
  //! a list of font
  std::vector<Font> m_fontList;
  //! a list of ruler
  std::vector<Paragraph> m_rulerList;
  //! a map id -> fontId
  std::map<int,int> m_idFontMap;
  //! a map pos -> fontId
  std::map<long,int> m_posFontMap;
  //! a map pos -> rulerId
  std::map<long,int> m_posRulerMap;
  //! a index used to know the next zone in MRWText::readZone
  int m_actZone;
  //! a flag to know if the zone is parsed
  mutable bool m_parsed;

  //! struct used to keep the information of a small zone of MRWTextInternal::Zone
  struct Information {
    //! constructor
    Information() : m_pos(), m_cPos(0,0), m_extra("") { }
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Information const &info) {
      if (info.m_cPos[0] || info.m_cPos[1])
        o << "cPos=" << std::hex << info.m_cPos << std::dec << ",";
      o << info.m_extra;
      return o;
    }

    //! the file position
    MWAWEntry m_pos;
    //! the characters positions
    Vec2l m_cPos;
    //! extra data
    std::string m_extra;
  };
};

////////////////////////////////////////
//! Internal: struct used to store the table of a MRWText
struct Table {
  struct Cell;
  struct Row;
  //! constructor
  Table(Zone const &zone) : m_zone(zone), m_rowsList() {
  }
  //! returns the next char position after the table
  long nextCharPos() const {
    if (!m_rowsList.size()) {
      MWAW_DEBUG_MSG(("MRWTextInternal::Table: can not compute the last position\n"));
      return -1;
    }
    return m_rowsList.back().m_lastChar;
  }
  //! the actual zone
  Zone const &m_zone;
  //! the list of row
  std::vector<Row> m_rowsList;

  //! a table row of a MRWText
  struct Row {
    //! constructor
    Row() :  m_lastChar(-1), m_height(0), m_cellsList() {
    }
    //! the last table position
    long m_lastChar;
    //! the table height ( <=0 a least )
    int m_height;
    //! a list of cell entry list
    std::vector<Cell> m_cellsList;
  };
  //! a table cell of a MRWText
  struct Cell {
    // constructor
    Cell() : m_entry(), m_rulerId(-1), m_width(-1) {
    }
    // returns true if the information are find
    bool ok() const {
      return m_entry.length()>0 && m_rulerId>=0 && m_width>=0;
    }
    //! the cell entry
    MWAWEntry m_entry;
    //! a list of cell ruler id
    int m_rulerId;
    //! the column width
    int m_width;
  };
};

////////////////////////////////////////
//! Internal: the state of a MRWText
struct State {
  //! constructor
  State() : m_version(-1), m_textZoneMap(), m_numPages(-1), m_actualPage(0) {
  }

  //! return a reference to a textzone ( if zone not exists, created it )
  Zone &getZone(int id) {
    std::map<int,Zone>::iterator it = m_textZoneMap.find(id);
    if (it != m_textZoneMap.end())
      return it->second;
    it = m_textZoneMap.insert
         (std::map<int,Zone>::value_type(id,Zone(id))).first;
    return it->second;
  }
  //! the file version
  mutable int m_version;
  //! a map id -> textZone
  std::map<int,Zone> m_textZoneMap;
  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MRWText::MRWText(MRWParser &parser) :
  m_parserState(parser.getParserState()), m_state(new MRWTextInternal::State), m_mainParser(&parser)
{
}

MRWText::~MRWText()
{
}

int MRWText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}

int MRWText::numPages() const
{
  if (m_state->m_numPages <= 0) {
    int nPages = 0;
    std::map<int,MRWTextInternal::Zone>::const_iterator it = m_state->m_textZoneMap.begin();
    for ( ; it != m_state->m_textZoneMap.end(); it++) {
      nPages = computeNumPages(it->second);
      if (nPages)
        break;
    }
    m_state->m_numPages=nPages ? nPages : 1;
  }
  return m_state->m_numPages;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
//     Text
////////////////////////////////////////////////////////////
bool MRWText::readZone(MRWEntry const &entry, int zoneId)
{
  if (entry.length() < 0x3) {
    MWAW_DEBUG_MSG(("MRWText::readZone: data seems to short\n"));
    return false;
  }

  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(entry.begin(), WPX_SEEK_SET);
  input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  m_mainParser->decodeZone(dataList);
  input->popLimit();

  if (dataList.size() != 1) {
    MWAW_DEBUG_MSG(("MRWText::readZone: can find my data\n"));
    return false;
  }

  libmwaw::DebugStream f;
  f << entry.name() << "[data]:";
  MRWStruct const &data = dataList[0];
  if (data.m_type) {
    MWAW_DEBUG_MSG(("MRWText::readZone: find unexpected type zone\n"));
    return false;
  }

  MRWTextInternal::Zone &zone = m_state->getZone(zoneId);
  if (zone.m_actZone < 0 || zone.m_actZone >= int(zone.m_infoList.size())) {
    MWAW_DEBUG_MSG(("MRWText::readZone: actZone seems bad\n"));
    if (zone.m_actZone < 0)
      zone.m_actZone = int(zone.m_infoList.size());
    zone.m_infoList.resize(size_t(zone.m_actZone)+1);
  }
  MRWTextInternal::Zone::Information &info
    = zone.m_infoList[size_t(zone.m_actZone++)];
  info.m_pos = data.m_pos;

  m_parserState->m_asciiFile.addPos(entry.begin());
  m_parserState->m_asciiFile.addNote(f.str().c_str());

  input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

int MRWText::computeNumPages(MRWTextInternal::Zone const &zone) const
{
  int nPages = 0;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  for (size_t z=0; z < zone.m_infoList.size(); z++) {
    MRWTextInternal::Zone::Information const &info=zone.m_infoList[z];
    if (!info.m_pos.valid()) continue;
    if (nPages==0) nPages=1;
    input->seek(info.m_pos.begin(), WPX_SEEK_SET);
    long numChar = info.m_pos.length();
    while (numChar-- > 0) {
      if (input->readULong(1)==0xc)
        nPages++;
    }
  }

  input->seek(pos, WPX_SEEK_SET);
  return nPages;
}

bool MRWText::readTextStruct(MRWEntry const &entry, int zoneId)
{
  if (entry.length() < entry.m_N) {
    MWAW_DEBUG_MSG(("MRWText::readTextStruct: data seems to short\n"));
    return false;
  }
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(entry.begin(), WPX_SEEK_SET);
  input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  m_mainParser->decodeZone(dataList, 1+22*entry.m_N);
  input->popLimit();

  if (int(dataList.size()) != 22*entry.m_N) {
    MWAW_DEBUG_MSG(("MRWText::readTextStruct: find unexpected number of data\n"));
    return false;
  }

  MRWTextInternal::Zone &zone = m_state->getZone(zoneId);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  size_t d = 0;
  for (int i = 0; i < entry.m_N; i++) {
    MRWTextInternal::Zone::Information info;
    ascFile.addPos(dataList[d].m_filePos);

    int posi[4]= {0,0,0,0};
    int dim[4]= {0,0,0,0}, dim2[4]= {0,0,0,0};
    f.str("");
    for (int j = 0; j < 22; j++) {
      MRWStruct const &data = dataList[d++];
      if (!data.isBasic()) {
        f << "###f" << j << "=" << data << ",";
        continue;
      }
      switch(j) {
      case 0:
      case 1:
        info.m_cPos[j] = data.value(0);
        break;
      case 2:
        if (data.value(0))
          f << "fl0=" << std::hex << data.value(0) << std::dec << ",";
        break;
      case 3:
      case 4:
      case 5:
      case 6:
        posi[j-3]=(int) data.value(0);
        while (j < 6)
          posi[++j-3]=(int) dataList[d++].value(0);
        if (posi[0] || posi[1])
          f << "pos0=" << posi[0] << ":" << posi[1] << ",";
        if (posi[2] || posi[3])
          f << "pos1=" << posi[2] << ":" << posi[3] << ",";
        break;
      case 8:
      case 9:
      case 10:
      case 11:
        dim[j-8]=(int) data.value(0);
        while (j < 11)
          dim[++j-8]=(int) dataList[d++].value(0);
        if (dim[0]||dim[1]||dim[2]||dim[3])
          f << "pos2=" << dim[1] << "x" << dim[0] << "<->" << dim[3] << "x" << dim[2] << ",";
        break;
      case 12:
        if (data.value(0)!=info.m_cPos[1]-info.m_cPos[0])
          f << "nChar(diff)=" << info.m_cPos[1]-info.m_cPos[0]-data.value(0) << ",";
        break;
      case 14: // a small number(cst) in the list of structure
      case 16: // an id?
        if (data.value(0))
          f << "f" << j << "=" << data.value(0) << ",";
        break;
      case 15: // 0xcc00|0xce40|... some flags? a dim
        if (data.value(0))
          f << "f" << j << "=" << std::hex << data.value(0) << std::dec << ",";
        break;
      case 17:
      case 18:
      case 19:
      case 20:
        dim2[j-17]=(int) data.value(0);
        while (j < 20)
          dim2[++j-17]=(int) dataList[d++].value(0);
        if (dim2[0]||dim2[1]||dim2[2]||dim2[3])
          f << "pos3=" << dim2[1] << "x" << dim2[0] << "<->" << dim2[3] << "x" << dim2[2] << ",";
        break;
      default:
        if (data.value(0))
          f << "#f" << j << "=" << data.value(0) << ",";
        break;
      }
    }
    info.m_extra = f.str();
    zone.m_infoList.push_back(info);
    f.str("");
    f << entry.name() << "-" << i << ":" << info;
    ascFile.addNote(f.str().c_str());
  }
  input->seek(entry.end(), WPX_SEEK_SET);

  return true;
}

bool MRWText::send(int zoneId)
{
  if (!m_parserState->m_listener) {
    MWAW_DEBUG_MSG(("MRWText::send: can not find the listener\n"));
    return false;
  }
  if (m_state->m_textZoneMap.find(zoneId) == m_state->m_textZoneMap.end()) {
    MWAW_DEBUG_MSG(("MRWText::send: can not find the text zone %d\n", zoneId));
    return false;
  }
  MRWTextInternal::Zone const &zone=m_state->getZone(zoneId);
  MWAWEntry entry;
  entry.setBegin(0);
  entry.setEnd(zone.length());
  entry.setId(zoneId);
  return send(zone,entry);
}

bool MRWText::send(MRWTextInternal::Zone const &zone, MWAWEntry const &entry)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("MRWText::send: can not find the listener\n"));
    return false;
  }
  zone.m_parsed = true;

  listener->setFont(MWAWFont());
  int actPage = 1;
  int numCols = 1;
  bool isMain=entry.id()==0;

  if (isMain) {
    m_mainParser->newPage(actPage);
    std::vector<int> width;
    m_mainParser->getColumnInfo(0, numCols, width);
    if (numCols > 1) {
      if (listener->isSectionOpened())
        listener->closeSection();
      MWAWSection sec;
      sec.m_columns.resize(size_t(numCols));
      for (size_t c=0; c < size_t(numCols); c++) {
        sec.m_columns[c].m_width = double(width[c]);
        sec.m_columns[c].m_widthUnit = WPX_POINT;
      }
      listener->openSection(sec);
    }
  }

  long firstPos=0;
  size_t firstZ=0;
  if (!zone.getPosition(entry.begin(), firstPos, firstZ)) {
    MWAW_DEBUG_MSG(("MRWText::send: can not find the beginning of the zone\n"));
    return false;
  }
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(firstPos, WPX_SEEK_SET);

  long actChar = entry.begin();
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  for (size_t z = firstZ ; z < zone.m_infoList.size(); z++) {
    if (actChar >= entry.end())
      break;
    long pos = (firstPos >= 0) ? firstPos : zone.m_infoList[z].m_pos.begin();
    long endPos = zone.m_infoList[z].m_pos.end();
    if (endPos > pos+entry.end()-actChar)
      endPos = pos+entry.end()-actChar;
    input->seek(pos, WPX_SEEK_SET);
    firstPos = -1;

    libmwaw::DebugStream f;
    f << "Text[" << std::hex << actChar << std::dec << "]:";
    int tokenEndPos = 0;
    while (!input->atEOS()) {
      long actPos = input->tell();
      if (actPos >= endPos) {
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());

        pos = actPos;
        f.str("");
        f << "Text:";
        break;
      }
      if (zone.m_posRulerMap.find(actChar)!=zone.m_posRulerMap.end()) {
        int id = zone.m_posRulerMap.find(actChar)->second;
        f << "[P" << id << "]";
        MRWTextInternal::Paragraph para;
        if (zone.getRuler(id, para)) {
          if (entry.id()>=0 && para.m_cellWidth>0) {
            MRWTextInternal::Table table(zone);
            MWAWEntry tableEntry;
            tableEntry.setBegin(actChar);
            tableEntry.setEnd(entry.end());
            if (!findTableStructure(table, tableEntry)) {
              MWAW_DEBUG_MSG(("MRWText::send: can not find table data\n"));
            } else if (!sendTable(table) || actChar >= table.nextCharPos()) {
              MWAW_DEBUG_MSG(("MRWText::send: can not send a table data\n"));
            } else {
              ascFile.addPos(pos);
              ascFile.addNote(f.str().c_str());

              f.str("");
              f << "Text:";
              actChar = table.nextCharPos();
              if (actChar >= entry.end())
                break;
              if (!zone.getPosition(actChar, firstPos, firstZ)) {
                MWAW_DEBUG_MSG(("MRWText::send: can not find the data after a table\n"));
                actChar = entry.end();
                break;
              }
              pos=firstPos;
              if (z == firstZ) {
                input->seek(firstPos, WPX_SEEK_SET);
                firstPos = -1;
                continue;
              } else {
                z = firstZ-1;
                break;
              }
            }

            input->seek(actPos, WPX_SEEK_SET);
          }
          setProperty(para);
        }
      }
      if (zone.m_posFontMap.find(actChar)!=zone.m_posFontMap.end()) {
        int id = zone.m_posFontMap.find(actChar)->second;
        f << "[F" << id << "]";
        MRWTextInternal::Font font;
        if (zone.getFont(id, font)) {
          listener->setFont(font.m_font);
          if (font.m_tokenId > 0) {
            m_mainParser->sendToken(zone.m_id, font.m_tokenId, font.m_font);
            tokenEndPos = -2;
          }
        }
      }

      char c = (char) input->readULong(1);
      ++actChar;
      if (tokenEndPos) {
        if ((c=='[' && tokenEndPos==-2)||(c==']' && tokenEndPos==-1)) {
          tokenEndPos++;
          f << c;
          continue;
        }
        tokenEndPos++;
        MWAW_DEBUG_MSG(("MRWText::send: find unexpected char for a token\n"));
      }
      switch(c) {
      case 0x6: { // end of line
        static bool first = true;
        if (first) {
          MWAW_DEBUG_MSG(("MRWText::send: find some table: unimplemented\n"));
          first = false;
        }
        f << "#";
        listener->insertEOL();
        break;
      }
      case 0x7: // fixme end of cell
        f << "#";
        listener->insertChar(' ');
        break;
      case 0x9:
        listener->insertTab();
        break;
      case 0xa:
        listener->insertEOL(true);
        break;
      case 0xc:
        if (isMain)
          m_mainParser->newPage(++actPage);
        break;
      case 0xd:
        listener->insertEOL();
        break;
      case 0xe:
        if (numCols > 1) {
          listener->insertBreak(MWAWContentListener::ColumnBreak);
          break;
        }
        MWAW_DEBUG_MSG(("MRWText::sendText: Find unexpected column break\n"));
        f << "###";
        if (isMain)
          m_mainParser->newPage(++actPage);
        break;

        // some special character
      case 0x11:
        listener->insertUnicode(0x2318);
        break;
      case 0x12:
        listener->insertUnicode(0x2713);
        break;
      case 0x14:
        listener->insertUnicode(0xF8FF);
        break;
      case 0x1f: // soft hyphen, ignore
        break;
      default:
        actChar+=listener->insertCharacter((unsigned char) c, input, endPos);
        break;
      }

      f << c;
      if (c==0xa || c==0xd || actPos==endPos) {
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());

        pos = actPos;
        f.str("");
        f << "Text:";
      }
    }
  }

  return true;
}

////////////////////////////////////////////////////////////
// table function
bool MRWText::sendTable(MRWTextInternal::Table &table)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("MRWText::sendTable: can not find the listener\n"));
    return false;
  }
  size_t nRows=table.m_rowsList.size();
  if (nRows == 0) {
    MWAW_DEBUG_MSG(("MRWText::sendTable: can not find the number of row\n"));
    return false;
  }
  // fixme: create a single table
  for (size_t r = 0; r < nRows; r++) {
    MRWTextInternal::Table::Row &row=table.m_rowsList[r];
    size_t nCells=row.m_cellsList.size();
    if (nCells == 0) {
      MWAW_DEBUG_MSG(("MRWText::sendTable: can not find the number of cells\n"));
      continue;
    }
    std::vector<float> colWidths(nCells);
    for (size_t c=0; c < nCells; c++)
      colWidths[c]=(float)row.m_cellsList[c].m_width;
    listener->openTable(colWidths, WPX_POINT);
    listener->openTableRow(-float(row.m_height), WPX_POINT);

    WPXPropertyList extras;
    for (size_t c=0; c < nCells; c++) {
      MRWTextInternal::Table::Cell const &cell=row.m_cellsList[c];
      MWAWCell fCell;
      MRWTextInternal::Paragraph para;
      if (table.m_zone.getRuler(cell.m_rulerId, para))
        para.update(m_mainParser->getPatternPercent(para.m_cellFill.m_patternId), fCell);
      fCell.position() = Vec2i((int)c,0);

      listener->openTableCell(fCell, extras);
      MWAWEntry entry(cell.m_entry);
      if (entry.length()<=1)
        listener->insertChar(' ');
      else {
        entry.setLength(entry.length()-1);
        send(table.m_zone, entry);
      }

      listener->closeTableCell();
    }

    listener->closeTableRow();
    listener->closeTable();
  }
  return true;
}

bool MRWText::findTableStructure(MRWTextInternal::Table &table, MWAWEntry const &entry)
{
  MRWTextInternal::Zone const &zone = table.m_zone;
  long firstPos=0;
  size_t firstZ=0;
  if (!zone.getPosition(entry.begin(), firstPos, firstZ))
    return false;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(firstPos, WPX_SEEK_SET);

  int actHeight=0, lastHeight = 0;
  long actChar = entry.begin();
  MRWTextInternal::Table::Row row;

  MRWTextInternal::Table::Cell cell;
  cell.m_entry.setBegin(actChar);
  bool firstCellInRow=true;
  for (size_t z=firstZ ; z < zone.m_infoList.size(); z++) {
    if (actChar >= entry.end())
      break;

    long endPos = zone.m_infoList[z].m_pos.end();
    if (z!=firstZ)
      input->seek(zone.m_infoList[z].m_pos.begin(), WPX_SEEK_SET);

    while (!input->atEOS()) {
      long actPos = input->tell();
      if (actPos == endPos)
        break;
      if (zone.m_posRulerMap.find(actChar)!=zone.m_posRulerMap.end()) {
        int id = zone.m_posRulerMap.find(actChar)->second;
        MRWTextInternal::Paragraph para;
        if (zone.getRuler(id, para)) {
          if (para.m_cellWidth > 0) {
            cell.m_rulerId = id;
            cell.m_width = para.m_cellWidth;
            lastHeight = para.m_cellHeight;
            if (lastHeight < 0 || (actHeight >= 0 && lastHeight > actHeight))
              actHeight=lastHeight;
          } else if (firstCellInRow)
            return table.m_rowsList.size();
        }
      }

      firstCellInRow = false;
      char c = (char) input->readULong(1);
      actChar++;
      if (c == 0x6) {
        if (!row.m_cellsList.size())
          return false;
        row.m_lastChar = actChar;
        row.m_height = actHeight;
        table.m_rowsList.push_back(row);

        // reset to look for the next row
        row=MRWTextInternal::Table::Row();
        actHeight=lastHeight;
        cell.m_entry.setBegin(actChar);
        firstCellInRow = true;
      } else if (c==0x7) {
        cell.m_entry.setEnd(actChar);
        if (!cell.ok())
          return false;
        row.m_cellsList.push_back(cell);
        cell.m_entry.setBegin(actChar);
      }
    }
  }
  return table.m_rowsList.size();
}

////////////////////////////////////////////////////////////
// read the data
bool MRWText::readPLCZone(MRWEntry const &entry, int zoneId)
{
  if (entry.length() < 2*entry.m_N-1) {
    MWAW_DEBUG_MSG(("MRWText::readPLCZone: data seems to short\n"));
    return false;
  }

  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(entry.begin(), WPX_SEEK_SET);
  input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  m_mainParser->decodeZone(dataList,1+2*entry.m_N);
  input->popLimit();

  if (int(dataList.size()) != 2*entry.m_N) {
    MWAW_DEBUG_MSG(("MRWText::readPLCZone: find unexpected number of data\n"));
    return false;
  }

  MRWTextInternal::Zone &zone = m_state->getZone(zoneId);
  bool isCharZone = entry.m_fileType==4;
  std::map<long,int> &map = isCharZone ? zone.m_posFontMap : zone.m_posRulerMap;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  long pos = entry.begin();
  for (size_t d=0; d < dataList.size(); d+=2) {
    if (d%40==0) {
      if (d) {
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
      }
      f.str("");
      f << entry.name() << ":";
      pos = dataList[d].m_filePos;
    }
    long cPos = dataList[d].value(0);
    int id = (int) dataList[d+1].value(0);
    map[cPos] = id;
    f << std::hex << cPos << std::dec; //pos
    if (isCharZone)
      f << "(F" << id << "),"; // id
    else
      f << "(P" << id << "),"; // id
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
//     Fonts
////////////////////////////////////////////////////////////
bool MRWText::readFontNames(MRWEntry const &entry, int zoneId)
{
  if (entry.length() < entry.m_N) {
    MWAW_DEBUG_MSG(("MRWText::readFontNames: data seems to short\n"));
    return false;
  }

  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(entry.begin(), WPX_SEEK_SET);
  input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  m_mainParser->decodeZone(dataList,1+19*entry.m_N);
  input->popLimit();

  if (int(dataList.size()) != 19*entry.m_N) {
    MWAW_DEBUG_MSG(("MRWText::readFontNames: find unexpected number of data\n"));
    return false;
  }

  MRWTextInternal::Zone &zone = m_state->getZone(zoneId);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  size_t d = 0;
  int val;
  for (int i = 0; i < entry.m_N; i++) {
    f.str("");
    f << entry.name() << "-FN" << i << ":";
    ascFile.addPos(dataList[d].m_filePos);
    std::string fontName("");
    for (int j = 0; j < 2; j++, d++) {
      MRWStruct const &data = dataList[d];
      if (data.m_type!=0 || !data.m_pos.valid()) {
        MWAW_DEBUG_MSG(("MRWText::readFontNames: name %d seems bad\n", j));
        f << "###" << data << ",";
        continue;
      }
      long pos = data.m_pos.begin();
      input->seek(pos, WPX_SEEK_SET);
      int fSz = int(input->readULong(1));
      if (fSz+1 > data.m_pos.length()) {
        MWAW_DEBUG_MSG(("MRWText::readFontNames: field name %d seems bad\n", j));
        f << data << "[###fSz=" << fSz << ",";
        continue;
      }
      std::string name("");
      for (int c = 0; c < fSz; c++)
        name+=(char) input->readULong(1);
      if (j == 0) {
        fontName = name;
        f << name << ",";
      } else
        f << "nFont=" << name << ",";
    }
    val = (int) dataList[d++].value(0);
    if (val != 4) // always 4
      f << "f0=" << val << ",";
    val = (int) dataList[d++].value(0);
    if (val) // always 0
      f << "f1=" << val << ",";
    int fId = (int) (uint16_t) dataList[d++].value(0);
    f << "fId=" << fId << ",";
    int fIdAux = (int) (uint16_t) dataList[d++].value(0);
    if (fIdAux)
      f << "f2=" << std::hex << fIdAux << std::dec << ",";
    for (int j = 6; j < 19; j++) { // f14=1,f15=0|3
      MRWStruct const &data = dataList[d++];
      if (data.m_type==0 || data.numValues() > 1)
        f << "f" << j-3 << "=" << data << ",";
      else if (data.value(0))
        f << "f" << j-3 << "=" << data.value(0) << ",";
    }
    if (fontName.length()) {
      // checkme:
      std::string family = (fIdAux&0xFF00) ==0x4000 ? "Osaka" : "";
      m_parserState->m_fontConverter->setCorrespondance(fId, fontName, family);
    }
    zone.m_idFontMap[i] = fId;
    ascFile.addNote(f.str().c_str());
  }
  input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

bool MRWText::readFonts(MRWEntry const &entry, int zoneId)
{
  if (entry.length() < entry.m_N+1) {
    MWAW_DEBUG_MSG(("MRWText::readFonts: data seems to short\n"));
    return false;
  }

  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(entry.begin(), WPX_SEEK_SET);
  input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  m_mainParser->decodeZone(dataList,1+1+77*entry.m_N);
  input->popLimit();

  if (int(dataList.size()) != 1+77*entry.m_N) {
    MWAW_DEBUG_MSG(("MRWText::readFonts: find unexpected number of data\n"));
    return false;
  }

  MRWTextInternal::Zone &zone = m_state->getZone(zoneId);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << entry.name() << ":unkn=" << dataList[0].value(0);
  ascFile.addPos(dataList[0].m_filePos);
  ascFile.addNote(f.str().c_str());

  size_t d = 1;
  for (int i = 0; i < entry.m_N; i++) {
    MRWTextInternal::Font font;
    f.str("");
    ascFile.addPos(dataList[d].m_filePos);

    size_t fD = d;
    long val;
    uint32_t fFlags=0;
    for (int j = 0; j < 77; j++) {
      MRWStruct const &dt = dataList[d++];
      if (!dt.isBasic()) continue;
      unsigned char color[3];
      switch (j) {
      case 0:
        font.m_localId=(int) dt.value(0);
        break;
      case 1: // 0|1
      case 2: // 0
      case 3: // small number
      case 4: // 0|..|10
      case 5: // 0|1|2
      case 6: // 0
      case 8: // small number or 1001,1002
      case 17: // small number between 4 && 39
      case 19: // 0
      case 20: // 0|1|3|5
      case 21: // 0
      case 22: // 0
      case 30: // small number between 0 or 0x3a9 ( a dim?)
      case 66: // 0|1
      case 74: // 0|1|2|3
        if (dt.value(0))
          f << "f" << j << "=" << dt.value(0) << ",";
        break;
      case 7:  // 0, 1000, 100000, 102004
        if (dt.value(0))
          f << "fl0=" << std::hex << dt.value(0) << std::dec << ",";
        break;
      case 9:
      case 10:
      case 11:
        color[0]=color[1]=color[2]=0;
        color[j-9]=(unsigned char) (dt.value(0)>>8);
        while (j < 11)
          color[++j-9] = (unsigned char) (dataList[d++].value(0));
        font.m_font.setColor(MWAWColor(color[0],color[1],color[2]));
        break;
      case 12: // big number
      case 16: // 0 or one time 0x180
        if (dt.value(0))
          f << "f" << j << "=" << std::hex << dt.value(0) << std::dec << ",";
        break;
      case 13:
      case 14:
      case 15:
        color[0]=color[1]=color[2]=0xFF;
        color[j-13]=(unsigned char) (dt.value(0)>>8);
        while (j < 15)
          color[++j-13] = (unsigned char) (dataList[d++].value(0)>>8);
        font.m_font.setBackgroundColor(MWAWColor(color[0],color[1],color[2]));
        break;
      case 18: {
        val = dt.value(0);
        font.m_font.setSize(float(val)/65536.f);
        break;
      }
      case 23: // kern
        if (dt.value(0))
          font.m_font.setDeltaLetterSpacing(float(dt.value(0))/65536.f);
        break;
      case 24: // 0
      case 25: // 0
      case 27: // 0
      case 29: // 0
      case 32: // 0
      case 33: // 0
      case 35: // 0
        if (dt.value(0))
          f << "f" << j << "=" << dt.value(0) << ",";
        break;
      case 26: // 0|very big number id f28
      case 28: // 0|very big number
      case 31: // 0|very big number
      case 34: // an id?
        if (!dt.value(0))
          break;
        switch(j) {
        case 28:
          f << "idA";
          break;
        case 31:
          f << "idB";
          break;
        default:
          f << "f" << j;
          break;
        }
        f << "=" << std::hex << int32_t(dt.value(0)) << std::dec << ",";
        break;
        // tokens
      case 36: // 0|1|6 related to token?
        if (dt.value(0))
          f << "tok0=" << dt.value(0) << ",";
        break;
      case 37: // another id?
        if (dt.value(0))
          f << "tok1=" << std::hex << dt.value(0) << std::dec << ",";
        break;
      case 38:
        font.m_tokenId = dt.value(0);
        break;
      case 39:
      case 40:
      case 41:
      case 42:
      case 43:
      case 44:
      case 45:
      case 46:
      case 47:
      case 48:
      case 49:
      case 50:
      case 54:
      case 55:
      case 56:
      case 57:
      case 58:
        if (int16_t(dt.value(0))==-1) {
          switch(j) {
          case 39:
            fFlags |= MWAWFont::boldBit;
            break;
          case 40:
            fFlags |= MWAWFont::italicBit;
            break;
          case 41:
            font.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
            break;
          case 42:
            fFlags |= MWAWFont::outlineBit;
            break;
          case 43:
            fFlags |= MWAWFont::shadowBit;
            break;
          case 44:
            font.m_font.setDeltaLetterSpacing(-1);
            break;
          case 45:
            font.m_font.setDeltaLetterSpacing(1);
            break;
          case 46:
            font.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
            font.m_font.setUnderlineType(MWAWFont::Line::Double);
            break;
          case 47:
            font.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
            font.m_font.setUnderlineWordFlag(true);
            break;
          case 48:
            font.m_font.setUnderlineStyle(MWAWFont::Line::Dot);
            break;
          case 49:
            fFlags |= MWAWFont::hiddenBit;
            break;
          case 50:
            font.m_font.setStrikeOutStyle(MWAWFont::Line::Simple);
            break;
          case 54:
            fFlags |= MWAWFont::allCapsBit;
            break;
          case 55:
            fFlags |= MWAWFont::lowercaseBit;
            break;
          case 56:
            fFlags |= MWAWFont::smallCapsBit;
            break;
          case 57:
            font.m_font.setOverline(font.m_font.getUnderline());
            font.m_font.setUnderline(MWAWFont::Line());
            break;
          case 58:
            fFlags |= MWAWFont::boxedBit;
            break;
          default:
            MWAW_DEBUG_MSG(("MRWText::readFonts: find unknown font flag: %d\n", j));
            f << "#f" << j << ",";
            break;
          }
        } else if (dt.value(0))
          f << "#f" << j << "=" << dt.value(0) << ",";
        break;
      case 51:
        if (dt.value(0) > 0)
          font.m_font.set(MWAWFont::Script((float)dt.value(0),WPX_POINT));
        else if (dt.value(0))
          f << "#superscript=" << dt.value(0) << ",";
        break;
      case 52:
        if (dt.value(0) > 0)
          font.m_font.set(MWAWFont::Script((float)-dt.value(0),WPX_POINT));
        else if (dt.value(0))
          f << "#subscript=" << dt.value(0) << ",";
        break;
      case 59:
        if (dt.value(0))
          font.m_font.set(MWAWFont::Script(float(dt.value(0))/3.0f,WPX_POINT,58));
        break;
      default:
        if (dt.value(0))
          f << "#f" << j << "=" << dt.value(0) << ",";
        break;
      }
    }
    font.m_font.setFlags(fFlags);
    // the error
    d = fD;
    for (int j = 0; j < 77; j++, d++) {
      if (!dataList[d].isBasic())
        f << "#f" << j << "=" << dataList[d] << ",";
    }
    zone.m_fontList.push_back(font);

    font.m_extra = f.str();
    f.str("");
    f << entry.name() << "-F" << i << ":"
      << font.m_font.getDebugString(m_parserState->m_fontConverter) << font;
    ascFile.addNote(f.str().c_str());
  }
  input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
//     Style
////////////////////////////////////////////////////////////
bool MRWText::readStyleNames(MRWEntry const &entry, int)
{
  if (entry.length() < entry.m_N) {
    MWAW_DEBUG_MSG(("MRWText::readStyleNames: data seems to short\n"));
    return false;
  }

  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(entry.begin(), WPX_SEEK_SET);
  input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  m_mainParser->decodeZone(dataList,1+2*entry.m_N);
  input->popLimit();

  if (int(dataList.size()) != 2*entry.m_N) {
    MWAW_DEBUG_MSG(("MRWText::readStyleNames: find unexpected number of data\n"));
    return false;
  }

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  size_t d = 0;
  for (int i = 0; i < entry.m_N; i++) {
    f.str("");
    f << entry.name() << "-" << i << ":";
    ascFile.addPos(dataList[d].m_filePos);
    if (!dataList[d].isBasic()) {
      MWAW_DEBUG_MSG(("MRWText::readStyleNames: bad id field for style %d\n", i));
      f << "###" << dataList[d] << ",";
    } else
      f << "id=" << dataList[d].value(0) << ",";
    d++;

    std::string name("");
    MRWStruct const &data = dataList[d++];
    if (data.m_type!=0 || !data.m_pos.valid()) {
      MWAW_DEBUG_MSG(("MRWText::readStyleNames: name %d seems bad\n", i));
      f << "###" << data << ",";
    } else {
      long pos = data.m_pos.begin();
      input->seek(pos, WPX_SEEK_SET);
      int fSz = int(input->readULong(1));
      if (fSz+1 > data.m_pos.length()) {
        MWAW_DEBUG_MSG(("MRWText::readStyleNames: field name %d seems bad\n", i));
        f << data << "[###fSz=" << fSz << ",";
      } else {
        for (int c = 0; c < fSz; c++)
          name+=(char) input->readULong(1);
        f << name << ",";
      }
    }
    ascFile.addNote(f.str().c_str());
  }
  input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
//     Paragraph
////////////////////////////////////////////////////////////
void MRWText::setProperty(MRWTextInternal::Paragraph const &ruler)
{
  if (!m_parserState->m_listener) return;
  m_parserState->m_listener->setParagraph(ruler);
}

bool MRWText::readRulers(MRWEntry const &entry, int zoneId)
{
  if (entry.length() < entry.m_N+1) {
    MWAW_DEBUG_MSG(("MRWText::readRulers: data seems to short\n"));
    return false;
  }

  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(entry.begin(), WPX_SEEK_SET);
  input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  m_mainParser->decodeZone(dataList,1+3*68*entry.m_N);
  input->popLimit();

  int numDatas = int(dataList.size());
  if (numDatas < 68*entry.m_N) {
    MWAW_DEBUG_MSG(("MRWText::readRulers: find unexpected number of data\n"));
    return false;
  }

  MRWTextInternal::Zone &zone = m_state->getZone(zoneId);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  size_t d = 0;
  for (int i = 0; i < entry.m_N; i++) {
    MRWTextInternal::Paragraph para;
    ascFile.addPos(dataList[d].m_filePos);

    if (int(d+68) > numDatas) {
      MWAW_DEBUG_MSG(("MRWText::readRulers: ruler %d is too short\n", i));
      f << "###";
      ascFile.addNote(f.str().c_str());
      return true;
    }
    size_t fD = d;
    unsigned char color[3];
    f.str("");
    int nTabs = 0;
    MWAWFont fontItem(3);
    MWAWListLevel level;
    for (int j = 0; j < 58; j++, d++) {
      MRWStruct const &dt = dataList[d];
      if (!dt.isBasic()) continue;
      switch(j) {
      case 0:
        switch(dt.value(0)) {
        case 0:
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
        case 4:
          para.m_justify = MWAWParagraph::JustificationFullAllLines;
          break;
        default:
          f << "#justify=" << dt.value(0) << ",";
        }
        break;
      case 3:
      case 4:
        para.m_margins[j-2] = float(dt.value(0))/72.f;
        break;
      case 5:
        para.m_margins[0] = float(dt.value(0))/72.f;
        break;
      case 6:
        if (!dt.value(0)) break;
        para.setInterline(double(dt.value(0))/65536., WPX_PERCENT);
        break;
      case 8:
        if (!dt.value(0)) break;
        para.setInterline(double(dt.value(0))/65536., WPX_POINT);
        break;
      case 9:
        if (!dt.value(0)) break;
        if (dt.value(0)<0) {
          MWAW_DEBUG_MSG(("MRWText::readRulers: find negative interline\n"));
          f << "#inteline=" << dt.value(0) << "[at least],";
        } else
          para.setInterline(double(dt.value(0)), WPX_POINT, MWAWParagraph::AtLeast);
        break;
      case 10:
        if (!dt.value(0)) break;
        para.m_spacings[1] = float(dt.value(0))/72.f;
        break;
      case 11:
        if (!dt.value(0)) break;
        para.m_spacings[2] = float(dt.value(0))/72.f;
        break;
      case 1: // always 0
      case 2: // small number between -15 and 15
      case 7: // always 0
        if (dt.value(0))
          f << "f" << j << "=" << dt.value(0) << ",";
        break;
      case 23: // big number related to fonts f28?
      case 24: // big number related to fonts f31?
        if (dt.value(0))
          f << "id" << char('A'+(j-23)) << "=" << std::hex << int32_t(dt.value(0)) << std::dec << ",";
        break;
      case 22: // find 1|2|4
        if (dt.value(0) != 1)
          f << "#f22=" << dt.value(0) << ",";
        break;
        //item properties
      case 27: {
        if (!dt.value(0))
          break;
        int fId = int(uint32_t(dt.value(0)>>16));
        if (fId) fontItem.setId(fId);
        int fSz = int(uint32_t(dt.value(0)&0xFFFF));
        if (fSz) fontItem.setSize(float(uint32_t(dt.value(0)&0xFFFF)));
        f << "itemFont=[id=" << fId << ", sz=" << fSz << "],";
        break;
      }
      case 28: {
        if (!dt.value(0))
          break;
        uint32_t strNum=(uint32_t)dt.value(0);
        unsigned char str[4];
        for (int depl=24, k=0; k < 4; k++, depl-=8)
          str[k]=(unsigned char)(strNum>>depl);
        unsigned char const *strPtr=str;
        int sz=int(*strPtr++);
        if (sz<=0 || sz>3 || !str[1]) {
          MWAW_DEBUG_MSG(("MRWText::readRulers: can not read bullet\n"));
          f << "#bullet=" << std::hex << strNum << std::dec << ",";
          break;
        }
        unsigned char c=*strPtr++;
        int unicode = m_parserState->m_fontConverter->unicode(3, c, strPtr, sz-1);
        if (unicode==-1)
          libmwaw::appendUnicode(c, level.m_bullet);
        else
          libmwaw::appendUnicode(uint32_t(unicode), level.m_bullet);
        break;
      }
      case 29: // 0|5|6|30|64
        if (!dt.value(0))
          break;
        switch(dt.value(0)&3) {
        case 0:
          break;
        case 1:
          level.m_alignment=MWAWListLevel::CENTER;
          break;
        case 2:
          level.m_alignment=MWAWListLevel::RIGHT;
          break;
        default:
          f << "#align[item]=3,";
          break;
        }
        if (dt.value(0)>>2)
          f << "f29[high]=" << std::hex << (int32_t(dt.value(0))>>2) << std::dec << ",";
        break;
      case 30:
        level.m_labelWidth=double(dt.value(0))/72.0;
        break;
        // cell properties
      case 40:
        para.m_cellWidth = (int) dt.value(0);
        break;
      case 41:
        para.m_cellHeight = (int) dt.value(0);
        break;
      case 42:
        para.m_cellSep = (int) dt.value(0);
        break;
        // border fill properties:
      case 19:
        para.m_paraFill.m_patternId= (int) dt.value(0);
        break;
      case 55:
        para.m_cellFill.m_patternId= (int) dt.value(0);
        break;
      case 15:
      case 16:
      case 17: // border para color
      case 31:
      case 32:
      case 33: // foreground para color
      case 35:
      case 36:
      case 37: // background para color
      case 43:
      case 44:
      case 45: // table border color
      case 47:
      case 48:
      case 49: // table background color
      case 51:
      case 52:
      case 53: { // table foreground color
        int debInd=-1;
        unsigned char defValue=0;
        if (j>=15&&j<=17)
          debInd=15;
        else if (j>=31&&j<=33)
          debInd=31;
        else if (j>=35&&j<=37) {
          debInd=35;
          defValue=0xFF;
        } else if (j>=43&&j<=45)
          debInd=43;
        else if (j>=47&&j<=49) {
          debInd=47;
          defValue=0xFF;
        } else if (j>=51&&j<=53) // table foreground color
          debInd=51;
        else {
          MWAW_DEBUG_MSG(("MRWText::readRulers: find unknown color idx\n"));
          f << "#col[debIndex=" << j << ",";
          break;
        }

        color[0]=color[1]=color[2]=defValue;
        color[j-debInd]=(unsigned char) (dt.value(0)>>8);
        while (j < debInd+2)
          color[++j-debInd] = (unsigned char) (dataList[++d].value(0)>>8);
        MWAWColor col(color[0],color[1],color[2]);
        switch(debInd) {
        case 15:
          para.m_paraFill.m_borderColor=col;
          break;
        case 31:
          para.m_paraFill.m_foreColor=col;
          break;
        case 35:
          para.m_paraFill.m_backColor=col;
          break;
        case 43:
          para.m_cellFill.m_borderColor=col;
          break;
        case 47:
          para.m_cellFill.m_backColor=col;
          break;
        case 51:
          para.m_cellFill.m_foreColor=col;
          break;
        default:
          MWAW_DEBUG_MSG(("MRWText::readRulers: find unknown color idx\n"));
          f << "#col[debIndex]=" << col << ",";
          break;
        }
        break;
      }
      case 14: // para border
      case 56: { // cell border
        MRWTextInternal::Paragraph::BorderFill &fill=
          (j==14) ? para.m_paraFill : para.m_cellFill;
        long val = dt.value(0);
        for (int b = 0, depl=0; b < 4; b++,depl+=8)
          fill.m_borderTypes[b] = int((val>>depl)&0xFF);
        break;
      }
      case 57:
        nTabs = (int) dt.value(0);
        break;
      default:
        if (dt.value(0))
          f << "#f" << j << "=" << dt.value(0) << ",";
        break;
      }
    }
    d = fD;
    for (int j = 0; j < 58; j++, d++) {
      if (!dataList[d].isBasic())
        f << "#f" << j << "=" << dataList[d].isBasic() << ",";
    }
    if (nTabs < 0 || int(d)+4*nTabs+10 > numDatas) {
      MWAW_DEBUG_MSG(("MRWText::readRulers: can not read numtabs\n"));
      para.m_extra = f.str();
      f.str("");
      f << entry.name() << "-P" << i << ":" << para << "," << "###";
      ascFile.addNote(f.str().c_str());
      zone.m_rulerList.push_back(para);
      return true;
    }
    if (nTabs) {
      for (int j = 0; j < nTabs; j++) {
        MWAWTabStop tab;
        for (int k = 0; k < 4; k++, d++) {
          MRWStruct const &dt = dataList[d];
          if (!dt.isBasic()) {
            f << "#tabs" << j << "[" << k << "]=" << dt << ",";
            continue;
          }
          switch(k) {
          case 0:
            switch(dt.value(0)) {
            case 1:
              break;
            case 2:
              tab.m_alignment = MWAWTabStop::CENTER;
              break;
            case 3:
              tab.m_alignment = MWAWTabStop::RIGHT;
              break;
            case 4:
              tab.m_alignment = MWAWTabStop::DECIMAL;
              break;
            default:
              f << "#tabAlign" << j << "=" << dt.value(0) << ",";
              break;
            }
            break;
          case 1:
            tab.m_position = float(dt.value(0))/72.f;
            break;
          case 2:
            if (dt.value(0)) {
              int unicode= m_parserState->m_fontConverter->unicode(3, (unsigned char) dt.value(0));
              if (unicode==-1)
                tab.m_leaderCharacter =uint16_t(dt.value(0));
              else
                tab.m_leaderCharacter = uint16_t(unicode);
            }
            break;
          default:
            if (dt.value(0))
              f << "#tabs" << j << "[" << 3 << "]=" << dt.value(0) << ",";
            break;
          }
        }
        para.m_tabs->push_back(tab);
      }
    }
    for (int j = 0; j < 10; j++, d++) {
      MRWStruct const &dt = dataList[d];
      if (j==1 && dt.m_type==0) { // a block with sz=40
        input->seek(dt.m_pos.begin(), WPX_SEEK_SET);
        int fSz = (int) input->readULong(1);
        if (fSz+1>dt.m_pos.length()) {
          MWAW_DEBUG_MSG(("MRWText::readRulers: can not read paragraph name\n"));
          f << "#name,";
          continue;
        }
        if (fSz == 0) continue;
        std::string name("");
        for (int k = 0; k < fSz; k++)
          name+=(char) input->readULong(1);
        f << "name=" << name << ",";
        continue;
      }
      if (!dt.isBasic() || j==1) {
        f << "#g" << j << "=" << dt << ",";
        continue;
      }
      switch(j) {
      case 0: // always 36 ?
        if (dt.value(0)!=36)
          f << "#g0=" << dt.value(0) << ",";
        break;
      case 4: { // find also flag&0x80
        long val = dt.value(0);
        if (val & 0x1000) {
          val &= 0xFFFFEFFF;

          level.m_type = MWAWListLevel::BULLET;
          if (!level.m_bullet.len())
            libmwaw::appendUnicode(0x2022, level.m_bullet);
          shared_ptr<MWAWList> list;
          list = m_parserState->m_listManager->getNewList(list, 1, level);
          if (!list) {
            MWAW_DEBUG_MSG(("MRWText::readRulers: can create a listn for bullet\n"));
            f << "###";
            break;
          }

          para.m_listLevelIndex = 1;
          para.m_listId=list->getId();

          // we must update the margins:
          para.m_margins[1]=*para.m_margins[0]+*para.m_margins[1];
          para.m_margins[0]=0;
        }
        if (val&0x4000) {
          val &= 0xFFFFDFFF;
          f << "hasBackborder,";
        } else
          para.m_paraFill.resetBorders();
        if (val&0x4000) {
          val &= 0xFFFFBFFF;
          f << "hasBackground,";
        } else
          para.m_paraFill.resetBackgroundColor();
        if (val)
          f << "flag?=" << std::hex << val << std::dec << ",";
        break;
      }
      case 7: // 1|3|de
      case 8: // 0|4 : 4 for header entry?
        if (dt.value(0))
          f << "g" << j << "=" << dt.value(0) << ",";
        break;
      default:
        if (dt.value(0))
          f << "#g" << j << "=" << dt.value(0) << ",";
        break;
      }
    }
    para.m_extra = f.str();
    para.update(m_mainParser->getPatternPercent(para.m_paraFill.m_patternId));
    zone.m_rulerList.push_back(para);
    f.str("");
    f << entry.name() << "-P" << i << ":" << para << ",";
    ascFile.addNote(f.str().c_str());
  }
  input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
//     the token
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
//     the sections
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

//! send data to the listener

void MRWText::flushExtra()
{
  if (!m_parserState->m_listener) return;
#ifdef DEBUG
  std::map<int,MRWTextInternal::Zone>::iterator it =
    m_state->m_textZoneMap.begin();
  for ( ; it != m_state->m_textZoneMap.end(); it++) {
    if (it->second.m_parsed)
      continue;
    send(it->first);
  }
#endif
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
