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
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include <libwpd/libwpd.h>
#include <libmwaw/libmwaw.hxx>

#include "libmwaw_internal.hxx"

#include "MWAWGraphicStyle.hxx"

#include "MWAWGraphicShape.hxx"

////////////////////////////////////////////////////////////
// MWAWGraphicShape::PathData
////////////////////////////////////////////////////////////
std::ostream &operator<<(std::ostream &o, MWAWGraphicShape::PathData const &path)
{
  o << path.m_type;
  switch(path.m_type) {
  case 'H':
    o << ":" << path.m_x[0];
  case 'V':
    o << ":" << path.m_x[1];
  case 'M':
  case 'L':
  case 'T':
    o << ":" << path.m_x;
    break;
  case 'Q':
  case 'S':
    o << ":" << path.m_x << ":" << path.m_x1;
    break;
  case 'C':
    o << ":" << path.m_x << ":" << path.m_x1 << ":" << path.m_x2;
    break;
  case 'A':
    o << ":" << path.m_x << ":r=" << path.m_r;
    if (path.m_largeAngle) o << ":largeAngle";
    if (path.m_sweep) o << ":sweep";
    if (path.m_rotate<0 || path.m_rotate>0) o << ":rot=" << path.m_rotate;
  case 'Z':
    break;
  default:
    o << "###";
  }
  return o;
}

bool MWAWGraphicShape::PathData::get(WPXPropertyList &list, Vec2f const &orig) const
{
  list.clear();
  std::string type("");
  type += m_type;
  list.insert("libwpg:path-action", type.c_str());
  if (m_type=='Z')
    return true;
  if (m_type=='H') {
    list.insert("svg:x",m_x[0]-orig[0], WPX_POINT);
    return true;
  }
  if (m_type=='V') {
    list.insert("svg:y",m_x[1]-orig[1], WPX_POINT);
    return true;
  }
  list.insert("svg:x",m_x[0]-orig[0], WPX_POINT);
  list.insert("svg:y",m_x[1]-orig[1], WPX_POINT);
  if (m_type=='M' || m_type=='L' || m_type=='T')
    return true;
  if (m_type=='A') {
    list.insert("svg:rx",m_r[0], WPX_POINT);
    list.insert("svg:ry",m_r[1], WPX_POINT);
    list.insert("libwpg:large-arc", m_largeAngle ? 1 : 0);
    list.insert("libwpg:sweep", m_sweep ? 1 : 0);
    list.insert("libwpg:rotate", m_rotate);
    return true;
  }
  list.insert("svg:x1",m_x1[0]-orig[0], WPX_POINT);
  list.insert("svg:y1",m_x1[1]-orig[1], WPX_POINT);
  if (m_type=='Q' || m_type=='S')
    return true;
  list.insert("svg:x2",m_x2[0]-orig[0], WPX_POINT);
  list.insert("svg:y2",m_x2[1]-orig[1], WPX_POINT);
  if (m_type=='C')
    return true;
  MWAW_DEBUG_MSG(("MWAWGraphicShape::PathData::get: unknown command %c\n", m_type));
  list.clear();
  return false;
}

int MWAWGraphicShape::PathData::cmp(MWAWGraphicShape::PathData const &a) const
{
  if (m_type < a.m_type) return 1;
  if (m_type > a.m_type) return 1;
  int diff = m_x.cmp(a.m_x);
  if (diff) return diff;
  diff = m_x1.cmp(a.m_x1);
  if (diff) return diff;
  diff = m_x2.cmp(a.m_x2);
  if (diff) return diff;
  diff = m_r.cmp(a.m_r);
  if (diff) return diff;
  if (m_rotate < a.m_rotate) return 1;
  if (m_rotate > a.m_rotate) return -1;
  if (m_largeAngle != a.m_largeAngle)
    return m_largeAngle ? 1 : -1;
  if (m_sweep != a.m_sweep)
    return m_sweep ? 1 : -1;
  return 0;
}

