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
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWGraphicListener.hxx"
#include "MWAWGraphicShape.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWHeader.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPictBitmap.hxx"
#include "MWAWPictData.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWPosition.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSubDocument.hxx"

#include "MacDrawProParser.hxx"

#include "MacDrawProStyleManager.hxx"

/** Internal: the structures of a MacDrawProStyleManager */
namespace MacDrawProStyleManagerInternal
{
////////////////////////////////////////
//! Internal: the state of a MacDrawProStyleManager
struct State {
  //! constructor
  State() : m_documentSize(),
    m_numColors(8), m_numBWPatterns(0), m_numColorPatterns(0), m_numPatternsInTool(0),
    m_colorList(), m_dashList(), m_fontList(), m_penSizeList(),
    m_BWPatternList(), m_colorPatternList()
  {
    for (int i=0; i<5; ++i) m_numStyleZones[i]=0;
  }
  //! returns a pattern if posible
  bool getPattern(int id, MWAWGraphicStyle::Pattern &pat)
  {
    if (m_BWPatternList.empty()) initBWPatterns();
    if (id<=0 || id>int(m_BWPatternList.size())) {
      MWAW_DEBUG_MSG(("MacDrawProStyleManagerInternal::getPattern: can not find pattern %d\n", id));
      return false;
    }
    pat=m_BWPatternList[size_t(id-1)];
    return true;
  }
  //! returns a color if posible
  bool getColor(int id, MWAWColor &col)
  {
    if (m_colorList.empty()) initColors();
    if (id<=0 || id>int(m_colorList.size())) {
      MWAW_DEBUG_MSG(("MacDrawProStyleManagerInternal::getColor: can not find color %d\n", id));
      return false;
    }
    col=m_colorList[size_t(id-1)];
    return true;
  }
  //! init the black and white patterns list
  void initBWPatterns();
  //! init the colors list
  void initColors();
  //! init the dashs list
  void initDashs();
  //! init the pens list
  void initPens();
  //! the document size (in point)
  Vec2f m_documentSize;
  //! the number of zones
  int m_numStyleZones[5];
  //! the number of color
  int m_numColors;
  //! the number of BW pattern
  int m_numBWPatterns;
  //! the number of color pattern
  int m_numColorPatterns;
  //! the number of pattern in tool list
  int m_numPatternsInTool;
  //! the size of the header zones
  long m_sizeHeaderZones[5];

  //! the color list
  std::vector<MWAWColor> m_colorList;
  //! the list of dash
  std::vector< std::vector<float> > m_dashList;
  //! the list of font
  std::vector<MWAWFont> m_fontList;
  //! the list of pen size
  std::vector<float> m_penSizeList;

