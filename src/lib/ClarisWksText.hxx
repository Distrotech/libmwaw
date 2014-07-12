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
 * Parser to Claris Works text document
 *
 */
#ifndef CLARIS_WKS_TEXT
#  define CLARIS_WKS_TEXT

#include <list>
#include <string>
#include <vector>

#include "libmwaw_internal.hxx"

#include "MWAWEntry.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

#include "ClarisWksStruct.hxx"

namespace ClarisWksTextInternal
{
class SubDocument;

struct Paragraph;
struct Zone;
struct State;
}

class ClarisWksDocument;
class ClarisWksParser;
class ClarisWksStyleManager;

/** \brief the main class to read the text part of Claris Works file
 *
 *
 *
 */
class ClarisWksText
{
  friend class ClarisWksTextInternal::SubDocument;
  friend class ClarisWksDocument;
  friend class ClarisWksParser;
  friend class ClarisWksStyleManager;

public:
  //! constructor
  ClarisWksText(ClarisWksDocument &document);
  //! destructor
  virtual ~ClarisWksText();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;
  /** updates the page span list and returns true if this is possible */
  bool updatePageSpanList(MWAWPageSpan const &page, std::vector<MWAWPageSpan> &spanList);

  //! reads the zone Text DSET
  shared_ptr<ClarisWksStruct::DSET> readDSETZone(ClarisWksStruct::DSET const &zone, MWAWEntry const &entry, bool &complete);

protected:
  /** sends a paragraph property to the listener */
  void setProperty(MWAWListener &listener, ClarisWksTextInternal::Paragraph const &ruler, int listId=-1);

  //! sends the zone data to the listener (if it exists )
  bool sendZone(int number, MWAWListenerPtr listener=MWAWListenerPtr());
  //! check if we can send a textzone as graphic
  bool canSendTextAsGraphic(int number) const;

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //
  // low level
  //

  //! try to read the paragraph
  bool readParagraphs(MWAWEntry const &entry, ClarisWksTextInternal::Zone &zone);

  //! try to read a font sequence
  bool readFonts(MWAWEntry const &entry, ClarisWksTextInternal::Zone &zone);

  //! try to the token zone)
  bool readTokens(MWAWEntry const &entry, ClarisWksTextInternal::Zone &zone);

  //! try to read the text zone size
  bool readTextZoneSize(MWAWEntry const &entry, ClarisWksTextInternal::Zone &zone);

  //! try to read the section
  bool readTextSection(ClarisWksTextInternal::Zone &zone);

  //! send the text zone to the listener
  bool sendText(ClarisWksTextInternal::Zone const &zone, MWAWListenerPtr listener);
  //! check if we can send a textzone has graphic
  bool canSendTextAsGraphic(ClarisWksTextInternal::Zone const &zone) const;

  //! try to find a list id which corresponds to the list beginning in actPos
  int findListId(ClarisWksTextInternal::Zone const &zone, int actListId, long cPos, long &lastPos);

  //! try to read a font
  bool readFont(int id, int &posC, MWAWFont &font);

  /** read the rulers block which is present at the beginning of the text in the first version of Claris Works : v1-2 */
  bool readParagraphs();

  /** the definition of ruler :
      present at the beginning of the text in the first version of Claris Works : v1-2,
      present in the STYL entries in v4-v6 files */
  bool readParagraph(int id=-1);

  // THE NAMED ENTRY

  /** read a STYL Paragraph sequence */
  bool readSTYL_RULR(int N, int fSz);

private:
  ClarisWksText(ClarisWksText const &orig);
  ClarisWksText &operator=(ClarisWksText const &orig);

protected:
  //
  // data
  //

  //! the document
  ClarisWksDocument &m_document;

  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<ClarisWksTextInternal::State> m_state;

  //! the main parser;
  MWAWParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
