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

#ifndef MW_PRO_PARSER
#  define MW_PRO_PARSER

#include <list>
#include <string>
#include <vector>

#include "MWAWPageSpan.hxx"

#include "MWAWPosition.hxx"

#include "MWAWEntry.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

class WPXBinaryData;
typedef class MWAWContentListener MWProContentListener;
typedef shared_ptr<MWProContentListener> MWProContentListenerPtr;

class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

namespace MWProParserInternal
{
struct State;
struct TextZoneData;
struct TextZone;
struct Token;
struct Zone;
class SubDocument;
}

class MWProStructures;
class MWProStructuresListenerState;

/** \brief the main class to read a MacWrite II and MacWrite Pro file
 *
 *
 *
 */
class MWProParser : public MWAWParser
{
  friend class MWProStructures;
  friend class MWProStructuresListenerState;
  friend class MWProParserInternal::SubDocument;

public:
  //! constructor
  MWProParser(MWAWInputStreamPtr input, MWAWHeader *header);
  //! destructor
  virtual ~MWProParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  /** returns the file version.
   *
   * this version is only correct after the header is parsed */
  int version() const;

  // the main parse function
  void parse(WPXDocumentInterface *documentInterface);

  //! Debugging: change the default ascii file
  void setAsciiName(char const *name) {
    m_asciiName = name;
  }

protected:
  //! inits all internal variables
  void init();

  //! sets the listener in this class and in the helper classes
  void setListener(MWProContentListenerPtr listen);

  //! creates the listener which will be associated to the document
  void createDocument(WPXDocumentInterface *documentInterface);

  //! finds the different objects zones
  bool createZones();

  //! retrieve the data which corresponds to a zone
  bool getZoneData(WPXBinaryData &data, int blockId);

  //! return the chain list of block ( used to get free blocks)
  bool getFreeZoneList(int blockId, std::vector<int> &blockLists);

  /** parse a data zone

  \note type=0 ( text entry), type = 1 ( graphic entry ), other unknown
  */
  bool parseDataZone(int blockId, int type);

  /** parse a text zone */
  bool parseTextZone(shared_ptr<MWProParserInternal::Zone> zone);

  /** try to read the text block entries */
  bool readTextEntries(shared_ptr<MWProParserInternal::Zone> zone,
                       std::vector<MWAWEntry> &res, int textLength);
  /** try to read the text id entries */
  bool readTextIds(shared_ptr<MWProParserInternal::Zone> zone,
                   std::vector<MWProParserInternal::TextZoneData> &res,
                   int textLength, int type);
  /** try to read the text token entries */
  bool readTextTokens(shared_ptr<MWProParserInternal::Zone> zone,
                      std::vector<MWProParserInternal::Token> &res,
                      int textLength);

  /** return the list of blockid called by token. A hack to help
      structures to retrieve the page attachment */
  std::vector<int> const &getBlocksCalledByToken() const;

  //! returns the page height, ie. paper size less margin (in inches)
  float pageHeight() const;
  //! returns the page width, ie. paper size less margin (in inches)
  float pageWidth() const;
  //! returns the document number of columns ( filed in MWII)
  int numColumns() const;

  //! adds a new page
  void newPage(int number, bool softBreak=false);

  //
  // interface with MWProParserStructures
  //

  //! send a text box
  bool sendTextZone(int blockId, bool mainZone = false);

  //! compute the number of hard page break
  int findNumHardBreaks(int blockId);

  //! try to send a picture
  bool sendPictureZone(int blockId, MWAWPosition const &pictPos,
                       WPXPropertyList extras = WPXPropertyList());

  //! send a textbox zone
  bool sendTextBoxZone(int blockId, MWAWPosition const &pos,
                       WPXPropertyList extras = WPXPropertyList());

  //! try to send an empty zone (can exist in MWPro1.5)
  bool sendEmptyFrameZone(MWAWPosition const &pos, WPXPropertyList extras);

  //
  // low level
  //

  //! read the print info zone
  bool readPrintInfo();

  //! try to read the doc header zone
  bool readDocHeader();

#ifdef DEBUG
  //! a debug function which can be used to check the block retrieving
  void saveOriginal(MWAWInputStreamPtr input);
#endif

  //! try to send a picture
  bool sendPicture(shared_ptr<MWProParserInternal::Zone> zone, MWAWPosition pictPos, WPXPropertyList const &extras);

  //! try to send a text
  bool sendText(shared_ptr<MWProParserInternal::TextZone> zone, bool mainZone = false);

  //! compute the number of hard page break
  int findNumHardBreaks(shared_ptr<MWProParserInternal::TextZone> zone);

  //! a debug function which can be used to save the unparsed block
  void checkUnparsed();

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
  //! the listener
  MWProContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<MWProParserInternal::State> m_state;

  //! the structures parser
  shared_ptr<MWProStructures> m_structures;

  //! the actual document size
  MWAWPageSpan m_pageSpan;

  //! the debug file
  libmwaw::DebugFile m_asciiFile;

  //! the debug file name
  std::string m_asciiName;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
