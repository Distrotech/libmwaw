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
#include <map>
#include <set>
#include <sstream>

#include <libwpd/libwpd.h>

#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWPictBasic.hxx"
#include "MWAWPictBitmap.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"

#include "CWParser.hxx"
#include "CWStruct.hxx"
#include "CWStyleManager.hxx"

#include "CWGraph.hxx"

#include "libmwaw_internal.hxx"

/** Internal: the structures of a CWGraph */
namespace CWGraphInternal
{
//! Internal: the structure used to a point of a CWGraph
struct CurvePoint {
  CurvePoint(Vec2f point=Vec2f()) : m_pos(point), m_type(1) {
    for (int i = 0; i < 2; i++) m_controlPoints[i] = point;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, CurvePoint const &pt) {
    o << pt.m_pos;
    if (pt.m_pos != pt.m_controlPoints[0])
      o << ":prev=" << pt.m_controlPoints[0];
    if (pt.m_pos != pt.m_controlPoints[1])
      o << ":next=" << pt.m_controlPoints[1];
    switch(pt.m_type) {
    case 0:
      o << ":point2";
      break;
    case 1:
      break;
    case 2:
      o << ":spline";
      break;
    case 3:
      o << ":spline2";
      break;
    default:
      o << ":#type=" << pt.m_type;
    }
    return o;
  }
  //! the main position
  Vec2f m_pos;
  //! the control point: previous, next
  Vec2f m_controlPoints[2];
  //! the point type
  int m_type;
};

//! Internal: the structure used to store a style of a CWGraph
struct Style {
  //! constructor
  Style(): m_id(-1), m_wrapping(0), m_lineFlags(0), m_lineWidth(1), m_surfacePatternType(0) {
    for (int i = 0; i < 2; i++) m_color[i] = m_pattern[i] = -1;
    for (int i = 0; i < 5; i++) m_flags[i] = 0;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Style const &st) {
    if (st.m_id >= 0) o << "id=" << st.m_id << ",";
    switch(st.m_wrapping & 3) {
    case 0:
      o << "wrap=none,";
      break; // RunThrough
    case 1:
      o << "wrap=regular,";
      break; // Page Wrap
    case 2:
      o << "wrap=irregular,";
      break; // Optimal page Wrap
    default:
      o << "#wrap=3,";
      break;
    }
    switch(st.m_surfacePatternType) {
    case 0:
      break; // pattern
    case 1:
      o << "wallPattern,";
      break;
    default:
      o << "pattType=" << st.m_surfacePatternType << ",";
    }
    if (st.m_lineWidth && st.m_lineWidth != 1)
      o << "lineW=" << st.m_lineWidth << ",";
    if (st.m_color[0] != -1 && st.m_color[0] != 1)
      o << "lineColor=" << st.m_color[0] << ",";
    if (st.m_color[1] != -1 && st.m_color[1])
      o << "surfColor=" << st.m_color[1] << ",";
    if (st.m_pattern[0] != -1 && st.m_pattern[0] != 2)
      o << "linePattern=" << st.m_pattern[0] << ",";
    if (st.m_pattern[1] != -1 && st.m_pattern[1] != 2)
      o << "surfPattern=" << st.m_pattern[1] << ",";
    if (st.m_lineFlags & 0x10)
      o << "useSurfGradient,";
    if (st.m_lineFlags & 0x40)
      o << "arrowBeg,";
    if (st.m_lineFlags & 0x80)
      o << "arrowEnd,";
    if (st.m_lineFlags & 0x2F)
      o << "lineFlags(?)=" << std::hex << int(st.m_lineFlags & 0x2F) << std::dec << ",";
    for (int i = 0; i < 5; i++) {
      if (st.m_flags[i])
        o << "fl" << i << "=" << std::hex << st.m_flags[i] << std::dec << ",";
    }
    return o;
  }
  //! the identificator
  int m_id;
  //! the wrap type
  int m_wrapping;
  //! the line flags
  int m_lineFlags;
  //! the line width
  int m_lineWidth;
  //! the line and surface color id
  int m_color[2];
  //! the surface pattern type
  int m_surfacePatternType;
  //! the line an surface id
  int m_pattern[2];
  //! the list of flags
  int m_flags[5];
};

//! Internal: the generic structure used to store a zone of a CWGraph
struct Zone {
  //! the list of types
  enum Type { T_Zone, T_Basic, T_Picture, T_Chart, T_DataBox, T_Unknown,
              /* basic subtype */
              T_Line, T_Rect, T_RectOval, T_Oval, T_Arc, T_Poly,
              /* picture subtype */
              T_Pict, T_QTim, T_Movie,
              /* bitmap type */
              T_Bitmap
            };
  //! constructor
  Zone() : m_page(-1), m_box(), m_style() {}
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Zone const &zone) {
    if (zone.m_page >= 0) o << "pg=" << zone.m_page << ",";
    o << "box=" << zone.m_box << ",";
    zone.print(o);
    o << "style=[" << zone.m_style << "],";
    return o;
  }
  //! destructor
  virtual ~Zone() {}
  //! return the main type
  virtual Type getType() const {
    return T_Unknown;
  }
  //! return the subtype
  virtual Type getSubType() const {
    return T_Unknown;
  }
  //! return the number of data to define this zone in the file
  virtual int getNumData() const {
    return 0;
  }
  //! print the data contains
  virtual void print(std::ostream &) const { }
  //! return a child corresponding to this zone
  virtual CWStruct::DSET::Child getChild() const {
    CWStruct::DSET::Child child;
    child.m_box = m_box;
    return child;
  }
  //! the page (checkme: or frame linked )
  int m_page;
  //! the bdbox
  Box2i m_box;
  //! the style
  Style m_style;
};

//! Internal: small class to store a basic graphic zone of a CWGraph
struct ZoneBasic : public Zone {
  //! constructor
  ZoneBasic(Zone const &z, Type type) : Zone(z), m_type(type), m_vertices() {
    for (int i = 0; i < 2; i++)
      m_values[i] = 0;
    for (int i = 0; i < 8; i++)
      m_flags[i] = 0;
  }
  //! print the data
  virtual void print(std::ostream &o) const;
  //! return the main type
  virtual Type getType() const {
    return T_Basic;
  }
  //! return the sub type
  virtual Type getSubType() const {
    return m_type;
  }
  //! return the number of data
  virtual int getNumData() const {
    if (m_type == T_Poly) return 1;
    return 0;
  }
  //! return a child corresponding to this zone
  virtual CWStruct::DSET::Child getChild() const {
    CWStruct::DSET::Child child;
    child.m_box = m_box;
    child.m_type = CWStruct::DSET::Child::GRAPHIC;
    return child;
  }

  //! the sub type
  Type m_type;
  //! arc : the angles, rectoval : the corner dimension
  float m_values[2];
  //! some unknown value
  int m_flags[8];
  //! the polygon vertices
  std::vector<CurvePoint> m_vertices;
};

void ZoneBasic::print(std::ostream &o) const
{
  switch (m_type) {
  case T_Line:
    o << "LINE,";
    break;
  case T_Rect:
    o << "RECT,";
    break;
  case T_RectOval:
    o << "RECTOVAL, cornerDim=" << m_values[0]<< "x" << m_values[1] << ",";
    break;
  case T_Oval:
    o << "OVAL,";
    break;
  case T_Arc:
    o << "ARC, angles=" << m_values[0]<< "x" << m_values[1] << ",";
    break;
  case T_Poly:
    o << "POLY,";
    break;
  case T_Zone:
  case T_Basic:
  case T_Picture:
  case T_Chart:
  case T_DataBox:
  case T_Unknown:
  case T_Pict:
  case T_QTim:
  case T_Movie:
  case T_Bitmap:
  default:
    o << "##type=" << m_type << ",";
    break;
  }
  if (m_vertices.size()) {
    o << "vertices=[";
    for (size_t i = 0; i < m_vertices.size(); i++)
      o << m_vertices[i] << ",";
    o << "],";
  }
  for (int i = 0; i < 8; i++)
    if (m_flags[i]) o << "fl" << i << "=" << m_flags[i] << ",";
}

//! Internal: the structure used to store a PICT or a MOVIE
struct ZonePict : public Zone {
  //! constructor
  ZonePict(Zone const &z, Type type) : Zone(z), m_type(type) {
  }
  //! print the data
  virtual void print(std::ostream &o) const {
    switch (m_type) {
    case T_Pict:
      o << "PICTURE,";
      break;
    case T_QTim:
      o << "QTIME,";
      break;
    case T_Movie:
      o << "MOVIE,";
      break;
    case T_Zone:
    case T_Basic:
    case T_Picture:
    case T_Chart:
    case T_DataBox:
    case T_Line:
    case T_Rect:
    case T_RectOval:
    case T_Oval:
    case T_Arc:
    case T_Poly:
    case T_Unknown:
    case T_Bitmap:
    default:
      o << "##type=" << m_type << ",";
      break;
    }
  }
  //! return the main type T_Picture
  virtual Type getType() const {
    return T_Picture;
  }
  //! return the sub type
  virtual Type getSubType() const {
    return m_type;
  }
  //! return the number of data in a file
  virtual int getNumData() const {
    return 2;
  }
  //! return a child corresponding to this zone
  virtual CWStruct::DSET::Child getChild() const {
    CWStruct::DSET::Child child;
    child.m_box = m_box;
    child.m_type = CWStruct::DSET::Child::GRAPHIC;
    return child;
  }

  //! the sub type
  Type m_type;
  //! the picture entry followed by a ps entry or ole entry ( if defined)
  MWAWEntry m_entries[2];
};

//! Internal: structure to store a bitmap of a CWGraph
struct ZoneBitmap : public Zone {
  //! constructor
  ZoneBitmap() : m_bitmapType(-1), m_size(0,0), m_entry(), m_colorMap() {
  }

  //! print the zone
  virtual void print(std::ostream &o) const {
    o << "BITMAP:" << m_size << ",";
    if (m_bitmapType >= 0) o << "type=" << m_bitmapType << ",";
  }
  //! return the main type (Bitmap)
  virtual Type getType() const {
    return T_Bitmap;
  }
  //! return the subtype (Bitmap)
  virtual Type getSubType() const {
    return  T_Bitmap;
  }

