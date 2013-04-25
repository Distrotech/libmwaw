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
#ifndef CW_GRAPH
#  define CW_GRAPH

#include <string>
#include <vector>

#include <libwpd/libwpd.h>

#include "libmwaw_internal.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "CWStruct.hxx"

class MWAWEntry;

class MWAWParserState;
typedef shared_ptr<MWAWParserState> MWAWParserStatePtr;

namespace CWGraphInternal
{
struct Group;
struct State;
struct Style;
struct Zone;
struct ZoneBasic;
struct ZoneBitmap;
struct ZonePict;
}

class CWParser;
class CWStyleManager;

/** \brief the main class to read the graphic part of Claris Works file
 *
 *
 *
 */
class CWGraph
{
  friend class CWParser;

public:
  //! constructor
  CWGraph(CWParser &parser);
  //! destructor
  virtual ~CWGraph();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

  //! reads the zone Group DSET
  shared_ptr<CWStruct::DSET> readGroupZone
  (CWStruct::DSET const &zone, MWAWEntry const &entry, bool &complete);

  //! reads the zone Bitmap DSET
  shared_ptr<CWStruct::DSET> readBitmapZone
  (CWStruct::DSET const &zone, MWAWEntry const &entry, bool &complete);

  //! reads a color map zone ( v4-v6)
  bool readColorList(MWAWEntry const &entry);

  //! return the color which corresponds to an id (if possible)
  bool getColor(int id, MWAWColor &col) const;

  //! return the pattern percent which corresponds to an id (or -1)
  float getPatternPercent(int id) const;

  //! return the wall paper color which corresponds to an id (if possible)
  bool getWallPaperColor(int id, MWAWColor &col) const;

  //! return the line color which corresponds to some ids (if possible)
  bool getLineColor(CWGraphInternal::Style const style, MWAWColor &col) const;
  //! return the surface color which corresponds to some ids (if possible)
  bool getSurfaceColor(CWGraphInternal::Style const style, MWAWColor &col) const;
protected:
  //! sends the zone data to the listener (if it exists )
  bool sendZone(int number, MWAWPosition pos=MWAWPosition());

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //
  // Intermediate level
  //

  //! update the group information to choose how to send the group data
  void updateInformation(CWGraphInternal::Group &group) const;
  /* read a simple group */
  shared_ptr<CWGraphInternal::Zone> readGroupDef(MWAWEntry const &entry);

  /* read a simple graphic zone */
  bool readBasicGraphic(MWAWEntry const &entry,
                        CWGraphInternal::ZoneBasic &zone);

  /* read the group data.

     \note \a beginGroupPos is only used to help debugging */
  bool readGroupData(CWGraphInternal::Group &group, long beginGroupPos);

  /* try to read the polygon data */
  bool readPolygonData(shared_ptr<CWGraphInternal::Zone> zone);

  /////////////
  /* try to read a pict data zone */
  bool readPictData(shared_ptr<CWGraphInternal::Zone> zone);

  /* read a picture */
  bool readPICT(CWGraphInternal::ZonePict &zone);

  /* read a postcript zone */
  bool readPS(CWGraphInternal::ZonePict &zone);

  /* read a ole document zone */
  bool readOLE(CWGraphInternal::ZonePict &zone);

/////////////
  /* try to read the qtime data zone */
  bool readQTimeData(shared_ptr<CWGraphInternal::Zone> zone);

  /* read a named picture */
  bool readNamedPict(CWGraphInternal::ZonePict &zone);

  /////////////
  /* try to read a bitmap zone */
  bool readBitmapColorMap(std::vector<MWAWColor> &cMap);

  /* try to read the bitmap  */
  bool readBitmapData(CWGraphInternal::ZoneBitmap &zone);
  //
  // low level
  //

  /* read the first zone of a group type */
  bool readGroupHeader(CWGraphInternal::Group &group);

  /* read some unknown data in first zone */
  bool readGroupUnknown(CWGraphInternal::Group &group, int zoneSz, int id);

  //! sends a picture zone
  bool sendPicture(CWGraphInternal::ZonePict &pict, MWAWPosition pos,
                   WPXPropertyList extras = WPXPropertyList());

  //! sends a basic graphic zone
  bool sendBasicPicture(CWGraphInternal::ZoneBasic &pict, MWAWPosition pos,
                        WPXPropertyList extras = WPXPropertyList());

  //! sends a bitmap graphic zone
  bool sendBitmap(CWGraphInternal::ZoneBitmap &pict, MWAWPosition pos,
                  WPXPropertyList extras = WPXPropertyList());

private:
  CWGraph(CWGraph const &orig);
  CWGraph &operator=(CWGraph const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<CWGraphInternal::State> m_state;

  //! the main parser;
  CWParser *m_mainParser;

  //! the style manager
  shared_ptr<CWStyleManager> m_styleManager;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
