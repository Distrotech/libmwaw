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
 * Class to read/store the text font and paragraph styles
 */

#ifndef MSW_TEXT_STYLES
#  define MSW_TEXT_STYLES

#include <iostream>
#include <string>

#include "libmwaw_internal.hxx"

#include "MWAWEntry.hxx"
#include "MWAWFont.hxx"
#include "MWAWParagraph.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

typedef class MWAWContentListener MSWContentListener;
typedef shared_ptr<MSWContentListener> MSWContentListenerPtr;

class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

class MSWParser;
class MSWText;

namespace MSWStruct
{
struct Font;
struct Paragraph;
struct Section;
}

namespace MSWTextStylesInternal
{
struct State;
}

/** \brief the main class to read/store the text font, paragraph, section stylesread */
class MSWTextStyles
{
  friend class MSWText;
public:
  enum ZoneType { TextZone, TextStructZone, StyleZone, InParagraphDefinition };
  //! constructor
  MSWTextStyles(MWAWInputStreamPtr ip, MSWText &textParser, MWAWFontConverterPtr &convertissor);
  //! destructor
  virtual ~MSWTextStyles();

  /** returns the file version */
  int version() const;

protected:
  //! sets the listener in this class and in the helper classes
  void setListener(MSWContentListenerPtr listen) {
    m_listener = listen;
  }

  // font:
  //! returns the default font
  MWAWFont const &getDefaultFont() const;
  //! return a font corresponding to an index
  bool getFont(ZoneType type, int id, MSWStruct::Font &actFont);
  /* try to send a font. If so, update the font */
  bool sendFont(ZoneType type, int id, MSWStruct::Font &actFont);

  /* send a font */
  void setProperty(MSWStruct::Font const &font);
  /** try to read a font.
      \param font : the read font,
      \param type : the zone in which the font is read */
  bool readFont(MSWStruct::Font &font, ZoneType type);

  // pararagraph:
  //! return a paragraph corresponding to an index
  bool getParagraph(ZoneType type, int id, MSWStruct::Paragraph &para);
  //! try to read a paragraph
  bool readParagraph(MSWStruct::Paragraph &para, int dataSz=-1);
  //! send a default paragraph
  void sendDefaultParagraph(MSWStruct::Font &actFont);

  //! send paragraph properties
  void setProperty(MSWStruct::Paragraph const &para,
                   MSWStruct::Font &actFont, bool recursifCall=false);

  //! read the main char/paragraph plc list
  bool readPLCList(MSWEntry &entry);
  //! read the paragraphs at the beginning of the text structure zone
  bool readTextStructList(MSWEntry &entry);
  /** read the end of a line zone (2 bytes).
      Returns a textstruct parag id or -1 */
  int readTextStructParaZone(std::string &extra);
  //! read the char/paragraph plc : type=0: char, type=1: parag
  bool readPLC(MSWEntry &entry, int type);

  // section:
  //! return a section corresponding to an index
  bool getSection(ZoneType type, int id, MSWStruct::Section &section);
  //! return a font corresponding to the section
  bool getSectionFont(ZoneType type, int id, MSWStruct::Font &font);
  //! read the text section
  bool readSection(MSWEntry &entry);
  //! try to send a section then update the font
  bool sendSection(int id, MSWStruct::Font &newFont);

  //! try to read the section data
  bool readSection(MSWStruct::Section &section, long pos);
  //! send section properties
  void setProperty(MSWStruct::Section const &sec,
                   MSWStruct::Font &actFont, bool recursifCall=false);

  //! try to read the styles zone
  bool readStyles(MSWEntry &entry);
  //! try to read the styles hierachy
  bool readStylesHierarchy(MSWEntry &entry, int N, std::vector<int> &previous);
  //! try to read the styles names and fill the number of "named" styles...
  bool readStylesNames(MSWEntry const &zone, int N, int &Nnamed);
  //! try to read the styles fonts
  bool readStylesFont(MSWEntry &zone, int N, std::vector<int> const &previous,
                      std::vector<int> const &order);
  //! try to read the styles fonts
  bool readStylesParagraph(MSWEntry &zone, int N, std::vector<int> const &previous,
                           std::vector<int> const &order);
  //! try to reorder the styles to find a good order
  std::vector<int> orderStyles(std::vector<int> const &previous);

  //! returns the debug file
  libmwaw::DebugFile &ascii() {
    return m_asciiFile;
  }

private:
  MSWTextStyles(MSWTextStyles const &orig);
  MSWTextStyles &operator=(MSWTextStyles const &orig);

protected:
  //
  // data
  //
  //! the input
  MWAWInputStreamPtr m_input;

  //! the listener
  MSWContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<MSWTextStylesInternal::State> m_state;

  //! the main parser;
  MSWParser *m_mainParser;

  //! the text parser;
  MSWText *m_textParser;

  //! the debug file
  libmwaw::DebugFile &m_asciiFile;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
