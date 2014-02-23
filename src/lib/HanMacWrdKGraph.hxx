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
#ifndef HAN_MAC_WRD_K_GRAPH
#  define HAN_MAC_WRD_K_GRAPH

#include <map>
#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "libmwaw_internal.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

namespace HanMacWrdKGraphInternal
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

struct HanMacWrdKZone;
class HanMacWrdKParser;

/** \brief the main class to read the graphic part of a HanMac Word file
 *
 *
 *
 */
class HanMacWrdKGraph
{
  friend class HanMacWrdKParser;
  friend class HanMacWrdKGraphInternal::SubDocument;
  friend struct HanMacWrdKGraphInternal::Table;

public:
  //! constructor
  HanMacWrdKGraph(HanMacWrdKParser &parser);
  //! destructor
  virtual ~HanMacWrdKGraph();

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
  bool readFrames(shared_ptr<HanMacWrdKZone> zone);
  /** try to read a picture zone (type d)*/
  bool readPicture(shared_ptr<HanMacWrdKZone> zone);
  /** check the group structures, the linked textbox */
  void prepareStructures();

  // interface with mainParser

  /** try to send a frame to the listener */
  bool sendFrame(long frameId, MWAWPosition pos, librevenge::RVNGPropertyList extras=librevenge::RVNGPropertyList());
  /** try to send a picture to the listener */
  bool sendPicture(long pictId, MWAWPosition pos, librevenge::RVNGPropertyList extras=librevenge::RVNGPropertyList());

  //! ask main parser to send a text zone
  bool sendText(long textId, long id, MWAWBasicListenerPtr listener=MWAWBasicListenerPtr());
  /** return a list textZId -> type which
      3(footnote), 4(textbox), 9(table), 10(comment) */
  std::map<long,int> getTextFrameInformations() const;

  //
  // low level
  //

  /** check the graph structures: ie. the group children */
  bool checkGroupStructures(long fileId, long fileSubId, std::multimap<long, long> &seens, bool inGroup);

  /** try to send a picture to the listener */
  bool sendPicture(HanMacWrdKGraphInternal::Picture const &picture, MWAWPosition pos, librevenge::RVNGPropertyList extras=librevenge::RVNGPropertyList());
  /** try to send a frame to the listener */
  bool sendFrame(HanMacWrdKGraphInternal::Frame const &frame, MWAWPosition pos, librevenge::RVNGPropertyList extras=librevenge::RVNGPropertyList());
  /** try to send a basic picture to the listener */
  bool sendShapeGraph(HanMacWrdKGraphInternal::ShapeGraph const &shape, MWAWPosition pos);
  /** try to send a picture frame */
  bool sendPictureFrame(HanMacWrdKGraphInternal::PictureFrame const &pict, MWAWPosition pos, librevenge::RVNGPropertyList extras=librevenge::RVNGPropertyList());
  /** try to send an empty picture */
  bool sendEmptyPicture(MWAWPosition pos);
  /** try to send a textbox to the listener */
  bool sendTextBox(HanMacWrdKGraphInternal::TextBox const &textbox, MWAWPosition pos, librevenge::RVNGPropertyList extras=librevenge::RVNGPropertyList());
  /** try to send a table unformatted*/
  bool sendTableUnformatted(long fId);

  /** try to send a group to the listener */
  bool sendGroup(long fId, MWAWPosition pos);
  /** try to send a group to the listener */
  bool sendGroup(HanMacWrdKGraphInternal::Group const &group, MWAWPosition pos);
  //! check if we can send a group as graphic
  bool canCreateGraphic(HanMacWrdKGraphInternal::Group const &group);
  /** try to send a group elements by elements */
  void sendGroupChild(HanMacWrdKGraphInternal::Group const &group, MWAWPosition const &pos);
  /** send the group as a graphic zone */
  void sendGroup(HanMacWrdKGraphInternal::Group const &group, MWAWGraphicListenerPtr &listener);

  /** try to read the basic graph data */
  shared_ptr<HanMacWrdKGraphInternal::ShapeGraph> readShapeGraph(shared_ptr<HanMacWrdKZone> zone, HanMacWrdKGraphInternal::Frame const &header);
  /** try to read the footnote data */
  shared_ptr<HanMacWrdKGraphInternal::FootnoteFrame> readFootnoteFrame(shared_ptr<HanMacWrdKZone> zone, HanMacWrdKGraphInternal::Frame const &header);
  /** try to read the group data */
  shared_ptr<HanMacWrdKGraphInternal::Group> readGroup(shared_ptr<HanMacWrdKZone> zone, HanMacWrdKGraphInternal::Frame const &header);
  /** try to read the picture data */
  shared_ptr<HanMacWrdKGraphInternal::PictureFrame> readPictureFrame(shared_ptr<HanMacWrdKZone> zone, HanMacWrdKGraphInternal::Frame const &header);
  /** try to read the table data */
  shared_ptr<HanMacWrdKGraphInternal::Table> readTable(shared_ptr<HanMacWrdKZone> zone, HanMacWrdKGraphInternal::Frame const &header);
  /** try to read the textbox data */
  shared_ptr<HanMacWrdKGraphInternal::TextBox> readTextBox(shared_ptr<HanMacWrdKZone> zone, HanMacWrdKGraphInternal::Frame const &header, bool isMemo);

private:
  HanMacWrdKGraph(HanMacWrdKGraph const &orig);
  HanMacWrdKGraph &operator=(HanMacWrdKGraph const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<HanMacWrdKGraphInternal::State> m_state;

  //! the main parser;
  HanMacWrdKParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
