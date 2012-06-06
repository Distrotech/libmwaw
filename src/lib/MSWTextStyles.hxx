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

#ifndef MSW_MWAW_TEXT_STYLES
#  define MSW_MWAW_TEXT_STYLES

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

namespace MSWTextStylesInternal
{
struct State;
struct Section;
}

/** \brief the main class to read/store the text font, paragraph, section stylesread */
class MSWTextStyles
{
  friend class MSWText;
public:
  struct Font;
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
  /* send a font */
  void setProperty(Font const &font);
  /** try to read a font.
      \param font : the read font,
      \param mainZone : true if we read a not style font
      \param initFont : if true begin by reinitializing the font */
  bool readFont(Font &font, bool mainZone, bool initFont=true);

  // section:
  //! read the text section
  bool readSection(MSWEntry &entry);
  //! try to read the section data
  bool readSection(MSWTextStylesInternal::Section &section, long pos);
  //! send section properties
  void setProperty(MSWTextStylesInternal::Section const &sec,
                   Font &actFont, bool recursive=false);

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

public:
  //! Internal: the font of MSWTextStyles
  struct Font {
    //! the constructor
    Font(): m_font(), m_size(0), m_value(0), m_default(true), m_extra("") {
      for (int i = 0; i < 3; i++) m_flags[i] = 0;
    }

    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Font const &font);

    //! the font
    MWAWFont m_font;
    //! a second size
    int m_size;
    //! a unknown value
    int m_value;
    //! some unknown flag
    int m_flags[3];
    //! true if is default
    bool m_default;
    //! extra data
    std::string m_extra;
  };
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
