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
//! Internal: the frame header of a HMWJGraph
struct Frame {
  //! constructor
  Frame() : m_type(-1), m_fileId(-1), m_id(-1), m_page(0),
    m_pos(), m_baseline(0.f), m_posFlags(0), m_lineWidth(0), m_parsed(false), m_extra("") {
    m_colors[0]=MWAWColor::black();
    m_colors[1]=MWAWColor::white();
    m_patterns[0] = m_patterns[1] = 1.f;
  }
  //! destructor
  virtual ~Frame() {
  }

  //! returns the line colors
  bool getLineColor(MWAWColor &color) const;
  //! returns the surface colors
  bool getSurfaceColor(MWAWColor &color) const;

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
  MWAWColor m_colors[2];
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
    o << "comments,"; // memo
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
  if (!grph.m_colors[0].isBlack())
    o << "lineColor=" << grph.m_colors[0] << ",";
  if (grph.m_patterns[0]<1.)
    o << "linePattern=" << 100.f*grph.m_patterns[0] << "%,";
  if (!grph.m_colors[1].isWhite())
    o << "surfColor=" << grph.m_colors[1] << ",";
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
//! Internal: the state of a HMWJGraph
struct State {
  //! constructor
  State() : m_numPages(0), m_colorList(), m_patternPercentList() { }
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

bool Frame::getLineColor(MWAWColor &color) const
{
  color = State::getColor(m_colors[0], m_patterns[0]);
  return true;
}

bool Frame::getSurfaceColor(MWAWColor &color) const
{
  color = State::getColor(m_colors[1], m_patterns[1]);
  return true;
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
  if (!m_mainParser->readClassicHeader(mainHeader,endPos) || mainHeader.m_fieldSize != 4) {
    MWAW_DEBUG_MSG(("HMWJGraph::readFrames: can not read the header\n"));
    f << "###sz=" << mainHeader.m_length;
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    return false;
  }
  long headerEnd=pos+4+mainHeader.m_length;
  long val;
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
  for (int i = 0; i < mainHeader.m_n; i++) {
    pos = input->tell();
    f.str("");
    f << entry.name() << "-" << i << ":";
    HMWJGraphInternal::Frame frame;
    if (!readFrame(frame)) {
      f << "###";
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
      return false;
    }
    f << frame;
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
  }

  // normally there remains 2 block, ...

  // block 0
  pos = input->tell();
  f.str("");
  f << entry.name() << "-A:";
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
    pos=input->tell();
    f.str("");
    f << entry.name() << "-A" << i << ":";
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

bool HMWJGraph::readFrame(HMWJGraphInternal::Frame &graph)
{
  graph = HMWJGraphInternal::Frame();
  MWAWInputStreamPtr input = m_parserState->m_input;
  libmwaw::DebugFile &asciiFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  long pos = input->tell();
  long len = (long) input->readULong(4);
  long endPos = pos+4+len;
  if (len < 32 || !m_mainParser->isFilePos(endPos)) {
    MWAW_DEBUG_MSG(("HMWJGraph::readFrame: can not read the frame length\n"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  int val;
  /* fl0=[0|1|2|3|4|6|8|9|a|b|c][2|6], fl1=0|1|20|24,
     fl2=0|8|c|e|10|14|14|40, fl3=0|10|80|c0 */
  for (int i = 0; i < 4; i++) {
    val = (int) input->readULong(1);
    if (val) f << "fl" << i << "=" << std::hex << val << std::dec << ",";
  }
  graph.m_page = (int) input->readLong(2);
  // f0=small number
  val = (int) input->readLong(2);
  if (val) f << "f0=" << val << ",";
  float dim[4];
  for (int i = 0; i < 4; i++)
    dim[i] = float(input->readLong(4))/65536.f;
  graph.m_pos = Box2f(Vec2f(dim[0],dim[1]),Vec2f(dim[2],dim[3]));
  graph.m_id = (int) input->readLong(2); // check me
  for (int i = 0; i < 3; i++) {
    val = (int) input->readLong(2);
    if (val) f << "f" << i+1 << "=" << val << ",";
  }
  val = (int) input->readULong(1);
  int type=val>>4;
  switch(type) {
  case 0:
    f << "line|...,";
    break;
  case 1:
    f << "rect,";
    break;
  case 2:
    f << "circle,";
    break;
  case 3:
    f << "line[axis],";
    break;
  case 4:
    f << "rectOval,";
    break;
  case 5:
    f << "arc,";
    break;
  case 6:
    f << "poly,";
    break;
  default:
    f << "#type=" << type << ",";
    break;
  }
  graph.m_lineWidth = val&0xF;
  graph.m_extra = f.str();

  asciiFile.addDelimiter(input->tell(),'|');
  input->seek(endPos, WPX_SEEK_SET);
  return true;
}

// try to read the picture
bool HMWJGraph::readPicture(MWAWEntry const &entry)
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
  f << "pictSz=" << sz;
#ifdef DEBUG_WITH_FILES
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

// try to read a table
bool HMWJGraph::readTable(MWAWEntry const &entry)
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
  long val;
  f << "unkn=[";
  for (int i = 0; i < 3; i++) {
    f << "[";
    for (int j=0; j < 2; j++) {
      val = input->readLong(1);
      if (val) f << "f" << j << "=" << val << ",";
    }
    val = input->readLong(2); // a small number : a dim ? or a number ?
    if (val) f << "dim?=" << val << ",";
    f << "],";
  }
  f << "],";
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
      f << entry.name() << "-cell" << i << "x" << j << ":";

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

  // normally there remains 3 blocks:
  for (int i = 0; i < 3; i++) {
    static char const *(what[])= {"rowY", "colX", "borderType" };
    pos = input->tell();
    f.str("");
    f << entry.name() << "-" << what[i] << ":";
    HMWJZoneHeader header(false);
    int const expectedFSize = i==2 ? 40 : 4;
    if (!m_mainParser->readClassicHeader(header,endPos) || header.m_fieldSize != expectedFSize) {
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
    if (i<2) {
      f << "pos=[";
      for (int j = 0; j < header.m_n; j++)
        f << double(input->readULong(4))/65536. << ",";
      f << "],";
    }
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    if (i==2) {
      for (int j = 0; j < header.m_n; j++) {
        pos = input->tell();
        f.str("");
        f << entry.name() << "-border" << j << ":";
        asciiFile.addPos(pos);
        asciiFile.addNote(f.str().c_str());
        input->seek(pos+40, WPX_SEEK_SET);
      }
    }
    input->seek(zoneEnd, WPX_SEEK_SET);
  }

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

////////////////////////////////////////////////////////////
// send data to a listener
////////////////////////////////////////////////////////////

// ----- table

////////////////////////////////////////////////////////////
// low level
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
// send data
////////////////////////////////////////////////////////////
bool HMWJGraph::sendPageGraphics()
{
  return true;
}

void HMWJGraph::flushExtra()
{
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
