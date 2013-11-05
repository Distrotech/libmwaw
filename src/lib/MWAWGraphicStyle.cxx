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

/* This header contains code specific to a pict mac file
 */
#include <string.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include <librevenge/librevenge.h>
#include <libmwaw/libmwaw.hxx>

#include "libmwaw_internal.hxx"

#include "MWAWFontConverter.hxx"
#include "MWAWPictBitmap.hxx"

#include "MWAWGraphicStyle.hxx"

////////////////////////////////////////////////////////////
// pattern
////////////////////////////////////////////////////////////
bool MWAWGraphicStyle::Pattern::getUniqueColor(MWAWColor &col) const
{
  if (empty() || m_picture.size() || m_data.empty()) return false;
  if (m_colors[0]==m_colors[1]) {
    col = m_colors[0];
    return true;
  }
  unsigned char def=m_data[0];
  if (def!=0 && def!=0xFF) return false;
  for (size_t c=1; c < m_data.size(); ++c)
    if (m_data[c]!=def) return false;
  col = m_colors[def ? 1 : 0];
  return true;
}

bool MWAWGraphicStyle::Pattern::getAverageColor(MWAWColor &color) const
{
  if (empty()) return false;
  if (m_picture.size()) {
    color=m_pictureAverageColor;
    return true;
  }
  if (m_data.empty()) return false;
  if (m_colors[0]==m_colors[1]) {
    color = m_colors[0];
    return true;
  }
  int numOne=0, numZero=0;
  for (size_t c=0; c < m_data.size(); ++c) {
    for (int depl=1, b=0; b < 8; ++b, depl*=2) {
      if (m_data[c] & depl)
        numOne++;
      else
        numZero++;
    }
  }
  if (!numOne && !numZero) return false;
  float percent=float(numOne)/float(numOne+numZero);
  color = MWAWColor::barycenter(1.f-percent,m_colors[0],percent,m_colors[1]);
  return true;
}

bool MWAWGraphicStyle::Pattern::getBinary(librevenge::RVNGBinaryData &data, std::string &type) const
{
  if (empty()) {
    MWAW_DEBUG_MSG(("MWAWGraphicStyle::Pattern::getBinary: called on invalid pattern\n"));
    return false;
  }
  if (m_picture.size()) {
    data=m_picture;
    type=m_pictureMime;
    return true;
  }
  /* We create a indexed bitmap to obtain a final binary data.

     But it will probably better to recode that differently
   */
  MWAWPictBitmapIndexed bitmap(m_dim);
  std::vector<MWAWColor> colors;
  for (int i=0; i < 2; ++i)
    colors.push_back(m_colors[i]);
  bitmap.setColors(colors);
  int numBytesByLines = m_dim[0]/8;
  unsigned char const *ptr = &m_data[0];
  std::vector<int> rowValues((size_t)m_dim[0]);
  for (int h=0; h < m_dim[1]; ++h) {
    size_t i=0;
    for (int b=0; b < numBytesByLines; ++b) {
      unsigned char c=*(ptr++);
      unsigned char depl=0x80;
      for (int byt=0; byt<8; ++byt) {
        rowValues[i++] = (c&depl) ? 1 : 0;
        depl=(unsigned char) (depl>>1);
      }
    }
    bitmap.setRow(h, &rowValues[0]);
  }
  return bitmap.getBinary(data,type);
}

