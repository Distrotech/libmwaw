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
 * Parser to convert HanMac Word-J document
 */
#ifndef HMWJ_PARSER
#  define HMWJ_PARSER

#include <iostream>
#include <string>
#include <vector>

#include <libwpd/libwpd.h>

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"
#include "MWAWPageSpan.hxx"

#include "MWAWParser.hxx"

class MWAWEntry;
class MWAWFont;
class MWAWParagraph;

namespace HMWJParserInternal
{
struct State;
class SubDocument;
}

class HMWJGraph;
class HMWJText;

/** \brief the main class to read a HanMac Word-J file
 *
 *
 *
 */
class HMWJParser : public MWAWParser
{
  friend class HMWJGraph;
  friend class HMWJText;
  friend class HMWJParserInternal::SubDocument;

public:
  //! constructor
  HMWJParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~HMWJParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(WPXDocumentInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! creates the listener which will be associated to the document
  void createDocument(WPXDocumentInterface *documentInterface);

  //! finds the different objects zones in a Hapanese File
  bool createZones();

  //! returns the page height, ie. paper size less margin (in inches)
  float pageHeight() const;
  //! returns the page width, ie. paper size less margin (in inches)
  float pageWidth() const;
  //! returns the page left top point ( in inches)
  Vec2f getPageLeftTop() const;

  //! adds a new page
  void newPage(int number);

  // interface with the text parser

  //! send a text zone (not implemented)
  bool sendText(long id, long subId=0);

  // interface with the graph parser

  //! returns the color associated with a pattern
  bool getColor(int colId, int patternId, MWAWColor &color) const;

  //
  // low level
  //

  /** try to read the zones list */
  bool readZonesList();
  /** try to read a generic zone */
  bool readZone(MWAWEntry &entry);
  /** try to read a printinfo zone*/
  bool readPrintInfo(MWAWEntry const &entry);
  /** try to read a unknown zone, just after the header*/
  bool readZoneb();
  /** try to read a unknown zone ( zone 4) */
  bool readZone4(MWAWEntry const &entry);
  /** check if an entry is in file */
  bool isFilePos(long pos);

protected:
  //
  // data
  //
  //! the state
  shared_ptr<HMWJParserInternal::State> m_state;

  //! the actual document size
  MWAWPageSpan m_pageSpan;

  //! the graph parser
  shared_ptr<HMWJGraph> m_graphParser;

  //! the text parser
  shared_ptr<HMWJText> m_textParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
