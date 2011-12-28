/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libmwaw
 * Copyright (C) 2009, 2011 Alonso Laurent (alonso@loria.fr)
 * Copyright (C) 2006, 2007 Andrew Ziem
 * Copyright (C) 2004-2006 Fridrich Strba (fridrich.strba@bluewin.ch)
 * Copyright (C) 2004 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2003-2005 William Lachance (william.lachance@sympatico.ca)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 * For further information visit http://libmwaw.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

/* This header contains code specific to a pict mac file
 */
#include <string.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include <libwpd/WPXBinaryData.h>
#include <libwpd/WPXProperty.h>
#include <libwpd/WPXPropertyList.h>
#include <libwpd/WPXString.h>

#include "libmwaw_tools.hxx"
#include "TMWAWPropertyHandler.hxx"
#include "TMWAWInputStream.hxx"

#include "TMWAWPictBasic.hxx"

namespace libmwaw_tools
{
//! Internal: creates the string "f pt"
static std::string getStringPt(float f)
{
  std::stringstream s;
  s << f << "pt";
  return s.str();
}

void PictBasic::startODG(TMWAWPropertyHandlerEncoder &doc) const
{
  Box2f bdbox = getBdBox();
  Vec2f size=bdbox.size();
  WPXPropertyList list;
  std::stringstream s;

  list.clear();
  list.insert("w",getStringPt(size.x()).c_str());
  list.insert("h",getStringPt(size.y()).c_str());
  doc.startElement("libmwaw:document", list);
  list.clear();
  getGraphicStyleProperty(list);
  doc.startElement("libmwaw:graphicStyle", list);
  doc.endElement("libmwaw:graphicStyle");
}
void PictBasic::endODG(TMWAWPropertyHandlerEncoder &doc) const
{
  doc.endElement("libmwaw:document");
}
void PictBasic::getStyle1DProperty(WPXPropertyList &list) const
{
  list.clear();
  list.insert("lineFill", "solid");
  std::stringstream s;
  s << std::hex << std::setfill('0') << "#"
    << std::setw(2) << m_lineColor[0]
    << std::setw(2) << m_lineColor[1]
    << std::setw(2) << m_lineColor[2];
  list.insert("lineColor", s.str().c_str());
  list.insert("lineWidth", getStringPt(m_lineWidth).c_str());
}

void PictBasic::getStyle2DProperty(WPXPropertyList &list) const
{
  PictBasic::getStyle1DProperty(list);
  list.insert("surfaceFill", "none");
  std::stringstream s;
  s << std::hex << std::setfill('0') << "#"
    << std::setw(2) << m_surfaceColor[0]
    << std::setw(2) << m_surfaceColor[1]
    << std::setw(2) << m_surfaceColor[2];
  list.insert("surfaceColor", s.str().c_str());
}


////////////////////////////////////////////////////////////
//
//    PictLine
//
////////////////////////////////////////////////////////////
bool PictLine::getODGBinary(WPXBinaryData &res) const
{
  Box2f bdbox = getBdBox();

  TMWAWPropertyHandlerEncoder doc;
  startODG(doc);

  WPXPropertyList list;
  list.clear();
  Vec2f pt=m_extremity[0]-bdbox[0];
  list.insert("x0",getStringPt(pt.x()).c_str());
  list.insert("y0",getStringPt(pt.y()).c_str());
  pt=m_extremity[1]-bdbox[0];
  list.insert("x1",getStringPt(pt.x()).c_str());
  list.insert("y1",getStringPt(pt.y()).c_str());
  doc.startElement("libmwaw:drawLine", list);
  doc.endElement("libmwaw:drawLine");

  endODG(doc);

  return doc.getData(res);
}

void PictLine::getGraphicStyleProperty(WPXPropertyList &list) const
{
  PictBasic::getStyle1DProperty(list);
  if (m_arrows[0]) {
    list.insert("startArrow", "true");
    list.insert("startArrowWidth", "5pt");
  }
  if (m_arrows[1]) {
    list.insert("endArrow", "true");
    list.insert("endArrowWidth", "5pt");
  }
}

////////////////////////////////////////////////////////////
//
//    PictRectangle
//
////////////////////////////////////////////////////////////
// to do: see how to manage the round corner
bool PictRectangle::getODGBinary(WPXBinaryData &res) const
{
  Box2f bdbox = getBdBox();

  TMWAWPropertyHandlerEncoder doc;
  startODG(doc);

  WPXPropertyList list;
  list.clear();
  Vec2f pt=m_rectBox[0]-bdbox[0];
  list.insert("x0",getStringPt(pt.x()).c_str());
  list.insert("y0",getStringPt(pt.y()).c_str());
  pt=m_rectBox[1]-m_rectBox[0];
  list.insert("w",getStringPt(pt.x()).c_str());
  list.insert("h",getStringPt(pt.y()).c_str());
  if (m_cornerWidth[0] > 0 && m_cornerWidth[1] > 0) {
    list.insert("rw",getStringPt(m_cornerWidth[0]).c_str());
    list.insert("rh",getStringPt(m_cornerWidth[1]).c_str());
  }
  doc.startElement("libmwaw:drawRectangle", list);
  doc.endElement("libmwaw:drawRectangle");

  endODG(doc);

  return doc.getData(res);
}

void PictRectangle::getGraphicStyleProperty(WPXPropertyList &list) const
{
  PictBasic::getStyle2DProperty(list);
}

////////////////////////////////////////////////////////////
//
//    PictCircle
//
////////////////////////////////////////////////////////////
bool PictCircle::getODGBinary(WPXBinaryData &res) const
{
  Box2f bdbox = getBdBox();

  TMWAWPropertyHandlerEncoder doc;
  startODG(doc);

  WPXPropertyList list;
  list.clear();
  Vec2f pt=m_circleBox[0]-bdbox[0];
  list.insert("x0",getStringPt(pt.x()).c_str());
  list.insert("y0",getStringPt(pt.y()).c_str());
  pt=m_circleBox[1]-m_circleBox[0];
  list.insert("w",getStringPt(pt.x()).c_str());
  list.insert("h",getStringPt(pt.y()).c_str());
  doc.startElement("libmwaw:drawCircle", list);
  doc.endElement("libmwaw:drawCircle");

  endODG(doc);

  return doc.getData(res);
}

void PictCircle::getGraphicStyleProperty(WPXPropertyList &list) const
{
  PictBasic::getStyle2DProperty(list);
}

////////////////////////////////////////////////////////////
//
//    PictArc
//
////////////////////////////////////////////////////////////
bool PictArc::getODGBinary(WPXBinaryData &res) const
{
  Box2f bdbox = getBdBox();

  TMWAWPropertyHandlerEncoder doc;
  std::stringstream s;
  startODG(doc);

  WPXPropertyList list;
  list.clear();
  Vec2f pt=m_circleBox[0]-bdbox[0];
  list.insert("x0",getStringPt(pt.x()).c_str());
  list.insert("y0",getStringPt(pt.y()).c_str());
  pt=m_circleBox[1]-m_circleBox[0];
  list.insert("w",getStringPt(pt.x()).c_str());
  list.insert("h",getStringPt(pt.y()).c_str());
  list.insert("angle0", m_angle[0], WPX_GENERIC);
  list.insert("angle1", m_angle[1], WPX_GENERIC);
  doc.startElement("libmwaw:drawArc", list);
  doc.endElement("libmwaw:drawArc");

  endODG(doc);

  return doc.getData(res);
}

void PictArc::getGraphicStyleProperty(WPXPropertyList &list) const
{
  PictBasic::getStyle1DProperty(list);
}

////////////////////////////////////////////////////////////
//
//    PictPolygon
//
////////////////////////////////////////////////////////////
bool PictPolygon::getODGBinary(WPXBinaryData &res) const
{
  int numPt = m_verticesList.size();
  if (numPt < 2) {
    MWAW_DEBUG_MSG(("PictPolygon::getODGBinary: can not draw a polygon with %d vertices\n", numPt));
    return false;
  }

  Box2f bdbox = getBdBox();

  TMWAWPropertyHandlerEncoder doc;
  startODG(doc);

  WPXPropertyList list;
  list.clear();
  Vec2f pt=bdbox[1]-bdbox[0];
  list.insert("w",getStringPt(pt.x()).c_str());
  list.insert("h",getStringPt(pt.y()).c_str());
  for (int i = 0; i < numPt; i++) {
    Vec2f pt=m_verticesList[i]-bdbox[0];
    std::stringstream s;
    s.str("");
    s << "x" << i;
    list.insert(s.str().c_str(),getStringPt(pt.x()).c_str());
    s.str("");
    s << "y" << i;
    list.insert(s.str().c_str(),getStringPt(pt.y()).c_str());
  }
  doc.startElement("libmwaw:drawPolygon", list);
  doc.endElement("libmwaw:drawPolygon");

  endODG(doc);

  return doc.getData(res);
}

void PictPolygon::getGraphicStyleProperty(WPXPropertyList &list) const
{
  PictBasic::getStyle1DProperty(list);
}
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
