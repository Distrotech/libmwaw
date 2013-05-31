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
 * Parser to Microsoft Works text document
 *
 */
#ifndef MSK3_TEXT
#  define MSK3_TEXT

#include <list>
#include <string>
#include <vector>

#include "libmwaw_internal.hxx"

#include "MWAWEntry.hxx"

namespace MSK3TextInternal
{
struct Font;
struct LineZone;
struct TextZone;
struct State;
}

class MSK3Parser;

/** \brief the main class to read the text part of Microsoft Works file
 *
 *
 *
 */
class MSK3Text
{
  friend class MSK3Parser;
public:
  //! constructor
  MSK3Text(MSK3Parser &parser);
  //! destructor
  virtual ~MSK3Text();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages(int zoneId) const;

protected:
  //! finds the different text zones. Returns the zoneId or -1.
  int createZones(int numLines=-1, bool mainZone=false);

  // reads the header/footer string : version v1-2
  std::string readHeaderFooterString(bool header);

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //! send a zone
  void sendZone(int zoneId);

  //! send a note
  void sendNote(int zoneId, int noteId);

  //! returns a header zone id ( or -1 )
  int getHeader() const;

  //! returns a footer zone id ( or -1 )
  int getFooter() const;

  //! return the lines and pages height ( for v1, ...)
  bool getLinesPagesHeight(int zoneId,
                           std::vector<int> &lines,
                           std::vector<int> &pages);

  //
  // low level
  //

  //! try to read a zone header
  bool readZoneHeader(MSK3TextInternal::LineZone &zone) const;

  //! prepare a zone
  void update(MSK3TextInternal::TextZone &zone);

  //! prepare the note zones given a zone and the position of the first note
  void updateNotes(MSK3TextInternal::TextZone &zone, int firstNote);

  /** sends the zone data to the listener. You can set limit to send
      a subzone data ( like note ) */
  void send(MSK3TextInternal::TextZone &zone, Vec2i limit=Vec2i(-1,-1));

  //! tries to read a font
  bool readFont(MSK3TextInternal::Font &font, long endPos);

  //! tries to read a paragraph
  bool readParagraph(MSK3TextInternal::LineZone &zone, MWAWParagraph &parag);

  //! tries to send a text zone
  bool sendText(MSK3TextInternal::LineZone &zone, int zoneId);

  //! tries to send a string (for v1-2, header/footer zone)
  bool sendString(std::string &str);

private:
  MSK3Text(MSK3Text const &orig);
  MSK3Text &operator=(MSK3Text const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<MSK3TextInternal::State> m_state;

  //! the main parser;
  MSK3Parser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
