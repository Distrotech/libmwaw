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
 * Parser to HanMac Word-J text document ( graphic part )
 *
 */
#ifndef HMWJ_GRAPH
#  define HMWJ_GRAPH

#include <string>
#include <vector>

#include <libwpd/libwpd.h>

#include "libmwaw_internal.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

class MWAWEntry;

class MWAWParserState;
typedef shared_ptr<MWAWParserState> MWAWParserStatePtr;

class MWAWPosition;

namespace HMWJGraphInternal
{
struct Frame;
struct State;
class SubDocument;
}

class HMWJParser;

/** \brief the main class to read the graphic part of a HanMac Word-J file
 *
 *
 *
 */
class HMWJGraph
{
  friend class HMWJParser;
  friend class HMWJGraphInternal::SubDocument;

public:
  //! constructor
  HMWJGraph(HMWJParser &parser);
  //! destructor
  virtual ~HMWJGraph();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:

  //! returns the color associated with a pattern
  bool getColor(int colId, int patternId, MWAWColor &color) const;

  //! try to send the page graphic
  bool sendPageGraphics();
  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //
  // Intermediate level
  //
  /** try to read the frames definition (type 3)*/
  bool readFrames(MWAWEntry const &entry);
  /** try to read a frame*/
  bool readFrame(HMWJGraphInternal::Frame &frame);
  /** try to read the pictures definition (type 6)*/
  bool readPicture(MWAWEntry const &entry);
  /** try to read a table (zone 7)*/
  bool readTable(MWAWEntry const &entry);

  // interface with mainParser


  //
  // low level
  //


private:
  HMWJGraph(HMWJGraph const &orig);
  HMWJGraph &operator=(HMWJGraph const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<HMWJGraphInternal::State> m_state;

  //! the main parser;
  HMWJParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
