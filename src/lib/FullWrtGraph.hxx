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
 * Parser to FullWrite Text document ( graphic part )
 *
 */
#ifndef FULL_WRT_GRAPH
#  define FULL_WRT_GRAPH

#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "libmwaw_internal.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "FullWrtStruct.hxx"

namespace FullWrtGraphInternal
{
struct SideBar;
struct State;
class SubDocument;
}

class FullWrtParser;

/** \brief the main class to read the graphic part of a FullWrite Text file
 *
 *
 *
 */
class FullWrtGraph
{
  friend class FullWrtParser;
  friend class FullWrtGraphInternal::SubDocument;

public:
  //! constructor
  FullWrtGraph(FullWrtParser &parser);
  //! destructor
  virtual ~FullWrtGraph();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:
  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //! try to send the page graphic
  bool sendPageGraphics();

  //! try to return a border corresponding to an id
  bool getBorder(int bId, FullWrtStruct::Border &border) const;

  //
  // Intermediate level
  //

  // interface with main parser

  //! try to send the child of a zone
  bool send(int fileId, MWAWColor const &fontColor);

  //
  // low level
  //

  //! try to read the border definiton (at the end of doc info)
  bool readBorderDocInfo(FullWrtStruct::EntryPtr zone);

  //! try to read a sidebar data (zone 13 or zone 14)
  shared_ptr<FullWrtStruct::ZoneHeader> readSideBar
  (FullWrtStruct::EntryPtr zone, FullWrtStruct::ZoneHeader const &doc);
  //! try to read the sidebar position zone
  bool readSideBarPosition(FullWrtStruct::EntryPtr zone, FullWrtGraphInternal::SideBar &frame);
  //! try to read the sidebar second zone
  bool readSideBarFormat(FullWrtStruct::EntryPtr zone, FullWrtGraphInternal::SideBar &frame);
  //! try to read the sidebar third zone
  bool readSideBarUnknown(FullWrtStruct::EntryPtr zone, FullWrtGraphInternal::SideBar &frame);
  //! try to send a sidebar
  bool sendSideBar(FullWrtGraphInternal::SideBar const &frame);
  //! check if a zone is a graphic zone
  bool readGraphic(FullWrtStruct::EntryPtr zone);
  //! send a graphic knowing the graphic fileId
  bool sendGraphic(int fId);
  //! send a graphic to a listener (if it exists)
  bool sendGraphic(FullWrtStruct::EntryPtr zone);

  //! try to read the graphic data
  shared_ptr<FullWrtStruct::ZoneHeader> readGraphicData
  (FullWrtStruct::EntryPtr zone, FullWrtStruct::ZoneHeader &doc);
private:
  FullWrtGraph(FullWrtGraph const &orig);
  FullWrtGraph &operator=(FullWrtGraph const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<FullWrtGraphInternal::State> m_state;

  //! the main parser;
  FullWrtParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
