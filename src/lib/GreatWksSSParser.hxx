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

#ifndef GREAT_WKS_SS_PARSER
#  define GREAT_WKS_SS_PARSER

#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "MWAWCell.hxx"
#include "MWAWDebug.hxx"

#include "MWAWParser.hxx"

namespace GreatWksSSParserInternal
{
class Cell;
struct State;
class SubDocument;
}

class GreatWksDocument;

/** \brief the main class to read a GreatWorks spreadsheet file
 */
class GreatWksSSParser : public MWAWSpreadsheetParser
{
  friend class GreatWksSSParserInternal::SubDocument;
public:
  //! constructor
  GreatWksSSParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~GreatWksSSParser();

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

  // interface with the text parser

  //! try to send the i^th header/footer
  bool sendHF(int id);

protected:
  //! finds the different objects zones
  bool createZones();

  //
  // low level
  //

  //! try to read the styles
  bool readStyles();
  //! read the spreadsheet block ( many unknown data )
  bool readSpreadsheet();
  //! try to read the chart zone: v2
  bool readChart();
  //! try to read a cell
  bool readCell(GreatWksSSParserInternal::Cell &cell);

  /** reads a cell */
  bool readCellInFormula(Vec2i const &pos, MWAWCellContent::FormulaInstruction &instr);
  /** try to read a string */
  bool readString(long endPos, std::string &res);
  /** try to read a number */
  bool readNumber(long endPos, double &res, bool &isNan);
  //! read to read a formula
  bool readFormula(Vec2i const &cPos, long endPos,
                   std::vector<MWAWCellContent::FormulaInstruction> &formula, std::string &error);

  //
  // data
  //

  //! the state
  shared_ptr<GreatWksSSParserInternal::State> m_state;

  //! the document
  shared_ptr<GreatWksDocument> m_document;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
