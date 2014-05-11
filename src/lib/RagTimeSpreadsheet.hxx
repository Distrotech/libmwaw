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

#include "MWAWEntry.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

namespace RagTimeSpreadsheetInternal
{
struct SpreadsheetZone;

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
  //! try to read a spreadsheet zone: v3-...
  bool readSpreadsheet(MWAWEntry &entry);
  //! try to read spreadsheet zone ( a big zone):v2
  bool readSpreadsheetV2(MWAWEntry &entry);
  //! try to read spreadsheet cells :v2
  bool readSpreadsheetCellsV2(MWAWEntry &entry);
  //! try to read spreadsheet end zone (positions, ...) :v2
  bool readSpreadsheetExtraV2(MWAWEntry &entry);

  //
  // low level
  //


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
