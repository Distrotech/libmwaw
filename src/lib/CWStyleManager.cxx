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
////////////////////////////////////////
//! Internal: the pattern of a CWStyleManager
struct Pattern : public MWAWGraphicStyle::Pattern {
  //! constructor ( 4 int by patterns )
  Pattern(uint16_t const *pat=0) : MWAWGraphicStyle::Pattern(), m_percent(0) {
    if (!pat) return;
    m_colors[0]=MWAWColor::white();
    m_colors[1]=MWAWColor::black();
    m_dim=Vec2i(8,8);
    m_data.resize(8);
    for (size_t i=0; i < 4; ++i) {
      uint16_t val=pat[i];
      m_data[2*i]=(unsigned char) (val>>8);
      m_data[2*i+1]=(unsigned char) (val&0xFF);
    }
    int numOnes=0;
    for (size_t j=0; j < 8; ++j) {
      uint8_t val=(uint8_t) m_data[j];
      for (int b=0; b < 8; b++) {
        if (val&1) ++numOnes;
        val = uint8_t(val>>1);
      }
    }
    m_percent=float(numOnes)/64.f;
  }
  //! the percentage
  float m_percent;
};

////////////////////////////////////////
//! Internal: the gradient of a CWStyleManager
struct Gradient {
  //! construtor
  Gradient(int type=0, int nColor=0, int angle=0, float decal=0) :
    m_type(type), m_numColors(nColor), m_angle(angle), m_decal(decal), m_box() {
    m_colors[0]=MWAWColor::black();
    m_colors[1]=MWAWColor::white();
  }
  //! check if the gradient is valid
  bool ok() const {
    return m_type>=0 && m_type<=2 && m_numColors>=2 && m_numColors<=4;
  }
  //! update the style
  bool update(MWAWGraphicStyle &style) const;
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Gradient const &gr) {
    switch(gr.m_type) {
    case 0:
      o << "linear,";
      break;
    case 1:
      o << "radial,";
      break;
    case 2:
      o << "rectangle,";
      break;
    default:
      o << "#type=" << gr.m_type << ",";
      break;
    }
    if (gr.m_angle)
      o << "angle=" << gr.m_angle << ",";
    if (gr.m_decal>0)
      o << "decal=" << gr.m_decal << ",";
    o << "col=[";
    for (int c=0; c < gr.m_numColors; ++c) {
      if (c>=4) break;
      o << gr.m_colors[c] << ",";
    }
    o << "],";
    if (gr.m_box!=Box2i(Vec2i(0,0),Vec2i(0,0)))
      o << "center=" << gr.m_box << ",";
    return o;
  }
  //! the type
  int m_type;
  //! the number of color
  int m_numColors;
  //! the color
  MWAWColor m_colors[4];
  //! the angle
  int m_angle;
  //! the decal
  float m_decal;
  //! the center bdbox
  Box2i m_box;
};

bool Gradient::update(MWAWGraphicStyle &style) const
{
  if (!ok()) return false;
  style.m_gradientStopList.resize(0);
  if (m_type==1 || m_type==2) {
    style.m_gradientType=m_type==1 ? MWAWGraphicStyle::G_Radial : MWAWGraphicStyle::G_Rectangular;
    for (int c=0; c < m_numColors; ++c)
      style.m_gradientStopList.push_back(MWAWGraphicStyle::GradientStop(float(c)/float(m_numColors-1), m_colors[m_numColors-1-c]));
    style.m_gradientPercentCenter=Vec2f(float(m_box.center()[1])/100.f, float(m_box.center()[0])/100.f);
    return true;
  }
  style.m_gradientAngle=float(m_angle+90);
  if (m_decal<=0.05f) {
    style.m_gradientType= MWAWGraphicStyle::G_Axial;
    for (int c=0; c < m_numColors; ++c)
      style.m_gradientStopList.push_back(MWAWGraphicStyle::GradientStop(float(c)/float(m_numColors-1), m_colors[m_numColors-1-c]));
    return true;
  }
  style.m_gradientType= MWAWGraphicStyle::G_Linear;
  if (m_decal >=0.95f)  {
    for (int c=0; c < m_numColors; ++c)
      style.m_gradientStopList.push_back(MWAWGraphicStyle::GradientStop(float(c)/float(m_numColors-1), m_colors[c]));
    return true;
  }
  for (int c=-m_numColors+1; c<m_numColors; ++c) {
    // checkme: look almost good
    float pos=float(c)/float(m_numColors-1)+(1-m_decal)/2.f;
    if (pos < 0) {
      if (c!=m_numColors-1 && float(c+1)/float(m_numColors-1)+(1-m_decal)/2.f>=0)
        continue;
      pos=0;
    }
    style.m_gradientStopList.push_back(MWAWGraphicStyle::GradientStop(pos>1?1:pos, m_colors[c<0?-c:c]));
    if (pos>=1)
      break;
  }
  return true;
}

//! Internal: the state of a CWStyleManager
struct State {
  //! constructor
  State() : m_version(-1), m_localFIdMap(), m_stylesMap(), m_lookupMap(), m_graphList(), m_ksenList(),
    m_colorList(), m_patternList(), m_gradientList() {
  }
  //! set the default color map
  void setDefaultColorList(int version);
  //! set the default pattern map
  void setDefaultPatternList(int version);
  //! set the default pattern map
  void setDefaultGradientList(int version);
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
  std::vector<MWAWGraphicStyle> m_graphList;
  //! the KSEN list
  std::vector<CWStyleManager::KSEN> m_ksenList;

  //! a list colorId -> color
  std::vector<MWAWColor> m_colorList;
  //! a list patternId -> pattern
  std::vector<Pattern> m_patternList;
  //! a list gradientId -> gradient
  std::vector<Gradient> m_gradientList;
};


