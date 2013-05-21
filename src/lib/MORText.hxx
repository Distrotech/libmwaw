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
 * Parser to More document
 *
 */
#ifndef MOR_TEXT
#  define MOR_TEXT

#include "libmwaw_internal.hxx"
#include "MWAWDebug.hxx"

class MWAWInputStream;
typedef shared_ptr<MWAWInputStream> MWAWInputStreamPtr;

class MWAWEntry;
class MWAWFont;
class MWAWParserState;
typedef shared_ptr<MWAWParserState> MWAWParserStatePtr;

namespace MORTextInternal
{
struct Paragraph;
struct State;
}

class MORParser;

/** \brief the main class to read the text part of More Text file
 *
 *
 *
 */
class MORText
{
  friend class MORParser;
public:
  //! constructor
  MORText(MORParser &parser);
  //! destructor
  virtual ~MORText();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:
  //! try to create the text zones
  bool createZones();
  //! send a main zone
  bool sendMainText();

  //
  // intermediate level
  //

  //! read the list of topic positions
  bool readTopic(MWAWEntry const &entry);

  //! read the list of comment/header/footer zones
  bool readComment(MWAWEntry const &entry);

  //! read a text entry
  bool readText(MWAWEntry const &entry);

  //! read the list of fonts
  bool readFonts(MWAWEntry const &entry);

  //! read the list of outlines
  bool readOutlineList(MWAWEntry const &entry);

  //! read a outline
  bool readOutline(MWAWEntry const &entry);

  /** try to read a fontname

  \note: fId is set to -1 is the field contains only a fontname
   */
  bool readFont(MWAWEntry const &entry, std::string &fName, int &fId);
  /** try to read some tabs */
  bool readTabs(MWAWEntry const &entry, MORTextInternal::Paragraph &para, std::string &mess);
  //! try to read either a font, a fontname, a pattern, a int
  bool readValue(MWAWEntry const &entry, long fDecal);

private:
  MORText(MORText const &orig);
  MORText &operator=(MORText const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<MORTextInternal::State> m_state;

  //! the main parser;
  MORParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
