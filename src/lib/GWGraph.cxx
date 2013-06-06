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
  //! constructor
  Frame() : m_type(-1), m_layout(-1), m_parent(0), m_order(-1), m_numChild(0), m_dataSize(0), m_box(), m_extra("") {
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
    if (zone.m_numChild > 0)
      o << "numChild=" << zone.m_numChild << ",";
    if (zone.m_dataSize > 0)
      o << "dataSize=" << zone.m_dataSize << ",";
    o << "box=" << zone.m_box << ",";
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
  //! the number of child ( for group)
  int m_numChild;
  //! the data size ( if know)
  long m_dataSize;
  //! the zone bdbox
  Box2f m_box;
  //! extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the state of a GWGraph
struct State {
  //! constructor
  State() : m_numPages(0) { }
  int m_numPages /* the number of pages */;
};


////////////////////////////////////////
//! Internal: the subdocument of a GWGraph
class SubDocument : public MWAWSubDocument
{
public:
  //! constructor
  SubDocument(GWGraph &pars, MWAWInputStreamPtr input, int id) :
    MWAWSubDocument(pars.m_mainParser, input, MWAWEntry()), m_graphParser(&pars), m_id(id) {}


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
  //! the zone id
  int m_id;

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
  m_input->seek(pos, WPX_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_graphParser != sDoc->m_graphParser) return true;
  if (m_id != sDoc->m_id) return true;
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
// graphic zone
////////////////////////////////////////////////////////////
bool GWGraph::readGraphicZone()
{
  int const vers=version();
  bool isDraw=m_mainParser->getDocumentType()==GWParser::DRAW;
  if (vers == 1 && !isDraw)
    return false;
  bool hasIdOrder=vers==2 && !isDraw;
  int const gZoneSize=vers==2 ? 0x1c : 0x1c+0x38;
  int const headerSize=hasIdOrder ? 22 : vers==2 ? 12 : 16;
  int const gDataSize=vers==2 ? 0xaa : 0x1e;
  int const gDatBSize=vers==2 ? 0x30 : 0x1a;
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos = input->tell();
  long beginPos=input->tell();
  GWGraphInternal::Frame zone;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  while(!input->atEOS()) {
    pos = input->tell();
    unsigned long value= input->readULong(4);
    int decal=-1;
    // first tabs
    if (value==0x20FFFF)
      decal = 0;
    else if (value==0x20FFFFFF)
      decal = 1;
    else if (value==0xFFFFFFFF)
      decal = 2;
    else if (value==0xFFFFFF2E)
      decal = 3;
    if (decal>=0) {
      input->seek(pos-decal, WPX_SEEK_SET);
      if (input->readULong(4)==0x20FFFF && input->readULong(4)==0xFFFF2E00)
        break;
      input->seek(pos+4, WPX_SEEK_SET);
      continue;
    }
    if (pos<beginPos+gZoneSize+gDataSize+gDatBSize+headerSize)
      continue;
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

    input->seek(pos-decal, WPX_SEEK_SET);
    int N=(int) input->readULong(2);
    if (input->readLong(2)!=0x36 || !m_mainParser->isFilePos(pos-decal+4+0x36*N)) {
      input->seek(pos+4, WPX_SEEK_SET);
      continue;
    }
    input->seek(pos-decal-headerSize, WPX_SEEK_SET);
    if (!isPageFrames()) {
      input->seek(pos+4, WPX_SEEK_SET);
      continue;
    }

    pos = pos-decal-headerSize-gZoneSize-gDataSize-gDatBSize;
    if (pos!=beginPos) {
      libmwaw::DebugStream f;
      f << "Entries(Unknown):";
      ascFile.addPos(beginPos);
      ascFile.addNote(f.str().c_str());
    }
    if (gZoneSize) {
      ascFile.addPos(pos);
      ascFile.addNote("Entries(GZoneHeader)");
      pos += gZoneSize;
    }

    ascFile.addPos(pos);
    ascFile.addNote(vers==1 ? "GDatC:" : "GData:");
    pos+=gDataSize;
    ascFile.addPos(pos);
    ascFile.addNote("GDatB:");
    pos+=gDatBSize;
    input->seek(pos, WPX_SEEK_SET);
    while(readPageFrames())
      pos=input->tell();
    input->seek(pos, WPX_SEEK_SET);
    return true;
  }

  input->seek(beginPos, WPX_SEEK_SET);
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
  std::vector<GWGraphInternal::Frame> frames;
  for (int i=0; i < nFrames; ++i) {
    pos = input->tell();
    f.str("");
    f << "GFrame[head]-F" << i+1 << ":";
    GWGraphInternal::Frame zone;
    if (!readFrameHeader(zone)) {
      MWAW_DEBUG_MSG(("GWGraph::readPageFrames: oops graphic detection is probably bad\n"));
      f << "###";
      input->seek(pos+0x36, WPX_SEEK_SET);
      zone=GWGraphInternal::Frame();
    } else
      f << zone;
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
    f << "Entries(GDatC): N=" << nData << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    int const gDatCSize= 0x1e;
    for (int i=0; i < nData; ++i) {
      pos = input->tell();
      f.str("");
      f << "GDatC-" << i << ",";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      input->seek(pos+gDatCSize, WPX_SEEK_SET);
    }
    pos=input->tell();
    if (pos!=zoneEnd) {
      ascFile.addPos(pos);
      ascFile.addNote("GDatC-end:###");
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
      int ord=frames[size_t(id-1)].m_order;
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
      GWGraphInternal::Frame &frame=frames[size_t(i)];
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
    for (size_t i=0; i < size_t(nFrames); i++)
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
    GWGraphInternal::Frame &zone=frames[size_t(id)];
    f.str("");
    f << "GFrame-data:F" << id+1 << "[" << zone << "]:";
    switch(zone.m_type) {
    case 0:
      MWAW_DEBUG_MSG(("GWGraph::readPageFrames: find group with type=0\n"));
      break;
    case 1:
      // can a textbox zone be followed by a picture list ?
      if (m_mainParser->readTextZone())
        break;
      input->seek(pos, WPX_SEEK_SET);
      ok = m_mainParser->readSimpleTextZone();
      break;
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
      if (zone.m_type==12) nPt+=3;
      long endData=pos+10+8*nPt;
      if (pos+zone.m_dataSize > endData)
        endData=pos+zone.m_dataSize;
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
    case 11:
      ok=readPicture();
      break;
    case 15: {
      int nGrp=(int) input->readLong(2);
      if (nGrp<0 || (endPos>0 && pos+4+2*nGrp>endPos) ||
          (endPos<0 && !m_mainParser->isFilePos(pos+4+2*nGrp))) {
        ok=false;
        break;
      }
      val=(int) input->readLong(2); // always 2
      if (val!=2) f << "f0=" << val << ",";
      f << "grpId=[";
      for (int j=0; j < nGrp; ++j) {
        val = (int) input->readLong(2);
        rootList.push_back(j);
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
    if (zone.m_dataSize>0 && input->tell()!=pos+zone.m_dataSize) {
      if (input->tell()>pos+zone.m_dataSize || !m_mainParser->isFilePos(pos+zone.m_dataSize)) {
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
      input->seek(pos+zone.m_dataSize, WPX_SEEK_SET);
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

bool GWGraph::readFrameHeader(GWGraphInternal::Frame &zone)
{
  zone=GWGraphInternal::Frame();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  long endPos=pos+54;
  if (!m_mainParser->isFilePos(endPos))
    return false;

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  zone.m_type=(int) input->readLong(1);
  if (zone.m_type<0||zone.m_type>16||input->readLong(1)) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  float dim[4];
  for (int i=0; i<4; ++i)
    dim[i]=float(input->readLong(4))/65536.f;
  if (dim[2]<dim[0] || dim[3]<dim[1]) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  zone.m_box=Box2f(Vec2f(dim[1],dim[0]),Vec2f(dim[3],dim[2]));
  zone.m_layout=(int) input->readULong(2);
  zone.m_parent=(int) input->readULong(2);
  zone.m_order=(int) input->readULong(2);
  switch(zone.m_type) {
  case 0xF:
    zone.m_numChild=(int) input->readULong(2);
    break;
  case 1:
  case 7:
  case 8:
  case 11:
  case 12:
    zone.m_dataSize=(long) input->readULong(4);
    break;
  default:
    break;
  }
  zone.m_extra=f.str();
  ascFile.addDelimiter(input->tell(),'|');
  input->seek(endPos, WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// picture
////////////////////////////////////////////////////////////
bool GWGraph::readPictureList(int nPict)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  for (int p=0; p<nPict; ++p) {
    long pos = input->tell();
    if (!readPicture()) {
      MWAW_DEBUG_MSG(("GWGraph::readPictureList: can not find picture %d\n", p));
      input->seek(pos, WPX_SEEK_SET);
      break;
    }
  }
  return true;
}

bool GWGraph::readPicture()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  long pos = input->tell();
  long fSz=(long) input->readULong(2);
  long endPos = pos+fSz;
  if (fSz<2 || !m_mainParser->isFilePos(endPos)) {
    MWAW_DEBUG_MSG(("GWGraph::readPicture: can not find picture\n"));
    input->seek(pos,WPX_SEEK_SET);
    return false;
  }
  f.str("");
  f << "Entries(Pictures):";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
#ifdef DEBUG_WITH_FILES
  ascFile.skipZone(pos+2, endPos-1);
  WPXBinaryData file;
  input->seek(pos,WPX_SEEK_SET);
  input->readDataBlock(fSz, file);

  static int volatile pictName = 0;
  libmwaw::DebugStream f2;
  f2 << "DATA-" << ++pictName << ".pct";
  libmwaw::Debug::dumpFile(file, f2.str().c_str());
#endif

  input->seek(endPos, WPX_SEEK_SET);
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
bool GWGraph::sendPageGraphics()
{
  if (!m_parserState->m_listener)
    return true;
  return true;
}

void GWGraph::flushExtra()
{
  if (!m_parserState->m_listener)
    return;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
