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

#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>

#include <libwpd/WPXBinaryData.h>
#include <libwpd/WPXString.h>

#include "TMWAWPictBasic.hxx"
#include "TMWAWPictBitmap.hxx"
#include "TMWAWPictMac.hxx"
#include "TMWAWPosition.hxx"

#include "MWAWStruct.hxx"
#include "MWAWTools.hxx"
#include "MWAWContentListener.hxx"

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
  MWAWStruct::Font m_font;
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
      1.0, 0.9, 0.7, 0.5, 0.7, 0.5, 0.7, 0.3, 0.7, 0.5,
      0.4, 0.3, 0.1, 0.25, 0.25, 0.5, 0.5, 0.2, 0.5, 0. /* empty */,
      0.1, 0.2, 0.4, 0.3, 0.5, 0.3, 0.3, 0.25, 0.25, 0.25,
      0.2, 0.3, 0.2, 0.3, 0.3, 0.3, 0.6, 0.4, 0. /* no */
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
//! Internal: the generic pict
struct Zone {
  enum Type { Unknown, Basic, Group, Pict, Text};
  //! constructor
  Zone() : m_subType(-1), m_pos(), m_dataPos(-1), m_fileId(-1), m_id(-1), m_page(-1), m_decal(), m_box(), m_line(-1),
    m_lineWidth(2), m_lineColor(0,0,0), m_linePattern(Pattern::P_Percent, 1.0), m_lineFlags(0),
    m_surfaceColor(255,255,255), m_surfacePattern(Pattern::P_None),m_extra(""), m_isSent(false) {
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
  virtual bool getBinaryData(TMWAWInputStreamPtr,
                             WPXBinaryData &res, std::string &type) const {
    res.clear();
    type="";
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
      x *= -1.0;
      min+=Vec2f(x,0);
    }
    if (y < 0) {
      y *= -1.0;
      min+=Vec2f(0,y);
    }
    Box2f res(min, min+Vec2f(x,y));
    float bExtra = needExtraBorderWidth();
    if (bExtra) res.extend(2.0*bExtra);
    return res;
  }

  TMWAWPosition getPosition(bool local) const {
    TMWAWPosition res;
    Box2f box = getLocalBox();
    if (local || m_page < 0) {
      res = TMWAWPosition(Vec2f(0,0), box.size(), WPX_POINT);
      res.setRelativePosition(TMWAWPosition::Char, TMWAWPosition::XLeft, TMWAWPosition::YTop);
    } else {
      res = TMWAWPosition(box.min()+m_decal, box.size(), WPX_POINT);
      res.setRelativePosition(TMWAWPosition::Page);
      res.setPage(m_page+1);
      res.m_wrapping =  TMWAWPosition::WRunThrough;
    }
    return res;
  }

  //! the virtual print function
  virtual void print(std::ostream &o) const;

  //! return a string #rrggbb to code a color
  static std::string getColorString(Vec3uc const &col) {
    std::stringstream s;
    s << std::hex << std::setfill('0') << "#"
      << std::setw(2) << int(col[0])
      << std::setw(2) << int(col[1])
      << std::setw(2) << int(col[2]);
    return s.str();
  }

  //! the type
  int m_subType;
  //! the file position
  IMWAWEntry m_pos;
  //! the data begin position
  long m_dataPos;
  //! the file id
  int m_fileId;
  //! the pict id
  int m_id;
  //! the page
  int m_page;
  //! the local position
  Vec2f m_decal;
  //! local bdbox
  Box2f m_box;
  //! the line position(v1)
  int m_line;
  //! the line width : 0= dotted, 1=half, 2=1, 3=2pt?, 4:4pt
  int m_lineWidth;
  //! the line color
  Vec3uc m_lineColor;
  //! the line pattern
  Pattern m_linePattern;
  //! the line flag
  int m_lineFlags;
  //! the 2D surface color
  Vec3uc m_surfaceColor;
  //! the line pattern
  Pattern m_surfacePattern;
  //! extra data
  std::string m_extra;
  //! true if the zone is send
  bool m_isSent;
};

