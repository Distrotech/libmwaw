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

#include "MWAWEntry.hxx"
#include "MWAWFontConverter.hxx"

#include "CWParser.hxx"
#include "CWText.hxx"

#include "CWStyleManager.hxx"

/** Internal: the structures of a CWStyleManagerInternal */
namespace CWStyleManagerInternal
{
struct State {
  //! constructor
  State() : m_version(-1), m_localFIdMap(), m_stylesMap(), m_lookupMap(), m_graphList(), m_ksenList() {
  }
  //! return a mac font id corresponding to a local id
  int getFontId(int localId) const {
    if (m_localFIdMap.find(localId)==m_localFIdMap.end())
      return localId;
    return m_localFIdMap.find(localId)->second;
  }

  //! the version
  int m_version;
  //! a map local fontId->fontId
  std::map<int, int> m_localFIdMap;
  //! the styles map id->style
  std::map<int, CWStyleManager::Style> m_stylesMap;
  //! the style lookupMap
  std::map<int, int> m_lookupMap;
  //! the Graphic list
  std::vector<CWStyleManager::Graphic> m_graphList;
  //! the KSEN list
  std::vector<CWStyleManager::KSEN> m_ksenList;
};
}

////////////////////////////////////////////////////
// Graphic function
////////////////////////////////////////////////////
std::ostream &operator<<(std::ostream &o, CWStyleManager::Graphic const &graph)
{
  if (graph.m_lineWidth && graph.m_lineWidth != 1)
    o << "lineW=" << graph.m_lineWidth << ",";
  if (!graph.m_color[0].isBlack())
    o << "lineColor=" << graph.m_color[0] << ",";
  if (!graph.m_color[1].isWhite())
    o << "surfColor=" << graph.m_color[1] << ",";
  if (graph.m_pattern[0] != -1 && graph.m_pattern[0] != 2)
    o << "linePattern=" << graph.m_pattern[0] << "[" << graph.m_patternPercent[0] << "],";
  if (graph.m_pattern[1] != -1 && graph.m_pattern[1] != 2)
    o << "surfPattern=" << graph.m_pattern[1] << "[" << graph.m_patternPercent[1] << "],";
  o << graph.m_extra;
  return o;
}

MWAWColor CWStyleManager::Graphic::getLineColor() const
{
  if (m_patternPercent[0] >= 1.0 || m_patternPercent[0] < 0)
    return m_color[0];
  return MWAWColor::barycenter(m_patternPercent[0], m_color[0], 1.f-m_patternPercent[0], MWAWColor::white());
}

MWAWColor CWStyleManager::Graphic::getSurfaceColor() const
{
  if (m_patternPercent[1] >= 1.0 || m_patternPercent[1] < 0)
    return m_color[1];
  return MWAWColor::barycenter(m_patternPercent[1], m_color[1], 1.f-m_patternPercent[1], MWAWColor::white());
}

////////////////////////////////////////////////////
// KSEN function
////////////////////////////////////////////////////
std::ostream &operator<<(std::ostream &o, CWStyleManager::KSEN const &ksen)
{
  switch(ksen.m_valign) {
  case 0:
    break;
  case 1:
    o<<"yCenter,";
    break;
  case 2:
    o<<"yBottom,";
    break;
  default:
    o << "valign=#" << ksen.m_valign << ",";
    break;
  }
  switch(ksen.m_lineType) {
  case MWAWBorder::None:
    o << "lType=none,";
    break;
  case MWAWBorder::Single:
    break;
  case MWAWBorder::Double:
    o << "double,";
    break;
  case MWAWBorder::Dot:
    o << "dotted,";
    break;
  case MWAWBorder::LargeDot:
    o << "dotted[large],";
    break;
  case MWAWBorder::Dash:
    o << "dash,";
    break;
  default:
    o << "lType=#" << int(ksen.m_lineType) << ",";
    break;
  }
  switch(ksen.m_lines) {
  case 0:
    break;
  case 1:
    o << "lines=LT<->RB,";
    break;
  case 2:
    o << "lines=LB<->RT,";
    break;
  case 3:
    o << "cross,";
    break;
  default:
    o << "lines=#" << ksen.m_lines << ",";
    break;
  }
  o << ksen.m_extra;
  return o;
}

