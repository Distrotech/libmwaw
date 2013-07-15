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

#ifndef MSK4_ZONE
#  define MSK4_ZONE

#include <map>
#include <string>

#include "MWAWDebug.hxx"
#include "MWAWEntry.hxx"
#include "MWAWInputStream.hxx"

#include "MSKParser.hxx"

class MSK4Parser;
class MSK4Text;
class MSKGraph;

namespace MSK4ZoneInternal
{
struct State;
}
namespace MSK4ParserInternal
{
class SubDocument;
}

/** The class which parses the main zones of a mac MS Works document v4
 *
 * This class must be associated with a MSK4Parser, which gives it the oles to parse.
 * This oles can be MN0, MacWorks/QHdr, MacWorks/QFtr, MacWorks/QFootnotes
 * and  MacWorks/QFrm\<number\> .
 *
 * It creates a MSKGraph, a MSK4Text to parse the
 *   the graphic and the text parts.
 *
 * It reads the entries:
 * - DOP : main document properties: dimension, ... (only parsed)
 * - FRAM : a zone which contains dimensions of objects (textbox, picture, ...) : only parsed
 * - PRNT : the printer information which contains page dimensions, margins, ...
 * - RLRB : an unknown zone which seems to contain some dimension ( only parsed) : maybe related to RBDR ( see MSK4Graph)
 * - SELN : the actual text/... selection
 *
 */
class MSK4Zone : public MSKParser
{
  friend class MSK4ParserInternal::SubDocument;
  friend class MSK4Parser;
  friend class MSKGraph;
  friend class MSK4Text;

public:
  //! constructor
  MSK4Zone(MWAWInputStreamPtr input, MWAWParserStatePtr parserState,
           MSK4Parser &parser, std::string const &oleName);
  //! destructor
  ~MSK4Zone();

protected:
  //! inits all internal variables
  void init();

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
  MWAWContentListenerPtr createListener
  (WPXDocumentInterface *interface,
   MWAWSubDocumentPtr &header, MWAWSubDocumentPtr &footer);

  //! returns the page height, ie. paper size less margin (in inches) less header/footer size
  double getTextHeight() const;

  //! returns the page top left point
  Vec2f getPageLeftTop() const;

  //! adds a new page
  void newPage(int number);

  /** creates a document for a footnote which some id (via MSK4Parser )
   *
   * \note if id==-1, the footnote will be empty */
  void sendFootNote(int id);
  //! sends text corresponding to the footnote id to the listener (via MSK4Text)
  void readFootNote(int id);

  /** send the frame text */
  void sendFrameText(MWAWEntry const &entry, std::string const &frame);

  /** send a rbil zone */
  void sendRBIL(int id, Vec2i const &sz);

  //! send an OLE zone
  void sendOLE(int id, MWAWPosition const &pos, WPXPropertyList frameExtras);

  /** return the text positions ( used for frame text) */
  MWAWEntry getTextPosition() const;

  //! empty implementation of the parse function ( to make the class not virtual)
  void parse(WPXDocumentInterface *) {
    MWAW_DEBUG_MSG(("MSK4Zone::parse: must not be called\n"));
  }
  //! empty implementation of the checkHeader function ( to make the class not virtual)
  bool checkHeader(MWAWHeader *, bool) {
    MWAW_DEBUG_MSG(("MSK4Zone::checkHeader: must not be called\n"));
    return false;
  }

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
   * \note this zone is only parsed, maybe MSK4Graph must parse this zone ? */
  bool readRLRB(MWAWInputStreamPtr input, MWAWEntry const &entry);

  /** parses the SELN zone which seems to contain some information about the actual
   *
   * \note this zone is only parsed ; the read data are not used */
  bool readSELN(MWAWInputStreamPtr input, MWAWEntry const &entry);

  //! inits the ascii file
  void setAscii(std::string const &oleName);

private:
  MSK4Zone(MSK4Zone const &orig);
  MSK4Zone &operator=(MSK4Zone const &orig);
protected:
  //
  // data
  //

  //! the main parser
  MSK4Parser *m_mainParser;

  //! the internal state
  shared_ptr<MSK4ZoneInternal::State> m_state;

  //! the list of entries, name->entry
  std::multimap<std::string, MWAWEntry> m_entryMap;

  //! the text parser
  shared_ptr<MSK4Text> m_textParser;

  //! the graph parser
  shared_ptr<MSKGraph> m_graphParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
