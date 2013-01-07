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

#include "MWAWCell.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWPictBasic.hxx"
#include "MWAWPictBitmap.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSubDocument.hxx"

#include "MSKGraph.hxx"

#include "MSKParser.hxx"

/** Internal: the structures of a MSKGraph */
namespace MSKGraphInternal
{

////////////////////////////////////////
//! Internal: the fonts
struct Font {
  //! the constructor
  Font(): m_font(), m_extra("") {
    for (int i = 0; i < 6; i++) m_flags[i] = 0;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Font const &font) {
    for (int i = 0; i < 6; i++) {
      if (!font.m_flags[i]) continue;
      o << "ft" << i << "=";
      if (i == 0) o << std::hex;
      o << font.m_flags[i] << std::dec << ",";
    }
    if (font.m_extra.length())
      o << font.m_extra << ",";
    return o;
  }

  //! the font
  MWAWFont m_font;
  //! some unknown flag
  int m_flags[6];
  //! extra data
  std::string m_extra;
};

/** Internal: the pattern */
struct Pattern {
  enum Type { P_Unknown, P_None, P_Percent };
  //! constructor
  Pattern(Type type=P_Unknown, float perc=1.0) : m_type(type), m_filled(perc) {}

  static float getPercentV2(int id) {
    float const (values[39]) = {
      1.0f, 0.9f, 0.7f, 0.5f, 0.7f, 0.5f, 0.7f, 0.3f, 0.7f, 0.5f,
      0.4f, 0.3f, 0.1f, 0.25f, 0.25f, 0.5f, 0.5f, 0.2f, 0.5f, 0.f /* empty */,
      0.1f, 0.2f, 0.4f, 0.3f, 0.5f, 0.3f, 0.3f, 0.25f, 0.25f, 0.25f,
      0.2f, 0.3f, 0.2f, 0.3f, 0.3f, 0.3f, 0.6f, 0.4f, 0.f /* no */
    };
    if (id >= 0 && id < 39) return values[id];
    MWAW_DEBUG_MSG(("MSKGraphInternal::Pattern::getPercentV2 find unknown id %d\n",id));
    return 1.0;
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Pattern const &pat) {
    switch(pat.m_type) {
    case P_Unknown:
      break;
    case P_None:
      o << "none,";
      break;
    case P_Percent:
      o << "percent=" << pat.m_filled << ",";
      break;
    default:
      o << "#type=" << int(pat.m_type) << ",";
    }
    return o;
  }

  //! return true if this correspond to a pattern
  bool hasPattern() const {
    return m_type == P_Percent;
  }
  //! the pattern type
  Type m_type;
  //! the approximated filled factor
  float m_filled;
};

////////////////////////////////////////
//! Internal: a list of zones ( for v4)
struct RBZone {
  RBZone(): m_isMain(true), m_id(-2), m_idList(), m_frame("") {}
  //! returns a unique id
  int getId() const {
    return m_isMain ? -1 : m_id;
  }
  //! the zone type: rbdr(true) or rbil
  bool m_isMain;
  //! the zone id
  int m_id;
  //! the list of rb
  std::vector<int> m_idList;
  //! the frame name ( if it exist )
  std::string m_frame;
};

////////////////////////////////////////
//! Internal: the generic pict
struct Zone {
  enum Type { Unknown, Basic, Group, Pict, Text, Textv4, Bitmap, TableZone, OLE};
  //! constructor
  Zone() : m_subType(-1), m_zoneId(-1), m_pos(), m_dataPos(-1), m_fileId(-1), m_page(-1), m_decal(), m_box(), m_line(-1),
    m_lineType(2), m_lineWidth(-1), m_lineColor(MWAWColor::black()), m_linePattern(Pattern::P_Percent, 1.0), m_lineFlags(0),
    m_surfaceColor(MWAWColor::white()), m_surfacePattern(Pattern::P_None),m_order(0), m_extra(""), m_isSent(false) {
    for (int i = 0; i < 3; i++) m_ids[i] = 0;
  }
  //! destructor
  virtual ~Zone() {}

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Zone const &pict) {
    pict.print(o);
    return o;
  }
  //! return the type
  virtual Type type() const {
    return Unknown;
  }

  //! return a binary data (if known)
  virtual bool getBinaryData(MWAWInputStreamPtr,
                             WPXBinaryData &res, std::string &pictType) const {
    res.clear();
    pictType="";
    return false;
  }

  //! return the extra border space ( if needed)
  virtual float needExtraBorderWidth() const {
    return 0.0;
  }

  //! add frame parameters to propList (if needed )
  virtual void fillFramePropertyList(WPXPropertyList &) const { }

  //! return the box
  Box2f getLocalBox() const {
    float x = m_box.size().x(), y=m_box.size().y();
    Vec2f min = m_box.min();
    if (x < 0) {
      x *= -1.0f;
      min+=Vec2f(x,0);
    }
    if (y < 0) {
      y *= -1.0f;
      min+=Vec2f(0,y);
    }
    Box2f res(min, min+Vec2f(x,y));
    float bExtra = needExtraBorderWidth();
    if (bExtra > 0) res.extend(2.0f*bExtra);
    return res;
  }

  MWAWPosition getPosition(MWAWPosition::AnchorTo rel) const {
    MWAWPosition res;
    Box2f box = getLocalBox();
    if (rel==MWAWPosition::Paragraph || rel==MWAWPosition::Frame) {
      res = MWAWPosition(box.min()+m_decal, box.size(), WPX_POINT);
      res.setRelativePosition(rel);
      if (rel==MWAWPosition::Paragraph)
        res.m_wrapping =  MWAWPosition::WBackground;
    } else if (rel!=MWAWPosition::Page || m_page < 0) {
      res = MWAWPosition(Vec2f(0,0), box.size(), WPX_POINT);
      res.setRelativePosition(MWAWPosition::Char,
                              MWAWPosition::XLeft, MWAWPosition::YTop);
    } else {
      res = MWAWPosition(box.min()+m_decal, box.size(), WPX_POINT);
      res.setRelativePosition(MWAWPosition::Page);
      res.setPage(m_page+1);
      res.m_wrapping =  MWAWPosition::WBackground;
    }
    if (m_order > 0) res.setOrder(m_order);
    return res;
  }

  //! the virtual print function
  virtual void print(std::ostream &o) const;

  //! the type
  int m_subType;
  //! the zone id
  int m_zoneId;
  //! the file position
  MWAWEntry m_pos;
  //! the data begin position
  long m_dataPos;
  //! the file id
  int m_fileId;
  //! the zones id (main, previous, next)
  long m_ids[3];
  //! the page
  int m_page;
  //! the local position
  Vec2f m_decal;
  //! local bdbox
  Box2f m_box;
  //! the line position(v1)
  int m_line;
  //! the line type (v2) : 0= dotted, 1=half, 2=1, 3=2pt?, 4:4pt
  int m_lineType;
  //! the line width (v3, ...)
  int m_lineWidth;
  //! the line color
  MWAWColor m_lineColor;
  //! the line pattern
  Pattern m_linePattern;
  //! the line flag
  int m_lineFlags;
  //! the 2D surface color
  MWAWColor m_surfaceColor;
  //! the line pattern
  Pattern m_surfacePattern;
  //! the picture order
  int m_order;
  //! extra data
  std::string m_extra;
  //! true if the zone is send
  bool m_isSent;
};

void Zone::print(std::ostream &o) const
{
  if (m_fileId >= 0) {
    o << "P" << m_fileId;
    if (m_zoneId >= 0) o << "[" << m_zoneId << "],";
    else o << ",";
  }
  for (int i = 0; i < 3; i++) {
    if (m_ids[i] <= 0) continue;
    switch(i) {
    case 0:
      o << "id=";
      break;
    case 1:
      o << "pId=";
      break;
    default:
      o << "nId=";
      break;
    }
    o << std::hex << m_ids[i] << std::dec << ",";
  }
  switch(m_subType) {
  case 0:
    o << "line,";
    break;
  case 1:
    o << "rect,";
    break;
  case 2:
    o << "rectOval,";
    break;
  case 3:
    o << "circle,";
    break;
  case 4:
    o << "arc,";
    break;
  case 5:
    o << "poly,";
    break;
  case 7:
    o << "pict,";
    break;
  case 8:
    o << "group,";
    break;
  case 9:
    o << "textbox,";
    break;
  case 0xa:
    o << "chart,";
    break;
  case 0xc:
    o << "equation/graph,";
    break;
  case 0xd:
    o << "bitmap,";
    break;
  case 0xe:
    o << "ssheet,";
    break;
  case 0xf:
    o << "textbox2,";
    break;
  case 0x10:
    o << "table,";
    break;
  case 0x100:
    o << "pict,";
    break; // V1 pict
  default:
    o << "#type=" << m_subType << ",";
  }
  if (m_page>=0) o << "page=" << m_page << ",";
  if (m_decal.x() < 0 || m_decal.x() > 0 || m_decal.y() < 0 || m_decal.y() > 0)
    o << "pos=" << m_decal << ",";
  o << "bdbox=" << m_box << ",";
  switch(m_lineType) {
  case 0:
    o << "line=dotted,";
    break;
  case 1:
    o << "lineWidth=1/2pt,";
    break;
  case 2:
    if (m_lineWidth >= 0) o << "lineWidth=" << m_lineWidth << "pt,";
    break;
  case 3:
    o << "lineWidth=2pt,";
    break;
  case 4:
    o << "lineWidth=4pt,";
    break;
  default:
    o << "#lineType=" << m_lineType << ",";
    break;
  }
  if (m_linePattern.m_type != Pattern::P_Percent ||
      m_linePattern.m_filled < 1.0 || m_linePattern.m_filled > 1.0)
    o << "linePattern=[" << m_linePattern << "],";
  if (!m_lineColor.isBlack())
    o << "lineColor=" << m_lineColor << ",";
  if (!m_surfaceColor.isWhite())
    o << "surfaceColor=" << m_surfaceColor << ",";
  if (m_surfacePattern.hasPattern())
    o << "surfacePattern=[" << m_surfacePattern << "],";
  /* linePattern: 38: none, 19: white, 25: diagonal(gray),
     0: black, 13: ~gray10, 26: horizontal
  */
  if (m_line >= 0) o << "line=" << m_line << ",";
  switch(m_lineFlags&3) {
  case 0:
    break;
  case 1:
    o << "endArrow,";
    break;
  case 2:
    o << "doubleArrow,";
    break;
  default:
    o << "#arrow=3,";
    break;
  }
  if (m_lineFlags& 0xFC)
    o << "#lineFlags=" << std::hex << int(m_lineFlags&0xFC) << std::dec << ",";
  if (m_extra.length()) o << m_extra;
}
////////////////////////////////////////
//! Internal: the group of a MSKGraph
struct GroupZone : public Zone {
  // constructor
  GroupZone(Zone const &z) :
    Zone(z), m_childs() { }

  //! return the type
  virtual Type type() const {
    return Group;
  }
  //! operator<<
  virtual void print(std::ostream &o) const {
    Zone::print(o);
    o << "childs=[";
    for (size_t i = 0; i < m_childs.size(); i++)
      o << "P" << m_childs[i] << ",";
    o << "],";
  }

  // list of child id
  std::vector<int> m_childs;
};

////////////////////////////////////////
//! Internal: the simple form of a MSKGraph ( line, rect, ...)
struct BasicForm : public Zone {
  //! constructor
  BasicForm(Zone const &z) : Zone(z), m_formBox(), m_angle(0), m_deltaAngle(0),
    m_vertices() {
  }

  //! return the type
  virtual Type type() const {
    return Basic;
  }
  //! operator<<
  virtual void print(std::ostream &o) const {
    Zone::print(o);
    if (m_formBox.size().x() > 0) o << "realBox=" << m_formBox << ",";
    if (m_subType == 4) o << "angl=" << m_angle << "[" << m_deltaAngle << "],";
    if (m_vertices.size()) {
      o << "pts=[";
      for (size_t i = 0; i < m_vertices.size(); i++)
        o << m_vertices[i] << ",";
      o << "],";
    }
  }

