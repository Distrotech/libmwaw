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

#ifndef MS_WKS_DB_PARSER
#  define MS_WKS_DB_PARSER

#include <list>
#include <string>
#include <vector>

#include "MWAWCell.hxx"
#include "MWAWDebug.hxx"
#include "MWAWEntry.hxx"
#include "MWAWInputStream.hxx"

#include "MsWksDocument.hxx"

#include "MWAWParser.hxx"

namespace MsWksDBParserInternal
{
class Form;
struct State;

class SubDocument;
}

class MsWksDocument;

/** \brief the main class to read a Microsoft Works database file and convert it in a spreadsheet file
 *
 *
 *
 */
class MsWksDBParser : public MWAWSpreadsheetParser
{
  friend class MsWksDBParserInternal::SubDocument;
public:
  //! constructor
  MsWksDBParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~MsWksDBParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(librevenge::RVNGSpreadsheetInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! creates the listener which will be associated to the document
  void createDocument(librevenge::RVNGSpreadsheetInterface *documentInterface);

  //! finds the different objects zones
  bool createZones();

  //
  // intermediate level
  //

  //! try to read the database zone
  bool readDataBase();

  //! try to send the main database
  bool sendDatabase();

  //
  // low level
  //

  ////////////////////////////////////////
  //     FIELD/RECORD/FILTERS
  ////////////////////////////////////////

  /** reads the list of the fields type v3-v4*/
  bool readFieldTypes();
  /** reads the list of the fields type v2 */
  bool readFieldTypesV2();
  /** reads the database contents: field's names and values

  \note if onlyCheck = true, only check if the zone is ok but do nothing */
  bool readRecords(bool onlyCheck);
  /** reads the filters */
  bool readFilters();
  /** reads the list of the columns size */
  bool readColSize(std::vector<int> &colSize);

  /** reads the default value */
  bool readDefaultValues();
  /** reads the formula value */
  bool readFormula();
  /** reads the serial value */
  bool readSerialFormula();


  ////////////////////////////////////////
  //     FORM
  ////////////////////////////////////////
  /** reads all the form */
  bool readForms();
  /** reads a list of dimension(?) corresponding to the fields type v2 */
  bool readFormV2();
  /** reads one form */
  bool readForm();
  /** read a form types */
  bool readFormTypes(MsWksDBParserInternal::Form &form);

  ////////////////////////////////////////
  //     REPORT
  ////////////////////////////////////////
  /** reads the reports */
  bool readReports();
  /** read a report header */
  bool readReportHeader();
  /** reads a report zone in v.2 file */
  bool readReportV2();

  ////////////////////////////////////////
  //     UNKNOWN
  ////////////////////////////////////////
  /** reads an unknown zone V2

   \note this header looks like the form v3-4 header, but
   is it really the form header ?*/
  bool readUnknownV2();


protected:
  //
  // data
  //
  //! the state
  shared_ptr<MsWksDBParserInternal::State> m_state;

  //! the list of different Zones
  std::vector<MWAWEntry> m_listZones;

  //! the actual zone
  shared_ptr<MsWksDocument> m_document;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
