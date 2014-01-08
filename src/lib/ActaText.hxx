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
 * Parser to Acta document
 *
 */
#ifndef AC_TEXT
#  define AC_TEXT

#include "libmwaw_internal.hxx"
#include "MWAWDebug.hxx"

namespace ActaTextInternal
{
struct Topic;
struct State;
}

class ActaParser;

/** \brief the main class to read the text part of Acta Text file
 *
 *
 *
 */
class ActaText
{
  friend class ActaParser;
public:
  //! constructor
  ActaText(ActaParser &parser);
  //! destructor
  virtual ~ActaText();

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

  //! return the color which corresponds to an id (if possible)
  bool getColor(int id, MWAWColor &col) const;

  //! try to read the topic definitions (line or graphic)
  bool readTopic();

  //! try to send a topic
  bool sendTopic(ActaTextInternal::Topic const &topic);

  //! try to read a text entry
  bool sendText(ActaTextInternal::Topic const &topic);

  //! try to read a graphic
  bool sendGraphic(ActaTextInternal::Topic const &topic);

  //! try to read a font
  bool readFont(MWAWFont &font, bool inPLC);

private:
  ActaText(ActaText const &orig);
  ActaText &operator=(ActaText const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<ActaTextInternal::State> m_state;

  //! the main parser
  ActaParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