////////////////////////////////////////////////////////////
// style
////////////////////////////////////////////////////////////
void MWAWGraphicStyle::addTo(librevenge::RVNGPropertyList &list, librevenge::RVNGPropertyListVector &gradient, bool only1D) const
{
  list.clear();
  if (!hasLine())
    list.insert("draw:stroke", "none");
  else if (m_lineDashWidth.size()>=2) {
    int nDots1=0, nDots2=0;
    float size1=0, size2=0, totalGap=0.0;
    for (size_t c=0; c+1 < m_lineDashWidth.size(); ) {
      float sz=m_lineDashWidth[c++];
      if (nDots2 && (sz<size2||sz>size2)) {
        static bool first=true;
        if (first) {
          MWAW_DEBUG_MSG(("MWAWGraphicStyle::addTo: can set all dash\n"));
          first = false;
        }
        break;
      }
      if (nDots2)
        nDots2++;
      else if (!nDots1 || (sz>=size1 && sz <= size1)) {
        nDots1++;
        size1=sz;
      } else {
        nDots2=1;
        size2=sz;
      }
      totalGap += m_lineDashWidth[c++];
    }
    list.insert("draw:stroke", "dash");
    list.insert("draw:dots1", nDots1);
    list.insert("draw:dots1-length", size1, librevenge::RVNG_POINT);
    if (nDots2) {
      list.insert("draw:dots2", nDots2);
      list.insert("draw:dots2-length", size2, librevenge::RVNG_POINT);
    }
    list.insert("draw:distance", totalGap/float(nDots1+nDots2), librevenge::RVNG_POINT);;
  } else
    list.insert("draw:stroke", "solid");
  list.insert("svg:stroke-color", m_lineColor.str().c_str());
  list.insert("svg:stroke-width", m_lineWidth,librevenge::RVNG_POINT);

  if (m_lineOpacity < 1)
    list.insert("svg:stroke-opacity", m_lineOpacity, librevenge::RVNG_PERCENT);
  switch(m_lineCap) {
  case C_Round:
    list.insert("svg:stroke-linecap", "round");
    break;
  case C_Square:
    list.insert("svg:stroke-linecap", "square");
    break;
  case C_Butt:
  default:
    break;
  }
  switch(m_lineJoin) {
  case J_Round:
    list.insert("svg:stroke-linejoin", "round");
    break;
  case J_Bevel:
    list.insert("svg:stroke-linejoin", "bevel");
    break;
  case J_Miter:
  default:
    break;
  }
  if (m_arrows[0]) {
    list.insert("draw:marker-start-path", "m10 0-10 30h20z");
    list.insert("draw:marker-start-viewbox", "0 0 20 30");
    list.insert("draw:marker-start-center", "false");
    list.insert("draw:marker-start-width", "5pt");
  }
  if (m_arrows[1]) {
    list.insert("draw:marker-end-path", "m10 0-10 30h20z");
    list.insert("draw:marker-end-viewbox", "0 0 20 30");
    list.insert("draw:marker-end-center", "false");
    list.insert("draw:marker-end-width", "5pt");
  }

  if (only1D || !hasSurface()) {
    list.insert("draw:fill", "none");
    return;
  }
  list.insert("svg:fill-rule", m_fillRuleEvenOdd ? "evenodd" : "nonzero");
  if (hasGradient()) {
    list.insert("draw:fill", "gradient");
    switch (m_gradientType) {
    case G_Axial:
      list.insert("draw:style", "axial");
      break;
    case G_Radial:
      list.insert("draw:style", "radial");
      break;
    case G_Rectangular:
      list.insert("draw:style", "rectangular");
      break;
    case G_Square:
      list.insert("draw:style", "square");
      break;
    case G_Ellipsoid:
      list.insert("draw:style", "ellipsoid");
      break;
    case G_Linear:
    case G_None:
    default:
      list.insert("draw:style", "linear");
      break;
    }
    if (m_gradientStopList.size()==2 && m_gradientStopList[0].m_offset <= 0 &&
        m_gradientStopList[1].m_offset >=1) {
      size_t first=(m_gradientType==G_Linear || m_gradientType==G_Axial) ? 0 : 1;
      list.insert("draw:start-color", m_gradientStopList[first].m_color.str().c_str());
      list.insert("libwpg:start-opacity", m_gradientStopList[first].m_opacity, librevenge::RVNG_PERCENT);
      list.insert("draw:end-color", m_gradientStopList[1-first].m_color.str().c_str());
      list.insert("libwpg:end-opacity", m_gradientStopList[1-first].m_opacity, librevenge::RVNG_PERCENT);
    } else {
      for (size_t s=0; s < m_gradientStopList.size(); ++s) {
        librevenge::RVNGPropertyList grad;
        grad.insert("svg:offset", m_gradientStopList[s].m_offset, librevenge::RVNG_PERCENT);
        grad.insert("svg:stop-color", m_gradientStopList[s].m_color.str().c_str());
        grad.insert("svg:stop-opacity", m_gradientStopList[s].m_opacity, librevenge::RVNG_PERCENT);
        gradient.append(grad);
      }
    }
    list.insert("draw:angle", m_gradientAngle);
    list.insert("draw:border", m_gradientBorder, librevenge::RVNG_PERCENT);
    if (m_gradientType != G_Linear) {
      list.insert("svg:cx", m_gradientPercentCenter[0], librevenge::RVNG_PERCENT);
      list.insert("svg:cy", m_gradientPercentCenter[1], librevenge::RVNG_PERCENT);
    }
    if (m_gradientType == G_Radial)
      list.insert("svg:r", m_gradientRadius, librevenge::RVNG_PERCENT); // checkme
  } else {
    bool done = false;
    MWAWColor surfaceColor=m_surfaceColor;
    float surfaceOpacity = m_surfaceOpacity;
    if (hasPattern()) {
      MWAWColor col;
      if (m_pattern.getUniqueColor(col)) {
        // no need to create a uniform pattern
        surfaceColor = col;
        surfaceOpacity = 1;
      } else {
        librevenge::RVNGBinaryData data;
        std::string mimeType;
        if (m_pattern.getBinary(data, mimeType)) {
          list.insert("draw:fill", "bitmap");
          list.insert("draw:fill-image", data.getBase64Data());
          list.insert("draw:fill-image-width", m_pattern.m_dim[0], librevenge::RVNG_POINT);
          list.insert("draw:fill-image-height", m_pattern.m_dim[1], librevenge::RVNG_POINT);
          list.insert("draw:fill-image-ref-point-x",0, librevenge::RVNG_POINT);
          list.insert("draw:fill-image-ref-point-y",0, librevenge::RVNG_POINT);
          list.insert("libwpg:mime-type", mimeType.c_str());
          done = true;
        }
      }
    }
    if (!done) {
      list.insert("draw:fill", "solid");
      list.insert("draw:fill-color", surfaceColor.str().c_str());
      list.insert("draw:opacity", surfaceOpacity, librevenge::RVNG_PERCENT);
    }
  }
  if (hasShadow()) {
    list.insert("draw:shadow", "vsible");
    list.insert("draw:shadow-color", m_shadowColor.str().c_str());
    list.insert("draw:shadow-opacity", m_shadowOpacity, librevenge::RVNG_PERCENT);
    // in cm
    list.insert("draw:shadow-offset-x", double(m_shadowOffset[0])/72.*2.54);
    list.insert("draw:shadow-offset-y", double(m_shadowOffset[1])/72.*2.54);
  }
}

int MWAWGraphicStyle::cmp(MWAWGraphicStyle const &a) const
{
  if (m_lineWidth < a.m_lineWidth) return -1;
  if (m_lineWidth > a.m_lineWidth) return 1;
  if (m_lineCap < a.m_lineCap) return -1;
  if (m_lineCap > a.m_lineCap) return 1;
  if (m_lineJoin < a.m_lineJoin) return -1;
  if (m_lineJoin > a.m_lineJoin) return 1;
  if (m_lineOpacity < a.m_lineOpacity) return -1;
  if (m_lineOpacity > a.m_lineOpacity) return 1;
  if (m_lineColor < a.m_lineColor) return -1;
  if (m_lineColor > a.m_lineColor) return 1;

  if (m_lineDashWidth.size() < a.m_lineDashWidth.size()) return -1;
  if (m_lineDashWidth.size() > a.m_lineDashWidth.size()) return 1;
  for (size_t d=0; d < m_lineDashWidth.size(); ++d) {
    if (m_lineDashWidth[d] > a.m_lineDashWidth[d]) return -1;
    if (m_lineDashWidth[d] < a.m_lineDashWidth[d]) return 1;
  }
  for (int i=0; i<2; ++i) {
    if (m_arrows[i]!=a.m_arrows[i])
      return m_arrows[i] ? 1 : -1;
    if (m_flip[i]!=a.m_flip[i])
      return m_flip[i] ? 1 : -1;
  }

  if (m_fillRuleEvenOdd != a.m_fillRuleEvenOdd) return m_fillRuleEvenOdd ? 1: -1;

  if (m_surfaceColor < a.m_surfaceColor) return -1;
  if (m_surfaceColor > a.m_surfaceColor) return 1;
  if (m_surfaceOpacity < a.m_surfaceOpacity) return -1;
  if (m_surfaceOpacity > a.m_surfaceOpacity) return 1;

  if (m_shadowColor < a.m_shadowColor) return -1;
  if (m_shadowColor > a.m_shadowColor) return 1;
  if (m_shadowOpacity < a.m_shadowOpacity) return -1;
  if (m_shadowOpacity > a.m_shadowOpacity) return 1;
  int diff=m_shadowOffset.cmp(a.m_shadowOffset);
  if (diff) return diff;

  diff = m_pattern.cmp(a.m_pattern);
  if (diff) return diff;

  if (m_gradientType < a.m_gradientType) return -1;
  if (m_gradientType > a.m_gradientType) return 1;
  if (m_gradientAngle < a.m_gradientAngle) return -1;
  if (m_gradientAngle > a.m_gradientAngle) return 1;
  if (m_gradientStopList.size() < a.m_gradientStopList.size()) return 1;
  if (m_gradientStopList.size() > a.m_gradientStopList.size()) return -1;
  for (size_t c=0; c < m_gradientStopList.size(); ++c) {
    diff = m_gradientStopList[c].cmp(m_gradientStopList[c]);
    if (diff) return diff;
  }
  if (m_gradientBorder < a.m_gradientBorder) return -1;
  if (m_gradientBorder > a.m_gradientBorder) return 1;
  diff=m_gradientPercentCenter.cmp(a.m_gradientPercentCenter);
  if (diff) return diff;
  if (m_gradientRadius < a.m_gradientRadius) return -1;
  if (m_gradientRadius > a.m_gradientRadius) return 1;
  if (m_rotate < a.m_rotate) return -1;
  if (m_rotate > a.m_rotate) return 1;
  return 0;
}

