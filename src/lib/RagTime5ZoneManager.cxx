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

#include <iomanip>
#include <iostream>
#include <sstream>

#include "MWAWDebug.hxx"
#include "MWAWPrinter.hxx"

#include "RagTime5Parser.hxx"
#include "RagTime5StructManager.hxx"

#include "RagTime5ZoneManager.hxx"

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
RagTime5ZoneManager::RagTime5ZoneManager(RagTime5Parser &parser) : m_mainParser(parser), m_structManager(m_mainParser.getStructManager())
{
}

RagTime5ZoneManager::~RagTime5ZoneManager()
{
}

////////////////////////////////////////////////////////////
// read basic structures
////////////////////////////////////////////////////////////
bool RagTime5ZoneManager::readFieldHeader(RagTime5Zone &zone, long endPos, std::string const &headerName, long &endDataPos, long expectedLVal)
{
  MWAWInputStreamPtr input=zone.getInput();
  long pos=input->tell();
  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;

  f << headerName << ":";
  long lVal, sz;
  bool ok=true;
  if (pos>=endPos || !RagTime5StructManager::readCompressedLong(input, endPos, lVal) ||
      !RagTime5StructManager::readCompressedLong(input,endPos,sz) || sz <= 7 || input->tell()+sz>endPos) {
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::readFieldHeader: can not read the main item\n"));
    f << "###";
    ok=false;
  }
  else {
    if (lVal!=expectedLVal)
      f << "f0=" << lVal << ",";
    f << "sz=" << sz << ",";
    endDataPos=input->tell()+sz;
  }
  if (!headerName.empty()) {
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return ok;
}

////////////////////////////////////////////////////////////
// zone function
////////////////////////////////////////////////////////////
void RagTime5Zone::createAsciiFile()
{
  if (m_asciiName.empty()) {
    MWAW_DEBUG_MSG(("RagTime5Zone::createAsciiFile: can not find the ascii name\n"));
    return;
  }
  if (m_localAsciiFile) {
    MWAW_DEBUG_MSG(("RagTime5Zone::createAsciiFile: the ascii file already exist\n"));
  }
  m_localAsciiFile.reset(new libmwaw::DebugFile(m_input));
  m_asciiFile = m_localAsciiFile.get();
  m_asciiFile->open(m_asciiName.c_str());
}

std::string RagTime5Zone::getZoneName() const
{
  switch (m_ids[0]) {
  case 0:
    if (m_fileType==F_Data)
      return "FileHeader";
    break;
  case 1: // with g4=1, gd=[1, lastDataZones]
    if (m_fileType==F_Main)
      return "ZoneInfo";
    break;
  case 3: // with no value or gd=[1,_] (if multiple)
    if (m_fileType==F_Main)
      return "Main3A";
    break;
  case 4:
    if (m_fileType==F_Main)
      return "ZoneLimits,";
    break;
  case 5:
    if (m_fileType==F_Main)
      return "FileLimits";
    break;
  case 6: // gd=[_,_]
    if (m_fileType==F_Main)
      return "Main6A";
    break;
  case 8: // type=UseCount, gd=[0,num>1], can be multiple
    if (m_fileType==F_Main)
      return "UnknCounter8";
    break;
  case 10: // type=SingleRef, gd=[1,id], Data id is a list types
    if (m_fileType==F_Main)
      return "Types";
    break;
  case 11: // type=SingleRef, gd=[1,id]
    if (m_fileType==F_Main)
      return "Cluster";
    break;
  default:
    break;
  }
  std::stringstream s;
  switch (m_fileType) {
  case F_Main:
    s << "Main" << m_ids[0] << "A";
    break;
  case F_Data:
    s << "Data" << m_ids[0] << "A";
    break;
  case F_Empty:
    s << "unused" << m_ids[0];
    break;
  case F_Unknown:
  default:
    s << "##zone" << m_subType << ":" << m_ids[0] << "";
    break;
  }
  return s.str();
}

std::ostream &operator<<(std::ostream &o, RagTime5Zone const &z)
{
  o << z.getZoneName();
  if (z.m_idsFlag[0])
    o << "[" << z.m_idsFlag[0] << "],";
  else
    o << ",";
  for (int i=1; i<3; ++i) {
    if (!z.m_kinds[i-1].empty()) {
      o << z.m_kinds[i-1] << ",";
      continue;
    }
    if (!z.m_ids[i] && !z.m_idsFlag[i]) continue;
    o << "id" << i << "=" << z.m_ids[i];
    if (z.m_idsFlag[i]==0)
      o << "*";
    else if (z.m_idsFlag[i]!=1)
      o << ":" << z.m_idsFlag[i] << ",";
    o << ",";
  }
  if (z.m_variableD[0] || z.m_variableD[1])
    o << "varD=[" << z.m_variableD[0] << "," << z.m_variableD[1] << "],";
  if (z.m_entry.valid())
    o << z.m_entry.begin() << "<->" << z.m_entry.end() << ",";
  else if (!z.m_entriesList.empty()) {
    o << "ptr=" << std::hex;
    for (size_t i=0; i< z.m_entriesList.size(); ++i) {
      o << z.m_entriesList[i].begin() << "<->" << z.m_entriesList[i].end();
      if (i+1<z.m_entriesList.size())
        o << "+";
    }
    o << std::dec << ",";
  }
  if (!z.m_hiLoEndian) o << "loHi[endian],";
  o << z.m_extra << ",";
  return o;
}

////////////////////////////////////////////////////////////
// link to cluster
////////////////////////////////////////////////////////////
bool RagTime5ZoneManager::readFieldClusters(Link const &link)
{
  if (link.m_ids.size()!=2) {
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::readFieldClusters: call with bad ids\n"));
    return false;
  }
  for (size_t i=0; i<2; ++i) {  // fielddef and fieldpos
    if (!link.m_ids[i]) continue;
    shared_ptr<RagTime5Zone> data=m_mainParser.getDataZone(link.m_ids[i]);
    if (!data || data->m_isParsed || data->getKindLastPart(data->m_kinds[1].empty())!="Cluster") {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readFieldClusters: the child cluster id %d seems bad\n", link.m_ids[i]));
      continue;
    }
    m_mainParser.readClusterZone(*data, 0x20000+int(i));
  }
  return true;
}

bool RagTime5ZoneManager::readUnknownClusterC(Link const &link)
{
  if (link.m_ids.size()!=4) {
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterC: call with bad ids\n"));
    return false;
  }
  for (size_t i=0; i<4; ++i) {
    if (!link.m_ids[i]) continue;
    shared_ptr<RagTime5Zone> data=m_mainParser.getDataZone(link.m_ids[i]);
    if (!data || data->m_isParsed || data->getKindLastPart(data->m_kinds[1].empty())!="Cluster") {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterC: the child cluster id %d seems bad\n", link.m_ids[i]));
      continue;
    }
    m_mainParser.readClusterZone(*data, 0x30000+int(i));
  }
  return true;
}

////////////////////////////////////////////////////////////
// main cluster fonction
////////////////////////////////////////////////////////////
bool RagTime5ZoneManager::readClusterZone(RagTime5Zone &zone, RagTime5ZoneManager::ClusterParser &parser, bool warnForUnparsed)
{
  MWAWEntry &entry=zone.m_entry;
  if (entry.length()==0 || entry.length()<13) {
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: the zone %d seems bad\n", zone.m_ids[0]));
    return false;
  }
  shared_ptr<Cluster> cluster=parser.getCluster();
  if (!cluster) {
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: oops, the cluster is not defined\n"));
    return false;
  }
  cluster->m_hiLoEndian=parser.m_hiLoEndian=zone.m_hiLoEndian;
  cluster->m_zoneId=zone.m_ids[0];

  MWAWInputStreamPtr input=zone.getInput();
  long endPos=entry.end();
  input->setReadInverted(!zone.m_hiLoEndian);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);

  libmwaw::DebugStream f;
  f.str("");
  f << "Entries(" << parser.getZoneName() << ")[" << zone << "]:";
  int val;
  for (int i=0; i<4; ++i) { // f0=f1=0, f2=1, f3=small number
    static int const(expected[])= {0,0,1,0};
    val=(int) input->readLong(2);
    if (val!=expected[i]) f << "f" << i << "=" << val << ",";
  }
  zone.m_isParsed=true;
  libmwaw::DebugFile &ascFile=zone.ascii();
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(endPos);
  ascFile.addNote("_");

  parser.m_dataId=-1;
  while (!input->isEnd()) {
    long pos=input->tell();
    if (pos>=endPos) break;
    ++parser.m_dataId;
    parser.m_link=Link();
    parser.startZone();

    long endDataPos;
    if (!readFieldHeader(zone, endPos, parser.getZoneName(parser.m_dataId), endDataPos)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }

    pos=input->tell();
    f.str("");
    f << parser.getZoneName(parser.m_dataId) << "-A:";
    if (!zone.m_hiLoEndian) f << "lohi,";
    long fSz;
    if (!RagTime5StructManager::readCompressedLong(input, endDataPos, fSz) || fSz<6 || input->tell()+fSz>endDataPos) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: can not read item A\n"));
      f << "###fSz";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
      continue;
    }
    long debSubDataPos=input->tell();
    long endSubDataPos=debSubDataPos+fSz;
    int fl=(int) input->readULong(2); // [01][13][0139b]
    int N=(int) input->readLong(4);
    if (!parser.parseZone(input, fSz, N, fl, f) && warnForUnparsed) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find an unparsed zone\n"));
      f << "###";
    }

    if (input->tell()!=endSubDataPos) {
      ascFile.addDelimiter(input->tell(),'|');
      input->seek(endSubDataPos, librevenge::RVNG_SEEK_SET);
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    int m=-1;
    while (!input->isEnd()) {
      pos=input->tell();
      if (pos+4>endDataPos)
        break;
      ++m;

      RagTime5StructManager::Field field;
      if (!m_structManager->readField(input, endDataPos, ascFile, field)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        break;
      }
      f.str("");
      f << parser.getZoneName(parser.m_dataId,m) << ":";
      if (!parser.parseField(field, m, f)) {
        if (warnForUnparsed) {
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find an unparsed field\n"));
          f << "###";
        }
        f << field;
      }
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    pos=input->tell();
    if (pos!=endDataPos) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find some extra data\n"));
      f.str("");
      f << parser.getZoneName(parser.m_dataId) <<  ":" << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    parser.endZone();
    input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
  }

  long pos=input->tell();
  if (pos!=endPos) {
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find some extra data\n"));
    f.str("");
    f << parser.getZoneName() << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->setReadInverted(false);

  return true;
}

////////////////////////////////////////////////////////////
// pattern cluster implementation
////////////////////////////////////////////////////////////

bool RagTime5ZoneManager::ClusterParser::readListHeader(MWAWInputStreamPtr &input, int type, Link &link, long(&values)[5], libmwaw::DebugStream &f)
{
  link.m_fileType[0]=input->readLong(4);
  values[0]=(long) input->readULong(4); // sometimes a type
  if (values[0])
    f << "type1=" << std::hex << values[0] << std::dec << ",";
  for (int i=1; i<5; ++i) { // always 0?
    values[i]=input->readLong(2);
    if (values[i]) f << "f" << i << "=" << values[i] << ",";
  }
  link.m_fileType[1]=(long) input->readULong(2);
  if (type>=0 && (link.m_fileType[1]&0xFFD7)!=type) {
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::ClusterParser::readListHeader: find unexpected file type\n"));
    f << "#fileType=" << std::hex << link.m_fileType[1] << std::dec << ",";
  }
  long pos=input->tell();
  link.m_type= RagTime5ZoneManager::Link::L_List;
  if (!RagTime5StructManager::readDataIdList(input, 2, link.m_ids)) {
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::ClusterParser::readListHeader: can not read the data id\n"));
    f << "###noData,";
    input->seek(pos+8, librevenge::RVNG_SEEK_SET);
    link=Link();
    return true;
  }
  if (!link.m_ids[1]) {
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::ClusterParser::readListHeader: can not read the data id\n"));
    f << "###noData,";
  }

  return true;
}

bool RagTime5ZoneManager::ClusterParser::readFixedSizeListHeader(MWAWInputStreamPtr &input, int type, bool readFieldSize,
    Link &link, long(&values)[5], libmwaw::DebugStream &f)
{
  link.m_fileType[0]=(long) input->readULong(4);
  values[0]=(long) input->readULong(4); // sometimes a type
  if (values[0])
    f << "type1=" << std::hex << values[0] << std::dec << ",";
  for (int i=1; i<5; ++i) { // always 0?
    values[i]=input->readLong(2);
    if (values[i]) f << "f" << i << "=" << values[i] << ",";
  }
  link.m_fileType[1]=(long) input->readULong(2);
  if (type>=0 && (link.m_fileType[1]&0xFFD7)!=type) {
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::ClusterParser::readFixedSizeListHeader: find unexpected file type\n"));
    f << "#fileType=" << std::hex << link.m_fileType[1] << std::dec << ",";
  }
  if (readFieldSize)
    link.m_fieldSize=(int) input->readULong(2);
  long pos=input->tell();
  if (!RagTime5StructManager::readDataIdList(input, 1, link.m_ids)) {
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::ClusterParser::readFixedSizeListHeader: can not read the data id\n"));
    f << "###noData,";
    input->seek(pos+4, librevenge::RVNG_SEEK_SET);
    link=Link();
    return true;
  }
  if (!link.m_ids[0] && readFieldSize) {
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::ClusterParser::readFixedSizeListHeader: can not read the data id\n"));
    f << "###noData,";
  }

  return true;
}

namespace RagTime5ZoneManagerInternal
{

//
//! low level: parser of color pattern cluster : zone 0x8042
//
struct ColPatCParser : public RagTime5ZoneManager::ClusterParser {
  //! constructor
  ColPatCParser() : ClusterParser(0x8042, "ClustColPat"), m_cluster(new RagTime5ZoneManager::Cluster)
  {
    m_cluster->m_type=RagTime5ZoneManager::Cluster::C_ColorPattern;
  }
  //! return the current cluster
  shared_ptr<RagTime5ZoneManager::Cluster> getCluster()
  {
    return m_cluster;
  }
  //! parse a zone
  bool parseZone(MWAWInputStreamPtr &input, long fSz, int N, int flag, libmwaw::DebugStream &f)
  {
    if ((m_dataId==0&&flag!=0x30) || (m_dataId==1&&flag!=0x10) || m_dataId>=2)
      f << "fl=" << std::hex << flag << std::dec << ",";

    int val;
    if (N==-5) {
      if (m_dataId || (fSz!=82 && fSz!=86)) {
        f << "###data,";
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::ColPatCParser::parseZone: find unexpected field\n"));
        return false;
      }
      for (int i=0; i<2; ++i) { // always 0?
        val=(int) input->readLong(2);
        if (val) f << "f" << i+1 << "=" << val << ",";
      }
      val=(int) input->readULong(4);
      if (val!=0x16a8042) f << "#fileType=" << std::hex << val << std::dec << ",";
      for (int i=0; i<2; ++i) {
        val=(int) input->readLong(2);
        if (val) f << "f" << i+3 << "=" << val << ",";
      }

      for (int wh=0; wh<2; ++wh) {
        long actPos=input->tell();
        RagTime5ZoneManager::Link link;
        f << "link" << wh << "=[";
        val=(int) input->readLong(2);
        if (val!= 0x10)
          f << "f0=" << val << ",";
        val=(int) input->readLong(2);
        if (val) // always 0?
          f << "f1=" << val << ",";
        link.m_N=(int) input->readLong(2);
        link.m_fileType[1]=(long) input->readULong(4);
        if ((wh==0 && link.m_fileType[1]!=0x84040) ||
            (wh==1 && link.m_fileType[1]!=0x16de842))
          f << "#fileType=" << std::hex << link.m_fileType[1] << std::dec << ",";
        for (int i=0; i<7; ++i) {
          val=(int) input->readLong(2); // always 0?
          if (val) f << "f" << i+2 << "=" << val << ",";
        }
        link.m_fieldSize=(int) input->readULong(2);
        std::vector<int> listIds;
        if (!RagTime5StructManager::readDataIdList(input, 1, listIds)) {
          MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::ColPatCParser::parseZone: can not read the data id\n"));
          f << "##link=" << link << "],";
          input->seek(actPos+30, librevenge::RVNG_SEEK_SET);
          continue;
        }
        if (listIds[0]) {
          link.m_ids.push_back(listIds[0]);
          m_cluster->m_linksList.push_back(link);
        }
        f << link;
        f << "],";
      }
      std::vector<int> listIds;
      if (!RagTime5StructManager::readDataIdList(input, 1, listIds)) {
        f << "##clusterIds";
        return true;
      }
      if (listIds[0]) {
        m_cluster->m_clusterIdsList.push_back(listIds[0]);
        f << "clusterId1=data" << listIds[0] << "A,";
      }
      if (fSz==82)
        return true;
      val=(int) input->readLong(4);
      if (val!=2)
        f << "g0=" << val << ",";
      return true;
    }

    if (N<=0 || m_dataId!=1) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::ColPatCParser::parseZone: find unexpected header N\n"));
      f << "###N=" << N << ",";
      return false;
    }
    if (fSz!=30) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::ColPatCParser::parseZone: find unexpected data size\n"));
      f << "###fSz=" << fSz << ",";
      return false;
    }

    RagTime5ZoneManager::Link link;
    link.m_N=N;
    long linkValues[5]; // f0=2b|2d|85|93
    if (readFixedSizeListHeader(input, 0X40, true, link, linkValues, f)) {
      f << link << ",";
      if (!link.empty())
        m_cluster->m_linksList.push_back(link);
    }
    return true;
  }
  //! parse a field
  bool parseField(RagTime5StructManager::Field const &field, int /*m*/, libmwaw::DebugStream &f)
  {
    if (m_dataId==0 && field.m_type==RagTime5StructManager::Field::T_FieldList && (field.m_fileType==0x16be055 || field.m_fileType==0x16be065)) {
      f << "unk" << (field.m_fileType==0x16be055 ? "0" : "1") << "=";
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Long && child.m_fileType==0xcf817) {
          f << child.m_longValue[0] << ",";
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::ColPatCParser::parseField: find unexpected color/pattern child field\n"));
        f << "#[" << child << "],";
      }
    }
    else {
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::ColPatCParser::parseField: find unexpected sub field\n"));
      f << "#" << field;
    }
    return true;
  }
