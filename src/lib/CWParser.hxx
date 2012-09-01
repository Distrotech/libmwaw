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

#include <list>
#include <string>
#include <vector>

#include "MWAWPageSpan.hxx"

#include "MWAWPosition.hxx"

#include "MWAWEntry.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

typedef class MWAWContentListener CWContentListener;
typedef shared_ptr<CWContentListener> CWContentListenerPtr;

class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

namespace CWParserInternal
{
struct State;
class SubDocument;
}

namespace CWStruct
{
struct DSET;
}

class CWDatabase;
class CWGraph;
class CWSpreadsheet;
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
  friend class CWSpreadsheet;
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
  void parse(WPXDocumentInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! sets the listener in this class and in the helper classes
  void setListener(CWContentListenerPtr listen);

  //! creates the listener which will be associated to the document
  void createDocument(WPXDocumentInterface *documentInterface);

  //! finds the different objects zones
  bool createZones();

  /** try to find the zone tree graph... and if possible, returns the
      main zone id */
  int findZonesGraph();

  /** find the number of children of a zone */
  int computeNumChildren(int zoneId) const;

  //! read a zone
  bool readZone();

  //! read the print info zone
  bool readPrintInfo();

  //! try to read a structured zone
  bool readStructZone(char const *zoneName, bool hasEntete);

  //! returns the page height, ie. paper size less margin (in inches)
  float pageHeight() const;
  //! returns the page width, ie. paper size less margin (in inches)
  float pageWidth() const;

  //! adds a new page
  void newPage(int number);

  //
  // interface with the text parser
  //

  //! send a zone
  bool sendZone(int zoneId);

  /** creates a document to send a footnote */
  void sendFootnote(int zoneId);

  //
  // interface with the graph parser
  //

  //! returns the color corresponding to colId (if possible)
  bool getColor(int colId, Vec3uc &col) const;

  //
  // low level
  //

  //! reads the document header
  bool readDocHeader();

  //! reads the end table ( appears in v3.0 : file version ? )
  bool readEndTable();

  /** reads the zone DSET

  set complete to true if we read all the zone */
  shared_ptr<CWStruct::DSET> readDSET(bool &complete);

  // THE NAMED ENTRY

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
  //! the listener
  CWContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<CWParserInternal::State> m_state;

  //! the actual document size
  MWAWPageSpan m_pageSpan;

  //! a flag to know if pageSpan is filled
  bool m_pageSpanSet;

  //! the database parser
  shared_ptr<CWDatabase> m_databaseParser;

  //! the graph parser
  shared_ptr<CWGraph> m_graphParser;

  //! the spreadsheet parser
  shared_ptr<CWSpreadsheet> m_spreadsheetParser;

  //! the table parser
  shared_ptr<CWTable> m_tableParser;

  //! the text parser
  shared_ptr<CWText> m_textParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
