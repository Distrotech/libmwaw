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

#ifndef MS_WKS3_PARSER
#  define MS_WKS3_PARSER

#include <vector>

#include "MWAWDebug.hxx"
#include "MWAWEntry.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

namespace MsWks3ParserInternal
{
struct State;
struct Zone;
class SubDocument;
}

class MsWksGraph;
class MsWksZone;
class MsWks3Text;

/** \brief the main class to read a Microsoft Works file
 *
 *
 *
 */
class MsWks3Parser : public MWAWTextParser
{
  friend class MsWks3ParserInternal::SubDocument;
  friend class MsWks3Text;
public:
  //! constructor
  MsWks3Parser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~MsWks3Parser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(librevenge::RVNGTextInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! creates the listener which will be associated to the document
  void createDocument(librevenge::RVNGTextInterface *documentInterface);

  //! finds the different objects zones
  bool createZones();

  //! returns the page height, ie. paper size less margin (in inches) less header/footer size
  double getTextHeight() const;

  //! adds a new page
  void newPage(int number, bool softBreak=false);

  //
  // intermediate level
  //

  //! try to read a generic zone
  bool readZone(MsWks3ParserInternal::Zone &zone);
  //! try to read the documentinfo ( zone2)
  bool readDocumentInfo();
  //! try to read a group zone (zone3)
  bool readGroup(MsWks3ParserInternal::Zone &zone, MWAWEntry &entry, int check);
  //! try to read a zone information (zone0)
  bool readGroupHeaderInfo(bool header, int check);
  /** try to send a note */
  bool sendFootNote(int zoneId, int noteId);

  /** try to send a text entry */
  void sendText(int id, int noteId=-1);

  /** try to send a zone */
  void sendZone(int zoneType);

  //
  // low level
  //

  //! read the print info zone
  bool readPrintInfo();

protected:
  //
  // data
  //
  //! the state
  shared_ptr<MsWks3ParserInternal::State> m_state;

  //! the list of different Zones
  std::vector<MWAWEntry> m_listZones;

  //! the graph parser
  shared_ptr<MsWksGraph> m_graphParser;

  //! the text parser
  shared_ptr<MsWks3Text> m_textParser;

  //! the actual zone
  shared_ptr<MsWksZone> m_zone;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
