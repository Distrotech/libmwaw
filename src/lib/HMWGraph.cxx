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

#include <libwpd/libwpd.h>

#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWPictBasic.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSubDocument.hxx"

#include "HMWParser.hxx"

#include "HMWGraph.hxx"

/** Internal: the structures of a HMWGraph */
namespace HMWGraphInternal
{
////////////////////////////////////////
//! Internal: the frame header of a HMWGraph
struct Frame {
  //! constructor
  Frame() : m_type(-1), m_fileId(-1), m_id(-1), m_page(0),
    m_pos(), m_baseline(0.f), m_posFlags(0), m_lineWidth(0), m_parsed(false), m_extra("") {
    m_colors[0]=0;
    m_colors[1]=0xFFFFFF;
    m_patterns[0] = m_patterns[1] = 1.f;
  }
  //! destructor
  virtual ~Frame() {
  }
  //! returns the line colors
  bool getLineColor(Vec3uc &color) const {
    int col[3] = {int((m_colors[0]>>16)&0xFF),int((m_colors[0]>>8)&0xFF),
                  int(m_colors[0]&0xFF)
                 };
    for (int i = 0; i < 3; i++)
      color[i] = (unsigned char)
                 (m_patterns[0]*float(col[i])+(1.f-m_patterns[0])*255.f);
    return true;
  }
  //! returns the surface colors
  bool getSurfaceColor(Vec3uc &color) const {
    int col[3] = {int((m_colors[1]>>16)&0xFF),int((m_colors[1]>>8)&0xFF),
                  int(m_colors[1]&0xFF)
                 };
    for (int i = 0; i < 3; i++)
      color[i] = (unsigned char)
                 (m_patterns[1]*float(col[i])+(1.f-m_patterns[1])*255.f);
    return true;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Frame const &grph);
  //! the graph type
  int m_type;
  //! the file id
  long m_fileId;
  //! the local id
  int m_id;
  //! the page
  int m_page;
  //! the position
  Box2f m_pos;
  //! the baseline
  float m_baseline;
  //! the graph anchor flags
  int m_posFlags;
  //! the border default size (before using width), 0 means Top, other unknown
  Vec2f m_borders[4];
  //! the line width
  float m_lineWidth;
  //! the line/surface colors
  uint32_t m_colors[2];
  //! the line/surface percent pattern
  float m_patterns[2];
  //! true if we have send the data
  mutable bool m_parsed;
  //! an extra string
  std::string m_extra;
};

std::ostream &operator<<(std::ostream &o, Frame const &grph)
{
  switch(grph.m_type) {
  case 3:
    o << "footnote[frame],";
    break;
  case 4:
    o << "textbox,";
    break;
  case 6:
    o << "picture,";
    break;
  case 8:
    o << "basicGraphic,";
    break;
  case 9:
    o << "table,";
    break;
  case 10:
    o << "textbox[withHeader],";
    break;
  case 11:
    o << "group";
    break;
  default:
    o << "#type=" << grph.m_type << ",";
  case -1:
    break;
  }
  if (grph.m_fileId > 0)
    o << "fileId="  << std::hex << grph.m_fileId << std::dec << ",";
  if (grph.m_id>0)
    o << "id=" << grph.m_id << ",";
  if (grph.m_page) o << "page=" << grph.m_page+1  << ",";
  o << "pos=" << grph.m_pos << ",";
  if (grph.m_baseline < 0 || grph.m_baseline>0) o << "baseline=" << grph.m_baseline << ",";
  int flag = grph.m_posFlags;
  if (flag & 2) o << "inGroup,";
  if (flag & 4) o << "wrap=around,"; // else overlap
  if (flag & 0x40) o << "lock,";
  if (!(flag & 0x80)) o << "transparent,"; // else opaque
  if (flag & 0x39) o << "posFlags=" << std::hex << (flag & 0x39) << std::dec << ",";
  o << "lineW=" << grph.m_lineWidth << ",";
  if (grph.m_colors[0])
    o << "lineColor=" << std::hex << grph.m_colors[0] << std::dec << ",";
  if (grph.m_patterns[0]<1.)
    o << "linePattern=" << 100.f*grph.m_patterns[0] << "%,";
  if (grph.m_colors[1]!=0xFFFFFF)
    o << "surfColor=" << std::hex << grph.m_colors[1] << std::dec << ",";
  if (grph.m_patterns[1]<1.)
    o << "surfPattern=" << 100.f*grph.m_patterns[1] << "%,";
  for (int i = 0; i < 4; i++) {
    if (grph.m_borders[i].x() > 0 || grph.m_borders[i].y() > 0)
      o << "border" << i << "=" << grph.m_borders[i] << ",";
  }
  o << grph.m_extra;
  return o;
}

////////////////////////////////////////
//! Internal: the geometrical graph of a HMWGraph
struct BasicGraph : public Frame {
  //! constructor
  BasicGraph(Frame const &orig) : Frame(orig), m_graphType(-1), m_arrowsFlag(0), m_cornerDim(0), m_listVertices() {
    m_extremity[0] = m_extremity[1] = Vec2f(0,0);
    m_angles[0] = 0;
    m_angles[1] = 90;
  }
  //! destructor
  ~BasicGraph() {}
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, BasicGraph const &graph) {
    o << graph.print();
    o << static_cast<Frame const &>(graph);
    return o;
  }
  //! print local data
  std::string print() const {
    std::stringstream s;
    switch(m_graphType) {
    case 0:
      s << "line,";
      break;
    case 1:
      s << "rect,";
      break;
    case 2:
      s << "circle,";
      break;
    case 3:
      s << "line[axisaligned],";
      break;
    case 4:
      s << "rectOval,";
      break;
    case 5:
      s << "arc,";
      break;
    case 6:
      s << "poly,";
      break;
    default:
      s << "#type=" << m_graphType << ",";
      break;
    }
    if (m_arrowsFlag&1) s << "startArrow,";
    if (m_arrowsFlag&2) s << "endArrow,";
    if (m_graphType==5) s << "angl=" << m_angles[0] << "<->" << m_angles[1] << ",";
    if (m_cornerDim > 0) s << "cornerDim=" << m_cornerDim << ",";
    if (m_arrowsFlag&0xfc) s << "#arrowFlags=" << std::hex << (m_arrowsFlag&0xfc) << std::dec << ",";
    if (m_extremity[0] != Vec2f(0,0) || m_extremity[1] != Vec2f(0,0))
      s << "extremity=" << m_extremity[0] << "<->" << m_extremity[1] << ",";
    if (m_listVertices.size()) {
      s << "pts=[";
      for (size_t pt = 0; pt < m_listVertices.size(); pt++)
        s << m_listVertices[pt] << ",";
      s << "],";
    }
    return s.str();
  }