////////////////////////////////////////////////////
// Style function
////////////////////////////////////////////////////
std::ostream &operator<<(std::ostream &o, CWStyleManager::Style const &style)
{
  if (style.m_styleId != -1) {
    o << "styleId=[" << style.m_styleId ;
    if (style.m_localStyleId != -1 && style.m_localStyleId != style.m_styleId)
      o << ",lId=" << style.m_localStyleId;
    o << "],";
  }
  if (style.m_fontId != -1)
    o << "font=[" << style.m_fontId << ",hash=" << style.m_fontHash << "],";
  if (style.m_rulerId != -1)
    o << "ruler=[" << style.m_rulerId << ",hash=" << style.m_rulerHash << "],";
  if (style.m_ksenId != -1)
    o << "ksenId=" << style.m_ksenId << ",";
  if (style.m_graphicId != -1)
    o << "graphicId=" << style.m_graphicId << ",";
  o << style.m_extra;
  return o;
}

////////////////////////////////////////////////////
// StyleManager function
////////////////////////////////////////////////////
CWStyleManager::CWStyleManager(MWAWInputStreamPtr ip, CWParser &parser, MWAWFontConverterPtr &convert) :
  m_input(ip), m_convertissor(convert), m_mainParser(&parser), m_state(), m_asciiFile(parser.ascii())
{
  m_state.reset(new CWStyleManagerInternal::State);
}

CWStyleManager::~CWStyleManager()
{
}

int CWStyleManager::version() const
{
  if (m_state->m_version <= 0) m_state->m_version = m_mainParser->version();
  return m_state->m_version;
}

// accessor
int CWStyleManager::getFontId(int localId) const
{
  return m_state->getFontId(localId);
}

bool CWStyleManager::get(int styleId, CWStyleManager::Style &style) const
{
  style = Style();
  if (version() <= 2 || m_state->m_lookupMap.find(styleId) == m_state->m_lookupMap.end())
    return false;
  int id = m_state->m_lookupMap.find(styleId)->second;
  if (id < 0 ||m_state-> m_stylesMap.find(id) ==m_state-> m_stylesMap.end())
    return false;
  style =m_state-> m_stylesMap.find(id)->second;
  return true;
}

bool CWStyleManager::get(int ksenId, CWStyleManager::KSEN &ksen) const
{
  ksen = KSEN();
  if (ksenId < 0) return false;
  if (ksenId >= int(m_state->m_ksenList.size())) {
    MWAW_DEBUG_MSG(("CWStyleManager::get: can not find ksen %d\n", ksenId));
    return false;
  }
  ksen = m_state->m_ksenList[size_t(ksenId)];
  return true;
}

bool CWStyleManager::get(int graphId, CWStyleManager::Graphic &graph) const
{
  graph = Graphic();
  if (graphId < 0) return false;
  if (graphId >= int(m_state->m_graphList.size())) {
    MWAW_DEBUG_MSG(("CWStyleManager::get: can not find graph %d\n", graphId));
    return false;
  }
  graph = m_state->m_graphList[size_t(graphId)];
  return true;
}

////////////////////////////////////////////////////////////
// read file main structure
////////////////////////////////////////////////////////////
bool CWStyleManager::readStyles(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.type() != "STYL")
    return false;
  long pos = entry.begin();
  m_input->seek(pos+4, WPX_SEEK_SET); // skip header
  long sz = (long) m_input->readULong(4);
  if (sz > entry.length()) {
    MWAW_DEBUG_MSG(("CWStyleManager::readStyles: pb with entry length"));
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  libmwaw::DebugStream f;
  f << "Entries(STYL):";
  if (version() <= 3) {
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(entry.end(), WPX_SEEK_SET);
    return true;
  }
  bool limitSet = true;
  if (version() <= 4) {
    // version 4 does not contents total length fields
    m_input->seek(-4, WPX_SEEK_CUR);
    limitSet = false;
  } else
    m_input->pushLimit(entry.end());
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  int id = 0;
  while (long(m_input->tell()) < entry.end()) {
    pos = m_input->tell();
    if (!readGenStyle(id)) {
      m_input->seek(pos, WPX_SEEK_SET);
      if (limitSet) m_input->popLimit();
      return false;
    }
    id++;
  }
  if (limitSet) m_input->popLimit();

  return true;
}

