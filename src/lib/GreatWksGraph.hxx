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
#ifndef GREAT_WKS_GRAPH
#  define GREAT_WKS_GRAPH

#include <set>
#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "libmwaw_internal.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

namespace GreatWksGraphInternal
{
struct Frame;
struct FrameGroup;
struct FrameShape;
struct FrameText;
struct Zone;

struct State;
class SubDocument;
}

class GreatWksParser;
class GreatWksSSParser;

/** \brief the main class to read the graphic part of a HanMac Word-J file
 *
 *
 *
 */
class GreatWksGraph
{
  friend class GreatWksParser;
  friend class GreatWksSSParser;
  friend class GreatWksGraphInternal::SubDocument;

public:
  //! constructor
  GreatWksGraph(MWAWParser &parser);
  //! destructor
  virtual ~GreatWksGraph();

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
  bool sendPageFrames(GreatWksGraphInternal::Zone const &zone);
  //! try to send a frame
  bool sendFrame(shared_ptr<GreatWksGraphInternal::Frame> frame, GreatWksGraphInternal::Zone const &zone);

  //! try to send a group
  bool sendGroup(GreatWksGraphInternal::FrameGroup const &group, GreatWksGraphInternal::Zone const &zone, MWAWPosition const &pos);
  /** try to send a group elements by elemenys*/
  void sendGroupChild(GreatWksGraphInternal::FrameGroup const &group, GreatWksGraphInternal::Zone const &zone, MWAWPosition const &pos);
  //! check if we can send a group as graphic
  bool canCreateGraphic(GreatWksGraphInternal::FrameGroup const &group, GreatWksGraphInternal::Zone const &zone);
  /** send the group as a graphic zone */
  void sendGroup(GreatWksGraphInternal::FrameGroup const &group, GreatWksGraphInternal::Zone const &zone, MWAWGraphicListenerPtr &listener);

  //! try to send a textbox
  bool sendTextbox(GreatWksGraphInternal::FrameText const &text, GreatWksGraphInternal::Zone const &zone, MWAWPosition const &pos);
  //! try to send a textbox via a graphiclistener
  bool sendTextboxAsGraphic(Box2f const &box, GreatWksGraphInternal::FrameText const &text, MWAWGraphicStyle const &style, MWAWGraphicListenerPtr listener);
  //! try to send the textbox text (via the mainParser)
  bool sendTextbox(MWAWEntry const &entry, MWAWBasicListenerPtr listener);

  //! try to send a picture
  bool sendPicture(MWAWEntry const &entry, MWAWPosition pos);
  //! try to send a basic picture
  bool sendShape(GreatWksGraphInternal::FrameShape const &graph, GreatWksGraphInternal::Zone const &zone, MWAWPosition const &pos);

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
  shared_ptr<GreatWksGraphInternal::Frame> readFrameHeader();
  //! try to read a frame extra data zone
  bool readFrameExtraData(GreatWksGraphInternal::Frame &frame, int id, long endPos=-1);

  //! try to read a zone style
  bool readStyle(MWAWGraphicStyle &style);
  //! try to read a line format style? in v1
  bool readLineFormat(std::string &extra);

  // interface with mainParser

  //
  // low level
  //
  //! try to read a frame extra data zone recursively ( draw method)
  bool readFrameExtraDataRec(GreatWksGraphInternal::Zone &zone, int id, std::set<int> &seens, long endPos=-1);
  //! check if the graph of zones is ok (ie. does not form loop)
  bool checkGraph(GreatWksGraphInternal::Zone &zone, int id, std::set<int> &seens);

  /** a struct used to defined the different callback */
  struct Callback {
    //! callback used check if a textbox can be send in a graphic zone, ie. does not contains any graphic
    typedef bool (MWAWParser:: *CanSendTextBoxAsGraphic)(MWAWEntry const &entry);
    //! callback used to send textbox
    typedef bool (MWAWParser:: *SendTextbox)(MWAWEntry const &entry, MWAWBasicListenerPtr listener);

    /** constructor */
    Callback() : m_canSendTextBoxAsGraphic(0), m_sendTextbox(0) { }
    /** copy constructor */
    Callback(Callback const &orig) : m_canSendTextBoxAsGraphic(0), m_sendTextbox(0)
    {
      *this=orig;
    }
    /** copy operator */
    Callback &operator=(Callback const &orig)
    {
      m_canSendTextBoxAsGraphic=orig.m_canSendTextBoxAsGraphic;
      m_sendTextbox=orig.m_sendTextbox;
      return *this;
    }
    /** the new page callback */
    CanSendTextBoxAsGraphic m_canSendTextBoxAsGraphic;
    /** the send textbox callback */
    SendTextbox m_sendTextbox;
  };
  //! set the callback
  void setCallback(Callback const &callback)
  {
    m_callback=callback;
  }
private:
  GreatWksGraph(GreatWksGraph const &orig);
  GreatWksGraph &operator=(GreatWksGraph const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;
  //! the state
  shared_ptr<GreatWksGraphInternal::State> m_state;
  //! the main parser;
  MWAWParser *m_mainParser;
  //! the different callback
  Callback m_callback;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
