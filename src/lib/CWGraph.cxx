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

#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>

#include <libwpd/WPXString.h>

#include "TMWAWPictBasic.hxx"
#include "TMWAWPictMac.hxx"
#include "TMWAWPosition.hxx"

#include "MWAWStruct.hxx"
#include "MWAWTools.hxx"
#include "MWAWContentListener.hxx"

#include "CWGraph.hxx"

#include "CWParser.hxx"
#include "CWStruct.hxx"

/** Internal: the structures of a CWGraph */
namespace CWGraphInternal
{

struct Style {
  Style(): m_id(-1), m_lineFlags(0), m_lineWidth(1) {
    for (int i = 0; i < 2; i++) m_color[i] = m_pattern[i] = -1;
    for (int i = 0; i < 5; i++) m_flags[i] = 0;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Style const &st) {
    if (st.m_id >= 0) o << "id=" << st.m_id << ",";
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
    if (st.m_lineFlags & 0x40)
      o << "arrowBeg,";
    if (st.m_lineFlags & 0x80)
      o << "arrowEnd,";
    if (st.m_lineFlags & 0x3F)
      o << "lineFlags(?)=" << std::hex << int(st.m_lineFlags & 0x3F) << std::dec << ",";
    for (int i = 0; i < 5; i++) {
      if (st.m_flags[i])
        o << "fl" << i << "=" << std::hex << st.m_flags[i] << std::dec << ",";
    }
    return o;
  }
  //
  int m_id;
  //
  int m_lineFlags;
  //
  int m_lineWidth;
  // the line and surface color
  int m_color[2];
  // the line an surface id
  int m_pattern[2];

  int m_flags[5];
};

struct Zone {
  enum Type { T_Zone, T_Basic, T_Picture, T_Chart, T_DataBox, T_Unknown,
              /* basic subtype */
              T_Line, T_Rect, T_RectOval, T_Oval, T_Arc, T_Poly,
              /* picture subtype */
              T_Pict, T_QTim,
              /* bitmap type */
              T_Bitmap
            };

  Zone() : m_page(-1), m_box(), m_style() {}
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Zone const &zone) {
    if (zone.m_page >= 0) o << "pg=" << zone.m_page << ",";
    o << "box=" << zone.m_box << ",";
    zone.print(o);
    o << "style=[" << zone.m_style << "],";
    return o;
  }

  virtual ~Zone() {}
  virtual Type getType() const {
    return T_Unknown;
  }
  virtual Type getSubType() const {
    return T_Unknown;
  }
  virtual int getNumData() const {
    return 0;
  }
  virtual void print(std::ostream &) const { }
  virtual CWStruct::DSET::Child getChild() const {
    CWStruct::DSET::Child child;
    child.m_box = m_box;
    return child;
  }
  //! the page
  int m_page;
  //! the bdbox
  Box2i m_box;
  //! the style
  Style m_style;
};

struct ZoneBasic : public Zone {
  ZoneBasic(Zone const &z, Type type) : Zone(z), m_type(type), m_vertices() {
    for (int i = 0; i < 2; i++)
      m_values[i] = 0;
    for (int i = 0; i < 8; i++)
      m_flags[i] = 0;
  }

  virtual void print(std::ostream &o) const {
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
    default:
      o << "##type = " << m_type << ",";
      break;
    }
    if (m_vertices.size()) {
      o << "vertices=[";
      for (int i = 0; i < int(m_vertices.size()); i++)
        o << m_vertices[i] << ",";
      o << "],";
    }
    for (int i = 0; i < 8; i++)
      if (m_flags[i]) o << "fl" << i << "=" << m_flags[i] << ",";
  }
  virtual Type getType() const {
    return T_Basic;
  }
  virtual Type getSubType() const {
    return m_type;
  }
  virtual int getNumData() const {
    if (m_type == T_Poly) return 1;
    return 0;
  }
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
  std::vector<Vec2f> m_vertices;
};

struct ZonePict : public Zone {
  ZonePict(Zone const &z, Type type) : Zone(z), m_type(type) {
  }

  virtual void print(std::ostream &o) const {
    switch (m_type) {
    case T_Pict:
      o << "PICTURE,";
      break;
    case T_QTim:
      o << "QTIME,";
      break;
    default:
      o << "##type = " << m_type << ",";
      break;
    }
  }
  virtual Type getType() const {
    return T_Picture;
  }
  virtual Type getSubType() const {
    return m_type;
  }
  virtual int getNumData() const {
    return 2;
  }