std::ostream &operator<<(std::ostream &o, MWAWGraphicStyle const &st)
{
  if (st.m_rotate<0 || st.m_rotate>0)
    o << "rot=" << st.m_rotate << ",";
  if (st.m_flip[0]) o << "flipX,";
  if (st.m_flip[1]) o << "flipY,";
  o << "line=[";
  if (st.m_lineWidth<1 || st.m_lineWidth>1)
    o << "width=" << st.m_lineWidth << ",";
  if (!st.m_lineDashWidth.empty()) {
    o << "dash=[";
    for (size_t d=0; d < st.m_lineDashWidth.size(); ++d)
      o << st.m_lineDashWidth[d] << ",";
    o << "],";
  }
  switch (st.m_lineCap) {
  case MWAWGraphicStyle::C_Square:
    o << "cap=square,";
    break;
  case MWAWGraphicStyle::C_Round:
    o << "cap=round,";
    break;
  case MWAWGraphicStyle::C_Butt:
  default:
    break;
  }
  switch (st.m_lineJoin) {
  case MWAWGraphicStyle::J_Bevel:
    o << "join=bevel,";
    break;
  case MWAWGraphicStyle::J_Round:
    o << "join=round,";
    break;
  case MWAWGraphicStyle::J_Miter:
  default:
    break;
  }
  if (st.m_lineOpacity<1)
    o << "opacity=" << st.m_lineOpacity << ",";
  if (!st.m_lineColor.isBlack())
    o << "color=" << st.m_lineColor << ",";
  if (st.m_arrows[0]) o << "arrow[start],";
  if (st.m_arrows[1]) o << "arrow[end],";
  o << "],";
  if (st.hasSurfaceColor()) {
    o << "surf=[";
    if (!st.m_surfaceColor.isWhite())
      o << "color=" << st.m_surfaceColor << ",";
    if (st.m_surfaceOpacity > 0)
      o << "opacity=" << st.m_surfaceOpacity << ",";
    o << "],";
    if (st.m_fillRuleEvenOdd)
      o << "fill[evenOdd],";
  }
  if (st.hasPattern())
    o << "pattern=[" << st.m_pattern << "],";
  if (st.hasGradient()) {
    o << "grad=[";
    switch (st.m_gradientType) {
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
    if (st.m_gradientAngle>0 || st.m_gradientAngle<0) o << "angle=" << st.m_gradientAngle << ",";
    if (st.m_gradientStopList.size() >= 2) {
      o << "stops=[";
      for (size_t s=0; s < st.m_gradientStopList.size(); ++s)
        o << "[" << st.m_gradientStopList[s] << "],";
      o << "],";
    }
    if (st.m_gradientBorder>0) o << "border=" << st.m_gradientBorder*100 << "%,";
    if (st.m_gradientPercentCenter != Vec2f(0.5f,0.5f)) o << "center=" << st.m_gradientPercentCenter << ",";
    if (st.m_gradientRadius<1) o << "radius=" << st.m_gradientRadius << ",";
    o << "],";
  }
  if (st.hasShadow()) {
    o << "shadow=[";
    if (!st.m_shadowColor.isBlack())
      o << "color=" << st.m_shadowColor << ",";
    if (st.m_shadowOpacity > 0)
      o << "opacity=" << st.m_shadowOpacity << ",";
    o << "offset=" << st.m_shadowOffset << ",";
    o << "],";
  }
  o << st.m_extra;
  return o;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
