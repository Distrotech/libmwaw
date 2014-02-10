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
 * Parser to convert spreadsheet Wingz document
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
 * \note need more file to be sure to treat all document,
 * ie. the structure of many zones is very uncertain, so ...
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

  //! finds the different objects zones
  bool createZones();

  //! read the print info zone
  bool readPrintInfo();

  //
  // low level
  //

  //! read the header zone ( a unknown zone)
  bool readZoneA();
  //! read a list of fonts
  bool readFontsList();
  //! read the second unknown zone
  bool readZoneB();
  //! read the spreadshet zone
  bool readSpreadsheet();
  //! read some spreadsheet size (col/row size or page break)
  bool readSpreadsheetSize();
  //! read a style in a spreadsheet
  bool readSpreadsheetStyle();
  //! read a spreadsheet style name
  bool readSpreadsheetStyleName();
  //! read a spreadsheet list of cell
  bool readSpreadsheetCellList();
  //! read a spreedsheet macro
  bool readSpreadsheetMacro();
  //! read a spreadsheet unknown zone 5
  bool readSpreadsheetZone5();
  //! read a graphic zone
  bool readGraphic();
  //! read a chart ( some graphic zone)
  bool readChartData();
  //! read a simple text zone data ( some graphic zone)
  bool readSimpleTextData();
  //! read a complex text zone data ( some graphic zone)
  bool readComplexTextData();
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
