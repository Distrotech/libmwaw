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
 * Parser to convert some WriterPlus text document ( a french text editor )
 *
 */
#ifndef WRITER_PLS_PARSER
#  define WRITER_PLS_PARSER

#include <vector>

#include "MWAWDebug.hxx"
#include "MWAWEntry.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

namespace WriterPlsParserInternal
{
struct State;
struct Font;
struct Line;
struct ParagraphData;
struct ParagraphInfo;
class SubDocument;
}

/** \brief the main class to read a Writerperfect file
 *
 *
 *
 */
class WriterPlsParser : public MWAWTextParser
{
  friend class WriterPlsParserInternal::SubDocument;

public:
  //! constructor
  WriterPlsParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~WriterPlsParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(librevenge::RVNGTextInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! creates the listener which will be associated to the document
  void createDocument(librevenge::RVNGTextInterface *documentInterface);

  //! finds the different objects zones
  bool createZones();

  //! read the print info zone
  bool readPrintInfo();

  //! read the main info for zone ( 0: MAIN ZONE, 1 : HEADER, 2 : FOOTER )
  bool readWindowsInfo(int zone);

  //! send a zone ( 0: MAIN ZONE, 1 : HEADER, 2 : FOOTER )
  bool sendWindow(int zone, Vec2i limits = Vec2i(-1,-1));

  //! read the page info zone
  bool readWindowsZone(int zone);

  //! read the page info zone
  bool readPageInfo(int zone);

  //! read the col info zone ?
  bool readColInfo(int zone);

  //! read the paragraph info zone
  bool readParagraphInfo(int zone);

  //! try to find the data which correspond to a section ( mainly column )
  bool findSection(int zone, Vec2i limits, MWAWSection &sec);

  //! read a section
  bool readSection(WriterPlsParserInternal::ParagraphInfo const &info, bool mainBlock);

  //! read a text
  bool readText(WriterPlsParserInternal::ParagraphInfo const &info);

  //! read a table
  bool readTable(WriterPlsParserInternal::ParagraphInfo const &info);

  //! read a graphic
  bool readGraphic(WriterPlsParserInternal::ParagraphInfo const &info);

  //! read a unknown section
  bool readUnknown(WriterPlsParserInternal::ParagraphInfo const &info);

  //! returns the page height, ie. paper size less margin (in inches) and header/footer
  double getTextHeight() const;

  //! adds a new page
  void newPage(int number);

  //
  // low level
  //

  //! reads a paragraph data
  bool readParagraphData(WriterPlsParserInternal::ParagraphInfo const &info, bool hasFonts,
                         WriterPlsParserInternal::ParagraphData &data);
  //! returns a paragraph corresponding to a paragraph data
  MWAWParagraph getParagraph(WriterPlsParserInternal::ParagraphData const &data);
  //! reads a list of font (with position)
  bool readFonts(int nFonts, int type,
                 std::vector<WriterPlsParserInternal::Font> &fonts);

  //! reads a list of line (with position)
  bool readLines(WriterPlsParserInternal::ParagraphInfo const &info,
                 int nLines, std::vector<WriterPlsParserInternal::Line> &lines);

protected:
  //
  // data
  //

  //! the state
  shared_ptr<WriterPlsParserInternal::State> m_state;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