bool CWStyleManager::readGenStyle(int id)
{
  long pos = m_input->tell();
  long sz = (long) m_input->readULong(4);
  long endPos = pos+4+sz;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("CWStyleManager::readGenStyle: pb with sub zone: %d", id));
    return false;
  }
  m_input->seek(pos+4, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "STYL-" << id << ":";
  if (sz < 16) {
    if (sz) f << "#";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(endPos, WPX_SEEK_SET);
    return true;
  }

  std::string name("");
  int N = (int) m_input->readLong(2);
  int type = (int) m_input->readLong(2);
  int val =  (int) m_input->readLong(2);
  int fSz =  (int) m_input->readLong(2);
  f << "N=" << N << ", type?=" << type <<", fSz=" << fSz << ",";
  if (val) f << "unkn=" << val << ",";

  for (int i = 0; i < 2; i++) {
    val = (int) m_input->readLong(2);
    if (val)  f << "f" << i << "=" << val << ",";
  }
  for (int i = 0; i < 4; i++)
    name += char(m_input->readULong(1));
  f << name;

  long actPos = m_input->tell();
  if (actPos != pos && actPos != endPos - N*fSz)
    ascii().addDelimiter(m_input->tell(), '|');

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  long numRemain = endPos - actPos;
  if (N > 0 && fSz > 0 && numRemain >= N*fSz) {
    m_input->seek(endPos-N*fSz, WPX_SEEK_SET);

    bool ok = false;
    if (name == "CHAR")
      ok = m_mainParser->m_textParser->readSTYL_CHAR(N, fSz);
    else if (name == "CELL")
      ok = readCellStyles(N, fSz);
    else if (name == "FNTM")
      ok = readFontNames(N, fSz);
    else if (name == "GRPH")
      ok = readGraphStyles(N, fSz);
    else if (name == "KSEN")
      ok = readKSEN(N, fSz);
    else if (name == "LKUP")
      ok = readLookUp(N, fSz);
    else if (name == "NAME")
      ok = readStyleNames(N, fSz);
    else if (name == "RULR")
      ok = m_mainParser->m_textParser->readSTYL_RULR(N, fSz);
    else if (name == "STYL")
      ok = readStylesDef(N, fSz);
    if (!ok) {
      m_input->seek(endPos-N*fSz, WPX_SEEK_SET);
      for (int i = 0; i < N; i++) {
        pos = m_input->tell();
        f.str("");
        f << "STYL-" << id << "/" << name << "-" << i << ":";
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());
        m_input->seek(fSz, WPX_SEEK_CUR);
      }
    }
  }

  m_input->seek(endPos, WPX_SEEK_SET);
  return true;
}

