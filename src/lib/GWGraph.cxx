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
//! Internal: the graphic zone of a GWGraph
struct Frame {
  //! the frame type
  enum Type { T_BAD, T_BASIC, T_GROUP, T_PICTURE, T_TEXT, T_UNSET };
  //! constructor
  Frame() : m_type(-1), m_layout(-1), m_parent(0), m_order(-1), m_dataSize(0), m_box(), m_page(-1), m_extra(""), m_parsed(false) {
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
    switch(zone.m_type) {
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
      o << "type=" << zone.m_type << ",";
      break;
    }
    if (zone.m_layout >= 0)
      o << "layout=" << zone.m_layout << ",";
    if (zone.m_order >= 0)
      o << "order=" << zone.m_order << ",";
    if (zone.m_parent > 0)
      o << "F" << zone.m_parent << "[parent],";
    if (zone.m_dataSize > 0)
      o << "dataSize=" << zone.m_dataSize << ",";
    o << "box=" << zone.m_box << ",";
    if (zone.m_page>0)
      o << "page=" << zone.m_page << ",";
    o << zone.m_extra;
    return o;
  }
  //! the zone type
  int m_type;
  //! the layout identifier
  int m_layout;
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
  FrameBasic() : Frame(), m_vertices() {
    for (int i=0; i < 2; ++i)
      m_values[i]=0;
  }
  //! return the frame type
  virtual Type getType() const {
    return T_BASIC;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream & o, FrameBasic const &grph) {
    o << static_cast<Frame const &>(grph);
    if (grph.m_type==4)
      o << "cornerDim=" << grph.m_values[0]<< "x" << grph.m_values[1] << ",";
    else if (grph.m_type==6)
      o << "angles=" << grph.m_values[0]<< "x" << grph.m_values[1] << ",";
    if (grph.m_vertices.size()) {
      o << "vertices=[";
      for (size_t i = 0; i < grph.m_vertices.size(); i++)
        o << grph.m_vertices[i] << ",";
      o << "],";
    }

    return o;
  }
  //! arc : the angles, rectoval : the corner dimension
  float m_values[2];
  //! the polygon vertices
  std::vector<Vec2f> m_vertices;
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
  //! operator<<
  friend std::ostream &operator<<(std::ostream & o, FrameGroup const &grp) {
    o << static_cast<Frame const &>(grp);
    if (grp.m_numChild)
      o << "nChild=" << grp.m_numChild << ",";
    return o;
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
  //! operator<<
  friend std::ostream &operator<<(std::ostream & o, FramePicture const &pic) {
    o << static_cast<Frame const &>(pic);
    if (pic.m_entry.valid())
      o << "pos=" << std::hex << pic.m_entry.begin() << "->" << pic.m_entry.end() << std::dec << ",";
    return o;
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
  //! operator<<
  friend std::ostream &operator<<(std::ostream & o, FrameText const &text) {
    o << static_cast<Frame const &>(text);
    if (text.m_entry.valid())
      o << "pos=" << std::hex << text.m_entry.begin() << "->" << text.m_entry.end() << std::dec << ",";
    return o;
  }
  //! the text entry
  MWAWEntry m_entry;
};

////////////////////////////////////////
//! Internal: the state of a GWGraph
struct State {
  //! constructor
  State() : m_frameList(), m_numPages(0) { }
  //! the list of frame
  std::vector<shared_ptr<Frame> > m_frameList;
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
  if (!m_mainParser->isFilePos(pos+headerSize))
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
  if (!m_mainParser->isFilePos(pos+headerSize+pageHeaderSize)) {
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

    ascFile.addPos(pos);
    ascFile.addNote("Entries(GDatB)[_]:");
    pos += 0x1e;
  } else {
    ascFile.addPos(pos);
    ascFile.addNote("Entries(GData)[_]:");
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
    if (input->readLong(2)!=0x36 || !m_mainParser->isFilePos(actPos-decal+4+0x36*N)) {
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
  bool hasIdUnknown=vers==2 && m_mainParser->getDocumentType()==GWParser::TEXT;
  int const headerSize=hasIdUnknown ? 22 : vers==2 ? 12 : 16;
  int const nZones= vers==2 ? 3 : 4;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  long endPos=pos+headerSize+4*nZones;
  if (!m_mainParser->isFilePos(endPos))
    return false;
  long sz=-1;
  input->seek(pos, WPX_SEEK_SET);
  if (hasIdUnknown) {
    input->seek(2, WPX_SEEK_CUR); // id
    sz=(long) input->readULong(4);
    endPos=input->tell()+sz;
  }
  long zoneSz[4]= {0,0,0,0};
  for (int i=0; i<nZones; ++i)
    zoneSz[i]=(long) input->readULong(4);
  if (hasIdUnknown &&
      (6+sz<headerSize+4*nZones || zoneSz[0]+zoneSz[1]+zoneSz[2]>sz ||
       !m_mainParser->isFilePos(endPos))) {
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

bool GWGraph::readPageFrames()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  int const vers=version();
  bool isDraw = m_mainParser->getDocumentType()==GWParser::DRAW;
  bool hasIdUnknown=vers==2 && !isDraw;
  int const nZones=hasIdUnknown ? 4 : vers==2 ? 3 : 4;
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
  if (hasIdUnknown) {
    int id=(int)input->readLong(2);
    f << "id=" << id << ",";
    endPos=pos+6+(long)input->readULong(4);
  }
  int val;
  static char const *(wh[])= {"head", "gdata", "root", "unknown"};
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
  f << "Entries(GData): N=" << nData << ",";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  int const gDataSize=vers==1 ? 0x34 : 0xaa;
  for (int i=0; i < nData; ++i) {
    pos = input->tell();
    f.str("");
    f << "GData-" << i << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+gDataSize, WPX_SEEK_SET);
  }
  pos=input->tell();
  if (pos!=zoneEnd) {
    ascFile.addPos(pos);
    ascFile.addNote("GData-end:###");
    input->seek(zoneEnd, WPX_SEEK_SET);
  }

  if (vers==1) {
    pos=input->tell();
    zoneEnd=pos+zoneSz[z++];
    nData=(int) input->readLong(2);
    input->seek(2, WPX_SEEK_CUR);
    f.str("");
    f << "Entries(GDatB): N=" << nData << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    int const gDatBSize= 0x1e;
    for (int i=0; i < nData; ++i) {
      pos = input->tell();
      f.str("");
      f << "GDatB-" << i << ",";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      input->seek(pos+gDatBSize, WPX_SEEK_SET);
    }
    pos=input->tell();
    if (pos!=zoneEnd) {
      ascFile.addPos(pos);
      ascFile.addNote("GDatB-end:###");
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
    pos=input->tell();
    if (input->atEOS()||pos==endPos)
      break;
    if (id<0|| id>=nFrames) {
      MWAW_DEBUG_MSG(("GWGraph::readPageFrames: can not find frame with id=%d\n",id));
      continue;
    }
    bool ok=true;
    shared_ptr<GWGraphInternal::Frame> zone=frames[size_t(id)];
    if (!zone) continue;
    m_state->m_frameList.push_back(zone);
    f.str("");
    f << "GFrame-data:F" << id+1 << "[" << *zone << "]:";
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
      if (!m_mainParser->isFilePos(pos+text.m_dataSize)) {
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
    case 7: // regular poly followed by some flags ?
    case 12: { // spline
      int nPt=(int) input->readLong(2);
      if (zone->m_type==12) nPt+=3;
      long endData=pos+10+8*nPt;
      if (pos+zone->m_dataSize > endData)
        endData=pos+zone->m_dataSize;
      if (nPt<0 || (endPos>0 && endData>endPos) ||
          (endPos<0 && !m_mainParser->isFilePos(endData))) {
        ok=false;
        break;
      }
      float dim[4];
      for (int j=0; j<4; ++j)
        dim[j]=float(input->readLong(4))/65536.f;
      f << "dim=" << dim[1] << "x" << dim[0] << "<->"
        << dim[3] << "x" << dim[2] << ",";
      f << "pt=[";
      float pt[2];
      for (int p=0; p<nPt; ++p) {
        pt[0]=float(input->readLong(4))/65536.f;
        pt[1]=float(input->readLong(4))/65536.f;
        f << pt[1] << "x" << pt[0] << ",";
      }
      f << "],";
      if (input->tell()!=endData) {
        ascFile.addDelimiter(input->tell(),'|');
        input->seek(endData,WPX_SEEK_SET);
      }
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
      if (!m_mainParser->isFilePos(pos+pict.m_dataSize)) {
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
          (endPos<0 && !m_mainParser->isFilePos(pos+4+2*nGrp))) {
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
      if (input->tell()>pos+zone->m_dataSize || !m_mainParser->isFilePos(pos+zone->m_dataSize)) {
        MWAW_DEBUG_MSG(("GWGraph::readPageFrames: must stop, file position seems bad\n"));
        ascFile.addPos(pos);
        ascFile.addNote("GFrame###");
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

    if (ok) continue;
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
  return true;
}

shared_ptr<GWGraphInternal::Frame> GWGraph::readFrameHeader()
{
  GWGraphInternal::Frame zone;
  shared_ptr<GWGraphInternal::Frame> res;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  long endPos=pos+54;
  if (!m_mainParser->isFilePos(endPos))
    return res;

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  zone.m_type=(int) input->readLong(1);
  if (zone.m_type<0||zone.m_type>16||input->readLong(1)) {
    input->seek(pos, WPX_SEEK_SET);
    return res;
  }
  float dim[4];
  for (int i=0; i<4; ++i)
    dim[i]=float(input->readLong(4))/65536.f;
  if (dim[2]<dim[0] || dim[3]<dim[1]) {
    input->seek(pos, WPX_SEEK_SET);
    return res;
  }
  zone.m_box=Box2f(Vec2f(dim[1],dim[0]),Vec2f(dim[3],dim[2]));
  zone.m_layout=(int) input->readULong(2);
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
  case 7:
  case 8:
  case 12:
    zone.m_dataSize=(long) input->readULong(4);
    break;
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
bool GWGraph::sendFrame(shared_ptr<GWGraphInternal::Frame> frame, int order)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener || !frame) {
    MWAW_DEBUG_MSG(("GWGraph::sendFrame: can not find a listener\n"));
    return false;
  }
  frame->m_parsed=true;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  Vec2f pageLT=72.f*m_mainParser->getPageLeftTop();
  MWAWPosition fPos(frame->m_box[0]+pageLT,frame->m_box.size(),WPX_POINT);
  fPos.setRelativePosition(MWAWPosition::Page);
  fPos.setPage(frame->m_page<0 ? 1: frame->m_page);
  if (order>=0)
    fPos.setOrder(order);
  fPos.m_wrapping = MWAWPosition::WBackground;
  bool ok=true;
  switch (frame->getType()) {
  case GWGraphInternal::Frame::T_BASIC: // DOME
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

bool GWGraph::sendPageGraphics()
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("GWGraph::sendPageGraphics: can not find a listener\n"));
    return false;
  }
  int order=0;
  for (size_t f=0; f < m_state->m_frameList.size(); ++f) {
    if (!m_state->m_frameList[f] || m_state->m_frameList[f]->m_parsed)
      continue;
    sendFrame(m_state->m_frameList[f],++order);
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
  for (size_t f=0; f < m_state->m_frameList.size(); ++f) {
    if (!m_state->m_frameList[f] || m_state->m_frameList[f]->m_parsed)
      continue;
    sendFrame(m_state->m_frameList[f],-1);
  }
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
