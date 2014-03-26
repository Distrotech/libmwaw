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

#ifndef MS_WKS4_ZONE
#  define MS_WKS4_ZONE

#include <map>
#include <string>

#include "MWAWDebug.hxx"
#include "MWAWEntry.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

#include "MsWksDocument.hxx"

class MsWksParser;
class MsWksDRParser;

namespace MsWks4ZoneInternal
{
struct State;
}

/** The class which parses the main zones of a mac MS Works document v4
 *
 * This class must be associated with a MsWksParser or a MsWksDocument, which gives it the oles to parse.
 * This oles can be MN0, MacWorks/QHdr, MacWorks/QFtr, MacWorks/QFootnotes
 * and  MacWorks/QFrm\<number\> .
 *
 * It creates a MsWksGraph, a MsWks4Text to parse the
 *   the graphic and the text parts.
 *
 * It reads the entries:
 * - DOP : main document properties: dimension, ... (only parsed)
 * - FRAM : a zone which contains dimensions of objects (textbox, picture, ...) : only parsed
 * - PRNT : the printer information which contains page dimensions, margins, ...
 * - RLRB : an unknown zone which seems to contain some dimension ( only parsed) : maybe related to RBDR ( see MsWks4Graph)
 * - SELN : the actual text/... selection
 *
 */
class MsWks4Zone
{
  friend class MsWksDocument;
  friend class MsWksDRParser;
  friend class MsWksParser;
  friend class MsWks4Text;

public:
  //! constructor
  MsWks4Zone(MWAWInputStreamPtr input, MWAWParserStatePtr parserState,
             MWAWParser &parser, std::string const &oleName);
  //! destructor
  ~MsWks4Zone();

protected:
  //! inits all internal variables
  void init();
  //! returns the actual input
  MWAWInputStreamPtr getInput();

  /** tries to find the beginning of the list of indices,
   * then try to find all entries in this list.
   *
   * Stores result in nameTable, offsetTable */
  bool parseHeaderIndex(MWAWInputStreamPtr &input);

  //! parses an index entry
  bool parseHeaderIndexEntry(MWAWInputStreamPtr &input);

  //! finds and parses all the zones to prepare the data
  bool createZones(bool mainOle);

  /** final reading of a text zone
   *
   * \note reads all textzone if !entry.valid(), if not does nothing */
  void readContentZones(MWAWEntry const &entry, bool mainOle);

  /** creates the main listener */
  MWAWTextListenerPtr createListener(librevenge::RVNGTextInterface *interface);

  //! returns the page height, ie. paper size less margin (in inches) less header/footer size
  double getTextHeight() const;

  //! adds a new page
  void newPage(int number, bool soft=false);

  //! sends text corresponding to the footnote id to the listener (via MsWks4Text)
  void readFootNote(int id);

  /** return the text positions ( used for frame text) */
  MWAWEntry getTextPosition() const;

  //
  // low level
  //

  /** reads the PRNT zone which contains the printer properties ( page dimension, margins, ...) */
  bool readPRNT(MWAWInputStreamPtr input, MWAWEntry const &entry, MWAWPageSpan &page);

  /** parses the DIO zone which contains the document properties (dimension, ...)
   *
   * \note this zone is only parsed ; the read data are not used.*/
  bool readDOP(MWAWInputStreamPtr input, MWAWEntry const &entry, MWAWPageSpan &page);

  /** parses the FRAM zone which contains some information about frames (header, footer, ...)
   *
   * \note this zone is only parsed ; the read data are not used */
  bool readFRAM(MWAWInputStreamPtr input, MWAWEntry const &entry);

  /** parses the RLRB zone which seems to contain some position in the page ?
   *
   * \note this zone is only parsed, maybe MsWksGraph must parse this zone ? */
  bool readRLRB(MWAWInputStreamPtr input, MWAWEntry const &entry);

  /** parses the SELN zone which seems to contain some information about the actual
   *
   * \note this zone is only parsed ; the read data are not used */
  bool readSELN(MWAWInputStreamPtr input, MWAWEntry const &entry);

  //! inits the ascii file
  void setAscii(std::string const &oleName);
  //! a DebugFile used to write what we recognize when we parse the document
  libmwaw::DebugFile &ascii();

private:
  MsWks4Zone(MsWks4Zone const &orig);
  MsWks4Zone &operator=(MsWks4Zone const &orig);
protected:
  //
  // data
  //

  //! the main parser
  MWAWParser *m_mainParser;

  //! the parser state
  shared_ptr<MWAWParserState> m_parserState;

  //! the internal state
  shared_ptr<MsWks4ZoneInternal::State> m_state;

  //! the zone data
  shared_ptr<MsWksDocument> m_document;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