void State::setDefaultColorList(int version)
{
  if (m_colorList.size()) return;
  if (version==1) {
    uint32_t const defCol[81] = {
      0xffffff,0x000000,0x222222,0x444444,0x555555,0x888888,0xbbbbbb,0xdddddd,
      0xeeeeee,0x440000,0x663300,0x996600,0x002200,0x003333,0x003399,0x000055,
      0x330066,0x660066,0x770000,0x993300,0xcc9900,0x004400,0x336666,0x0033ff,
      0x000077,0x660099,0x990066,0xaa0000,0xcc3300,0xffcc00,0x006600,0x006666,
      0x0066ff,0x0000aa,0x663399,0xcc0099,0xdd0000,0xff3300,0xffff00,0x008800,
      0x009999,0x0099ff,0x0000dd,0x9900cc,0xff0099,0xff3333,0xff6600,0xffff33,
      0x00ee00,0x00cccc,0x00ccff,0x3366ff,0x9933ff,0xff33cc,0xff6666,0xff6633,
      0xffff66,0x66ff66,0x66cccc,0x66ffff,0x3399ff,0x9966ff,0xff66ff,0xff9999,
      0xff9966,0xffff99,0x99ff99,0x66ffcc,0x99ffff,0x66ccff,0x9999ff,0xff99ff,
      0xffcccc,0xffcc99,0xffffcc,0xccffcc,0x99ffcc,0xccffff,0x99ccff,0xccccff,
      0xffccff
    };
    m_colorList.resize(81);
    for (size_t i = 0; i < 81; i++)
      m_colorList[i] = defCol[i];
  } else {
    uint32_t const defCol[256] = {
      0xffffff,0x0,0x777777,0x555555,0xffff00,0xff6600,0xdd0000,0xff0099,
      0x660099,0xdd,0x99ff,0xee00,0x6600,0x663300,0x996633,0xbbbbbb,
      0xffffcc,0xffff99,0xffff66,0xffff33,0xffccff,0xffcccc,0xffcc99,0xffcc66,
      0xffcc33,0xffcc00,0xff99ff,0xff99cc,0xff9999,0xff9966,0xff9933,0xff9900,
      0xff66ff,0xff66cc,0xff6699,0xff6666,0xff6633,0xff33ff,0xff33cc,0xff3399,
      0xff3366,0xff3333,0xff3300,0xff00ff,0xff00cc,0xff0066,0xff0033,0xff0000,
      0xccffff,0xccffcc,0xccff99,0xccff66,0xccff33,0xccff00,0xccccff,0xcccccc,
      0xcccc99,0xcccc66,0xcccc33,0xcccc00,0xcc99ff,0xcc99cc,0xcc9999,0xcc9966,
      0xcc9933,0xcc9900,0xcc66ff,0xcc66cc,0xcc6699,0xcc6666,0xcc6633,0xcc6600,
      0xcc33ff,0xcc33cc,0xcc3399,0xcc3366,0xcc3333,0xcc3300,0xcc00ff,0xcc00cc,
      0xcc0099,0xcc0066,0xcc0033,0xcc0000,0x99ffff,0x99ffcc,0x99ff99,0x99ff66,
      0x99ff33,0x99ff00,0x99ccff,0x99cccc,0x99cc99,0x99cc66,0x99cc33,0x99cc00,
      0x9999ff,0x9999cc,0x999999,0x999966,0x999933,0x999900,0x9966ff,0x9966cc,
      0x996699,0x996666,0x996600,0x9933ff,0x9933cc,0x993399,0x993366,0x993333,
      0x993300,0x9900ff,0x9900cc,0x990099,0x990066,0x990033,0x990000,0x66ffff,
      0x66ffcc,0x66ff99,0x66ff66,0x66ff33,0x66ff00,0x66ccff,0x66cccc,0x66cc99,
      0x66cc66,0x66cc33,0x66cc00,0x6699ff,0x6699cc,0x669999,0x669966,0x669933,
      0x669900,0x6666ff,0x6666cc,0x666699,0x666666,0x666633,0x666600,0x6633ff,
      0x6633cc,0x663399,0x663366,0x663333,0x6600ff,0x6600cc,0x660066,0x660033,
      0x660000,0x33ffff,0x33ffcc,0x33ff99,0x33ff66,0x33ff33,0x33ff00,0x33ccff,
      0x33cccc,0x33cc99,0x33cc66,0x33cc33,0x33cc00,0x3399ff,0x3399cc,0x339999,
      0x339966,0x339933,0x339900,0x3366ff,0x3366cc,0x336699,0x336666,0x336633,
      0x336600,0x3333ff,0x3333cc,0x333399,0x333366,0x333333,0x333300,0x3300ff,
      0x3300cc,0x330099,0x330066,0x330033,0x330000,0xffff,0xffcc,0xff99,
      0xff66,0xff33,0xff00,0xccff,0xcccc,0xcc99,0xcc66,0xcc33,
      0xcc00,0x99cc,0x9999,0x9966,0x9933,0x9900,0x66ff,0x66cc,
      0x6699,0x6666,0x6633,0x33ff,0x33cc,0x3399,0x3366,0x3333,
      0x3300,0xff,0xcc,0x99,0x66,0x33,0xdd0000,0xbb0000,
      0xaa0000,0x880000,0x770000,0x550000,0x440000,0x220000,0x110000,0xdd00,
      0xbb00,0xaa00,0x8800,0x7700,0x5500,0x4400,0x2200,0x1100,
      0xee,0xbb,0xaa,0x88,0x77,0x55,0x44,0x22,
      0x11,0xeeeeee,0xdddddd,0xaaaaaa,0x888888,0x444444,0x222222,0x111111,
    };
    m_colorList.resize(256);
    for (size_t i = 0; i < 256; i++)
      m_colorList[i] = defCol[i];
  }
}

