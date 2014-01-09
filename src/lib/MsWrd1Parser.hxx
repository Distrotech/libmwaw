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

#ifndef MS_WRD1_PARSER
#  define MS_WRD1_PARSER

#include <vector>

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

namespace MsWrd1ParserInternal
{
struct Font;
struct Paragraph;
struct State;
class SubDocument;
}

/** \brief the main class to read a Microsoft Word 1 file
 *
 *
 *
 */
class MsWrd1Parser : public MWAWTextParser
{
  friend class MsWrd1ParserInternal::SubDocument;
public:
  //! constructor
  MsWrd1Parser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~MsWrd1Parser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(librevenge::RVNGTextInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! creates the listener which will be associated to the document
  void createDocument(librevenge::RVNGTextInterface *documentInterface);

  //! try to send the main zone
  void sendMain();

  //! adds a new page
  void newPage(int number);

  //! finds the different zones
  bool createZones();

  //! send the text structure to the listener
  bool sendText(MWAWEntry const &entry, bool main=false);

  //
  // internal level
  //

  /** try to read a char property */
  bool readFont(long fPos, MsWrd1ParserInternal::Font &font);

  /** try to read a paragraph property */
  bool readParagraph(long fPos, MsWrd1ParserInternal::Paragraph &para);

  /** try to read the footnote correspondance ( zone2 ) */
  bool readFootnoteCorrespondance(Vec2i limit);

  /** try to read the document info (zone 3) */
  bool readDocInfo(Vec2i limit);
  /** try to read the list of zones: separator between text and footnote? (zone 4) */
  bool readZones(Vec2i limit);
  /** try to read the page break (zone 5) */
  bool readPageBreak(Vec2i limit);

  /** prepare the data: separate header/footer zones to the main stream... */
  bool prepareTextZones();

  //
  // low level
  //

  /** shorten an entry if the last character is EOL */
  void removeLastCharIfEOL(MWAWEntry &entry);
  /** read the two first zones (char and paragraph) */
  bool readPLC(Vec2i limits, int wh);

  /** send the ruler properties */
  void setProperty(MsWrd1ParserInternal::Paragraph const &para);


protected:
  //
  // data
  //
  //! the state
  shared_ptr<MsWrd1ParserInternal::State> m_state;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
