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
// gradient of MacDrawProStyleManagerInternal
struct Gradient {
  //! constructor
  Gradient() : m_type(MWAWGraphicStyle::G_None), m_stopList(), m_angle(0), m_percentCenter(0.5f,0.5f), m_extra("")
  {
  }
  //! returns true if the gradient is defined
  bool hasGradient() const
  {
    return m_type != MWAWGraphicStyle::G_None && (int) m_stopList.size() >= 2;
  }
  //! a print operator
  friend std::ostream &operator<<(std::ostream &o, Gradient const &grad);

  //! the gradient type
  MWAWGraphicStyle::GradientType m_type;
  //! the list of gradient limits
  std::vector<MWAWGraphicStyle::GradientStop> m_stopList;
  //! the gradient angle
  float m_angle;
  //! the gradient center
  MWAWVec2f m_percentCenter;
  //! extra data
  std::string m_extra;
};

std::ostream &operator<<(std::ostream &o, Gradient const &grad)
{
  if (!grad.hasGradient()) {
    o << "none," << grad.m_extra;
    return o;
  }
  switch (grad.m_type) {
  case MWAWGraphicStyle::G_Axial:
    o << "axial,";
    break;
  case MWAWGraphicStyle::G_Linear:
    o << "linear,";
    break;
  case MWAWGraphicStyle::G_Radial:
    o << "radial,";
    break;
  case MWAWGraphicStyle::G_Rectangular:
    o << "rectangular,";
    break;
  case MWAWGraphicStyle::G_Square:
    o << "square,";
    break;
  case MWAWGraphicStyle::G_Ellipsoid:
    o << "ellipsoid,";
    break;
  case MWAWGraphicStyle::G_None:
  default:
    break;
  }
  if (grad.m_angle>0 || grad.m_angle<0) o << "angle=" << grad.m_angle << ",";
  if (grad.m_stopList.size() >= 2) {
    o << "stops=[";
    for (size_t s=0; s < grad.m_stopList.size(); ++s)
      o << "[" << grad.m_stopList[s] << "],";
    o << "],";
  }
  if (grad.m_percentCenter != MWAWVec2f(0.5f,0.5f)) o << "center=" << grad.m_percentCenter << ",";
  o << grad.m_extra;
  return o;
}
////////////////////////////////////////
//! Internal: the state of a MacDrawProStyleManager
struct State {
  //! constructor
  State() : m_documentSize(),
    m_numColors(8), m_numBWPatterns(0), m_numColorPatterns(0), m_numPatternsInTool(0),
    m_colorList(), m_displayColorList(), m_dashList(),
    m_fontList(), m_paragraphList(), m_penSizeList(),
    m_BWPatternList(), m_colorPatternList(), m_gradientList()
  {
    for (int i=0; i<6; ++i) m_numStyleZones[i]=0;
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
  MWAWVec2f m_documentSize;
  //! the number of zones
  int m_numStyleZones[6];
  //! the number of color
  int m_numColors;
  //! the number of BW pattern
  int m_numBWPatterns;
  //! the number of color pattern
  int m_numColorPatterns;
  //! the number of pattern in tool list
  int m_numPatternsInTool;

  //! the color list
  std::vector<MWAWColor> m_colorList;
  //! the display color list
  std::vector<MWAWColor> m_displayColorList;
  //! the list of dash
  std::vector< std::vector<float> > m_dashList;
  //! the list of font
  std::vector<MWAWFont> m_fontList;
  //! the list of paragraph
  std::vector<MWAWParagraph> m_paragraphList;
  //! the list of pen size
  std::vector<float> m_penSizeList;

  //! the patterns list
  std::vector<MWAWGraphicStyle::Pattern> m_BWPatternList;
  //! the color patterns list
  std::vector<MWAWGraphicStyle::Pattern> m_colorPatternList;
  //! the gradient list
  std::vector<Gradient> m_gradientList;
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
  m_dashList.push_back(dash);
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
    pat.m_dim=MWAWVec2i(8,8);
    pat.m_data.resize(8);
    pat.m_colors[0]=MWAWColor::white();
    pat.m_colors[1]=MWAWColor::black();

    uint16_t const *patPtr=&patterns[4*i];
    for (size_t j=0; j<8; j+=2, ++patPtr) {
      pat.m_data[j]=uint8_t((*patPtr)>>8);
      pat.m_data[j+1]=uint8_t((*patPtr)&0xFF);
    }
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
  if (cId==0) return false; // none
  if (m_parserState->m_version>0) {
    int colType=(cId>>14);
    switch (colType) {
    case 1:
      cId&=0x3FFF;
      m_state->initColors();
      if (cId<0||cId>=int(m_state->m_colorList.size())) {
        MWAW_DEBUG_MSG(("MacDrawProStyleManager::getColor: can not find color %d\n", cId));
        return false;
      }
      color=m_state->m_colorList[size_t(cId)];
      return true;
    case 2:
      cId&=0x3FFF;
      if (cId<0||cId>=int(m_state->m_displayColorList.size())) {
        MWAW_DEBUG_MSG(("MacDrawProStyleManager::getColor: can not find displayColor %d\n", cId));
        return false;
      }
      color=m_state->m_displayColorList[size_t(cId)];
      return true;
    default:
      MWAW_DEBUG_MSG(("MacDrawProStyleManager::getColor: can not find color %d\n", cId));
      return false;
    }
  }
  m_state->initColors();
  if (cId<=0||cId>int(m_state->m_colorList.size())) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::getColor: can not find color %d\n", cId));
    return false;
  }
  color=m_state->m_colorList[size_t(cId-1)];
  return true;
}

bool MacDrawProStyleManager::getFont(int fId, MWAWFont &font) const
{
  if (fId==0) return false; // none
  if (fId<=0||fId>int(m_state->m_fontList.size())) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::getFont: can not find font %d\n", fId));
    return false;
  }
  font=m_state->m_fontList[size_t(fId-1)];
  return true;
}

