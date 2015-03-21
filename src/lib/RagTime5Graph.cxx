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

#include <librevenge/librevenge.h>

#include "MWAWFont.hxx"
#include "MWAWGraphicShape.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWListener.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSubDocument.hxx"

#include "RagTime5Parser.hxx"
#include "RagTime5StructManager.hxx"
#include "RagTime5ZoneManager.hxx"

#include "RagTime5Graph.hxx"

#include "libmwaw_internal.hxx"

/** Internal: the structures of a RagTime5Graph */
namespace RagTime5GraphInternal
{
////////////////////////////////////////
//! Internal: the helper to read field for a RagTime5Graph
struct FieldParser : public RagTime5StructManager::FieldParser {
  //! enum used to define the zone type
  enum Type { Z_Styles, Z_Colors };

  //! constructor
  FieldParser(RagTime5Graph &parser, Type type) :
    RagTime5StructManager::FieldParser(type==Z_Styles ? "GraphStyle" : "GraphColor"), m_type(type), m_mainParser(parser)
  {
    m_regroupFields=(m_type==Z_Styles);
  }
  //! return the debug name corresponding to a field
  std::string getZoneName(int n) const
  {
    std::stringstream s;
    s << (m_type==Z_Styles ? "GraphStyle-GS" : "GraphColor-GC") << n;
    return s.str();
  }
  //! parse a field
  virtual bool parseField(RagTime5StructManager::Field &field, RagTime5Zone &zone, int /*n*/, libmwaw::DebugStream &f)
  {
    if (m_type==Z_Styles) {
      RagTime5StructManager::GraphicStyle style;
      MWAWInputStreamPtr input=zone.getInput();
      if (style.read(input, field))
        f << style;
      else
        f << "##" << field;
    }
    else
      f << field;
    return true;
  }

protected:
  //! the zone type
  Type m_type;
  //! the main parser
  RagTime5Graph &m_mainParser;
};

//! Internal: the shape of a RagTime5Graph
struct Shape {
  //! the different shape
  enum Type { S_Line, S_Rect, S_RectOval, S_Circle, S_Pie, S_Arc, S_Polygon, S_Spline, S_RegularPoly, S_TextBox, S_Group, S_Unknown };
};
////////////////////////////////////////
//! Internal: the state of a RagTime5Graph
struct State {
  //! enum used to defined list of classical pict
  enum PictureType { P_Pict, P_Tiff, P_Epsf, P_Jpeg, P_PNG, P_ScreenRep, P_WMF, P_Unknown };

  //! constructor
  State() : m_numPages(0), m_shapeTypeIdVector() { }
  //! the number of pages
  int m_numPages;
  //! try to return a set type
  Shape::Type getShapeType(int id) const;
  //! returns the picture type corresponding to a name
  static PictureType getPictureType(std::string const &type)
  {
    if (type=="TIFF") return P_Tiff;
    if (type=="PICT") return P_Pict;
    if (type=="PNG") return P_PNG;
    if (type=="JPEG") return P_Jpeg;
    if (type=="WMF") return P_WMF;
    if (type=="EPSF") return P_Epsf;
    if (type=="ScreenRep" || type=="Thumbnail") return P_ScreenRep;
    return P_Unknown;
  }

