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

/*
 * Parser to Claris Works text document
 *
 */
#ifndef CW_MWAW_PARSER
#  define CW_MWAW_PARSER

#include <list>
#include <string>
#include <vector>

#include "DMWAWPageSpan.hxx"

#include "TMWAWPosition.hxx"

#include "IMWAWEntry.hxx"
#include "IMWAWContentListener.hxx"
#include "IMWAWSubDocument.hxx"

#include "TMWAWDebug.hxx"
#include "TMWAWInputStream.hxx"

#include "IMWAWParser.hxx"

typedef class MWAWContentListener CWContentListener;
typedef shared_ptr<CWContentListener> CWContentListenerPtr;

namespace MWAWTools
{
class Convertissor;
typedef shared_ptr<Convertissor> ConvertissorPtr;
}

namespace libmwaw_tools
{
class PictData;
}

namespace CWParserInternal
{
struct State;
class SubDocument;
}

namespace CWStruct
{
struct DSET;
}

class CWDatabase;
class CWGraph;
class CWSpreadsheet;
class CWTable;
class CWText;

/** \brief the main class to read a Claris Works file
 *
 *
 *
 */
class CWParser : public IMWAWParser
{
  friend class CWParserInternal::SubDocument;
  friend class CWDatabase;
  friend class CWGraph;
  friend class CWSpreadsheet;
  friend class CWTable;
  friend class CWText;

public:
  //! constructor
  CWParser(TMWAWInputStreamPtr input, IMWAWHeader * header);
  //! destructor
  virtual ~CWParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(IMWAWHeader *header, bool strict=false);

  /** returns the file version.
   *
   * this version is only correct after the header is parsed */
  int version() const;

  // the main parse function
  void parse(WPXDocumentInterface *documentInterface);

  //! Debugging: change the default ascii file
  void setAsciiName(char const *name) {
    m_asciiName = name;
  }

protected:
  //! inits all internal variables
  void init();

  //! sets the listener in this class and in the helper classes
  void setListener(CWContentListenerPtr listen);

  //! creates the listener which will be associated to the document
  void createDocument(WPXDocumentInterface *documentInterface);

  //! finds the different objects zones
  bool createZones();

  /** try to find the zone tree graph... and if possible, returns the
      main zone id */
  int findZonesGraph();

  /** find the number of children of a zone */
  int computeNumChildren(int zoneId) const;

  //! read a zone
  bool readZone();

  //! read the print info zone
  bool readPrintInfo();

  //! returns the page height, ie. paper size less margin (in inches)
  float pageHeight() const;
  //! returns the page width, ie. paper size less margin (in inches)
  float pageWidth() const;

  //! adds a new page
  void newPage(int number);

  //
  // interface with the text parser
  //

  //! send a text zone
  bool sendTextZone(int zoneId);

  /** creates a document to send a footnote */
  void sendFootnote(int zoneId);

  //
  // interface with the graph parser
  //

  //! returns the color corresponding to colId (if possible)
  bool getColor(int colId, Vec3uc &col) const;

  //
  // low level
  //

  //! reads the document header
  bool readDocHeader();

  //! reads the end table ( appears in v3.0 : file version ? )
  bool readEndTable();

  /** reads the zone DSET

  set complete to true if we read all the zone */
  shared_ptr<CWStruct::DSET> readDSET(bool &complete);

  // THE NAMED ENTRY

  /* read the document properties */
  bool readDSUM(IMWAWEntry const &entry, bool inHeader);

  /* SNAP (in v6) : size[4]/size[2] picture... */
  bool readSNAP(IMWAWEntry const &entry);

  /* sequence of plist of printer : in v6
   */
  bool readCPRT(IMWAWEntry const &entry);

  /* style: sequence of zone : 1 by style ?*/

  //! returns the debug file
  libmwaw_tools::DebugFile &ascii() {
    return m_asciiFile;
  }

  //! return the ascii file name
  std::string const &asciiName() const {
    return m_asciiName;
  }

protected:


  //
  // data
  //
  //! the listener
  CWContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWTools::ConvertissorPtr m_convertissor;

  //! the state
  shared_ptr<CWParserInternal::State> m_state;

  //! the actual document size
  DMWAWPageSpan m_pageSpan;

  //! the database parser
  shared_ptr<CWDatabase> m_databaseParser;

  //! the graph parser
  shared_ptr<CWGraph> m_graphParser;

  //! the spreadsheet parser
  shared_ptr<CWSpreadsheet> m_spreadsheetParser;

  //! the table parser
  shared_ptr<CWTable> m_tableParser;

  //! the text parser
  shared_ptr<CWText> m_textParser;

  //! a list of created subdocuments
  std::vector<shared_ptr<CWParserInternal::SubDocument> > m_listSubDocuments;

  //! the debug file
  libmwaw_tools::DebugFile m_asciiFile;

  //! the debug file name
  std::string m_asciiName;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
