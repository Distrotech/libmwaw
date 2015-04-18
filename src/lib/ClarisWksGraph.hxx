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
 * Parser to Claris Works text document ( graphic part )
 *
 */
#ifndef CLARIS_WKS_GRAPH
#  define CLARIS_WKS_GRAPH

#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "libmwaw_internal.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"
#include "MWAWPosition.hxx"

#include "ClarisWksStruct.hxx"

namespace ClarisWksGraphInternal
{
struct Group;
struct State;
struct Style;
struct Zone;
struct Chart;
struct ZoneShape;
struct Bitmap;
struct ZonePict;

class SubDocument;
}

class ClarisWksDocument;
class ClarisWksParser;
class MWAWParser;

/** \brief the main class to read the graphic part of Claris Works file
 *
 *
 *
 */
class ClarisWksGraph
{
  friend class ClarisWksGraphInternal::SubDocument;
  friend class ClarisWksDocument;
  friend class ClarisWksParser;

public:
  //! constructor
  ClarisWksGraph(ClarisWksDocument &document);
  //! destructor
  virtual ~ClarisWksGraph();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

  //! returns the page dimension if known (in point)
  bool getPageDimension(MWAWVec2f &dim) const;

  //! compute the pages position
  void computePositions() const;

  //! find the master zone to the content zones in a graphic document
  void findMasterPage() const;

  //! reads the zone Group DSET
  shared_ptr<ClarisWksStruct::DSET> readGroupZone
  (ClarisWksStruct::DSET const &zone, MWAWEntry const &entry, bool &complete);

  //! reads the zone Bitmap DSET
  shared_ptr<ClarisWksStruct::DSET> readBitmapZone
  (ClarisWksStruct::DSET const &zone, MWAWEntry const &entry, bool &complete);

  //! return the surface color which corresponds to some ids (if possible)
  bool getSurfaceColor(ClarisWksGraphInternal::Style const &style, MWAWColor &col) const;
protected:
  //! check if we can send a group as graphic
  bool canSendGroupAsGraphic(int number) const;
  //! sends the page element
  bool sendPageGraphics(int groupId);
  //! sends the master zone (ie. the background zone in a graphic document)
  bool sendMaster(int pg);
  //! sends the zone data to the listener (if it exists )
  bool sendGroup(int number, MWAWListenerPtr listener, MWAWPosition const &pos=MWAWPosition());
  //! check if we can send a group as graphic
  bool canSendBitmapAsGraphic(int number) const;
  //! sends the bitmap data to the listener (if it exists )
  bool sendBitmap(int number, MWAWListenerPtr listener, MWAWPosition const &pos=MWAWPosition());

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  // interface with main parser

  //! ask the main parser to send a zone
  void askToSend(int number, MWAWListenerPtr listener, MWAWPosition const &pos=MWAWPosition());

  //
  // Intermediate level
  //

  //! update the group information to choose how to send the group data
  void updateGroup(ClarisWksGraphInternal::Group &group) const;
  //! check if we can send a group as graphic
  bool canSendAsGraphic(ClarisWksGraphInternal::Group &group) const;
  //! send a group
  bool sendGroup(ClarisWksGraphInternal::Group &group, MWAWPosition const &position);
  //! send a child group as graphic or as presentation
  bool sendGroupChild(std::vector<shared_ptr<ClarisWksGraphInternal::Zone> > const &lChild, MWAWListenerPtr listener, MWAWVec2f const &leftTop);
  //! send a group child
  bool sendGroupChild(shared_ptr<ClarisWksGraphInternal::Zone> zone, MWAWPosition position);
  //! send the child element corresponding to some page
  bool sendPageChild(ClarisWksGraphInternal::Group &group);
  /* read a simple group */
  shared_ptr<ClarisWksGraphInternal::Zone> readGroupDef(MWAWEntry const &entry);
  /* read the group data.

     \note \a beginGroupPos is only used to help debugging */
  bool readGroupData(ClarisWksGraphInternal::Group &group, long beginGroupPos);

  /* read a simple graphic zone */
  bool readShape(MWAWEntry const &entry,
                 ClarisWksGraphInternal::ZoneShape &zone);

  /* try to read the chart data */
  bool readChartData(shared_ptr<ClarisWksGraphInternal::Zone> zone);

  /* try to read a pict data zone */
  bool readPictData(shared_ptr<ClarisWksGraphInternal::Zone> zone);

  /* try to read the polygon data */
  bool readPolygonData(shared_ptr<ClarisWksGraphInternal::Zone> zone);

  /* read a picture */
  bool readPICT(ClarisWksGraphInternal::ZonePict &zone);

  /* read a postcript zone */
  bool readPS(ClarisWksGraphInternal::ZonePict &zone);

  /* read a ole document zone */
  bool readOLE(ClarisWksGraphInternal::ZonePict &zone);

  /////////////
  /* try to read the qtime data zone */
  bool readQTimeData(shared_ptr<ClarisWksGraphInternal::Zone> zone);

  /* read a named picture */
  bool readNamedPict(ClarisWksGraphInternal::ZonePict &zone);

  /////////////
  /* try to read a bitmap zone */
  bool readBitmapColorMap(std::vector<MWAWColor> &cMap);

  /* try to read the bitmap  */
  bool readBitmapData(ClarisWksGraphInternal::Bitmap &zone);
  //
  // low level
  //

  /* read the first zone of a group type */
  bool readGroupHeader(ClarisWksGraphInternal::Group &group);

  /* read some unknown data in first zone */
  bool readGroupUnknown(ClarisWksGraphInternal::Group &group, int zoneSz, int id);

  //! sends a picture zone
  bool sendPicture(ClarisWksGraphInternal::ZonePict &pict, MWAWPosition pos);

  //! sends a basic graphic zone
  bool sendShape(ClarisWksGraphInternal::ZoneShape &pict, MWAWPosition pos);

  //! sends a bitmap graphic zone
  bool sendBitmap(ClarisWksGraphInternal::Bitmap &pict, MWAWListener &listener, MWAWPosition pos);

private:
  ClarisWksGraph(ClarisWksGraph const &orig);
  ClarisWksGraph &operator=(ClarisWksGraph const &orig);

protected:
  //
  // data
  //
  //! the document
  ClarisWksDocument &m_document;

  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<ClarisWksGraphInternal::State> m_state;

  //! the main parser;
  MWAWParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
