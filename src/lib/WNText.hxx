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

#include "libmwaw_tools.hxx"

#include "DMWAWPageSpan.hxx"

#include "TMWAWPosition.hxx"

#include "IMWAWEntry.hxx"
#include "IMWAWContentListener.hxx"
#include "IMWAWSubDocument.hxx"

#include "TMWAWDebug.hxx"
#include "TMWAWInputStream.hxx"

#include "IMWAWParser.hxx"

typedef class MWAWContentListener WNContentListener;
typedef shared_ptr<WNContentListener> WNContentListenerPtr;

namespace MWAWStruct
{
struct Font;
}

namespace MWAWTools
{
class Convertissor;
typedef shared_ptr<Convertissor> ConvertissorPtr;
}

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
  friend class WNTextInternal::Cell;
public:
  //! constructor
  WNText(TMWAWInputStreamPtr ip, WNParser &parser, MWAWTools::ConvertissorPtr &convertissor);
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

  /*
   * \param font the font's properties
   * \param force if false, we only sent differences from the actual font */
  void setProperty(MWAWStruct::Font const &font,
                   MWAWStruct::Font &previousFont,
                   bool force = false);
  /** sends a paragraph property to the listener */
  void setProperty(WNTextInternal::Ruler const &ruler);

  //
  // low level
  //

  //! try to read the fonts zone
  bool readFontNames(WNEntry const &entry);

  //! read a font
  bool readFont(TMWAWInputStream &input, bool inStyle, WNTextInternal::Font &font);

  //! read a ruler
  bool readRuler(TMWAWInputStream &input, WNTextInternal::Ruler &ruler);

  //! read a token
  bool readToken(TMWAWInputStream &input, WNTextInternal::Token &token);

  //! read a table frame (checkme)
  bool readTable(TMWAWInputStream &input, WNTextInternal::TableData &table);

  //! try to read the styles zone
  bool readStyles(WNEntry const &entry);

  //! returns the debug file
  libmwaw_tools::DebugFile &ascii() {
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
  TMWAWInputStreamPtr m_input;

  //! the listener
  WNContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWTools::ConvertissorPtr m_convertissor;

  //! the state
  shared_ptr<WNTextInternal::State> m_state;

  //! the list of entry
  shared_ptr<WNEntryManager> m_entryManager;

  //! the main parser;
  WNParser *m_mainParser;

  //! the debug file
  libmwaw_tools::DebugFile &m_asciiFile;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