  //! return a child corresponding to this zone
  virtual CWStruct::DSET::Child getChild() const {
    CWStruct::DSET::Child child;
    child.m_box = m_box;
    child.m_type = CWStruct::DSET::Child::GRAPHIC;
    return child;
  }

  //! the bitmap type
  int m_bitmapType;

  //! the bitmap size
  Vec2i m_size;
  //! the bitmap entry
  MWAWEntry m_entry;
  //! the color map
  std::vector<MWAWColor> m_colorMap;
};

//! Internal: structure to store a link to a zone of a CWGraph
struct ZoneZone : public Zone {
  //! constructor
  ZoneZone(Zone const &z) : Zone(z), m_id(-1), m_subId(-1), m_styleId(-1), m_wrappingSep(5) {
    for (int i = 0; i < 9; i++)
      m_flags[i] = 0;
  }
  //! print the zone
  virtual void print(std::ostream &o) const {
    o << "ZONE, id=" << m_id << ",";
    if (m_subId > 0) o << "subId=" << m_subId << ",";
    if (m_styleId >= 0) o << "styleId=" << m_styleId << ",";
    if (m_wrappingSep != 5) o << "wrappingSep=" << m_wrappingSep << ",";
    for (int i = 0; i < 9; i++) {
      if (m_flags[i]) o << "fl" << i << "=" << m_flags[i] << ",";
    }
  }
  //! return the main type Zone
  virtual Type getType() const {
    return T_Zone;
  }
  //! return the sub type Zone
  virtual Type getSubType() const {
    return T_Zone;
  }

  //! return a child corresponding to this zone
  virtual CWStruct::DSET::Child getChild() const {
    CWStruct::DSET::Child child;
    child.m_box = m_box;
    child.m_id = m_id;
    child.m_type = CWStruct::DSET::Child::ZONE;
    return child;
  }

  //! the zoneId
  int m_id;
  //! the zoneSubId: can be page/column/frame linked number
  int m_subId;
  //! the style id
  int m_styleId;
  //! the wraping separator
  int m_wrappingSep;
  //! flag
  int m_flags[9];
};

//! Internal: structure used to store an unknown zone of a CWGraph
struct ZoneUnknown : public Zone {
  //! construtor
  ZoneUnknown(Zone const &z) : Zone(z), m_type(T_Unknown), m_typeId(-1) {
  }
  //! print the zone
  virtual void print(std::ostream &o) const {
    switch(m_type) {
    case T_DataBox:
      o << "BOX(database),";
      break;
    case T_Chart:
      o << "CHART,";
      break;
    case T_Zone:
    case T_Basic:
    case T_Picture:
    case T_Line:
    case T_Rect:
    case T_RectOval:
    case T_Oval:
    case T_Arc:
    case T_Poly:
    case T_Unknown:
    case T_Pict:
    case T_QTim:
    case T_Movie:
    case T_Bitmap:
    default:
      o << "##type=" << m_typeId << ",";
      break;
    }
  }
  //! return the main type
  virtual Type getType() const {
    return m_type;
  }
  //! return the sub type
  virtual Type getSubType() const {
    return m_type;
  }
  //! return the number of data
  virtual int getNumData() const {
    return m_type == T_Chart ? 2 : 0;
  }
  //! return a child corresponding to this zone
  virtual CWStruct::DSET::Child getChild() const {
    CWStruct::DSET::Child child;
    child.m_box = m_box;
    child.m_type = CWStruct::DSET::Child::GRAPHIC;
    return child;
  }

  //! the sub type
  Type m_type;
  //! type number
  int m_typeId;
};

////////////////////////////////////////
//! Internal: class which stores a group of graphics, ...
struct Group : public CWStruct::DSET {
  struct LinkedZones;
  //! constructor
  Group(CWStruct::DSET const dset = CWStruct::DSET()) :
    CWStruct::DSET(dset), m_zones(), m_hasMainZone(false), m_totalNumber(0),
    m_blockToSendList(), m_idLinkedZonesMap() {
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Group const &doc) {
    o << static_cast<CWStruct::DSET const &>(doc);
    return o;
  }

  /** check if we need to send the frame content and if needed
      update the property list adding the frame name
   */
  bool needSendFrameContent(int id, int subId, WPXPropertyList &framePList,
                            WPXPropertyList &textboxPList) const;

  /** the list of child zones */
  std::vector<shared_ptr<Zone> > m_zones;

  //! a flag to know if this zone contains or no the call to zone 1
  bool m_hasMainZone;
  //! the number of zone to send
  int m_totalNumber;
  //! the list of block to send
  std::vector<size_t> m_blockToSendList;
  //! a map zone id to the list of zones
  std::map<int, LinkedZones> m_idLinkedZonesMap;

  //! a small class of Group used to store a list a set of text zone
  struct LinkedZones {
    //! constructor
    LinkedZones(int frameId) :  m_frameId(frameId), m_mapIdChild() {
    }
    //! the frame basic id
    int m_frameId;
    //! map zoneId -> group child
    std::map<int, size_t> m_mapIdChild;
  };

};

bool Group::needSendFrameContent(int id, int subId, WPXPropertyList &frameList,
                                 WPXPropertyList &textboxList) const
{
  if (m_idLinkedZonesMap.find(id) == m_idLinkedZonesMap.end())
    return subId==0;
  LinkedZones const &lZones = m_idLinkedZonesMap.find(id)->second;
  std::map<int, size_t>::const_iterator it = lZones.m_mapIdChild.find(subId);
  if (it == lZones.m_mapIdChild.end()) {
    MWAW_DEBUG_MSG(("CWGraphInternal::Group::addFrameNameProperty: can not find frame %d[%d]\n", id, subId));
    return subId==0;
  }
  bool res = true;
  if (it != lZones.m_mapIdChild.begin()) {
    WPXString fName;
    fName.sprintf("Frame%d-%d", id, subId);
    frameList.insert("libwpd:frame-name",fName);
    res = false;
  }
  it++;
  if (it != lZones.m_mapIdChild.end()) {
    WPXString fName;
    fName.sprintf("Frame%d-%d", id, it->first);
    textboxList.insert("libwpd:next-frame-name",fName);
  }
  return res;
}

////////////////////////////////////////
//! Internal: the state of a CWGraph
struct State {
  //! constructor
  State() : m_zoneMap(), m_colorList(), m_patternList(), m_wallpaperList(), m_frameId(0) { }
  //! set the default color map
  void setDefaultColorList(int version);
  //! set the default pattern map
  void setDefaultPatternList(int version);
  //! set the default pattern map
  void setDefaultWallPaperList(int version);

  std::map<int, shared_ptr<Group> > m_zoneMap;
  //! a list colorId -> color
  std::vector<MWAWColor> m_colorList;
  //! a list patternId -> percent
  std::vector<float> m_patternList;
  //! a list wallPaperId -> color
  std::vector<MWAWColor> m_wallpaperList;
  //! a int used to defined linked frame
  int m_frameId;
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
  static float const defPercentPattern[64] = {
    0.0f, 1.0f, 0.968750f, 0.93750f, 0.8750f, 0.750f, 0.50f, 0.250f,
    0.250f, 0.18750f, 0.18750f, 0.1250f, 0.06250f, 0.06250f, 0.031250f, 0.015625f,
    0.750f, 0.50f, 0.250f, 0.3750f, 0.250f, 0.1250f, 0.250f, 0.1250f,
    0.750f, 0.50f, 0.250f, 0.3750f, 0.250f, 0.1250f, 0.250f, 0.1250f,
    0.750f, 0.50f, 0.50f, 0.50f, 0.50f, 0.250f, 0.250f, 0.234375f,
    0.6250f, 0.3750f, 0.1250f, 0.250f, 0.218750f, 0.218750f, 0.1250f, 0.093750f,
    0.50f, 0.56250f, 0.43750f, 0.3750f, 0.218750f, 0.281250f, 0.18750f, 0.093750f,
    0.593750f, 0.56250f, 0.515625f, 0.343750f, 0.31250f, 0.250f, 0.250f, 0.234375f,
  };
  m_patternList.resize(1+64);
  m_patternList[0] = -1.; // checkme: none ?
  for (size_t i = 0; i < 64; i++)
    m_patternList[i+1]=defPercentPattern[i];
}

void State::setDefaultWallPaperList(int version)
{
  if (version <= 2 || m_wallpaperList.size())
    return;
  // checkme: does ClarisWork v4 version has wallpaper?
  uint32_t const defCol[20] = {
    0xdcdcdc, 0x0000cd, 0xeeeeee, 0xeedd8e, 0xc71585,
    0xc9c9c9, 0xcd853f, 0x696969, 0xfa8072, 0x6495ed,
    0x4682b4, 0xdaa520, 0xcd5c5c, 0xb22222, 0x8b8682,
    0xb03060, 0xeeeee0, 0x4682b4, 0xfa8072, 0x505050
  };
  m_wallpaperList.resize(20);
  for (size_t i = 0; i < 20; i++)
    m_wallpaperList[i] = defCol[i];
}

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
CWGraph::CWGraph(CWParser &parser) :
  m_parserState(parser.getParserState()), m_state(new CWGraphInternal::State),
  m_mainParser(&parser), m_styleManager(parser.m_styleManager)
{
}

CWGraph::~CWGraph()
{ }

int CWGraph::version() const
{
  return m_parserState->m_version;
}

// fixme
int CWGraph::numPages() const
{
  return 1;
}
////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////
bool CWGraph::getColor(int id, MWAWColor &col) const
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

float CWGraph::getPatternPercent(int id) const
{
  int numPattern = (int) m_state->m_patternList.size();
  if (!numPattern) {
    m_state->setDefaultPatternList(version());
    numPattern = int(m_state->m_patternList.size());
  }
  if (id < 0 || id >= numPattern)
    return -1.;
  return m_state->m_patternList[size_t(id)];
}

