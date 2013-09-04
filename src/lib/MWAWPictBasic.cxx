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
#include <map>
#include <iostream>
#include <sstream>
#include <string>

#include <libwpd/libwpd.h>
#include <libmwaw/libmwaw.hxx>

#include "libmwaw_internal.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWInputStream.hxx"
#include "MWAWParagraph.hxx"

#include "MWAWPictBasic.hxx"

////////////////////////////////////////////////////////////
// virtual class
////////////////////////////////////////////////////////////
bool MWAWPictBasic::getODGBinary(WPXBinaryData &res) const
{
  MWAWPropertyHandlerEncoder doc;
  startODG(doc);
  if (!send(doc, getBdBox()[0]))
    return false;
  endODG(doc);
  return doc.getData(res);
}

void MWAWPictBasic::startODG(MWAWPropertyHandlerEncoder &doc) const
{
  Box2f bdbox = getBdBox();
  Vec2f size=bdbox.size();
  WPXPropertyList list;
  list.insert("svg:x",0, WPX_POINT);
  list.insert("svg:y",0, WPX_POINT);
  list.insert("svg:width",size.x(), WPX_POINT);
  list.insert("svg:height",size.y(), WPX_POINT);
  list.insert("libwpg:enforce-frame",1);
  doc.startElement("Graphics", list);
}

void MWAWPictBasic::endODG(MWAWPropertyHandlerEncoder &doc) const
{
  doc.endElement("Graphics");
}

void MWAWPictBasic::sendStyle(MWAWPropertyHandlerEncoder &doc) const
{
  WPXPropertyListVector gradient;
  WPXPropertyList list;
  getGraphicStyleProperty(list, gradient);
  doc.startElement("SetStyle", list, gradient);
  doc.endElement("SetStyle");
}

////////////////////////////////////////////////////////////
//
//    MWAWPictLine
//
////////////////////////////////////////////////////////////
bool MWAWPictLine::send(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const
{
  sendStyle(doc);

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
bool MWAWPictRectangle::send(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const
{
  sendStyle(doc);

  WPXPropertyList list;
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
bool MWAWPictCircle::send(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const
{
  sendStyle(doc);

  WPXPropertyList list;
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
bool MWAWPictArc::send(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const
{
  sendStyle(doc);

  Vec2f center=0.5*(m_circleBox[0]+m_circleBox[1])-orig;
  Vec2f rad=0.5*(m_circleBox[1]-m_circleBox[0]);
  float angl0=m_angle[0];
  float angl1=m_angle[1];
  if (rad[1]<0) {
    static bool first=true;
    if (first) {
      MWAW_DEBUG_MSG(("MWAWPictArc::send: oops radiusY is negative, inverse it\n"));
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
bool MWAWPictPath::Command::get(WPXPropertyList &list) const
{
  list.clear();
  std::string type("");
  type += m_type;
  list.insert("libwpg:path-action", type.c_str());
  if (m_type=='Z')
    return true;
  if (m_type=='H') {
    list.insert("svg:x",m_x[0], WPX_POINT);
    return true;
  }
  if (m_type=='V') {
    list.insert("svg:y",m_x[1], WPX_POINT);
    return true;
  }
  list.insert("svg:x",m_x[0], WPX_POINT);
  list.insert("svg:y",m_x[1], WPX_POINT);
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
  list.insert("svg:x1",m_x1[0], WPX_POINT);
  list.insert("svg:y1",m_x1[1], WPX_POINT);
  if (m_type=='Q' || m_type=='S')
    return true;
  list.insert("svg:x2",m_x2[0], WPX_POINT);
  list.insert("svg:y2",m_x2[1], WPX_POINT);
  if (m_type=='C')
    return true;
  MWAW_DEBUG_MSG(("MWAWPictPath::Command::get: unknown command %c\n", m_type));
  list.clear();
  return false;
}

int MWAWPictPath::Command::cmp(MWAWPictPath::Command const &a) const
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

void MWAWPictPath::add(MWAWPictPath::Command const &command)
{
  m_path.push_back(command);
}

int MWAWPictPath::cmp(MWAWPict const &a) const
{
  int diff = MWAWPictBasic::cmp(a);
  if (diff) return diff;
  MWAWPictPath const &aPath = static_cast<MWAWPictPath const &>(a);
  if (m_path.size() != aPath.m_path.size())
    return m_path.size()<aPath.m_path.size() ? -1 : 1;
  for (size_t c=0; c < m_path.size(); ++c) {
    diff = m_path[c].cmp(aPath.m_path[c]);
    if (diff) return diff;
  }
  return 0;
}

bool MWAWPictPath::send(MWAWPropertyHandlerEncoder &doc, Vec2f const &) const
{
  size_t n=m_path.size();
  if (!n) {
    MWAW_DEBUG_MSG(("MWAWPictPath::send: the path is not defined\n"));
    return false;
  }

  sendStyle(doc);

  WPXPropertyListVector path;
  for (size_t c=0; c < m_path.size(); ++c) {
    WPXPropertyList list;
    if (m_path[c].get(list))
      path.append(list);
  }
  if ((m_style.hasGradient() || m_style.hasSurface()) && m_path[n-1].m_type != 'Z') {
    // odg need a closed path to draw surface, so ...
    WPXPropertyList list;
    list.insert("libwpg:path-action", "Z");
    path.append(list);
  }
  doc.startElement("Path", WPXPropertyList(), path);
  doc.endElement("Path");
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
bool MWAWPictPolygon::send(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const
{
  size_t numPt = m_verticesList.size();
  if (numPt < 2) {
    MWAW_DEBUG_MSG(("MWAWPictPolygon::send: can not draw a polygon with %ld vertices\n", long(numPt)));
    return false;
  }

  sendStyle(doc);

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
//    MWAWPictSimpleText
//
////////////////////////////////////////////////////////////
MWAWPictSimpleText::MWAWPictSimpleText(MWAWGraphicStyleManager &graphicManager, Box2f bdBox) :
  MWAWPictBasic(graphicManager), m_LTPadding(0,0), m_RBPadding(0,0), m_textBuffer(""), m_lineBreakSet(), m_fontId(20), m_posFontMap(), m_posParagraphMap()
{
  setBdBox(bdBox);
}

MWAWPictSimpleText::~MWAWPictSimpleText()
{
}

void MWAWPictSimpleText::insertTab()
{
  m_textBuffer.append('\t');
}

void MWAWPictSimpleText::insertEOL()
{
  if (!m_textBuffer.cstr())
    m_lineBreakSet.insert(0);
  else
    m_lineBreakSet.insert((int) strlen(m_textBuffer.cstr()));
  m_textBuffer.append('\n');
}

void MWAWPictSimpleText::insertUnicodeString(WPXString const &str)
{
  m_textBuffer.append(str);
}

void MWAWPictSimpleText::insertCharacter(unsigned char c)
{
  if (c=='\t') {
    insertTab();
    return;
  }
  if (c=='\n') {
    insertEOL();
    return;
  }
  int unicode = m_graphicManager.getFontConverter()->unicode(m_fontId, c);
  if (unicode!=-1)
    libmwaw::appendUnicode((uint32_t) unicode, m_textBuffer);
  else if (c >= 0x20)
    libmwaw::appendUnicode((uint32_t) c, m_textBuffer);
  else {
    MWAW_DEBUG_MSG(("MWAWPictSimpleText::insertCharacter: call with char %x\n", int(c)));
  }
}

///////////////////
// field :
///////////////////
void MWAWPictSimpleText::insertField(MWAWField const &field)
{
  switch(field.m_type) {
  case MWAWField::None:
    break;
  case MWAWField::PageCount:
    insertUnicodeString("#C#");
    break;
  case MWAWField::PageNumber:
    insertUnicodeString("#P#");
    break;
  case MWAWField::Database:
    if (field.m_data.length())
      insertUnicodeString(field.m_data.c_str());
    else
      insertUnicodeString("#DATAFIELD#");
    break;
  case MWAWField::Title:
    insertUnicodeString("#TITLE#");
    break;
  case MWAWField::Date:
  case MWAWField::Time: {
    std::string format(field.m_DTFormat);
    if (format.length()==0) {
      if (field.m_type==MWAWField::Date)
        format="%m/%d/%y";
      else
        format="%I:%M:%S %p";
    }
    time_t now = time ( 0L );
    struct tm timeinfo = *(localtime ( &now));
    char buf[256];
    strftime(buf, 256, format.c_str(), &timeinfo);
    WPXString tmp(buf);
    insertUnicodeString(tmp);
    break;
  }
  case MWAWField::Link:
    if (field.m_data.length()) {
      insertUnicodeString(field.m_data.c_str());
      break;
    }
  default:
    MWAW_DEBUG_MSG(("MWAWPictSimpleText::insertField: must not be called with type=%d\n", int(field.m_type)));
    break;
  }
}

void MWAWPictSimpleText::setFont(MWAWFont const &font)
{
  if (font.id()>=0) m_fontId=font.id();
  if (!m_textBuffer.cstr())
    m_posFontMap[0]=font;
  else
    m_posFontMap[(int) strlen(m_textBuffer.cstr())]=font;
}

void MWAWPictSimpleText::setParagraph(MWAWParagraph const &paragraph)
{
  if (!m_textBuffer.cstr())
    m_posParagraphMap[0]=paragraph;
  else
    m_posParagraphMap[(int) strlen(m_textBuffer.cstr())]=paragraph;
}

bool MWAWPictSimpleText::send(MWAWPropertyHandlerEncoder &doc, Vec2f const &) const
{
  WPXPropertyList list;
  Vec2f size=getBdBox().size();
  if (m_style.hasGradient(true)) {
    // ok, first send a background rectangle
    sendStyle(doc);

    list.insert("svg:x",0, WPX_POINT);
    list.insert("svg:y",0, WPX_POINT);
    list.insert("svg:width",size.x(), WPX_POINT);
    list.insert("svg:height",size.y(), WPX_POINT);
    doc.startElement("Rectangle", list);
    doc.endElement("Rectangle");

    list.clear();
    list.insert("draw:stroke", "none");
    list.insert("draw:fill", "none");
  } else {
    WPXPropertyListVector grad;
    getGraphicStyleProperty(list, grad);
  }
  list.insert("svg:x",0, WPX_POINT);
  list.insert("svg:y",0, WPX_POINT);
  list.insert("svg:width",size.x(), WPX_POINT);
  list.insert("svg:height",size.y(), WPX_POINT);
  list.insert("fo:padding-top",m_LTPadding[1], WPX_POINT);
  list.insert("fo:padding-bottom",m_RBPadding[1], WPX_POINT);
  list.insert("fo:padding-left",m_LTPadding[0], WPX_POINT);
  list.insert("fo:padding-right",m_RBPadding[0], WPX_POINT);
  //list.insert("draw:textarea-vertical-align", "middle");
  doc.startElement("TextObject", list, WPXPropertyListVector());

  int actPos=0;
  std::map<int,MWAWFont>::const_iterator it1=m_posFontMap.begin();
  std::map<int,MWAWParagraph>::const_iterator it2=m_posParagraphMap.begin();
  std::set<int>::const_iterator it3=m_lineBreakSet.begin();
  std::string buffer("");
  buffer=m_textBuffer.cstr();
  int totalLength=(int) buffer.length();

  bool lineOpened=false;
  WPXPropertyList paraList;
  do {
    bool firstIsLineBreak=false;
    if (it3 != m_lineBreakSet.end() && *it3==actPos) {
      it3++;
      firstIsLineBreak=true;
      if (lineOpened)
        doc.endElement("TextLine");
      lineOpened=false;
    }
    if (it2 != m_posParagraphMap.end() && it2->first==actPos) {
      paraList.clear();
      it2++->second.addTo(paraList, false);
    }
    if (it1 != m_posFontMap.end() && it1->first==actPos) {
      list.clear();
      it1++->second.addTo(list, m_graphicManager.getFontConverter());
    }
    int nextPos= totalLength;
    if (it1 != m_posFontMap.end() && it1->first < nextPos)
      nextPos=it1->first;
    if (it2 != m_posParagraphMap.end() && it2->first < nextPos)
      nextPos=it2->first;
    if (it3 != m_lineBreakSet.end() && *it3 < nextPos)
      nextPos=*it3;
    if (nextPos>totalLength) nextPos=totalLength;

    if (!lineOpened) {
      lineOpened = true;
      doc.startElement("TextLine", paraList);
    }
    if (nextPos < actPos) {
      MWAW_DEBUG_MSG(("MWAWPictSimpleText::send: oops nextPos is smaller than actPos!!!\n"));
      break;
    }
    std::string text("");
    if (firstIsLineBreak) ++actPos;
    if (nextPos > actPos)
      text=buffer.substr(size_t(actPos),size_t(nextPos-actPos));
    doc.startElement("TextSpan", list);
    doc.characters(text.c_str());
    doc.endElement("TextSpan");
    actPos = nextPos;
  } while (actPos < totalLength);

  if (lineOpened)
    doc.endElement("TextLine");
  doc.endElement("TextObject");
  return true;
}

int MWAWPictSimpleText::cmp(MWAWPict const &a) const
{
  int diff = MWAWPictBasic::cmp(a);
  if (diff) return diff;
  MWAWPictSimpleText const &aText = static_cast<MWAWPictSimpleText const &>(a);
  if (m_posFontMap.size() < aText.m_posFontMap.size()) return 1;
  if (m_posFontMap.size() > aText.m_posFontMap.size()) return 1;
  std::map<int,MWAWFont>::const_iterator fIt1=m_posFontMap.begin(), fIt2=aText.m_posFontMap.begin();
  while (fIt1 != m_posFontMap.end()) {
    diff = fIt1->first-fIt2->first;
    if (diff) return diff < 0 ? 1 : -1;
    diff = fIt1++->second.cmp(fIt2++->second);
    if (diff) return diff;
  }
  std::map<int,MWAWParagraph>::const_iterator pIt1=m_posParagraphMap.begin(), pIt2=aText.m_posParagraphMap.begin();
  while (pIt1 != m_posParagraphMap.end()) {
    diff = pIt1->first-pIt2->first;
    if (diff) return diff < 0 ? 1 : -1;
    diff = pIt1++->second.cmp(pIt2++->second);
    if (diff) return diff;
  }
  if (m_textBuffer != aText.m_textBuffer) {
    if (m_textBuffer.len() < aText.m_textBuffer.len()) return -1;
    if (m_textBuffer.len() > aText.m_textBuffer.len()) return 1;
    char const *dt1 = m_textBuffer.cstr();
    char const *dt2 = aText.m_textBuffer.cstr();
    for (int i=0; i < aText.m_textBuffer.len(); ++i, ++dt1, ++dt2) {
      if (*dt1 < *dt2) return -1;
      if (*dt1 > *dt2) return 1;
    }
  }
  return 0;
}

void MWAWPictSimpleText::getGraphicStyleProperty(WPXPropertyList &list, WPXPropertyListVector &gradient) const
{
  m_style.addTo(list, gradient);
}

////////////////////////////////////////////////////////////
//
//    MWAWPictGraphicObject
//
////////////////////////////////////////////////////////////
int MWAWPictGraphicObject::cmp(MWAWPict const &a) const
{
  int diff = MWAWPictBasic::cmp(a);
  if (diff) return diff;
  MWAWPictGraphicObject const &aObject = static_cast<MWAWPictGraphicObject const &>(a);
  if (m_mimeType<aObject.m_mimeType) return 1;
  if (m_mimeType>aObject.m_mimeType) return -1;
  if (m_data.size()<aObject.m_data.size()) return 1;
  if (m_data.size()>aObject.m_data.size()) return -1;
  unsigned char const *buf1=m_data.getDataBuffer();
  unsigned char const *buf2=aObject.m_data.getDataBuffer();
  for (unsigned long l=0; l<m_data.size(); ++l, ++buf1, ++buf2) {
    if (*buf1 < *buf2) return 1;
    if (*buf1 > *buf2) return -1;
  }
  return 0;
}

bool MWAWPictGraphicObject::send(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const
{
  if (!m_data.size() || m_mimeType.empty()) {
    MWAW_DEBUG_MSG(("MWAWPictGraphicObject::send: can not find the data\n"));
    return false;
  }

  sendStyle(doc);

  WPXPropertyList list;
  Box2f bdbox=getBdBox();
  Vec2f pt=bdbox[0]-orig;
  list.insert("svg:x",pt.x(), WPX_POINT);
  list.insert("svg:y",pt.y(), WPX_POINT);
  pt=bdbox.size();
  list.insert("svg:width",pt.x(), WPX_POINT);
  list.insert("svg:height",pt.y(), WPX_POINT);
  list.insert("libwpg:mime-type", m_mimeType.c_str());
  doc.startElement("GraphicObject", list, m_data);
  doc.endElement("GraphicObject");

  return true;
}

void MWAWPictGraphicObject::getGraphicStyleProperty(WPXPropertyList &list, WPXPropertyListVector &gradient) const
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

  m_child.push_back(child);
}

void MWAWPictGroup::getGraphicStyleProperty(WPXPropertyList &, WPXPropertyListVector &) const
{
}

bool MWAWPictGroup::send(MWAWPropertyHandlerEncoder &doc, Vec2f const &orig) const
{
  if (m_child.empty())
    return false;

  sendStyle(doc);

  // first check if we need to use different layer
  std::multimap<int,size_t> idByLayerMap;
  for (size_t c=0; c < m_child.size(); ++c) {
    if (!m_child[c]) continue;
    idByLayerMap.insert(std::multimap<int,size_t>::value_type(m_child[c]->getLayer(), c));
  }
  if (idByLayerMap.size() <= 1) {
    for (size_t c=0; c < m_child.size(); ++c) {
      if (!m_child[c]) continue;
      m_child[c]->send(doc, orig);
    }
    return true;
  }
  std::multimap<int,size_t>::const_iterator it=idByLayerMap.begin();
  while(it != idByLayerMap.end()) {
    int layer=it->first;
    WPXPropertyList list;
    list.insert("svg:id", m_graphicManager.getNewLayerId());
    doc.startElement("Layer", list);
    while (it != idByLayerMap.end() && it->first==layer)
      m_child[it++->second]->send(doc,orig);
    doc.endElement("Layer");
  }
  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
