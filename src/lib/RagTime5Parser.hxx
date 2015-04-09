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

#ifndef RAG_TIME_5_PARSER
#  define RAG_TIME_5_PARSER

#include <map>
#include <string>
#include <set>
#include <vector>

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

#include "RagTime5StructManager.hxx"
#include "RagTime5ClusterManager.hxx"

namespace RagTime5ParserInternal
{
struct DocInfoFieldParser;
struct State;
class SubDocument;
}

class RagTime5Graph;
class RagTime5StructManager;
class RagTime5Text;
class RagTime5Zone;
class RagTime5ClusterManager;

/** \brief the main class to read a RagTime v5 file
 *
 *
 *
 */
class RagTime5Parser : public MWAWTextParser
{
  friend class RagTime5Graph;
  friend class RagTime5Text;
  friend class RagTime5ClusterManager;
  friend struct RagTime5ParserInternal::DocInfoFieldParser;
  friend class RagTime5ParserInternal::SubDocument;

public:
  //! constructor
  RagTime5Parser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~RagTime5Parser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(librevenge::RVNGTextInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //
  // interface
  //

  //! returns the structure manager
  shared_ptr<RagTime5StructManager> getStructManager();

  //! creates the listener which will be associated to the document
  void createDocument(librevenge::RVNGTextInterface *documentInterface);
  //! try to send the different zones
  bool sendZones();

  //! adds a new page
  void newPage(int number);

  //! finds the different objects zones
  bool createZones();
  //! try to create the main data zones list
  bool findDataZones(MWAWEntry const &entry);
  //! returns the zone corresponding to a data id (or 0)
  shared_ptr<RagTime5Zone> getDataZone(int dataId) const;
  //! try to update a zone: create a new input if the zone is stored in different positions, ...
  bool update(RagTime5Zone &zone);
  //! try to read the zone data
  bool readZoneData(RagTime5Zone &zone);
  //! try to unpack a zone
  bool unpackZone(RagTime5Zone &zone, MWAWEntry const &entry, std::vector<unsigned char> &data);
  //! try to unpack a zone
  bool unpackZone(RagTime5Zone &zone);

  //! try to read the different cluster zones
  bool readClusterZones();
  //! try to read the cluster root list (in general Data14)
  bool readClusterRootList(RagTime5ClusterManager::ClusterRoot &root, std::set<int> &seens);
  //! try to read a cluster zone
  bool readClusterZone(RagTime5Zone &zone, int type=-1);
  //! try to read a cluster link zone
  bool readClusterLinkList(RagTime5Zone &zone,
                           RagTime5ClusterManager::Link const &link);
  //! try to read a cluster list link zone
  bool readClusterLinkList(RagTime5ClusterManager::Link const &link,
                           RagTime5ClusterManager::Link const &nameLink,
                           std::vector<int> &list, std::string const &name="");

  //! try to read a string zone ( zone with id1=21,id2=23:24)
  bool readString(RagTime5Zone &zone, std::string &string);
  //! try to read a unicode string zone
  bool readUnicodeString(RagTime5Zone &zone);
  //! try to read a int/long zone data
  bool readLongListWithSize(int dataId, int fSz, std::vector<long> &list, std::string const &zoneName="");
  //! try to read a positions zone in data
  bool readPositions(int posId, std::vector<long> &listPosition);
  //! try to read/get the list of long of a L_LongList
  bool readLongList(RagTime5ClusterManager::Link const &link, std::vector<long> &list);
  //! try to read a list of unicode string zone
  bool readUnicodeStringList(RagTime5ClusterManager::Link const &link, std::map<int, librevenge::RVNGString> &idToStringMap);

  //! try to read the document version zone
  bool readDocumentVersion(RagTime5Zone &zone);
  //! try to read the main cluster
  bool readClusterRootData(RagTime5ClusterManager::ClusterRoot &cluster);
  //! try to read the list of format
  bool readFormats(RagTime5ClusterManager::Cluster &cluster);
  //! try to read the field data
  bool readClusterFieldsData(RagTime5ClusterManager::Cluster &cluster);
  //! try to read the layout cluster (type 4001)
  bool readClusterLayoutData(RagTime5ClusterManager::ClusterLayout &cluster);
  //! try to read the pipeline cluster data
  bool readClusterPipelineData(RagTime5ClusterManager::Cluster &cluster);
  //! try to read the main doc info cluster data
  bool readDocInfoClusterData(RagTime5Zone &zone, MWAWEntry const &entry);
  //! try to read the unknown clusterA data
  bool readClusterScriptData(RagTime5ClusterManager::ClusterScript &cluster);
  //! try to read the unknown clusterA data
  bool readUnknownClusterAData(RagTime5ClusterManager::ClusterUnknownA &cluster);
  //! try to read the unknown clusterB data
  bool readUnknownClusterBData(RagTime5ClusterManager::Cluster &cluster);
  //! try to read the unknown clusterC data
  bool readUnknownClusterCData(RagTime5ClusterManager::Cluster &cluster);
  //! try to read the unknown clusterD data
  bool readUnknownClusterDData(RagTime5ClusterManager::ClusterUnknownD &cluster);
  //! try to read a list of unknown zone 6 bytes data
  bool readUnknZoneA(RagTime5Zone &zone, RagTime5ClusterManager::Link const &link);

  //! try to read a structured zone
  bool readStructZone(RagTime5ClusterManager::Cluster &cluster, RagTime5StructManager::FieldParser &parser, int headerSz);
  //! try to read a data in a structured zone
  bool readStructData(RagTime5Zone &zone, long endPos, int n, int headerSz,
                      RagTime5StructManager::FieldParser &parser, librevenge::RVNGString const &dataName);

  //! try to read a list zone
  bool readListZone(RagTime5ClusterManager::Link const &link);
  //! try to read a list zone
  bool readListZone(RagTime5ClusterManager::Link const &link, RagTime5StructManager::DataParser &parser);
  //! try to read a fixed size zone
  bool readFixedSizeZone(RagTime5ClusterManager::Link const &link, std::string const &name);
  //! try to read a fixed size zone
  bool readFixedSizeZone(RagTime5ClusterManager::Link const &link, RagTime5StructManager::DataParser &parser);
  //! flush unsent zone (debugging function)
  void flushExtra();

protected:

  //
  // data
  //
  //! the state
  shared_ptr<RagTime5ParserInternal::State> m_state;
  //! the graph manager
  shared_ptr<RagTime5Graph> m_graphParser;
  //! the text manager
  shared_ptr<RagTime5Text> m_textParser;
  //! the structure manager
  shared_ptr<RagTime5StructManager> m_structManager;
  //! the cluster manager
  shared_ptr<RagTime5ClusterManager> m_clusterManager;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
