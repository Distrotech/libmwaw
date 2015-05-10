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

#ifndef MACDRAFT_PARSER
#  define MACDRAFT_PARSER

#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

namespace MacDraftParserInternal
{
struct Shape;
struct State;

class SubDocument;
}

/** \brief the main class to read a MacDraft v1 file
 *
 */
class MacDraftParser : public MWAWGraphicParser
{
  friend class MacDraftParserInternal::SubDocument;
public:
  //! constructor
  MacDraftParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~MacDraftParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(librevenge::RVNGDrawingInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! creates the listener which will be associated to the document
  void createDocument(librevenge::RVNGDrawingInterface *documentInterface);

protected:
  //! finds the different objects zones
  bool createZones();

  // Intermediate level

  //! try to read an object
  bool readObject();
  //! try to read a label
  bool readLabel();
  //! try to read a pattern
  bool readPattern();
  //! try to read bitmap definition
  bool readBitmapDefinition(MacDraftParserInternal::Shape &bitmap);
  //! try to read bitmap data
  bool readBitmapData();
  //! try to a unknown zone
  bool readZone();
  //! try to read the print info zone
  bool readPrintInfo();
  //! try to the doc header zone ( mainly unknown )
  bool readDocHeader();

  //
  // low level
  //

  //! try to send a shape
  bool send(MacDraftParserInternal::Shape const &shape);
  //! try to send a bitmap
  bool sendBitmap(MacDraftParserInternal::Shape const &bitmap, MWAWPosition const &position);
  //! try to send a text zone to the listener
  bool sendText(int zoneId);

  //
  // data
  //
  //! the state
  shared_ptr<MacDraftParserInternal::State> m_state;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
