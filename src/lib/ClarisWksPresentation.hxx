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
 * Parser to Claris Works text document ( presentation part )
 *
 */
#ifndef CLARIS_WKS_PRESENTATION
#  define CLARIS_WKS_PRESENTATION

#include <vector>

#include "MWAWEntry.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

#include "ClarisWksStruct.hxx"

namespace ClarisWksPresentationInternal
{
struct Presentation;
struct State;

class SubDocument;
}

class ClarisWksPRParser;

class ClarisWksDocument;

/** \brief the main class to read the text part of Claris Works file
 *
 *
 *
 */
class ClarisWksPresentation
{
  friend class ClarisWksDocument;
  friend class ClarisWksPRParser;
  friend class ClarisWksPresentationInternal::SubDocument;

public:
  //! constructor
  ClarisWksPresentation(ClarisWksDocument &document);
  //! destructor
  virtual ~ClarisWksPresentation();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;
  /** updates the page span list and returns true if this is possible */
  bool updatePageSpanList(MWAWPageSpan const &page, std::vector<MWAWPageSpan> &spanList);

  //! reads the zone presentation DSET
  shared_ptr<ClarisWksStruct::DSET> readPresentationZone
  (ClarisWksStruct::DSET const &zone, MWAWEntry const &entry, bool &complete);

  //! update the slide zone types
  void updateSlideTypes() const;

  //! disconnect the master zone to the content zones
  void disconnectMasterFromContents() const;

protected:
  //! sends the master zone (ie. the background zone)
  bool sendMaster();
  //! sends the zone data to the listener (if it exists )
  bool sendZone(int number);

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  // interface with main parser

  //! ask the main parser to send a zone
  void askToSend(int number);

  //
  // Intermediate level
  //

  /** try to read the first presentation zone ( the slide name ? ) */
  bool readZone1(ClarisWksPresentationInternal::Presentation &pres);

  /** try to read the second presentation zone ( title ) */
  bool readZone2(ClarisWksPresentationInternal::Presentation &pres);


  //
  // low level
  //

private:
  ClarisWksPresentation(ClarisWksPresentation const &orig);
  ClarisWksPresentation &operator=(ClarisWksPresentation const &orig);

protected:
  //
  // data
  //

  //! the document
  ClarisWksDocument &m_document;

  //! the parser state
  MWAWParserStatePtr m_parserState;

  //! the state
  shared_ptr<ClarisWksPresentationInternal::State> m_state;

  //! the main parser;
  MWAWParser *m_mainParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
