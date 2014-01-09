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
 * Parser to Mariner Write text document ( graphic part )
 *
 */
#ifndef MARINER_WRT_GRAPH
#  define MARINER_WRT_GRAPH

#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "libmwaw_internal.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

namespace MarinerWrtGraphInternal
{
struct State;
struct PSZone;
struct Token;
class SubDocument;
}

struct MarinerWrtEntry;
struct MarinerWrtStruct;
class MarinerWrtParser;

/** \brief the main class to read the graphic part of a Mariner Write file
 *
 *
 *
 */
class MarinerWrtGraph
{
  friend class MarinerWrtParser;
  friend class MarinerWrtGraphInternal::SubDocument;

public:
  //! constructor
  MarinerWrtGraph(MarinerWrtParser &parser);
  //! destructor
  virtual ~MarinerWrtGraph();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:
  //! try to send the page graphic
  bool sendPageGraphics();
  //! sends the data which have not yet been sent to the listener
  void flushExtra();
  //! return the pattern percent which corresponds to an id (or -1)
  float getPatternPercent(int id) const;

  //
  // Intermediate level
  //

  /** try to read a postscript zone */
  bool readPostscript(MarinerWrtEntry const &entry, int zoneId);

  /** try to read a token zone (can be a picture or a field) */
  bool readToken(MarinerWrtEntry const &entry, int zoneId);
  /** try to read the first token zone ( which can contains some field text )*/
  bool readTokenBlock0(MarinerWrtStruct const &data, MarinerWrtGraphInternal::Token &tkn, std::string &res);
  /** try to send a picture token as char */
  void sendPicture(MarinerWrtGraphInternal::Token const &tkn);
  /** try to send a rule */
  void sendRule(MarinerWrtGraphInternal::Token const &tkn);
  /** try to send a ps picture as pos */
  void sendPSZone(MarinerWrtGraphInternal::PSZone const &ps, MWAWPosition const &pos);

  // interface with mainParser

  //! try to send a token
  void sendToken(int zoneId, long tokenId);
  //! ask the main parser to send a text zone
  void sendText(int zoneId);

private:
  MarinerWrtGraph(MarinerWrtGraph const &orig);
  MarinerWrtGraph &operator=(MarinerWrtGraph const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<MarinerWrtGraphInternal::State> m_state;

  //! the main parser;
  MarinerWrtParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
