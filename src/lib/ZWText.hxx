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
 * Parser to ZWrite document
 *
 */
#ifndef ZW_TEXT
#  define ZW_TEXT

#include "libmwaw_internal.hxx"
#include "MWAWDebug.hxx"

class MWAWInputStream;
typedef shared_ptr<MWAWInputStream> MWAWInputStreamPtr;

class MWAWContentListener;
typedef class MWAWContentListener ZWContentListener;
typedef shared_ptr<ZWContentListener> ZWContentListenerPtr;
class MWAWEntry;
class MWAWFont;
class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;
class MWAWParagraph;

namespace ZWTextInternal
{
struct Section;
struct State;
class SubDocument;
}

class ZWParser;

/** \brief the main class to read the text part of LightWay Text file
 *
 *
 *
 */
class ZWText
{
  friend class ZWParser;
  friend class ZWTextInternal::SubDocument;
public:
  //! constructor
  ZWText(MWAWInputStreamPtr ip, ZWParser &parser, MWAWFontConverterPtr &convertissor);
  //! destructor
  virtual ~ZWText();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:
  //! the list of code in the text
  enum TextCode { None, Center, BookMark, NewPage, Tag, Link };

  //! sets the listener in this class and in the helper classes
  void setListener(ZWContentListenerPtr listen) {
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

  //! try to send a section
  bool sendText(ZWTextInternal::Section const &zone, MWAWEntry const &entry);
  //! try to send a section using an id
  bool sendText(int sectionId, MWAWEntry const &entry);
  //! check if a character after '<' corresponds to a text code
  TextCode isTextCode(MWAWInputStreamPtr &input, long endPos, MWAWEntry &dPos);

  //! read the header/footer zone
  bool readHFZone(MWAWEntry const &entry);
  //! returns true if there is a header/footer
  bool hasHeaderFooter(bool header) const;
  //! try to send the header/footer
  bool sendHeaderFooter(bool header);

  //! read the styles
  bool readStyles(MWAWEntry const &entry);

  /** send the character properties */
  void setProperty(MWAWFont const &font);
  //! read a section fonts
  bool readSectionFonts(MWAWEntry const &entry);

  //
  // low level
  //

  //! returns the debug file
  libmwaw::DebugFile &ascii() {
    return m_asciiFile;
  }

private:
  ZWText(ZWText const &orig);
  ZWText &operator=(ZWText const &orig);

protected:
  //
  // data
  //
  //! the input
  MWAWInputStreamPtr m_input;

  //! the listener
  ZWContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<ZWTextInternal::State> m_state;

  //! the main parser;
  ZWParser *m_mainParser;

  //! the debug file
  libmwaw::DebugFile &m_asciiFile;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