bool CWStyleManager::readStylesDef(int N, int fSz)
{
  m_state->m_stylesMap.clear();
  if (fSz == 0 || N== 0)
    return true;
  if (fSz < 28) {
    MWAW_DEBUG_MSG(("CWStyleManager::readStylesDef: Find old data size %d\n", fSz));
    return false;
  }
  libmwaw::DebugStream f;
  int val;

  for (int i = 0; i < N; i++) {
    long pos = m_input->tell();
    Style style;
    f.str("");
    val = (int) m_input->readLong(2);
    if (val != -1) f << "f0=" << val << ",";
    val = (int) m_input->readLong(2);
    if (val) f << "f1=" << val << ",";
    f << "used?=" << m_input->readLong(2) << ",";
    style.m_localStyleId = (int) m_input->readLong(2);
    if (i != style.m_localStyleId && style.m_localStyleId != -1) f << "#styleId,";
    style.m_styleId = (int) m_input->readLong(2);
    for (int j = 0; j < 2; j++) {
      // unknown : hash, dataId ?
      f << "g" << j << "=" << m_input->readLong(1) << ",";
    }
    for (int j = 2; j < 4; j++)
      f << "g" << j << "=" << m_input->readLong(2) << ",";
    int lookupId2 = (int) m_input->readLong(2);
    f << "lookupId2=" << lookupId2 << ",";
    style.m_fontId = (int) m_input->readLong(2);
    style.m_fontHash = (int) m_input->readLong(2);
    style.m_graphicId = (int) m_input->readLong(2);
    style.m_rulerId = (int) m_input->readLong(2);
    if (fSz >= 30)
      style.m_ksenId = (int) m_input->readLong(2);
    style.m_rulerHash = (int) m_input->readLong(2);
    style.m_extra = f.str();
    if (m_state->m_stylesMap.find(i)==m_state->m_stylesMap.end())
      m_state->m_stylesMap[i] = style;
    else {
      MWAW_DEBUG_MSG(("CWStyleManager::readStylesDef: style %d already exists\n", i));
    }
    if (long(m_input->tell()) != pos+fSz)
      ascii().addDelimiter(m_input->tell(), '|');

    f.str("");
    if (!i)
      f << "Entries(Style)-0:" << style;
    else
      f << "Style-" << i << ":" << style;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+fSz, WPX_SEEK_SET);
  }
  return true;
}

