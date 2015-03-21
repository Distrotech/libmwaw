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

#ifndef RAG_TIME_5_ZONE_MANAGER
#  define RAG_TIME_5_ZONE_MANAGER

#include <map>
#include <ostream>
#include <string>
#include <vector>

#include "libmwaw_internal.hxx"
#include "MWAWDebug.hxx"
#include "MWAWEntry.hxx"

class RagTime5Parser;
class RagTime5StructManager;

//! main zone in a RagTime v5-v6 document
class RagTime5Zone
{
public:
  //! the zone file type
  enum FileType { F_Main, F_Data, F_Empty, F_Unknown };
  //! constructor
  RagTime5Zone(MWAWInputStreamPtr input, libmwaw::DebugFile &asc):
    m_fileType(F_Unknown), m_subType(0), m_defPosition(0), m_entry(), m_name(""), m_hiLoEndian(true),
    m_entriesList(), m_extra(""), m_isParsed(false),
    m_input(input), m_defaultInput(true), m_asciiName(""), m_asciiFile(&asc), m_localAsciiFile()
  {
    for (int i=0; i<3; ++i) m_ids[i]=m_idsFlag[i]=0;
    for (int i=0; i<2; ++i) m_kinds[i]="";
    for (int i=0; i<2; ++i) m_variableD[i]=0;
  }
  //! destructor
  virtual ~RagTime5Zone() {}
  //! returns the zone name
  std::string getZoneName() const;
  //! returns true if the zone is a header zone(header, list zone, ...)
  bool isHeaderZone() const
  {
    return (m_fileType==F_Data && m_ids[0]==0) ||
           (m_fileType==F_Main && (m_ids[0]==1 || m_ids[0]==4 || m_ids[0]==5));
  }
  //! returns the main type
  std::string getKindLastPart(bool main=true) const
  {
    std::string res(m_kinds[main ? 0 : 1]);
    std::string::size_type pos = res.find_last_of(':');
    if (pos == std::string::npos) return res;
    return res.substr(pos+1);
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, RagTime5Zone const &z);
  //! returns the current input
  MWAWInputStreamPtr getInput()
  {
    return m_input;
  }
  //! reset the current input
  void setInput(MWAWInputStreamPtr input)
  {
    m_input = input;
    m_defaultInput = false;
  }
  //! returns true if the input correspond to the basic file
  bool isMainInput() const
  {
    return m_defaultInput;
  }
  //! returns the current ascii file
  libmwaw::DebugFile &ascii()
  {
    if (!m_defaultInput && !m_localAsciiFile)
      createAsciiFile();
    return *m_asciiFile;
  }
  //! defines the ascii name
  void setAsciiFileName(std::string const &name)
  {
    m_asciiName = name;
  }
  //! creates the ascii file
  void createAsciiFile();

  //! the zone file type
  FileType m_fileType;
  //! the zone sub type
  int m_subType;
  //! the position of the definition in the main zones
  long m_defPosition;
  //! the zone types: normal and packing
  std::string m_kinds[2];
  //! the zone entry
  MWAWEntry m_entry;
  //! the zone name ( mainly used for debugging)
  std::string m_name;
  //! true if the endian is hilo
  bool m_hiLoEndian;
  //! the zone id
  int m_ids[3];
  //! the zone flag
  int m_idsFlag[3];
  //! the list of original entries
  std::vector<MWAWEntry> m_entriesList;
  //! extra data
  std::string m_extra;
  //! the content of the zone D if it exists
  int m_variableD[2];
  //! a flag to know if the zone is parsed
  bool m_isParsed;
protected:
  //! the main input
  MWAWInputStreamPtr m_input;
  //! a flag used to know if the input is or not the default input
  bool m_defaultInput;
  //! the ascii file name ( used if we need to create a ascii file)
  std::string m_asciiName;
  //! the ascii file corresponding to an input
  libmwaw::DebugFile *m_asciiFile;
  //! the local ascii file ( if we need to create a new input)
  shared_ptr<libmwaw::DebugFile> m_localAsciiFile;
private:
  RagTime5Zone(RagTime5Zone const &orig);
  RagTime5Zone &operator=(RagTime5Zone const &orig);
};

//! basic class used to manage RagTime 5/6 zones
class RagTime5ZoneManager
{
public:
  struct Link;
  struct Cluster;

  //! constructor
  RagTime5ZoneManager(RagTime5Parser &parser);
  //! destructor
  ~RagTime5ZoneManager();

  //! try to read n data id
  static bool readDataIdList(MWAWInputStreamPtr input, int n, std::vector<int> &listIds);

  //! try to read a cluster zone
  bool readClusterZone(RagTime5Zone &zone, Cluster &cluster, int type=-1);

  //! try to read a style cluster: C_Formats, C_Units, C_GraphicColors, C_TextStyles, C_GraphicStyles;
  bool readStyleCluster(RagTime5Zone &zone, Cluster &cluster);
  //! try to read a field cluster: either fielddef or fieldpos
  bool readFieldCluster(RagTime5Zone &zone, Cluster &cluster, int type);

  //! try to read a 104,204,4204 cluster
  bool readUnknownClusterA(RagTime5Zone &zone, Cluster &cluster);
  //! try to read a unknown cluster ( first internal child of the root cluster )
  bool readUnknownClusterB(RagTime5Zone &zone, Cluster &cluster);
  //! try to read a unknown cluster
  bool readUnknownClusterC(RagTime5Zone &zone, Cluster &cluster, int type);

  //! try to read some field cluster
  bool readFieldClusters(Link const &link);
  //! try to read some unknown cluster
  bool readUnknownClusterC(Link const &link);

  //! a link to a small zone (or set of zones) in RagTime 5/6 documents
  struct Link {
    //! the link type
    enum Type { L_FieldCluster, L_FieldDef, L_FieldPos,
                L_ColorPattern,
                L_Graphic, L_GraphicTransform, L_GraphicType,
                L_Text, L_TextUnknown,
                L_ClusterLink,
                L_ConditionFormula, L_LinkDef, L_SettingsList, L_UnicodeList,
                L_FieldsList, L_List,
                L_UnknownClusterB, L_UnknownClusterC,
                L_Unknown
              };
    //! constructor
    Link(Type type=L_Unknown) : m_type(type), m_name(""), m_ids(), m_N(0), m_fieldSize(0), m_longList()
    {
      for (int i=0; i<2; ++i)
        m_fileType[i]=0;
    }
    //! returns true if all link are empty
    bool empty() const
    {
      for (size_t i=0; i<m_ids.size(); ++i)
        if (m_ids[i]>0) return false;
      return true;
    }
    //! returns the zone name
    std::string getZoneName() const
    {
      switch (m_type) {
      case L_ClusterLink:
        return "clustLink";
      case L_FieldCluster:
        return "fieldCluster";
      case L_FieldDef:
        return "fieldDef";
      case L_FieldPos:
        return "fieldPos";
      case L_ColorPattern:
        return "color/pattern";
      case L_ConditionFormula:
        return "condFormData";
      case L_Graphic:
        return "graphData";
      case L_GraphicTransform:
        return "graphTransform";
      case L_GraphicType:
        return "graphType";
      case L_LinkDef:
        return "linkDef";
      case L_SettingsList:
        return "settings";
      case L_Text:
        return "textData";
      case L_TextUnknown:
        return "TextUnknown";
      case L_UnicodeList:
        return "unicodeListLink";
      case L_UnknownClusterB:
        return "unknClustB";
      case L_UnknownClusterC:
        return "unknClustC";
      case L_FieldsList:
        if (!m_name.empty())
          return m_name;
        return "fieldsList[unkn]";
      case L_List:
      case L_Unknown:
      default:
        break;
      }
      std::stringstream s;
      if (m_type==L_List)
        s << "ListZone";
      else
        s << "FixZone";
      s << std::hex << m_fileType[0] << "_" << m_fileType[1] << std::dec;
      if (m_fieldSize)
        s << "_" << m_fieldSize;
      s << "A";
      return s.str();
    }
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Link const &z)
    {
      if (z.empty()) return o;
      o << z.getZoneName() << ":";
      size_t numLinks=z.m_ids.size();
      if (numLinks>1) o << "[";
      for (size_t i=0; i<numLinks; ++i) {
        if (z.m_ids[i]<=0)
          o << "_";
        else
          o << "data" << z.m_ids[i] << "A";
        if (i+1!=numLinks) o << ",";
      }
      if (numLinks>1) o << "]";
      if (z.m_fieldSize&0x8000)
        o << "[" << std::hex << z.m_fieldSize << std::dec << ":" << z.m_N << "]";
      else
        o << "[" << z.m_fieldSize << ":" << z.m_N << "]";
      return o;
    }
    //! the link type
    Type m_type;
    //! the link name
    std::string m_name;
    //! the data ids
    std::vector<int> m_ids;
    //! the number of data ( or some flag if m_N & 0x8020)
    int m_N;
    //! the field size
    int m_fieldSize;
    //! the zone type in file
    long m_fileType[2];
    //! a list of long used to store decal
    std::vector<long> m_longList;
  };
  //! the cluster data
  struct Cluster {
    //! constructor
    Cluster() : m_type(C_Unknown), m_hiLoEndian(true), m_dataLink(), m_nameLink(), m_linksList(), m_clusterIds()
    {
    }
    //! the cluster type
    enum Type {
      C_ColorPattern,
      C_Formats,
      C_GraphicData, C_GraphicColors, C_GraphicStyles,
      C_TextData, C_TextStyles,
      C_Units,
      C_ClusterA, C_ClusterB, C_ClusterC,
      C_Unknown
    };
    //! the cluster type
    Type m_type;
    //! the cluster hiLo endian
    bool m_hiLoEndian;
    //! the main data link
    Link m_dataLink;
    //! the name link
    Link m_nameLink;
    //! the link list
    std::vector<Link> m_linksList;
    //! the cluster ids
    std::vector<int> m_clusterIds;
  };
protected:
  //! the main parser
  RagTime5Parser &m_mainParser;
  //! the structure manager
  shared_ptr<RagTime5StructManager> m_structManager;
private:
  RagTime5ZoneManager(RagTime5ZoneManager const &orig);
  RagTime5ZoneManager operator=(RagTime5ZoneManager const &orig);
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