  virtual CWStruct::DSET::Child getChild() const {
    CWStruct::DSET::Child child;
    child.m_box = m_box;
    child.m_type = CWStruct::DSET::Child::GRAPHIC;
    return child;
  }

  //! the sub type
  Type m_type;
  //! the picture entry followed by a ps entry ( if defined)
  IMWAWEntry m_entries[2];
};

struct ZoneBitmap : public Zone {
  ZoneBitmap() : m_bitmapType(-1), m_size(0,0), m_entry(), m_colorMap() {
  }

  virtual void print(std::ostream &o) const {
    o << "BITMAP:" << m_size << ",";
    if (m_bitmapType >= 0) o << "type=" << m_bitmapType << ",";
  }
  virtual Type getType() const {
    return T_Bitmap;
  }
  virtual Type getSubType() const {
    return  T_Bitmap;
  }

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
  IMWAWEntry m_entry;
  //! the color map
  std::vector<Vec3uc> m_colorMap;
};

struct ZoneZone : public Zone {
  ZoneZone(Zone const &z) : Zone(z), m_id(-1) {
    for (int i = 0; i < 9; i++)
      m_flags[i] = 0;
  }

  virtual void print(std::ostream &o) const {
    o << "ZONE, id=" << m_id << ",";
    for (int i = 0; i < 9; i++) {
      if (m_flags[i]) o << "fl" << i << "=" << m_flags[i] << ",";
    }
  }
  virtual Type getType() const {
    return T_Zone;
  }
  virtual Type getSubType() const {
    return T_Zone;
  }

  virtual CWStruct::DSET::Child getChild() const {
    CWStruct::DSET::Child child;
    child.m_box = m_box;
    child.m_id = m_id;
    child.m_type = CWStruct::DSET::Child::ZONE;
    return child;
  }

  //! the zoneId
  int m_id;
  //! flag
  int m_flags[9];
};

struct ZoneUnknown : public Zone {
  ZoneUnknown(Zone const &z) : Zone(z), m_type(T_Unknown), m_typeId(-1) {
  }

  virtual void print(std::ostream &o) const {
    switch(m_type) {
    case T_DataBox:
      o << "BOX(database),";
      break;
    case T_Chart:
      o << "CHART,";
      break;
    default:
      o << "##type=" << m_typeId << ",";
      break;
    }
  }
  virtual Type getType() const {
    return m_type;
  }
  virtual Type getSubType() const {
    return m_type;
  }
  virtual int getNumData() const {
    return m_type == T_Chart ? 2 : 0;
  }
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
////////////////////////////////////////

struct Group : public CWStruct::DSET {
  Group(CWStruct::DSET const dset = CWStruct::DSET()) :
    CWStruct::DSET(dset), m_zones(), m_parsed(false) {
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Group const &doc) {
    o << static_cast<CWStruct::DSET const &>(doc);
    return o;
  }


  std::vector<shared_ptr<Zone> > m_zones; // the data zone

  bool m_parsed;
};

////////////////////////////////////////
//! Internal: the state of a CWGraph
struct State {
  //! constructor
  State() : m_zoneMap() {
  }