  //! the vector of shape type id
  std::vector<int> m_shapeTypeIdVector;
};

Shape::Type State::getShapeType(int id) const
{
  if (id<=0 || id>(int) m_shapeTypeIdVector.size()) {
    MWAW_DEBUG_MSG(("RagTime5GraphInternal::State::getShapeType: find some unknown id %d\n", id));
    return Shape::S_Unknown;
  }
  int type=m_shapeTypeIdVector[size_t(id-1)];
  switch (type) {
  case 0x14e8842:
    return Shape::S_Rect;
  case 0x14e9042:
    return Shape::S_Circle;
  case 0x14e9842:
    return Shape::S_RectOval;
  case 0x14ea042:
    return Shape::S_Arc;
  case 0x14ea842:
    return Shape::S_TextBox;
  case 0x14eb842:
    return Shape::S_Polygon;
  case 0x14ec842:
    return Shape::S_Line;
  case 0x14ed842:
    return Shape::S_Spline;
  case 0x14f0042:
    return Shape::S_Group;
  case 0x14f8842:
    return Shape::S_Pie;
  case 0x1bbc042:
    return Shape::S_RegularPoly;
  default:
    break;
  }
  MWAW_DEBUG_MSG(("RagTime5GraphInternal::::State::getShapeType: find some unknown type %x\n", (unsigned int) type));
  return Shape::S_Unknown;
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
RagTime5Graph::RagTime5Graph(RagTime5Parser &parser) :
  m_mainParser(parser), m_structManager(m_mainParser.getStructManager()), m_parserState(parser.getParserState()),
  m_state(new RagTime5GraphInternal::State)
{
}

RagTime5Graph::~RagTime5Graph()
{ }

int RagTime5Graph::version() const
{
  return m_parserState->m_version;
}

int RagTime5Graph::numPages() const
{
  // TODO IMPLEMENT ME
  MWAW_DEBUG_MSG(("RagTime5Graph::numPages: is not implemented\n"));
  return 0;
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// main graphic
////////////////////////////////////////////////////////////
bool RagTime5Graph::readGraphicTypes(RagTime5Zone &/*zone*/, RagTime5ZoneManager::Link const &link)
{
  if (link.m_ids.empty()) {
    MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicTypes: call with no zone\n"));
    return false;
  }
  shared_ptr<RagTime5Zone> dataZone=m_mainParser.getDataZone(link.m_ids[0]);
  // not frequent, but can happen...
  if (dataZone && !dataZone->m_entry.valid())
    return true;
  if (!dataZone || dataZone->getKindLastPart(dataZone->m_kinds[1].empty())!="ItemData") {
    MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicTypes: the first zone seems bad\n"));
    return false;
  }
  long length=dataZone->m_entry.length();
  std::vector<long> const &positions=link.m_longList;
  if (!length) {
    if (positions.empty()) return true;
    MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicTypes: can not find the type positions for zone %d\n", link.m_ids[0]));
    return false;
  }

  MWAWInputStreamPtr input=dataZone->getInput();
  input->setReadInverted(!dataZone->m_hiLoEndian);
  dataZone->m_isParsed=true;
  libmwaw::DebugFile &ascFile=dataZone->ascii();
  libmwaw::DebugStream f;
  f << "Entries(GraphType)[" << *dataZone << "]:";
  input->seek(dataZone->m_entry.begin(), librevenge::RVNG_SEEK_SET);
  ascFile.addPos(dataZone->m_entry.end());
  ascFile.addNote("_");
  if (positions.size()<=1) {
    f << "###";
    ascFile.addPos(dataZone->m_entry.begin());
    ascFile.addNote(f.str().c_str());
    input->setReadInverted(false);
    return false;
  }
  ascFile.addPos(dataZone->m_entry.begin());
  ascFile.addNote(f.str().c_str());
  m_state->m_shapeTypeIdVector.resize(size_t((int) positions.size()-1),0);
  for (size_t i=0; i+1<positions.size(); ++i) {
    int dLength=int(positions[i+1]-positions[i]);
    if (!dLength) continue;
    long pos=dataZone->m_entry.begin()+positions[i];
    f.str("");
    f  << "GraphType-" << i << ":";
    if (positions[i+1]>length || dLength<16) {
      MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicTypes: something look bad for positions %d\n", (int) i));
      f << "###";
      if (positions[i]<length) {
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
      }
      continue;
    }
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    int type=(int) input->readULong(4);
    m_state->m_shapeTypeIdVector[i]=type;
    f << "type=" << std::hex << type << std::dec << ",";
    for (int j=0; j<4; ++j) { // always 0
      int val=(int) input->readLong(2);
      if (val)  f << "f" << j << "=" << val << ",";
    }
    int N=(int) input->readULong(4);
    if (dLength!=N*4+16) {
      MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicTypes: the number of data seems bad\n"));
      f << "##N=" << N << ",";
      N=0;
    }
    if (N) {
      f << "unkn=[" << std::hex;
      for (int j=0; j<N; ++j)
        f << input->readULong(4) << ",";
      f << std::dec << "],";
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->setReadInverted(false);
  return true;
}

////////////////////////////////////////////////////////////
// colors
////////////////////////////////////////////////////////////
bool RagTime5Graph::readGraphicColors(RagTime5ZoneManager::Cluster &cluster)
{
  RagTime5GraphInternal::FieldParser fieldParser(*this, RagTime5GraphInternal::FieldParser::Z_Colors);
  return m_mainParser.readStructZone(cluster, fieldParser);
}

bool RagTime5Graph::readColorPatternZone(RagTime5ZoneManager::Cluster &cluster)
{
  // normally empty, but ...
  std::map<int, librevenge::RVNGString> idToNameMap;
  if (!cluster.m_nameLink.empty()) {
    m_mainParser.readUnicodeStringList(cluster.m_nameLink, idToNameMap);
    cluster.m_nameLink=RagTime5ZoneManager::Link();
  }

  RagTime5ZoneManager::Link const &link=cluster.m_dataLink;
  if (link.m_ids.empty()) {
    MWAW_DEBUG_MSG(("RagTime5Graph::readColorPatternZone: can not find any zone\n"));
    return false;
  }
  for (size_t i=0; i<2; ++i) {
    if (i>=link.m_ids.size()) break;
    int const dataId=link.m_ids[i];
    if (dataId==0) continue;

    std::string what(i==0 ? "GraphCPCol" : "GraphCPPat");
    shared_ptr<RagTime5Zone> dataZone=m_mainParser.getDataZone(dataId);
    if (!dataZone || !dataZone->m_entry.valid() ||
        dataZone->getKindLastPart(dataZone->m_kinds[1].empty())!="ItemData") {
      MWAW_DEBUG_MSG(("RagTime5Graph::readColorPatternZone: the data zone %s seems bad\n", what.c_str()));
      continue;
    }

    dataZone->m_isParsed=true;
    MWAWEntry entry=dataZone->m_entry;
    libmwaw::DebugFile &ascFile=dataZone->ascii();
    libmwaw::DebugStream f;
    f << "Entries(" << what << ")[" << *dataZone << "]:";
    ascFile.addPos(entry.end());
    ascFile.addNote("_");
    int const expectedSz=i==0 ? 10 : 8;
    if ((entry.length()%expectedSz)!=0) {
      MWAW_DEBUG_MSG(("RagTime5Graph::readColorPatternZone: the zone %s size seems bad\n", what.c_str()));
      f << "###";
      ascFile.addPos(entry.begin());
      ascFile.addNote(f.str().c_str());
      continue;
    }

    ascFile.addPos(entry.begin());
    ascFile.addNote(f.str().c_str());

    MWAWInputStreamPtr input=dataZone->getInput();

    int N=int(entry.length()/expectedSz);
    input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
    for (int j=0; j<N; ++j) {
      long pos=input->tell();
      f.str("");
      f << what << "-" << j+1 << ":";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      input->seek(pos+expectedSz, librevenge::RVNG_SEEK_SET);
    }
    input->setReadInverted(false);
  }

  return true;
}

////////////////////////////////////////////////////////////
// style
////////////////////////////////////////////////////////////
bool RagTime5Graph::readGraphicStyles(RagTime5ZoneManager::Cluster &cluster)
{
  RagTime5GraphInternal::FieldParser fieldParser(*this, RagTime5GraphInternal::FieldParser::Z_Styles);
  return m_mainParser.readStructZone(cluster, fieldParser);
}

////////////////////////////////////////////////////////////
// shape
////////////////////////////////////////////////////////////
bool RagTime5Graph::readGraphicZone(RagTime5ZoneManager::Cluster &cluster)
{
  RagTime5ZoneManager::Link const &link= cluster.m_dataLink;
  if (link.m_ids.size()<3 || !link.m_ids[1])
    return false;
  if (link.m_ids[2] && !readGraphicUnknown(link.m_ids[2])) {
    MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicZone: the zone id=%d seems bad\n", link.m_ids[2]));
  }
  std::map<int, librevenge::RVNGString> idToNameMap;
  if (!cluster.m_nameLink.empty()) {
    m_mainParser.readUnicodeStringList(cluster.m_nameLink, idToNameMap);
    cluster.m_nameLink=RagTime5ZoneManager::Link();
  }

  std::vector<long> decal;
  if (link.m_ids[0])
    m_mainParser.readPositions(link.m_ids[0], decal);
  if (decal.empty())
    decal=link.m_longList;
  int const dataId=link.m_ids[1];
  shared_ptr<RagTime5Zone> dataZone=m_mainParser.getDataZone(dataId);
  if (!dataZone || !dataZone->m_entry.valid() ||
      dataZone->getKindLastPart(dataZone->m_kinds[1].empty())!="ItemData") {
    if (decal.size()==1) {
      // a graphic zone with 0 zone is ok...
      dataZone->m_isParsed=true;
      return true;
    }
    MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicZone: the data zone %d seems bad\n", dataId));
    return false;
  }
  dataZone->m_isParsed=true;
  MWAWEntry entry=dataZone->m_entry;
  libmwaw::DebugFile &ascFile=dataZone->ascii();
  libmwaw::DebugStream f;
  f << "Entries(GraphData)[" << *dataZone << "]:";
  ascFile.addPos(entry.end());
  ascFile.addNote("_");
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());

  int N=int(decal.size());
  MWAWInputStreamPtr input=dataZone->getInput();
  input->setReadInverted(!dataZone->m_hiLoEndian);
  long debPos=entry.begin();
  long endPos=entry.end();
  if (N==0) {
    MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicZone: can not find decal list for zone %d, let try to continue\n", dataId));
    input->seek(debPos, librevenge::RVNG_SEEK_SET);
    int n=0;
    while (input->tell()+8 < endPos) {
      long pos=input->tell();
      int id=++n;
      librevenge::RVNGString name("");
      if (idToNameMap.find(id)!=idToNameMap.end())
        name=idToNameMap.find(id)->second;
      if (!readGraphic(*dataZone, endPos, id, name)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        break;
      }
    }
    if (input->tell()!=endPos) {
      static bool first=true;
      if (first) {
        MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicZone: can not read some block\n"));
        first=false;
      }
      ascFile.addPos(debPos);
      ascFile.addNote("###");
    }
  }
  else {
    for (int i=0; i<N-1; ++i) {
      long pos=decal[size_t(i)];
      long nextPos=decal[size_t(i+1)];
      if (pos<0 || debPos+pos>endPos) {
        MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicZone: can not read the data zone %d-%d seems bad\n", dataId, i));
        continue;
      }
      input->seek(debPos+pos, librevenge::RVNG_SEEK_SET);
      librevenge::RVNGString name("");
      if (idToNameMap.find(i+1)!=idToNameMap.end())
        name=idToNameMap.find(i+1)->second;
      readGraphic(*dataZone, debPos+nextPos, i+1, name);
      if (input->tell()!=debPos+nextPos) {
        static bool first=true;
        if (first) {
          MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicZone: can not read some block\n"));
          first=false;
        }
        ascFile.addPos(debPos+pos);
        ascFile.addNote("###");
      }
    }
  }
  return true;
}

bool RagTime5Graph::readGraphic(RagTime5Zone &zone, long endPos, int n, librevenge::RVNGString const &dataName)
{
  MWAWInputStreamPtr input=zone.getInput();
  long pos=input->tell();
  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  f << "GraphData-" << n << ":";
  if (!dataName.empty())
    f << "\"" << dataName.cstr() << "\",";
  if (pos+42>endPos) {
    MWAW_DEBUG_MSG(("RagTime5Graph::readGraphic: a graphic seems bad\n"));
    f<<"###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  unsigned int fl=(unsigned int) input->readULong(4);
  if (fl&1) f << "arrow[beg],";
  if (fl&2) f << "arrow[end],";
  if (fl&0x8)
    f << "hasTransf,";
  if (fl&0x40)
    f << "text[flowArround],";
  if (fl&0x200) f << "fixed,";
  if (fl&0x400) f << "hasName,";
  if (fl&0x800) f << "hasDist[bordTB],";
  if (fl&0x1000) f << "hasDist[flowTB],";
  if ((fl&0x4000)==0) f << "noPrint,";
  if (fl&0x8000) f << "hasDist[bordLR],";
  if (fl&0x10000) f << "hasDist[flowLR],";
  if (fl&0x40000) f << "protected,";
  if (fl&0x100000) f << "hasBorder,"; // checkme, maybe related to link data
  fl &= 0xFFEA21B4;
  if (fl) f << "fl1=" << std::hex << fl << std::dec << ",";
  int val;
  for (int i=0; i<7; ++i) { // f1[order?]=0..5c, f3=0..16, f5=0..45, f6=0..58
    val=(int) input->readLong(2);
    if (!val) continue;
    if (i==1)
      f << "order?=" << val << ",";
    else if (i==6)
      f << "linkTo=" << val << ",";
    else
      f << "f" << i << "=" << val << ",";
  }
  val=(int) input->readLong(2);
  RagTime5GraphInternal::Shape::Type type=m_state->getShapeType(val);
  int typeFieldSize=8;
  switch (type) {
  case RagTime5GraphInternal::Shape::S_Rect:
    f << "rect,";
    break;
  case RagTime5GraphInternal::Shape::S_RectOval:
    typeFieldSize+=8;
    f << "rectoval,";
    break;
  case RagTime5GraphInternal::Shape::S_Circle:
    f << "circle,";
    break;
  case RagTime5GraphInternal::Shape::S_Pie:
    typeFieldSize+=10;
    f << "pie,";
    break;
  case RagTime5GraphInternal::Shape::S_Arc:
    typeFieldSize+=10;
    f << "arc,";
    break;
  case RagTime5GraphInternal::Shape::S_Group:
    typeFieldSize=6;
    f << "group,";
    break;
  case RagTime5GraphInternal::Shape::S_Line:
    f << "line,";
    break;
  case RagTime5GraphInternal::Shape::S_Polygon:
    typeFieldSize=10;
    f << "poly,";
    break;
  case RagTime5GraphInternal::Shape::S_Spline:
    typeFieldSize=18;
    f << "spline,";
    break;
  case RagTime5GraphInternal::Shape::S_TextBox:
    typeFieldSize+=4;
    f << "textbox,";
    break;
  case RagTime5GraphInternal::Shape::S_RegularPoly:
    typeFieldSize=16;
    f << "poly[regular],";
    break;
  // also b and c
  case RagTime5GraphInternal::Shape::S_Unknown:
  default:
    if (val<=0 || val>(int) m_state->m_shapeTypeIdVector.size()) {
      f << "###type[id]=" << val << ",";
    }
    else
      f << "type=" << std::hex << m_state->m_shapeTypeIdVector[size_t(val-1)] << std::dec << ",";
  }
  val=(int) input->readLong(2);
  if (val)
    f << "trans[id]=GT" << val << ",";
  val=(int) input->readLong(2);
  if (val)
    f << "surf[id]=GS" << val << ",";
  float dim[4];
  for (int i=0; i<4; ++i) dim[i]=float(input->readLong(4))/65536.f;
  f << "dim=" << Box2f(Vec2f(dim[0],dim[1]), Vec2f(dim[2],dim[3])) << ",";
  long dataPos=input->tell();
  if (fl&0xFF) {
    val=(int) input->readLong(2);
    if (val)
      f << "border[id]=GS" << val << ",";
  }
  if (input->tell()+typeFieldSize>endPos) {
    MWAW_DEBUG_MSG(("RagTime5Graph::readGraphic: the data size seems too short\n"));
    f << "###sz,";
    input->seek(dataPos, librevenge::RVNG_SEEK_SET);
    ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  bool ok=true;
  if (type!=RagTime5GraphInternal::Shape::S_Polygon && type!=RagTime5GraphInternal::Shape::S_RegularPoly
      && type!=RagTime5GraphInternal::Shape::S_Spline && type!=RagTime5GraphInternal::Shape::S_Group) {
    for (int i=0; i<4; ++i) dim[i]=float(input->readLong(4))/65536.f;
    if ((dim[2]<=dim[0] || dim[3]<=dim[1]) && type != RagTime5GraphInternal::Shape::S_Line) {
      f << "###";
      ok=false;
    }
    f << "dim2=" << Box2f(Vec2f(dim[0],dim[1]), Vec2f(dim[2],dim[3])) << ",";
  }
  switch (type) {
  case RagTime5GraphInternal::Shape::S_Rect:
  case RagTime5GraphInternal::Shape::S_Circle:
  case RagTime5GraphInternal::Shape::S_Line:
    break;
  case RagTime5GraphInternal::Shape::S_RectOval:
    f << "round=" << float(input->readLong(4))/65536.f << "x" << float(input->readLong(4))/65536.f << ",";
    break;
  case RagTime5GraphInternal::Shape::S_Arc:
  case RagTime5GraphInternal::Shape::S_Pie:
    f << "angle=" << 360.f *float(input->readLong(4))/65536.f << "x" << 360.f *float(input->readLong(4))/65536.f << ",";
    val=(int) input->readLong(2);
    if (val) f << "h1=" << val << ",";
    break;
  case RagTime5GraphInternal::Shape::S_TextBox:
    val=(int) input->readLong(2);
    if (val!=0x400) // always 400?
      f << "h1=" << val << ",";
    val=(int) input->readLong(2); // 1-3
    if (val) f << "h2=" << val << ",";
    break;
  case RagTime5GraphInternal::Shape::S_Polygon:
  case RagTime5GraphInternal::Shape::S_RegularPoly:
  case RagTime5GraphInternal::Shape::S_Spline: {
    long actPos=input->tell();
    bool isSpline=type==RagTime5GraphInternal::Shape::S_Spline;
    if (actPos+10+(isSpline ? 8 : 0)>endPos) {
      MWAW_DEBUG_MSG(("RagTime5Graph::readGraphic: can not read the polygon data\n"));
      break;
    }
    val=(int) input->readLong(2);
    if (val) f << "h1=" << val << ",";
    if (isSpline) {
      for (int i=0; i<4; ++i) dim[i]=float(input->readLong(4))/65536.f;
      if ((dim[2]<=dim[0] || dim[3]<=dim[1]) && type != RagTime5GraphInternal::Shape::S_Line) {
        f << "###";
        ok=false;
      }
      f << "dim2=" << Box2f(Vec2f(dim[0],dim[1]), Vec2f(dim[2],dim[3])) << ",";
    }
    for (int i=0; i<2; ++i) { // h2=0|1
      val=(int) input->readLong(2);
      if (val) f << "h" << i+2 << "=" << val << ",";
    }
    int N=(int) input->readULong(4);
    actPos=input->tell();
    if (actPos+N*8+(type==RagTime5GraphInternal::Shape::S_RegularPoly ? 6 : 0)>endPos) {
      MWAW_DEBUG_MSG(("RagTime5Graph::readGraphic: can not read the polygon number of points\n"));
      f << "#N=" << N << ",";
      ok=false;
      break;
    }
    f << "pts=[";
    for (int i=0; i<N; ++i)
      f << float(input->readLong(4))/65536.f << "x" << float(input->readLong(4))/65536.f << ",";
    f << "],";
    if (type!=RagTime5GraphInternal::Shape::S_RegularPoly)
      break;
    // the number of points with define a regular polygon
    f << "N=" << input->readLong(2) << ",";
    val=(int) input->readLong(4);
    if (val) f << "rot=" << 360.*float(val)/65536.f << ",";
    break;
  }
  case RagTime5GraphInternal::Shape::S_Group: {
    for (int i=0; i<2; ++i) { // always 0
      val=(int) input->readLong(2);
      if (val) f << "h" << i+1 << "=" << val << ",";
    }
    int N=(int) input->readULong(2);
    long actPos=input->tell();
    if (actPos+N*4>endPos) {
      MWAW_DEBUG_MSG(("RagTime5Graph::readGraphic: can not read the group number of points\n"));
      f << "#N=" << N << ",";
      ok=false;
      break;
    }
    f << "child=[";
    for (int i=0; i<N; ++i)
      f << input->readLong(4) << ",";
    f << "],";
    break;
  }
  case RagTime5GraphInternal::Shape::S_Unknown:
  default:
    ok=false;
    break;
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return ok;
}

bool RagTime5Graph::readGraphicUnknown(int typeId)
{
  if (!typeId)
    return false;

  shared_ptr<RagTime5Zone> zone=m_mainParser.getDataZone(typeId);
  if (!zone || !zone->m_entry.valid() || (zone->m_entry.length()%10) ||
      zone->getKindLastPart(zone->m_kinds[1].empty())!="ItemData") {
    MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicUnknown: the entry of zone %d seems bad\n", typeId));
    return false;
  }
  MWAWEntry entry=zone->m_entry;
  MWAWInputStreamPtr input=zone->getInput();
  input->setReadInverted(!zone->m_hiLoEndian);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);

  libmwaw::DebugFile &ascFile=zone->ascii();
  libmwaw::DebugStream f;
  zone->m_isParsed=true;
  ascFile.addPos(entry.end());
  ascFile.addNote("_");

  f << "Entries(GraphUnknown)[" << *zone << "]:";
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());

  int N=int(entry.length()/10);
  for (int i=0; i<N; ++i) {
    long pos=input->tell();
    f.str("");
    f << "GraphUnknown-" << i << ":";

    int type=(int) input->readLong(4);
    int id=(int) input->readLong(4);
    if (id==0) {
      ascFile.addPos(pos);
      ascFile.addNote("_");
      input->seek(pos+10, librevenge::RVNG_SEEK_SET);
      continue;
    }

    f << "id=" << id << ",unknown=" << type << ",";
    int fl=(int) input->readLong(2);
    if (fl) f << "fl=" << std::hex << fl << std::dec << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  input->setReadInverted(false);
  return true;
}

////////////////////////////////////////////////////////////
// transformation
////////////////////////////////////////////////////////////
bool RagTime5Graph::readGraphicTransformations(RagTime5Zone &/*zone*/, RagTime5ZoneManager::Link const &link)
{
  if (link.empty() || link.m_ids[0]==0 || link.m_fieldSize<34) {
    MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicTransformations: can not find the transformation id\n"));
    return false;
  }

  shared_ptr<RagTime5Zone> dataZone=m_mainParser.getDataZone(link.m_ids[0]);
  if (!dataZone || !dataZone->m_entry.valid() || dataZone->m_entry.length()!=link.m_N*link.m_fieldSize ||
      dataZone->getKindLastPart(dataZone->m_kinds[1].empty())!="ItemData") {
    if (link.m_N==0 && !dataZone->m_entry.valid()) {
      // an empty transformation zone is ok...
      dataZone->m_isParsed=true;
      return true;
    }

    MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicTransformations: the transformation zone %d seems bad\n", link.m_ids[0]));
    return false;
  }
  MWAWEntry entry=dataZone->m_entry;
  MWAWInputStreamPtr input=dataZone->getInput();
  input->setReadInverted(!dataZone->m_hiLoEndian);

  dataZone->m_isParsed=true;
  libmwaw::DebugFile &ascFile=dataZone->ascii();
  libmwaw::DebugStream f;
  f << "Entries(GraphTransform)[" << *dataZone << "]:";
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(entry.end());
  ascFile.addNote("_");

  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  for (int i=0; i<link.m_N; ++i) {
    long pos=input->tell();
    f.str("");
    f << "GraphTransform-GT" << i+1 << ":mat=[";
    for (int j=0; j<9; ++j) {
      if ((j%3)==0) f << "[";
      bool isShort=(j==8) && (link.m_fieldSize==34);
      long val=input->readLong(isShort ? 2 : 4);
      if (!val) f << "_";
      else if (isShort) f << val;
      else f << float(val)/65536.f;
      if ((j%3)==2) f << "]";
      f << ",";
    }
    f << "],";
    if (input->tell()!=pos+link.m_fieldSize)
      ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+link.m_fieldSize, librevenge::RVNG_SEEK_SET);
  }
  input->setReadInverted(false);
  return true;
}

////////////////////////////////////////////////////////////
// picture
////////////////////////////////////////////////////////////
bool RagTime5Graph::readPictureList(RagTime5Zone &zone)
{
  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  if (zone.m_name.empty())
    f << "Entries(PictureList)[" << zone << "]:";
  else
    f << "Entries(" << zone.m_name << ")[pictureList," << zone << "]:";
  MWAWEntry &entry=zone.m_entry;
  zone.m_isParsed=true;
  std::vector<int> listIds;
  if (entry.valid()) {
    ascFile.addPos(entry.end());
    ascFile.addNote("_");
    m_mainParser.ascii().addPos(zone.m_defPosition);
    m_mainParser.ascii().addNote("picture[list]");

    if (entry.length()%4) {
      MWAW_DEBUG_MSG(("RagTime5Graph::readPictureList: the entry size seems bad\n"));
      f << "###";
      ascFile.addPos(entry.begin());
      ascFile.addNote(f.str().c_str());
      return false;
    }

    MWAWInputStreamPtr input = zone.getInput();
    input->setReadInverted(!zone.m_hiLoEndian); // checkme never seens
    input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);

    int N=int(entry.length()/4);
    for (int i=0; i<N; ++i) {
      int val=(int) input->readLong(2); // always 1
      int id=(int) input->readLong(2);
      if (val==1) {
        f << "Data" << id << ",";
        listIds.push_back(id);
      }
      else if (val)
        f << "#" << i << ":" << val << ",";
    }
    ascFile.addPos(entry.begin());
    ascFile.addNote(f.str().c_str());
    input->setReadInverted(false);
  }
  else if (zone.m_variableD[0]==1)
    listIds.push_back(zone.m_variableD[1]);
  else {
    MWAW_DEBUG_MSG(("RagTime5Graph::readPictureList: can not find the list of pictures\n"));
    return false;
  }
  for (size_t i=0; i<listIds.size(); ++i) {
    shared_ptr<RagTime5Zone> dataZone=m_mainParser.getDataZone(listIds[i]);
    if (!dataZone || !dataZone->m_entry.valid() ||
        m_state->getPictureType(dataZone->getKindLastPart())==RagTime5GraphInternal::State::P_Unknown) {
      MWAW_DEBUG_MSG(("RagTime5Graph::readPictureList: can not find the picture %d\n", listIds[i]));
      continue;
    }
    readPicture(*dataZone);
  }
  return true;
}

bool RagTime5Graph::readPicture(RagTime5Zone &zone)
{
  MWAWEntry const &entry=zone.m_entry;
  if (entry.length()<=40)
    return false;
  RagTime5GraphInternal::State::PictureType type=m_state->getPictureType(zone.getKindLastPart());
  bool testForScreenRep=false;
  if (type==RagTime5GraphInternal::State::P_ScreenRep && !zone.m_kinds[1].empty()) {
    type=m_state->getPictureType(zone.getKindLastPart(false));
    if (type==RagTime5GraphInternal::State::P_Unknown)
      type=RagTime5GraphInternal::State::P_ScreenRep;
    else
      testForScreenRep=true;
  }
  if (type==RagTime5GraphInternal::State::P_Unknown)
    return false;
  MWAWInputStreamPtr input = zone.getInput();
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  long val;
  std::string extension("");
  bool ok=true;
  switch (type) {
  case RagTime5GraphInternal::State::P_Epsf:
    val=(long) input->readULong(4);
    if (val!=(long) 0xc5d0d3c6 && val != (long) 0x25215053) {
      ok=false;
      break;
    }
    extension="eps";
#if 0
    // when header==0xc5d0d3c6, we may want to decompose the data
    input->setReadInverted(true);
    MWAWEntry fEntry[3];
    for (int i=0; i<3; ++i) {
      fEntry[i].setBegin((long) input->readULong(4));
      fEntry[i].setLength((long) input->readULong(4));
      if (!fEntry[i].length()) continue;
      f << "decal" << i << "=" << std::dec << fEntry[i].begin() << "<->" << fEntry[i].end() << std::hex << ",";
      if (fEntry[i].begin()<0x1c ||fEntry[i].end()>zone.m_entry.length()) {
        MWAW_DEBUG_MSG(("RagTime5Graph::readPicture: the address %d seems too big\n", i));
        fEntry[i]=MWAWEntry();
        f << "###";
        continue;
      }
    }
    for (int i=0; i<2; ++i) { // always -1,0
      if (input->tell()>=pos+fDataPos)
        break;
      val=(int) input->readLong(2);
      if (val!=i-1)
        f << "f" << i << "=" << val << ",";
    }
    // now first fEntry=eps file, second WMF?, third=tiff file
#endif
    break;
  case RagTime5GraphInternal::State::P_Jpeg:
    val=(long) input->readULong(2);
    // jpeg format begin by 0xffd8 and jpeg-2000 format begin by 0000 000c 6a50...
    if (val!=0xffd8 && (val!=0 || input->readULong(4)!=0xc6a50 || input->readULong(4)!=0x20200d0a)) {
      ok=false;
      break;
    }
    extension="jpg";
    break;
  case RagTime5GraphInternal::State::P_Pict:
    input->seek(10, librevenge::RVNG_SEEK_CUR);
    val=(long) input->readULong(2);
    if (val!=0x1101 && val !=0x11) {
      ok=false;
      break;
    }
    extension="pct";
    break;
  case RagTime5GraphInternal::State::P_PNG:
    if (input->readULong(4) != 0x89504e47) {
      ok=false;
      break;
    }
    extension="png";
    break;
  case RagTime5GraphInternal::State::P_ScreenRep:
    val=(long) input->readULong(1);
    if (val!=0x49 && val!=0x4d) {
      ok=false;
      break;
    }
    MWAW_DEBUG_MSG(("RagTime5Graph::readPicture: find unknown picture format for zone %d\n", zone.m_ids[0]));
    extension="sRep";
    break;
  case RagTime5GraphInternal::State::P_Tiff:
    val=(long) input->readULong(2);
    if (val!=0x4949 && val != 0x4d4d) {
      ok=false;
      break;
    }
    val=(long) input->readULong(2);
    /* find also frequently 4d4d 00dd b300 d61e here ?
       and one time 4d 00 b3 2a d6 */
    if (val!=0x2a00 && val!=0x002a) {
      static bool first=true;
      if (first) {
        MWAW_DEBUG_MSG(("RagTime5Graph::readPicture: some tiffs seem bad, zone %d, ...\n", zone.m_ids[0]));
        first=false;
      }
      extension="check.tiff";
    }
    else
      extension="tiff";
    break;
  case RagTime5GraphInternal::State::P_WMF:
    if (input->readULong(4)!=0x01000900) {
      ok=false;
      break;
    }
    extension="wmf";
    break;
  case RagTime5GraphInternal::State::P_Unknown:
  default:
    ok=false;
    break;
  }
  if (!ok && testForScreenRep) {
    input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
    val=(long) input->readULong(1);
    if (val==0x49 || val==0x4d) {
      ok=true;
      MWAW_DEBUG_MSG(("RagTime5Graph::readPicture: find unknown picture format for zone %d\n", zone.m_ids[0]));
      extension="sRep";
      type=RagTime5GraphInternal::State::P_ScreenRep;
    }
  }
  zone.m_isParsed=true;
  libmwaw::DebugStream f;
  f << "picture[" << extension << "],";
  m_mainParser.ascii().addPos(zone.m_defPosition);
  m_mainParser.ascii().addNote(f.str().c_str());
  if (!ok) {
    f.str("");
    f << "Entries(BADPICT)[" << zone << "]:###";
    libmwaw::DebugFile &ascFile=zone.ascii();
    ascFile.addPos(zone.m_entry.begin());
    ascFile.addNote(f.str().c_str());
    return true;
  }
#ifdef DEBUG_WITH_FILES
  if (type==RagTime5GraphInternal::State::P_ScreenRep) {
    libmwaw::DebugFile &ascFile=zone.ascii();
    f.str("");
    f << "Entries(ScrRep)[" << zone << "]:";
    ascFile.addPos(zone.m_entry.begin());
    ascFile.addNote(f.str().c_str());
    return true;
  }
  if (zone.isMainInput())
    m_mainParser.ascii().skipZone(entry.begin(), entry.end()-1);
  librevenge::RVNGBinaryData file;
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  input->readDataBlock(entry.length(), file);
  static int volatile pictName = 0;
  f.str("");
  f << "Pict-" << ++pictName << "." << extension;
  libmwaw::Debug::dumpFile(file, f.str().c_str());
#endif
  return true;
}

bool RagTime5Graph::readPictureMatch(RagTime5Zone &zone, bool color)
{
  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  if (zone.m_name.empty())
    f << "Entries(" << (color ? "PictureColMatch" : "PictureMatch") << ")[" << zone << "]:";
  else
    f << "Entries(" << zone.m_name << "[" << (color ? "pictureColMatch" : "pictureMatch") << ")[" << zone << "]:";
  MWAWEntry &entry=zone.m_entry;
  zone.m_isParsed=true;
  ascFile.addPos(entry.end());
  ascFile.addNote("_");
  m_mainParser.ascii().addPos(zone.m_defPosition);
  m_mainParser.ascii().addNote(color ? "picture[matchCol]" : "picture[match]");

  int const expectedSz=color ? 42 : 32;
  if (entry.length() != expectedSz) {
    MWAW_DEBUG_MSG(("RagTime5Graph::readPictureMatch: the entry size seems bad\n"));
    f << "###";
    ascFile.addPos(entry.begin());
    ascFile.addNote(f.str().c_str());
    return false;
  }

  MWAWInputStreamPtr input = zone.getInput();
  input->setReadInverted(!zone.m_hiLoEndian); // checkme never seens
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);

  int val;
  for (int i=0; i<4; ++i) {
    static int const(expected[])= {0,0,0x7fffffff,0x7fffffff};
    val=(int) input->readLong(4);
    if (val!=expected[i])
      f << "f" << i << "=" << val << ",";
  }
  int dim[2];
  for (int i=0; i<2; ++i)
    dim[i]=(int) input->readLong(2);
  f << "dim=" << dim[0] << "x" << dim[1] << ",";
  for (int i=0; i<2; ++i) { // f2=0-3, f4=0-1
    val=(int) input->readLong(2);
    if (val)
      f << "f" << i+2 << "=" << val << ",";
  }
  // a very big number
  f << "ID?=" << std::hex << input->readULong(4) << std::dec << ",";
  for (int i=0; i<2; ++i) { // f5=f6=0
    val=(int) input->readLong(2);
    if (val)
      f << "f" << i+4 << "=" << val << ",";
  }
  if (color) {
    for (int i=0; i<5; ++i) { // g0=a|32, g1=0|1, other 0, color and pattern ?
      val=(int) input->readLong(2);
      if (val)
        f << "g" << i << "=" << val << ",";
    }
  }
  input->setReadInverted(false);
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());

  return true;
}



////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// interface send function
////////////////////////////////////////////////////////////

void RagTime5Graph::flushExtra()
{
  MWAW_DEBUG_MSG(("RagTime5Graph::flushExtra: is not implemented\n"));
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