bool MacDrawProStyleManager::getParagraph(int fId, MWAWParagraph &para) const
{
  if (fId<0||fId>=int(m_state->m_paragraphList.size())) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::getParagraph: can not find paragraph %d\n", fId));
    return false;
  }
  para=m_state->m_paragraphList[size_t(fId)];
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
  if (m_parserState->m_version>0) {
    if ((pId&0xC000)!=0x8000) {
      MWAW_DEBUG_MSG(("MacDrawProStyleManager::getPattern: can not find BW pattern %d\n", pId));
      return false;
    }
    pId&=0x7FFF;
    if (pId<0||pId>=int(m_state->m_BWPatternList.size())) {
      MWAW_DEBUG_MSG(("MacDrawProStyleManager::getPattern: can not find BW pattern %d\n", pId));
      return false;
    }
    pattern=m_state->m_BWPatternList[size_t(pId)];
    return true;
  }
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
    return true;
  }

  m_state->initBWPatterns();
  if (pId<=0||pId>int(m_state->m_BWPatternList.size())) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::getPattern: can not find BW pattern %d\n", pId));
    return false;
  }
  pattern=m_state->m_BWPatternList[size_t(pId-1)];

  return true;
}

bool MacDrawProStyleManager::updateGradient(int gId, MWAWGraphicStyle &style) const
{
  if (gId<0 || gId>int(m_state->m_gradientList.size())) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::updateGradient: can not find gradient %d\n", gId));
    return false;
  }
  MacDrawProStyleManagerInternal::Gradient const &gradient=m_state->m_gradientList[size_t(gId)];
  style.m_gradientType=gradient.m_type;
  style.m_gradientStopList=gradient.m_stopList;
  style.m_gradientAngle=gradient.m_angle;
  style.m_gradientPercentCenter=gradient.m_percentCenter;
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
  int const vers=m_parserState->m_version;
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=input->tell();
  if (!input->checkPosition(pos+10+14)) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readHeaderInfoStylePart: the zone is too short\n"));
    return false;
  }

  libmwaw::DebugStream f;
  f << "NZones=[";
  int const NZones=vers==0 ? 5 : 6;
  for (int i=0; i<NZones; ++i) {
    m_state->m_numStyleZones[i]=(int)input->readULong(2);
    f << m_state->m_numStyleZones[i] << ",";
  }
  f << "],";
  if (vers>0) {
    extra=f.str();
    return true;
  }
  for (int i=0; i<7; ++i) {
    int val=(int) input->readLong(2);
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

bool MacDrawProStyleManager::readStyles(long const(&sizeZones)[6])
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  long pos;
  int const numZones=m_parserState->m_version==0 ? 5:6;
  for (int i=0; i<numZones; ++i) {
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
    case 5:
      done=readParagraphStyles(entry);
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

  int const vers=m_parserState->m_version;
  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  entry.setParsed(true);
  long pos=entry.begin();
  int const expectedSize=vers==0 ? 18 : 22;
  if ((entry.length()%expectedSize)!=0) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readFontStyles: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }
  int N=int(entry.length()/expectedSize);
  if (N!=m_state->m_numStyleZones[2]) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readFontStyles: oops the number of fonts seems odd\n"));
  }

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int j=0; j<N; ++j) {
    pos=input->tell();
    f.str("");
    int val=(int) input->readLong(2);
    if (val!=1) f<<"numUsed=" << val << ",";
    float sz;
    if (vers==0) {
      f << "h[total]=" << input->readLong(2) << ",";
      sz=float(input->readULong(2));
    }
    else {
      f << "h[total]=" << double(input->readLong(4))/65536. << ",";
      sz=float(input->readULong(4))/65536.f;
    }
    MWAWFont font;
    font.setId((int) input->readULong(2));
    uint32_t flags = 0;
    int flag=(int) input->readULong(vers==0 ? 1 : 2);
    if (flag&0x1) flags |= MWAWFont::boldBit;
    if (flag&0x2) flags |= MWAWFont::italicBit;
    if (flag&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (flag&0x8) flags |= MWAWFont::embossBit;
    if (flag&0x10) flags |= MWAWFont::shadowBit;
    if (flag&0x100) font.set(MWAWFont::Script::super100());
    if (flag&0x200) font.set(MWAWFont::Script::sub100());
    if (flag&0x800) flags |= MWAWFont::smallCapsBit;
    if (flag&0xF4E0) f << "#fl0=" << std::hex << (flag&0xF4E0) << std::dec << ",";
    font.setFlags(flags);
    if (vers==0) {
      val=(int) input->readULong(1);
      if (val)
        f << "fl2=" << val << ",";
    }
    float fSz=(float) input->readULong(2);
    if (vers==1) fSz/=4.f;
    font.setSize(fSz);
    if (sz<fSz || sz>fSz)
      f << "sz2=" << sz << ",";
    if (vers==0) {
      unsigned char col[3];
      for (int k=0; k < 3; k++)
        col[k] = (unsigned char)(input->readULong(2)>>8);
      if (col[0] || col[1] || col[2])
        font.setColor(MWAWColor(col[0],col[1],col[2]));
    }
    else {
      // now ?, back color
      for (int i=0; i<2; ++i) {
        val=(int) input->readULong(2);
        if (val)
          f << "f" << i+1 << "=" << std::hex << val << std::dec << ",";
      }
      val=(int) input->readULong(2);
      MWAWColor color;
      if ((val&0xC000)==0x4000) {
        static bool first=true;
        if (first) {
          MWAW_DEBUG_MSG(("MacDrawProStyleManager::readFontStyles: find some 4000 color\n"));
          first=false;
        }
        f << "##color=" << std::hex << val << std::dec << ",";
        font.setColor(MWAWColor::white());
      }
      else if (val && getColor(val, color)) {
        f << "color=" << color << ",";
        font.setColor(color);
      }
      else if (val)
        f << "###color=" << std::hex << val << std::dec << ",";
    }
    font.m_extra=f.str();
    m_state->m_fontList.push_back(font);

    f.str("");
    f << "Entries(FontStyle):F" << j+1 << ":";
    f << font.getDebugString(m_parserState->m_fontConverter);
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
  int const vers=m_parserState->m_version;
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
  if (!inRsrc && N!=m_state->m_numStyleZones[vers==0 ? 3 : 4]) {
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
  int const vers=m_parserState->m_version;
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
  if (!inRsrc && N!=m_state->m_numStyleZones[vers==0 ? 4 : 5]) {
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
      if (k==0 && inRsrc && (solid&0x8000)) // frequent
        solid &= 0x7FFF;
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
      f << "pen=" << MWAWVec2i(dim[1],dim[0]) << ",";
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

bool MacDrawProStyleManager::readParagraphStyles(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readParagraphStyles: the entry is bad\n"));
    return false;
  }

  int const vers=m_parserState->m_version;
  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  entry.setParsed(true);
  long pos=entry.begin();
  int const expectedSize=0xee;
  if ((entry.length()%expectedSize)!=0 || vers==0) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readParagraphStyles: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }
  int N=int(entry.length()/expectedSize);
  if (N!=m_state->m_numStyleZones[3]) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readParagraphStyles: oops the number of paragraphs seems odd\n"));
  }

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int j=0; j<N; ++j) {
    MWAWParagraph para;
    pos=input->tell();
    f.str("");
    f << "Entries(Paragraph)[P" << j << "]:";
    int val=(int) input->readLong(2);
    if (val!=1) f << "numUsed=" << val << ",";
    val=(int) input->readLong(2);
    switch (val) {
    case 0: // left
      break;
    case -1:
      para.m_justify = MWAWParagraph::JustificationRight;
      f << "right,";
      break;
    case 1:
      para.m_justify = MWAWParagraph::JustificationCenter;
      f << "center,";
      break;
    case 2:
      para.m_justify = MWAWParagraph::JustificationFull;
      f << "justified,";
      break;
    default:
      f << "#align=" << val << ",";
      break;
    }
    val=(int) input->readLong(2); // always 0?
    if (val) f << "f0=" << val << ",";
    para.m_marginsUnit=librevenge::RVNG_POINT;
    for (int i=0; i<3; ++i)
      para.m_margins[i<2 ? 1-i : 2]=double(input->readLong(4))/65536.;
    para.m_margins[2] = -*para.m_margins[2];// rigth margin is defined from right border
    double spacings[3];
    for (int i=0; i<3; ++i)
      spacings[i]=double(input->readLong(4))/65536.;
    for (int i=0; i<3; ++i) {
      val=(int) input->readLong(2);
      if (spacings[i]>=0 && spacings[i]<=0) continue; // default
      if (val==-1) { // percent
        if (i==0)
          para.setInterline(1.0+spacings[i], librevenge::RVNG_PERCENT);
        else // assume line with height=12pt
          para.m_spacings[i]=spacings[i]*12./72.;
      }
      else {
        if (i==0)
          para.setInterline(spacings[i], librevenge::RVNG_POINT, MWAWParagraph::AtLeast);
        else
          para.m_spacings[i]=spacings[i]/72.;
      }
    }
    int nTabs=(int) input->readLong(2);
    if (nTabs<0||nTabs>20) {
      MWAW_DEBUG_MSG(("MacDrawProStyleManager::readParagraphStyles: oops the number of tabs seems odd\n"));
      f << "#nTabs=" << nTabs << ",";
      nTabs=0;
    }
    float leftPos=float(*para.m_margins[1])/72.f;
    for (int i=0; i<nTabs; ++i) {
      MWAWTabStop tab;
      val=(int) input->readLong(2);
      switch (val) {
      case 0: // left
        break;
      case 1:
        tab.m_alignment=MWAWTabStop::RIGHT;
        break;
      case 2:
        tab.m_alignment=MWAWTabStop::CENTER;
        break;
      case 3:
        tab.m_alignment=MWAWTabStop::DECIMAL;
        break;
      default:
        f << "#align=" << val << ",";
        break;
      }
      val=(int) input->readLong(2);
      if (val!=0x20) {
        int unicode= m_parserState->m_fontConverter->unicode(3, (unsigned char) val);
        if (unicode==-1)
          tab.m_leaderCharacter = uint16_t(val);
        else
          tab.m_leaderCharacter = uint16_t(unicode);
      }
      tab.m_position=leftPos+double(input->readLong(4))/65536./72;
      val=(int) input->readLong(2);
      if (val!=0x2e) {
        int unicode= m_parserState->m_fontConverter->unicode(3, (unsigned char) val);
        if (unicode==-1)
          tab.m_decimalCharacter = uint16_t(val);
        else
          tab.m_decimalCharacter = uint16_t(unicode);
      }
      para.m_tabs->push_back(tab);
    }
    m_state->m_paragraphList.push_back(para);
    f << para;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+expectedSize, librevenge::RVNG_SEEK_SET);
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
  for (int z = 0; z < 14; z++) {
    char const *(zNames[]) = {
      "Dinf", "Pref",
      "Ctbl", "bawP", "colP", "patR",
      "Aset", "Dset", "DRul", "Pset", "Rset", "Rst2", "Dvws",
      "Dstl"
    };
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
  /* find also
     mmpp: with 0000000000000000000000000000000000000000000000000000000000000000
     pPrf: with 0001000[01]01000002000000000000
     sPrf: spelling preference ? ie find in one file and contains strings "Main/User dictionary"...
     xPrf: with 000000010000000[13]
   */

  if (m_parserState->m_version==0) return true;

  // first read the palette
  it = entryMap.lower_bound("PaDB");
  while (it != entryMap.end()) {
    if (it->first != "PaDB")
      break;
    MWAWEntry const &entry = it++->second;
    readPaletteDef(entry);
  }

  for (int z = 0; z < 10; z++) {
    char const *(zProNames[]) = {
      "Prf1", "Prf2", "Prf3", "Prf4", "Prf5", "Prf6", "Prf8", "Prf9",
      "UPDL", "Grid"
    };
    it = entryMap.lower_bound(zProNames[z]);
    while (it != entryMap.end()) {
      if (it->first != zProNames[z])
        break;
      MWAWEntry const &entry = it++->second;
      switch (z) {
      case 0: // pref 1
        readPreferences1(entry);
        break;
      case 1: // pref 2
        readPreferencesListBool(entry,6);
        break;
      case 2: // pref 3
        readPreferencesListBool(entry,2);
        break;
      case 3: // pref 4 ( always find 0,0,0,1 : so maybe 2 int)
        readPreferencesListBool(entry,4);
        break;
      case 4: // pref 5
        readPreferencesListBool(entry,6);
        break;
      case 5: // pref 6
        readPreferences6(entry);
        break;
      case 6: // pref 8
        readPreferences8(entry);
        break;
      case 7: // pref 9 ( begins by 4 bools, after alway find 0 so...)
        readPreferencesListBool(entry,20);
        break;
      case 8: // unknown: RSRCUPDL
        readUPDL(entry);
        break;
      case 9: // unknown: id stored in RSRCUPDL
        readGrid(entry);
        break;
      default:
        MWAW_DEBUG_MSG(("MacDrawProStyleManager::readRSRCZones: called with unexpected settings %d\n", z));
        break;
      }
    }
  }

  // read the menu list
  for (int z=0; z<4; ++z) {
    char const *(menuNames[]) = { "fnt", "siz", "sty", "vue" };
    readListNames(menuNames[z]);
  }

  /* find also
     STYI with 00000006000300000000
     + some clut used to store bitmap colormap */
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
  int vers=m_parserState->m_version;
  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(DocumentInfo)[" << entry.id() << "]:";
  entry.setParsed(true);

  long pos=entry.begin();
  int const expectedSize=vers==0 ? 0x58 : 0x72;
  if (entry.length()!=expectedSize) {
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
  if (vers) {
    f << "num2?=[";
    for (int i=0; i<13; ++i) { // series of small number
      val=(int) input->readLong(2);
      if (val) f << val << ",";
      else f << "_,";
    }
    f << "],";
  }
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
  m_state->m_documentSize=MWAWVec2f(fDim[0],fDim[1]);
  f << "document[size]=" << m_state->m_documentSize << ",";
  val=(int) input->readLong(2); // 0|1
  if (val) f << "f5=" << val << ",";
  for (int i=0; i<2; ++i) fDim[i]=float(input->readLong(4))/65536.f;
  f << "pos1?=" << MWAWVec2f(fDim[0],fDim[1]) << ",";
  for (int i=0; i<4; ++i) { // always 0?
    val=(int) input->readLong(2);
    if (val)
      f << "g" << i << "=" << val << ",";
  }
  for (int i=0; i<2; ++i) fDim[i]=float(input->readLong(4))/65536.f;
  f << "page[size]=" << MWAWVec2f(fDim[1],fDim[0]) << ",";

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
  pat.m_dim=MWAWVec2i(8,8);
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
    MWAWPictBitmapIndexed bitmap(MWAWVec2i(8,8));
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
          (MWAWVec2i(8,8), binary, type,MWAWColor((unsigned char)(sumColors[0]/64),(unsigned char)(sumColors[1]/64),(unsigned char)(sumColors[2]/64)));
    }
    else {
      MWAW_DEBUG_MSG(("MacDrawProStyleManager::readColorPatterns: oops can not create the binary data\n"));
      pat.m_dim=MWAWVec2i(8,8);
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
  int const vers=m_parserState->m_version;
  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(View)[" << entry.type() << entry.id() << "]:";
  entry.setParsed(true);

  long pos=entry.begin();
  int const dataSize=vers==0 ? 8 : 12;
  if ((entry.length()%dataSize)!=0) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readViews: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  int N=int(entry.length()/dataSize);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "View[V" << i+1 << "]:";
    int val=(int) input->readULong(2);
    if (!val) {
      ascFile.addPos(pos);
      ascFile.addNote("_");
      input->seek(pos+dataSize, librevenge::RVNG_SEEK_SET);
      continue;
    }
    if (val!=1) f << "numUsed?=" << val << ",";
    val=(int) input->readULong(2); // 0 or 1
    if (val) f << "f0=" << val << ",";
    if (vers==0)
      f << "pos=" << input->readLong(2) << "x" << input->readLong(2) << ",";
    else
      f << "pos=" << double(input->readLong(4))/65536 << "x" << double(input->readLong(4))/65536 << ",";
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

// MacDraw Pro specific resources
bool MacDrawProStyleManager::readPaletteDef(MWAWEntry const &entry)
{
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPaletteDef: the entry is bad\n"));
    return false;
  }

  if (entry.id()<0 || entry.id()>3) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPaletteDef: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  std::multimap<std::string, MWAWEntry> &entryMap = m_parserState->m_rsrcParser->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::iterator it;
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;

  f << "Entries(PaletteDef)[" << entry.type() << entry.id() << "]:";
  entry.setParsed(true);
  long pos=entry.begin();
  if (entry.length()!=80) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPaletteDef: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int id=(int) input->readULong(2);
  if (id!=entry.id()) f << "#id=" << id << ",";
  int val=(int) input->readLong(2); // always 0
  if (val) f << "f0=" << val << ",";
  for (int i=0; i<2; ++i) { // almost always 0 but can be big numbers
    val=(int) input->readULong(4);
    if (val)
      f << "ID" << i << "=" << std::hex << val << std::dec << ",";
  }
  id=(int) input->readULong(2);
  if (id!=entry.id()) f << "#id2=" << id << ",";
  // first the list
  std::string name("");
  for (int i=0; i<4; ++i) name+=(char) input->readULong(1);
  int dataSz=(int) input->readULong(2);
  f << name << ",";
  if (!name.empty()) {
    it=entryMap.find(name);
    if (it!=entryMap.end()) {
      long actPos=input->tell();
      readPaletteData(it->second, dataSz);
      input->seek(actPos, librevenge::RVNG_SEEK_SET);
    }
    // it==entry.map is ok has default palette are not stored
  }
  for (int i=0; i<6; ++i) { // f3=f5=id, other 0
    val=(int) input->readULong(2);
    if (i==3 || i==5) {
      if (val==id) continue;
    }
    else if (!val) continue;
    f << "f" << i+1 << "=" << val << ",";
  }
  val=(int) input->readULong(4); // another big number
  if (val)
    f << "ID2=" << std::hex << val << std::dec << ",";
  for (int i=0; i<6; ++i) { // always 0?
    val=(int) input->readULong(2);
    if (val) f << "f" << i+7 << "=" << val << ",";
  }

  // now the map
  name.clear();
  for (int i=0; i<4; ++i) name+=(char) input->readULong(1);
  dataSz=(int) input->readULong(2);
  int N=(int) input->readULong(2);
  f << name << "[" << N << "],";
  if (!name.empty()) {
    it=entryMap.find(name);
    if (it!=entryMap.end()) {
      long actPos=input->tell();
      readPaletteMap(it->second, N, dataSz);
      input->seek(actPos, librevenge::RVNG_SEEK_SET);
    }
    else if (N) {
      MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPaletteDef: can not find the %s map\n", name.c_str()));
      f << "###";
    }
  }

  for (int i=0; i<2; ++i) { // g1=id, other 0
    val=(int) input->readULong(2);
    if (i==1) {
      if (val==id) continue;
    }
    else if (!val) continue;
    f << "g" << i << "=" << val << ",";
  }
  // the names list
  name.clear();
  for (int i=0; i<4; ++i) name+=(char) input->readULong(1);
  f << name << ",";
  if (!name.empty()) {
    it=entryMap.find(name);
    if (it!=entryMap.end()) {
      long actPos=input->tell();
      readListNames(it->second);
      input->seek(actPos, librevenge::RVNG_SEEK_SET);
    }
    // ok if default
  }
  for (int i=0; i<2; ++i) { // id=0, g2-3=[b,e], id=1-2, g2-3=[e,20], id=3, g2-3=[10,20]
    val=(int) input->readULong(2);
    if (val)
      f << "g" << i+2 << "=" << val << ",";
  }
  val=(int) input->readULong(4); // another big number
  if (val)
    f << "ID3=" << std::hex << val << std::dec << ",";
  for (int i=0; i<4; ++i) { // always 0
    val=(int) input->readULong(2);
    if (val)
      f << "g" << i+5 << "=" << val << ",";
  }

  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool MacDrawProStyleManager::readPaletteData(MWAWEntry const &entry, int dataSz)
{
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPaletteData: the entry is bad\n"));
    return false;
  }
  if (entry.id()!=128) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPaletteData: the entry id is odd\n"));
  }

  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  entry.setParsed(true);

  if (entry.type()=="CoEL")
    return readColorPalette(entry, dataSz);
  if (entry.type()=="PaEL")
    return readPatternPalette(entry, dataSz);
  if (entry.type()=="RaEL")
    return readGradientPalette(entry, dataSz);
  if (entry.type()=="FaEL")
    return readFAPalette(entry, dataSz);

  long pos=entry.begin();
  libmwaw::DebugStream f;
  f << "Entries(" << entry.type() << ")[" << entry.type() << entry.id() << "]:";
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (!dataSz || entry.length()!=2+dataSz*N) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPaletteData: can not compute the number of entry\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPaletteData: find some unknown entry %s\n", entry.type().c_str()));
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << entry.type() << "-" << i << ":";
    input->seek(pos+dataSz, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool MacDrawProStyleManager::readPaletteMap(MWAWEntry const &entry, int N, int dataSz)
{
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPaletteMap: the entry is bad\n"));
    return false;
  }
  if (entry.id()<0 || entry.id()>3) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPaletteMap: the entry id is odd\n"));
  }

  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  entry.setParsed(true);

  long pos=entry.begin();
  if (entry.length()==10) { // no data
    ascFile.addPos(pos-4);
    ascFile.addNote("_");
    return true;
  }

  if (entry.type()=="DPCo")
    return readColorMap(entry, N, dataSz);
  if (entry.type()=="DPPa")
    return readPatternMap(entry, N, dataSz);
  if (entry.type()=="DPRa")
    return readGradientMap(entry, N, dataSz);
  if (entry.type()=="DPFa")
    return readFAMap(entry, N, dataSz);

  libmwaw::DebugStream f;
  f << "Entries(" << entry.type() << ")[" << entry.type() << entry.id() << "]:";
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  if (!dataSz || entry.length()!=dataSz*N) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPaletteMap: can not compute the number of entry\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPaletteMap: find some unknown entry %s\n", entry.type().c_str()));
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << entry.type() << "-" << i << ":";
    input->seek(pos+dataSz, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool MacDrawProStyleManager::readColorMap(MWAWEntry const &entry, int N, int fieldSize)
{
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readColorMap: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=0) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readColorMap: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(ColorMap)[" << entry.type() << entry.id() << "]:";
  entry.setParsed(true);

  long pos=entry.begin();
  if (fieldSize < 20 || entry.length() != N*fieldSize) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readColorMap: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  m_state->m_displayColorList.clear();
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "ColorMap-" << i << "]:";
    int val=(int) input->readLong(2); // always 0
    if (val) f << "f0=" << val << ",";
    val=(int) input->readLong(2);
    if (val!=1) f << "numUsed=" << val << ",";
    for (int j=0; j<2; ++j) { // always 0
      val=(int) input->readLong(2);
      if (val) f << "f" << j+1 << "=" << val << ",";
    }
    int type=(int) input->readULong(2);
    unsigned char col[4];
    for (int j=0; j<4; ++j) col[j]=(unsigned char)(input->readULong(2)>>8);
    MWAWColor color;
    switch (type&3) {
    case 1: // checkme
      f << "rgb,";
      color=MWAWColor(col[0],col[1],col[2]);
      break;
    case 2:
      color=MWAWColor::colorFromCMYK(col[0],col[1],col[2],col[3]);
      break;
    case 3:
      color=MWAWColor::colorFromHSL(col[0],col[1],col[2]);
      break;
    default:
      f << "type" << (type&3) << ",";
      color=MWAWColor(col[0],col[1],col[2]);
      break;
    }
    m_state->m_displayColorList.push_back(color);
    f << color << ",";
    val=(int) input->readLong(2);
    if (val!=-1) f << "id=" << val << ",";
    input->seek(pos+fieldSize, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool MacDrawProStyleManager::readColorPalette(MWAWEntry const &entry, int fieldSize)
{
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readColorPalette: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=128) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readColorPalette: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(ColorList)[" << entry.type() << entry.id() << "]:";
  entry.setParsed(true);

  long pos=entry.begin();
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (entry.length()!=N*fieldSize+2 || fieldSize<16) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readColorPalette: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "ColorList-" << i << "]:";
    for (int j=0; j<2; ++j) {
      int val=(int) input->readLong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }
    MWAWColor color;
    int type=(int) input->readULong(2);
    unsigned char col[4];
    for (int j=0; j<4; ++j) col[j]=(unsigned char)(input->readULong(2)>>8);
    switch (type&3) {
    case 1: // checkme
      f << "rgb,";
      color=MWAWColor(col[0],col[1],col[2]);
      break;
    case 2:
      color=MWAWColor::colorFromCMYK(col[0],col[1],col[2],col[3]);
      break;
    case 3:
      color=MWAWColor::colorFromHSL(col[0],col[1],col[2]);
      break;
    default:
      MWAW_DEBUG_MSG(("MacDrawProStyleManager::readColorPalette: find unknown color type\n"));
      f << "type" << (type&3) << ",";
      color=MWAWColor(col[0],col[1],col[2]);
      break;
    }
    f << color << ",";
    if (type&0xfc) f << "type[high]=" << (type>>2) << ",";
    input->seek(pos+fieldSize, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool MacDrawProStyleManager::readPatternMap(MWAWEntry const &entry, int N, int fieldSize)
{
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPatternMap: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=1) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPatternMap: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(PatternMap)[" << entry.type() << entry.id() << "]:";
  entry.setParsed(true);
  long pos=entry.begin();
  if (fieldSize<18 || entry.length()!=N*fieldSize) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPatternMap: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  m_state->m_BWPatternList.clear();
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "PatternMap-" << i << "]:";
    int val=(int) input->readLong(2); // always 0
    if (val) f << "f0=" << val << ",";
    val=(int) input->readLong(2);
    if (val!=1) f << "numUsed=" << val << ",";
    for (int j=0; j<3; ++j) { // always 0
      val=(int) input->readLong(2);
      if (val) f << "f" << j+1 << "=" << val << ",";
    }
    MWAWGraphicStyle::Pattern pat;
    pat.m_dim=MWAWVec2i(8,8);
    pat.m_data.resize(8);
    pat.m_colors[0]=MWAWColor::white();
    pat.m_colors[1]=MWAWColor::black();
    for (size_t j=0; j<8; ++j) pat.m_data[j]=(uint8_t) input->readULong(1);
    f << pat << ",";
    m_state->m_BWPatternList.push_back(pat);

    input->seek(pos+fieldSize, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool MacDrawProStyleManager::readPatternPalette(MWAWEntry const &entry, int fieldSize)
{
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPatternPalette: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=128) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPatternPalette: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(PatternPalette)[" << entry.type() << entry.id() << "]:";
  entry.setParsed(true);

  long pos=entry.begin();
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (entry.length()!=N*fieldSize+2 || fieldSize<14) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPatternPalette: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "PatternPalette-" << i << "]:";
    for (int j=0; j<2; ++j) {
      int val=(int) input->readLong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }
    int type=(int) input->readULong(2); // find 20 for 0 and 1 type
    if (type) f  << "type=" << std::hex << type << std::dec << ",";
    f << "pat=[" << std::hex;
    for (int j=0; j<8; ++j)
      f << input->readULong(1) << ",";
    f << std::dec << "],";
    input->seek(pos+fieldSize, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool MacDrawProStyleManager::readFAMap(MWAWEntry const &entry, int N, int fieldSize)
{
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readFAMap: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=3) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readFAMap: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(FAMap)[" << entry.type() << entry.id() << "]:";
  entry.setParsed(true);
  long pos=entry.begin();
  if (fieldSize<54 || entry.length()!=N*fieldSize) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readFAMap: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  m_state->m_BWPatternList.clear();
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "FAMap-" << i << "]:";
    int val=(int) input->readLong(2); // always 0
    if (val) f << "f0=" << val << ",";
    val=(int) input->readLong(2);
    if (val!=1) f << "numUsed=" << val << ",";

    input->seek(pos+fieldSize, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool MacDrawProStyleManager::readFAPalette(MWAWEntry const &entry, int fieldSize)
{
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readFAPalette: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=128) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readFAPalette: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(FAList)[" << entry.type() << entry.id() << "]:";
  entry.setParsed(true);

  long pos=entry.begin();
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (entry.length()!=N*fieldSize+2 || fieldSize<90) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readFAPalette: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "FAList-" << i << "]:";
    for (int j=0; j<5; ++j) { // always 0
      int val=(int) input->readLong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }
    ascFile.addDelimiter(input->tell(),'|');
    input->seek(pos+fieldSize, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool MacDrawProStyleManager::readGradientMap(MWAWEntry const &entry, int N, int fieldSize)
{
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readGradientMap: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=2) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readGradientMap: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(GradientMap)[" << entry.type() << entry.id() << "]:";
  entry.setParsed(true);
  long pos=entry.begin();
  if (fieldSize<56 || entry.length()!=N*56) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readGradientMap: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  m_state->m_gradientList.clear();
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<N; ++i) {
    MacDrawProStyleManagerInternal::Gradient gradient;
    pos=input->tell();
    f.str("");
    int val=(int) input->readLong(2); // always 0
    if (val) f << "f0=" << val << ",";
    val=(int) input->readLong(2);
    if (val!=1) f << "numUsed=" << val << ",";
    for (int j=0; j<2; ++j) { // always 0
      val=(int) input->readLong(2);
      if (val) f << "f" << j+1 << "=" << val << ",";
    }
    int type=(int) input->readLong(2);
    float decal=0;
    switch (type) {
    case 0:
      gradient.m_type = MWAWGraphicStyle::G_Linear;
      decal=float(input->readLong(4))/65536.f;
      if (decal>0.5f-1e-3f && decal < 0.5f+1e-3) {
        decal=0;
        gradient.m_type = MWAWGraphicStyle::G_Axial;
      }
      f << "decal=" << decal << ",";
      break;
    case 1:
      gradient.m_type = MWAWGraphicStyle::G_Radial;
    // fall through expected
    case 2: {
      if (type==2) gradient.m_type = MWAWGraphicStyle::G_Rectangular;
      int dim[4]; // square which defined the center rectangle
      for (int j=0; j<4; ++j) dim[j]=(int) input->readULong(1);
      gradient.m_percentCenter=MWAWVec2f(float(dim[1]+dim[3])/200.f, float(dim[0]+dim[2])/200.f);
      break;
    }
    default:
      f << "#type=" << type << ",";
      break;
    }
    ascFile.addDelimiter(input->tell(),'|');
    input->seek(pos+16, librevenge::RVNG_SEEK_SET);
    ascFile.addDelimiter(input->tell(),'|');
    MWAWColor colors[4];
    for (int j=0; j<4; ++j) {
      unsigned char col[4];
      for (int k=0; k<4; ++k) col[k]=(unsigned char)(input->readULong(2)>>8);
      colors[j]=MWAWColor(col[0],col[1],col[2]);
    }
    int which=(int) input->readULong(2);
    std::vector<MWAWColor> listColor;
    f << "col=[";
    if (which&0x8000) listColor.push_back(colors[0]);
    if (which&0x4000) listColor.push_back(colors[1]);
    if (which&0x2000) listColor.push_back(colors[2]);
    if (which&0x1000) listColor.push_back(colors[3]);
    f << "],";
    size_t numColors=listColor.size();
    if ((type==1 || type==2) || decal>1.f-1e-3f) { // reverse color
      for (size_t j=0; j<numColors/2; ++j) {
        MWAWColor tmp=listColor[j];
        listColor[j]=listColor[numColors-1-j];
        listColor[numColors-1-j]=tmp;
      }
      decal=0;
    }
    if (decal>1e-3f && numColors>1) {
      float step=decal/float(numColors-1);
      float gradPos=0;
      for (size_t j=numColors-1; j>0; --j, gradPos+=step)
        gradient.m_stopList.push_back(MWAWGraphicStyle::GradientStop(gradPos, listColor[j]));
    }
    float step=numColors>1 ? (1.f-decal)/float(numColors-1) : (1.f-decal);
    float gradPos=decal;
    for (size_t j=0; j<numColors; ++j, gradPos+=step)
      gradient.m_stopList.push_back(MWAWGraphicStyle::GradientStop(gradPos, listColor[j]));
    gradient.m_angle=90.f+float(which&0xFFF);
    if (which&0xFFF) f << "angle=" << (which&0xFFF) << ",";
    for (int j=0; j<3; ++j) { // always 0?
      val=(int) input->readLong(2);
      if (val) f << "g" << j << "=" << val << ",";
    }
    gradient.m_extra=f.str();
    m_state->m_gradientList.push_back(gradient);

    f.str("");
    f << "GradientMap-" << i << "]:" << gradient;

    input->seek(pos+fieldSize, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool MacDrawProStyleManager::readGradientPalette(MWAWEntry const &entry, int fieldSize)
{
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readGradientPalette: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=128) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readGradientPalette: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(GradientList)[" << entry.type() << entry.id() << "]:";
  entry.setParsed(true);

  long pos=entry.begin();
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (entry.length()!=N*fieldSize+2 || fieldSize<52) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readGradientPalette: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "GradientList-" << i << "]:";
    for (int j=0; j<2; ++j) { // always 0
      int val=(int) input->readLong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }
    int type=(int) input->readLong(2);
    switch (type) {
    case 0:
      f << "directional,";
      f << "decal=" << float(input->readLong(4))/65536.f << ",";
      break;
    case 1:
      f << "concentric,";
    // fall through expected
    case 2: {
      if (type==2) f << "radial,";
      int dim[4]; // square which defined the center rectangle
      for (int j=0; j<4; ++j) dim[j]=(int) input->readULong(1);
      f << "center=" << float(dim[1]+dim[3])/200.f << "x" << float(dim[0]+dim[2])/200.f << ",";
      break;
    }
    default:
      f << "#type=" << type << ",";
      break;
    }
    ascFile.addDelimiter(input->tell(),'|');
    input->seek(pos+12, librevenge::RVNG_SEEK_SET);
    ascFile.addDelimiter(input->tell(),'|');
    MWAWColor colors[4];
    for (int j=0; j<4; ++j) {
      unsigned char col[4];
      for (int k=0; k<4; ++k) col[k]=(unsigned char)(input->readULong(2)>>8);
      colors[j]=MWAWColor::colorFromCMYK(col[0],col[1],col[2],col[3]);
    }
    int which=(int) input->readULong(2);
    f << "col=[";
    if (which&0x8000) f << colors[0] << ",";
    if (which&0x4000) f << colors[1] << ",";
    if (which&0x2000) f << colors[2] << ",";
    if (which&0x1000) f << colors[3] << ",";
    f << "],";
    if (which&0xFFF) f << "angle=" << (which&0xFFF) << ",";
    for (int j=0; j<3; ++j) { // always 0?
      int val=(int) input->readLong(2);
      if (val) f << "g" << j << "=" << val << ",";
    }
    input->seek(pos+fieldSize, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool MacDrawProStyleManager::readListNames(char const *type)
{
  if (!type || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readListNames: the type is bad\n"));
    return false;
  }

  std::string dataDef(type);
  dataDef+="L";

  std::multimap<std::string, MWAWEntry> &entryMap = m_parserState->m_rsrcParser->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::iterator it= entryMap.find(dataDef);
  if (it == entryMap.end())
    return true;
  MWAWEntry &entry=it->second;
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readListNames: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=512) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readListNames: the entry id is odd\n"));
  }

  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(LNames)[" << entry.type() << entry.id() << "]:";
  entry.setParsed(true);
  long pos=entry.begin();
  input->seek(pos, librevenge::RVNG_SEEK_SET);

  if (entry.length()!=8) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readListNames: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }

  f << "dSz=" << input->readULong(2) << ",";
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  f << "rsId[lAttr]=" << std::hex << input->readULong(4) << ","; // last field in the RSRCMap's mainType
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());

  std::string dataName(type);
  dataName+="D";
  it= entryMap.find(dataName);
  if (it!=entryMap.end())
    readListNames(it->second, N);
  else {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readListNames: can not find main type=%s\n", type));
  }

  return true;
}

bool MacDrawProStyleManager::readListNames(MWAWEntry const &entry, int N)
{
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readListNames: the entry is bad\n"));
    return false;
  }

  if (entry.id()!=128 && entry.id()!=512) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readListNames: the entry id is odd\n"));
  }
  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  std::string fName(N<0 ? "ListNames" : "LNames");
  f << "Entries(" << fName << ")[" << entry.type() << entry.id() << "]:";
  entry.setParsed(true);

  long pos=entry.begin();
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  if (N<0) {
    N=(int) input->readULong(2);
    f << "N=" << N << ",";
  }
  if (entry.length()<2+N) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readListNames: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());

  long const endPos=entry.end();
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    int dSz=(int) input->readULong(1);

    f.str("");
    f << fName << "-" << i << ":";
    if (pos+dSz+1 > endPos) {
      MWAW_DEBUG_MSG(("MacDrawProStyleManager::readListNames: the data size seems bad\n"));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    std::string name("");
    for (int c=0; c<dSz; ++c) name+=(char) input->readULong(1);
    f << name << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  pos=input->tell();
  if (pos!=endPos) { // extra space can be reserved here
    ascFile.addPos(pos);
    ascFile.addNote("ListNames-end:#");
  }
  return true;
}

bool MacDrawProStyleManager::readUPDL(MWAWEntry const &entry)
{
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readUPDL: the entry is bad\n"));
    return false;
  }

  if (entry.id()<0 || entry.id()>3) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readUPDL: the entry id is odd\n"));
  }

  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(UPDL)[" << entry.type() << entry.id() << "]:";
  entry.setParsed(true);

  long pos=entry.begin();
  if (entry.length()!=0x2e) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readUPDL: the entry seems too short\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int val=(int) input->readLong(2); // always 1
  if (val!=1) f << "f0=" << val << ",";
  val=(int) input->readLong(2);
  f << "grid[num/resId]=" << val << ",";
  f << "ID1=" << std::hex << input->readULong(4) << std::dec << ","; // big number
  // some number that are also present in the grid header
  for (int i=0; i<8; ++i) {
    val=(int) input->readLong(2);
    if (val) f << "f" << i+1 << "=" << val << ",";
  }
  // for other iD?
  for (int i=0; i<3; ++i) {
    val=(int) input->readULong(4);
    if (val) f << "ID" << i+1 << "=" << std::hex << val << std::dec << ",";
  }
  for (int i=0; i<5; ++i) { // g1=g2=[0|-1], other 0
    val=(int) input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool MacDrawProStyleManager::readGrid(MWAWEntry const &entry)
{
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readGrid: the entry is bad\n"));
    return false;
  }

  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(Grid)[" << entry.type() << entry.id() << "]:";
  entry.setParsed(true);

  long pos=entry.begin();
  if (entry.length()<22) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readGrid: the entry seems too short\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (entry.length()!=N*14+22) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readGrid: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  input->seek(pos+22, librevenge::RVNG_SEEK_SET);
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "Grid-" << i << "]:";
    int val=(int) input->readLong(2); // always 0
    if (val) f << "f0=" << val << ",";
    f << "ids=["; // some ids, [i, ?, next]
    for (int j=0; j<3; ++j) f << input->readULong(2) << ",";
    f << "],";
    f << "row=" << input->readLong(2) << ",";
    f << "x=" << double(input->readLong(4))/65536. << ",";

    input->seek(pos+14, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

// preferences
bool MacDrawProStyleManager::readPreferencesListBool(MWAWEntry const &entry, int num)
{
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPreferences2: the entry is bad\n"));
    return false;
  }

  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(" << entry.type() << ")[" << entry.id() << "]:";
  entry.setParsed(true);
  if (entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPreferences2: the entry id is odd\n"));
  }

  long pos=entry.begin();
  if (entry.length()!=num) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPreferences2: the entry seems too short\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<num; ++i) {
    int val=(int) input->readLong(1);
    if (val==1) f << "fl" << i << ",";
    else if (val) f << "fl" << i << "=" << val << ",";
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool MacDrawProStyleManager::readPreferences1(MWAWEntry const &entry)
{
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPreferences1: the entry is bad\n"));
    return false;
  }

  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(Prf1)[" << entry.type() << ":" << entry.id() << "]:";
  entry.setParsed(true);
  if (entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPreferences1: the entry id is odd\n"));
  }

  long pos=entry.begin();
  if (entry.length()!=0x2e) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPreferences1: the entry seems too short\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int val=(int) input->readLong(2); // prefId?
  if (val!=1) f << "#id[pref]?=" << val << ",";
  for (int i=0; i<2; ++i) { // always 0
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  for (int i=0; i<4; ++i) { // dim3 often empty
    int dim[4];
    for (int j=0; j<4; ++j) dim[j]=(int) input->readLong(2);
    if (dim[0]||dim[1]||dim[2]||dim[3])
      f << "dim" << i << "=" << MWAWBox2i(MWAWVec2i(dim[1],dim[0]),MWAWVec2i(dim[3],dim[2])) << ",";
  }
  for (int i=0; i<4; ++i) { // always 0 expected f5=2
    val=(int) input->readLong(2);
    if (val) f << "f" << i+2 << "=" << val << ",";
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool MacDrawProStyleManager::readPreferences6(MWAWEntry const &entry)
{
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPreferences6: the entry is bad\n"));
    return false;
  }

  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(PrfSpelling)[" << entry.type() << ":" << entry.id() << "]:";
  entry.setParsed(true);
  if (entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPreferences6: the entry id is odd\n"));
  }

  long pos=entry.begin();
  if (entry.length()!=0x92) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPreferences6: the entry seems too short\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<4; ++i) { // always -1
    int val=(int) input->readLong(2);
    if (val!=-1)
      f << "f" << i << "=" << val << ",";
  }
  for (int i=0; i<2; ++i) { // main and user dictionary name
    int sSz=(int) input->readULong(1);
    if (sSz>63) {
      MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPreferences6: can not read some size string\n"));
      f << "###sSz" << i << "=" << sSz << ",";
    }
    else {
      std::string name("");
      for (int c=0; c<sSz; ++c) name+=(char) input->readLong(1);
      f << name << ",";
    }
    input->seek(pos+8+64*(i+1), librevenge::RVNG_SEEK_SET);
  }
  for (int i=0; i<5; ++i) {
    int val=(int) input->readLong(2);
    static int const(expected[])= {-999,-999,1,0,0};
    if (val!=expected[i])
      f << "g" << i << "=" << val << ",";
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool MacDrawProStyleManager::readPreferences8(MWAWEntry const &entry)
{
  if (!entry.valid() || !m_parserState->m_rsrcParser) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPreferences8: the entry is bad\n"));
    return false;
  }

  MWAWInputStreamPtr input = m_parserState->m_rsrcParser->getInput();
  libmwaw::DebugFile &ascFile = m_parserState->m_rsrcParser->ascii();
  libmwaw::DebugStream f;
  f << "Entries(Prf8)[" << entry.type() << ":" << entry.id() << "]:";
  entry.setParsed(true);
  if (entry.id()!=256) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPreferences8: the entry id is odd\n"));
  }

  long pos=entry.begin();
  if (entry.length()!=40) {
    MWAW_DEBUG_MSG(("MacDrawProStyleManager::readPreferences8: the entry seems too short\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<10; ++i) { // always 1 excepted f0 in 0.5 .. 1.23
    int val=(int) input->readLong(4);
    if (val==0x10000) continue;
    if (val) f << "f" << i << "=" << double(val)/65536. << ",";
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
