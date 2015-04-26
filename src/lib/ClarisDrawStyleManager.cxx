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

#include "ClarisDrawParser.hxx"

#include "ClarisDrawStyleManager.hxx"

/** Internal: the structures of a ClarisDrawStyleManager */
namespace ClarisDrawStyleManagerInternal
{
// gradient of ClarisDrawStyleManagerInternal
struct Gradient {
  //! construtor
  Gradient(int type=0, int nColor=0, int angle=0, float decal=0) :
    m_type(type), m_numColors(nColor), m_angle(angle), m_decal(decal), m_box()
  {
    m_colors[0]=MWAWColor::black();
    m_colors[1]=MWAWColor::white();
  }
  //! check if the gradient is valid
  bool ok() const
  {
    return m_type>=0 && m_type<=2 && m_numColors>=2 && m_numColors<=4;
  }
  //! update the style
  bool update(MWAWGraphicStyle &style) const;
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Gradient const &gr)
  {
    switch (gr.m_type) {
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
    if (gr.m_box!=MWAWBox2i(MWAWVec2i(0,0),MWAWVec2i(0,0)))
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
  MWAWBox2i m_box;
};
bool Gradient::update(MWAWGraphicStyle &style) const
{
  if (!ok()) return false;
  style.m_gradientStopList.resize(0);
  if (m_type==1 || m_type==2) {
    style.m_gradientType=m_type==1 ? MWAWGraphicStyle::G_Radial : MWAWGraphicStyle::G_Rectangular;
    for (int c=0; c < m_numColors; ++c)
      style.m_gradientStopList.push_back(MWAWGraphicStyle::GradientStop(float(c)/float(m_numColors-1), m_colors[c]));
    style.m_gradientPercentCenter=MWAWVec2f(float(m_box.center()[1])/100.f, float(m_box.center()[0])/100.f);
    return true;
  }
  style.m_gradientAngle=float(m_angle+90);
  if (m_decal>=0.5f-0.1e-3f && m_decal<=0.5f+0.1e-3f) {
    style.m_gradientType= MWAWGraphicStyle::G_Axial;
    for (int c=0; c < m_numColors; ++c)
      style.m_gradientStopList.push_back(MWAWGraphicStyle::GradientStop(float(c)/float(m_numColors-1), m_colors[m_numColors-1-c]));
    return true;
  }
  style.m_gradientType= MWAWGraphicStyle::G_Linear;
  if (m_decal <= 0.05f) {
    for (int c=0; c < m_numColors; ++c)
      style.m_gradientStopList.push_back(MWAWGraphicStyle::GradientStop(float(c)/float(m_numColors-1), m_colors[m_numColors-1-c]));
    return true;
  }
  if (m_decal >=0.95f)  {
    for (int c=0; c < m_numColors; ++c)
      style.m_gradientStopList.push_back(MWAWGraphicStyle::GradientStop(float(c)/float(m_numColors-1), m_colors[c]));
    return true;
  }
  for (int c=-m_numColors+1; c<m_numColors; ++c) {
    // checkme: look almost good
    float pos=float(c)/float(m_numColors-1)+m_decal/2.f;
    if (pos < 0) {
      if (c!=m_numColors-1 && float(c+1)/float(m_numColors-1)+(1-m_decal)/2.f>=0)
        continue;
      pos=0;
    }
    style.m_gradientStopList.push_back(MWAWGraphicStyle::GradientStop(pos>1?1:pos, m_colors[m_numColors-1+(c<0?c:-c)]));
    if (pos>=1)
      break;
  }
  return true;
}

////////////////////////////////////////
//! Internal: the state of a ClarisDrawStyleManager
struct State {
  //! constructor
  State() : m_documentSize(),
    m_numColors(8), m_numBWPatterns(0), m_numPatternsInTool(0),
    m_colorList(), m_displayColorList(), m_dashList(),
    m_fontList(), m_paragraphList(),
    m_BWPatternList(), m_gradientList()
  {
    for (int i=0; i<6; ++i) m_numStyleZones[i]=0;
  }
  //! init the black and white patterns list
  void initBWPatterns();
  //! init the colors list
  void initColors();
  //! init the dashs list
  void initDashs();
  //! init the gradient list
  void initGradients();
  //! the document size (in point)
  MWAWVec2f m_documentSize;
  //! the number of zones
  int m_numStyleZones[6];
  //! the number of color
  int m_numColors;
  //! the number of BW pattern
  int m_numBWPatterns;
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

  //! the patterns list
  std::vector<MWAWGraphicStyle::Pattern> m_BWPatternList;
  //! the gradient list
  std::vector<Gradient> m_gradientList;
};

void State::initColors()
{
  if (!m_colorList.empty()) return;
  /* there also exist a 81 color palettes

     and a Paint 168 color palettes
     0xffffff,0x000000,0x777777,0x555555,0xffff00,0xff6600,0xdd0000,0xff0099,
     0x660099,0x0000dd,0x0099ff,0x00ee00,0x006600,0x663300,0x996633,0xbbbbbb,
     0x0c0c0c,0x191919,0x262626,0x333333,0x404040,0x4c4c4c,0x595959,0x666666,
     0x737373,0x808080,0x8c8c8c,0x999999,0xa6a6a6,0xb3b3b3,0xbfbfbf,0xcccccc,
     0xd9d9d9,0xe6e6e6,0xf3f3f3,0x660000,0x661900,0x663300,0x664c00,0x665900,
     0x666600,0x336600,0x006600,0x006633,0x00664c,0x006666,0x005966,0x004c66,
     0x004066,0x003366,0x001f66,0x000066,0x330066,0x660066,0x660044,0x66002a,
     0x990000,0x992600,0x994c00,0x997300,0x998600,0x999900,0x4c9900,0x009900,
     0x00994c,0x009973,0x009999,0x008699,0x007399,0x006099,0x004c99,0x002e99,
     0x000099,0x4c0099,0x990099,0x990066,0x990040,0xcc0000,0xcc3300,0xcc6600,
     0xcc9900,0xccb300,0xcccc00,0x66cc00,0x00cc00,0x00cc66,0x00cc99,0x00cccc,
     0x00b3cc,0x0099cc,0x007fcc,0x0066cc,0x003ecc,0x0000cc,0x6600cc,0xcc00cc,
     0xcc0088,0xcc0055,0xff0000,0xff4000,0xff7f00,0xffc000,0xffdf00,0xffff00,
     0x80ff00,0x00ff00,0x00ff7f,0x00ffc0,0x00ffff,0x00e0ff,0x00c0ff,0x00a0ff,
     0x0080ff,0x004eff,0x0000ff,0x7f00ff,0xff00ff,0xff00aa,0xff006a,0xff4c4c,
     0xff794c,0xffa64c,0xffd34c,0xffe94c,0xffff4c,0xa6ff4c,0x4cff4c,0x4cffa6,
     0x4cffd3,0x4cffff,0x4ce9ff,0x4cd3ff,0x4cbcff,0x4ca6ff,0x4c83ff,0x4c4cff,
     0xa64cff,0xff4cff,0xff4cc4,0xff4c97,0xff9999,0xffb399,0xffcc99,0xffe699,
     0xfff399,0xffff99,0xccff99,0x99ff99,0x99ffcc,0x99ffe6,0x99ffff,0x99f3ff,
     0x99e6ff,0x99d9ff,0x99ccff,0x99b8ff,0x9999ff,0xcc99ff,0xff99ff,0xff99dd,
     0xff99c4,0xffcccc,0xffd9cc,0xffe6cc,0xfff3cc,0xfff9cc,0xffffcc,0xe6ffcc,
     0xccffcc,0xccffe6,0xccfff3,0xccffff,0xccf9ff,0xccf3ff,0xccecff,0xcce6ff,
     0xccdcff,0xccccff,0xe6ccff,0xffccff,0xffccee,0xffcce2,0x000000,0x000000,
   */
  uint32_t const defCol[256] = {
    0xffffff,0x000000,0x777777,0x555555,0xffff00,0xff6600,0xdd0000,0xff0099,
    0x660099,0x0000dd,0x0099ff,0x00ee00,0x006600,0x663300,0x996633,0xbbbbbb,
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
    0x3300cc,0x330099,0x330066,0x330033,0x330000,0x00ffff,0x00ffcc,0x00ff99,
    0x00ff66,0x00ff33,0x00ff00,0x00ccff,0x00cccc,0x00cc99,0x00cc66,0x00cc33,
    0x00cc00,0x0099cc,0x009999,0x009966,0x009933,0x009900,0x0066ff,0x0066cc,
    0x006699,0x006666,0x006633,0x0033ff,0x0033cc,0x003399,0x003366,0x003333,
    0x003300,0x0000ff,0x0000cc,0x000099,0x000066,0x000033,0xee0000,0xbb0000,
    0xaa0000,0x880000,0x770000,0x550000,0x440000,0x220000,0x110000,0x00dd00,
    0x00bb00,0x00aa00,0x008800,0x007700,0x005500,0x004400,0x002200,0x001100,
    0x0000ee,0x0000bb,0x0000aa,0x000088,0x000077,0x000055,0x000044,0x000022,
    0x000011,0xeeeeee,0xdddddd,0xaaaaaa,0x888888,0x444444,0x222222,0x111111,
  };
  m_colorList.resize(256);
  for (size_t i = 0; i < 256; i++)
    m_colorList[i] = defCol[i];
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

void State::initGradients()
{
  if (!m_gradientList.empty()) return;
  Gradient grad;
// grad0
  grad=Gradient(0,2,0,0);
  grad.m_colors[0]=MWAWColor(0xffffff);
  grad.m_colors[1]=MWAWColor(0x000000);
  m_gradientList.push_back(grad);
// grad1
  grad=Gradient(0,2,180,0);
  grad.m_colors[0]=MWAWColor(0xffffff);
  grad.m_colors[1]=MWAWColor(0x000000);
  m_gradientList.push_back(grad);
// grad2
  grad=Gradient(0,2,90,0.5f);
  grad.m_colors[0]=MWAWColor(0x000000);
  grad.m_colors[1]=MWAWColor(0xffffff);
  m_gradientList.push_back(grad);
// grad3
  grad=Gradient(0,2,315,0);
  grad.m_colors[0]=MWAWColor(0xffffff);
  grad.m_colors[1]=MWAWColor(0x000000);
  m_gradientList.push_back(grad);
// grad4
  grad=Gradient(0,2,225,0);
  grad.m_colors[0]=MWAWColor(0xffffff);
  grad.m_colors[1]=MWAWColor(0x000000);
  m_gradientList.push_back(grad);
// grad5
  grad=Gradient(2,2,0,0);
  grad.m_colors[0]=MWAWColor(0xffffff);
  grad.m_colors[1]=MWAWColor(0x000000);
  grad.m_box=MWAWBox2i(MWAWVec2i(79,80),MWAWVec2i(79,80));
  m_gradientList.push_back(grad);
// grad6
  grad=Gradient(2,2,0,0);
  grad.m_colors[0]=MWAWColor(0xffffff);
  grad.m_colors[1]=MWAWColor(0x000000);
  grad.m_box=MWAWBox2i(MWAWVec2i(81,20),MWAWVec2i(81,20));
  m_gradientList.push_back(grad);
// grad7
  grad=Gradient(2,2,0,0);
  grad.m_colors[0]=MWAWColor(0xffffff);
  grad.m_colors[1]=MWAWColor(0x000000);
  grad.m_box=MWAWBox2i(MWAWVec2i(50,50),MWAWVec2i(50,50));
  m_gradientList.push_back(grad);
// grad8
  grad=Gradient(0,2,90,0);
  grad.m_colors[0]=MWAWColor(0xffffff);
  grad.m_colors[1]=MWAWColor(0x000000);
  m_gradientList.push_back(grad);
// grad9
  grad=Gradient(0,2,270,0.989578f);
  grad.m_colors[0]=MWAWColor(0x000000);
  grad.m_colors[1]=MWAWColor(0xffffff);
  m_gradientList.push_back(grad);
// grad10
  grad=Gradient(0,2,0,0.5f);
  grad.m_colors[0]=MWAWColor(0x000000);
  grad.m_colors[1]=MWAWColor(0xffffff);
  m_gradientList.push_back(grad);
// grad11
  grad=Gradient(0,2,45,0);
  grad.m_colors[0]=MWAWColor(0xffffff);
  grad.m_colors[1]=MWAWColor(0x000000);
  m_gradientList.push_back(grad);
// grad12
  grad=Gradient(0,2,135,0);
  grad.m_colors[0]=MWAWColor(0xffffff);
  grad.m_colors[1]=MWAWColor(0x000000);
  m_gradientList.push_back(grad);
// grad13
  grad=Gradient(2,2,0,0);
  grad.m_colors[0]=MWAWColor(0xffffff);
  grad.m_colors[1]=MWAWColor(0x000000);
  grad.m_box=MWAWBox2i(MWAWVec2i(22,77),MWAWVec2i(23,77));
  m_gradientList.push_back(grad);
// grad14
  grad=Gradient(2,2,0,0);
  grad.m_colors[0]=MWAWColor(0xffffff);
  grad.m_colors[1]=MWAWColor(0x000000);
  grad.m_box=MWAWBox2i(MWAWVec2i(22,22),MWAWVec2i(22,22));
  m_gradientList.push_back(grad);
// grad15
  grad=Gradient(1,2,180,0);
  grad.m_colors[0]=MWAWColor(0xffffff);
  grad.m_colors[1]=MWAWColor(0x000000);
  grad.m_box=MWAWBox2i(MWAWVec2i(0,1),MWAWVec2i(0,0));
  m_gradientList.push_back(grad);
// grad16
  grad=Gradient(0,4,0,0);
  grad.m_colors[0]=MWAWColor(0x00ff00);
  grad.m_colors[1]=MWAWColor(0xffff00);
  grad.m_colors[2]=MWAWColor(0xff7f00);
  grad.m_colors[3]=MWAWColor(0xff0000);
  m_gradientList.push_back(grad);
// grad17
  grad=Gradient(0,4,0,0);
  grad.m_colors[0]=MWAWColor(0x0000ff);
  grad.m_colors[1]=MWAWColor(0x00a0ff);
  grad.m_colors[2]=MWAWColor(0x00ffc0);
  grad.m_colors[3]=MWAWColor(0x00ff00);
  m_gradientList.push_back(grad);
// grad18
  grad=Gradient(0,4,0,0);
  grad.m_colors[0]=MWAWColor(0xff0000);
  grad.m_colors[1]=MWAWColor(0xff00aa);
  grad.m_colors[2]=MWAWColor(0x7f00ff);
  grad.m_colors[3]=MWAWColor(0x0000ff);
  m_gradientList.push_back(grad);
// grad19
  grad=Gradient(0,3,135,0);
  grad.m_colors[0]=MWAWColor(0x4cbcff);
  grad.m_colors[1]=MWAWColor(0x000000);
  grad.m_colors[2]=MWAWColor(0x660066);
  m_gradientList.push_back(grad);
// grad20
  grad=Gradient(0,3,225,0);
  grad.m_colors[0]=MWAWColor(0xa64cff);
  grad.m_colors[1]=MWAWColor(0x4c4cff);
  grad.m_colors[2]=MWAWColor(0xff00ff);
  m_gradientList.push_back(grad);
// grad21
  grad=Gradient(2,3,0,0);
  grad.m_colors[0]=MWAWColor(0xffcc66);
  grad.m_colors[1]=MWAWColor(0x993300);
  grad.m_colors[2]=MWAWColor(0x111111);
  grad.m_box=MWAWBox2i(MWAWVec2i(23,22),MWAWVec2i(27,24));
  m_gradientList.push_back(grad);
// grad22
  grad=Gradient(2,3,0,0);
  grad.m_colors[0]=MWAWColor(0x3300ff);
  grad.m_colors[1]=MWAWColor(0x3300ff);
  grad.m_colors[2]=MWAWColor(0x0c0c0c);
  grad.m_box=MWAWBox2i(MWAWVec2i(28,26),MWAWVec2i(28,31));
  m_gradientList.push_back(grad);
// grad23
  grad=Gradient(1,4,135,0);
  grad.m_colors[0]=MWAWColor(0x000000);
  grad.m_colors[1]=MWAWColor(0x0000ff);
  grad.m_colors[2]=MWAWColor(0x000000);
  grad.m_colors[3]=MWAWColor(0x000000);
  grad.m_box=MWAWBox2i(MWAWVec2i(255,247),MWAWVec2i(255,250));
  m_gradientList.push_back(grad);
// grad24
  grad=Gradient(0,3,270,0.998474f);
  grad.m_colors[0]=MWAWColor(0x000000);
  grad.m_colors[1]=MWAWColor(0x330099);
  grad.m_colors[2]=MWAWColor(0xffc000);
  m_gradientList.push_back(grad);
// grad25
  grad=Gradient(0,3,270,0);
  grad.m_colors[0]=MWAWColor(0x000000);
  grad.m_colors[1]=MWAWColor(0x4c4cff);
  grad.m_colors[2]=MWAWColor(0x660044);
  m_gradientList.push_back(grad);
// grad26
  grad=Gradient(0,3,180,0);
  grad.m_colors[0]=MWAWColor(0x666666);
  grad.m_colors[1]=MWAWColor(0x33cc33);
  grad.m_colors[2]=MWAWColor(0xcc00ff);
  m_gradientList.push_back(grad);
// grad27
  grad=Gradient(0,2,45,0);
  grad.m_colors[0]=MWAWColor(0xff0000);
  grad.m_colors[1]=MWAWColor(0x664c00);
  m_gradientList.push_back(grad);
// grad28
  grad=Gradient(0,4,145,0);
  grad.m_colors[0]=MWAWColor(0x006099);
  grad.m_colors[1]=MWAWColor(0x00e0ff);
  grad.m_colors[2]=MWAWColor(0x00cc99);
  grad.m_colors[3]=MWAWColor(0x006633);
  m_gradientList.push_back(grad);
// grad29
  grad=Gradient(2,3,0,0);
  grad.m_colors[0]=MWAWColor(0xcccc33);
  grad.m_colors[1]=MWAWColor(0x666600);
  grad.m_colors[2]=MWAWColor(0x111111);
  grad.m_box=MWAWBox2i(MWAWVec2i(23,21),MWAWVec2i(23,21));
  m_gradientList.push_back(grad);
// grad30
  grad=Gradient(2,3,0,0);
  grad.m_colors[0]=MWAWColor(0x00cc33);
  grad.m_colors[1]=MWAWColor(0x005500);
  grad.m_colors[2]=MWAWColor(0x0c0c0c);
  grad.m_box=MWAWBox2i(MWAWVec2i(19,21),MWAWVec2i(22,26));
  m_gradientList.push_back(grad);
// grad31
  grad=Gradient(1,3,45,0);
  grad.m_colors[0]=MWAWColor(0xff4c4c);
  grad.m_colors[1]=MWAWColor(0x000000);
  grad.m_colors[2]=MWAWColor(0x66002a);
  grad.m_box=MWAWBox2i(MWAWVec2i(255,224),MWAWVec2i(255,237));
  m_gradientList.push_back(grad);
// grad32
  grad=Gradient(0,3,180,0);
  grad.m_colors[0]=MWAWColor(0xff00ff);
  grad.m_colors[1]=MWAWColor(0x6666cc);
  grad.m_colors[2]=MWAWColor(0x9999ff);
  m_gradientList.push_back(grad);
// grad33
  grad=Gradient(0,2,177,0.660599f);
  grad.m_colors[0]=MWAWColor(0x00cc00);
  grad.m_colors[1]=MWAWColor(0xffdf00);
  m_gradientList.push_back(grad);
// grad34
  grad=Gradient(0,4,270,0.576279f);
  grad.m_colors[0]=MWAWColor(0x66002a);
  grad.m_colors[1]=MWAWColor(0x990066);
  grad.m_colors[2]=MWAWColor(0x990099);
  grad.m_colors[3]=MWAWColor(0xcc00cc);
  m_gradientList.push_back(grad);
// grad35
  grad=Gradient(0,2,155,0.712677f);
  grad.m_colors[0]=MWAWColor(0xff0000);
  grad.m_colors[1]=MWAWColor(0xffff00);
  m_gradientList.push_back(grad);
// grad36
  grad=Gradient(0,4,45,0);
  grad.m_colors[0]=MWAWColor(0x004066);
  grad.m_colors[1]=MWAWColor(0x00e0ff);
  grad.m_colors[2]=MWAWColor(0xffffcc);
  grad.m_colors[3]=MWAWColor(0xff4c4c);
  m_gradientList.push_back(grad);
// grad37
  grad=Gradient(2,3,0,0);
  grad.m_colors[0]=MWAWColor(0xff9999);
  grad.m_colors[1]=MWAWColor(0x990000);
  grad.m_colors[2]=MWAWColor(0x262626);
  grad.m_box=MWAWBox2i(MWAWVec2i(17,19),MWAWVec2i(23,26));
  m_gradientList.push_back(grad);
// grad38
  grad=Gradient(2,3,0,0);
  grad.m_colors[0]=MWAWColor(0x0099cc);
  grad.m_colors[1]=MWAWColor(0x0033cc);
  grad.m_colors[2]=MWAWColor(0x111111);
  grad.m_box=MWAWBox2i(MWAWVec2i(18,18),MWAWVec2i(22,21));
  m_gradientList.push_back(grad);
// grad39
  grad=Gradient(1,2,214,0);
  grad.m_colors[0]=MWAWColor(0xffdf00);
  grad.m_colors[1]=MWAWColor(0x0000cc);
  grad.m_box=MWAWBox2i(MWAWVec2i(0,24),MWAWVec2i(255,232));
  m_gradientList.push_back(grad);
// grad40
  grad=Gradient(0,3,0,0);
  grad.m_colors[0]=MWAWColor(0x330033);
  grad.m_colors[1]=MWAWColor(0x006666);
  grad.m_colors[2]=MWAWColor(0x660033);
  m_gradientList.push_back(grad);
// grad41
  grad=Gradient(0,2,90,0);
  grad.m_colors[0]=MWAWColor(0x005966);
  grad.m_colors[1]=MWAWColor(0xff4c4c);
  m_gradientList.push_back(grad);
// grad42
  grad=Gradient(0,3,180,0);
  grad.m_colors[0]=MWAWColor(0x000000);
  grad.m_colors[1]=MWAWColor(0x0080ff);
  grad.m_colors[2]=MWAWColor(0x000000);
  m_gradientList.push_back(grad);
// grad43
  grad=Gradient(0,3,45,0);
  grad.m_colors[0]=MWAWColor(0x004066);
  grad.m_colors[1]=MWAWColor(0x4c4cff);
  grad.m_colors[2]=MWAWColor(0x6600cc);
  m_gradientList.push_back(grad);
// grad44
  grad=Gradient(0,4,135,0);
  grad.m_colors[0]=MWAWColor(0x330066);
  grad.m_colors[1]=MWAWColor(0xa64cff);
  grad.m_colors[2]=MWAWColor(0xffff99);
  grad.m_colors[3]=MWAWColor(0xccccff);
  m_gradientList.push_back(grad);
// grad45
  grad=Gradient(2,2,0,0);
  grad.m_colors[0]=MWAWColor(0x7f00ff);
  grad.m_colors[1]=MWAWColor(0x990066);
  grad.m_box=MWAWBox2i(MWAWVec2i(70,24),MWAWVec2i(78,30));
  m_gradientList.push_back(grad);
// grad46
  grad=Gradient(2,2,0,0);
  grad.m_colors[0]=MWAWColor(0x7f00ff);
  grad.m_colors[1]=MWAWColor(0xff0000);
  grad.m_box=MWAWBox2i(MWAWVec2i(70,24),MWAWVec2i(78,30));
  m_gradientList.push_back(grad);
// grad47
  grad=Gradient(1,3,166,0);
  grad.m_colors[0]=MWAWColor(0x008699);
  grad.m_colors[1]=MWAWColor(0x4c9900);
  grad.m_colors[2]=MWAWColor(0x990066);
  m_gradientList.push_back(grad);
// grad48
  grad=Gradient(0,3,180,0);
  grad.m_colors[0]=MWAWColor(0xcc99ff);
  grad.m_colors[1]=MWAWColor(0xff7f00);
  grad.m_colors[2]=MWAWColor(0xff0000);
  m_gradientList.push_back(grad);
// grad49
  grad=Gradient(0,2,180,0);
  grad.m_colors[0]=MWAWColor(0xffffff);
  grad.m_colors[1]=MWAWColor(0xffff00);
  m_gradientList.push_back(grad);
// grad50
  grad=Gradient(0,4,270,0);
  grad.m_colors[0]=MWAWColor(0x000000);
  grad.m_colors[1]=MWAWColor(0xa64cff);
  grad.m_colors[2]=MWAWColor(0x330066);
  grad.m_colors[3]=MWAWColor(0x0000dd);
  m_gradientList.push_back(grad);
// grad51
  grad=Gradient(0,3,315,0);
  grad.m_colors[0]=MWAWColor(0x4cff4c);
  grad.m_colors[1]=MWAWColor(0x4cffd3);
  grad.m_colors[2]=MWAWColor(0x005966);
  m_gradientList.push_back(grad);
// grad52
  grad=Gradient(0,2,225,0);
  grad.m_colors[0]=MWAWColor(0x990066);
  grad.m_colors[1]=MWAWColor(0x0000ff);
  m_gradientList.push_back(grad);
// grad53
  grad=Gradient(2,4,0,0);
  grad.m_colors[0]=MWAWColor(0xfff399);
  grad.m_colors[1]=MWAWColor(0xffc000);
  grad.m_colors[2]=MWAWColor(0xff0000);
  grad.m_colors[3]=MWAWColor(0x661900);
  grad.m_box=MWAWBox2i(MWAWVec2i(90,43),MWAWVec2i(100,50));
  m_gradientList.push_back(grad);
// grad54
  grad=Gradient(2,2,0,0);
  grad.m_colors[0]=MWAWColor(0xffff4c);
  grad.m_colors[1]=MWAWColor(0xff00aa);
  grad.m_box=MWAWBox2i(MWAWVec2i(0,0),MWAWVec2i(7,7));
  m_gradientList.push_back(grad);
// grad55
  grad=Gradient(1,4,360,0);
  grad.m_colors[0]=MWAWColor(0x003ecc);
  grad.m_colors[1]=MWAWColor(0x000000);
  grad.m_colors[2]=MWAWColor(0x00ffc0);
  grad.m_colors[3]=MWAWColor(0x990066);
  m_gradientList.push_back(grad);
// grad56
  grad=Gradient(0,3,270,0);
  grad.m_colors[0]=MWAWColor(0x0c0c0c);
  grad.m_colors[1]=MWAWColor(0x6633ff);
  grad.m_colors[2]=MWAWColor(0x666600);
  m_gradientList.push_back(grad);
// grad57
  grad=Gradient(0,4,90,0);
  grad.m_colors[0]=MWAWColor(0x00ff7f);
  grad.m_colors[1]=MWAWColor(0x000000);
  grad.m_colors[2]=MWAWColor(0x000000);
  grad.m_colors[3]=MWAWColor(0xff0000);
  m_gradientList.push_back(grad);
// grad58
  grad=Gradient(0,4,0,0);
  grad.m_colors[0]=MWAWColor(0xff794c);
  grad.m_colors[1]=MWAWColor(0xff9999);
  grad.m_colors[2]=MWAWColor(0x00ffff);
  grad.m_colors[3]=MWAWColor(0x6600cc);
  m_gradientList.push_back(grad);
// grad59
  grad=Gradient(0,2,45,0.958786f);
  grad.m_colors[0]=MWAWColor(0xffff00);
  grad.m_colors[1]=MWAWColor(0xff00aa);
  m_gradientList.push_back(grad);
// grad60
  grad=Gradient(0,4,135,0);
  grad.m_colors[0]=MWAWColor(0x660066);
  grad.m_colors[1]=MWAWColor(0x006099);
  grad.m_colors[2]=MWAWColor(0xff00ff);
  grad.m_colors[3]=MWAWColor(0xff006a);
  m_gradientList.push_back(grad);
// grad61
  grad=Gradient(2,3,0,0);
  grad.m_colors[0]=MWAWColor(0xf3f3f3);
  grad.m_colors[1]=MWAWColor(0x99ffff);
  grad.m_colors[2]=MWAWColor(0xff99c4);
  grad.m_box=MWAWBox2i(MWAWVec2i(18,58),MWAWVec2i(25,89));
  m_gradientList.push_back(grad);
// grad62
  grad=Gradient(2,3,0,0);
  grad.m_colors[0]=MWAWColor(0xff006a);
  grad.m_colors[1]=MWAWColor(0x660044);
  grad.m_colors[2]=MWAWColor(0x001f66);
  grad.m_box=MWAWBox2i(MWAWVec2i(0,0),MWAWVec2i(7,100));
  m_gradientList.push_back(grad);
// grad63
  grad=Gradient(1,2,294,0);
  grad.m_colors[0]=MWAWColor(0x0000ff);
  grad.m_colors[1]=MWAWColor(0xccccff);
  grad.m_box=MWAWBox2i(MWAWVec2i(0,13),MWAWVec2i(0,29));
  m_gradientList.push_back(grad);
}

void State::initBWPatterns()
{
  if (!m_BWPatternList.empty()) return;
  m_BWPatternList.resize(64);
  for (size_t i = 0; i < 64; i++) {
    static uint16_t const(s_pattern[4*64]) = {
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
    MWAWGraphicStyle::Pattern pat;
    pat.m_dim=MWAWVec2i(8,8);
    pat.m_data.resize(8);
    pat.m_colors[0]=MWAWColor::white();
    pat.m_colors[1]=MWAWColor::black();

    uint16_t const *patPtr=&s_pattern[4*i];
    for (size_t j=0; j<8; j+=2, ++patPtr) {
      pat.m_data[j]=uint8_t((*patPtr)>>8);
      pat.m_data[j+1]=uint8_t((*patPtr)&0xFF);
    }
    m_BWPatternList[i]=pat;
  }
}

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
ClarisDrawStyleManager::ClarisDrawStyleManager(ClarisDrawParser &parser) :
  m_parser(parser), m_parserState(parser.getParserState()), m_state(new ClarisDrawStyleManagerInternal::State)
{
}

ClarisDrawStyleManager::~ClarisDrawStyleManager()
{
}

////////////////////////////////////////////////////////////
//
// Interface
//
////////////////////////////////////////////////////////////
bool ClarisDrawStyleManager::getColor(int cId, MWAWColor &color) const
{
  m_state->initColors();
  if (cId<0||cId>=int(m_state->m_colorList.size())) {
    MWAW_DEBUG_MSG(("ClarisDrawStyleManager::getColor: can not find color %d\n", cId));
    return false;
  }
  color=m_state->m_colorList[size_t(cId)];
  return true;
}

bool ClarisDrawStyleManager::getFont(int fId, MWAWFont &font) const
{
  if (fId==0) return false; // none
  if (fId<=0||fId>int(m_state->m_fontList.size())) {
    MWAW_DEBUG_MSG(("ClarisDrawStyleManager::getFont: can not find font %d\n", fId));
    return false;
  }
  font=m_state->m_fontList[size_t(fId-1)];
  return true;
}

bool ClarisDrawStyleManager::getParagraph(int fId, MWAWParagraph &para) const
{
  if (fId<0||fId>=int(m_state->m_paragraphList.size())) {
    MWAW_DEBUG_MSG(("ClarisDrawStyleManager::getParagraph: can not find paragraph %d\n", fId));
    return false;
  }
  para=m_state->m_paragraphList[size_t(fId)];
  return true;
}

bool ClarisDrawStyleManager::getDash(int dId, std::vector<float> &dash) const
{
  if (dId==0) // a solid line
    return false;
  if (m_state->m_dashList.empty())
    m_state->initDashs();
  if (dId<=0||dId>int(m_state->m_dashList.size())) {
    MWAW_DEBUG_MSG(("ClarisDrawStyleManager::getDash: can not find dash %d\n", dId));
    return false;
  }
  dash=m_state->m_dashList[size_t(dId-1)];
  return true;
}

bool ClarisDrawStyleManager::getPattern(int pId, MWAWGraphicStyle::Pattern &pattern) const
{
  if (pId==0) // no pattern
    return false;

  m_state->initBWPatterns();
  if (pId<=0||pId>int(m_state->m_BWPatternList.size())) {
    MWAW_DEBUG_MSG(("ClarisDrawStyleManager::getPattern: can not find BW pattern %d\n", pId));
    return false;
  }
  pattern=m_state->m_BWPatternList[size_t(pId-1)];

  return true;
}

bool ClarisDrawStyleManager::updateGradient(int gId, MWAWGraphicStyle &style) const
{
  if (m_state->m_gradientList.empty())
    m_state->initGradients();
  if (gId<0 || gId>int(m_state->m_gradientList.size())) {
    MWAW_DEBUG_MSG(("ClarisDrawStyleManager::updateGradient: can not find gradient %d\n", gId));
    return false;
  }
  ClarisDrawStyleManagerInternal::Gradient const &gradient=m_state->m_gradientList[size_t(gId)];
  if (!gradient.update(style)) return false;
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
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool ClarisDrawStyleManager::readFontNames()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  if (!input->checkPosition(pos+8)) return false;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(FNTM):";
  if (input->readULong(4) != 0x464e544d)
    return false;
  long sz = (long) input->readULong(4);
  if (sz==0) {
    ascFile.addPos(pos);
    ascFile.addNote("_");
    return true;
  }
  long endPos=pos+4+sz;
  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("ClarisDrawStyleManager::readFontNames: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }

  int N = (int) input->readLong(2);
  f << "N=" << N << ",";
  int type = (int) input->readLong(2);
  if (type != -1)
    f << "#type=" << type << ",";
  int val = (int) input->readLong(2);
  if (val) f << "#unkn=" << val << ",";
  int fSz = (int) input->readULong(2);
  int hSz = (int) input->readULong(2);
  if (!fSz || N*fSz+hSz+12 != sz) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisDrawStyleManager::readFontNames: unexpected field/header size\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }

  if (long(input->tell()) != pos+8+12+hSz) {
    ascFile.addDelimiter(input->tell(), '|');
    input->seek(pos+8+12+hSz, librevenge::RVNG_SEEK_SET);
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  pos=input->tell();
  if (fSz!=72) {
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisDrawStyleManager::readFontNames: no sure how to read the data\n"));
    ascFile.addPos(pos);
    ascFile.addNote("FNTM-data:###");
    return true;
  }

  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "FNTM-" << i << ":";
    int fId=(int) input->readULong(2);
    f << "fId=" << fId << ",";
    val=(int) input->readULong(2); // always fId?
    if (val!=fId) f << "fId2=" << val << ",";
    for (int j=0; j<2; ++j) { // always 0?
      val=(int) input->readLong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }
    int sSz=(int) input->readULong(1);
    if (!sSz || sSz+9>fSz) { // sSz==0 is probably normal
      MWAW_DEBUG_MSG(("ClarisDrawStyleManager::readFontNames: the string size seems bad\n"));
      f << "###";
    }
    else {
      std::string name("");
      for (int s=0; s<sSz; ++s) name+=(char) input->readULong(1);
      f << "'" << name << "'";
      m_parserState->m_fontConverter->setCorrespondance(fId, name);
    }
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool ClarisDrawStyleManager::readFontStyles()
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  if (!input->checkPosition(endPos)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisDrawStyleManager::readFontStyles: unexpected zone size\n"));
    return false;
  }
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(FontStyle):";

  if (sz==0) {
    ascFile.addPos(pos);
    ascFile.addNote("_");
    return true;
  }

  int N = (int) input->readLong(2);
  f << "N=" << N << ",";
  int type = (int) input->readLong(2);
  if (type != -1)
    f << "#type=" << type << ",";
  int val = (int) input->readLong(2);
  if (val) f << "#unkn=" << val << ",";
  int fSz = (int) input->readULong(2);
  int hSz = (int) input->readULong(2);
  if (fSz!=40 || N *fSz+hSz+12 != sz) {
    MWAW_DEBUG_MSG(("ClarisDrawStyleManager::readFontStyles: unexpected size\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  val = (int) input->readLong(2);
  if (val!=1)
    f << "f0=" << val << ",";
  if (long(input->tell()) != pos+4+hSz)
    ascFile.addDelimiter(input->tell(), '|');
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  long debPos = endPos-N*fSz;
  for (int i = 0; i < N; i++) {
    input->seek(debPos, librevenge::RVNG_SEEK_SET);
    f.str("");
    f << "FontStyle-" << i << ":";
    int fId=(int) input->readULong(2);
    f << "fId=" << fId << ",";
    val=(int) input->readULong(2); // always fId?
    if (val!=fId) f << "fId2=" << val << ",";
    val=(int) input->readULong(2);
    if (val) f << "size=" << val << ",";
    val=(int) input->readULong(2);
    if (val!=0x2001)
      f << "fl=" << std::hex << val << std::dec << ",";
    int sSz=(int) input->readULong(1);
    if (!sSz || sSz+9>fSz) {
      MWAW_DEBUG_MSG(("ClarisDrawStyleManager::readFontStyles: the string size seems bad\n"));
      f << "###";
    }
    else {
      std::string name("");
      for (int s=0; s<sSz; ++s) name+=(char) input->readULong(1);
      f << "'" << name << "'";
    }
    ascFile.addPos(debPos);
    ascFile.addNote(f.str().c_str());
    debPos += fSz;
  }
  input->seek(endPos,librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// styles/settings present in data/rsrc fork
////////////////////////////////////////////////////////////

bool ClarisDrawStyleManager::readArrows()
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=input->tell();
  long sz=(long) input->readULong(4);
  long endPos=pos+4+sz;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(Arrow):";

  if (sz==0) {
    ascFile.addPos(pos);
    ascFile.addNote("_");
    return true;
  }
  if (sz < 12 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("ClarisDrawStyleManager::readArrows: the data size seems bad\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  int N=(int) input->readLong(2);
  f << "N=" << N << ",";
  long val;
  static long const expectedVal[]= {-1,0,0x14,0,2}; // type, ?, fSz, ?, ?
  for (int i=0; i<5; ++i) {
    val= input->readLong(2);
    if (i==2 && val!=20) { // fSz
      input->seek(pos,librevenge::RVNG_SEEK_SET);
      return false;
    }
    if (val!=expectedVal[i]) f << "f" << i << "=" << val << ",";
  }
  if (12+20*N!=sz) {
    f << "###";
    MWAW_DEBUG_MSG(("ClarisDrawStyleManager::readArrows: unexpected field size\n"));
    ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(endPos,librevenge::RVNG_SEEK_SET);
    return true;
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "Arrow-A" << i+1 << ",";
    double value;
    bool isNAN;
    if (!input->readDouble8(value, isNAN))
      f << "###,";
    else
      f << value << ",";
    val=input->readLong(4);
    if (val) f<<"id=" << val << ",";
    float pt[2]; // position of the symetric point in the arrow head
    for (int k=0; k<2; ++k) pt[k]=float(input->readULong(4))/256.f;
    f << "pt=" << pt[1] << "x" << pt[0] << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool ClarisDrawStyleManager::readDashs()
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=input->tell();
  long sz=(long) input->readULong(4);
  long endPos=pos+4+sz;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(Dash):";

  if (sz==0) {
    ascFile.addPos(pos);
    ascFile.addNote("_");
    return true;
  }
  if (sz < 12 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("ClarisDrawStyleManager::readDashs: the data size seems bad\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  int N=(int) input->readLong(2);
  f << "N=" << N << ",";
  long val;
  static long const expectedVal[]= {-1,0,0x1c,0,2}; // type, ?, fSz, ?, ?
  for (int i=0; i<5; ++i) {
    val= input->readLong(2);
    if (i==2 && val!=28) { // fSz
      input->seek(pos,librevenge::RVNG_SEEK_SET);
      return false;
    }
    if (val!=expectedVal[i]) f << "f" << i << "=" << val << ",";
  }
  if (12+28*N!=sz) {
    f << "###";
    MWAW_DEBUG_MSG(("ClarisDrawStyleManager::readDashs: unexpected field size\n"));
    ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(endPos,librevenge::RVNG_SEEK_SET);
    return true;
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "Dash-D" << i+1 << ",";
    val=input->readLong(4);
    if (val!=1) f<<"numUsed=" << val << ",";
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
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool ClarisDrawStyleManager::readRulers()
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=input->tell();
  long sz=(long) input->readULong(4);
  long endPos=pos+4+sz;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(RulerStyle):";

  if (sz==0) {
    ascFile.addPos(pos);
    ascFile.addNote("_");
    return true;
  }
  if (sz < 12 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("ClarisDrawStyleManager::readRulers: the data size seems bad\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  int N=(int) input->readLong(2);
  f << "N=" << N << ",";
  long val;
  static long const expectedVal[]= {-1,0,0x1c,0,2}; // type, ?, fSz, ?, ?
  for (int i=0; i<5; ++i) {
    val= input->readLong(2);
    if (i==2 && val!=28) { // fSz
      input->seek(pos,librevenge::RVNG_SEEK_SET);
      return false;
    }
    if (val!=expectedVal[i]) f << "f" << i << "=" << val << ",";
  }
  if (12+28*N!=sz) {
    f << "###";
    MWAW_DEBUG_MSG(("ClarisDrawStyleManager::readRulers: unexpected field size\n"));
    ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(endPos,librevenge::RVNG_SEEK_SET);
    return true;
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "RulerStyle-R" << i+1 << ":";
    int numUsed=(int) input->readULong(4);
    if (numUsed!=1) f << "numUsed=" << numUsed << ",";
    for (int j=0; j<2; ++j) {
      double value;
      bool isNAN;
      if (!input->readDouble10(value, isNAN))
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
// read file main structure
////////////////////////////////////////////////////////////
bool ClarisDrawStyleManager::readColorList()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  long sz=(long) input->readULong(4);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(ColorList):";
  if (sz==0) {
    ascFile.addPos(pos);
    ascFile.addNote("_");
    return true;
  }
  if (sz < 12 || !input->checkPosition(pos+sz+4)) {
    f << "###";
    MWAW_DEBUG_MSG(("ClarisDrawStyleManager::readColorList: can read color size\n"));
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos,librevenge::RVNG_SEEK_SET);
    return false;
  }
  int N=(int) input->readLong(2);
  f << "N=" << N << ",";
  long val;
  static long const expectedVal[]= {-1,0,6,0,1}; // type, ?, fSz, ?, ?
  for (int i=0; i<5; ++i) {
    val= input->readLong(2);
    if (i==2 && val!=6) {
      input->seek(pos,librevenge::RVNG_SEEK_SET);
      return false;
    }
    if (val!=expectedVal[i]) f << "f" << i << "=" << val << ",";
  }
  if (12+6*N!=sz) {
    f << "###";
    MWAW_DEBUG_MSG(("ClarisDrawStyleManager::readColorList: unexpected color size\n"));
    ascFile.addDelimiter(input->tell(),'|');
  }
  else {
    m_state->m_colorList.clear();
    for (int i=0; i < N; ++i) {
      unsigned char color[3];
      for (int c=0; c < 3; c++) color[c] = (unsigned char)(input->readULong(2)/256);
      MWAWColor col(color[0], color[1],color[2]);
      m_state->m_colorList.push_back(col);
      f << col << ",";
    }
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  input->seek(pos+4+sz,librevenge::RVNG_SEEK_SET);
  return true;
}

bool ClarisDrawStyleManager::readPatternList()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  long sz=(long) input->readULong(4);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(PatternList):";
  if (sz==0) {
    ascFile.addPos(pos);
    ascFile.addNote("_");
    return true;
  }
  if (sz < 140 || !input->checkPosition(pos+sz+4)) {
    f << "###";
    MWAW_DEBUG_MSG(("ClarisDrawStyleManager::readPatternList: can read pattern size\n"));
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos,librevenge::RVNG_SEEK_SET);
    return false;
  }
  int N=(int) input->readLong(2);
  f << "N=" << N << ",";
  long val;
  static long const expectedVal[]= {-1,0,8,0x80,1}; // type, ?, fSz, ?, ?
  for (int i=0; i<5; ++i) {
    val= input->readLong(2);
    if (i==2 && val!=8) {
      input->seek(pos,librevenge::RVNG_SEEK_SET);
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
    MWAW_DEBUG_MSG(("ClarisDrawStyleManager::readPatternList: unexpected pattern size\n"));
    ascFile.addDelimiter(input->tell(),'|');
  }
  else {
    m_state->initBWPatterns();
    for (int i=0; i < N; ++i) {
      MWAWGraphicStyle::Pattern pattern;
      pattern.m_colors[0]=MWAWColor::white();
      pattern.m_colors[1]=MWAWColor::black();
      pattern.m_dim=MWAWVec2i(8,8);
      pattern.m_data.resize(8);
      for (size_t j=0; j<8; ++j) pattern.m_data[j]=(uint8_t) input->readULong(1);
      m_state->m_BWPatternList.push_back(pattern);
      f << "pat" << i+64 << "=[" << pattern << "],";
    }
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  input->seek(pos+4+sz,librevenge::RVNG_SEEK_SET);
  return true;
}

bool ClarisDrawStyleManager::readGradientList()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  long sz=(long) input->readULong(4);
  long finalPos=pos+sz+4;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(GradientList):";
  if (sz==0) {
    ascFile.addPos(pos);
    ascFile.addNote("_");
    return true;
  }
  if (sz < 76 ||  !input->checkPosition(finalPos)) {
    f << "###";
    MWAW_DEBUG_MSG(("ClarisDrawStyleManager::readGradientList: can read pattern size\n"));
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos,librevenge::RVNG_SEEK_SET);
    return false;
  }
  int N=(int) input->readLong(2);
  f << "N=" << N << ",";
  long val;
  static long const expectedVal[]= {-1,0,0x28,0x40,1}; // type, ?, fSz, ?, ?
  for (int i=0; i<5; ++i) {
    val= input->readLong(2);
    if (i==2 && val!=0x28) { // fSz
      input->seek(pos,librevenge::RVNG_SEEK_SET);
      return false;
    }
    if (val!=expectedVal[i]) f << "f" << i << "=" << val << ",";
  }
  int const numDefGrad=64;
  if (12+2*numDefGrad+40*N!=sz) {
    f << "###";
    MWAW_DEBUG_MSG(("ClarisDrawStyleManager::readGradientList: unexpected pattern size\n"));
    ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(finalPos,librevenge::RVNG_SEEK_SET);
    return true;
  }

  for (int i=0; i < numDefGrad; ++i) {
    val= input->readLong(2);
    if (val!=i) f << "grad" << i << "=" << val << ",";
  }

  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  m_state->initGradients();
  for (int i=0; i < N; ++i) {
    pos=input->tell();
    f.str("");
    f << "GradientList-" << numDefGrad+i << ":";
    ClarisDrawStyleManagerInternal::Gradient grad;
    for (int j=0; j<4; ++j) {
      unsigned char color[3];
      for (int c=0; c < 3; c++) color[c] = (unsigned char)(input->readULong(2)/256);
      grad.m_colors[j]= MWAWColor(color[0], color[1],color[2]);
    }
    grad.m_numColors=(int) input->readLong(1);
    grad.m_type=(int) input->readLong(1);
    grad.m_angle=(int)input->readLong(2);
    grad.m_decal=float(input->readLong(4))/65536.f;
    int dim[4];
    for (int j=0; j<4; ++j) dim[j]=(int)input->readLong(2);
    grad.m_box=MWAWBox2i(MWAWVec2i(dim[0],dim[1]),MWAWVec2i(dim[2],dim[3]));
    f << grad;
    if (!grad.ok()) {
      f << "##";
      MWAW_DEBUG_MSG(("ClarisDrawStyleManager::readGradientList: can read the number of color or type\n"));
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      input->seek(finalPos,librevenge::RVNG_SEEK_SET);
      return true;
    }
    m_state->m_gradientList.push_back(grad);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+40, librevenge::RVNG_SEEK_SET);
  }
  input->seek(finalPos,librevenge::RVNG_SEEK_SET);
  return true;
}



// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
