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
 * Text parser to BeagleWorks document
 *
 */
#ifndef BEAGLE_WKS_TEXT
#  define BEAGLE_WKS_TEXT

#include "libmwaw_internal.hxx"
#include "MWAWDebug.hxx"

namespace BeagleWksTextInternal
{
struct Font;
struct Section;
struct State;

class SubDocument;
}

class BeagleWksParser;
class BeagleWksStructManager;

/** \brief the main class to read the text part of BeagleWorks Text file
 *
 *
 *
 */
class BeagleWksText
{
  friend class BeagleWksParser;
  friend class BeagleWksTextInternal::SubDocument;
public:
  //! constructor
  BeagleWksText(BeagleWksParser &parser);
  //! destructor
  virtual ~BeagleWksText();

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
  //! try to send a text zone
  bool sendText(MWAWEntry entry);
  //! try to send a header/footer id
  bool sendHF(int hfId, int sectId);
  //! returns the font
  MWAWFont getFont(BeagleWksTextInternal::Font const &ft) const;
  //! try to read a font properties
  bool readFont(BeagleWksTextInternal::Font &font, long endPos);
  //! try to read a paragraph knowing end pos
  bool readParagraph(MWAWParagraph &para, long endPos, bool inSection=false);
  //! try to read a section
  bool readSection(MWAWEntry const &entry, BeagleWksTextInternal::Section &section);
private:
  BeagleWksText(BeagleWksText const &orig);
  BeagleWksText &operator=(BeagleWksText const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<BeagleWksTextInternal::State> m_state;
  //! the structure manager
  shared_ptr<BeagleWksStructManager> m_structureManager;

  //! the main parser;
  BeagleWksParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
