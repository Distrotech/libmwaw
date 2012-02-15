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
 * parser for Microsoft Word ( version 4.0-5.1 )
 */
#ifndef MSW_MWAW_PARSER
#  define MSW_MWAW_PARSER

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

typedef class MWAWContentListener MSWContentListener;
typedef shared_ptr<MSWContentListener> MSWContentListenerPtr;

namespace MWAWTools
{
class Convertissor;
typedef shared_ptr<Convertissor> ConvertissorPtr;
}

namespace libmwaw_tools
{
class PictData;
}

namespace MSWParserInternal
{
struct Entry;
struct TextEntry;
struct Font;
struct Object;
struct Paragraph;
struct State;
class SubDocument;
}

/** \brief the main class to read a Microsoft Word file
 *
 *
 *
 */
class MSWParser : public IMWAWParser
{
  friend class MSWParserInternal::SubDocument;

public:
  //! constructor
  MSWParser(TMWAWInputStreamPtr input, IMWAWHeader * header);
  //! destructor
  virtual ~MSWParser();

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
  void setListener(MSWContentListenerPtr listen);

  //! creates the listener which will be associated to the document
  void createDocument(WPXDocumentInterface *documentInterface);

  //! finds the different objects zones
  bool createZones();

  //! read the list of zones
  bool readZoneList();

  //! read the font names
  bool readFontNames(MSWParserInternal::Entry &entry);

  //! try to read a font
  bool readFont(MSWParserInternal::Font &font);

  //! try to read a paragraph
  bool readParagraph(MSWParserInternal::Paragraph &para, int dataSz=-1);

  //! read the print info zone
  bool readPrintInfo(MSWParserInternal::Entry &entry);

  //! read the printer name
  bool readPrinter(MSWParserInternal::Entry &entry);

  //! read the document sumary
  bool readDocSum(MSWParserInternal::Entry &entry);

  //! read a zone which consists in a list of int
  bool readIntsZone(MSWParserInternal::Entry &entry, int sz, std::vector<int> &list);

  //! read a zone which consists in a list of string
  bool readStringsZone(MSWParserInternal::Entry &entry, std::vector<std::string> &list);

  //! read the text section ?
  bool readSection(MSWParserInternal::Entry &entry);

  //! read the section data
  bool readSectionData(MSWParserInternal::Entry &entry);

  //! read the page limit ?
  bool readPageBreak(MSWParserInternal::Entry &entry);

  //! read the text ?
  bool readTextData2(MSWParserInternal::Entry &entry);

  //! read the objects
  bool readObjects();

  //! read the object list
  bool readObjectList(MSWParserInternal::Entry &entry);

  //! read the object flags
  bool readObjectFlags(MSWParserInternal::Entry &entry);

  //! read an object
  bool readObject(MSWParserInternal::Object &obj);

  //! read the line info(zone)
  bool readLineInfo(MSWParserInternal::Entry &entry);

  //! read the glossary data
  bool readGlossary(MSWParserInternal::Entry &entry);

  //! read the zone 17(unknown)
  bool readZone17(MSWParserInternal::Entry &entry);

  //! read the zone 18(some paragraph style+some text position?)
  bool readZone18(MSWParserInternal::Entry &entry);

  //! temporary function used to detect some picture
  void searchPictures();

  //! try to read a text zone
  bool readText(MSWParserInternal::TextEntry &entry);

  //! read a picture data
  bool readPicture(MSWParserInternal::Entry &entry);

  //! returns the page height, ie. paper size less margin (in inches)
  float pageHeight() const;
  //! returns the page width, ie. paper size less margin (in inches)
  float pageWidth() const;

  //! adds a new page
  void newPage(int number);

  /*
   * interface with subdocument
   */

  //
  // low level
  //
  //! try to read the styles zone
  bool readStyles(MSWParserInternal::Entry &entry);

  //! read a file entry
  MSWParserInternal::Entry readEntry(std::string type, int id=-1);

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
  MSWContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWTools::ConvertissorPtr m_convertissor;

  //! the state
  shared_ptr<MSWParserInternal::State> m_state;

  //! the actual document size
  DMWAWPageSpan m_pageSpan;

  //! a list of created subdocuments
  std::vector<shared_ptr<MSWParserInternal::SubDocument> > m_listSubDocuments;

  //! the debug file
  libmwaw_tools::DebugFile m_asciiFile;

  //! the debug file name
  std::string m_asciiName;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
