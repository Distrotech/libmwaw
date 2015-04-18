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
 * parser for WriteNow 3.0 and 4.0
 *
 * Note: WriteNow 2.0 seems very different
 */
#ifndef WRITE_NOW_PARSER
#  define WRITE_NOW_PARSER

#include <list>
#include <string>
#include <vector>

#include "MWAWDebug.hxx"
#include "MWAWEntry.hxx"
#include "MWAWInputStream.hxx"
#include "MWAWPosition.hxx"

#include "MWAWParser.hxx"

namespace WriteNowParserInternal
{
struct State;
class SubDocument;
}

struct WriteNowEntry;
struct WriteNowEntryManager;

class WriteNowText;

/** \brief the main class to read a WriteNow file
 *
 *
 *
 */
class WriteNowParser : public MWAWTextParser
{
  friend class WriteNowText;
  friend class WriteNowParserInternal::SubDocument;

public:
  //! constructor
  WriteNowParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~WriteNowParser();

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

  //! returns the columns information
  void getColumnInfo(int &numColumns, int &width) const;

  //! adds a new page
  void newPage(int number);

  /*
   * interface with WriteNowText
   */
  //! returns the color which corresponds to colId
  bool getColor(int colId, MWAWColor &col) const;

  //! try to send a footnote entry
  void sendFootnote(WriteNowEntry const &entry);

  //! try to send the graphic zone
  bool sendGraphic(int gId, MWAWBox2i const &bdbox);

  /*
   * interface with subdocument
   */

  //! try to send an entry
  void send(WriteNowEntry const &entry);

  //
  // low level
  //

  //! try to read the document entries zone v3-v4
  bool readDocEntries();

  //! try to read the document entries zone v2
  bool readDocEntriesV2();

  /** try to read the graphic zone (unknown + list of entries )
      and to create the graphic data zone
   */
  bool parseGraphicZone(WriteNowEntry const &entry);

  //! try to read the colormap zone
  bool readColorMap(WriteNowEntry const &entry);

  //! try to read the print info zone
  bool readPrintInfo(WriteNowEntry const &entry);

  //! try to read the last generic zones
  bool readGenericUnkn(WriteNowEntry const &entry);

  //! try to send a picture to the listener
  bool sendPicture(WriteNowEntry const &entry, MWAWBox2i const &bdbox);

  //! read a file entry
  WriteNowEntry readEntry();

  //! check if a position is inside the file
  bool checkIfPositionValid(long pos);

protected:
  //
  // data
  //
  //! the state
  shared_ptr<WriteNowParserInternal::State> m_state;

  //! the list of entry
  shared_ptr<WriteNowEntryManager> m_entryManager;

  //! the text parser
  shared_ptr<WriteNowText> m_textParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
