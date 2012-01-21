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

#ifndef MW_PRO_PARSER
#  define MW_PRO_PARSER

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

typedef class MWAWContentListener MWProContentListener;
typedef shared_ptr<MWProContentListener> MWProContentListenerPtr;

namespace MWAWTools
{
class Convertissor;
typedef shared_ptr<Convertissor> ConvertissorPtr;
}

namespace libmwaw_tools
{
class PictData;
}

namespace MWProParserInternal
{
struct State;
class SubDocument;
}

/** \brief the main class to read a MacWrite Pro file
 *
 *
 *
 */
class MWProParser : public IMWAWParser
{
  friend class MWProParserInternal::SubDocument;

public:
  //! constructor
  MWProParser(TMWAWInputStreamPtr input, IMWAWHeader * header);
  //! destructor
  virtual ~MWProParser();

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
  void setListener(MWProContentListenerPtr listen);

  //! creates the listener which will be associated to the document
  void createDocument(WPXDocumentInterface *documentInterface);

  //! finds the different objects zones
  bool createZones();

  //! returns the page height, ie. paper size less margin (in inches)
  float pageHeight() const;
  //! returns the page width, ie. paper size less margin (in inches)
  float pageWidth() const;

  //! adds a new page
  void newPage(int number);

  //
  // low level
  //

  //! read the print info zone
  bool readPrintInfo();

  //! try to read the doc header zone
  bool readDocHeader();

  //! try to read the paragraph styles zone which begins at address 0x200
  bool readStyles();

  //! try to read a style
  bool readStyle(int styleId);

  //! try to read the character styles zone
  bool readCharStyles();

  //! try to read a 16 bytes the zone which follow the char styles zone
  bool readZoneA();

  //! try to read the fonts zone
  bool readFonts();

  //! try to read a zone which follow the fonts zone(checkme)
  bool readZoneB();

  //! try to read a zone which follow the fonts zone
  bool readZoneC();

  bool readAZone();

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
  MWProContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWTools::ConvertissorPtr m_convertissor;

  //! the state
  shared_ptr<MWProParserInternal::State> m_state;

  //! the actual document size
  DMWAWPageSpan m_pageSpan;

  //! a list of created subdocuments
  std::vector<shared_ptr<MWProParserInternal::SubDocument> > m_listSubDocuments;

  //! the debug file
  libmwaw_tools::DebugFile m_asciiFile;

  //! the debug file name
  std::string m_asciiName;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
