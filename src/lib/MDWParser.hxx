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
 * Parser to convert MindWrite document
 */
#ifndef MDW_PARSER
#  define MDW_PARSER

#include <string>
#include <vector>

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

namespace MDWParserInternal
{
struct LineInfo;
struct State;
class SubDocument;
}

/** \brief the main class to read a MindWrite file
 *
 *
 *
 */
class MDWParser : public MWAWParser
{
  friend class MDWParserInternal::SubDocument;

public:
  //! constructor
  MDWParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~MDWParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(WPXDocumentInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! creates the listener which will be associated to the document
  void createDocument(WPXDocumentInterface *documentInterface);

  //! finds the different objects zones
  bool createZones();

  //! try to send a zone
  bool sendZone(int i);

  //! try to read a graphic
  bool readGraphic(MDWParserInternal::LineInfo const &line);

  //! try to read a ruler
  bool readRuler(MDWParserInternal::LineInfo &line);

  //! try to read a compressed text zone
  bool readCompressedText(MDWParserInternal::LineInfo const &line);

  //! try to read a non compressed text zone
  bool readText(MDWParserInternal::LineInfo const &line);

  //! try to send the text
  void sendText(std::string const &text, std::vector<MWAWFont> const &fonts, std::vector<int> const &textPos);
  //! try to send the header/footer zone
  void sendHeaderFooter(bool header);
  //! try to send the header/footer fields as text (in a textbox)
  void sendHeaderFooterFields(bool header);
  //! try to read the fonts
  bool readFonts(MWAWEntry const &entry, std::vector<MWAWFont> &fonts, std::vector<int> &textPos);

  //! read the print info zone
  bool readPrintInfo(MWAWEntry &entry);

  //! read the lines information zone
  bool readLinesInfo(MWAWEntry &entry);

  //! read the last zone ( pos + 7fffffff )
  bool readLastZone(MWAWEntry &entry);

  //! read the 8th zone ( unknown zone)
  bool readZone8(MWAWEntry &entry);
  //! read the heading state
  bool readHeadingStates(MWAWEntry &entry);
  //! read the heading field zone ( zone 12 )
  bool readHeadingFields(MWAWEntry &entry);
  //! read the heading property zone
  bool readHeadingProperties(MWAWEntry &entry);
  //! read the heading custom zone
  bool readHeadingCustom(MWAWEntry &entry);

  /** sends a paragraph property to the listener */
  void setProperty(MWAWParagraph const &para);

  //! adds a new page
  void newPage(int number);

  //
  // low level
  //

  //! read a file entry
  MWAWEntry readEntry();

protected:
  //
  // data
  //
  //! the state
  shared_ptr<MDWParserInternal::State> m_state;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
