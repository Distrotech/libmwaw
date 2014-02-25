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

#ifndef BEAGLE_WKS_PARSER
#  define BEAGLE_WKS_PARSER

#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "MWAWDebug.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

#include "BeagleWksStructManager.hxx"

namespace BeagleWksParserInternal
{
struct State;
}

class BeagleWksText;

/** \brief the main class to read a BeagleWorks file
 */
class BeagleWksParser : public MWAWTextParser
{
  friend class BeagleWksText;
public:
  //! constructor
  BeagleWksParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~BeagleWksParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(librevenge::RVNGTextInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! creates the listener which will be associated to the document
  void createDocument(librevenge::RVNGTextInterface *documentInterface);

  //! returns the page left top point ( in inches)
  Vec2f getPageLeftTop() const;
  //! adds a new page
  void newPage(int number);

  // interface with the text parser

  //! try to insert the pId picture (as char)
  bool sendFrame(int pId);

protected:
  //! finds the different objects zones
  bool createZones();

  //! read the resource fork zone
  bool readRSRCZones();

  //! try to send the page graphic
  bool sendPageFrames();

  //! try to send a frame
  bool sendFrame(BeagleWksStructManager::Frame const &frame);

  //! try to send a picture
  bool sendPicture(int pId, MWAWPosition const &pos,
                   MWAWGraphicStyle const &style=MWAWGraphicStyle::emptyStyle());
  //
  // low level
  //

  // data fork

  //! read the print info zone
  bool readPrintInfo();
  //! read the last zone
  bool readLastZone();

  //! return the input input
  MWAWInputStreamPtr rsrcInput();

  //! a DebugFile used to write what we recognize when we parse the document in rsrc
  libmwaw::DebugFile &rsrcAscii();

  //
  // data
  //

  //! the state
  shared_ptr<BeagleWksParserInternal::State> m_state;

  //! the structure manager
  shared_ptr<BeagleWksStructManager> m_structureManager;
  //! the text parser
  shared_ptr<BeagleWksText> m_textParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
