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
 * Parser to Claris Works text document ( table part )
 *
 */
#ifndef CW_TABLE
#  define CW_TABLE

#include <list>
#include <string>
#include <vector>

#include "MWAWEntry.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

#include "CWStruct.hxx"

namespace CWTableInternal
{
struct Cell;
struct Table;
struct State;
}

class CWParser;
class CWStyleManager;

/** \brief the main class to read the text part of Claris Works file
 *
 *
 *
 */
class CWTable
{
  friend class CWParser;
  friend struct CWTableInternal::Cell;
  friend struct CWTableInternal::Table;

public:
  //! constructor
  CWTable(CWParser &parser);
  //! destructor
  virtual ~CWTable();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

  //! reads the zone Text DSET
  shared_ptr<CWStruct::DSET> readTableZone
  (CWStruct::DSET const &zone, MWAWEntry const &entry, bool &complete);

  //! update the cell
  void updateCell(CWTableInternal::Cell const &cell, MWAWCell &rCell, WPXPropertyList &pList);

protected:
  //! sends the zone data to the listener (if it exists )
  bool sendZone(int number);

  //! ask the main parser to send a zone
  bool askMainToSendZone(int number);

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //
  // Intermediate level
  //

  //! try to read the table border
  bool readTableBorders(CWTableInternal::Table &table);

  //! try to read the table cells
  bool readTableCells(CWTableInternal::Table &table);

  //! try to read the table border
  bool readTableBordersId(CWTableInternal::Table &table);

  //! try to read a list of pointer ( unknown meaning )
  bool readTablePointers(CWTableInternal::Table &table);

private:
  CWTable(CWTable const &orig);
  CWTable &operator=(CWTable const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<CWTableInternal::State> m_state;

  //! the main parser;
  CWParser *m_mainParser;

  //! the style manager
  shared_ptr<CWStyleManager> m_styleManager;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
