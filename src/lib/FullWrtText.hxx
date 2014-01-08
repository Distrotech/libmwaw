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
 * Parser to FullWrite text document
 *
 */
#ifndef FULL_WRT_TEXT
#  define FULL_WRT_TEXT

#include "libmwaw_internal.hxx"


#include "MWAWDebug.hxx"

#include "FullWrtStruct.hxx"

namespace FullWrtTextInternal
{
struct Font;
struct Paragraph;

struct LineHeader;
struct Zone;

struct State;
}

class FullWrtParser;

/** \brief the main class to read the text part of writenow file
 *
 *
 *
 */
class FullWrtText
{
  friend class FullWrtParser;
public:
  //! constructor
  FullWrtText(FullWrtParser &parser);
  //! destructor
  virtual ~FullWrtText();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //! send a main zone
  bool sendMainText();
  //! return the header/footer blockid ( or -1)
  int getHeaderFooterId(bool header, int page, int &numSimillar) const;

  //! send a id zone
  bool send(int zId, MWAWColor fontColor=MWAWColor::black());

  //
  // intermediate level
  //

  //! check if a zone is a text zone, if so read it...
  bool readTextData(FullWrtStruct::EntryPtr zone);

  //! send the text
  bool send(shared_ptr<FullWrtTextInternal::Zone> zone, MWAWColor fontColor=MWAWColor::black());

  //! send a simple line
  void send(shared_ptr<FullWrtTextInternal::Zone> zone, int numChar,
            FullWrtTextInternal::Font &font, FullWrtTextInternal::Paragraph &ruler,
            std::string &str);

  //! try send a table row
  bool sendTable(shared_ptr<FullWrtTextInternal::Zone> zone,
                 FullWrtTextInternal::LineHeader const &lHeader,
                 FullWrtTextInternal::Font &font, FullWrtTextInternal::Paragraph &ruler,
                 std::string &str);
  //! send a hidden item
  bool sendHiddenItem(int id, FullWrtTextInternal::Font &font, FullWrtTextInternal::Paragraph &ruler);

  //! prepare the different data (called sortZones and createItemStructures)
  void prepareData()
  {
    sortZones();
    createItemStructures();
  }

  //! sort the different zones, finding the main zone, ...
  void sortZones();
  //! create the item structures
  void createItemStructures();

  //
  // low level
  //

  //! try to read the header of a line
  bool readLineHeader(shared_ptr<FullWrtTextInternal::Zone> zone, FullWrtTextInternal::LineHeader &lHeader);

  //! check if the input of the zone points to a item zone in DataStruct Zone
  bool readItem(FullWrtStruct::EntryPtr zone, int id=-1, bool hidden=false);

  //! check if the input of the zone points to a paragraph zone in DataStruct Zone
  bool readParagraphTabs(FullWrtStruct::EntryPtr zone, int id=-1);
  //! try to read the paragraph modifier (at the end of doc info)
  bool readParaModDocInfo(FullWrtStruct::EntryPtr zone);

  //! try to read a style
  bool readStyle(FullWrtStruct::EntryPtr zone);

  //! try to read the font/paragraph modifier zone (Zone1f)
  bool readDataMod(FullWrtStruct::EntryPtr zone, int id);

  //! check if the input of the zone points to the columns definition, ...
  bool readColumns(FullWrtStruct::EntryPtr zone);

private:
  FullWrtText(FullWrtText const &orig);
  FullWrtText &operator=(FullWrtText const &orig);

protected:
  //
  // data
  //
  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<FullWrtTextInternal::State> m_state;

  //! the main parser;
  FullWrtParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
