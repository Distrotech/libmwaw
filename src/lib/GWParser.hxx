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

#ifndef GW_PARSER
#  define GW_PARSER

#include <string>
#include <vector>

#include <libwpd/libwpd.h>

#include "MWAWDebug.hxx"

#include "MWAWParser.hxx"

namespace GWParserInternal
{
struct State;
class SubDocument;
}

class GWGraph;
class GWText;

/** \brief the main class to read a GreatWorks text file
 */
class GWParser : public MWAWParser
{
  friend class GWParserInternal::SubDocument;
  friend class GWGraph;
  friend class GWText;
public:
  //! an enum used to defined the document type
  enum DocType { DRAW, TEXT, UNKNOWN };
  //! constructor
  GWParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~GWParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(WPXDocumentInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! creates the listener which will be associated to the document
  void createDocument(WPXDocumentInterface *documentInterface);

  //! returns the page left top point ( in inches)
  Vec2f getPageLeftTop() const;
  //! adds a new page
  void newPage(int number);

  // interface with the text parser

  //! return the main section
  MWAWSection getMainSection() const;
  //! try to send the i^th header/footer
  bool sendHF(int id);
  //! try to  textbox's text
  bool sendTextbox(MWAWEntry const &entry);

  // interface with the graph parser

  //! try to send a picture
  bool sendPicture(MWAWEntry const &entry, MWAWPosition pos);

  // general interface
  DocType getDocumentType() const;

protected:
  //! finds the different objects zones
  bool createZones();
  //! finds the different objects zones ( for a draw file)
  bool createDrawZones();

  //! read the resource fork zone
  bool readRSRCZones();

  //
  // low level
  //

  //! read a PrintInfo block ( PRNT resource block )
  bool readPrintInfo(MWAWEntry const &entry);

  //! read the windows positions ( WPSN resource block )
  bool readWPSN(MWAWEntry const &entry);

  //! read the DocInfo block ( many unknown data )
  bool readDocInfo();
  //! read a unknown zone ( ARRs resource block: v2 )
  bool readARRs(MWAWEntry const &entry);
  //! read a unknown zone ( DaHS resource block: v2 )
  bool readDaHS(MWAWEntry const &entry);
  //! read a unknown zone ( GrDS resource block: v2 )
  bool readGrDS(MWAWEntry const &entry);
  //! read a unknown zone ( NxED resource block: v2 )
  bool readNxEd(MWAWEntry const &entry);

  //! return the input input
  MWAWInputStreamPtr rsrcInput();

  //! a DebugFile used to write what we recognize when we parse the document in rsrc
  libmwaw::DebugFile &rsrcAscii();

  //
  // data
  //

  //! the state
  shared_ptr<GWParserInternal::State> m_state;

  //! the graph parser
  shared_ptr<GWGraph> m_graphParser;
  //! the text parser
  shared_ptr<GWText> m_textParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
