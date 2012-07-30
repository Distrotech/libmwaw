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
#include <sstream>

#include <libwpd/WPXString.h>

#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWPictBasic.hxx"
#include "MWAWPictBitmap.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"

#include "CWParser.hxx"
#include "CWStruct.hxx"

#include "CWGraph.hxx"

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
              T_Pict, T_QTim, T_Movie,
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
      o << "##type = " << m_type << ",";
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
  MWAWEntry m_entries[2];
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
  MWAWEntry m_entry;
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
  State() : m_zoneMap(), m_colorMap() { }
  // set the default color map
  void setDefaultColorMap(int version);

  std::map<int, shared_ptr<Group> > m_zoneMap;
  std::vector<Vec3uc> m_colorMap;
};

void State::setDefaultColorMap(int version)
{
  if (version == 1 || m_colorMap.size()) return;
  int const defCol[256] = {
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
  m_colorMap.resize(256);
  for (size_t i = 0; i < 256; i++)
    m_colorMap[i] = Vec3uc((unsigned char)((defCol[i]>>16)&0xff), (unsigned char)((defCol[i]>>8)&0xff),(unsigned char)(defCol[i]&0xff));
}

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
CWGraph::CWGraph
(MWAWInputStreamPtr ip, CWParser &parser, MWAWFontConverterPtr &convert) :
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
bool CWGraph::getColor(int id, Vec3uc &col) const
{
  int numColor = (int) m_state->m_colorMap.size();
  if (!numColor) {
    m_state->setDefaultColorMap(version());
    numColor = int(m_state->m_colorMap.size());
  }
  if (id < 0 || id >= numColor)
    return false;
  col = m_state->m_colorMap[size_t(id)];
  return true;
}

////////////////////////////////////////////////////////////
// a group of data mainly graphic
////////////////////////////////////////////////////////////
shared_ptr<CWStruct::DSET> CWGraph::readGroupZone
(CWStruct::DSET const &zone, MWAWEntry const &entry, bool &complete)
{
  complete = false;
  if (!entry.valid() || zone.m_type != 0)
    return shared_ptr<CWStruct::DSET>();
  long pos = entry.begin();
  m_input->seek(pos+8+16, WPX_SEEK_SET); // avoid header+8 generic number
  libmwaw::DebugStream f;
  shared_ptr<CWGraphInternal::Group>
  graphicZone(new CWGraphInternal::Group(zone));

  f << "Entries(GroupDef):" << *graphicZone << ",";

  ascii().addDelimiter(m_input->tell(), '|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  // read the last part
  long data0Length = zone.m_dataSz;
  long N = zone.m_numData;
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

  graphicZone->m_childs.resize(size_t(N));
  for (int i = 0; i < N; i++) {
    pos = m_input->tell();
    MWAWEntry gEntry;
    gEntry.setBegin(pos);
    gEntry.setLength(data0Length);
    shared_ptr<CWGraphInternal::Zone> def = readGroupDef(gEntry);
    graphicZone->m_zones.push_back(def);

    if (def)
      graphicZone->m_childs[size_t(i)] = def->getChild();
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
bool CWGraph::readColorMap(MWAWEntry const &entry)
{
  if (!entry.valid()) return false;
  long pos = entry.begin();
  m_input->seek(pos+4, WPX_SEEK_SET); // avoid header
  if (entry.length() == 4) return true;

  libmwaw::DebugStream f;
  f << "Entries(ColorMap):";
  int N = (int) m_input->readULong(2);
  f << "N=" << N << ",";
  int val;
  for(int i = 0; i < 2; i++) {
    val = (int) m_input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }

  int const fSz = 16;
  if (pos+10+N*fSz > entry.end()) {
    MWAW_DEBUG_MSG(("CWGraph::readColorMap: can not read data\n"));
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  ascii().addDelimiter(m_input->tell(),'|');
  m_input->seek(entry.end()-N*fSz, WPX_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  m_state->m_colorMap.resize(size_t(N));
  for (int i = 0; i < N; i++) {
    pos = m_input->tell();
    unsigned char color[3];
    for (int c=0; c < 3; c++) color[c] = (unsigned char) (m_input->readULong(2)/256);
    m_state->m_colorMap[size_t(i)]= Vec3uc(color[0], color[1],color[2]);

    f.str("");
    f << "ColorMap[" << i << "]:";
    ascii().addDelimiter(m_input->tell(),'|');
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+fSz, WPX_SEEK_SET);
  }

  m_input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

shared_ptr<CWStruct::DSET> CWGraph::readBitmapZone
(CWStruct::DSET const &zone, MWAWEntry const &entry, bool &complete)
{
  complete = false;
  if (!entry.valid() || zone.m_type != 4)
    return shared_ptr<CWStruct::DSET>();
  long pos = entry.begin();
  m_input->seek(pos+8+16, WPX_SEEK_SET); // avoid header+8 generic number
  libmwaw::DebugStream f;
  shared_ptr<CWGraphInternal::Group>
  graphicZone(new CWGraphInternal::Group(zone));

  f << "Entries(BitmapDef):" << *graphicZone << ",";

  ascii().addDelimiter(m_input->tell(), '|');

  // read the last part
  long data0Length = zone.m_dataSz;
  long N = zone.m_numData;
  if (entry.length() -8-12 != data0Length*N + zone.m_headerSz) {
    if (data0Length == 0 && N) {
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());

      MWAW_DEBUG_MSG(("CWGraph::readBitmapZone: can not find definition size\n"));
      m_input->seek(entry.end(), WPX_SEEK_SET);
      return shared_ptr<CWStruct::DSET>();
    }

    MWAW_DEBUG_MSG(("CWGraph::readBitmapZone: unexpected size for zone definition, try to continue\n"));
  }

  shared_ptr<CWGraphInternal::ZoneBitmap> bitmap(new CWGraphInternal::ZoneBitmap());

  bool sizeSet=false;
  int sizePos = (version() == 1) ? 0: 88;
  if (sizePos && pos+sizePos+4+N*data0Length < entry.end()) {
    m_input->seek(pos+sizePos, WPX_SEEK_SET);
    ascii().addDelimiter(pos+sizePos,'[');
    int dim[2]; // ( we must add 2 to add the border )
    for (int j = 0; j < 2; j++)
      dim[j] = (int) m_input->readLong(2);
    f << "sz=" << dim[1] << "x" << dim[0] << ",";
    if (dim[0] > 0 && dim[1] > 0) {
      bitmap->m_size = Vec2i(dim[1]+2, dim[0]+2);
      sizeSet = true;
    }
    ascii().addDelimiter(m_input->tell(),']');
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  /** the end of this block is very simillar to a bitmapdef, excepted
      maybe the first integer  .... */
  if (long(m_input->tell())+(N+1)*data0Length <= entry.end())
    N++;

  m_input->seek(entry.end()-N*data0Length, WPX_SEEK_SET);

  for (int i = 0; i < N; i++) {
    pos = m_input->tell();
    MWAWEntry gEntry;
    gEntry.setBegin(pos);
    gEntry.setLength(data0Length);
    f.str("");
    f << "BitmapDef-" << i << ":";
    long val = (long) m_input->readULong(4);
    if (val) {
      if (i == 0)
        f << "unkn=" << val << ",";
      else
        f << "ptr=" << std::hex << val << std::dec << ",";
    }
    // f0 : 0 true color, if not number of bytes
    for (int j = 0; j < 3; j++) {
      val = (int) m_input->readLong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }
    int dim[2]; // ( we must add 2 to add the border )
    for (int j = 0; j < 2; j++)
      dim[j] = (int) m_input->readLong(2);
    if (i == N-1 && !sizeSet)
      bitmap->m_size = Vec2i(dim[0]+2, dim[1]+2);

    f << "dim?=" << dim[0] << "x" << dim[1] << ",";
    for (int j = 3; j < 6; j++) {
      val = (int) m_input->readLong(2);
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
  m_input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "GroupDef:";

  CWGraphInternal::Zone zone;
  CWGraphInternal::Style &style = zone.m_style;

  int typeId = (int) m_input->readULong(1);
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
  style.m_flags[0] = (int) m_input->readULong(1);
  style.m_lineFlags = (int) m_input->readULong(1);
  style.m_flags[1] = (int) m_input->readULong(1);

  int dim[4];
  for (int j = 0; j < 4; j++) {
    int val = int(m_input->readLong(4)/256);
    dim[j] = val;
    if (val < -100) f << "##dim?,";
  }
  zone.m_box = Box2i(Vec2i(dim[1], dim[0]), Vec2i(dim[3], dim[2]));
  style.m_lineWidth = (int) m_input->readLong(1);
  style.m_flags[2] = (int) m_input->readLong(1);
  for (int j = 0; j < 2; j++)
    style.m_color[j] = (int) m_input->readULong(1);

  for (int j = 0; j < 2; j++) {
    style.m_flags[3+j] =  (int) m_input->readULong(1); // probably also related to surface
    style.m_pattern[j] = (int) m_input->readULong(1);
  }

  if (version() > 1)
    style.m_id = (int) m_input->readLong(2);

  switch (type) {
  case CWGraphInternal::Zone::T_Zone: {
    CWGraphInternal::ZoneZone *z = new CWGraphInternal::ZoneZone(zone);
    res.reset(z);
    z->m_flags[0] = (int) m_input->readLong(2);
    z->m_id = (int) m_input->readULong(2);

    int numRemains = int(entry.end()-long(m_input->tell()));
    numRemains/=2;
    if (numRemains > 8) numRemains = 8;
    for (int j = 0; j < numRemains; j++)
      z->m_flags[j+1] = (int) m_input->readLong(2);
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


  libmwaw::DebugStream f;

  long pos, sz;
  size_t numChilds = zone->m_zones.size();
  int numError = 0;
  for (size_t i = 0; i < numChilds; i++) {
    shared_ptr<CWGraphInternal::Zone> z = zone->m_zones[i];
    int numZoneExpected = z ? z->getNumData() : 0;

    if (numZoneExpected) {
      pos = m_input->tell();
      sz = (long) m_input->readULong(4);
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
        sz = (long) m_input->readULong(4);
      }
      m_input->seek(pos, WPX_SEEK_SET);
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
          sz = (long) m_input->readULong(4);
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
      sz = (long) m_input->readULong(4);
      if (sz == 0) {
        ascii().addPos(pos);
        ascii().addNote("Nop");
        continue;
      }
      MWAW_DEBUG_MSG(("CWGraph::readGroupData: find not null entry for a end of zone: %d\n", z->getSubType()));
      m_input->seek(pos, WPX_SEEK_SET);
    }
  }

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
  long actPos = m_input->tell();
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
      zone.m_values[i] = (float) m_input->readLong(2);
    break;
  }
  case CWGraphInternal::Zone::T_RectOval: {
    if (remainBytes < 8) {
      MWAW_DEBUG_MSG(("CWGraph::readSimpleGraphicZone: arc zone is too short\n"));
      return false;
    }
    for (int i = 0; i < 2; i++) {
      zone.m_values[size_t(i)] = float(m_input->readLong(2))/2.0f;
      zone.m_flags[actFlag++] = (int) m_input->readULong(2);
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

  int numRemain = int(entry.end()-m_input->tell());
  numRemain /= 2;
  if (numRemain+actFlag > 8) numRemain = 8-actFlag;
  for (int i = 0; i < numRemain; i++)
    zone.m_flags[actFlag++] = (int) m_input->readLong(2);

  return true;
}

bool CWGraph::readGroupHeader()
{
  long pos = m_input->tell();
  libmwaw::DebugStream f;
  f << "Entries(GroupHead):";
  long sz = (long) m_input->readULong(4);
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
  int N = (int) m_input->readULong(2);
  f << "N=" << N << ",";
  int type = (int) m_input->readLong(2);
  if (type != -1)
    f << "#type=" << type << ",";
  int val = (int) m_input->readLong(2);
  if (val) f << "#unkn=" << val << ",";
  int fSz = (int) m_input->readULong(2);
  if (!fSz || N *fSz+12 != sz) {
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  for (int i = 0; i < 2; i++) { // always 0, 2
    val = (int) m_input->readLong(2);
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
    sz = (long) m_input->readULong(4);
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
    sz = (long) m_input->readULong(4);
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
  long sz = (long) m_input->readULong(4);
  long endPos = pos+4+sz;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos || sz < 12) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWGraph::readPolygonData: file is too short\n"));
    return false;
  }

  m_input->seek(pos+4, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(PolygonData):";
  int N = (int) m_input->readULong(2);
  f << "N=" << N << ",";
  int val = (int) m_input->readLong(2);
  if (val != -1) f << "f0=" << val << ",";
  val = (int) m_input->readLong(2);
  if (val) f << "f1=" << val << ",";
  int fSz = (int) m_input->readLong(2);
  if (sz != 12+fSz*N) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWGraph::readPolygonData: find odd data size\n"));
    return false;
  }
  for (int i = 2; i < 4; i++) {
    val = (int) m_input->readLong(2);
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
      position[j] = float(m_input->readLong(4))/256.f;
    bZone->m_vertices.push_back(Vec2f(position[1], position[0]));
    f << position[1] << "x" << position[0] << ",";
    if (fSz >= 26) {
      for (int cPt = 0; cPt < 2; cPt++) {
        float ctrlPos[2];
        for (int j = 0; j < 2; j++)
          ctrlPos[j] = float(m_input->readLong(4))/256.f;
        if (position[0] < ctrlPos[0] || position[0] > ctrlPos[0] ||
            position[1] < ctrlPos[1] || position[1] > ctrlPos[1])
          f << "ctrPt" << cPt << "=" << ctrlPos[1] << "x" << ctrlPos[0] << ",";
      }
      int fl = (int) m_input->readULong(2);
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
  if (!zone || (zone->getSubType() != CWGraphInternal::Zone::T_Pict &&
                zone->getSubType() != CWGraphInternal::Zone::T_Movie))
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
  long sz = (long) m_input->readULong(4);
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
  if (readPS(*pZone))
    return true;

  // find some msword document (with some embedding: Ole entry ?)
  MWAW_DEBUG_MSG(("CWGraph::readPictData: unknown data file\n"));
#ifdef DEBUG_WITH_FILES
  if (1) {
    WPXBinaryData file;
    m_input->seek(pos+4, WPX_SEEK_SET);
    m_input->readDataBlock(sz, file);
    static int volatile pictName = 0;
    libmwaw::DebugStream f;
    f << "DATA-" << ++pictName;
    libmwaw::Debug::dumpFile(file, f.str().c_str());
  }
#endif
  ascii().addPos(pos);
  if (zone->getSubType() == CWGraphInternal::Zone::T_Movie)
    ascii().addNote("Entries(MovieData2)");
  else
    ascii().addNote("Entries(PictData2)");
  ascii().skipZone(pos+4, pos+4+sz-1);

  m_input->seek(pos+4+sz, WPX_SEEK_SET);
  return true;
}

bool CWGraph::readPICT(CWGraphInternal::ZonePict &zone)
{
  long pos = m_input->tell();
  long sz = (long) m_input->readULong(4);
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
  libmwaw::DebugStream f;
  f << "Entries(Graphic):";

  Box2f box;
  m_input->seek(pos+4, WPX_SEEK_SET);

  MWAWPict::ReadResult res = MWAWPictData::check(m_input, (int)sz, box);
  if (res == MWAWPict::MWAW_R_BAD) {
    MWAW_DEBUG_MSG(("CWGraph::readPict: can not find the picture\n"));
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  zone.m_entries[0].setBegin(pos+4);
  zone.m_entries[0].setEnd(endPos);
  m_input->seek(endPos, WPX_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  return true;
}

bool CWGraph::readPS(CWGraphInternal::ZonePict &zone)
{
  long pos = m_input->tell();
  long sz = (long) m_input->readULong(4);
  long header = (long) m_input->readULong(4);
  if (header != 0x25215053L) {
    return false;
  }
  long endPos = pos+4+sz;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
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
    libmwaw::DebugStream f;
    f << "PostScript-" << ++pictName << ".ps";
    libmwaw::Debug::dumpFile(file, f.str().c_str());
  }
#endif

  libmwaw::DebugStream f;
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
  long header = (long) m_input->readULong(4);
  if (header != 0x5154494dL) {
    MWAW_DEBUG_MSG(("CWGraph::readQTimeData: find a odd qtim zone\n"));
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  libmwaw::DebugStream f;
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
    char c = (char) m_input->readULong(1);
    if (c < ' ' || c > 'z') {
      MWAW_DEBUG_MSG(("CWGraph::readNamedPict: can not find the name\n"));
      return false;
    }
    name+=c;
  }
  long sz = (long) m_input->readULong(4);
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
    libmwaw::DebugStream f;
    f << "PICT2-" << ++pictName << "." << name;
    libmwaw::Debug::dumpFile(file, f.str().c_str());
  }
#endif

  libmwaw::DebugStream f;
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
  long sz = (long) m_input->readULong(4);
  long endPos = pos+4+sz;
  if (!sz) {
    ascii().addPos(pos);
    ascii().addNote("Nop");
    return true;
  }
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("CWGraph::readBitmapColorMap: file is too short\n"));
    return false;
  }

  m_input->seek(pos+4, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(BitmapColor):";
  f << "unkn=" << m_input->readLong(4) << ",";
  int maxColor = (int) m_input->readLong(4);
  if (sz != 8+8*(maxColor+1)) {
    MWAW_DEBUG_MSG(("CWGraph::readBitmapColorMap: sz is odd\n"));
    return false;
  }
  cMap.resize(size_t(maxColor+1));
  for (int i = 0; i <= maxColor; i++) {
    int id = (int) m_input->readULong(2);
    if (id != i) {
      MWAW_DEBUG_MSG(("CWGraph::readBitmapColorMap: find odd index : %d\n", i));
      return false;
    }
    unsigned char col[3];
    for (int c = 0; c < 3; c++) col[c] = (unsigned char)(m_input->readULong(2)>>8);
    cMap[(size_t)i] = Vec3uc(col[0], col[1], col[2]);
  }

  m_input->seek(endPos, WPX_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  return true;
}

bool CWGraph::readBitmapData(CWGraphInternal::ZoneBitmap &zone)
{
  long pos = m_input->tell();
  long sz = (long) m_input->readULong(4);
  long endPos = pos+4+sz;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos || !sz) {
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
  libmwaw::DebugStream f;
  f << "Entries(BitmapData):nBytes=" << numBytes;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  ascii().skipZone(pos+4, endPos-1);

  return true;
}

////////////////////////////////////////////////////////////
// send data to the listener
////////////////////////////////////////////////////////////
bool CWGraph::sendZone(int number)
{
  std::map<int, shared_ptr<CWGraphInternal::Group> >::iterator iter
    = m_state->m_zoneMap.find(number);
  if (iter == m_state->m_zoneMap.end())
    return false;
  shared_ptr<CWGraphInternal::Group> group = iter->second;
  for (size_t g = 0; g < group->m_zones.size(); g++) {
    switch (group->m_zones[g]->getType()) {
    case CWGraphInternal::Zone::T_Zone:
      // fixme
      break;
    case CWGraphInternal::Zone::T_Picture:
      sendPicture
      (reinterpret_cast<CWGraphInternal::ZonePict &>(*group->m_zones[g]));
      break;
    case CWGraphInternal::Zone::T_Basic:
#ifdef DEBUG
      sendBasicPicture
      (reinterpret_cast<CWGraphInternal::ZoneBasic &>(*group->m_zones[g]));
#endif
      break;
    case CWGraphInternal::Zone::T_Bitmap:
      sendBitmap
      (reinterpret_cast<CWGraphInternal::ZoneBitmap &>(*group->m_zones[g]));
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
  group->m_parsed = true;
  return true;
}

bool CWGraph::sendBasicPicture(CWGraphInternal::ZoneBasic &pict)
{
  if (!m_listener) return true;

  Vec2i pictSz = pict.m_box.size();
  if (pictSz[0] < 0) pictSz.setX(-pictSz[0]);
  if (pictSz[1] < 0) pictSz.setY(-pictSz[1]);
  Box2i box(Vec2i(0,0), pictSz);

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
    if (!pict.m_vertices.size()) break;
    // if (pict.m_style.m_lineFlags & 1) : we must close the polygon ?
    MWAWPictPolygon *res=new MWAWPictPolygon(box, pict.m_vertices);
    pictPtr.reset(res);
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
  pictPtr->setLineWidth((float)pict.m_style.m_lineWidth);
  Vec3uc color;
  if (getColor(pict.m_style.m_color[0], color)) pictPtr->setLineColor(color[0], color[1], color[2]);
  if (getColor(pict.m_style.m_color[1], color)) pictPtr->setSurfaceColor(color[0], color[1], color[2]);
  WPXBinaryData data;
  std::string type;
  if (!pictPtr->getBinary(data,type)) return false;
  Box2f pictBox= pictPtr->getBdBox();
  pictBox.extend(4.0);
  MWAWPosition pictPos=MWAWPosition(pictBox[0],pictBox.size(), WPX_POINT);
  pictPos.setRelativePosition(MWAWPosition::Char);


  m_listener->insertPicture(pictPos,data, type);
  return true;
}

bool CWGraph::sendBitmap(CWGraphInternal::ZoneBitmap &bitmap)
{
  if (!bitmap.m_entry.valid() || !bitmap.m_bitmapType)
    return false;

  if (!m_listener)
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
  m_input->seek(bitmap.m_entry.begin(), WPX_SEEK_SET);
  for (int r = 0; r < bitmap.m_size[1]; r++) {
    for (int c = 0; c < bitmap.m_size[0]; c++) {
      long val = (long) m_input->readULong(fSz);
      if (indexed) {
        bmapIndexed->set(c,r,(int)val);
        continue;
      }
      switch(fSz) {
      case 1:
        bmapColor->set(c,r, Vec3uc((unsigned char)val,(unsigned char)val,(unsigned char)val));
        break;
      case 2: // rgb compressed ?
        bmapColor->set(c,r, Vec3uc((unsigned char)(((val>>10)&0x1F) << 3),(unsigned char)(((val>>5)&0x1F) << 3),(unsigned char)(((val>>0)&0x1F) << 3)));
        break;
      case 4:
        bmapColor->set(c,r, Vec3uc((unsigned char)((val>>16)&0xff),(unsigned char)((val>>8)&0xff),(unsigned char)((val>>0)&0xff)));
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

  MWAWPosition pictPos=MWAWPosition(Vec2f(0,0),bitmap.m_box.size(), WPX_POINT);
  pictPos.setRelativePosition(MWAWPosition::Char);
  WPXBinaryData data;
  std::string type;
  if (!bmap->getBinary(data,type)) return false;

  m_listener->insertPicture(pictPos, data);

  return true;
}

bool CWGraph::sendPicture(CWGraphInternal::ZonePict &pict)
{
  bool send = false;
  for (int z = 0; z < 2; z++) {
    MWAWEntry entry = pict.m_entries[z];
    if (!entry.valid())
      continue;

    Box2f box;
    m_input->seek(entry.begin(), WPX_SEEK_SET);

    MWAWPosition pictPos=MWAWPosition(Vec2f(0,0),pict.m_box.size(), WPX_POINT);
    pictPos.setRelativePosition(MWAWPosition::Char);
    switch(pict.getSubType()) {
    case CWGraphInternal::Zone::T_Movie:
    case CWGraphInternal::Zone::T_Pict: {
      shared_ptr<MWAWPict> thePict(MWAWPictData::get(m_input, (int)entry.length()));
      if (thePict) {
        if (!send && m_listener) {
          WPXBinaryData data;
          std::string type;
          if (thePict->getBinary(data,type))
            m_listener->insertPicture(pictPos, data, type);
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
      if (!send && m_listener) {
        WPXBinaryData data;
        m_input->seek(entry.begin(), WPX_SEEK_SET);
        m_input->readDataBlock(entry.length(), data);
        m_listener->insertPicture(pictPos, data);
      }
      send = true;
      break;
    }

#ifdef DEBUG_WITH_FILES
    ascii().skipZone(entry.begin(), entry.end()-1);
    WPXBinaryData file;
    m_input->seek(entry.begin(), WPX_SEEK_SET);
    m_input->readDataBlock(entry.length(), file);
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
    if (m_listener) m_listener->insertEOL();
    sendZone(iter->first);
  }
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