  //! the patterns list
  std::vector<MWAWGraphicStyle::Pattern> m_BWPatternList;
  //! the color patterns list
  std::vector<MWAWGraphicStyle::Pattern> m_colorPatternList;
};

void State::initColors()
{
  if (!m_colorList.empty()) return;
  m_colorList.push_back(MWAWColor::white());
  m_colorList.push_back(MWAWColor::black());
  m_colorList.push_back(MWAWColor(0xDD,0x8,0x6));
  m_colorList.push_back(MWAWColor(0,0x80,0x11));
  m_colorList.push_back(MWAWColor(0,0,0xd4));
  m_colorList.push_back(MWAWColor(0xFC,0xF3,0x5));
  m_colorList.push_back(MWAWColor(0x2,0xAB,0xEB));
  m_colorList.push_back(MWAWColor(0xF2,0x8,0x85));
}

void State::initDashs()
{
  if (!m_dashList.empty()) return;
  std::vector<float> dash;
  // 1: 9x9
  dash.push_back(9);
  dash.push_back(9);
  m_dashList.push_back(dash);
  // 2: 27x9
  dash[0]=27;
  m_dashList.push_back(dash);
  // 3: 18x18
  dash[0]=dash[1]=18;
  m_dashList.push_back(dash);
  // 4: 54x18
  dash[0]=54;
  m_dashList.push_back(dash);
  // 5: 72x9, 9x9
  dash.resize(4,9);
  dash[0]=72;
  dash[1]=9;
  m_dashList.push_back(dash);
  // 6: 72x9, 9x9, 9x9
  dash.resize(6,9);
}

void State::initPens()
{
  if (!m_penSizeList.empty()) return;
  static float const(values[])= {1,2,4,6,8,10};
  for (int i=0; i<6; ++i) m_penSizeList.push_back(values[i]);
}

void State::initBWPatterns()
{
  if (!m_BWPatternList.empty()) return;
  for (int i=0; i<59; ++i) {
    static uint16_t const(patterns[]) = {
      0xf0f0,0x3c3c,0x0f0f,0xc3c3, 0x1111,0x4444,0x1111,0x4444,
      0x7777,0xbbbb,0x7777,0xbbbb, 0x9999,0xcccc,0x9999,0xcccc,
      0x0000,0x0000,0x0000,0x0000, 0xffff,0xffff,0xffff,0xffff,
      0x7f7f,0xffff,0xf7f7,0xffff, 0x7f7f,0xf7f7,0x7f7f,0xf7f7,
      0x7777,0xdddd,0x7777,0xdddd, 0x7777,0x7777,0x7777,0x7777,
      0xaaaa,0xaaaa,0xaaaa,0xaaaa, 0x8888,0x8888,0x8888,0x8888,
      0x8888,0x2222,0x8888,0x2222, 0x8080,0x0808,0x8080,0x0808,
      0x8080,0x0000,0x0808,0x0000, 0x8080,0x4141,0x0808,0x1414,
      0xffff,0x8080,0xffff,0x0808, 0x8181,0x2424,0x8181,0x2424,
      0x8080,0x2020,0x0808,0x0202, 0xe0e0,0x3838,0x0e0e,0x8383,
      0x7777,0xdddd,0x7777,0xdddd, 0x8888,0x2222,0x8888,0x2222,
      0x9999,0x6666,0x9999,0x6666, 0x2020,0x8080,0x0808,0x0202,
      0xffff,0xffff,0xffff,0xffff, 0xffff,0x0000,0xffff,0x0000,
      0xcccc,0x0000,0x3333,0x0000, 0xf0f0,0xf0f0,0x0f0f,0x0f0f,
      0xffff,0x8888,0xffff,0x8888, 0xaaaa,0xaaaa,0xaaaa,0xaaaa,
      0x0101,0x0404,0x1010,0x4040, 0x8383,0x0e0e,0x3838,0xe0e0,
      0xeeee,0xbbbb,0xeeee,0xbbbb, 0x1111,0x4444,0x1111,0x4444,
      0x3333,0xcccc,0x3333,0xcccc, 0x4040,0x0000,0x0404,0x0000,
      0xaaaa,0xaaaa,0xaaaa,0xaaaa, 0x8888,0x8888,0x8888,0x8888,
      0x0101,0x1010,0x0101,0x1010, 0x0000,0x1414,0x5555,0x1414,
      0xffff,0x8080,0x8080,0x8080, 0x8282,0x2828,0x2828,0x8282,
      0x8080,0x0000,0x0000,0x0000, 0x8080,0x0202,0x0101,0x4040,
      0xaaaa,0xaaaa,0xaaaa,0xaaaa, 0x5555,0x5555,0x5555,0x5555,
      0xdddd,0x7777,0xdddd,0x7777, 0xaaaa,0x8080,0x8888,0x8080,
      0x0808,0x2222,0x8080,0x0202, 0xb1b1,0x0303,0xd8d8,0x0c0c,
      0x8888,0x2222,0x8888,0xaaaa, 0x8282,0x3939,0x8282,0x0101,
      0x0303,0x4848,0x0c0c,0x0101, 0x5555,0x4040,0x5555,0x0404,
      0x1010,0x5454,0xffff,0x0404, 0x2020,0x8888,0x8888,0x0505,
      0xbfbf,0xbfbf,0xb0b0,0xb0b0, 0xf8f8,0x2222,0x8f8f,0x2222,
      0x7777,0x8f8f,0x7777,0xf8f8,
    };
    MWAWGraphicStyle::Pattern pat;
    pat.m_dim=Vec2i(8,8);
    pat.m_data.resize(8);
    pat.m_colors[0]=MWAWColor::white();
    pat.m_colors[1]=MWAWColor::black();

    uint16_t const *patPtr=&patterns[4*i];
    for (size_t j=0; j<8; j+=2, ++patPtr) {
      pat.m_data[j]=uint8_t((*patPtr)>>8);
      pat.m_data[j+1]=uint8_t((*patPtr)&0xFF);
    }
    if (i==0) m_BWPatternList.push_back(pat); // none pattern
    m_BWPatternList.push_back(pat);
  }
}

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MacDrawProStyleManager::MacDrawProStyleManager(MacDrawProParser &parser) :
  m_parser(parser), m_parserState(parser.getParserState()), m_state(new MacDrawProStyleManagerInternal::State)
{
}

MacDrawProStyleManager::~MacDrawProStyleManager()
{
}

////////////////////////////////////////////////////////////
//
// Interface
//
////////////////////////////////////////////////////////////
bool MacDrawProStyleManager::getColor(int cId, MWAWColor &color) const
{
  m_state->initColors();
  if (cId==0) return false; // none
  if (cId<=0||cId>int(m_state->m_colorList.size())) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::getColor: can not find color %d\n", cId));
    return false;
  }
  color=m_state->m_colorList[size_t(cId-1)];
  return true;
}
bool MacDrawProStyleManager::getDash(int dId, std::vector<float> &dash) const
{
  if (dId==0) // a solid line
    return false;
  if (dId<=0||dId>int(m_state->m_dashList.size())) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::getDash: can not find dash %d\n", dId));
    return false;
  }
  dash=m_state->m_dashList[size_t(dId-1)];
  return true;
}
bool MacDrawProStyleManager::getPenSize(int pId, float &penSize) const
{
  m_state->initPens();
  if (pId<=0||pId>int(m_state->m_penSizeList.size())) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::getPenSize: can not find pen %d\n", pId));
    return false;
  }
  penSize=m_state->m_penSizeList[size_t(pId-1)];
  return true;
}