  virtual float needExtraBorderWidth() const {
    switch(m_lineType) {
    case 2:
      if (m_lineWidth >= 0) return float(0.5*(1+m_lineWidth));
      return 1.0;
    case 3:
      return 1.5;
    case 4:
      return 2.5;
    default:
      return 0.0;
    }
  }

  virtual bool getBinaryData(MWAWInputStreamPtr,
                             WPXBinaryData &res, std::string &type) const;

  //! the form bdbox ( used by arc )
  Box2i m_formBox;

  //! the angle ( used by arc )
  int m_angle, m_deltaAngle /** the delta angle */;
  //! the list of vertices ( used by polygon)
  std::vector<Vec2f> m_vertices;
};

bool BasicForm::getBinaryData(MWAWInputStreamPtr,
                              WPXBinaryData &data, std::string &pictType) const
{
  data.clear();
  pictType="";
  shared_ptr<MWAWPict> pict;
  float lineW = 1.0;
  switch(m_lineType) {
  case 0: // fixme dotted
  case 1:
    lineW = 0.5;
    break;
  case 2:
    if (m_lineWidth >= 0) lineW = float(m_lineWidth);
    break;
  case 3:
    lineW = 2.0;
    break;
  case 4:
    lineW = 4.0;
    break;
  default:
    break;
  }
  MWAWColor lineColor=MWAWColor::black();
  bool hasLineColor = false;
  if (m_linePattern.hasPattern()) {
    lineColor = MWAWColor::barycenter(m_linePattern.m_filled, m_lineColor, 1.f-m_linePattern.m_filled, m_surfaceColor);
    hasLineColor = true;
  } else if (m_linePattern.m_type == MSKGraphInternal::Pattern::P_None)
    lineW = 0.;
  bool hasSurfaceColor = false;
  MWAWColor surfaceColor=MWAWColor::white();
  if (m_surfacePattern.hasPattern()) {
    surfaceColor = MWAWColor::barycenter(m_surfacePattern.m_filled, m_surfaceColor, 1.f-m_surfacePattern.m_filled, m_lineColor);
    hasSurfaceColor = true;
  }

  switch(m_subType) {
  case 0: {
    MWAWPictLine *pct=new MWAWPictLine(m_box.min(), m_box.max());
    switch(m_lineFlags&3) {
    case 2:
      pct->setArrow(0, true);
    case 1:
      pct->setArrow(1, true);
      break;
    default:
      break;
    }
    pct->setLineWidth(lineW);
    if (hasLineColor) pct->setLineColor(lineColor);
    pict.reset(pct);
    break;
  }
  case 1: {
    MWAWPictRectangle *pct=new MWAWPictRectangle(m_box);
    pct->setLineWidth(lineW);
    if (hasLineColor) pct->setLineColor(lineColor);
    if (hasSurfaceColor) pct->setSurfaceColor(surfaceColor);
    pict.reset(pct);
    break;
  }
  case 2: {
    MWAWPictRectangle *pct=new MWAWPictRectangle(m_box);
    int sz = 10;
    if (m_box.size().x() > 0 && m_box.size().x() < 2*sz)
      sz = int(m_box.size().x())/2;
    if (m_box.size().y() > 0 && m_box.size().y() < 2*sz)
      sz = int(m_box.size().y())/2;
    pct->setRoundCornerWidth(sz);
    pct->setLineWidth(lineW);
    if (hasLineColor) pct->setLineColor(lineColor);
    if (hasSurfaceColor) pct->setSurfaceColor(surfaceColor);
    pict.reset(pct);
    break;
  }
  case 3: {
    MWAWPictCircle *pct=new MWAWPictCircle(m_box);
    pct->setLineWidth(lineW);
    if (hasLineColor) pct->setLineColor(lineColor);
    if (hasSurfaceColor) pct->setSurfaceColor(surfaceColor);
    pict.reset(pct);
    break;
  }
  case 4: {
    int angl2 = m_angle+((m_deltaAngle>0) ? m_deltaAngle : -m_deltaAngle);
    MWAWPictArc *pct=new MWAWPictArc(m_box, m_formBox, float(450-angl2), float(450-m_angle));
    pct->setLineWidth(lineW);
    if (hasLineColor) pct->setLineColor(lineColor);
    if (hasSurfaceColor) pct->setSurfaceColor(surfaceColor);
    pict.reset(pct);
    break;
  }
  case 5: {
    MWAWPictPolygon *pct = new MWAWPictPolygon(m_box, m_vertices);
    pct->setLineWidth(lineW);
    if (hasLineColor) pct->setLineColor(lineColor);
    if (hasSurfaceColor) pct->setSurfaceColor(surfaceColor);
    pict.reset(pct);
    break;
  }
  default:
    MWAW_DEBUG_MSG(("MSKGraphInternal::FormPict::getBinaryData: find unknown type\n"));
    break;
  }
  if (!pict) return false;

  return pict->getBinary(data,pictType);
}

////////////////////////////////////////
//! Internal: the picture of a MSKGraph
struct DataPict : public Zone {
  //! constructor
  DataPict(Zone const &z) : Zone(z), m_dataEndPos(-1), m_naturalBox() { }
  //! empty constructor
  DataPict() : Zone(), m_dataEndPos(-1), m_naturalBox() { }

  //! return the type
  virtual Type type() const {
    return Pict;
  }
  virtual bool getBinaryData(MWAWInputStreamPtr ip,
                             WPXBinaryData &res, std::string &type) const;

  //! operator<<
  virtual void print(std::ostream &o) const {
    Zone::print(o);
  }
  //! the end of data (only defined when different to m_pos.end())
  long m_dataEndPos;
  //! the pict box (if known )
  mutable Box2f m_naturalBox;
};

bool DataPict::getBinaryData(MWAWInputStreamPtr ip,
                             WPXBinaryData &data, std::string &pictType) const
{
  data.clear();
  pictType="";
  long endPos = m_dataEndPos<=0 ? m_pos.end() : m_dataEndPos;
  long pictSize = endPos-m_dataPos;
  if (pictSize < 0) {
    MWAW_DEBUG_MSG(("MSKGraphInternal::DataPict::getBinaryData: picture size is bad\n"));
    return false;
  }

#ifdef DEBUG_WITH_FILES
  if (1) {
    WPXBinaryData file;
    ip->seek(m_dataPos, WPX_SEEK_SET);
    ip->readDataBlock(pictSize, file);
    static int volatile pictName = 0;
    libmwaw::DebugStream f;
    f << "Pict-" << ++pictName << ".pct";
    libmwaw::Debug::dumpFile(file, f.str().c_str());
  }
#endif

  ip->seek(m_dataPos, WPX_SEEK_SET);
  MWAWPict::ReadResult res = MWAWPictData::check(ip, (int)pictSize, m_naturalBox);
  if (res == MWAWPict::MWAW_R_BAD) {
    MWAW_DEBUG_MSG(("MSKGraphInternal::DataPict::getBinaryData: can not find the picture\n"));
    return false;
  }

  ip->seek(m_dataPos, WPX_SEEK_SET);
  shared_ptr<MWAWPict> pict(MWAWPictData::get(ip, (int)pictSize));

  if (!pict)
    return false;

  return pict->getBinary(data,pictType);
}

////////////////////////////////////////
//! Internal: the bitmap of a MSKGraph
struct DataBitmap : public Zone {
  //! constructor
  DataBitmap(Zone const &z) : Zone(z), m_numRows(0), m_numCols(0), m_dataSize(0),
    m_naturalBox() { }
  //! empty constructor
  DataBitmap() : Zone(), m_numRows(0), m_numCols(0), m_dataSize(0), m_naturalBox() { }

  //! return the type
  virtual Type type() const {
    return Bitmap;
  }
  bool getPictureData(MWAWInputStreamPtr ip, WPXBinaryData &res,
                      std::string &type, std::vector<MWAWColor> const &palette) const;

  //! operator<<
  virtual void print(std::ostream &o) const {
    o << "nRows=" << m_numRows << ",";
    o << "nCols=" << m_numCols << ",";
    if (m_dataSize > 0)
      o << "dSize=" << std::hex << m_dataSize << std::dec << ",";
    Zone::print(o);
  }

  int m_numRows /** the number of rows*/, m_numCols/** the number of columns*/;
  long m_dataSize /** the bitmap data size */;
  //! the pict box (if known )
  mutable Box2f m_naturalBox;
};

bool DataBitmap::getPictureData
(MWAWInputStreamPtr ip, WPXBinaryData &data, std::string &pictType,
 std::vector<MWAWColor> const &palette) const
{
  data.clear();
  pictType="";
  if (m_dataSize <= 0 || m_dataSize < m_numRows*m_numCols) {
    MWAW_DEBUG_MSG(("MSKGraphInternal::DataBitmap::getPictureData: dataSize size is bad\n"));
    return false;
  }
  int szCol = int(m_dataSize/m_numRows);
  long pos = m_dataPos;

  MWAWPictBitmapIndexed *btmap = new MWAWPictBitmapIndexed(Vec2i(m_numCols, m_numRows));
  if (!btmap) return false;
  btmap->setColors(palette);
  shared_ptr<MWAWPict> pict(btmap);
  for (int i = 0; i < m_numRows; i++) {
    ip->seek(pos, WPX_SEEK_SET);

    unsigned long numRead;
    uint8_t const *value = ip->read((size_t) m_numCols, numRead);
    if (!value || int(numRead) != m_numCols) return false;
    btmap->setRow(i, value);

    pos += szCol;
  }

  return pict->getBinary(data,pictType);
}

////////////////////////////////////////
//! Internal: the table of a MSKGraph
struct Table : public Zone {
  //! the cell content
  struct Cell {
    Cell():m_pos(-1,-1), m_font(), m_text("") {}
    //! the cell position
    Vec2i m_pos;
    //! the font
    MWAWFont m_font;
    //! the text
    std::string m_text;
  };
  //! constructor
  Table(Zone const &z) : Zone(z), m_numRows(0), m_numCols(0),
    m_rowsDim(), m_colsDim(), m_font(), m_cellsList() { }
  //! empty constructor
  Table() : Zone(), m_numRows(0), m_numCols(0),  m_rowsDim(), m_colsDim(),
    m_font(), m_cellsList() { }

  //! return the type
  virtual Type type() const {
    return TableZone;
  }

  //! try to find a cell
  Cell const *getCell(Vec2i const pos) const {
    for (size_t i = 0; i < m_cellsList.size(); i++) {
      if (m_cellsList[i].m_pos == pos)
        return &m_cellsList[i];
    }
    return 0;
  }
  //! operator<<
  virtual void print(std::ostream &o) const {
    o << "nRows=" << m_numRows << ",";
    o << "nCols=" << m_numCols << ",";
    Zone::print(o);
  }

  int m_numRows /** the number of rows*/, m_numCols/** the number of columns*/;
  std::vector<int> m_rowsDim/**the rows dimensions*/, m_colsDim/*the columns dimensions*/;
  //! the default font
  MWAWFont m_font;
  //! the list of cell
  std::vector<Cell> m_cellsList;
};

////////////////////////////////////////
//! Internal: the textbox of a MSKGraph ( v2-v3)
struct TextBox : public Zone {
  //! constructor
  TextBox(Zone const &z) : Zone(z), m_numPositions(-1), m_fontsList(), m_positions(), m_formats(), m_text(""), m_justify(libmwaw::JustificationLeft)
  { }