void State::setDefaultPatternList(int)
{
  if (m_patternList.size()) return;
  static uint16_t const (s_pattern[4*64]) = {
    0x0000, 0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0x7fff, 0xffff, 0xf7ff, 0xffff, 0x7fff, 0xf7ff, 0x7fff, 0xf7ff,
    0xffee, 0xffbb, 0xffee, 0xffbb, 0x77dd, 0x77dd, 0x77dd, 0x77dd, 0xaa55, 0xaa55, 0xaa55, 0xaa55, 0x8822, 0x8822, 0x8822, 0x8822,
    0xaa00, 0xaa00, 0xaa00, 0xaa00, 0xaa00, 0x4400, 0xaa00, 0x1100, 0x8800, 0xaa00, 0x8800, 0xaa00, 0x8800, 0x2200, 0x8800, 0x2200,
    0x8000, 0x0800, 0x8000, 0x0800, 0x8800, 0x0000, 0x8800, 0x0000, 0x8000, 0x0000, 0x0800, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001,
    0xeedd, 0xbb77, 0xeedd, 0xbb77, 0x3366, 0xcc99, 0x3366, 0xcc99, 0x1122, 0x4488, 0x1122, 0x4488, 0x8307, 0x0e1c, 0x3870, 0xe0c1,
    0x0306, 0x0c18, 0x3060, 0xc081, 0x0102, 0x0408, 0x1020, 0x4080, 0xffff, 0x0000, 0x0000, 0x0000, 0xff00, 0x0000, 0x0000, 0x0000,
    0x77bb, 0xddee, 0x77bb, 0xddee, 0x99cc, 0x6633, 0x99cc, 0x6633, 0x8844, 0x2211, 0x8844, 0x2211, 0xe070, 0x381c, 0x0e07, 0x83c1,
    0xc060, 0x3018, 0x0c06, 0x0381, 0x8040, 0x2010, 0x0804, 0x0201, 0xc0c0, 0xc0c0, 0xc0c0, 0xc0c0, 0x8080, 0x8080, 0x8080, 0x8080,
    0xffaa, 0xffaa, 0xffaa, 0xffaa, 0xe4e4, 0xe4e4, 0xe4e4, 0xe4e4, 0xffff, 0xff00, 0x00ff, 0x0000, 0xaaaa, 0xaaaa, 0xaaaa, 0xaaaa,
    0xff00, 0xff00, 0xff00, 0xff00, 0xff00, 0x0000, 0xff00, 0x0000, 0x8888, 0x8888, 0x8888, 0x8888, 0xff80, 0x8080, 0x8080, 0x8080,
    0x4ecf, 0xfce4, 0x273f, 0xf372, 0x6006, 0x36b1, 0x8118, 0x1b63, 0x2004, 0x4002, 0x1080, 0x0801, 0x9060, 0x0609, 0x9060, 0x0609,
    0x8814, 0x2241, 0x8800, 0xaa00, 0x2050, 0x8888, 0x8888, 0x0502, 0xaa00, 0x8000, 0x8800, 0x8000, 0x2040, 0x8000, 0x0804, 0x0200,
    0xf0f0, 0xf0f0, 0x0f0f, 0x0f0f, 0x0077, 0x7777, 0x0077, 0x7777, 0xff88, 0x8888, 0xff88, 0x8888, 0xaa44, 0xaa11, 0xaa44, 0xaa11,
    0x8244, 0x2810, 0x2844, 0x8201, 0x8080, 0x413e, 0x0808, 0x14e3, 0x8142, 0x2418, 0x1020, 0x4080, 0x40a0, 0x0000, 0x040a, 0x0000,
    0x7789, 0x8f8f, 0x7798, 0xf8f8, 0xf1f8, 0x6cc6, 0x8f1f, 0x3663, 0xbf00, 0xbfbf, 0xb0b0, 0xb0b0, 0xff80, 0x8080, 0xff08, 0x0808,
    0x1020, 0x54aa, 0xff02, 0x0408, 0x0008, 0x142a, 0x552a, 0x1408, 0x55a0, 0x4040, 0x550a, 0x0404, 0x8244, 0x3944, 0x8201, 0x0101
  };
  m_patternList.resize(64);
  for (size_t i = 0; i < 64; i++)
    m_patternList[i]=Pattern(&s_pattern[i*4]);
}