protected:
  //! the current cluster
  shared_ptr<RagTime5ZoneManager::Cluster> m_cluster;
};

//
//! try to read a layout cluster: 4001
//
struct LayoutCParser : public RagTime5ZoneManager::ClusterParser {
  //! constructor
  LayoutCParser() : ClusterParser(0x4001, "ClustLayout"), m_cluster(new RagTime5ZoneManager::ClusterLayout),
    m_actualZone(0), m_numZones(0), m_what(-1), m_linkId(-1), m_fieldName("")
  {
    m_cluster->m_type=RagTime5ZoneManager::Cluster::C_Layout;
  }
  //! return the current cluster
  shared_ptr<RagTime5ZoneManager::Cluster> getCluster()
  {
    return m_cluster;
  }
  //! start a new zone
  void startZone()
  {
    if (m_what<=0)
      ++m_what;
    else if (m_what==1) {
      if (++m_actualZone>=m_numZones)
        ++m_what;
    }
  }
  //! end of a start zone call
  void endZone()
  {
    if (m_link.empty())
      return;
    switch (m_linkId) {
    case 0:
      m_cluster->m_listItemLink=m_link;
      break;
    case 1:
      m_cluster->m_pipelineLink=m_link;
      break;
    case 2:
      m_cluster->m_settingLinks.push_back(m_link);
      break;
    case 3:
      if (m_cluster->m_nameLink.empty())
        m_cluster->m_nameLink=m_link;
      else {
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::LayoutCParser::endZone: oops the name link is already set\n"));
        m_cluster->m_linksList.push_back(m_link);
      }
      break;
    default:
      m_cluster->m_linksList.push_back(m_link);
      break;
    }
  }
  //! parse a zone
  bool parseZone(MWAWInputStreamPtr &input, long fSz, int N, int flag, libmwaw::DebugStream &f)
  {
    m_fieldName="";
    m_linkId=-1;
    if (m_what==0)
      return parseHeaderZone(input,fSz,N,flag,f);
    if (m_what==1)
      return parseZoneBlock(input,fSz,N,flag,f);
    m_what=2;

    if (N<0) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::LayoutCParser::endZone: N value seems bad\n"));
      f << "###N=" << N << ",";
      return true;
    }
    int val;
    m_link.m_N=N;
    long linkValues[5];
    switch (fSz) {
    case 28:
      readFixedSizeListHeader(input, 0, false, m_link, linkValues, f);
      if (m_link.m_fileType[0]==0x35800) {
        m_what=4;
        f << "zone[longs],";
        m_fieldName="zone:longs";
      }
      else if (m_link.m_fileType[0]==0x3e800) {
        m_what=4;
        f << "list[long0],";
        m_fieldName="list:longs0";
      }
      else {
        f << "###fType=" << std::hex << m_link.m_fileType[0] << std::dec << ",";
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::LayoutCParser::parseZone: the field fSz28 type seems bad\n"));
        return true;
      }
      f << m_link << ",";
      m_link.m_type=RagTime5ZoneManager::Link::L_LongList;
      m_link.m_fieldSize=4;
      return true;
    case 30: {
      readFixedSizeListHeader(input, -1, true, m_link, linkValues, f);
      long expectedFileType1=0;
      if (m_link.m_fileType[0]==0x14b9800) { // 10
        m_linkId=0;
        m_what=3;
        m_fieldName="data0";
        expectedFileType1=0x10;
      }
      else if (m_link.m_fileType[0]==0) { // 0
        m_linkId=1;
        m_fieldName="pipeline";
      }
      else {
        f << "###fType=" << std::hex << m_link.m_fileType[0] << std::dec << ",";
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::LayoutCParser::parseZone: the field fSz30 type seems bad\n"));
        return true;
      }
      if ((m_link.m_fileType[1]&0xFFD7)!=expectedFileType1) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::LayoutCParser::parseZone: fileType1 seems odd[fSz=30]\n"));
        f << "###fileType1=" << std::hex << m_link.m_fileType[1] << std::dec << ",";
      }
      f << m_fieldName << "," << m_link << ",";
      return true;
    }
    case 32: {
      readListHeader(input, -1, m_link, linkValues, f);
      long expectedFileType1=0;
      if (m_link.m_fileType[0]==0) {
        expectedFileType1=0x200;
        m_linkId=3;
        m_what=4;
        m_link.m_type=RagTime5ZoneManager::Link::L_UnicodeList;
        m_fieldName="unicodeList";
      }
      else if (m_link.m_fileType[0]==0x47040) {
        m_what=4;
        m_linkId=2;
        m_link.m_name=m_fieldName="settings";
      }
      else {
        f << "###fType=" << std::hex << m_link.m_fileType[0] << std::dec << ",";
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::LayoutCParser::parseZone: the field fSz32 type seems bad\n"));
        return true;
      }
      if ((m_link.m_fileType[1]&0xFFDF)!=expectedFileType1) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::LayoutCParser::parseZone: fileType1 seems odd[fSz=32]\n"));
        f << "###fileType1=" << std::hex << m_link.m_fileType[1] << std::dec << ",";
      }
      f <<  m_fieldName << "," << m_link << ",";
      return true;
    }
    case 36: // follow when it exists the fSz=30 zone, no auxilliar data
      f << "unicode[def],";
      m_fieldName="unicode[def]";
      for (int i=0; i<2; ++i) { // always 0
        val=(int) input->readLong(2);
        if (val) f << "f" << i << "=" << val << ",";
      }
      val=(int) input->readULong(4);
      if (val!=0x7d01a) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::LayoutCParser::parseZone: find unexpected filetype[fSz=36]\n"));
        f << "###fileType=" << std::hex << val << std::dec << ",";
      }
      val=(int) input->readLong(4); // 0 or small number
      if (val) f << "id0=" << val << ",";
      for (int i=0; i<3; ++i) { // f4=10
        val=(int) input->readLong(2);
        if (val) f << "f" << i+2 << "=" << val << ",";
      }
      f << "ids=[";
      for (int i=0; i<3; ++i) { // an increasing sequence
        val=(int) input->readLong(4);
        if (val)
          f << val << ",";
        else
          f << "_,";
      }
      f << "],";
      return true;
    case 38: // in 1 or 2 exemplar, no auxilliar data
      f << "settings[Def],";
      m_fieldName="settings[Def]";
      val=(int) input->readULong(4);
      if (val!=0x47040) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::LayoutCParser::parseZone: find unexpected type[fSz=38]\n"));
        f << "##fileType=" << std::hex << val << std::dec << ",";
      }
      val=(int) input->readULong(4); // 0|1c07007|1492042
      if (val) f << "fileType1=" << std::hex << val << std::dec << ",";
      for (int i=0; i<5; ++i) { // f1=0|8, f4=10
        val=(int) input->readLong(2);
        if (val) f << "f" << i << "=" << val << ",";
      }
      f << "ids=[";
      for (int i=0; i<3; ++i) { // small increasing sequence
        val=(int) input->readLong(4);
        if (val) f << val << ",";
        else f << "_,";
      }
      f << "],";
      val=(int) input->readLong(2); // always 0
      if (val) f << "f5=" << val << ",";
      break;
    case 54: { // can be in multiple exemplar ~ 1 by zone, no auxilliar data
      f << "data1,";
      m_fieldName="data1";
      for (int i=0; i<2; ++i) { // always 0
        val=(int) input->readLong(2);
        if (val) f << "f" << i << "=" << val << ",";
      }
      float dim[2];
      for (int i=0; i<2; ++i) dim[i]=float(input->readLong(4))/65536.f;
      f << "sz=" << Vec2f(dim[0],dim[1]) << ",";

      std::vector<int> listIds;
      long actPos=input->tell();
      if (!RagTime5StructManager::readDataIdList(input, 1, listIds)) {
        f << "###cluster1,";
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::LayoutCParser::parseZone: can not read cluster block[fSz=54]\n"));
        input->seek(actPos+4, librevenge::RVNG_SEEK_SET);
      }
      else if (listIds[0]) { // link to the new cluster e zone ? (can be also 0)
        m_cluster->m_clusterIdsList.push_back(listIds[0]);
        f << "cluster0=data" << listIds[0] << "A,";
      }
      for (int i=0; i<7; ++i) { // always 0
        val=(int) input->readLong(2);
        if (val) f << "f" << i+2 << "=" << val << ",";
      }
      for (int i=0; i<9; ++i) { // g0=1, g1=1-7, g2=0-d, g3=0-1, g8=2-8
        val=(int) input->readLong(2);
        if (val) f << "g" << i << "=" << val << ",";
      }
      return true;
    }
    default:
      f << "###fSz=" << fSz << ",";
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::LayoutCParser::parseZone: find unexpected file size\n"));
      break;
    }
    return true;
  }
  //! parse a field
  bool parseField(RagTime5StructManager::Field const &field, int /*m*/, libmwaw::DebugStream &f)
  {
    if (!m_fieldName.empty())
      f << m_fieldName << ",";
    switch (m_what) {
    case 0: // main
      if (field.m_type==RagTime5StructManager::Field::T_FieldList && field.m_fileType==0x14b5815) {
        for (size_t i=0; i<field.m_fieldList.size(); ++i) {
          RagTime5StructManager::Field const &child=field.m_fieldList[i];
          if (child.m_type==RagTime5StructManager::Field::T_LongList && child.m_fileType==0xcf042) {
            // TODO: storeme
            f << "ids=[";
            for (size_t j=0; j<child.m_longList.size(); ++j) {
              if (child.m_longList[j]==0)
                f << "_,";
              else
                f << child.m_longList[j] << ",";
            }
            continue;
          }
          MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::LayoutCParser::parseField: find unexpected main field\n"));
          f << "###[" << child << "],";
        }
        break;
      }
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::LayoutCParser::parseField: find unexpected main field\n"));
      f << "###" << field;
      break;
    case 3: // data0
      if (field.m_type==RagTime5StructManager::Field::T_Unstructured && field.m_fileType==0xce017) {
        // rare, always 2
        f << "unkn="<< field.m_extra << ",";
        break;
      }
      if (field.m_type==RagTime5StructManager::Field::T_Long && field.m_fileType==0xcf817) {
        // a small value between 2a and 61
        f << "f0="<<field.m_longValue[0] << ",";
        break;
      }
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::LayoutCParser::parseField: find unexpected data0 field\n"));
      f << "###" << field;
      break;
    case 4: // list
      if (field.m_type==RagTime5StructManager::Field::T_LongList && field.m_fileType==0xce842) {
        f << "pos=[";
        for (size_t i=0; i<field.m_longList.size(); ++i) {
          if (field.m_longList[i]==0)
            f << "_,";
          else if (field.m_longList[i]>1000)
            f << std::hex << field.m_longList[i] << std::dec << ",";
          else
            f << field.m_longList[i] << ",";
        }
        f << "],";
        m_link.m_longList=field.m_longList;
        break;
      }
      if (field.m_type==RagTime5StructManager::Field::T_Unstructured && field.m_fileType==0xce017) {
        // a small value 2|4|a|1c|40
        f << "unkn="<<field.m_extra << ",";
        break;
      }
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::LayoutCParser::parseField: find unexpected list field\n"));
      f << "###" << field;
      break;
    default:
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::LayoutCParser::parseField: find unexpected sub field\n"));
      f << "###" << field;
      break;
    }
    return true;
  }
protected:
  //! parse a zone block
  bool parseZoneBlock(MWAWInputStreamPtr &input, long fSz, int N, int flag, libmwaw::DebugStream &f)
  {
    if (N<0 || m_what!=1 || (fSz!=50 && fSz!=66)) {
      f << "###N=" << N << ",fSz=" << fSz << ",";
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::LayoutCParser::parseZoneBlock: find unexpected main field\n"));
      return false;
    }
    f << "block, fl=" << std::hex << flag << std::dec << ",";
    m_fieldName="block";
    if (N!=1) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::LayoutCParser::parseZoneBlock: zone N seems badA\n"));
      f << "#N=" << N << ",";
    }
    int val;
    for (int i=0; i<2; ++i) { // always 0
      val=(int) input->readLong(2);
      if (val)
        f << "f" << i << "=" << val << ",";
    }
    float dim[2];
    for (int i=0; i<2; ++i) dim[i]=float(input->readLong(4))/65536.f;
    m_cluster->m_zoneDimensions.push_back(Vec2f(dim[0],dim[1]));
    f << "sz=" << Vec2f(dim[0],dim[1]) << ",";
    std::vector<int> listIds;
    long actPos=input->tell();
    if (!RagTime5StructManager::readDataIdList(input, 1, listIds)) {
      f << "###cluster0,";
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::LayoutCParser::parseZoneBlock: can not read first cluster block\n"));
      input->seek(actPos+4, librevenge::RVNG_SEEK_SET);
    }
    else if (listIds[0]) { // link to a cluster e zone
      m_cluster->m_clusterIdsList.push_back(listIds[0]);
      f << "cluster0=data" << listIds[0] << "A,";
    }
    for (int i=0; i<2; ++i) { // always 0: another item?
      val=(int) input->readLong(2);
      if (val)
        f << "f" << i+2 << "=" << val << ",";
    }
    listIds.clear();
    actPos=input->tell();
    if (!RagTime5StructManager::readDataIdList(input, 1, listIds)) {
      f << "###cluster1,";
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::LayoutCParser::parseZoneBlock: can not read second cluster block\n"));
      input->seek(actPos+4, librevenge::RVNG_SEEK_SET);
    }
    else if (listIds[0]) { // link to the new cluster 4001 zone ?
      m_cluster->m_clusterIdsList.push_back(listIds[0]);
      f << "cluster1=data" << listIds[0] << "A,";
    }
    for (int i=0; i<2; ++i) { // either 0,0 or 1,small number (but does not seems a data id )
      val=(int) input->readLong(2);
      if (val)
        f << "f" << i+4 << "=" << val << ",";
    }
    val=(int) input->readLong(4); // alwas 1?
    if (val!=1) f << "f6=" << val << ",";
    f << "unkn=[";
    for (int i=0; i<4; ++i) { // small number
      val=(int) input->readLong(2);
      if (val)
        f << val << ",";
      else
        f << "_,";
    }
    f << "],";
    for (int i=0; i<2; ++i) { // g1=0|4|5
      val=(int) input->readLong(2);
      if (val)
        f << "g" << i << "=" << val << ",";
    }
    if (fSz==66) {
      for (int i=0; i<2; ++i) { // g2=0|7|8, g3=5-12
        val=(int) input->readLong(4);
        if (val)
          f << "g" << i+2 << "=" << val << ",";
      }
      for (int i=0; i<2; ++i) { // fl0=0|1
        val=(int) input->readLong(1);
        if (val)
          f << "fl" << i << "=" << val << ",";
      }
      for (int i=0; i<3; ++i) { // g4=0|1, g6=g4
        val=(int) input->readLong(2);
        if (val)
          f << "g" << i+4 << "=" << val << ",";
      }
    }
    return true;
  }

  //! parse the header zone
  bool parseHeaderZone(MWAWInputStreamPtr &input, long fSz, int N, int flag, libmwaw::DebugStream &f)
  {
    f << "header, fl=" << std::hex << flag << std::dec << ",";
    m_fieldName="header";
    if (N!=-5 || m_what!=0 || (fSz!=123 && fSz!=127 && fSz!=128 && fSz!=132)) {
      f << "###N=" << N << ",fSz=" << fSz << ",";
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::LayoutCParser::parseHeaderZone: find unexpected main field\n"));
      return false;
    }
    int val;
    for (int i=0; i<2; ++i) { // always 0?
      val=(int) input->readLong(2);
      if (val) f << "f" << i+1 << "=" << val << ",";
    }
    val=(int) input->readLong(2);
    f << "id=" << val << ",";
    val=(int) input->readULong(2);
    if (val!=m_type) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::LayoutCParser::parseHeaderZone: unexpected zone type\n"));
      f << "##zoneType=" << std::hex << val << std::dec << ",";
    }
    for (int i=0; i<2; ++i) { // f0=0, f1=4-6
      val=(int) input->readLong(4);
      if (val)
        f << "f" << i << "=" << val << ",";
    }
    val=(int) input->readLong(2); // always 16
    if (val!=16)
      f << "f2=" << val << ",";
    m_numZones=(int) input->readLong(4);
    if (m_numZones!=1)
      f << "num[zone1]=" << m_numZones << ",";
    long fileType=(long) input->readULong(4);
    if (fileType!=0x14b6052) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::LayoutCParser::parseHeaderZone: find unexpected filetype\n"));
      f << "#fileType0=" << std::hex << fileType << std::dec << ",";
    }
    for (int i=0; i<9; ++i) { // f11=0x60,
      val=(int) input->readLong(2);
      if (val)
        f << "f" << i+5 << "=" << val << ",";
    }
    val=(int) input->readLong(1);
    if (val!=1) f << "fl=" << val << ",";
    if (fSz==128 || fSz==132) {
      for (int i=0; i<5; ++i) { // unsure find only 0 here
        val=(int) input->readLong(1);
        if (val)
          f << "flA" << i << "=" << val << ",";
      }
    }
    val=(int) input->readLong(4);
    if (val) // 0-20
      f << "g0=" << val << ",";
    long actPos=input->tell();
    std::vector<int> listIds;
    if (!RagTime5StructManager::readDataIdList(input, 1, listIds)) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::LayoutCParser::parseHeaderZone: can not read first cluster block\n"));
      f << "##badCluster,";
      input->seek(actPos+4, librevenge::RVNG_SEEK_SET);
    }
    else if (listIds[0]) { // find link to a named frame cluster
      m_cluster->m_clusterIdsList.push_back(listIds[0]);
      f << "clusterId1=data" << listIds[0] << "A,";
    }
    for (int i=0; i<2; ++i) { // always 0
      val=(int) input->readLong(2);
      if (val)
        f << "g" << i+1 << "=" << val << ",";
    }
    float dim[4];
    for (int i=0; i<2; ++i) dim[i]=float(input->readLong(4))/65536.f;
    Vec2f frameSize(dim[0],dim[1]);
    f << "sz=" << frameSize << ",";
    for (int i=0; i<2; ++i) dim[i]=float(input->readLong(4))/65536.f;
    if (Vec2f(dim[0],dim[1])!=frameSize)
      f << "sz2=" << Vec2f(dim[0],dim[1]) << ",";
    for (int i=0; i<10; ++i) { // find g3=0|1|69, g8=0|5|8, g9=g11=g12=1, g10=small number(but not a data id)
      val=(int) input->readLong(2);
      if (val)
        f << "g" << i+3 << "=" << val << ",";
    }
    for (int i=0; i<4; ++i) dim[i]=float(input->readLong(4))/65536.f;
    f << "dim=" << Box2f(Vec2f(dim[0],dim[1]), Vec2f(dim[2],dim[3])) << ",";
    for (int i=0; i<4; ++i) { // h3=0|1|3|6
      val=(int) input->readLong(2);
      if (val)
        f << "h" << i << "=" << val << ",";
    }
    if (fSz==127 || fSz==132) {
      for (int i=0; i<2; ++i) { // always 0
        val=(int) input->readLong(2);
        if (val)
          f << "h" << i+3 << "=" << val << ",";
      }
    }
    return true;
  }

  //! the current cluster
  shared_ptr<RagTime5ZoneManager::ClusterLayout> m_cluster;
  //! the actual zone
  int m_actualZone;
  //! the number of zones
  int m_numZones;
  //! a index to know which field is parsed :  0: main, 1:list of zones, 2: unknown, 3:data0, 4:list
  int m_what;
  //! the link id : 0: listItem, 1: pipeline, 2: settinglinks, 3: namelink,
  int m_linkId;
  //! the actual field name
  std::string m_fieldName;
};

//
//! try to read a pipeline cluster: 104,204,4104, 4204
//
struct PipelineCParser : public RagTime5ZoneManager::ClusterParser {
  //! constructor
  PipelineCParser(int type) : ClusterParser(type, "ClustPipeline"), m_cluster(new RagTime5ZoneManager::Cluster)
  {
    m_cluster->m_type=RagTime5ZoneManager::Cluster::C_Pipeline;
  }
  //! return the current cluster
  shared_ptr<RagTime5ZoneManager::Cluster> getCluster()
  {
    return m_cluster;
  }
  //! parse a zone
  bool parseZone(MWAWInputStreamPtr &input, long fSz, int N, int flag, libmwaw::DebugStream &f)
  {
    if (flag!=0x31)
      f << "fl=" << std::hex << flag << std::dec << ",";
    if (m_dataId || N!=-5) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::PipelineCParser::parseZone: find unexpected header\n"));
      f << "###type" << std::hex << N << std::dec;
      return true;
    }
    if (fSz!=76 && fSz!=110) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::PipelineCParser::parseZone: find unexpected file size\n"));
      f << "###fSz=" << fSz << ",";
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
    if (val!=m_type) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::PipelineCParser::parseZone: the zone type seems odd\n"));
      f << "##zoneType=" << std::hex << val << std::dec << ",";
    }
    val=(int) input->readLong(2); // always 0?
    if (val) f << "f4=" << val << ",";
    for (int i=0; i<7; ++i) { // g1, g2, g3 small int other 0
      val=(int) input->readLong(4);
      if (i==2)
        m_link.m_N=val;
      else if (val) f << "g" << i << "=" << val << ",";
    }
    m_link.m_fileType[1]=(long) input->readULong(2);
    m_link.m_fieldSize=(int) input->readULong(2);

    std::vector<int> listIds;
    long actPos=input->tell();
    if (!RagTime5StructManager::readDataIdList(input, 2, listIds)) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::PipelineCParser::parseZone: can not read the first list id\n"));
      f << "##listIds,";
      input->seek(actPos, librevenge::RVNG_SEEK_SET);
    }
    else {
      if (listIds[0]) {
        m_link.m_ids.push_back(listIds[0]);
        m_cluster->m_dataLink=m_link;
        f << "data1=data" << listIds[0] << "A,";
      }
      if (listIds[1]) {
        m_cluster->m_clusterIdsList.push_back(listIds[1]);
        f << "clusterId1=data" << listIds[1] << "A,";
      }
    }
    unsigned long ulVal=input->readULong(4);
    if (ulVal) {
      f << "h0=" << (ulVal&0x7FFFFFFF);
      if (ulVal&0x80000000) f << "[h],";
      else f << ",";
    }
    val=(int) input->readLong(2); // always 1?
    if (val!=1) f << "h1=" << val << ",";
    listIds.clear();
    if (!RagTime5StructManager::readDataIdList(input, 2, listIds)) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::PipelineCParser::parseZone: can not read the cluster list id\n"));
      f << "##listClusterIds,";
      return true;
    }
    if (listIds[0]) { // find some 4104 cluster
      m_cluster->m_clusterIdsList.push_back(listIds[0]);
      f << "cluster[nextId]=data" << listIds[0] << "A,";
    }
    if (listIds[1]) { // find some 4001 cluster
      m_cluster->m_clusterIdsList.push_back(listIds[1]);
      f << "cluster[id]=data" << listIds[1] << "A,";
    }
    val=(int) input->readULong(2); // 2[08a][01]
    f << "fl=" << std::hex << val << std::dec << ",";
    for (int i=0; i<2; ++i) { // h2=0|4|a, h3=small number
      val=(int) input->readLong(2);
      if (val) f << "h" << i+2 << "=" << val << ",";
    }
    if (fSz==76) return true;

    for (int i=0; i<7; ++i) { // g1, g2, g3 small int other 0
      val=(int) input->readLong(i==0 ? 2 : 4);
      if (i==2)
        m_link.m_N=val;
      else if (val) f << "g" << i << "=" << val << ",";
    }
    m_link.m_fileType[1]=(long) input->readULong(2);
    m_link.m_fieldSize=(int) input->readULong(2);

    listIds.clear();
    if (!RagTime5StructManager::readDataIdList(input, 1, listIds)) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::PipelineCParser::parseZone: can not read the second list id\n"));
      f << "##listIds2,";
      return true;
    }
    if (listIds[0]) {
      m_link.m_ids.clear();
      m_link.m_ids.push_back(listIds[0]);
      f << "data2=data" << listIds[0] << "A,";
      m_cluster->m_linksList.push_back(m_link);
    }
    return true;
  }
protected:
  //! the current cluster
  shared_ptr<RagTime5ZoneManager::Cluster> m_cluster;
};


//
//! try to read a root cluster: 4001
//
struct RootCParser : public RagTime5ZoneManager::ClusterParser {
  //! constructor
  RootCParser() : ClusterParser(0, "ClustRoot"), m_cluster(new RagTime5ZoneManager::ClusterRoot),
    m_what(-1), m_expectedId(-1), m_linkId(-1), m_fieldName("")
  {
    m_cluster->m_type=RagTime5ZoneManager::Cluster::C_Root;
  }
  //! return the current cluster
  shared_ptr<RagTime5ZoneManager::Cluster> getCluster()
  {
    return m_cluster;
  }
  //! end of a start zone call
  void endZone()
  {
    if (m_link.empty())
      return;
    if (m_dataId==0) {
      if (m_cluster->m_dataLink.empty())
        m_cluster->m_dataLink=m_link;
      else {
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::endZone: oops the main link is already set\n"));
        m_cluster->m_linksList.push_back(m_link);
      }
    }
    else if (m_what==3)
      m_cluster->m_graphicTypeLink=m_link;
    else if (m_linkId==0)
      m_cluster->m_listClusterName=m_link;
    else if (m_linkId==2)
      m_cluster->m_linkUnknown=m_link;
    else if (m_linkId==1)
      m_cluster->m_docInfoLink=m_link;
    else if (m_linkId==3)
      m_cluster->m_settingLinks.push_back(m_link);
    else if (m_linkId==4)
      m_cluster->m_conditionFormulaLinks.push_back(m_link);
    else if (m_linkId==5)
      m_cluster->m_listClusterUnkn=m_link;
    else
      m_cluster->m_linksList.push_back(m_link);
  }

  //! parse the header zone
  bool parseZone(MWAWInputStreamPtr &input, long fSz, int N, int flag, libmwaw::DebugStream &f)
  {
    m_what=m_linkId=-1;
    m_fieldName="";
    ++m_expectedId;
    if (m_dataId==0)
      return parseHeaderZone(input, fSz, N, flag, f);
    if (isANameHeader(N)) {
      if (m_expectedId<5 || m_expectedId >7)  {
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseZone: expected n seems bad\n"));
        f << "#n=" << m_expectedId << ",";
        m_expectedId=7;
      }
      f << "fileName,";
      m_fieldName="filename";
      m_what=1;
      return true;
    }
    if (N<0) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseZone: expected N value\n"));
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
    case 0: // main
      if (field.m_type==RagTime5StructManager::Field::T_ZoneId && field.m_fileType==0x14510b7) {
        if (field.m_longValue[0]) {
          m_cluster->m_styleClusterIds[7]=(int) field.m_longValue[0];
          f << "col/pattern[id]=dataA" << field.m_longValue[0] << ",";
        }
        break;
      }
      if (field.m_type==RagTime5StructManager::Field::T_Long && field.m_fileType==0x3c057) {
        // small number between 8 and 10
        if (field.m_longValue[0])
          f << "unkn0=" << field.m_longValue[0] << ",";
        break;
      }
      if (field.m_type==RagTime5StructManager::Field::T_FieldList && field.m_fileType==0x1451025) {
        f << "decal=[";
        for (size_t i=0; i<field.m_fieldList.size(); ++i) {
          RagTime5StructManager::Field const &child=field.m_fieldList[i];
          if (child.m_type==RagTime5StructManager::Field::T_Unstructured && child.m_fileType==0xce017) {
            // can be very long, seems to contain more 0 than 1
            f << "unkn1="<<child.m_extra << ",";
            continue;
          }
          MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseField: find unexpected decal child[main]\n"));
          f << "###[" << child << "],";
        }
        f << "],";
        break;
      }
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseField: find unexpected child[main]\n"));
      f << "###" << field << ",";
      break;
    case 1: // filename
      if (field.m_type==RagTime5StructManager::Field::T_Unicode && field.m_fileType==0xc8042) {
        m_cluster->m_fileName=field.m_string.cstr();
        f << field.m_string.cstr();
        break;
      }
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseField: find unexpected filename field\n"));
      f << "###" << field << ",";
      break;
    case 2: // list
      if (field.m_type==RagTime5StructManager::Field::T_LongList && field.m_fileType==0xce842) {
        f << "pos=[";
        for (size_t i=0; i<field.m_longList.size(); ++i) {
          if (field.m_longList[i]==0)
            f << "_,";
          else if (field.m_longList[i]>1000)
            f << std::hex << field.m_longList[i] << std::dec << ",";
          else
            f << field.m_longList[i] << ",";
        }
        f << "],";
        m_link.m_longList=field.m_longList;
        break;
      }
      if (field.m_type==RagTime5StructManager::Field::T_Unstructured && field.m_fileType==0xce017) {
        // a small value 2|4|a|1c|40
        f << "unkn="<<field.m_extra << ",";
        break;
      }
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseField: find unexpected list link field\n"));
      f << "###" << field << ",";
      break;
    case 3: // graph type
      if (field.m_type==RagTime5StructManager::Field::T_FieldList && field.m_fileType==0x14eb015) {
        f << "decal=[";
        for (size_t i=0; i<field.m_fieldList.size(); ++i) {
          RagTime5StructManager::Field const &child=field.m_fieldList[i];
          if (child.m_type==RagTime5StructManager::Field::T_LongList && child.m_fileType==0xce842) {
            for (size_t j=0; j<child.m_longList.size(); ++j) {
              if (child.m_longList[j]==0)
                f << "_,";
              else if (child.m_longList[j]>1000)
                f << std::hex << child.m_longList[j] << std::dec << ",";
              else
                f << child.m_longList[j] << ",";
            }
            m_link.m_longList=child.m_longList;
            continue;
          }
          MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseField: find unexpected decal child[graphType]\n"));
          f << "###[" << child << "],";
        }
        f << "],";
        break;
      }
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseField: find unexpected graph type field\n"));
      f << "###" << field << ",";
      break;
    default:
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseField: find unexpected field\n"));
      f << "###" << field << ",";
      break;
    }
    return true;
  }
protected:
  //! parse a data block
  bool parseDataZone(MWAWInputStreamPtr &input, long fSz, int N, int flag, libmwaw::DebugStream &f)
  {
    f << "header, fl=" << std::hex << flag << std::dec << ",";
    int val;
    long linkValues[5];
    m_link.m_N=N;
    switch (fSz) {
    case 26:  // in general block8 or 9 no auxiallary data
      if (m_expectedId<8) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseZone: expected n seems bad\n"));
        f << "###n=" << m_expectedId << ",";
        m_expectedId=8;
      }
      m_fieldName="block1a";
      val=(int) input->readULong(4); // small number or 0
      if (val) f << "N2=" << val << ",";
      m_link.m_fileType[0]=(int) input->readULong(4);
      if (m_link.m_fileType[0]!=0x14b4042) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseDataZone: unexpected type for block1a\n"));
        f << "##fileType=" << std::hex << m_link.m_fileType[0] << std::dec << ",";
      }
      for (int i=0; i<6; ++i) { // f4=c
        val=(int) input->readLong(2);
        if (val) f << "f" << i << "=" << val << ",";
      }
      break;
    case 28: //expectedN==2 or near 9 or 10...
      readFixedSizeListHeader(input, 0, false, m_link, linkValues, f);
      if (m_link.m_fileType[0]==0x35800) {
        f << "zone[longs],";
        m_fieldName="zone:longs";
      }
      else if (m_link.m_fileType[0]==0x3e800) {
        if (m_expectedId<2) {
          MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseDataZone: unexpected expectedN for list[long]\n"));
          f << "##expectedN=" << m_expectedId << ",";
          m_expectedId=2;
        }
        if (m_expectedId)
          m_linkId=5;
        f << "list[long0],";
        m_fieldName="list:longs0";
      }
      else {
        f << "###fType=" << std::hex << m_link.m_fileType[0] << std::dec << ",";
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseDataZone: the field fSz=28 seems bad\n"));
      }
      f << m_link << ",";
      m_link.m_fieldSize=4;
      m_link.m_type=RagTime5ZoneManager::Link::L_LongList;
      m_link.m_name="LongListRoot";
      m_what=2;
      break;
    case 30: {
      input->seek(4, librevenge::RVNG_SEEK_CUR);
      int typ=(int) input->readULong(4);
      if (typ==0x15e5042) {
        // first near n=9, second near n=15 with no other data
        // no auxilliar data expected
        input->seek(-8, librevenge::RVNG_SEEK_CUR);
        val=(int) input->readLong(4);
        if (val) f << "N2=" << val << ",";
        input->seek(4, librevenge::RVNG_SEEK_CUR); // seek id
        m_fieldName="unknDataD";
        for (int i=0; i<4; ++i) { // f0, f3: small number
          val=(int) input->readULong(4);
          if (val) f << "f" << i << "=" << val << ",";
        }
        break;
      }
      if (typ) {
        f << "###fType0=" << std::hex << typ << std::dec << ",";
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseDataZone: the field fSz=30 seems bad\n"));
        break;
      }
      // field3 and other near 10
      input->seek(-8, librevenge::RVNG_SEEK_CUR);
      readFixedSizeListHeader(input, -1, true, m_link, linkValues, f);
      if (m_link.m_ids.empty() || !m_link.m_ids[0]) {
        f << "#noCluster,";
        break;
      }
      f << m_link << ",";
      if (m_expectedId<=3) {
        if (m_expectedId!=3) {
          MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseDataZone: bad expected N for cluster list id\n"));
          f << "##expectedN=" << m_expectedId << ",";
          m_expectedId=3;
        }
        if ((m_link.m_fileType[1]&0xFFD7)!=0x40 || m_link.m_fieldSize!=8) {
          MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseDataZone: find odd definition for cluster list id\n"));
          f << "##[" << std::hex << m_link.m_fileType[1] << std::dec << ":" << m_link.m_fieldSize << "],";
        }
        m_cluster->m_listClusterId=m_link.m_ids[0];
        m_fieldName="clusterList";
        m_link=RagTime5ZoneManager::Link();
        break;
      }
      if ((m_link.m_fileType[1]&0xFFD7)!=0 || m_link.m_fieldSize!=4) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseDataZone: find odd definition for cluster id and fSz=1e\n"));
        f << "##[" << std::hex << m_link.m_fileType[1] << std::dec << ":" << m_link.m_fieldSize << "],";
      }
      m_fieldName="unknDataC";
      break;
    }
    case 32: // expectedN=1|(5|6)| after 8
      m_what=2;
      m_link.m_N=N;
      readListHeader(input, m_dataId==1 ? 0x200 : -1, m_link, linkValues, f);
      if (m_dataId==1) {
        m_fieldName="zone:names";
        m_linkId=0;
        if (linkValues[0]!=0x7d01a) {
          MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseDataZone: unexpected type for zone[name]\n"));
          f << "##fileType=" << std::hex << m_link.m_fileType[0] << std::dec << ",";
        }
        break;
      }
      if ((m_link.m_fileType[1]&0xFFD7)==0x8010) {
        m_linkId=1;
        m_expectedId=5;
        m_link.m_name=m_fieldName="docInfo";
      }
      else if ((m_link.m_fileType[1]&0xFFD7)==0xc010) {
        m_linkId=2;
        m_expectedId=6;
        m_link.m_name=m_fieldName="unknRootField6";
      }
      else if (m_link.m_fileType[0]==0x47040 && (m_link.m_fileType[1]&0xFFD7)==0) {
        m_linkId=3;
        m_link.m_name=m_fieldName="settings";
      }
      else if ((m_link.m_fileType[1]&0xFFD7)==0x10) {
        m_linkId=4;
        m_link.m_name=m_fieldName="condFormula";
      }
      else if ((m_link.m_fileType[1]&0xFFD7)==0x310) {
        // TODO: check what is it: an unicode list ?
        m_link.m_name=m_fieldName="unknDataE";
      }
      else {
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseDataZone: unexpected type1 for zone with fSz=32\n"));
        f << "##fileType1=" << std::hex << m_link.m_fileType[1] << std::dec << ",";
      }
      break;
    case 38:
      m_fieldName="field4";
      val=(int) input->readULong(4);
      if (m_expectedId>4 || val!=0x47040) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseDataZone: unexpected data of size 38\n"));
        f << "##fileType=" << std::hex << val << std::dec << ",";
        break;
      }
      if (m_expectedId!=4) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseDataZone: bad expected N for field 4\n"));
        f << "##expectedN=" << m_expectedId << ",";
        m_expectedId=4;
      }
      for (int i=0; i<6; ++i) { // always 0
        val=(int) input->readLong(2);
        if (val) f << "f" << i << "=" << val << ",";
      }
      val=(int) input->readULong(2);
      if ((val&0xFFD7)!=0x10) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseDataZone: unexpected fileType1 for field 4\n"));
        f << "##fileType1=" << std::hex << val << std::dec << ",";
      }
      f << "val=[";
      for (int i=0; i<3; ++i) { // small int, often with f1=f0+1, f2=f1+1
        val=(int) input->readULong(4);
        if (val)
          f << val << ",";
        else
          f << "_,";
      }
      f << "],";
      val=(int) input->readULong(2); // always 0?
      if (val) f << "f0=" << val << ",";
      break;
    case 52:
      m_what=3;
      m_fieldName="graphTypes";
      if (N!=1) f << "##N=" << N << ",";
      for (int i=0; i<2; ++i) { // always 0 and small val
        val=(int) input->readLong(2);
        if (val) f << "f" << i+1 << "=" << val << ",";
      }
      m_link.m_fileType[0]=(int) input->readLong(4);
      if (m_link.m_fileType[0]!=0x14e6042) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseDataZone[graph]: find unexpected fileType\n"));
        f << "###fileType=" << std::hex << m_link.m_fileType[0] << std::dec << ",";
      }
      for (int i=0; i<14; ++i) { // g1=0-2, g2=10[size?], g4=1-8[N], g13=30
        val=(int) input->readLong(2);
        if (val) f << "g" << i << "=" << val << ",";
      }
      if (RagTime5StructManager::readDataIdList(input, 2, m_link.m_ids) && m_link.m_ids[1]) {
        m_link.m_fileType[1]=0x30;
        m_link.m_fieldSize=16;
      }
      val=(int) input->readLong(2);
      if (val) // small number
        f << "h0=" << val << ",";
      break;
    case 78: {
      m_fieldName="fieldLists";
      if (N!=1) f << "##N=" << N << ",";
      val=(int) input->readULong(4);
      if (val) f << "id=" << val << ",";
      val=(int) input->readULong(4);
      if (val!=0x154a042) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseDataZone: find odd type for fSz=78\n"));
        f << "##[" << std::hex << val << std::dec << ":" << m_link.m_fieldSize << "],";
      }
      for (int i=0; i<2; ++i) { // always 0
        val=(int) input->readULong(2);
        if (val)
          f << "f" << i << "=" << val << ",";
      }
      std::vector<int> listIds;
      long actPos=input->tell();
      if (!RagTime5StructManager::readDataIdList(input, 2, listIds)) {
        f << "###fieldId,";
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseDataZone: can not read field ids\n"));
        input->seek(actPos+8, librevenge::RVNG_SEEK_SET);
      }
      else if (listIds[0] || listIds[1]) { // fielddef and fieldpos
        RagTime5ZoneManager::Link fieldLink;
        fieldLink.m_type= RagTime5ZoneManager::Link::L_ClusterLink;
        fieldLink.m_ids=listIds;
        m_cluster->m_fieldClusterLink=fieldLink;
        f << "fields," << fieldLink << ",";
      }
      val=(int) input->readULong(4);
      if (val) f << "id2=" << val << ",";
      for (int i=0; i<4; ++i) { // always 0
        val=(int) input->readULong(2);
        if (val)
          f << "f" << i+2 << "=" << val << ",";
      }
      for (int i=0; i<2; ++i) { // always 1,0
        val=(int) input->readULong(1);
        if (val!=1-i)
          f << "fl" << i << "=" << val << ",";
      }
      val=(int) input->readLong(2);
      if (val!=100)
        f << "f6=" << val << ",";
      f << "ID=[";
      for (int i=0; i<4; ++i) { // very big number
        val=(int) input->readULong(4);
        if (val)
          f << std::hex << val << std::dec << ",";
        else
          f << "_,";
      }
      f << "],";
      listIds.clear();
      actPos=input->tell();
      if (!RagTime5StructManager::readDataIdList(input, 4, listIds)) {
        f << "###clusterCId,";
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseDataZone: can not read clusterC ids\n"));
        input->seek(actPos+16, librevenge::RVNG_SEEK_SET);
      }
      else if (listIds[0] || listIds[1] || listIds[2] || listIds[3]) {
        RagTime5ZoneManager::Link fieldLink;
        fieldLink.m_type= RagTime5ZoneManager::Link::L_UnknownClusterC;
        fieldLink.m_ids=listIds;
        m_cluster->m_linksList.push_back(fieldLink);
        f << fieldLink << ",";
      }

      val=(int) input->readULong(4);
      if (val) f << "id3=" << val << ",";
      break;
    }
    default:
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseDataZone: find unexpected data field\n"));
      f << "###N=" << N << ",fSz=" << fSz << ",";
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
    if (N!=-2 || m_dataId!=0 || (fSz!=215 && fSz!=220)) {
      f << "###N=" << N << ",fSz=" << fSz << ",";
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseHeaderZone: find unexpected main field\n"));
      return true;
    }
    m_what=0;
    int val=(int) input->readLong(4); // 8|9|a
    f << "f1=" << val << ",";
    for (int i=0; i<4; ++i) { // f2=0-7, f3=1|3
      val=(int) input->readLong(2);
      if (val) f << "f" << i+2 << "=" << val << ",";
    }
    val=(int) input->readLong(4); // 7|8
    f << "N1=" << val << ",";
    std::vector<int> listIds;
    long actPos=input->tell();
    if (!RagTime5StructManager::readDataIdList(input, 1, listIds) || !listIds[0]) {
      f << "###cluster[child],";
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseHeaderZone: can not find the cluster's child\n"));
      input->seek(actPos+4, librevenge::RVNG_SEEK_SET);
    }
    else { // link to unknown cluster zone
      m_cluster->m_clusterIds[0]=listIds[0];
      f << "unknClustB=data" << listIds[0] << "A,";
    }
    for (int i=0; i<21; ++i) { // always g0=g11=g18=16, other 0 ?
      val=(int) input->readLong(2);
      if (val) f << "g" << i << "=" << val << ",";
    }
    val=(int) input->readULong(4);
    if (val!=0x3c052)
      f << "#fileType=" << std::hex << val << std::dec << ",";
    for (int i=0; i<9; ++i) { // always h6=6
      val=(int) input->readLong(2);
      if (val) f << "h" << i << "=" << val << ",";
    }
    for (int i=0; i<3; ++i) { // can be 1,11,10
      val=(int) input->readULong(1);
      if (val)
        f << "fl" << i << "=" << std::hex << val << std::dec << ",";
    }
    if (fSz==220) {
      for (int i=0; i<2; ++i) { // h10=1, h11=16
        val=(int) input->readLong(2);
        if (val) f << "h" << i+9 << "=" << val << ",";
      }
      val=(int) input->readLong(1);
      if (val) f << "h11=" << val << ",";
    }
    val=(int) input->readLong(4); // e-5a
    if (val) f << "N2=" << val << ",";
    for (int i=0; i<9; ++i) { // j8=18
      val=(int) input->readLong(2);
      if (val) f << "j" << i << "=" << val << ",";
    }
    for (int i=0; i<3; ++i) {
      val=(int) input->readLong(4);
      if (val!=i+2)
        f << "j" << 9+i << "=" << val << ",";
    }
    actPos=input->tell();
    listIds.clear();
    if (!RagTime5StructManager::readDataIdList(input, 4, listIds)) {
      f << "###style[child],";
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseHeaderZone: can not find the style's child\n"));
      input->seek(actPos+16, librevenge::RVNG_SEEK_SET);
    }
    else {
      for (size_t i=0; i<4; ++i) {
        if (listIds[i]==0) continue;
        m_cluster->m_styleClusterIds[i]=listIds[i];
        static char const *(wh[])= { "graph", "units", "units2", "text" };
        f << wh[i] << "Style=data" << listIds[i] << "A,";
      }
    }
    val=(int) input->readLong(4); // always 5?
    if (val!=80) f << "N3=" << val << ",";
    actPos=input->tell();
    listIds.clear();
    if (!RagTime5StructManager::readDataIdList(input, 3, listIds)) {
      f << "###style[child],";
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootCParser::parseHeaderZone: can not find the style2's child\n"));
      input->seek(actPos+12, librevenge::RVNG_SEEK_SET);
    }
    else {
      for (size_t i=0; i<3; ++i) {
        if (listIds[i]==0) continue;
        m_cluster->m_styleClusterIds[i+4]=listIds[i];
        static char const *(wh[])= { "format", "#unk", "graphColor" };
        f << wh[i] << "Style=data" << listIds[i] << "A,";
      }
    }
    for (int i=0; i<9; ++i) { // k6=0|6, k7=0|7
      val=(int) input->readULong(4); // maybe some dim
      static int const(expected[])= {0xc000, 0x2665, 0xc000, 0x2665, 0xc000, 0xc000, 0, 0, 0};
      if (val!=expected[i])
        f << "k" << i << "=" << std::hex << val << std::dec << ",";
    }
    for (int i=0; i<2; ++i) { // l0=0|1|2, l1=0|1
      val=(int) input->readLong(2); // 0|1|2
      if (val)
        f << "l" << i <<"=" << val << ",";
    }
    // a very big number
    f << "ID=" << std::hex << input->readULong(4) << std::dec << ",";
    for (int i=0; i<6; ++i) { // always 0
      val=(int) input->readLong(2); // 0|1|2
      if (val)
        f << "l" << i+2 <<"=" << val << ",";
    }

    return true;
  }
  //! the current cluster
  shared_ptr<RagTime5ZoneManager::ClusterRoot> m_cluster;
  //! a index to know which field is parsed :  0: main, 1: filename, 2: list, 3: graph type
  int m_what;
  //! a index to known which field is expected : 0:data ... 7: filename, ...
  int m_expectedId;
  //! the link id : 0: zone[names], 1: field5=doc[info]?, 2: field6, 3: settings, 4: formula, 5: cluster[list]
  int m_linkId;
  //! the actual field name
  std::string m_fieldName;
};

//
//! try to read a basic root child cluster: either fielddef or fieldpos or a first internal child of the root (unknown) or another child
//
struct RootChildCParser : public RagTime5ZoneManager::ClusterParser {
  //! constructor
  RootChildCParser(int type) : ClusterParser(type, "ClustCRoot_BAD"), m_cluster(new RagTime5ZoneManager::Cluster)
  {
    switch (type) {
    case 0x10000:
      m_name="ClustUnkB";
      m_cluster->m_type=RagTime5ZoneManager::Cluster::C_ClusterB;
      break;
    case 0x20000:
      m_name="ClustField_Def";
      m_cluster->m_type=RagTime5ZoneManager::Cluster::C_Fields;
      break;
    case 0x20001:
      m_name="ClustField_Pos";
      m_cluster->m_type=RagTime5ZoneManager::Cluster::C_Fields;
      break;
    case 0x30000:
      m_name="ClustUnkC_A";
      m_cluster->m_type=RagTime5ZoneManager::Cluster::C_ClusterC;
      break;
    case 0x30001:
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootChildCParser::RootChildCParser: find zone ClustUnkC_B\n"));
      m_name="ClustUnkC_B";
      m_cluster->m_type=RagTime5ZoneManager::Cluster::C_ClusterC;
      break;
    case 0x30002:
      m_name="ClustUnkC_C";
      m_cluster->m_type=RagTime5ZoneManager::Cluster::C_ClusterC;
      break;
    case 0x30003:
      m_name="ClustUnkC_D";
      m_cluster->m_type=RagTime5ZoneManager::Cluster::C_ClusterC;
      break;
    default:
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootChildCParser::RootChildCParser: find unknown type\n"));
      break;
    }
  }
  //! return the current cluster
  shared_ptr<RagTime5ZoneManager::Cluster> getCluster()
  {
    return m_cluster;
  }
  //! parse a zone
  bool parseZone(MWAWInputStreamPtr &input, long fSz, int N, int flag, libmwaw::DebugStream &f)
  {
    if ((m_dataId==0 && flag!=0x30) || (m_dataId==1 && flag !=0x30))
      f << "fl=" << std::hex << flag << std::dec << ",";
    bool ok=false, isList=true;
    int expectedFileType1=0;
    switch (m_type) {
    case 0x10000:
      ok=m_dataId==0 && fSz==32;
      expectedFileType1=0x4010;
      break;
    case 0x20000:
      ok=m_dataId==0 && fSz==41;
      expectedFileType1=0x1010;
      break;
    case 0x20001:
      ok=m_dataId==0 && fSz==32;
      expectedFileType1=0x1010;
      break;
    case 0x30000:
      ok=m_dataId==0 && fSz==34;
      expectedFileType1=0x50;
      isList=false;
      break;
    case 0x30002:
      if (m_dataId==0 && fSz==40) {
        ok=true;
        expectedFileType1=0x8010;
      }
      else if (m_dataId==1 && fSz==30) {
        ok=true;
        expectedFileType1=0x50;
        isList=false;
      }
      break;
    case 0x30003:
      ok=m_dataId==0 && fSz==32;
      expectedFileType1=0x310;
      break;
    default:
      break;
    }
    if (N<=0 || !ok) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootChildCParser::parseZone: find unexpected header\n"));
      f << "###type" << std::hex << N << std::dec;
      return true;
    }

    int val;
    m_link.m_N=N;
    long linkValues[5]; // for type=0x30002, f0=3c|60, for fixed size f0=54, other 0
    if (!isList) {
      if (readFixedSizeListHeader(input, expectedFileType1, true, m_link, linkValues, f)) {
        m_link.m_fileType[0]=m_type < 0x30000 ? m_type : m_type-0x30000;
        f << m_link << ",";
      }
      if (fSz==30) return true;
      for (int i=0; i<2; ++i) { // find 0
        val=(int) input->readLong(2);
        if (val) f << "g" << i << "=" << val << ",";
      }
      return true;
    }

    if (readListHeader(input, expectedFileType1, m_link, linkValues, f)) {
      m_link.m_fileType[0]=m_type < 0x30000 ? m_type : m_type-0x30000;
      f << m_link << ",";
    }

    if (fSz==32) return true;

    if (m_type==0x20000) {
      std::vector<int> listIds;
      bool hasCluster=false;
      if (RagTime5StructManager::readDataIdList(input, 1, listIds) && listIds[0]) {
        m_cluster->m_clusterIdsList.push_back(listIds[0]);
        f << "clusterId1=data" << listIds[0] << "A,";
        hasCluster=true;
      }
      val=(int) input->readLong(1);
      if ((hasCluster && val!=1) || (!hasCluster && val))
        f << "#hasCluster=" << val << ",";
      for (int i=0; i<2; ++i) { // always 0
        val=(int) input->readLong(2);
        if (val) f << "g" << i << "=" << val << ",";
      }
    }
    else if (m_type==0x30002) {
      for (int i=0; i<2; ++i) { // find 0
        val=(int) input->readLong(4);
        if (val) f << "g" << i << "=" << val << ",";
      }
    }
    return true;
  }
  //! parse a field
  bool parseField(RagTime5StructManager::Field const &field, int /*m*/, libmwaw::DebugStream &f)
  {
    if (m_dataId==0 && field.m_type==RagTime5StructManager::Field::T_LongList && field.m_fileType==0xce842) {
      f << "pos=[";
      for (size_t i=0; i<field.m_longList.size(); ++i)
        f << field.m_longList[i] << ",";
      f << "],";
      m_link.m_longList=field.m_longList;
    }
    else if (m_dataId==0 && field.m_type==RagTime5StructManager::Field::T_Unstructured && field.m_fileType==0xce017)
      // pos find 2|4|8
      // def find f801|000f00
      f << "unkn="<<field.m_extra << ",";
    else {
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::RootChildCParser::parseField: find unexpected sub field\n"));
      f << "#" << field;
    }
    return true;
  }
  //! end of a start zone call
  void endZone()
  {
    if (m_link.empty())
      return;
    if (m_dataId==0)
      m_cluster->m_dataLink=m_link;
    else
      m_cluster->m_linksList.push_back(m_link);
  }
protected:
  //! the current cluster
  shared_ptr<RagTime5ZoneManager::Cluster> m_cluster;
};

//
//! low level: parser of script cluster : zone 2,a,4002,400a
//
struct ScriptCParser : public RagTime5ZoneManager::ClusterParser {
  //! constructor
  ScriptCParser(int type) : ClusterParser(type, "ClustScript"), m_cluster(new RagTime5ZoneManager::ClusterScript), m_what(-1), m_fieldName("")
  {
    m_cluster->m_type=RagTime5ZoneManager::Cluster::C_Script;
  }
  //! return the current cluster
  shared_ptr<RagTime5ZoneManager::Cluster> getCluster()
  {
    return m_cluster;
  }

  //! parse a zone
  bool parseZone(MWAWInputStreamPtr &input, long fSz, int N, int flag, libmwaw::DebugStream &f)
  {
    f << "fl=" << std::hex << flag << std::dec << ",";
    m_what=-1;
    m_fieldName="";
    if (isANameHeader(N)) {
      if (m_dataId!=1)  {
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::ScriptCParser::parseZone: expected n seems bad\n"));
        f << "#n=" << m_dataId << ",";
      }
      f << "scriptName,";
      m_fieldName="script:name";
      m_what=1;
      return true;
    }

    int val;
    if (N==-5) {
      if (m_dataId!=0 || (fSz!=38 && fSz!=74)) {
        f << "###main,";
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::ScriptCParser::parseZone: can not read the main data\n"));
        return true;
      }
      m_what=0;
      m_fieldName="main";
      for (int i=0; i<2; ++i) { // always 0?
        val=(int) input->readLong(2);
        if (val) f << "f" << i+1 << "=" << val << ",";
      }
      val=(int) input->readLong(2);
      f << "id=" << val << ",";
      val=(int) input->readULong(2);
      if (val!=m_type) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::ScriptCParser::parseZone: unexpected zone type[graph]\n"));
        f << "##zoneType=" << std::hex << val << std::dec << ",";
      }
      if (fSz==38) { // find in zone a ( with a next data field which is a link to a cluster )
        // probably in relation with zone fSz=60
        val=(int) input->readLong(4); // find with 2, probably a number
        if (val) f << "f0=" << val << ",";
        for (int i=0; i<6; ++i) { // always 0
          val=(int) input->readLong(2);
          if (val) f << "f" << i+1 << "=" << val << ",";
        }
        std::string code(""); // find betr
        for (int i=0; i<4; ++i) code+=(char) input->readULong(1);
        if (!code.empty()) f << code << ",";
        for (int i=0; i<2; ++i) { // always 0
          val=(int) input->readLong(2);
          if (val) f << "g" << i << "=" << val << ",";
        }
        return true;
      }
      val=(int) input->readLong(4); // find with 0|3|4, probably a number
      if (val) f << "f0=" << val << ",";
      for (int i=0; i<10; ++i) { // f4=2|8, f6=0|1|2|5, f7=1-5, f8=3, f10=0-4
        val=(int) input->readLong(2);
        if (val) f << "f" << i+1 << "=" << val << ",";
      }
      val=(int) input->readLong(4);  // find 0|93037 maybe a type
      if (val) f << "fileType=" << std::hex << val << std::dec << ",";
      val=(int) input->readLong(2); // always 0
      if (val) f << "f11=" << val << ",";
      val=(int) input->readLong(2);
      if (val!=0 && val!=2) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::ScriptCParser::parseZone: the unicode field size seems bad\n"));
        f << "#fieldSize=" << val << ",";
      }
      long actPos=input->tell();
      std::vector<int> listIds;
      if (!RagTime5StructManager::readDataIdList(input, 1, listIds)) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::ScriptCParser::parseZone: can not find the unicode string data\n"));
        f << "##noData,";
        input->seek(actPos+2, librevenge::RVNG_SEEK_SET);
      }
      else if (listIds[0]) {
        // find a script comment
        RagTime5ZoneManager::Link scriptLink;
        scriptLink.m_type=RagTime5ZoneManager::Link::L_List;
        scriptLink.m_name="scriptComment";
        scriptLink.m_ids.push_back(listIds[0]);
        m_cluster->m_scriptComment=scriptLink;
        f << scriptLink << ",";
      }
      for (int i=0; i<10; ++i) { // g0=1-8d, g2=2|3, g3=1f40, g5=0-26, g9=0|30
        val=(int) input->readLong(2);
        if (i==3) {
          if (val==0x1f40) continue;
          f << "#fileType1=" << std::hex << val << std::dec << ",";
        }
        else if (val)
          f << "g" << i << "=" << val << ",";
      }
      std::string code(""); // find cent, left
      for (int i=0; i<4; ++i) code+=(char) input->readULong(1);
      if (!code.empty()) f << "align=" << code << ",";
      return true;
    }

    if (N<0 || m_dataId==0 || (fSz!=32 && fSz!=36)) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::ScriptCParser::parseZone: find unexpected data block\n"));
      f << "###N=" << N << ",";
      return true;
    }

    m_link.m_N=N;
    long linkValues[5];
    readListHeader(input, fSz==32 ? 0x600 : 0x10, m_link, linkValues, f);
    if (fSz==32) {
      m_what=2;
      m_fieldName="unicodeList";
      m_link.m_type=RagTime5ZoneManager::Link::L_UnicodeList;
    }
    else {
      m_what=3;
      m_fieldName="scriptLink";
      m_link.m_name="scriptDataA";
    }
    f << m_link << ",";
    if (fSz==32) return true;

    for (int i=0; i<2; ++i) { // g0: small number between 38 and 64, g1: 0|-1
      val=(int) input->readLong(2);
      if (val) f << "g" << i << "=" << val << ",";
    }
    return true;
  }
  //! parse a field
  bool parseField(RagTime5StructManager::Field const &field, int /*m*/, libmwaw::DebugStream &f)
  {
    if (!m_fieldName.empty())
      f << m_fieldName << ",";
    switch (m_what) {
    case 1:
      if (field.m_type==RagTime5StructManager::Field::T_Unicode && field.m_fileType==0xc8042) {
        m_cluster->m_scriptName=field.m_string.cstr();
        f << field.m_string.cstr();
        break;
      }
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::ScriptCParser::parseField: find unexpected script field\n"));
      f << "###" << field;
      break;
    case 2:
    case 3: {
      if (field.m_type==RagTime5StructManager::Field::T_LongList && field.m_fileType==0xce842) {
        f << "pos=[";
        for (size_t i=0; i<field.m_longList.size(); ++i)
          f << field.m_longList[i] << ",";
        f << "],";
        m_link.m_longList=field.m_longList;
        break;
      }
      if (field.m_type==RagTime5StructManager::Field::T_Unstructured && field.m_fileType==0xce017) {
        // a small value 2 (can be first data)
        f << "unkn="<<field.m_extra << ",";
        break;
      }
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::ScriptCParser::parseField: find unexpected list link field\n"));
      f << "###" << field;
      break;
    }
    default:
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::ScriptCParser::parseField: find unexpected field\n"));
      f << "###" << field;
      break;
    }
    return true;
  }
  //! end of a start zone call
  void endZone()
  {
    if (m_link.empty())
      return;
    if (m_what==2) {
      if (m_cluster->m_nameLink.empty())
        m_cluster->m_nameLink=m_link;
      else {
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::ScriptCParser::endZone: oops the name link is already set\n"));
        m_cluster->m_linksList.push_back(m_link);
      }
    }
    else if (m_what==3)
      m_cluster->m_dataLink=m_link;
    else
      m_cluster->m_linksList.push_back(m_link);
  }

protected:
  //! the current cluster
  shared_ptr<RagTime5ZoneManager::ClusterScript> m_cluster;
  //! a index to know which field is parsed :  0: main, 1:scriptname, 2: list names, 3: list data
  int m_what;
  //! the actual field name
  std::string m_fieldName;
};

//
//! low level: parser of script cluster : zone 480
//
struct StyleCParser : public RagTime5ZoneManager::ClusterParser {
  //! constructor
  StyleCParser() : ClusterParser(0x480, "ClustStyle"), m_cluster(new RagTime5ZoneManager::Cluster), m_what(-1), m_fieldName("")
  {
  }
  //! return the current cluster
  shared_ptr<RagTime5ZoneManager::Cluster> getCluster()
  {
    return m_cluster;
  }
  //! try to parse a zone
  bool parseZone(MWAWInputStreamPtr &input, long fSz, int N, int flag, libmwaw::DebugStream &f)
  {
    f << "fl=" << std::hex << flag << std::dec << ",";
    m_fieldName="";

    int val;
    m_what=-1;
    if (N!=-5) {
      if (N<0 || m_dataId==0 || (fSz!=28 && fSz!=32 && fSz!=36)) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::StyleCParser::parseZone: unexpected header\n"));
        f << "##N=" << N << ",";
        return true;
      }
      m_link.m_N=N;
      long linkValues[5];
      if (fSz==28) { // n=2,3 with fSz=28, type=0x3e800, can have no data
        if (m_dataId!=2 && m_dataId!=3) {
          MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::StyleCParser::parseZone: dataId seems bad\n"));
          f << "##n=" << m_dataId << ",";
        }
        readFixedSizeListHeader(input, 0, false, m_link, linkValues, f);
        if (m_link.m_fileType[0]==0x35800) {
          m_what=2;
          f << "zone[longs],";
          m_fieldName="zone:longs";
        }
        else if (m_link.m_fileType[0]==0x3e800) {
          m_what=3;
          f << "list[long0],";
          m_fieldName="list:longs0";
        }
        else {
          f << "###fType=" << std::hex << m_link.m_fileType[0] << std::dec << ",";
          MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::StyleCParser::parseZone: the field 2,3 type seems bad\n"));
          return true;
        }
        f << m_link << ",";
        m_link.m_fieldSize=4;
        m_link.m_type=RagTime5ZoneManager::Link::L_LongList;
        if (m_cluster->m_type!=RagTime5ZoneManager::Cluster::C_FormatStyles && !m_link.m_ids.empty() && m_link.m_ids[0]) {
          f << "###unexpected";
          MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::StyleCParser::parseZone: find link for not format data\n"));
        }
        return true;
      }
      if (fSz==32) {
        readListHeader(input, 0x200, m_link, linkValues, f);
        if (m_dataId!=4) {
          MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::StyleCParser::parseZone: dataId seems bad\n"));
          f << "##n=" << m_dataId << ",";
        }
        m_fieldName="unicodeList";
        m_link.m_type=RagTime5ZoneManager::Link::L_UnicodeList;
        f << m_link << ",";
        m_what=4;
        return true;
      }
      if (m_dataId!=1) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::StyleCParser::parseZone: dataId seems bad\n"));
        f << "##n=" << m_dataId << ",";
      }
      m_what=1;
      for (int i=0; i<2; ++i) { // always 0
        val=(int) input->readLong(2);
        if (val) f << "f" << i << "=" << val << ",";
      }
      m_link.m_fileType[0]=(int) input->readULong(4);
      if (m_link.m_fileType[0]!=0x7d01a) {
        f << "###fType=" << std::hex << m_link.m_fileType[0] << std::dec << ",";
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::StyleCParser::parseZone: the field 1 type seems bad\n"));
      }
      for (int i=0; i<4; ++i) { // always 0
        val=(int) input->readLong(2);
        if (val) f << "f" << i+2 << "=" << val << ",";
      }
      m_link.m_fileType[1]=(int) input->readULong(2);
      if (m_link.m_fileType[1]!=0x10 && m_link.m_fileType[1]!=0x18) {
        f << "###fType1=" << std::hex << m_link.m_fileType[1] << std::dec << ",";
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::StyleCParser::parseZone: the field 1 type1 seems bad\n"));
      }
      for (int i=0; i<3; ++i) { // always 3,4,5 ?
        val=(int) input->readLong(4);
        if (val!=i+3) f << "g" << i << "=" << val << ",";
      }
      return true;
    }

    if ((fSz!=58 && fSz!=64 && fSz!=66 && fSz!=68) || m_dataId!=0) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::StyleCParser::parseZone: find unknown block\n"));
      f << "###unknown,";
      return true;
    }

    m_what=0;
    for (int i=0; i<2; ++i) { // always 0?
      val=(int) input->readLong(2);
      if (val) f << "f" << i+1 << "=" << val << ",";
    }
    val=(int) input->readLong(2);
    f << "id=" << val << ",";
    val=(int) input->readULong(2);
    if (val!=0x480) {
      f << "###type=" << std::hex << val << std::dec << ",";
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::StyleCParser::parseZone: the field format seems bad\n"));
    }
    m_link.m_N=val;
    for (int i=0; i<13; ++i) { // g3=2, g4=10, g6 and g8 2 small int
      val=(int) input->readLong(2);
      if (val) f << "g" << i << "=" << val << ",";
    }
    m_link.m_fileType[0]=(int) input->readULong(4);
    if (m_link.m_fileType[0] != 0x01473857 && m_link.m_fileType[0] != 0x0146e827) {
      f << "###fileType=" << std::hex << m_link.m_fileType[0] << std::dec << ",";
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::StyleCParser::parseZone: the field type seems bad\n"));
    }
    m_link.m_fileType[1]=(int) input->readULong(2); // c018|c030|c038 or type ?
    if (!RagTime5StructManager::readDataIdList(input, 2, m_link.m_ids) || m_link.m_ids[1]==0) {
      f << "###noData,";
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::StyleCParser::parseZone: can not find any data\n"));
    }
    m_link.m_type=RagTime5ZoneManager::Link::L_FieldsList;
    if (fSz==58) {
      if (m_link.m_fileType[0] == 0x0146e827) {
        m_link.m_name=m_fieldName="formats";
        m_cluster->m_type=RagTime5ZoneManager::Cluster::C_FormatStyles;
      }
      else {
        m_link.m_name=m_fieldName="units";
        m_cluster->m_type=RagTime5ZoneManager::Cluster::C_UnitStyles;
      }
    }
    else if (fSz==64) {
      m_link.m_name=m_fieldName="graphColor";
      m_cluster->m_type=RagTime5ZoneManager::Cluster::C_ColorStyles;
    }
    else if (fSz==66) {
      m_link.m_name=m_fieldName="textStyle";
      m_cluster->m_type=RagTime5ZoneManager::Cluster::C_TextStyles;
    }
    else {
      m_link.m_name=m_fieldName="graphStyle";
      m_cluster->m_type=RagTime5ZoneManager::Cluster::C_GraphicStyles;
    }

    return true;
  }
  //! parse a field
  bool parseField(RagTime5StructManager::Field const &field, int /*m*/, libmwaw::DebugStream &f)
  {
    if (!m_fieldName.empty())
      f << m_fieldName << ",";
    switch (m_what) {
    case 0: {
      long expectedVal=m_cluster->m_type==RagTime5ZoneManager::Cluster::C_FormatStyles ? 0x146e815 : 0x1473815;
      if (field.m_type==RagTime5StructManager::Field::T_FieldList && field.m_fileType==expectedVal) {
        f << "decal=[";
        for (size_t i=0; i<field.m_fieldList.size(); ++i) {
          RagTime5StructManager::Field const &child=field.m_fieldList[i];
          if (child.m_type==RagTime5StructManager::Field::T_LongList && child.m_fileType==0xce842) {
            for (size_t j=0; j<child.m_longList.size(); ++j)
              f << child.m_longList[j] << ",";
            m_link.m_longList=child.m_longList;
            continue;
          }
          if (child.m_type==RagTime5StructManager::Field::T_Unstructured && child.m_fileType==0xce017) {
            // a list of small int 0104|0110|22f8ffff7f3f
            f << "unkn0=" << child.m_extra << ",";
            continue;
          }
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readStyleCluster: find unexpected child[main]\n"));
          f << "###[" << child << "],";
        }
        f << "],";
        break;
      }
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readStyleCluster: find unexpected field[main]\n"));
      f << "###" << field;
      break;
    }
    case 1:
      if (field.m_type==RagTime5StructManager::Field::T_Unstructured && field.m_fileType==0xce017) {
        // a small value 2 (can be first data)
        f << "unkn="<<field.m_extra << ",";
        break;
      }
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::StyleCParser::parseField: find unexpected field[zone1]\n"));
      f << "###" << field;
      break;
    case 2:
    case 3:
      if (field.m_type==RagTime5StructManager::Field::T_LongList && field.m_fileType==0xce842) {
        f << "data=[";
        for (size_t i=0; i<field.m_longList.size(); ++i) {
          if (field.m_longList[i]==0)
            f << "_,";
          else if ((int)field.m_longList[i]==(int) 0x80000000)
            f << "inf,";
          else
            f << field.m_longList[i] << ",";
        }
        f << "],";
        m_link.m_longList=field.m_longList;
        break;
      }
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::StyleCParser::parseField: find unexpected field[zone23–\n"));
      f << "###" << field;
      break;
    case 4:
      if (field.m_type==RagTime5StructManager::Field::T_LongList && field.m_fileType==0xce842) {
        f << "data=[";
        for (size_t i=0; i<field.m_longList.size(); ++i) {
          f << field.m_longList[i] << ",";
        }
        f << "],";
        m_link.m_longList=field.m_longList;
        break;
      }
      if (field.m_type==RagTime5StructManager::Field::T_Unstructured && field.m_fileType==0xce017) {
        // a small value 2 (can be first data)
        f << "unkn="<<field.m_extra << ",";
        break;
      }
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::StyleCParser::parseField: find unexpected unicode field\n"));
      f << "###" << field;
      break;
    default:
      MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::StyleCParser::parseField: find unexpected list link field\n"));
      f << "###" << field;
      break;
    }
    return true;
  }
  //! end of a start zone call
  void endZone()
  {
    if (m_link.empty())
      return;
    if (m_what==0) {
      if (m_cluster->m_dataLink.empty())
        m_cluster->m_dataLink=m_link;
      else {
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::StyleCParser::endZone: oops the main link is already set\n"));
        m_cluster->m_linksList.push_back(m_link);
      }
    }
    else if (m_what==4) {
      if (m_cluster->m_nameLink.empty())
        m_cluster->m_nameLink=m_link;
      else {
        MWAW_DEBUG_MSG(("RagTime5ZoneManagerInternal::StyleCParser::endZone: oops the name link is already set\n"));
        m_cluster->m_linksList.push_back(m_link);
      }
    }
    else
      m_cluster->m_linksList.push_back(m_link);
  }
protected:
  //! the current cluster
  shared_ptr<RagTime5ZoneManager::Cluster> m_cluster;
  //! a index to know which field is parsed :  0: main, 1, 2/3: field , 4: unicode list
  int m_what;
  //! the actual field name
  std::string m_fieldName;
};

//
//! low level: parser of unknown cluster
//
struct UnknownCParser : public RagTime5ZoneManager::ClusterParser {
  //! constructor
  UnknownCParser(int type) : ClusterParser(type, "ClustUnknown")
  {
  }
  //! parse a zone
  virtual bool parseZone(MWAWInputStreamPtr &/*input*/, long /*fSz*/, int /*N*/, int /*flag*/, libmwaw::DebugStream &f)
  {
    if (m_dataId==0) f << "###";
    return false;
  }
};

}

int RagTime5ZoneManager::getClusterZoneType(RagTime5Zone &zone)
{
  MWAWEntry const &entry=zone.m_entry;
  if (entry.length()==0 || entry.length()<13) return -1;
  MWAWInputStreamPtr input=zone.getInput();
  long endPos=entry.end();
  input->setReadInverted(!zone.m_hiLoEndian);
  input->seek(entry.begin()+8, librevenge::RVNG_SEEK_SET);
  long endDataPos;
  if (!readFieldHeader(zone, endPos, "", endDataPos)) {
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::getClusterZoneType: can not read item A\n"));
    input->setReadInverted(false);
    return -1;
  }

  long fSz;
  if (!RagTime5StructManager::readCompressedLong(input, endDataPos, fSz) || fSz<6 || input->tell()+fSz>endDataPos) {
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::getClusterZoneType: can not read item A\n"));
    input->setReadInverted(false);
    return -1;
  }
  int res=-1;
  input->seek(2, librevenge::RVNG_SEEK_CUR); // skip flag
  int N=(int) input->readLong(4);
  switch (N) {
  case -2:
    res=0;
    break;
  case -5:
    input->seek(6, librevenge::RVNG_SEEK_CUR); // skip id, ...
    res=(int) input->readULong(2);
    break;
  default:
    if (N<0) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::getClusterZoneType: unexpected N value\n"));
      break;
    }
    if (fSz==0x20) {
      input->seek(16, librevenge::RVNG_SEEK_CUR);
      int fieldType=(int) input->readULong(2);
      if ((fieldType&0xFFD7)==0x1010)
        res=0x20001;
      else if ((fieldType&0xFFD7)==0x310)
        res=0x30003;
      else if ((fieldType&0xFFD7)==0x4010)
        res=0x10000;
      else {
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::getClusterZoneType: unexpected field type %x\n", unsigned(fieldType)));
      }
    }
    else if (fSz==0x22)
      res=0x30000;
    else if (fSz==0x28)
      res=0x30002;
    else if (fSz==0x29)
      res=0x20000;
    else {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::getClusterZoneType: unexpected fSz=%ld\n", fSz));
    }
    break;
  }
  input->setReadInverted(false);
  return res;
}

