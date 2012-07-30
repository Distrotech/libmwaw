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

#ifndef MW_PRO_STRUCTURES
#  define MW_PRO_STRUCTURES

#include <list>
#include <string>
#include <vector>

#include "MWAWPosition.hxx"

#include "MWAWEntry.hxx"
#include "MWAWContentListener.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

class WPXBinaryData;
typedef class MWAWContentListener MWProContentListener;
typedef shared_ptr<MWProContentListener> MWProContentListenerPtr;

class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;
class MWProParser;

namespace MWProParserInternal
{
class SubDocument;
}

namespace MWProStructuresInternal
{
struct Block;
struct Cell;
struct Font;
struct Paragraph;
struct Section;
struct State;
class SubDocument;
}

class MWProStructures;

/** \brief an interface to transmit the info of MWProStructures to a listener
 */
class MWProStructuresListenerState
{
public:
  //! the constructor
  MWProStructuresListenerState(shared_ptr<MWProStructures> structures, bool mainZone);
  //! the destructor
  ~MWProStructuresListenerState();

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
  int numSection() const {
    if (!m_isMainZone) {
      MWAW_DEBUG_MSG(("MWProStructuresListenerState::numSection: not called in main zone\n"));
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

  void sendFont(MWProStructuresInternal::Font const &font);
  void sendParagraph(MWProStructuresInternal::Paragraph const &para);

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
  shared_ptr<MWProStructures> m_structures;
  // the current font
  shared_ptr<MWProStructuresInternal::Font> m_font;
  // the current paragraph
  shared_ptr<MWProStructuresInternal::Paragraph> m_paragraph;
};

/** \brief the main class to read the structures part of MacWrite Pro file
 *
 *
 *
 */
class MWProStructures
{
  friend class MWProParser;
  friend class MWProParserInternal::SubDocument;
  friend struct MWProStructuresInternal::Cell;
  friend class MWProStructuresListenerState;
public:
  //! constructor
  MWProStructures(MWProParser &mainParser);
  //! destructor
  virtual ~MWProStructures();

  /** returns the file version.
   *
   * this version is only correct after the header is parsed */
  int version() const;

  //! Debugging: change the default ascii file (by default struct )
  void setAsciiName(char const *name) {
    m_asciiName = name;
  }

protected:
  //! inits all internal variables
  void init();

  //! sets the listener in this class and in the helper classes
  void setListener(MWProContentListenerPtr listen);

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
  int getHeaderId(int page);
  //! return the footer blockid ( or 0)
  int getFooterId(int page);

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
  bool readParagraph(MWProStructuresInternal::Paragraph &para);

  //! returns the size of the block end data
  int getEndBlockSize();

  //! try to read a block entry
  shared_ptr<MWProStructuresInternal::Block> readBlock();

  //! try to read a block entry
  shared_ptr<MWProStructuresInternal::Block> readBlockV2(int id);

  //! try to read the list of block entries
  bool readBlocksList();

  //! try to read the fonts zone
  bool readFontsName();

  //! try to read the list of fonts
  bool readFontsDef();

  //! try to read a font
  bool readFont(MWProStructuresInternal::Font &font);

  //! try to read the section info ?
  bool readSections(std::vector<MWProStructuresInternal::Section> &sections);

  //! try to read a 16 bytes the zone which follow the char styles zone ( the selection?)
  bool readSelection();

  //! try to read a zone which follow the fonts zone(checkme)
  bool readStructB();

  //! try to read a string
  bool readString(MWAWInputStreamPtr input, std::string &res);

  //! try to return the color corresponding to colId
  bool getColor(int colId, Vec3uc &color) const;

  //! returns true if the block is already sent ( or does not exists)
  bool isSent(int blockId);

  /** try to send a block which corresponds to blockid

      note: blockId=-noteId to send footnote in MW2
   */
  bool send(int blockId, bool mainZone=false);

  //! returns the debug file
  libmwaw::DebugFile &ascii() {
    return m_asciiFile;
  }

  //! return the ascii file name
  std::string const &asciiName() const {
    return m_asciiName;
  }

protected:
  //
  // data
  //

  //! the main input
  MWAWInputStreamPtr m_input;

  //! the main parser
  MWProParser &m_mainParser;

  //! the listener
  MWProContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<MWProStructuresInternal::State> m_state;

  //! the debug file
  libmwaw::DebugFile m_asciiFile;

  //! the debug file name
  std::string m_asciiName;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