bool CWStyleManager::readLookUp(int N, int fSz)
{
  m_state->m_lookupMap.clear();

  if (fSz == 0 || N== 0) return true;
  libmwaw::DebugStream f;

  for (int i = 0; i < N; i++) {
    long pos = m_input->tell();
    f.str("");
    if (i == 0) f << "Entries(StylLookUp): StylLookUp-0:";
    else f << "StylLookUp-" << i << ":";
    int val = (int) m_input->readLong(2);
    if (m_state->m_stylesMap.find(val)!=m_state->m_stylesMap.end() &&
        m_state->m_stylesMap.find(val)->second.m_localStyleId != val &&
        m_state->m_stylesMap.find(val)->second.m_localStyleId != -1) {
      MWAW_DEBUG_MSG(("CWStyleManager::readLookUp: find some incoherence between style and lookup\n"));
      f << "##";
    }
    m_state->m_lookupMap[i] = val;
    f << "styleId=" << val;
    if (fSz != 2) {
      ascii().addDelimiter(m_input->tell(), '|');
      m_input->seek(pos+fSz, WPX_SEEK_SET);
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  return true;
}

////////////////////////////////////////////////////////////
// small structure
////////////////////////////////////////////////////////////
bool CWStyleManager::readFontNames(int N, int fSz)
{
  if (fSz == 0 || N== 0) return true;
  if (fSz < 16) return false;

  libmwaw::DebugStream f;
  for (int i = 0; i < N; i++) {
    long pos = m_input->tell();
    f.str("");
    if (i == 0) f << "Entries(FntNames): FntNames-0:";
    else f << "FntNames-" << i << ":";
    int fontEncoding = (int) m_input->readULong(2);
    f << "nameEncoding=" << fontEncoding << ",";
    f << "type?=" << m_input->readLong(2) << ",";

    int nChar = (int) m_input->readULong(1);
    if (5+nChar > fSz) {
      static bool first = true;
      if (first) {
        MWAW_DEBUG_MSG(("CWStyleManager::readFontNames: pb with name field %d", i));
        first = false;
      }
      f << "#";
    } else {
      std::string name("");
      bool ok = true;
      for (int c = 0; c < nChar; c++) {
        char ch = (char) m_input->readULong(1);
        if (ch == '\0') {
          MWAW_DEBUG_MSG(("CWStyleManager::readFontNames: pb with name field %d\n", i));
          ok = false;
          break;
        } else if (ch & 0x80) {
          static bool first = true;
          if (first) {
            MWAW_DEBUG_MSG(("CWStyleManager::readFontNames: find odd font\n"));
            first = false;
          }
          if (fontEncoding!=0x4000)
            ok = false;
        }
        name += ch;
      }
      f << "'" << name << "'";
      if (name.length() && ok) {
        std::string family = fontEncoding==0x4000 ? "Osaka" : "";
        m_state->m_localFIdMap[i]=m_convertissor->getId(name, family);
      }
    }
    if (long(m_input->tell()) != pos+fSz) {
      ascii().addDelimiter(m_input->tell(), '|');
      m_input->seek(pos+fSz, WPX_SEEK_SET);
    }

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool CWStyleManager::readStyleNames(int N, int fSz)
{
  if (fSz == 0 || N== 0) return true;
  libmwaw::DebugStream f;
  for (int i = 0; i < N; i++) {
    long pos = m_input->tell();
    f.str("");
    if (i == 0) f << "Entries(StylName): StylName-0:";
    else f << "StylName-" << i << ":";
    f << "id=" << m_input->readLong(2) << ",";
    if (fSz > 4) {
      int nChar = (int) m_input->readULong(1);
      if (3+nChar > fSz) {
        static bool first = true;
        if (first) {
          MWAW_DEBUG_MSG(("CWStyleManager::readStyleNames: pb with name field %d", i));
          first = false;
        }
        f << "#";
      } else {
        std::string name("");
        for (int c = 0; c < nChar; c++)
          name += char(m_input->readULong(1));
        f << "'" << name << "'";
      }
    }
    if (long(m_input->tell()) != pos+fSz) {
      ascii().addDelimiter(m_input->tell(), '|');
      m_input->seek(pos+fSz, WPX_SEEK_SET);
    }

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool CWStyleManager::readCellStyles(int N, int fSz)
{
  if (fSz == 0 || N== 0) return true;
  if (fSz < 18) {
    MWAW_DEBUG_MSG(("CWStyleManager::readCellStyles: Find old ruler size %d\n", fSz));
    return false;
  }
  libmwaw::DebugStream f;
  int val;
  for (int i = 0; i < N; i++) {
    long pos = m_input->tell();
    f.str("");
    if (!i)
      f << "Entries(CellStyle)-0:";
    else
      f << "CellStyle-" << i << ":";
    // 3 int, id or color ?
    for (int j = 0; j < 3; j++) {
      val = (int) m_input->readLong(2);
      if (val == -1) continue;
      f << "f" << j << "=" << val << ",";
    }
    /* g0=0|4|8|c|1f, g1:number which frequently with 8|c|d
       g2=0|4|c|13|17|1f, g3:number which frequently with 8|c|d
       g4=0|1|8, g5=0|2, g6=0-3, g7=0-f,
     */
    for (int j = 0; j < 8; j++) {
      val = (int) m_input->readULong(1);
      if (val)
        f << "g" << j << "=" << std::hex << val << std::dec << ",";
    }
    // h0=h1=0, h2=h3=0|1
    for (int j = 0; j < 4; j++) {
      val = (int) m_input->readULong(1);
      if (val)
        f << "h" << j << "=" << val << ",";
    }
    if (long(m_input->tell()) != pos+fSz)
      ascii().addDelimiter(m_input->tell(), '|');
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+fSz, WPX_SEEK_SET);
  }
  return true;
}

bool CWStyleManager::readGraphStyles(int N, int fSz)
{
  if (fSz == 0 || N== 0) return true;
  int const vers = version();
  if ((vers <= 4 && fSz < 24) || (vers >= 5 && fSz < 28)) {
    MWAW_DEBUG_MSG(("CWStyleManager::readGraphStyles: Find old ruler size %d\n", fSz));
    return false;
  }
  libmwaw::DebugStream f;
  int val;
  std::vector<int16_t> values16; // some values can be small or little endian, so...
  std::vector<int32_t> values32;
  for (int i = 0; i < N; i++) {
    long pos = m_input->tell();
    f.str("");
    Graphic graph;
    // 3 int, id (find either f0=<small number> or f1=0, f2=small number
    for (int j = 0; j < 3; j++) {
      val = (int) m_input->readLong(2);
      if (val == -1) continue;
      f << "f" << j << "=" << val << ",";
    }

    values16.resize(0);
    values32.resize(0);
    // 2 two dim
    for (int j = 0; j < 2; j++)
      values16.push_back((int16_t)m_input->readLong(2));
    graph.m_lineWidth=(int) m_input->readULong(1);
    val = (int) m_input->readULong(1); // 0|1|4|80
    if (val)
      f << "f3=" << std::hex << val << std::dec << ",";
    for (int j = 0; j < 2; j++) {
      int col = (int) m_input->readULong(1);
      MWAWColor color;
      if (m_mainParser->getColor(col, color))
        graph.m_color[j] = color;
      else
        f << "#col" << j << "=" << col << ",";
    }
    for (int j = 0; j < 3; j++)
      values16.push_back((int16_t)m_input->readLong(2));

    m_mainParser->checkOrdering(values16, values32);
    if (values16[0] || values16[1])
      f << "dim=" << values16[0] << "x" << values16[1] << ",";
    graph.m_pattern[0] = values16[2];
    graph.m_pattern[1] = values16[3];
    for (int j = 0; j < 2; j++) {
      graph.m_patternPercent[j] = m_mainParser->getPatternPercent(graph.m_pattern[j]);
      if (graph.m_patternPercent[j] < 0) {
        f << "#pId" << j << ",";
        graph.m_patternPercent[j] = 1.0;
      }
    }
    if (values16[4])
      f << "g0=" << values16[4] << ",";

    val = (int) m_input->readULong(1); // 0|1|2
    if (val) f << "g1=" << val << ",";
    val = (int) m_input->readULong(2); // 0|1
    if (val) f << "g2=" << val << ",";

    graph.m_extra = f.str();
    m_state->m_graphList.push_back(graph);
    f.str("");
    if (!i)
      f << "Entries(GrphStyle)-0:" << graph;
    else
      f << "GrphStyle-" << i << ":" << graph;
    if (long(m_input->tell()) != pos+fSz)
      ascii().addDelimiter(m_input->tell(), '|');
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+fSz, WPX_SEEK_SET);
  }
  return true;
}


bool CWStyleManager::readKSEN(int N, int fSz)
{
  if (fSz == 0 || N== 0) return true;
  m_state->m_ksenList.resize(0);
  if (fSz != 14) {
    MWAW_DEBUG_MSG(("CWStyleManager::readKSEN: Find odd ksen size %d\n", fSz));
  }
  libmwaw::DebugStream f;

  long val;
  for (int i = 0; i < N; i++) {
    long pos = m_input->tell();
    KSEN ksen;
    f.str("");
    val = m_input->readLong(2); // always -1
    if (val != -1)
      f << "unkn=" << val << ",";
    val = m_input->readLong(4); // a big number
    if (val != -1)
      f << "f0=" << val << ",";
    for (int j = 0; j < 2; j++) { // fl0=[0|1|2|4|8|f]: a pos?, fl1=[0|8|9|b|d|f] ?
      val = m_input->readLong(2);
      if (val) f << "fl" << j << "=" << std::hex << val << std::dec << ",";
    }
    val = m_input->readLong(1); // 0-5
    switch(val) {
    case 0:
      break;
    case 1:
      ksen.m_lineType = MWAWBorder::Dash;
      break;
    case 2:
      ksen.m_lineType = MWAWBorder::Dot;
      break;
    case 3:
      ksen.m_lineType = MWAWBorder::Double;
      break;
    case 4:
      ksen.m_lineType = MWAWBorder::Double;
      f << "w[external]=2,";
      break;
    case 5:
      ksen.m_lineType = MWAWBorder::Double;
      f << "w[internal]=2,";
      break;
    default:
      f << "#lineType=" << val << ",";
      break;
    }
    ksen.m_valign = (int) m_input->readLong(1);
    ksen.m_lines = (int) m_input->readLong(1);
    val = m_input->readLong(1); // 0-18
    if (val) f << "g1=" << val << ",";
    ksen.m_extra = f.str();
    m_state->m_ksenList.push_back(ksen);
    f.str("");
    if (!i)
      f << "Entries(Ksen)-0:";
    else
      f << "Ksen-" << i << ":";
    f << ksen;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());

    m_input->seek(pos+fSz, WPX_SEEK_SET);
  }
  return true;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
