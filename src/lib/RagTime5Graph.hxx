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
 * Parser to RagTime 5-6 document ( graphic part )
 *
 */
#ifndef RAGTIME5_GRAPH
#  define RAGTIME5_GRAPH

#include <string>
#include <map>
#include <vector>

#include <librevenge/librevenge.h>

#include "libmwaw_internal.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"
#include "MWAWPosition.hxx"

#include "RagTime5StructManager.hxx"

namespace RagTime5GraphInternal
{
struct State;
struct FieldParser;

class SubDocument;
}

class RagTime5Parser;
class RagTime5StructManager;

class MWAWGraphicStyle;

/** \brief the main class to read the graphic part of RagTime 56 file
 *
 *
 *
 */
class RagTime5Graph
{
  friend struct RagTime5GraphInternal::FieldParser;
  friend class RagTime5GraphInternal::SubDocument;
  friend class RagTime5Parser;

public:
  //! constructor
  RagTime5Graph(RagTime5Parser &parser);
  //! destructor
  virtual ~RagTime5Graph();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  // interface with main parser


  //
  // Intermediate level
  //

  //
  // picture
  //

  //! try to read a picture zone
  bool readPicture(RagTime5StructManager::Zone &zone);
  //! try to read a picture list
  bool readPictureList(RagTime5StructManager::Zone &zone);
  //! try to read a picture match zone
  bool readPictureMatch(RagTime5StructManager::Zone &zone, bool color);

  //
  // basic graphic
  //

  //! try to read a main graphic types
  bool readGraphicTypes(RagTime5StructManager::Zone &zone, RagTime5StructManager::Link const &link);

  //! try to read a main graphic styles
  bool readGraphicStyles(RagTime5StructManager::Cluster &cluster);

  //! try to read a graphic color zone
  bool readGraphicColors(RagTime5StructManager::Cluster &cluster);

  //! try to read a graphic zone
  bool readGraphicZone(RagTime5StructManager::Cluster &cluster);
  //! try to read a graphic unknown zone in data
  bool readGraphicUnknown(int typeId);
  //! try to read a graphic
  bool readGraphic(RagTime5StructManager::Zone &dataZone, long endPos, int n, librevenge::RVNGString const &dataName);

  //! try to read a graphic transformations zone
  bool readGraphicTransformations(RagTime5StructManager::Zone &zone, RagTime5StructManager::Link const &link);

  //
  // low level
  //


private:
  RagTime5Graph(RagTime5Graph const &orig);
  RagTime5Graph &operator=(RagTime5Graph const &orig);

protected:
  //
  // data
  //
  //! the parser
  RagTime5Parser &m_mainParser;

  //! the structure manager
  shared_ptr<RagTime5StructManager> m_structManager;
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<RagTime5GraphInternal::State> m_state;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
