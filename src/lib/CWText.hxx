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

/*
 * Parser to Claris Works text document
 *
 */
#ifndef CW_MWAW_TEXT
#  define CW_MWAW_TEXT

#include <list>
#include <string>
#include <vector>

#include "libmwaw_internal.hxx"

#include "MWAWEntry.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

typedef class MWAWContentListener CWContentListener;
typedef shared_ptr<CWContentListener> CWContentListenerPtr;

class MWAWFont;
class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

namespace CWStruct
{
struct DSET;
}

namespace CWTextInternal
{
struct Paragraph;
struct Zone;
struct State;
}

class CWParser;

/** \brief the main class to read the text part of Claris Works file
 *
 *
 *
 */
class CWText
{
  friend class CWParser;

public:
  //! constructor
  CWText(MWAWInputStreamPtr ip, CWParser &parser, MWAWFontConverterPtr &convertissor);
  //! destructor
  virtual ~CWText();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

  //! reads the zone Text DSET
  shared_ptr<CWStruct::DSET> readDSETZone(CWStruct::DSET const &zone, MWAWEntry const &entry, bool &complete);

protected:

  //! sets the listener in this class and in the helper classes
  void setListener(CWContentListenerPtr listen) {
    m_listener = listen;
  }

  /* sends a character property to the listener
   * \param font the font's properties */
  void setProperty(MWAWFont const &font);
  /** sends a paragraph property to the listener */
  void setProperty(CWTextInternal::Paragraph const &ruler);

  //! sends the zone data to the listener (if it exists )
  bool sendZone(int number);

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //
  // low level
  //

  //! try to read the paragraph
  bool readParagraphs(MWAWEntry const &entry, CWTextInternal::Zone &zone);

  //! try to read a font sequence
  bool readFonts(MWAWEntry const &entry, CWTextInternal::Zone &zone);

  //! try to the token zone)
  bool readTokens(MWAWEntry const &entry, CWTextInternal::Zone &zone);

  //! try to read the text zone size
  bool readTextZoneSize(MWAWEntry const &entry, CWTextInternal::Zone &zone);

  //! try to read the section
  bool readTextSection(CWTextInternal::Zone &zone);

  //! send the text zone to the listener
  bool sendText(CWTextInternal::Zone const &zone);

  //! try to read a font
  bool readFont(int id, int &posC, MWAWFont &font);

  //! try to read a named font
  bool readChar(int id, int fontSize, MWAWFont &font);

  /** read the rulers block which is present at the beginning of the text in the first version of Claris Works : v1-2 */
  bool readParagraphs();

  /** the definition of ruler :
      present at the beginning of the text in the first version of Claris Works : v1-2,
      present in the STYL entries in v4-v6 files */
  bool readParagraph(int id=-1);

  // THE NAMED ENTRY

  /* STYL (in v4-6) : styles definition */
  bool readSTYLs(MWAWEntry const &entry);

  /* read a STYL subzone (in v4-6) */
  bool readSTYL(int id);

  /* read a STYL sequence */
  bool readSTYL_STYL(int N, int fSz);

  /* read a STYL Lookup sequence */
  bool readSTYL_LKUP(int N, int fSz);

  /* read a STYL Name sequence */
  bool readSTYL_NAME(int N, int fSz);

  /* read a STYL Font Name sequence */
  bool readSTYL_FNTM(int N, int fSz);

  /* read a STYL Font sequence */
  bool readSTYL_CHAR(int N, int fSz);

  /* read a STYL Paragraph sequence */
  bool readSTYL_RULR(int N, int fSz);

  /* style: sequence of zone : 1 by style ?*/

  //! returns the debug file
  libmwaw::DebugFile &ascii() {
    return m_asciiFile;
  }

private:
  CWText(CWText const &orig);
  CWText &operator=(CWText const &orig);

protected:
  //
  // data
  //
  //! the input
  MWAWInputStreamPtr m_input;

  //! the listener
  CWContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<CWTextInternal::State> m_state;

  //! the main parser;
  CWParser *m_mainParser;

  //! the debug file
  libmwaw::DebugFile &m_asciiFile;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