void State::setDefaultGradientList(int)
{
  if (!m_gradientList.empty()) return;
  Gradient grad;
  // grad0
  grad=Gradient(0,2,0,1);
  m_gradientList.push_back(grad);
  // grad1
  grad=Gradient(0,2,180,1);
  m_gradientList.push_back(grad);
  // grad2
  grad=Gradient(0,2,90,0);
  m_gradientList.push_back(grad);
  // grad3
  grad=Gradient(0,2,315,1);
  m_gradientList.push_back(grad);
  // grad4
  grad=Gradient(0,2,225,1);
  m_gradientList.push_back(grad);
  // grad5
  grad=Gradient(2,2,0,0);
  grad.m_box=Box2i(Vec2i(79,80),Vec2i(79,80));
  m_gradientList.push_back(grad);
  // grad6
  grad=Gradient(2,2,0,0);
  grad.m_box=Box2i(Vec2i(81,20),Vec2i(81,20));
  m_gradientList.push_back(grad);
  // grad7
  grad=Gradient(2,2,0,0);
  grad.m_box=Box2i(Vec2i(50,50),Vec2i(50,50));
  m_gradientList.push_back(grad);
  // grad8
  grad=Gradient(0,2,90,1);
  m_gradientList.push_back(grad);
  // grad9
  grad=Gradient(0,2,270,0.979172f);
  m_gradientList.push_back(grad);
  // grad10
  grad=Gradient(0,2,0,0);
  m_gradientList.push_back(grad);
  // grad11
  grad=Gradient(0,2,45,1);
  m_gradientList.push_back(grad);
  // grad12
  grad=Gradient(0,2,135,1);
  m_gradientList.push_back(grad);
  // grad13
  grad=Gradient(2,2,0,0);
  grad.m_box=Box2i(Vec2i(22,77),Vec2i(23,77));
  m_gradientList.push_back(grad);
  // grad14
  grad=Gradient(2,2,0,0);
  grad.m_box=Box2i(Vec2i(22,22),Vec2i(22,22));
  m_gradientList.push_back(grad);
  // grad15
  grad=Gradient(1,2,0,0);
  grad.m_box=Box2i(Vec2i(51,50),Vec2i(51,50));
  m_gradientList.push_back(grad);
  // grad16
  grad=Gradient(2,3,0,0);
  grad.m_box=Box2i(Vec2i(79,15),Vec2i(86,22));
  grad.m_colors[1]=MWAWColor(0xaa0000);
  grad.m_colors[2]=MWAWColor(0xcc3300);
  m_gradientList.push_back(grad);
  // grad17
  grad=Gradient(0,4,0,1);
  grad.m_colors[0]=MWAWColor(0xff33cc);
  grad.m_colors[1]=MWAWColor(0xdd0000);
  grad.m_colors[2]=MWAWColor(0xdd0000);
  grad.m_colors[3]=MWAWColor(0x000000);
  m_gradientList.push_back(grad);
  // grad18
  grad=Gradient(0,3,112,0.80835f);
  grad.m_colors[0]=MWAWColor(0x0000dd);
  grad.m_colors[1]=MWAWColor(0x000077);
  grad.m_colors[2]=MWAWColor(0xff3333);
  m_gradientList.push_back(grad);
  // grad19
  grad=Gradient(1,4,332,0);
  grad.m_box=Box2i(Vec2i(77,71),Vec2i(77,71));
  grad.m_colors[0]=MWAWColor(0xffff00);
  grad.m_colors[1]=MWAWColor(0xff3300);
  grad.m_colors[2]=MWAWColor(0x9900cc);
  grad.m_colors[3]=MWAWColor(0x0000dd);
  m_gradientList.push_back(grad);
  // grad20
  grad=Gradient(0,3,270,0.625f);
  grad.m_colors[1]=MWAWColor(0x0000dd);
  grad.m_colors[2]=MWAWColor(0x00cccc);
  m_gradientList.push_back(grad);
  // grad21
  grad=Gradient(0,2,270,0.229172f);
  grad.m_colors[0]=MWAWColor(0x0000aa);
  grad.m_colors[1]=MWAWColor(0xdddddd);
  m_gradientList.push_back(grad);
  // grad22
  grad=Gradient(0,3,90,0.729172f);
  grad.m_colors[0]=MWAWColor(0xffffff);
  grad.m_colors[2]=MWAWColor(0x9999ff);
  m_gradientList.push_back(grad);
  // grad23
  grad=Gradient(2,4,0,0);
  grad.m_box=Box2i(Vec2i(41,40),Vec2i(62,62));
  grad.m_colors[0]=MWAWColor(0xffffff);
  grad.m_colors[1]=MWAWColor(0xccffff);
  grad.m_colors[2]=MWAWColor(0x99ffff);
  grad.m_colors[3]=MWAWColor(0x66ffff);
  m_gradientList.push_back(grad);
  // grad24
  grad=Gradient(0,3,90,0.854172f);
  grad.m_colors[1]=MWAWColor(0xdd0000);
  grad.m_colors[2]=MWAWColor(0xffcc00);
  m_gradientList.push_back(grad);
  // grad25
  grad=Gradient(0,4,315,0.633453f);
  grad.m_colors[0]=MWAWColor(0xcc3300);
  grad.m_colors[1]=MWAWColor(0xff6600);
  grad.m_colors[2]=MWAWColor(0xffcc00);
  grad.m_colors[3]=MWAWColor(0xffff00);
  m_gradientList.push_back(grad);
  // grad26
  grad=Gradient(0,3,45,0.721832f);
  grad.m_colors[1]=MWAWColor(0x0000dd);
  grad.m_colors[2]=MWAWColor(0xff0099);
  m_gradientList.push_back(grad);
  // grad27
  grad=Gradient(0,3,180,1);
  grad.m_colors[1]=MWAWColor(0x0000dd);
  grad.m_colors[2]=MWAWColor(0x9900cc);
  m_gradientList.push_back(grad);
  // grad28
  grad=Gradient(0,4,90,0.81987f);
  grad.m_colors[1]=MWAWColor(0x9900cc);
  grad.m_colors[2]=MWAWColor(0x9933ff);
  grad.m_colors[3]=MWAWColor(0x66ffff);
  m_gradientList.push_back(grad);
  // grad29
  grad=Gradient(0,4,270,0.916672f);
  grad.m_colors[0]=MWAWColor(0x0066ff);
  grad.m_colors[1]=MWAWColor(0x00ccff);
  grad.m_colors[2]=MWAWColor(0xffffcc);
  grad.m_colors[3]=MWAWColor(0xff6633);
  m_gradientList.push_back(grad);
  // grad30
  grad=Gradient(2,2,0,0);
  grad.m_box=Box2i(Vec2i(0,88),Vec2i(12,100));
  grad.m_colors[0]=MWAWColor(0xff6600);
  grad.m_colors[1]=MWAWColor(0xffff00);
  m_gradientList.push_back(grad);
  // grad31
  grad=Gradient(2,4,0,0);
  grad.m_box=Box2i(Vec2i(99,52),Vec2i(100,54));
  grad.m_colors[0]=MWAWColor(0xffffff);
  grad.m_colors[1]=MWAWColor(0xffffcc);
  grad.m_colors[2]=MWAWColor(0xffff66);
  grad.m_colors[3]=MWAWColor(0xffcc00);
  m_gradientList.push_back(grad);

}
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
  case MWAWBorder::Simple:
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
  switch(ksen.m_lineRepeat) {
  case MWAWBorder::Single:
    break;
  case MWAWBorder::Double:
    o << "double,";
    break;
  case MWAWBorder::Triple:
    o << "triple,";
    break;
  default:
    o << "lRepeat=#" << int(ksen.m_lineRepeat) << ",";
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
CWStyleManager::CWStyleManager(CWParser &parser) :
  m_parserState(parser.getParserState()), m_mainParser(&parser), m_state()
{
  m_state.reset(new CWStyleManagerInternal::State);
}

CWStyleManager::~CWStyleManager()
{
}

int CWStyleManager::version() const
{
  if (m_state->m_version <= 0) m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}

bool CWStyleManager::getColor(int id, MWAWColor &col) const
{
  int numColor = (int) m_state->m_colorList.size();
  if (!numColor) {
    m_state->setDefaultColorList(version());
    numColor = int(m_state->m_colorList.size());
  }
  if (id < 0 || id >= numColor)
    return false;
  col = m_state->m_colorList[size_t(id)];
  return true;
}

bool CWStyleManager::getPattern(int id, MWAWGraphicStyle::Pattern &pattern, float &percent) const
{
  if (m_state->m_patternList.empty())
    m_state->setDefaultPatternList(version());
  if (id <= 0 || id > int(m_state->m_patternList.size()))
    return false;
  CWStyleManagerInternal::Pattern const &pat=m_state->m_patternList[size_t(id-1)];
  pattern = pat;
  percent = pat.m_percent;
  return true;
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

bool CWStyleManager::get(int graphId, MWAWGraphicStyle &graph) const
{
  graph = MWAWGraphicStyle();
  if (graphId < 0) return false;
  if (graphId >= int(m_state->m_graphList.size())) {
    MWAW_DEBUG_MSG(("CWStyleManager::get: can not find graph %d\n", graphId));
    return false;
  }
  graph = m_state->m_graphList[size_t(graphId)];
  return true;
}

bool CWStyleManager::updateGradient(int id, MWAWGraphicStyle &style) const
{
  if (m_state->m_gradientList.empty())
    m_state->setDefaultGradientList(version());
  if (id < 0 || id>=int(m_state->m_gradientList.size())) {
    MWAW_DEBUG_MSG(("CWStyleManager::updateGradiant: called with id=%d\n", id));
    return false;
  }
  if (!m_state->m_gradientList[size_t(id)].update(style)) return false;
  // try to update the color list
  size_t numColors=style.m_gradientStopList.size();
  if (numColors<=1) return true;
  float f=1.f/float(numColors);
  MWAWColor col=MWAWColor::barycenter(f,style.m_gradientStopList[0].m_color,f,style.m_gradientStopList[1].m_color);
  for (size_t c=2; c<numColors; c++)
    col=MWAWColor::barycenter(1,col,f,style.m_gradientStopList[c].m_color);
  style.setSurfaceColor(col);
  return true;
}

////////////////////////////////////////////////////////////
// read file main structure
////////////////////////////////////////////////////////////
bool CWStyleManager::readPatternList(long endPos)
{
  int vers=version();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  long sz=(long) input->readULong(4);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(PatternList):";
  if (sz<0 ||  (sz && sz < 140) || (endPos>0 && pos+sz+4>endPos) ||
      (endPos<=0 && !input->checkPosition(pos+sz+4))) {
    f << "###";
    MWAW_DEBUG_MSG(("CWStyleManager::readPatternList: can read pattern size\n"));
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos,WPX_SEEK_SET);
    return false;
  }
  if (sz==0) {
    ascFile.addPos(pos);
    ascFile.addNote("_");
    return true;
  }
  int N=(int) input->readLong(2);
  f << "N=" << N << ",";
  long val;
  static long const expectedVal[]= {-1,0,8,0x80,1}; // type, ?, fSz, ?, ?
  for (int i=0; i<5; ++i) {
    val= input->readLong(2);
    if (i==2 && val!=8) {
      input->seek(pos,WPX_SEEK_SET);
      return false;
    }
    if (val!=expectedVal[i]) f << "f" << i << "=" << val << ",";
  }
  for (int i=0; i < 64; ++i) {
    val= input->readLong(2);
    if (val!=i) f << "pat" << i << "=" << val << ",";
  }
  if (140+8*N!=sz) {
    f << "###";
    MWAW_DEBUG_MSG(("CWStyleManager::readPatternList: unexpected pattern size\n"));
    ascFile.addDelimiter(input->tell(),'|');
  } else {
    m_state->setDefaultPatternList(vers);
    for (int i=0; i < N; ++i) {
      uint16_t pat[4];
      for (int j=0; j<4; ++j) pat[j]=(uint16_t) input->readULong(2);
      CWStyleManagerInternal::Pattern pattern(pat);
      m_state->m_patternList.push_back(pattern);
      f << "pat" << i+64 << "=[" << pattern << "],";
    }
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  input->seek(pos+4+sz,WPX_SEEK_SET);
  return true;
}

bool CWStyleManager::readGradientList(long endPos)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  long sz=(long) input->readULong(4);
  long finalPos=pos+sz+4;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(GradientList):";
  if (sz<0 || (sz && sz < 76) || (endPos>0 && finalPos>endPos) ||
      (endPos<0 && !input->checkPosition(finalPos))) {
    f << "###";
    MWAW_DEBUG_MSG(("CWStyleManager::readGradientList: can read pattern size\n"));
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos,WPX_SEEK_SET);
    return false;
  }
  if (sz==0) {
    ascFile.addPos(pos);
    ascFile.addNote("_");
    return true;
  }
  int N=(int) input->readLong(2);
  f << "N=" << N << ",";
  long val;
  static long const expectedVal[]= {-1,0,0x28,0x40,1}; // type, ?, fSz, ?, ?
  for (int i=0; i<5; ++i) {
    val= input->readLong(2);
    if (i==2 && val!=0x28) {
      input->seek(pos,WPX_SEEK_SET);
      return false;
    }
    if (val!=expectedVal[i]) f << "f" << i << "=" << val << ",";
  }
  for (int i=0; i < 32; ++i) {
    val= input->readLong(2);
    if (val!=i) f << "grad" << i << "=" << val << ",";
  }
  if (76+40*N!=sz) {
    f << "###";
    MWAW_DEBUG_MSG(("CWStyleManager::readGradientList: unexpected pattern size\n"));
    ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(finalPos,WPX_SEEK_SET);
    return true;
  }

  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  m_state->setDefaultGradientList(version());
  for (int i=0; i < N; ++i) {
    pos=input->tell();
    f.str("");
    f << "GradientList-" << 32+i << ":";
    CWStyleManagerInternal::Gradient grad;
    for (int j=0; j<4; ++j) {
      unsigned char color[3];
      for (int c=0; c < 3; c++) color[c] = (unsigned char) (input->readULong(2)/256);
      grad.m_colors[j]= MWAWColor(color[0], color[1],color[2]);
    }
    grad.m_numColors=(int) input->readLong(1);
    grad.m_type=(int) input->readLong(1);
    grad.m_angle=(int)input->readLong(2);
    grad.m_decal=float(input->readLong(4))/65536.f;
    int dim[4];
    for (int j=0; j<4; ++j) dim[j]=(int)input->readLong(2);
    grad.m_box=Box2i(Vec2i(dim[0],dim[1]),Vec2i(dim[2],dim[3]));
    f << grad;
    if (!grad.ok()) {
      f << "##";
      MWAW_DEBUG_MSG(("CWStyleManager::readGradientList: can read the number of color or type\n"));
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      input->seek(finalPos,WPX_SEEK_SET);
      return true;
    }
    m_state->m_gradientList.push_back(grad);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+40, WPX_SEEK_SET);
  }
  input->seek(finalPos,WPX_SEEK_SET);
  return true;
}

