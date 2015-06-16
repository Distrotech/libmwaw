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

#ifndef MACDRAFT5_PARSER
#  define MACDRAFT5_PARSER

#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

namespace MacDraft5ParserInternal
{
struct Layout;
struct Shape;
struct State;

class SubDocument;
}

class MacDraft5StyleManager;
/** \brief the main class to read a MacDraft5 v4-v5 file
 *
 */
class MacDraft5Parser : public MWAWGraphicParser
{
  friend class MacDraft5StyleManager;
  friend class MacDraft5ParserInternal::SubDocument;
public:
  //! constructor
  MacDraft5Parser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~MacDraft5Parser();

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

  //! try to read the print info zone
  bool readPrintInfo();
  //! try to read the doc header zone ( mainly unknown )
  bool readDocHeader();
  //! try to read last doc zone
  bool readDocFooter();
  //! try to read the library header zone ( mainly unknown )
  bool readLibraryHeader();
  //! try to read last library zone
  bool readLibraryFooter();

  //
  // low level
  //

  //! try to read an object
  bool readObject(MacDraft5ParserInternal::Layout &layout);
  //! try to read a label
  bool readLabel();
  //! try to read a list of strings
  bool readStringList();

  //! try to read the layout zone
  bool readLayout(MacDraft5ParserInternal::Layout &layout);

  //
  // read resource
  //

  //! try to read a LAYI:0 resource
  bool readLayoutDefinitions(MWAWEntry const &entry, bool inRsrc);
  //! try to read Link:128 resource
  bool readLinks(MWAWEntry const &entry, bool inRsrc);
  //! try to read a PICT entry (in data fork)
  bool readPICT(MWAWEntry const &entry, librevenge::RVNGBinaryData &pict);
  //! try to read the PICT unknown entry: pnot:0
  bool readPICTList(MWAWEntry const &entry, bool inRsrc);
  //! try to read a list of views : VIEW:1
  bool readViews(MWAWEntry const &entry, bool inRsrc);

  //
  // data
  //

  //! the style manager
  shared_ptr<MacDraft5StyleManager> m_styleManager;
  //! the state
  shared_ptr<MacDraft5ParserInternal::State> m_state;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
