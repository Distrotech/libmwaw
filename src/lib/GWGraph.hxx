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
struct FrameShape;
struct Zone;

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

  //
  // Intermediate level
  //

  // RSRC
  //! read a pattern list block ( PAT# resource block )
  bool readPatterns(MWAWEntry const &entry);
  //! read a list of color and maybe patterns ( PlTT resource block: v2 )
  bool readPalettes(MWAWEntry const &entry);

  // DataFork: pict
  //! try to send all data corresponding to a zone
  bool sendPageFrames(GWGraphInternal::Zone const &zone);
  //! try to send a frame
  bool sendFrame(shared_ptr<GWGraphInternal::Frame> frame, GWGraphInternal::Zone const &zone, int order);
  //! try to send the textbox text
  bool sendTextbox(MWAWEntry const &entry);
  //! try to send a picture
  bool sendPicture(MWAWEntry const &entry, MWAWPosition pos);
  //! try to send a basic picture
  bool sendShape(GWGraphInternal::FrameShape const &graph, GWGraphInternal::Zone const &zone, MWAWPosition pos);

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
  //! try to read a frame extra data zone
  bool readFrameExtraData(GWGraphInternal::Frame &frame, int id, long endPos=-1);

  //! try to read a zone style
  bool readStyle(MWAWGraphicStyle &style);
  //! try to read a line format style? in v1
  bool readLineFormat(std::string &extra);

  // interface with mainParser

  //
  // low level
  //
  //! try to read a frame extra data zone recursively ( draw method)
  bool readFrameExtraDataRec(GWGraphInternal::Zone &zone, int id, std::set<int> &seens, long endPos=-1);

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
