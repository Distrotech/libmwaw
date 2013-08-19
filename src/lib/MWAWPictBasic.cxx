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

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include <libwpd/libwpd.h>
#include <libmwaw/libmwaw.hxx>

#include "libmwaw_internal.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWPictBasic.hxx"

////////////////////////////////////////////////////////////
// style
////////////////////////////////////////////////////////////
void MWAWPictBasic::Style::addTo(WPXPropertyList &list, bool only1D) const
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
          MWAW_DEBUG_MSG(("MWAWPictBasic::Style::addTo: can set all dash\n"));
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
    list.insert("draw:dots1-length", size1, WPX_POINT);
    if (nDots2) {
      list.insert("draw:dots2", nDots2);
      list.insert("draw:dots2-length", size2, WPX_POINT);
    }
    list.insert("draw:distance", totalGap/float(nDots1+nDots2), WPX_POINT);;
  } else
    list.insert("draw:stroke", "solid");
  list.insert("svg:stroke-color", m_lineColor.str().c_str());
  list.insert("svg:stroke-width", m_lineWidth,WPX_POINT);

  if (m_lineOpacity < 1)
    list.insert("svg:stroke-opacity", m_lineOpacity, WPX_PERCENT);
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

  if (only1D || (!hasSurface()&&!hasGradient())) {
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
    list.insert("draw:start-color", m_gradientColor[0].str().c_str());
    list.insert("libwpg:start-opacity", m_gradientOpacity[0], WPX_PERCENT);
    list.insert("draw:end-color", m_gradientColor[1].str().c_str());
    list.insert("libwpg:end-opacity", m_gradientOpacity[1], WPX_PERCENT);

    list.insert("draw:border", m_gradientBorder, WPX_PERCENT);
    if (m_gradientType != G_Linear) {
      list.insert("svg:cx", m_gradientPercentCenter[0], WPX_PERCENT);
      list.insert("svg:cy", m_gradientPercentCenter[1], WPX_PERCENT);
    }
    if (m_gradientType == G_Radial)
      list.insert("svg:r", m_gradientRadius, WPX_PERCENT); // checkme
  } else {
    list.insert("draw:fill", "solid");
    list.insert("draw:fill-color", m_surfaceColor.str().c_str());
    list.insert("draw:opacity", m_surfaceOpacity, WPX_PERCENT);
  }
  if (hasShadow()) {
    list.insert("draw:shadow", "vsible");
    list.insert("draw:shadow-color", m_shadowColor.str().c_str());
    list.insert("draw:shadow-opacity", m_shadowOpacity, WPX_PERCENT);
    // in cm
    list.insert("draw:shadow-offset-x", double(m_shadowOffset[0])/72.*2.54);
    list.insert("draw:shadow-offset-y", double(m_shadowOffset[1])/72.*2.54);
  }
}

int MWAWPictBasic::Style::cmp(MWAWPictBasic::Style const &a) const
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

  if (m_gradientType < a.m_gradientType) return -1;
  if (m_gradientType > a.m_gradientType) return 1;
  for (int i=0; i<2; ++i) {
    if (m_gradientColor[i] < a.m_gradientColor[i]) return -1;
    if (m_gradientColor[i] > a.m_gradientColor[i]) return 1;
    if (m_gradientOpacity[i] < a.m_gradientOpacity[i]) return -1;
    if (m_gradientOpacity[i] > a.m_gradientOpacity[i]) return 1;
  }
  if (m_gradientBorder < a.m_gradientBorder) return -1;
  if (m_gradientBorder > a.m_gradientBorder) return 1;
  diff=m_gradientPercentCenter.cmp(a.m_gradientPercentCenter);
  if (diff) return diff;
  if (m_gradientRadius < a.m_gradientRadius) return -1;
  if (m_gradientRadius > a.m_gradientRadius) return 1;
  return 0;
}

std::ostream &operator<<(std::ostream &o, MWAWPictBasic::Style const st)
{
  o << "line=[";
  if (st.m_lineWidth<1 || st.m_lineWidth>1)
    o << "width=" << st.m_lineWidth << ",";
  if (!st.m_lineDashWidth.empty()) {
    o << "dash=[";
    for (size_t d=0; st.m_lineDashWidth.size(); ++d)
      o << st.m_lineDashWidth[d] << ",";
    o << "],";
  }
  switch (st.m_lineCap) {
  case MWAWPictBasic::Style::C_Square:
    o << "cap=square,";
    break;
  case MWAWPictBasic::Style::C_Round:
    o << "cap=round,";
    break;
  case MWAWPictBasic::Style::C_Butt:
  default:
    break;
  }
  switch (st.m_lineJoin) {
  case MWAWPictBasic::Style::J_Bevel:
    o << "join=bevel,";
    break;
  case MWAWPictBasic::Style::J_Round:
    o << "join=round,";
    break;
  case MWAWPictBasic::Style::J_Miter:
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
  if (st.hasSurface()) {
    o << "surf=[";
    if (!st.m_surfaceColor.isWhite())
      o << "color=" << st.m_surfaceColor << ",";
    if (st.m_surfaceOpacity > 0)
      o << "opacity=" << st.m_surfaceOpacity << ",";
    o << "],";
    if (st.m_fillRuleEvenOdd)
      o << "fill[evenOdd],";
  }
  if (st.hasGradient()) {
    o << "grad=[";
    switch (st.m_gradientType) {
    case MWAWPictBasic::Style::G_Axial:
      o << "axial,";
      break;
    case MWAWPictBasic::Style::G_Linear:
      o << "linear,";
      break;
    case MWAWPictBasic::Style::G_Radial:
      o << "radial,";
      break;
    case MWAWPictBasic::Style::G_Rectangular:
      o << "rectangular,";
      break;
    case MWAWPictBasic::Style::G_Square:
      o << "square,";
      break;
    case MWAWPictBasic::Style::G_Ellipsoid:
      o << "ellipsoid,";
      break;
    case MWAWPictBasic::Style::G_None:
    default:
      break;
    }
    for (int i=0; i<2; ++i)
      o << "col" << i << st.m_gradientColor[i] << "[" << st.m_gradientOpacity[i]*100 << "%],";
    if (st.m_gradientBorder>0) o << "border=" << st.m_gradientBorder*100 << "%,";
    if (st.m_gradientPercentCenter != Vec2f(0.5f,0.5f)) o << "center=" << st.m_gradientPercentCenter << ",";
    if (st.m_gradientRadius>0) o << "radius=" << st.m_gradientRadius << ",";
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
  return o;
}

////////////////////////////////////////////////////////////
// virtual class
////////////////////////////////////////////////////////////
bool MWAWPictBasic::getODGBinary(WPXBinaryData &res) const
{
  MWAWPropertyHandlerEncoder doc;
  startODG(doc);
  if (!getODGBinary(doc, getBdBox()[0]))
    return false;
  endODG(doc);
  return doc.getData(res);
}

void MWAWPictBasic::startODG(MWAWPropertyHandlerEncoder &doc) const
{
  Box2f bdbox = getBdBox();
  Vec2f size=bdbox.size();
  WPXPropertyList list;

  list.clear();
  list.insert("svg:x",0, WPX_POINT);
  list.insert("svg:y",0, WPX_POINT);
  list.insert("svg:width",size.x(), WPX_POINT);
  list.insert("svg:height",size.y(), WPX_POINT);
  list.insert("libwpg:enforce-frame",1);
  doc.startElement("Graphics", list);
  list.clear();
  getGraphicStyleProperty(list);
  doc.startElement("SetStyle", list, WPXPropertyListVector());
  doc.endElement("SetStyle");
}
void MWAWPictBasic::endODG(MWAWPropertyHandlerEncoder &doc) const
{
  doc.endElement("Graphics");
}

////////////////////////////////////////////////////////////
//
//    MWAWPictLine
//
////////////////////////////////////////////////////////////
bool MWAWPictLine::getODGBinary(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const
{
  WPXPropertyList list;
  WPXPropertyListVector vect;
  list.clear();
  Vec2f pt=m_extremity[0]-orig;
  list.insert("svg:x",pt.x(), WPX_POINT);
  list.insert("svg:y",pt.y(), WPX_POINT);
  vect.append(list);
  pt=m_extremity[1]-orig;
  list.insert("svg:x",pt.x(), WPX_POINT);
  list.insert("svg:y",pt.y(), WPX_POINT);
  vect.append(list);
  doc.startElement("Polyline", WPXPropertyList(), vect);
  doc.endElement("Polyline");
  return true;
}

void MWAWPictLine::getGraphicStyleProperty(WPXPropertyList &list) const
{
  m_style.addTo(list, true);
}

////////////////////////////////////////////////////////////
//
//    MWAWPictRectangle
//
////////////////////////////////////////////////////////////
// to do: see how to manage the round corner
bool MWAWPictRectangle::getODGBinary(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const
{
  WPXPropertyList list;
  list.clear();
  Vec2f pt=m_rectBox[0]-orig;
  list.insert("svg:x",pt.x(), WPX_POINT);
  list.insert("svg:y",pt.y(), WPX_POINT);
  pt=m_rectBox[1]-m_rectBox[0];
  list.insert("svg:width",pt.x(), WPX_POINT);
  list.insert("svg:height",pt.y(), WPX_POINT);
  if (m_cornerWidth[0] > 0 && m_cornerWidth[1] > 0) {
    list.insert("svg:rx",double(m_cornerWidth[0]), WPX_POINT);
    list.insert("svg:ry",double(m_cornerWidth[1]), WPX_POINT);
  }
  doc.startElement("Rectangle", list);
  doc.endElement("Rectangle");

  return true;
}

void MWAWPictRectangle::getGraphicStyleProperty(WPXPropertyList &list) const
{
  m_style.addTo(list);
}

////////////////////////////////////////////////////////////
//
//    MWAWPictCircle
//
////////////////////////////////////////////////////////////
bool MWAWPictCircle::getODGBinary(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const
{
  WPXPropertyList list;
  list.clear();
  Vec2f pt=0.5*(m_circleBox[0]+m_circleBox[1])-orig;
  list.insert("svg:cx",pt.x(), WPX_POINT);
  list.insert("svg:cy",pt.y(), WPX_POINT);
  pt=0.5*(m_circleBox[1]-m_circleBox[0]);
  list.insert("svg:rx",pt.x(), WPX_POINT);
  list.insert("svg:ry",pt.y(), WPX_POINT);
  doc.startElement("Ellipse", list);
  doc.endElement("Ellipse");

  return true;
}

void MWAWPictCircle::getGraphicStyleProperty(WPXPropertyList &list) const
{
  m_style.addTo(list);
}

////////////////////////////////////////////////////////////
//
//    MWAWPictArc
//
////////////////////////////////////////////////////////////
bool MWAWPictArc::getODGBinary(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const
{
  Vec2f center=0.5*(m_circleBox[0]+m_circleBox[1])-orig;
  Vec2f rad=0.5*(m_circleBox[1]-m_circleBox[0]);
  float angl0=m_angle[0];
  float angl1=m_angle[1];
  if (rad[1]<0) {
    static bool first=true;
    if (first) {
      MWAW_DEBUG_MSG(("MWAWPictArc::getODGBinary: oops radiusY is negative, inverse it\n"));
      first=false;
    }
    rad[1]=-rad[1];
  }
  while (angl1<angl0)
    angl1+=360.f;
  while (angl1>angl0+360.f)
    angl1-=360.f;
  if (angl1-angl0>=180.f && angl1-angl0<=180.f)
    angl1+=0.01;
  WPXPropertyListVector vect;
  WPXPropertyList list;
  float angl=angl0*float(M_PI/180.);
  Vec2f pt=center+Vec2f(std::cos(angl)*rad[0],-std::sin(angl)*rad[1]);
  list.insert("libwpg:path-action", "M");
  list.insert("svg:x",pt.x(), WPX_POINT);
  list.insert("svg:y",pt.y(), WPX_POINT);
  vect.append(list);

  list.clear();
  angl=angl1*float(M_PI/180.);
  pt=center+Vec2f(std::cos(angl)*rad[0],-std::sin(angl)*rad[1]);
  list.insert("libwpg:path-action", "A");
  list.insert("libwpg:large-arc", (angl1-angl0<180.f)?0:1);
  list.insert("libwpg:sweep", 0);
  list.insert("svg:rx",rad.x(), WPX_POINT);
  list.insert("svg:ry",rad.y(), WPX_POINT);
  list.insert("svg:x",pt.x(), WPX_POINT);
  list.insert("svg:y",pt.y(), WPX_POINT);
  vect.append(list);
  doc.startElement("Path", WPXPropertyList(), vect);
  doc.endElement("Path");

  return true;
}

void MWAWPictArc::getGraphicStyleProperty(WPXPropertyList &list) const
{
  m_style.addTo(list);
}

////////////////////////////////////////////////////////////
//
//    MWAWPictPath
//
////////////////////////////////////////////////////////////
int MWAWPictPath::cmp(MWAWPict const &a) const
{
  int diff = MWAWPictBasic::cmp(a);
  if (diff) return diff;
  MWAWPictPath const &aPath = static_cast<MWAWPictPath const &>(a);
  if (m_path.count() != aPath.m_path.count())
    return m_path.count()<aPath.m_path.count() ? -1 : 1;
  for (unsigned long c=0; c < m_path.count(); c++) {
    WPXPropertyList::Iter it1(m_path[c]);
    it1.rewind();
    WPXPropertyList::Iter it2(aPath.m_path[c]);
    it2.rewind();
    while(1) {
      if (!it1.next()) {
        if (it2.next()) return 1;
        break;
      }
      if (!it2.next())
        return -1;
      diff=strcmp(it1.key(),it2.key());
      if (diff) return diff;
      diff=strcmp(it1()->getStr().cstr(),it2()->getStr().cstr());
      if (diff) return diff;
    }
  }
  return 0;
}

bool MWAWPictPath::getODGBinary(MWAWPropertyHandlerEncoder &doc, Vec2f const &) const
{
  if (!m_path.count()) {
    MWAW_DEBUG_MSG(("MWAWPictPath::getODGBinary: the path is not defined\n"));
    return false;
  }

  doc.startElement("Path", WPXPropertyList(), m_path);
  doc.endElement("Path");

  return true;
}

void MWAWPictPath::getGraphicStyleProperty(WPXPropertyList &list) const
{
  m_style.addTo(list);
}

////////////////////////////////////////////////////////////
//
//    MWAWPictPolygon
//
////////////////////////////////////////////////////////////
bool MWAWPictPolygon::getODGBinary(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const
{
  size_t numPt = m_verticesList.size();
  if (numPt < 2) {
    MWAW_DEBUG_MSG(("MWAWPictPolygon::getODGBinary: can not draw a polygon with %ld vertices\n", long(numPt)));
    return false;
  }

  WPXPropertyListVector vect;
  for (size_t i = 0; i < numPt; i++) {
    WPXPropertyList list;
    Vec2f pt=m_verticesList[i]-orig;
    list.insert("svg:x", pt.x()/72., WPX_INCH);
    list.insert("svg:y", pt.y()/72., WPX_INCH);
    vect.append(list);
  }
  if (!m_style.hasSurface()) {
    doc.startElement("Polyline", WPXPropertyList(), vect);
    doc.endElement("Polyline");
  } else {
    doc.startElement("Polygon", WPXPropertyList(), vect);
    doc.endElement("Polygon");
  }
  return true;
}

void MWAWPictPolygon::getGraphicStyleProperty(WPXPropertyList &list) const
{
  m_style.addTo(list);
}

////////////////////////////////////////////////////////////
//
//    MWAWPictGroup
//
////////////////////////////////////////////////////////////
void MWAWPictGroup::addChild(shared_ptr<MWAWPictBasic> child)
{
  if (!child) {
    MWAW_DEBUG_MSG(("MWAWPictGroup::addChild: called without child\n"));
    return;
  }
  Box2f cBDBox=child->getBdBox();
  if (m_child.empty())
    setBdBox(cBDBox);
  else {
    Box2f bdbox=getBdBox();
    Vec2f pt=bdbox[0];
    if (cBDBox[0][0]<pt[0]) pt[0]=cBDBox[0][0];
    if (cBDBox[0][1]<pt[1]) pt[1]=cBDBox[0][1];
    bdbox.setMin(pt);

    pt=bdbox[1];
    if (cBDBox[1][0]>pt[0]) pt[0]=cBDBox[1][0];
    if (cBDBox[1][1]>pt[1]) pt[1]=cBDBox[1][1];
    bdbox.setMax(pt);
    setBdBox(bdbox);
  }
  m_child.push_back(child);
}

void MWAWPictGroup::getGraphicStyleProperty(WPXPropertyList &) const
{
}

bool MWAWPictGroup::getODGBinary(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const
{
  if (m_child.empty())
    return false;
  for (size_t c=0; c < m_child.size(); ++c) {
    if (!m_child[c]) continue;
    m_child[c]->getODGBinary(doc, orig);
  }
  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