bool CWStyleManager::readColorList(MWAWEntry const &entry)
{
  if (!entry.valid()) return false;
  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos+4, WPX_SEEK_SET); // avoid header
  if (entry.length() == 4) return true;

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(ColorList):";
  int N = (int) input->readULong(2);
  f << "N=" << N << ",";
  for(int i = 0; i < 2; i++) {
    int val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }

  int const fSz = 16;
  if (pos+10+N*fSz > entry.end()) {
    MWAW_DEBUG_MSG(("CWStyleManager::readColorList: can not read data\n"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  ascFile.addDelimiter(input->tell(),'|');
  input->seek(entry.end()-N*fSz, WPX_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  m_state->m_colorList.resize(size_t(N));
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    unsigned char color[3];
    for (int c=0; c < 3; c++) color[c] = (unsigned char) (input->readULong(2)/256);
    m_state->m_colorList[size_t(i)]= MWAWColor(color[0], color[1],color[2]);

    f.str("");
    f << "ColorList[" << i << "]:";
    ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+fSz, WPX_SEEK_SET);
  }

  input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

bool CWStyleManager::readStyles(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.type() != "STYL")
    return false;
  int vers=version();
  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos+4, WPX_SEEK_SET); // skip header
  long sz = (long) input->readULong(4);
  if (sz > entry.length()) {
    MWAW_DEBUG_MSG(("CWStyleManager::readStyles: pb with entry length"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(STYL):";
  if (vers <= 3) {
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(entry.end(), WPX_SEEK_SET);
    return true;
  }
  bool limitSet = true;
  if (vers <= 4) {
    // version 4 does not contents total length fields
    input->seek(-4, WPX_SEEK_CUR);
    limitSet = false;
  } else
    input->pushLimit(entry.end());
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  int id = 0;
  while (long(input->tell()) < entry.end()) {
    pos = input->tell();
    if (!readGenStyle(id)) {
      input->seek(pos, WPX_SEEK_SET);
      if (limitSet) input->popLimit();
      return false;
    }
    id++;
  }
  if (limitSet) input->popLimit();

  return true;
}

bool CWStyleManager::readGenStyle(int id)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("CWStyleManager::readGenStyle: pb with sub zone: %d", id));
    return false;
  }
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "STYL-" << id << ":";
  if (sz < 16) {
    if (sz) f << "#";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(endPos, WPX_SEEK_SET);
    return true;
  }

  std::string name("");
  int N = (int) input->readLong(2);
  int type = (int) input->readLong(2);
  int val =  (int) input->readLong(2);
  int fSz =  (int) input->readLong(2);
  f << "N=" << N << ", type?=" << type <<", fSz=" << fSz << ",";
  if (val) f << "unkn=" << val << ",";
  int unkn[2];
  for (int i = 0; i < 2; i++) {
    unkn[i] = (int) input->readLong(2);
    if (unkn[i])  f << "f" << i << "=" << val << ",";
  }
  // check here for gradient definition...
  if (version()>4 && type==-1 && val==0 && fSz==0x28 && unkn[0]==0x40 && unkn[1]==1) {
    input->seek(pos, WPX_SEEK_SET);
    if (readGradientList(endPos)) return true;
    input->seek(pos+16, WPX_SEEK_SET);
  }
  for (int i = 0; i < 4; i++)
    name += char(input->readULong(1));
  f << name;

  long actPos = input->tell();
  if (actPos != pos && actPos != endPos - N*fSz)
    ascFile.addDelimiter(input->tell(), '|');

  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  long numRemain = endPos - actPos;
  if (N > 0 && fSz > 0 && numRemain >= N*fSz) {
    input->seek(endPos-N*fSz, WPX_SEEK_SET);

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
      input->seek(endPos-N*fSz, WPX_SEEK_SET);
      for (int i = 0; i < N; i++) {
        pos = input->tell();
        f.str("");
        f << "STYL-" << id << "/" << name << "-" << i << ":";
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        input->seek(fSz, WPX_SEEK_CUR);
      }
    }
  }

  input->seek(endPos, WPX_SEEK_SET);
  if (name=="NAME") {
    if (!readPatternList()) {
      MWAW_DEBUG_MSG(("CWStyleManager::readGenStyle: can not find the pattern list\n"));
      input->seek(endPos, WPX_SEEK_SET);
    } else if (version()==4) {
      endPos=input->tell();
      if (!readGradientList()) {
        MWAW_DEBUG_MSG(("CWStyleManager::readGenStyle: can not find the gradient list\n"));
        input->seek(endPos, WPX_SEEK_SET);
      }
    }
  }

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
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  for (int i = 0; i < N; i++) {
    long pos = input->tell();
    Style style;
    f.str("");
    int val = (int) input->readLong(2);
    if (val != -1) f << "f0=" << val << ",";
    val = (int) input->readLong(2);
    if (val) f << "f1=" << val << ",";
    f << "used?=" << input->readLong(2) << ",";
    style.m_localStyleId = (int) input->readLong(2);
    if (i != style.m_localStyleId && style.m_localStyleId != -1) f << "#styleId,";
    style.m_styleId = (int) input->readLong(2);
    for (int j = 0; j < 2; j++) {
      // unknown : hash, dataId ?
      f << "g" << j << "=" << input->readLong(1) << ",";
    }
    for (int j = 2; j < 4; j++)
      f << "g" << j << "=" << input->readLong(2) << ",";
    int lookupId2 = (int) input->readLong(2);
    f << "lookupId2=" << lookupId2 << ",";
    style.m_fontId = (int) input->readLong(2);
    style.m_fontHash = (int) input->readLong(2);
    style.m_graphicId = (int) input->readLong(2);
    style.m_rulerId = (int) input->readLong(2);
    if (fSz >= 30)
      style.m_ksenId = (int) input->readLong(2);
    style.m_rulerHash = (int) input->readLong(2);
    style.m_extra = f.str();
    if (m_state->m_stylesMap.find(i)==m_state->m_stylesMap.end())
      m_state->m_stylesMap[i] = style;
    else {
      MWAW_DEBUG_MSG(("CWStyleManager::readStylesDef: style %d already exists\n", i));
    }
    if (long(input->tell()) != pos+fSz)
      ascFile.addDelimiter(input->tell(), '|');

    f.str("");
    if (!i)
      f << "Entries(Style)-0:" << style;
    else
      f << "Style-" << i << ":" << style;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+fSz, WPX_SEEK_SET);
  }
  return true;
}

