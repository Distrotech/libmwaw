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
 * Parser to convert MindWrite document
 */
#ifndef MDW_PARSER
#  define MDW_PARSER

#include <list>
#include <string>
#include <vector>

#include "MWAWPageSpan.hxx"

#include "MWAWPosition.hxx"

#include "MWAWEntry.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

class WPXString;

typedef class MWAWContentListener MDWContentListener;
typedef shared_ptr<MDWContentListener> MDWContentListenerPtr;
class MWAWEntry;
class MWAWFont;
struct MWAWParagraph;
class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

namespace MDWParserInternal
{
struct LineInfo;
struct State;
class SubDocument;
}

/** \brief the main class to read a MindWrite file
 *
 *
 *
 */
class MDWParser : public MWAWParser
{
  friend class MDWParserInternal::SubDocument;

public:
  //! constructor
  MDWParser(MWAWInputStreamPtr input, MWAWHeader *header);
  //! destructor
  virtual ~MDWParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(WPXDocumentInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! sets the listener in this class and in the helper classes
  void setListener(MDWContentListenerPtr listen);

  //! creates the listener which will be associated to the document
  void createDocument(WPXDocumentInterface *documentInterface);

  //! finds the different objects zones
  bool createZones();

  //! try to send a zone
  bool sendZone(int i);

  //! try to read a graphic
  bool readGraphic(MDWParserInternal::LineInfo const &line);

  //! try to read a ruler
  bool readRuler(MDWParserInternal::LineInfo const &line, MWAWParagraph &para);

  //! try to read a compressed text zone
  bool readCompressedText(MDWParserInternal::LineInfo const &line);

  //! try to read a non compressed text zone
  bool readText(MDWParserInternal::LineInfo const &line);

  //! try to send the text
  void sendText(std::string const &text, std::vector<MWAWFont> const &fonts, std::vector<int> const &textPos);

  //! try to read the fonts
  bool readFonts(MWAWEntry const &entry, std::vector<MWAWFont> &fonts, std::vector<int> &textPos);

  //! update the actual ruler using lineInfo
  void updateRuler(MDWParserInternal::LineInfo const &line);

  //! read the print info zone
  bool readPrintInfo(MWAWEntry &entry);

  //! read the lines information zone
  bool readLinesInfo(MWAWEntry &entry);

  //! read the last zone ( pos + 7fffffff )
  bool readLastZone(MWAWEntry &entry);

  //! read the 12 th zone
  bool readZone12(MWAWEntry &entry);

  /* sends a character property to the listener
   * \param font the font's properties */
  void setProperty(MWAWFont const &font);
  /** sends a paragraph property to the listener */
  void setProperty(MWAWParagraph const &para);

  //! returns the page height, ie. paper size less margin (in inches)
  float pageHeight() const;
  //! returns the page width, ie. paper size less margin (in inches)
  float pageWidth() const;

  //! adds a new page
  void newPage(int number);

  //
  // low level
  //

  //! read a file entry
  MWAWEntry readEntry();

  /** check if an entry is in file */
  bool isFilePos(long pos);

protected:
  //
  // data
  //
  //! the listener
  MDWContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<MDWParserInternal::State> m_state;

  //! the actual document size
  MWAWPageSpan m_pageSpan;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
