/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libmwaw
 * Copyright (C) 2009, 2011 Alonso Laurent (alonso@loria.fr)
 * Copyright (C) 2006, 2007 Andrew Ziem
 * Copyright (C) 2004-2006 Fridrich Strba (fridrich.strba@bluewin.ch)
 * Copyright (C) 2004 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2003-2005 William Lachance (william.lachance@sympatico.ca)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 * For further information visit http://libwps.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

#ifndef MSK4_ZONE
#  define MSK4_ZONE

#include <map>
#include <string>

#include "MWAWDebug.hxx"
#include "MWAWEntry.hxx"
#include "MWAWInputStream.hxx"
#include "MWAWPageSpan.hxx"

#include "MSKParser.hxx"

class MWAWHeader;
class MWAWSubDocument;
typedef shared_ptr<MWAWSubDocument> MWAWSubDocumentPtr;

class MSK4Parser;
class MSK4Text;
class MSKGraph;

typedef class MWAWContentListener MSKContentListener;
typedef shared_ptr<MSKContentListener> MSKContentListenerPtr;

class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

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
  MSK4Zone(MWAWInputStreamPtr input, MWAWHeader * header,
           MSK4Parser &parser, MWAWFontConverterPtr &convertissor,
           std::string const &oleName);
  //! destructor
  ~MSK4Zone();

  //! reinitializes all data
  void reset(MWAWInputStreamPtr &input, std::string const &entryName);

protected:
  //! inits all internal variables
  void init();

  //! sets the listener in this class and in the helper classes
  void setListener(MSKContentListenerPtr listen);

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
  MSKContentListenerPtr createListener
  (WPXDocumentInterface *interface,
   MWAWSubDocumentPtr &header, MWAWSubDocumentPtr &footer);

  //! returns the page height, ie. paper size less margin (in inches)
  float pageHeight() const;
  //! returns the page width, ie. paper size less margin (in inches)
  float pageWidth() const;

  //! returns the page top left point
  Vec2f getPageTopLeft() const;

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

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the internal state
  shared_ptr<MSK4ZoneInternal::State> m_state;

  //! the list of entries, name->entry
  std::multimap<std::string, MWAWEntry> m_entryMap;

  //! the document size
  MWAWPageSpan m_pageSpan;

  //! the text parser
  shared_ptr<MSK4Text> m_textParser;

  //! the graph parser
  shared_ptr<MSKGraph> m_graphParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