void Zone::print(std::ostream &o) const
{
  if (m_fileId >= 0) o << "P" << m_fileId << ",";
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
  if (m_id > 0) o << "id=" << std::hex << m_id << std::dec << ",";
  if (m_page>=0) o << "page=" << m_page << ",";
  if (m_decal.x() || m_decal.y()) o << "pos=" << m_decal << ",";
  o << "bdbox=" << m_box << ",";
  switch(m_lineWidth) {
  case 0:
    o << "line=dotted,";
    break;
  case 1:
    o << "lineWidth=1/2pt,";
    break;
  case 2:
    break;
  case 3:
    o << "lineWidth=2pt,";
    break;
  case 4:
    o << "lineWidth=4pt,";
    break;
  default:
    o << "#lineWidth=" << m_lineWidth << ",";
    break;
  }
  if (m_linePattern.m_type != Pattern::P_Percent || m_linePattern.m_filled != 1.0)
    o << "linePattern=[" << m_linePattern << "],";
  if (m_lineColor[0] || m_lineColor[0]|| m_lineColor[2])
    o << "lineColor=" << int(m_lineColor[0]) << "x"
      << int(m_lineColor[1]) << "x" << int(m_lineColor[2]) << ",";
  if (m_surfaceColor[0]!=255 || m_surfaceColor[0]!=255|| m_surfaceColor[2]!=255)
    o << "surfaceColor=" << int(m_surfaceColor[0]) << "x"
      << int(m_surfaceColor[1]) << "x" << int(m_surfaceColor[2]) << ",";
  if (m_surfacePattern.hasPattern())
    o << "surfacePattern=[" << m_surfacePattern << "],";
  /* linePattern: 38: none, 19: white, 25: diagonal(gray),
     0: black, 13: ~gray10, 26: horizontal
  */
  if (m_line >= 0) o << "line=" << m_line << ",";
  if (m_lineFlags&1) o << "endArrow,";
  if (m_lineFlags& 0xFE)
    o << "#lineFlags=" << std::hex << int(m_lineFlags&0xFE) << std::dec << ",";
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
    for (int i = 0; i < int(m_childs.size()); i++)
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
      for (int i = 0; i < int(m_vertices.size()); i++)
        o << m_vertices[i] << ",";
      o << "],";
    }
  }

  virtual float needExtraBorderWidth() const {
    switch(m_lineWidth) {
    case 2:
      return 1.0;
    case 3:
      return 1.5;
    case 4:
      return 2.5;
    default:
      return 0.0;
    }
  }

  virtual bool getBinaryData(TMWAWInputStreamPtr,
                             WPXBinaryData &res, std::string &type) const;

  //! the form bdbox ( used by arc )
  Box2i m_formBox;

  //! the angle ( used by arc )
  int m_angle, m_deltaAngle /** the delta angle */;
  //! the list of vertices ( used by polygon)
  std::vector<Vec2f> m_vertices;
};

