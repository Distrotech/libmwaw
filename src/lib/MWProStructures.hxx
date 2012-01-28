/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libmwaw
 * Copyright (C) 2009, 2011 Alonso Laurent (alonso@loria.fr)
 * Copyright (C) 2006, 2007 Andrew Ziem
 * Copyright (C) 2004-2006 Fridrich Strba (fridrich.strba@bluewin.ch)
 * Copyright (C) 2004 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2003-2005 William Lachance (william.lachance@sympatico.ca)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 * For further information visit http://libwps.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

#ifndef MW_PRO_STRUCTURES
#  define MW_PRO_STRUCTURES

#include <list>
#include <string>
#include <vector>

#include "TMWAWPosition.hxx"

#include "IMWAWEntry.hxx"
#include "IMWAWContentListener.hxx"

#include "TMWAWDebug.hxx"
#include "TMWAWInputStream.hxx"

class WPXBinaryData;
typedef class MWAWContentListener MWProContentListener;
typedef shared_ptr<MWProContentListener> MWProContentListenerPtr;

namespace MWAWTools
{
class Convertissor;
typedef shared_ptr<Convertissor> ConvertissorPtr;
}

class MWProParser;

namespace MWProStructuresInternal
{
struct Block;
struct Font;
struct Paragraph;
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
  MWProStructuresListenerState(shared_ptr<MWProStructures> structures);
  //! the destructor
  ~MWProStructuresListenerState();
  //! try to send a character style
  bool sendFont(int id, bool force);
  //! try to send a paragraph
  bool sendParagraph(int id);
  //! send a character
  void sendChar(char c);

  //! debug function which returns a string corresponding to a fontId
  std::string getFontDebugString(int fontId);

  //! debug function which returns a string corresponding to a paragrapId
  std::string getParagraphDebugString(int paraId);

protected:
  void sendParagraph(MWProStructuresInternal::Paragraph const &para);

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

  //! flush not send zones
  void flushExtra();

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

  //! try to read a block entry
  bool readBlock(MWProStructuresInternal::Block &block);

  //! try to read the list of block entries
  bool readBlocksList();

  //! try to read the fonts zone
  bool readFontsName();

  //! try to read the list of fonts
  bool readFontsDef();

  //! try to read a font
  bool readFont(MWProStructuresInternal::Font &font);

  //! try to read a 16 bytes the zone which follow the char styles zone
  bool readStructA();

  //! try to read a zone which follow the fonts zone(checkme)
  bool readStructB();

  //! try to read a zone which follow the list of paragraphs
  bool readStructD();

  //! try to read a string
  bool readString(TMWAWInputStreamPtr input, std::string &res);

  //! try to return the color corresponding to colId
  bool getColor(int colId, Vec3uc &color) const;

  //! returns the debug file
  libmwaw_tools::DebugFile &ascii() {
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
  TMWAWInputStreamPtr m_input;

  //! the main parser
  MWProParser &m_mainParser;

  //! the listener
  MWProContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWTools::ConvertissorPtr m_convertissor;

  //! the state
  shared_ptr<MWProStructuresInternal::State> m_state;

  //! the debug file
  libmwaw_tools::DebugFile m_asciiFile;

  //! the debug file name
  std::string m_asciiName;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
