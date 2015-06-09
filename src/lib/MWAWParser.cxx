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

#include "MWAWFontConverter.hxx"
#include "MWAWGraphicListener.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWList.hxx"
#include "MWAWPresentationListener.hxx"
#include "MWAWSpreadsheetListener.hxx"
#include "MWAWTextListener.hxx"

#include "MWAWParser.hxx"

MWAWParserState::MWAWParserState(MWAWParserState::Type type, MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  m_type(type), m_kind(MWAWDocument::MWAW_K_TEXT), m_version(0), m_input(input), m_header(header),
  m_rsrcParser(rsrcParser), m_pageSpan(), m_fontConverter(),
  m_graphicListener(), m_listManager(), m_presentationListener(), m_spreadsheetListener(), m_textListener(), m_asciiFile(input)
{
  if (header) {
    m_version=header->getMajorVersion();
    m_kind=header->getKind();
  }
  m_fontConverter.reset(new MWAWFontConverter);
  m_listManager.reset(new MWAWListManager);
}

MWAWParserState::~MWAWParserState()
{
  if (getMainListener()) try {
      /* must never happen, only sanity check....

            Ie. the parser which creates a listener, must delete it.
            */
      MWAW_DEBUG_MSG(("MWAWParserState::~MWAWParserState: the listener is NOT closed, call enddocument without any subdoc\n"));
      getMainListener()->endDocument(false);
    }
    catch (const libmwaw::ParseException &) {
      MWAW_DEBUG_MSG(("MWAWParserState::~MWAWParserState: endDocument FAILS\n"));
      /* must never happen too...

      Ie. the different parsers are responsable to create enough pages,
      if we have exception here, this will indicate a second error in code
      */
    }
}

MWAWListenerPtr MWAWParserState::getMainListener()
{
  switch (m_type) {
  case Graphic:
    return m_graphicListener;
  case Presentation:
    return m_presentationListener;
  case Spreadsheet:
    return m_spreadsheetListener;
  case Text:
    return m_textListener;
  default:
    MWAW_DEBUG_MSG(("MWAWParserState:::getMainListener unexpected document type\n"));
  }
  return MWAWListenerPtr();
}

MWAWParser::MWAWParser(MWAWParserState::Type type, MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header):
  m_parserState(), m_asciiName("")
{
  m_parserState.reset(new MWAWParserState(type, input, rsrcParser, header));
}

MWAWParser::~MWAWParser()
{
}

void MWAWParser::setFontConverter(MWAWFontConverterPtr fontConverter)
{
  m_parserState->m_fontConverter=fontConverter;
}

MWAWListenerPtr MWAWParser::getMainListener()
{
  return m_parserState->getMainListener();
}

void MWAWParser::setGraphicListener(MWAWGraphicListenerPtr &listener)
{
  m_parserState->m_graphicListener=listener;
}

void MWAWParser::resetGraphicListener()
{
  if (getGraphicListener()) getGraphicListener()->endDocument();
  m_parserState->m_graphicListener.reset();
}

void MWAWParser::setPresentationListener(MWAWPresentationListenerPtr &listener)
{
  m_parserState->m_presentationListener=listener;
}

void MWAWParser::resetPresentationListener()
{
  if (getPresentationListener()) getPresentationListener()->endDocument();
  m_parserState->m_presentationListener.reset();
}

void MWAWParser::setSpreadsheetListener(MWAWSpreadsheetListenerPtr &listener)
{
  m_parserState->m_spreadsheetListener=listener;
}

void MWAWParser::resetSpreadsheetListener()
{
  if (getSpreadsheetListener()) getSpreadsheetListener()->endDocument();
  m_parserState->m_spreadsheetListener.reset();
}

void MWAWParser::setTextListener(MWAWTextListenerPtr &listener)
{
  m_parserState->m_textListener=listener;
}

void MWAWParser::resetTextListener()
{
  if (getTextListener()) getTextListener()->endDocument();
  m_parserState->m_textListener.reset();
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
