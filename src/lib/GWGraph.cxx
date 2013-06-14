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
#include "MWAWPictBasic.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSubDocument.hxx"

#include "GWParser.hxx"

#include "GWGraph.hxx"

/** Internal: the structures of a GWGraph */
namespace GWGraphInternal
{
////////////////////////////////////////
//! Internal: the graphic style of a GWGraph
struct Style {
  //! constructor
  Style() : m_lineWidth(1,1), m_linePatternPercent(1), m_lineArrow(0), m_lineColor(), m_surfaceColor(), m_shadeColor(), m_extra("") {
    m_lineColor.m_patternPercent=1;
    m_shadeColor.m_hasPattern=false;
  }
  //! return the line width
  float lineWidth() const {
    return (m_lineWidth[0]+m_lineWidth[1])/2.f;
  }
  //! return the color for a line or a surface
  MWAWColor getColor(bool line) const {
    if (!line) {
      if (m_shadeColor.m_hasPattern)
        return m_shadeColor.getColor();
      return m_surfaceColor.getColor();
    }
    MWAWColor res=m_lineColor.getColor();
    if (m_linePatternPercent<0||m_linePatternPercent>1)
      return res;
    return MWAWColor::barycenter
           (m_linePatternPercent,res,1-m_linePatternPercent,MWAWColor::white());
  }
  //! return true if we have a surface color
  bool hasSurfaceColor() const {
    return m_surfaceColor.m_hasPattern || m_shadeColor.m_hasPattern;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream & o, Style const &style) {
    if (style.m_lineWidth != Vec2f(1,1))
      o << "line[w]=" << style.m_lineWidth << ",";
    if (style.m_linePatternPercent<1)
      o << "linePercent=" << style.m_linePatternPercent << ",";
    if (!style.m_lineColor.isDefault())
      o << "line[col]=[" << style.m_lineColor << "]" << ",";
    if (!style.m_surfaceColor.isDefault())
      o << "surf[col]=[" << style.m_surfaceColor << "]" << ",";
    if (style.m_shadeColor.m_hasPattern)
      o << "shade[col]=[" << style.m_shadeColor << "]" << ",";
    switch(style.m_lineArrow) {
    case 0: // unset
    case 1: // none
      break;
    case 2:
      o << "arrow=\'>\',";
      break;
    case 3:
      o << "arrow=\'<\',";
      break;
    case 4:
      o << "arrow=\'<>\',";
      break;
    default:
      o<< "#arrow=" << style.m_lineArrow << ",";
    }
    o << style.m_extra << ",";
    return o;
  }

  //! struct used to defined a color in a GWGraphInternal::Style
  struct Color {
    //! constructor
    Color() : m_hasPattern(true), m_patternPercent(0), m_extra("") {
      m_color[0]=MWAWColor::black();
      m_color[1]=MWAWColor::white();
    }
    //! returns true if the field is unchanged
    bool isDefault() const {
      return m_hasPattern && m_patternPercent<=0 && m_color[0].isBlack() && m_color[1].isWhite();
    }
    //! return the final color
    MWAWColor getColor() const {
      if (!m_hasPattern)
        return MWAWColor::white();
      return MWAWColor::barycenter(m_patternPercent,m_color[0],1.f-m_patternPercent,m_color[1]);
    }
    //! operator<<
    friend std::ostream &operator<<(std::ostream & o, Color const &color) {
      if (!color.m_hasPattern) {
        o << "none,";
        return o;
      }
      if (!color.m_color[0].isBlack())
        o << "front=" << color.m_color[0] << ",";
      if (!color.m_color[1].isWhite())
        o << "back=" << color.m_color[1] << ",";
      if (color.m_patternPercent>0)
        o << "pattern=" << color.m_patternPercent << ",";
      o << color.m_extra;
      return o;
    }
    //! true if we have a pattern
    bool m_hasPattern;
    //! front and back color
    MWAWColor m_color[2];
    //! the percent pattern
    float m_patternPercent;
    //! extra data
    std::string m_extra;
  };

