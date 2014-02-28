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

#ifndef MWAW_PARSER_H
#define MWAW_PARSER_H

#include <ostream>
#include <string>
#include <vector>

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWEntry.hxx"
#include "MWAWHeader.hxx"
#include "MWAWPageSpan.hxx"

/** a class to define the parser state */
class MWAWParserState
{
public:
  //! the parser state type
  enum Type { Graphic, Spreadsheet, Text };
  //! Constructor
  MWAWParserState(Type type, MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  ~MWAWParserState();
  //! returns the main listener
  MWAWListenerPtr getMainListener();
  //! the state type
  Type m_type;
  //! the document kind
  MWAWDocument::Kind m_kind;
  //! the actual version
  int m_version;
  //! the input
  MWAWInputStreamPtr m_input;
  //! the header
  MWAWHeader *m_header;
  //! the resource parser
  MWAWRSRCParserPtr m_rsrcParser;
  //! the actual document size
  MWAWPageSpan m_pageSpan;

  //! the font converter
  MWAWFontConverterPtr m_fontConverter;
  //! the graphic listener
  MWAWGraphicListenerPtr m_graphicListener;
  //! the list manager
  MWAWListManagerPtr m_listManager;
  //! the spreadsheet listener
  MWAWSpreadsheetListenerPtr m_spreadsheetListener;
  //! the text listener
  MWAWTextListenerPtr m_textListener;

  //! the debug file
  libmwaw::DebugFile m_asciiFile;

private:
  MWAWParserState(MWAWParserState const &orig);
  MWAWParserState &operator=(MWAWParserState const &orig);
};

/** virtual class which defines the ancestor of all main zone parser */
class MWAWParser
{
public:
  //! virtual destructor
  virtual ~MWAWParser();
  //! virtual function used to check if the document header is correct (or not)
  virtual bool checkHeader(MWAWHeader *header, bool strict=false) = 0;

  //! returns the works version
  int version() const
  {
    return m_parserState->m_version;
  }
  //! returns the parser state
  MWAWParserStatePtr getParserState()
  {
    return m_parserState;
  }
  //! returns the header
  MWAWHeader *getHeader()
  {
    return m_parserState->m_header;
  }
  //! returns the actual input
  MWAWInputStreamPtr &getInput()
  {
    return m_parserState->m_input;
  }
  //! returns the main listener
  MWAWListenerPtr getMainListener();
  //! returns the graphic listener
  MWAWGraphicListenerPtr &getGraphicListener()
  {
    return m_parserState->m_graphicListener;
  }
  //! returns the spreadsheet listener
  MWAWSpreadsheetListenerPtr &getSpreadsheetListener()
  {
    return m_parserState->m_spreadsheetListener;
  }
  //! returns the text listener
  MWAWTextListenerPtr &getTextListener()
  {
    return m_parserState->m_textListener;
  }
  //! returns the font converter
  MWAWFontConverterPtr &getFontConverter()
  {
    return m_parserState->m_fontConverter;
  }
  //! returns the actual page dimension
  MWAWPageSpan const &getPageSpan() const
  {
    return m_parserState->m_pageSpan;
  }
  //! returns the actual page dimension
  MWAWPageSpan &getPageSpan()
  {
    return m_parserState->m_pageSpan;
  }
  //! returns the form length
  double getFormLength() const
  {
    return m_parserState->m_pageSpan.getFormLength();
  }
  //! returns the form width
  double getFormWidth() const
  {
    return m_parserState->m_pageSpan.getFormWidth();
  }
  //! returns the page length (form length without margin )
  double getPageLength() const
  {
    return m_parserState->m_pageSpan.getPageLength();
  }
  //! returns the page width (form width without margin )
  double getPageWidth() const
  {
    return m_parserState->m_pageSpan.getPageWidth();
  }
  //! returns the rsrc parser
  MWAWRSRCParserPtr &getRSRCParser()
  {
    return m_parserState->m_rsrcParser;
  }
  //! a DebugFile used to write what we recognize when we parse the document
  libmwaw::DebugFile &ascii()
  {
    return m_parserState->m_asciiFile;
  }
protected:
  //! constructor (protected)
  MWAWParser(MWAWParserState::Type type, MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! constructor using a state
  MWAWParser(MWAWParserStatePtr state) : m_parserState(state), m_asciiName("") { }

  //! sets the works version
  void setVersion(int vers)
  {
    m_parserState->m_version = vers;
  }
  //! sets the graphic listener
  void setGraphicListener(MWAWGraphicListenerPtr &listener);
  //! resets the listener
  void resetGraphicListener();
  //! sets the spreadsheet listener
  void setSpreadsheetListener(MWAWSpreadsheetListenerPtr &listener);
  //! resets the listener
  void resetSpreadsheetListener();
  //! sets the text listener
  void setTextListener(MWAWTextListenerPtr &listener);
  //! resets the listener
  void resetTextListener();
  //! sets the font convertor
  void setFontConverter(MWAWFontConverterPtr fontConverter);
  //! Debugging: change the default ascii file
  void setAsciiName(char const *name)
  {
    m_asciiName = name;
  }
  //! return the ascii file name
  std::string const &asciiName() const
  {
    return m_asciiName;
  }

private:
  //! private copy constructor: forbidden
  MWAWParser(const MWAWParser &);
  //! private operator=: forbidden
  MWAWParser &operator=(const MWAWParser &);

  //! the parser state
  MWAWParserStatePtr m_parserState;
  //! the debug file name
  std::string m_asciiName;
};

/** virtual class which defines the ancestor of all graphic zone parser */
class MWAWGraphicParser : public MWAWParser
{
public:
  //! virtual function used to parse the input
  virtual void parse(librevenge::RVNGDrawingInterface *documentInterface) = 0;
protected:
  //! constructor (protected)
  MWAWGraphicParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) : MWAWParser(MWAWParserState::Graphic, input, rsrcParser, header) {}
  //! constructor using a state
  MWAWGraphicParser(MWAWParserStatePtr state) : MWAWParser(state) {}
};

/** virtual class which defines the ancestor of all spreadsheet zone parser */
class MWAWSpreadsheetParser : public MWAWParser
{
public:
  //! virtual function used to parse the input
  virtual void parse(librevenge::RVNGSpreadsheetInterface *documentInterface) = 0;
protected:
  //! constructor (protected)
  MWAWSpreadsheetParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) : MWAWParser(MWAWParserState::Spreadsheet, input, rsrcParser, header) {}
  //! constructor using a state
  MWAWSpreadsheetParser(MWAWParserStatePtr state) : MWAWParser(state) {}
};

/** virtual class which defines the ancestor of all text zone parser */
class MWAWTextParser : public MWAWParser
{
public:
  //! virtual function used to parse the input
  virtual void parse(librevenge::RVNGTextInterface *documentInterface) = 0;
protected:
  //! constructor (protected)
  MWAWTextParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) : MWAWParser(MWAWParserState::Text, input, rsrcParser, header) {}
  //! constructor using a state
  MWAWTextParser(MWAWParserStatePtr state) : MWAWParser(state) {}
};

#endif /* MWAWPARSER_H */
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
