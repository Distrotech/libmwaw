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
#include <sstream>
#include <string>
#include <vector>

#include "libmwaw_internal.hxx"
#include "MWAWDebug.hxx"
#include "MWAWEntry.hxx"

#include "RagTime5StructManager.hxx"

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

namespace RagTime5ZoneManagerInternal
{
struct State;
}

//! basic class used to manage RagTime 5/6 zones
class RagTime5ZoneManager
{
public:
  struct Link;

  struct Cluster;
  struct ClusterRoot;
  struct ClusterParser;

  friend struct ClusterParser;

  //! constructor
  RagTime5ZoneManager(RagTime5Parser &parser);
  //! destructor
  ~RagTime5ZoneManager();

  //! try to read a cluster zone
  bool readCluster(RagTime5Zone &zone, ClusterParser &parser, bool warnForUnparsed=true);
  //! try to read a cluster zone
  shared_ptr<Cluster> readCluster(RagTime5Zone &zone, int type=-1);
  //! try to read the cluster root list (in general Data14)
  bool readClusterMainList(ClusterRoot &root, std::vector<int> &list);

  //! try to read some field cluster
  bool readFieldClusters(Link const &link);
  //! try to read some unknown cluster
  bool readUnknownClusterC(Link const &link);
  //! try to find a cluster zone type ( heuristic when the cluster type is unknown )
  int getClusterZoneType(RagTime5Zone &zone);
  //! try to return basic information about the header cluster's zone
  bool getClusterBasicHeaderInfo(RagTime5Zone &zone, long &N, long &fSz, long &debHeaderPos);

  // low level

  //! try to read a field header, if ok set the endDataPos positions
  bool readFieldHeader(RagTime5Zone &zone, long endPos, std::string const &headerName, long &endDataPos, long expectedLVal=-99999);
  //! returns "data"+id+"A" ( followed by the cluster type and name if know)
  std::string getClusterName(int id);

  //! a link to a small zone (or set of zones) in RagTime 5/6 documents
  struct Link {
    //! the link type
    enum Type { L_Graphic, L_GraphicTransform,
                L_Text, L_TextUnknown,
                L_ClusterLink,
                L_LinkDef,
                L_LongList, L_UnicodeList,
                L_FieldsList, L_List,
                L_UnknownClusterC, L_UnknownItem,
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
      if (m_type==L_LongList && !m_longList.empty())
        return false;
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
      case L_Graphic:
        return "graphData";
      case L_GraphicTransform:
        return "graphTransform";
      case L_LinkDef:
        return "linkDef";
      case L_LongList:
        if (!m_name.empty())
          return m_name;
        else {
          std::stringstream s;
          s << "longList" << m_fieldSize;
          return s.str();
        }
      case L_Text:
        return "textData";
      case L_TextUnknown:
        return "TextUnknown";
      case L_UnicodeList:
        return "unicodeListLink";
      case L_UnknownClusterC:
        return "unknownClusterC";
      case L_UnknownItem:
        if (!m_name.empty())
          return m_name;
        return "UnknownItem";
      case L_FieldsList:
        if (!m_name.empty())
          return m_name;
        return "fieldsList[unkn]";
      case L_List:
        if (!m_name.empty())
          return m_name;
        break;
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

  ////////////////////////////////////////////////////////////
  // cluster classes
  ////////////////////////////////////////////////////////////

  //! the cluster data
  struct Cluster {
    //! constructor
    Cluster() : m_type(C_Unknown), m_zoneId(0), m_hiLoEndian(true), m_dataLink(), m_nameLink(), m_fieldClusterLink(),
      m_conditionFormulaLinks(), m_settingLinks(), m_linksList(), m_clusterIdsList()
    {
    }
    //! destructor
    virtual ~Cluster() {}
    //! the cluster type
    enum Type {
      C_ColorPattern, C_Fields, C_GraphicData, C_Layout, C_Pipeline,
      C_Root, C_Script, C_TextData,

      // the styles
      C_ColorStyles, C_FormatStyles, C_GraphicStyles, C_TextStyles, C_UnitStyles,
      // unknown clusters
      C_ClusterA, C_ClusterB, C_ClusterC,

      C_Unknown
    };
    //! the cluster type
    Type m_type;
    //! the zone id
    int m_zoneId;
    //! the cluster hiLo endian
    bool m_hiLoEndian;
    //! the main data link
    Link m_dataLink;
    //! the name link
    Link m_nameLink;
    //! the field cluster links (def and pos)
    Link m_fieldClusterLink;
    //! the conditions formula links
    std::vector<Link> m_conditionFormulaLinks;
    //! the settings links
    std::vector<Link> m_settingLinks;
    //! the link list
    std::vector<Link> m_linksList;
    //! the cluster ids
    std::vector<int> m_clusterIdsList;
  };

  //! the cluster graphic
  struct ClusterGraphic : public Cluster {
    //! constructor
    ClusterGraphic() : Cluster()
    {
    }
    //! destructor
    virtual ~ClusterGraphic() {}
    //! two cluster links: list of pipeline: fixedSize=12, second list with field size 10)
    Link m_clusterLinks[2];
  };

  //! the layout cluster ( 4001 zone)
  struct ClusterLayout : public Cluster {
    //! constructor
    ClusterLayout() : Cluster(), m_zoneDimensions(), m_pipelineLink(), m_listItemLink()
    {
    }
    //! destructor
    virtual ~ClusterLayout() {}
    //! list of zone's dimensions
    std::vector<Vec2f> m_zoneDimensions;
    //! link to a pipeline cluster list
    Link m_pipelineLink;
    //! link to  a zone of fieldSize 8(unknown)
    Link m_listItemLink;
  };

  //! the cluster for root
  struct ClusterRoot : public Cluster {
    //! constructor
    ClusterRoot() : Cluster(), m_graphicTypeLink(), m_docInfoLink(),
      m_listClusterId(0), m_listClusterName(), m_listClusterUnkn(), m_linkUnknown(), m_fileName("")
    {
      for (int i=0; i<8; ++i) m_styleClusterIds[i]=0;
      for (int i=0; i<1; ++i) m_clusterIds[i]=0;
    }
    //! destructor
    virtual ~ClusterRoot() {}
    //! the list of style cluster ( graph, units, unitsbis, text, format, unknown, graphcolor, col/pattern id)
    int m_styleClusterIds[8];

    //! other cluster id (unknown cluster b, )
    int m_clusterIds[1];

    //! the graphic type id
    Link m_graphicTypeLink;

    //! the doc info link
    Link m_docInfoLink;

    //! the cluster list id
    int m_listClusterId;
    //! the cluster list id name zone link
    Link m_listClusterName;
    //! an unknown link related to the cluster list
    Link m_listClusterUnkn;

    //! other link: scripts and field 6
    Link m_linkUnknown;

    //! the filename if known
    librevenge::RVNGString m_fileName;
  };

  //! the cluster script ( 2/a/4002/400a zone)
  struct ClusterScript : public Cluster {
    //! constructor
    ClusterScript() : Cluster(), m_scriptComment(), m_scriptName("")
    {
    }
    //! destructor
    virtual ~ClusterScript() {}
    //! the script comment zone
    Link m_scriptComment;
    //! the scriptname if known
    librevenge::RVNGString m_scriptName;
  };
  //! the cluster unknown A data
  struct ClusterUnknownA : public Cluster {
    //! constructor
    ClusterUnknownA() : Cluster(), m_auxilliarLink(), m_clusterLink()
    {
    }
    //! destructor
    virtual ~ClusterUnknownA() {}
    //! the first auxilliar data
    Link m_auxilliarLink;
    //! cluster links list of size 28
    Link m_clusterLink;
  };


  ////////////////////////////////////////////////////////////
  // parser class
  ////////////////////////////////////////////////////////////

  //! virtual class use to parse the cluster data
  struct ClusterParser {
    //! constructor
    ClusterParser(RagTime5ZoneManager &parser, int type, std::string const &zoneName) :
      m_parser(parser), m_type(type), m_hiLoEndian(true), m_name(zoneName), m_dataId(0), m_link()
    {
    }
    //! destructor
    virtual ~ClusterParser() {}
    //! return the current cluster
    virtual shared_ptr<Cluster> getCluster()=0;
    //! return the debug name corresponding to a zone
    virtual std::string getZoneName() const
    {
      return m_name;
    }
    //! return the debug name corresponding to a cluster
    virtual std::string getZoneName(int n, int m=-1) const
    {
      std::stringstream s;
      s << m_name << "-" << n;
      if (m>=0)
        s << "-B" << m;
      return s.str();
    }
    //! start a new zone
    virtual void startZone()
    {
    }
    //! parse a zone
    virtual bool parseZone(MWAWInputStreamPtr &/*input*/, long /*fSz*/, int /*N*/, int /*flag*/, libmwaw::DebugStream &/*f*/)
    {
      return false;
    }
    //! end of a start zone call
    virtual void endZone()
    {
    }
    //! parse a n_dataId:m
    virtual bool parseField(RagTime5StructManager::Field const &/*field*/, int /*m*/, libmwaw::DebugStream &/*f*/)
    {
      return false;
    }

    //
    // some tools
    //

    //! return true if N correspond to a file/script name
    bool isANameHeader(long N) const
    {
      return (m_hiLoEndian && N==int(0x80000000)) || (!m_hiLoEndian && N==0x8000);
    }

    //! read the first part of a link list (fSz>=32)
    bool readListHeader(MWAWInputStreamPtr &input, int type, Link &link, long(&values)[5], libmwaw::DebugStream &f, bool test=false);
    //! read the first part of a fixed size list (fSz>=30)
    bool readFixedSizeListHeader(MWAWInputStreamPtr &input, int type, bool readFieldSize, Link &link, long(&values)[5], libmwaw::DebugStream &f, bool test=false);
    //! returns "data"+id+"A" ( followed by the cluster type and name if know)
    std::string getClusterName(int id);
    //! the main parser
    RagTime5ZoneManager &m_parser;
    //! the cluster type
    int m_type;
    //! zone endian
    bool m_hiLoEndian;
    //! the cluster name
    std::string m_name;
    //! the actual zone id
    int m_dataId;
    //! the actual link
    Link m_link;
  private:
    ClusterParser(ClusterParser const &orig);
    ClusterParser &operator=(ClusterParser const &orig);
  };
protected:
  //! the state
  shared_ptr<RagTime5ZoneManagerInternal::State> m_state;
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
