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
 * Parser to convert FullWrite document
 *
 */
#ifndef FP_MWAW_PARSER
#  define FP_MWAW_PARSER

#include <list>
#include <string>
#include <vector>

#include <libwpd/libwpd.h>

#include "MWAWDebug.hxx"
#include "MWAWEntry.hxx"
#include "MWAWInputStream.hxx"
#include "MWAWPageSpan.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWParser.hxx"

/** the definition of a zone in the file */
struct FWEntry : public MWAWEntry {
  FWEntry(MWAWInputStreamPtr input);
  ~FWEntry();

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, FWEntry const &entry);

  //! create a inputstream, ... if needed
  void update();
  //! write the debug file, ...
  void closeDebugFile();

  //! returns a reference to the ascii file
  libmwaw::DebugFile &getAsciiFile();
  //! basic operator==
  bool operator==(const FWEntry &a) const;
  //! basic operator!=
  bool operator!=(const FWEntry &a) const {
    return !operator==(a);
  }

  //! the input
  MWAWInputStreamPtr m_input;
  //! the next entry id
  int m_nextId;
  //! the zone type id find in DStruct
  int m_type;
  //! the type id (find in FZoneFlags)
  int m_typeId;
  //! some unknown values
  int m_values[3];
  //! the main data ( if the entry comes from several zone )
  WPXBinaryData m_data;
  //! the debug file
  shared_ptr<libmwaw::DebugFile> m_asciiFile;
private:
  FWEntry(FWEntry const &);
  FWEntry &operator=(FWEntry const &);
};

namespace FWParserInternal
{
struct DocZoneData;
struct State;
class SubDocument;
}

class FWText;

/** \brief the main class to read a FullWrite file
 *
 *
 *
 */
class FWParser : public MWAWParser
{
  friend class FWText;
  friend class FWParserInternal::SubDocument;

public:
  //! constructor
  FWParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~FWParser();

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

  //! create the file zone ( first step of create zones)
  bool createFileZones();

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //! returns the page height, ie. paper size less margin (in inches)
  float pageHeight() const;
  //! returns the page width, ie. paper size less margin (in inches)
  float pageWidth() const;

  //! adds a new page
  void newPage(int number);

  //! find the last position of the document and read data
  bool readDocPosition();

  //! try to read the file zones main flags
  bool readFileZoneFlags(shared_ptr<FWEntry> zone);

  //! try to read the file zones position
  bool readFileZonePos(shared_ptr<FWEntry> zone);

  //! try to read the zone containing the data of each doc zone (ie. Zone0)
  bool readDocZoneData(shared_ptr<FWEntry> zone);

  //! try to read the zone which stores the structure of zone0, ...  (ie. Zone1)
  bool readDocZoneStruct(shared_ptr<FWEntry> zone);
  //! returns the number of zone struct
  int getNumDocZoneStruct() const;
  //! check if a zone is a graphic zone, ...
  bool readGraphic(shared_ptr<FWEntry> zone);

  //! send a graphic to a listener (if it exists)
  bool sendGraphic(shared_ptr<FWEntry> zone);

  //! try to read zone2, a zone which stores the document information zone, ...
  bool readDocInfo(shared_ptr<FWEntry> zone);

  //! try to read the end of zone2 (only v2) ?
  bool readEndDocInfo(shared_ptr<FWEntry> zone);

  //! try to read the list of citation (at the end of doc info)
  bool readCitationDocInfo(shared_ptr<FWEntry> zone);

  //! try read the print info zone
  bool readPrintInfo(shared_ptr<FWEntry> zone);

  //
  // interface to the text parser
  //

  //! try to send a footnote/endnote entry
  void sendText(int docId, libmwaw::SubDocumentType type, int which=0);
  //! try to send a graphic
  void sendGraphic(int docId);
  //! try to send a variable, in pratice do nothing
  void sendVariable(int docId);
  //! try to send a reference, in pratice do nothing
  void sendReference(int docId);
  //! ask the text parser to send a zone
  bool send(int fileId);

  //
  // low level
  //

  //! try to read the data of zone 13 or 14 (unknown zone)
  bool readDoc1314Data(shared_ptr<FWEntry> zone, FWParserInternal::DocZoneData &doc);
  //! try to read the graphic data
  bool readGraphicData(shared_ptr<FWEntry> zone, FWParserInternal::DocZoneData &doc);
  //! try to read the reference data
  bool readReferenceData(shared_ptr<FWEntry> zone);
  //! try to read the data header of a classical zone
  bool readDocDataHeader(shared_ptr<FWEntry> zone, FWParserInternal::DocZoneData &doc);
  //! try to read the data of a zone which begins with a generic header
  bool readGenericDocData(shared_ptr<FWEntry> zone, FWParserInternal::DocZoneData &doc);

protected:
  //
  // data
  //
  //! the state
  shared_ptr<FWParserInternal::State> m_state;

  //! the actual document size
  MWAWPageSpan m_pageSpan;

  //! the text parser
  shared_ptr<FWText> m_textParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