  //! the graphic type: line, rectangle, ...
  int m_graphType;
  //! the lines arrow flag
  int m_arrowsFlag;
  //! the two extremities for a line
  Vec2f m_extremity[2];
  //! the arc angles in degrees
  int m_angles[2];
  //! the rectOval corner dimension
  float m_cornerDim;
  //! the list of vertices for a polygon
  std::vector<Vec2f> m_listVertices;
};

////////////////////////////////////////
//! Internal: the group of a HMWGraph
struct Group : public Frame {
  struct Child;
  //! constructor
  Group(Frame const &orig) : Frame(orig), m_childsList() {
  }
  //! destructor
  ~Group() {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Group const &group) {
    o << group.print();
    o << static_cast<Frame const &>(group);
    return o;
  }
  //! print local data
  std::string print() const;
  //! the list of child
  std::vector<Child> m_childsList;
  //! struct to store child data in HMWGraphInternal::Group
  struct Child {
    //! constructor
    Child() : m_fileId(-1) {
      for (int i = 0; i < 2; i++) m_values[i]=0;
    }
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Child const &ch) {
      if (ch.m_fileId > 0)
        o << "fileId="  << std::hex << ch.m_fileId << std::dec << ",";
      for (int i = 0; i < 2; i++) {
        if (ch.m_values[i] == 0)
          continue;
        o << "f" << i << "=" << ch.m_values[i] << ",";
      }
      return o;
    }
    //! the child id
    long m_fileId;
    //! two values
    int m_values[2];
  };
};

std::string Group::print() const
{
  std::stringstream s;
  for (size_t i = 0; i < m_childsList.size(); i++)
    s << "chld" << i << "=[" << m_childsList[i] << "],";
  return s.str();
}

////////////////////////////////////////
//! Internal: the picture of a HMWGraph
struct PictureFrame : public Frame {
  //! constructor
  PictureFrame(Frame const &orig) : Frame(orig), m_type(0), m_dim(0,0), m_borderDim(0,0) {
    for (int i = 0; i < 7; i++) m_values[i] = 0;
  }
  //! destructor
  ~PictureFrame() {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, PictureFrame const &picture) {
    o << picture.print();
    o << static_cast<Frame const &>(picture);
    return o;
  }
  //! print local data
  std::string print() const {
    std::stringstream s;
    if (m_type) s << "type?=" << m_type << ",";
    if (m_dim[0] || m_dim[1])
      s << "dim?=" << m_dim << ",";
    if (m_borderDim[0] > 0 || m_borderDim[1] > 0)
      s << "borderDim?=" << m_borderDim << ",";
    for (int i = 0; i < 7; i++) {
      if (m_values[i]) s << "f" << i << "=" << m_values[i];
    }
    return s.str();
  }

  //! a type
  int m_type;
  //! a dim?
  Vec2i m_dim;
  //! the border dim?
  Vec2f m_borderDim;
  //! some unknown int
  int m_values[7];
};
////////////////////////////////////////
//! Internal: the table of a HMWGraph
struct Table : public Frame {
  struct Cell;
  //! constructor
  Table(Frame const &orig) : Frame(orig), m_rows(0), m_columns(0), m_numCells(0) {
  }
  //! destructor
  ~Table() {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Table const &table) {
    o << table.print();
    o << static_cast<Frame const &>(table);
    return o;
  }
  //! print local data
  std::string print() const {
    std::stringstream s;
    if (m_rows)
      s << "nRows="  << m_rows << ",";
    if (m_columns)
      s << "nColumns="  << m_columns << ",";
    if (m_numCells)
      s << "nCells="  << m_numCells << ",";
    return s.str();
  }
  //! a cell of a table in HMWGraph
  struct Cell {
    //! constructor
    Cell(): m_row(-1), m_col(-1), m_span(1,1), m_dim(), m_id(-1), m_fileId(-1), m_extra("") {
    }
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Cell const &cell);
    //! the row
    int m_row;
    //! the column
    int m_col;
    //! the span ( numRow x numCol )
    Vec2i m_span;
    //! the dimension
    Vec2f m_dim;
    //! the cell id ( corresponding to the last data in the main zones list )
    int m_id;
    //! the file id
    long m_fileId;
    //! extra data
    std::string m_extra;
  };

  //! the number of row
  int m_rows;
  //! the number of columns
  int m_columns;
  //! the number of cells
  int m_numCells;
};

std::ostream &operator<<(std::ostream &o, Table::Cell const &cell)
{
  o << "row=" << cell.m_row << ",";
  o << "col=" << cell.m_col << ",";
  if (cell.m_span.x() != 1 || cell.m_span.y() != 1)
    o << "span=" << cell.m_span << ",";
  o << "dim=" << cell.m_dim << ",";
  if (cell.m_id > 0)
    o << "cellId="  << std::hex << cell.m_id << std::dec << ",";
  if (cell.m_fileId > 0)
    o << "fileId="  << std::hex << cell.m_fileId << std::dec << ",";
  o << cell.m_extra;
  return o;
}

