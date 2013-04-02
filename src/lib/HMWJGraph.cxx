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

#include "MWAWCell.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWPictBasic.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSubDocument.hxx"

#include "HMWJParser.hxx"

#include "HMWJGraph.hxx"

/** Internal: the structures of a HMWJGraph */
namespace HMWJGraphInternal
{
////////////////////////////////////////
//! a cell format in HMWJGraph
struct CellFormat {
public:
  //! constructor
  CellFormat(): m_backColor(MWAWColor::white()), m_borders(), m_extra("") {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, CellFormat const &frmt) {
    if (!frmt.m_backColor.isWhite())
      o << "backColor=" << frmt.m_backColor << ",";
    char const *(what[]) = {"T", "L", "B", "R"};
    for (size_t b = 0; b < frmt.m_borders.size(); b++)
      o << "bord" << what[b] << "=[" << frmt.m_borders[b] << "],";
    o << frmt.m_extra;
    return o;
  }
  //! the background color
  MWAWColor m_backColor;
  //! the border: order defined by MWAWBorder::Pos
  std::vector<MWAWBorder> m_borders;
  //! extra data
  std::string m_extra;
};

////////////////////////////////////////
//! a table cell in a table in HMWJGraph
struct TableCell {
  //! constructor
  TableCell(): m_row(-1), m_col(-1), m_span(1,1), m_zId(0), m_cPos(-1), m_fileId(0), m_formatId(0), m_flags(0), m_extra("") {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, TableCell const &cell);
  //! the row
  int m_row;
  //! the column
  int m_col;
  //! the span ( numRow x numCol )
  Vec2i m_span;
  //! the cell zone id
  long m_zId;
  //! the first character position in m_zId
  long m_cPos;
  //! the file id
  long m_fileId;
  //! the cell format id
  int m_formatId;
  //! the cell flags
  int m_flags;
  //! extra data
  std::string m_extra;
};

std::ostream &operator<<(std::ostream &o, TableCell const &cell)
{
  o << "row=" << cell.m_row << ",";
  o << "col=" << cell.m_col << ",";
  if (cell.m_span.x() != 1 || cell.m_span.y() != 1)
    o << "span=" << cell.m_span << ",";
  if (cell.m_flags&0x80) o << "vAlign=center,";
  if (cell.m_flags&0x100) o << "justify[full],";
  if (cell.m_flags&0x200) o << "line[TL->BR],";
  if (cell.m_flags&0x400) o << "line[BL->TR],";
  if (cell.m_flags&0x800) o << "lock,";
  if (cell.m_flags&0x1000) o << "merge,";
  if (cell.m_flags&0x2000) o << "inactive,";
  if (cell.m_flags&0xC07F)
    o << "#linesFlags=" << std::hex << (cell.m_flags&0xC07F) << std::dec << ",";
  if (cell.m_zId > 0)
    o << "cellId="  << std::hex << cell.m_zId << std::dec << "[" << cell.m_cPos << "],";
  if (cell.m_formatId > 0)
    o << "formatId="  << std::hex << cell.m_formatId << std::dec << ",";
  o << cell.m_extra;
  return o;
}

////////////////////////////////////////
//! Internal: the table of a HMWJGraph
struct Table {
  //! constructor
  Table() : m_rows(1), m_columns(1), m_height(0), m_textFileId(0),
    m_cellsList(), m_rowsDim(), m_columnsDim(), m_cellsId(), m_formatsList(), m_hasExtraLines(false), m_parsed(false) {
  }
  //! destructor
  ~Table() {
  }
  //! returns a cell position ( before any merge cell )
  size_t getCellPos(int row, int col) const {
    return size_t(row*m_columns+col);
  }
  //! the number of row
  int m_rows;
  //! the number of columns
  int m_columns;
  //! the table height
  int m_height;
  //! the text file id
  long m_textFileId;
  //! the list of cells
  std::vector<TableCell> m_cellsList;
  //! the rows dimension ( in points )
  mutable std::vector<float> m_rowsDim;
  //! the columns dimension ( in points )
  mutable std::vector<float> m_columnsDim;
  //! a list of id for each cells
  mutable std::vector<int> m_cellsId;
  //! a list of cell format
  std::vector<CellFormat> m_formatsList;
  //! a flag to know if the table has some extra line
  mutable bool m_hasExtraLines;
  //! true if sent to the listener
  mutable bool m_parsed;
};

////////////////////////////////////////
//! a frame format in HMWJGraph
struct FrameFormat {
public:
  //! constructor
  FrameFormat(): m_lineWidth(1.0), m_extra("") {
    m_color[0] = MWAWColor::black();
    m_color[1] = MWAWColor::white();
    for (int i = 0; i < 4; i++)
      m_intWrap[i] = m_extWrap[i]=1.0;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, FrameFormat const &frmt) {
    if (frmt.m_lineWidth<1.0||frmt.m_lineWidth>1.0)
      o << "lineWidth=" << frmt.m_lineWidth << ",";
    if (!frmt.m_color[0].isBlack())
      o << "lineColor=" << frmt.m_color[0] << ",";
    if (!frmt.m_color[1].isWhite())
      o << "surfColor=" << frmt.m_color[1] << ",";
    bool intDiff=false, extDiff=false;
    for (int i=1; i < 4; i++) {
      if (frmt.m_intWrap[i]<frmt.m_intWrap[0] || frmt.m_intWrap[i]>frmt.m_intWrap[0])
        intDiff=true;
      if (frmt.m_extWrap[i]<frmt.m_extWrap[0] || frmt.m_extWrap[i]>frmt.m_extWrap[0])
        extDiff=true;
    }
    if (intDiff) {
      o << "dim/intWrap/border=[";
      for (int i=0; i < 4; i++)
        o << frmt.m_intWrap[i] << ",";
      o << "],";
    } else
      o << "dim/intWrap/border=" << frmt.m_intWrap[0] << ",";
    if (extDiff) {
      o << "exterior[wrap]=[";
      for (int i=0; i < 4; i++)
        o << frmt.m_extWrap[i] << ",";
      o << "],";
    } else
      o << "exterior[wrap]=" << frmt.m_extWrap[0] << ",";
    o << frmt.m_extra;
    return o;
  }
  //! the line width
  double m_lineWidth;
  //! the line/surface color
  MWAWColor m_color[2];
  //! the interior wrap dim
  double m_intWrap[4];
  //! the exterior wrap dim
  double m_extWrap[4];
  //! extra data
  std::string m_extra;
};
////////////////////////////////////////
//! Internal: the frame header of a HMWJGraph
struct Frame {
  //! constructor
  Frame() : m_type(-1), m_fileId(-1), m_id(-1), m_formatId(0), m_page(0),
    m_pos(), m_baseline(0.f), m_posFlags(0), m_parsed(false), m_extra("") {
  }
  //! destructor
  virtual ~Frame() {
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Frame const &grph);
  //! the graph type
  int m_type;
  //! the file id
  long m_fileId;
  //! the local id
  int m_id;
  //! the format id
  int m_formatId;
  //! the page
  int m_page;
  //! the position
  Box2f m_pos;
  //! the baseline
  float m_baseline;
  //! the graph anchor flags
  int m_posFlags;
  //! true if we have send the data
  mutable bool m_parsed;
  //! an extra string
  std::string m_extra;
};


std::ostream &operator<<(std::ostream &o, Frame const &grph)
{
  switch(grph.m_type) {
  case 0: // text or column
    break;
  case 1:
    o << "header,";
    break;
  case 2:
    o << "footer,";
    break;
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
    o << "comments,"; // memo
    break;
  case 11:
    o << "group";
    break;
  case 12:
    o << "footnote[sep],";
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
  if (grph.m_formatId > 0)
    o << "formatId=" << grph.m_formatId << ",";
  if (grph.m_page) o << "page=" << grph.m_page+1  << ",";
  o << "pos=" << grph.m_pos << ",";
  if (grph.m_baseline < 0 || grph.m_baseline>0) o << "baseline=" << grph.m_baseline << ",";
  int flag = grph.m_posFlags;
  if (flag & 2) o << "inGroup,";
  if (flag & 4) o << "wrap=around,"; // else overlap
  if (flag & 0x40) o << "lock,";
  if (!(flag & 0x80)) o << "transparent,"; // else opaque
  if (flag & 0x39) o << "posFlags=" << std::hex << (flag & 0x39) << std::dec << ",";
  o << grph.m_extra;
  return o;
}

////////////////////////////////////////
//! Internal: a group of a HMWJGraph
struct GroupFrame :  public Frame {
public:
  //! constructor
  GroupFrame(Frame const &orig) : Frame(orig), m_zId(0), m_childsList() {
  }
  //! the group id
  long m_zId;
  //! the child list
  std::vector<long> m_childsList;
};

////////////////////////////////////////
//! Internal: the text frame (basic, header, footer, footnote) of a HMWJGraph
struct TextFrame :  public Frame {
public:
  //! constructor
  TextFrame(Frame const &orig) : Frame(orig), m_width(0), m_cPos(0) {
  }
  //! print local data
  std::string print() const {
    std::stringstream s;
    if (m_width > 0)
      s << "width=" << m_width << ",";
    if (m_cPos)
      s << "cPos[first]=" << m_cPos << ",";
    return s.str();
  }
  //! the zone width
  double m_width;
  //! the first char pos
  long m_cPos;
};

////////////////////////////////////////
//! Internal: the geometrical graph of a HMWJGraph
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
//! Internal: the state of a HMWJGraph
struct State {
  //! constructor
  State() : m_framesList(), m_tablesList(),
    m_numPages(0), m_colorList(), m_patternPercentList() { }
  //! tries to find the lId the frame of a given type
  shared_ptr<Frame> findFrame(int type, int lId) const {
    int actId = 0;
    for (size_t f = 0; f < m_framesList.size(); f++) {
      if (!m_framesList[f] || m_framesList[f]->m_type != type)
        continue;
      if (actId++==lId)
        return m_framesList[f];
    }
    return shared_ptr<Frame>();
  }

  //! returns a color correspond to an id
  bool getColor(int id, MWAWColor &col) {
    initColors();
    if (id < 0 || id >= int(m_colorList.size())) {
      MWAW_DEBUG_MSG(("HMWJGraphInternal::State::getColor: can not find color %d\n", id));
      return false;
    }
    col = m_colorList[size_t(id)];
    return true;
  }
  //! returns a pattern correspond to an id
  bool getPatternPercent(int id, float &percent) {
    initPatterns();
    if (id < 0 || id >= int(m_patternPercentList.size())) {
      MWAW_DEBUG_MSG(("HMWJGraphInternal::State::getPatternPercent: can not find pattern %d\n", id));
      return false;
    }
    percent = m_patternPercentList[size_t(id)];
    return true;
  }
  //! returns a color corresponding to a pattern and a color
  static MWAWColor getColor(MWAWColor col, float pattern) {
    return MWAWColor::barycenter(pattern,col,1.f-pattern,MWAWColor::white());
  }

  //! init the color list
  void initColors();
  //! init the pattenr list
  void initPatterns();

  /** the list of frames */
  std::vector<shared_ptr<Frame> > m_framesList;
  /** the list of table */
  std::vector<Table> m_tablesList;
  int m_numPages /* the number of pages */;
  //! a list colorId -> color
  std::vector<MWAWColor> m_colorList;
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
//! Internal: the subdocument of a HMWJGraph
class SubDocument : public MWAWSubDocument
{
public:
  //! the document type
  enum Type { Picture, FrameInFrame, Text, UnformattedTable, EmptyPicture };
  //! constructor
  SubDocument(HMWJGraph &pars, MWAWInputStreamPtr input, Type type, long id, long subId=0) :
    MWAWSubDocument(pars.m_mainParser, input, MWAWEntry()), m_graphParser(&pars), m_type(type), m_id(id), m_subId(subId), m_pos() {}

  //! constructor
  SubDocument(HMWJGraph &pars, MWAWInputStreamPtr input, MWAWPosition pos, Type type, long id, int subId=0) :
    MWAWSubDocument(pars.m_mainParser, input, MWAWEntry()), m_graphParser(&pars), m_type(type), m_id(id), m_subId(subId), m_pos(pos) {}

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
  HMWJGraph *m_graphParser;
  //! the zone type
  Type m_type;
  //! the zone id
  long m_id;
  //! the zone subId ( for table cell )
  long m_subId;
  //! the position in a frame
  MWAWPosition m_pos;

private:
  SubDocument(SubDocument const &orig);
  SubDocument &operator=(SubDocument const &orig);
};

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("HMWJGraphInternal::SubDocument::parse: no listener\n"));
    return;
  }
  assert(m_graphParser);

