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

#ifndef CLARISDRAW_PARSER
#  define CLARISDRAW_PARSER

#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

#include "ClarisWksStruct.hxx"

namespace ClarisDrawParserInternal
{
struct State;

class SubDocument;
}

class ClarisDrawGraph;
class ClarisDrawText;
class ClarisDrawStyleManager;

/** \brief the main class to read a ClarisDraw v1 file
 *
 */
class ClarisDrawParser : public MWAWGraphicParser
{
  friend class ClarisDrawGraph;
  friend class ClarisDrawText;
public:
  //! constructor
  ClarisDrawParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~ClarisDrawParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  //! the main parse function
  void parse(librevenge::RVNGDrawingInterface *documentInterface);

  //! returns the file type corresponding to a zone id
  int getFileType(int zoneId) const;
  //! sends a text zone
  bool sendTextZone(int number, int subZone=-1);

protected:
  //! inits all internal variables
  void init();

  //! creates the listener which will be associated to the document
  void createDocument(librevenge::RVNGDrawingInterface *documentInterface);

protected:
  //! finds the different objects zones
  bool createZones();
  //! try to read a zone
  bool readZone();

  // Intermediate level

  //! try to read a document header
  bool readDocHeader();
  //! try to read the library header
  bool readLibraryHeader();
  //! try to read a DSET structure
  shared_ptr<ClarisWksStruct::DSET> readDSET();

  //! try to read the print info zone
  bool readPrintInfo();
  //! try to read the document info zone
  bool readDocInfo();
  //! try to read the layout
  bool readLayouts();

  //
  // low level
  //

protected:
  //
  // data
  //

  //! the state
  shared_ptr<ClarisDrawParserInternal::State> m_state;
  //! the style manager
  shared_ptr<ClarisDrawStyleManager> m_styleManager;
  //! the graph parser
  shared_ptr<ClarisDrawGraph> m_graphParser;
  //! the text parser
  shared_ptr<ClarisDrawText> m_textParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