bool BasicForm::getBinaryData(TMWAWInputStreamPtr,
                              WPXBinaryData &data, std::string &type) const
{
  data.clear();
  type="";
  shared_ptr<libmwaw_tools::Pict> pict;
  float lineW = 1.0;
  switch(m_lineWidth) {
  case 0: // fixme dotted
  case 1:
    lineW = 0.5;
    break;
  case 2:
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
  Vec3uc lineColor(0,0,0);
  bool hasLineColor = false;
  if (m_linePattern.hasPattern()) {
    lineColor = m_linePattern.m_filled*m_lineColor + (1-m_linePattern.m_filled)*m_surfaceColor;
    hasLineColor = true;
  } else if (m_linePattern.m_type == MSKGraphInternal::Pattern::P_None)
    lineW = 0.;
  bool hasSurfaceColor = false;
  Vec3uc surfaceColor(255,255,255);
  if (m_surfacePattern.hasPattern()) {
    surfaceColor = m_surfacePattern.m_filled*m_surfaceColor + (1-m_surfacePattern.m_filled)*m_lineColor;
    hasSurfaceColor = true;
  }

  switch(m_subType) {
  case 0: {
    libmwaw_tools::PictLine *pct=new libmwaw_tools::PictLine(m_box.min(), m_box.max());
    if (m_lineFlags & 1) pct->setArrow(1, true);
    if (lineW != 1.0) pct->setLineWidth(lineW);
    if (hasLineColor) pct->setLineColor(lineColor[0], lineColor[1], lineColor[2]);
    pict.reset(pct);
    break;
  }
  case 1: {
    libmwaw_tools::PictRectangle *pct=new libmwaw_tools::PictRectangle(m_box);
    if (lineW != 1.0) pct->setLineWidth(lineW);
    if (hasLineColor) pct->setLineColor(lineColor[0], lineColor[1], lineColor[2]);
    if (hasSurfaceColor) pct->setSurfaceColor(surfaceColor[0], surfaceColor[1], surfaceColor[2]);
    pict.reset(pct);
    break;
  }
  case 2: {
    libmwaw_tools::PictRectangle *pct=new libmwaw_tools::PictRectangle(m_box);
    int sz = 10;
    if (m_box.size().x() > 0 && m_box.size().x() < 2*sz)
      sz = int(m_box.size().x())/2;
    if (m_box.size().y() > 0 && m_box.size().y() < 2*sz)
      sz = int(m_box.size().y())/2;
    pct->setRoundCornerWidth(sz);
    if (lineW != 1.0) pct->setLineWidth(lineW);
    if (hasLineColor) pct->setLineColor(lineColor[0], lineColor[1], lineColor[2]);
    if (hasSurfaceColor) pct->setSurfaceColor(surfaceColor[0], surfaceColor[1], surfaceColor[2]);
    pict.reset(pct);
    break;
  }
  case 3: {
    libmwaw_tools::PictCircle *pct=new libmwaw_tools::PictCircle(m_box);
    if (lineW != 1.0) pct->setLineWidth(lineW);
    if (hasLineColor) pct->setLineColor(lineColor[0], lineColor[1], lineColor[2]);
    if (hasSurfaceColor) pct->setSurfaceColor(surfaceColor[0], surfaceColor[1], surfaceColor[2]);
    pict.reset(pct);
    break;
  }
  case 4: {
    int angl2 = m_angle+((m_deltaAngle>0) ? m_deltaAngle : -m_deltaAngle);
    libmwaw_tools::PictArc *pct=new libmwaw_tools::PictArc(m_box, m_formBox, 450-angl2, 450-m_angle);
    if (lineW != 1.0) pct->setLineWidth(lineW);
    if (hasLineColor) pct->setLineColor(lineColor[0], lineColor[1], lineColor[2]);
    if (hasSurfaceColor) pct->setSurfaceColor(surfaceColor[0], surfaceColor[1], surfaceColor[2]);
    pict.reset(pct);
    break;
  }
  case 5: {
    libmwaw_tools::PictPolygon *pct = new libmwaw_tools::PictPolygon(m_box, m_vertices);
    if (lineW != 1.0) pct->setLineWidth(lineW);
    if (hasLineColor) pct->setLineColor(lineColor[0], lineColor[1], lineColor[2]);
    if (hasSurfaceColor) pct->setSurfaceColor(surfaceColor[0], surfaceColor[1], surfaceColor[2]);
    pict.reset(pct);
    break;
  }
  default:
    MWAW_DEBUG_MSG(("MSKGraphInternal::FormPict::getBinaryData: find unknown type\n"));
    break;
  }
  if (!pict) return false;

  return pict->getBinary(data,type);
}

////////////////////////////////////////
//! Internal: the picture of a MSKGraph
struct DataPict : public Zone {
  //! constructor
  DataPict(Zone const &z) : Zone(z), m_naturalBox() { }
  //! empty constructor
  DataPict() : Zone(), m_naturalBox() { }

  //! return the type
  virtual Type type() const {
    return Pict;
  }
  virtual bool getBinaryData(TMWAWInputStreamPtr ip,
                             WPXBinaryData &res, std::string &type) const;

  //! operator<<
  virtual void print(std::ostream &o) const {
    Zone::print(o);
  }
  //! the pict box (if known )
  mutable Box2f m_naturalBox;
};

bool DataPict::getBinaryData(TMWAWInputStreamPtr ip,
                             WPXBinaryData &data, std::string &type) const
{
  data.clear();
  type="";
  long pictSize = m_pos.end()-m_dataPos;
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
    libmwaw_tools::DebugStream f;
    f << "Pict-" << ++pictName << ".pct";
    libmwaw_tools::Debug::dumpFile(file, f.str().c_str());
  }
#endif

  ip->seek(m_dataPos, WPX_SEEK_SET);
  libmwaw_tools::Pict::ReadResult res =
    libmwaw_tools::PictData::check(ip, pictSize, m_naturalBox);
  if (res == libmwaw_tools::Pict::MWAW_R_BAD) {
    MWAW_DEBUG_MSG(("MSKGraphInternal::DataPict::getBinaryData: can not find the picture\n"));
    return false;
  }

  ip->seek(m_dataPos, WPX_SEEK_SET);
  shared_ptr<libmwaw_tools::Pict> pict(libmwaw_tools::PictData::get(ip, pictSize));

  if (!pict)
    return false;

  return pict->getBinary(data,type);
}

////////////////////////////////////////
//! Internal: the picture of a MSKGraph
struct TextBox : public Zone {
  //! constructor
  TextBox(Zone const &z) : Zone(z), m_numPositions(-1), m_fontsList(), m_positions(), m_formats(), m_text(""), m_sentPhase(0)
  { }

