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
 * Parser to GreatWorks document
 *
 */
#ifndef GW_TEXT
#  define GW_TEXT

#include "libmwaw_internal.hxx"
#include "MWAWDebug.hxx"

namespace GWTextInternal
{
struct State;
struct Token;
struct Zone;
}

class GWParser;

/** \brief the main class to read the text part of GreatWorks Text file
 *
 *
 *
 */
class GWText
{
  friend class GWParser;
public:
  //! constructor
  GWText(GWParser &parser);
  //! destructor
  virtual ~GWText();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:
  //! finds the different objects zones
  bool createZones(int expectedHF);
  //! send a main zone
  bool sendMainText();
  //! return the number of header/footer zones
  int numHFZones() const;
  //! try to send the i^th header/footer
  bool sendHF(int id);
  //! try to send the textbox text
  bool sendTextbox(MWAWEntry const &entry);
  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //
  // intermediate level
  //

  //! try to read the font names zone
  bool readFontNames();
  //! try to read a zone ( textheader+fonts+rulers)
  bool readZone(GWTextInternal::Zone &zone);
  //! try to read the end of a zone ( line + frame position )
  bool readZonePositions(GWTextInternal::Zone &zone);
  //! try to send a zone
  bool sendZone(GWTextInternal::Zone const &zone);
  //! try to send simplified textbox zone
  bool sendSimpleTextbox(MWAWEntry const &entry);
  //! try to read a font
  bool readFont(MWAWFont &font);
  //! try to read a ruler
  bool readRuler(MWAWParagraph &para);
  //! try to read a token
  bool readToken(GWTextInternal::Token &token, long &nChar);

  //! heuristic function used to find the next zone
  bool findNextZone();

private:
  GWText(GWText const &orig);
  GWText &operator=(GWText const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<GWTextInternal::State> m_state;

  //! the main parser;
  GWParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
