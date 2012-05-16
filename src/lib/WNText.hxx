/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libmwaw
 * Copyright (C) 2009, 2011 Alonso Laurent (alonso@loria.fr)
 * Copyright (C) 2006, 2007 Andrew Ziem
 * Copyright (C) 2004-2006 Fridrich Strba (fridrich.strba@bluewin.ch)
 * Copyright (C) 2004 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2003-2005 William Lachance (william.lachance@sympatico.ca)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 * For further information visit http://libwps.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

/*
 * Parser to WriteNow text document
 *
 */
#ifndef WN_MWAW_TEXT
#  define WN_MWAW_TEXT

#include <list>
#include <string>
#include <vector>

#include "libmwaw_internal.hxx"

#include "MWAWPosition.hxx"

#include "MWAWEntry.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

typedef class MWAWContentListener WNContentListener;
typedef shared_ptr<WNContentListener> WNContentListenerPtr;

class MWAWFont;

class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

namespace WNTextInternal
{
struct ContentZone;
struct ContentZones;

struct Font;
struct Ruler;

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
  WNText(MWAWInputStreamPtr ip, WNParser &parser, MWAWFontConverterPtr &convertissor);
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

  //! sets the listener in this class and in the helper classes
  void setListener(WNContentListenerPtr listen) {
    m_listener = listen;
  }

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
            WNTextInternal::Ruler &ruler);

  /* sends a font property to the listener

   * \param font the font's properties */
  void setProperty(MWAWFont const &font, MWAWFont &previousFont);
  /** sends a paragraph property to the listener */
  void setProperty(WNTextInternal::Ruler const &ruler);

  //
  // low level
  //

  //! try to read the fonts zone
  bool readFontNames(WNEntry const &entry);

  //! read a font
  bool readFont(MWAWInputStream &input, bool inStyle, WNTextInternal::Font &font);

  //! read a ruler
  bool readRuler(MWAWInputStream &input, WNTextInternal::Ruler &ruler);

  //! read a token
  bool readToken(MWAWInputStream &input, WNTextInternal::Token &token);

  //! read a token (v2)
  bool readTokenV2(MWAWInputStream &input, WNTextInternal::Token &token);

  //! read a table frame (checkme)
  bool readTable(MWAWInputStream &input, WNTextInternal::TableData &table);

  //! try to read the styles zone
  bool readStyles(WNEntry const &entry);

  //! returns the debug file
  libmwaw::DebugFile &ascii() {
    return m_asciiFile;
  }

private:
  WNText(WNText const &orig);
  WNText &operator=(WNText const &orig);

protected:
  //
  // data
  //
  //! the input
  MWAWInputStreamPtr m_input;

  //! the listener
  WNContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<WNTextInternal::State> m_state;

  //! the list of entry
  shared_ptr<WNEntryManager> m_entryManager;

  //! the main parser;
  WNParser *m_mainParser;

  //! the debug file
  libmwaw::DebugFile &m_asciiFile;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
