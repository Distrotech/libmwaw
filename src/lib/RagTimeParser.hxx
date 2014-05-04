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

#ifndef RAG_TIME_PARSER
#  define RAG_TIME_PARSER

#include <vector>

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

namespace RagTimeParserInternal
{
struct State;
class SubDocument;
}

/** \brief the main class to read a RagTime v3 file
 *
 *
 *
 */
class RagTimeParser : public MWAWTextParser
{
  friend class RagTimeParserInternal::SubDocument;

public:
  //! constructor
  RagTimeParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~RagTimeParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(librevenge::RVNGTextInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! creates the listener which will be associated to the document
  void createDocument(librevenge::RVNGTextInterface *documentInterface);

  //! adds a new page
  void newPage(int number);

  //! finds the different objects zones
  bool createZones();

  //! try to create the main data zones list
  bool findDataZones();

  //! try to create the resource zones list
  bool findRsrcZones();

  //! try to read pictZone ( a big zone)
  bool readPictZone(MWAWEntry &entry);

  //! try to read pictZone ( a big zone):v2
  bool readPictZoneV2(MWAWEntry &entry);

  //! try to read a text zone
  bool readTextZone(MWAWEntry &entry);

  // some rsrc zone

  //! try to read the color map:v2
  bool readColorMapV2(MWAWEntry &entry);

  // unknown data fork zone

  //! try to read zone0 ( unknown content of size 40)
  bool readZone0(MWAWEntry &entry);

  //! try to read zone3 ( a big zone):v2
  bool readZone3V2(MWAWEntry &entry);

  //! try to read zone3 ( a big zone)
  bool readZone3(MWAWEntry &entry);

  //! try to read zone6 ( a big zone)
  bool readZone6(MWAWEntry &entry);

  // unknown rsrc fork zone
  //! try to read the BeDc zone ( zone of size 52, one by file with id=0);
  bool readRsrcBeDc(MWAWEntry &entry);
  //! try to read the FHwl zone ( big zone, one by file with id=0);
  bool readRsrcFHwl(MWAWEntry &entry);
protected:
  //
  // data
  //
  //! the state
  shared_ptr<RagTimeParserInternal::State> m_state;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
