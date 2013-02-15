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
 * Parser to Mariner Write text document
 *
 */
#ifndef MRW_TEXT
#  define MRW_TEXT

#include "libmwaw_internal.hxx"
#include "MWAWDebug.hxx"

class MWAWEntry;
class MWAWFont;
class MWAWParserState;
typedef shared_ptr<MWAWParserState> MWAWParserStatePtr;
class MWAWSubDocument;

namespace MRWTextInternal
{
struct Paragraph;
struct State;
struct Table;
struct Zone;
}

struct MRWEntry;
class MRWParser;

/** \brief the main class to read the text part of Mariner Write file
 *
 *
 *
 */
class MRWText
{
  friend class MRWParser;
public:
  //! constructor
  MRWText(MRWParser &parser);
  //! destructor
  virtual ~MRWText();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:
  /** sends a paragraph property to the listener */
  void setProperty(MRWTextInternal::Paragraph const &ruler);

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //
  // intermediate level
  //
  /** try to send a zone (knowing zoneId) */
  bool send(int zoneId);
  /** try to send a zone */
  bool send(MRWTextInternal::Zone const &zone, MWAWEntry const &entry);

  /** try to find the table structure beginning in actual position */
  bool findTableStructure(MRWTextInternal::Table &table, MWAWEntry const &entry);
  /** try to send a table */
  bool sendTable(MRWTextInternal::Table &table);
  /** try to read the text struct */
  bool readTextStruct(MRWEntry const &entry, int zoneId);
  /** try to read a text zone */
  bool readZone(MRWEntry const &entry, int zoneId);
  /** try to compute the number of pages of a zone, returns 0 if not data */
  int computeNumPages(MRWTextInternal::Zone const &zone) const;
  /** try to read a font zone */
  bool readFonts(MRWEntry const &entry, int zoneId);

  /** try to read a font name zone */
  bool readFontNames(MRWEntry const &entry, int zoneId);

  /** try to read a ruler zone */
  bool readRulers(MRWEntry const &entry, int zoneId);

  /** try to read a PLC zone: position in text to char(zone 4) or ruler(zone 5) id */
  bool readPLCZone(MRWEntry const &entry, int zoneId);

  /** try to read a style name zone */
  bool readStyleNames(MRWEntry const &entry, int zoneId);

private:
  MRWText(MRWText const &orig);
  MRWText &operator=(MRWText const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<MRWTextInternal::State> m_state;

  //! the main parser;
  MRWParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
