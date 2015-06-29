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
 * Parser to RagTime 5-6 document ( spreadsheet part )
 *
 */
#ifndef RAGTIME5_SPREADSHEET
#  define RAGTIME5_SPREADSHEET

#include <string>
#include <map>
#include <vector>

#include <librevenge/librevenge.h>

#include "libmwaw_internal.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"
#include "MWAWPosition.hxx"

#include "RagTime5StructManager.hxx"
#include "RagTime5ClusterManager.hxx"

namespace RagTime5SpreadsheetInternal
{
struct State;
struct FieldParser;

class SubDocument;
}

class RagTime5Parser;
class RagTime5StructManager;
class RagTime5StyleManager;
class RagTime5Zone;

/** \brief the main class to read the spreadsheet part of RagTime 56 file
 *
 *
 *
 */
class RagTime5Spreadsheet
{
  friend class RagTime5SpreadsheetInternal::SubDocument;
  friend class RagTime5Parser;

public:
  //! constructor
  RagTime5Spreadsheet(RagTime5Parser &parser);
  //! destructor
  virtual ~RagTime5Spreadsheet();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  // interface with main parser

  //! try to read a spreadsheet cluster
  bool readSpreadsheetCluster(RagTime5Zone &zone, int zoneType);

  //! try to read a chart cluster
  bool readChartCluster(RagTime5Zone &zone, int zoneType);

  //
  // Intermediate level
  //

  //! try to read a spreadsheet unknown zone 1
  bool readUnknownZone1(RagTime5Zone &zone, RagTime5ClusterManager::Link const &link);
  //! try to read a spreadsheet unknown zone 2
  bool readUnknownZone2(RagTime5Zone &zone, RagTime5ClusterManager::Link const &link);
  //! try to read a spreadsheet unknown zone 3
  bool readUnknownZone3(RagTime5Zone &zone, RagTime5ClusterManager::Link const &link);
  //! try to read a spreadsheet unknown zone 4
  bool readUnknownZone4(RagTime5Zone &zone, RagTime5ClusterManager::Link const &link);

  //
  // basic
  //

  //
  // low level
  //


private:
  RagTime5Spreadsheet(RagTime5Spreadsheet const &orig);
  RagTime5Spreadsheet &operator=(RagTime5Spreadsheet const &orig);

protected:
  //
  // data
  //
  //! the parser
  RagTime5Parser &m_mainParser;

  //! the structure manager
  shared_ptr<RagTime5StructManager> m_structManager;
  //! the style manager
  shared_ptr<RagTime5StyleManager> m_styleManager;
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<RagTime5SpreadsheetInternal::State> m_state;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