bool MacDrawProStyleManager::getPattern(int pId, MWAWGraphicStyle::Pattern &pattern) const
{
  if (pId==0) // no pattern
    return false;
  if (pId&0x4000) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::getPattern: call with uniform color id=%d\n", (pId&0x3FFF)));
    return false;
  }
  if (pId&0x8000) {
    pId&=0x3FFF;
    if (pId<=0||pId>int(m_state->m_colorPatternList.size())) {
      MWAW_DEBUG_MSG(("MacDrawProStyleManager::getPattern: can not find color pattern %d\n", pId));
      return false;
    }
    pattern=m_state->m_colorPatternList[size_t(pId-1)];
  }

  m_state->initBWPatterns();
  if (pId<=0||pId>int(m_state->m_BWPatternList.size())) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::getPattern: can not find BW pattern %d\n", pId));
    return false;
  }
  pattern=m_state->m_BWPatternList[size_t(pId-1)];

  return true;
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool MacDrawProStyleManager::readHeaderInfoStylePart(std::string &extra)
{
  extra="";
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=input->tell();
  if (!input->checkPosition(pos+10+14)) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readHeaderInfoStylePart: the zone is too short\n"));
    return false;
  }

  libmwaw::DebugStream f;
  f << "NZones=[";
  for (int i=0; i<5; ++i) {
    m_state->m_numStyleZones[i]=(int)input->readULong(2);
    f << m_state->m_numStyleZones[i] << ",";
  }
  f << "],";
  for (int i=0; i<7; ++i) {
    int val=(int) input->readULong(2);
    if (!val) continue;
    switch (i) {
    case 0:
      m_state->m_numPatternsInTool=val;
      f << "num[PatternsTool]=" << val << ",";
      break;
    case 1:
      m_state->m_numBWPatterns=val;
      f << "num[BWPatterns]=" << val << ",";
      break;
    case 2:
      m_state->m_numColors=val;
      if (val!=8) f << "numColors=" << val << ",";
      break;
    case 3:
      m_state->m_numColorPatterns=val;
      f << "num[colPatterns]=" << val << ",";
      break;
    default:
      f << "g" << i << "=" << val << ",";
      break;
    }
  }
  extra=f.str();
  return true;
}

bool MacDrawProStyleManager::readStyles(long const(&sizeZones)[5])
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  long pos;
  for (int i=0; i<5; ++i) {
    if (!sizeZones[i])
      continue;
    pos=input->tell();

    bool done=false;
    MWAWEntry entry;
    entry.setBegin(pos);
    entry.setLength(sizeZones[i]);
    switch (i) {
    case 0:
      done=readRulers(entry);
      break;
    case 1:
      done=readPens(entry);
      break;
    case 2:
      done=readDashs(entry);
      break;
    case 3:
      done=readArrows(entry);
      break;
    case 4:
      done=readFontStyles(entry);
      break;
    default:
      break;
    }
    if (done) continue;
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readStyles: can not read zone %d\n", i));
    ascFile.addPos(entry.end());
    ascFile.addNote("Entries(Styles):###");
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  }

  return true;
}

