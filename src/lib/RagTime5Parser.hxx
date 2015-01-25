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

#include <string>

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

#include "RagTime5StructManager.hxx"

namespace RagTime5ParserInternal
{
struct State;
struct FieldParser;
class SubDocument;
}

class RagTime5Graph;
class RagTime5Text;
class RagTime5StructManager;

/** \brief the main class to read a RagTime v5 file
 *
 *
 *
 */
class RagTime5Parser : public MWAWTextParser
{
  friend class RagTime5Graph;
  friend class RagTime5Text;
  friend struct RagTime5ParserInternal::FieldParser;
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
  shared_ptr<RagTime5StructManager::Zone> getDataZone(int dataId) const;
  //! try to update a zone: create a new input if the zone is stored in different positions, ...
  bool update(RagTime5StructManager::Zone &zone);
  //! try to read the zone data
  bool readZoneData(RagTime5StructManager::Zone &zone);
  //! try to unpack a zone
  bool unpackZone(RagTime5StructManager::Zone &zone, MWAWEntry const &entry, std::vector<unsigned char> &data);
  //! try to unpack a zone
  bool unpackZone(RagTime5StructManager::Zone &zone);

  //! try to read the different cluster zones
  bool readClusterZones();
  //! try to read a cluster zone
  bool readClusterZone(RagTime5StructManager::Zone &zone);
  //! try to read a cluster list (in general Data14)
  bool readClusterList(RagTime5StructManager::Zone &zone);

  //! try to read a string zone ( zone with id1=21,id2=23:24)
  bool readString(RagTime5StructManager::Zone &zone, std::string &string);
  //! try to read a unicode string zone
  bool readUnicodeString(RagTime5StructManager::Zone &zone);
  //! try to read a positions zone in data
  bool readPositions(int posId, std::vector<long> &listPosition);
  //! try to read a list of unicode string zone
  bool readUnicodeStringList(RagTime5StructManager::Zone &zone, RagTime5StructManager::Link const &link);
  //! try to read a list of unknown zone 6 bytes data
  bool readUnknZoneA(RagTime5StructManager::Zone &zone, RagTime5StructManager::Link const &link);

  //! try to read the document version zone
  bool readDocumentVersion(RagTime5StructManager::Zone &zone);

  //! try to read a structured zone
  bool readStructZone(RagTime5StructManager::Cluster &cluster, RagTime5StructManager::FieldParser &parser);
  //! try to read a data in a structured zone
  bool readStructData(RagTime5StructManager::Zone &zone, long endPos, int n, RagTime5StructManager::FieldParser &parser);

  //! try to read a data in a structured zone
  bool readStructData(RagTime5StructManager::Zone &zone, long endPos, int n, std::string const &zoneName);
  //! try to read a list zone
  bool readListZone(RagTime5StructManager::Zone &zone, RagTime5StructManager::Link const &link);
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
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