  //! return the type
  virtual Type type() const {
    return Text;
  }
  //! operator<<
  virtual void print(std::ostream &o) const {
    Zone::print(o);
    switch(m_justify) {
    case libmwaw::JustificationLeft:
      break;
    case libmwaw::JustificationCenter:
      o << ",centered";
      break;
    case libmwaw::JustificationRight:
      o << ",right";
      break;
    case libmwaw::JustificationFull:
      o << ",full";
      break;
    case libmwaw::JustificationFullAllLines:
      o << ",fullAllLines";
      break;
    default:
      o << ",#just=" << m_justify;
      break;
    }
  }

  //! add frame parameters to propList (if needed )
  virtual void fillFramePropertyList(WPXPropertyList &extras) const {
    if (!m_surfaceColor.isWhite())
      extras.insert("fo:background-color", m_surfaceColor.str().c_str());
  }

  //! the number of positions
  int m_numPositions;
  //! the list of fonts
  std::vector<Font> m_fontsList;
  //! the list of positions
  std::vector<int> m_positions;
  //! the list of format
  std::vector<int> m_formats;
  //! the text
  std::string m_text;
  //! the paragraph alignement
  libmwaw::Justification m_justify;
};

////////////////////////////////////////
//! Internal: the ole zone of a MSKGraph ( v4)
struct OLEZone : public Zone {
  //! constructor
  OLEZone(Zone const &z) : Zone(z), m_oleId(-1), m_dim()
  { }

  //! return the type
  virtual Type type() const {
    return OLE;
  }
  //! operator<<
  virtual void print(std::ostream &o) const {
    if (m_oleId >= 0) o << "ole" << m_oleId << ",";
    if (m_dim[0] > 0 && m_dim[1] > 0) o << "dim=" << m_dim << ",";
    Zone::print(o);
  }

  //! the ole id
  int m_oleId;
  //! the dimension
  Vec2i m_dim;
};

////////////////////////////////////////
//! Internal: the textbox of a MSKGraph ( v4)
struct TextBoxv4 : public Zone {
  //! constructor
  TextBoxv4(Zone const &z) : Zone(z), m_text(), m_frame("")
  { }

  //! return the type
  virtual Type type() const {
    return Textv4;
  }
  //! operator<<
  virtual void print(std::ostream &o) const {
    Zone::print(o);
    if (m_text.valid()) o << ", textPos=[" << m_text.begin() << "-" << m_text.end() << "]";
  }

  //! add frame parameters to propList (if needed )
  virtual void fillFramePropertyList(WPXPropertyList &extras) const {
    if (!m_surfaceColor.isWhite())
      extras.insert("fo:background-color", m_surfaceColor.str().c_str());
  }

  //! the text of positions (0-0: means no text)
  MWAWEntry m_text;
  //! the frame name
  std::string m_frame;
};

////////////////////////////////////////
//! Internal: the state of a MSKGraph
struct State {
  //! constructor
  State() : m_version(-1), m_zonesList(), m_RBsMap(), m_font(20,12), m_numPages(0) { }

  //! the version
  int m_version;

  //! the list of zone
  std::vector<shared_ptr<Zone> > m_zonesList;

  //! the RBIL zone id->list id
  std::map<int, RBZone> m_RBsMap;

  //! the actual font
  MWAWFont m_font;

  //! the number of pages
  int m_numPages;
};

////////////////////////////////////////
//! Internal: the subdocument of a MSKGraph
class SubDocument : public MWAWSubDocument
{
public:
  enum Type { RBILZone, Table, TextBox, TextBoxv4 };
  SubDocument(MSKGraph &pars, MWAWInputStreamPtr input, Type type,
              int zoneId) :
    MWAWSubDocument(pars.m_mainParser, input, MWAWEntry()), m_graphParser(&pars), m_type(type), m_id(zoneId), m_frame("") {}
  SubDocument(MSKGraph &pars, MWAWInputStreamPtr input, Type type,
              MWAWEntry const &entry, std::string frame=std::string("")) :
    MWAWSubDocument(pars.m_mainParser, input, entry), m_graphParser(&pars), m_type(type), m_id(-1), m_frame(frame) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }

  //! returns the subdocument \a id
  int getId() const {
    return m_id;
  }
  //! sets the subdocument \a id
  void setId(int vid) {
    m_id = vid;
  }

  //! the parser function
  void parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type);
private:
  SubDocument(SubDocument const &orig);
  SubDocument &operator=(SubDocument const &orig);

protected:
  /** the graph parser */
  MSKGraph *m_graphParser;
  /** the type */
  Type m_type;
  /** the subdocument id*/
  int m_id;
  /** the frame name: for textv4 */
  std::string m_frame;
};

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("MSKParser::SubDocument::parse: no listener\n"));
    return;
  }
  MSKContentListener *listen = dynamic_cast<MSKContentListener *>(listener.get());
  if (!listen) {
    MWAW_DEBUG_MSG(("SubDocument::parse: bad listener\n"));
    return;
  }
  assert(m_graphParser);

  long pos = m_input->tell();
  switch(m_type) {
  case Table:
    m_graphParser->sendTable(m_id);
    break;
  case TextBox:
    m_graphParser->sendTextBox(m_id);
    break;
  case TextBoxv4:
    m_graphParser->sendFrameText(m_zone, m_frame);
    break;
  case RBILZone: {
    MSKGraph::SendData sendData;
    sendData.m_type = MSKGraph::SendData::RBIL;
    sendData.m_id = m_id;
    sendData.m_anchor =  MWAWPosition::Frame;
    m_graphParser->sendObjects(sendData);
    break;
  }
  default:
    MWAW_DEBUG_MSG(("MSKGraph::SubDocument::parse: unexpected zone type\n"));
    break;
  }
  m_input->seek(pos, WPX_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_graphParser != sDoc->m_graphParser) return true;
  if (m_id != sDoc->m_id) return true;
  if (m_type != sDoc->m_type) return true;
  if (m_frame != sDoc->m_frame) return true;
  return false;
}

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MSKGraph::MSKGraph
(MWAWInputStreamPtr ip, MSKParser &parser, MWAWFontConverterPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert), m_state(new MSKGraphInternal::State),
  m_mainParser(&parser), m_asciiFile(&parser.ascii())
{
}

MSKGraph::~MSKGraph()
{ }

void MSKGraph::reset(MSKParser &parser, MWAWFontConverterPtr &convert)
{
  m_mainParser=&parser;
  m_convertissor = convert;

  m_input=parser.getInput();
  m_state.reset(new MSKGraphInternal::State);
  m_listener.reset();
  m_asciiFile = &parser.ascii();
}

int MSKGraph::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_mainParser->version();
  return m_state->m_version;
}

int MSKGraph::numPages(int zoneId) const
{
  if (m_state->m_numPages > 0)
    return m_state->m_numPages;

  int maxPage = 0;
  size_t numZones = m_state->m_zonesList.size();
  for (size_t i = 0; i < numZones; i++) {
    shared_ptr<MSKGraphInternal::Zone> zone = m_state->m_zonesList[i];
    if (zoneId >= 0 && zone->m_zoneId!=zoneId) continue;
    if (zone->m_page > maxPage)
      maxPage = zone->m_page;
  }
  m_state->m_numPages = maxPage+1;
  return m_state->m_numPages;
}

