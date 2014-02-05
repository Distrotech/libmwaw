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

#ifndef MAC_WRT_PARSER
#  define MAC_WRT_PARSER

#include <vector>

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

namespace MacWrtParserInternal
{
struct State;
struct Information;
class SubDocument;
}

/** \brief the main class to read a MacWrite file
 *
 *
 *
 */
class MacWrtParser : public MWAWTextParser
{
  friend class MacWrtParserInternal::SubDocument;

public:
  //! constructor
  MacWrtParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~MacWrtParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(librevenge::RVNGTextInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! creates the listener which will be associated to the document
  void createDocument(librevenge::RVNGTextInterface *documentInterface);

  //! send a zone ( 0: MAIN ZONE, 1 : HEADER, 2 : FOOTER )
  bool sendWindow(int zone);

  //! finds the different objects zones
  bool createZones();

  //! finds the different objects zones (version <= 3)
  bool createZonesV3();

  //! read the print info zone
  bool readPrintInfo();

  //! read the windows zone
  bool readWindowsInfo(int wh);

  //! read the line height
  bool readLinesHeight(MWAWEntry const &entry, std::vector<int> &firstParagLine,
                       std::vector<int> &linesHeight);

  //! read the information ( version <= 3)
  bool readInformationsV3(int numInfo,
                          std::vector<MacWrtParserInternal::Information> &informations);

  //! read the information
  bool readInformations(MWAWEntry const &entry,
                        std::vector<MacWrtParserInternal::Information> &informations);

  //! read a paragraph
  bool readParagraph(MacWrtParserInternal::Information const &info);

  //! read a graphics
  bool readGraphic(MacWrtParserInternal::Information const &info);
  /** test if a graphic is empty. In v5, some empty graphic are added
      before a page break, so it better to remove them */
  static bool isMagicPic(librevenge::RVNGBinaryData const &dt);

  //! read a text zone
  bool readText(MacWrtParserInternal::Information const &info, std::vector<int> const &lineHeight);

  //! read a page break zone ( version <= 3)
  bool readPageBreak(MacWrtParserInternal::Information const &info);

  //! check the free list
  bool checkFreeList();

  //! adds a new page
  void newPage(int number);

protected:
  //
  // data
  //
  //! the state
  shared_ptr<MacWrtParserInternal::State> m_state;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
