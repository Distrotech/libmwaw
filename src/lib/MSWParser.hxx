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
 * parser for Microsoft Word ( version 3.0-5.1 )
 */
#ifndef MSW_MWAW_PARSER
#  define MSW_MWAW_PARSER

#include <list>
#include <map>
#include <string>
#include <vector>

#include "MWAWDebug.hxx"
#include "MWAWEntry.hxx"
#include "MWAWInputStream.hxx"
#include "MWAWPosition.hxx"

#include "MWAWParser.hxx"

namespace MSWParserInternal
{
struct Object;
struct State;
class SubDocument;
}

class MSWText;
class MSWTextStyles;

//! the entry of MSWParser
struct MSWEntry : public MWAWEntry {
  MSWEntry() : MWAWEntry(), m_pictType(-1) {
  }
  /** \brief returns the text id
   *
   * This field is used to differentiate main text, header, ...)
   */
  int pictType() const {
    return m_pictType;
  }
  //! sets the picture id
  void setPictType(int newId) {
    m_pictType = newId;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, MSWEntry const &entry);
  //! the picture identificator
  int m_pictType;
};

/** \brief the main class to read a Microsoft Word file
 *
 *
 *
 */
class MSWParser : public MWAWParser
{
  friend class MSWText;
  friend class MSWTextStyles;
  friend class MSWParserInternal::SubDocument;

public:
  //! constructor
  MSWParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~MSWParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  //! the main parse function
  void parse(WPXDocumentInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! creates the listener which will be associated to the document
  void createDocument(WPXDocumentInterface *documentInterface);

  //! finds the different zones
  bool createZones();

  //! finish reading the header (v3)
  bool readHeaderEndV3();

  //! read the list of zones
  bool readZoneList();

  //! read the print info zone
  bool readPrintInfo(MSWEntry &entry);

  //! read the printer name
  bool readPrinter(MSWEntry &entry);

  //! read the document sumary
  bool readDocSum(MSWEntry &entry);

  //! read a zone which consists in a list of string
  bool readStringsZone(MSWEntry &entry, std::vector<std::string> &list);

  //! read the objects
  bool readObjects();

  //! read the object list
  bool readObjectList(MSWEntry &entry);

  //! read the object flags
  bool readObjectFlags(MSWEntry &entry);

  //! read an object
  bool readObject(MSWParserInternal::Object &obj);

  //! read the page dimensions + ?
  bool readDocumentInfo(MSWEntry &entry);

  //! read the zone 17( some bdbox + text position ?)
  bool readZone17(MSWEntry &entry);

  //! check if a position corresponds or not to a picture entry
  bool checkPicturePos(long pos, int type);

  //! read a picture data
  bool readPicture(MSWEntry &entry);
  //! send a picture
  void sendPicture(long fPos, int cPos, MWAWPosition::AnchorTo anchor=MWAWPosition::Char);

  //! returns the color corresponding to an id
  bool getColor(int id, MWAWColor &col) const;

  //! adds a new page
  void newPage(int number);

  /*
   * interface with subdocument
   */
  //! try to send a footnote id
  void sendFootnote(int id);

  //! try to send a bookmark field id
  void sendFieldComment(int id);

  //! try to send a footnote, a field to the textParser
  void send(int id, libmwaw::SubDocumentType type);
  //! try to send a text to the textParser
  void send(MWAWEntry const &entry);

  //
  // low level
  //

  /** check if an entry is in file */
  bool isFilePos(long pos);

  //! read a file entry
  MSWEntry readEntry(std::string type, int id=-1);

protected:
  //
  // data
  //
  //! the state
  shared_ptr<MSWParserInternal::State> m_state;

  //! the list of entries
  std::multimap<std::string, MSWEntry> m_entryMap;

  //! the text parser
  shared_ptr<MSWText> m_textParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
