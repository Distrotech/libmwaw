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
 * Parser to Microsoft Works text document ( table and chart )
 */
#ifndef MSK_MWAW_TABLE
#  define MSK_MWAW_TABLE

#include <list>
#include <string>
#include <vector>

#include "MWAWEntry.hxx"
#include "MWAWParser.hxx"

#include "MSKGraph.hxx"

namespace MSKTableInternal
{
struct Table;
struct State;
}

class MSKParser;

/** \brief the main class to read the table ( or a chart ) of a Microsoft Works file */
class MSKTable
{
  friend class MSKGraph;
public:
  //! constructor
  MSKTable(MSKParser &parser, MSKGraph &graph);
  //! destructor
  virtual ~MSKTable();

  /** returns the file version */
  int version() const;

  //! try to read a table zone
  bool readTable(int numCol, int numRow, int zoneId, MSKGraph::Style const &style);
  //! try to  a table zone
  bool sendTable(int zoneId);

  //! try to read a chart zone
  bool readChart(int chartId, MSKGraph::Style const &style);
  //! fix the correspondance between a chart and the zone id
  void setChartZoneId(int chartId, int zoneId);
  //! try to  a chart zone
  bool sendChart(int chartId);

private:
  MSKTable(MSKTable const &orig);
  MSKTable &operator=(MSKTable const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<MSKTableInternal::State> m_state;

  //! the main parser;
  MSKParser *m_mainParser;
  //! the graph parser;
  MSKGraph *m_graphParser;
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:

