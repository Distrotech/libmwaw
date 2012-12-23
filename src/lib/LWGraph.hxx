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
 * Parser to LightWay Text document ( graphic part )
 *
 */
#ifndef LW_GRAPH
#  define LW_GRAPH

#include <string>
#include <vector>

#include <libwpd/libwpd.h>

#include "libmwaw_internal.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

typedef class MWAWContentListener LWContentListener;
typedef shared_ptr<LWContentListener> LWContentListenerPtr;

class MWAWEntry;

class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

class MWAWPosition;

namespace LWGraphInternal
{
struct State;
class SubDocument;
}

class LWParser;

/** \brief the main class to read the graphic part of a LightWay Text file
 *
 *
 *
 */
class LWGraph
{
  friend class LWParser;
  friend class LWGraphInternal::SubDocument;

public:
  //! constructor
  LWGraph(MWAWInputStreamPtr ip, LWParser &parser, MWAWFontConverterPtr &convertissor);
  //! destructor
  virtual ~LWGraph();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:

  //! sets the listener in this class and in the helper classes
  void setListener(LWContentListenerPtr listen) {
    m_listener = listen;
  }

  //! finds the different graphic zones
  bool createZones();

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //! try to send the page graphic
  bool sendPageGraphics();

  //
  // Intermediate level
  //

  //! read the JPEG resource
  bool readJPEG(MWAWEntry const &entry);

  //
  // low level
  //

  //! returns the debug file
  libmwaw::DebugFile &ascii() {
    return m_asciiFile;
  }

private:
  LWGraph(LWGraph const &orig);
  LWGraph &operator=(LWGraph const &orig);

protected:
  //
  // data
  //
  //! the input
  MWAWInputStreamPtr m_input;

  //! the listener
  LWContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<LWGraphInternal::State> m_state;

  //! the main parser;
  LWParser *m_mainParser;

  //! the debug file
  libmwaw::DebugFile &m_asciiFile;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