void MSKGraph::sendFrameText(MWAWEntry const &entry, std::string const &frame)
{
  m_mainParser->sendFrameText(entry, frame);
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool MSKGraph::readPictHeader(MSKGraphInternal::Zone &pict)
{
  if (m_input->readULong(1) != 0) return false;
  pict = MSKGraphInternal::Zone();
  pict.m_subType = (int) m_input->readULong(1);
  if (pict.m_subType > 0x10 || pict.m_subType == 6 || pict.m_subType == 0xb)
    return false;
  int vers = version();
  if (vers <= 3 && pict.m_subType > 9)
    return false;

  libmwaw::DebugStream f;
  int val;
  if (vers >= 3) {
    val = (int) m_input->readLong(2);
    if (vers == 4)
      pict.m_page = val;
    else if (val)
      f << "f0=" << val << ",";
  }
  // color
  for (int i = 0; i < 2; i++) {
    int rId = (int) m_input->readLong(2);
    int cId = (vers <= 2) ? rId+1 : rId;
    MWAWColor col;
    if (m_mainParser->getColor(cId,col,vers <= 3 ? vers : 3)) {
      if (i) pict.m_surfaceColor = col;
      else pict.m_lineColor = col;
    } else
      f << "#col" << i << "=" << rId << ",";
  }
  if (vers <= 2) {
    for (int i = 0; i < 2; i++) {
      int pId = (int) m_input->readLong(2);
      float percent = MSKGraphInternal::Pattern::getPercentV2(pId);
      MSKGraphInternal::Pattern::Type type = pId == 38 ?
                                             MSKGraphInternal::Pattern::P_None :
                                             MSKGraphInternal::Pattern::P_Percent;
      if (i) pict.m_surfacePattern = MSKGraphInternal::Pattern(type, percent);
      else pict.m_linePattern = MSKGraphInternal::Pattern(type, percent);
    }
    pict.m_lineType=(int) m_input->readLong(2);
  } else {
    for (int i = 0; i < 2; i++) {
      if (i) f << "surface";
      else f << "line";
      f << "Pattern=[";
      val =  (int) m_input->readULong(2);
      if (val != 0xFA3) f << std::hex << val << ",";
      else f << "_,";
      int patId = (int) m_input->readULong(2);
      if (patId) f << patId << ",";
      else f << "_,";
      val = (int) m_input->readULong(1);
      if (val) f << "unkn=" << std::hex << val << std::dec << ",";
      val = (int) m_input->readULong(1);
      if (val >= 0 && val <= 100) {
        MSKGraphInternal::Pattern pat(patId ? MSKGraphInternal::Pattern::P_Percent : MSKGraphInternal::Pattern::P_None, float(val/100.));
        if (i) pict.m_surfacePattern = pat;
        else pict.m_linePattern = pat;
      } else
        f << "##";
      f << val << "%,";
      f << "],";
    }
    int penSize[2];
    for (int i = 0; i < 2; i++)
      penSize[i] = (int) m_input->readLong(2);
    if (penSize[0]==penSize[1])
      pict.m_lineWidth=penSize[0];
    else {
      f << "pen=" << penSize[0] << "x" << penSize[1] << ",";
      pict.m_lineWidth=(penSize[0]+penSize[1]+1)/2;
    }
    if (pict.m_lineWidth < 0 || pict.m_lineWidth > 10) {
      f << "##penSize=" << pict.m_lineWidth << ",";
      pict.m_lineWidth = 1;
    }
    val =  (int) m_input->readLong(2);
    if (val)
      f << "f1=" << val << ",";
  }

  int offset[4];
  for (int i = 0; i < 4; i++)
    offset[i] = (int) m_input->readLong(2);
  pict.m_decal = Vec2f(float(offset[0]+offset[3]), float(offset[1]+offset[2]));

  // the two point which allows to create the form ( in general the bdbox)
  float dim[4];
  for (int i = 0; i < 4; i++)
    dim[i] = float(m_input->readLong(4))/65536.f;
  pict.m_box=Box2f(Vec2f(dim[0],dim[1]),Vec2f(dim[2],dim[3]));

  int flags = (int) m_input->readLong(1);
  if (vers >= 4 && flags==2) {
    // 2: rotations, 0: nothing, other ?
    f << ", Rot=[";
    f << m_input->readLong(2) << ",";
    for (int i = 0; i < 31; i++)
      f << m_input->readLong(2) << ",";
    f << "]";
  } else if (flags) f << "fl0=" << flags << ",";
  pict.m_lineFlags = (int) m_input->readULong(1);
  if (vers >= 3) pict.m_ids[0] = (long) m_input->readULong(4);
  pict.m_extra = f.str();
  pict.m_dataPos = m_input->tell();
  return true;
}

int MSKGraph::getEntryPicture(int zoneId, MWAWEntry &zone)
{
  int zId = -1;
  MSKGraphInternal::Zone pict;
  long pos = m_input->tell();

  if (!readPictHeader(pict))
    return zId;
  pict.m_zoneId = zoneId;
  pict.m_pos.setBegin(pos);
  libmwaw::DebugStream f;
  int vers = version();
  long debData = m_input->tell();
  long dataSize = 0;
  int versSize = 0;
  switch(pict.m_subType) {
  case 0:
  case 1:
  case 2:
  case 3:
    dataSize = 1;
    break;
  case 4: // arc
    dataSize = 0xd;
    break;
  case 5: { // poly
    m_input->seek(3, WPX_SEEK_CUR);
    int N = (int) m_input->readULong(2);
    dataSize = 9+N*8;
    break;
  }
  case 7: { // picture
    if (vers >= 3) versSize = 0x14;
    dataSize = 5;
    m_input->seek(debData+5+versSize-2, WPX_SEEK_SET);
    dataSize += (int) m_input->readULong(2);
    break;
  }
  case 8: // group
    if (vers >= 3) versSize = 4;
    dataSize = 0x1b;
    break;
  case 9: // textbox v<=3
    dataSize = 0x21;
    if (vers >= 3) dataSize += 0x10;
    break;
  case 0xa: // chart v4
    dataSize = 50;
    break;
  case 0xc: // equation v4
    dataSize = 0x11;
    break;
  case 0xd: { // bitmap v4
    m_input->seek(debData+0x29, WPX_SEEK_SET);
    long sz = (long) m_input->readULong(4);
    dataSize = 0x29+4+sz;
    break;
  }
  case 0xe: { // spreadsheet v4
    m_input->seek(debData+0xa7, WPX_SEEK_SET);
    int pSize = (int) m_input->readULong(2);
    if (pSize == 0) return zId;
    dataSize = 0xa9+pSize;
    if (!m_mainParser->checkIfPositionValid(debData+dataSize))
      return zId;

    m_input->seek(debData+dataSize, WPX_SEEK_SET);
    for (int i = 0; i < 2; i++) {
      long sz = (long) m_input->readULong(4);
      if (sz<0 || (sz>>28)) return zId;
      dataSize += 4 + sz;
      m_input->seek(sz, WPX_SEEK_CUR);
    }
    break;
  }
  case 0xf: { // textbox v4
    m_input->seek(debData+0x39, WPX_SEEK_SET);
    dataSize = 0x3b+ (long) m_input->readULong(2);
    break;
  }
  case 0x10: { // table v4
    m_input->seek(debData+0x57, WPX_SEEK_SET);
    dataSize = 0x59+ (long) m_input->readULong(2);
    m_input->seek(debData+dataSize, WPX_SEEK_SET);

    for (int i = 0; i < 3; i++) {
      long sz = (long) m_input->readULong(4);
      if (sz<0 || ((sz>>28))) return zId;
      dataSize += 4 + sz;
      m_input->seek(sz, WPX_SEEK_CUR);
    }

    break;
  }
  default:
    MWAW_DEBUG_MSG(("MSKGraph::getEntryPicture: type %d is not umplemented\n", pict.m_subType));
    return zId;
  }

  pict.m_pos.setEnd(debData+dataSize+versSize);
  if (!m_mainParser->checkIfPositionValid(pict.m_pos.end()))
    return zId;

  m_input->seek(debData, WPX_SEEK_SET);
  if (versSize) {
    switch(pict.m_subType) {
    case 7: {
      long ptr = (long) m_input->readULong(4);
      f << std::hex << "ptr2=" << ptr << std::dec << ",";
      f << "depth?=" << m_input->readLong(1) << ",";
      float dim[4];
      for (int i = 0; i < 4; i++)
        dim[i] = float(m_input->readLong(4))/65536.f;
      Box2f box(Vec2f(dim[1], dim[0]), Vec2f(dim[3], dim[2]));
      f << "bdbox2=" << box << ",";
      break;
    }
    default:
      break;
    }
  }
  int val = (int) m_input->readLong(1); // 0 and sometimes -1
  if (val) f << "g0=" << val << ",";
  pict.m_dataPos++;

  if (pict.m_subType > 0xd) {
    f << ", " << std::hex << m_input->readULong(4) << std::dec << ", BdBox2=(";
    for (int i = 0; i < 4; i++)
      f << float(m_input->readLong(4))/65536.f << ", ";
    f << ")";
  }

  shared_ptr<MSKGraphInternal::Zone> res;
  switch (pict.m_subType) {
  case 0:
  case 1:
  case 2:
  case 3:
    res.reset(new MSKGraphInternal::BasicForm(pict));
    break;
  case 4: {
    MSKGraphInternal::BasicForm *form  = new MSKGraphInternal::BasicForm(pict);
    res.reset(form);
    form->m_angle = (int) m_input->readLong(2);
    form->m_deltaAngle = (int) m_input->readLong(2);
    int dim[4]; // real Bdbox
    for (int i = 0; i < 4; i++)
      dim[i] = (int) m_input->readLong(2);
    form->m_formBox = form->m_box;
    form->m_box = Box2i(Vec2i(dim[1],dim[0]), Vec2i(dim[3],dim[2]));
    break;
  }
  case 5: {
    MSKGraphInternal::BasicForm *form  = new MSKGraphInternal::BasicForm(pict);
    res.reset(form);
    val = (int) m_input->readULong(2);
    if (val) f << "g1=" << val << ",";
    int numPt = (int) m_input->readLong(2);
    long ptr = (long) m_input->readULong(4);
    f << std::hex << "ptr2=" << ptr << std::dec << ",";
    for (int i = 0; i < numPt; i++) {
      float x = float(m_input->readLong(4))/65336.f;
      float y = float(m_input->readLong(4))/65336.f;
      form->m_vertices.push_back(Vec2f(x,y));
    }
    break;
  }
  case 7: {
    val =  (int) m_input->readULong(vers >= 3 ? 1 : 2);
    if (val) f << "g1=" << val << ",";
    // skip size (already read)
    pict.m_dataPos = m_input->tell()+2;
    MSKGraphInternal::DataPict *pct  = new MSKGraphInternal::DataPict(pict);
    res.reset(pct);
    ascii().skipZone(pct->m_dataPos, pct->m_pos.end()-1);
    break;
  }
  case 8:
    res = readGroup(pict);
    break;
  case 9: { // textbox normal
    libmwaw::Justification justify = libmwaw::JustificationLeft;
    val = (int) m_input->readLong(2);
    switch(val) {
    case 0:
      break;
    case 1:
      justify = libmwaw::JustificationCenter;
      break;
    case 2:
      justify = libmwaw::JustificationFull;
      break;
    case -1:
      justify = libmwaw::JustificationRight;
      break;
    default:
      f << "##align=" << val << ",";
      break;
    }
    if (vers >= 3) {
      f << "h=" << m_input->readLong(4) << ",";
      for (int i = 0; i < 6; i++) {
        val = (int) m_input->readLong(2);
        if (val) f << "g" << i+2 << "=" << val << ",";
      }
      pict.m_dataPos += 0x10;
    }
    f << "Fl=[";
    for (int i = 0; i < 4; i++) {
      val = (int) m_input->readLong(2);
      if (val) f << std::hex << val << std::dec << ",";
      else f << ",";
    }
    f << "],";
    int numPos = (int) m_input->readLong(2);
    if (numPos < 0) return zId;
    f << "numFonts=" << m_input->readLong(2);

    long off[4];
    for (int i = 0; i < 4; i++)
      off[i] = (long) m_input->readULong(4);
    f << ", Ptrs=[" <<  std::hex << std::setw(8) << off[2] << ", " << std::setw(8) << off[0]
      << ", " << std::dec << long(off[1]-off[0])
      << ", "	<< std::dec << long(off[3]-off[0]) << "]";

    MSKGraphInternal::TextBox *text  = new MSKGraphInternal::TextBox(pict);
    text->m_justify = justify;
    text->m_numPositions = numPos;
    res.reset(text);
    if (!readText(*text)) return zId;
    res->m_pos.setEnd(m_input->tell());
    break;
  }
  case 0xa: { // chart
    size_t actualZone = m_state->m_zonesList.size();
    if (!readChart(pict)) {
      m_state->m_zonesList.resize(actualZone);
      return zId;
    }

    f.str("");
    f << "Entries(Grapha):" << pict;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());

    return int(m_state->m_zonesList.size())-1;
  }
  case 0xc: { // equation
    MSKGraphInternal::OLEZone *ole  = new MSKGraphInternal::OLEZone(pict);
    res.reset(ole);
    int dim[2];
    for (int i = 0; i < 2; i++)
      dim[i] = (int) m_input->readLong(4);
    ole->m_dim = Vec2i(dim[0], dim[1]);
    val = (int) m_input->readULong(2); // always 0x4f4d ?
    f << "g0=" << std::hex << val << std::dec << ",";
    ole->m_oleId=(int) m_input->readULong(4);
    val = (int) m_input->readLong(2); // always 0?
    if (val) f << "g1=" << val << ",";
    break;
  }
  case 0xd: { // bitmap
    libmwaw::DebugStream f2;
    f2 << "Graphd(II): fl(";

    long actPos = m_input->tell();
    for (int i = 0; i < 2; i++)
      f2 << m_input->readLong(2) << ", ";
    f2 << "), ";
    int nCol = (int) m_input->readLong(2);
    int nRow = (int) m_input->readLong(2);
    if (nRow <= 0 || nCol <= 0) return zId;

    f2 << "nRow=" << nRow << ", " << "nCol=" << nCol << ", ";

    f2 << std::hex << m_input->readULong(4) << std::dec << ", ";

    for (int i = 0; i < 3; i++) {
      f2 << "bdbox" << i << "=(";
      for (int d= 0; d < 4; d++) f2 << m_input->readLong(2) << ", ";
      f2 << "), ";
      if (i == 1) f2 << "unk=" << m_input->readLong(2) << ", ";
    }
    int sizeLine =  (int) m_input->readLong(2);
    f2 << "lineSize(?)=" << sizeLine << ", ";
    long bitmapSize = m_input->readLong(4);
    f2 << "bitmapSize=" << std::hex << bitmapSize << ", ";

    if (bitmapSize <= 0 || (bitmapSize%nRow) != 0) {
      // sometimes, another row is added: only for big picture?
      if (bitmapSize>0 && (bitmapSize%(nRow+1)) == 0) nRow++;
      else if (bitmapSize < nCol*nRow || bitmapSize > 2*nCol*nRow)
        return zId;
      else { // maybe a not implemented case
        MWAW_DEBUG_MSG(("MSKGraph::getEntryPicture: bitmap size is a little odd\n"));
        f2 << "###";
        ascii().addPos(actPos);
        ascii().addNote(f2.str().c_str());
        ascii().addDelimiter(m_input->tell(),'|');
        break;
      }
    }

    int szCol = int(bitmapSize/nRow);
    if (szCol < nCol) return zId;

    ascii().addPos(actPos);
    ascii().addNote(f2.str().c_str());

    pict.m_dataPos = m_input->tell();
    MSKGraphInternal::DataBitmap *pct  = new MSKGraphInternal::DataBitmap(pict);
    pct->m_numRows = nRow;
    pct->m_numCols = nCol;
    pct->m_dataSize = bitmapSize;
    res.reset(pct);
    break;
  }
  case 0xe: {
    long actPos = m_input->tell();
    ascii().addPos(actPos);
    ascii().addNote("Graphe(I)");

    // first: the picture ( fixme: kept while we do not parse the spreadsheet )
    m_input->seek(144, WPX_SEEK_CUR);
    actPos = m_input->tell();
    ascii().addPos(actPos);
    ascii().addNote("Graphe(pict)");
    long dSize = (long) m_input->readLong(4);
    if (dSize < 0) return zId;
    pict.m_dataPos = actPos+4;

    MSKGraphInternal::DataPict *pct  = new MSKGraphInternal::DataPict(pict);
    pct->m_dataEndPos = actPos+4+dSize;
    res.reset(pct);
    ascii().skipZone(pct->m_dataPos, pct->m_dataEndPos-1);
    m_input->seek(actPos+4+dSize, WPX_SEEK_SET);

    // now the spreadsheet ( a classic WKS file )
    actPos = m_input->tell();
    dSize = (long) m_input->readULong(4);
    if (dSize < 0) return zId;
    ascii().addPos(actPos);
    ascii().addNote("Graphe(sheet)");
    ascii().skipZone(actPos+4, actPos+3+dSize);
#ifdef DEBUG_WITH_FILES
    if (dSize > 0) {
      WPXBinaryData file;
      m_input->seek(actPos+4, WPX_SEEK_SET);
      m_input->readDataBlock(dSize, file);
      static int volatile sheetName = 0;
      libmwaw::DebugStream f2;
      f2 << "Sheet-" << ++sheetName << ".wks";
      libmwaw::Debug::dumpFile(file, f2.str().c_str());
    }
#endif
    m_input->seek(actPos+4+dSize, WPX_SEEK_SET);

    actPos = m_input->tell();
    ascii().addPos(actPos);
    ascii().addNote("Graphe(colWidth?)"); // blocksize, unknown+list of 100 w
    break;
  }
  case 0xf: { // new text box v4 (a picture is stored)
    if (vers < 4) return false;
    MSKGraphInternal::TextBoxv4 *textbox = new MSKGraphInternal::TextBoxv4(pict);
    res.reset(textbox);
    textbox->m_ids[1] = (long) m_input->readULong(4);
    textbox->m_ids[2] = (long) m_input->readULong(4);
    f << "," << std::hex << m_input->readULong(4)<< std::dec << ",";
    // always 0 ?
    for (int i = 0; i < 6; i++) {
      val = (int) m_input->readLong(2);
      if (val) f << "f" << i << "=" << val << ",";
    }
    textbox->m_text.setBegin(m_input->readLong(4));
    textbox->m_text.setEnd(m_input->readLong(4));

    // always 0 ?
    val = (int) m_input->readLong(2);
    if (val) f << "f10=" << val << ",";
    long sz = (long) m_input->readULong(4);
    if (sz+0x3b != dataSize)
      f << "###sz=" << sz << ",";

    pict.m_dataPos = m_input->tell();
    if (pict.m_dataPos != pict.m_pos.end()) {
#ifdef DEBUG_WITH_FILES
      WPXBinaryData file;
      m_input->readDataBlock(pict.m_pos.end()-pict.m_dataPos, file);
      static int volatile textboxName = 0;
      libmwaw::DebugStream f2;
      f2 << "TextBox-" << ++textboxName << ".pct";
      libmwaw::Debug::dumpFile(file, f2.str().c_str());
#endif
      ascii().skipZone(pict.m_dataPos, pict.m_pos.end()-1);
    }
    break;
  }
  case 0x10: { // basic table
    libmwaw::DebugStream f2;
    f2 << "Graph10(II): fl=(";
    long actPos = m_input->tell();
    for (int i = 0; i < 3; i++)
      f2 << m_input->readLong(2) << ", ";
    f2 << "), ";
    int nRow = (int) m_input->readLong(2);
    int nCol = (int) m_input->readLong(2);
    f2 << "nRow=" << nRow << ", " << "nCol=" << nCol << ", ";

    // basic name font
    int nbChar = (int) m_input->readULong(1);
    if (nbChar > 31) return false;
    std::string fName;
    for (int c = 0; c < nbChar; c++)
      fName+=(char) m_input->readLong(1);
    f2 << fName << ",";
    m_input->seek(actPos+10+32, WPX_SEEK_SET);
    int fSz = (int) m_input->readLong(2);
    if (fSz) f << "fSz=" << fSz << ",";

    ascii().addDelimiter(m_input->tell(),'|');
    ascii().addPos(actPos);
    ascii().addNote(f2.str().c_str());
    m_input->seek(actPos+0x40, WPX_SEEK_SET);

    // a pict
    actPos = m_input->tell();
    ascii().addPos(actPos);
    ascii().addNote("Graph10(pict)");
    long dSize = (long) m_input->readLong(4);
    if (dSize < 0) return zId;
    pict.m_dataPos = actPos+4;

    MSKGraphInternal::DataPict *pct  = new MSKGraphInternal::DataPict(pict);
    pct->m_dataEndPos = actPos+4+dSize;
    res.reset(pct);
    ascii().skipZone(pct->m_dataPos, pct->m_dataEndPos-1);
    m_input->seek(actPos+4+dSize, WPX_SEEK_SET);

    // the table
    MSKGraphInternal::Table *table  = new MSKGraphInternal::Table(pict);
    table->m_numRows = nRow;
    table->m_numCols = nCol;
    if (readTable(*table))
      res.reset(table);
    break;
  }
  default:
    ascii().addDelimiter(debData, '|');
    break;
  }

  if (!res)
    res.reset(new MSKGraphInternal::Zone(pict));
  res->m_extra += f.str();

  zId = int(m_state->m_zonesList.size());
  res->m_fileId = zId;
  m_state->m_zonesList.push_back(res);

  f.str("");
  f << "Entries(Graph" << std::hex << res->m_subType << std::dec << "):" << *res;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  zone = res->m_pos;
  zone.setType("Graphic");
  m_input->seek(res->m_pos.end(), WPX_SEEK_SET);

  return zId;
}

