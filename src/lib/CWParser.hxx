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
#ifndef CW_MWAW_PARSER
#  define CW_MWAW_PARSER

#include <set>
#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "MWAWEntry.hxx"
#include "MWAWPosition.hxx"

#include "MWAWParser.hxx"

#include "CWStruct.hxx"

namespace CWParserInternal
{
struct State;
class SubDocument;
}

class CWDatabase;
class CWGraph;
class CWPresentation;
class CWSpreadsheet;
class CWStyleManager;
class CWTable;
class CWText;

/** \brief the main class to read a Claris Works file
 *
 *
 *
 */
class CWParser : public MWAWParser
{
  friend class CWParserInternal::SubDocument;
  friend class CWDatabase;
  friend class CWGraph;
  friend class CWPresentation;
  friend class CWSpreadsheet;
  friend class CWStyleManager;
  friend class CWTable;
  friend class CWText;

public:
  //! constructor
  CWParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~CWParser();

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
  shared_ptr<CWStruct::DSET> getZone(int zId) const;

  /** try to find the zone dags structure... */
  bool exploreZonesGraph();
  /** try to find the zone tree graph ( DSF) function*/
  bool exploreZonesGraphRec(int zId, std::set<int> &notDoneList);

  /** try to type the main zones */
  void typeMainZones();

  /** try to type the main zones recursif, returns the father id*/
  int typeMainZonesRec(int zId, CWStruct::DSET::Type type, int maxHeight);

  //! read a zone
  bool readZone();

  //! read the print info zone
  bool readPrintInfo();

  //! try to read a structured zone
  bool readStructZone(char const *zoneName, bool hasEntete);

  /** try to read a int structured zone
      where \a fSz to the int size: 1(int8), 2(int16), 4(int32) */
  bool readStructIntZone(char const *zoneName, bool hasEntete, int fSz, std::vector<int> &res);

  //! returns the number of expected pages ( accross pages x down page)
  Vec2i getDocumentPages() const;
  //! returns the page height, ie. paper size less margin (in inches) less header/footer size
  double getTextHeight() const;
  //! returns the page left top point ( in inches)
  Vec2f getPageLeftTop() const;
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

  //! returns the columns information
  MWAWSection getMainSection() const;

  //
  // interface with the graph parser
  //

  //! returns the header/footer id
  void getHeaderFooterId(int &headerId, int &footerId) const;

  //
  // low level
  //

  //! reads the document header
  bool readDocHeader();

  //! reads the document info part ( end of the header)
  bool readDocInfo();

  //! reads the end table ( appears in v3.0 : file version ? )
  bool readEndTable();

  /** reads the zone DSET

  set complete to true if we read all the zone */
  shared_ptr<CWStruct::DSET> readDSET(bool &complete);

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

  /** small fonction used to check unusual endian ordering of a list of int16_t, int32_t*/
  void checkOrdering(std::vector<int16_t> &vec16, std::vector<int32_t> &vec32) const;

protected:


  //
  // data
  //
  //! the state
  shared_ptr<CWParserInternal::State> m_state;

  //! a flag to know if pageSpan is filled
  bool m_pageSpanSet;

  //! the database parser
  shared_ptr<CWDatabase> m_databaseParser;

  //! the graph parser
  shared_ptr<CWGraph> m_graphParser;

  //! the spreadsheet parser
  shared_ptr<CWPresentation> m_presentationParser;

  //! the spreadsheet parser
  shared_ptr<CWSpreadsheet> m_spreadsheetParser;

  //! the style manager
  shared_ptr<CWStyleManager> m_styleManager;

  //! the table parser
  shared_ptr<CWTable> m_tableParser;

  //! the text parser
  shared_ptr<CWText> m_textParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
