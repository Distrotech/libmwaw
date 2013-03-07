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
 * Parser to HanMac Word-J text document
 *
 */
#ifndef HMWJ_TEXT
#  define HMWJ_TEXT

#include "libmwaw_internal.hxx"
#include "MWAWDebug.hxx"

class MWAWParserState;
typedef shared_ptr<MWAWParserState> MWAWParserStatePtr;

class MWAWSubDocument;

namespace HMWJTextInternal
{
struct Font;
class SubDocument;
struct State;
}

class HMWJParser;

/** \brief the main class to read the text part of HanMac Word-J file
 *
 *
 *
 */
class HMWJText
{
  friend class HMWJTextInternal::SubDocument;
  friend class HMWJParser;
public:
  //! constructor
  HMWJText(HMWJParser &parser);
  //! destructor
  virtual ~HMWJText();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:
  //! send a text zone
  bool sendText(long id, long subId=0);
  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //
  // intermediate level
  //

  /** try to read the fonts name zone (type 5)*/
  bool readFontNames(MWAWEntry const &entry);
  /** try to read the style zone (type 3) */
  bool readStyles(MWAWEntry const &entry);

  //
  // low level
  //

private:
  HMWJText(HMWJText const &orig);
  HMWJText &operator=(HMWJText const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<HMWJTextInternal::State> m_state;

  //! the main parser;
  HMWJParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
