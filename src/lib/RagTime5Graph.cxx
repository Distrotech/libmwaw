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
#include "RagTime5ClusterManager.hxx"

#include "RagTime5Graph.hxx"

#include "libmwaw_internal.hxx"

/** Internal: the structures of a RagTime5Graph */
namespace RagTime5GraphInternal
{
//! Internal: the helper to read a clustList
struct ClustListParser : public RagTime5StructManager::DataParser {
  //! constructor
  ClustListParser(RagTime5ClusterManager &clusterManager, std::string const &zoneName) :
    RagTime5StructManager::DataParser(zoneName), m_clusterList(), m_clusterManager(clusterManager)
  {
  }

  std::string getClusterName(int id) const
  {
    return m_clusterManager.getClusterName(id);
  }

  //! try to parse a data
  bool parseData(MWAWInputStreamPtr &input, long endPos, RagTime5Zone &/*zone*/, int /*n*/, libmwaw::DebugStream &f)
  {
    long pos=input->tell();
    if (endPos-pos!=28) {
      MWAW_DEBUG_MSG(("RagTime5GraphInternal::ClustListParser::parse: bad data size\n"));
      return false;
    }
    std::vector<int> listIds;
    if (!RagTime5StructManager::readDataIdList(input, 1, listIds)) {
      MWAW_DEBUG_MSG(("RagTime5GraphInternal::ClustListParser::parse: can not read an cluster id\n"));
      f << "##clusterIds,";
      return false;
    }
    if (listIds[0]) {
      m_clusterList.push_back(listIds[0]);
      // a e,2003,200b, ... cluster
      f << getClusterName(listIds[0]) << ",";
    }
    unsigned long lVal=input->readULong(4); // c00..small number
    if ((lVal&0xc0000000)==0xc0000000)
      f << "f0=" << (lVal&0x3fffffff) << "*,";
    else if ((lVal&0xc0000000)==0xc0000000)
      f << "f0=" << (lVal&0x3fffffff) << "[" << (lVal>>30) << "],";
    else
      f << "f0" << lVal << ",";
    int val=(int) input->readLong(2); // 0|2
    if (val) f << "f1=" << val << ",";
    float dim[4];
    for (int i=0; i<4; ++i) dim[i]=float(input->readLong(4))/65536.f;
    // very often (0x0<->1x1), if not, we often have dim[0]+dim[2]~1 and dim[1]+dim[3]~1, some margins?
    f << "dim=" << Box2f(Vec2f(dim[0],dim[1]),Vec2f(dim[2],dim[3])) << ",";
    val=(int) input->readLong(2); // 0|1
    if (val) f << "f2=" << val << ",";
    return true;
  }

  //! the list of read cluster
  std::vector<int> m_clusterList;
  //! the name
  std::map<int, librevenge::RVNGString> m_idToNameMap;
private:
  //! the main zone manager
  RagTime5ClusterManager &m_clusterManager;
  //! copy constructor, not implemented
  ClustListParser(ClustListParser &orig);
  //! copy operator, not implemented
  ClustListParser &operator=(ClustListParser &orig);
};

//! Internal: the helper to read an integer list
struct IntListParser : public RagTime5StructManager::DataParser {
  //! constructor
  IntListParser(int fieldSz, std::string const &zoneName) : RagTime5StructManager::DataParser(zoneName), m_fieldSize(fieldSz), m_dataList()
  {
    if (m_fieldSize!=1 && m_fieldSize!=2 && m_fieldSize!=4) {
      MWAW_DEBUG_MSG(("RagTime5GraphInternal::IntListParser: bad field size\n"));
      m_fieldSize=0;
    }
  }
  //! try to parse a data
  bool parseData(MWAWInputStreamPtr &input, long endPos, RagTime5Zone &/*zone*/, int, libmwaw::DebugStream &f)
  {
    long pos=input->tell();
    if (m_fieldSize<=0 || (endPos-pos)%m_fieldSize) {
      MWAW_DEBUG_MSG(("RagTime5GraphInternal::IntListParser::parseData: bad data size\n"));
      return false;
    }
    int N=int((endPos-pos)/m_fieldSize);
    f << "data=[";
    for (int i=0; i<N; ++i) {
      int val=(int) input->readLong(m_fieldSize);
      f << val << ",";
      m_dataList.push_back(val);
    }
    f << "],";
    return true;
  }

  //! the field size
  int m_fieldSize;
  //! the list of read int
  std::vector<int> m_dataList;