  //! return the type
  virtual Type type() const {
    return Text;
  }
  //! operator<<
  virtual void print(std::ostream &o) const {
    Zone::print(o);
  }

  //! add frame parameters to propList (if needed )
  virtual void fillFramePropertyList(WPXPropertyList &extras) const {
    if (m_surfaceColor[0]!=255 || m_surfaceColor[1]!=255 || m_surfaceColor[2]!=255)
      extras.insert("fo:background-color", getColorString(m_surfaceColor).c_str());
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
  //! 0 if not sent, 1 if the frame is already sent (and we write the text)
  bool m_sentPhase;
};

////////////////////////////////////////
//! Internal: the state of a MSKGraph
struct State {
  //! constructor
  State() : m_version(-1), m_zonesList(), m_font(20,12), m_numPages(0) { }

  //! the version
  int m_version;

  //! the list of zone
  std::vector<shared_ptr<Zone> > m_zonesList;

  //! the actual font
  MWAWStruct::Font m_font;

  //! the number of pages
  int m_numPages;
};
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MSKGraph::MSKGraph
(TMWAWInputStreamPtr ip, MSKParser &parser, MWAWTools::ConvertissorPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert), m_state(new MSKGraphInternal::State),
  m_mainParser(&parser), m_asciiFile(parser.ascii())
{
}

MSKGraph::~MSKGraph()
{ }

int MSKGraph::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_mainParser->version();
  return m_state->m_version;
}

int MSKGraph::numPages() const
{
  if (m_state->m_numPages > 0)
    return m_state->m_numPages;

  int maxPage = 0;
  int numZones = m_state->m_zonesList.size();
  for (int i = 0; i < numZones; i++) {
    shared_ptr<MSKGraphInternal::Zone> zone = m_state->m_zonesList[i];
    if (zone->m_page > maxPage)
      maxPage = zone->m_page;
  }
  m_state->m_numPages = maxPage+1;
  return m_state->m_numPages;
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
  pict.m_subType = m_input->readULong(1);
  if (pict.m_subType > 0x10 || pict.m_subType == 6 ||
      (pict.m_subType >= 0xa && pict.m_subType < 0xd))
    return false;
  int vers = version();

  libmwaw_tools::DebugStream f;
  int val;
  if (vers <= 2) {
    for (int i = 0; i < 2; i++) {
      int cId = m_input->readLong(2)+1;
      Vec3uc col;
      if (m_mainParser->getColor(cId,col)) {
        if (i) pict.m_surfaceColor = col;
        else pict.m_lineColor = col;
      } else
        f << "#col" << i << "=" << cId-1 << ",";
    }
    for (int i = 0; i < 2; i++) {
      int pId = m_input->readLong(2);
      float percent = MSKGraphInternal::Pattern::getPercentV2(pId);
      MSKGraphInternal::Pattern::Type type = pId == 38 ?
                                             MSKGraphInternal::Pattern::P_None :
                                             MSKGraphInternal::Pattern::P_Percent;
      if (i) pict.m_surfacePattern = MSKGraphInternal::Pattern(type, percent);
      else pict.m_linePattern = MSKGraphInternal::Pattern(type, percent);
    }
    pict.m_lineWidth=m_input->readLong(2);
  } else {
    pict.m_page = m_input->readLong(2);
    f << "unkn=[";
    for (int i = 0; i < 11; i++) {  // fl[9] == pensize ?
      val = m_input->readLong(2);
      if (val)
        f << val << ",";
      else
        f << "_,";
    }
    f << "],";
  }

  int offset[4];
  for (int i = 0; i < 4; i++)
    offset[i] = m_input->readLong(2);
  pict.m_decal = Vec2f(offset[0]+offset[3], offset[1]+offset[2]);

  // the two point which allows to create the form ( in general the bdbox)
  float dim[4];
  for (int i = 0; i < 4; i++)
    dim[i] = m_input->readLong(4)/65536.;
  pict.m_box=Box2f(Vec2f(dim[0],dim[1]),Vec2f(dim[2],dim[3]));

  int flags = m_input->readLong(1);
  if (vers >= 4 && flags==2) {
    // 2: rotations, 0: nothing, other ?
    f << ", Rot=[";
    f << m_input->readLong(2) << ",";
    for (int i = 0; i < 31; i++)
      f << m_input->readLong(2) << ",";
    f << "]";
  } else if (flags) f << "fl0=" << flags << ",";
  pict.m_lineFlags = m_input->readULong(1);
  if (vers >= 3) pict.m_id = m_input->readULong(4);
  pict.m_extra = f.str();
  pict.m_dataPos = m_input->tell();
  return true;
}