bool CWGraph::getWallPaperColor(int id, MWAWColor &col) const
{
  int numWallpaper = (int) m_state->m_wallpaperList.size();
  if (!numWallpaper) {
    m_state->setDefaultWallPaperList(version());
    numWallpaper = int(m_state->m_wallpaperList.size());
  }
  if (id < 0 || id >= numWallpaper)
    return false;
  col = m_state->m_wallpaperList[size_t(id)];
  return true;
}

bool CWGraph::getLineColor(CWGraphInternal::Style const style, MWAWColor &col) const
{
  MWAWColor fCol;
  if (!getColor(style.m_color[0], fCol))
    return false;
  col = fCol;
  float percent = getPatternPercent(style.m_pattern[0]);
  if (percent < 0)
    return true;
  col = MWAWColor::barycenter(percent,fCol,(1.f-percent),MWAWColor::white());
  return true;
}

bool CWGraph::getSurfaceColor(CWGraphInternal::Style const style, MWAWColor &col) const
{
  if (style.m_surfacePatternType==1)
    return getWallPaperColor(style.m_pattern[1], col);
  MWAWColor fCol;
  if (!getColor(style.m_color[1], fCol))
    return false;
  col = fCol;
  if (style.m_surfacePatternType!=0)
    return true;
  float percent = getPatternPercent(style.m_pattern[1]);
  if (percent < 0)
    return true;
  col = MWAWColor::barycenter(percent,fCol,(1.f-percent),MWAWColor::white());
  return true;
}

////////////////////////////////////////////////////////////
// a group of data mainly graphic
////////////////////////////////////////////////////////////
shared_ptr<CWStruct::DSET> CWGraph::readGroupZone
(CWStruct::DSET const &zone, MWAWEntry const &entry, bool &complete)
{
  complete = false;
  if (!entry.valid() || zone.m_fileType != 0)
    return shared_ptr<CWStruct::DSET>();
  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos+8+16, WPX_SEEK_SET); // avoid header+8 generic number
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  shared_ptr<CWGraphInternal::Group> group(new CWGraphInternal::Group(zone));

  f << "Entries(GroupDef):" << *group << ",";
  int val = (int) input->readLong(2); // a small int between 0 and 3
  switch (val) {
  case 0:
    break; // normal
  case 3:
    f << "database/spreadsheet,";
    break;
  default:
    f << "#type?=" << val << ",";
    break;
  }
  val = (int) input->readLong(2); // a small number between 0 and 1e8
  if (val) f << "f1=" << val << ",";

  ascFile.addDelimiter(input->tell(), '|');
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  // read the last part
  long data0Length = zone.m_dataSz;
  long N = zone.m_numData;
  if (entry.length() -8-12 != data0Length*N + zone.m_headerSz) {
    if (data0Length == 0 && N) {
      MWAW_DEBUG_MSG(("CWGraph::readGroupZone: can not find definition size\n"));
      input->seek(entry.end(), WPX_SEEK_SET);
      return shared_ptr<CWStruct::DSET>();
    }

    MWAW_DEBUG_MSG(("CWGraph::readGroupZone: unexpected size for zone definition, try to continue\n"));
  }

  long beginDefGroup = entry.end()-N*data0Length;
  if (long(input->tell())+42 <= beginDefGroup) {
    input->seek(beginDefGroup-42, WPX_SEEK_SET);
    pos = input->tell();
    if (!readGroupUnknown(*group, 42, -1)) {
      ascFile.addPos(pos);
      ascFile.addNote("GroupDef(Head-###)");
    }
  }

  input->seek(beginDefGroup, WPX_SEEK_SET);

  group->m_childs.resize(size_t(N));
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    MWAWEntry gEntry;
    gEntry.setBegin(pos);
    gEntry.setLength(data0Length);
    shared_ptr<CWGraphInternal::Zone> def = readGroupDef(gEntry);
    group->m_zones.push_back(def);

    if (def)
      group->m_childs[size_t(i)] = def->getChild();
    else {
      f.str("");
      f << "GroupDef#";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    input->seek(gEntry.end(), WPX_SEEK_SET);
  }

  input->seek(entry.end(), WPX_SEEK_SET);

  if (readGroupData(*group, entry.begin())) {
    // fixme: do something here
  }

  if (m_state->m_zoneMap.find(group->m_id) != m_state->m_zoneMap.end()) {
    MWAW_DEBUG_MSG(("CWGraph::readGroupZone: zone %d already exists!!!\n", group->m_id));
  } else
    m_state->m_zoneMap[group->m_id] = group;

  return group;
}

