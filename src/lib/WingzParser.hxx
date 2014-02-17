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
 * Parser to convert spreadsheet ClarisResolve/Wingz document
 *
 */
#ifndef WINGZ_PARSER
#  define WINGZ_PARSER

#include <vector>

#include "MWAWDebug.hxx"

#include "MWAWParser.hxx"

namespace WingzParserInternal
{
struct State;
}

/** \brief the main class to read a Wingz document
 *
 * \note need more files to be sure to treat all documents, ie. as
 * some flags (defining the following type's field) can appear in many
 * structures, only having a lot of documents can allow to find all
 * this type's fields...
 *
 */
class WingzParser : public MWAWSpreadsheetParser
{
public:
  //! constructor
  WingzParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~WingzParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(librevenge::RVNGSpreadsheetInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! creates the listener which will be associated to the document
  void createDocument(librevenge::RVNGSpreadsheetInterface *documentInterface);

  //! try to send the main spreadsheet
  bool sendSpreadsheet();

  //! finds the different objects zones
  bool createZones();

  //! read the print info zone
  bool readPrintInfo();

  //
  // low level
  //

  //! read the preference zone ( the first zone)
  bool readPreferences();
  //! read the spreadshet zone
  bool readSpreadsheet();
  //! read some spreadsheet size (col/row size or page break)
  bool readSpreadsheetSize();
  //! read a page break zone
  bool readSpreadsheetPBreak();
  //! read a style in a spreadsheet
  bool readSpreadsheetStyle();
  //! read a spreadsheet cell name
  bool readSpreadsheetCellName();
  //! read a formula in a spreadsheet
  bool readFormula();
  //! read a spreadsheet list of cell
  bool readSpreadsheetCellList();
  //! read a spreadsheet unknown zone 5
  bool readSpreadsheetZone5();
  //! read a graphic zone
  bool readGraphic();
  //! read a chart ( some graphic zone)
  bool readChartData();
  //! read a text zone or a button zone ( some graphic zone)
  bool readTextZone();
  //! read a macro in a text zone ( some graphic zone)
  bool readMacro();

  //! try to find the next spreadsheet zone
  bool findNextZone(int lastType=0);
protected:
  //
  // data
  //

  //! the state
  shared_ptr<WingzParserInternal::State> m_state;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