  //! the line dimension ( width, height)
  Vec2f m_lineWidth;
  //! the line pattern filled percent
  float m_linePatternPercent;
  //! the line arrow type (v2)
  int m_lineArrow;
  //! the line color
  Color m_lineColor;
  //! the surface color
  Color m_surfaceColor;
  //! the shade color
  Color m_shadeColor;
  //! extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the graphic zone of a GWGraph
struct Frame {
  //! the frame type
  enum Type { T_BAD, T_BASIC, T_GROUP, T_PICTURE, T_TEXT, T_UNSET };
  //! constructor
  Frame() : m_type(-1), m_style(-1), m_parent(0), m_order(-1), m_dataSize(0), m_box(), m_page(-1), m_extra(""), m_parsed(false) {
  }
  //! destructor
  virtual ~Frame() {
  }
  //! return the frame type
  virtual Type getType() const {
    return T_UNSET;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream & o, Frame const &zone) {
    zone.print(o);
    return o;
  }
  //! a virtual print function
  virtual void print(std::ostream & o) const {
    switch(m_type) {
    case 1:
      o << "text,";
      break;
    case 2:
      o << "line,";
      break;
    case 3:
      o << "rect,";
      break;
    case 4:
      o << "roundrect,";
      break;
    case 5:
      o << "oval,";
      break;
    case 6:
      o << "arc,";
      break;
    case 7:
      o << "poly[regular],";
      break;
    case 8:
      o << "poly,";
      break;
    case 11:
      o << "picture,";
      break;
    case 12:
      o << "spline,";
      break;
    case 15:
      o << "group,";
      break;
    default:
      o << "type=" << m_type << ",";
      break;
    }
    if (m_style >= 0)
      o << "S" << m_style << ",";
    if (m_order >= 0)
      o << "order=" << m_order << ",";
    if (m_parent > 0)
      o << "F" << m_parent << "[parent],";
    if (m_dataSize > 0)
      o << "dataSize=" << m_dataSize << ",";
    o << "box=" << m_box << ",";
    if (m_page>0)
      o << "page=" << m_page << ",";
    o << m_extra;
  }
  //! the zone type
  int m_type;
  //! the style identifier
  int m_style;
  //! the parent identifier
  int m_parent;
  //! the z order
  int m_order;
  //! the data size ( if know)
  long m_dataSize;
  //! the zone bdbox
  Box2f m_box;
  //! the page
  int m_page;
  //! extra data
  std::string m_extra;
  //! true if the frame is send
  mutable bool m_parsed;
};

////////////////////////////////////////
//! Internal: a unknown zone of a GWGraph
struct FrameBad : public Frame {
  //! constructor
  FrameBad() : Frame() {
  }
  //! return the frame type
  virtual Type getType() const {
    return T_BAD;
  }
};

////////////////////////////////////////
//! Internal: the basic graphic of a GWGraph
struct FrameBasic : public Frame {
  //! constructor
  FrameBasic(Frame const &frame) : Frame(frame), m_vertices(), m_lineArrow(0), m_lineFormat(0) {
    for (int i=0; i < 3; ++i)
      m_values[i]=0;
  }
  //! returns a picture corresponding to the frame
  shared_ptr<MWAWPictBasic> getPicture(Style const &style) const;
  //! return the frame type
  virtual Type getType() const {
    return T_BASIC;
  }
  //! print function
  virtual void print(std::ostream & o) const {
    Frame::print(o);
    if (m_type==4) {
      if (m_values[0]==1)
        o << "cornerDim=" << m_values[1] << ",";
      else if (m_values[0]==2)
        o << "cornerDim=minLength/2,";
      else
        o << "#type[corner]=" << m_values[0] << ",";
    } else if (m_type==6) {
      o << "angles=" << m_values[0]<< "x" << m_values[1] << ",";
      if (m_values[2]==1) o << "closed,";
      else if (m_values[2]) o << "#type[angle]=" << m_values[2] << ",";
    }
    if (m_vertices.size()) {
      o << "vertices=[";
      for (size_t i = 0; i < m_vertices.size(); ++i)
        o << m_vertices[i] << ",";
      o << "],";
    }
    switch(m_lineArrow) {
    case 0: // unset
    case 1: // none
      break;
    case 2:
      o << "arrow=\'>\',";
      break;
    case 3:
      o << "arrow=\'<\',";
      break;
    case 4:
      o << "arrow=\'<>\',";
      break;
    default:
      o<< "#arrow=" << m_lineArrow << ",";
    }
    if (m_lineFormat)
      o << "L" << m_lineFormat << ",";
  }
  //! arc : the angles, open/close, rectoval : type, the corner dimension
  int m_values[3];
  //! the polygon vertices
  std::vector<Vec2f> m_vertices;
  //! the line arrow style (in v1)
  int m_lineArrow;
  //! the line format?
  int m_lineFormat;
};

shared_ptr<MWAWPictBasic> FrameBasic::getPicture(Style const &style) const
{
  shared_ptr<MWAWPictBasic> res;
  Box2f box(Vec2f(0,0), m_box.size());

  switch(m_type) {
  case 2: {
    if (m_vertices.size()<2) {
      MWAW_DEBUG_MSG(("FrameBasic::getPicture: can not find line points\n"));
      break;
    }
    MWAWPictLine *pict=new MWAWPictLine(m_vertices[0],m_vertices[1]);
    int arrow=(m_lineArrow<=1) ? style.m_lineArrow : m_lineArrow;
    switch(arrow) {
    case 2:
      pict->setArrow(1, true);
      break;
    case 3:
      pict->setArrow(0, true);
      break;
    case 4:
      pict->setArrow(0, true);
      pict->setArrow(1, true);
      break;
    default:
      break;
    }
    res.reset(pict);
    break;
  }
  case 3:
    res.reset(new MWAWPictRectangle(box));
    break;
  case 4: {
    int width=m_values[1];
    if (m_values[0]==2)
      width=m_box.size()[1]>m_box.size()[0] ?
            int(m_box.size()[0]) : int(m_box.size()[1]);
    MWAWPictRectangle *rect=new MWAWPictRectangle(box);
    res.reset(rect);
    rect->setRoundCornerWidth((width+1)/2);
    break;
  }
  case 5:
    res.reset(new MWAWPictCircle(box));
    break;
  case 6: {
    int angle[2] = { int(90-m_values[0]-m_values[1]),
                     int(90-m_values[0])
                   };
    if (m_values[1]<0) {
      angle[0]=int(90-m_values[0]);
      angle[1]=int(90-m_values[0]-m_values[1]);
    } else if (m_values[1]==360)
      angle[0]=int(90-m_values[0]-359);
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
    for (int i = 0; i < 2; ++i)
      limitAngle[i] = (angle[i] < 0) ? int(angle[i]/90)-1 : int(angle[i]/90);
    for (int bord = limitAngle[0]; bord <= limitAngle[1]+1; ++bord) {
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
    res.reset(new MWAWPictArc(realBox,box, float(angle[0]), float(angle[1])));
    break;
  }
  case 7:
  case 8:
    if (m_vertices.size()<2) {
      MWAW_DEBUG_MSG(("FrameBasic::getPicture: can not find polygon points\n"));
      break;
    }
    res.reset(new MWAWPictPolygon(box, m_vertices));
    break;
  case 12: {
    size_t nbPt=m_vertices.size();
    if (nbPt<4 || (nbPt%3)!=1) {
      MWAW_DEBUG_MSG(("FrameBasic::getPicture: the spline number of points seems bad\n"));
      break;
    }
    std::stringstream s;
    s << "M " << m_vertices[0][0] << " " << m_vertices[0][1];
    for (size_t pt=1; pt < nbPt; pt+=3) {
      bool hasFirstC=m_vertices[pt-1]!=m_vertices[pt];
      bool hasSecondC=m_vertices[pt+1]!=m_vertices[pt+2];
      s << " ";
      if (!hasFirstC && !hasSecondC)
        s << "L";
      else if (hasFirstC)
        s << "C" << m_vertices[pt][0] << " " << m_vertices[pt][1]
          << " " << m_vertices[pt+1][0] << " " << m_vertices[pt+1][1];
      else if (hasSecondC)
        s << "S" << m_vertices[pt+1][0] << " " << m_vertices[pt+1][1];
      s << " " << m_vertices[pt+2][0] << " " << m_vertices[pt+2][1];
    }
    s << " Z";
    res.reset(new MWAWPictPath(box, s.str()));
  }
  default:
    break;
  }
  if (!res)
    return res;
  res->setLineWidth(style.lineWidth());
  res->setLineColor(style.getColor(true));
  res->setSurfaceColor(style.getColor(false), style.hasSurfaceColor());
  return res;
}

////////////////////////////////////////
//! Internal: the group zone of a GWGraph
struct FrameGroup : public Frame {
  //! constructor
  FrameGroup(Frame const &frame) : Frame(frame), m_numChild(0), m_childList() {
  }
  //! return the frame type
  virtual Type getType() const {
    return T_GROUP;
  }
  //! print funtion
  virtual void print(std::ostream & o) const {
    Frame::print(o);
    if (m_numChild)
      o << "nChild=" << m_numChild << ",";
  }
  //! the number of child
  int m_numChild;
  //! the list of child
  std::vector<int> m_childList;
};

////////////////////////////////////////
//! Internal: the picture zone of a GWGraph
struct FramePicture : public Frame {
  //! constructor
  FramePicture(Frame const &frame) : Frame(frame), m_entry() {
  }
  //! return the frame type
  virtual Type getType() const {
    return T_PICTURE;
  }
  //! print funtion
  virtual void print(std::ostream & o) const {
    Frame::print(o);
    if (m_entry.valid())
      o << "pos=" << std::hex << m_entry.begin() << "->" << m_entry.end() << std::dec << ",";
  }
  //! the picture entry
  MWAWEntry m_entry;
};

////////////////////////////////////////
//! Internal: the text zone of a GWGraph
struct FrameText : public Frame {
  //! constructor
  FrameText(Frame const &frame) : Frame(frame), m_entry() {
  }
  //! return the frame type
  virtual Type getType() const {
    return T_TEXT;
  }
  //! print funtion
  virtual void print(std::ostream & o) const {
    Frame::print(o);
    if (m_entry.valid())
      o << "pos=" << std::hex << m_entry.begin() << "->" << m_entry.end() << std::dec << ",";
  }
  //! the text entry
  MWAWEntry m_entry;
};

////////////////////////////////////////
//! Internal: a list of graphic corresponding to a page
struct Zone {
  //! constructor
  Zone() : m_page(-1), m_frameList(), m_styleList(), m_parsed(false) {
  }
  //! the page number (if known)
  int m_page;
  //! the list of frame
  std::vector<shared_ptr<Frame> > m_frameList;
  //! the list of style
  std::vector<Style> m_styleList;
  //! true if we have send the data
  mutable bool m_parsed;
};

////////////////////////////////////////
//! Internal: the state of a GWGraph
struct State {
  //! constructor
  State() : m_zoneList(), m_numPages(0) { }
  //! the list of zone ( one by page)
  std::vector<Zone> m_zoneList;
  int m_numPages /* the number of pages */;
};


////////////////////////////////////////
//! Internal: the subdocument of a GWGraph
class SubDocument : public MWAWSubDocument
{
public:
  //! constructor
  SubDocument(GWGraph &pars, MWAWInputStreamPtr input, MWAWEntry entry) :
    MWAWSubDocument(pars.m_mainParser, input, MWAWEntry(entry)), m_graphParser(&pars) {}


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
  GWGraph *m_graphParser;

private:
  SubDocument(SubDocument const &orig);
  SubDocument &operator=(SubDocument const &orig);
};

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("GWGraphInternal::SubDocument::parse: no listener\n"));
    return;
  }
  assert(m_graphParser);

