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

#ifndef MS_WKS4_PARSER
#  define MS_WKS4_PARSER

#include <list>
#include <vector>

#include "MWAWEntry.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

namespace MsWks4ParserInternal
{
struct State;
class SubDocument;
}

class MsWks4Zone;

/** \brief the main class to read a MS Works document v4
 *
 * This class is associated with a MsWks4Parser which reads:
 * the main Ole zones MN0, MacWorks/QHdr, MacWorks/QFtr, MacWorks/QFootnotes
 * and which parses MacWorks/QFrm\<number\>.
 * It also uses an MWAWOleParser in order to find  pictures
 * in the other Ole zones.
 */
class MsWks4Parser : public MWAWTextParser
{
  friend class MsWks4ParserInternal::SubDocument;
  friend class MsWks4Zone;
public:
  //! construtor
  MsWks4Parser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~MsWks4Parser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  //! the main parse function, called with the documentInterface
  virtual void parse(librevenge::RVNGTextInterface *documentInterface);

protected:
  /** finds the principal ole zone: Ole pictures and MN0,
      then tries to find the main structures. Finally, parses the document */
  bool createStructures();

  //! tries to parse the ole zones which have not yet been parsed
  void flushExtra();

  //
  // subdocument helper
  //
  /** creates a subdocument corresponding to a footnote (indicated by id)
   *
   * \note if \a id < 0 meaning that the text corresponding to the note was not
   * found, an empty footnote will be created */
  void sendFootNote(int id);

  /** send the frame text */
  void sendFrameText(MWAWEntry const &entry, std::string const &frame);

  //! send an OLE zone
  void sendOLE(int id, MWAWPosition const &pos, librevenge::RVNGPropertyList frameExtras);

private:
  MsWks4Parser(MsWks4Parser const &orig);
  MsWks4Parser &operator=(MsWks4Parser const &orig);

protected:
  //! the state
  shared_ptr<MsWks4ParserInternal::State> m_state;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
