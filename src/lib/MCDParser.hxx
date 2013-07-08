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

#ifndef MCD_PARSER
#  define MCD_PARSER

#include <string>
#include <vector>

#include <libwpd/libwpd.h>

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

namespace MCDParserInternal
{
struct State;
}

/** \brief the main class to read a MacDoc file
 */
class MCDParser : public MWAWParser
{
  friend class MCDText;
public:
  //! constructor
  MCDParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~MCDParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(WPXDocumentInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! creates the listener which will be associated to the document
  void createDocument(WPXDocumentInterface *documentInterface);

  //! adds a new page
  void newPage(int number);

protected:
  //! finds the different objects zones
  bool createZones();

  //! try to send the contents
  bool sendContents();

  // Intermediate level

  //! try to send a picture
  bool sendPicture(MWAWEntry const &entry);
  //! try to read a font index entry
  bool readFont(MWAWEntry const &entry);
  //! try to read an index entry
  bool readIndex(MWAWEntry const &entry);
  //! try to set the index level and return the next index value (or -1)
  int updateIndex(int actIndex, int actLevel);
  //! try to read a bookmark entry
  bool readBookmark(MWAWEntry const &entry);
  //! try to read a file entry
  bool readFile(MWAWEntry const &entry);
  //! try to read a MDwp entry (window pos?)
  bool readWP(MWAWEntry const &entry);

  //! try to send the index
  bool sendIndex();

  //! return the input input
  MWAWInputStreamPtr rsrcInput();

  //! a DebugFile used to write what we recognize when we parse the document in rsrc
  libmwaw::DebugFile &rsrcAscii();

  //
  // data
  //
  //! the state
  shared_ptr<MCDParserInternal::State> m_state;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