////////////////////////////////////////////////////////////
// MWAWGraphicShape
////////////////////////////////////////////////////////////
MWAWGraphicShape MWAWGraphicShape::line(Vec2f const &orig, Vec2f const &dest)
{
  MWAWGraphicShape res;
  res.m_type = MWAWGraphicShape::Line;
  res.m_vertices.resize(2);
  res.m_vertices[0]=orig;
  res.m_vertices[1]=dest;
  Vec2f minPt(orig), maxPt(orig);
  for (int c=0; c<2; ++c) {
    if (orig[c] < dest[c])
      maxPt[c]=dest[c];
    else
      minPt[c]=dest[c];
  }
  res.m_bdBox=Box2f(minPt,maxPt);
  return res;
}

std::ostream &operator<<(std::ostream &o, MWAWGraphicShape const &sh)
{
  o << "box=" << o << sh.m_bdBox << ",";
  switch(sh.m_type) {
  case MWAWGraphicShape::Line:
    o << "line,";
    if (sh.m_vertices.size()!=2)
      o << "###pts,";
    else
      o << "pts=" << sh.m_vertices[0] << "<->" << sh.m_vertices[1] << ",";
    break;
  case MWAWGraphicShape::Rectangle:
    o << "rect,";
    if (sh.m_formBox!=sh.m_bdBox)
      o << "box[rect]=" << sh.m_formBox << ",";
    if (sh.m_cornerWidth!=Vec2f(0,0))
      o << "corners=" << sh.m_cornerWidth << ",";
    break;
  case MWAWGraphicShape::Circle:
    o << "circle,";
    break;
  case MWAWGraphicShape::Arc:
    o << "arc,";
    o << "box[ellipse]=" << sh.m_formBox << ",";
    o << "angle=" << sh.m_arcAngles << ",";
    break;
  case MWAWGraphicShape::Polygon:
    o << "polygons,pts=[";
    for (size_t pt=0; pt < sh.m_vertices.size(); ++pt)
      o << sh.m_vertices[pt] << ",";
    o << "],";
    break;
  case MWAWGraphicShape::Path:
    o << "path,pts=[";
    for (size_t pt=0; pt < sh.m_path.size(); ++pt)
      o << sh.m_path[pt] << ",";
    o << "],";
    break;
  case MWAWGraphicShape::ShapeUnknown:
  default:
    o << "###unknwown[shape],";
    break;
  }
  o << sh.m_extra;
  return o;
}

int MWAWGraphicShape::cmp(MWAWGraphicShape const &a) const
{
  if (m_type < a.m_type) return 1;
  if (m_type > a.m_type) return -1;
  int diff = m_bdBox.cmp(a.m_bdBox);
  if (diff) return diff;
  diff = m_formBox.cmp(a.m_formBox);
  if (diff) return diff;
  diff = m_cornerWidth.cmp(a.m_cornerWidth);
  if (diff) return diff;
  diff = m_arcAngles.cmp(a.m_arcAngles);
  if (diff) return diff;
  if (m_vertices.size()<a.m_vertices.size()) return -1;
  if (m_vertices.size()>a.m_vertices.size()) return -1;
  for (size_t pt=0; pt < m_vertices.size(); ++pt) {
    diff = m_vertices[pt].cmp(a.m_vertices[pt]);
    if (diff) return diff;
  }
  if (m_path.size()<a.m_path.size()) return -1;
  if (m_path.size()>a.m_path.size()) return -1;
  for (size_t pt=0; pt < m_path.size(); ++pt) {
    diff = m_path[pt].cmp(a.m_path[pt]);
    if (diff) return diff;
  }
  return 0;
}

Box2f MWAWGraphicShape::getBdBox(MWAWGraphicStyle const &style) const
{
  Box2f bdBox=m_bdBox;
  if (style.hasLine())
    bdBox.extend(style.m_lineWidth/2.f);
  if (m_type==Line) {
    // fixme: add 4pt for each arrows
    int numArrows=(style.m_arrows[0] ? 1 : 0)+(style.m_arrows[1] ? 1 : 0);
    if (numArrows) bdBox.extend(float(2*numArrows));
  }
  return bdBox;
}

