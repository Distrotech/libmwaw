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
 * Parser to Claris Works text document
 *
 */
#ifndef CLARIS_WKS_PARSER
#  define CLARIS_WKS_PARSER

#include <set>
#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "MWAWEntry.hxx"
#include "MWAWPosition.hxx"

#include "MWAWParser.hxx"

#include "ClarisWksStruct.hxx"

namespace ClarisWksParserInternal
{
struct State;
class SubDocument;
}

class ClarisWksDocument;

/** \brief the main class to read a Claris Works file
 *
 *
 *
 */
class ClarisWksParser : public MWAWTextParser
{
  friend class ClarisWksParserInternal::SubDocument;
  friend class ClarisWksDocument;

public:
  //! constructor
  ClarisWksParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~ClarisWksParser();

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

  //! return the zone corresponding to an id ( low level)
  shared_ptr<ClarisWksStruct::DSET> getZone(int zId) const;

  /** try to find the zone dags structure... */
  bool exploreZonesGraph();
  /** try to find the zone tree graph ( DSF) function*/
  bool exploreZonesGraphRec(int zId, std::set<int> &notDoneList);

  /** try to type the main zones */
  void typeMainZones();

  /** try to type the main zones recursif, returns the father id*/
  int typeMainZonesRec(int zId, ClarisWksStruct::DSET::Type type, int maxHeight);

  //! read a zone
  bool readZone();

  //! adds a new page
  void newPage(int number);

  //
  // interface with the text parser
  //

  //! check if we can send a zone as a graphic
  bool canSendZoneAsGraphic(int number) const;
  //! send a zone
  bool sendZone(int zoneId, bool asGraphic, MWAWPosition pos=MWAWPosition());
  //! indicate that a zone is already parsed
  void forceParsed(int zoneId);

  /** creates a document to send a footnote */
  void sendFootnote(int zoneId);

  //
  // low level
  //

  //! reads the end table ( appears in v3.0 : file version ? )
  bool readEndTable();

  /** reads the zone DSET

  set complete to true if we read all the zone */
  shared_ptr<ClarisWksStruct::DSET> readDSET(bool &complete);

  // THE NAMED ENTRY

  /* read the list of mark */
  bool readMARKList(MWAWEntry const &entry);

  /* read a URL mark */
  bool readURL(long endPos);

  /* read a bookmark mark */
  bool readBookmark(long endPos);

  /* read a document mark */
  bool readDocumentMark(long endPos);

  /* read a end mark */
  bool readEndMark(long endPos);

  /* read the document summary */
  bool readDSUM(MWAWEntry const &entry, bool inHeader);

  /* read the temporary file name ? */
  bool readTNAM(MWAWEntry const &entry);

  /* SNAP (in v6) : size[4]/size[2] picture... */
  bool readSNAP(MWAWEntry const &entry);

  /* sequence of plist of printer : in v6
   */
  bool readCPRT(MWAWEntry const &entry);

protected:


  //
  // data
  //
  //! the state
  shared_ptr<ClarisWksParserInternal::State> m_state;

  //! the style manager
  shared_ptr<ClarisWksDocument> m_document;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
