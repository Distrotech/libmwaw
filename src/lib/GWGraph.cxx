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
#include "MWAWGraphicShape.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSubDocument.hxx"

#include "GWParser.hxx"

#include "GWGraph.hxx"

/** Internal: the structures of a GWGraph */
namespace GWGraphInternal
{
////////////////////////////////////////
//! Internal: the graphic zone of a GWGraph
struct Frame {
  //! the frame type
  enum Type { T_BAD, T_BASIC, T_GROUP, T_PICTURE, T_TEXT, T_UNSET };
  //! constructor
  Frame() : m_type(-1), m_styleId(-1), m_parent(0), m_order(-1), m_dataSize(0), m_box(), m_page(-1), m_extra(""), m_parsed(false) {
  }
  //! destructor
  virtual ~Frame() {
  }
  //! return the frame type
  virtual Type getType() const {
    return T_UNSET;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Frame const &zone) {
    zone.print(o);
    return o;
  }
  //! a virtual print function
  virtual void print(std::ostream &o) const {
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
    if (m_styleId >= 0)
      o << "S" << m_styleId << ",";
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
  int m_styleId;
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
//! Internal: the basic shape of a GWGraph
struct FrameShape : public Frame {
  //! constructor
  FrameShape(Frame const &frame) : Frame(frame), m_shape(), m_lineArrow(0), m_lineFormat(0) {
  }
  //! return the frame type
  virtual Type getType() const {
    return T_BASIC;
  }
  //! print function
  virtual void print(std::ostream &o) const {
    Frame::print(o);
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
  //! the shape
  MWAWGraphicShape m_shape;
  //! the line arrow style (in v1)
  int m_lineArrow;
  //! the line format?
  int m_lineFormat;
private:
  FrameShape(FrameShape const &);
  FrameShape &operator=(FrameShape const &);
};

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
  virtual void print(std::ostream &o) const {
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
  virtual void print(std::ostream &o) const {
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
  virtual void print(std::ostream &o) const {
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
  Zone() : m_page(-1), m_frameList(), m_frameOrderList(), m_styleList(), m_parsed(false) {
  }
  //! the page number (if known)
  int m_page;
  //! the list of frame
  std::vector<shared_ptr<Frame> > m_frameList;
  //! the list of id list of frame to send
  std::vector<int> m_frameOrderList;
  //! the list of style
  std::vector<MWAWGraphicStyle> m_styleList;
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
    MWAWGraphicStyle::Pattern pat;
    pat.m_dim=Vec2i(8,8);
    pat.m_data.resize(8);
    for (size_t j=0; j < 8; ++j)
      pat.m_data[j]=(unsigned char) input->readLong(1);
    f << pat;
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

  // 16 sets like: ffff, 6464, 0202 : maybe some color
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
    MWAWGraphicStyle style;
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
    if (i!=nZones-1 && nData *fSz+4!=zoneSz[i]) {
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

bool GWGraph::readStyle(MWAWGraphicStyle &style)
{
  style=MWAWGraphicStyle();
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
  if (dim[0]<dim[1] || dim[0]>dim[1]) f << "lineWidth[real]=" << Vec2f(dim[1],dim[0]) << ",";
  style.m_lineWidth=(dim[1]+dim[0])/2.f;
  if  (vers==1) {
    for (int i=0; i < 2; ++i) { // two flags 0|1
      val=(int) input->readULong(1);
      if (val==0 || val==1) {
        if (i==0)
          style.m_lineOpacity=float(val);
        else
          style.m_surfaceOpacity=float(val);
      } else
        f << "#hasPat" << i << "=" << val << ",";
    }
    MWAWGraphicStyle::Pattern patterns[2];
    for (int i=0; i < 2; ++i) {
      patterns[i].m_dim=Vec2i(8,8);
      patterns[i].m_data.resize(8);
      for (size_t j=0; j < 8; ++j)
        patterns[i].m_data[j]=(unsigned char) input->readULong(1);
    }
    for (int i=0; i < 4; ++i) {
      unsigned char col[3];
      for (int j=0; j < 3; ++j)
        col[j]=(unsigned char)(input->readULong(2)>>8);
      patterns[i/2].m_colors[1-i%2]=MWAWColor(col[0], col[1], col[2]);
    }
    if (!patterns[0].getUniqueColor(style.m_lineColor)) {
      f << "linePattern=[" << patterns[0] << "],";
      patterns[0].getAverageColor(style.m_lineColor);
    }
    if (!patterns[1].getUniqueColor(style.m_surfaceColor)) {
      if (style.m_surfaceOpacity<=0)
        f << "surfPattern=[" << patterns[1] << "],";
      else
        style.setPattern(patterns[1]);
    }

    style.m_extra=f.str();
    input->seek(endPos, WPX_SEEK_SET);
    return true;
  }
  MWAWGraphicStyle::Pattern patterns[2];
  for (int i=0; i < 4; ++i) {
    val=(int) input->readULong(2);
    if (val) f << "col" << i << "=" << std::hex << val << std::dec << ",";
    unsigned char col[3];
    for (int j=0; j < 3; ++j)
      col[j]=(unsigned char)(input->readULong(2)>>8);
    patterns[i/2].m_colors[1-i%2]=MWAWColor(col[0], col[1], col[2]);
  }
  val=(int) input->readULong(2);
  if (val) f << "col4=" << std::hex << val << std::dec << ",";
  for (int i=0; i < 2; ++i) {
    val=(int) input->readULong(2); // small number
    if (i==0)
      style.m_lineOpacity=val ? 1.0 : 0.0;
    else
      style.m_surfaceOpacity=val ? 1.0 : 0.0;
    if (val>1)
      f << "pat" << i << "=" << val << ",";
    patterns[i].m_dim=Vec2i(8,8);
    patterns[i].m_data.resize(8);
    for (size_t j=0; j < 8; ++j)
      patterns[i].m_data[j]=(unsigned char) input->readULong(1);
  }
  if (!patterns[0].getUniqueColor(style.m_lineColor)) {
    f << "linePattern=[" << patterns[0] << "],";
    patterns[0].getAverageColor(style.m_lineColor);
  }
  if (!patterns[1].getUniqueColor(style.m_surfaceColor)) {
    if (style.m_surfaceOpacity<=0)
      f << "surfPattern=[" << patterns[1] << "],";
    else
      style.setPattern(patterns[1]);
  }

  val=(int) input->readULong(2);
  if (val!=1) f << "patId=" << val << ",";

  int nDash=val==1 ? 1 : (int)input->readULong(2);
  if (nDash<0||nDash>6) {
    MWAW_DEBUG_MSG(("GWGraph::readStyle: can not read number of line dash\n"));
    f << "#nDash=" << nDash << ",";
  } else {
    for (int i=0; i < nDash; ++i) {
      float w=float(input->readLong(4))/65536.f;
      if (w<=0) {
        if (i==0 && nDash==1)
          break;
        MWAW_DEBUG_MSG(("GWGraph::readStyle: the line dash seems bad\n"));
        f << "###dash" << i << ":w=" << w << ",";
        style.m_lineDashWidth.resize(0);
        break;
      }
      style.m_lineDashWidth.push_back(w);
    }
  }
  input->seek(pos+116, WPX_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  ascFile.addDelimiter(pos+92,'|'); // some data?
  ascFile.addDelimiter(input->tell(),'|');
  int gradId=(int) input->readLong(2);
  if (gradId) f << "gradientId=" << gradId << ",";
  int gradType=(int) input->readLong(2);
  if (gradId>=1 && gradId<=16 && (gradType>=1&&gradType<=3)) {
    style.m_gradientStopList.resize(2);
    style.m_gradientStopList[0]=MWAWGraphicStyle::GradientStop(0.0, MWAWColor::white());
    style.m_gradientStopList[1]=MWAWGraphicStyle::GradientStop(1.0, MWAWColor::black());
    style.m_gradientType = MWAWGraphicStyle::G_Linear;
    switch(gradId) {
    case 1:
      if (gradType!=1) style.m_gradientType = MWAWGraphicStyle::G_None;
      style.m_gradientAngle = -90;
      break;
    case 2:
      if (gradType!=1) style.m_gradientType = MWAWGraphicStyle::G_None;
      style.m_gradientAngle = -45;
      break;
    case 3:
      if (gradType!=1) style.m_gradientType = MWAWGraphicStyle::G_None;
      break;
    case 4:
      if (gradType!=1) style.m_gradientType = MWAWGraphicStyle::G_None;
      style.m_gradientStopList[0]=MWAWGraphicStyle::GradientStop(0.0, MWAWColor::black());
      style.m_gradientStopList[1]=MWAWGraphicStyle::GradientStop(1.0, MWAWColor::white());
      style.m_gradientType=MWAWGraphicStyle::G_Axial;
      style.m_gradientAngle = -45;
      break;
    case 5:
      if (gradType!=1) style.m_gradientType = MWAWGraphicStyle::G_None;
      style.m_gradientAngle = 90;
      break;
    case 6:
      if (gradType!=1) style.m_gradientType = MWAWGraphicStyle::G_None;
      style.m_gradientStopList[0]=MWAWGraphicStyle::GradientStop(0.0, MWAWColor::black());
      style.m_gradientStopList[1]=MWAWGraphicStyle::GradientStop(1.0, MWAWColor::white());
      style.m_gradientType=MWAWGraphicStyle::G_Axial;
      style.m_gradientAngle = 45;
      break;
    case 7:
      if (gradType!=1) style.m_gradientType = MWAWGraphicStyle::G_None;
      style.m_gradientAngle = 180;
      break;
    case 8:
      if (gradType!=1) style.m_gradientType = MWAWGraphicStyle::G_None;
      style.m_gradientAngle = -135;
      break;
    case 9:
      if (gradType!=3) style.m_gradientType = MWAWGraphicStyle::G_None;
      style.m_gradientPercentCenter=Vec2f(.5f,.5f);
      style.m_gradientStopList[0]=MWAWGraphicStyle::GradientStop(0.0, MWAWColor::black());
      style.m_gradientStopList[1]=MWAWGraphicStyle::GradientStop(1.0, MWAWColor::white());
      style.m_gradientType=MWAWGraphicStyle::G_Square;
      break;
    case 10:
      if (gradType!=3) style.m_gradientType = MWAWGraphicStyle::G_None;
      style.m_gradientPercentCenter=Vec2f(.5f,.5f);
      style.m_gradientType=MWAWGraphicStyle::G_Square;
      break;
    case 11:
    case 12:
      if (gradType!=2) style.m_gradientType = MWAWGraphicStyle::G_None;
      style.m_gradientStopList[0]=MWAWGraphicStyle::GradientStop(0.0, MWAWColor::black());
      style.m_gradientStopList[1]=MWAWGraphicStyle::GradientStop(1.0, MWAWColor::white());
      style.m_gradientType=MWAWGraphicStyle::G_Radial;
      break;
    case 13:
      if (gradType!=1) style.m_gradientType = MWAWGraphicStyle::G_None;
      style.m_gradientStopList[0]=MWAWGraphicStyle::GradientStop(0.0, MWAWColor(0x88,0,0));
      style.m_gradientStopList[1]=MWAWGraphicStyle::GradientStop(1.0, MWAWColor(0xee,0,0));
      style.m_gradientAngle = 90;
      break;
    case 14:
      if (gradType!=1) style.m_gradientType = MWAWGraphicStyle::G_None;
      style.m_gradientStopList[0]=MWAWGraphicStyle::GradientStop(0.0, MWAWColor(0,0x55,0));
      style.m_gradientStopList[1]=MWAWGraphicStyle::GradientStop(1.0, MWAWColor(0,0xee,0));
      style.m_gradientAngle = 90;
      break;
    case 15:
      if (gradType!=1) style.m_gradientType = MWAWGraphicStyle::G_None;
      style.m_gradientStopList[0]=MWAWGraphicStyle::GradientStop(0.0, MWAWColor(0,0,0x88));
      style.m_gradientStopList[1]=MWAWGraphicStyle::GradientStop(1.0, MWAWColor(0,0,0xff));
      style.m_gradientAngle = 90;
      break;
    case 16:
      if (gradType!=1) style.m_gradientType = MWAWGraphicStyle::G_None;
      style.m_gradientStopList[0]=MWAWGraphicStyle::GradientStop(0.0, MWAWColor(0xff,0xff,0));
      style.m_gradientStopList[1]=MWAWGraphicStyle::GradientStop(1.0, MWAWColor(0xff,0xff,0xcc));
      style.m_gradientAngle = 90;
      break;
    default:
      break;
    }
    if (style.m_gradientType==MWAWGraphicStyle::G_None) {
      MWAW_DEBUG_MSG(("GWGraph::readStyle: find odd gradient\n"));
      f << "grad[##type]=" << gradType << ",";
    }
  }
  for (size_t i=0; i < 2; ++i) {
    unsigned char col[3];
    for (int j=0; j < 3; ++j)
      col[j]=(unsigned char)(input->readULong(2)>>8);
    f << "grad[col" << i << "]=" << MWAWColor(col[0], col[1], col[2]) << ",";
  }
  input->seek(2, WPX_SEEK_CUR); // junk ?
  val=(int) input->readLong(2);
  switch(val) {
  case 1: // none
    break;
  case 2:
    style.m_arrows[1]=true;
    break;
  case 3:
    style.m_arrows[0]=true;
    break;
  case 4:
    style.m_arrows[0]=style.m_arrows[1]=true;
    break;
  default:
    f << "#lineArrows=" << val << ",";
  }
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
  std::vector<shared_ptr<GWGraphInternal::Frame> > &frames=pageZone.m_frameList;
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
    MWAWGraphicStyle style;
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

  if (isDraw) {
    // in draw document, we must sequence the frames recursively ...
    // sort rootList using m_order
    std::map<int,int> orderMap;
    for (size_t i=0; i < rootList.size(); ++i) {
      int id=rootList[i];
      if (id<=0 || id>nFrames) {
        MWAW_DEBUG_MSG(("GWGraph::readPageFrames: can not find order for frame %d\n",id));
        continue;
      }
      int ord=frames[size_t(id-1)]->m_order;
      if (orderMap.find(ord)!=orderMap.end()) {
        MWAW_DEBUG_MSG(("GWGraph::readPageFrames: oops order %d already exist\n",ord));
        continue;
      }
      orderMap[ord]=id;
    }
    rootList.resize(0);
    for (std::map<int,int>::iterator it=orderMap.begin(); it!=orderMap.end(); ++it)
      rootList.push_back(it->second);

    std::set<int> seens;
    for (size_t c=rootList.size(); c > 0; ) {
      if (!readFrameExtraDataRec(pageZone, rootList[--c]-1, seens, endPos)) {
        if (endPos>0)
          input->seek(endPos, WPX_SEEK_SET);
        return false;
      }
    }
    m_state->m_zoneList.push_back(pageZone);
    return true;
  }

  // in text document, we must go through the last frame to the first frame to retrieve data
  for (int id=nFrames-1; id >= 0; --id) {
    if (id<0|| id>=nFrames) {
      MWAW_DEBUG_MSG(("GWGraph::readPageFrames: can not find frame with id=%d\n",id));
      continue;
    }
    shared_ptr<GWGraphInternal::Frame> zone=frames[size_t(id)];
    if (!zone) continue;
    pos=input->tell();
    bool ok = readFrameExtraData(*zone, id, endPos);
    f.str("");
    f << "GFrame-data:F" << id+1 << "[" << *zone << "]:";
    if (zone->m_dataSize>0 && input->tell()!=pos+zone->m_dataSize) {
      if (input->tell()>pos+zone->m_dataSize || !input->checkPosition(pos+zone->m_dataSize)) {
        MWAW_DEBUG_MSG(("GWGraph::readPageFrames: must stop, file position seems bad\n"));
        f << "###";
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        if (endPos>0) {
          input->seek(endPos, WPX_SEEK_SET);
          return true;
        }
        return false;
      }
      if (ok)
        pageZone.m_frameOrderList.push_back(id);
      else {
        f << "[##unparsed]";
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
      }
      input->seek(pos+zone->m_dataSize, WPX_SEEK_SET);
      continue;
    }
    if (ok) {
      pageZone.m_frameOrderList.push_back(id);
      continue;
    }
    MWAW_DEBUG_MSG(("GWGraph::readPageFrames: must stop parsing graphic data\n"));
    if (endPos>0)
      input->seek(endPos, WPX_SEEK_SET);
    f << "###";
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

bool GWGraph::readFrameExtraDataRec(GWGraphInternal::Zone &zone, int id, std::set<int> &seens, long endPos)
{
  if (seens.find(id)!=seens.end()) {
    MWAW_DEBUG_MSG(("GWGraph::readPageFrames: index %d is already read\n", id));
    return false;
  }
  if (id < 0 || id >= int(zone.m_frameList.size())) {
    MWAW_DEBUG_MSG(("GWGraph::readFrameExtraDataRec: can not find zone %d\n", id));
    return false;
  }
  seens.insert(id);
  shared_ptr<GWGraphInternal::Frame> frame=zone.m_frameList[size_t(id)];
  if (!frame) return true;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  if (!readFrameExtraData(*frame, id, endPos)) return false;
  if (frame->m_dataSize>0 && input->tell()!=pos+frame->m_dataSize) {
    if (input->tell()>pos+frame->m_dataSize || !input->checkPosition(pos+frame->m_dataSize)) {
      MWAW_DEBUG_MSG(("GWGraph::readFrameExtraDataRec: must stop, file position seems bad\n"));
      libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
      ascFile.addPos(pos);
      ascFile.addNote("GFrame-Data###");
      if (endPos>0)
        input->seek(endPos, WPX_SEEK_SET);
      return false;
    }
    input->seek(pos+frame->m_dataSize, WPX_SEEK_SET);
  }
  zone.m_frameOrderList.push_back(id);
  if (frame->getType()!=GWGraphInternal::Frame::T_GROUP)
    return true;
  GWGraphInternal::FrameGroup &group=reinterpret_cast<GWGraphInternal::FrameGroup &>(*frame);
  for (size_t c=0; c < group.m_childList.size(); ++c) {
    if (!readFrameExtraDataRec(zone, group.m_childList[c]-1, seens, endPos)) {
      group.m_childList.resize(c);
      return false;
    }
  }
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
  zone.m_styleId=(int) input->readULong(2);
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
    GWGraphInternal::FrameShape *graph=new GWGraphInternal::FrameShape(zone);
    res.reset(graph);
    if (vers==1) {
      graph->m_lineArrow=(int) input->readLong(2);
      graph->m_lineFormat=(int) input->readLong(2);
    }
    float points[4];
    for (int i=0; i<4; ++i)
      points[i]=float(input->readLong(4))/65536;
    graph->m_shape=MWAWGraphicShape::line(Vec2f(points[1],points[0]), Vec2f(points[3],points[2]));
    break;
  }
  case 4: {
    GWGraphInternal::FrameShape *graph=new GWGraphInternal::FrameShape(zone);
    res.reset(graph);
    int roundType = (int) input->readLong(2);
    float cornerDim = (float) input->readLong(2);
    graph->m_shape = MWAWGraphicShape::rectangle(zone.m_box);
    switch(roundType) {
    case 1: // normal
      graph->m_shape.m_cornerWidth[0]=cornerDim > zone.m_box.size()[0] ? zone.m_box.size()[0]/2.f : cornerDim/2.f;
      graph->m_shape.m_cornerWidth[1]=cornerDim > zone.m_box.size()[1] ? zone.m_box.size()[1]/2.f : cornerDim/2.f;
      break;
    case 2:
      f << "cornerDim[minWidth/2],";
      cornerDim = zone.m_box.size()[0] < zone.m_box.size()[1] ? zone.m_box.size()[0] : zone.m_box.size()[1];
      graph->m_shape.m_cornerWidth[0]=graph->m_shape.m_cornerWidth[1]=0.5f*cornerDim;
      break;
    default:
      f << "#type[corner]=" << roundType << "[" << cornerDim << "],";
      break;
    }
    break;
  }
  case 6: {
    GWGraphInternal::FrameShape *graph=new GWGraphInternal::FrameShape(zone);
    res.reset(graph);
    int fileAngle[2];
    for (int i=0; i < 2; ++i) // angles
      fileAngle[i]=(int) input->readLong(2);
    int type=(int) input->readLong(1); // 0:open, 1: close
    if (type==1) f << "closed,";
    else if (type) f << "#type=" << type << ",";

    int angle[2] = { int(90-fileAngle[0]-fileAngle[1]), int(90-fileAngle[0]) };
    if (fileAngle[1]<0) {
      angle[0]=int(90-fileAngle[0]);
      angle[1]=int(90-fileAngle[0]-fileAngle[1]);
    } else if (fileAngle[1]==360)
      angle[0]=int(90-fileAngle[0]-359);
    while (angle[1] > 360) {
      angle[0]-=360;
      angle[1]-=360;
    }
    while (angle[0] < -360) {
      angle[0]+=360;
      angle[1]+=360;
    }
    Vec2f center = zone.m_box.center();
    Vec2f axis = 0.5*Vec2f(zone.m_box.size());
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
    graph->m_shape = MWAWGraphicShape::arc(realBox, zone.m_box, Vec2f(float(angle[0]), float(angle[1])));
    break;
  }
  case 3: // rect: no data
  case 5: { // oval no data
    GWGraphInternal::FrameShape *graph=new GWGraphInternal::FrameShape(zone);
    MWAWGraphicShape &shape = graph->m_shape;
    res.reset(graph);
    shape.m_bdBox = shape.m_formBox = zone.m_box;
    shape.m_type = zone.m_type==3 ? MWAWGraphicShape::Rectangle : MWAWGraphicShape::Circle;
    break;
  }
  case 7:
  case 8:
  case 12: {
    GWGraphInternal::FrameShape *graph=new GWGraphInternal::FrameShape(zone);
    res.reset(graph);
    graph->m_shape = zone.m_type==12 ? MWAWGraphicShape::path(zone.m_box) : MWAWGraphicShape::polygon(zone.m_box);
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

bool GWGraph::readFrameExtraData(GWGraphInternal::Frame &frame, int id, long endPos)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "GFrame-data:F" << id+1 << "[" << frame << "]:";
  long pos=input->tell();
  int val;
  switch(frame.m_type) {
  case 0:
    MWAW_DEBUG_MSG(("GWGraph::readFrameExtraData: find group with type=0\n"));
    return false;
  case 1: {
    if (frame.getType()!=GWGraphInternal::Frame::T_TEXT) {
      MWAW_DEBUG_MSG(("GWGraph::readFrameExtraData: unexpected type for text\n"));
      return false;
    }
    GWGraphInternal::FrameText &text=reinterpret_cast<GWGraphInternal::FrameText &>(frame);
    if (!input->checkPosition(pos+text.m_dataSize)) {
      MWAW_DEBUG_MSG(("GWGraph::readFrameExtraData: text size seems bad\n"));
      return false;
    }
    text.m_entry.setBegin(pos);
    text.m_entry.setLength(text.m_dataSize);
    input->seek(pos+text.m_dataSize, WPX_SEEK_SET);
    return true;
  }
  case 2:
  case 3:
  case 4:
  case 5:
  case 6:
    return true;
  case 8: // poly normal
  case 7: // regular poly
  case 12: { // spline
    if (frame.getType()!=GWGraphInternal::Frame::T_BASIC) {
      MWAW_DEBUG_MSG(("GWGraph::readFrameExtraData: unexpected type for basic graph\n"));
      return false;
    }
    int nPt=(int) input->readLong(2);
    long endData=pos+10+8*nPt;
    if (pos+frame.m_dataSize > endData)
      endData=pos+frame.m_dataSize;
    if (nPt<0 || (endPos>0 && endData>endPos) ||
        (endPos<0 && !input->checkPosition(endData)))
      return false;
    float dim[4];
    for (int j=0; j<4; ++j)
      dim[j]=float(input->readLong(4))/65536.f;
    f << "dim=" << dim[1] << "x" << dim[0] << "<->"
      << dim[3] << "x" << dim[2] << ",";
    f << "pt=[";
    GWGraphInternal::FrameShape &graph=reinterpret_cast<GWGraphInternal::FrameShape &>(frame);
    float pt[2];
    std::vector<Vec2f> vertices;
    for (int p=0; p<nPt; ++p) {
      pt[0]=float(input->readLong(4))/65536.f;
      pt[1]=float(input->readLong(4))/65536.f;
      vertices.push_back(Vec2f(pt[1],pt[0]));
      f << pt[1] << "x" << pt[0] << ",";
    }
    f << "],";
    if (graph.m_shape.m_type == MWAWGraphicShape::Polygon)
      graph.m_shape.m_vertices = vertices;
    else if (graph.m_shape.m_type == MWAWGraphicShape::Path) {
      if (nPt<4 || (nPt%3)!=1) {
        MWAW_DEBUG_MSG(("GWGraph::readFrameExtraData: the spline number of points seems bad\n"));
        f << "###";
      } else {
        std::vector<MWAWGraphicShape::PathData> &path=graph.m_shape.m_path;
        path.push_back(MWAWGraphicShape::PathData('M', vertices[0]));

        for (size_t p=1; p < size_t(nPt); p+=3) {
          bool hasFirstC=vertices[p-1]!=vertices[p];
          bool hasSecondC=vertices[p+1]!=vertices[p+2];
          if (!hasFirstC && !hasSecondC)
            path.push_back(MWAWGraphicShape::PathData('L', vertices[p+2]));
          else if (hasFirstC)
            path.push_back(MWAWGraphicShape::PathData('C', vertices[p+2], vertices[p+1], vertices[p+1]));
          else
            path.push_back(MWAWGraphicShape::PathData('S', vertices[p+2], vertices[p+1]));
        }
        path.push_back(MWAWGraphicShape::PathData('Z'));
      }
    } else {
      MWAW_DEBUG_MSG(("GWGraph::readFrameExtraData: find unexpected vertices\n"));
      f << "###";
    }
    if (input->tell()!=endData) {
      ascFile.addDelimiter(input->tell(),'|');
      input->seek(endData,WPX_SEEK_SET);
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  case 11: {
    if (frame.getType()!=GWGraphInternal::Frame::T_PICTURE) {
      MWAW_DEBUG_MSG(("GWGraph::readFrameExtraData: unexpected type for picture\n"));
      return false;
    }
    GWGraphInternal::FramePicture &pict=reinterpret_cast<GWGraphInternal::FramePicture &>(frame);
    if (!input->checkPosition(pos+pict.m_dataSize)) {
      MWAW_DEBUG_MSG(("GWGraph::readFrameExtraData: picture size seems bad\n"));
      return false;
    }
    pict.m_entry.setBegin(pos);
    pict.m_entry.setLength(pict.m_dataSize);
    input->seek(pos+pict.m_dataSize, WPX_SEEK_SET);
    return true;
  }
  case 15: {
    int nGrp=(int) input->readLong(2);
    if (nGrp<0 || (endPos>0 && pos+4+2*nGrp>endPos) ||
        (endPos<0 && !input->checkPosition(pos+4+2*nGrp))) {
      MWAW_DEBUG_MSG(("GWGraph::readFrameExtraData: unexpected number of group\n"));
      return false;
    }

    if (frame.getType()!=GWGraphInternal::Frame::T_GROUP) {
      MWAW_DEBUG_MSG(("GWGraph::readFrameExtraData: unexpected type for group\n"));
      input->seek(pos+4+2*nGrp, WPX_SEEK_SET);
      f << "###[internal]";
      return false;
    }
    GWGraphInternal::FrameGroup &group=reinterpret_cast<GWGraphInternal::FrameGroup &>(frame);
    if (nGrp != group.m_numChild) {
      f << "###[N=" << group.m_numChild << "]";
      MWAW_DEBUG_MSG(("GWGraph::readFrameExtraData: unexpected number of group child\n"));
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
    return true;
  }
  default:
    break;
  }
  MWAW_DEBUG_MSG(("GWGraph::readFrameExtraData: unexpected type\n"));
  return false;
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
// send data
////////////////////////////////////////////////////////////
bool GWGraph::sendShape(GWGraphInternal::FrameShape const &graph, GWGraphInternal::Zone const &zone, MWAWPosition pos)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("GWGraph::sendShape: can not find a listener\n"));
    return false;
  }
  MWAWGraphicStyle style;
  if (graph.m_styleId>=1 && graph.m_styleId <= int(zone.m_styleList.size()))
    style = zone.m_styleList[size_t(graph.m_styleId-1)];
  if (graph.m_shape.m_type!=MWAWGraphicShape::Line) {
    style.m_arrows[0]=style.m_arrows[1]=false;
    style.m_lineDashWidth.resize(0);
  } else if (graph.m_lineArrow > 1) {
    switch(graph.m_lineArrow) {
    case 2:
      style.m_arrows[1]=true;
      break;
    case 3:
      style.m_arrows[0]=true;
      break;
    case 4:
      style.m_arrows[0]=style.m_arrows[1]=true;
      break;
    default:
      break;
    }
  }

  pos.setOrigin(pos.origin()-Vec2f(2,2));
  pos.setSize(pos.size()+Vec2f(4,4));
  listener->insertPicture(pos,graph.m_shape, style);
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
    ok = sendShape(reinterpret_cast<GWGraphInternal::FrameShape const &>(*frame), zone, fPos);
    break;
  case GWGraphInternal::Frame::T_PICTURE:
    ok = sendPicture(reinterpret_cast<GWGraphInternal::FramePicture const &>(*frame).m_entry, fPos);
    break;
  case GWGraphInternal::Frame::T_GROUP: // ok to ignore
    break;
  case GWGraphInternal::Frame::T_TEXT: {
    GWGraphInternal::FrameText const &text=
      reinterpret_cast<GWGraphInternal::FrameText const &>(*frame);
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
  int order=int(zone.m_frameOrderList.size());
  for (size_t f=0; f < zone.m_frameOrderList.size(); ++f) {
    size_t id=(size_t) zone.m_frameOrderList[f];
    if (!zone.m_frameList[id])
      continue;
    shared_ptr<GWGraphInternal::Frame> frame=zone.m_frameList[id];
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
