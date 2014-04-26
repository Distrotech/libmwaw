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
 * Parser to BeagleWorks document
 *
 */
#ifndef BW_TEXT
#  define BW_TEXT

#include "libmwaw_internal.hxx"
#include "MWAWDebug.hxx"

namespace BWTextInternal
{
struct Font;
struct Section;
struct State;

class SubDocument;
}

class BWParser;

/** \brief the main class to read the text part of BeagleWorks Text file
 *
 *
 *
 */
class BWText
{
  friend class BWParser;
  friend class BWTextInternal::SubDocument;
public:
  //! constructor
  BWText(BWParser &parser);
  //! destructor
  virtual ~BWText();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:
  //! try to create the text zones
  bool createZones(MWAWEntry &entry);
  //! send a main zone
  bool sendMainText();
  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //
  // intermediate level
  //

  //! update the number of pages and the number of page by section
  void countPages();
  //! return an header subdocument
  shared_ptr<MWAWSubDocument> getHeader(int page, int &numSimillar);
  //! return a footer subdocument
  shared_ptr<MWAWSubDocument> getFooter(int page, int &numSimillar);
  /** update the page span list */
  void updatePageSpanList(std::vector<MWAWPageSpan> &spanList);
  //! try to the font names
  bool readFontsName(MWAWEntry &entry);
  //! try to send a text zone
  bool sendText(MWAWEntry entry);
  //! try to send a header/footer id
  bool sendHF(int hfId, int sectId);
  //! try to read a font properties
  bool readFont(BWTextInternal::Font &font, long endPos);
  //! try to read a paragraph knowing end pos
  bool readParagraph(MWAWParagraph &para, long endPos, bool inSection=false);
  //! try to read a section
  bool readSection(MWAWEntry const &entry, BWTextInternal::Section &section);
private:
  BWText(BWText const &orig);
  BWText &operator=(BWText const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<BWTextInternal::State> m_state;

  //! the main parser;
  BWParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab: