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

#ifndef MOR_PARSER
#  define MOR_PARSER

#include <string>
#include <vector>

#include <libwpd/libwpd.h>

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

namespace MORParserInternal
{
class SubDocument;
struct State;
}

class MORText;

/** \brief a namespace used to define basic structures in a More file
 */
namespace MORStruct
{
struct Pattern {
  //!constructor
  Pattern() : m_frontColor(MWAWColor::black()), m_backColor(MWAWColor::white()) {
    for (int i=0; i<8; i++) m_pattern[i]=0;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Pattern const &pat);
  //! the pattern
  unsigned char m_pattern[8];
  //! the front color
  MWAWColor m_frontColor;
  //! the back color
  MWAWColor m_backColor;
};
}

/** \brief the main class to read a More file
 */
class MORParser : public MWAWParser
{
  friend class MORParserInternal::SubDocument;
  friend class MORText;
public:
  //! constructor
  MORParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~MORParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(WPXDocumentInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! creates the listener which will be associated to the document
  void createDocument(WPXDocumentInterface *documentInterface);

  //! returns the page left top point ( in inches)
  Vec2f getPageLeftTop() const;
  //! adds a new page
  void newPage(int number);
  //! return the color which corresponds to an id (if possible)
  bool getColor(int id, MWAWColor &col) const;

  // interface with the text parser

protected:
  //! finds the different objects zones
  bool createZones();

  //! read the list of zones ( v2-3) : first 0x80 bytes
  bool readZonesList();

  //! read a PrintInfo zone ( first block )
  bool readPrintInfo(MWAWEntry const &entry);

  //! read a docinfo zone ( second block )
  bool readDocumentInfo(MWAWEntry const &entry);

  //! read the list of slide definitions
  bool readSlideList(MWAWEntry const &entry);

  //! read a slide definitions
  bool readSlide(MWAWEntry const &entry);

  //! read a graphic ( in a slide )
  bool readGraphic(MWAWEntry const &entry);

  //! read a unknown zone ( block 9 )
  bool readUnknown9(MWAWEntry const &entry);

  //! read a color zone ( beginning of block 9 )
  bool readColors(long endPos);

  //! read a pattern ( some sub zone of block 9)
  bool readPattern(long endPos, MORStruct::Pattern &pattern);

  //! read a backside ( some sub zone of block 9)
  bool readBackside(long endPos, std::string &extra);

  //! read the list of free file position
  bool readFreePos(MWAWEntry const &entry);

  //! read the last subzone find in a block 9 ( unknown meaning)
  bool readUnkn9Sub(long endPos);

  //
  // low level
  //

  /** check if an entry is in file */
  bool isFilePos(long pos);

  //! check if the entry is valid, if so store it in the list of entry
  bool checkAndStore(MWAWEntry const &entry);

  //! check if the entry is valid defined by the begin pos points to a zone: dataSz data
  bool checkAndFindSize(MWAWEntry &entry);

  //! return the input input
  MWAWInputStreamPtr rsrcInput();

  //! a DebugFile used to write what we recognize when we parse the document in rsrc
  libmwaw::DebugFile &rsrcAscii();

  //
  // data
  //

  //! the state
  shared_ptr<MORParserInternal::State> m_state;

  //! the text parser
  shared_ptr<MORText> m_textParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
