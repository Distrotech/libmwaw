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
 * Parser to Microsoft Word text document
 *
 */
#ifndef MSW_MWAW_TEXT
#  define MSW_MWAW_TEXT

#include <list>
#include <string>
#include <vector>

#include "libmwaw_tools.hxx"

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

namespace MSWTextInternal
{
struct Font;
struct Paragraph;
struct Section;
struct State;
}

class MSWEntry;
class MSWParser;

/** \brief the main class to read the text part of Claris Works file
 *
 *
 *
 */
class MSWText
{
  friend class MSWParser;

public:
  //! constructor
  MSWText(TMWAWInputStreamPtr ip, MSWParser &parser, MWAWTools::ConvertissorPtr &convertissor);
  //! destructor
  virtual ~MSWText();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;


protected:
  //! sets the listener in this class and in the helper classes
  void setListener(MSWContentListenerPtr listen) {
    m_listener = listen;
  }

  //! send a text zone
  bool sendText(IMWAWEntry const &textEntry);
  /* send a font
   *
   * \param font the font's properties*/
  void setProperty(MSWTextInternal::Font const &font);

  //! finds the different zones given defaut text zone and textlength
  bool createZones(long bot, long (&textLength)[3]);

  //! read the text structure(some paragraph style+some text position?)
  bool readTextStruct(MSWEntry &entry);

  //! read the page limit ?
  bool readPageBreak(MSWEntry &entry);

  //! read the line info(zone)
  bool readLineInfo(MSWEntry &entry);

  //! read the text section ?
  bool readSection(MSWEntry &entry);

  //! read the char/paragraph plc list
  bool readPLCList(MSWEntry &entry);

  //! read the note data
  bool readNotes(MSWEntry &entry, std::vector<long> const &notePos);

  //! read the footnote pos in text + val
  bool readFootnotesPos(MSWEntry &entry, std::vector<long> const &noteDef);

  //! read the footnote data
  bool readFootnotesData(MSWEntry &entry);

  //! read the font names
  bool readFontNames(MSWEntry &entry);

  //! try to read the styles zone
  bool readStyles(MSWEntry &entry);

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //
  // low level
  //

  //! try to read a font
  bool readFont(MSWTextInternal::Font &font, bool mainZone);

  //! try to read a paragraph
  bool readParagraph(MSWTextInternal::Paragraph &para, int dataSz=-1);

  //! try to read the section data
  bool readSection(MSWTextInternal::Section &section, long pos);

  //! read the char/paragraph plc : type=0: char, type=1: parag
  bool readPLC(MSWEntry &entry, int type);

  //! read a zone which consists in a list of int
  bool readLongZone(MSWEntry &entry, int sz, std::vector<long> &list);

  //! finds the style which must be used for each style
  void updateTextEntryStyle();

  //! returns the debug file
  libmwaw_tools::DebugFile &ascii() {
    return m_asciiFile;
  }

private:
  MSWText(MSWText const &orig);
  MSWText &operator=(MSWText const &orig);

protected:
  //
  // data
  //
  //! the input
  TMWAWInputStreamPtr m_input;

  //! the listener
  MSWContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWTools::ConvertissorPtr m_convertissor;

  //! the state
  shared_ptr<MSWTextInternal::State> m_state;

  //! the main parser;
  MSWParser *m_mainParser;

  //! the debug file
  libmwaw_tools::DebugFile &m_asciiFile;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
