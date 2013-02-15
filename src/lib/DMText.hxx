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
 * Parser to Nisus text document
 *
 */
#ifndef DM_TEXT
#  define DM_TEXT

#include "libmwaw_internal.hxx"
#include "MWAWDebug.hxx"

class MWAWInputStream;
typedef shared_ptr<MWAWInputStream> MWAWInputStreamPtr;

class MWAWEntry;
class MWAWFont;
class MWAWParserState;
typedef shared_ptr<MWAWParserState> MWAWParserStatePtr;
class MWAWPageSpan;
class MWAWSubDocument;

namespace DMTextInternal
{
struct Zone;

class SubDocument;
struct State;
}

class DMParser;

/** \brief the main class to read the text part of DocMaker file
 *
 *
 *
 */
class DMText
{
  friend class DMTextInternal::SubDocument;
  friend class DMParser;
public:
  //! constructor
  DMText(DMParser &parser);
  //! destructor
  virtual ~DMText();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

  /** returns the number of chapter */
  int numChapters() const;

  //! send a string as comment
  void sendComment(std::string const &str);

protected:

  //! finds the different text zones
  bool createZones();

  //! send a main zone
  bool sendMainText();

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //
  // intermediate level
  //

  /** compute the number of page of a zone*/
  void computeNumPages(DMTextInternal::Zone const &zone) const;

  /** update the page span list */
  void updatePageSpanList(std::vector<MWAWPageSpan> &spanList);

  /** try to send the text corresponding to a zone */
  bool sendText(DMTextInternal::Zone const &zone);

  //! try to read the font name ( resource rQDF )
  bool readFontNames(MWAWEntry const &entry);

  //! try to read the styles ( resource styl )
  bool readStyles(MWAWEntry const &entry);

  //! try to read a TOC zone? ( resource cnt# )
  bool readTOC(MWAWEntry const &entry);

  //! try to send a TOC zone
  bool sendTOC();

  //! try to read the windows information zone? ( resource Wndo )
  bool readWindows(MWAWEntry const &entry);

  //! try to read the footer zone ( resource foot )
  bool readFooter(MWAWEntry const &entry);

  //! try to send a footer corresponding to a zone id
  bool sendFooter(int zId);

  //
  // low level
  //

  //! send a string to the listener
  void sendString(std::string const &str) const;
private:
  DMText(DMText const &orig);
  DMText &operator=(DMText const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<DMTextInternal::State> m_state;

  //! the main parser;
  DMParser *m_mainParser;

};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
