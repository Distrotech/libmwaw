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
#ifndef FW_MWAW_TEXT
#  define FW_MWAW_TEXT

#include <list>
#include <string>
#include <vector>

#include "libmwaw_tools.hxx"

#include "DMWAWPageSpan.hxx"

#include "MWAWPosition.hxx"

#include "MWAWEntry.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

typedef class MWAWContentListener FWContentListener;
typedef shared_ptr<FWContentListener> FWContentListenerPtr;

class MWAWFont;

class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

namespace FWTextInternal
{
struct Font;
struct Ruler;

struct Zone;

struct State;
}

struct FWEntry;
struct FWEntryManager;

class FWParser;

/** \brief the main class to read the text part of writenow file
 *
 *
 *
 */
class FWText
{
  friend class FWParser;
public:
  //! constructor
  FWText(MWAWInputStreamPtr ip, FWParser &parser, MWAWFontConverterPtr &convertissor);
  //! destructor
  virtual ~FWText();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:

  //! sets the listener in this class and in the helper classes
  void setListener(FWContentListenerPtr listen) {
    m_listener = listen;
  }

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //! send a main zone
  bool sendMainText();

  //! send a id zone
  bool send(int zId);

  //
  // intermediate level
  //

  //! check if a zone is a text zone, if so read it...
  bool readTextData(shared_ptr<FWEntry> zone);

  //! send the text
  bool send(shared_ptr<FWTextInternal::Zone> zone);

  //! send a simple line
  void send(shared_ptr<FWTextInternal::Zone> zone, int numChar,
            MWAWFont &font);

  //! check if a zone is a style zone, if so read it...
  bool readStyle(shared_ptr<FWEntry> zone);

  //! sort the different zones, finding the main zone, ...
  void sortZones();

  //
  // low level
  //

  /* send the character properties
   * \param font the font's properties */
  void setProperty(MWAWFont const &font, MWAWFont &previousFont);

  /* send the ruler properties */
  void setProperty(FWTextInternal::Ruler const &para);

  //! check if the input of the zone points to a paragraph zone, ...
  bool readParagraph(shared_ptr<FWEntry> zone);

  //! check if the input of the zone points to the columns definition, ...
  bool readColumns(shared_ptr<FWEntry> zone);

  //! check if the input of the zone points to the correspondance definition, ...
  bool readCorrespondance(shared_ptr<FWEntry> zone, bool extraCheck=false);
  //! check if the input of the zone points to a custom style, ...
  bool readStyleName(shared_ptr<FWEntry> zone);

private:
  FWText(FWText const &orig);
  FWText &operator=(FWText const &orig);

protected:
  //
  // data
  //
  //! the input
  MWAWInputStreamPtr m_input;

  //! the listener
  FWContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<FWTextInternal::State> m_state;

  //! the main parser;
  FWParser *m_mainParser;

};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
