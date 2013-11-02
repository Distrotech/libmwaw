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

#ifndef MSK_PARSER
#  define MSK_PARSER

#include <string>

#include "libmwaw_internal.hxx"

#include "MWAWParser.hxx"

class RVNGPropertyList;

class MSKGraph;
class MSKTable;

namespace MSKParserInternal
{
struct State;
}

/** \brief generic parser for Microsoft Works file
 *
 *
 *
 */
class MSKParser : public MWAWParser
{
  friend class MSKGraph;
  friend class MSKTable;
public:
  //! constructor
  MSKParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! constructor using the parser state
  MSKParser(MWAWInputStreamPtr input, MWAWParserStatePtr parserState);

  //! destructor
  virtual ~MSKParser();

  //! returns the actual input
  MWAWInputStreamPtr &getInput() {
    return m_input;
  }

  //! return the color which correspond to an index
  bool getColor(int id, MWAWColor &col, int vers=-1) const;

  //! return a list of color corresponding to a version
  static std::vector<MWAWColor> const &getPalette(int vers);

  //! virtual function used to send the text of a frame (v4)
  virtual void sendFrameText(MWAWEntry const &entry, std::string const &frame);

  //! virtual function used to send an OLE (v4)
  virtual void sendOLE(int id, MWAWPosition const &pos,
                       RVNGPropertyList frameExtras);

  //! returns the page top left point
  virtual Vec2f getPageLeftTop() const = 0;

  //! a DebugFile used to write what we recognize when we parse the document
  libmwaw::DebugFile &ascii() {
    return m_asciiFile;
  }
protected:
  //! the state
  shared_ptr<MSKParserInternal::State> m_state;
  //! the input which can be an OLE in MSWorks 4 file
  MWAWInputStreamPtr m_input;
  //! the debug file of the actual input
  libmwaw::DebugFile m_asciiFile;
};

#endif
