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
 * Parser to spreadsheet's part of a RagTime document
 *
 */
#ifndef RAGTIME_SPREADSHEET
#  define RAGTIME_SPREADSHEET

#include <list>
#include <string>
#include <vector>

#include "libmwaw_internal.hxx"

#include "MWAWCell.hxx"
#include "MWAWDebug.hxx"
#include "MWAWEntry.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

namespace RagTimeSpreadsheetInternal
{
struct ComplexBlock;

struct Cell;
struct Spreadsheet;

struct State;
}

class RagTimeParser;

/** \brief the main class to read the spreadsheet part of ragTime file
 *
 *
 *
 */
class RagTimeSpreadsheet
{
  friend class RagTimeParser;
public:
  //! constructor
  RagTimeSpreadsheet(RagTimeParser &parser);
  //! destructor
  virtual ~RagTimeSpreadsheet();

  /** returns the file version */
  int version() const;

protected:
  // generic resource ( used mainly by spreadsheet )

  //! returns the ith date format or ""
  bool getDateTimeFormat(int dtId, std::string &dtFormat) const;
  //! try to read the numeric/date format table zone: FoTa
  bool readNumericFormat(MWAWEntry &entry);

  // specific spreadsheet resource

  //! try to read a SpXX resource
  bool readResource(MWAWEntry &entry);
  //! try to read the SpDo zone (a spreadsheet zone with id=0)
  bool readRsrcSpDo(MWAWEntry &entry);
  //! try to read the SpDI zone (a spreadsheet zone zone with id=0)
  bool readRsrcSpDI(MWAWEntry &entry);

  //

  //! try to read a spreadsheet zone: v3-...
  bool readSpreadsheet(MWAWEntry &entry);
  //! try to read a the last spreadsheet zone
  bool readSpreadsheetZone9(MWAWEntry const &entry, RagTimeSpreadsheetInternal::Spreadsheet &sheet);
  //! try to read a simple structured spreadsheet zone
  bool readSpreadsheetSimpleStructure(MWAWEntry const &entry, RagTimeSpreadsheetInternal::Spreadsheet &sheet);
  //! try to read a complex structured spreadsheet zone
  bool readSpreadsheetComplexStructure(MWAWEntry const &entry, RagTimeSpreadsheetInternal::Spreadsheet &sheet);

  //! try to read a spreadsheet cells content
  bool readSpreadsheetCellContent(Vec2i const &cellPos, long endPos, RagTimeSpreadsheetInternal::Spreadsheet &sheet);
  //! try to read a spreadsheet cell's format
  bool readSpreadsheetCellFormat(Vec2i const &cellPos, long endPos, RagTimeSpreadsheetInternal::Spreadsheet &sheet);

  //! try to read a list of position
  bool readPositionsList(MWAWEntry const &entry, std::vector<long> &posList, long &lastDataPos);
  //! try to read a complex bock header
  bool readBlockHeader(MWAWEntry const &entry, RagTimeSpreadsheetInternal::ComplexBlock &block);

  //! try to read spreadsheet zone ( a big zone):v2
  bool readSpreadsheetV2(MWAWEntry &entry);
  //! try to read spreadsheet cells :v2
  bool readSpreadsheetCellsV2(MWAWEntry &entry, RagTimeSpreadsheetInternal::Spreadsheet &sheet);
  //! try to read spreadsheet end zone (positions, ...) :v2
  bool readSpreadsheetExtraV2(MWAWEntry &entry, RagTimeSpreadsheetInternal::Spreadsheet &sheet);

  //! send a spreadsheet corresponding to zId
  bool send(int zId, MWAWPosition const &pos);
  //! flush extra data
  void flushExtra();

  //
  // low level
  //

  //! try to read a cell :v2
  bool readSpreadsheetCellV2(RagTimeSpreadsheetInternal::Cell &cell, long endPos);
  //! try to read a formula
  bool readFormulaV2(Vec2i const &cellPos, std::vector<MWAWCellContent::FormulaInstruction> &formula, long endPos, std::string &extra);
  //! try to read a formula: v3...
  bool readFormula(Vec2i const &cellPos, std::vector<MWAWCellContent::FormulaInstruction> &formula, long endPos, std::string &extra);
  //! try to read a cell in a formula
  bool readCellInFormulaV2(Vec2i const &pos, bool canBeList, MWAWCellContent::FormulaInstruction &instr, long endPos, std::string &extra);
  //! try to read a cell in a formula
  bool readCellInFormula(Vec2i const &pos, bool canBeList, MWAWCellContent::FormulaInstruction &instr, long endPos, std::string &extra);
  //! send a spreadsheet to a listener
  bool send(RagTimeSpreadsheetInternal::Spreadsheet &sheet, MWAWSpreadsheetListenerPtr listener);

private:
  RagTimeSpreadsheet(RagTimeSpreadsheet const &orig);
  RagTimeSpreadsheet &operator=(RagTimeSpreadsheet const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<RagTimeSpreadsheetInternal::State> m_state;

  //! the main parser;
  RagTimeParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
