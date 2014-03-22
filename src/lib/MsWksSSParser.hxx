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

#ifndef MS_WKS_SS_PARSER
#  define MS_WKS_SS_PARSER

#include <list>
#include <string>
#include <vector>

#include "MWAWCell.hxx"
#include "MWAWDebug.hxx"
#include "MWAWEntry.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

namespace MsWksSSParserInternal
{
struct State;
struct Zone;

class Cell;
class SubDocument;
}

class MsWksGraph;
class MsWks3Text;
class MsWksDocument;

/** \brief the main class to read a Microsoft Works spreadsheet file
 *
 *
 *
 */
class MsWksSSParser : public MWAWSpreadsheetParser
{
  friend class MsWksSSParserInternal::SubDocument;
  friend class MsWksGraph;
public:
  //! constructor
  MsWksSSParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~MsWksSSParser();

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

  //! try to read the spreadsheet data zone
  bool readSSheetZone();
  //! try to read a generic zone
  bool readZone(MsWksSSParserInternal::Zone &zone);
  //! try to read the documentinfo ( zone2)
  bool readDocumentInfo(long sz=-1);
  //! try to read a group zone (zone3)
  bool readGroup(MsWksSSParserInternal::Zone &zone, MWAWEntry &entry, int check);
  //! try to read a header/footer group (v>=3)
  bool readGroupHeaderFooter(bool header);

  /** try to send a note */
  void sendNote(int id);

  /** try to send a text zone */
  void sendText(int id);

  /** try to send a zone */
  void sendZone(int zoneType);

  //! try to send the main spreadsheet
  bool sendSpreadsheet();

  //
  // low level
  //

  //! read the print info zone
  bool readPrintInfo();
  //!reads the end of the header
  bool readEndHeader();
  //!reads a cell content data
  bool readCell(int sz, Vec2i const &cellPos, MsWksSSParserInternal::Cell &cell);

  /** reads a cell */
  bool readCellInFormula(MWAWCellContent::FormulaInstruction &instr);

  /** try to read a string */
  bool readString(long endPos, std::string &res);
  /** try to read a number */
  bool readNumber(long endPos, double &res, bool &isNan, std::string &str);

  /* reads a formula */
  bool readFormula(long endPos, MWAWCellContent &content, std::string &extra);

protected:
  //
  // data
  //
  //! the state
  shared_ptr<MsWksSSParserInternal::State> m_state;

  //! the list of different Zones
  std::vector<MWAWEntry> m_listZones;

  //! the actual zone
  shared_ptr<MsWksDocument> m_document;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