  std::map<int, shared_ptr<Group> > m_zoneMap;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
CWGraph::CWGraph
(TMWAWInputStreamPtr ip, CWParser &parser, MWAWTools::ConvertissorPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert), m_state(new CWGraphInternal::State),
  m_mainParser(&parser), m_asciiFile(parser.ascii())
{
}

CWGraph::~CWGraph()
{ }

int CWGraph::version() const
{
  return m_mainParser->version();
}

// fixme
int CWGraph::numPages() const
{
  return 1;
}
////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// a group of data mainly graphic
////////////////////////////////////////////////////////////
shared_ptr<CWStruct::DSET> CWGraph::readGroupZone
(CWStruct::DSET const &zone, IMWAWEntry const &entry, bool &complete)
{
  complete = false;
  if (!entry.valid() || zone.m_type != 0)
    return shared_ptr<CWStruct::DSET>();
  long pos = entry.begin();
  m_input->seek(pos+8+16, WPX_SEEK_SET); // avoid header+8 generic number
  libmwaw_tools::DebugStream f;
  shared_ptr<CWGraphInternal::Group>
  graphicZone(new CWGraphInternal::Group(zone));

  f << "Entries(GroupDef):" << *graphicZone << ",";

  ascii().addDelimiter(m_input->tell(), '|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  // read the last part
  int data0Length = zone.m_dataSz;
  int N = zone.m_numData;
  if (entry.length() -8-12 != data0Length*N + zone.m_headerSz) {
    if (data0Length == 0 && N) {
      MWAW_DEBUG_MSG(("CWGraph::readGroupZone: can not find definition size\n"));
      m_input->seek(entry.end(), WPX_SEEK_SET);
      return shared_ptr<CWStruct::DSET>();
    }

    MWAW_DEBUG_MSG(("CWGraph::readGroupZone: unexpected size for zone definition, try to continue\n"));
  }

  long beginDefGroup = entry.end()-N*data0Length;
  if (long(m_input->tell())+42 <= beginDefGroup) {
    // an unknown zone simillar to a groupHead :-~
    ascii().addPos(beginDefGroup-42);
    ascii().addNote("GroupHead");
  }

  m_input->seek(beginDefGroup, WPX_SEEK_SET);

  graphicZone->m_childs.resize(N);
  for (int i = 0; i < N; i++) {
    pos = m_input->tell();
    IMWAWEntry gEntry;
    gEntry.setBegin(pos);
    gEntry.setLength(data0Length);
    shared_ptr<CWGraphInternal::Zone> def = readGroupDef(gEntry);
    graphicZone->m_zones.push_back(def);

    if (def)
      graphicZone->m_childs[i] = def->getChild();
    else {
      f.str("");
      f << "GroupDef#";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
    }
    m_input->seek(gEntry.end(), WPX_SEEK_SET);
  }

  m_input->seek(entry.end(), WPX_SEEK_SET);

  if (readGroupData(graphicZone)) {
    // fixme: do something here
  }

  if (m_state->m_zoneMap.find(graphicZone->m_id) != m_state->m_zoneMap.end()) {
    MWAW_DEBUG_MSG(("CWGraph::readGroupZone: zone %d already exists!!!\n", graphicZone->m_id));
  } else
    m_state->m_zoneMap[graphicZone->m_id] = graphicZone;

  return graphicZone;
}

////////////////////////////////////////////////////////////
// a group of data mainly graphic
////////////////////////////////////////////////////////////
shared_ptr<CWStruct::DSET> CWGraph::readBitmapZone
(CWStruct::DSET const &zone, IMWAWEntry const &entry, bool &complete)
{
  complete = false;
  if (!entry.valid() || zone.m_type != 4)
    return shared_ptr<CWStruct::DSET>();
  long pos = entry.begin();
  m_input->seek(pos+8+16, WPX_SEEK_SET); // avoid header+8 generic number
  libmwaw_tools::DebugStream f;
  shared_ptr<CWGraphInternal::Group>
  graphicZone(new CWGraphInternal::Group(zone));

  f << "Entries(BitmapDef):" << *graphicZone << ",";

  ascii().addDelimiter(m_input->tell(), '|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  // read the last part
  int data0Length = zone.m_dataSz;
  int N = zone.m_numData;
  if (entry.length() -8-12 != data0Length*N + zone.m_headerSz) {
    if (data0Length == 0 && N) {
      MWAW_DEBUG_MSG(("CWGraph::readBitmapZone: can not find definition size\n"));
      m_input->seek(entry.end(), WPX_SEEK_SET);
      return shared_ptr<CWStruct::DSET>();
    }

    MWAW_DEBUG_MSG(("CWGraph::readBitmapZone: unexpected size for zone definition, try to continue\n"));
  }

  /** the end of this block is very simillar to a bitmapdef, excepted
      maybe the first integer  .... */
  if (long(m_input->tell())+(N+1)*data0Length <= entry.end())
    N++;

  m_input->seek(entry.end()-N*data0Length, WPX_SEEK_SET);

  shared_ptr<CWGraphInternal::ZoneBitmap> bitmap
  (new CWGraphInternal::ZoneBitmap());
  for (int i = 0; i < N; i++) {
    pos = m_input->tell();
    IMWAWEntry gEntry;
    gEntry.setBegin(pos);
    gEntry.setLength(data0Length);
    f.str("");
    f << "BitmapDef-" << i << ":";
    long val = m_input->readULong(4);
    if (val) {
      if (i == 0)
        f << "unkn=" << val << ",";
      else
        f << "ptr=" << std::hex << val << std::dec << ",";
    }
    // f0 : 0 true color, if not number of bytes
    for (int j = 0; j < 3; j++) {
      val = m_input->readLong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }
    int dim[2]; // ( we must add 2 to add the border )
    for (int j = 0; j < 2; j++)
      dim[j] = m_input->readLong(2);
    if (i == N-1)
      bitmap->m_size = Vec2i(dim[0]+2, dim[1]+2);

    f << "dim?=" << dim[0] << "x" << dim[1] << ",";
    for (int j = 3; j < 6; j++) {
      val = m_input->readLong(2);
      if ((j != 5 && val!=1) || (j==5 && val)) // always 1, 1, 0
        f << "f" << j << "=" << val << ",";
    }
    if (long(m_input->tell()) != gEntry.end())
      ascii().addDelimiter(m_input->tell(), '|');
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(gEntry.end(), WPX_SEEK_SET);
  }

  m_input->seek(entry.end(), WPX_SEEK_SET);
  pos = entry.end();
  bool ok = readBitmapColorMap( bitmap->m_colorMap);
  if (ok) {
    pos = m_input->tell();
    ok = readBitmapData(*bitmap);
  }
  if (ok) {
    graphicZone->m_zones.resize(1);
    graphicZone->m_zones[0] = bitmap;
  } else
    m_input->seek(pos, WPX_SEEK_SET);

  // fixme: in general followed by another zone
  graphicZone->m_otherChilds.push_back(graphicZone->m_id+1);

  if (m_state->m_zoneMap.find(graphicZone->m_id) != m_state->m_zoneMap.end()) {
    MWAW_DEBUG_MSG(("CWGraph::readGroupZone: zone %d already exists!!!\n", graphicZone->m_id));
  } else
    m_state->m_zoneMap[graphicZone->m_id] = graphicZone;

  return graphicZone;
}

shared_ptr<CWGraphInternal::Zone> CWGraph::readGroupDef(IMWAWEntry const &entry)
{
  shared_ptr<CWGraphInternal::Zone> res;
  if (entry.length() < 32) {
    if (version() > 1 || entry.length() < 30) {
      MWAW_DEBUG_MSG(("CWGraph::readGroupDef: sz is too short!!!\n"));
      return res;
    }
  }
  long pos = entry.begin();
  m_input->seek(pos, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "GroupDef:";

  CWGraphInternal::Zone zone;
  CWGraphInternal::Style &style = zone.m_style;

  int typeId = m_input->readULong(1);
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
    case 18:
      type = CWGraphInternal::Zone::T_QTim;
      break;
    default:
      break;
    }
    break;
  }
  style.m_flags[0] = m_input->readULong(1);
  style.m_lineFlags = m_input->readULong(1);
  style.m_flags[1] = m_input->readULong(1);

  int dim[4];
  for (int j = 0; j < 4; j++) {
    int val = int(m_input->readLong(4)/256.);
    dim[j] = val;
    if (val < -100) f << "##dim?,";
  }
  zone.m_box = Box2i(Vec2i(dim[1], dim[0]), Vec2i(dim[3], dim[2]));
  style.m_lineWidth = m_input->readLong(1);
  style.m_flags[2] = m_input->readLong(1);
  for (int j = 0; j < 2; j++)
    style.m_color[j] = m_input->readULong(1);

  for (int j = 0; j < 2; j++) {
    style.m_flags[3+j] =  m_input->readULong(1); // probably also related to surface
    style.m_pattern[j] = m_input->readULong(1);
  }

  if (version() > 1)
    style.m_id = m_input->readLong(2);

  switch (type) {
  case CWGraphInternal::Zone::T_Zone: {
    CWGraphInternal::ZoneZone *z = new CWGraphInternal::ZoneZone(zone);
    res.reset(z);
    z->m_flags[0] = m_input->readLong(2);
    z->m_id = m_input->readULong(2);

    int numRemains = entry.end()-long(m_input->tell());
    numRemains/=2;
    if (numRemains > 8) numRemains = 8;
    for (int j = 0; j < numRemains; j++)
      z->m_flags[j+1] = m_input->readLong(2);
    break;
  }
  case CWGraphInternal::Zone::T_Pict:
  case CWGraphInternal::Zone::T_QTim:
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
  default: {
    CWGraphInternal::ZoneUnknown *z = new CWGraphInternal::ZoneUnknown(zone);
    res.reset(z);
    z->m_type = type;
    z->m_typeId = typeId;
    break;
  }
  }

  f << *res;

  long actPos = m_input->tell();
  if (actPos != entry.begin() && actPos != entry.end())
    ascii().addDelimiter(m_input->tell(),'|');
  m_input->seek(entry.end(), WPX_SEEK_SET);

  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  return res;
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool CWGraph::readGroupData(shared_ptr<CWGraphInternal::Group> zone)
{
  //  bool complete = false;
  if (!readGroupHeader()) {
    MWAW_DEBUG_MSG(("CWGraph::readGroupData: unexpected graphic1\n"));
    return false;
  }


  libmwaw_tools::DebugStream f;

  long pos, sz;
  int numChilds = zone->m_zones.size();
  int numError = 0;
  for (int i = 0; i < numChilds; i++) {
    shared_ptr<CWGraphInternal::Zone> z = zone->m_zones[i];
    int numZoneExpected = z ? z->getNumData() : 0;

    if (numZoneExpected) {
      pos = m_input->tell();
      sz = m_input->readULong(4);
      f.str("");
      if (sz == 0) {
        MWAW_DEBUG_MSG(("CWGraph::readGroupData: find a nop zone for type: %d\n",
                        z->getSubType()));
        ascii().addPos(pos);
        ascii().addNote("#Nop");
        if (numError++) {
          MWAW_DEBUG_MSG(("CWGraph::readGroupData: too many errors, zone parsing STOPS\n"));
          return false;
        }
        pos = m_input->tell();
        sz = m_input->readULong(4);
      }
      m_input->seek(pos, WPX_SEEK_SET);
      bool parsed = true;
      switch(z->getSubType()) {
      case CWGraphInternal::Zone::T_QTim:
        if (!readQTimeData(z))
          return false;
        break;
      case CWGraphInternal::Zone::T_Pict:
        if (!readPictData(z))
          return false;
        break;
      case CWGraphInternal::Zone::T_Poly:
        if (!readPolygonData(z))
          return false;
        break;
      default:
        parsed = false;
        break;
      }

      if (!parsed) {
        m_input->seek(pos+4+sz, WPX_SEEK_SET);
        if (long(m_input->tell()) != pos+4+sz) {
          m_input->seek(pos, WPX_SEEK_SET);
          MWAW_DEBUG_MSG(("CWGraph::readGroupData: find a odd zone for type: %d\n",
                          z->getSubType()));
          return false;
        }
        f.str("");
        if (z->getSubType() == CWGraphInternal::Zone::T_Chart)
          f << "Entries(ChartData)";
        else
          f << "Entries(UnknownDATA)-" << z->getSubType();
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());
        m_input->seek(pos+4+sz, WPX_SEEK_SET);
        if (numZoneExpected==2) {
          pos = m_input->tell();
          sz = m_input->readULong(4);
          if (sz) {
            m_input->seek(pos, WPX_SEEK_SET);
            MWAW_DEBUG_MSG(("CWGraph::readGroupData: two zones is not implemented for zone: %d\n",
                            z->getSubType()));
            return false;
          }
          ascii().addPos(pos);
          ascii().addNote("NOP");
        }
      }
    }
    if (version()==6) {
      pos = m_input->tell();
      sz = m_input->readULong(4);
      if (sz == 0) {
        ascii().addPos(pos);
        ascii().addNote("Nop");
        continue;
      }
      MWAW_DEBUG_MSG(("CWGraph::readGroupData: find not null entry for a end of zone: %d\n", z->getSubType()));
      ascii().addPos(pos);
      ascii().addNote("Entries(GroupDEnd)");
      m_input->seek(pos+4+sz, WPX_SEEK_SET);
    }
  }

  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////
bool CWGraph::readBasicGraphic(IMWAWEntry const &entry,
                               CWGraphInternal::ZoneBasic &zone)
{
  long actPos = m_input->tell();
  int remainBytes = entry.end()-actPos;
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
      zone.m_values[i] = m_input->readLong(2);
    break;
  }
  case CWGraphInternal::Zone::T_RectOval: {
    if (remainBytes < 8) {
      MWAW_DEBUG_MSG(("CWGraph::readSimpleGraphicZone: arc zone is too short\n"));
      return false;
    }
    for (int i = 0; i < 2; i++) {
      zone.m_values[i] = m_input->readLong(2)/2.0;
      zone.m_flags[actFlag++] = m_input->readULong(2);
    }
    break;
  }
  default:
    MWAW_DEBUG_MSG(("CWGraph::readSimpleGraphicZone: unknown type\n"));
    return false;
  }

  int numRemain = entry.end()-m_input->tell();
  numRemain /= 2;
  if (numRemain+actFlag > 8) numRemain = 8-actFlag;
  for (int i = 0; i < numRemain; i++)
    zone.m_flags[actFlag++] = m_input->readLong(2);

  return true;
}

bool CWGraph::readGroupHeader()
{
  long pos = m_input->tell();
  libmwaw_tools::DebugStream f;
  f << "Entries(GroupHead):";
  int sz = m_input->readULong(4);
  long endPos = pos+4+sz;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()!=endPos) || (sz && sz < 16)) {
    MWAW_DEBUG_MSG(("CWGraph::readGroupHeader: zone is too short\n"));
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  if (sz == 0) {
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return true;
  }

  m_input->seek(pos+4, WPX_SEEK_SET);
  int N = m_input->readULong(2);
  f << "N=" << N << ",";
  int type = m_input->readLong(2);
  if (type != -1)
    f << "#type=" << type << ",";
  int val = m_input->readLong(2);
  if (val) f << "#unkn=" << val << ",";
  int fSz = m_input->readULong(2);
  if (!fSz || N*fSz+12 != sz) {
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  for (int i = 0; i < 2; i++) { // always 0, 2
    val = m_input->readLong(2);
    if (val)
      f << "f" << i << "=" << val;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  m_input->seek(pos+4+12, WPX_SEEK_SET);
  for (int i = 0; i < N; i++) {
    pos = m_input->tell();
    f.str("");
    f << "GroupHead-" << i << ":";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+fSz, WPX_SEEK_SET);
  }

  // now try to read the graphic data
  for (int i = 0; i < N; i++) {
    pos = m_input->tell();
    sz = m_input->readULong(4);
    m_input->seek(pos+sz+4, WPX_SEEK_SET);
    if (long(m_input->tell())!= pos+sz+4) {
      m_input->seek(pos, WPX_SEEK_SET);
      MWAW_DEBUG_MSG(("CWGraph::readGroupHeader: can not find data for %d\n", i));
      return false;
    }
    f.str("");
    f << "GroupHead-" << i << "(Data):";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+sz+4, WPX_SEEK_SET);
  }

  // read the last block

  /* now we can read the last block between the data
     000000100002ffff0000000200000001001c0000
     000000100002ffff0000000200000001000e0000
     000000100002ffff000000020000000101dc01c1
     000000100002ffff00000002000000011d01f2ff
     000000100002ffff000000020000000102d801ef
     000000100002ffff00000002000000010046fff3
  */

  int numBlock = 1;//version()==6 ? N : 1;
  for (int i = 0; i < numBlock; i++) {
    pos= m_input->tell();
    int sz = m_input->readULong(4);
    m_input->seek(pos+4+sz, WPX_SEEK_SET);
    if (long(m_input->tell()) != pos+4+sz) {
      MWAW_DEBUG_MSG(("CWGraph::readGroupHeader: pb with last block\n"));
      m_input->seek(pos, WPX_SEEK_SET);
      return false;
    }

    f.str("");
    f << "GroupHead(End";
    if (i) f << "-" << i;
    f << ")";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+4+sz, WPX_SEEK_SET);
  }

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
  long pos = m_input->tell();
  long sz = m_input->readULong(4);
  long endPos = pos+4+sz;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos || sz < 12) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWGraph::readPolygonData: file is too short\n"));
    return false;
  }

  m_input->seek(pos+4, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "Entries(PolygonData):";
  int N = m_input->readULong(2);
  f << "N=" << N << ",";
  int val = m_input->readLong(2);
  if (val != -1) f << "f0=" << val << ",";
  val = m_input->readLong(2);
  if (val) f << "f1=" << val << ",";
  int fSz = m_input->readLong(2);
  if (sz != 12+fSz*N) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWGraph::readPolygonData: find odd data size\n"));
    return false;
  }
  for (int i = 2; i < 4; i++) {
    val = m_input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";

  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = m_input->tell();
    f.str("");
    f << "PolygonData-" << i << ":";
    float position[2];
    for (int j = 0; j < 2; j++)
      position[j] = m_input->readLong(4)/256.;
    bZone->m_vertices.push_back(Vec2f(position[0], position[1]));
    f << position[0] << "x" << position[1] << ",";
    if (fSz >= 26) {
      for (int cPt = 0; cPt < 2; cPt++) {
        float ctrlPos[2];
        for (int j = 0; j < 2; j++)
          ctrlPos[j] = m_input->readLong(4)/256.;
        if (position[0] != ctrlPos[0] || position[1] != ctrlPos[1])
          f << "ctrPt" << cPt << "=" << ctrlPos[0] << "x" << ctrlPos[1] << ",";
      }
      int fl = m_input->readULong(2);
      switch (fl>>14) {
      case 1:
        break;
      case 2:
        f << "spline,";
        break; // or bezier ?
      default:
        f << "#type=" << int(fl>>14) << ",";
        break;
      }
      if (fl&0x3FFF)
        f << "unkn=" << std::hex << int(fl&0x3FFF) << std::dec << ",";
    }

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());

    m_input->seek(pos+fSz, WPX_SEEK_SET);
  }
  m_input->seek(endPos, WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// read some picture
////////////////////////////////////////////////////////////
bool CWGraph::readPictData(shared_ptr<CWGraphInternal::Zone> zone)
{
  if (!zone || zone->getSubType() != CWGraphInternal::Zone::T_Pict)
    return false;
  CWGraphInternal::ZonePict *pZone =
    reinterpret_cast<CWGraphInternal::ZonePict *>(zone.get());
  long pos = m_input->tell();
  if (!readPICT(*pZone)) {
    MWAW_DEBUG_MSG(("CWGraph::readPictData: find a odd pict\n"));
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  pos = m_input->tell();
  long sz = m_input->readULong(4);
  m_input->seek(pos+4+sz, WPX_SEEK_SET);
  if (long(m_input->tell()) != pos+4+sz) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWGraph::readPictData: find a end zone for graphic\n"));
    return false;
  }
  if (sz == 0) {
    ascii().addPos(pos);
    ascii().addNote("Nop");
    return true;
  }

  m_input->seek(pos, WPX_SEEK_SET);
  if (!readPS(*pZone)) {
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  return true;
}

bool CWGraph::readPICT(CWGraphInternal::ZonePict &zone)
{
  long pos = m_input->tell();
  long sz = m_input->readULong(4);
  long endPos = pos+4+sz;
  if (sz < 12) {
    MWAW_DEBUG_MSG(("CWGraph::readPict: file is too short\n"));
    return false;
  }

  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("CWGraph::readPict: file is too short\n"));
    return false;
  }
  libmwaw_tools::DebugStream f;
  f << "Entries(Graphic):";

  Box2f box;
  m_input->seek(pos+4, WPX_SEEK_SET);

  libmwaw_tools::Pict::ReadResult res =
    libmwaw_tools::PictData::check(m_input, sz, box);
  if (res == libmwaw_tools::Pict::MWAW_R_BAD) {
    MWAW_DEBUG_MSG(("CWGraph::readPict: can not find the picture\n"));
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  zone.m_entries[0].setBegin(pos+4);
  zone.m_entries[0].setEnd(endPos);
#ifdef DEBUG_WITH_FILES
  if (1) {
    WPXBinaryData file;
    m_input->seek(pos+4, WPX_SEEK_SET);
    m_input->readDataBlock(sz, file);
    static int volatile pictName = 0;
    libmwaw_tools::DebugStream f;
    f << "PICT-" << ++pictName << ".pct";
    libmwaw_tools::Debug::dumpFile(file, f.str().c_str());
  }
#endif

  // get the picture
  ascii().skipZone(pos+4, endPos-1);
  m_input->seek(endPos, WPX_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  return true;
}

bool CWGraph::readPS(CWGraphInternal::ZonePict &zone)
{
  long pos = m_input->tell();
  long sz = m_input->readULong(4);
  long header = m_input->readULong(4);
  if (header != 0x25215053L) {
    MWAW_DEBUG_MSG(("CWGraph::readNamedPict: not a postcript file\n"));
    return false;
  }
  long endPos = pos+4+sz;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("CWGraph::readNamedPict: file is too short\n"));
    return false;
  }
  zone.m_entries[1].setBegin(pos+4);
  zone.m_entries[1].setEnd(endPos);
#ifdef DEBUG_WITH_FILES
  if (1) {
    WPXBinaryData file;
    m_input->seek(pos+4, WPX_SEEK_SET);
    m_input->readDataBlock(sz, file);
    static int volatile pictName = 0;
    libmwaw_tools::DebugStream f;
    f << "PostScript-" << ++pictName << ".ps";
    libmwaw_tools::Debug::dumpFile(file, f.str().c_str());
  }
#endif

  libmwaw_tools::DebugStream f;
  f << "Entries(PostScript):";
  m_input->seek(endPos, WPX_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  ascii().skipZone(pos+4, endPos-1);

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
  long pos = m_input->tell();
  long header = m_input->readULong(4);
  if (header != 0x5154494dL) {
    MWAW_DEBUG_MSG(("CWGraph::readQTimeData: find a odd qtim zone\n"));
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  libmwaw_tools::DebugStream f;
  f << "Entries(QTIM):";
  for (int i = 0; i < 2; i++) f << "f" << i << "=" << m_input->readULong(2) << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos = m_input->tell();
  if (!readNamedPict(*pZone)) {
    MWAW_DEBUG_MSG(("CWGraph::readQTimeData: find a odd named pict\n"));
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  return true;
}


bool CWGraph::readNamedPict(CWGraphInternal::ZonePict &zone)
{
  long pos = m_input->tell();
  std::string name("");
  for (int i = 0; i < 4; i++) {
    char c = m_input->readULong(1);
    if (c < ' ' || c > 'z') {
      MWAW_DEBUG_MSG(("CWGraph::readNamedPict: can not find the name\n"));
      return false;
    }
    name+=c;
  }
  long sz = m_input->readULong(4);
  long endPos = pos+8+sz;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos || !sz) {
    MWAW_DEBUG_MSG(("CWGraph::readNamedPict: file is too short\n"));
    return false;
  }

  zone.m_entries[0].setBegin(pos+8);
  zone.m_entries[0].setEnd(endPos);

#ifdef DEBUG_WITH_FILES
  if (1) {
    WPXBinaryData file;
    m_input->seek(pos+8, WPX_SEEK_SET);
    m_input->readDataBlock(sz, file);
    static int volatile pictName = 0;
    libmwaw_tools::DebugStream f;
    f << "PICT2-" << ++pictName << "." << name;
    libmwaw_tools::Debug::dumpFile(file, f.str().c_str());
  }
#endif

  libmwaw_tools::DebugStream f;
  f << "Entries(" << name << "):";
  m_input->seek(endPos, WPX_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  ascii().skipZone(pos+8, endPos-1);

  return true;
}

////////////////////////////////////////////////////////////
// read bitmap picture
////////////////////////////////////////////////////////////
bool CWGraph::readBitmapColorMap(std::vector<Vec3uc> &cMap)
{
  cMap.resize(0);
  long pos = m_input->tell();
  long sz = m_input->readULong(4);
  long endPos = pos+4+sz;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos || !sz) {
    MWAW_DEBUG_MSG(("CWGraph::readBitmapColorMap: file is too short\n"));
    return false;
  }

  m_input->seek(pos+4, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "Entries(BitmapColor):";
  f << "unkn=" << m_input->readLong(4) << ",";
  int maxColor = m_input->readLong(4);
  if (sz != 8+8*(maxColor+1)) {
    MWAW_DEBUG_MSG(("CWGraph::readBitmapColorMap: sz is odd\n"));
    return false;
  }
  cMap.resize(maxColor+1);
  for (int i = 0; i <= maxColor; i++) {
    int id = m_input->readULong(2);
    if (id != i) {
      MWAW_DEBUG_MSG(("CWGraph::readBitmapColorMap: find odd index : %d\n", i));
      return false;
    }
    int col[3];
    for (int c = 0; c < 3; c++) col[c] = (m_input->readULong(2)>>8);
    cMap[i] = Vec3uc(col[0], col[1], col[2]);
  }

  m_input->seek(endPos, WPX_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  return true;
}

bool CWGraph::readBitmapData(CWGraphInternal::ZoneBitmap &zone)
{
  long pos = m_input->tell();
  long sz = m_input->readULong(4);
  long endPos = pos+4+sz;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos || !sz) {
    MWAW_DEBUG_MSG(("CWGraph::readBitmapColorMap: file is too short\n"));
    return false;
  }
  long numColors = zone.m_size[0]*zone.m_size[1];
  int numBytes = numColors ? sz/numColors : 0;
  if (sz != numBytes*numColors) {
    MWAW_DEBUG_MSG(("CWGraph::readBitmapColorMap: unexpected size\n"));
    return false;
  }
  zone.m_bitmapType = numBytes;
  zone.m_entry.setBegin(pos+4);
  zone.m_entry.setEnd(endPos);
  libmwaw_tools::DebugStream f;
  f << "Entries(BitmapData):nBytes=" << numBytes;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  ascii().skipZone(pos+4, endPos-1);

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