int MSKGraph::getEntryPicture(IMWAWEntry &zone)
{
  int zId = -1;
  MSKGraphInternal::Zone pict;
  long pos = m_input->tell();

  if (!readPictHeader(pict))
    return zId;

  pict.m_pos.setBegin(pos);
  libmwaw_tools::DebugStream f;
  int vers = version();
  long debData = m_input->tell();
  int dataSize = 0, versSize = 0;
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
    int N = m_input->readULong(2);
    dataSize = 9+N*8;
    break;
  }
  case 7: { // picture
    if (vers >= 3) versSize = 0x14;
    dataSize = 5;
    m_input->seek(debData+5+versSize-2, WPX_SEEK_SET);
    dataSize += m_input->readULong(2);
    break;
  }
  case 8: // group
    if (vers >= 3) versSize = 4;
    dataSize = 0x1b;
    break;
  case 9:
    dataSize = 0x21;
    if (vers >= 3) dataSize += 0x10;
    break;
  case 0xd: { // bitmap v4
    m_input->seek(debData+0x29, WPX_SEEK_SET);
    long sz = m_input->readULong(4);
    dataSize = 0x29+4+sz;
    break;
  }
  case 0xe: { // spreadsheet v4
    m_input->seek(debData+0xa7, WPX_SEEK_SET);
    int pSize = m_input->readULong(2);
    if (pSize == 0) return zId;
    dataSize = 0xa9+pSize;
    if (!m_mainParser->checkIfPositionValid(debData+dataSize))
      return zId;

    m_input->seek(debData+dataSize, WPX_SEEK_SET);
    for (int i = 0; i < 2; i++)
      dataSize += 4+m_input->readULong(4);
    break;
  }
  case 0xf: { // textbox v4
    m_input->seek(debData+0x39, WPX_SEEK_SET);
    dataSize = 0x3b+ m_input->readULong(2);
    break;
  }
  case 0x10: { // table v4
    m_input->seek(debData+0x57, WPX_SEEK_SET);
    dataSize = 0x59+ m_input->readULong(2);
    m_input->seek(debData+dataSize, WPX_SEEK_SET);

    for (int i = 0; i < 3; i++)
      dataSize += 4 + m_input->readULong(4);

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
      long ptr = m_input->readULong(4);
      f << std::hex << "ptr2=" << ptr << std::dec << ",";
      f << "depth?=" << m_input->readLong(1) << ",";
      float dim[4];
      for (int i = 0; i < 4; i++)
        dim[i] = m_input->readLong(4)/65536.;
      Box2f box(Vec2f(dim[1], dim[0]), Vec2f(dim[3], dim[2]));
      f << "bdbox=" << box << ",";
      break;
    }
    default:
      break;
    }
  }
  int val = m_input->readLong(1); // 0 and sometimes -1
  if (val) f << "g0=" << val << ",";
  pict.m_dataPos++;

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
    form->m_angle = m_input->readLong(2);
    form->m_deltaAngle = m_input->readLong(2);
    int dim[4]; // real Bdbox
    for (int i = 0; i < 4; i++)
      dim[i] = m_input->readLong(2);
    form->m_formBox = form->m_box;
    form->m_box = Box2i(Vec2i(dim[1],dim[0]), Vec2i(dim[3],dim[2]));
    break;
  }
  case 5: {
    MSKGraphInternal::BasicForm *form  = new MSKGraphInternal::BasicForm(pict);
    res.reset(form);
    val = m_input->readULong(2);
    if (val) f << "g1=" << val << ",";
    int numPt = m_input->readLong(2);
    long ptr = m_input->readULong(4);
    f << std::hex << "ptr2=" << ptr << std::dec << ",";
    for (int i = 0; i < numPt; i++) {
      float x = m_input->readLong(4)/65336.;
      float y = m_input->readLong(4)/65336.;
      form->m_vertices.push_back(Vec2f(x,y));
    }
    break;
  }
  case 7: {
    val =  m_input->readULong(2);
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
    if (vers >= 3) {
      val = m_input->readLong(2);
      if (val) f << "g1=" << val << ",";
      f << "h=" << m_input->readLong(4);
      for (int i = 0; i < 5; i++) {
        val = m_input->readLong(2);
        if (val) f << "g" << i+2 << "=" << val << ",";
      }
      pict.m_dataPos += 0x10;
    }
    f << "Fl=[";
    for (int i = 0; i < 5; i++) {
      val = m_input->readLong(2);
      if (val) f << std::hex << val << std::dec << ",";
      else f << ",";
    }
    f << "],";
    int numPos = m_input->readLong(2);
    if (numPos < 0) return zId;
    f << "numFonts=" << m_input->readLong(2);

    long off[4];
    for (int i = 0; i < 4; i++)
      off[i] = m_input->readULong(4);
    f << ", Ptrs=[" <<  std::hex << std::setw(8) << off[2] << ", " << std::setw(8) << off[0]
      << ", " << std::dec << long(off[1]-off[0])
      << ", "	<< std::dec << long(off[3]-off[0]) << "]";

    MSKGraphInternal::TextBox *text  = new MSKGraphInternal::TextBox(pict);
    text->m_numPositions = numPos;
    res.reset(text);
    if (!readText(*text)) return zId;
    res->m_pos.setEnd(m_input->tell());
    break;
  }
  default:
    ascii().addDelimiter(debData, '|');
    break;
  }

  if (!res)
    res.reset(new MSKGraphInternal::Zone(pict));
  res->m_extra += f.str();

  zId = m_state->m_zonesList.size();
  res->m_fileId = zId;
  m_state->m_zonesList.push_back(res);

  f.str("");
  f << "Entries(Graph" << res->m_subType << "):" << *res;
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  zone = res->m_pos;
  zone.setType("Graphic");
  m_input->seek(res->m_pos.end(), WPX_SEEK_SET);

  return zId;
}

void MSKGraph::computePositions(std::vector<int> &linesH, std::vector<int> &pagesH)
{
  int numLines = linesH.size();
  int numPages = pagesH.size();
  int numZones = m_state->m_zonesList.size();
  for (int i = 0; i < numZones; i++) {
    shared_ptr<MSKGraphInternal::Zone> zone = m_state->m_zonesList[i];
    if (zone->m_line >= 0) {
      int h;
      if (zone->m_line >= numLines) {
        MWAW_DEBUG_MSG(("MSKGraph::computePositions: linepos is too big\n"));
        h = linesH[numLines-1];
      } else
        h = linesH[zone->m_line];
      zone->m_decal = Vec2f(0, h);
    }
    if (zone->m_page < 0) {
      float h = zone->m_decal.y();
      float middleH=zone->m_box.center().y();
      h+=middleH;
      int p = 0;
      while (p < numPages) {
        if (h < pagesH[p]) break;
        h -= pagesH[p++];
      }
      zone->m_page = p;
      zone->m_decal.setY(h-middleH);
    }
  }
}

int MSKGraph::getEntryPictureV1(IMWAWEntry &zone)
{
  int zId = -1;
  if (m_input->atEOS()) return zId;

  long pos = m_input->tell();
  if (m_input->readULong(1) != 1) return zId;

  libmwaw_tools::DebugStream f;
  long ptr = m_input->readULong(2);
  int flag = m_input->readULong(1);
  long size = m_input->readULong(2)+6;
  if (size < 22) return zId;

  shared_ptr<MSKGraphInternal::DataPict> pict(new MSKGraphInternal::DataPict);
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
  pict->m_line = m_input->readLong(2);
  int val = m_input->readLong(2); // almost always equal to m_linePOs
  if (val !=  pict->m_line)
    f <<  "linePos2=" << std::hex << val << std::dec << ",";
  int dim[4]; // pictbox
  for (int i = 0; i < 4; i++)
    dim[i] = m_input->readLong(2);
  pict->m_box = Box2f(Vec2f(dim[1], dim[0]), Vec2f(dim[3],dim[2]));

  Vec2i pictMin = pict->m_box.min(), pictSize = pict->m_box.size();
  if (pictSize.x() < 0 || pictSize.y() < 0) return zId;

  if (pictSize.x() > 3000 || pictSize.y() > 3000 ||
      pictMin.x() < -200 || pictMin.y() < -200) return zId;
  pict->m_dataPos = m_input->tell();

  zone = pict->m_pos;
  zone.setType("GraphEntry");

  pict->m_extra = f.str();
  zId = m_state->m_zonesList.size();
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

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

// read a group
shared_ptr<MSKGraphInternal::GroupZone> MSKGraph::readGroup(MSKGraphInternal::Zone &header)
{
  shared_ptr<MSKGraphInternal::GroupZone> group(new MSKGraphInternal::GroupZone(header));
  libmwaw_tools::DebugStream f;
  m_input->seek(header.m_dataPos, WPX_SEEK_SET);
  long dim[4];
  for (int i = 0; i < 4; i++) dim[i] = m_input->readLong(4);
  f << "groupDim=" << dim[0] << "x" << dim[1] << "<->" << dim[2] << "x" << dim[3] << ",";
  long ptr[2];
  for (int i = 0; i < 2; i++)
    ptr[i] = m_input->readULong(4);
  f << "ptr0=" << std::hex << ptr[0] << std::dec << ",";
  if (ptr[0] != ptr[1])
    f << "ptr1=" << std::hex << ptr[1] << std::dec << ",";
  int val;
  if (version() >= 3) {
    val = m_input->readULong(4);
    if (val) f << "g1=" << val << ",";
  }

  m_input->seek(header.m_pos.end()-2, WPX_SEEK_SET);
  int N = m_input->readULong(2);
  IMWAWEntry childZone;
  int childId;
  for (int i = 0; i < N; i++) {
    long pos = m_input->tell();
    childId = getEntryPicture(childZone);
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

  libmwaw_tools::DebugStream f;
  f << "Entries(SmallText):";
  long pos = m_input->tell();
  if (!m_mainParser->checkIfPositionValid(pos+4*(textBox.m_numPositions+1))) return false;

  // first read the set of (positions, font)
  f << "pos=[";
  int nbFonts = 0;
  for (int i = 0; i <= textBox.m_numPositions; i++) {
    int pos = m_input->readLong(2);
    int form = m_input->readLong(2);
    f << pos << ":" << form << ", ";

    if (pos < 0 || form < -1) return false;
    if ((form == -1 && i != textBox.m_numPositions) ||
        (i && pos < textBox.m_positions[i-1])) {
      MWAW_DEBUG_MSG(("MSKGraph::readText: find odd positions\n"));
      f << "#";
      continue;
    }

    textBox.m_positions.push_back(pos);
    textBox.m_formats.push_back(form);
    if (form >= nbFonts)  nbFonts = form+1;
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
  long sizeOfData = m_input->readULong(4);
  int numFonts = (sizeOfData%0x12 == 0) ? sizeOfData/0x12 : 0;

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
        << "(" << m_convertissor->getFontDebugString(font.m_font) << "," << font << "),";

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
    sizeOfData = m_input->readULong(4);
    if (sizeOfData == nChar) {
      bool ok = true;
      // ok we try to read the string
      std::string chaine("");
      for (int i = 0; i < sizeOfData; i++) {
        unsigned char c = m_input->readULong(1);
        if (c == 0) {
          ok = false;
          break;
        }
        chaine += c;
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

void MSKGraph::send(MSKGraphInternal::TextBox &textBox)
{
  if (!m_listener) return;
  MSKGraphInternal::Font actFont;
  actFont.m_font = MWAWStruct::Font(20,12);
  setProperty(actFont);
  int numFonts = textBox.m_fontsList.size();
  int actFormatPos = 0;
  int numFormats = textBox.m_formats.size();
  if (numFormats != int(textBox.m_positions.size())) {
    MWAW_DEBUG_MSG(("MSKGraph::send: positions and formats have different length\n"));
    if (numFormats > int(textBox.m_positions.size()))
      numFormats = textBox.m_positions.size();
  }
  for (int i = 0; i < int(textBox.m_text.length()); i++) {
    if (actFormatPos < numFormats && textBox.m_positions[actFormatPos]==i) {
      int id = textBox.m_formats[actFormatPos++];
      if (id < 0 || id >= numFonts) {
        MWAW_DEBUG_MSG(("MSKGraph::send: can not find a font\n"));
      } else {
        actFont = textBox.m_fontsList[id];
        setProperty(actFont);
      }
    }
    unsigned char c = textBox.m_text[i];
    switch(c) {
    case 0x9:
      MWAW_DEBUG_MSG(("MSKGraph::sendText: find some tab\n"));
      m_listener->insertCharacter(' ');
      break;
    case 0xd:
      m_listener->insertEOL();
      break;
    case 0x19:
      m_listener->insertField(IMWAWContentListener::Title);
      break;
    case 0x18:
      m_listener->insertField(IMWAWContentListener::PageNumber);
      break;
    case 0x16:
      MWAW_DEBUG_MSG(("MSKGraph::sendText: find some time\n"));
      m_listener->insertField(IMWAWContentListener::Time);
      break;
    case 0x17:
      MWAW_DEBUG_MSG(("MSKGraph::sendText: find some date\n"));
      m_listener->insertField(IMWAWContentListener::Date);
      break;
    case 0x14: // fixme
      MWAW_DEBUG_MSG(("MSKGraph::sendText: footnote are not implemented\n"));
      break;
    default:
      if (c <= 0x1f) {
        MWAW_DEBUG_MSG(("MSKGraph::sendText: find char=%x\n",int(c)));
      } else {
        int unicode = m_convertissor->getUnicode (actFont.m_font, c);
        if (unicode == -1)
          m_listener->insertCharacter(c);
        else
          m_listener->insertUnicode(unicode);
      }
      break;
    }
  }
}

void MSKGraph::setProperty(MSKGraphInternal::Font const &font)
{
  if (!m_listener) return;
  font.m_font.sendTo(m_listener.get(), m_convertissor, m_state->m_font, true);
}

bool MSKGraph::readFont(MSKGraphInternal::Font &font)
{
  long pos = m_input->tell();
  libmwaw_tools::DebugStream f;
  if (!m_mainParser->checkIfPositionValid(pos+18))
    return false;
  font = MSKGraphInternal::Font();
  for (int i = 0; i < 3; i++)
    font.m_flags[i] = m_input->readLong(2);
  font.m_font.setFont(m_input->readULong(2));
  int flags = m_input->readULong(1);
  int flag = 0;
  if (flags & 0x1) flag |= DMWAW_BOLD_BIT;
  if (flags & 0x2) flag |= DMWAW_ITALICS_BIT;
  if (flags & 0x4) flag |= DMWAW_UNDERLINE_BIT;
  if (flags & 0x8) flag |= DMWAW_EMBOSS_BIT;
  if (flags & 0x10) flag |= DMWAW_SHADOW_BIT;
  if (flags & 0x20) flag |= DMWAW_SUPERSCRIPT_BIT;
  if (flags & 0x40) flag |= DMWAW_SUBSCRIPT_BIT;
  if (flags & 0x80) f << "#smaller,";
  font.m_font.setFlags(flag);

  int val = m_input->readULong(1);
  if (val) f << "#flags2=" << val << ",";
  font.m_font.setSize(m_input->readULong(2));

  int color[3];
  for (int i = 0; i < 3; i++) color[i] = (m_input->readULong(2)>>8);
  font.m_font.setColor(color);
  font.m_extra = f.str();
  return true;
}

void MSKGraph::send(int id, bool local)
{
  if (id < 0 || id >= int(m_state->m_zonesList.size())) {
    MWAW_DEBUG_MSG(("MSKGraph::send: can not find zone %d\n", id));
    return;
  }
  if (!m_listener) return;
  shared_ptr<MSKGraphInternal::Zone> zone = m_state->m_zonesList[id];
  zone->m_isSent = true;

  TMWAWPosition pictPos = zone->getPosition(local);
  if (!local)
    pictPos.setOrigin(pictPos.origin()+72.*m_mainParser->getPageTopLeft());
  WPXPropertyList extras;
  zone->fillFramePropertyList(extras);

  switch (zone->type()) {
  case MSKGraphInternal::Zone::Text: {
    MSKGraphInternal::TextBox &textBox = reinterpret_cast<MSKGraphInternal::TextBox &>(*zone);
    if (textBox.m_sentPhase == 0) {
      textBox.m_sentPhase=1;
      m_mainParser->sendTextBox(id, pictPos, extras);
    } else {
      textBox.m_sentPhase=2;
      send(textBox);
      textBox.m_sentPhase=0;
    }
    return;
  }
  case MSKGraphInternal::Zone::Group:
    return;
  case MSKGraphInternal::Zone::Basic:
  case MSKGraphInternal::Zone::Pict: {
    WPXBinaryData data;
    std::string type;
    if (!zone->getBinaryData(m_input, data,type))
      break;
    m_listener->insertPicture(pictPos, data, type, extras);
    return;
  }
  default:
    break;
  }

  MWAW_DEBUG_MSG(("MSKGraph::send: can not send zone %d\n", id));
}

void MSKGraph::sendAll()
{
  for (int i = 0; i < int(m_state->m_zonesList.size()); i++) {
    shared_ptr<MSKGraphInternal::Zone> zone = m_state->m_zonesList[i];
    if (zone->m_isSent) continue;
    send(i, false);
  }
}

void MSKGraph::flushExtra()
{
  for (int i = 0; i < int(m_state->m_zonesList.size()); i++) {
    shared_ptr<MSKGraphInternal::Zone> zone = m_state->m_zonesList[i];
    if (zone->m_isSent) continue;
    send(i);
  }
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
