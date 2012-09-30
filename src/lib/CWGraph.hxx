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

class MWAWContentListener;
typedef class MWAWContentListener CWContentListener;
typedef shared_ptr<CWContentListener> CWContentListenerPtr;

class MWAWEntry;

class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

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
  CWGraph(MWAWInputStreamPtr ip, CWParser &parser, MWAWFontConverterPtr &convertissor);
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
  bool readColorMap(MWAWEntry const &entry);

  //! return the color which corresponds to an id (if possible)
  bool getColor(int id, Vec3uc &col) const;
protected:

  //! sets the listener in this class and in the helper classes
  void setListener(CWContentListenerPtr listen) {
    m_listener = listen;
  }

  //! sends the zone data to the listener (if it exists )
  bool sendZone(int number, MWAWPosition::AnchorTo anchor);

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //
  // Intermediate level
  //

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
  bool readBitmapColorMap(std::vector<Vec3uc> &cMap);

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

  //! returns the debug file
  libmwaw::DebugFile &ascii() {
    return m_asciiFile;
  }

private:
  CWGraph(CWGraph const &orig);
  CWGraph &operator=(CWGraph const &orig);

protected:
  //
  // data
  //
  //! the input
  MWAWInputStreamPtr m_input;

  //! the listener
  CWContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<CWGraphInternal::State> m_state;

  //! the main parser;
  CWParser *m_mainParser;

  //! the debug file
  libmwaw::DebugFile &m_asciiFile;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
