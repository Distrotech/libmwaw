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

#ifndef BW_SS_PARSER
#  define BW_SS_PARSER

#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "MWAWCell.hxx"
#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

namespace BWSSParserInternal
{
class SubDocument;

struct Cell;
struct Chart;
struct Spreadsheet;
struct State;
}

class BWStructManager;

/** \brief the main class to read a BeagleWorks spreadsheet file
 */
class BWSSParser : public MWAWSpreadsheetParser
{
  friend struct BWSSParserInternal::Chart;
  friend class BWSSParserInternal::SubDocument;
public:
  //! constructor
  BWSSParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~BWSSParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(librevenge::RVNGSpreadsheetInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! creates the listener which will be associated to the document
  void createDocument(librevenge::RVNGSpreadsheetInterface *documentInterface);

  //! returns the page left top point ( in inches)
  Vec2f getPageLeftTop() const;
  //! adds a new page
  void newPage(int number);

  //! try to send the main spreadsheet
  bool sendSpreadsheet();
  //! try to send a text zone
  bool sendText(MWAWEntry entry, bool headerFooter=false);
  //! try to send the page graphic
  bool sendPageFrames();
  //! try to send a frame
  bool sendFrame(BWStructManager::Frame const &frame);
  //! try to send a picture
  bool sendPicture(int pId, MWAWPosition const &pos,
                   librevenge::RVNGPropertyList frameExtras=librevenge::RVNGPropertyList());
protected:
  //! finds the different objects zones
  bool createZones();

  //! read the resource fork zone
  bool readRSRCZones();

  //
  // low level
  //

  // data fork similar than in BWParser ...

  //! read the print info zone
  bool readPrintInfo();

  // data fork

  //! read the document information ( pref + header/footer)
  bool readDocumentInfo();

  //! read the chart zone
  bool readChartZone();

  //! read a chart
  bool readChart();

  //! read the spreadsheet zone
  bool readSpreadsheet();

  //! read the spreadsheet row
  bool readRowSheet(BWSSParserInternal::Spreadsheet &sheet);

  //! read a cell row
  bool readCellSheet(BWSSParserInternal::Cell &cell);

  //! read an unknown zone ( which appears before and after the columns's width zone )
  bool readZone0();

  //! read the columns widths
  bool readColumnWidths(BWSSParserInternal::Spreadsheet &sheet);

  //! read the differents formula
  bool readFormula(BWSSParserInternal::Spreadsheet &sheet);

  // resource fork

  //! return the input input
  MWAWInputStreamPtr rsrcInput();

  //! a DebugFile used to write what we recognize when we parse the document in rsrc
  libmwaw::DebugFile &rsrcAscii();

  //
  // formula data
  //
  /* reads a cell */
  bool readCellInFormula(Vec2i actPos, MWAWCellContent::FormulaInstruction &instr);
  /* reads a formula */
  bool readFormula(long endPos, Vec2i const &pos,	std::vector<MWAWCellContent::FormulaInstruction> &formula, std::string &error);


  //! the state
  shared_ptr<BWSSParserInternal::State> m_state;

  //! the structure manager
  shared_ptr<BWStructManager> m_structureManager;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