  //! copy constructor, not implemented
  IntListParser(IntListParser &orig);
  //! copy operator, not implemented
  IntListParser &operator=(IntListParser &orig);
};

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
bool RagTime5Graph::readGraphicTypes(RagTime5ClusterManager::Link const &link)
{
  if (link.empty() || link.m_ids.size()<2) {
    MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicTypes: call with no zone\n"));
    return false;
  }
  shared_ptr<RagTime5Zone> dataZone=m_mainParser.getDataZone(link.m_ids[1]);
  // not frequent, but can happen...
  if (dataZone && !dataZone->m_entry.valid())
    return true;
  if (!dataZone || dataZone->getKindLastPart(dataZone->m_kinds[1].empty())!="ItemData") {
    MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicTypes: the first zone seems bad\n"));
    return false;
  }
  long length=dataZone->m_entry.length();
  std::vector<long> decal;
  if (link.m_ids[0])
    m_mainParser.readPositions(link.m_ids[0], decal);
  if (decal.empty())
    decal=link.m_longList;
  if (!length) {
    if (decal.empty()) return true;
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
  if (decal.size()<=1) {
    f << "###";
    ascFile.addPos(dataZone->m_entry.begin());
    ascFile.addNote(f.str().c_str());
    input->setReadInverted(false);
    return false;
  }
  ascFile.addPos(dataZone->m_entry.begin());
  ascFile.addNote(f.str().c_str());
  m_state->m_shapeTypeIdVector.resize(size_t((int) decal.size()-1),0);
  for (size_t i=0; i+1<decal.size(); ++i) {
    int dLength=int(decal[i+1]-decal[i]);
    if (!dLength) continue;
    long pos=dataZone->m_entry.begin()+decal[i];
    f.str("");
    f  << "GraphType-" << i << ":";
    if (decal[i+1]>length || dLength<16) {
      MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicTypes: something look bad for decal %d\n", (int) i));
      f << "###";
      if (decal[i]<length) {
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
bool RagTime5Graph::readGraphicColors(RagTime5ClusterManager::Cluster &cluster)
{
  RagTime5GraphInternal::FieldParser fieldParser(*this, RagTime5GraphInternal::FieldParser::Z_Colors);
  return m_mainParser.readStructZone(cluster, fieldParser, 14);
}

bool RagTime5Graph::readColorPatternZone(RagTime5ClusterManager::Cluster &cluster)
{
  for (size_t i=0; i<cluster.m_linksList.size(); ++i) {
    RagTime5ClusterManager::Link const &lnk=cluster.m_linksList[i];
    shared_ptr<RagTime5Zone> data=m_mainParser.getDataZone(lnk.m_ids[0]);
    if (!data->m_entry.valid()) {
      if (lnk.m_N*lnk.m_fieldSize) {
        MWAW_DEBUG_MSG(("RagTime5Graph::readColorPatternZone: can not find data zone %d\n", lnk.m_ids[0]));
      }
      continue;
    }
    long pos=data->m_entry.begin();
    data->m_isParsed=true;
    libmwaw::DebugFile &dAscFile=data->ascii();
    libmwaw::DebugStream f;
    std::string what("unkn");
    switch (lnk.m_fileType[1]) {
    case 0x40:
      what="col2";
      break;
    case 0x84040:
      what="color";
      break;
    case 0x16de842:
      what="pattern";
      break;
    default:
      MWAW_DEBUG_MSG(("RagTime5Graph::readColorPatternZone: find unexpected field\n"));
      break;
    }

    if (lnk.m_fieldSize<=0 || lnk.m_N*lnk.m_fieldSize!=data->m_entry.length()) {
      MWAW_DEBUG_MSG(("RagTime5Graph::readColorPatternZone: bad fieldSize/N for zone %d\n", lnk.m_ids[0]));
      f << "Entries(GraphCPData)[" << *data << "]:###" << what;
      dAscFile.addPos(pos);
      dAscFile.addNote(f.str().c_str());
      continue;
    }
    MWAWInputStreamPtr input=data->getInput();
    input->setReadInverted(!data->m_hiLoEndian);
    if (lnk.m_fieldSize!=8 && lnk.m_fieldSize!=10) {
      MWAW_DEBUG_MSG(("RagTime5Graph::readColorPatternZone: find some unknown field size for zone %d\n", lnk.m_ids[0]));
    }
    for (int j=0; j<lnk.m_N; ++j) {
      f.str("");
      if (j==0)
        f << "Entries(GraphCPData)[" << *data << "]:";
      else
        f << "GraphCPData-" << j+1 << ":";
      f << what << ",";
      if (lnk.m_fieldSize==10) {
        int val=(int) input->readLong(2);
        if (val!=1)
          f << "numUsed?=" << val << ",";
        unsigned char col[4];
        for (long k=0; k<4; ++k) // unsure if rgba, or ?
          col[k]=(unsigned char)(input->readULong(2)>>8);
        f << MWAWColor(col[0],col[1],col[2],col[3]);
      }
      else if (lnk.m_fieldSize==8) {
        MWAWGraphicStyle::Pattern pat;
        pat.m_colors[0]=MWAWColor::white();
        pat.m_colors[1]=MWAWColor::black();
        pat.m_dim=Vec2i(8,8);
        pat.m_data.resize(8);
        for (size_t k=0; k < 8; ++k)
          pat.m_data[k]=(unsigned char)input->readULong(1);
        f << pat;
      }
      else
        f << "###";
      dAscFile.addPos(pos);
      dAscFile.addNote(f.str().c_str());
      pos+=lnk.m_fieldSize;
    }
    input->setReadInverted(false);
  }

  return true;
}

////////////////////////////////////////////////////////////
// style
////////////////////////////////////////////////////////////
bool RagTime5Graph::readGraphicStyles(RagTime5ClusterManager::Cluster &cluster)
{
  RagTime5GraphInternal::FieldParser fieldParser(*this, RagTime5GraphInternal::FieldParser::Z_Styles);
  return m_mainParser.readStructZone(cluster, fieldParser, 14);
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
bool RagTime5Graph::readGraphicTransformations(RagTime5ClusterManager::Link const &link)
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

////////////////////////////////////////////////////////////
//
// read cluster data
//
////////////////////////////////////////////////////////////

namespace RagTime5GraphInternal
{
//! the picture cluster
struct ClusterPicture : public RagTime5ClusterManager::Cluster {
  //! constructor
  ClusterPicture() : RagTime5ClusterManager::Cluster(), m_auxilliarLink(), m_clusterLink()
  {
  }
  //! the first auxilliar data
  RagTime5ClusterManager::Link m_auxilliarLink;
  //! cluster links list of size 28
  RagTime5ClusterManager::Link m_clusterLink;
};

//
//! low level: parser of picture cluster
//
struct PictCParser : public RagTime5ClusterManager::ClusterParser {
  //! constructor
  PictCParser(RagTime5ClusterManager &parser, int type) :
    ClusterParser(parser, type, "ClustPict"), m_cluster(new ClusterPicture), m_what(-1), m_linkId(-1), m_fieldName("")
  {
  }
  //! return the current cluster
  shared_ptr<RagTime5ClusterManager::Cluster> getCluster()
  {
    return m_cluster;
  }
  //! return the current cluster
  shared_ptr<ClusterPicture> getPictureCluster()
  {
    return m_cluster;
  }
  //! end of a start zone call
  void endZone()
  {
    if (m_link.empty())
      return;
    switch (m_linkId) {
    case 0:
      m_cluster->m_auxilliarLink=m_link;
      break;
    case 1:
      m_cluster->m_clusterLink=m_link;
      break;
    default:
      if (m_what==0) {
        if (m_cluster->m_dataLink.empty())
          m_cluster->m_dataLink=m_link;
        else {
          MWAW_DEBUG_MSG(("RagTime5ClusterManagerInternal::PictCParser::endZone: oops the main link is already set\n"));
          m_cluster->m_linksList.push_back(m_link);
        }
      }
      else
        m_cluster->m_linksList.push_back(m_link);
      break;
    }
  }
  //! parse a zone
  bool parseZone(MWAWInputStreamPtr &input, long fSz, int N, int flag, libmwaw::DebugStream &f)
  {
    m_what=m_linkId=-1;
    m_fieldName="";
    if (N==-5)
      return parseHeaderZone(input,fSz,N,flag,f);
    if (N<0) {
      MWAW_DEBUG_MSG(("RagTime5ClusterManagerInternal::PictCParser::parseZone: expected N value\n"));
      f << "###N=" << N << ",";
      return true;
    }
    return parseDataZone(input, fSz, N, flag, f);
  }
  //! parse a field
  bool parseField(RagTime5StructManager::Field const &field, int /*m*/, libmwaw::DebugStream &f)
  {
    if (!m_fieldName.empty())
      f << m_fieldName << ",";
    switch (m_what) {
    case 0:
      if (field.m_type==RagTime5StructManager::Field::T_LongList && field.m_fileType==0x170c8e5) {
        f << "pos=[";
        for (size_t i=0; i<field.m_longList.size(); ++i)
          f << field.m_longList[i] << ",";
        f << "],";
        m_link.m_longList=field.m_longList;
        break;
      }
      MWAW_DEBUG_MSG(("RagTime5ClusterManagerInternal::PictCParser::parseField: find unexpected header field\n"));
      f << "###" << field << ",";
      break;
    case 1: // list link
      if (field.m_type==RagTime5StructManager::Field::T_LongList && field.m_fileType==0xce842) {
        f << "pos=[";
        for (size_t i=0; i<field.m_longList.size(); ++i)
          f << field.m_longList[i] << ",";
        f << "],";
        m_link.m_longList=field.m_longList;
        break;
      }
      if (field.m_type==RagTime5StructManager::Field::T_Unstructured && field.m_fileType==0xce017) {
        // a small value 2|4|a|1c|40
        f << "unkn="<<field.m_extra << ",";
        break;
      }
      MWAW_DEBUG_MSG(("RagTime5ClusterManagerInternal::PictCParser::parseField: find unexpected list link field\n"));
      f << "###" << field << ",";
      break;
    default:
      MWAW_DEBUG_MSG(("RagTime5ClusterManagerInternal::PictCParser::parseField: find unexpected field\n"));
      f << "###" << field << ",";
      break;
    }
    return true;
  }
protected:
  //! parse a data block, find fSz=36, 36|36|28|28|32
  bool parseDataZone(MWAWInputStreamPtr &input, long fSz, int N, int flag, libmwaw::DebugStream &f)
  {
    f << "fl=" << std::hex << flag << std::dec << ",";
    long pos=input->tell();
    m_link.m_N=N;
    switch (fSz) {
    case 28:
    case 32:
    case 36: {
      long linkValues[4];
      std::string mess;
      if (!readLinkHeader(input, fSz, m_link, linkValues, mess)) {
        if (fSz==36 && linkValues[0]==0x17d4842) {
          input->seek(pos, librevenge::RVNG_SEEK_SET);
          f << "type=17d4842,";
          int val=(int) input->readLong(4);
          if (val) f << "#f0=" << val << ",";
          input->seek(4, librevenge::RVNG_SEEK_CUR);
          for (int i=0; i<2; ++i) {
            val=(int) input->readLong(4);
            if (val) f << "f" << i+1 << "=" << val << ",";
          }
          val=(int) input->readULong(2);
          if ((val&0xFFD7)!=0x10) {
            MWAW_DEBUG_MSG(("RagTime5ClusterManagerInternal::PictCParser::parseZone: find unexpected type1[fSz36]\n"));
            f << "#fileType1=" << std::hex << val << std::dec << ",";
          }
          // increasing sequence
          for (int i=0; i<3; ++i) { // g0=4, g1=g0+1, g2=g1+1
            val=(int) input->readLong(4);
            if (val) f << "g" << i << "=" << val << ",";
          }
          break;
        }
        f << "###fType=" << std::hex << m_link.m_fileType[0] << std::dec << ",";
        MWAW_DEBUG_MSG(("RagTime5ClusterManagerInternal::PictCParser::parseZone: the field fSz28... type seems bad\n"));
        return true;
      }
      m_what=1;
      long expectedFileType1=0;
      if (m_link.m_fileType[0]==0x35800)
        m_fieldName="zone:longs";
      else if (m_link.m_fileType[0]==0x3e800)
        m_fieldName="list:longs0";
      else if (m_link.m_fileType[0]==(long) 0x80045080) {
        m_link.m_name="pictListInt";
        m_fieldName="listInt";
        m_linkId=0;
      }
      else if (fSz==36 && m_link.m_fileType[0]==0) {
        expectedFileType1=0x10;
        // field of sz=28, dataId + ?
        m_linkId=1;
        m_link.m_name="pict_ClustList";
        m_fieldName="clustList";
      }
      else {
        f << "###fType=" << std::hex << m_link.m_fileType[0] << std::dec << ",";
        MWAW_DEBUG_MSG(("RagTime5ClusterManagerInternal::PictCParser::parseZone: the field fSz28... type seems bad\n"));
        return true;
      }
      if (expectedFileType1>=0 && (m_link.m_fileType[1]&0xFFD7)!=expectedFileType1) {
        MWAW_DEBUG_MSG(("RagTime5ClusterManagerInternal::PictCParser::parseZone: fileType1 seems odd[fSz=28...]\n"));
        f << "###fileType1=" << std::hex << m_link.m_fileType[1] << std::dec << ",";
      }
      f << m_link << "," << mess;
      break;
    }
    default:
      MWAW_DEBUG_MSG(("RagTime5ClusterManagerInternal::PictCParser::parseZone: find unexpected fieldSze\n"));
      f << "##fSz=" << fSz << ",";
      break;
    }
    if (!m_fieldName.empty())
      f << m_fieldName << ",";
    return true;
  }
  //! parse the header zone
  bool parseHeaderZone(MWAWInputStreamPtr &input, long fSz, int N, int flag, libmwaw::DebugStream &f)
  {
    f << "header, fl=" << std::hex << flag << std::dec << ",";
    m_fieldName="header";
    m_what=0;
    if (N!=-5 || m_dataId!=0 || (fSz!=104 && fSz!=109)) {
      f << "###N=" << N << ",fSz=" << fSz << ",";
      MWAW_DEBUG_MSG(("RagTime5ClusterManagerInternal::PictCParser::parseHeaderZone: find unexpected main field\n"));
      return true;
    }
    int val;
    for (int i=0; i<2; ++i) { // always 0?
      val=(int) input->readLong(2);
      if (val) f << "f" << i+1 << "=" << val << ",";
    }
    val=(int) input->readLong(2);
    f << "id=" << val << ",";
    val=(int) input->readULong(2);
    if (m_type>0 && val!=m_type) {
      MWAW_DEBUG_MSG(("RagTime5ClusterManagerInternal::PictCParser::parseHeaderZone: unexpected zone type\n"));
      f << "##zoneType=" << std::hex << val << std::dec << ",";
    }
    for (int i=0; i<2; ++i) {// f0=0|2|3, f1=0|3
      val=(int) input->readLong(4);
      if (val) f << "f" << i << "=" << val << ",";
    }
    for (int i=0; i<5; ++i) {
      val=(int) input->readLong(2);
      static int const(expected[])= {2, 0, 0x2000, 0, 0x2710};
      if (val!=expected[i]) f << "f" << i+2 << "=" << val << ",";
    }
    val=(int) input->readLong(4);
    if (val!=0x3f7ff5) {
      MWAW_DEBUG_MSG(("RagTime5ClusterManagerInternal::PictCParser::parseHeaderZone: unexpected type [104|109]\n"));
      f << "#fieldType=" << std::hex << val << std::dec << ",";
    }
    for (int i=0; i<2; ++i) { // always 1,1 ?
      val=(int) input->readLong(1);
      if (val!=1) f << "fl" << i << "=" << val << ",";
    }
    float dim[4];
    for (int i=0; i<4; ++i) dim[i]=float(input->readLong(4))/65536.f;
    f << "dim=" << Vec2f(dim[0],dim[1]) << ",sz=" << Vec2f(dim[2],dim[3]) << ",";
    for (int i=0; i<5; ++i) { // fl2=708|718|f18|...|7d4b, fl3=0|4, f5=800|900|8000,fl6=0|1|a
      val=(int) input->readULong(2);
      if (val) f << "fl" << i+2 << "=" << std::hex << val << std::dec << ",";
    }
    for (int i=0; i<4; ++i) { // some selection ?
      val= (int) input->readLong(4);
      if ((i<2&&val)||(i>=2&&val!=0x7FFFFFFF))
        f << "g" << i << "=" << val << ",";
    }
    for (int i=0; i<6; ++i) { // h2=0|1|3|8
      val= (int) input->readLong(2);
      if (val) f << "h" << i << "=" << val << ",";
    }
    // find 5b84|171704|171804|172d84, so unsure
    m_link.m_fileType[0]=input->readLong(4);
    if (m_link.m_fileType[0])
      f << "fieldType1=" << std::hex << m_link.m_fileType[0] << std::dec << ",";
    std::vector<int> listIds;
    long actPos=input->tell();
    if (!RagTime5StructManager::readDataIdList(input, 1, listIds)) {
      MWAW_DEBUG_MSG(("RagTime5ClusterManagerInternal::PictCParser::parseHeaderZone: can not find the data[104|109]\n"));
      f << "##noData,";
      m_link.m_ids.clear();
      input->seek(actPos+2, librevenge::RVNG_SEEK_SET);
    }
    else if (listIds[0]) { // find always with a block of size 0: next file the picture ?...
      RagTime5ClusterManager::Link unknLink;
      unknLink=RagTime5ClusterManager::Link::L_UnknownItem;
      unknLink.m_name="UnknMain104-109";
      unknLink.m_ids.push_back(listIds[0]);
      f << unknLink << ",";
      m_cluster->m_linksList.push_back(unknLink);
    }
    for (int i=0; i<2; ++i) { // always 0
      val= (int) input->readLong(2);
      if (val) f << "h" << i+6 << "=" << val << ",";
    }
    if (fSz==109) {
      int dim2[2];
      for (int i=0; i<2; ++i) dim2[i]=(int) input->readLong(2);
      f << "dim2=" << Vec2i(dim2[0], dim2[1]) << ",";
      val= (int) input->readLong(1); // 0 or 1
      if (val) f << "h8=" << val << ",";
    }
    return true;
  }

  //! the current cluster
  shared_ptr<ClusterPicture> m_cluster;
  //! a index to know which field is parsed :  0: main, 1: list
  int m_what;
  //! the link id: 0: fieldSz=8 ?, data2: dataId+?
  int m_linkId;
  //! the actual field name
  std::string m_fieldName;
private:
  //! copy constructor (not implemented)
  PictCParser(PictCParser const &orig);
  //! copy operator (not implemented)
  PictCParser &operator=(PictCParser const &orig);
};

//! the shape cluster
struct ClusterGraphic : public RagTime5ClusterManager::Cluster {
  //! constructor
  ClusterGraphic() : RagTime5ClusterManager::Cluster()
  {
  }
  //! destructor
  virtual ~ClusterGraphic() {}
  //! two cluster links: list of pipeline: fixedSize=12, second list with field size 10)
  RagTime5ClusterManager::Link m_clusterLinks[2];
};

//
//! low level: parser of graph cluster
//
struct GraphicCParser : public RagTime5ClusterManager::ClusterParser {
  //! constructor
  GraphicCParser(RagTime5ClusterManager &parser, int type) :
    ClusterParser(parser, type, "ClustGraph"), m_cluster(new ClusterGraphic), m_what(-1), m_linkId(-1), m_fieldName("")
  {
  }
  //! return the current cluster
  shared_ptr<RagTime5ClusterManager::Cluster> getCluster()
  {
    return m_cluster;
  }
  //! return the current graphic cluster
  shared_ptr<ClusterGraphic> getGraphicCluster()
  {
    return m_cluster;
  }
  //! end of a start zone call
  void endZone()
  {
    if (m_link.empty())
      return;
    switch (m_linkId) {
    case 0:
      if (m_cluster->m_nameLink.empty())
        m_cluster->m_nameLink=m_link;
      else {
        MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::endZone: oops the name link is already set\n"));
        m_cluster->m_linksList.push_back(m_link);
      }
      break;
    case 1:
      m_cluster->m_conditionFormulaLinks.push_back(m_link);
      break;
    case 2:
    case 3:
      m_cluster->m_clusterLinks[m_linkId-2]=m_link;
      break;
    default:
      if (m_what==0) {
        if (m_cluster->m_dataLink.empty())
          m_cluster->m_dataLink=m_link;
        else {
          MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::endZone: oops the main link is already set\n"));
          m_cluster->m_linksList.push_back(m_link);
        }
      }
      else
        m_cluster->m_linksList.push_back(m_link);
      break;
    }
  }
  //! parse a zone
  bool parseZone(MWAWInputStreamPtr &input, long fSz, int N, int flag, libmwaw::DebugStream &f)
  {
    m_what=m_linkId=-1;
    m_fieldName="";
    if (N==-5)
      return parseHeaderZone(input,fSz,N,flag,f);
    if (N<0) {
      MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseZone: expected N value\n"));
      f << "###N=" << N << ",";
      return true;
    }
    return parseDataZone(input, fSz, N, flag, f);
  }
  //! parse a field
  bool parseField(RagTime5StructManager::Field const &field, int /*m*/, libmwaw::DebugStream &f)
  {
    if (!m_fieldName.empty())
      f << m_fieldName << ",";
    switch (m_what) {
    case 0: // graph data
      if (field.m_type==RagTime5StructManager::Field::T_Long && field.m_fileType==0x3c057) {
        // a small value 3|4
        f << "f0="<<field.m_longValue[0] << ",";
        break;
      }
      if (field.m_type==RagTime5StructManager::Field::T_FieldList && field.m_fileType==0x14e6825) {
        f << "decal=[";
        for (size_t i=0; i<field.m_fieldList.size(); ++i) {
          RagTime5StructManager::Field const &child=field.m_fieldList[i];
          if (child.m_type==RagTime5StructManager::Field::T_LongList && child.m_fileType==0xce842) {
            for (size_t j=0; j<child.m_longList.size(); ++j)
              f << child.m_longList[j] << ",";
            m_link.m_longList=child.m_longList;
            continue;
          }
          MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseField: find unexpected decal child[graph]\n"));
          f << "##[" << child << "],";
        }
        f << "],";
        break;
      }
      if (field.m_type==RagTime5StructManager::Field::T_FieldList && field.m_fileType==0x14e6875) {
        f << "listFlag?=[";
        for (size_t i=0; i<field.m_fieldList.size(); ++i) {
          RagTime5StructManager::Field const &child=field.m_fieldList[i];
          if (child.m_type==RagTime5StructManager::Field::T_Unstructured && child.m_fileType==0xce017)
            f << child.m_extra << ","; // find data with different length there
          else {
            MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseField: find unexpected unstructured child[graphZones]\n"));
            f << "##" << child << ",";
          }
        }
        f << "],";
        break;
      }
      MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseField: find unexpected child[graphZones]\n"));
      f << "##" << field << ",";
      break;
    case 1: // list link
      if (field.m_type==RagTime5StructManager::Field::T_LongList && field.m_fileType==0xce842) {
        f << "pos=[";
        for (size_t i=0; i<field.m_longList.size(); ++i)
          f << field.m_longList[i] << ",";
        f << "],";
        m_link.m_longList=field.m_longList;
        break;
      }
      if (field.m_type==RagTime5StructManager::Field::T_Unstructured && field.m_fileType==0xce017) {
        // a small value 2|4|a|1c|40
        f << "unkn="<<field.m_extra << ",";
        break;
      }
      // only with long2 list and with unk=[4]
      if (field.m_type==RagTime5StructManager::Field::T_LongList && field.m_fileType==0xcf042) {
        f << "unkn=[";
        for (size_t j=0; j<field.m_longList.size(); ++j) {
          if (field.m_longList[j]==0)
            f << "_,";
          else
            f << field.m_longList[j] << ",";
        }
        break;
      }
      MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseField: find unexpected list link field\n"));
      f << "###" << field;
      break;
    case 2: // cluster link, graph transform
      if (field.m_type==RagTime5StructManager::Field::T_Long && field.m_fileType==0xcf817) {
        // only in graph transform, small value between 3b|51|52|78
        f << "f0="<<field.m_longValue[0] << ",";
        break;
      }
      if (field.m_type==RagTime5StructManager::Field::T_Unstructured && field.m_fileType==0xce017) {
        // a small value 2|4|a|1c|40
        f << "unkn="<<field.m_extra << ",";
        break;
      }
      MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseField: find unexpected cluster link field\n"));
      f << "###" << field;
      break;
    case 3: // fSz=91
      if (field.m_type==RagTime5StructManager::Field::T_FieldList && field.m_fileType==0x14f1825) {
        f << "list=["; // find only list with one element: 1,8,11
        for (size_t i=0; i<field.m_fieldList.size(); ++i) {
          RagTime5StructManager::Field const &child=field.m_fieldList[i];
          if (child.m_type==RagTime5StructManager::Field::T_LongList && child.m_fileType==0xce842) {
            for (size_t j=0; j<child.m_longList.size(); ++j)
              f << child.m_longList[j] << ",";
            m_link.m_longList=child.m_longList;
            continue;
          }
          MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseField: find unexpected child[fSz=91]\n"));
          f << "##[" << child << "],";
        }
        f << "],";
        break;
      }
      MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseField: find unexpected cluster field[fSz=91]\n"));
      f << "###" << field;
      break;
    default:
      MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseField: find unexpected field\n"));
      f << "###" << field;
      break;
    }
    return true;
  }
protected:
  //! parse a data block
  bool parseDataZone(MWAWInputStreamPtr &input, long fSz, int N, int flag, libmwaw::DebugStream &f)
  {
    f << "fl=" << std::hex << flag << std::dec << ",";
    long pos=input->tell();
    long endPos=pos+fSz-6;
    int val;
    long linkValues[4];
    std::string mess("");
    m_link.m_N=N;
    switch (fSz) {
    case 28:
    case 29:
    case 30:
    case 32:
    case 34:
    case 36: {
      if (!readLinkHeader(input, fSz, m_link, linkValues, mess)) {
        if (fSz==36) {
          input->seek(pos, librevenge::RVNG_SEEK_SET);
          val=(int) input->readLong(4);
          long type=(int) input->readULong(4);
          if (type==0x7d01a||(type&0xFFFFFF8F)==0x14e818a) {
            m_what=2;
            f << "type=" << std::hex << type << std::dec << ",";
            if (val) f << "#f0=" << val << ",";
            for (int i=0; i<2; ++i) { // f1=0|7-11
              val=(int) input->readLong(4);
              if (val) f << "f" << i+1 << "=" << val << ",";
            }
            val=(int) input->readULong(2);
            if ((val&0xFFD7)!=0x10) {
              MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseDataZone: find unexpected type1[fSz36]\n"));
              f << "#fileType1=" << std::hex << val << std::dec << ",";
            }
            // increasing sequence
            for (int i=0; i<3; ++i) { // g0=3-a, g1=g0+1, g2=g1+1
              val=(int) input->readLong(4);
              if (val) f << "g" << i << "=" << val << ",";
            }
            break;
          }
        }
        MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseDataZone: find unexpected link\n"));
        f << "###link";
        return true;
      }
      long expectedFileType1=0;
      if (m_link.m_fileType[0]==0x34800) {
        m_what=1;
        if (linkValues[0]!=0x14ff840) {
          MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseDataZone: find unexpected linkValue[0]\n"));
          f << "#lValues0,";
        }
        // linkValues[2]=4|5|8
        m_fieldName="zone:longs1";
      }
      else if (m_link.m_fileType[0]==0x35800) {
        m_what=1;
        m_fieldName="zone:longs";
      }
      else if (m_link.m_fileType[0]==0x3e800) {
        m_what=1;
        m_fieldName="list:longs0";
      }
      else if (m_link.m_fileType[0]==0x3c052) {
        m_what=1;
        if (linkValues[0]!=0x1454877) {
          MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseDataZone: find unexpected linkValue[0]\n"));
          f << "#lValues0,";
        }
        // linkValues[2]=5|8|9
        m_fieldName="zone:longs2";
        expectedFileType1=0x50;
      }
      else if (m_link.m_fileType[0]==0x9f840) {
        if (m_link.m_fieldSize!=34 && m_link.m_fieldSize!=36) {
          MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseDataZone: find unexpected fieldSize[fSz28...]\n"));
          f << "###fielSize,";
        }
        expectedFileType1=0x10;
        if (linkValues[0]!=0x1500040) {
          MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseDataZone: find unexpected linkValues[fSz28...]\n"));
          f << "#linkValue0,";
        }
        // linkValues[2]=0|5
        m_link.m_type=RagTime5ClusterManager::Link::L_GraphicTransform;
        m_what=2;
        m_fieldName="graphTransform";
      }
      else if (m_link.m_fileType[0]==0x33000) {
        if (m_link.m_fieldSize!=4) {
          MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseDataZone: find unexpected fieldSize[fSz28...]\n"));
          f << "###fielSize,";
        }
        m_fieldName="listLn2";
      }
      else if (fSz==30 && m_link.m_fieldSize==12) {
        m_what=2;
        m_linkId=2;
        m_fieldName="clustLink";
        m_link.m_type=RagTime5ClusterManager::Link::L_ClusterLink;
        expectedFileType1=0xd0;
      }
      else if (fSz==30 && m_link.m_fileType[0]==0) {
        if (m_link.m_fieldSize!=8) {
          MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseDataZone: find unexpected fieldSize[fSz28...]\n"));
          f << "###fielSize,";
        }
        m_fieldName="listLn3";
      }
      else if (m_link.m_fileType[0]==0x14ff040) {
        if (linkValues[0]!=0x14ff040) {
          MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseDataZone: find unexpected linkValues[fSz28...]\n"));
          f << "#linkValue0,";
        }
        m_what=1;
        m_linkId=1;
        m_link.m_name=m_fieldName="condFormula";
        expectedFileType1=0x10;
      }
      else if (fSz==32 && (m_link.m_fileType[1]&0xFFD7)==0x200) {
        m_what=1;
        m_fieldName="unicodeList";
        m_linkId=0;
        m_link.m_type=RagTime5ClusterManager::Link::L_UnicodeList;
        expectedFileType1=0x200;
      }
      else if (fSz==36 && m_link.m_fileType[0]==0) {
        m_what=1;
        m_link.m_name=m_fieldName="clustLink2";
        m_linkId=3;
        expectedFileType1=0x10;
      }
      else {
        f << "###fType=" << std::hex << m_link.m_fileType[0] << std::dec << ",";
        MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseDataZone: the field fSz28 type seems bad\n"));
        return true;
      }
      if (expectedFileType1>=0 && (m_link.m_fileType[1]&0xFFD7)!=expectedFileType1) {
        MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseDataZone: fileType1 seems odd[fSz=28...]\n"));
        f << "###fileType1=" << std::hex << m_link.m_fileType[1] << std::dec << ",";
      }
      f << m_link << "," << mess;
      m_link.m_fileType[0]=0;
      int remain=int(endPos-input->tell());
      if (remain==0) break;
      if (remain==4) {
        for (int i=0; i<2; ++i) { // g0=3a-4f, g1=0|-1
          val=(int) input->readLong(2);
          if (val)
            f << "g" << i << "=" << val << ",";
        }
        break;
      }
      val=(int) input->readLong(1);
      if (val!=1) // always 1
        f << "g0=" << val << ",";
      if (remain<6) break;
      val=(int) input->readLong(1);
      if (val) // always 0
        f << "g1=" << val << ",";
      for (int i=0; i<2; ++i) { // g3=0|3c042
        val=(int) input->readLong(2);
        if (val)
          f << "g" << i+2 << "=" << val << ",";
      }
      break;
    }
    case 91:
      m_what=3;
      if (N) // find always 0
        f << "#N=" << N << ",";
      for (int i=0; i<2; ++i) { // always 0
        val=(int) input->readLong(2);
        if (val)
          f << "f" << i << "=" << val << ",";
      }
      val=(int) input->readLong(4); // always 1
      if (val!=1)
        f << "f2=" << val << ",";
      val=(int) input->readLong(2); // 0|4
      if (val) f << "f3=" << val << ",";
      val=(int) input->readULong(2); // ?
      if (val) f << "fl=" << std::hex << val << std::dec << ",";
      val=(int) input->readULong(4);
      if (val!=0x14e7842) {
        MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseDataZone: find unexpected file type\n"));
        f << "##filetype0=" << std::hex << val << std::dec << ",";
      }
      for (int i=0; i<4; ++i) { // f8=0|5|6|9
        val=(int) input->readLong(2);
        if (val)
          f << "f" << i+4 << "=" << val << ",";
      }
      for (int wh=0; wh<2; ++wh) { // checkme unsure about field separations
        f << "unkn" << wh << "=[";
        val=(int) input->readLong(1); // 0
        if (val) f << "g0=" << val << ",";
        for (int i=0; i<3; ++i) { // g3=0|1
          static int const(expected[])= {16, 0, 0};
          val=(int) input->readLong(2); // 16
          if (val!=expected[i])
            f << "g" << i+1 << "=" << val << ",";
        }
        val=(int) input->readLong(1); // 0
        if (val) f << "g4=" << val << ",";
        for (int i=0; i<7; ++i) { // g5=348,
          val=(int) input->readLong(2);
          if (val) f << "g" << i+5 << "=" << val << ",";
        }
        val=(int) input->readLong(1); // 0
        if (val) f << "h0=" << val << ",";
        for (int i=0; i<2; ++i) { // h1=21|29
          val=(int) input->readLong(2);
          if (val) f << "h" << i+1 << "=" << val << ",";
        }
        val=(int) input->readLong(1); // 0
        if (val) f << "h3=" << val << ",";
        f << "],";
      }
      for (int i=0; i<5; ++i) { // always 0
        val=(int) input->readLong(1);
        if (val) f << "g" << i << "=" << val << ",";
      }
      break;
    default:
      MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseDataZone: find unexpected field size\n"));
      f << "##fSz=" << fSz << ",";
      break;
    }
    if (!m_fieldName.empty())
      f << m_fieldName << ",";

    return true;
  }
  //! parse the header zone
  bool parseHeaderZone(MWAWInputStreamPtr &input, long fSz, int N, int flag, libmwaw::DebugStream &f)
  {
    f << "header, fl=" << std::hex << flag << std::dec << ",";
    m_fieldName="header";
    if (N!=-5 || m_dataId!=0 || fSz!=118) {
      f << "###N=" << N << ",fSz=" << fSz << ",";
      MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseHeaderZone: find unexpected main field\n"));
      return true;
    }
    m_what=0;

    int val;
    for (int i=0; i<2; ++i) { // always 0?
      val=(int) input->readLong(2);
      if (val) f << "f" << i+1 << "=" << val << ",";
    }
    val=(int) input->readLong(2);
    f << "id=" << val << ",";
    val=(int) input->readULong(2);
    if (m_type>0 && val!=m_type) {
      MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseHeaderZone: unexpected zone type\n"));
      f << "##zoneType=" << std::hex << val << std::dec << ",";
    }
    m_fieldName="graphZone";
    for (int i=0; i<2; ++i) { // f4=0|2|3
      val=(int) input->readLong(2);
      if (val) f << "f" << i+3 << "=" << val << ",";
    }
    val=(int) input->readLong(4); // 0|2|3|4
    if (val) f << "f5=" << val << ",";
    m_link.m_fileType[0]=(long) input->readULong(4); // find 0|80|81|880|8000|8080

    if ((m_link.m_fileType[0]&0x777E)!=0) {
      MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseHeaderZone: the file type0 seems bad[graph]\n"));
      f << "##fileType0=" << std::hex << m_link.m_fileType[0] << std::dec << ",";
    }
    else if (m_link.m_fileType[0])
      f << "fileType0=" << std::hex << m_link.m_fileType[0] << std::dec << ",";
    for (int wh=0; wh<2; ++wh) {
      f << "block" << wh << "[";
      val=(int) input->readLong(2); // 1 or 10
      if (val!=1) f << "g0=" << val << ",";
      for (int i=0; i<5; ++i) { // g1=numData+1? and g2 small number, other 0 ?
        val=(int) input->readLong(4);
        if (val) f << "g" << i+1 << "=" << val << ",";
      }
      if (wh==0) {
        m_link.m_fileType[1]=(long) input->readULong(2);
        if (m_link.m_fileType[1]!=0x8000 && m_link.m_fileType[1]!=0x8020) {
          MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseHeaderZone: the file type1 seems bad[graph]\n"));
          f << "##fileType1=" << std::hex << m_link.m_fileType[1] << std::dec << ",";
        }
        long actPos=input->tell();
        if (!RagTime5StructManager::readDataIdList(input, 2, m_link.m_ids) || m_link.m_ids[1]==0) {
          MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseHeaderZone: can not find the graph data\n"));
          f << "##noData,";
          m_link.m_ids.clear();
          m_link.m_ids.resize(2,0);
          input->seek(actPos+8, librevenge::RVNG_SEEK_SET);
        }
        val=(int) input->readLong(2); // always 0
        if (val) f << "g6=" << val << ",";
        val=(int) input->readLong(4); // 0|2
        if (val) f << "g7=" << val << ",";
        float dim[2];
        for (int i=0; i<2; ++i) dim[i]=float(input->readLong(4))/65536.f;
        f << "dim=" << Vec2f(dim[0], dim[1]) << ",";
        for (int i=0; i<4; ++i) { // always 0
          val=(int) input->readLong(2);
          if (val) f << "h" << i << "=" << val << ",";
        }
      }
      else {
        RagTime5ClusterManager::Link unknLink;
        unknLink.m_fileType[1]=(long) input->readULong(2);
        unknLink.m_fieldSize=(int) input->readULong(2);
        if ((unknLink.m_fileType[1]!=0x50 && unknLink.m_fileType[1]!=0x58) || unknLink.m_fieldSize!=10) {
          MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseHeaderZone: the file type2 seems bad[graph]\n"));
          f << "##fileType2=" << std::hex << unknLink.m_fileType[1] << std::dec << "[" << unknLink.m_fieldSize << "],";
        }
        // fixme store unknLink instead of updating the main link
        std::vector<int> listIds;
        if (RagTime5StructManager::readDataIdList(input, 3, listIds)) {
          m_link.m_ids.push_back(listIds[0]);
          for (size_t i=1; i<3; ++i) {
            if (!listIds[i]) continue;
            m_cluster->m_clusterIdsList.push_back(listIds[i]);
            f << "clusterId" << i << "=" << getClusterName(listIds[i]) << ",";
          }
        }
        else {
          MWAW_DEBUG_MSG(("RagTime5GraphInternal::GraphicCParser::parseHeaderZone: can not read unkn link list[graph]\n"));
          f << "##graph[unknown],";
        }
      }
      f << "],";
    }
    f << m_link << ",";
    if (!m_fieldName.empty())
      f << m_fieldName << ",";
    return true;
  }

  //! the current cluster
  shared_ptr<ClusterGraphic> m_cluster;
  //! a index to know which field is parsed :  0: graphdata, 1: list, 2: clustLink, graph transform, 3:fSz=91
  int m_what;
  //! the link id: 0: unicode, 1: condition, 2: clustLink, 3: clustLink[list]
  int m_linkId;
  //! the actual field name
  std::string m_fieldName;
private:
  //! copy constructor (not implemented)
  GraphicCParser(GraphicCParser const &orig);
  //! copy operator (not implemented)
  GraphicCParser &operator=(GraphicCParser const &orig);
};

}

////////////////////////////////////////////////////////////
// picture
////////////////////////////////////////////////////////////
bool RagTime5Graph::readPictureCluster(RagTime5Zone &zone, int zoneType)
{
  shared_ptr<RagTime5ClusterManager> clusterManager=m_mainParser.getClusterManager();
  if (!clusterManager) {
    MWAW_DEBUG_MSG(("RagTime5Graph::readPictureCluster: oops can not find the cluster manager\n"));
    return false;
  }
  RagTime5GraphInternal::PictCParser parser(*clusterManager, zoneType);
  if (!clusterManager->readCluster(zone, parser) || !parser.getPictureCluster()) {
    MWAW_DEBUG_MSG(("RagTime5Graph::readPictureCluster: oops can not find the cluster\n"));
    return false;
  }

  shared_ptr<RagTime5GraphInternal::ClusterPicture> cluster=parser.getPictureCluster();
  m_mainParser.checkClusterList(cluster->m_clusterIdsList);

  if (!cluster->m_auxilliarLink.empty()) { // list of increasing int sequence....
    RagTime5GraphInternal::IntListParser intParser(2, "PictListInt");
    m_mainParser.readListZone(cluster->m_auxilliarLink, intParser);
  }
  if (!cluster->m_clusterLink.empty()) {
    RagTime5GraphInternal::ClustListParser clustParser(*clusterManager, "Pict_ClustList");
    m_mainParser.readListZone(cluster->m_clusterLink, clustParser);
  }
  for (size_t i=0; i<cluster->m_linksList.size(); ++i) {
    RagTime5ClusterManager::Link const &lnk=cluster->m_linksList[i];
    if (lnk.m_type==RagTime5ClusterManager::Link::L_List) {
      m_mainParser.readListZone(lnk);
      continue;
    }
    std::stringstream s;
    s << "PictData" << lnk.m_fieldSize;
    RagTime5StructManager::DataParser defaultParser(s.str());
    m_mainParser.readFixedSizeZone(lnk, defaultParser);
  }

  return true;
}

////////////////////////////////////////////////////////////
// shape
////////////////////////////////////////////////////////////
bool RagTime5Graph::readGraphicCluster(RagTime5Zone &zone, int zoneType)
{
  shared_ptr<RagTime5ClusterManager> clusterManager=m_mainParser.getClusterManager();
  if (!clusterManager) {
    MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicCluster: oops can not find the cluster manager\n"));
    return false;
  }
  RagTime5GraphInternal::GraphicCParser parser(*clusterManager, zoneType);
  if (!clusterManager->readCluster(zone, parser) || !parser.getGraphicCluster()) {
    MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicCluster: oops can not find the cluster\n"));
    return false;
  }

  shared_ptr<RagTime5GraphInternal::ClusterGraphic> cluster=parser.getGraphicCluster();
  m_mainParser.checkClusterList(cluster->m_clusterIdsList);

  RagTime5ClusterManager::Link const &link= cluster->m_dataLink;
  if (link.m_ids.size()<2 || !link.m_ids[1]) {
    MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicCluster: can not find main data\n"));
    return true;
  }
  if (link.m_ids.size()>=3 && link.m_ids[2] && !readGraphicUnknown(link.m_ids[2])) {
    MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicCluster: the zone id=%d seems bad\n", link.m_ids[2]));
  }
  std::map<int, librevenge::RVNGString> idToNameMap;
  if (!cluster->m_nameLink.empty()) {
    m_mainParser.readUnicodeStringList(cluster->m_nameLink, idToNameMap);
    cluster->m_nameLink=RagTime5ClusterManager::Link();
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
    MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicCluster: the data zone %d seems bad\n", dataId));
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
    MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicCluster: can not find decal list for zone %d, let try to continue\n", dataId));
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
        MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicCluster: can not read some block\n"));
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
        MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicCluster: can not read the data zone %d-%d seems bad\n", dataId, i));
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
          MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicCluster: can not read some block\n"));
          first=false;
        }
        ascFile.addPos(debPos+pos);
        ascFile.addNote("###");
      }
    }
  }
  if (!cluster->m_clusterLinks[0].empty()) {
    // change me
    shared_ptr<RagTime5Zone> data=m_mainParser.getDataZone(cluster->m_clusterLinks[0].m_ids[0]);
    if (!data || data->m_isParsed) {
      MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicCluster: can not find data zone %d\n", cluster->m_clusterLinks[0].m_ids[0]));
    }
    else {
      data->m_hiLoEndian=cluster->m_hiLoEndian;
      m_mainParser.readClusterLinkList(*data, cluster->m_clusterLinks[0]);
    }
  }
  if (!cluster->m_clusterLinks[1].empty()) {
    std::vector<int> list;
    m_mainParser.readClusterLinkList(cluster->m_clusterLinks[1], RagTime5ClusterManager::Link(), list, "GraphClustLst");
  }
  // can have some condition formula
  for (int wh=0; wh<2; ++wh) {
    std::vector<RagTime5ClusterManager::Link> const &list=wh==0 ? cluster->m_conditionFormulaLinks : cluster->m_settingLinks;
    for (size_t i=0; i<list.size(); ++i) {
      if (list[i].empty()) continue;
      RagTime5ClusterManager::Cluster unknCluster;
      unknCluster.m_dataLink=list[i];
      RagTime5StructManager::FieldParser defaultParser(wh==0 ? "CondFormula" : "Settings");
      m_mainParser.readStructZone(unknCluster, defaultParser, 0);
    }
  }

  for (size_t i=0; i<cluster->m_linksList.size(); ++i) {
    RagTime5ClusterManager::Link const &lnk=cluster->m_linksList[i];
    if (lnk.m_type==RagTime5ClusterManager::Link::L_List) {
      m_mainParser.readListZone(lnk);
    }
    else if (lnk.m_type==RagTime5ClusterManager::Link::L_LongList) {
      std::vector<long> list;
      m_mainParser.readLongList(lnk, list);
    }
    else if (lnk.m_type==RagTime5ClusterManager::Link::L_GraphicTransform)
      readGraphicTransformations(lnk);
    else
      m_mainParser.readFixedSizeZone(lnk, "");
  }
  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
