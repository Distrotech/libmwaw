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
 * Parser to HanMac Word text document ( graphic part )
 *
 */
#ifndef HMWK_GRAPH
#  define HMWK_GRAPH

#include <map>
#include <string>
#include <vector>

#include <libwpd/libwpd.h>

#include "libmwaw_internal.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

namespace HMWKGraphInternal
{
struct Frame;
struct ShapeGraph;
struct FootnoteFrame;
struct Group;
struct PictureFrame;
struct Table;
struct TableCell;
struct TextBox;

struct Picture;

struct State;
class SubDocument;
}

struct HMWKZone;
class HMWKParser;

/** \brief the main class to read the graphic part of a HanMac Word file
 *
 *
 *
 */
class HMWKGraph
{
  friend class HMWKParser;
  friend class HMWKGraphInternal::SubDocument;
  friend struct HMWKGraphInternal::Table;

public:
  //! constructor
  HMWKGraph(HMWKParser &parser);
  //! destructor
  virtual ~HMWKGraph();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:

  //! returns the color associated with a pattern
  bool getColor(int colId, int patternId, MWAWColor &color) const;

  //! try to send the page graphic
  bool sendPageGraphics(std::vector<long> const &doNotSendIds);
  //! sends the data which have not yet been sent to the listener
  void flushExtra();


  //
  // Intermediate level
  //

  /** try to read the frame definition (type 2)*/
  bool readFrames(shared_ptr<HMWKZone> zone);
  /** try to read a picture zone (type d)*/
  bool readPicture(shared_ptr<HMWKZone> zone);
  /** check the group structures, the linked textbox */
  void prepareStructures();

  // interface with mainParser

  /** try to send a frame to the listener */
  bool sendFrame(long frameId, MWAWPosition pos, WPXPropertyList extras=WPXPropertyList());
  /** try to send a picture to the listener */
  bool sendPicture(long pictId, MWAWPosition pos, WPXPropertyList extras=WPXPropertyList());

  //! ask main parser to send a text zone
  bool sendText(long textId, long id, bool asGraphic=false);
  /** return a list textZId -> type which
      3(footnote), 4(textbox), 9(table), 10(comment) */
  std::map<long,int> getTextFrameInformations() const;

  //
  // low level
  //

  /** check the graph structures: ie. the group children */
  bool checkGroupStructures(long fileId, long fileSubId, std::multimap<long, long> &seens, bool inGroup);

  /** try to send a picture to the listener */
  bool sendPicture(HMWKGraphInternal::Picture const &picture, MWAWPosition pos, WPXPropertyList extras=WPXPropertyList());
  /** try to send a frame to the listener */
  bool sendFrame(HMWKGraphInternal::Frame const &frame, MWAWPosition pos, WPXPropertyList extras=WPXPropertyList());
  /** try to send a basic picture to the listener */
  bool sendShapeGraph(HMWKGraphInternal::ShapeGraph const &shape, MWAWPosition pos);
  /** try to send a picture frame */
  bool sendPictureFrame(HMWKGraphInternal::PictureFrame const &pict, MWAWPosition pos, WPXPropertyList extras=WPXPropertyList());
  /** try to send an empty picture */
  bool sendEmptyPicture(MWAWPosition pos);
  /** try to send a textbox to the listener */
  bool sendTextBox(HMWKGraphInternal::TextBox const &textbox, MWAWPosition pos, WPXPropertyList extras=WPXPropertyList());
  /** try to send a table unformatted*/
  bool sendTableUnformatted(long fId);

  /** try to send a group to the listener */
  bool sendGroup(long fId, MWAWPosition pos);
  /** try to send a group to the listener */
  bool sendGroup(HMWKGraphInternal::Group const &group, MWAWPosition pos);
  //! check if we can send a group as graphic
  bool canCreateGraphic(HMWKGraphInternal::Group const &group);
  /** try to send a group elements by elements */
  void sendGroupChild(HMWKGraphInternal::Group const &group, MWAWPosition const &pos);
  /** send the group as a graphic zone */
  void sendGroup(HMWKGraphInternal::Group const &group, MWAWGraphicListenerPtr &listener);

  /** try to read the basic graph data */
  shared_ptr<HMWKGraphInternal::ShapeGraph> readShapeGraph(shared_ptr<HMWKZone> zone, HMWKGraphInternal::Frame const &header);
  /** try to read the footnote data */
  shared_ptr<HMWKGraphInternal::FootnoteFrame> readFootnoteFrame(shared_ptr<HMWKZone> zone, HMWKGraphInternal::Frame const &header);
  /** try to read the group data */
  shared_ptr<HMWKGraphInternal::Group> readGroup(shared_ptr<HMWKZone> zone, HMWKGraphInternal::Frame const &header);
  /** try to read the picture data */
  shared_ptr<HMWKGraphInternal::PictureFrame> readPictureFrame(shared_ptr<HMWKZone> zone, HMWKGraphInternal::Frame const &header);
  /** try to read the table data */
  shared_ptr<HMWKGraphInternal::Table> readTable(shared_ptr<HMWKZone> zone, HMWKGraphInternal::Frame const &header);
  /** try to read the textbox data */
  shared_ptr<HMWKGraphInternal::TextBox> readTextBox(shared_ptr<HMWKZone> zone, HMWKGraphInternal::Frame const &header, bool isMemo);

private:
  HMWKGraph(HMWKGraph const &orig);
  HMWKGraph &operator=(HMWKGraph const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<HMWKGraphInternal::State> m_state;

  //! the main parser;
  HMWKParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
