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
#ifndef HMW_GRAPH
#  define HMW_GRAPH

#include <string>
#include <vector>

#include <libwpd/libwpd.h>

#include "libmwaw_internal.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

typedef class MWAWContentListener HMWContentListener;
typedef shared_ptr<HMWContentListener> HMWContentListenerPtr;

class MWAWEntry;

class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

class MWAWPosition;

namespace HMWGraphInternal
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

struct HMWZone;
class HMWParser;

/** \brief the main class to read the graphic part of a Nisus file
 *
 *
 *
 */
class HMWGraph
{
  friend class HMWParser;
  friend class HMWGraphInternal::SubDocument;

public:
  //! constructor
  HMWGraph(MWAWInputStreamPtr ip, HMWParser &parser, MWAWFontConverterPtr &convertissor);
  //! destructor
  virtual ~HMWGraph();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:

  //! sets the listener in this class and in the helper classes
  void setListener(HMWContentListenerPtr listen) {
    m_listener = listen;
  }

  //! returns the color associated with a pattern
  bool getColor(int colId, int patternId, uint32_t &color) const;

  //! try to send the page graphic
  bool sendPageGraphics();
  //! sends the data which have not yet been sent to the listener
  void flushExtra();


  //
  // Intermediate level
  //

  /** try to read the frame definition (type 2)*/
  bool readFrames(shared_ptr<HMWZone> zone);
  /** try to read a picture zone (type d)*/
  bool readPicture(shared_ptr<HMWZone> zone);


  // interface with mainParser

  /** try to send a frame to the listener */
  bool sendFrame(long frameId, MWAWPosition pos, WPXPropertyList extras=WPXPropertyList());
  /** try to send a picture to the listener */
  bool sendPicture(long pictId, MWAWPosition pos, WPXPropertyList extras=WPXPropertyList());

  //! ask main parser to send a text zone
  bool sendText(long textId, int id);

  //
  // low level
  //

  /** try to send a picture to the listener */
  bool sendPicture(HMWGraphInternal::Picture const &picture, MWAWPosition pos, WPXPropertyList extras=WPXPropertyList());

  /** try to send a frame to the listener */
  bool sendFrame(HMWGraphInternal::Frame const &frame, MWAWPosition pos, WPXPropertyList extras=WPXPropertyList());

  /** try to send a basic picture to the listener */
  bool sendBasicGraph(HMWGraphInternal::BasicGraph const &pict, MWAWPosition pos, WPXPropertyList extras=WPXPropertyList());

  /** try to send a picture frame */
  bool sendPictureFrame(HMWGraphInternal::PictureFrame const &pict, MWAWPosition pos, WPXPropertyList extras=WPXPropertyList());
  /** try to send an empty picture */
  bool sendEmptyPicture(MWAWPosition pos);

  /** try to send a textbox to the listener */
  bool sendTextBox(HMWGraphInternal::TextBox const &textbox, MWAWPosition pos, WPXPropertyList extras=WPXPropertyList());

  /** try to send a table */
  bool sendTable(HMWGraphInternal::Table const &table);
  /** try to send a table unformatted*/
  bool sendTableUnformatted(long fId);
  /** check if the table is correct and if it can be send to a listener */
  bool updateTable(HMWGraphInternal::Table const &table);
  /** try to send a table */
  bool sendPreTableData(HMWGraphInternal::Table const &table);
  /** try to send a cell in a table */
  bool sendTableCell(HMWGraphInternal::TableCell const &cell);

  /** try to read the basic graph data */
  shared_ptr<HMWGraphInternal::BasicGraph> readBasicGraph(shared_ptr<HMWZone> zone, HMWGraphInternal::Frame const &header);
  /** try to read the group data */
  shared_ptr<HMWGraphInternal::Group> readGroup(shared_ptr<HMWZone> zone, HMWGraphInternal::Frame const &header);
  /** try to read the picture data */
  shared_ptr<HMWGraphInternal::PictureFrame> readPictureFrame(shared_ptr<HMWZone> zone, HMWGraphInternal::Frame const &header);
  /** try to read the table data */
  shared_ptr<HMWGraphInternal::Table> readTable(shared_ptr<HMWZone> zone, HMWGraphInternal::Frame const &header);
  /** try to read the textbox data */
  shared_ptr<HMWGraphInternal::TextBox> readTextBox(shared_ptr<HMWZone> zone, HMWGraphInternal::Frame const &header, bool isMemo);

  //! returns the debug file
  libmwaw::DebugFile &ascii() {
    return m_asciiFile;
  }

private:
  HMWGraph(HMWGraph const &orig);
  HMWGraph &operator=(HMWGraph const &orig);

protected:
  //
  // data
  //
  //! the input
  MWAWInputStreamPtr m_input;

  //! the listener
  HMWContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<HMWGraphInternal::State> m_state;

  //! the main parser;
  HMWParser *m_mainParser;

  //! the debug file
  libmwaw::DebugFile &m_asciiFile;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