////////////////////////////////////////
//! Internal: the textbox of a HMWGraph
struct TextBox : public Frame {
  struct Cell;
  //! constructor
  TextBox(Frame const &orig, bool isHeaderBox) : Frame(orig), m_headerTextBox(isHeaderBox) {
    for (int i = 0; i < 4; i++) m_values[i] = 0;
    for (int i = 0; i < 2; i++) m_flags[i] = 0;
    for (int i = 0; i < 2; i++) m_dim[i] = 0;
  }
  //! destructor
  ~TextBox() {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, TextBox const &textbox) {
    o << textbox.print();
    o << static_cast<Frame const &>(textbox);
    return o;
  }
  //! print local data
  std::string print() const {
    std::stringstream s;
    for (int i = 0; i < 4; i++) { // 0|1, 0, 0, 0
      if (m_values[i])
        s << "f" << i << "=" << m_values[i] << ",";
    }
    for (int i = 0; i < 2; i++) { // 0|1, 0|1
      if (m_flags[i])
        s << "fl" << i << "=" << m_flags[i] << ",";
    }
    for (int i = 0; i < 2; i++) { // 0x14, 0x14
      if (m_dim[i])
        s << "dim" << i << "=" << m_dim[i] << ",";
    }

    return s.str();
  }

  //! a flag to know if this is a header textbox
  bool m_headerTextBox;
  //! four unknown value
  int m_values[4];
  //! two unknown flag
  int m_flags[2];
  //! two unknown dim for header textbox
  float m_dim[2];
};

////////////////////////////////////////
//! Internal: the picture of a HMWGraph
struct Picture {
  //! constructor
  Picture(shared_ptr<HMWZone> zone) : m_zone(zone), m_fileId(-1), m_parsed(false), m_extra("") {
    m_pos[0] = m_pos[1] = 0;
  }
  //! destructor
  ~Picture() {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Picture const &pict) {
    if (pict.m_fileId >= 0)
      o << "fileId="  << std::hex << pict.m_fileId << std::dec << ",";
    o << pict.m_extra;
    return o;
  }
  //! the main zone
  shared_ptr<HMWZone> m_zone;
  //! the first and last position of the picture data in the zone
  long m_pos[2];
  //! the file id
  long m_fileId;
  //! a flag to know if the picture was send to the receiver
  bool m_parsed;
  //! extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the state of a HMWGraph
struct State {
  //! constructor
  State() : m_numPages(0), m_framesList(), m_picturesList(), m_colorList(), m_patternPercentList() { }
  //! returns a color correspond to an id
  bool getColor(int id, uint32_t &col) {
    initColors();
    if (id < 0 || id >= int(m_colorList.size())) {
      MWAW_DEBUG_MSG(("HMWGraphInternal::State::getColor: can not find color %d\n", id));
      return false;
    }
    col = m_colorList[size_t(id)];
    return true;
  }
  //! returns a pattern correspond to an id
  bool getPatternPercent(int id, float &percent) {
    initPatterns();
    if (id < 0 || id >= int(m_patternPercentList.size())) {
      MWAW_DEBUG_MSG(("HMWGraphInternal::State::getPatternPercent: can not find pattern %d\n", id));
      return false;
    }
    percent = m_patternPercentList[size_t(id)];
    return true;
  }

  //! init the color list
  void initColors();
  //! init the pattenr list
  void initPatterns();

