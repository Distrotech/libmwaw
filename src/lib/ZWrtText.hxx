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
#ifndef Z_WRT_TEXT
#  define Z_WRT_TEXT

#include "libmwaw_internal.hxx"
#include "MWAWDebug.hxx"

namespace ZWrtTextInternal
{
struct Section;
struct State;
class SubDocument;
}

class ZWrtParser;

/** \brief the main class to read the text part of ZWrite Text file
 *
 *
 *
 */
class ZWrtText
{
  friend class ZWrtParser;
  friend class ZWrtTextInternal::SubDocument;
public:
  //! constructor
  ZWrtText(ZWrtParser &parser);
  //! destructor
  virtual ~ZWrtText();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:
  //! the list of code in the text
  enum TextCode { None, Center, BookMark, NewPage, Tag, Link };

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
  bool sendText(ZWrtTextInternal::Section const &zone, MWAWEntry const &entry);
  //! try to send a section using an id
  bool sendText(int sectionId, MWAWEntry const &entry);
  //! check if a character after '<' corresponds to a text code
  TextCode isTextCode(MWAWInputStreamPtr &input, long endPos, MWAWEntry &dPos) const;

  //! read the header/footer zone
  bool readHFZone(MWAWEntry const &entry);
  //! returns true if there is a header/footer
  bool hasHeaderFooter(bool header) const;
  //! try to send the header/footer
  bool sendHeaderFooter(bool header);

  //! read the styles
  bool readStyles(MWAWEntry const &entry);

  //! read a section fonts
  bool readSectionFonts(MWAWEntry const &entry);

private:
  ZWrtText(ZWrtText const &orig);
  ZWrtText &operator=(ZWrtText const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<ZWrtTextInternal::State> m_state;

  //! the main parser;
  ZWrtParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