shared_ptr<RagTime5ZoneManager::Cluster> RagTime5ZoneManager::readClusterZone(RagTime5Zone &zone, int zoneType)
{
  // something is bad, try to retrieve the correct zone type using heuristic
  if (zoneType==-1)
    zoneType=getClusterZoneType(zone);

  shared_ptr<ClusterParser> parser;
  switch (zoneType) {
  case 0:
    parser.reset(new RagTime5ZoneManagerInternal::RootCParser);
    break;
  case 0x2:
  case 0xa:
  case 0x4002:
  case 0x400a:
    parser.reset(new RagTime5ZoneManagerInternal::ScriptCParser(zoneType));
    break;
  case 0x104:
  case 0x204:
  case 0x4104:
  case 0x4204:
    parser.reset(new RagTime5ZoneManagerInternal::PipelineCParser(zoneType));
    break;
  case 0x480:
    parser.reset(new RagTime5ZoneManagerInternal::StyleCParser);
    break;
  case 0x4001:
    parser.reset(new RagTime5ZoneManagerInternal::LayoutCParser);
    break;
  case 0x8042:
    parser.reset(new RagTime5ZoneManagerInternal::ColPatCParser);
    break;
  case 0x10000: // first child of root
  case 0x20000: // field def
  case 0x20001: // field pos
  case 0x30000: // 0th element of child list root
  case 0x30001: // 1th element of child list root, never seen
  case 0x30002: // 2th element of child list root
  case 0x30003: // 3th element of child list root
    parser.reset(new RagTime5ZoneManagerInternal::RootChildCParser(zoneType));
    break;
  default:
    break;
  }
  if (parser) {
    readClusterZone(zone, *parser);
    return parser->getCluster();
  }

  MWAWEntry &entry=zone.m_entry;
  if (entry.length()==0 || entry.length()<13) return shared_ptr<Cluster>();

  shared_ptr<Cluster> cluster(new Cluster);
  cluster->m_hiLoEndian=zone.m_hiLoEndian;
  cluster->m_zoneId=zone.m_ids[0];

  MWAWInputStreamPtr input=zone.getInput();
  long endPos=entry.end();
  input->setReadInverted(!zone.m_hiLoEndian);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  if (zoneType==-1)
    f << "ClustUnkn";
  else
    f << "Clust" << std::hex << zoneType << std::dec << "A";
  std::string zoneName(f.str());
  f.str("");
  f << "Entries(" << zoneName << ")[" << zone << "]:";
  int val;
  for (int i=0; i<4; ++i) { // f0=f1=0, f2=1, f3=small number
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  zone.m_isParsed=true;
  libmwaw::DebugFile &ascFile=zone.ascii();
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(endPos);
  ascFile.addNote("_");

  int n=0;
  while (!input->isEnd()) {
    long pos=input->tell();
    if (pos>=endPos) break;
    f.str("");
    f << zoneName << "-" << n;
    long endDataPos;
    if (!readFieldHeader(zone, endPos, f.str(), endDataPos)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }

    pos=input->tell();
    f.str("");
    f << zoneName << "-" << n << "-A:";
    if (!zone.m_hiLoEndian) f << "lohi,";
    long fSz;
    if (!RagTime5StructManager::readCompressedLong(input, endDataPos, fSz) || fSz<6 || input->tell()+fSz>endDataPos) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: can not read item A\n"));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      ++n;
      input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
      continue;
    }
    long debSubDataPos=input->tell();
    long endSubDataPos=debSubDataPos+fSz;
    int fl=(int) input->readULong(2); // [01][13][0139b]
    f << "fl=" << std::hex << fl << std::dec << ",";
    int N=(int) input->readLong(4);
    std::stringstream s;
    s << "type" << std::hex << N << std::dec;

    enum What { W_Unknown, W_ListLink, W_FixedListLink,
                W_GraphZones,
                W_LinkDef,
                W_TextUnknown, W_TextZones
              };
    What what=W_Unknown;
    std::string name=s.str();
    Link link;
    int linkItemId=-1;
    bool isMainLink=false, isNamedLink=false;

    switch (N) {
    case -5:
      name="header";
      if (fSz<12) {
        f << "###data,";
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: can not read the data id\n"));
        break;
      }
      for (int i=0; i<2; ++i) { // always 0?
        val=(int) input->readLong(2);
        if (val) f << "f" << i+1 << "=" << val << ",";
      }
      val=(int) input->readLong(2);
      f << "id=" << val << ",";
      val=(int) input->readULong(2);
      if (val!=zoneType) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: unexpected zone type[graph]\n"));
        f << "##zoneType=" << std::hex << val << std::dec << ",";
      }
      if (fSz==64) {  // find in zone b ( with a next data field which is a link to a cluster )
        // probably in relation with zone ClusterScript
        f << "sz64,";
        val=(int) input->readLong(4); // find with 2, probably a number
        if (val) f << "f0=" << val << ",";
        for (int i=0; i<4; ++i) { // always 0
          val=(int) input->readLong(2);
          if (val) f << "f" << i+1 << "=" << val << ",";
        }
        float dim[2];
        for (int i=0; i<2; ++i) dim[i]=float(input->readLong(4))/65536.f;
        f << "dim=" << Vec2f(dim[0],dim[1]) << ",";
        for (int i=0; i<15; ++i) { // always 0
          val=(int) input->readLong(2);
          if (val) f << "g" << i << "=" << val << ",";
        }
        break;
      }
      if (fSz==104||fSz==109) { // f0=0|2|3, f1=0|3
        name="st104-9";
        f << "sz104,";
        for (int i=0; i<2; ++i) {
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
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: unexpected type [104|109]\n"));
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
        link.m_fileType[0]=input->readLong(4);
        if (link.m_fileType[0])
          f << "fieldType1=" << std::hex << link.m_fileType[0] << std::dec << ",";
        std::vector<int> listIds;
        long actPos=input->tell();
        if (!m_structManager->readDataIdList(input, 1, listIds)) {
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: can not find the data[104|109]\n"));
          f << "##noData,";
          link.m_ids.clear();
          input->seek(actPos+2, librevenge::RVNG_SEEK_SET);
        }
        else if (listIds[0]) { // find always with a block of size 0...
          Link unknLink;
          unknLink=Link::L_UnknownItem;
          unknLink.m_name="UnknMain104-109";
          unknLink.m_ids.push_back(listIds[0]);
          f << unknLink << ",";
          cluster->m_linksList.push_back(unknLink);
        }
        for (int i=0; i<2; ++i) { // always 0
          val= (int) input->readLong(2);
          if (val) f << "h" << i+6 << "=" << val << ",";
        }
        if (fSz==104) break;
        int dim2[2];
        for (int i=0; i<2; ++i) dim2[i]=(int) input->readLong(2);
        f << "dim2=" << Vec2i(dim2[0], dim2[1]) << ",";
        val= (int) input->readLong(1); // 0 or 1
        if (val) f << "h8=" << val << ",";
        break;
      }
      if (fSz==118) {  // find in zone : 3|e|b
        cluster->m_type=Cluster::C_GraphicData;
        what=W_GraphZones;
        name="graphZone";
        isMainLink=true;
        f << "graph,";
        for (int i=0; i<2; ++i) { // f4=0|2|3
          val=(int) input->readLong(2);
          if (val) f << "f" << i+3 << "=" << val << ",";
        }
        val=(int) input->readLong(4); // 0|2|3|4
        if (val) f << "f5=" << val << ",";
        link.m_fileType[0]=(long) input->readULong(4); // find 0|80|81|880|8000|8080

        if ((link.m_fileType[0]&0x777E)!=0) {
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: the file type0 seems bad[graph]\n"));
          f << "##fileType0=" << std::hex << link.m_fileType[0] << std::dec << ",";
        }
        else if (link.m_fileType[0])
          f << "fileType0=" << std::hex << link.m_fileType[0] << std::dec << ",";
        for (int wh=0; wh<2; ++wh) {
          f << "block" << wh << "[";
          val=(int) input->readLong(2); // 1 or 10
          if (val!=1) f << "g0=" << val << ",";
          for (int i=0; i<5; ++i) { // g1=numData+1? and g2 small number, other 0 ?
            val=(int) input->readLong(4);
            if (val) f << "g" << i+1 << "=" << val << ",";
          }
          link.m_type=Link::L_Graphic;
          if (wh==0) {
            link.m_fileType[1]=(long) input->readULong(2);
            if (link.m_fileType[1]!=0x8000 && link.m_fileType[1]!=0x8020) {
              MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: the file type1 seems bad[graph]\n"));
              f << "##fileType1=" << std::hex << link.m_fileType[1] << std::dec << ",";
            }
            long actPos=input->tell();
            if (!m_structManager->readDataIdList(input, 2, link.m_ids) || link.m_ids[1]==0) {
              MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: can not find the graph data\n"));
              f << "##noData,";
              link.m_ids.clear();
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
            Link unknLink;
            unknLink.m_fileType[1]=(long) input->readULong(2);
            unknLink.m_fieldSize=(int) input->readULong(2);
            if ((unknLink.m_fileType[1]!=0x50 && unknLink.m_fileType[1]!=0x58) || unknLink.m_fieldSize!=10) {
              MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: the file type2 seems bad[graph]\n"));
              f << "##fileType2=" << std::hex << unknLink.m_fileType[1] << std::dec << "[" << unknLink.m_fieldSize << "],";
            }
            // fixme store unknLink instead of updating the main link
            std::vector<int> listIds;
            if (m_structManager->readDataIdList(input, 3, listIds)) {
              link.m_ids.push_back(listIds[0]);
              for (size_t i=1; i<3; ++i) {
                if (!listIds[i]) continue;
                cluster->m_clusterIdsList.push_back(listIds[i]);
                f << "clusterId" << i << "=data" << listIds[i] << "A,";
              }
            }
            else {
              MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: can not read unkn link list[graph]\n"));
              f << "##graph[unknown],";
            }
          }
          f << "],";
        }

        break;
      }
      if (fSz==134) {
        for (int i=0; i<9; ++i) { // f1=0|7-b, f3=0|8-b
          val=(int) input->readULong(2);
          if (val)
            f << "f" << i << "=" << val << ",";
        }
        for (int i=0; i<2; ++i) { // g0=0|1, g1=0|1
          val=(int) input->readULong(1);
          if (val)
            f << "g" << i << "=" << val << ",";
        }
        val=(int) input->readULong(2); // [02][08]0[12c]
        if (val) f << "fl=" << std::hex << val << std::dec << ",";
        for (int i=0; i<12; ++i) { // h1=2, h3=0|3, h5=3|4, h7=h5+1, h9=h7+1, h11=h9+1
          val=(int) input->readULong(2);
          if (val)
            f << "h" << i << "=" << val << ",";
        }
        std::vector<int> listIds;
        if (!m_structManager->readDataIdList(input, 2, listIds)) {
          f << "##field,";
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: can not read the field definitions\n"));
          break;
        }
        if (listIds[0] || listIds[1]) { // fielddef and fieldpos
          Link fieldLink;
          fieldLink.m_type=Link::L_ClusterLink;
          fieldLink.m_ids=listIds;
          cluster->m_fieldClusterLink=fieldLink;
          f << "fields," << fieldLink << ",";
        }
        break;
      }
      if (fSz==135||fSz==140||fSz==143) {
        f << "sz135...,";
        for (int i=0; i<2; ++i) { // f0=9-5d, f1=0
          val=(int) input->readLong(4);
          if (val)
            f << "f" << i << "=" << val << ",";
        }
        val=(int) input->readLong(1); // 0|1
        if (val)
          f << "fl=" << val << ",";
        val=(int) input->readULong(2);
        if (val) // [08]0[08][049]
          f << "fl2=" << std::hex << val << std::dec << ",";
        val=(int) input->readLong(1); // 1|1d
        if (val!=1)
          f << "fl3=" << val << ",";
        val=(int) input->readULong(2); // alway 10
        if (val!=0x10)
          f << "f2=" << val << ",";
        val=(int) input->readLong(4);
        if (val)
          f << "f3=" << val << ",";
        for (int i=0; i<11; ++i) { // g8=40|60
          val=(int) input->readLong(2);
          if (val)
            f << "g" << i << "=" << val << ",";
        }
        val=(int) input->readLong(1); // always 1
        if (val!=1)
          f << "fl4=" << val << ",";
        if (fSz==140) {
          for (int i=0; i<5; ++i) { // unsure find only 0 here
            val=(int) input->readLong(1);
            if (val)
              f << "flA" << i << "=" << val << ",";
          }
        }

        for (int i=0; i<2; ++i) { // always 1,2
          val=(int) input->readLong(4);
          if (val!=i+1)
            f << "h" << i << "=" << val << ",";
        }
        for (int i=0; i<2; ++i) { // always 0,4
          val=(int) input->readLong(2);
          if (val)
            f << "h" << i+2 << "=" << val << ",";
        }
        for (int i=0; i<4; ++i) { // always h4=3, h5=small number, h6=h5+1
          val=(int) input->readLong(4);
          if (val)
            f << "h" << i+4 << "=" << val << ",";
        }
        for (int i=0; i<2; ++i) {  // always 1,4
          val=(int) input->readLong(2);
          if (val)
            f << "h" << i+8 << "=" << val << ",";
        }
        val=(int) input->readULong(4);
        if (val!=0x5555)
          f << "#fileType=" << std::hex << val << std::dec << ",";
        val=(int) input->readULong(4);
        if (val!=0x18000)
          f << "#fileType2=" << std::hex << val << std::dec << ",";
        for (int i=0; i<5; ++i) { // always 0
          val=(int) input->readLong(2);
          if (val)
            f << "j" << i << "=" << val << ",";
        }
        for (int i=0; i<5; ++i) { // j5=0|5, j6=0|5, j7=small number, j8=0|5
          val=(int) input->readLong(4);
          if (val)
            f << "j" << i+5 << "=" << val << ",";
        }
        f << "IDS=[";
        for (int i=0; i<2; ++i) // unsure, junk
          f << std::hex << input->readULong(4) << std::dec << ",";
        f << "],";
        val=(int) input->readULong(2); // c00|cef
        if (val)
          f << "fl5=" << std::hex << val << std::dec << ",";
        if (fSz==135||fSz==140)
          break;
        for (int i=0; i<4; ++i) { // always 0
          val=(int) input->readLong(2);
          if (val)
            f << "k" << i << "=" << val << ",";
        }
        break;
      }
      f << "unknMain,";
      break;
    default: {
      if (what!=W_Unknown) break;

      input->seek(debSubDataPos+18, librevenge::RVNG_SEEK_SET);
      long type=(long) input->readULong(4);
      if (type==0x15f3817||type==0x15e4817) {
        link.m_fileType[0]=type;
        link.m_fileType[1]=(long) input->readULong(2);
        link.m_fieldSize=(int) input->readULong(2);
        if (m_structManager->readDataIdList(input, 1, link.m_ids) && link.m_ids[0]) {
          link.m_N=N;
          shared_ptr<RagTime5Zone> data=m_mainParser.getDataZone(link.m_ids[0]);
          if (data) {
            if (link.m_fileType[0]==0x15e4817) {
              name="textUnknown";
              what=W_TextUnknown;
              link.m_type=Link::L_TextUnknown;
            }
            else {
              name="linkDef";
              what=W_LinkDef;
              link.m_type=Link::L_LinkDef;
              if (fSz>=71) { // the definitions
                ascFile.addDelimiter(input->tell(),'|');
                input->seek(debSubDataPos+63, librevenge::RVNG_SEEK_SET);
                ascFile.addDelimiter(input->tell(),'|');
                std::vector<int> dataId;
                if (m_structManager->readDataIdList(input, 2, dataId) && dataId[1]) {
                  link.m_ids.push_back(dataId[0]);
                  link.m_ids.push_back(dataId[1]);
                }
              }
            }
            break;
          }
        }
      }

      input->seek(debSubDataPos+6, librevenge::RVNG_SEEK_SET);
      if (fSz==28) {
        link.m_fileType[0]=(int) input->readULong(4);
        if (link.m_fileType[0]!=0x100004) {
          f << "fType=" << std::hex << link.m_fileType[0] << std::dec << ",";
          break;
        }
        link.m_type=Link::L_Text;
        name="textZone";
        what=W_TextZones;
        cluster->m_type=Cluster::C_TextData;
        isMainLink=true;
        f << "f0=" << N << ",";
        val=(int) input->readLong(2); // always 0?
        if (val) f << "f1=" << val << ",";
        val=(int) input->readLong(2); // always f?
        if (val!=15) f << "f2=" << val << ",";
        std::vector<int> listIds;
        if (m_structManager->readDataIdList(input, 1, listIds))
          link.m_ids.push_back((int) listIds[0]);
        else {
          f << "#link0,";
          link.m_ids.push_back(0);
        }
        link.m_N=(int) input->readULong(4);
        val=(int) input->readLong(1); // always 0?
        if (val) f << "f3=" << val << ",";
        listIds.clear();
        if (m_structManager->readDataIdList(input, 1, listIds) && listIds[0])
          link.m_ids.push_back(listIds[0]);
        else {
          f << "#link1,";
          link.m_ids.push_back(0);
        }
        val=(int) input->readLong(1); // always 1?
        if (val) f << "f4=" << val << ",";
        break;
      }
      if (fSz==30) {
        link.m_fileType[0]=(int) input->readULong(4);
        input->seek(debSubDataPos+22, librevenge::RVNG_SEEK_SET);
        link.m_fileType[1]=(int) input->readULong(2);
        link.m_fieldSize=(int) input->readULong(2);
        if (m_structManager->readDataIdList(input, 1, link.m_ids) && link.m_ids[0]) {
          link.m_N=N;
          shared_ptr<RagTime5Zone> data=m_mainParser.getDataZone(link.m_ids[0]);

          if (data) {
            if (link.m_fileType[0]==0x9f840) {
              what=W_FixedListLink;
              if (link.m_fileType[1]!=0x10) // 10 or 18
                f << "f1=" << link.m_fileType[1] << ",";
              link.m_fileType[1]=0;
              link.m_type=Link::L_GraphicTransform;
              name="graphTransform";
            }
            else if (!link.m_fieldSize || data->m_entry.length()!=N*link.m_fieldSize)
              link=Link();
            else if ((link.m_fileType[1]==0xd0||link.m_fileType[1]==0xd8) && link.m_fieldSize==12) {
              name="clusterLink";
              link.m_type=Link::L_ClusterLink;
            }
            else {
              what=W_FixedListLink;
              link.m_fileType[0]=0;
              name="listLn2";
              ascFile.addDelimiter(debSubDataPos+22,'|');
            }
          }
          else
            link=Link();
        }
        else
          link=Link();
        input->seek(debSubDataPos+6, librevenge::RVNG_SEEK_SET);
        break;
      }
      if (fSz==32 || fSz==36 || fSz==41) {
        what=W_ListLink;
        long unknType=(long) input->readULong(4);
        if (unknType) f << "unknType=" << std::hex << unknType << std::dec << ",";
        link.m_fileType[0]=(long) input->readULong(4);
        input->seek(debSubDataPos+20, librevenge::RVNG_SEEK_SET);
        link.m_type=Link::L_List;
        val=(int) input->readLong(2);
        if (val) f << "f3=" << val << ",";
        link.m_fileType[1]=(long) input->readULong(2);
        if (!m_structManager->readDataIdList(input, 2, link.m_ids) || !link.m_ids[1]) {
          if (fSz==41 || fSz==36)
            what=W_Unknown;
          else
            f << "###";
          link=Link();
          break;
        }
        if (link.m_fileType[1]==0x220 || link.m_fileType[1]==0x200 || link.m_fileType[1]==0x620) {
          name="unicodeList";
          if (link.m_fileType[0]==0x7d01a) name+="[layout]";
          isNamedLink=true;
          link.m_type=Link::L_UnicodeList;
        }
        else if (unknType==0x47040) {
          if (link.m_fileType[1]==0x20)
            f << "hasNoPos,";
          else if (link.m_fileType[1]) {
            MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone[settings]: find unexpected flags\n"));
            f << "###flags=" << std::hex << link.m_fileType[1] << std::dec << ",";
          }
          linkItemId=1;
          link.m_name=name="settings";
        }
        // fSz=41 is the field list
        // todo 0x80045080 is a list of 2 int
        // fileType[1]==4030: layout list
        else if (link.m_fileType[1]==0x30 && fSz==32) {
          linkItemId=0;
          link.m_name=name="condFormula";
        }
        else
          name="listLink";
        /* checkme:
           link.m_fileType[0]==20|0 is a list of fields
           link.m_fileType[0]==1010|1038 is a list of sequence of 16 bytes
        */
        ascFile.addDelimiter(debSubDataPos+20,'|');
        break;
      }
      if (fSz==0x50  && N==1) {
        name="graphDim";
        val=(int) input->readULong(2); // always 0?
        if (val) f << "f0=" << val << ",";
        val=(int) input->readULong(2);
        if (val) f << "id=" << val << ",";
        val=(int) input->readULong(2); //[04][01248a][01][23]
        if (val) f << "fl=" << std::hex << val << std::dec << ",";
        for (int i=0; i<2; ++i) {
          val=(int) input->readULong(2); //f1=0-1e, f2=0|3ffe
          if (val) f << "f" << i+1 << "=" << val << ",";
        }
        float dim[4];
        for (int i=0; i<4; ++i) dim[i]=float(input->readLong(4))/65536.f;
        Box2f box(Vec2f(dim[0],dim[1]), Vec2f(dim[2],dim[3]));
        f << "box=" << box << ",";
        for (int i=0; i<4; ++i) dim[i]=float(input->readLong(4))/65536.f;
        Box2f box2(Vec2f(dim[0],dim[1]), Vec2f(dim[2],dim[3]));
        if (box!=box2)
          f << "boxA=" << box2 << ",";
        for (int i=0; i<7; ++i) { // g1=0|2, g3=9|7
          val=(int) input->readLong(2);
          if (val) f << "g" << i << "=" << val << ",";
        }
        for (int i=0; i<9; ++i) { // h0=a flag?, h2= a flag?, h5=h6=0|-1
          val=(int) input->readLong(2);
          if (val) f << "h" << i << "=" << val << ",";
        }
        break;
      }
      break;
    }
    }
    if (link.empty())
      f << name << ",";
    else
      f << link << ",";
    if (fSz>32 && N>0 && what==W_Unknown) {
      long actPos=input->tell();
      input->seek(debSubDataPos+18, librevenge::RVNG_SEEK_SET);
      link.m_fileType[0]=(long) input->readULong(4);
      link.m_fileType[1]=(long) input->readULong(2);
      link.m_fieldSize=(int) input->readULong(2);
      if (m_structManager->readDataIdList(input, 1, link.m_ids) && link.m_ids[0]) {
        link.m_N=N;
        shared_ptr<RagTime5Zone> data=m_mainParser.getDataZone(link.m_ids[0]);
        if (data && data->m_entry.length()==N*link.m_fieldSize) {
          name+="-fixZone";
          what=W_FixedListLink;

          f << link << ",";
          ascFile.addDelimiter(debSubDataPos+18,'|');
          ascFile.addDelimiter(debSubDataPos+30,'|');
        }
        else
          link=Link();
      }
      else
        link=Link();
      input->seek(actPos, librevenge::RVNG_SEEK_SET);
    }
    if (input->tell()!=pos && input->tell()!=endSubDataPos)
      ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(endSubDataPos, librevenge::RVNG_SEEK_SET);

    int m=0;

    while (!input->isEnd()) {
      pos=input->tell();
      if (pos+4>endDataPos)
        break;
      RagTime5StructManager::Field field;
      if (!m_structManager->readField(input, endDataPos, ascFile, field)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        break;
      }
      f.str("");
      f << zoneName << "-" << n << "-B" << m++ << "[" << name << "]:";
      if (!zone.m_hiLoEndian) f << "lohi,";
      bool done=false;
      switch (what) {
      case W_ListLink: {
        if (field.m_type==RagTime5StructManager::Field::T_LongList && field.m_fileType==0xce842) {
          f << "pos=[";
          for (size_t i=0; i<field.m_longList.size(); ++i)
            f << field.m_longList[i] << ",";
          f << "],";
          link.m_longList=field.m_longList;
          done=true;
          break;
        }
        if (field.m_type==RagTime5StructManager::Field::T_Unstructured && field.m_fileType==0xce017) {
          // a small value 2|4|a|1c|40
          f << "unkn="<<field.m_extra << ",";
          done=true;
          break;
        }
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find unexpected list link field\n"));
        f << "###";
        break;
      }
      case W_LinkDef: {
        if (field.m_type==RagTime5StructManager::Field::T_FieldList &&
            (field.m_fileType==0x15f4815 /* v5?*/ || field.m_fileType==0x160f815 /* v6? */)) {
          f << "decal=[";
          for (size_t i=0; i<field.m_fieldList.size(); ++i) {
            RagTime5StructManager::Field const &child=field.m_fieldList[i];
            if (child.m_type==RagTime5StructManager::Field::T_LongList && child.m_fileType==0xce842) {
              for (size_t j=0; j<child.m_longList.size(); ++j)
                f << child.m_longList[j] << ",";
              link.m_longList=child.m_longList;
              continue;
            }
            MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find unexpected decal child[list]\n"));
            f << "#[" << child << "],";
          }
          f << "],";
          done=true;
          break;
        }
        else if (field.m_type==RagTime5StructManager::Field::T_Unstructured && field.m_fileType==0xce017) {
          f << "unkn0=" << field.m_extra; // always 2: next value ?
          done=true;
          break;
        }
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find unexpected child[list]\n"));
        break;
      }

      case W_GraphZones: {
        done=true;
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
              link.m_longList=child.m_longList;
              continue;
            }
            MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find unexpected decal child[graph]\n"));
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
              MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find unexpected unstructured child[graphZones]\n"));
              f << "##" << child << ",";
            }
          }
          f << "],";
          break;
        }
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find unexpected child[graphZones]\n"));
        f << "##" << field << ",";
        break;
      }
      case W_TextUnknown:
      case W_TextZones:
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find unexpected text zones child\n"));
        f << "###";
        break;
      case W_FixedListLink:
      case W_Unknown:
      default:
        break;
      }
      if (done) {
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        continue;
      }
      f << field;
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    if (!link.empty()) {
      if (isMainLink) {
        if (cluster->m_dataLink.empty())
          cluster->m_dataLink=link;
        else {
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: oops the main link is already set\n"));
          cluster->m_linksList.push_back(link);
        }
      }
      else if (link.m_type==Link::L_List && linkItemId==0)
        cluster->m_conditionFormulaLinks.push_back(link);
      else if (link.m_type==Link::L_List && linkItemId==1)
        cluster->m_settingLinks.push_back(link);
      else if (isNamedLink) {
        if (cluster->m_nameLink.empty())
          cluster->m_nameLink=link;
        else {
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: oops the name link is already set\n"));
          cluster->m_linksList.push_back(link);
        }
      }
      else
        cluster->m_linksList.push_back(link);
    }
    pos=input->tell();
    if (pos!=endDataPos) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find some extra data\n"));
      f.str("");
      f << zoneName << "-" << n << ":" << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    ++n;
    input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
  }
  long pos=input->tell();
  if (pos!=endPos) {
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find some extra data\n"));
    f.str("");
    f << zoneName << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->setReadInverted(false);

  return cluster;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
