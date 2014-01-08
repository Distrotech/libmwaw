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

/*
 * Parser to convert FullWrite document
 *
 */
#ifndef FP_MWAW_PARSER
#  define FP_MWAW_PARSER

#include <list>
#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "MWAWInputStream.hxx"

#include "FullWrtStruct.hxx"
#include "MWAWParser.hxx"

namespace FullWrtParserInternal
{
struct State;
class SubDocument;
}

class FullWrtGraph;
class FullWrtText;

/** \brief the main class to read a FullWrite file
 *
 *
 *
 */
class FullWrtParser : public MWAWTextParser
{
  friend class FullWrtGraph;
  friend class FullWrtText;
  friend class FullWrtParserInternal::SubDocument;

public:
  //! constructor
  FullWrtParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~FullWrtParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(librevenge::RVNGTextInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! creates the listener which will be associated to the document
  void createDocument(librevenge::RVNGTextInterface *documentInterface);

  //! finds the different objects zones
  bool createZones();

  //! create the file zone ( first step of create zones)
  bool createFileZones();

  //! adds a new page
  void newPage(int number);

  //! find the last position of the document and read data
  bool readDocPosition();

  //! try to read the file zones main flags
  bool readFileZoneFlags(FullWrtStruct::EntryPtr zone);

  //! try to read the file zones position
  bool readFileZonePos(FullWrtStruct::EntryPtr zone);

  //! try to read the zone containing the data of each doc zone (ie. Zone0)
  bool readDocZoneData(FullWrtStruct::EntryPtr zone);

  //! try to read the zone which stores the structure of zone0, ...  (ie. Zone1)
  bool readDocZoneStruct(FullWrtStruct::EntryPtr zone);

  //! try to read zone2, a zone which stores the document information zone, ...
  bool readDocInfo(FullWrtStruct::EntryPtr zone);

  //! try to read the end of zone2 (only v2) ?
  bool readEndDocInfo(FullWrtStruct::EntryPtr zone);

  //! try to read the list of citation (at the end of doc info)
  bool readCitationDocInfo(FullWrtStruct::EntryPtr zone);

  //! try read the print info zone
  bool readPrintInfo(FullWrtStruct::EntryPtr zone);

  //
  // interface to the graph parser
  //

  //! returns the page left top point ( in inches)
  Vec2f getPageLeftTop() const;
  //! try to return a border corresponding to an id
  bool getBorder(int bId, FullWrtStruct::Border &border) const;

  //
  // interface to the text parser
  //

  //! try to send a footnote/endnote entry
  void sendText(int docId, libmwaw::SubDocumentType type, MWAWNote::Type which=MWAWNote::FootNote);
  //! try to send a graphic
  void sendGraphic(int docId);
  //! try to send a variable, in pratice do nothing
  void sendVariable(int docId);
  //! try to send a reference, in pratice do nothing
  void sendReference(int docId);
  //! ask the text parser to send a zone
  bool send(int fileId, MWAWColor fontColor=MWAWColor::black());

  //
  // interface ( mainly debugging function)
  //

  //! returns the number of zone struct
  int getNumDocZoneStruct() const;
  /** returns a the type of a document zone ( mainly used for debugging) */
  std::string getDocumentTypeName(int zId) const;

  //
  // low level
  //

  //! try to read the reference data
  bool readReferenceData(FullWrtStruct::EntryPtr zone);
  //! try to read the data of a zone which begins with a generic header
  bool readGenericDocData(FullWrtStruct::EntryPtr zone, FullWrtStruct::ZoneHeader &doc);

protected:
  //
  // data
  //
  //! the state
  shared_ptr<FullWrtParserInternal::State> m_state;

  //! the graph parser
  shared_ptr<FullWrtGraph> m_graphParser;
  //! the text parser
  shared_ptr<FullWrtText> m_textParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
