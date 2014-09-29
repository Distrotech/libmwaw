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

#ifndef RAG_TIME_5_PARSER
#  define RAG_TIME_5_PARSER

#include <set>
#include <vector>

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

namespace RagTime5ParserInternal
{
struct State;
struct Zone;
class SubDocument;
}

/** \brief the main class to read a RagTime v5 file
 *
 *
 *
 */
class RagTime5Parser : public MWAWTextParser
{
  friend class RagTime5ParserInternal::SubDocument;

public:
  //! constructor
  RagTime5Parser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~RagTime5Parser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(librevenge::RVNGTextInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //
  // interface
  //

  //! returns the ith color ( if possible)
  bool getColor(int colId, MWAWColor &color, int listId=-1) const;
  //! returns a new unique zone id
  int getNewZoneId();

  //! creates the listener which will be associated to the document
  void createDocument(librevenge::RVNGTextInterface *documentInterface);
  //! try to send the different zones
  bool sendZones();

  //! adds a new page
  void newPage(int number);

  //! finds the different objects zones
  bool createZones();
  //! try to create the main data zones list
  bool findDataZones(MWAWEntry const &entry);
  //! try to update a zone: create a new input if the zone is stored in different positions, ...
  bool update(RagTime5ParserInternal::Zone &zone);
  //! try to unpack a zone
  bool unpackZone(RagTime5ParserInternal::Zone &zone);

  //! try to read string zone ( zone with id=21)
  bool readStringZone(RagTime5ParserInternal::Zone &zone);
  //! try to read a list of unknown zone
  bool readListZone(RagTime5ParserInternal::Zone &zone);

  //! try to read a PICT zone
  bool readPICT(RagTime5ParserInternal::Zone &zone, MWAWEntry &entry);
  //! try to read a TIFF zone
  bool readTIFF(RagTime5ParserInternal::Zone &zone, MWAWEntry &entry);
  //! try to read a OLE zone
  bool readOLEZone(RagTime5ParserInternal::Zone &zone);

  //! flush unsent zone (debugging function)
  void flushExtra();

protected:
  //
  // data
  //
  //! the state
  shared_ptr<RagTime5ParserInternal::State> m_state;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
