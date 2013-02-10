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

#ifndef TT_PARSER
#  define TT_PARSER

#include <string>
#include <vector>

#include <libwpd/libwpd.h>

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"
#include "MWAWPageSpan.hxx"

#include "MWAWParser.hxx"

class MWAWContentListener;
typedef shared_ptr<MWAWContentListener> MWAWContentListenerPtr;

class MWAWEntry;
class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;
class MWAWPosition;

namespace TTParserInternal
{
struct State;
}

/** \brief the main class to read a SimpleText/TeachText/Tex-Edit file
 */
class TTParser : public MWAWParser
{
  friend class TTText;
public:
  //! constructor
  TTParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~TTParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(WPXDocumentInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! sets the listener in this class and in the helper classes
  void setListener(MWAWContentListenerPtr listen);

  //! creates the listener which will be associated to the document
  void createDocument(WPXDocumentInterface *documentInterface);

  //! adds a new page
  void newPage(int number);

protected:
  //! finds the different objects zones
  bool createZones();
  //! try to read the styles ( resource styl : SimpleText,id=128, Tex-Edit,id=1000 )
  bool readStyles(MWAWEntry const &entry);

  //! try to read the unknown wrct structure ( only in TexEdit,id=1000 )
  bool readWRCT(MWAWEntry const &entry);

  // Intermediate level

  /** compute the number of page of a zone*/
  int computeNumPages() const;

  /** try to send the main text*/
  bool sendText();
  //! try to send a picture knowing the id
  bool sendPicture(int id);
  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //! return the input input
  MWAWInputStreamPtr rsrcInput();
  //! a DebugFile used to write what we recognize when we parse the document in rsrc
  libmwaw::DebugFile &rsrcAscii();

  //
  // data
  //
  //! the listener
  MWAWContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<TTParserInternal::State> m_state;

  //! the actual document size
  MWAWPageSpan m_pageSpan;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
