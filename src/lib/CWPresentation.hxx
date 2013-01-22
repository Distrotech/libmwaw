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
 * Parser to Claris Works text document ( presentation part )
 *
 */
#ifndef CW_MWAW_PRESENTATION
#  define CW_MWAW_PRESENTATION

#include <vector>

#include "MWAWEntry.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

#include "CWStruct.hxx"

class MWAWContentListener;
typedef shared_ptr<MWAWContentListener> MWAWContentListenerPtr;

class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

namespace CWPresentationInternal
{
struct Presentation;
struct State;
}

class CWParser;
class CWStyleManager;

/** \brief the main class to read the text part of Claris Works file
 *
 *
 *
 */
class CWPresentation
{
  friend class CWParser;

public:
  //! constructor
  CWPresentation(MWAWInputStreamPtr ip, CWParser &parser, MWAWFontConverterPtr &convertissor);
  //! destructor
  virtual ~CWPresentation();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

  //! reads the zone presentation DSET
  shared_ptr<CWStruct::DSET> readPresentationZone
  (CWStruct::DSET const &zone, MWAWEntry const &entry, bool &complete);

  //! returns the list of slide id
  std::vector<int> getSlidesList() const;

protected:

  //! sets the listener in this class and in the helper classes
  void setListener(MWAWContentListenerPtr listen) {
    m_listener = listen;
  }

  //! sends the zone data to the listener (if it exists )
  bool sendZone(int number);

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //
  // Intermediate level
  //

  /** try to read the first presentation zone ( the slide name ? ) */
  bool readZone1(CWPresentationInternal::Presentation &pres);

  /** try to read the second presentation zone ( title ) */
  bool readZone2(CWPresentationInternal::Presentation &pres);


  //
  // low level
  //

  //! returns the debug file
  libmwaw::DebugFile &ascii() {
    return m_asciiFile;
  }

private:
  CWPresentation(CWPresentation const &orig);
  CWPresentation &operator=(CWPresentation const &orig);

protected:
  //
  // data
  //
  //! the input
  MWAWInputStreamPtr m_input;

  //! the listener
  MWAWContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<CWPresentationInternal::State> m_state;

  //! the main parser;
  CWParser *m_mainParser;

  //! the style manager
  shared_ptr<CWStyleManager> m_styleManager;

  //! the debug file
  libmwaw::DebugFile &m_asciiFile;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
