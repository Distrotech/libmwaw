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

#ifndef LW_PARSER
#  define LW_PARSER

#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

namespace LWParserInternal
{
class SubDocument;
struct State;
}

class LWGraph;
class LWText;

/** \brief the main class to read a LightWay Text file
 */
class LWParser : public MWAWParser
{
  friend class LWParserInternal::SubDocument;
  friend class LWGraph;
  friend class LWText;
public:
  //! constructor
  LWParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~LWParser();

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
  //! returns the number of column and the column separator (in point)
  bool getColumnInfo(int &numCols, int &colSep) const;
  //! adds a new page
  void newPage(int number);
  //! returns true if the main text is in data fork
  bool textInDataFork() const;

  // interface with the graph parser

  //! ask the graph parser to send a graph
  void sendGraphic(int graphId);

  // interface with the text parser

  //! try to send the header/footer
  bool sendHeaderFooter(bool header);

protected:
  //! finds the different objects zones
  bool createZones();

  //! read a PrintInfo block
  bool readPrintInfo(MWAWEntry const &entry);
  //! read a DocInfo block
  bool readDocInfo(MWAWEntry const &entry);
  //! read a Document, Header/Footer block (1000)
  bool readDocument(MWAWEntry const &entry);
  //! read a LWSR block (1002)
  bool readLWSR2(MWAWEntry const &entry);
  //! read a MPSR block (1005)
  bool readMPSR5(MWAWEntry const &entry);
  //! read a TOC page block
  bool readTOCPage(MWAWEntry const &entry);
  //! read a TOC data block
  bool readTOC(MWAWEntry const &entry);

  //! return the input input
  MWAWInputStreamPtr rsrcInput();

  //! a DebugFile used to write what we recognize when we parse the document in rsrc
  libmwaw::DebugFile &rsrcAscii();

  //
  // data
  //
  //! the state
  shared_ptr<LWParserInternal::State> m_state;

  //! a flag to know if page span has been set
  bool m_pageSpanSet;

  //! the graph parser
  shared_ptr<LWGraph> m_graphParser;

  //! the text parser
  shared_ptr<LWText> m_textParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