  long pos = m_input->tell();
  m_graphParser->sendTextbox(m_zone);
  m_input->seek(pos, WPX_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_graphParser != sDoc->m_graphParser) return true;
  return false;
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
GWGraph::GWGraph(GWParser &parser) :
  m_parserState(parser.getParserState()), m_state(new GWGraphInternal::State),
  m_mainParser(&parser)
{
}

GWGraph::~GWGraph()
{ }

int GWGraph::version() const
{
  return m_parserState->m_version;
}


int GWGraph::numPages() const
{
  if (m_state->m_numPages)
    return m_state->m_numPages;
  int nPages = 0;
  for (size_t z=0; z < m_state->m_zoneList.size(); ++z) {
    if (m_state->m_zoneList[z].m_page>nPages)
      nPages=m_state->m_zoneList[z].m_page>nPages;
  }

  m_state->m_numPages = nPages;
  return nPages;
}

bool GWGraph::sendTextbox(MWAWEntry const &entry)
{
  return m_mainParser->sendTextbox(entry);
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read the patterns list
////////////////////////////////////////////////////////////
bool GWGraph::readPatterns(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%8) != 2) {
    MWAW_DEBUG_MSG(("GWGraph::readPatterns: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  libmwaw::DebugStream f;
  entry.setParsed(true);

  input->seek(pos, WPX_SEEK_SET);
  f << "Entries(Pattern):";
  int N=(int) input->readLong(2);
  f << "N=" << N << ",";
  if (2+8*N!=int(entry.length())) {
    f << "###";
    MWAW_DEBUG_MSG(("GWGraph::readPatterns: the number of entries seems bad\n"));
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  for (int i=0; i < N; ++i) {
    pos = input->tell();
    f.str("");
    f << "Pattern-" << i << ":";
    for (int j=0; j < 8; ++j)
      f << std::hex << input->readULong(2) << std::dec << ",";
    input->seek(pos+8, WPX_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool GWGraph::readPalettes(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() != 0x664) {
    MWAW_DEBUG_MSG(("GWGraph::readPalettes: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  libmwaw::DebugStream f;
  entry.setParsed(true);

  input->seek(pos, WPX_SEEK_SET);
  f << "Entries(Palette):";
  int val=(int) input->readLong(2);
  if (val!=2)
    f << "#f0=" << val << ",";
  val=(int) input->readLong(2);
  if (val!=8)
    f << "#f1=" << val << ",";
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());

  // 16 sets: a1a1, a2a2, a3a3: what is that
  for (int i=0; i < 16; ++i) {
    pos = input->tell();
    f.str("");
    f << "Palette-" << i << ":";
    for (int j=0; j < 3; ++j)
      f << std::hex << input->readULong(2) << std::dec << ",";
    input->seek(pos+6, WPX_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  for (int i=0; i < 256; ++i) {
    pos = input->tell();
    f.str("");
    if (i==0) f << "Entries(Colors)-0:";
    else f << "Colors-" << i << ":";
    unsigned char col[3];
    for (int j=0; j < 3; ++j)
      col[j]=(unsigned char)(input->readULong(2)>>8);
    f << MWAWColor(col[0], col[1], col[2]) << ",";
    input->seek(pos+6, WPX_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  return true;
}

////////////////////////////////////////////////////////////
// graphic zone ( main header )
////////////////////////////////////////////////////////////
bool GWGraph::isGraphicZone()
{
  int const vers=version();
  bool isDraw=m_mainParser->getDocumentType()==GWParser::DRAW;
  if (vers == 1 && !isDraw)
    return false;
  int headerSize;
  if (vers==1)
    headerSize= 0x1c+0x38+0x1e +0x1a;
  else
    headerSize= 0x1c+0xaa+0x30;
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos = input->tell();
  if (!input->checkPosition(pos+headerSize))
    return false;

  int dim[4];
  for (int st=0; st<2; ++st) {
    for (int i=0; i<4; ++i)
      dim[i]=(int) input->readLong(2);
    if (dim[0]>=dim[2] || dim[1]>=dim[3] || dim[2]<=0 || dim[3]<=0) {
      input->seek(pos, WPX_SEEK_SET);
      return false;
    }
  }

  input->seek(pos+headerSize, WPX_SEEK_SET);
  int pageHeaderSize=vers==1 ? 16 : isDraw ? 12 : 22;
  if (!input->checkPosition(pos+headerSize+pageHeaderSize)) {
    bool ok=input->atEOS();
    input->seek(pos, WPX_SEEK_SET);
    return ok;
  }
  bool ok=isPageFrames();
  input->seek(pos, WPX_SEEK_SET);
  return ok;
}

bool GWGraph::readGraphicZone()
{
  int const vers=version();
  bool isDraw=m_mainParser->getDocumentType()==GWParser::DRAW;
  if (vers == 1 && !isDraw)
    return false;

  MWAWInputStreamPtr input = m_parserState->m_input;
  long beginPos = input->tell();
  if (!isGraphicZone() && !findGraphicZone()) {
    input->seek(beginPos, WPX_SEEK_SET);
    return false;
  }
  long pos = input->tell();
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  if (pos!=beginPos) {
    ascFile.addPos(beginPos);
    ascFile.addNote("Entries(Unknown):");
  }
  libmwaw::DebugStream f;
  f << "Entries(GZoneHeader):";
  for (int st=0; st<2; ++st) {
    int dim[4];
    for (int i=0; i<4; ++i)
      dim[i]=(int) input->readLong(2);
    f << "dim" << st << "=" << dim[1] << "x" << dim[0]
      << "<->"<< dim[3] << "x" << dim[2] << ",";
  }
  ascFile.addDelimiter(input->tell(),'|');
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  pos += 0x1c;
  if (vers==1) {
    ascFile.addPos(pos);
    ascFile.addNote("GZoneHeader-II");
    pos += 0x38;

    input->seek(pos, WPX_SEEK_SET);
    f.str("");
    f << "Entries(GLineFormat):";
    std::string extra;
    if (!readLineFormat(extra))
      f << "###";
    else
      f << extra;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    pos += 0x1e;
  } else {
    input->seek(pos, WPX_SEEK_SET);
    GWGraphInternal::Style style;
    f.str("");
    f << "Entries(GStyle):";
    if (!readStyle(style))
      f << "###";
    else
      f << style;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    pos += 0xaa;

    ascFile.addPos(pos);
    ascFile.addNote("Entries(GDatC)[_]:");
    pos += 0x16;
  }
  ascFile.addPos(pos);
  ascFile.addNote("Entries(GDatD)[_]:");
  pos += 0x1a;

  input->seek(pos, WPX_SEEK_SET);
  while(!input->atEOS() && readPageFrames())
    pos=input->tell();
  input->seek(pos, WPX_SEEK_SET);
  return true;
}

bool GWGraph::findGraphicZone()
{
  int const vers=version();
  bool isDraw=m_mainParser->getDocumentType()==GWParser::DRAW;
  if (vers == 1 && !isDraw)
    return false;
  int headerSize;
  if (vers==1)
    headerSize= 0x1c+0x38+0x1e +0x1a;
  else
    headerSize= 0x1c+0xaa+0x30;
  int pageHeaderSize=vers==1 ? 16 : isDraw ? 12 : 22;

  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos = input->tell();
  input->seek(pos+headerSize+pageHeaderSize, WPX_SEEK_SET);
  while(!input->atEOS()) {
    long actPos = input->tell();
    unsigned long value= input->readULong(4);
    int decal=-1;
    // if we find some tabs, we have a problem
    if (value==0x20FFFF)
      decal = 0;
    else if (value==0x20FFFFFF)
      decal = 1;
    else if (value==0xFFFFFFFF)
      decal = 2;
    else if (value==0xFFFFFF2E)
      decal = 3;
    if (decal>=0) {
      input->seek(actPos-decal, WPX_SEEK_SET);
      if (input->readULong(4)==0x20FFFF && input->readULong(4)==0xFFFF2E00)
        break;
      input->seek(actPos+4, WPX_SEEK_SET);
      continue;
    }

    // graphic size
    if ((value>>24)==0x36)
      decal = 3;
    else if ((value>>16)==0x36)
      decal = 2;
    else if (((value>>8)&0xFFFF)==0x36)
      decal = 1;
    else if ((value&0xFFFF)==0x36)
      decal = 0;
    if (decal==-1)
      continue;

    input->seek(actPos-decal, WPX_SEEK_SET);
    int N=(int) input->readULong(2);
    if (input->readLong(2)!=0x36 || !input->checkPosition(actPos-decal+4+0x36*N)) {
      input->seek(actPos+4, WPX_SEEK_SET);
      continue;
    }
    input->seek(actPos-decal-pageHeaderSize-headerSize, WPX_SEEK_SET);
    if (!isGraphicZone()) {
      input->seek(actPos+4, WPX_SEEK_SET);
      continue;
    }
    input->seek(actPos-decal-pageHeaderSize-headerSize, WPX_SEEK_SET);
    return true;
  }
  input->seek(pos, WPX_SEEK_SET);
  return false;
}

////////////////////////////////////////////////////////////
// graphic zone ( page frames)
////////////////////////////////////////////////////////////
bool GWGraph::isPageFrames()
{
  int const vers=version();
  bool hasPageUnknown=vers==2 && m_mainParser->getDocumentType()==GWParser::TEXT;
  int const headerSize=hasPageUnknown ? 22 : vers==2 ? 12 : 16;
  int const nZones= vers==2 ? 3 : 4;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  long endPos=pos+headerSize+4*nZones;
  if (!input->checkPosition(endPos))
    return false;
  long sz=-1;
  input->seek(pos, WPX_SEEK_SET);
  if (hasPageUnknown) {
    input->seek(2, WPX_SEEK_CUR); // page
    sz=(long) input->readULong(4);
    endPos=input->tell()+sz;
  }
  long zoneSz[4]= {0,0,0,0};
  for (int i=0; i<nZones; ++i)
    zoneSz[i]=(long) input->readULong(4);
  if (hasPageUnknown &&
      (6+sz<headerSize+4*nZones || zoneSz[0]+zoneSz[1]+zoneSz[2]>sz ||
       !input->checkPosition(endPos))) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  pos+=headerSize;
  input->seek(pos, WPX_SEEK_SET);
  int expectedSz[]= {0x36, 0xaa, 0x2, 0};
  if (vers==1) {
    expectedSz[1]=0x34;
    expectedSz[2]=0x1e;
    expectedSz[3]=2;
  }
  for (int i=0; i < nZones; ++i) {
    pos=input->tell();
    if (pos==endPos)
      return true;
    int nData=(int) input->readLong(2);
    int fSz=(int) input->readLong(2);
    if (nData<0 || (nData!=0 && fSz!=expectedSz[i]) || nData*fSz+4 > zoneSz[i]) {
      input->seek(pos, WPX_SEEK_SET);
      return false;
    }
    if (i!=nZones-1 && nData*fSz+4!=zoneSz[i]) {
      MWAW_DEBUG_MSG(("GWGraph::isPageFrames: find a diff of %ld for data %d\n", zoneSz[i]-nData*fSz-4, i));
      if ((2*nData+4)*fSz+4 < zoneSz[i]) {
        input->seek(pos, WPX_SEEK_SET);
        return false;
      }
    }
    input->seek(expectedSz[i]*nData, WPX_SEEK_CUR);
  }
  input->seek(pos, WPX_SEEK_SET);
  return true;
}

bool GWGraph::readStyle(GWGraphInternal::Style &style)
{
  style=GWGraphInternal::Style();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugStream f;
  int const vers=version();
  int const gDataSize=vers==1 ? 0x34 : 0xaa;

  long pos=input->tell();
  long endPos=pos+gDataSize;
  if (!input->checkPosition(endPos))
    return false;

  int val=(int) input->readLong(2);
  if (val) f << "used=" << val << ",";
  float dim[2];
  for (int i=0; i <2; ++i)
    dim[i]=float(input->readLong(4))/65536.f;
  style.m_lineWidth=Vec2f(dim[1],dim[0]);
  if  (vers==1) {
    for (int i=0; i < 2; ++i) { // two flags 0|1
      val=(int) input->readULong(1);
      if (val==1) continue;
      if (val)
        f << "#hasPat" << i << "=" << val << ",";
      else if (i==0)
        style.m_lineColor.m_hasPattern=false;
      else
        style.m_surfaceColor.m_hasPattern=false;
    }
    for (int i=0; i < 2; ++i) {
      int nBytes=0;
      for (int j=0; j < 8; ++j) {
        val=(int) input->readULong(1);
        for (int depl=1, b=0; b < 8; ++b, depl*=2) {
          if (val & depl)
            nBytes++;
        }
      }
      if (i==0)
        style.m_lineColor.m_patternPercent=float(nBytes)/64.f;
      else
        style.m_surfaceColor.m_patternPercent=float(nBytes)/64.f;
    }
    for (int i=0; i < 4; ++i) {
      unsigned char col[3];
      for (int j=0; j < 3; ++j)
        col[j]=(unsigned char)(input->readULong(2)>>8);
      switch(i) {
      case 0:
      case 1:
        style.m_lineColor.m_color[i]=MWAWColor(col[0], col[1], col[2]);
        break;
      case 2:
      case 3:
        style.m_surfaceColor.m_color[i-2]=MWAWColor(col[0], col[1], col[2]);
      default:
        break;
      }
    }
    style.m_extra=f.str();
    input->seek(endPos, WPX_SEEK_SET);
    return true;
  }

  for (int i=0; i < 4; ++i) {
    val=(int) input->readULong(2);
    if (val) f << "col" << i << "=" << std::hex << val << std::dec << ",";
    unsigned char col[3];
    for (int j=0; j < 3; ++j)
      col[j]=(unsigned char)(input->readULong(2)>>8);

    switch(i) {
    case 0:
    case 1:
      style.m_lineColor.m_color[i]=MWAWColor(col[0], col[1], col[2]);
      break;
    case 2:
    case 3:
      style.m_surfaceColor.m_color[i-2]=MWAWColor(col[0], col[1], col[2]);
    default:
      break;
    }
  }
  val=(int) input->readULong(2);
  if (val) f << "col4=" << std::hex << val << std::dec << ",";
  for (int i=0; i < 2; ++i) {
    val=(int) input->readULong(2); // small number
    if (val) f << "pat" << i << "=" << val << ",";
    else if (i==0)
      style.m_lineColor.m_hasPattern=false;
    else
      style.m_surfaceColor.m_hasPattern=false;
    int nBytes=0;
    for (int j=0; j < 8; ++j) {
      val=(int) input->readULong(1);
      for (int depl=1, b=0; b < 8; ++b, depl*=2) {
        if (val & depl)
          nBytes++;
      }
    }
    if (i==0)
      style.m_lineColor.m_patternPercent=float(nBytes)/64.f;
    else
      style.m_surfaceColor.m_patternPercent=float(nBytes)/64.f;
  }
  val=(int) input->readULong(2);
  if (val!=1) f << "patId=" << val << ",";
  int nPattern=val==1 ? 1 : (int)input->readULong(2);
  if (nPattern<0||nPattern>6) {
    MWAW_DEBUG_MSG(("GWGraph::readStyle: can not read number of line pattern\n"));
    f << "#nPattern=" << nPattern << ",";
  } else {
    f << "pat=[";
    float fill=0, empty=0;
    for (int i=0; i < nPattern; ++i) {
      float w=float(input->readLong(4))/65536.f;
      if (i%2)
        empty+=w;
      else
        fill+=w;
      f << w << ",";
    }
    f << "],";
    if (empty > 0 && fill>=0)
      style.m_linePatternPercent = fill/(fill+empty);
  }
  input->seek(pos+116, WPX_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  ascFile.addDelimiter(pos+92,'|'); // some data?
  ascFile.addDelimiter(input->tell(),'|');
  int shadeId=(int) input->readLong(2);
  if (shadeId) f << "shaderId=" << shadeId << ",";
  val=(int) input->readLong(2);
  if (shadeId>=1 && shadeId<=16 && val==1) {
    style.m_shadeColor.m_hasPattern=true;
    if (shadeId==4||shadeId==6||shadeId>=12)
      style.m_shadeColor.m_patternPercent=0.75f;
    else
      style.m_shadeColor.m_patternPercent=0.2f;
  }
  for (int i=0; i < 2; ++i) {
    unsigned char col[3];
    for (int j=0; j < 3; ++j)
      col[j]=(unsigned char)(input->readULong(2)>>8);
    style.m_shadeColor.m_color[i]=MWAWColor(col[0], col[1], col[2]);
  }
  input->seek(2, WPX_SEEK_CUR); // junk ?
  style.m_lineArrow=(int) input->readLong(2);
  val=(int) input->readLong(2);
  if (val!=1)
    f << "#lineArrow[unset],";
  style.m_extra=f.str();

  pos = input->tell();
  std::string extra("");
  f.str("");
  f << "Entries(GLineFormat):";
  if (readLineFormat(extra))
    f << extra;
  else
    f << "###";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  ascFile.addDelimiter(input->tell(),'|');
  input->seek(endPos, WPX_SEEK_SET);

  return true;
}

bool GWGraph::readLineFormat(std::string &extra)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugStream f;
  int const gDataSize=0x1e;
  long pos=input->tell();
  if (!input->checkPosition(pos+gDataSize))
    return false;

  // find: f0=1, f1=0, f2=9|c f3=3|5, f5=c|f, f0+a dim?
  for (int i=0; i<5; ++i) {
    int val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }

  extra=f.str();
  // then 3ffda4bc7d1934f709244002fcfb724c879139b4
  m_parserState->m_asciiFile.addDelimiter(input->tell(),'|');
  input->seek(pos+gDataSize, WPX_SEEK_SET);
  return true;
}

bool GWGraph::readPageFrames()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  int const vers=version();
  bool isDraw = m_mainParser->getDocumentType()==GWParser::DRAW;
  bool hasPageUnknown=vers==2 && !isDraw;
  int const nZones=hasPageUnknown ? 4 : vers==2 ? 3 : 4;
  long pos=input->tell();
  if (!isPageFrames()) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(GFrame):";
  input->seek(pos, WPX_SEEK_SET);
  long endPos=-1;
  GWGraphInternal::Zone pageZone;
  if (hasPageUnknown) {
    pageZone.m_page = (int)input->readLong(2);
    f << "page=" << pageZone.m_page << ",";
    endPos=pos+6+(long)input->readULong(4);
  }
  int val;
  static char const *(wh[])= {"head", "gstyle", "root", "unknown"};
  long zoneSz[4]= {0,0,0,0};
  for (int i=0; i < nZones; ++i) {
    zoneSz[i] = (long) input->readULong(4);
    if (zoneSz[i])
      f << wh[i] << "[sz]=" << std::hex << zoneSz[i] << std::dec << ",";
  }

  int z=0;
  long zoneEnd=input->tell()+zoneSz[z++];
  int nFrames=(int) input->readLong(2);
  input->seek(2, WPX_SEEK_CUR);
  f << "nFrames=" << nFrames << ",";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  std::vector<shared_ptr<GWGraphInternal::Frame> > frames;
  for (int i=0; i < nFrames; ++i) {
    pos = input->tell();
    f.str("");
    f << "GFrame[head]-F" << i+1 << ":";
    shared_ptr<GWGraphInternal::Frame> zone=readFrameHeader();
    if (!zone) {
      MWAW_DEBUG_MSG(("GWGraph::readPageFrames: oops graphic detection is probably bad\n"));
      f << "###";
      input->seek(pos+0x36, WPX_SEEK_SET);
      zone.reset(new GWGraphInternal::FrameBad());
    } else
      f << *zone;
    zone->m_page=pageZone.m_page;
    frames.push_back(zone);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  pos=input->tell();
  if (pos!=zoneEnd) {
    ascFile.addPos(pos);
    ascFile.addNote("GFrame[head]-end:###");
    input->seek(zoneEnd, WPX_SEEK_SET);
  }

  pos=input->tell();
  zoneEnd=pos+zoneSz[z++];
  int nData=(int) input->readLong(2);
  input->seek(2, WPX_SEEK_CUR);
  f.str("");
  f << "Entries(GStyle): N=" << nData << ",";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  int const gDataSize=vers==1 ? 0x34 : 0xaa;
  for (int i=0; i < nData; ++i) {
    pos = input->tell();
    f.str("");
    f << "GStyle-S" << i+1 << ":";
    GWGraphInternal::Style style;
    if (!readStyle(style))
      f << "###";
    else
      f << style;
    pageZone.m_styleList.push_back(style);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+gDataSize, WPX_SEEK_SET);
  }
  pos=input->tell();
  if (pos!=zoneEnd) {
    ascFile.addPos(pos);
    ascFile.addNote("GStyle-end:###");
    input->seek(zoneEnd, WPX_SEEK_SET);
  }

  if (vers==1) {
    pos=input->tell();
    zoneEnd=pos+zoneSz[z++];
    nData=(int) input->readLong(2);
    input->seek(2, WPX_SEEK_CUR);
    f.str("");
    f << "Entries(GLineFormat): N=" << nData << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    int const gLineFSize= 0x1e;
    for (int i=0; i < nData; ++i) {
      pos = input->tell();
      f.str("");
      f << "GLineFormat-L" << i << ",";
      std::string extra("");
      if (!readLineFormat(extra))
        f << "###";
      else
        f << extra;
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      input->seek(pos+gLineFSize, WPX_SEEK_SET);
    }
    pos=input->tell();
    if (pos!=zoneEnd) {
      ascFile.addPos(pos);
      ascFile.addNote("GLineFormat-end:###");
      input->seek(zoneEnd, WPX_SEEK_SET);
    }
  }

  pos = input->tell();
  zoneEnd=pos+zoneSz[z++];
  int nRoots=(int) input->readLong(2);
  input->seek(2, WPX_SEEK_CUR);
  f.str("");
  f << "GFrame[roots]: N=" << nRoots << ",roots=[";
  std::vector<int> rootList;
  for (int i=0; i < nRoots; ++i) {
    val = (int) input->readLong(2);
    if (val==0) {
      f << "_,";
      continue;
    }
    rootList.push_back(val);
    f << "F" << val << ",";
  }
  f << "],";
  if (input->tell()!=zoneEnd) { // ok
    ascFile.addDelimiter(input->tell(),'|');
    input->seek(zoneEnd, WPX_SEEK_SET);
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  std::vector<int> order;
  /* checkme:
     in text document, we must go through the last frame to the first frame to retrieve data
     in draw document, we must sequence the frames recursively ...
  */
  if (isDraw) {
    // sort rootList using m_order
    std::map<int,int> rootOrderMap;
    for (size_t i=0; i < rootList.size(); ++i) {
      int id=rootList[i];
      if (id<=0 || id>nFrames) {
        MWAW_DEBUG_MSG(("GWGraph::readPageFrames: can not find order for frame %d\n",id));
        continue;
      }
      int ord=frames[size_t(id-1)]->m_order;
      if (rootOrderMap.find(ord)!=rootOrderMap.end()) {
        MWAW_DEBUG_MSG(("GWGraph::readPageFrames: oops order %d already exist\n",ord));
        continue;
      }
      rootOrderMap[ord]=id;
    }
    rootList.resize(0);
    for (std::map<int,int>::iterator it=rootOrderMap.begin(); it!=rootOrderMap.end(); ++it)
      rootList.push_back(it->second);

    // we need now to reconstruct the tree structure
    std::vector<std::vector<int> > childs(size_t(nFrames+1));
    childs[0]=rootList;
    for (size_t i=0; i<size_t(nFrames); ++i) {
      GWGraphInternal::Frame &frame=*(frames[size_t(i)]);
      if (frame.m_parent<=0) continue;
      if (frame.m_parent>nFrames) {
        MWAW_DEBUG_MSG(("GWGraph::readPageFrames: find unknown parent with id=%d\n",frame.m_parent));
        continue;
      }
      childs[size_t(frame.m_parent)].push_back(int(i+1));
    }

    std::set<int> seens;
    buildFrameDataReadOrderFromTree(childs, 0, order, seens);
    if ((int) seens.size()!=nFrames+1 || (int) order.size()!=nFrames) {
      MWAW_DEBUG_MSG(("GWGraph::readPageFrames: do not find some child\n"));
    }
  } else {
    order.resize(size_t(nFrames));
    for (size_t i=0; i < size_t(nFrames); ++i)
      order[i]=nFrames-int(i);
  }
  // extra data
  for (size_t i=0; i < order.size(); ++i) {
    int id=order[i]-1;
    if (id<0|| id>=nFrames) {
      MWAW_DEBUG_MSG(("GWGraph::readPageFrames: can not find frame with id=%d\n",id));
      continue;
    }
    bool ok=true;
    shared_ptr<GWGraphInternal::Frame> zone=frames[size_t(id)];
    if (!zone) continue;
    f.str("");
    f << "GFrame-data:F" << id+1 << "[" << *zone << "]:";
    pos=input->tell();
    switch(zone->m_type) {
    case 0:
      MWAW_DEBUG_MSG(("GWGraph::readPageFrames: find group with type=0\n"));
      break;
    case 1: {
      ok=false;
      if (zone->getType()!=GWGraphInternal::Frame::T_TEXT) {
        MWAW_DEBUG_MSG(("GWGraph::readPageFrames: unexpected type for text\n"));
        break;
      }
      GWGraphInternal::FrameText &text=
        static_cast<GWGraphInternal::FrameText &>(*zone);
      if (!input->checkPosition(pos+text.m_dataSize)) {
        MWAW_DEBUG_MSG(("GWGraph::readPageFrames: text size seems bad\n"));
        break;
      }
      text.m_entry.setBegin(pos);
      text.m_entry.setLength(text.m_dataSize);
      input->seek(pos+text.m_dataSize, WPX_SEEK_SET);
      ok=true;
      break;
    }
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
      break;
    case 8: // poly normal
    case 7: // regular poly
    case 12: { // spline
      ok=false;
      if (zone->getType()!=GWGraphInternal::Frame::T_BASIC) {
        MWAW_DEBUG_MSG(("GWGraph::readPageFrames: unexpected type for basic graph\n"));
        break;
      }
      int nPt=(int) input->readLong(2);
      long endData=pos+10+8*nPt;
      if (pos+zone->m_dataSize > endData)
        endData=pos+zone->m_dataSize;
      if (nPt<0 || (endPos>0 && endData>endPos) ||
          (endPos<0 && !input->checkPosition(endData)))
        break;
      float dim[4];
      for (int j=0; j<4; ++j)
        dim[j]=float(input->readLong(4))/65536.f;
      f << "dim=" << dim[1] << "x" << dim[0] << "<->"
        << dim[3] << "x" << dim[2] << ",";
      f << "pt=[";
      GWGraphInternal::FrameBasic &graph=
        static_cast<GWGraphInternal::FrameBasic &>(*zone);
      float pt[2];
      Vec2f orig=graph.m_box[0];
      for (int p=0; p<nPt; ++p) {
        pt[0]=float(input->readLong(4))/65536.f;
        pt[1]=float(input->readLong(4))/65536.f;
        graph.m_vertices.push_back(Vec2f(pt[1],pt[0])-orig);
        f << pt[1] << "x" << pt[0] << ",";
      }
      f << "],";
      if (input->tell()!=endData) {
        ascFile.addDelimiter(input->tell(),'|');
        input->seek(endData,WPX_SEEK_SET);
      }
      ok = true;
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      break;
    }
    case 11: {
      ok=false;
      if (zone->getType()!=GWGraphInternal::Frame::T_PICTURE) {
        MWAW_DEBUG_MSG(("GWGraph::readPageFrames: unexpected type for picture\n"));
        break;
      }
      GWGraphInternal::FramePicture &pict=
        static_cast<GWGraphInternal::FramePicture &>(*zone);
      if (!input->checkPosition(pos+pict.m_dataSize)) {
        MWAW_DEBUG_MSG(("GWGraph::readPageFrames: picture size seems bad\n"));
        break;
      }
      pict.m_entry.setBegin(pos);
      pict.m_entry.setLength(pict.m_dataSize);
      input->seek(pos+pict.m_dataSize, WPX_SEEK_SET);
      ok=true;
      break;
    }
    case 15: {
      int nGrp=(int) input->readLong(2);
      if (nGrp<0 || (endPos>0 && pos+4+2*nGrp>endPos) ||
          (endPos<0 && !input->checkPosition(pos+4+2*nGrp))) {
        ok=false;
        break;
      }

      if (zone->getType()!=GWGraphInternal::Frame::T_GROUP) {
        MWAW_DEBUG_MSG(("GWGraph::readPageFrames: unexpected type for group\n"));
        input->seek(pos+4+2*nGrp, WPX_SEEK_SET);
        f << "###[internal]";
        break;
      }
      GWGraphInternal::FrameGroup &group=
        static_cast<GWGraphInternal::FrameGroup &>(*zone);

      if (nGrp != group.m_numChild) {
        f << "###[N=" << group.m_numChild << "]";
        MWAW_DEBUG_MSG(("GWGraph::readPageFrames: unexpected number of group child\n"));
      }
      val=(int) input->readLong(2); // always 2
      if (val!=2) f << "f0=" << val << ",";
      f << "grpId=[";
      for (int j=0; j < nGrp; ++j) {
        val = (int) input->readLong(2);
        group.m_childList.push_back(val);
        f << val << ",";
      }
      f << "],";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      break;
    }
    default:
      ok = false;
      break;
    }
    if (zone->m_dataSize>0 && input->tell()!=pos+zone->m_dataSize) {
      if (input->tell()>pos+zone->m_dataSize || !input->checkPosition(pos+zone->m_dataSize)) {
        MWAW_DEBUG_MSG(("GWGraph::readPageFrames: must stop, file position seems bad\n"));
        ascFile.addPos(pos);
        ascFile.addNote("GFrame###");
        if (endPos>0) {
          input->seek(endPos, WPX_SEEK_SET);
          return true;
        }
        return false;
      }
      if (ok) {
        ascFile.addPos(pos);
        ascFile.addNote("_");
      } else {
        f << "[##unparsed]";
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
      }
      input->seek(pos+zone->m_dataSize, WPX_SEEK_SET);
      continue;
    }

    if (ok) {
      pageZone.m_frameList.push_back(zone);
      continue;
    }
    MWAW_DEBUG_MSG(("GWGraph::readPageFrames: must stop parsing graphic data\n"));
    f << "###";
    if (endPos>0)
      input->seek(endPos, WPX_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  pos=input->tell();
  if (endPos>0 && pos!=endPos) {
    MWAW_DEBUG_MSG(("GWGraph::readPageFrames: find some end data\n"));
    input->seek(endPos, WPX_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote("GFrame-end:###");
  }
  m_state->m_zoneList.push_back(pageZone);
  return true;
}

shared_ptr<GWGraphInternal::Frame> GWGraph::readFrameHeader()
{
  int const vers=version();
  GWGraphInternal::Frame zone;
  shared_ptr<GWGraphInternal::Frame> res;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  long endPos=pos+54;
  if (!input->checkPosition(endPos))
    return res;

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  zone.m_type=(int) input->readLong(1);
  int locked=(int)input->readLong(1);
  if (zone.m_type<0||zone.m_type>16||locked<0||locked>1) {
    input->seek(pos, WPX_SEEK_SET);
    return res;
  }
  if (locked) f << "lock,";
  float dim[4];
  for (int i=0; i<4; ++i)
    dim[i]=float(input->readLong(4))/65536.f;
  if (dim[2]<dim[0] || dim[3]<dim[1]) {
    input->seek(pos, WPX_SEEK_SET);
    return res;
  }
  zone.m_box=Box2f(Vec2f(dim[1],dim[0]),Vec2f(dim[3],dim[2]));
  zone.m_style=(int) input->readULong(2);
  zone.m_parent=(int) input->readULong(2);
  zone.m_order=(int) input->readULong(2);
  switch(zone.m_type) {
  case 1:
    res.reset(new GWGraphInternal::FrameText(zone));
    res->m_dataSize=(long) input->readULong(4);
    break;
  case 11:
    res.reset(new GWGraphInternal::FramePicture(zone));
    res->m_dataSize=(long) input->readULong(4);
    break;
  case 15: {
    GWGraphInternal::FrameGroup *grp=new GWGraphInternal::FrameGroup(zone);
    res.reset(grp);
    grp->m_numChild=(int) input->readULong(2);
    break;
  }
  case 2: {
    GWGraphInternal::FrameBasic *graph=new GWGraphInternal::FrameBasic(zone);
    res.reset(graph);
    if (vers==1) {
      graph->m_lineArrow=(int) input->readLong(2);
      graph->m_lineFormat=(int) input->readLong(2);
    }
    float points[4];
    for (int i=0; i<4; ++i)
      points[i]=float(input->readLong(4))/65536;
    Vec2f orig(dim[1],dim[0]);
    graph->m_vertices.push_back(Vec2f(points[1],points[0])-orig);
    graph->m_vertices.push_back(Vec2f(points[3],points[2])-orig);
    break;
  }
  case 4: {
    GWGraphInternal::FrameBasic *graph=new GWGraphInternal::FrameBasic(zone);
    res.reset(graph);
    graph->m_values[0]=(int) input->readLong(2); // type
    graph->m_values[1]=(int) input->readLong(2); // corner dimension
    break;
  }
  case 6: {
    GWGraphInternal::FrameBasic *graph=new GWGraphInternal::FrameBasic(zone);
    res.reset(graph);
    for (int i=0; i < 2; ++i) // angles
      graph->m_values[i]=(int) input->readLong(2);
    graph->m_values[2]=(int) input->readLong(1); // 0:open, 1: close
    break;
  }
  case 3: // rect: no data
  case 5: {// oval no data
    GWGraphInternal::FrameBasic *graph=new GWGraphInternal::FrameBasic(zone);
    res.reset(graph);
    break;
  }
  case 7:
  case 8:
  case 12: {
    GWGraphInternal::FrameBasic *graph=new GWGraphInternal::FrameBasic(zone);
    res.reset(graph);
    graph->m_dataSize=(long) input->readULong(4);
    break;
  }
  default:
    break;
  }
  if (!res)
    res.reset(new GWGraphInternal::Frame(zone));
  res->m_extra=f.str();
  ascFile.addDelimiter(input->tell(),'|');
  input->seek(endPos, WPX_SEEK_SET);
  return res;
}

////////////////////////////////////////////////////////////
// picture
////////////////////////////////////////////////////////////
bool GWGraph::sendPicture(MWAWEntry const &entry, MWAWPosition pos)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("GWGraph::sendPicture: can not find the listener\n"));
    return true;
  }
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("GWGraph::sendPicture: can not find the entry\n"));
    return false;
  }
  entry.setParsed(true);
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long actPos = input->tell();

  input->seek(entry.begin(), WPX_SEEK_SET);
  shared_ptr<MWAWPict> thePict(MWAWPictData::get(input, (int)entry.length()));
  if (thePict) {
    WPXBinaryData data;
    std::string type;
    if (thePict->getBinary(data,type))
      listener->insertPicture(pos, data, type);
  }

#ifdef DEBUG_WITH_FILES
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  ascFile.skipZone(entry.begin(), entry.end()-1);
  WPXBinaryData file;
  input->seek(entry.begin(),WPX_SEEK_SET);
  input->readDataBlock(entry.length(), file);

  static int volatile pictName = 0;
  libmwaw::DebugStream f;
  f << "DATA-" << ++pictName << ".pct";
  libmwaw::Debug::dumpFile(file, f.str().c_str());
#endif

  input->seek(actPos, WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// send data to a listener
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
// low level
////////////////////////////////////////////////////////////
void GWGraph::buildFrameDataReadOrderFromTree
(std::vector<std::vector<int> > const &tree, int id, std::vector<int> &order, std::set<int> &seen)
{
  if (seen.find(id)!=seen.end()) {
    MWAW_DEBUG_MSG(("GWGraph::buildFrameDataReadOrderFromTree: id %d is already visited\n", id));
    return;
  }
  if (id < 0 || id >= int(tree.size())) {
    MWAW_DEBUG_MSG(("GWGraph::buildFrameDataReadOrderFromTree: the id %d seens bad\n", id));
    return;
  }
  seen.insert(id);
  std::vector<int> const &childs=tree[size_t(id)];
  if (id)
    order.push_back(id);

  bool isIdPushed=true;
  for (size_t c=childs.size(); c > 0; c--) {
    if (!isIdPushed && id > childs[c-1]) {
      order.push_back(id);
      isIdPushed=true;
    }
    buildFrameDataReadOrderFromTree(tree, childs[c-1], order, seen);
  }
  if (!isIdPushed && id)
    order.push_back(id);
}

////////////////////////////////////////////////////////////
// send data
////////////////////////////////////////////////////////////
bool GWGraph::sendBasic(GWGraphInternal::FrameBasic const &graph, GWGraphInternal::Zone const &zone, MWAWPosition pos)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("GWGraph::sendBasic: can not find a listener\n"));
    return false;
  }
  GWGraphInternal::Style style;
  if (graph.m_style>=1 && graph.m_style <= int(zone.m_styleList.size()))
    style = zone.m_styleList[size_t(graph.m_style-1)];
  shared_ptr<MWAWPictBasic> pict=graph.getPicture(style);
  if (!pict)
    return false;

  WPXBinaryData data;
  std::string type;
  if (!pict->getBinary(data,type))
    return false;

  pos.setOrigin(pos.origin()-Vec2f(2,2));
  pos.setSize(pos.size()+Vec2f(4,4));
  listener->insertPicture(pos,data, type);
  return true;
}

bool GWGraph::sendFrame(shared_ptr<GWGraphInternal::Frame> frame, GWGraphInternal::Zone const &zone, int order)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener || !frame) {
    MWAW_DEBUG_MSG(("GWGraph::sendFrame: can not find a listener\n"));
    return false;
  }
  frame->m_parsed=true;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  Vec2f LTPos(0,0);
  if (m_mainParser->getDocumentType()==GWParser::DRAW)
    LTPos=72.f*m_mainParser->getPageLeftTop();
  MWAWPosition fPos(frame->m_box[0]+LTPos,frame->m_box.size(),WPX_POINT);
  fPos.setRelativePosition(MWAWPosition::Page);
  fPos.setPage(frame->m_page<0 ? 1: frame->m_page);
  if (order>=0)
    fPos.setOrder(order);
  fPos.m_wrapping = MWAWPosition::WBackground;
  bool ok=true;
  switch (frame->getType()) {
  case GWGraphInternal::Frame::T_BASIC:
    ok = sendBasic(static_cast<GWGraphInternal::FrameBasic const &>(*frame), zone, fPos);
    break;
  case GWGraphInternal::Frame::T_PICTURE:
    ok = sendPicture(static_cast<GWGraphInternal::FramePicture const &>(*frame).m_entry, fPos);
    break;
  case GWGraphInternal::Frame::T_GROUP: // ok to ignore
    break;
  case GWGraphInternal::Frame::T_TEXT: {
    GWGraphInternal::FrameText const & text=
      static_cast<GWGraphInternal::FrameText const &>(*frame);
    shared_ptr<MWAWSubDocument> doc(new GWGraphInternal::SubDocument(*this, input, text.m_entry));
    Vec2f fSz=fPos.size();
    // increase slightly x and set y to atleast
    fPos.setSize(Vec2f(fSz[0]+3,-fSz[1]));
    listener->insertTextBox(fPos, doc);
    break;
  }
  case GWGraphInternal::Frame::T_BAD:
  case GWGraphInternal::Frame::T_UNSET:
  default:
    ok=false;
  }
  input->seek(pos, WPX_SEEK_SET);
  return ok;
}

bool GWGraph::sendPageFrames(GWGraphInternal::Zone const &zone)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("GWGraph::sendPageFrames: can not find a listener\n"));
    return false;
  }
  bool isDraw = m_mainParser->getDocumentType()==GWParser::DRAW;
  zone.m_parsed=true;
  int order=int(zone.m_frameList.size());
  for (size_t f=0; f < zone.m_frameList.size(); ++f) {
    if (!zone.m_frameList[f])
      continue;
    shared_ptr<GWGraphInternal::Frame> frame=zone.m_frameList[f];
    if (frame->m_parsed)
      continue;
    sendFrame(frame,zone,isDraw ? --order : frame->m_order);
  }
  return true;
}

bool GWGraph::sendPageGraphics()
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("GWGraph::sendPageGraphics: can not find a listener\n"));
    return false;
  }
  for (size_t z=0; z < m_state->m_zoneList.size(); ++z) {
    if (m_state->m_zoneList[z].m_parsed)
      continue;
    sendPageFrames(m_state->m_zoneList[z]);
  }
  return true;
}

void GWGraph::flushExtra()
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("GWGraph::flushExtra: can not find a listener\n"));
    return;
  }
  for (size_t z=0; z < m_state->m_zoneList.size(); ++z) {
    if (m_state->m_zoneList[z].m_parsed)
      continue;
    sendPageFrames(m_state->m_zoneList[z]);
  }
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
