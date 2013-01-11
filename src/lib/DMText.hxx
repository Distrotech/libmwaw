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
 * Parser to Nisus text document
 *
 */
#ifndef DM_TEXT
#  define DM_TEXT

#include "libmwaw_internal.hxx"
#include "MWAWDebug.hxx"

class MWAWInputStream;
typedef shared_ptr<MWAWInputStream> MWAWInputStreamPtr;

class MWAWContentListener;
typedef class MWAWContentListener DMContentListener;
typedef shared_ptr<DMContentListener> DMContentListenerPtr;

class MWAWEntry;

class MWAWFont;
class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

class MWAWSubDocument;

namespace DMTextInternal
{
class SubDocument;
struct State;
}

class DMParser;

/** \brief the main class to read the text part of DocMaker file
 *
 *
 *
 */
class DMText
{
  friend class DMTextInternal::SubDocument;
  friend class DMParser;
public:
  //! constructor
  DMText(MWAWInputStreamPtr ip, DMParser &parser, MWAWFontConverterPtr &convertissor);
  //! destructor
  virtual ~DMText();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:

  //! sets the listener in this class and in the helper classes
  void setListener(DMContentListenerPtr listen) {
    m_listener = listen;
  }

  //! finds the different text zones
  bool createZones();

  //! send a main zone
  bool sendMainText();

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //
  // intermediate level
  //

  /** compute the positions */
  void computePositions();

  //! try to read the font name ( resource rQDF )
  bool readFontNames(MWAWEntry const &entry);

  //! try to read the footer zone ( resource foot )
  bool readFooter(MWAWEntry const &entry);

  //
  // low level
  //

private:
  DMText(DMText const &orig);
  DMText &operator=(DMText const &orig);

protected:
  //
  // data
  //
  //! the input
  MWAWInputStreamPtr m_input;

  //! the listener
  DMContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<DMTextInternal::State> m_state;

  //! the main parser;
  DMParser *m_mainParser;

};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
