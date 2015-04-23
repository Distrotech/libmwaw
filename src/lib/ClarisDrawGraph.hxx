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

namespace ClarisDrawGraphInternal
{
struct Group;
struct State;
struct Style;
struct Zone;
struct ZoneShape;
struct Bitmap;

class SubDocument;
}

class ClarisDrawParser;

/** \brief the main class to read the graphic part of Claris Works file
 *
 *
 *
 */
class ClarisDrawGraph
{
  friend class ClarisDrawGraphInternal::SubDocument;
  friend class ClarisDrawParser;
  friend class ClarisWksParser;

public:
  //! constructor
  ClarisDrawGraph(ClarisDrawParser &parser);
  //! destructor
  virtual ~ClarisDrawGraph();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

  //! reads the zone Group DSET
  shared_ptr<ClarisWksStruct::DSET> readGroupZone(ClarisWksStruct::DSET const &zone, MWAWEntry const &entry);

  //! reads the zone Bitmap DSET
  shared_ptr<ClarisWksStruct::DSET> readBitmapZone(ClarisWksStruct::DSET const &zone, MWAWEntry const &entry);

  //! return the surface color which corresponds to some ids (if possible)
  bool getSurfaceColor(ClarisDrawGraphInternal::Style const &style, MWAWColor &col) const;
protected:

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  // interface with main parser


  //
  // Intermediate level
  //

  /* read a simple group */
  shared_ptr<ClarisDrawGraphInternal::Zone> readGroupDef(MWAWEntry const &entry);
  /* read the group data.

     \note \a beginGroupPos is only used to help debugging */
  bool readGroupData(ClarisDrawGraphInternal::Group &group, long beginGroupPos);

  /* read a simple graphic zone */
  bool readShape(MWAWEntry const &entry,
                 ClarisDrawGraphInternal::ZoneShape &zone);

  /* try to read the polygon data */
  bool readPolygonData(shared_ptr<ClarisDrawGraphInternal::Zone> zone);

  /////////////
  /* try to read a bitmap zone */
  bool readBitmapColorMap(std::vector<MWAWColor> &cMap);

  /* try to read the bitmap  */
  bool readBitmapData(ClarisDrawGraphInternal::Bitmap &zone);
  //
  // low level
  //

  /* read the first zone of a group type */
  bool readGroupHeader(ClarisDrawGraphInternal::Group &group);

  /* read some unknown data in first zone */
  bool readGroupUnknown(ClarisDrawGraphInternal::Group &group, int zoneSz, int id);

private:
  ClarisDrawGraph(ClarisDrawGraph const &orig);
  ClarisDrawGraph &operator=(ClarisDrawGraph const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<ClarisDrawGraphInternal::State> m_state;

  //! the main parser;
  ClarisDrawParser *m_mainParser;
  //! the style manager
  shared_ptr<ClarisDrawStyleManager> m_styleManager;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