  int m_numPages /* the number of pages */;
  //! a list of graphs
  std::vector<shared_ptr<Frame> > m_framesList;
  //! a list of picture
  std::vector<shared_ptr<Picture> > m_picturesList;
  //! a list colorId -> color
  std::vector<uint32_t> m_colorList;
  //! a list patternId -> percent
  std::vector<float> m_patternPercentList;
};

void State::initPatterns()
{
  if (m_patternPercentList.size()) return;
  float const patterns[64] = {
    0.f, 1.f, 0.96875f, 0.9375f, 0.875f, 0.75f, 0.5f, 0.25f,
    0.25f, 0.1875f, 0.1875f, 0.125f, 0.0625f, 0.0625f, 0.03125f, 0.015625f,
    0.75f, 0.5f, 0.25f, 0.375f, 0.25f, 0.125f, 0.25f, 0.125f,
    0.75f, 0.5f, 0.25f, 0.375f, 0.25f, 0.125f, 0.25f, 0.125f,
    0.75f, 0.5f, 0.5f, 0.5f, 0.5f, 0.25f, 0.25f, 0.234375f,
    0.625f, 0.375f, 0.125f, 0.25f, 0.21875f, 0.21875f, 0.125f, 0.09375f,
    0.5f, 0.5625f, 0.4375f, 0.375f, 0.21875f, 0.28125f, 0.1875f, 0.09375f,
    0.59375f, 0.5625f, 0.515625f, 0.34375f, 0.3125f, 0.25f, 0.25f, 0.234375f
  };
  m_patternPercentList.resize(64);
  for (size_t i=0; i < 64; i++)
    m_patternPercentList[i] = patterns[i];
}

void State::initColors()
{
  if (m_colorList.size()) return;
  uint32_t const defCol[256] = {
    0x000000, 0xffffff, 0xffffcc, 0xffff99, 0xffff66, 0xffff33, 0xffff00, 0xffccff,
    0xffcccc, 0xffcc99, 0xffcc66, 0xffcc33, 0xffcc00, 0xff99ff, 0xff99cc, 0xff9999,
    0xff9966, 0xff9933, 0xff9900, 0xff66ff, 0xff66cc, 0xff6699, 0xff6666, 0xff6633,
    0xff6600, 0xff33ff, 0xff33cc, 0xff3399, 0xff3366, 0xff3333, 0xff3300, 0xff00ff,
    0xff00cc, 0xff0099, 0xff0066, 0xff0033, 0xff0000, 0xccffff, 0xccffcc, 0xccff99,
    0xccff66, 0xccff33, 0xccff00, 0xccccff, 0xcccccc, 0xcccc99, 0xcccc66, 0xcccc33,
    0xcccc00, 0xcc99ff, 0xcc99cc, 0xcc9999, 0xcc9966, 0xcc9933, 0xcc9900, 0xcc66ff,
    0xcc66cc, 0xcc6699, 0xcc6666, 0xcc6633, 0xcc6600, 0xcc33ff, 0xcc33cc, 0xcc3399,
    0xcc3366, 0xcc3333, 0xcc3300, 0xcc00ff, 0xcc00cc, 0xcc0099, 0xcc0066, 0xcc0033,
    0xcc0000, 0x99ffff, 0x99ffcc, 0x99ff99, 0x99ff66, 0x99ff33, 0x99ff00, 0x99ccff,
    0x99cccc, 0x99cc99, 0x99cc66, 0x99cc33, 0x99cc00, 0x9999ff, 0x9999cc, 0x999999,
    0x999966, 0x999933, 0x999900, 0x9966ff, 0x9966cc, 0x996699, 0x996666, 0x996633,
    0x996600, 0x9933ff, 0x9933cc, 0x993399, 0x993366, 0x993333, 0x993300, 0x9900ff,
    0x9900cc, 0x990099, 0x990066, 0x990033, 0x990000, 0x66ffff, 0x66ffcc, 0x66ff99,
    0x66ff66, 0x66ff33, 0x66ff00, 0x66ccff, 0x66cccc, 0x66cc99, 0x66cc66, 0x66cc33,
    0x66cc00, 0x6699ff, 0x6699cc, 0x669999, 0x669966, 0x669933, 0x669900, 0x6666ff,
    0x6666cc, 0x666699, 0x666666, 0x666633, 0x666600, 0x6633ff, 0x6633cc, 0x663399,
    0x663366, 0x663333, 0x663300, 0x6600ff, 0x6600cc, 0x660099, 0x660066, 0x660033,
    0x660000, 0x33ffff, 0x33ffcc, 0x33ff99, 0x33ff66, 0x33ff33, 0x33ff00, 0x33ccff,
    0x33cccc, 0x33cc99, 0x33cc66, 0x33cc33, 0x33cc00, 0x3399ff, 0x3399cc, 0x339999,
    0x339966, 0x339933, 0x339900, 0x3366ff, 0x3366cc, 0x336699, 0x336666, 0x336633,
    0x336600, 0x3333ff, 0x3333cc, 0x333399, 0x333366, 0x333333, 0x333300, 0x3300ff,
    0x3300cc, 0x330099, 0x330066, 0x330033, 0x330000, 0x00ffff, 0x00ffcc, 0x00ff99,
    0x00ff66, 0x00ff33, 0x00ff00, 0x00ccff, 0x00cccc, 0x00cc99, 0x00cc66, 0x00cc33,
    0x00cc00, 0x0099ff, 0x0099cc, 0x009999, 0x009966, 0x009933, 0x009900, 0x0066ff,
    0x0066cc, 0x006699, 0x006666, 0x006633, 0x006600, 0x0033ff, 0x0033cc, 0x003399,
    0x003366, 0x003333, 0x003300, 0x0000ff, 0x0000cc, 0x000099, 0x000066, 0x000033,
    0xee0000, 0xdd0000, 0xbb0000, 0xaa0000, 0x880000, 0x770000, 0x550000, 0x440000,
    0x220000, 0x110000, 0x00ee00, 0x00dd00, 0x00bb00, 0x00aa00, 0x008800, 0x007700,
    0x005500, 0x004400, 0x002200, 0x001100, 0x0000ee, 0x0000dd, 0x0000bb, 0x0000aa,
    0x000088, 0x000077, 0x000055, 0x000044, 0x000022, 0x000011, 0xeeeeee, 0xdddddd,
    0xbbbbbb, 0xaaaaaa, 0x888888, 0x777777, 0x555555, 0x444444, 0x222222, 0x111111,
  };
  m_colorList.resize(256);
  for (size_t i = 0; i < 256; i++)
    m_colorList[i] = defCol[i];
}

////////////////////////////////////////
//! Internal: the subdocument of a HMWGraph
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(HMWGraph &pars, MWAWInputStreamPtr input, int id, MWAWPosition const &pos, WPXPropertyList const &extras) :
    MWAWSubDocument(pars.m_mainParser, input, MWAWEntry()), m_graphParser(&pars), m_id(id), m_position(pos), m_extras(extras) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  /** the graph parser */
  HMWGraph *m_graphParser;
  //! the pict id
  int m_id;
  //! the pict position
  MWAWPosition m_position;
  //! the property list
  WPXPropertyList m_extras;
private:
  SubDocument(SubDocument const &orig);
  SubDocument &operator=(SubDocument const &orig);
};

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("SubDocument::parse: no listener\n"));
    return;
  }
  HMWContentListener *listen = dynamic_cast<HMWContentListener *>(listener.get());
  if (!listen) {
    MWAW_DEBUG_MSG(("SubDocument::parse: bad listener\n"));
    return;
  }

  assert(m_graphParser);

  long pos = m_input->tell();
  //  m_graphParser->sendPicture(m_id, true, m_position, m_extras);
  m_input->seek(pos, WPX_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_graphParser != sDoc->m_graphParser) return true;
  if (m_id != sDoc->m_id) return true;
  if (m_position != sDoc->m_position) return true;
  return false;
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
HMWGraph::HMWGraph
(MWAWInputStreamPtr ip, HMWParser &parser, MWAWFontConverterPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert), m_state(new HMWGraphInternal::State),
  m_mainParser(&parser), m_asciiFile(parser.ascii())
{
}

HMWGraph::~HMWGraph()
{ }

int HMWGraph::version() const
{
  return m_mainParser->version();
}

int HMWGraph::numPages() const
{
  if (m_state->m_framesList.size())
    m_state->m_numPages = 1;
  return m_state->m_numPages;
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////

// a small zone: related to frame pos + data?
bool HMWGraph::readFrames(shared_ptr<HMWZone> zone)
{
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWGraph::readFrames: called without any zone\n"));
    return false;
  }

  long dataSz = (long) zone->m_data.size();
  if (dataSz < 70) {
    MWAW_DEBUG_MSG(("HMWGraph::readFrames: the zone seems too short\n"));
    return false;
  }

  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->m_asciiFile;
  libmwaw::DebugStream f;
  zone->m_parsed = true;
  long pos=0;
  input->seek(pos, WPX_SEEK_SET);
  long val;
  HMWGraphInternal::Frame graph;
  graph.m_type = (int) input->readULong(1);
  val = (long) input->readULong(1);
  if (val) f << "#f0=" << std::hex << val << std::dec << ",";
  graph.m_posFlags = (int) input->readULong(1);
  val = (long) input->readULong(1);
  if (val) f << "#f1=" << std::hex << val << std::dec << ",";
  graph.m_page  = (int) input->readLong(2);
  float dim[4];
  for (int i = 0; i < 4; i++)
    dim[i] = float(input->readLong(4))/65536.f;
  graph.m_pos = Box2f(Vec2f(dim[0],dim[1]),Vec2f(dim[2],dim[3]));

  for (int i = 0; i < 4; i++) { // border size, 0=Top, other unknown
    float bd[2];
    for (int j = 0; j < 2; j++)
      bd[j] = float(input->readLong(4))/65536.f;
    graph.m_borders[i] = Vec2f(bd[0],bd[1]);
  }
  graph.m_lineWidth = float(input->readLong(4))/65536.f;
  val = (long) input->readULong(2);
  if (val) f << "#g0=" << val << ",";
  for (int i = 0; i < 2; i++) {
    int color = (int) input->readULong(2);
    uint32_t col;
    if (m_state->getColor(color, col))
      graph.m_colors[i] = col;
    else
      f << "#color[" << i << "]=" << color << ",";
    int pattern = (int) input->readULong(2);
    float patPercent;
    if (m_state->getPatternPercent(pattern, patPercent))
      graph.m_patterns[i] = patPercent;
    else
      f << "#pattern[" << i << "]=" << pattern << ",";
  }
  graph.m_id=(int) input->readLong(2);
  graph.m_baseline = float(input->readLong(4))/65536.f;
  for (int i = 1; i < 3; i++) {
    val = (long) input->readULong(2);
    if (val) f << "#g" << i << "=" << val << ",";
  }

  graph.m_extra=f.str();
  f.str("");

  f << zone->name() << "(A):PTR=" << std::hex << zone->m_filePos << std::dec << "," << graph;

  asciiFile.addDelimiter(input->tell(),'|');
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  shared_ptr<HMWGraphInternal::Frame> frame;
  switch(graph.m_type) {
  case 4:
  case 10:
    frame = readTextBox(zone, graph,graph.m_type==10);
    break;
  case 6:
    frame = readPictureFrame(zone, graph);
    break;
  case 8:
    frame = readBasicGraph(zone, graph);
    break;
  case 9:
    frame = readTable(zone, graph);
    break;
  case 11:
    frame = readGroup(zone, graph);
    break;
  default:
    break;
  }
  if (frame)
    m_state->m_framesList.push_back(frame);
  return true;
}

// read a picture
bool HMWGraph::readPicture(shared_ptr<HMWZone> zone)
{
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWGraph::readPicture: called without any zone\n"));
    return false;
  }

  long dataSz = (long) zone->m_data.size();
  if (dataSz < 86) {
    MWAW_DEBUG_MSG(("HMWGraph::readPicture: the zone seems too short\n"));
    return false;
  }

  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->m_asciiFile;
  libmwaw::DebugStream f;
  zone->m_parsed = true;

  shared_ptr<HMWGraphInternal::Picture> picture(new HMWGraphInternal::Picture(zone));

  long pos=0;
  input->seek(pos, WPX_SEEK_SET);
  picture->m_fileId = (long) input->readULong(4);
  long val;
  for (int i=0; i < 39; i++) {
    val = input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  long pictSz = (long) input->readULong(4);
  if (pictSz < 0 || pictSz+86 > dataSz) {
    MWAW_DEBUG_MSG(("HMWGraph::readPicture: problem reading the picture size\n"));
    return false;
  }
  picture->m_pos[0] = input->tell();
  picture->m_pos[1] = picture->m_pos[0]+pictSz;
  picture->m_extra = f.str();
  m_state->m_picturesList.push_back(picture);

  f.str("");
  f << zone->name() << ":PTR=" << std::hex << zone->m_filePos << std::dec << "," << *picture;
  f << "pictSz=" << pictSz << ",";
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  pos = input->tell();
#ifdef DEBUG_WITH_FILES
  WPXBinaryData file;
  input->seek(pos, WPX_SEEK_SET);
  input->readDataBlock(pictSz, file);

  libmwaw::DebugStream f2;
  static int volatile pictName = 0;
  f2 << "Pict" << ++pictName << ".pct";
  libmwaw::Debug::dumpFile(file, f2.str().c_str());
#endif
  asciiFile.skipZone(pos, pos+pictSz-1);

  return true;
}

////////////////////////////////////////////////////////////
// send data to a listener
////////////////////////////////////////////////////////////
/** try to send a frame to the listener */
bool HMWGraph::send(HMWGraphInternal::Frame const &frame, MWAWPosition pos, WPXPropertyList extras)
{
  if (!m_listener) return true;

  frame.m_parsed = true;
  switch(frame.m_type) {
  case 8:
    return sendBasicGraph(reinterpret_cast<HMWGraphInternal::BasicGraph const &>(frame), pos, extras);
  default:
    MWAW_DEBUG_MSG(("HMWGraph::send: sending type %d is not implemented\n", frame.m_type));
    break;
  }
  return false;
}

bool HMWGraph::sendBasicGraph(HMWGraphInternal::BasicGraph const &pict, MWAWPosition pos, WPXPropertyList extras)
{
  if (!m_listener) return true;
  Vec2f pictSz = pict.m_pos.size();
  if (pictSz[0] < 0) pictSz.setX(-pictSz[0]);
  if (pictSz[1] < 0) pictSz.setY(-pictSz[1]);
  Box2f box(Vec2f(0,0), pictSz);

  if (pos.size()[0] <= 0 || pos.size()[1] <= 0)
    pos.setSize(pictSz);

  shared_ptr<MWAWPictBasic> pictPtr;
  switch(pict.m_graphType) {
  case 0:
  case 3: {
    Vec2f minPt(pict.m_extremity[0]);
    if (minPt[0] > pict.m_extremity[1][0])
      minPt[0] = pict.m_extremity[1][0];
    if (minPt[1] > pict.m_extremity[1][1])
      minPt[1] = pict.m_extremity[1][1];
    MWAWPictLine *res=new MWAWPictLine(pict.m_extremity[0]-minPt, pict.m_extremity[1]-minPt);
    pictPtr.reset(res);
    if (pict.m_arrowsFlag&1) res->setArrow(0, true);
    if (pict.m_arrowsFlag&2) res->setArrow(1, true);
    break;
  }
  case 1: {
    MWAWPictRectangle *res=new MWAWPictRectangle(box);
    pictPtr.reset(res);
    break;
  }
  case 2: {
    MWAWPictCircle *res=new MWAWPictCircle(box);
    pictPtr.reset(res);
    break;
  }
  case 4: {
    MWAWPictRectangle *res=new MWAWPictRectangle(box);
    int roundValues[2];
    for (int i = 0; i < 2; i++) {
      if (2.f*pict.m_cornerDim <= pictSz[i])
        roundValues[i] = int(pict.m_cornerDim+1);
      else if (pict.m_cornerDim >= 4.0f)
        roundValues[i]= (int(pict.m_cornerDim)+1)/2;
      else
        roundValues[i]=1;
    }
    res->setRoundCornerWidth(roundValues[0], roundValues[1]);
    pictPtr.reset(res);
    break;
  }
  case 5: {
    int angle[2] = { int(90-pict.m_angles[1]), int(90-pict.m_angles[0])};

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
  case 6: {
    std::vector<Vec2f> listPts = pict.m_listVertices;
    size_t numPts = listPts.size();
    if (!numPts) break;
    Vec2f minPt(listPts[0]);
    for (size_t i = 1; i < numPts; i++) {
      if (minPt[0] > listPts[i][0])
        minPt[0] = listPts[i][0];
      if (minPt[1] > listPts[i][1])
        minPt[1] = listPts[i][1];
    }
    for (size_t i = 0; i < numPts; i++)
      listPts[i] -= minPt;
    MWAWPictPolygon *res=new MWAWPictPolygon(box, listPts);
    pictPtr.reset(res);
    break;
  }
  default:
    return false;
  }

  if (!pictPtr)
    return false;
  pictPtr->setLineWidth(pict.m_lineWidth);
  Vec3uc color;
  if (pict.getLineColor(color))
    pictPtr->setLineColor(color[0], color[1], color[2]);
  if (pict.getSurfaceColor(color))
    pictPtr->setSurfaceColor(color[0], color[1], color[2]);

  WPXBinaryData data;
  std::string type;
  if (!pictPtr->getBinary(data,type)) return false;

  pos.setOrigin(pos.origin()-Vec2f(2,2));
  pos.setSize(pos.size()+Vec2f(4,4));
  m_listener->insertPicture(pos,data, type, extras);
  return true;
}

////////////////////////////////////////////////////////////
// low level
////////////////////////////////////////////////////////////

// try to read a small graphic
shared_ptr<HMWGraphInternal::BasicGraph> HMWGraph::readBasicGraph(shared_ptr<HMWZone> zone, HMWGraphInternal::Frame const &header)
{
  shared_ptr<HMWGraphInternal::BasicGraph> graph;
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWGraph::readBasicGraph: called without any zone\n"));
    return graph;
  }

  MWAWInputStreamPtr input = zone->m_input;
  long dataSz = (long) zone->m_data.size();
  long pos = input->tell();
  if (pos+26 > dataSz) {
    MWAW_DEBUG_MSG(("HMWGraph::readBasicGraph: the zone seems too short\n"));
    return graph;
  }

  graph.reset(new HMWGraphInternal::BasicGraph(header));
  libmwaw::DebugFile &asciiFile = zone->m_asciiFile;
  libmwaw::DebugStream f;
  graph->m_graphType = (int) input->readLong(1);
  long val;
  bool ok = true;
  switch(graph->m_graphType) {
  case 0:
  case 3: { // lines
    if (pos+28 > dataSz) {
      f << "###";
      ok = false;
      break;
    }
    graph->m_arrowsFlag = (int) input->readLong(1);
    for (int i = 0; i < 5; i++) { // always 0
      val = input->readLong(2);
      if (val) f << "f" << i << "=" << val << ",";
    }
    float coord[2];
    for (int pt = 0; pt < 2; pt++) {
      for (int i = 0; i < 2; i++)
        coord[i] = float(input->readLong(4))/65536.f;
      graph->m_extremity[pt] = Vec2f(coord[1],coord[0]);
    }
    break;
  }
  case 1: // rectangle
  case 2: // circle
    for (int i = 0; i < 13; i++) { // always 0
      val = input->readLong(2);
      if (val) f << "f" << i << "=" << val << ",";
    }
    break;
  case 4: // rectOval
    if (pos+28 > dataSz) {
      f << "###";
      ok = false;
      break;
    }
    for (int i = 0; i < 4; i++) {
      val = input->readLong(i ? 2 : 1);
      if (val) f << "f" << i << "=" << val << ",";
    }
    graph->m_cornerDim = float(input->readLong(4))/65536.f;
    for (int i = 0; i < 8; i++) {
      val = input->readLong(i);
      if (val) f << "g" << i << "=" << val << ",";
    }
    break;
  case 5: { // arc
    val = input->readLong(2);
    if (val) f << "f0=" << val << ",";
    int transf = (int) input->readULong(1);
    if (transf>=0 && transf <= 3) {
      graph->m_angles[0] = transf*90;
      graph->m_angles[1] = graph->m_angles[0]+90;
    } else {
      f << "#transf=" << transf << ",";
      MWAW_DEBUG_MSG(("HMWGraph::readBasicGraph: find unexpected transformation for arc\n"));
      ok = false;
      break;
    }
    for (int i = 0; i < 12; i++) { // always 0
      val = input->readLong(2);
      if (val) f << "f" << i+1 << "=" << val << ",";
    }
    break;
  }
  case 6: { // poly
    for (int i = 0; i < 5; i++) { // find f3=0|1, other 0
      val = input->readLong(1);
      if (val) f << "f" << i << "=" << val << ",";
    }
    int numPt = (int) input->readLong(2);
    if (numPt < 0 || 28+8*numPt > dataSz) {
      MWAW_DEBUG_MSG(("HMWGraph::readBasicGraph: find unexpected number of points\n"));
      f << "#pt=" << numPt << ",";
      ok = false;
      break;
    }
    for (int i = 0; i < 10; i++) { //always 0
      val = input->readLong(2);
      if (val) f << "g" << i << "=" << val << ",";
    }
    for (int i = 0; i < numPt; i++) {
      float dim[2];
      for (int c=0; c < 2; c++)
        dim[c] = float(input->readLong(4))/65536.f;
      graph->m_listVertices.push_back(Vec2f(dim[1], dim[0]));
    }
    break;
  }
  default:
    MWAW_DEBUG_MSG(("HMWGraph::readBasicGraph: find unexpected graphic subType\n"));
    f << "###";
    ok = false;
    break;
  }

  std::string extra = f.str();
  graph->m_extra += extra;

  f.str("");
  f << "FrameDef(graphData):" << graph->print() << extra;

  asciiFile.addDelimiter(input->tell(),'|');
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  if (!ok)
    graph.reset();
  return graph;
}

// try to read the group data
shared_ptr<HMWGraphInternal::Group> HMWGraph::readGroup(shared_ptr<HMWZone> zone, HMWGraphInternal::Frame const &header)
{
  shared_ptr<HMWGraphInternal::Group> group;
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWGraph::readGroup: called without any zone\n"));
    return group;
  }

  MWAWInputStreamPtr input = zone->m_input;
  long dataSz = (long) zone->m_data.size();
  long pos = input->tell();
  if (pos+2 > dataSz) {
    MWAW_DEBUG_MSG(("HMWGraph::readGroup: the zone seems too short\n"));
    return group;
  }
  int N = (int) input->readULong(2);
  if (pos+2+8*N > dataSz) {
    MWAW_DEBUG_MSG(("HMWGraph::readGroup: can not read N\n"));
    return group;
  }
  group.reset(new HMWGraphInternal::Group(header));
  libmwaw::DebugFile &asciiFile = zone->m_asciiFile;
  libmwaw::DebugStream f;
  for (int i = 0; i < N; i++) {
    HMWGraphInternal::Group::Child child;
    child.m_fileId = (long) input->readULong(4);
    for (int j = 0; j < 2; j++) // f0=4|6|8, f1=0
      child.m_values[j] = (int) input->readLong(2);
    group->m_childsList.push_back(child);
  }
  f << "FrameDef(groupData):" << group->print();

  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  return group;
}

// try to read a picture frame
shared_ptr<HMWGraphInternal::PictureFrame> HMWGraph::readPictureFrame(shared_ptr<HMWZone> zone, HMWGraphInternal::Frame const &header)
{
  shared_ptr<HMWGraphInternal::PictureFrame> picture;
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWGraph::readPicture: called without any zone\n"));
    return picture;
  }

  MWAWInputStreamPtr input = zone->m_input;
  long dataSz = (long) zone->m_data.size();
  long pos = input->tell();
  if (pos+32 > dataSz) {
    MWAW_DEBUG_MSG(("HMWGraph::readPicture: the zone seems too short\n"));
    return picture;
  }

  picture.reset(new HMWGraphInternal::PictureFrame(header));
  libmwaw::DebugFile &asciiFile = zone->m_asciiFile;
  libmwaw::DebugStream f;
  picture->m_type = (int) input->readLong(2); // 0 or 4 : or maybe wrapping
  for (int i = 0; i < 5; i++) // always 0
    picture->m_values[i] =  (int) input->readLong(2);
  float bDim[2];
  for (int i = 0; i < 2; i++)
    bDim[i] = float(input->readLong(4))/65536.f;
  picture->m_borderDim = Vec2f(bDim[0], bDim[1]);
  for (int i = 5; i < 7; i++) // always 0
    picture->m_values[i] =  (int) input->readLong(2);
  int dim[2]; //0,0 or 3e,3c
  for (int i = 0; i < 2; i++)
    dim[i] = (int) input->readLong(2);
  picture->m_dim  = Vec2i(dim[0], dim[1]);
  picture->m_fileId = (long) input->readULong(4);

  f << "FrameDef(pictureData):";
  if (picture->m_fileId)
    f << "fId=" << std::hex << picture->m_fileId << std::dec << ",";
  f << picture->print();
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  return picture;
}

// try to read the textbox data
shared_ptr<HMWGraphInternal::TextBox> HMWGraph::readTextBox(shared_ptr<HMWZone> zone, HMWGraphInternal::Frame const &header, bool hasHeader)
{
  shared_ptr<HMWGraphInternal::TextBox> textbox;
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWGraph::readTextBox: called without any zone\n"));
    return textbox;
  }

  MWAWInputStreamPtr input = zone->m_input;
  long dataSz = (long) zone->m_data.size();
  long pos = input->tell();
  int expectedSize = hasHeader ? 20 : 12;
  if (pos+expectedSize > dataSz) {
    MWAW_DEBUG_MSG(("HMWGraph::readTextBox: the zone seems too short\n"));
    return textbox;
  }

  textbox.reset(new HMWGraphInternal::TextBox(header, hasHeader));
  libmwaw::DebugFile &asciiFile = zone->m_asciiFile;
  libmwaw::DebugStream f;
  for (int i = 0; i < 2; i++) // 0|1, 0||1
    textbox->m_flags[i] = (int) input->readULong(1);
  for (int i = 0; i < 3; i++) // 0|1, 0, 0
    textbox->m_values[i] = (int) input->readLong(2);
  textbox->m_fileId = (long) input->readULong(4);
  if (hasHeader) { // checkme
    for (int i = 0; i < 2; i++)
      textbox->m_dim[i] = float(input->readLong(4))/65536.f;
  }
  f.str("");
  f << "FrameDef(textboxData):";
  f << "fId=" << std::hex << textbox->m_fileId << std::dec << "," << textbox->print();
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  return textbox;
}

// try to read the table data
shared_ptr<HMWGraphInternal::Table> HMWGraph::readTable(shared_ptr<HMWZone> zone, HMWGraphInternal::Frame const &header)
{
  shared_ptr<HMWGraphInternal::Table> table;
  if (!zone) {
    MWAW_DEBUG_MSG(("HMWGraph::readTable: called without any zone\n"));
    return table;
  }

  MWAWInputStreamPtr input = zone->m_input;
  long dataSz = (long) zone->m_data.size();
  long pos = input->tell();
  if (pos+20 > dataSz) {
    MWAW_DEBUG_MSG(("HMWGraph::readTable: the zone seems too short\n"));
    return table;
  }

  table.reset(new HMWGraphInternal::Table(header));
  libmwaw::DebugFile &asciiFile = zone->m_asciiFile;
  libmwaw::DebugStream f;
  long val;
  for (int i = 0; i < 4; i++) {
    val = input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  table->m_fileId = (long) input->readULong(4);
  table->m_rows = (int) input->readLong(2);
  table->m_columns = (int) input->readLong(2);
  table->m_numCells = (int) input->readLong(2);

  val = input->readLong(2);
  if (val) f << "f4=" << val << ",";
  std::string extra = f.str();
  table->m_extra += extra;

  f.str("");
  f << "FrameDef(tableData):";
  f << "fId=" << std::hex << table->m_fileId << std::dec << ","
    << table->print() << extra;
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  for (int i = 0; i < table->m_numCells; i++) {
    if (input->atEOS()) break;
    pos = input->tell();
    f.str("");
    if (pos+80 > dataSz) {
      MWAW_DEBUG_MSG(("HMWGraph::readTable: can not read cell %d\n", i));
      f <<  "FrameDef(tableCell-" << i << "):###";
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
      break;
    }
    HMWGraphInternal::Table::Cell cell;
    cell.m_row = (int) input->readLong(2);
    cell.m_col = (int) input->readLong(2);
    int span[2];
    for (int j = 0; j < 2; j++)
      span[j] =  (int) input->readLong(2);
    cell.m_span = Vec2i(span[0], span[1]);
    float dim[2];
    for (int j = 0; j < 2; j++)
      dim[j] = float(input->readLong(4))/65536.f;
    cell.m_dim = Vec2f(dim[0], dim[1]);
    // checkme: where the border begin?
    for (int j = 0; j < 4; j++) { // always 1,1,0,0?
      val = input->readLong(2);
      if ((j<2 && val!=1) || (j>=2 && val)) f << "f" << j << "=" << val << ",";
    }
    for (int b = 0; b < 4; b++) { // find _,4000,_,_,1,_, and 1,_,_,_,1,_,
      f << "bord" << b << "=[";
      for (int j = 0; j < 6; j++) {
        val = (long) input->readULong(2);
        if (val) f << std::hex << val << std::dec << ",";
        else f << "_,";
      }
      f << "],";
    }
    cell.m_fileId = (long) input->readULong(4);
    val = input->readLong(2);
    if (val) f << "#f4=" << val << ",";
    cell.m_id = (int) input->readLong(2);
    cell.m_extra = f.str();
    f.str("");
    f <<  "FrameDef(tableCell-" << i << "):" << cell;

    asciiFile.addDelimiter(input->tell(),'|');
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos+80, WPX_SEEK_SET);
  }

  return table;
}

////////////////////////////////////////////////////////////
// send data
////////////////////////////////////////////////////////////

void HMWGraph::flushExtra()
{
  for (size_t i = 0; i < m_state->m_framesList.size(); i++) {
    if (!m_state->m_framesList[i]) continue;
    HMWGraphInternal::Frame const &frame = *m_state->m_framesList[i];
    if (frame.m_parsed)
      continue;
    MWAWPosition pos(Vec2f(0,0),Vec2f(0,0),WPX_POINT);
    pos.setRelativePosition(MWAWPosition::Char);
    send(reinterpret_cast<HMWGraphInternal::BasicGraph const &>(frame), pos);
  }
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
