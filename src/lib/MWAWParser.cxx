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

#include "MWAWContentListener.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWList.hxx"

#include "MWAWParser.hxx"

MWAWParserState::MWAWParserState(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  m_version(0), m_input(input), m_header(header),
  m_rsrcParser(rsrcParser), m_fontConverter(), m_graphicStyleManager(), m_listManager(), m_listener(), m_asciiFile(input)
{
  if (header) m_version=header->getMajorVersion();
  m_fontConverter.reset(new MWAWFontConverter);
  m_graphicStyleManager.reset(new MWAWGraphicStyleManager(m_fontConverter));
  m_listManager.reset(new MWAWListManager);
}

MWAWParserState::~MWAWParserState()
{
  if (m_listener.get()) try {
      /* must never happen, only sanity check....

      Ie. the parser which creates a listener, must delete it.
      */
      MWAW_DEBUG_MSG(("MWAWParserState::~MWAWParserState: the listener is NOT closed, call enddocument without any subdoc\n"));
      m_listener->endDocument(false);
    } catch (const libmwaw::ParseException &) {
      MWAW_DEBUG_MSG(("MWAWParserState::~MWAWParserState: endDocument FAILS\n"));
      /* must never happen too...

      Ie. the different parsers are responsable to create enough pages,
      if we have exception here, this will indicate a second error in code
      */
    }
}

MWAWParser::MWAWParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header):
  m_parserState(), m_pageSpan(), m_asciiName("")
{
  m_parserState.reset(new MWAWParserState(input, rsrcParser, header));
}

MWAWParser::~MWAWParser()
{
}

void MWAWParser::setListener(MWAWContentListenerPtr &listener)
{
  m_parserState->m_listener=listener;
}

void MWAWParser::resetListener()
{
  if (getListener()) getListener()->endDocument();
  m_parserState->m_listener.reset();
}

void MWAWParser::setFontConverter(MWAWFontConverterPtr fontConverter)
{
  m_parserState->m_fontConverter=fontConverter;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
