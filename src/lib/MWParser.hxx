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

#ifndef MW_MWAW_PARSER
#  define MW_MWAW_PARSER

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

typedef class MWAWContentListener MWContentListener;
typedef shared_ptr<MWContentListener> MWContentListenerPtr;

namespace MWAWTools
{
class Convertissor;
typedef shared_ptr<Convertissor> ConvertissorPtr;
}

namespace libmwaw_tools
{
class PictData;
}

namespace MWParserInternal
{
struct State;
struct Information;
struct Paragraph;
class SubDocument;
}

/** \brief the main class to read a MacWrite file
 *
 *
 *
 */
class MWParser : public IMWAWParser
{
  friend class MWParserInternal::SubDocument;

public:
  //! constructor
  MWParser(TMWAWInputStreamPtr input, IMWAWHeader * header);
  //! destructor
  virtual ~MWParser();

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
  void setListener(MWContentListenerPtr listen);

  //! creates the listener which will be associated to the document
  void createDocument(WPXDocumentInterface *documentInterface);

  //! send a zone ( 0: MAIN ZONE, 1 : HEADER, 2 : FOOTER )
  bool sendWindow(int zone);

  //! finds the different objects zones
  bool createZones();

  //! finds the different objects zones (version <= 3)
  bool createZonesV3();

  //! read the print info zone
  bool readPrintInfo();

  //! read the windows zone
  bool readWindowsInfo(int wh);

  //! read the line height
  bool readLinesHeight(IMWAWEntry const &entry, std::vector<int> &firstParagLine,
                       std::vector<int> &linesHeight);

  //! read the information ( version <= 3)
  bool readInformationsV3(int numInfo,
                          std::vector<MWParserInternal::Information> &informations);

  //! read the information
  bool readInformations(IMWAWEntry const &entry,
                        std::vector<MWParserInternal::Information> &informations);

  //! read a ruler
  bool readParagraph(MWParserInternal::Information const &info);

  //! read a graphics
  bool readGraphic(MWParserInternal::Information const &info);

  //! read a text zone
  bool readText(MWParserInternal::Information const &info, std::vector<int> const &lineHeight);

  //! read a page break zone ( version <= 3)
  bool readPageBreak(MWParserInternal::Information const &info);

  //! check the free list
  bool checkFreeList();

  //! returns the page height, ie. paper size less margin (in inches)
  float pageHeight() const;
  //! returns the page width, ie. paper size less margin (in inches)
  float pageWidth() const;

  //! adds a new page
  void newPage(int number);

  //
  // low level
  //

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
  MWContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWTools::ConvertissorPtr m_convertissor;

  //! the state
  shared_ptr<MWParserInternal::State> m_state;

  //! the actual document size
  DMWAWPageSpan m_pageSpan;

  //! a list of created subdocuments
  std::vector<shared_ptr<MWParserInternal::SubDocument> > m_listSubDocuments;

  //! the debug file
  libmwaw_tools::DebugFile m_asciiFile;

  //! the debug file name
  std::string m_asciiName;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
