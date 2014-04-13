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

#ifndef GREAT_WKS_DB_PARSER
#  define GREAT_WKS_DB_PARSER

#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "MWAWCell.hxx"
#include "MWAWDebug.hxx"

#include "MWAWParser.hxx"

namespace GreatWksDBParserInternal
{
struct Block;
struct BlockHeader;
struct Field;
class Cell;
struct State;
class SubDocument;
}

class GreatWksDocument;

/** \brief the main class to read a GreatWorks database file
 */
class GreatWksDBParser : public MWAWSpreadsheetParser
{
  friend class GreatWksDBParserInternal::SubDocument;
public:
  //! constructor
  GreatWksDBParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~GreatWksDBParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(librevenge::RVNGSpreadsheetInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! creates the listener which will be associated to the document
  void createDocument(librevenge::RVNGSpreadsheetInterface *documentInterface);

  //! try to send the main database
  bool sendDatabase();

  // interface with the text parser

  //! try to send the i^th header/footer
  bool sendHF(int id);

protected:
  //! finds the different objects zones
  bool createZones();

  //
  // low level
  //

  //! try to read the styles
  bool readStyles();
  //! read the database block
  bool readDatabase();

  //! try to read the database header: list of zones, ...
  bool readHeader();
  //! try to read the fields' zone
  bool readFields(MWAWEntry const &zone);
  //! try to read a field
  bool readField(GreatWksDBParserInternal::Field &field);
  //! try to read a zone which links a field to zone record
  bool readFieldLink(GreatWksDBParserInternal::Field &field);
  //! try to read a list of records corresponding to field
  bool readRecords(GreatWksDBParserInternal::Field &field);
  //! try to read a int's zone
  bool readIntList(MWAWEntry const &zone, std::vector<int> &list);
  //! try to read the second small zone (probably a list of zone 9 zones)
  bool readZone2(MWAWEntry const &zone);
  //! try to read the zone 12(unknown)
  bool readZone12(MWAWEntry const &zone);
  //! try to read an unknown small zone, ie. a default reader: type, 0, size, N, dSz
  bool readSmallZone(MWAWEntry const &zone);
  //! check if a pointer correspond or not to a small zone entry, if so update the entry
  bool checkSmallZone(MWAWEntry &zone);

  //! try to read the record list (the 3th big zone)
  bool readRecordList(GreatWksDBParserInternal::Block &block);
  //! try to read the 0th big zone
  bool readBlockHeader0(GreatWksDBParserInternal::Block &block);
  //! try to read the 1th, 2th big zone, maybe a list position <-> recordId
  bool readRecordPosition(GreatWksDBParserInternal::Block &block);
  //! try to read a big block entry
  bool readBlockHeader(GreatWksDBParserInternal::BlockHeader &entry);
  //! try to create a block corresponding to an entry
  shared_ptr<GreatWksDBParserInternal::Block> createBlock(GreatWksDBParserInternal::BlockHeader &entry);
  //! try to read a unknown block, knowing the field size
  bool readBlock(GreatWksDBParserInternal::Block &block, int fieldSize);

  //
  // data
  //

  //! the state
  shared_ptr<GreatWksDBParserInternal::State> m_state;

  //! the document
  shared_ptr<GreatWksDocument> m_document;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
