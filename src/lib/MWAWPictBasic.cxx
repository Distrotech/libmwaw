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
#include <map>
#include <iostream>
#include <sstream>
#include <string>

#include <libwpd/libwpd.h>
#include <libmwaw/libmwaw.hxx>

#include "libmwaw_internal.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWPictBasic.hxx"

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

  WPXPropertyListVector gradient;
  list.clear();
  getGraphicStyleProperty(list, gradient);
  doc.startElement("SetStyle", list, gradient);
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

void MWAWPictLine::getGraphicStyleProperty(WPXPropertyList &list, WPXPropertyListVector &gradient) const
{
  m_style.addTo(list, gradient, true);
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

void MWAWPictRectangle::getGraphicStyleProperty(WPXPropertyList &list, WPXPropertyListVector &gradient) const
{
  m_style.addTo(list, gradient);
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

void MWAWPictCircle::getGraphicStyleProperty(WPXPropertyList &list, WPXPropertyListVector &gradient) const
{
  m_style.addTo(list, gradient);
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

void MWAWPictArc::getGraphicStyleProperty(WPXPropertyList &list, WPXPropertyListVector &gradient) const
{
  m_style.addTo(list, gradient);
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
  unsigned long n=m_path.count();
  if (!n) {
    MWAW_DEBUG_MSG(("MWAWPictPath::getODGBinary: the path is not defined\n"));
    return false;
  }

  if ((m_style.hasGradient() || m_style.hasSurface()) &&
      (!m_path[n-1]["libwpg:path-action"] || m_path[n-1]["libwpg:path-action"]->getStr() != "Z")) {
    // odg need a closed path to draw surface, so ...
    WPXPropertyListVector path(m_path);
    WPXPropertyList list;
    list.insert("libwpg:path-action", "Z");
    path.append(list);
    doc.startElement("Path", WPXPropertyList(), path);
    doc.endElement("Path");
  } else {
    doc.startElement("Path", WPXPropertyList(), m_path);
    doc.endElement("Path");
  }

  return true;
}

void MWAWPictPath::getGraphicStyleProperty(WPXPropertyList &list, WPXPropertyListVector &gradient) const
{
  m_style.addTo(list, gradient);
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
  if (!m_style.hasSurface() && !m_style.hasGradient()) {
    doc.startElement("Polyline", WPXPropertyList(), vect);
    doc.endElement("Polyline");
  } else {
    doc.startElement("Polygon", WPXPropertyList(), vect);
    doc.endElement("Polygon");
  }
  return true;
}

void MWAWPictPolygon::getGraphicStyleProperty(WPXPropertyList &list, WPXPropertyListVector &gradient) const
{
  m_style.addTo(list, gradient);
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

void MWAWPictGroup::getGraphicStyleProperty(WPXPropertyList &, WPXPropertyListVector &) const
{
}

bool MWAWPictGroup::getODGBinary(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const
{
  if (m_child.empty())
    return false;
  // first check if we need to use different layer
  std::multimap<int,size_t> idByLayerMap;
  for (size_t c=0; c < m_child.size(); ++c) {
    if (!m_child[c]) continue;
    idByLayerMap.insert(std::multimap<int,size_t>::value_type(m_child[c]->getLayer(), c));
  }
  if (idByLayerMap.size() <= 1) {
    for (size_t c=0; c < m_child.size(); ++c) {
      if (!m_child[c]) continue;
      m_child[c]->getODGBinary(doc, orig);
    }
    return true;
  }
  std::multimap<int,size_t>::const_iterator it=idByLayerMap.begin();
  while(it != idByLayerMap.end()) {
    int layer=it->first;
    WPXPropertyList list;
    list.insert("svg:id", layer);
    doc.startElement("Layer", list);
    while (it != idByLayerMap.end() && it->first==layer)
      m_child[it++->second]->getODGBinary(doc,orig);
    doc.endElement("Layer");
  }
  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
