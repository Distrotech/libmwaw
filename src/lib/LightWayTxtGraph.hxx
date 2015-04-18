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
 * Parser to LightWay Text document ( graphic part )
 *
 */
#ifndef LIGHT_WAY_TXT_GRAPH
#  define LIGHT_WAY_TXT_GRAPH

#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "libmwaw_internal.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

namespace LightWayTxtGraphInternal
{
struct State;
}

class LightWayTxtParser;

/** \brief the main class to read the graphic part of a LightWay Text file
 *
 *
 *
 */
class LightWayTxtGraph
{
  friend class LightWayTxtParser;

public:
  //! constructor
  LightWayTxtGraph(LightWayTxtParser &parser);
  //! destructor
  virtual ~LightWayTxtGraph();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:
  //! finds the different graphic zones
  bool createZones();

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //! try to send the page graphic
  bool sendPageGraphics();

  //! try to send a graph
  void send(int id);

  //
  // Intermediate level
  //

  //! try to send a JPEG resource
  bool sendJPEG(MWAWEntry const &entry);

  //! try to send a PICT resource
  bool sendPICT(MWAWEntry const &entry);

  //
  // low level
  //

  //! try to find a JPEG size
  static bool findJPEGSize(librevenge::RVNGBinaryData const &data, MWAWVec2i &sz);

private:
  LightWayTxtGraph(LightWayTxtGraph const &orig);
  LightWayTxtGraph &operator=(LightWayTxtGraph const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<LightWayTxtGraphInternal::State> m_state;

  //! the main parser;
  LightWayTxtParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
