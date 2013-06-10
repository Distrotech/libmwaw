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
 * Parser to GreatWorks text document ( graphic part )
 *
 */
#ifndef GW_GRAPH
#  define GW_GRAPH

#include <set>
#include <string>
#include <vector>

#include <libwpd/libwpd.h>

#include "libmwaw_internal.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

namespace GWGraphInternal
{
struct Frame;

struct State;
class SubDocument;
}

class GWParser;

/** \brief the main class to read the graphic part of a HanMac Word-J file
 *
 *
 *
 */
class GWGraph
{
  friend class GWParser;
  friend class GWGraphInternal::SubDocument;

public:
  //! constructor
  GWGraph(GWParser &parser);
  //! destructor
  virtual ~GWGraph();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:
  //! try to send the page graphic
  bool sendPageGraphics();
  //! sends the data which have not yet been sent to the listener
  void flushExtra();
  //! try to send a frame
  bool sendFrame(shared_ptr<GWGraphInternal::Frame> frame, int order);

  //
  // Intermediate level
  //

  // RSRC
  //! read a pattern list block ( PAT# resource block )
  bool readPatterns(MWAWEntry const &entry);
  //! read a list of color and maybe patterns ( PlTT resource block: v2 )
  bool readPalettes(MWAWEntry const &entry);

  // DataFork: pict
  //! try to send the textbox text
  bool sendTextbox(MWAWEntry const &entry);
  //! try to send a picture
  bool sendPicture(MWAWEntry const &entry, MWAWPosition pos);

  // DataFork: graphic zone

  //! try to read the graphic zone ( draw file or end of v2 text file)
  bool readGraphicZone();
  //! return true if this corresponds to a graphic zone
  bool isGraphicZone();
  //! try to find the beginning of the next graphic zone
  bool findGraphicZone();

  //! check if a zone is or not a page frame zone
  bool isPageFrames();
  //! try to read a list of page frame ( picture, texture or basic )
  bool readPageFrames();
  //! try to read a basic frame header
  shared_ptr<GWGraphInternal::Frame> readFrameHeader();

  // interface with mainParser

  //
  // low level
  //
  //! reconstruct the order to used for reading the frame data
  static void buildFrameDataReadOrderFromTree
  (std::vector<std::vector<int> > const &tree, int id, std::vector<int> &order, std::set<int> &seen);

private:
  GWGraph(GWGraph const &orig);
  GWGraph &operator=(GWGraph const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<GWGraphInternal::State> m_state;

  //! the main parser;
  GWParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
