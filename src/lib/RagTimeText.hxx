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
 * Parser to RagTime text document
 *
 */
#ifndef RAGTIME_TEXT
#  define RAGTIME_TEXT

#include <list>
#include <string>
#include <vector>

#include "libmwaw_internal.hxx"

#include "MWAWEntry.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

namespace RagTimeTextInternal
{
struct TextZone;
struct Token;

struct State;
}

class RagTimeParser;

/** \brief the main class to read the text part of ragTime file
 *
 *
 *
 */
class RagTimeText
{
  friend class RagTimeParser;
public:
  //! constructor
  RagTimeText(RagTimeParser &parser);
  //! destructor
  virtual ~RagTimeText();

  /** returns the file version */
  int version() const;

  //! returns a mac font id corresponding to a local id
  int getFontId(int localId) const;
  //! returns font style corresponding to a char style id
  bool getCharStyle(int charId, MWAWFont &font) const;

protected:
  /** try to read the font name: the FHFo structure: FileH?Font zone */
  bool readFontNames(MWAWEntry &entry);
  //! try to read the character properties zone: FHsl zone
  bool readCharProperties(MWAWEntry &entry);

  //! try to read a text zone (knowing the zone width in point and the font color)
  bool readTextZone(MWAWEntry &entry, int width, MWAWColor const &fontColor=MWAWColor::black());
  //! try to read the character properties (knowing the font color)
  bool readFonts(RagTimeTextInternal::TextZone &zone, MWAWColor const &color, long endPos);
  //! try to read the paragraph properties (knowing the zone width in point used to determine the right margin)
  bool readParagraphs(RagTimeTextInternal::TextZone &zone, int width, long endPos);
  //! try to read the token zones
  bool readTokens(RagTimeTextInternal::TextZone &zone, long endPos);

  //! try to send a text zone
  bool send(int id);
  //! flush extra zone
  void flushExtra();

  //
  // low level
  //

  //! try to send a text zone
  bool send(RagTimeTextInternal::TextZone const &zone);

private:
  RagTimeText(RagTimeText const &orig);
  RagTimeText &operator=(RagTimeText const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<RagTimeTextInternal::State> m_state;

  //! the main parser;
  RagTimeParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
