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

#ifndef MAC_WRT_PRO_STRUCTURES
#  define MAC_WRT_PRO_STRUCTURES

#include <list>
#include <string>
#include <vector>

#include "MWAWPosition.hxx"

#include "MWAWEntry.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

class MacWrtProParser;

namespace MacWrtProParserInternal
{
class SubDocument;
}

namespace MacWrtProStructuresInternal
{
struct Block;
struct Cell;
struct Font;
struct Paragraph;
struct Section;
struct State;
class SubDocument;
}

class MacWrtProStructures;

/** \brief an interface to transmit the info of MacWrtProStructures to a listener
 */
class MacWrtProStructuresListenerState
{
public:
  //! the constructor
  MacWrtProStructuresListenerState(shared_ptr<MacWrtProStructures> structures, bool mainZone);
  //! the destructor
  ~MacWrtProStructuresListenerState();

  //! returns true if the block is already sent ( or does not exists)
  bool isSent(int blockId);
  //! try to send a block which corresponds to blockid
  bool send(int blockId);

  //! try to send the i^th section
  void sendSection(int numSection);
  //! try to send a character style
  bool sendFont(int id);
  //! try to send a paragraph
  bool sendParagraph(int id);
  //! send a character
  void sendChar(char c);

  //! force resent data : font + paragraph
  bool resendAll();

  //! returns the actual section
  int numSection() const
  {
    if (!m_isMainZone) {
      MWAW_DEBUG_MSG(("MacWrtProStructuresListenerState::numSection: not called in main zone\n"));
      return 0;
    }
    return m_section;
  }

  //! return a list of page break position ( as some page break are soft )
  std::vector<int> getPageBreaksPos() const;
  //! insert a page break ( if we are not on a new page )
  void insertSoftPageBreak();

  //! debug function which returns a string corresponding to a fontId
  std::string getFontDebugString(int fontId);

  //! debug function which returns a string corresponding to a paragrapId
  std::string getParagraphDebugString(int paraId);

protected:
  //! create a new page
  bool newPage(bool softBreak=false);

  //! sends the font properties to the listener
  void sendFont(MacWrtProStructuresInternal::Font const &font);
  //! sends the paragraph properties to the listener
  void sendParagraph(MacWrtProStructuresInternal::Paragraph const &para);

  // true if this is the mainZone
  bool m_isMainZone;
  // the actual page
  int m_actPage;
  // the actual tab
  int m_actTab;
  // the number of tab
  int m_numTab;
  // the actual section ( if mainZone )
  int m_section;
  // the actual number of columns
  int m_numCols;
  // a flag to know if a new page has just been open
  bool m_newPageDone;
  // the main structure parser
  shared_ptr<MacWrtProStructures> m_structures;
  // the current font
  shared_ptr<MacWrtProStructuresInternal::Font> m_font;
  // the current paragraph
  shared_ptr<MacWrtProStructuresInternal::Paragraph> m_paragraph;
};

/** \brief the main class to read the structures part of MacWrite Pro file
 *
 *
 *
 */
class MacWrtProStructures
{
  friend class MacWrtProParser;
  friend class MacWrtProParserInternal::SubDocument;
  friend struct MacWrtProStructuresInternal::Cell;
  friend class MacWrtProStructuresListenerState;
public:
  //! constructor
  MacWrtProStructures(MacWrtProParser &mainParser);
  //! destructor
  virtual ~MacWrtProStructures();

  /** returns the file version.
   *
   * this version is only correct after the header is parsed */
  int version() const;

  //! Debugging: change the default ascii file (by default struct )
  void setAsciiName(char const *name)
  {
    m_asciiName = name;
  }

protected:
  //! inits all internal variables
  void init();

  //! finds the different objects zones
  bool createZones();

  /** finds the different objects zones in a MacWriteII file

  \note: this function is called by createZones
   */
  bool createZonesV2();

  //! returns the number of pages
  int numPages() const;

  //! send the main zone
  bool sendMainZone();

  //! return the header blockid ( or 0)
  int getHeaderId(int page, int &numSimillar) const;
  //! return the footer blockid ( or 0)
  int getFooterId(int page, int &numSimillar) const;

  //! flush not send zones
  void flushExtra();

  //! look for pages structures
  void buildPageStructures();

  //! look for tables structures and if so, prepare data
  void buildTableStructures();

  //
  // low level
  //

  //! try to read the paragraph styles zone which begins at address 0x200
  bool readStyles();

  //! try to read a style
  bool readStyle(int styleId);

  //! try to read the character styles zone
  bool readCharStyles();

  //! try to read a list of paragraph
  bool readParagraphs();

  //! try to read a paragraph
  bool readParagraph(MacWrtProStructuresInternal::Paragraph &para);

  //! returns the size of the block end data
  int getEndBlockSize();

  //! try to read a block entry
  shared_ptr<MacWrtProStructuresInternal::Block> readBlock();

  //! try to read a block entry
  shared_ptr<MacWrtProStructuresInternal::Block> readBlockV2(int id);

  //! try to read the list of block entries
  bool readBlocksList();

  //! try to read the fonts zone
  bool readFontsName();

  //! try to read the list of fonts
  bool readFontsDef();

  //! try to read a font
  bool readFont(MacWrtProStructuresInternal::Font &font);

  //! try to read the section info ?
  bool readSections(std::vector<MacWrtProStructuresInternal::Section> &sections);

  //! try to read a 16 bytes the zone which follow the char styles zone ( the selection?)
  bool readSelection();

  //! try to read a zone which follow the fonts zone(checkme)
  bool readStructB();

  //! try to read a string
  static bool readString(MWAWInputStreamPtr input, std::string &res);

  //! try to return the color corresponding to colId
  bool getColor(int colId, MWAWColor &color) const;

  //! try to return the pattern corresponding to patId
  bool getPattern(int patId, float &patternPercent) const;

  //! try to return the color corresponding to colId and patId
  bool getColor(int colId, int patId, MWAWColor &color) const;

  //! returns true if the block is already sent ( or does not exists)
  bool isSent(int blockId);

  /** try to send a block which corresponds to blockid

      note: blockId=-noteId to send footnote in MW2
   */
  bool send(int blockId, bool mainZone=false);

  //! returns the actual listener
  MWAWTextListenerPtr &getTextListener();

  //! returns the debug file
  libmwaw::DebugFile &ascii()
  {
    return m_asciiFile;
  }

  //! return the ascii file name
  std::string const &asciiName() const
  {
    return m_asciiName;
  }

protected:
  //
  // data
  //

  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the main input
  MWAWInputStreamPtr m_input;

  //! the main parser
  MacWrtProParser &m_mainParser;

  //! the state
  shared_ptr<MacWrtProStructuresInternal::State> m_state;

  //! the debug file
  libmwaw::DebugFile m_asciiFile;

  //! the debug file name
  std::string m_asciiName;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
