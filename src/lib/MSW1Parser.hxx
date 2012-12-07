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

#ifndef MSW1_PARSER
#  define MSW1_PARSER

#include <vector>

#include "MWAWPageSpan.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

class MWAWEntry;
class MWAWContentListener;
typedef class MWAWContentListener MSW1ContentListener;
typedef shared_ptr<MSW1ContentListener> MSW1ContentListenerPtr;
class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

namespace MSW1ParserInternal
{
struct Font;
struct Paragraph;
struct State;
class SubDocument;
}

/** \brief the main class to read a Microsoft Word 1 file
 *
 *
 *
 */
class MSW1Parser : public MWAWParser
{
  friend class MSW1ParserInternal::SubDocument;
public:
  //! constructor
  MSW1Parser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~MSW1Parser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(WPXDocumentInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();
  //! sets the listener in this class and in the helper classes
  void setListener(MSW1ContentListenerPtr listen);

  //! creates the listener which will be associated to the document
  void createDocument(WPXDocumentInterface *documentInterface);

  //! try to send the main zone
  void sendMain();

  //! returns the page height, ie. paper size less margin (in inches)
  float pageHeight() const;
  //! returns the page width, ie. paper size less margin (in inches)
  float pageWidth() const;

  //! adds a new page
  void newPage(int number);


  //! finds the different zones
  bool createZones();

  //! send the text structure to the listener
  bool sendText(MWAWEntry const &entry, bool main=false);

  //
  // internal level
  //

  /** try to read a char property */
  bool readFont(long fPos, MSW1ParserInternal::Font &font);

  /** try to read a paragraph property */
  bool readParagraph(long fPos, MSW1ParserInternal::Paragraph &para);

  /** try to read the footnote correspondance ( zone2 ) */
  bool readFootnoteCorrespondance(Vec2i limit);

  /** try to read the document info (zone 3) */
  bool readDocInfo(Vec2i limit);
  /** try to read the list of zones: separator between text and footnote? (zone 4) */
  bool readZones(Vec2i limit);
  /** try to read the page break (zone 5) */
  bool readPageBreak(Vec2i limit);

  /** prepare the data: separate header/footer zones to the main stream... */
  bool prepareTextZones();

  //
  // low level
  //

  /** check if an entry is in file */
  bool isFilePos(long pos);
  /** shorten an entry if the last character is EOL */
  void removeLastCharIfEOL(MWAWEntry &entry);
  /** read the two first zones (char and paragraph) */
  bool readPLC(Vec2i limits, int wh);

  /** send the character properties */
  void setProperty(MSW1ParserInternal::Font const &font, MSW1ParserInternal::Font &previousFont);
  /** send the ruler properties */
  void setProperty(MSW1ParserInternal::Paragraph const &para);


protected:
  //
  // data
  //
  //! the listener
  MSW1ContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<MSW1ParserInternal::State> m_state;

  //! the actual document size
  MWAWPageSpan m_pageSpan;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab: