/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libmwaw
 * Copyright (C) 2009, 2011 Alonso Laurent (alonso@loria.fr)
 * Copyright (C) 2006, 2007 Andrew Ziem
 * Copyright (C) 2004-2006 Fridrich Strba (fridrich.strba@bluewin.ch)
 * Copyright (C) 2004 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2003-2005 William Lachance (william.lachance@sympatico.ca)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 * For further information visit http://libwps.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

/*
 * Parser to Claris Works text document ( graphic part )
 *
 */
#ifndef CW_MWAW_GRAPH
#  define CW_MWAW_GRAPH

#include <list>
#include <string>
#include <vector>

#include "DMWAWPageSpan.hxx"

#include "MWAWPosition.hxx"

#include "MWAWEntry.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

#include "CWStruct.hxx"

typedef class MWAWContentListener CWContentListener;
typedef shared_ptr<CWContentListener> CWContentListenerPtr;

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

/** \brief the main class to read the text part of Claris Works file
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
  bool sendZone(int number);

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

  /* read the group data */
  bool readGroupData(shared_ptr<CWGraphInternal::Group> zone);

  /* try to read the polygon data */
  bool readPolygonData(shared_ptr<CWGraphInternal::Zone> zone);

  /////////////
  /* try to read a pict data zone */
  bool readPictData(shared_ptr<CWGraphInternal::Zone> zone);

  /* read a picture */
  bool readPICT(CWGraphInternal::ZonePict &zone);

  /* read a postcript zone */
  bool readPS(CWGraphInternal::ZonePict &zone);

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
  bool readGroupHeader();

  //! sends a picture zone
  bool sendPicture(CWGraphInternal::ZonePict &pict);

  //! sends a basic graphic zone
  bool sendBasicPicture(CWGraphInternal::ZoneBasic &pict);

  //! sends a bitmap graphic zone
  bool sendBitmap(CWGraphInternal::ZoneBitmap &pict);

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