void MSKGraph::computePositions(int zoneId, std::vector<int> &linesH, std::vector<int> &pagesH)
{
  int numLines = int(linesH.size());
  int nPages = int(pagesH.size());
  size_t numZones = m_state->m_zonesList.size();
  for (size_t i = 0; i < numZones; i++) {
    shared_ptr<MSKGraphInternal::Zone> zone = m_state->m_zonesList[i];
    if (zone->m_zoneId != -1 && zoneId != zone->m_zoneId) continue;
    if (zone->m_line >= 0) {
      int h = 0;
      if (zone->m_line >= numLines) {
        MWAW_DEBUG_MSG(("MSKGraph::computePositions: linepos is too big\n"));
        if (numLines)
          h = linesH[(size_t) numLines-1];
      } else
        h = linesH[(size_t) zone->m_line];
      zone->m_decal = Vec2f(0, float(h));
    }
    if (zone->m_page < 0 && zone->m_page != -2) {
      float h = zone->m_decal.y();
      float middleH=zone->m_box.center().y();
      h+=middleH;
      int p = 0;
      while (p < nPages) {
        if (h < pagesH[(size_t) p]) break;
        h -= float(pagesH[(size_t) p++]);
      }
      zone->m_page = p;
      zone->m_decal.setY(h-middleH);
    }
  }
}

int MSKGraph::getEntryPictureV1(int zoneId, MWAWEntry &zone)
{
  int zId = -1;
  if (m_input->atEOS()) return zId;

  long pos = m_input->tell();
  if (m_input->readULong(1) != 1) return zId;

  libmwaw::DebugStream f;
  long ptr = (long) m_input->readULong(2);
  int flag = (int) m_input->readULong(1);
  long size = (long) m_input->readULong(2)+6;
  if (size < 22) return zId;

  shared_ptr<MSKGraphInternal::DataPict> pict(new MSKGraphInternal::DataPict);
  pict->m_zoneId = zoneId;
  pict->m_subType = 0x100;
  pict->m_pos.setBegin(pos);
  pict->m_pos.setLength(size);
  // check if we can go to the next zone
  if (!m_mainParser->checkIfPositionValid(pict->m_pos.end())) return zId;

  if (ptr) f << std::hex << "ptr0=" << ptr << ",";
  if (flag) f << std::hex << "fl=" << flag << ",";

  ptr = m_input->readLong(4);
  if (ptr)
    f << "ptr1=" << std::hex << ptr << std::dec << ";";
  pict->m_line = (int) m_input->readLong(2);
  int val = (int) m_input->readLong(2); // almost always equal to m_linePOs
  if (val !=  pict->m_line)
    f <<  "linePos2=" << std::hex << val << std::dec << ",";
  int dim[4]; // pictbox
  for (int i = 0; i < 4; i++)
    dim[i] = (int) m_input->readLong(2);
  pict->m_box = Box2f(Vec2f(float(dim[1]), float(dim[0])), Vec2f(float(dim[3]),float(dim[2])));

  Vec2i pictMin = pict->m_box.min(), pictSize = pict->m_box.size();
  if (pictSize.x() < 0 || pictSize.y() < 0) return zId;

  if (pictSize.x() > 3000 || pictSize.y() > 3000 ||
      pictMin.x() < -200 || pictMin.y() < -200) return zId;
  pict->m_dataPos = m_input->tell();

  zone = pict->m_pos;
  zone.setType("GraphEntry");

  pict->m_extra = f.str();
  zId = int(m_state->m_zonesList.size());
  pict->m_fileId = zId;
  m_state->m_zonesList.push_back(pict);

  f.str("");
  f << "Entries(GraphEntry):" << *pict;

  ascii().skipZone(pict->m_dataPos, pict->m_pos.end()-1);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  m_input->seek(pict->m_pos.end(), WPX_SEEK_SET);
  return zId;

}

// a list of picture
bool MSKGraph::readRB(MWAWInputStreamPtr input, MWAWEntry const &entry)
{
  if (entry.length() < 0x164) return false;
  entry.setParsed(true);
  libmwaw::DebugStream f;
  MSKGraphInternal::RBZone zone;
  zone.m_isMain = entry.name()=="RBDR";
  zone.m_id = entry.id();

  uint32_t page_offset = (uint32_t) entry.begin();
  long endOfPage = entry.end();

  input->seek(long(page_offset), WPX_SEEK_SET);
  f << input->readLong(4) << ", ";
  for (int i = 0; i < 4; i++) {
    long val = input->readLong(4);
    if (val) f << "#t" << i << "=" << val << ", ";
  }
  f << "type?=" << std::hex << input->readLong(2) << std::dec << ", ";
  f << "numPage=" << input->readLong(2) << ", ";
  for (int i = 0; i < 11; i++) {
    long val = input->readLong(2);
    if (i >= 8 && (val < -100 || val > 100)) f << "###";
    f << val << ", ";
  }
  f << ", unk=(";
  for (int i = 0; i < 2; i++)
    f << input->readLong(4) << ",";
  f << "), ";
  for (int i = 0; i < 9; i++) {
    long val = input->readLong(2);
    if (val) f << "#u" << i << "=" << val << ", ";
  }
  f << std::hex << "sz?=" << input->readLong(4) << std::dec << ", ";
  for (int i = 0; i < 2; i++) {
    long val = input->readLong(2);
    if (val) f << "#v" << i << "=" << val << ", ";
  }

  f << "unk1=(";
  for (int i = 0; i < 9; i++) {
    long val = input->readLong(2);
    if (val) f << val << ",";
    else f << "_,";
  }
  f << "), ";
  std::string oleName;
  while(input->tell() < long(page_offset)+0x162) {
    char val  = (char) input->readLong(1);
    if (val == 0) break;
    oleName+= val;
    if (oleName.length() > 30) break;
  }
  if (!oleName.empty()) {
    zone.m_frame = oleName;
    f << "ole='" << oleName << "', ";
  }

  int i = int(input->tell()-long(page_offset));
  if ((i%2) == 1) {
    int val = (int) input->readLong(1);
    if (val) f << "f" << i << "=" << val << ",";
    i++;
  }
  while (i != 0x162) {
    int val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
    i+=2;
  }
  int n = (int) input->readLong(2);
  f << "N= " << n;
  ascii().addPos(long(page_offset));
  ascii().addNote(f.str().c_str());

  if (n == 0) return true;

  input->pushLimit(endOfPage);
  while (input->tell()+20 < endOfPage) {
    long debPos = input->tell();
    size_t actId = m_state->m_zonesList.size();
    MWAWEntry pict;
    int pictId = getEntryPicture(0, pict);
    if (pictId < 0 || m_input->tell() <= debPos) {
      f.str("");
      f << "###" << entry.name();
      ascii().addPos(debPos);
      ascii().addNote(f.str().c_str());
      break;
    }
    for (size_t z = actId; z < m_state->m_zonesList.size(); z++) {
      zone.m_idList.push_back(int(z));
      shared_ptr<MSKGraphInternal::Zone> pictZone =
        m_state->m_zonesList[z];
      if (!zone.m_isMain)
        pictZone->m_page = -2;
    }
  }
  input->popLimit();

  if (zone.m_idList.size() == 0) return false;
  int zId = zone.getId();
  if (m_state->m_RBsMap.find(zId) != m_state->m_RBsMap.end()) {
    MWAW_DEBUG_MSG(("MSKGraph::readRB: zone %d is already filled\n", zId));
    return false;
  }
  m_state->m_RBsMap[zId]=zone;
  checkTextBoxLinks(zId);
  return true;
}

void MSKGraph::checkTextBoxLinks(int zId)
{
  std::map<int, MSKGraphInternal::RBZone>::const_iterator rbIt
    = m_state->m_RBsMap.find(zId);
  if (rbIt==m_state->m_RBsMap.end())
    return;
  std::vector<int> const &listIds = rbIt->second.m_idList;
  std::string const &fName = rbIt->second.m_frame;
  int numZones = int(m_state->m_zonesList.size());
  std::set<long> textIds;
  std::map<long,long> prevLinks, nextLinks;
  bool ok = true;
  for (size_t z = 0; z < listIds.size(); z++) {
    int id = listIds[z];
    if (id < 0 || id >= numZones) continue;
    shared_ptr<MSKGraphInternal::Zone> zone = m_state->m_zonesList[size_t(id)];
    if (zone->type() != MSKGraphInternal::Zone::Textv4)
      continue;
    reinterpret_cast<MSKGraphInternal::TextBoxv4 &>(*zone).m_frame = fName;
    if (textIds.find(zone->m_ids[0]) != textIds.end()) {
      MWAW_DEBUG_MSG(("MSKGraph::checkTextBoxLinks: id %lX already exists\n", zone->m_ids[0]));
      ok = false;
      break;
    }
    textIds.insert(zone->m_ids[0]);
    if (zone->m_ids[1]>0)
      prevLinks.insert(std::map<long,long>::value_type(zone->m_ids[0],zone->m_ids[1]));
    if (zone->m_ids[2]>0)
      nextLinks.insert(std::map<long,long>::value_type(zone->m_ids[0],zone->m_ids[2]));
  }
  size_t numLinks = nextLinks.size();
  for (std::map<long,long>::const_iterator link=nextLinks.begin();
       link!=nextLinks.end(); link++) {
    if (prevLinks.find(link->second)==prevLinks.end() ||
        prevLinks.find(link->second)->second!=link->first) {
      MWAW_DEBUG_MSG(("MSKGraph::checkTextBoxLinks: can not find prevLinks: %lX<->%lX already exists\n", link->first, link->second));
      ok = false;
      break;
    }
    // check loops
    size_t w = 0;
    long actText = link->second;
    while (1) {
      if (nextLinks.find(actText)==nextLinks.end())
        break;
      actText = nextLinks.find(actText)->second;
      if (w++ > numLinks) {
        MWAW_DEBUG_MSG(("MSKGraph::checkTextBoxLinks:find a loop for id %lX\n", link->first));
        ok = false;
        break;
      }
    }
  }
  if (!ok) {
    MWAW_DEBUG_MSG(("MSKGraph::checkTextBoxLinks: problem find with text links\n"));
    for (size_t z = 0; z < m_state->m_zonesList.size(); z++) {
      shared_ptr<MSKGraphInternal::Zone> zone = m_state->m_zonesList[z];
      if (zone->type() != MSKGraphInternal::Zone::Textv4)
        continue;
      zone->m_ids[1] = zone->m_ids[2] = 0;
    }
  }
}

bool MSKGraph::readPictureV4(MWAWInputStreamPtr /*input*/, MWAWEntry const &entry)
{
  if (!entry.hasType("PICT")) {
    MWAW_DEBUG_MSG(("MSKGraph::readPictureV4: unknown type='%s'\n", entry.type().c_str()));
    return false;
  }
  entry.setParsed(true);

  MSKGraphInternal::Zone pict;
  pict.m_pos = entry;
  pict.m_dataPos = entry.begin();
  pict.m_page = -2;
  pict.m_zoneId = -1;

  MSKGraphInternal::DataPict *pct  = new MSKGraphInternal::DataPict(pict);
  shared_ptr<MSKGraphInternal::Zone>res(pct);
  ascii().skipZone(entry.begin(), entry.end()-1);

  int zId = int(m_state->m_zonesList.size());
  res->m_fileId = zId;
  m_state->m_zonesList.push_back(res);

  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

// read a group
shared_ptr<MSKGraphInternal::GroupZone> MSKGraph::readGroup(MSKGraphInternal::Zone &header)
{
  shared_ptr<MSKGraphInternal::GroupZone> group(new MSKGraphInternal::GroupZone(header));
  libmwaw::DebugStream f;
  m_input->seek(header.m_dataPos, WPX_SEEK_SET);
  long dim[4];
  for (int i = 0; i < 4; i++) dim[i] = m_input->readLong(4);
  f << "groupDim=" << dim[0] << "x" << dim[1] << "<->" << dim[2] << "x" << dim[3] << ",";
  long ptr[2];
  for (int i = 0; i < 2; i++)
    ptr[i] = (long) m_input->readULong(4);
  f << "ptr0=" << std::hex << ptr[0] << std::dec << ",";
  if (ptr[0] != ptr[1])
    f << "ptr1=" << std::hex << ptr[1] << std::dec << ",";
  int val;
  if (version() >= 3) {
    val = (int) m_input->readULong(4);
    if (val) f << "g1=" << val << ",";
  }

  m_input->seek(header.m_pos.end()-2, WPX_SEEK_SET);
  int N = (int) m_input->readULong(2);
  MWAWEntry childZone;
  int childId;
  for (int i = 0; i < N; i++) {
    long pos = m_input->tell();
    childId = getEntryPicture(header.m_zoneId, childZone);
    if (childId < 0) {
      MWAW_DEBUG_MSG(("MSKGraph::readGroup: can not find child\n"));
      m_input->seek(pos, WPX_SEEK_SET);
      f << "#child,";
      break;
    }
    group->m_childs.push_back(childId);
  }
  group->m_extra += f.str();
  group->m_pos.setEnd(m_input->tell());
  return group;
}

// read a textbox zone
bool MSKGraph::readText(MSKGraphInternal::TextBox &textBox)
{
  if (textBox.m_numPositions < 0) return false; // can an empty text exist

  libmwaw::DebugStream f;
  f << "Entries(SmallText):";
  long pos = m_input->tell();
  if (!m_mainParser->checkIfPositionValid(pos+4*(textBox.m_numPositions+1))) return false;

  // first read the set of (positions, font)
  f << "pos=[";
  int nbFonts = 0;
  for (int i = 0; i <= textBox.m_numPositions; i++) {
    int fPos = (int) m_input->readLong(2);
    int form = (int) m_input->readLong(2);
    f << fPos << ":" << form << ", ";

    if (fPos < 0 || form < -1) return false;
    if ((form == -1 && i != textBox.m_numPositions) ||
        (i && fPos < textBox.m_positions[(size_t) i-1])) {
      MWAW_DEBUG_MSG(("MSKGraph::readText: find odd positions\n"));
      f << "#";
      continue;
    }

    textBox.m_positions.push_back(fPos);
    textBox.m_formats.push_back(form);
    if (form >= nbFonts) nbFonts = form+1;
  }
  f << "] ";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos = m_input->tell();
  f.str("");
  f << "SmallText:Fonts ";

  // actualPos, -1, only exists if actualPos!= 0 ? We ignored it.
  m_input->readLong(2);
  if (m_input->readLong(2) != -1)
    m_input->seek(pos,WPX_SEEK_SET);
  else {
    ascii().addPos(pos);
    ascii().addNote("SmallText:char Pos");
    pos = m_input->tell();
  }
  f.str("");

  long endFontPos = m_input->tell();
  long sizeOfData = (long) m_input->readULong(4);
  int numFonts = (sizeOfData%0x12 == 0) ? int(sizeOfData/0x12) : 0;

  if (numFonts >= nbFonts) {
    endFontPos = m_input->tell()+4+sizeOfData;

    ascii().addPos(pos);
    ascii().addNote("SmallText: Fonts");

    for (int i = 0; i < numFonts; i++) {
      pos = m_input->tell();
      MSKGraphInternal::Font font;
      if (!readFont(font)) {
        m_input->seek(endFontPos, WPX_SEEK_SET);
        break;
      }
      textBox.m_fontsList.push_back(font);

      f.str("");
      f << "SmallText:Font"<< i
        << "(" << font.m_font.getDebugString(m_convertissor) << "," << font << "),";

      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      pos = m_input->tell();
    }
  }
  int nChar = textBox.m_positions.back()-1;
  if (nbFonts > int(textBox.m_fontsList.size())) {
    MWAW_DEBUG_MSG(("MSKGraph::readText: can not read the fonts\n"));
    ascii().addPos(pos);
    ascii().addNote("SmallText:###");
    m_input->seek(endFontPos,WPX_SEEK_SET);
    textBox.m_fontsList.resize(0);
    textBox.m_positions.resize(0);
    textBox.m_numPositions = 0;
  }

  // now, syntax is : long(size) + size char
  //      - 0x16 - 0 - 0 - Fonts (default fonts)
  //      - 0x08 followed by two long, maybe interesting to look
  //      - 0x0c (or 0x18) seems followed by small int
  //      - nbChar : the strings (final)

  f.str("");
  f << "SmallText:";
  while(1) {
    if (m_input->atEOS()) return false;

    pos = m_input->tell();
    sizeOfData = (long) m_input->readULong(4);
    if (sizeOfData == nChar) {
      bool ok = true;
      // ok we try to read the string
      std::string chaine("");
      for (int i = 0; i < sizeOfData; i++) {
        unsigned char c = (unsigned char)m_input->readULong(1);
        if (c == 0) {
          ok = false;
          break;
        }
        chaine += (char) c;
      }

      if (!ok) {
        m_input->seek(pos+4,WPX_SEEK_SET);
        ok = true;
      } else {
        textBox.m_text = chaine;
        f << "=" << chaine;
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());
        return true;
      }
    }

    if (sizeOfData <= 100+nChar && (sizeOfData%2==0) ) {
      if (m_input->seek(sizeOfData, WPX_SEEK_CUR) != 0) return false;
      f << "#";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      f.str("");
      f << "SmallText:Text";
      continue;
    }

    // fixme: we can try to find the next string
    MWAW_DEBUG_MSG(("MSKGraph::readText: problem reading text\n"));
    f << "#";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());

    return false;
  }
  return true;
}

