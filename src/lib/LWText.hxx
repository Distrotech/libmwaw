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
 * Parser to LightWay Text document
 *
 */
#ifndef LW_TEXT
#  define LW_TEXT

#include "libmwaw_internal.hxx"
#include "MWAWDebug.hxx"

class MWAWEntry;

namespace LWTextInternal
{
struct Font;
struct State;
}

class LWParser;

/** \brief the main class to read the text part of LightWay Text file
 *
 *
 *
 */
class LWText
{
  friend class LWParser;
public:
  //! constructor
  LWText(LWParser &parser);
  //! destructor
  virtual ~LWText();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:
  //! finds the different text zones
  bool createZones();

  //! send a main zone
  bool sendMainText();

  //! return a color corresponding to an id
  bool getColor(int id, MWAWColor &col) const;

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //
  // intermediate level
  //

  /** compute the positions */
  void computePositions();

  //! read the fonts ( styl resource)
  bool readFonts(MWAWEntry const &entry);
  //! read the Font2 resource ( underline, upperline, ...)
  bool readFont2(MWAWEntry const &entry);

  //! read the rulers (stylx resource)
  bool readRulers(MWAWEntry const &entry);
  /** send the paragraph properties */
  void setProperty(MWAWParagraph const &para);

  //! read the ruby data
  bool readRuby(MWAWEntry const &entry);

  //! read the header/footer part of the document zone
  bool readDocumentHF(MWAWEntry const &entry);
  //! returns true if there is a header/footer
  bool hasHeaderFooter(bool header) const;
  //! try to send the header/footer
  bool sendHeaderFooter(bool header);

  //! read the unknown styu resource
  bool readStyleU(MWAWEntry const &entry);

  //! read the styl resource
  bool readUnknownStyle(MWAWEntry const &entry);

private:
  LWText(LWText const &orig);
  LWText &operator=(LWText const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<LWTextInternal::State> m_state;

  //! the main parser;
  LWParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
