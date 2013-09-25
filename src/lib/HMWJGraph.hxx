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

#include <map>
#include <set>
#include <string>
#include <vector>

#include <libwpd/libwpd.h>

#include "libmwaw_internal.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

namespace HMWJGraphInternal
{
struct CellFormat;
struct Frame;
struct CommentFrame;
struct Group;
struct PictureFrame;
struct ShapeGraph;
struct TableFrame;
struct TextboxFrame;
struct TextFrame;
struct Table;
struct TableCell;

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
  friend struct HMWJGraphInternal::Table;
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

  /** check the group structures, the linked textbox */
  void prepareStructures();
  //! try to send the page graphic
  bool sendPageGraphics(std::vector<long> const &doNotSendIds);
  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //
  // Intermediate level
  //
  /** try to read the frames definition (type 3)*/
  bool readFrames(MWAWEntry const &entry);
  /** try to read a frame*/
  shared_ptr<HMWJGraphInternal::Frame> readFrame(int id);
  /** try to read the basic graph data */
  shared_ptr<HMWJGraphInternal::ShapeGraph> readShapeGraph(HMWJGraphInternal::Frame const &header, long endPos);
  /** try to read the comment data  */
  shared_ptr<HMWJGraphInternal::CommentFrame> readCommentData(HMWJGraphInternal::Frame const &header, long endPos);
  /** try to read the picture data  */
  shared_ptr<HMWJGraphInternal::PictureFrame> readPictureData(HMWJGraphInternal::Frame const &header, long endPos);
  /** try to read the table data  */
  shared_ptr<HMWJGraphInternal::TableFrame> readTableData(HMWJGraphInternal::Frame const &header, long endPos);
  /** try to read a textbox data */
  shared_ptr<HMWJGraphInternal::TextboxFrame> readTextboxData(HMWJGraphInternal::Frame const &header, long endPos);
  /** try to read a text data (text, header/footer, footnote)  */
  shared_ptr<HMWJGraphInternal::TextFrame> readTextData(HMWJGraphInternal::Frame const &header, long endPos);
  /** try to read the groupd data ( type 9 )*/
  bool readGroupData(MWAWEntry const &entry, int actZone);
  /** try to read the graph data (zone 8)*/
  bool readGraphData(MWAWEntry const &entry, int actZone);
  /** try to read the pictures definition (type 6)*/
  bool readPicture(MWAWEntry const &entry, int actZone);
  /** try to read a table (zone 7)*/
  bool readTable(MWAWEntry const &entry, int actZone);
  /** try to read a list of format */
  bool readTableFormatsList(HMWJGraphInternal::Table &table, long endPos);


  /** try to send a frame to the listener */
  bool sendFrame(HMWJGraphInternal::Frame const &frame, MWAWPosition pos, WPXPropertyList extras=WPXPropertyList());
  /** try to send a basic picture to the listener */
  bool sendShapeGraph(HMWJGraphInternal::ShapeGraph const &pict, MWAWPosition pos);
  /** try to send a comment box to the listener */
  bool sendComment(HMWJGraphInternal::CommentFrame const &textbox, MWAWPosition pos, WPXPropertyList extras=WPXPropertyList());
  /** try to send a picture frame */
  bool sendPictureFrame(HMWJGraphInternal::PictureFrame const &pict, MWAWPosition pos, WPXPropertyList extras=WPXPropertyList());
  /** try to send an empty picture */
  bool sendEmptyPicture(MWAWPosition pos);
  /** try to send a textbox to the listener */
  bool sendTextbox(HMWJGraphInternal::TextboxFrame const &textbox, MWAWPosition pos, WPXPropertyList extras=WPXPropertyList());
  /** try to send a table unformatted*/
  bool sendTableUnformatted(long zId);

  /** try to send a group to the listener */
  bool sendGroup(long zId, MWAWPosition pos);
  /** try to send a group to the listener */
  bool sendGroup(HMWJGraphInternal::Group const &group, MWAWPosition pos);
  //! check if we can send a group as graphic
  bool canCreateGraphic(HMWJGraphInternal::Group const &group);
  /** try to send a group elements by elements */
  void sendGroupChild(HMWJGraphInternal::Group const &group, MWAWPosition const &pos);
  /** send the group as a graphic zone */
  void sendGroup(HMWJGraphInternal::Group const &group, MWAWGraphicListenerPtr &listener);

  // interface with mainParser

  /** return a list textZId -> type which type=0(main), 1(header),
      2(footer), 3(footnote), 4(textbox), 9(table), 10(comment) */
  std::map<long,int> getTextFrameInformations() const;
  /** return the footnote text zone id and the list of first char position */
  bool getFootnoteInformations(long &textZId, std::vector<long> &fPosList) const;
  /** try to send a frame to the listener */
  bool sendFrame(long frameId, MWAWPosition pos, WPXPropertyList extras=WPXPropertyList());
  //! ask main parser to send a text zone
  bool sendText(long textId, long fPos, bool asGraphic=false);

  //
  // low level
  //
  /** check the graph structures: ie. the group children */
  bool checkGroupStructures(long zId, std::set<long> &seens, bool inGroup);


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