bool MSKGraph::readTable(MSKGraphInternal::Table &table)
{
  long actPos = m_input->tell();
  libmwaw::DebugStream f, f2;
  f << "Entries(Table): ";

  // first we read the dim
  for (int i = 0; i < 2; i++) {
    std::vector<int> &dim = i==0 ? table.m_rowsDim : table.m_colsDim;
    dim.resize(0);
    int sz = (int) m_input->readLong(4);
    if (i == 0 && sz != 2*table.m_numRows) return false;
    if (i == 1 && sz != 2*table.m_numCols) return false;

    if (i == 0) f << "rowS=(";
    else f << "colS=(";

    for (int j = 0; j < sz/2; j++) {
      int val = (int) m_input->readLong(2);

      if (val < -10) return false;

      dim.push_back(val);
      f << val << ",";
    }
    f << "), ";
  }

  long sz = m_input->readLong(4);
  f << "szOfCells=" << sz;
  ascii().addPos(actPos);
  ascii().addNote(f.str().c_str());

  actPos = m_input->tell();
  long endPos = actPos+sz;
  // now we read the data for each size
  while (m_input->tell() != endPos) {
    f.str("");
    actPos = m_input->tell();
    MSKGraphInternal::Table::Cell cell;
    int y = (int) m_input->readLong(2);
    int x = (int) m_input->readLong(2);
    cell.m_pos = Vec2i(x,y);
    if (x < 0 || y < 0 ||
        x >= table.m_numCols || y >= table.m_numRows) return false;

    f << "Table:("<< cell.m_pos << "):";
    int nbChar = (int) m_input->readLong(1);
    if (nbChar < 0 || actPos+5+nbChar > endPos) return false;

    std::string fName("");
    for (int c = 0; c < nbChar; c++)
      fName +=(char) m_input->readLong(1);

    m_input->seek(actPos+34, WPX_SEEK_SET);
    f << std::hex << "unk=" << m_input->readLong(2) << ", "; // 0|827
    int v = (int) m_input->readLong(2);
    if (v) f << "f0=" << v << ", ";
    int fSize = (int) m_input->readLong(2);
    v = (int) m_input->readLong(2);
    if (v) f2 << "unkn0=" << v << ", ";
    int fFlags = (int) m_input->readLong(2);

    nbChar = (int) m_input->readLong(4);
    if (nbChar <= 0 || m_input->tell()+nbChar > endPos) return false;

    v = (int) m_input->readLong(2);
    if (v) f << "f1=" << v << ", ";
    int fColors = (int) m_input->readLong(2);
    if (fColors != 255)
      f2 << std::dec << "fColorId=" << fColors << ", "; // indexed
    // 0 : invisible, b9: a red, ff : black
    v = (int) m_input->readLong(2);
    if (v) f << "f2=" << v << ", ";
    int bgColors = (int) m_input->readLong(2);
    if (bgColors)
      f2 << std::dec << "bgColorId(?)=" << bgColors << ", "; // indexed

    cell.m_font=MWAWFont(m_convertissor->getId(fName), fSize);
    uint32_t flags = 0;
    if (fFlags & 0x1) flags |= MWAWFont::boldBit;
    if (fFlags & 0x2) flags |= MWAWFont::italicBit;
    if (fFlags & 0x4) cell.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (fFlags & 0x8) flags |= MWAWFont::embossBit;
    if (fFlags & 0x10) flags |= MWAWFont::shadowBit;
    if (fFlags & 0x20) cell.m_font.set(MWAWFont::Script::super());
    if (fFlags & 0x40) cell.m_font.set(MWAWFont::Script::sub());
    cell.m_font.setFlags(flags);

    if (fColors != 0xFF) {
      MWAWColor col(0xD0, 0xD0, 0xD0); // see how to do that
      cell.m_font.setColor(col);
    }
    f << "[" << cell.m_font.getDebugString(m_convertissor) << "," << f2.str()<< "],";
    // check what happens, if the size of text is greater than 4
    for (int c = 0; c < nbChar; c++)
      cell.m_text+=(char) m_input->readLong(1);
    f << cell.m_text;

    table.m_cellsList.push_back(cell);

    ascii().addPos(actPos);
    ascii().addNote(f.str().c_str());
  }

  return true;
}

