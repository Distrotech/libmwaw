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
 * Parser to Nisus text document ( graphic part )
 *
 */
#ifndef NS_GRAPH
#  define NS_GRAPH

#include <string>
#include <vector>

#include <libwpd/WPXPropertyList.h>

#include "libmwaw_internal.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "NSStruct.hxx"

typedef class MWAWContentListener NSContentListener;
typedef shared_ptr<NSContentListener> NSContentListenerPtr;

class MWAWEntry;

class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

class MWAWPosition;

namespace NSGraphInternal
{
struct RSSOEntry;
struct State;
class SubDocument;
}

class NSParser;

/** \brief the main class to read the graphic part of a Nisus file
 *
 *
 *
 */
class NSGraph
{
  friend class NSParser;
  friend class NSGraphInternal::SubDocument;

public:
  //! constructor
  NSGraph(MWAWInputStreamPtr ip, NSParser &parser, MWAWFontConverterPtr &convertissor);
  //! destructor
  virtual ~NSGraph();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:

  //! sets the listener in this class and in the helper classes
  void setListener(NSContentListenerPtr listen) {
    m_listener = listen;
  }

  //! finds the different graphic zones
  bool createZones();

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //! try to send a picture
  bool sendPicture(int pictId, bool inPictRsrc, MWAWPosition pictPos,
                   WPXPropertyList extras = WPXPropertyList());
  //! try to send the page graphic
  bool sendPageGraphics();

  //
  // Intermediate level
  //

  //! read the PLAC resource: a list of picture placements ?
  bool readPLAC(MWAWEntry const &entry);
  //! parse the PLDT resource: a unknown resource
  bool readPLDT(NSStruct::RecursifData const &data);
  //! read the PGRA resource: the number of page? graphics
  bool readPGRA(MWAWEntry const &entry);

  //
  // low level
  //

  //! try to find a RSSO entry in a picture file
  std::vector<NSGraphInternal::RSSOEntry> findRSSOEntry(MWAWInputStreamPtr inp) const;

  //! returns the debug file
  libmwaw::DebugFile &ascii() {
    return m_asciiFile;
  }

private:
  NSGraph(NSGraph const &orig);
  NSGraph &operator=(NSGraph const &orig);

protected:
  //
  // data
  //
  //! the input
  MWAWInputStreamPtr m_input;

  //! the listener
  NSContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<NSGraphInternal::State> m_state;

  //! the main parser;
  NSParser *m_mainParser;

  //! the debug file
  libmwaw::DebugFile &m_asciiFile;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
