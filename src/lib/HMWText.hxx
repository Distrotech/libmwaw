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
 * Parser to HanMac Word text document
 *
 */
#ifndef HMW_TEXT
#  define HMW_TEXT

#include "libmwaw_internal.hxx"
#include "MWAWDebug.hxx"

class MWAWInputStream;
typedef shared_ptr<MWAWInputStream> MWAWInputStreamPtr;

class MWAWContentListener;
typedef class MWAWContentListener HMWContentListener;
typedef shared_ptr<HMWContentListener> HMWContentListenerPtr;

class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

class MWAWSubDocument;

namespace HMWTextInternal
{
struct Font;
struct Paragraph;
class SubDocument;
struct State;
}

struct HMWZone;
class HMWParser;

/** \brief the main class to read the text part of HanMac Word file
 *
 *
 *
 */
class HMWText
{
  friend class HMWTextInternal::SubDocument;
  friend class HMWParser;
public:
  //! constructor
  HMWText(MWAWInputStreamPtr ip, HMWParser &parser, MWAWFontConverterPtr &convertissor);
  //! destructor
  virtual ~HMWText();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:

  //! sets the listener in this class and in the helper classes
  void setListener(HMWContentListenerPtr listen) {
    m_listener = listen;
  }

#if 0
  //! finds the different text zones
  bool createZones();
#endif

  //! send a main zone
  bool sendMainText();

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //
  // intermediate level
  //

  /** try to read a text zone (type 1)*/
  bool readTextZone(shared_ptr<HMWZone> zone);
  /** try to read the fonts name zone (type 5)*/
  bool readFontNames(shared_ptr<HMWZone> zone);
  /** try to read the style zone (type 3) */
  bool readStyles(shared_ptr<HMWZone> zone);

  //
  // low level
  //
  /** try to read a font in a text zone */
  bool readFont(shared_ptr<HMWZone> zone, HMWTextInternal::Font &font);
  /** try to read a paragraph in a text zone */
  bool readParagraph(shared_ptr<HMWZone> zone, HMWTextInternal::Paragraph &para);

private:
  HMWText(HMWText const &orig);
  HMWText &operator=(HMWText const &orig);

protected:
  //
  // data
  //
  //! the input
  MWAWInputStreamPtr m_input;

  //! the listener
  HMWContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<HMWTextInternal::State> m_state;

  //! the main parser;
  HMWParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
