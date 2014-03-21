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

#ifndef GREAT_WKS_DOCUMENT
#  define GREAT_WKS_DOCUMENT

#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "MWAWDebug.hxx"

namespace GreatWksDocumentInternal
{
struct State;
}

class GreatWksGraph;
class GreatWksParser;
class GreatWksDRParser;
class GreatWksSSParser;
class GreatWksText;

/** \brief the main class to read/store generic data of a GreatWorks document
 */
class GreatWksDocument
{
  friend class GreatWksParser;
  friend class GreatWksDRParser;
  friend class GreatWksSSParser;
public:
  //! constructor
  GreatWksDocument(MWAWParser &parser);
  //! destructor
  virtual ~GreatWksDocument();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  //! returns the main parser
  MWAWParser &getMainParser()
  {
    return *m_parser;
  }
  //! returns the graph parser
  shared_ptr<GreatWksGraph> getGraphParser()
  {
    return m_graphParser;
  }
  //! returns the text parser
  shared_ptr<GreatWksText> getTextParser()
  {
    return m_textParser;
  }

  //! return the main section
  MWAWSection getMainSection() const;
  /** send a page break */
  void newPage(int page);
  //! send a picture
  bool sendPicture(MWAWEntry const &entry, MWAWPosition pos);

  // interface with the graph parser

  //! check if a textbox can be send in a graphic zone, ie. does not contains any graphic
  bool canSendTextboxAsGraphic(MWAWEntry const &entry);
  //! try to send textbox
  bool sendTextbox(MWAWEntry const &entry, MWAWListenerPtr listener);
protected:

protected:

  //! read the resource fork zone
  bool readRSRCZones();

  //
  // low level
  //

  //! read a PrintInfo block ( PRNT resource block )
  bool readPrintInfo(MWAWEntry const &entry);

  //! read the windows positions ( WPSN resource block )
  bool readWPSN(MWAWEntry const &entry);

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

private:
  GreatWksDocument(GreatWksDocument const &orig);
  GreatWksDocument &operator=(GreatWksDocument const &orig);

  //
  // data
  //

protected:
  //! the state
  shared_ptr<GreatWksDocumentInternal::State> m_state;
public:
  //! the parser state
  shared_ptr<MWAWParserState> m_parserState;

protected:
  //! the main parser
  MWAWParser *m_parser;
  //! the graph document
  shared_ptr<GreatWksGraph> m_graphParser;
  //! the text document
  shared_ptr<GreatWksText> m_textParser;

  //! callback used to return the main section
  typedef MWAWSection(MWAWParser::* GetMainSection)() const;
  /** callback used to send a page break */
  typedef void (MWAWParser::* NewPage)(int page);

  /** the getMainSection callback */
  GetMainSection m_getMainSection;
  /** the new page callback */
  NewPage m_newPage;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