  long pos = m_input->tell();
  m_input->seek(pos, WPX_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_graphParser != sDoc->m_graphParser) return true;
  if (m_type != sDoc->m_type) return true;
  if (m_id != sDoc->m_id) return true;
  if (m_subId != sDoc->m_subId) return true;
  if (m_pos != sDoc->m_pos) return true;
  return false;
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
HMWJGraph::HMWJGraph(HMWJParser &parser) :
  m_parserState(parser.getParserState()), m_state(new HMWJGraphInternal::State),
  m_mainParser(&parser)
{
}

HMWJGraph::~HMWJGraph()
{ }

int HMWJGraph::version() const
{
  return m_parserState->m_version;
}

bool HMWJGraph::getColor(int colId, int patternId, MWAWColor &color) const
{
  if (!m_state->getColor(colId, color) ) {
    MWAW_DEBUG_MSG(("HMWJGraph::getColor: can not find color for id=%d\n", colId));
    return false;
  }
  float percent = 1.0;
  if (!m_state->getPatternPercent(patternId, percent) ) {
    MWAW_DEBUG_MSG(("HMWJGraph::getColor: can not find pattern for id=%d\n", patternId));
    return false;
  }
  color = m_state->getColor(color, percent);
  return true;
}

int HMWJGraph::numPages() const
{
  if (m_state->m_numPages)
    return m_state->m_numPages;
  int nPages = 0;
  m_state->m_numPages = nPages;
  return nPages;
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool HMWJGraph::readFrames(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("HMWJGraph::readFrames: called without any entry\n"));
    return false;
  }
  if (entry.length() <= 8) {
    MWAW_DEBUG_MSG(("HMWJGraph::readFrames: the entry seems too short\n"));
    return false;
  }

  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &asciiFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  entry.setParsed(true);

  long pos = entry.begin()+8; // skip header
  long endPos = entry.end();
  input->seek(pos, WPX_SEEK_SET);

  // first read the header
  f << entry.name() << "[header]:";
  HMWJZoneHeader mainHeader(true);
  if (!m_mainParser->readClassicHeader(mainHeader,endPos) || mainHeader.m_fieldSize != 4 ||
      16+12+mainHeader.m_n*4 > mainHeader.m_length) {
    MWAW_DEBUG_MSG(("HMWJGraph::readFrames: can not read the header\n"));
    f << "###sz=" << mainHeader.m_length;
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    return false;
  }
  long headerEnd=pos+4+mainHeader.m_length;
  long val;
  for (int i = 0; i < 2; i++) {
    val = (long) input->readULong(4);
    f << "id" << i << "=" << std::hex << val << std::dec << ",";
  }
  for (int i = 0; i < 2; i++) { // f0:small number, 0
    val = input->readLong(2);
    if (val)
      f << "f" << i << "=" << val << ",";
  }
  f << "listIds=[";
  for (int i = 0; i < mainHeader.m_n; i++) {
    val = (long) input->readULong(4);
    f << std::hex << val << std::dec << ",";
  }
  f << std::dec << "],";
  if (input->tell()!=headerEnd) {
    asciiFile.addDelimiter(input->tell(),'|');
    input->seek(headerEnd, WPX_SEEK_SET);
  }
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  // the data
  m_state->m_framesList.resize(size_t(mainHeader.m_n));
  for (int i = 0; i < mainHeader.m_n; i++) {
    pos = input->tell();
    shared_ptr<HMWJGraphInternal::Frame> frame=readFrame(i);
    if (!frame) {
      f << "###";
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
      return false;
    }
    m_state->m_framesList[size_t(i)]=frame;
  }

  // normally there remains 2 block, ...

  // block 0
  pos = input->tell();
  f.str("");
  f << entry.name() << "-Format:";
  HMWJZoneHeader header(false);
  if (!m_mainParser->readClassicHeader(header,endPos) || header.m_fieldSize!=48) {
    MWAW_DEBUG_MSG(("HMWJGraph::readFrames: can not read auxilliary block A\n"));
    f << "###" << header;
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    return false;
  }
  long zoneEnd=pos+4+header.m_length;
  f << header;
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  for (int i = 0; i < header.m_n; i++) {
    HMWJGraphInternal::FrameFormat format;
    pos=input->tell();
    f.str("");
    val = input->readLong(2);
    if (val != -2)
      f << "f0=" << val << ",";
    val = (long) input->readULong(2);
    if (val)
      f << "f1=" << std::hex << val << std::dec << ",";
    for (int j =0; j < 4; j++)
      format.m_intWrap[j] = double(input->readLong(4))/65536.;
    for (int j =0; j < 4; j++)
      format.m_extWrap[j] = double(input->readLong(4))/65536.;
    format.m_lineWidth= double(input->readLong(4))/65536.;
    val = (int) input->readULong(1);
    if (val) f << "#lineFlags=" << val << ",";
    for (int j = 0; j < 2; j++) {
      int color = (int) input->readULong(1);
      MWAWColor col = j==0 ? MWAWColor::black() : MWAWColor::white();
      if (!m_state->getColor(color, col))
        f << "#color[" << j << "]=" << color << ",";
      int pattern = (int) input->readULong(1);
      float patPercent = 1.0;
      if (!m_state->getPatternPercent(pattern, patPercent))
        f << "#pattern[" << j << "]=" << pattern << ",";
      format.m_color[j]= m_state->getColor(col, patPercent);
    }
    for (int j = 0; j < 3; j++) { // always 0
      val = (int) input->readULong(1);
      if (val) f << "g" << j << "=" << val << ",";
    }
    format.m_extra=f.str();
    f.str("");
    f << entry.name() << "-F" << i << ":" << format;
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos+48, WPX_SEEK_SET);
  }
  input->seek(zoneEnd, WPX_SEEK_SET);

  // block B
  pos = input->tell();
  f.str("");
  f << entry.name() << "-B:";
  header=HMWJZoneHeader(false);
  if (!m_mainParser->readClassicHeader(header,endPos) || header.m_fieldSize!=8 ||
      16+2+header.m_n*8 > header.m_length) {
    MWAW_DEBUG_MSG(("HMWJGraph::readFrames: can not read auxilliary block B\n"));
    f << "###" << header;
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    return false;
  }
  for (int i = 0; i < 2; i++) { // f0=1|3|4=N?
    val = input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  f << "unk=[";
  for (int i = 0; i < header.m_n; i++) {
    f << "[";
    for (int j = 0; j < 2; j++) { // always 0?
      val = input->readLong(2);
      if (val) f << val << ",";
      else f << "_,";
    }
    f << std::hex << input->readULong(4) << std::dec; // id
    f << "],";
  }
  zoneEnd=pos+4+header.m_length;
  f << header;
  input->seek(zoneEnd, WPX_SEEK_SET);
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  // and for each n, a list
  for (int i = 0; i < header.m_n; i++) {
    pos = input->tell();
    f.str("");
    f << entry.name() << "-B" << i << ":";
    HMWJZoneHeader lHeader(false);
    if (!m_mainParser->readClassicHeader(lHeader,endPos) || lHeader.m_fieldSize!=4) {
      MWAW_DEBUG_MSG(("HMWJGraph::readFrames: can not read auxilliary block B%d\n",i));
      f << "###" << lHeader;
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
      return false;
    }
    f << "listId?=[" << std::hex;
    for (int j = 0; j < lHeader.m_n; j++) {
      val = (long) input->readULong(4);
      f << val << ",";
    }
    f << std::dec << "],";

    zoneEnd=pos+4+lHeader.m_length;
    f << header;
    input->seek(zoneEnd, WPX_SEEK_SET);
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
  }

  asciiFile.addPos(endPos);
  asciiFile.addNote("_");
  pos = input->tell();
  if (pos!=endPos) {
    MWAW_DEBUG_MSG(("HMWJGraph::readFrames: find unexpected end data\n"));
    f.str("");
    f << entry.name() << "###:";
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
  }
  return true;
}

shared_ptr<HMWJGraphInternal::Frame> HMWJGraph::readFrame(int id)
{
  shared_ptr<HMWJGraphInternal::Frame> res;
  HMWJGraphInternal::Frame graph;
  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &asciiFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  long pos = input->tell();
  long len = (long) input->readULong(4);
  long endPos = pos+4+len;
  if (len < 32 || !m_mainParser->isFilePos(endPos)) {
    MWAW_DEBUG_MSG(("HMWJGraph::readFrame: can not read the frame length\n"));
    input->seek(pos, WPX_SEEK_SET);
    return res;
  }

  int fl = (int) input->readULong(1);
  graph.m_type=(fl>>4);
  f << "f0=" << std::hex << (fl&0xf) << std::dec << ",";
  int val;
  /* fl0=[0|1|2|3|4|6|8|9|a|b|c][2|6], fl1=0|1|20|24,
     fl2=0|8|c|e|10|14|14|40|8a, fl3=0|10|80|c0 */
  for (int i = 1; i < 4; i++) {
    val = (int) input->readULong(1);
    if (val) f << "fl" << i << "=" << std::hex << val << std::dec << ",";
  }
  graph.m_page = (int) input->readLong(2);
  graph.m_formatId = (int) input->readULong(2);
  float dim[4];
  for (int i = 0; i < 4; i++)
    dim[i] = float(input->readLong(4))/65536.f;
  graph.m_pos = Box2f(Vec2f(dim[0],dim[1]),Vec2f(dim[2],dim[3]));
  graph.m_id = (int) input->readLong(2); // check me
  val = (int) input->readLong(2);
  if (val) f << "f1=" << val << ",";
  graph.m_baseline  = float(input->readLong(4))/65536.f;
  graph.m_extra = f.str();

  f.str("");
  f << "FrameDef-" << id << ":" << graph;
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  switch(graph.m_type) {
  case 0: // text
  case 1: // header
  case 2: // footer
  case 3: // footnote
    res=readTextData(graph, endPos);
    break;
  case 8:
    res=readBasicGraph(graph, endPos);
    break;
  case 11:
    if (len < 36) {
      MWAW_DEBUG_MSG(("HMWJGraph::readFrame: can not read the group id\n"));
      break;
    } else {
      HMWJGraphInternal::GroupFrame *group =
        new HMWJGraphInternal::GroupFrame(graph);
      res.reset(group);
      pos =input->tell();
      group->m_zId = (long) input->readULong(4);
      f.str("");
      f << "FrameDef-group:zId=" << std::hex << group->m_zId << std::dec << ",";
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
      break;
    }
  default:
    break;
  }
  if (!res)
    res.reset(new HMWJGraphInternal::Frame(graph));
  if (input->tell() != endPos)
    asciiFile.addDelimiter(input->tell(),'|');
  input->seek(endPos, WPX_SEEK_SET);
  return res;
}

bool HMWJGraph::readGroupData(MWAWEntry const &entry, int actZone)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("HMWJGraph::readGroupData: called without any entry\n"));
    return false;
  }
  if (entry.length() == 8) {
    MWAW_DEBUG_MSG(("HMWJGraph::readGroupData: find an empty zone\n"));
    entry.setParsed(true);
    return true;
  }
  if (entry.length() < 12) {
    MWAW_DEBUG_MSG(("HMWJGraph::readGroupData: the entry seems too short\n"));
    return false;
  }

  shared_ptr<HMWJGraphInternal::Frame> frame =
    m_state->findFrame(11, actZone);
  std::vector<long> dummyList;
  std::vector<long> *idsList=&dummyList;
  if (!frame) {
    MWAW_DEBUG_MSG(("HMWJGraph::readGroupData: can not find group %d\n", actZone));
  } else {
    HMWJGraphInternal::GroupFrame *group =
      reinterpret_cast<HMWJGraphInternal::GroupFrame *>(frame.get());
    idsList = &group->m_childsList;
  }

  long pos = entry.begin()+8; // skip header
  long endPos = entry.end();

  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &asciiFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  entry.setParsed(true);
  input->seek(pos, WPX_SEEK_SET);
  // first read the header
  f << entry.name() << "[header]:";
  HMWJZoneHeader mainHeader(true);
  if (!m_mainParser->readClassicHeader(mainHeader,endPos) || mainHeader.m_fieldSize!=4) {
    MWAW_DEBUG_MSG(("HMWJGraph::readGroupData: can not read an entry\n"));
    f << "###sz=" << mainHeader.m_length;
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    return false;
  }
  long headerEnd=pos+4+mainHeader.m_length;
  f << mainHeader;
  long val;
  f << "listId=[" << std::hex;
  idsList->resize(size_t(mainHeader.m_n), 0);
  for (int i = 0; i < mainHeader.m_n; i++) {
    val = (long) input->readULong(4);
    (*idsList)[size_t(i)]=val;
    f << val << ",";
  }
  f << std::dec << "],";
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  if (input->tell()!=headerEnd) {
    asciiFile.addDelimiter(input->tell(),'|');
    input->seek(headerEnd, WPX_SEEK_SET);
  }

  pos = input->tell();
  if (pos!=endPos) {
    f.str("");
    f << entry.name() << "[last]:###";
    MWAW_DEBUG_MSG(("HMWJGraph::readGroupData: find unexpected end of data\n"));
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
  }

  return true;
}

// try to read the graph data
bool HMWJGraph::readGraphData(MWAWEntry const &entry, int actZone)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("HMWJGraph::readGraphData: called without any entry\n"));
    return false;
  }
  if (entry.length() < 12) {
    MWAW_DEBUG_MSG(("HMWJGraph::readGraphData: the entry seems too short\n"));
    return false;
  }

  long pos = entry.begin()+8; // skip header
  long endPos = entry.end();

  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &asciiFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  entry.setParsed(true);
  input->seek(pos, WPX_SEEK_SET);
  // first read the header
  f << entry.name() << "[header]:";
  HMWJZoneHeader mainHeader(false);
  if (!m_mainParser->readClassicHeader(mainHeader,endPos) || mainHeader.m_fieldSize!=8) {
    // sz=12 is ok, means no data
    if (entry.length() != 12) {
      MWAW_DEBUG_MSG(("HMWJGraph::readGraphData: can not read an entry\n"));
      f << "###sz=" << mainHeader.m_length;
    }
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    return false;
  }
  long headerEnd=pos+4+mainHeader.m_length;
  f << mainHeader;

  std::vector<Vec2f> lVertices(size_t(mainHeader.m_n));
  f << "listPt=[";
  for (int i = 0; i < mainHeader.m_n; i++) {
    pos = input->tell();
    float point[2];
    for (int j = 0; j < 2; j++)
      point[j] = float(input->readLong(4))/65536.f;
    Vec2f pt(point[0], point[1]);
    lVertices[size_t(i)]=pt;
    f << pt << ",";
    input->seek(pos+8, WPX_SEEK_SET);
  }
  f << "],";

  shared_ptr<HMWJGraphInternal::Frame> frame = m_state->findFrame(8, actZone);
  if (!frame) {
    MWAW_DEBUG_MSG(("HMWJGraph::readGraphData: can not find basic graph %d\n", actZone));
  } else {
    HMWJGraphInternal::BasicGraph *graph =
      reinterpret_cast<HMWJGraphInternal::BasicGraph *>(frame.get());
    if (graph->m_graphType != 6) {
      MWAW_DEBUG_MSG(("HMWJGraph::readGraphData: basic graph %d is not a polygon\n", actZone));
    } else
      graph->m_listVertices = lVertices;
  }

  asciiFile.addPos(entry.begin()+8);
  asciiFile.addNote(f.str().c_str());

  if (headerEnd!=endPos) {
    f.str("");
    f << entry.name() << "[last]:###";
    MWAW_DEBUG_MSG(("HMWJGraph::readGraphData: find unexpected end of data\n"));
    asciiFile.addPos(headerEnd);
    asciiFile.addNote(f.str().c_str());
  }

  return true;
}

// try to read the picture
bool HMWJGraph::readPicture(MWAWEntry const &entry, int /*actZone*/)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("HMWJGraph::readPicture: called without any entry\n"));
    return false;
  }
  if (entry.length() < 12) {
    MWAW_DEBUG_MSG(("HMWJGraph::readPicture: the entry seems too short\n"));
    return false;
  }

  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugStream f;
  entry.setParsed(true);

  long pos = entry.begin()+8; // skip header
  input->seek(pos, WPX_SEEK_SET);
  long sz=(long) input->readULong(4);
  if (sz+12 != entry.length()) {
    MWAW_DEBUG_MSG(("HMWJGraph::readPicture: the entry sz seems bad\n"));
    return false;
  }
  f << "pictSz=" << sz;
#ifdef DEBUG_WITH_FILES
  libmwaw::DebugFile &asciiFile = m_parserState->m_asciiFile;
  if (1) {
    f.str("");

    WPXBinaryData data;
    input->readDataBlock(sz, data);

    static int volatile pictName = 0;
    f << "Pict" << ++pictName << ".pct";
    libmwaw::Debug::dumpFile(data, f.str().c_str());
    asciiFile.skipZone(entry.begin()+12, entry.end()-1);
  }
#endif
  return true;
}

// table
bool HMWJGraph::readTable(MWAWEntry const &entry, int /*actZone*/)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("HMWJGraph::readTable: called without any entry\n"));
    return false;
  }
  if (entry.length() == 8) {
    MWAW_DEBUG_MSG(("HMWJGraph::readTable: find an empty zone\n"));
    entry.setParsed(true);
    return true;
  }
  if (entry.length() < 12) {
    MWAW_DEBUG_MSG(("HMWJGraph::readTable: the entry seems too short\n"));
    return false;
  }
  long pos = entry.begin()+8; // skip header
  long endPos = entry.end();

  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &asciiFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f, f2;
  entry.setParsed(true);
  input->seek(pos, WPX_SEEK_SET);
  // first read the header
  f << entry.name() << "[header]:";
  HMWJZoneHeader mainHeader(true);
  if (!m_mainParser->readClassicHeader(mainHeader,endPos) || mainHeader.m_fieldSize!=4 ||
      mainHeader.m_length < 16+12+4*mainHeader.m_n) {
    MWAW_DEBUG_MSG(("HMWJGraph::readTable: can not read an entry\n"));
    f << "###sz=" << mainHeader.m_length;
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    return false;
  }
  long headerEnd=pos+4+mainHeader.m_length;
  f << mainHeader;
  m_state->m_tablesList.push_back(HMWJGraphInternal::Table());
  HMWJGraphInternal::Table &table = m_state->m_tablesList.back();
  table.m_rows = (int) input->readULong(1);
  table.m_columns = (int) input->readULong(1);
  f << "dim=" << table.m_rows << "x" << table.m_columns << ",";
  long val;
  for (int i = 0; i < 4; i++) { // f0=4|5|7|8|9, f1=1|7|107, f2=3|4|5|6, f3=0
    val = (long) input->readULong(2);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  table.m_height = (int) input->readLong(2);
  f << "h=" << table.m_height << ",";
  f << "listId=[" << std::hex;
  std::vector<long> listIds;
  for (int i = 0; i < mainHeader.m_n; i++) {
    val = (long) input->readULong(4);
    listIds.push_back(val);
    f << val << ",";
  }
  f << std::dec << "],";
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  if (input->tell()!=headerEnd) {
    asciiFile.addDelimiter(input->tell(),'|');
    input->seek(headerEnd, WPX_SEEK_SET);
  }

  // first read the row
  for (int i = 0; i < mainHeader.m_n; i++) {
    pos = input->tell();
    f.str("");
    f << entry.name() << "-row" << i << ":";
    HMWJZoneHeader header(false);
    if (!m_mainParser->readClassicHeader(header,endPos) || header.m_fieldSize!=16) {
      MWAW_DEBUG_MSG(("HMWJGraph::readTable: can not read zone %d\n", i));
      f << "###" << header;
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
      if (header.m_length<16 || pos+4+header.m_length>endPos)
        return false;
      input->seek(pos+4+header.m_length, WPX_SEEK_SET);
      continue;
    }
    long zoneEnd=pos+4+header.m_length;
    f << header;

    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());

    // the different cells in a row
    for (int j = 0; j < header.m_n; j++) {
      pos = input->tell();
      f.str("");
      HMWJGraphInternal::TableCell cell;
      cell.m_row = i;
      cell.m_col = j;
      cell.m_cPos = (long) input->readULong(4);
      cell.m_zId = (long) input->readULong(4);
      cell.m_flags = (int) input->readULong(2);
      val = input->readLong(2);
      if (val) f << "#f0=" << val << ",";
      cell.m_formatId = (int) input->readLong(2);
      int dim[2]; // for merge, inactive -> the other limit cell
      for (int k=0; k < 2; k++)
        dim[k]=(int)input->readULong(1);
      if (cell.m_flags & 0x1000) {
        cell.m_span[0] = dim[0]+1-i;
        cell.m_span[1] = dim[1]+1-j;
      }
      cell.m_extra = f.str();
      table.m_cellsList.push_back(cell);
      f.str("");
      f << entry.name() << "-cell:" << cell;
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
      input->seek(pos+16, WPX_SEEK_SET);
    }

    if (input->tell() != zoneEnd) {
      asciiFile.addDelimiter(input->tell(),'|');
      input->seek(zoneEnd, WPX_SEEK_SET);
    }
  }
  asciiFile.addPos(endPos);
  asciiFile.addNote("_");
  if (input->tell()==endPos) {
    MWAW_DEBUG_MSG(("HMWJGraph::readTable: can not find the 3 last blocks\n"));
    return true;
  }

  for (int i = 0; i < 2; i++) {
    pos = input->tell();
    f.str("");
    f << entry.name() << "-" << (i==0 ? "rowY" : "colX") << ":";
    HMWJZoneHeader header(false);
    if (!m_mainParser->readClassicHeader(header,endPos) || header.m_fieldSize != 4) {
      MWAW_DEBUG_MSG(("HMWJGraph::readTable: can not read zone %d\n", i));
      f << "###" << header;
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
      if (header.m_length<16 || pos+4+header.m_length>endPos)
        return false;
      input->seek(pos+4+header.m_length, WPX_SEEK_SET);
      continue;
    }
    long zoneEnd=pos+4+header.m_length;
    f << header;
    std::vector<float> &dim = i==0 ? table.m_rowsDim : table.m_columnsDim;
    f << "pos=[";
    float prevPos = 0.;
    for (int j = 0; j < header.m_n; j++) {
      float cPos = float(input->readULong(4))/65536.f;
      f << cPos << ",";
      if (j!=0)
        dim.push_back(cPos-prevPos);
      prevPos=cPos;
    }
    f << "],";
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(zoneEnd, WPX_SEEK_SET);
  }

  // finally the format
  readTableFormatsList(table, endPos);

  if (input->tell() != endPos) {
    MWAW_DEBUG_MSG(("HMWJGraph::readTable: find unexpected last block\n"));
    pos = input->tell();
    f.str("");
    f << entry.name() << "-###:";
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
  }

  return true;
}

bool HMWJGraph::readTableFormatsList(HMWJGraphInternal::Table &table, long endPos)
{
  table.m_formatsList.clear();

  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &asciiFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f, f2;

  long pos = input->tell();
  f.str("");
  f << "Table-format:";
  HMWJZoneHeader header(false);
  if (!m_mainParser->readClassicHeader(header,endPos) || header.m_fieldSize != 40) {
    MWAW_DEBUG_MSG(("HMWJGraph::readTableFormatsList: can not read format\n"));
    f << "###" << header;
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  long zoneEnd=pos+4+header.m_length;
  f << header;
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  long val;
  table.m_formatsList.resize(size_t(header.m_n));
  for (int i = 0; i < header.m_n; i++) {
    HMWJGraphInternal::CellFormat format;
    pos = input->tell();
    f.str("");
    val = input->readLong(2); // always -2
    if (val != -2)
      f << "f0=" << val << ",";
    val = (long) input->readULong(2); // 0|2004|51|1dd4
    if (val)
      f << "#f1=" << std::hex << val << std::dec << ",";

    int color, pattern;
    float patPercent;
    format.m_borders.resize(4);
    static char const *(what[]) = {"T", "L", "B", "R"};
    static size_t const which[] = { MWAWBorder::Top, MWAWBorder::Left, MWAWBorder::Bottom, MWAWBorder::Right };
    for (int b=0; b < 4; b++) {
      f2.str("");
      MWAWBorder border;
      border.m_width=float(input->readLong(4))/65536.f;
      int type = int(input->readLong(1));
      switch(type) {
      case 0: // solid
        break;
      case 1:
        border.m_type = MWAWBorder::Double;
        break;
      case 2:
        border.m_type = MWAWBorder::Double;
        f2 << "bottom[w=2],";
        break;
      case 3:
        border.m_type = MWAWBorder::Double;
        f2 << "top[w=2],";
        break;
      default:
        f2 << "#style=" << type << ",";
        break;
      }
      color = (int) input->readULong(1);
      MWAWColor col = MWAWColor::black();
      if (!m_state->getColor(color, col))
        f2 << "#color=" << color << ",";
      pattern = (int) input->readULong(1);
      patPercent = 1.0;
      if (!m_state->getPatternPercent(pattern, patPercent))
        f2 << "#pattern=" << pattern << ",";
      border.m_color = m_state->getColor(col, patPercent);
      val = (long) input->readULong(1);
      if (val) f2 << "unkn=" << val << ",";

      format.m_borders[which[b]] = border;
      if (f2.str().length())
        f << "bord" << what[b] << "=[" << f2.str() << "],";
    }
    color = (int) input->readULong(1);
    MWAWColor backCol = MWAWColor::white();
    if (!m_state->getColor(color, backCol))
      f << "#backcolor=" << color << ",";
    pattern = (int) input->readULong(1);
    patPercent = 1.0;
    if (!m_state->getPatternPercent(pattern, patPercent))
      f << "#backPattern=" << pattern << ",";
    format.m_backColor = m_state->getColor(backCol, patPercent);
    format.m_extra = f.str();
    table.m_formatsList[size_t(i)]=format;
    f.str("");
    f << "Table-format" << i << ":" << format;
    asciiFile.addDelimiter(input->tell(),'|');
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos+header.m_fieldSize, WPX_SEEK_SET);
  }
  input->seek(zoneEnd, WPX_SEEK_SET);
  return true;
}


////////////////////////////////////////////////////////////
// send data to a listener
////////////////////////////////////////////////////////////

// ----- table
bool HMWJGraph::updateTable(HMWJGraphInternal::Table const &table)
{
  if (table.m_cellsId.size()) return true;

  int nRows = table.m_rows;
  int nColumns = table.m_columns;
  size_t nCells = table.m_cellsList.size();
  if (!nRows || !nColumns || !nCells || int(nCells) > nRows*nColumns) {
    MWAW_DEBUG_MSG(("HMWJGraph::updateTable: find an odd table\n"));
    return false;
  }
  if (table.m_rowsDim.size() < size_t(nRows) ||
      table.m_columnsDim.size() < size_t(nColumns)) {
    MWAW_DEBUG_MSG(("HMWJGraph::updateTable: can not determine table dimensions\n"));
    return false;
  }

  table.m_cellsId.resize(size_t(nRows*nColumns),-1);
  for (size_t i=0; i < nCells; i++) {
    HMWJGraphInternal::TableCell const &cell=table.m_cellsList[i];
    if (cell.m_flags&0x2000)
      continue;
    if (cell.m_flags&0x600) table.m_hasExtraLines = true;
    for (int r = cell.m_row; r < cell.m_row + cell.m_span[0]; r++) {
      if (r >= nRows) {
        MWAW_DEBUG_MSG(("HMWJGraph::updateTable: find a bad row cell span\n"));
        continue;
      }
      for (int c = cell.m_col; c < cell.m_col + cell.m_span[1]; c++) {
        if (c >= nColumns) {
          MWAW_DEBUG_MSG(("HMWJGraph::updateTable: find a bad col cell span\n"));
          continue;
        }
        size_t cPos = table.getCellPos(r,c);
        if (table.m_cellsId[cPos]!=-1) {
          MWAW_DEBUG_MSG(("HMWJGraph::updateTable: oops find some cell in this position\n"));
          table.m_cellsId.resize(0);
          return false;
        }
        table.m_cellsId[cPos] = int(i);
      }
    }
  }
  return true;
}

bool HMWJGraph::sendTableCell(HMWJGraphInternal::TableCell const &cell,
                              std::vector<HMWJGraphInternal::CellFormat> const &lFormat)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener)
    return true;
  if (cell.m_flags&0x2000) {
    MWAW_DEBUG_MSG(("HMWJGraph::sendTableCell: call on inactive file\n"));
    return false;
  }

  WPXPropertyList pList;
  MWAWCell fCell;
  fCell.position() = Vec2i(cell.m_col,cell.m_row);
  Vec2i span = cell.m_span;
  if (span[0]<1) span[0]=1;
  if (span[1]<1) span[1]=1;
  fCell.setNumSpannedCells(Vec2i(span[1],span[0]));

  if (cell.m_flags&0x80) fCell.setVAlignement(MWAWCell::VALIGN_CENTER);
  if (cell.m_formatId >= 0 && size_t(cell.m_formatId) < lFormat.size()) {
    HMWJGraphInternal::CellFormat const &format = lFormat[size_t(cell.m_formatId)];
    fCell.setBackgroundColor(format.m_backColor);
    static int const (wh[]) = { MWAWBorder::LeftBit,  MWAWBorder::RightBit, MWAWBorder::TopBit, MWAWBorder::BottomBit};
    for (size_t b = 0; b < format.m_borders.size(); b++)
      fCell.setBorders(wh[b], format.m_borders[b]);
  } else {
    static bool first = true;
    if (first) {
      MWAW_DEBUG_MSG(("HMWJGraph::sendTableCell: can not find the format\n"));
      first = false;
    }
  }

  listener->openTableCell(fCell, pList);
  if (cell.m_zId)
    m_mainParser->sendText(cell.m_zId, cell.m_cPos);
  listener->closeTableCell();

  return true;
}

bool HMWJGraph::sendPreTableData(HMWJGraphInternal::Table const &table)
{
  if (!m_parserState->m_listener)
    return true;
  if (!updateTable(table) || !table.m_hasExtraLines)
    return false;

  int nRows = table.m_rows;
  int nColumns = table.m_columns;
  size_t nCells = table.m_cellsList.size();
  std::vector<float> rowsPos, columnsPos;
  rowsPos.resize(size_t(nRows+1)+1);
  rowsPos[0] = 0;
  for (size_t r = 0; r < size_t(nRows); r++)
    rowsPos[r+1] = rowsPos[r]+table.m_rowsDim[r];
  columnsPos.resize(size_t(nColumns+1)+1);
  columnsPos[0] = 0;
  for (size_t c = 0; c < size_t(nColumns); c++)
    columnsPos[c+1] = columnsPos[c]+table.m_columnsDim[c];

  for (size_t c = 0; c < nCells; c++) {
    HMWJGraphInternal::TableCell const &cell= table.m_cellsList[c];
    if (!(cell.m_flags&0x600)) continue;
    if (cell.m_row+cell.m_span[0] > nRows ||
        cell.m_col+cell.m_span[1] > nColumns)
      continue;
    Box2f box;
    box.setMin(Vec2f(columnsPos[size_t(cell.m_col)], rowsPos[size_t(cell.m_row)]));
    box.setMax(Vec2f(columnsPos[size_t(cell.m_col+cell.m_span[1])],
                     rowsPos[size_t(cell.m_row+cell.m_span[0])]));

    shared_ptr<MWAWPictLine> lines[2];
    if (cell.m_flags & 0x200)
      lines[0].reset(new MWAWPictLine(Vec2f(0,0), box.size()));
    if (cell.m_flags & 0x400)
      lines[1].reset(new MWAWPictLine(Vec2f(0,box.size()[1]), Vec2f(box.size()[0], 0)));

    for (int i = 0; i < 2; i++) {
      if (!lines[i]) continue;

      WPXBinaryData data;
      std::string type;
      if (!lines[i]->getBinary(data,type)) continue;

      MWAWPosition pos(box[0], box.size(), WPX_POINT);
      pos.setRelativePosition(MWAWPosition::Frame);
      pos.setOrder(-1);
      m_parserState->m_listener->insertPicture(pos, data, type);
    }
  }
  return true;
}
bool HMWJGraph::sendTableUnformatted(HMWJGraphInternal::Table const &table)
{
  if (!m_parserState->m_listener)
    return true;
  table.m_parsed = true;
  for (size_t c = 0; c < table.m_cellsList.size(); c++) {
    HMWJGraphInternal::TableCell const &cell= table.m_cellsList[c];
    if (cell.m_flags&0x2000)
      continue;
    if (cell.m_zId)
      m_mainParser->sendText(cell.m_zId, cell.m_cPos);
  }
  return true;
}

bool HMWJGraph::sendTable(HMWJGraphInternal::Table const &table)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener)
    return true;
  table.m_parsed = true;
  if (!updateTable(table)) {
    // ok no other choice here
    sendTableUnformatted(table);
    return true;
  }
  int nRows = table.m_rows;
  int nColumns = table.m_columns;

  // ok send the data
  listener->openTable(table.m_columnsDim, WPX_POINT);
  for (size_t r = 0; r < size_t(nRows); r++) {
    listener->openTableRow(table.m_rowsDim[r], WPX_POINT);
    for (size_t c = 0; c < size_t(nColumns); c++) {
      size_t cPos = table.getCellPos(int(r),int(c));
      int id = table.m_cellsId[cPos];
      if (id == -1) {
        listener->addEmptyTableCell(Vec2i(int(c), int(r)));
        continue;
      }

      HMWJGraphInternal::TableCell const &cell=table.m_cellsList[size_t(table.m_cellsId[cPos])];
      if (int(r) != cell.m_row || int(c) != cell.m_col) continue;

      sendTableCell(cell, table.m_formatsList);
    }
    listener->closeTableRow();
  }
  listener->closeTable();

  return true;
}

////////////////////////////////////////////////////////////
// low level
////////////////////////////////////////////////////////////

// try to read a basic text zone
shared_ptr<HMWJGraphInternal::TextFrame> HMWJGraph::readTextData(HMWJGraphInternal::Frame const &header, long endPos)
{
  shared_ptr<HMWJGraphInternal::TextFrame> text;

  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &asciiFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  long pos = input->tell();
  if (endPos<pos+20) {
    MWAW_DEBUG_MSG(("HMWJGraph::readTextData: the zone seems too short\n"));
    return text;
  }
  text.reset(new HMWJGraphInternal::TextFrame(header));
  text->m_width = double(input->readLong(4))/65536.;
  long val = input->readLong(2); // small number between 1 and 0x17
  if (val!=1)
    f << "f0=" << val << ",";
  val = input->readLong(2);// always 0?
  if (val)
    f << "f1=" << val << ",";
  text->m_cPos = (long) input->readULong(4);
  for (int i=0; i < 2; i++) {
    val = (long) input->readULong(4);
    f << "id" << i << "=" << val << ",";
  }
  std::string extra;
  text->m_extra += extra;
  f.str("");
  f << "FrameDef(Text-data):" << text->print() << extra;
  if (input->tell() != endPos)
    asciiFile.addDelimiter(input->tell(),'|');
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  return text;
}

// try to read a small graphic
shared_ptr<HMWJGraphInternal::BasicGraph> HMWJGraph::readBasicGraph(HMWJGraphInternal::Frame const &header, long endPos)
{
  shared_ptr<HMWJGraphInternal::BasicGraph> graph;

  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &asciiFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  long pos = input->tell();
  if (endPos<pos+36) {
    MWAW_DEBUG_MSG(("HMWJGraph::readBasicGraph: the zone seems too short\n"));
    return graph;
  }

  graph.reset(new HMWJGraphInternal::BasicGraph(header));
  long val = (int) input->readULong(1);
  graph->m_graphType = (int) (val>>4);
  int flag = int(val&0xf);
  bool isLine = graph->m_graphType==0 || graph->m_graphType==3;
  bool ok = graph->m_graphType >= 0 && graph->m_graphType < 7;
  if (isLine) {
    graph->m_arrowsFlag = (flag>>2)&0x3;
    flag &= 0x3;
  }
  int flag1 = (int) input->readULong(1);
  if (graph->m_graphType==5) { // arc
    int transf = (int) (2*(flag&1) && (flag1>>7));
    int decal = (transf%2) ? 4-transf : transf;
    graph->m_angles[0] = decal*90;
    graph->m_angles[1] = graph->m_angles[0]+90;
    flag &= 0xe;
    flag1 &= 0x7f;
  }
  if (flag) f << "#fl0=" << std::hex << flag << std::dec << ",";
  if (flag1) f << "#fl1=" << std::hex << flag1 << std::dec << ",";
  val = input->readLong(2); // always 0
  if (val) f << "f0=" << val << ",";

  val = input->readLong(4);
  if (graph->m_graphType==4)
    graph->m_cornerDim = float(val)/65536.f;
  else if (val)
    f << "#cornerDim=" << val << ",";
  if (isLine) {
    float coord[2];
    for (int pt = 0; pt < 2; pt++) {
      for (int i = 0; i < 2; i++)
        coord[i] = float(input->readLong(4))/65536.f;
      graph->m_extremity[pt] = Vec2f(coord[1],coord[0]);
    }
  } else {
    for (int i = 0; i < 4; i++) {
      val = input->readLong(4);
      if (val) f << "#coord" << i << "=" << val << ",";
    }
  }
  long id = (long) input->readULong(4);
  if (id) {
    if (graph->m_graphType!=6)
      f << "#id0=" << std::hex << id << std::dec << ",";
    else
      f << "id[poly]=" << std::hex << id << std::dec << ",";
  }
  id = (long) input->readULong(4);
  f << "id=" << std::hex << id << std::dec << ",";
  for (int i = 0; i < 2; i++) { // always 1|0
    val = input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  std::string extra;
  graph->m_extra += extra;

  f.str("");
  f << "FrameDef(basicGraphic-data):" << graph->print() << extra;

  if (input->tell() != endPos)
    asciiFile.addDelimiter(input->tell(),'|');
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  if (!ok)
    graph.reset();
  return graph;
}

////////////////////////////////////////////////////////////
// send data
////////////////////////////////////////////////////////////
bool HMWJGraph::sendPageGraphics()
{
  return true;
}

void HMWJGraph::flushExtra()
{
  if (!m_parserState->m_listener)
    return;
  for (size_t t=0; t < m_state->m_tablesList.size(); t++) {
    HMWJGraphInternal::Table const &table = m_state->m_tablesList[t];
    if (table.m_parsed)
      continue;
    sendTable(table);
  }
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
