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
#include "MWAWListener.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSubDocument.hxx"

#include "RagTime5Parser.hxx"
#include "RagTime5StructManager.hxx"
#include "RagTime5ClusterManager.hxx"

#include "RagTime5Spreadsheet.hxx"

#include "libmwaw_internal.hxx"

/** Internal: the structures of a RagTime5Spreadsheet */
namespace RagTime5SpreadsheetInternal
{
////////////////////////////////////////
//! Internal: the state of a RagTime5Spreadsheet
struct State {
  //! constructor
  State() : m_numPages(0) { }
  //! the number of pages
  int m_numPages;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
RagTime5Spreadsheet::RagTime5Spreadsheet(RagTime5Parser &parser) :
  m_mainParser(parser), m_structManager(m_mainParser.getStructManager()), m_parserState(parser.getParserState()),
  m_state(new RagTime5SpreadsheetInternal::State)
{
}

RagTime5Spreadsheet::~RagTime5Spreadsheet()
{ }

int RagTime5Spreadsheet::version() const
{
  return m_parserState->m_version;
}

int RagTime5Spreadsheet::numPages() const
{
  // TODO IMPLEMENT ME
  MWAW_DEBUG_MSG(("RagTime5Spreadsheet::numPages: is not implemented\n"));
  return 0;
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// interface send function
////////////////////////////////////////////////////////////

void RagTime5Spreadsheet::flushExtra()
{
  MWAW_DEBUG_MSG(("RagTime5Spreadsheet::flushExtra: is not implemented\n"));
}

////////////////////////////////////////////////////////////
// cluster parser
////////////////////////////////////////////////////////////

namespace RagTime5SpreadsheetInternal
{

//! low level: the spreadsheet cluster data
struct ClusterSpreadsheet : public RagTime5ClusterManager::Cluster {
  //! constructor
  ClusterSpreadsheet() : RagTime5ClusterManager::Cluster(), m_clusterLink()
  {
  }
  //! destructor
  virtual ~ClusterSpreadsheet() {}
  //! cluster links list of size 10
  RagTime5ClusterManager::Link m_clusterLink;
};

//
//! low level: parser of main spreadsheet cluster
//
struct SpreadsheetCParser : public RagTime5ClusterManager::ClusterParser {
  //! constructor
  SpreadsheetCParser(RagTime5ClusterManager &parser, int type) :
    ClusterParser(parser, type, "ClustSheet"), m_cluster(new ClusterSpreadsheet), m_what(-1), m_linkId(-1), m_fieldName("")
  {
  }
  //! return the spreadsheet cluster
  shared_ptr<ClusterSpreadsheet> getSpreadsheetCluster()
  {
    return m_cluster;
  }
  //! return the current cluster
  shared_ptr<RagTime5ClusterManager::Cluster> getCluster()
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
        MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::endZone: oops the name link is already set\n"));
        m_cluster->m_linksList.push_back(m_link);
      }
      break;
    case 1:
      if (m_cluster->m_clusterLink.empty())
        m_cluster->m_clusterLink=m_link;
      else {
        MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::endZone: oops the cluster link is already set\n"));
        m_cluster->m_linksList.push_back(m_link);
      }
      break;
    default:
      if (m_what==0) {
        if (m_cluster->m_dataLink.empty())
          m_cluster->m_dataLink=m_link;
        else {
          MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::endZone: oops the main link is already set\n"));
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
      MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseZone: expected N value\n"));
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
      // only with long2 list and with unk=[10-15]
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

      MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseField: find unexpected list link field\n"));
      f << "###" << field;
      break;
    default:
      f << field;
      break;
    }
    return true;
  }
protected:
  //! parse a data block
  bool parseDataZone(MWAWInputStreamPtr &input, long fSz, int N, int flag, libmwaw::DebugStream &f)
  {
    // seems to begin by 34,[58]^{0|1},69,71,16,
    f << "fl=" << std::hex << flag << std::dec << ",";
    long pos=input->tell();
    bool ok=true;
    int val;
    long linkValues[5];
    std::string mess;
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
          ok=false;
          break;
        }
        f << "###fType=" << std::hex << m_link.m_fileType[0] << std::dec << ",";
        MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseDataZone: the field fSz28... type seems bad\n"));
        return true;
      }
      long expectedFileType1=0;
      m_what=1;
      if (m_link.m_fileType[0]==0x35800)
        m_fieldName="zone:longs";
      else if (m_link.m_fileType[0]==0x3e800)
        m_fieldName="list:longs0";
      else if (m_link.m_fileType[0]==0x3c052) {
        if (linkValues[0]!=0x1454877) {
          MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseDataZone: find unexpected linkValue[0]\n"));
          f << "#lValues0,";
        }
        // linkValues[2]=5|8|9
        expectedFileType1=0x50;
        m_fieldName="zone:longs2";
      }
      else if (fSz==30 && m_link.m_fieldSize==12) { // find link to checkbox, ...
        expectedFileType1=0xd0;
        m_fieldName="clustLink";
        m_link.m_type=RagTime5ClusterManager::Link::L_ClusterLink;
        m_linkId=1;
      }
      else if (fSz==32 && m_link.m_fileType[0]==0) {
        expectedFileType1=0x200;
        m_linkId=0;
        m_link.m_type=RagTime5ClusterManager::Link::L_UnicodeList;
      }
      else if (fSz==34 && m_link.m_fileType[0]==0) {
        expectedFileType1=0x10;
        m_fieldName="data1_sheet";
        m_link.m_name="Data1_sheet";
      }
      else if (fSz==34 && m_link.m_fieldSize==0xa) {
        expectedFileType1=-1;
        m_fieldName="data2";
      }
      else if (fSz==34 && m_link.m_fieldSize==0x16) {
        expectedFileType1=-1;
        m_fieldName="data3";
      }
      else if (fSz==34 && m_link.m_fieldSize==0x18) {
        expectedFileType1=-1;
        m_fieldName="data4";
      }
      else if (fSz==36 && m_link.m_fileType[0]==0) {
        m_fieldName="listLink";
        expectedFileType1=0x10;
      }
      else {
        f << "###fType=" << m_link << ",";
        MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseDataZone: the field fSz28 type seems bad\n"));
        return true;
      }
      if (expectedFileType1>=0 && (m_link.m_fileType[1]&0xFFD7)!=expectedFileType1) {
        MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseDataZone: fileType1 seems odd[fSz=28...]\n"));
        f << "###fileType1=" << std::hex << m_link.m_fileType[1] << std::dec << ",";
      }
      m_link.m_fileType[0]=0;
      f << m_link << "," << mess;
      if (fSz==29) {
        val=(int) input->readLong(1);
        if (val!=1) // always 1
          f << "g0=" << val << ",";
      }
      break;
    }
    default:
      ok=false;
      break;
    }
    if (!m_fieldName.empty())
      f << m_fieldName << ",";
    if (ok)
      return true;
    f << "unparsed,";
    if ((fSz==0x22||fSz==0x27||fSz==0x32||fSz==0x34) && N>0) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      if (readLinkHeader(input, fSz, m_link, linkValues, mess)) {
        m_link.m_N=N;
        m_fieldName="UnknFixZone";
        f << m_fieldName << "," << m_link << "," << mess;
        m_link.m_fileType[0]=linkValues[0];
      }
      else
        m_link=RagTime5ClusterManager::Link();
    }

    return true;
  }
  //! parse the header zone
  bool parseHeaderZone(MWAWInputStreamPtr &input, long fSz, int N, int flag, libmwaw::DebugStream &f)
  {
    f << "header, fl=" << std::hex << flag << std::dec << ",";
    m_fieldName="header";
    m_what=0;
    if (N!=-5 || m_dataId!=0 || fSz != 134) {
      f << "###N=" << N << ",fSz=" << fSz << ",";
      MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseHeaderZone: find unexpected main field\n"));
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
      MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseHeaderZone: unexpected zone type\n"));
      f << "##zoneType=" << std::hex << val << std::dec << ",";
    }
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
    if (!RagTime5StructManager::readDataIdList(input, 2, listIds)) {
      f << "##field,";
      MWAW_DEBUG_MSG(("RagTime5SpreadsheetInternal::SpreadsheetCParser::parseHeaderZone: can not read the field definitions\n"));
    }
    else if (listIds[0] || listIds[1]) { // fielddef and fieldpos
      RagTime5ClusterManager::Link fieldLink;
      fieldLink.m_type=RagTime5ClusterManager::Link::L_ClusterLink;
      fieldLink.m_ids=listIds;
      m_cluster->m_fieldClusterLink=fieldLink;
      f << "fields," << fieldLink << ",";
    }
    if (!m_fieldName.empty())
      f << m_fieldName << ",";
    return true;
  }

  //! the current cluster
  shared_ptr<ClusterSpreadsheet> m_cluster;
  //! a index to know which field is parsed :  0: main, 1: list
  int m_what;
  //! the link id: 0: unicode, 1: clust link
  int m_linkId;
  //! the actual field name
  std::string m_fieldName;
private:
  //! copy constructor (not implemented)
  SpreadsheetCParser(SpreadsheetCParser const &orig);
  //! copy operator (not implemented)
  SpreadsheetCParser &operator=(SpreadsheetCParser const &orig);
};
}

bool RagTime5Spreadsheet::readSpreadsheetCluster(RagTime5Zone &zone, int zoneType)
{
  shared_ptr<RagTime5ClusterManager> clusterManager=m_mainParser.getClusterManager();
  if (!clusterManager) {
    MWAW_DEBUG_MSG(("RagTime5Spreadsheet::readSpreadsheetCluster: oops can not find the cluster manager\n"));
    return false;
  }
  RagTime5SpreadsheetInternal::SpreadsheetCParser parser(*clusterManager, zoneType);
  if (!clusterManager->readCluster(zone, parser) || !parser.getSpreadsheetCluster()) {
    MWAW_DEBUG_MSG(("RagTime5Spreadsheet::readSpreadsheetCluster: oops can not find the cluster\n"));
    return false;
  }
  shared_ptr<RagTime5SpreadsheetInternal::ClusterSpreadsheet> cluster=parser.getSpreadsheetCluster();
  m_mainParser.checkClusterList(cluster->m_clusterIdsList);

  if (!cluster->m_dataLink.empty()) {
    MWAW_DEBUG_MSG(("RagTime5Spreadsheet::readSpreadsheetCluster: oops do not how to parse the main data\n"));
  }

  if (!cluster->m_fieldClusterLink.empty())
    m_mainParser.getClusterManager()->readFieldClusters(cluster->m_fieldClusterLink);
  if (!cluster->m_clusterLink.m_ids.empty()) {
    shared_ptr<RagTime5Zone> dataZone=m_mainParser.getDataZone(cluster->m_clusterLink.m_ids[0]);
    if (!dataZone || !dataZone->m_entry.valid() ||
        dataZone->getKindLastPart(dataZone->m_kinds[1].empty())!="ItemData") {
      MWAW_DEBUG_MSG(("RagTime5Spreadsheet::readSpreadsheetCluster: the data zone %d seems bad\n",
                      cluster->m_clusterLink.m_ids[0]));
    }
    else
      m_mainParser.readClusterLinkList(*dataZone, cluster->m_clusterLink);
  }
  if (!cluster->m_nameLink.empty()) {
    std::map<int, librevenge::RVNGString> idToStringMap;
    m_mainParser.readUnicodeStringList(cluster->m_nameLink, idToStringMap);
  }

  for (size_t i=0; i<cluster->m_linksList.size(); ++i) {
    RagTime5ClusterManager::Link const &lnk=cluster->m_linksList[i];
    if (lnk.m_type==RagTime5ClusterManager::Link::L_List) {
      m_mainParser.readListZone(lnk);
      continue;
    }
    std::stringstream s;
    s << "Data" << lnk.m_fieldSize << "_sheet";
    RagTime5StructManager::DataParser defaultParser(s.str());
    m_mainParser.readFixedSizeZone(lnk, defaultParser);
  }

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