bool MacDrawProStyleManager::readFontStyles(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readFontStyles: the entry is bad\n"));
    return false;
  }

  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  entry.setParsed(true);
  long pos=entry.begin();
  if ((entry.length()%18)!=0) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readFontStyles: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }
  int N=int(entry.length()/18);
  if (N!=m_state->m_numStyleZones[2]) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readFontStyles: oops the number of fonts seems odd\n"));
  }

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int j=0; j<N; ++j) {
    pos=input->tell();
    f.str("");
    int val=(int) input->readLong(2);
    if (val!=1) f<<"numUsed=" << val << ",";
    f << "h[total]=" << input->readLong(2) << ",";
    int sz=(int) input->readULong(2);
    MWAWFont font;
    font.setId((int) input->readULong(2));
    uint32_t flags = 0;
    int flag=(int) input->readULong(1);
    if (flag&0x1) flags |= MWAWFont::boldBit;
    if (flag&0x2) flags |= MWAWFont::italicBit;
    if (flag&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (flag&0x8) flags |= MWAWFont::embossBit;
    if (flag&0x10) flags |= MWAWFont::shadowBit;
    if (flag&0xE0) f << "#fl0=" << std::hex << (flag&0xE0) << std::dec << ",";
    font.setFlags(flags);
    val=(int) input->readULong(1);
    if (val)
      f << "fl2=" << val << ",";
    val=(int) input->readULong(2);
    font.setSize((float) val);
    if (sz!=val)
      f << "sz2=" << sz << ",";
    unsigned char col[3];
    for (int k=0; k < 3; k++)
      col[k] = (unsigned char)(input->readULong(2)>>8);
    if (col[0] || col[1] || col[2])
      font.setColor(MWAWColor(col[0],col[1],col[2]));
    font.m_extra=f.str();
    m_state->m_fontList.push_back(font);

    f.str("");
    f << "Entries(FontStyle):F" << j+1 << ":";
    f << font.getDebugString(m_parserState->m_fontConverter) << font.m_extra;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// styles/settings present in data/rsrc fork
////////////////////////////////////////////////////////////

bool MacDrawProStyleManager::readArrows(MWAWEntry const &entry, bool inRsrc)
{
  if (!entry.valid() || (inRsrc && !m_parserState->m_rsrcParser)) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readArrows: the entry is bad\n"));
    return false;
  }

  if (inRsrc && entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readArrows: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = inRsrc ? m_parserState->m_rsrcParser->getInput() : m_parserState->m_input;
  libmwaw::DebugFile &ascFile = inRsrc ? m_parserState->m_rsrcParser->ascii() : m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(Arrow)";
  if (inRsrc)
    f << "[" << entry.id() << "]:";
  else
    f << ":";
  entry.setParsed(true);
  long pos=entry.begin();
  int const expectedSz=inRsrc ? 10 : 14;
  if ((entry.length()%expectedSz)!=0) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readArrows: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(inRsrc ? pos-4 : pos);
    ascFile.addNote(f.str().c_str());
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }
  if (inRsrc) {
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
  }
  int N=int(entry.length()/expectedSz);
  if (!inRsrc && N!=m_state->m_numStyleZones[3]) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readArrows: oops the number of arrows seems odd\n"));
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "Entries(Arrow):A" << i+1 << ",";
    if (!inRsrc) {
      int val=(int) input->readLong(2); // always 0
      if (val) f<<"f0=" << val << ",";
      val=(int) input->readLong(2);
      if (val!=1) f<<"numUsed=" << val << ",";
      val=(int) input->readLong(2);
      if (val!=i+1) f << "id=" << val << ",";
    }
    float pt[2]; // position of the symetric point in the arrow head
    for (int k=0; k<2; ++k) pt[k]=float(input->readULong(4))/65536.f;
    f << "pt=" << pt[1] << "x" << pt[0] << ",";
    if (inRsrc) {
      int val=(int) input->readLong(2);
      if (val!=i+1) f << "id=" << val << ",";
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool MacDrawProStyleManager::readDashs(MWAWEntry const &entry, bool inRsrc)
{
  if (!entry.valid() || (inRsrc && !m_parserState->m_rsrcParser)) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readDashs: the entry is bad\n"));
    return false;
  }

  if (inRsrc && entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readDashs: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = inRsrc ? m_parserState->m_rsrcParser->getInput() : m_parserState->m_input;
  libmwaw::DebugFile &ascFile = inRsrc ? m_parserState->m_rsrcParser->ascii() : m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(Dash)";
  if (inRsrc)
    f << "[" << entry.id() << "]:";
  else
    f << ":";
  entry.setParsed(true);
  long pos=entry.begin();
  int const expectedSz=inRsrc ? 26 : 28;
  if ((entry.length()%expectedSz)!=0) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readDashs: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(inRsrc ? pos-4 : pos);
    ascFile.addNote(f.str().c_str());
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }
  m_state->m_dashList.clear();
  if (inRsrc) {
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
  }
  int N=int(entry.length()/expectedSz);
  if (!inRsrc && N!=m_state->m_numStyleZones[4]) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readDashs: oops the number of dashs seems odd\n"));
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "Entries(Dash):D" << i+1 << ",";
    if (!inRsrc) {
      int val=(int) input->readLong(2); // always 0
      if (val) f<<"f0=" << val << ",";
      val=(int) input->readLong(2);
      if (val!=1) f<<"numUsed=" << val << ",";
    }
    f << "dash=[";
    std::vector<float> dash;
    for (int k=0; k<3; ++k) {
      long solid=(long) input->readULong(4);
      long empty=(long) input->readULong(4);
      if (!solid && !empty) continue;
      dash.push_back(float(solid)/65536.f);
      dash.push_back(float(empty)/65536.f);
      f << double(solid)/65536. << "x" << double(empty)/65536. << ",";
    }
    m_state->m_dashList.push_back(dash);
    f << "],";
    if (inRsrc) {
      int val=(int) input->readLong(2);
      if (val!=i+1) f << "id=" << val << ",";
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool MacDrawProStyleManager::readPens(MWAWEntry const &entry, bool inRsrc)
{
  if (!entry.valid() || (inRsrc && !m_parserState->m_rsrcParser)) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPens: the entry is bad\n"));
    return false;
  }

  if (inRsrc && entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPens: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = inRsrc ? m_parserState->m_rsrcParser->getInput() : m_parserState->m_input;
  libmwaw::DebugFile &ascFile = inRsrc ? m_parserState->m_rsrcParser->ascii() : m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(Pen)";
  if (inRsrc)
    f << "[" << entry.id() << "]:";
  else
    f << ":";
  entry.setParsed(true);
  long pos=entry.begin();
  int const expectedSz=inRsrc ? 8 : 12;
  if ((entry.length()%expectedSz)!=0) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPens: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(inRsrc ? pos-4 : pos);
    ascFile.addNote(f.str().c_str());
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }
  m_state->m_penSizeList.clear();
  if (inRsrc) {
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
  }
  int N=int(entry.length()/expectedSz);
  if (!inRsrc && N!=m_state->m_numStyleZones[1]) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPens: oops the number of pens seems odd\n"));
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "Entries(Pen):P" << i+1 << ",";
    if (!inRsrc) {
      int val=(int) input->readLong(2); // always 0
      if (val) f<<"f0=" << val << ",";
      val=(int) input->readLong(2);
      if (val!=1) f<<"numUsed=" << val << ",";
    }
    long penSize=(long) input->readULong(4);
    if (penSize!=0x10000) f << "pen[size]=" << float(penSize)/65536.f << ",";
    if (!inRsrc) {
      int dim[2];
      for (int j=0; j<2; ++j) dim[j]=(int) input->readULong(2);
      f << "pen=" << Vec2i(dim[1],dim[0]) << ",";
      m_state->m_penSizeList.push_back(float(dim[0]+dim[1])/2.f);
    }
    else {
      m_state->m_penSizeList.push_back(float(penSize)/65536.f);
      int val=(int) input->readLong(2); // unit ?
      if (val) f << "fl=" << std::hex << val << std::dec << ",";
      val=(int) input->readLong(2);
      if (val!=i+1) f << "id=" << val << ",";
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool MacDrawProStyleManager::readRulers(MWAWEntry const &entry, bool inRsrc)
{
  if (!entry.valid() || (inRsrc && !m_parserState->m_rsrcParser)) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readRulers: the entry is bad\n"));
    return false;
  }

  if (inRsrc && entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readRulers: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = inRsrc ? m_parserState->m_rsrcParser->getInput() : m_parserState->m_input;
  libmwaw::DebugFile &ascFile = inRsrc ? m_parserState->m_rsrcParser->ascii() : m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(RulerStyle)";
  if (inRsrc)
    f << "[" << entry.id() << "]:";
  else
    f << ":";
  entry.setParsed(true);

  long pos=entry.begin();
  if ((entry.length()%24)!=0) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readRulers: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(inRsrc ? pos-4 : pos);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  if (inRsrc) {
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
  }
  int N=int(entry.length()/24);
  if (!inRsrc && N!=m_state->m_numStyleZones[0]) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readRulers: oops the number of rulers seems odd\n"));
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "Entries(RulerStyle):R" << i+1 << ":";
    int numUsed=(int) input->readULong(4);
    if (numUsed!=1) f << "numUsed=" << numUsed << ",";
    for (int j=0; j<2; ++j) {
      double value;
      bool isNAN;
      if (!input->readDouble8(value, isNAN))
        f << "###,";
      else
        f << value << ",";
    }
    f << "num[subd]=" << input->readULong(2) << ",";
    f << "fl=" << std::hex << input->readULong(2) << std::dec << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// RSRC zones
////////////////////////////////////////////////////////////
bool MacDrawProStyleManager::readRSRCZones()
{
  MWAWRSRCParserPtr rsrcParser = m_parserState->m_rsrcParser;
  if (!rsrcParser) return false;

  readFontNames();

  std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::iterator it;
  // the 256 zones
  char const *(zNames[]) = {
    "Dinf", "Pref",
    "Ctbl", "bawP", "colP", "patR",
    "Aset", "Dset", "DRul", "Pset", "Rset", "Rst2", "Dvws",
    "Dstl"
  };
  /* find also
     mmpp: with 0000000000000000000000000000000000000000000000000000000000000000
     pPrf: with 0001000[01]01000002000000000000
     sPrf: spelling preference ? ie find in one file and contains strings "Main/User dictionary"...
     xPrf: with 000000010000000[13]
   */
  for (int z = 0; z < 14; z++) {
    it = entryMap.lower_bound(zNames[z]);
    while (it != entryMap.end()) {
      if (it->first != zNames[z])
        break;
      MWAWEntry const &entry = it++->second;
      switch (z) {
      case 0: // document info
        readDocumentInfo(entry);
        break;
      case 1: // preferences
        readPreferences(entry);
        break;
      case 2: // color table
        readColors(entry);
        break;
      case 3: // BW patterns
        readBWPatterns(entry);
        break;
      case 4: // color patterns
        readColorPatterns(entry);
        break;
      case 5: // list of patterns in tools
        readPatternsToolList(entry);
        break;
      case 6: // arrow definitions
        readArrows(entry, true);
        break;
      case 7: // dashes
        readDashs(entry, true);
        break;
      case 8: // ruler
        readRulers(entry, true);
        break;
      case 9: // pen size
        readPens(entry, true);
        break;
      case 10: // ruler settings
      case 11:
        readRulerSettings(entry);
        break;
      case 12: // views positions
        readViews(entry);
        break;
      case 13: // unknown
        readRSRCDstl(entry);
        break;
      default:
        MWAW_DEBUG_MSG(("MacDrawProStyleManager::readRSRCZones: called with unexpected settings %d\n", z));
        break;
      }
    }
  }
  // Ctbl: list of 16*data: numUsed, id, color, pos?
  return true;
}

bool MacDrawProStyleManager::readPreferences(MWAWEntry const &entry)
{
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPreferences: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPreferences: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(Preferences)[" << entry.id() << "]:";
  entry.setParsed(true);

  long pos=entry.begin();
  if (entry.length()!=0x1a) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPreferences: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int val=(int) input->readLong(2); // always 0?
  if (val) f << "f0=" << val << ",";
  f << "fl=[";
  for (int i=0; i<4; ++i) { // fl0=20|30, fl2=1|2, fl3=2|3
    val=(int) input->readULong(1);
    f << std::hex << val << std::dec << ",";
  }
  f << "],";
  for (int i=0; i<5; ++i) { // f1=12|1b, f2=f4=f5=0, f3=1e
    val=(int) input->readLong(2);
    if (val)
      f << "f" << i+1 << "=" << val << ",";
  }
  val=(int) input->readULong(2);
  if (val!=0xC000) f << "g0=" << std::hex << val << std::dec << ",";
  for (int i=0; i<4; ++i) { // g1=0, g2=0|8, g3=0|4, g5=0|3
    val=(int) input->readLong(2);
    if (val)
      f << "g" << i+1 << "=" << val << ",";
  }

  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool MacDrawProStyleManager::readDocumentInfo(MWAWEntry const &entry)
{
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readDocumentInfo: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readDocumentInfo: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(DocumentInfo)[" << entry.id() << "]:";
  entry.setParsed(true);

  long pos=entry.begin();
  if (entry.length()!=0x58) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readDocumentInfo: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int val;
  f << "fl=[";
  for (int i=0; i<4; ++i) { // fl0=24|64 (version?), fl1=d|f|1d, f2=2, f3=8|9|10
    val=(int) input->readULong(1);
    f << std::hex << val << std::dec << ",";
  }
  f << "],";
  f << "windows[dim]?=[";
  for (int i=0; i<4; ++i) { // 29|2a,3,d2|1b0, 110|250
    val=(int) input->readLong(2);
    f << val << ",";
  }
  f << "],";
  f << "num?=[";
  for (int i=0; i<14; ++i) { // series of small number
    val=(int) input->readLong(2);
    if (val) f << val << ",";
    else f << "_,";
  }
  f << "],";
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "DocumentInfo-II:";
  for (int i=0; i<5; ++i) { // f3=2, f4=0|20, other 0
    val=(int) input->readLong(2);
    if (val)
      f << "f" << i << "=" << val << ",";
  }
  int dim[2];
  for (int i=0; i<2; ++i) dim[i]=(int) input->readULong(2);
  if (dim[0]||dim[1]) f << "pos0?=" << dim[0] << "x" << dim[1] << ",";
  float fDim[2];
  for (int i=0; i<2; ++i) fDim[i]=float(input->readLong(4))/65536.f;
  m_state->m_documentSize=Vec2f(fDim[0],fDim[1]);
  f << "document[size]=" << m_state->m_documentSize << ",";
  val=(int) input->readLong(2); // 0|1
  if (val) f << "f5=" << val << ",";
  for (int i=0; i<2; ++i) fDim[i]=float(input->readLong(4))/65536.f;
  f << "pos1?=" << Vec2f(fDim[0],fDim[1]) << ",";
  for (int i=0; i<4; ++i) { // always 0?
    val=(int) input->readLong(2);
    if (val)
      f << "g" << i << "=" << val << ",";
  }
  for (int i=0; i<2; ++i) fDim[i]=float(input->readLong(4))/65536.f;
  f << "page[size]=" << Vec2f(fDim[1],fDim[0]) << ",";

  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool MacDrawProStyleManager::readFontNames()
{
  MWAWRSRCParserPtr rsrcParser = m_parserState->m_rsrcParser;
  if (!rsrcParser) return false;
  std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
  MWAWEntry idPosEntry, nameEntry;
  if (entryMap.find("Fnms")!=entryMap.end())
    nameEntry=entryMap.find("Fnms")->second;
  if (entryMap.find("Fmtx")!=entryMap.end())
    idPosEntry=entryMap.find("Fmtx")->second;
  if (!nameEntry.valid() && !idPosEntry.valid()) return true;
  if (!nameEntry.valid() || !idPosEntry.valid() || (idPosEntry.length()%8)!=0 ||
      nameEntry.id()!=256 || idPosEntry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readFontNames: the entries seems incoherent\n"));
    return false;
  }

  entryMap.find("Fnms")->second.setParsed(true);
  entryMap.find("Fmtx")->second.setParsed(true);

  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;

  // first read the id and the string delta pos
  long pos=idPosEntry.begin();
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "Entries(FontName)[pos]:";
  int N=int(idPosEntry.length()/8);
  std::map<int, long> idPosMap;
  for (int i=0; i<N; ++i) {
    int id=(int) input->readULong(4);
    long dPos=(long) input->readULong(4);
    f << std::hex << dPos << std::dec << "[id=" << id << "],";
    idPosMap[id]=dPos;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());

  // now read the name
  pos=nameEntry.begin();
  long endPos=nameEntry.end();
  f.str("");
  f << "Entries(FontName):";
  for (std::map<int, long>::const_iterator it=idPosMap.begin(); it!=idPosMap.end(); ++it) {
    long dPos=it->second;
    if (dPos<0 || pos+dPos+1>endPos) {
      MWAW_DEBUG_MSG(("MacDrawProStyleManager::readFontNames: the name positions seems bad\n"));
      f << "###[id=" << it->first << "],";
      continue;
    }
    input->seek(pos+dPos, librevenge::RVNG_SEEK_SET);
    int sSz=(int) input->readULong(1);
    if (sSz==0 || pos+dPos+1+sSz>endPos) {
      MWAW_DEBUG_MSG(("MacDrawProStyleManager::readFontNames: the name size seems bad\n"));
      f << "###[id=" << it->first << "],";
      continue;
    }
    std::string name("");
    for (int i=0; i<sSz; ++i) name+=(char) input->readULong(1);
    f << name << "[id=" << it->first << "],";
    m_parserState->m_fontConverter->setCorrespondance(it->first, name);
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());

  return true;
}

bool MacDrawProStyleManager::readColors(MWAWEntry const &entry)
{
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readColors: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readColors: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(ColorMap)";
  f << "[" << entry.id() << "]:";
  entry.setParsed(true);
  long pos=entry.begin();
  if ((entry.length()%16)!=0) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readColors: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  m_state->m_colorList.clear();
  int N=int(entry.length()/16);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "ColorMap-" << i << ":";
    int val=(int) input->readULong(4);
    if (val!=1) f << "num[used]=" << val << ",";
    val =(int) input->readULong(2); // index of ?
    f << "id0=" << val << ",";
    unsigned char col[3];
    for (int k=0; k < 3; k++)
      col[k] = (unsigned char)(input->readULong(2)>>8);
    MWAWColor color(col[0],col[1],col[2]);
    f << color << ",";
    m_state->m_colorList.push_back(color);
    val=(int) input->readULong(2); // always 0?
    if (val) f << "f0=" << val << ",";
    val =(int) input->readULong(2); // index of ?
    f << "id1=" << val << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool MacDrawProStyleManager::readBWPatterns(MWAWEntry const &entry)
{
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readBWPatterns: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readBWPatterns: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(BWPattern)";
  f << "[" << entry.id() << "]:";
  entry.setParsed(true);
  long pos=entry.begin();
  if ((entry.length()%12)!=0) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readBWPatterns: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  m_state->m_BWPatternList.clear();
  int N=int(entry.length()/12);
  if (N!=m_state->m_numBWPatterns) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readBWPatterns: the number of patterns seems bad\n"));
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);

  MWAWGraphicStyle::Pattern pat;
  pat.m_dim=Vec2i(8,8);
  pat.m_data.resize(8);
  pat.m_colors[0]=MWAWColor::white();
  pat.m_colors[1]=MWAWColor::black();
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "BWPattern-" << i+1 << ":";
    int val=(int) input->readULong(4);
    if (val!=1) f << "num[used]=" << val << ",";
    for (size_t j=0; j<8; ++j)
      pat.m_data[j]=uint8_t(input->readULong(1));
    f << pat;
    m_state->m_BWPatternList.push_back(pat);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool MacDrawProStyleManager::readColorPatterns(MWAWEntry const &entry)
{
  if (!entry.length()) { // ok not color patterns
    entry.setParsed(true);
    return true;
  }
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readColorPatterns: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readColorPatterns: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(ColorPattern)";
  f << "[" << entry.id() << "]:";
  entry.setParsed(true);
  long pos=entry.begin();
  if ((entry.length()%70)!=0) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readColorPatterns: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  m_state->m_colorPatternList.clear();

  int N=int(entry.length()/70);
  if (N!=m_state->m_numColorPatterns) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readColorPatterns: the number of patterns seems bad\n"));
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  if (m_state->m_colorList.empty()) m_state->initColors();

  int numColors=(int)m_state->m_colorList.size();
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    MWAWPictBitmapIndexed bitmap(Vec2i(8,8));
    bitmap.setColors(m_state->m_colorList);

    f.str("");
    f << "ColorPattern-" << i+1 << ":";
    int val=(int) input->readULong(4);
    if (val!=1) f << "num[used]=" << val << ",";
    // the final pattern id
    f << "id=" << input->readLong(2) << ",";
    int indices[8];
    int sumColors[3]= {0,0,0};
    for (int r=0; r<8; ++r) {
      for (size_t c=0; c<8; ++c) {
        int id=(int) input->readULong(1);
        if (id<0 || id>=numColors) {
          static bool first=true;
          if (first) {
            MWAW_DEBUG_MSG(("MacDrawProStyleManager::readColorPatterns: find some bad indices\n"));
            f << "#id=" << id << ",";
            first=false;
          }
          indices[c]=0;
          continue;
        }
        indices[c]=id;
        MWAWColor const &col=m_state->m_colorList[size_t(id)];
        sumColors[0]+=(int)col.getRed();
        sumColors[1]+=(int)col.getGreen();
        sumColors[2]+=(int)col.getBlue();
      }
      bitmap.setRow(r, indices);
    }
    MWAWGraphicStyle::Pattern pat;
    librevenge::RVNGBinaryData binary;
    std::string type;
    if (bitmap.getBinary(binary,type)) {
      pat=MWAWGraphicStyle::Pattern
          (Vec2i(8,8), binary, type,MWAWColor((unsigned char)(sumColors[0]/64),(unsigned char)(sumColors[1]/64),(unsigned char)(sumColors[2]/64)));
    }
    else {
      MWAW_DEBUG_MSG(("MacDrawProStyleManager::readColorPatterns: oops can not create the binary data\n"));
      pat.m_dim=Vec2i(8,8);
      pat.m_data.resize(8, 0);
      pat.m_colors[0]=MWAWColor::white();
      pat.m_colors[1]=MWAWColor::black();
    }
    m_state->m_colorPatternList.push_back(pat);
    f << pat;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool MacDrawProStyleManager::readPatternsToolList(MWAWEntry const &entry)
{
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPatternsToolList: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPatternsToolList: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(PatternTool)[" << entry.id() << "]:";
  entry.setParsed(true);

  long pos=entry.begin();
  if ((entry.length()%4)!=0) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPatternsToolList: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  int N=int(entry.length()/4);
  if (N!=m_state->m_numPatternsInTool) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPatternsToolList: the number of patterns seems bad\n"));
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "pats=[";
  for (int i=0; i<N; ++i) {
    int val=(int) input->readULong(2);
    if (val&0x8000) f << "C" << (val&0x7FFF);
    else if (val&0x4000) f << "CC" << (val&0x3FFF); // pattern created using the color list
    else if (val) f << "BW" << val;
    else f << "_";
    val=(int) input->readULong(2);
    if (val&0x8000) f << ":#C" << (val&0x7FFF); // does not seems to appear
    else if (val&0x4000) f << ":#CC" << (val&0x3FFF); // does not seems to appear
    else if (val) f << ":BW" << val; // follows normally a C or a CC pattern
    f << ","; // follows normally a BW or a _ pattern
  }
  f << "],";
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool MacDrawProStyleManager::readRulerSettings(MWAWEntry const &entry)
{
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readRulerSettings: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readRulerSettings: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(RulerSetting)[" << entry.type() << entry.id() << "]:";
  entry.setParsed(true);

  long pos=entry.begin();
  if ((entry.length()%24)!=0) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readRulerSettings: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  int N=int(entry.length()/24);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "RulerSetting[R" << i+1 << "]:";
    for (int j=0; j<2; ++j)
      f << "dim" << j << "=" << float(input->readULong(4))/65536.f << ",";
    int val;
    for (int j=0; j<2; ++j) { // seems related to RulerStyle flags
      val=(int) input->readULong(2);
      if (val) f << "fl" << j << "=" << std::hex << val << std::dec << ",";
    }
    for (int j=0; j<4; ++j) { // always 0
      val=(int) input->readLong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }
    val=(int) input->readLong(2);
    if (val) f << "numSub=" << val << ",";
    val=(int) input->readLong(2);
    if (val!=i+1) f << "id=" << val << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+24, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

bool MacDrawProStyleManager::readViews(MWAWEntry const &entry)
{
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readViews: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readViews: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(View)[" << entry.type() << entry.id() << "]:";
  entry.setParsed(true);

  long pos=entry.begin();
  if ((entry.length()%8)!=0) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readViews: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  int N=int(entry.length()/8);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "View[V" << i+1 << "]:";
    int val=(int) input->readULong(2);
    if (!val) {
      ascFile.addPos(pos);
      ascFile.addNote("_");
      input->seek(pos+8, librevenge::RVNG_SEEK_SET);
      continue;
    }
    if (val!=1) f << "numUsed?=" << val << ",";
    val=(int) input->readULong(2); // 0 or 1
    if (val) f << "f0=" << val << ",";
    f << "pos=" << input->readLong(2) << "x" << input->readLong(2) << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

// unknown rsrc data
bool MacDrawProStyleManager::readRSRCDstl(MWAWEntry const &entry)
{
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readRSRCDstls: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readRSRCDstls: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(UnknDstl)[" << entry.id() << "]:";
  entry.setParsed(true);

  long pos=entry.begin();
  // find always length=18, but I suppose that it can be greater
  if (entry.length()<18 || (entry.length()%2)!=0) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readRSRCDstls: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  // almost always [] but find [0,1]|[0,2]|[3,2,4]|[3,4,5,7]
  int N=int(entry.length())/2;
  f << "list=[";
  for (int i=0; i<N; ++i) {
    int val=(int) input->readLong(2);
    if (val==-1) {
      input->seek(-2, librevenge::RVNG_SEEK_CUR);
      break;
    }
    f << val << ",";
  }
  f << "],";
  if (input->tell()!=entry.end())
    ascFile.addDelimiter(input->tell(),'|');
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
