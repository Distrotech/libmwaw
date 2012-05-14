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
 * Parser to Microsoft Works text document
 *
 */
#ifndef MSK_MWAW_TEXT
#  define MSK_MWAW_TEXT

#include <list>
#include <string>
#include <vector>

#include "libmwaw_tools.hxx"

#include "DMWAWPageSpan.hxx"

#include "MWAWPosition.hxx"

#include "MWAWEntry.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

typedef class MWAWContentListener MSKContentListener;
typedef shared_ptr<MSKContentListener> MSKContentListenerPtr;

class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

namespace MSKTextInternal
{
struct Font;
struct Paragraph;
struct LineZone;
struct TextZone;
struct State;
}

class MSKParser;

/** \brief the main class to read the text part of Microsoft Works file
 *
 *
 *
 */
class MSKText
{
  friend class MSKParser;
public:
  //! constructor
  MSKText(MWAWInputStreamPtr ip, MSKParser &parser, MWAWFontConverterPtr &convertissor);
  //! destructor
  virtual ~MSKText();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;

protected:

  //! sets the listener in this class and in the helper classes
  void setListener(MSKContentListenerPtr listen) {
    m_listener = listen;
  }

  //! finds the different text zones
  bool createZones();

  // reads the header/footer string : version v1-2
  std::string readHeaderFooterString(bool header);

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //! send a zone ( 0: mainZone)
  void sendZone(int zoneId=0);

  //! returns a header zone id ( or -1 )
  int getHeader() const;

  //! returns a footer zone id ( or -1 )
  int getFooter() const;

  //! return the lines and pages height ( for v1, ...)
  bool getLinesPagesHeight(std::vector<int> &lines,
                           std::vector<int> &pages);

  //
  // low level
  //

  //! try to read a zone header
  bool readZoneHeader(MSKTextInternal::LineZone &zone) const;

  //! prepare a zone
  void update(MSKTextInternal::TextZone &zone);

  //! sends the zone data to the listener
  void send(MSKTextInternal::TextZone &zone);

  //! tries to read a font
  bool readFont(MSKTextInternal::Font &font, long endPos);

  //! send the font properties
  void setProperty(MSKTextInternal::Font const &font);

  //! tries to read a paragraph
  bool readParagraph(MSKTextInternal::LineZone &zone, MSKTextInternal::Paragraph &parag);

  //! send the paragraph properties
  void setProperty(MSKTextInternal::Paragraph const &para);

  //! tries to send a text zone
  bool sendText(MSKTextInternal::LineZone &zone);

  //! tries to send a string (for v1-2, header/footer zone)
  bool sendString(std::string &str);

  //! returns the debug file
  libmwaw::DebugFile &ascii() {
    return m_asciiFile;
  }

private:
  MSKText(MSKText const &orig);
  MSKText &operator=(MSKText const &orig);

protected:
  //
  // data
  //
  //! the input
  MWAWInputStreamPtr m_input;

  //! the listener
  MSKContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<MSKTextInternal::State> m_state;

  //! the main parser;
  MSKParser *m_mainParser;

  //! the debug file
  libmwaw::DebugFile &m_asciiFile;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