bool CWStyleManager::readLookUp(int N, int fSz)
{
  m_state->m_lookupMap.clear();

  if (fSz == 0 || N== 0) return true;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  for (int i = 0; i < N; i++) {
    long pos = input->tell();
    f.str("");
    if (i == 0) f << "Entries(StylLookUp): StylLookUp-LK0:";
    else f << "StylLookUp-LK" << i << ":";
    int val = (int) input->readLong(2);
    if (m_state->m_stylesMap.find(val)!=m_state->m_stylesMap.end() &&
        m_state->m_stylesMap.find(val)->second.m_localStyleId != val &&
        m_state->m_stylesMap.find(val)->second.m_localStyleId != -1) {
      MWAW_DEBUG_MSG(("CWStyleManager::readLookUp: find some incoherence between style and lookup\n"));
      f << "##";
    }
    m_state->m_lookupMap[i] = val;
    f << "styleId=" << val;
    if (fSz != 2) {
      ascFile.addDelimiter(input->tell(), '|');
      input->seek(pos+fSz, WPX_SEEK_SET);
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
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

  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  for (int i = 0; i < N; i++) {
    long pos = input->tell();
    f.str("");
    if (i == 0) f << "Entries(FntNames): FntNames-0:";
    else f << "FntNames-" << i << ":";
    int fontEncoding = (int) input->readULong(2);
    f << "nameEncoding=" << fontEncoding << ",";
    f << "type?=" << input->readLong(2) << ",";

    int nChar = (int) input->readULong(1);
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
        char ch = (char) input->readULong(1);
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
        m_state->m_localFIdMap[i]=m_parserState->m_fontConverter->getId(name, family);
      }
    }
    if (long(input->tell()) != pos+fSz) {
      ascFile.addDelimiter(input->tell(), '|');
      input->seek(pos+fSz, WPX_SEEK_SET);
    }

    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool CWStyleManager::readStyleNames(int N, int fSz)
{
  if (fSz == 0 || N== 0) return true;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  for (int i = 0; i < N; i++) {
    long pos = input->tell();
    f.str("");
    if (i == 0) f << "Entries(StylName): StylName-0:";
    else f << "StylName-" << i << ":";
    f << "id=" << input->readLong(2) << ",";
    if (fSz > 4) {
      int nChar = (int) input->readULong(1);
      if (3+nChar > fSz) {
        static bool first = true;
        if (first) {
          MWAW_DEBUG_MSG(("CWStyleManager::readStyleNames: pb with name field %d\n", i));
          first = false;
        }
        f << "#";
      } else {
        std::string name("");
        for (int c = 0; c < nChar; c++)
          name += char(input->readULong(1));
        f << "'" << name << "'";
      }
    }
    if (long(input->tell()) != pos+fSz) {
      ascFile.addDelimiter(input->tell(), '|');
      input->seek(pos+fSz, WPX_SEEK_SET);
    }

    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
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
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  int val;
  for (int i = 0; i < N; i++) {
    long pos = input->tell();
    f.str("");
    if (!i)
      f << "Entries(CellStyle)-0:";
    else
      f << "CellStyle-" << i << ":";
    // 3 int, id or color ?
    for (int j = 0; j < 3; j++) {
      val = (int) input->readLong(2);
      if (val == -1) continue;
      f << "f" << j << "=" << val << ",";
    }
    /* g0=0|4|8|c|1f, g1:number which frequently with 8|c|d
       g2=0|4|c|13|17|1f, g3:number which frequently with 8|c|d
       g4=0|1|8, g5=0|2, g6=0-3, g7=0-f,
     */
    for (int j = 0; j < 8; j++) {
      val = (int) input->readULong(1);
      if (val)
        f << "g" << j << "=" << std::hex << val << std::dec << ",";
    }
    // h0=h1=0, h2=h3=0|1
    for (int j = 0; j < 4; j++) {
      val = (int) input->readULong(1);
      if (val)
        f << "h" << j << "=" << val << ",";
    }
    if (long(input->tell()) != pos+fSz)
      ascFile.addDelimiter(input->tell(), '|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+fSz, WPX_SEEK_SET);
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
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  int val;
  std::vector<int16_t> values16; // some values can be small or little endian, so...
  std::vector<int32_t> values32;
  for (int i = 0; i < N; i++) {
    long pos = input->tell();
    f.str("");
    MWAWGraphicStyle graph;
    // 3 int, id (find either f0=<small number> or f1=0, f2=small number
    for (int j = 0; j < 3; j++) {
      val = (int) input->readLong(2);
      if (val == -1) continue;
      f << "f" << j << "=" << val << ",";
    }

    values16.resize(0);
    values32.resize(0);
    // 2 two dim
    for (int j = 0; j < 2; j++)
      values16.push_back((int16_t)input->readLong(2));
    graph.m_lineWidth=(int) input->readULong(1);
    val = (int) input->readULong(1); // 0|1|4|80
    if (val)
      f << "f3=" << std::hex << val << std::dec << ",";
    int col[2];
    for (int j = 0; j < 2; j++)
      col[j] = (int) input->readULong(1);
    for (int j = 0; j < 3; j++)
      values16.push_back((int16_t)input->readLong(2));

    m_mainParser->checkOrdering(values16, values32);
    if (values16[0] || values16[1])
      f << "dim=" << values16[0] << "x" << values16[1] << ",";
    for (size_t j = 0; j < 2; ++j) {
      if (values16[j+2]==1) {
        if (j==0) graph.m_lineOpacity=0;
        else graph.m_surfaceOpacity=0;
        continue;
      }
      MWAWColor color;
      if (!getColor(col[j], color)) {
        f << "#col" << j << "=" << col << ",";
        continue;
      }
      MWAWGraphicStyle::Pattern pattern;
      float percent;
      if (values16[j+2] && getPattern(values16[j+2], pattern, percent)) {
        pattern.m_colors[1]=color;
        if (!pattern.getUniqueColor(color)) {
          if (j) graph.m_pattern=pattern;
          pattern.getAverageColor(color);
        }
      } else if (values16[j+2])
        f << "###pat" << j << "=" << values16[j+2];

      if (j==0) graph.m_lineColor = color;
      else graph.setSurfaceColor(color);
    }
    if (values16[4])
      f << "g0=" << values16[4] << ",";

    val = (int) input->readULong(1); // 0|1|2
    if (val) f << "g1=" << val << ",";
    val = (int) input->readULong(2); // 0|1
    if (val) f << "g2=" << val << ",";

    graph.m_extra = f.str();
    m_state->m_graphList.push_back(graph);
    f.str("");
    if (!i)
      f << "Entries(GrphStyle)-0:" << graph;
    else
      f << "GrphStyle-" << i << ":" << graph;
    if (long(input->tell()) != pos+fSz)
      ascFile.addDelimiter(input->tell(), '|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+fSz, WPX_SEEK_SET);
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
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  for (int i = 0; i < N; i++) {
    long pos = input->tell();
    KSEN ksen;
    f.str("");
    long val = input->readLong(2); // always -1
    if (val != -1)
      f << "unkn=" << val << ",";
    val = input->readLong(4); // a big number
    if (val != -1)
      f << "f0=" << val << ",";
    for (int j = 0; j < 2; j++) { // fl0=[0|1|2|4|8|f]: a pos?, fl1=[0|8|9|b|d|f] ?
      val = input->readLong(2);
      if (val) f << "fl" << j << "=" << std::hex << val << std::dec << ",";
    }
    val = input->readLong(1); // 0-5
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
      ksen.m_lineRepeat = MWAWBorder::Double;
      break;
    case 4:
      ksen.m_lineRepeat = MWAWBorder::Double;
      f << "w[external]=2,";
      break;
    case 5:
      ksen.m_lineRepeat = MWAWBorder::Double;
      f << "w[internal]=2,";
      break;
    default:
      f << "#lineType=" << val << ",";
      break;
    }
    ksen.m_valign = (int) input->readLong(1);
    ksen.m_lines = (int) input->readLong(1);
    val = input->readLong(1); // 0-18
    if (val) f << "g1=" << val << ",";
    ksen.m_extra = f.str();
    m_state->m_ksenList.push_back(ksen);
    f.str("");
    if (!i)
      f << "Entries(Ksen)-0:";
    else
      f << "Ksen-" << i << ":";
    f << ksen;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    input->seek(pos+fSz, WPX_SEEK_SET);
  }
  return true;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