bool MSKGraph::readChart(MSKGraphInternal::Zone &zone)
{
  long pos = m_input->tell();
  if (version() <= 3)
    return false;
  if (!m_mainParser->checkIfPositionValid(pos+306))
    return false;

  libmwaw::DebugStream f;
  f << "Entries(Chart):";

  int val = (int) m_input->readLong(2);
  switch(val) {
  case 1:
    f << "bar,";
    break;
  case 2:
    f << "stacked,";
    break;
  case 3:
    f << "line,";
    break; // checkme
  case 4:
    f << "combo,";
    break; // checkme
  case 5:
    f << "pie,";
    break; // checkme
  case 6:
    f << "hi-lo-choose,";
    break; // checkme
  default:
    f << "#type=val";
    break;
  }
  for (int i = 0; i < 4; i++) {
    val = (int) m_input->readLong(2);
    if (val) f << "col" << i << "=" << val << ",";
  }
  f << "rows=";
  for (int i = 0; i < 2; i++) {
    val = (int) m_input->readLong(2);
    f << val;
    if (i==0) f << "-";
    else f << ",";
  }
  val =  (int) m_input->readLong(2);
  if (val) f << "colLabels=" << val << ",";
  val =  (int) m_input->readLong(2);
  if (val) f << "rowLabels=" << val << ",";
  std::string name("");
  int sz = (int) m_input->readULong(1);
  if (sz > 31) {
    MWAW_DEBUG_MSG(("MSKGraph::readChart: string size is too long\n"));
    return false;
  }
  for (int i = 0; i < sz; i++) {
    char c = (char) m_input->readLong(1);
    if (!c) break;
    name+=c;
  }
  f << name << ",";
  m_input->seek(pos+50, WPX_SEEK_SET);
  for (int i = 0; i < 128; i++) { // always 0 ?
    val =  (int) m_input->readLong(2);
    if (val) f << "g" << i << "=" << val << std::dec << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos = m_input->tell();
  ascii().addPos(pos);
  ascii().addNote("Chart(II)");
  m_input->seek(2428, WPX_SEEK_CUR);

  // three textbox
  MWAWEntry childZone;
  int childId;
  for (int i = 0; i < 3; i++) {
    pos = m_input->tell();
    childId = getEntryPicture(zone.m_zoneId, childZone);
    if (childId < 0) {
      MWAW_DEBUG_MSG(("MSKGraph::readChart: can not find textbox\n"));
      m_input->seek(pos, WPX_SEEK_SET);
      return false;
    }
    if (childId < int(m_state->m_zonesList.size()))
      m_state->m_zonesList[size_t(childId)]->m_order = i+2;
  }
  // the background picture
  pos = m_input->tell();
  long dataSz = (long) m_input->readULong(4);
  long smDataSz = (long) m_input->readULong(2);
  if (!dataSz || (dataSz&0xFFFF) != smDataSz) {
    MWAW_DEBUG_MSG(("MSKGraph::readChart: last pict size seems odd\n"));
    m_input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  MSKGraphInternal::DataPict *pct  = new MSKGraphInternal::DataPict(zone);
  shared_ptr<MSKGraphInternal::Zone> res(pct);
  pct->m_dataPos = pos+4;
  pct->m_pos.setEnd(pos+4+dataSz);
  ascii().skipZone(pct->m_dataPos, pct->m_pos.end()-1);

  int zId = int(m_state->m_zonesList.size());
  pct->m_fileId = zId;
  pct->m_order = 1;
  m_state->m_zonesList.push_back(res);

  ascii().addPos(pos);
  ascii().addNote("Chart(picture)");
#ifdef DEBUG_WITH_FILES
  WPXBinaryData file;
  m_input->seek(pos+4, WPX_SEEK_SET);
  m_input->readDataBlock(dataSz, file);
  static int volatile chartName = 0;
  libmwaw::DebugStream f2;
  f2 << "Chart-" << ++chartName << ".pct";
  libmwaw::Debug::dumpFile(file, f2.str().c_str());
#endif
  m_input->seek(pos+4+dataSz, WPX_SEEK_SET);

  // last the value ( by columns ? )
  for (int i = 0; i < 4; i++) {
    pos = m_input->tell();
    dataSz = (long) m_input->readULong(4);
    if (dataSz%0x10) {
      MWAW_DEBUG_MSG(("MSKGraph::readChart: can not read end last zone\n"));
      m_input->seek(pos, WPX_SEEK_SET);
      return false;
    }
    f.str("");
    f << "Chart(A" << i << ")";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    int numLine = int(dataSz/0x10);
    for (int l = 0; l < numLine; l++) {
      f.str("");
      f << "Chart(A" << i << "-" << l << ")";
      ascii().addPos(pos+4+0x10*l);
      ascii().addNote(f.str().c_str());
    }
    m_input->seek(pos+4+dataSz, WPX_SEEK_SET);
  }
  pos = m_input->tell();
  return true;
}

void MSKGraph::sendTextBox(int zoneId)
{
  if (!m_listener) return;
  if (zoneId < 0 || zoneId >= int(m_state->m_zonesList.size())) {
    MWAW_DEBUG_MSG(("MSKGraph::sendTextBox: can not find textbox %d\n", zoneId));
    return;
  }
  shared_ptr<MSKGraphInternal::Zone> zone = m_state->m_zonesList[(size_t)zoneId];
  if (!zone) return;
  MSKGraphInternal::TextBox &textBox = reinterpret_cast<MSKGraphInternal::TextBox &>(*zone);
  MSKGraphInternal::Font actFont;
  actFont.m_font = MWAWFont(20,12);
  setProperty(actFont);
  m_listener->setParagraphJustification(textBox.m_justify);
  int numFonts = int(textBox.m_fontsList.size());
  int actFormatPos = 0;
  int numFormats = int(textBox.m_formats.size());
  if (numFormats != int(textBox.m_positions.size())) {
    MWAW_DEBUG_MSG(("MSKGraph::sendTextBox: positions and formats have different length\n"));
    if (numFormats > int(textBox.m_positions.size()))
      numFormats = int(textBox.m_positions.size());
  }
  for (size_t i = 0; i < textBox.m_text.length(); i++) {
    if (actFormatPos < numFormats && textBox.m_positions[(size_t)actFormatPos]==int(i)) {
      int id = textBox.m_formats[(size_t)actFormatPos++];
      if (id < 0 || id >= numFonts) {
        MWAW_DEBUG_MSG(("MSKGraph::sendTextBox: can not find a font\n"));
      } else {
        actFont = textBox.m_fontsList[(size_t)id];
        setProperty(actFont);
      }
    }
    unsigned char c = (unsigned char) textBox.m_text[i];
    switch(c) {
    case 0x9:
      MWAW_DEBUG_MSG(("MSKGraph::sendTextBox: find some tab\n"));
      m_listener->insertCharacter(' ');
      break;
    case 0xd:
      m_listener->insertEOL();
      break;
    case 0x19:
      m_listener->insertField(MWAWContentListener::Title);
      break;
    case 0x18:
      m_listener->insertField(MWAWContentListener::PageNumber);
      break;
    case 0x16:
      MWAW_DEBUG_MSG(("MSKGraph::sendTextBox: find some time\n"));
      m_listener->insertField(MWAWContentListener::Time);
      break;
    case 0x17:
      MWAW_DEBUG_MSG(("MSKGraph::sendTextBox: find some date\n"));
      m_listener->insertField(MWAWContentListener::Date);
      break;
    case 0x14: // fixme
      MWAW_DEBUG_MSG(("MSKGraph::sendTextBox: footnote are not implemented\n"));
      break;
    default:
      if (c <= 0x1f) {
        MWAW_DEBUG_MSG(("MSKGraph::sendTextBox: find char=%x\n",int(c)));
      } else {
        int unicode = m_convertissor->unicode (actFont.m_font.id(), c);
        if (unicode == -1)
          m_listener->insertCharacter(c);
        else
          m_listener->insertUnicode((uint32_t)unicode);
      }
      break;
    }
  }
}

void MSKGraph::sendTable(int zoneId)
{
  if (!m_listener) return;

  if (zoneId < 0 || zoneId >= int(m_state->m_zonesList.size())) {
    MWAW_DEBUG_MSG(("MSKGraph::sendTable: can not find textbox %d\n", zoneId));
    return;
  }
  shared_ptr<MSKGraphInternal::Zone> zone = m_state->m_zonesList[(size_t)zoneId];
  if (!zone) return;
  MSKGraphInternal::Table &table = reinterpret_cast<MSKGraphInternal::Table &>(*zone);

  // open the table
  size_t nCols = table.m_colsDim.size();
  size_t nRows = table.m_rowsDim.size();
  if (!nCols || !nRows) {
    MWAW_DEBUG_MSG(("MSKGraph::sendTable: problem with dimensions\n"));
    return;
  }
  std::vector<float> colsDims(nCols);
  for (size_t c = 0; c < nCols; c++) colsDims[c] = float(table.m_colsDim[c]);
  m_listener->openTable(colsDims, WPX_POINT);

  MWAWFont actFont;
  int const borderPos = MWAWBorder::TopBit | MWAWBorder::RightBit |
                        MWAWBorder::BottomBit | MWAWBorder::LeftBit;
  MWAWBorder border;
  for (size_t row = 0; row < nRows; row++) {
    m_listener->openTableRow(float(table.m_rowsDim[row]), WPX_POINT);

    for (size_t col = 0; col < nCols; col++) {
      WPXPropertyList emptyList;
      MWAWCell cell;
      Vec2i cellPosition(Vec2i((int)row,(int)col));
      cell.setPosition(cellPosition);
      cell.setBorders(borderPos, border);
      // fixme setBackgroundColor
      m_listener->setParagraphJustification(libmwaw::JustificationCenter);
      m_listener->openTableCell(cell, emptyList);

      MSKGraphInternal::Table::Cell const *tCell=table.getCell(cellPosition);
      if (tCell) {
        tCell->m_font.sendTo(m_listener.get(), m_convertissor, actFont);
        size_t nChar = tCell->m_text.size();
        for (size_t ch = 0; ch < nChar; ch++) {
          unsigned char c = (unsigned char) tCell->m_text[ch];
          switch(c) {
          case 0x9:
            MWAW_DEBUG_MSG(("MSKGraph::sendTable: find a tab\n"));
            m_listener->insertCharacter(' ');
            break;
          case 0xd:
            m_listener->insertEOL();
            break;
          default:
            if (c <= 0x1f) {
              MWAW_DEBUG_MSG(("MSKGraph::sendTable: find char=%x\n",int(c)));
            } else {
              int unicode = m_convertissor->unicode (actFont.id(), c);
              if (unicode == -1)
                m_listener->insertCharacter(c);
              else
                m_listener->insertUnicode((uint32_t)unicode);
            }
            break;
          }
        }
      }

      m_listener->closeTableCell();
    }
    m_listener->closeTableRow();
  }

  // close the table
  m_listener->closeTable();
}

void MSKGraph::setProperty(MSKGraphInternal::Font const &font)
{
  if (!m_listener) return;
  font.m_font.sendTo(m_listener.get(), m_convertissor, m_state->m_font);
}

bool MSKGraph::readFont(MSKGraphInternal::Font &font)
{
  long pos = m_input->tell();
  libmwaw::DebugStream f;
  if (!m_mainParser->checkIfPositionValid(pos+18))
    return false;
  font = MSKGraphInternal::Font();
  for (int i = 0; i < 3; i++)
    font.m_flags[i] = (int) m_input->readLong(2);
  font.m_font.setFont((int) m_input->readULong(2));
  int flags = (int) m_input->readULong(1);
  uint32_t flag = 0;
  if (flags & 0x1) flag |= MWAWFont::boldBit;
  if (flags & 0x2) flag |= MWAWFont::italicBit;
  if (flags & 0x4) font.m_font.setUnderlineStyle(MWAWFont::Line::Simple);
  if (flags & 0x8) flag |= MWAWFont::embossBit;
  if (flags & 0x10) flag |= MWAWFont::shadowBit;
  if (flags & 0x20) font.m_font.set(MWAWFont::Script::super());
  if (flags & 0x40) font.m_font.set(MWAWFont::Script::sub());
  if (flags & 0x80) f << "#smaller,";
  font.m_font.setFlags(flag);

  int val = (int) m_input->readULong(1);
  if (val) f << "#flags2=" << val << ",";
  font.m_font.setSize((int) m_input->readULong(2));

  unsigned char color[3];
  for (int i = 0; i < 3; i++) color[i] = (unsigned char) (m_input->readULong(2)>>8);
  font.m_font.setColor(MWAWColor(color[0],color[1],color[2]));
  font.m_extra = f.str();
  return true;
}

void MSKGraph::send(int id, MWAWPosition::AnchorTo anchor)
{
  if (id < 0 || id >= int(m_state->m_zonesList.size())) {
    MWAW_DEBUG_MSG(("MSKGraph::send: can not find zone %d\n", id));
    return;
  }
  if (!m_listener) return;
  shared_ptr<MSKGraphInternal::Zone> zone = m_state->m_zonesList[(size_t)id];
  zone->m_isSent = true;

  MWAWPosition pictPos = zone->getPosition(anchor);
  if (anchor == MWAWPosition::Page)
    pictPos.setOrigin(pictPos.origin()+72.*m_mainParser->getPageTopLeft());
  WPXPropertyList extras;
  zone->fillFramePropertyList(extras);

  switch (zone->type()) {
  case MSKGraphInternal::Zone::Text: {
    shared_ptr<MSKGraphInternal::SubDocument> subdoc
    (new MSKGraphInternal::SubDocument(*this, m_input, MSKGraphInternal::SubDocument::TextBox, id));
    m_listener->insertTextBox(pictPos, subdoc, extras);
    return;
  }
  case MSKGraphInternal::Zone::TableZone: {
    shared_ptr<MSKGraphInternal::SubDocument> subdoc
    (new MSKGraphInternal::SubDocument(*this, m_input, MSKGraphInternal::SubDocument::Table, id));
    m_listener->insertTextBox(pictPos, subdoc, extras);
    return;
  }
  case MSKGraphInternal::Zone::Group:
    return;
  case MSKGraphInternal::Zone::Bitmap: {
    MSKGraphInternal::DataBitmap &bmap = reinterpret_cast<MSKGraphInternal::DataBitmap &>(*zone);
    WPXBinaryData data;
    std::string type;
    if (!bmap.getPictureData(m_input, data,type,m_mainParser->getPalette(4)))
      break;
    ascii().skipZone(bmap.m_dataPos, bmap.m_pos.end()-1);
    m_listener->insertPicture(pictPos, data, type, extras);
    return;
  }
  case MSKGraphInternal::Zone::Basic:
  case MSKGraphInternal::Zone::Pict: {
    WPXBinaryData data;
    std::string type;
    if (!zone->getBinaryData(m_input, data,type))
      break;
    m_listener->insertPicture(pictPos, data, type, extras);
    return;
  }
  case MSKGraphInternal::Zone::Textv4: {
    MSKGraphInternal::TextBoxv4 &textbox = reinterpret_cast<MSKGraphInternal::TextBoxv4 &>(*zone);
    shared_ptr<MSKGraphInternal::SubDocument> subdoc
    (new MSKGraphInternal::SubDocument(*this, m_input, MSKGraphInternal::SubDocument::TextBoxv4, textbox.m_text, textbox.m_frame));
    WPXPropertyList textboxExtra;
    if (zone->m_ids[1] > 0) {
      WPXString fName;
      fName.sprintf("Frame%ld", zone->m_ids[0]);
      extras.insert("libwpd:frame-name",fName);
    }
    if (zone->m_ids[2] > 0) {
      WPXString fName;
      fName.sprintf("Frame%ld", zone->m_ids[2]);
      textboxExtra.insert("libwpd:next-frame-name",fName);
    }
    m_listener->insertTextBox(pictPos, subdoc, extras, textboxExtra);
    return;
  }
  case MSKGraphInternal::Zone::OLE: {
    MSKGraphInternal::OLEZone &ole = reinterpret_cast<MSKGraphInternal::OLEZone &>(*zone);
    m_mainParser->sendOLE(ole.m_oleId, pictPos, extras);
    return;
  }
  case MSKGraphInternal::Zone::Unknown:
  default:
    break;
  }

  MWAW_DEBUG_MSG(("MSKGraph::send: can not send zone %d\n", id));
}

void MSKGraph::sendAll(int zoneId, bool mainZone)
{
  for (size_t i = 0; i < m_state->m_zonesList.size(); i++) {
    shared_ptr<MSKGraphInternal::Zone> zone = m_state->m_zonesList[i];
    if (zoneId >= 0 && zoneId!=zone->m_zoneId)
      continue;
    send(int(i), mainZone ? MWAWPosition::Page : MWAWPosition::Paragraph);
  }
}

void MSKGraph::sendObjects(MSKGraph::SendData what)
{
  if (!m_listener) {
    MWAW_DEBUG_MSG(("MSKGraph::sendObjects: listener is not set\n"));
    return;
  }

  bool first = true;
  int numZones = int(m_state->m_zonesList.size());
  std::vector<int> listIds;
  MSKGraphInternal::RBZone *rbZone=0;
  switch(what.m_type) {
  case MSKGraph::SendData::ALL: {
    listIds.resize(size_t(numZones));
    for (int i = 0; i < numZones; i++) listIds[size_t(i)]=i;
    break;
  }
  case MSKGraph::SendData::RBDR:
  case MSKGraph::SendData::RBIL: {
    int zId = what.m_type==MSKGraph::SendData::RBDR ? -1 : what.m_id;
    if (m_state->m_RBsMap.find(zId)!=m_state->m_RBsMap.end())
      rbZone = &m_state->m_RBsMap.find(zId)->second;
    break;
  }
  default:
    break;
  }
  if (rbZone)
    listIds=rbZone->m_idList;
  if (what.m_type==MSKGraph::SendData::RBIL) {
    if (!rbZone) {
      MWAW_DEBUG_MSG(("MSKGraph::sendObjects: can find RBIL zone %d\n", what.m_id));
      return;
    }
    if (listIds.size() != 1) {
      if (what.m_anchor == MWAWPosition::Char ||
          what.m_anchor == MWAWPosition::CharBaseLine) {
        shared_ptr<MSKGraphInternal::SubDocument> subdoc
        (new MSKGraphInternal::SubDocument(*this, m_input, MSKGraphInternal::SubDocument::RBILZone, what.m_id));
        MWAWPosition pictPos(Vec2f(0,0), what.m_size, WPX_POINT);;
        pictPos.setRelativePosition(MWAWPosition::Char,
                                    MWAWPosition::XLeft, MWAWPosition::YTop);
        pictPos.m_wrapping =  MWAWPosition::WBackground;
        m_listener->insertTextBox(pictPos, subdoc);
        return;
      }
    }
  }
  for (size_t i = 0; i < listIds.size(); i++) {
    int id = listIds[i];
    if (id < 0 || id >= numZones) continue;
    shared_ptr<MSKGraphInternal::Zone> zone = m_state->m_zonesList[size_t(id)];
    if (zone->m_isSent) {
      if (what.m_type == MSKGraph::SendData::ALL ||
          what.m_anchor == MWAWPosition::Page) continue;
    }
    if (what.m_anchor == MWAWPosition::Page) {
      if (what.m_page > 0 && zone->m_page != what.m_page) continue;
      else if (what.m_page==0 && zone->m_page <= 0) continue;
    }

    int oldPage = zone->m_page;
    if (zone->m_page > 0) zone->m_page--;
    if (first) {
      first = false;
      if (what.m_anchor == MWAWPosition::Page && (!m_listener->isSectionOpened() && !m_listener->isParagraphOpened()))
        m_listener->insertCharacter(' ');
    }
    send(int(id), what.m_anchor);
    zone->m_page = oldPage;
  }
}

void MSKGraph::flushExtra()
{
  for (size_t i = 0; i < m_state->m_zonesList.size(); i++) {
    shared_ptr<MSKGraphInternal::Zone> zone = m_state->m_zonesList[i];
    if (zone->m_isSent) continue;
    send(int(i), MWAWPosition::Char);
  }
}

////////////////////////////////////////////////////////////
// basic function
////////////////////////////////////////////////////////////
MSKGraph::SendData::SendData() : m_type(RBDR), m_id(-1), m_anchor(MWAWPosition::Char), m_page(-1), m_size()
{
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
