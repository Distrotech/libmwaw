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
 * Parser to Mariner Write text document ( graphic part )
 *
 */
#ifndef MRW_GRAPH
#  define MRW_GRAPH

#include <string>
#include <vector>

#include <libwpd/libwpd.h>

#include "libmwaw_internal.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

typedef class MWAWContentListener MRWContentListener;
typedef shared_ptr<MRWContentListener> MRWContentListenerPtr;

class MWAWEntry;

class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

class MWAWPosition;

namespace MRWGraphInternal
{
struct State;
class SubDocument;
}

struct MRWEntry;
struct MRWStruct;
class MRWParser;

/** \brief the main class to read the graphic part of a Mariner Write file
 *
 *
 *
 */
class MRWGraph
{
  friend class MRWParser;
  friend class MRWGraphInternal::SubDocument;

public:
  //! constructor
  MRWGraph(MWAWInputStreamPtr ip, MRWParser &parser, MWAWFontConverterPtr &convertissor);
  //! destructor
  virtual ~MRWGraph();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:

  //! sets the listener in this class and in the helper classes
  void setListener(MRWContentListenerPtr listen) {
    m_listener = listen;
  }

  //! try to send the page graphic
  bool sendPageGraphics();
  //! sends the data which have not yet been sent to the listener
  void flushExtra();


  //
  // Intermediate level
  //

  /** try to read a postscript zone */
  bool readPostscript(MRWEntry const &entry, int zoneId);

  /** try to read a token zone (can be a picture or a field) */
  bool readToken(MRWEntry const &entry, int zoneId);
  /** try to read the first token zone ( which can contains some field text )*/
  bool readTokenBlock0(MRWStruct const &data, std::string &res);

  // interface with mainParser


  //
  // low level
  //

  //! returns the debug file
  libmwaw::DebugFile &ascii() {
    return m_asciiFile;
  }

private:
  MRWGraph(MRWGraph const &orig);
  MRWGraph &operator=(MRWGraph const &orig);

protected:
  //
  // data
  //
  //! the input
  MWAWInputStreamPtr m_input;

  //! the listener
  MRWContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<MRWGraphInternal::State> m_state;

  //! the main parser;
  MRWParser *m_mainParser;

  //! the debug file
  libmwaw::DebugFile &m_asciiFile;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
