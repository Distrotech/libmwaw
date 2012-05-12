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
 * Parser to convert some old text document ( created between 1987 and 1988)
 *    with an unknown application
 *
 * Named by elimination: can be WriterPlus document ( a french text editor )
 *       or maybe Nisus(?)
 *
 *       Eliminated: MacWrite, MicrosoftWorks, ClarisWorks, WriteNow
 *
 */
#ifndef WP_MWAW_PARSER
#  define WP_MWAW_PARSER

#include <list>
#include <string>
#include <vector>

#include "DMWAWPageSpan.hxx"

#include "MWAWPosition.hxx"

#include "MWAWEntry.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

typedef class MWAWContentListener WPContentListener;
typedef shared_ptr<WPContentListener> WPContentListenerPtr;

namespace MWAWTools
{
class Convertissor;
typedef shared_ptr<Convertissor> ConvertissorPtr;
}

namespace libmwaw_tools
{
class PictData;
}

namespace WPParserInternal
{
struct State;
struct Font;
struct Line;
struct ParagraphData;
struct ParagraphInfo;
class SubDocument;
}

/** \brief the main class to read a Writerperfect file
 *
 *
 *
 */
class WPParser : public MWAWParser
{
  friend class WPParserInternal::SubDocument;

public:
  //! constructor
  WPParser(MWAWInputStreamPtr input, MWAWHeader *header);
  //! destructor
  virtual ~WPParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

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
  void setListener(WPContentListenerPtr listen);

  //! creates the listener which will be associated to the document
  void createDocument(WPXDocumentInterface *documentInterface);

  //! finds the different objects zones
  bool createZones();

  //! read the print info zone
  bool readPrintInfo();

  //! read the main info for zone ( 0: MAIN ZONE, 1 : HEADER, 2 : FOOTER )
  bool readWindowsInfo(int zone);

  //! send a zone ( 0: MAIN ZONE, 1 : HEADER, 2 : FOOTER )
  bool sendWindow(int zone, Vec2i limits = Vec2i(-1,-1));

  //! read the page info zone
  bool readWindowsZone(int zone);

  //! read the page info zone
  bool readPageInfo(int zone);

  //! read the col info zone ?
  bool readColInfo(int zone);

  //! read the paragraph info zone
  bool readParagraphInfo(int zone);

  //! try to find the column size which correspond to a section
  bool findSectionColumns(int zone, Vec2i limits, std::vector<int> &colSize);

  //! read a section
  bool readSection(WPParserInternal::ParagraphInfo const &info, bool mainBlock);

  //! read a text
  bool readText(WPParserInternal::ParagraphInfo const &info);

  //! read a table
  bool readTable(WPParserInternal::ParagraphInfo const &info);

  //! read a graphic
  bool readGraphic(WPParserInternal::ParagraphInfo const &info);

  //! read a unknown section
  bool readUnknown(WPParserInternal::ParagraphInfo const &info);

  //! returns the page height, ie. paper size less margin (in inches)
  float pageHeight() const;
  //! returns the page width, ie. paper size less margin (in inches)
  float pageWidth() const;

  //! adds a new page
  void newPage(int number);

  //
  // low level
  //

  //! read a paragraph data
  bool readParagraphData(WPParserInternal::ParagraphInfo const &info, bool hasFonts,
                         WPParserInternal::ParagraphData &data);

  //! read a list of font (with position)
  bool readFonts(int nFonts, int type,
                 std::vector<WPParserInternal::Font> &fonts);

  //! read a list of line (with position)
  bool readLines(WPParserInternal::ParagraphInfo const &info,
                 int nLines, std::vector<WPParserInternal::Line> &lines);


  //! returns the debug file
  libmwaw::DebugFile &ascii() {
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
  WPContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWTools::ConvertissorPtr m_convertissor;

  //! the state
  shared_ptr<WPParserInternal::State> m_state;

  //! the actual document size
  DMWAWPageSpan m_pageSpan;

  //! a list of created subdocuments
  std::vector<shared_ptr<WPParserInternal::SubDocument> > m_listSubDocuments;

  //! the debug file
  libmwaw::DebugFile m_asciiFile;

  //! the debug file name
  std::string m_asciiName;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
