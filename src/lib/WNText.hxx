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
 * Parser to WriteNow text document
 *
 */
#ifndef WN_TEXT
#  define WN_TEXT

#include <list>
#include <string>
#include <vector>

#include "libmwaw_internal.hxx"

#include "MWAWEntry.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

namespace WNTextInternal
{
struct ContentZone;
struct ContentZones;

struct Font;
struct Paragraph;

struct TableData;
struct Token;

struct Cell;

struct State;
}

struct WNEntry;
struct WNEntryManager;

class WNParser;

/** \brief the main class to read the text part of writenow file
 *
 *
 *
 */
class WNText
{
  friend class WNParser;
  friend struct WNTextInternal::Cell;
public:
  //! constructor
  WNText(WNParser &parser);
  //! destructor
  virtual ~WNText();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

  /** returns the header entry (if defined) */
  WNEntry getHeader() const;

  /** returns the footer entry (if defined) */
  WNEntry getFooter() const;

protected:
  //! finds the different text zones
  bool createZones();

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  /** try to read the text zone ( list of entries )
      and to create the text data zone */
  bool parseZone(WNEntry const &entry, std::vector<WNEntry> &listData);

  //! parse a text data zone ( and create the associated structure )
  shared_ptr<WNTextInternal::ContentZones> parseContent(WNEntry const &entry);

  /** send all the content zone of a zone defined by id
      0: main, 1  header/footer, 2: footnote
  */
  void sendZone(int id);

  //! send the text to the listener
  bool send(WNEntry const &entry);

  //! send the text to the listener
  bool send(std::vector<WNTextInternal::ContentZone> &listZones,
            std::vector<shared_ptr<WNTextInternal::ContentZones> > &footnoteList,
            WNTextInternal::Paragraph &ruler);

  /** sends a paragraph property to the listener */
  void setProperty(WNTextInternal::Paragraph const &ruler);

  //
  // low level
  //

  //! try to read the fonts zone
  bool readFontNames(WNEntry const &entry);

  //! read a font
  bool readFont(MWAWInputStream &input, bool inStyle, WNTextInternal::Font &font);

  //! read a paragraph format
  bool readParagraph(MWAWInputStream &input, WNTextInternal::Paragraph &ruler);

  //! read a token
  bool readToken(MWAWInputStream &input, WNTextInternal::Token &token);

  //! read a token (v2)
  bool readTokenV2(MWAWInputStream &input, WNTextInternal::Token &token);

  //! read a table frame (checkme)
  bool readTable(MWAWInputStream &input, WNTextInternal::TableData &table);

  //! try to read the styles zone
  bool readStyles(WNEntry const &entry);

private:
  WNText(WNText const &orig);
  WNText &operator=(WNText const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<WNTextInternal::State> m_state;

  //! the list of entry
  shared_ptr<WNEntryManager> m_entryManager;

  //! the main parser;
  WNParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
