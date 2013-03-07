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

namespace HMWKGraphInternal
{
struct Frame;
struct BasicGraph;
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
  bool sendPageGraphics();
  //! sends the data which have not yet been sent to the listener
  void flushExtra();


  //
  // Intermediate level
  //

  /** try to read the frame definition (type 2)*/
  bool readFrames(shared_ptr<HMWKZone> zone);
  /** try to read a picture zone (type d)*/
  bool readPicture(shared_ptr<HMWKZone> zone);


  // interface with mainParser

  /** try to send a frame to the listener */
  bool sendFrame(long frameId, MWAWPosition pos, WPXPropertyList extras=WPXPropertyList());
  /** try to send a picture to the listener */
  bool sendPicture(long pictId, MWAWPosition pos, WPXPropertyList extras=WPXPropertyList());

  //! ask main parser to send a text zone
  bool sendText(long textId, long id);

  //
  // low level
  //

  /** try to send a picture to the listener */
  bool sendPicture(HMWKGraphInternal::Picture const &picture, MWAWPosition pos, WPXPropertyList extras=WPXPropertyList());

  /** try to send a frame to the listener */
  bool sendFrame(HMWKGraphInternal::Frame const &frame, MWAWPosition pos, WPXPropertyList extras=WPXPropertyList());

  /** try to send a basic picture to the listener */
  bool sendBasicGraph(HMWKGraphInternal::BasicGraph const &pict, MWAWPosition pos, WPXPropertyList extras=WPXPropertyList());

  /** try to send a picture frame */
  bool sendPictureFrame(HMWKGraphInternal::PictureFrame const &pict, MWAWPosition pos, WPXPropertyList extras=WPXPropertyList());
  /** try to send an empty picture */
  bool sendEmptyPicture(MWAWPosition pos);

  /** try to send a textbox to the listener */
  bool sendTextBox(HMWKGraphInternal::TextBox const &textbox, MWAWPosition pos, WPXPropertyList extras=WPXPropertyList());

  /** try to send a table */
  bool sendTable(HMWKGraphInternal::Table const &table);
  /** try to send a table unformatted*/
  bool sendTableUnformatted(long fId);
  /** check if the table is correct and if it can be send to a listener */
  bool updateTable(HMWKGraphInternal::Table const &table);
  /** try to send a table */
  bool sendPreTableData(HMWKGraphInternal::Table const &table);
  /** try to send a cell in a table */
  bool sendTableCell(HMWKGraphInternal::TableCell const &cell);

  /** try to read the basic graph data */
  shared_ptr<HMWKGraphInternal::BasicGraph> readBasicGraph(shared_ptr<HMWKZone> zone, HMWKGraphInternal::Frame const &header);
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