bool MWAWGraphicShape::send(MWAWPropertyHandlerEncoder &doc, MWAWGraphicStyle const &style, Vec2f const &orig) const
{
  Vec2f pt;
  WPXPropertyList list;
  WPXPropertyListVector vect;
  style.addTo(list, vect, m_type==Line);
  doc.startElement("SetStyle", list, vect);
  doc.endElement("SetStyle");

  list.clear();
  vect=WPXPropertyListVector();
  switch(m_type) {
  case Line:
    if (m_vertices.size()!=2) break;
    pt=m_vertices[0]-orig;
    list.insert("svg:x",pt.x(), WPX_POINT);
    list.insert("svg:y",pt.y(), WPX_POINT);
    vect.append(list);
    pt=m_vertices[1]-orig;
    list.insert("svg:x",pt.x(), WPX_POINT);
    list.insert("svg:y",pt.y(), WPX_POINT);
    vect.append(list);
    doc.startElement("Polyline", WPXPropertyList(), vect);
    doc.endElement("Polyline");
    return true;
  case Rectangle:
    if (m_cornerWidth[0] > 0 && m_cornerWidth[1] > 0) {
      list.insert("svg:rx",double(m_cornerWidth[0]), WPX_POINT);
      list.insert("svg:ry",double(m_cornerWidth[1]), WPX_POINT);
    }
  case Circle:
    pt=m_formBox[0]-orig;
    list.insert("svg:x",pt.x(), WPX_POINT);
    list.insert("svg:y",pt.y(), WPX_POINT);
    pt=m_formBox.size();
    list.insert("svg:width",pt.x(), WPX_POINT);
    list.insert("svg:height",pt.y(), WPX_POINT);
    if (m_type==Rectangle) {
      doc.startElement("Rectangle", list);
      doc.endElement("Rectangle");
    } else {
      doc.startElement("Ellipse", list);
      doc.endElement("Ellipse");
    }
    return true;
  case Arc: {
    Vec2f center=0.5*(m_formBox[0]+m_formBox[1])-orig;
    Vec2f rad=0.5*(m_formBox[1]-m_formBox[0]);
    float angl0=m_arcAngles[0];
    float angl1=m_arcAngles[1];
    if (rad[1]<0) {
      static bool first=true;
      if (first) {
        MWAW_DEBUG_MSG(("MWAWGraphicShape::send: oops radiusY for arc is negative, inverse it\n"));
        first=false;
      }
      rad[1]=-rad[1];
    }
    while (angl1<angl0)
      angl1+=360.f;
    while (angl1>angl0+360.f)
      angl1-=360.f;
    if (angl1-angl0>=180.f && angl1-angl0<=180.f)
      angl1+=0.01f;
    float angl=angl0*float(M_PI/180.);
    pt=center+Vec2f(std::cos(angl)*rad[0],-std::sin(angl)*rad[1]);
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
  case Polygon: {
    size_t n=m_vertices.size();
    if (n<2) break;
    for (size_t i = 0; i < n; ++i) {
      list.clear();
      pt=m_vertices[i]-orig;
      list.insert("svg:x", pt.x(), WPX_POINT);
      list.insert("svg:y", pt.y(), WPX_POINT);
      vect.append(list);
    }
    if (!style.hasSurface()) {
      doc.startElement("Polyline", WPXPropertyList(), vect);
      doc.endElement("Polyline");
    } else {
      doc.startElement("Polygon", WPXPropertyList(), vect);
      doc.endElement("Polygon");
    }
    return true;
  }
  case Path: {
    size_t n=m_path.size();
    if (!n) break;
    for (size_t c=0; c < n; ++c) {
      list.clear();
      if (m_path[c].get(list, orig))
        vect.append(list);
    }
    if (style.hasSurface() && m_path[n-1].m_type != 'Z') {
      // odg need a closed path to draw surface, so ...
      list.clear();
      list.insert("libwpg:path-action", "Z");
      vect.append(list);
    }
    doc.startElement("Path", WPXPropertyList(), vect);
    doc.endElement("Path");
    return true;
  }
  case ShapeUnknown:
  default:
    break;
  }
  MWAW_DEBUG_MSG(("MWAWGraphicShape::send: can not send a shape with type=%d\n", int(m_type)));
  return false;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:

