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
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSubDocument.hxx"
#include "MWAWTable.hxx"

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
struct TableCell : public MWAWTableCell {
  //! constructor
  TableCell(long tId): MWAWTableCell(), m_zId(0), m_tId(tId), m_cPos(-1), m_fileId(0), m_formatId(0), m_flags(0), m_extra("") {
  }
  //! use cell format to finish updating cell
  void update(CellFormat const &format);
  //! call when the content of a cell must be send
  virtual bool sendContent(MWAWContentListenerPtr listener, MWAWTable &table);
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, TableCell const &cell);
  //! the cell zone id
  long m_zId;
  //! the cell text zone id
  long m_tId;
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

void TableCell::update(CellFormat const &format)
{
  setBackgroundColor(format.m_backColor);
  static int const (wh[]) = { libmwaw::LeftBit,  libmwaw::RightBit, libmwaw::TopBit, libmwaw::BottomBit};
  for (size_t b = 0; b < format.m_borders.size(); b++)
    setBorders(wh[b], format.m_borders[b]);
}

std::ostream &operator<<(std::ostream &o, TableCell const &cell)
{
  o << static_cast<MWAWTableCell const &>(cell);
  if (cell.m_flags&0x100) o << "justify[full],";
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
struct Table: public MWAWTable {
  //! constructor
  Table(HMWJGraph &parser) : MWAWTable(MWAWTable::CellPositionBit|MWAWTable::TableDimBit), m_parser(&parser),
    m_rows(1), m_columns(1), m_height(0), m_textFileId(0), m_formatsList() {
  }
  //! destructor
  ~Table() {
  }
  //! update all cells using the formats list
  void updateCells();
  //! send a text zone
  bool sendText(long id, long cPos) const {
    return m_parser->sendText(id, cPos);
  }
  //! the graph parser
  HMWJGraph *m_parser;
  //! the number of row
  int m_rows;
  //! the number of columns
  int m_columns;
  //! the table height
  int m_height;
  //! the text file id
  long m_textFileId;
  //! a list of cell format
  std::vector<CellFormat> m_formatsList;

private:
  Table(Table const &orig);
  Table &operator=(Table const &orig);
};

bool TableCell::sendContent(MWAWContentListenerPtr, MWAWTable &table)
{
  if (m_tId)
    return static_cast<Table &>(table).sendText(m_tId, m_cPos);
  return true;
}

void Table::updateCells()
{
  int numFormats=(int) m_formatsList.size();
  for (int c=0; c<numCells(); ++c) {
    if (!get(c)) continue;
    TableCell &cell=static_cast<TableCell &>(*get(c));
    if (cell.m_formatId < 0 || cell.m_formatId>=numFormats) {
      static bool first = true;
      if (first) {
        MWAW_DEBUG_MSG(("HMWJGraphInternal::Table::updateCells: can not find the format\n"));
        first = false;
      }
      continue;
    }
    cell.update(m_formatsList[size_t(cell.m_formatId)]);
  }
}

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
  //! returns true if the frame data are read
  virtual bool valid() const {
    return false;
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
//! Internal: the comment frame of a HMWJGraph
struct CommentFrame :  public Frame {
public:
  //! constructor
  CommentFrame(Frame const &orig) : Frame(orig), m_zId(0), m_width(0), m_cPos(0), m_dim(0,0) {
  }
  //! returns true if the frame data are read
  virtual bool valid() const {
    return true;
  }
  //! print local data
  std::string print() const {
    std::stringstream s;
    if (m_zId) s << "zId[TZone]=" << std::hex << m_zId << std::dec << ",";
    if (m_dim[0]>0 || m_dim[1] > 0)
      s << "auxi[dim]=" << m_dim << ",";
    if (m_width > 0)
      s << "width=" << m_width << ",";
    if (m_cPos)
      s << "cPos[first]=" << m_cPos << ",";
    return s.str();
  }
  //! the text id
  long m_zId;
  //! the zone width
  double m_width;
  //! the first char pos
  long m_cPos;
  //! the auxilliary dim
  Vec2f m_dim;
};

////////////////////////////////////////
//! Internal: a group of a HMWJGraph
struct GroupFrame :  public Frame {
public:
  //! constructor
  GroupFrame(Frame const &orig) : Frame(orig), m_zId(0), m_childsList() {
  }
  //! returns true if the frame data are read
  virtual bool valid() const {
    return true;
  }
  //! the group id
  long m_zId;
  //! the child list
  std::vector<long> m_childsList;
};

////////////////////////////////////////
//! Internal: the picture frame of a HMWJGraph
struct PictureFrame :  public Frame {
public:
  //! constructor
  PictureFrame(Frame const &orig) : Frame(orig), m_entry(), m_zId(0), m_dim(100,100), m_scale(1,1) {
  }
  //! returns true if the frame data are read
  virtual bool valid() const {
    return true;
  }
  //! print local data
  std::string print() const {
    std::stringstream s;
    if (m_zId) s << "zId=" << std::hex << m_zId << std::dec << ",";
    s << "dim[original]=" << m_dim << ",";
    s << "scale=" << m_scale << ",";
    return s.str();
  }
  //! the picture entry
  MWAWEntry m_entry;
  //! the picture id
  long m_zId;
  //! the picture size
  Vec2i m_dim;
  //! the scale
  Vec2f m_scale;
};

////////////////////////////////////////
//! Internal: a footnote separator of a HMWJGraph
struct SeparatorFrame :  public Frame {
public:
  //! constructor
  SeparatorFrame(Frame const &orig) : Frame(orig) {
  }
  //! returns true if the frame data are read
  virtual bool valid() const {
    return true;
  }
};

////////////////////////////////////////
//! Internal: the table frame of a HMWJGraph
struct TableFrame :  public Frame {
public:
  //! constructor
  TableFrame(Frame const &orig) : Frame(orig), m_zId(0), m_width(0), m_length(0), m_table() {
  }
  //! returns true if the frame data are read
  virtual bool valid() const {
    return true;
  }
  //! print local data
  std::string print() const {
    std::stringstream s;
    if (m_zId) s << "zId[TZone]=" << std::hex << m_zId << std::dec << ",";
    if (m_width > 0)
      s << "width=" << m_width << ",";
    if (m_length)
      s << "length[text?]=" << m_length << ",";
    return s.str();
  }
  //! the textzone id
  long m_zId;
  //! the zone width
  double m_width;
  //! related to text length?
  long m_length;
  //! the table
  shared_ptr<Table> m_table;
};

////////////////////////////////////////
//! Internal: the textbox frame of a HMWJGraph
struct TextboxFrame :  public Frame {
public:
  //! constructor
  TextboxFrame(Frame const &orig) : Frame(orig), m_zId(0), m_width(0), m_cPos(0) {
  }
  //! returns true if the frame data are read
  virtual bool valid() const {
    return true;
  }
  //! print local data
  std::string print() const {
    std::stringstream s;
    if (m_zId) s << "zId[TZone]=" << std::hex << m_zId << std::dec << ",";
    if (m_width > 0)
      s << "width=" << m_width << ",";
    if (m_cPos)
      s << "cPos[first]=" << m_cPos << ",";
    return s.str();
  }
  //! the text id
  long m_zId;
  //! the zone width
  double m_width;
  //! the first char pos
  long m_cPos;
};

////////////////////////////////////////
//! Internal: the text frame (basic, header, footer, footnote) of a HMWJGraph
struct TextFrame :  public Frame {
public:
  //! constructor
  TextFrame(Frame const &orig) : Frame(orig), m_zId(0), m_width(0), m_cPos(0) {
  }
  //! returns true if the frame data are read
  virtual bool valid() const {
    return true;
  }
  //! print local data
  std::string print() const {
    std::stringstream s;
    if (m_zId) s << "zId[TZone]=" << std::hex << m_zId << std::dec << ",";
    if (m_width > 0)
      s << "width=" << m_width << ",";
    if (m_cPos)
      s << "cPos[first]=" << m_cPos << ",";
    return s.str();
  }
  //! the text id
  long m_zId;
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
  //! returns true if the frame data are read
  virtual bool valid() const {
    return true;
  }
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
  State() : m_framesList(), m_framesMap(), m_frameFormatsList(), m_numPages(0), m_colorList(), m_patternPercentList() { }
  //! tries to find the lId the frame of a given type
  shared_ptr<Frame> findFrame(int type, int lId) const {
    int actId = 0;
    for (size_t f = 0; f < m_framesList.size(); f++) {
      if (!m_framesList[f] || m_framesList[f]->m_type != type)
        continue;
      if (actId++==lId) {
        if (!m_framesList[f]->valid())
          break;
        return m_framesList[f];
      }
    }
    return shared_ptr<Frame>();
  }
  //! returns the frame format corresponding to an id
  FrameFormat const &getFrameFormat(int id) const {
    if (id >= 0 && id < (int) m_frameFormatsList.size())
      return m_frameFormatsList[size_t(id)];
    static FrameFormat defFormat;
    MWAW_DEBUG_MSG(("HMWJGraphInternal::State::getFrameFormat: can not find format %d\n", id));
    return defFormat;
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
  /** a map zId->frame pos in frames list */
  std::map<long, int> m_framesMap;
  /** the list of frame format */
  std::vector<FrameFormat> m_frameFormatsList;
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
  enum Type { FrameInFrame, Text, UnformattedTable, EmptyPicture };
  //! constructor
  SubDocument(HMWJGraph &pars, MWAWInputStreamPtr input, Type type, long id, long firstChar=0) :
    MWAWSubDocument(pars.m_mainParser, input, MWAWEntry()), m_graphParser(&pars), m_type(type), m_id(id), m_firstChar(firstChar), m_pos() {}

  //! constructor
  SubDocument(HMWJGraph &pars, MWAWInputStreamPtr input, MWAWPosition pos, Type type, long id, int firstChar=0) :
    MWAWSubDocument(pars.m_mainParser, input, MWAWEntry()), m_graphParser(&pars), m_type(type), m_id(id), m_firstChar(firstChar), m_pos(pos) {}

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
  //! the first char position
  long m_firstChar;
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
  switch(m_type) {
  case EmptyPicture:
    m_graphParser->sendEmptyPicture(m_pos);
    break;
  case FrameInFrame:
    m_graphParser->sendFrame(m_id, m_pos);
    break;
  case Text:
    m_graphParser->sendText(m_id, m_firstChar);
    break;
  case UnformattedTable:
    m_graphParser->sendTableUnformatted(m_id);
    break;
  default:
    MWAW_DEBUG_MSG(("HMWJGraphInternal::SubDocument::parse: send type %d is not implemented\n", m_type));
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
  if (m_type != sDoc->m_type) return true;
  if (m_id != sDoc->m_id) return true;
  if (m_firstChar != sDoc->m_firstChar) return true;
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
  for (size_t f=0 ; f < m_state->m_framesList.size(); f++) {
    if (!m_state->m_framesList[f]) continue;
    HMWJGraphInternal::Frame const &frame = *m_state->m_framesList[f];
    if (!frame.valid()) continue;
    int page = frame.m_page+1;
    if (page <= nPages) continue;
    if (page >= nPages+100) continue; // a pb ?
    nPages = page;
  }
  m_state->m_numPages = nPages;
  return nPages;
}

bool HMWJGraph::sendText(long textId, long fPos)
{
  return m_mainParser->sendText(textId, fPos);
}

std::map<long,int> HMWJGraph::getTextFrameInformations() const
{
  std::map<long,int> mapIdType;
  for (size_t f=0; f < m_state->m_framesList.size(); f++) {
    if (!m_state->m_framesList[f]) continue;
    HMWJGraphInternal::Frame const &frame = *m_state->m_framesList[f];
    if (!frame.valid())
      continue;
    long zId=0;
    switch(frame.m_type) {
    case 0:
    case 1:
    case 2:
    case 3:
      zId=static_cast<HMWJGraphInternal::TextFrame const &>(frame).m_zId;
      break;
    case 4:
      zId=static_cast<HMWJGraphInternal::TextboxFrame const &>(frame).m_zId;
      break;
    case 9:
      zId=static_cast<HMWJGraphInternal::TableFrame const &>(frame).m_zId;
      break;
    case 10:
      zId=static_cast<HMWJGraphInternal::CommentFrame const &>(frame).m_zId;
      break;
    default:
      break;
    }
    if (!zId) continue;
    if (mapIdType.find(zId) == mapIdType.end())
      mapIdType[zId] = frame.m_type;
    else if (mapIdType.find(zId)->second != frame.m_type) {
      MWAW_DEBUG_MSG(("HMWJGraph::getTextFrameInformations: id %lx already set\n", zId));
    }
  }
  return mapIdType;
}

bool HMWJGraph::getFootnoteInformations(long &textZId, std::vector<long> &fPosList) const
{
  fPosList.clear();
  textZId = 0;
  for (size_t f=0; f < m_state->m_framesList.size(); f++) {
    if (!m_state->m_framesList[f]) continue;
    HMWJGraphInternal::Frame const &frame = *m_state->m_framesList[f];
    if (!frame.valid() || frame.m_type != 3)
      continue;
    HMWJGraphInternal::TextFrame const &text=static_cast<HMWJGraphInternal::TextFrame const &>(frame);
    if (textZId && text.m_zId != textZId) {
      MWAW_DEBUG_MSG(("HMWJGraph::readFrames: find different textIds\n"));
    } else if (!textZId)
      textZId = text.m_zId;
    fPosList.push_back(text.m_cPos);
  }
  return fPosList.size();
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
  std::vector<long> lIds(size_t(mainHeader.m_n));
  for (int i = 0; i < mainHeader.m_n; i++) {
    val = (long) input->readULong(4);
    lIds[size_t(i)]=val;
    m_state->m_framesMap[val]=i;
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
    frame->m_fileId = lIds[size_t(i)];
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
    m_state->m_frameFormatsList.push_back(format);
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
  if (len < 32 || !input->checkPosition(endPos)) {
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
  case 4:
    res=readTextboxData(graph, endPos);
    break;
  case 6:
    res=readPictureData(graph, endPos);
    break;
  case 8:
    res=readBasicGraph(graph, endPos);
    break;
  case 9:
    res=readTableData(graph, endPos);
    break;
  case 10:
    res=readCommentData(graph, endPos);
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
  case 12:
    if (len < 52) {
      MWAW_DEBUG_MSG(("HMWJGraph::readFrame: can not read the footnote[sep] data\n"));
      break;
    } else {
      HMWJGraphInternal::SeparatorFrame *sep = new HMWJGraphInternal::SeparatorFrame(graph);
      res.reset(sep);
      pos =input->tell();
      f.str("");
      f << "FrameDef-footnote[sep];";
      for (int i = 0; i < 8; i++) { // f0=256,f2=8,f4=2,f6=146
        val = (int) input->readLong(2);
        if (val) f << "f" << i << "=" << val << ",";
      }
      f << "zId=" << std::hex << (long) input->readULong(4) << std::dec << ",";
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
      static_cast<HMWJGraphInternal::GroupFrame *>(frame.get());
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
  f << "listId=[" << std::hex;
  idsList->resize(size_t(mainHeader.m_n), 0);
  for (int i = 0; i < mainHeader.m_n; i++) {
    long val = (long) input->readULong(4);
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
    Vec2f pt(point[1], point[0]);
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
      static_cast<HMWJGraphInternal::BasicGraph *>(frame.get());
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
bool HMWJGraph::readPicture(MWAWEntry const &entry, int actZone)
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
  libmwaw::DebugFile &asciiFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  entry.setParsed(true);

  long pos = entry.begin()+8; // skip header
  input->seek(pos, WPX_SEEK_SET);
  long sz=(long) input->readULong(4);
  if (sz+12 != entry.length()) {
    MWAW_DEBUG_MSG(("HMWJGraph::readPicture: the entry sz seems bad\n"));
    return false;
  }
  f << "Picture:pictSz=" << sz;
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  asciiFile.skipZone(entry.begin()+12, entry.end()-1);

  shared_ptr<HMWJGraphInternal::Frame> frame = m_state->findFrame(6, actZone);
  if (!frame) {
    MWAW_DEBUG_MSG(("HMWJGraph::readPicture: can not find picture %d\n", actZone));
  } else {
    HMWJGraphInternal::PictureFrame *picture =
      static_cast<HMWJGraphInternal::PictureFrame *>(frame.get());
    picture->m_entry.setBegin(pos+4);
    picture->m_entry.setLength(sz);
  }

  return true;
}

// table
bool HMWJGraph::readTable(MWAWEntry const &entry, int actZone)
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
  libmwaw::DebugStream f;
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
  shared_ptr<HMWJGraphInternal::Table> table(new HMWJGraphInternal::Table(*this));

  long textId = 0;
  shared_ptr<HMWJGraphInternal::Frame> frame = m_state->findFrame(9, actZone);
  if (!frame || !frame->valid()) {
    MWAW_DEBUG_MSG(("HMWJTable::readTable: can not find basic table %d\n", actZone));
  } else {
    HMWJGraphInternal::TableFrame *tableFrame =
      static_cast<HMWJGraphInternal::TableFrame *>(frame.get());
    tableFrame->m_table = table;
    textId = tableFrame->m_zId;
  }

  table->m_rows = (int) input->readULong(1);
  table->m_columns = (int) input->readULong(1);
  f << "dim=" << table->m_rows << "x" << table->m_columns << ",";
  long val;
  for (int i = 0; i < 4; i++) { // f0=4|5|7|8|9, f1=1|7|107, f2=3|4|5|6, f3=0
    val = (long) input->readULong(2);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  table->m_height = (int) input->readLong(2);
  f << "h=" << table->m_height << ",";
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
      shared_ptr<HMWJGraphInternal::TableCell> cell(new HMWJGraphInternal::TableCell(textId));
      cell->setPosition(Vec2i(j,i));
      cell->m_cPos = (long) input->readULong(4);
      cell->m_zId = (long) input->readULong(4);
      cell->m_flags = (int) input->readULong(2);
      if (cell->m_flags&0x80)
        cell->setVAlignement(MWAWCell::VALIGN_CENTER);
      switch ((cell->m_flags>>9)&3) {
      case 1:
        cell->setExtraLine(MWAWCell::E_Line1);
        break;
      case 2:
        cell->setExtraLine(MWAWCell::E_Line2);
        break;
      case 3:
        cell->setExtraLine(MWAWCell::E_Cross);
        break;
      case 0: // none
      default:
        break;
      }
      val = input->readLong(2);
      if (val) f << "#f0=" << val << ",";
      cell->m_formatId = (int) input->readLong(2);
      int dim[2]; // for merge, inactive -> the other limit cell
      for (int k=0; k < 2; k++)
        dim[k]=(int)input->readULong(1);
      if (cell->m_flags & 0x1000) {
        if (dim[1]>=j&&dim[0]>=i)
          cell->setNumSpannedCells(Vec2i(dim[1]+1-j,dim[0]+1-i));
        else {
          static bool first = true;
          if (first) {
            MWAW_DEBUG_MSG(("HMWJGraph::readTable: can not determine the span\n"));
            first = false;
          }
          f << "##span=" << dim[1]+1-j << "x" << dim[0]+1-i << ",";
        }
      }
      cell->m_extra = f.str();
      // do not push the ignore cell
      if ((cell->m_flags&0x2000)==0)
        table->add(cell);
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
    std::vector<float> &dim = i==0 ? table->m_rowsSize : table->m_colsSize;
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
  readTableFormatsList(*table, endPos);
  table->updateCells();

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
  table.m_formatsList.resize(size_t(header.m_n));
  for (int i = 0; i < header.m_n; i++) {
    HMWJGraphInternal::CellFormat format;
    pos = input->tell();
    f.str("");
    long val = input->readLong(2); // always -2
    if (val != -2)
      f << "f0=" << val << ",";
    val = (long) input->readULong(2); // 0|2004|51|1dd4
    if (val)
      f << "#f1=" << std::hex << val << std::dec << ",";

    int color, pattern;
    float patPercent;
    format.m_borders.resize(4);
    static char const *(what[]) = {"T", "L", "B", "R"};
    static size_t const which[] = { libmwaw::Top, libmwaw::Left, libmwaw::Bottom, libmwaw::Right };
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

bool HMWJGraph::sendFrame(long frameId, MWAWPosition pos, WPXPropertyList extras)
{
  if (!m_parserState->m_listener) return true;

  std::map<long, int >::const_iterator fIt=
    m_state->m_framesMap.find(frameId);
  if (fIt == m_state->m_framesMap.end() || fIt->second < 0 || fIt->second >= int(m_state->m_framesList.size())) {
    MWAW_DEBUG_MSG(("HMWJGraph::sendFrame: can not find frame %lx\n", frameId));
    return false;
  }
  shared_ptr<HMWJGraphInternal::Frame> frame = m_state->m_framesList[size_t(fIt->second)];
  if (!frame || !frame->valid()) {
    MWAW_DEBUG_MSG(("HMWJGraph::sendFrame: frame %lx is not initialized\n", frameId));
    return false;
  }
  return sendFrame(*frame, pos, extras);
}

// --- basic shape
bool HMWJGraph::sendBasicGraph(HMWJGraphInternal::BasicGraph const &pict, MWAWPosition pos, WPXPropertyList extras)
{
  if (!m_parserState->m_listener) return true;
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
  HMWJGraphInternal::FrameFormat const &format=
    m_state->getFrameFormat(pict.m_formatId);
  pictPtr->setLineWidth((float) format.m_lineWidth);
  pictPtr->setLineColor(format.m_color[0]);
  pictPtr->setSurfaceColor(format.m_color[1]);

  WPXBinaryData data;
  std::string type;
  if (!pictPtr->getBinary(data,type)) return false;

  pos.setOrigin(pos.origin()-Vec2f(2,2));
  pos.setSize(pos.size()+Vec2f(4,4));
  m_parserState->m_listener->insertPicture(pos,data, type, extras);
  return true;
}

// picture
bool HMWJGraph::sendPictureFrame(HMWJGraphInternal::PictureFrame const &pict, MWAWPosition pos, WPXPropertyList extras)
{
  if (!m_parserState->m_listener) return true;
#ifdef DEBUG_WITH_FILES
  bool firstTime = pict.m_parsed == false;
#endif
  pict.m_parsed = true;
  Vec2f pictSz = pict.m_pos.size();
  if (pictSz[0] < 0) pictSz.setX(-pictSz[0]);
  if (pictSz[1] < 0) pictSz.setY(-pictSz[1]);

  if (pos.size()[0] <= 0 || pos.size()[1] <= 0)
    pos.setSize(pictSz);

  if (!pict.m_entry.valid()) {
    MWAW_DEBUG_MSG(("HMWJGraph::sendPictureFrame: can not find picture data\n"));
    sendEmptyPicture(pos);
    return true;
  }
  //fixme: check if we have border

  MWAWInputStreamPtr input = m_parserState->m_input;
  long fPos = input->tell();
  input->seek(pict.m_entry.begin(), WPX_SEEK_SET);
  WPXBinaryData data;
  input->readDataBlock(pict.m_entry.length(), data);
  input->seek(fPos, WPX_SEEK_SET);

#ifdef DEBUG_WITH_FILES
  if (firstTime) {
    libmwaw::DebugStream f;
    static int volatile pictName = 0;
    f << "Pict" << ++pictName << ".pct1";
    libmwaw::Debug::dumpFile(data, f.str().c_str());
  }
#endif

  m_parserState->m_listener->insertPicture(pos, data, "image/pict", extras);

  return true;
}

bool HMWJGraph::sendEmptyPicture(MWAWPosition pos)
{
  if (!m_parserState->m_listener)
    return true;
  Vec2f pictSz = pos.size();
  shared_ptr<MWAWPict> pict;
  MWAWPosition pictPos(Vec2f(0,0), pictSz, WPX_POINT);
  pictPos.setRelativePosition(MWAWPosition::Frame);
  pictPos.setOrder(-1);

  for (int i = 0; i < 3; i++) {
    if (i==0)
      pict.reset(new MWAWPictRectangle(Box2f(Vec2f(0,0), pictSz)));
    else if (i==1)
      pict.reset(new MWAWPictLine(Vec2f(0,0), pictSz));
    else
      pict.reset(new MWAWPictLine(Vec2f(0,pictSz[1]), Vec2f(pictSz[0], 0)));
    WPXBinaryData data;
    std::string type;
    if (!pict->getBinary(data,type)) continue;

    m_parserState->m_listener->insertPicture(pictPos, data, type);
  }
  return true;
}

// ----- comment box
bool HMWJGraph::sendComment(HMWJGraphInternal::CommentFrame const &comment, MWAWPosition pos, WPXPropertyList extras)
{
  if (!m_parserState->m_listener) return true;
  Vec2f commentSz = comment.m_pos.size();
  if (commentSz[0] < 0) commentSz.setX(-commentSz[0]);
  if (commentSz[1] < 0) commentSz.setY(-commentSz[1]);
  if (comment.m_dim[0] > commentSz[0]) commentSz[0]=comment.m_dim[0];
  if (comment.m_dim[1] > commentSz[1]) commentSz[1]=comment.m_dim[1];
  pos.setSize(commentSz);

  WPXPropertyList pList(extras);

  HMWJGraphInternal::FrameFormat const &format=
    m_state->getFrameFormat(comment.m_formatId);

  std::stringstream stream;
  stream << format.m_lineWidth*0.03 << "cm solid " << format.m_color[0];
  pList.insert("fo:border-left", stream.str().c_str());
  pList.insert("fo:border-bottom", stream.str().c_str());
  pList.insert("fo:border-right", stream.str().c_str());

  stream.str("");
  stream << 20*format.m_lineWidth*0.03 << "cm solid " << format.m_color[0];
  pList.insert("fo:border-top", stream.str().c_str());

  if (!format.m_color[1].isWhite())
    pList.insert("fo:background-color", format.m_color[1].str().c_str());

  MWAWSubDocumentPtr subdoc(new HMWJGraphInternal::SubDocument(*this, m_parserState->m_input, HMWJGraphInternal::SubDocument::Text, comment.m_zId));
  m_parserState->m_listener->insertTextBox(pos, subdoc, pList);

  return true;
}

// ----- textbox
bool HMWJGraph::sendTextbox(HMWJGraphInternal::TextboxFrame const &textbox, MWAWPosition pos, WPXPropertyList extras)
{
  if (!m_parserState->m_listener) return true;
  Vec2f textboxSz = textbox.m_pos.size();
  if (textboxSz[0] < 0) textboxSz.setX(-textboxSz[0]);
  if (textboxSz[1] < 0) textboxSz.setY(-textboxSz[1]);

  if (pos.size()[0] <= 0 || pos.size()[1] <= 0)
    pos.setSize(textboxSz);
  WPXPropertyList pList(extras);

  HMWJGraphInternal::FrameFormat const &format=
    m_state->getFrameFormat(textbox.m_formatId);

  if (format.m_lineWidth > 0) {
    std::stringstream stream;
    stream << format.m_lineWidth*0.03 << "cm solid " << format.m_color[0];
    pList.insert("fo:border", stream.str().c_str());
  }

  if (!format.m_color[1].isWhite())
    pList.insert("fo:background-color", format.m_color[1].str().c_str());

  MWAWSubDocumentPtr subdoc(new HMWJGraphInternal::SubDocument(*this, m_parserState->m_input, HMWJGraphInternal::SubDocument::Text, textbox.m_zId));
  m_parserState->m_listener->insertTextBox(pos, subdoc, pList);

  return true;
}

// ----- table
bool HMWJGraph::sendTableUnformatted(long fId)
{
  if (!m_parserState->m_listener)
    return true;
  std::map<long, int>::const_iterator fIt = m_state->m_framesMap.find(fId);
  if (fIt == m_state->m_framesMap.end()) {
    MWAW_DEBUG_MSG(("HMWJGraph::sendTableUnformatted: can not find table %lx\n", fId));
    return false;
  }
  int id = fIt->second;
  if (id < 0 || id >= (int) m_state->m_framesList.size())
    return false;
  HMWJGraphInternal::Frame &frame = *m_state->m_framesList[size_t(id)];
  if (!frame.valid() || frame.m_type != 9) {
    MWAW_DEBUG_MSG(("HMWJGraph::sendTableUnformatted: can not find table %lx(II)\n", fId));
    return false;
  }
  HMWJGraphInternal::Table &table = reinterpret_cast<HMWJGraphInternal::Table &>(frame);
  table.sendAsText(m_parserState->m_listener);
  return true;
}

////////////////////////////////////////////////////////////
// low level
////////////////////////////////////////////////////////////

bool HMWJGraph::sendFrame(HMWJGraphInternal::Frame const &frame, MWAWPosition pos, WPXPropertyList extras)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) return true;

  if (!frame.valid()) {
    frame.m_parsed = true;
    MWAW_DEBUG_MSG(("HMWJGraph::sendFrame: called with invalid frame\n"));
    return false;
  }

  MWAWInputStreamPtr &input= m_parserState->m_input;
  switch(frame.m_type) {
  case 4:
    frame.m_parsed = true;
    return sendTextbox(static_cast<HMWJGraphInternal::TextboxFrame const &>(frame), pos, extras);
  case 6: {
    HMWJGraphInternal::PictureFrame const &pict =
      static_cast<HMWJGraphInternal::PictureFrame const &>(frame);
    if (!pict.m_entry.valid()) {
      Vec2f sz(pos.size());
      if (sz[0] <= 0 || sz[1] <= 0)
        sz=pict.m_pos.size();
      if (sz[0] < 0) sz.setX(-sz[0]);
      if (sz[1] < 0) sz.setY(-sz[1]);
      pos.setSize(sz);

      frame.m_parsed = true;
      MWAWPosition framePos(pos);
      framePos.m_anchorTo = MWAWPosition::Frame;
      framePos.setOrigin(Vec2f(0,0));

      MWAWSubDocumentPtr subdoc
      (new HMWJGraphInternal::SubDocument
       (*this, input, framePos, HMWJGraphInternal::SubDocument::EmptyPicture, 0));
      listener->insertTextBox(pos, subdoc, extras);
      return true;
    }
    return sendPictureFrame(pict, pos, extras);
  }
  case 8:
    frame.m_parsed = true;
    return sendBasicGraph(static_cast<HMWJGraphInternal::BasicGraph const &>(frame), pos, extras);
  case 9: {
    frame.m_parsed = true;
    HMWJGraphInternal::TableFrame const &tableFrame = static_cast<HMWJGraphInternal::TableFrame const &>(frame);
    if (!tableFrame.m_table) {
      MWAW_DEBUG_MSG(("HMWJGraph::sendFrame: can not find the table\n"));
      return false;
    }
    HMWJGraphInternal::Table &table = *tableFrame.m_table;

    if (!table.updateTable()) {
      MWAW_DEBUG_MSG(("HMWJGraph::sendFrame: can not find the table structure\n"));
      MWAWSubDocumentPtr subdoc
      (new HMWJGraphInternal::SubDocument
       (*this, input, HMWJGraphInternal::SubDocument::UnformattedTable, frame.m_fileId));
      listener->insertTextBox(pos, subdoc, extras);
      return true;
    }
    if (pos.m_anchorTo==MWAWPosition::Page ||
        (pos.m_anchorTo!=MWAWPosition::Frame && table.hasExtraLines())) {
      MWAWPosition framePos(pos);
      framePos.m_anchorTo = MWAWPosition::Frame;
      framePos.setOrigin(Vec2f(0,0));

      MWAWSubDocumentPtr subdoc
      (new HMWJGraphInternal::SubDocument
       (*this, input, framePos, HMWJGraphInternal::SubDocument::FrameInFrame, frame.m_fileId));
      listener->insertTextBox(pos, subdoc, extras);
      return true;
    }
    if (table.sendTable(listener, pos.m_anchorTo==MWAWPosition::Frame))
      return true;
    return table.sendAsText(listener);
  }
  case 10:
    frame.m_parsed = true;
    return sendComment(static_cast<HMWJGraphInternal::CommentFrame const &>(frame), pos, extras);
  case 11: // group: fixme must be implement for char position...
    MWAW_DEBUG_MSG(("HMWJGraph::sendFrame: sending group is not implemented\n"));
    break;
  default:
    MWAW_DEBUG_MSG(("HMWJGraph::sendFrame: sending type %d is not implemented\n", frame.m_type));
    break;
  }
  frame.m_parsed = true;
  return false;
}

// try to read a basic comment zone
shared_ptr<HMWJGraphInternal::CommentFrame> HMWJGraph::readCommentData(HMWJGraphInternal::Frame const &header, long endPos)
{
  shared_ptr<HMWJGraphInternal::CommentFrame> comment;

  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &asciiFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  long pos = input->tell();
  if (endPos<pos+40) {
    MWAW_DEBUG_MSG(("HMWJGraph::readCommentData: the zone seems too short\n"));
    return comment;
  }
  comment.reset(new HMWJGraphInternal::CommentFrame(header));
  comment->m_width = double(input->readLong(4))/65536.;
  long val = input->readLong(2); // small number between 1 and 0x17
  if (val!=1)
    f << "f0=" << val << ",";
  val = input->readLong(2);// always 0?
  if (val)
    f << "f1=" << val << ",";
  comment->m_cPos = (long) input->readULong(4);
  val = (long) input->readULong(4);
  f << "id0=" << std::hex << val << std::dec << ",";
  comment->m_zId = (long) input->readULong(4);
  for (int i=0; i < 4; i++) { // g2=8000 if close?
    val = input->readLong(2);
    if (val)
      f << "g" << i << "=" << val << ",";
  }
  float dim[2];
  for (int i = 0; i < 2; i++)
    dim[i] = float(input->readLong(4))/65536.f;
  comment->m_dim=Vec2f(dim[1],dim[0]);
  for (int i=0; i < 2; i++) {
    val = input->readLong(2);
    if (val)
      f << "g" << i+4 << "=" << val << ",";
  }

  std::string extra=f.str();
  comment->m_extra += extra;
  f.str("");
  f << "FrameDef(Comment-data):" << comment->print() << extra;
  if (input->tell() != endPos)
    asciiFile.addDelimiter(input->tell(),'|');
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  return comment;
}

// try to read a basic picture zone
shared_ptr<HMWJGraphInternal::PictureFrame> HMWJGraph::readPictureData(HMWJGraphInternal::Frame const &header, long endPos)
{
  shared_ptr<HMWJGraphInternal::PictureFrame> picture;

  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &asciiFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  long pos = input->tell();
  if (endPos<pos+40) {
    MWAW_DEBUG_MSG(("HMWJGraph::readPictureData: the zone seems too short\n"));
    return picture;
  }
  picture.reset(new HMWJGraphInternal::PictureFrame(header));
  long val;
  for (int i=0; i < 2; i++) { // always 0
    val = input->readLong(2);
    if (val)
      f << "f" << i << "=" << val << ",";
  }
  float fDim[2]; // a small size, typically 1x1
  for (int i = 0; i < 2; i++)
    fDim[i] = float(input->readLong(4))/65536.f;
  picture->m_scale = Vec2f(fDim[0], fDim[1]);
  picture->m_zId = (long) input->readULong(4);
  for (int i = 0; i < 2; i++) { // f2=0, f3=0|-1 : maybe front/back color?
    val = input->readLong(4);
    if (val)
      f << "f" << i << "=" << val << ",";
  }
  int dim[2];
  for (int i = 0; i < 2; i++)
    dim[i] = int(input->readLong(2));
  picture->m_dim=Vec2i(dim[0],dim[1]); // checkme: xy
  for (int i = 0; i < 6; i++) { // g2=8400
    val = (long) input->readULong(2);
    if (val)
      f << "g" << i << "=" << std::hex << val << std::dec << ",";
  }
  std::string extra = f.str();
  picture->m_extra += extra;
  f.str("");
  f << "FrameDef(picture-data):" << picture->print() << extra;
  if (input->tell() != endPos)
    asciiFile.addDelimiter(input->tell(),'|');
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  return picture;
}

// try to read a basic table zone
shared_ptr<HMWJGraphInternal::TableFrame> HMWJGraph::readTableData(HMWJGraphInternal::Frame const &header, long endPos)
{
  shared_ptr<HMWJGraphInternal::TableFrame> table;

  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &asciiFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  long pos = input->tell();
  if (endPos<pos+28) {
    MWAW_DEBUG_MSG(("HMWJGraph::readTableData: the zone seems too short\n"));
    return table;
  }
  table.reset(new HMWJGraphInternal::TableFrame(header));
  table->m_width = double(input->readLong(4))/65536.;
  long val = input->readLong(2); // small number between 1 and 3
  if (val!=1)
    f << "f0=" << val << ",";
  val = input->readLong(2);// always 0?
  if (val)
    f << "f1=" << val << ",";
  table->m_length = (long) input->readULong(4);
  val = (long) input->readULong(4);
  f << "id0=" << std::hex << val << std::dec << ",";
  table->m_zId = (long) input->readULong(4);
  for (int i = 0; i < 2; i++) {
    val = input->readLong(2);
    if (val) f << "f" << i+2 << "=" << val << ",";
  }
  val = (long) input->readULong(4);
  f << "id1=" << std::hex << val << std::dec << ",";
  std::string extra=f.str();
  table->m_extra += extra;
  f.str("");
  f << "FrameDef(table-data):" << table->print() << extra;
  if (input->tell() != endPos)
    asciiFile.addDelimiter(input->tell(),'|');
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  return table;
}

// try to read a basic text box zone
shared_ptr<HMWJGraphInternal::TextboxFrame> HMWJGraph::readTextboxData(HMWJGraphInternal::Frame const &header, long endPos)
{
  shared_ptr<HMWJGraphInternal::TextboxFrame> textbox;

  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &asciiFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  long pos = input->tell();
  if (endPos<pos+24) {
    MWAW_DEBUG_MSG(("HMWJGraph::readTextboxData: the zone seems too short\n"));
    return textbox;
  }
  textbox.reset(new HMWJGraphInternal::TextboxFrame(header));
  textbox->m_width = double(input->readLong(4))/65536.;
  long val = input->readLong(2); // small number between 1 and 0x17
  if (val!=1)
    f << "f0=" << val << ",";
  val = input->readLong(2);// always 0?
  if (val)
    f << "f1=" << val << ",";
  textbox->m_cPos = (long) input->readULong(4);
  val = (long) input->readULong(4);
  f << "id0=" << std::hex << val << std::dec << ",";
  textbox->m_zId = (long) input->readULong(4);
  float dim = float(input->readLong(4))/65536.f; // a small negative number: 0, -4 or -6.5
  if (dim < 0 || dim > 0)
    f << "dim?=" << dim << ",";
  std::string extra=f.str();
  textbox->m_extra += extra;
  f.str("");
  f << "FrameDef(Textbox-data):" << textbox->print() << extra;
  if (input->tell() != endPos)
    asciiFile.addDelimiter(input->tell(),'|');
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  return textbox;
}

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
  val = (long) input->readULong(4);
  f << "id0=" << std::hex << val << std::dec << ",";
  text->m_zId = (long) input->readULong(4);

  std::string extra=f.str();
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
    int transf = (int) ((2*(flag&1)) | (flag1>>7));
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
  std::string extra = f.str();
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
bool HMWJGraph::sendPageGraphics(std::vector<long> const &doNotSendIds)
{
  if (!m_parserState->m_listener)
    return true;
  std::set<long> notSend;
  for (size_t i=0; i < doNotSendIds.size(); i++)
    notSend.insert(doNotSendIds[i]);
  std::map<long, int >::const_iterator fIt= m_state->m_framesMap.begin();
  int numFrames = int(m_state->m_framesList.size());
  for ( ; fIt != m_state->m_framesMap.end(); ++fIt) {
    int id = fIt->second;
    if (notSend.find(fIt->first) != notSend.end() || id < 0 || id >= numFrames ||
        !m_state->m_framesList[size_t(id)])
      continue;
    HMWJGraphInternal::Frame const &frame = *m_state->m_framesList[size_t(id)];
    if (!frame.valid() || frame.m_parsed)
      continue;
    if (frame.m_type <= 3 || frame.m_type == 12) continue;
    MWAWPosition pos(frame.m_pos[0],frame.m_pos.size(),WPX_POINT);
    pos.setRelativePosition(MWAWPosition::Page);
    pos.setPage(frame.m_page+1);
    sendFrame(frame, pos);
  }
  return true;
}

void HMWJGraph::flushExtra()
{
  if (!m_parserState->m_listener)
    return;
  for (size_t f=0; f < m_state->m_framesList.size(); f++) {
    if (!m_state->m_framesList[f]) continue;
    HMWJGraphInternal::Frame const &frame = *m_state->m_framesList[f];
    if (!frame.valid() || frame.m_parsed)
      continue;
    if (frame.m_type <= 3 || frame.m_type == 12) continue;
    MWAWPosition pos(Vec2f(0,0),Vec2f(0,0),WPX_POINT);
    pos.setRelativePosition(MWAWPosition::Char);
    sendFrame(frame, pos);
  }
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