////////////////////////////////////////////////////////////
// a group of data mainly graphic
////////////////////////////////////////////////////////////
bool CWGraph::readColorList(MWAWEntry const &entry)
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
  int val;
  for(int i = 0; i < 2; i++) {
    val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }

  int const fSz = 16;
  if (pos+10+N*fSz > entry.end()) {
    MWAW_DEBUG_MSG(("CWGraph::readColorList: can not read data\n"));
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

shared_ptr<CWStruct::DSET> CWGraph::readBitmapZone
(CWStruct::DSET const &zone, MWAWEntry const &entry, bool &complete)
{
  complete = false;
  if (!entry.valid() || zone.m_fileType != 4)
    return shared_ptr<CWStruct::DSET>();
  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos+8+16, WPX_SEEK_SET); // avoid header+8 generic number
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  shared_ptr<CWGraphInternal::Group>
  graphicZone(new CWGraphInternal::Group(zone));

  f << "Entries(BitmapDef):" << *graphicZone << ",";

  ascFile.addDelimiter(input->tell(), '|');

  // read the last part
  long data0Length = zone.m_dataSz;
  long N = zone.m_numData;
  if (entry.length() -8-12 != data0Length*N + zone.m_headerSz) {
    if (data0Length == 0 && N) {
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      MWAW_DEBUG_MSG(("CWGraph::readBitmapZone: can not find definition size\n"));
      input->seek(entry.end(), WPX_SEEK_SET);
      return shared_ptr<CWStruct::DSET>();
    }

    MWAW_DEBUG_MSG(("CWGraph::readBitmapZone: unexpected size for zone definition, try to continue\n"));
  }

  shared_ptr<CWGraphInternal::ZoneBitmap> bitmap(new CWGraphInternal::ZoneBitmap());

  bool sizeSet=false;
  int sizePos = (version() == 1) ? 0: 88;
  if (sizePos && pos+sizePos+4+N*data0Length < entry.end()) {
    input->seek(pos+sizePos, WPX_SEEK_SET);
    ascFile.addDelimiter(pos+sizePos,'[');
    int dim[2]; // ( we must add 2 to add the border )
    for (int j = 0; j < 2; j++)
      dim[j] = (int) input->readLong(2);
    f << "sz=" << dim[1] << "x" << dim[0] << ",";
    if (dim[0] > 0 && dim[1] > 0) {
      bitmap->m_size = Vec2i(dim[1]+2, dim[0]+2);
      sizeSet = true;
    }
    ascFile.addDelimiter(input->tell(),']');
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  /** the end of this block is very simillar to a bitmapdef, excepted
      maybe the first integer  .... */
  if (long(input->tell())+(N+1)*data0Length <= entry.end())
    N++;

  input->seek(entry.end()-N*data0Length, WPX_SEEK_SET);

  for (int i = 0; i < N; i++) {
    pos = input->tell();
    MWAWEntry gEntry;
    gEntry.setBegin(pos);
    gEntry.setLength(data0Length);
    f.str("");
    f << "BitmapDef-" << i << ":";
    long val = (long) input->readULong(4);
    if (val) {
      if (i == 0)
        f << "unkn=" << val << ",";
      else
        f << "ptr=" << std::hex << val << std::dec << ",";
    }
    // f0 : 0 true color, if not number of bytes
    for (int j = 0; j < 3; j++) {
      val = (int) input->readLong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }
    int dim[2]; // ( we must add 2 to add the border )
    for (int j = 0; j < 2; j++)
      dim[j] = (int) input->readLong(2);
    if (i == N-1 && !sizeSet)
      bitmap->m_size = Vec2i(dim[0]+2, dim[1]+2);

    f << "dim?=" << dim[0] << "x" << dim[1] << ",";
    for (int j = 3; j < 6; j++) {
      val = (int) input->readLong(2);
      if ((j != 5 && val!=1) || (j==5 && val)) // always 1, 1, 0
        f << "f" << j << "=" << val << ",";
    }
    if (long(input->tell()) != gEntry.end())
      ascFile.addDelimiter(input->tell(), '|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(gEntry.end(), WPX_SEEK_SET);
  }

  input->seek(entry.end(), WPX_SEEK_SET);
  pos = entry.end();
  bool ok = readBitmapColorMap( bitmap->m_colorMap);
  if (ok) {
    pos = input->tell();
    ok = readBitmapData(*bitmap);
  }
  if (ok) {
    graphicZone->m_zones.resize(1);
    graphicZone->m_zones[0] = bitmap;
  } else
    input->seek(pos, WPX_SEEK_SET);

  // fixme: in general followed by another zone
  graphicZone->m_otherChilds.push_back(graphicZone->m_id+1);

  if (m_state->m_zoneMap.find(graphicZone->m_id) != m_state->m_zoneMap.end()) {
    MWAW_DEBUG_MSG(("CWGraph::readGroupZone: zone %d already exists!!!\n", graphicZone->m_id));
  } else
    m_state->m_zoneMap[graphicZone->m_id] = graphicZone;

  return graphicZone;
}

shared_ptr<CWGraphInternal::Zone> CWGraph::readGroupDef(MWAWEntry const &entry)
{
  shared_ptr<CWGraphInternal::Zone> res;
  if (entry.length() < 32) {
    if (version() > 1 || entry.length() < 30) {
      MWAW_DEBUG_MSG(("CWGraph::readGroupDef: sz is too short!!!\n"));
      return res;
    }
  }
  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "GroupDef:";

  CWGraphInternal::Zone zone;
  CWGraphInternal::Style &style = zone.m_style;

  int typeId = (int) input->readULong(1);
  CWGraphInternal::Zone::Type type = CWGraphInternal::Zone::T_Unknown;

  switch(version()) {
  case 1:
    switch(typeId) {
    case 1:
      type = CWGraphInternal::Zone::T_Zone;
      break;
    case 4:
      type = CWGraphInternal::Zone::T_Line;
      break;
    case 5:
      type = CWGraphInternal::Zone::T_Rect;
      break;
    case 6:
      type = CWGraphInternal::Zone::T_RectOval;
      break;
    case 7:
      type = CWGraphInternal::Zone::T_Oval;
      break;
    case 8:
      type = CWGraphInternal::Zone::T_Arc;
      break;
    case 9:
      type = CWGraphInternal::Zone::T_Poly;
      break;
    case 11:
      type = CWGraphInternal::Zone::T_Pict;
      break;
    case 13:
      type = CWGraphInternal::Zone::T_DataBox;
      break;
    default:
      break;
    }
    break;
  default:
    switch(typeId) {
    case 1:
      type = CWGraphInternal::Zone::T_Zone;
      break;
    case 2:
      type = CWGraphInternal::Zone::T_Line;
      break;
    case 3:
      type = CWGraphInternal::Zone::T_Rect;
      break;
    case 4:
      type = CWGraphInternal::Zone::T_RectOval;
      break;
    case 5:
      type = CWGraphInternal::Zone::T_Oval;
      break;
    case 6:
      type = CWGraphInternal::Zone::T_Arc;
      break;
    case 7:
      type = CWGraphInternal::Zone::T_Poly;
      break;
    case 8:
      type = CWGraphInternal::Zone::T_Pict;
      break;
    case 9:
      type = CWGraphInternal::Zone::T_Chart;
      break;
    case 10:
      type = CWGraphInternal::Zone::T_DataBox;
      break;
    case 14:
      type = CWGraphInternal::Zone::T_Movie;
      break;
    case 18:
      type = CWGraphInternal::Zone::T_QTim;
      break;
    default:
      break;
    }
    break;
  }
  int val = (int) input->readULong(1);
  style.m_wrapping = (val & 3);
  style.m_surfacePatternType = (val >> 2);
  style.m_lineFlags = (int) input->readULong(1);
  style.m_flags[0] = (int) input->readULong(1);

  int dim[4];
  for (int j = 0; j < 4; j++) {
    val = int(input->readLong(4)/256);
    dim[j] = val;
    if (val < -100) f << "##dim?,";
  }
  zone.m_box = Box2i(Vec2i(dim[1], dim[0]), Vec2i(dim[3], dim[2]));
  style.m_lineWidth = (int) input->readLong(1);
  style.m_flags[1] = (int) input->readLong(1);
  for (int j = 0; j < 2; j++)
    style.m_color[j] = (int) input->readULong(1);

  for (int j = 0; j < 2; j++) {
    style.m_flags[2+j] =  (int) input->readULong(1); // probably also related to surface
    style.m_pattern[j] = (int) input->readULong(1);
  }

  if (version() > 1)
    style.m_id = (int) input->readLong(2);

  switch (type) {
  case CWGraphInternal::Zone::T_Zone: {
    int nFlags = 0;
    CWGraphInternal::ZoneZone *z = new CWGraphInternal::ZoneZone(zone);
    res.reset(z);
    z->m_flags[nFlags++] = (int) input->readLong(2);
    z->m_id = (int) input->readULong(2);

    int numRemains = int(entry.end()-long(input->tell()));
    numRemains/=2;
    // v1-2:3 v4-v5:6 v6:8
    if (numRemains > 8) numRemains = 8;
    for (int j = 0; j < numRemains; j++) {
      val = (int) input->readLong(2);
      switch(j) {
      case 1:
        z->m_subId = val;
        break;
      case 2:
        z->m_wrappingSep = val;
        break;
      case 3: // checkme
        z->m_styleId = val;
        break;
      default:
        z->m_flags[nFlags++] = val;
        break;
      }
    }
    break;
  }
  case CWGraphInternal::Zone::T_Pict:
  case CWGraphInternal::Zone::T_QTim:
  case CWGraphInternal::Zone::T_Movie:
    res.reset(new CWGraphInternal::ZonePict(zone, type));
    break;
  case CWGraphInternal::Zone::T_Line:
  case CWGraphInternal::Zone::T_Rect:
  case CWGraphInternal::Zone::T_RectOval:
  case CWGraphInternal::Zone::T_Oval:
  case CWGraphInternal::Zone::T_Arc:
  case CWGraphInternal::Zone::T_Poly: {
    CWGraphInternal::ZoneBasic *z = new CWGraphInternal::ZoneBasic(zone, type);
    res.reset(z);
    readBasicGraphic(entry, *z);
    break;
  }
  case CWGraphInternal::Zone::T_DataBox:
  case CWGraphInternal::Zone::T_Chart:
  case CWGraphInternal::Zone::T_Basic:
  case CWGraphInternal::Zone::T_Picture:
  case CWGraphInternal::Zone::T_Unknown:
  case CWGraphInternal::Zone::T_Bitmap:
  default: {
    CWGraphInternal::ZoneUnknown *z = new CWGraphInternal::ZoneUnknown(zone);
    res.reset(z);
    z->m_type = type;
    z->m_typeId = typeId;
    break;
  }
  }

  f << *res;

  long actPos = input->tell();
  if (actPos != entry.begin() && actPos != entry.end())
    ascFile.addDelimiter(input->tell(),'|');
  input->seek(entry.end(), WPX_SEEK_SET);

  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  return res;
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool CWGraph::readGroupData(CWGraphInternal::Group &group, long beginGroupPos)
{
  //  bool complete = false;
  if (!readGroupHeader(group)) {
    MWAW_DEBUG_MSG(("CWGraph::readGroupData: unexpected graphic1\n"));
    return false;
  }

  MWAWInputStreamPtr &input= m_parserState->m_input;

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  int const vers = version();
  long pos, sz;
  size_t numChilds = group.m_zones.size();
  int numError = 0;
  for (size_t i = 0; i < numChilds; i++) {
    shared_ptr<CWGraphInternal::Zone> z = group.m_zones[i];
    int numZoneExpected = z ? z->getNumData() : 0;

    if (numZoneExpected) {
      pos = input->tell();
      sz = (long) input->readULong(4);
      f.str("");
      if (sz == 0) {
        MWAW_DEBUG_MSG(("CWGraph::readGroupData: find a nop zone for type: %d\n",
                        z->getSubType()));
        ascFile.addPos(pos);
        ascFile.addNote("#Nop");
        if (!numError++) {
          ascFile.addPos(beginGroupPos);
          ascFile.addNote("###");
        } else {
          MWAW_DEBUG_MSG(("CWGraph::readGroupData: too many errors, zone parsing STOPS\n"));
          return false;
        }
        pos = input->tell();
        sz = (long) input->readULong(4);
      }
      input->seek(pos, WPX_SEEK_SET);
      bool parsed = true;
      switch(z->getSubType()) {
      case CWGraphInternal::Zone::T_QTim:
        if (!readQTimeData(z))
          return false;
        break;
      case CWGraphInternal::Zone::T_Movie:
        // FIXME: pict ( containing movie ) + ??? +
      case CWGraphInternal::Zone::T_Pict:
        if (!readPictData(z))
          return false;
        break;
      case CWGraphInternal::Zone::T_Poly:
        if (!readPolygonData(z))
          return false;
        break;
      case CWGraphInternal::Zone::T_Line:
      case CWGraphInternal::Zone::T_Rect:
      case CWGraphInternal::Zone::T_RectOval:
      case CWGraphInternal::Zone::T_Oval:
      case CWGraphInternal::Zone::T_Arc:
      case CWGraphInternal::Zone::T_Zone:
      case CWGraphInternal::Zone::T_Basic:
      case CWGraphInternal::Zone::T_Picture:
      case CWGraphInternal::Zone::T_Chart:
      case CWGraphInternal::Zone::T_DataBox:
      case CWGraphInternal::Zone::T_Unknown:
      case CWGraphInternal::Zone::T_Bitmap:
      default:
        parsed = false;
        break;
      }

      if (!parsed) {
        input->seek(pos+4+sz, WPX_SEEK_SET);
        if (long(input->tell()) != pos+4+sz) {
          input->seek(pos, WPX_SEEK_SET);
          MWAW_DEBUG_MSG(("CWGraph::readGroupData: find a odd zone for type: %d\n",
                          z->getSubType()));
          return false;
        }
        f.str("");
        if (z->getSubType() == CWGraphInternal::Zone::T_Chart)
          f << "Entries(ChartData)";
        else
          f << "Entries(UnknownDATA)-" << z->getSubType();
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        input->seek(pos+4+sz, WPX_SEEK_SET);
        if (numZoneExpected==2) {
          pos = input->tell();
          sz = (long) input->readULong(4);
          if (sz) {
            input->seek(pos, WPX_SEEK_SET);
            MWAW_DEBUG_MSG(("CWGraph::readGroupData: two zones is not implemented for zone: %d\n",
                            z->getSubType()));
            return false;
          }
          ascFile.addPos(pos);
          ascFile.addNote("NOP");
        }
      }
    }
    if (vers>=6) {
      pos = input->tell();
      sz = (long) input->readULong(4);
      if (sz == 0) {
        ascFile.addPos(pos);
        ascFile.addNote("Nop");
        continue;
      }
      MWAW_DEBUG_MSG(("CWGraph::readGroupData: find not null entry for a end of zone: %d\n", z->getSubType()));
      input->seek(pos, WPX_SEEK_SET);
    }
  }

  if (input->atEOS())
    return true;
  // sanity check: normaly no zero except maybe for the last zone
  pos = input->tell();
  sz = (long) input->readULong(4);
  if (sz == 0 && !input->atEOS()) {
    MWAW_DEBUG_MSG(("CWGraph::readGroupData: find unexpected nop data at end of zone\n"));
    ascFile.addPos(beginGroupPos);
    ascFile.addNote("###");
  }
  input->seek(pos, WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////
bool CWGraph::readBasicGraphic(MWAWEntry const &entry,
                               CWGraphInternal::ZoneBasic &zone)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long actPos = input->tell();
  int remainBytes = int(entry.end()-actPos);
  if (remainBytes < 0)
    return false;

  int actFlag = 0;
  switch(zone.getSubType()) {
  case CWGraphInternal::Zone::T_Line:
  case CWGraphInternal::Zone::T_Rect:
  case CWGraphInternal::Zone::T_Oval:
    break;
  case CWGraphInternal::Zone::T_Poly:
    break; // value are defined in next zone
  case CWGraphInternal::Zone::T_Arc: {
    if (remainBytes < 4) {
      MWAW_DEBUG_MSG(("CWGraph::readSimpleGraphicZone: arc zone is too short\n"));
      return false;
    }
    for (int i = 0; i < 2; i++)
      zone.m_values[i] = (float) input->readLong(2);
    break;
  }
  case CWGraphInternal::Zone::T_RectOval: {
    if (remainBytes < 8) {
      MWAW_DEBUG_MSG(("CWGraph::readSimpleGraphicZone: arc zone is too short\n"));
      return false;
    }
    for (int i = 0; i < 2; i++) {
      zone.m_values[size_t(i)] = float(input->readLong(2))/2.0f;
      zone.m_flags[actFlag++] = (int) input->readULong(2);
    }
    break;
  }
  case CWGraphInternal::Zone::T_Zone:
  case CWGraphInternal::Zone::T_Basic:
  case CWGraphInternal::Zone::T_Picture:
  case CWGraphInternal::Zone::T_Chart:
  case CWGraphInternal::Zone::T_DataBox:
  case CWGraphInternal::Zone::T_Unknown:
  case CWGraphInternal::Zone::T_Pict:
  case CWGraphInternal::Zone::T_QTim:
  case CWGraphInternal::Zone::T_Movie:
  case CWGraphInternal::Zone::T_Bitmap:
  default:
    MWAW_DEBUG_MSG(("CWGraph::readSimpleGraphicZone: unknown type\n"));
    return false;
  }

  int numRemain = int(entry.end()-input->tell());
  numRemain /= 2;
  if (numRemain+actFlag > 8) numRemain = 8-actFlag;
  for (int i = 0; i < numRemain; i++)
    zone.m_flags[actFlag++] = (int) input->readLong(2);

  return true;
}

bool CWGraph::readGroupHeader(CWGraphInternal::Group &group)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  // int const vers=version();
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "GroupDef(Header):";
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()!=endPos) || (sz && sz < 16)) {
    MWAW_DEBUG_MSG(("CWGraph::readGroupHeader: zone is too short\n"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  if (sz == 0) {
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return true;
  }

  input->seek(pos+4, WPX_SEEK_SET);
  int N = (int) input->readULong(2);
  f << "N=" << N << ",";
  int type = (int) input->readLong(2);
  if (type != -1)
    f << "#type=" << type << ",";
  int val = (int) input->readLong(2);
  if (val) f << "#unkn=" << val << ",";
  int fSz = (int) input->readULong(2);
  if (!fSz || N *fSz+12 != sz) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  for (int i = 0; i < 2; i++) { // always 0, 2
    val = (int) input->readLong(2);
    if (val)
      f << "f" << i << "=" << val;
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  input->seek(pos+4+12, WPX_SEEK_SET);
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    if (readGroupUnknown(group, fSz, i))
      continue;
    ascFile.addPos(pos);
    ascFile.addNote("GroupDef(Head-###)");
    input->seek(pos+fSz, WPX_SEEK_SET);
  }

  /** a list of int16 : find
      00320060 00480060 0048ffe9 013a0173 01ba0173 01ea02a0
      01f8ffe7 02080295 020c012c 02140218 02ae01c1
      02ca02c9-02cc02c6-02400000
      03f801e6
      8002e3ff e0010000 ee02e6ff */
  int numHeader = N+1;//vers >=6 ? N+1 : 2*N;
  for (int i = 0; i < numHeader; i++) {
    pos = input->tell();
    std::vector<int> res;
    bool ok = m_mainParser->readStructIntZone("", false, 2, res);
    f.str("");
    f << "[GroupDef(data" << i << ")]";
    if (ok) {
      if (input->tell() != pos+4) {
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
      }
      continue;
    }
    input->seek(pos, WPX_SEEK_SET);
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    MWAW_DEBUG_MSG(("CWGraph::readGroupHeader: can not find data for %d\n", i));
    return true;
  }

  return true;
}

bool CWGraph::readGroupUnknown(CWGraphInternal::Group &/*group*/, int zoneSz, int id)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  input->seek(pos+zoneSz, WPX_SEEK_SET);
  if (input->tell() != pos+zoneSz) {
    input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWGraph::readGroupUnknown: zone is too short\n"));
    return false;
  }
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "GroupDef(Head-";
  if (id >= 0) f << id << "):";
  else f << "_" << "):";
  if (zoneSz < 42) {
    MWAW_DEBUG_MSG(("CWGraph::readGroupUnknown: zone is too short\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return true;
  }

  input->seek(pos, WPX_SEEK_SET);
  int type = (int) input->readLong(2); // find -1, 0, 3
  if (type) f << "f0=" << type << ",";
  long val;
  for (int i = 0; i < 6; i++) {
    /** find f1=8|9|f|14|15|2a|40|73|e9, f2=0|d4, f5=0|80, f6=0|33 */
    val = (long) input->readULong(1);
    if (val) f << "f" << i+1 << "=" << std::hex << val << std::dec << ",";
  }
  std::vector<int16_t> values16; // some values can be small or little endian, so...
  std::vector<int32_t> values32;
  for (int i = 0; i < 2; i++)
    values32.push_back((int32_t) input->readLong(4));
  // now two smal number also in big or small endian
  for (int i = 0; i < 2; i++)
    values16.push_back((int16_t) input->readLong(2));
  // a very big number
  values32.push_back((int32_t) input->readLong(4));
  m_mainParser->checkOrdering(values16, values32);

  if (values32[0] || values32[1]) f << "dim=" << values32[0] << "x" << values32[1] << ",";
  for (size_t i = 0; i < 2; i++) {
    if (values16[i])
      f << "g" << int(i) << "=" << values16[i] << ",";
  }
  if (values32[2])
    f << "g2=" << std::hex << values32[2] << std::dec << ",";

  if (input->tell() != pos+zoneSz) {
    ascFile.addDelimiter(input->tell(), '|');
    input->seek(pos+zoneSz, WPX_SEEK_SET);
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read the polygon vertices
////////////////////////////////////////////////////////////
bool CWGraph::readPolygonData(shared_ptr<CWGraphInternal::Zone> zone)
{
  if (!zone || zone->getSubType() != CWGraphInternal::Zone::T_Poly)
    return false;
  CWGraphInternal::ZoneBasic *bZone =
    reinterpret_cast<CWGraphInternal::ZoneBasic *>(zone.get());
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos || sz < 12) {
    input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWGraph::readPolygonData: file is too short\n"));
    return false;
  }

  input->seek(pos+4, WPX_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(PolygonData):";
  int N = (int) input->readULong(2);
  f << "N=" << N << ",";
  int val = (int) input->readLong(2);
  if (val != -1) f << "f0=" << val << ",";
  val = (int) input->readLong(2);
  if (val) f << "f1=" << val << ",";
  int fSz = (int) input->readLong(2);
  if (sz != 12+fSz*N) {
    input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWGraph::readPolygonData: find odd data size\n"));
    return false;
  }
  for (int i = 2; i < 4; i++) {
    val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";

  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    f << "PolygonData-" << i << ":";
    float position[2];
    for (int j = 0; j < 2; j++)
      position[j] = float(input->readLong(4))/256.f;
    CWGraphInternal::CurvePoint point(Vec2f(position[1], position[0]));
    if (fSz >= 26) {
      for (int cPt = 0; cPt < 2; cPt++) {
        float ctrlPos[2];
        for (int j = 0; j < 2; j++)
          ctrlPos[j] = float(input->readLong(4))/256.f;
        point.m_controlPoints[cPt] = Vec2f(ctrlPos[1], ctrlPos[0]);
      }
      int fl = (int) input->readULong(2);
      point.m_type = (fl>>14);
      f << point << ",";
      if (fl&0x3FFF)
        f << "unkn=" << std::hex << int(fl&0x3FFF) << std::dec << ",";
    } else
      f << point << ",";

    bZone->m_vertices.push_back(point);

    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    input->seek(pos+fSz, WPX_SEEK_SET);
  }
  input->seek(endPos, WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// read some picture
////////////////////////////////////////////////////////////
bool CWGraph::readPictData(shared_ptr<CWGraphInternal::Zone> zone)
{
  if (!zone || (zone->getSubType() != CWGraphInternal::Zone::T_Pict &&
                zone->getSubType() != CWGraphInternal::Zone::T_Movie))
    return false;
  CWGraphInternal::ZonePict *pZone =
    reinterpret_cast<CWGraphInternal::ZonePict *>(zone.get());
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  if (!readPICT(*pZone)) {
    MWAW_DEBUG_MSG(("CWGraph::readPictData: find a odd pict\n"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  pos = input->tell();
  long sz = (long) input->readULong(4);
  input->seek(pos+4+sz, WPX_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  if (long(input->tell()) != pos+4+sz) {
    input->seek(pos, WPX_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote("###");
    MWAW_DEBUG_MSG(("CWGraph::readPictData: find a end zone for graphic\n"));
    return false;
  }
  if (sz == 0) {
    ascFile.addPos(pos);
    ascFile.addNote("Nop");
    return true;
  }

  // fixme: use readPS for a mac file and readOLE for a pc file
  input->seek(pos, WPX_SEEK_SET);
  if (readPS(*pZone))
    return true;

  input->seek(pos, WPX_SEEK_SET);
  if (readOLE(*pZone))
    return true;

  MWAW_DEBUG_MSG(("CWGraph::readPictData: unknown data file\n"));
#ifdef DEBUG_WITH_FILES
  if (1) {
    WPXBinaryData file;
    input->seek(pos+4, WPX_SEEK_SET);
    input->readDataBlock(sz, file);
    static int volatile pictName = 0;
    libmwaw::DebugStream f;
    f << "DATA-" << ++pictName;
    libmwaw::Debug::dumpFile(file, f.str().c_str());
  }
#endif
  ascFile.addPos(pos);
  if (zone->getSubType() == CWGraphInternal::Zone::T_Movie)
    ascFile.addNote("Entries(MovieData2):#"); // find filesignature: ALMC...
  else
    ascFile.addNote("Entries(PictData2):#");
  ascFile.skipZone(pos+4, pos+4+sz-1);

  input->seek(pos+4+sz, WPX_SEEK_SET);
  return true;
}

bool CWGraph::readPICT(CWGraphInternal::ZonePict &zone)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  if (sz < 12) {
    MWAW_DEBUG_MSG(("CWGraph::readPict: file is too short\n"));
    return false;
  }

  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("CWGraph::readPict: file is too short\n"));
    return false;
  }
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(Graphic):";

  Box2f box;
  input->seek(pos+4, WPX_SEEK_SET);

  MWAWPict::ReadResult res = MWAWPictData::check(input, (int)sz, box);
  if (res == MWAWPict::MWAW_R_BAD) {
    MWAW_DEBUG_MSG(("CWGraph::readPict: can not find the picture\n"));
    input->seek(pos, WPX_SEEK_SET);
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    return false;
  }

  zone.m_entries[0].setBegin(pos+4);
  zone.m_entries[0].setEnd(endPos);
  input->seek(endPos, WPX_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  return true;
}

bool CWGraph::readPS(CWGraphInternal::ZonePict &zone)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long header = (long) input->readULong(4);
  if (header != 0x25215053L) {
    return false;
  }
  long endPos = pos+4+sz;
  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos) {
    return false;
  }
  zone.m_entries[1].setBegin(pos+4);
  zone.m_entries[1].setEnd(endPos);
  zone.m_entries[1].setType("PS");
#ifdef DEBUG_WITH_FILES
  if (1) {
    WPXBinaryData file;
    input->seek(pos+4, WPX_SEEK_SET);
    input->readDataBlock(sz, file);
    static int volatile pictName = 0;
    libmwaw::DebugStream f;
    f << "PostScript-" << ++pictName << ".ps";
    libmwaw::Debug::dumpFile(file, f.str().c_str());
  }
#endif

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(PostScript):";
  input->seek(endPos, WPX_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  ascFile.skipZone(pos+4, endPos-1);

  return true;
}

bool CWGraph::readOLE(CWGraphInternal::ZonePict &zone)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long val = input->readLong(4);
  if (sz <= 24 || val != 0 || input->readULong(4) != 0x1000000)
    return false;
  long endPos = pos+4+sz;
  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos)
    return false;
  input->seek(pos+12, WPX_SEEK_SET);
  // now a dim in little endian
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(OLE):";
  int dim[4];
  for (int i = 0; i < 4; i++)
    dim[i] = (int)(int32_t)MWAWInputStream::readULong(input->input().get(), 4, 0, true);
  if (dim[0] >= dim[2] || dim[1] >= dim[3]) return false;
  f << "dim=" << dim[1] << "x" << dim[0] << "<->" << dim[3] << "x" << dim[2] << ",";
  zone.m_entries[1].setBegin(pos+28);
  zone.m_entries[1].setEnd(endPos);
  zone.m_entries[1].setType("OLE");
#ifdef DEBUG_WITH_FILES
  if (1) {
    WPXBinaryData file;
    input->seek(pos+28, WPX_SEEK_SET);
    input->readDataBlock(sz-24, file);
    static int volatile pictName = 0;
    libmwaw::DebugStream f2;
    f2 << "OLE-" << ++pictName;
    libmwaw::Debug::dumpFile(file, f2.str().c_str());
  }
#endif

  input->seek(endPos, WPX_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  ascFile.skipZone(pos+28, endPos-1);

  return true;
}

////////////////////////////////////////////////////////////
// read Qtime picture
////////////////////////////////////////////////////////////
bool CWGraph::readQTimeData(shared_ptr<CWGraphInternal::Zone> zone)
{
  if (!zone || zone->getSubType() != CWGraphInternal::Zone::T_QTim)
    return false;
  CWGraphInternal::ZonePict *pZone =
    reinterpret_cast<CWGraphInternal::ZonePict *>(zone.get());
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  bool ok = true;
  std::string name("");
  for (int i = 0; i < 4; i++) {
    char c = (char) input->readULong(1);
    if (c == 0) ok = false;
    name += c;
  }
  if (!ok) {
    MWAW_DEBUG_MSG(("CWGraph::readQTimeData: find a odd qtim zone\n"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(QTIM):"<< name << ":";
  for (int i = 0; i < 2; i++) f << "f" << i << "=" << input->readULong(2) << ",";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  pos = input->tell();
  if (!readNamedPict(*pZone)) {
    MWAW_DEBUG_MSG(("CWGraph::readQTimeData: find a odd named pict\n"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  return true;
}


bool CWGraph::readNamedPict(CWGraphInternal::ZonePict &zone)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  std::string name("");
  for (int i = 0; i < 4; i++) {
    char c = (char) input->readULong(1);
    if (c < ' ' || c > 'z') {
      MWAW_DEBUG_MSG(("CWGraph::readNamedPict: can not find the name\n"));
      return false;
    }
    name+=c;
  }
  long sz = (long) input->readULong(4);
  long endPos = pos+8+sz;
  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos || !sz) {
    MWAW_DEBUG_MSG(("CWGraph::readNamedPict: file is too short\n"));
    return false;
  }

  zone.m_entries[0].setBegin(pos+8);
  zone.m_entries[0].setEnd(endPos);

#ifdef DEBUG_WITH_FILES
  if (1) {
    WPXBinaryData file;
    input->seek(pos+8, WPX_SEEK_SET);
    input->readDataBlock(sz, file);
    static int volatile pictName = 0;
    libmwaw::DebugStream f;
    f << "PICT2-" << ++pictName << "." << name;
    libmwaw::Debug::dumpFile(file, f.str().c_str());
  }
#endif

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(" << name << "):";
  input->seek(endPos, WPX_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  ascFile.skipZone(pos+8, endPos-1);

  return true;
}

////////////////////////////////////////////////////////////
// read bitmap picture
////////////////////////////////////////////////////////////
bool CWGraph::readBitmapColorMap(std::vector<MWAWColor> &cMap)
{
  cMap.resize(0);
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  if (!sz) {
    ascFile.addPos(pos);
    ascFile.addNote("Nop");
    return true;
  }
  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("CWGraph::readBitmapColorMap: file is too short\n"));
    return false;
  }

  input->seek(pos+4, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(BitmapColor):";
  f << "unkn=" << input->readLong(4) << ",";
  int maxColor = (int) input->readLong(4);
  if (sz != 8+8*(maxColor+1)) {
    MWAW_DEBUG_MSG(("CWGraph::readBitmapColorMap: sz is odd\n"));
    return false;
  }
  cMap.resize(size_t(maxColor+1));
  for (int i = 0; i <= maxColor; i++) {
    int id = (int) input->readULong(2);
    if (id != i) {
      MWAW_DEBUG_MSG(("CWGraph::readBitmapColorMap: find odd index : %d\n", i));
      return false;
    }
    unsigned char col[3];
    for (int c = 0; c < 3; c++) col[c] = (unsigned char)(input->readULong(2)>>8);
    cMap[(size_t)i] = MWAWColor(col[0], col[1], col[2]);
  }

  input->seek(endPos, WPX_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  return true;
}

bool CWGraph::readBitmapData(CWGraphInternal::ZoneBitmap &zone)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos || !sz) {
    MWAW_DEBUG_MSG(("CWGraph::readBitmapData: file is too short\n"));
    return false;
  }
  /* Fixme: this code can not works for the packed bitmap*/
  long numColors = zone.m_size[0]*zone.m_size[1];
  int numBytes = numColors ? int(sz/numColors) : 0;
  if (sz != numBytes*numColors) {
    MWAW_DEBUG_MSG(("CWGraph::readBitmapData: unexpected size\n"));
    return false;
  }
  zone.m_bitmapType = numBytes;
  zone.m_entry.setBegin(pos+4);
  zone.m_entry.setEnd(endPos);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(BitmapData):nBytes=" << numBytes;
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  ascFile.skipZone(pos+4, endPos-1);

  return true;
}

////////////////////////////////////////////////////////////
// update the group information
////////////////////////////////////////////////////////////
void CWGraph::updateInformation(CWGraphInternal::Group &group)
{
  if (group.m_blockToSendList.size() || group.m_idLinkedZonesMap.size())
    return;
  std::set<int> forbiddenZone;

  if (group.m_type == CWStruct::DSET::T_Main) {
    int headerId=0, footerId=0;
    m_mainParser->getHeaderFooterId(headerId, footerId);
    if (headerId) forbiddenZone.insert(headerId);
    if (footerId) forbiddenZone.insert(footerId);
  }

  for (size_t g = 0; g < group.m_zones.size(); g++) {
    CWGraphInternal::Zone *child = group.m_zones[g].get();
    if (!child) continue;
    if (child->getType() != CWGraphInternal::Zone::T_Zone) {
      group.m_blockToSendList.push_back(g);
      group.m_totalNumber++;
      continue;
    }

    CWGraphInternal::ZoneZone const &childZone =
      reinterpret_cast<CWGraphInternal::ZoneZone &>(*child);
    int zId = childZone.m_id;
    if (!group.okChildId(zId) || forbiddenZone.find(zId) != forbiddenZone.end())
      continue;

    group.m_totalNumber++;
    if (zId==1) {
      group.m_hasMainZone = true;
      continue;
    }

    group.m_blockToSendList.push_back(g);

    if (group.m_idLinkedZonesMap.find(zId) == group.m_idLinkedZonesMap.end())
      group.m_idLinkedZonesMap.insert
      (std::map<int,CWGraphInternal::Group::LinkedZones>::value_type
       (zId,CWGraphInternal::Group::LinkedZones(m_state->m_frameId++)));
    CWGraphInternal::Group::LinkedZones &lZone = group.m_idLinkedZonesMap.find(zId)->second;
    if (lZone.m_mapIdChild.find(childZone.m_subId) != lZone.m_mapIdChild.end()) {
      MWAW_DEBUG_MSG(("CWGraph::updateInformation: zone %d already find with subId %d\n",
                      zId, childZone.m_subId));
      continue;
    }
    lZone.m_mapIdChild[childZone.m_subId] = g;
  }
}

////////////////////////////////////////////////////////////
// send data to the listener
////////////////////////////////////////////////////////////
bool CWGraph::sendZone(int number, MWAWPosition position)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("CWGraph::sendZone: can not find the listener\n"));
    return false;
  }
  std::map<int, shared_ptr<CWGraphInternal::Group> >::iterator iter
    = m_state->m_zoneMap.find(number);
  if (iter == m_state->m_zoneMap.end() || !iter->second)
    return false;
  shared_ptr<CWGraphInternal::Group> group = iter->second;
  updateInformation(*group);

  bool mainGroup = group->m_type == CWStruct::DSET::T_Main;
  libmwaw::SubDocumentType inDocType;
  if (!listener->isSubDocumentOpened(inDocType))
    inDocType = libmwaw::DOC_NONE;
  Vec2f leftTop(0,0);
  float pageHeight = 0.0;
  if (mainGroup) {
    leftTop = 72.0f*m_mainParser->getPageLeftTop();
    pageHeight = 72.0f*m_mainParser->pageHeight();
  }

  if (group->m_totalNumber > 1 &&
      (position.m_anchorTo==MWAWPosition::Char ||
       position.m_anchorTo==MWAWPosition::CharBaseLine)) {
    // we need to a frame, ...
    m_mainParser->sendZoneInFrame(number, position);
    return true;
  }

  MWAWPosition::AnchorTo suggestedAnchor = MWAWPosition::Char;
  switch(inDocType) {
  case libmwaw::DOC_TEXT_BOX:
    suggestedAnchor=MWAWPosition::Char;
    break;
  case libmwaw::DOC_HEADER_FOOTER:
  case libmwaw::DOC_NOTE:
    suggestedAnchor=MWAWPosition::Frame;
    break;
  case libmwaw::DOC_TABLE:
  case libmwaw::DOC_COMMENT_ANNOTATION:
    suggestedAnchor=MWAWPosition::Char;
    break;
  default:
  case libmwaw::DOC_NONE:
    suggestedAnchor= mainGroup ? MWAWPosition::Page : MWAWPosition::Char;
    break;
  }
  std::vector<size_t> notDone;
  for (int st = 0; st < 2; st++) {
    std::vector<size_t> const &toDo = st==0 ? group->m_blockToSendList : notDone;
    if (st == 1) {
      suggestedAnchor = MWAWPosition::Char;
      if (group->m_hasMainZone)
        m_mainParser->sendZone(1);
    }
    size_t numZones = toDo.size();
    for (size_t g = 0; g < numZones; g++) {
      CWGraphInternal::Zone *child = group->m_zones[toDo[g]].get();
      if (!child) continue;

      bool posValidSet = child->m_box.size()[0] > 0 && child->m_box.size()[0] > 1;
      MWAWPosition pos(position);
      pos.setOrder(int(g)+1);
      if (pos.m_anchorTo==MWAWPosition::Unknown) {
        pos = MWAWPosition(child->m_box[0], child->m_box.size(), WPX_POINT);
        pos.setRelativePosition(suggestedAnchor);
        if (suggestedAnchor == MWAWPosition::Page) {
          int pg = child->m_page >= 0 ? child->m_page+1 : 1;
          Vec2f orig = pos.origin()+leftTop;
          pos.setPagePos(pg, orig);
          pos.m_wrapping =  MWAWPosition::WBackground;
          pos.setOrder(-int(g)-1);
          if (pos.origin()[1]+pos.size()[1] >= pageHeight
              || listener->isSectionOpened()) {
            notDone.push_back(toDo[g]);
            continue;
          }
        } else if (st==1 || suggestedAnchor == MWAWPosition::Char)
          pos.setOrigin(Vec2f(0,0));
      }
      if (pos.size()[0] <= 0 || pos.size()[1] <= 0)
        pos.setSize(child->m_box.size());
      switch (child->getType()) {
      case CWGraphInternal::Zone::T_Zone: {
        CWGraphInternal::ZoneZone const &childZone =
          reinterpret_cast<CWGraphInternal::ZoneZone &>(*child);
        int zId = childZone.m_id;
        CWStruct::DSET::Type type = m_mainParser->getZoneType(zId);
        CWGraphInternal::Style const cStyle = childZone.m_style;
        WPXPropertyList extras, textboxExtras;
        MWAWColor color;
        switch (cStyle.m_wrapping&3) {
        case 0:
          pos.m_wrapping = MWAWPosition::WRunThrough;
          break;
        case 1:
        case 2:
          pos.m_wrapping = MWAWPosition::WDynamic;
          break;
        default:
          break;
        }
        if (cStyle.m_color[1] > 0 && getSurfaceColor(cStyle, color))
          extras.insert("fo:background-color", color.str().c_str());
        else
          extras.insert("style:background-transparency", "100%");
        if (cStyle.m_lineWidth > 0 && cStyle.m_color[0] > 0 &&
            getLineColor(cStyle, color)) {
          std::stringstream stream;
          stream << cStyle.m_lineWidth*0.03 << "cm solid "
                 << color;
          extras.insert("fo:border", stream.str().c_str());
          // extend the frame to add border
          float extend = float(cStyle.m_lineWidth*0.85);
          pos.setOrigin(pos.origin()-Vec2f(extend,extend));
          pos.setSize(pos.size()+2.0*Vec2f(extend,extend));
        }
        int fZid = zId;
        if (!group->needSendFrameContent(zId, childZone.m_subId, extras, textboxExtras))
          fZid = -1;
        if ((type == CWStruct::DSET::T_Frame ||
             type == CWStruct::DSET::T_Table) && posValidSet)
          m_mainParser->sendZoneInFrame(fZid, pos, extras, textboxExtras);
        else if (fZid == -1)
          break;
        else if (pos.m_anchorTo == MWAWPosition::Page)
          notDone.push_back(toDo[g]);
        else
          m_mainParser->sendZone(zId, position);
        break;
      }
      case CWGraphInternal::Zone::T_Picture:
        sendPicture
        (reinterpret_cast<CWGraphInternal::ZonePict &>(*child), pos);
        break;
      case CWGraphInternal::Zone::T_Basic:
        sendBasicPicture
        (reinterpret_cast<CWGraphInternal::ZoneBasic &>(*child), pos);
        break;
      case CWGraphInternal::Zone::T_Bitmap:
        sendBitmap
        (reinterpret_cast<CWGraphInternal::ZoneBitmap &>(*child), pos);
        break;
      case CWGraphInternal::Zone::T_DataBox:
      case CWGraphInternal::Zone::T_Chart:
      case CWGraphInternal::Zone::T_Unknown:
        break;
      case CWGraphInternal::Zone::T_Pict:
      case CWGraphInternal::Zone::T_QTim:
      case CWGraphInternal::Zone::T_Movie:
      case CWGraphInternal::Zone::T_Line:
      case CWGraphInternal::Zone::T_Rect:
      case CWGraphInternal::Zone::T_RectOval:
      case CWGraphInternal::Zone::T_Oval:
      case CWGraphInternal::Zone::T_Arc:
      case CWGraphInternal::Zone::T_Poly:
      default:
        MWAW_DEBUG_MSG(("CWGraph::sendZone: find unknown zone\n"));
        break;
      }
    }
  }
  group->m_parsed = true;
  return true;
}

bool CWGraph::sendBasicPicture(CWGraphInternal::ZoneBasic &pict,
                               MWAWPosition pos, WPXPropertyList extras)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) return true;

  Vec2i pictSz = pict.m_box.size();
  if (pictSz[0] < 0) pictSz.setX(-pictSz[0]);
  if (pictSz[1] < 0) pictSz.setY(-pictSz[1]);
  Box2i box(Vec2i(0,0), pictSz);

  if (pos.size()[0] < 0 || pos.size()[1] < 0)
    pos.setSize(pictSz);

  shared_ptr<MWAWPictBasic> pictPtr;
  switch(pict.getSubType()) {
  case CWGraphInternal::Zone::T_Line: {
    MWAWPictLine *res=new MWAWPictLine(Vec2i(0,0), pict.m_box.size());
    pictPtr.reset(res);
    if (pict.m_style.m_lineFlags & 0x40) res->setArrow(0, true);
    if (pict.m_style.m_lineFlags & 0x80) res->setArrow(1, true);
    break;
  }
  case CWGraphInternal::Zone::T_Rect: {
    MWAWPictRectangle *res=new MWAWPictRectangle(box);
    pictPtr.reset(res);
    break;
  }
  case CWGraphInternal::Zone::T_RectOval: {
    MWAWPictRectangle *res=new MWAWPictRectangle(box);
    int roundValues[2];
    for (int i = 0; i < 2; i++) {
      if (2*pict.m_values[i] <= pictSz[i])
        roundValues[i] = int(pict.m_values[i]);
      else if (pictSz[i] >= 4.0)
        roundValues[i]= int(pictSz[i])/2-1;
      else
        roundValues[i]=1;
    }
    res->setRoundCornerWidth(roundValues[0], roundValues[1]);
    pictPtr.reset(res);
    break;
  }
  case CWGraphInternal::Zone::T_Oval: {
    MWAWPictCircle *res=new MWAWPictCircle(box);
    pictPtr.reset(res);
    break;
  }
  case CWGraphInternal::Zone::T_Arc: {
    int angle[2] = { int(90-pict.m_values[0]-pict.m_values[1]),
                     int(90-pict.m_values[0])
                   };
    while (angle[1] > 360) {
      angle[0]-=360;
      angle[1]-=360;
    }
    while (angle[0] < -360) {
      angle[0]+=360;
      angle[1]+=360;
    }
    Vec2f center = box.center();
    Vec2f axis = 0.5*Vec2f(box.size());
    // we must compute the real bd box
    float minVal[2] = { 0, 0 }, maxVal[2] = { 0, 0 };
    int limitAngle[2];
    for (int i = 0; i < 2; i++)
      limitAngle[i] = (angle[i] < 0) ? int(angle[i]/90)-1 : int(angle[i]/90);
    for (int bord = limitAngle[0]; bord <= limitAngle[1]+1; bord++) {
      float ang = (bord == limitAngle[0]) ? float(angle[0]) :
                  (bord == limitAngle[1]+1) ? float(angle[1]) : float(90 * bord);
      ang *= float(M_PI/180.);
      float actVal[2] = { axis[0] *std::cos(ang), -axis[1] *std::sin(ang)};
      if (actVal[0] < minVal[0]) minVal[0] = actVal[0];
      else if (actVal[0] > maxVal[0]) maxVal[0] = actVal[0];
      if (actVal[1] < minVal[1]) minVal[1] = actVal[1];
      else if (actVal[1] > maxVal[1]) maxVal[1] = actVal[1];
    }
    Box2i realBox(Vec2i(int(center[0]+minVal[0]),int(center[1]+minVal[1])),
                  Vec2i(int(center[0]+maxVal[0]),int(center[1]+maxVal[1])));
    MWAWPictArc *res=new MWAWPictArc(realBox,box, float(angle[0]), float(angle[1]));
    pictPtr.reset(res);
    break;
  }
  case CWGraphInternal::Zone::T_Poly: {
    size_t numPts = pict.m_vertices.size();
    if (!numPts) break;
    std::vector<Vec2f> listVertices;
    listVertices.resize(numPts);
    bool isSpline = false;
    for (size_t i = 0; i < numPts; i++) {
      listVertices[i] = pict.m_vertices[i].m_pos;
      if (pict.m_vertices[i].m_type >= 2)
        isSpline = true;
    }
    if (!isSpline) {
      // if (pict.m_style.m_lineFlags & 1) : we must close the polygon ?
      MWAWPictPolygon *res=new MWAWPictPolygon(box, listVertices);
      pictPtr.reset(res);
    } else {
      std::stringstream s;
      Vec2f prevPoint, pt1;
      bool hasPrevPoint = false;
      for (size_t i = 0; i < numPts; i++) {
        CWGraphInternal::CurvePoint const &pt = pict.m_vertices[i];
        if (pt.m_type >= 2) pt1 = pt.m_controlPoints[0];
        else pt1 = pt.m_pos;
        if (hasPrevPoint) {
          s << "C " << prevPoint[0] << " " << prevPoint[1]
            << " " << pt1[0] << " " << pt1[1] << " ";
        } else if (i==0)
          s << "M ";
        else if (pt.m_type < 2)
          s << "L ";
        else
          s << "S " << pt1[0] << " " << pt1[1] << " ";
        s << pt.m_pos[0] << " " << pt.m_pos[1] << " ";
        hasPrevPoint = pt.m_type >= 2;
        if (hasPrevPoint) prevPoint =  pt.m_controlPoints[1];
      }
      s << "Z";
      MWAWPictPath *res=new MWAWPictPath(box, s.str());
      pictPtr.reset(res);
    }
    break;
  }
  case CWGraphInternal::Zone::T_Zone:
  case CWGraphInternal::Zone::T_Basic:
  case CWGraphInternal::Zone::T_Picture:
  case CWGraphInternal::Zone::T_Chart:
  case CWGraphInternal::Zone::T_DataBox:
  case CWGraphInternal::Zone::T_Unknown:
  case CWGraphInternal::Zone::T_Bitmap:
  case CWGraphInternal::Zone::T_Pict:
  case CWGraphInternal::Zone::T_QTim:
  case CWGraphInternal::Zone::T_Movie:
  default:
    break;
  }

  if (!pictPtr)
    return false;
  CWGraphInternal::Style const &style = pict.m_style;
  pictPtr->setLineWidth((float)style.m_lineWidth);
  MWAWColor color;
  if (getLineColor(style, color))
    pictPtr->setLineColor(color);
  if (getSurfaceColor(style, color))
    pictPtr->setSurfaceColor(color);
  WPXBinaryData data;
  std::string type;

  if (!pictPtr->getBinary(data,type)) return false;

  pos.setOrigin(pos.origin()-Vec2f(2,2));
  pos.setSize(pos.size()+Vec2f(4,4));
  listener->insertPicture(pos,data, type, extras);
  return true;
}

bool CWGraph::sendBitmap(CWGraphInternal::ZoneBitmap &bitmap,
                         MWAWPosition pos, WPXPropertyList extras)
{
  if (!bitmap.m_entry.valid() || !bitmap.m_bitmapType)
    return false;

  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener)
    return true;
  int numColors = int(bitmap.m_colorMap.size());
  shared_ptr<MWAWPictBitmap> bmap;

  MWAWPictBitmapIndexed *bmapIndexed = 0;
  MWAWPictBitmapColor *bmapColor = 0;
  bool indexed = false;
  if (numColors > 2) {
    bmapIndexed =  new MWAWPictBitmapIndexed(bitmap.m_size);
    bmapIndexed->setColors(bitmap.m_colorMap);
    bmap.reset(bmapIndexed);
    indexed = true;
  } else
    bmap.reset((bmapColor=new MWAWPictBitmapColor(bitmap.m_size)));

  //! let go
  int fSz = bitmap.m_bitmapType;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(bitmap.m_entry.begin(), WPX_SEEK_SET);
  for (int r = 0; r < bitmap.m_size[1]; r++) {
    for (int c = 0; c < bitmap.m_size[0]; c++) {
      long val = (long) input->readULong(fSz);
      if (indexed) {
        bmapIndexed->set(c,r,(int)val);
        continue;
      }
      switch(fSz) {
      case 1:
        bmapColor->set(c,r, MWAWColor((unsigned char)val,(unsigned char)val,(unsigned char)val));
        break;
      case 2: // rgb compressed ?
        bmapColor->set(c,r, MWAWColor((unsigned char)(((val>>10)&0x1F) << 3),(unsigned char)(((val>>5)&0x1F) << 3),(unsigned char)(((val>>0)&0x1F) << 3)));
        break;
      case 4:
        bmapColor->set(c,r, MWAWColor(uint32_t(val)));
        break;
      default: {
        static bool first = true;
        if (first) {
          MWAW_DEBUG_MSG(("CWGraph::sendBitmap: unknown data size\n"));
          first = false;
        }
        break;
      }
      }
    }
  }

  WPXBinaryData data;
  std::string type;
  if (!bmap->getBinary(data,type)) return false;
  if (pos.size()[0] < 0 || pos.size()[1] < 0)
    pos.setSize(bitmap.m_box.size());
  listener->insertPicture(pos, data, "image/pict", extras);

  return true;
}

bool CWGraph::sendPicture(CWGraphInternal::ZonePict &pict,
                          MWAWPosition pos, WPXPropertyList extras)
{
  bool send = false;
  bool posOk = pos.size()[0] > 0 && pos.size()[1] > 0;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  for (int z = 0; z < 2; z++) {
    MWAWEntry entry = pict.m_entries[z];
    if (!entry.valid())
      continue;
    if (!posOk) pos.setSize(pict.m_box.size());
    input->seek(entry.begin(), WPX_SEEK_SET);

    switch(pict.getSubType()) {
    case CWGraphInternal::Zone::T_Movie:
    case CWGraphInternal::Zone::T_Pict: {
      shared_ptr<MWAWPict> thePict(MWAWPictData::get(input, (int)entry.length()));
      if (thePict) {
        if (!send && listener) {
          WPXBinaryData data;
          std::string type;
          if (thePict->getBinary(data,type))
            listener->insertPicture(pos, data, type, extras);
        }
        send = true;
      }
      break;
    }
    case CWGraphInternal::Zone::T_Line:
    case CWGraphInternal::Zone::T_Rect:
    case CWGraphInternal::Zone::T_RectOval:
    case CWGraphInternal::Zone::T_Oval:
    case CWGraphInternal::Zone::T_Arc:
    case CWGraphInternal::Zone::T_Poly:
    case CWGraphInternal::Zone::T_Zone:
    case CWGraphInternal::Zone::T_Basic:
    case CWGraphInternal::Zone::T_Picture:
    case CWGraphInternal::Zone::T_Chart:
    case CWGraphInternal::Zone::T_DataBox:
    case CWGraphInternal::Zone::T_Unknown:
    case CWGraphInternal::Zone::T_Bitmap:
    case CWGraphInternal::Zone::T_QTim:
    default:
      if (!send && listener) {
        WPXBinaryData data;
        input->seek(entry.begin(), WPX_SEEK_SET);
        input->readDataBlock(entry.length(), data);
        listener->insertPicture(pos, data, "image/pict", extras);
      }
      send = true;
      break;
    }

#ifdef DEBUG_WITH_FILES
    m_parserState->m_asciiFile.skipZone(entry.begin(), entry.end()-1);
    WPXBinaryData file;
    input->seek(entry.begin(), WPX_SEEK_SET);
    input->readDataBlock(entry.length(), file);
    static int volatile pictName = 0;
    libmwaw::DebugStream f;
    f << "PICT-" << ++pictName;
    libmwaw::Debug::dumpFile(file, f.str().c_str());
#endif
  }
  return send;
}

void CWGraph::flushExtra()
{
  std::map<int, shared_ptr<CWGraphInternal::Group> >::iterator iter
    = m_state->m_zoneMap.begin();
  for ( ; iter !=  m_state->m_zoneMap.end(); iter++) {
    shared_ptr<CWGraphInternal::Group> zone = iter->second;
    if (zone->m_parsed)
      continue;
    if (m_parserState->m_listener) m_parserState->m_listener->insertEOL();
    MWAWPosition pos(Vec2f(0,0),Vec2f(0,0),WPX_POINT);
    pos.setRelativePosition(MWAWPosition::Char);
    sendZone(iter->first, pos);
  }
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
