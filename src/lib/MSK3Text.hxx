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
#ifndef MSK3_TEXT
#  define MSK3_TEXT

#include <list>
#include <string>
#include <vector>

#include "libmwaw_internal.hxx"

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

namespace MSK3TextInternal
{
struct Font;
struct Paragraph;
struct LineZone;
struct TextZone;
struct State;
}

class MSK3Parser;

/** \brief the main class to read the text part of Microsoft Works file
 *
 *
 *
 */
class MSK3Text
{
  friend class MSK3Parser;
public:
  //! constructor
  MSK3Text(MWAWInputStreamPtr ip, MSK3Parser &parser, MWAWFontConverterPtr &convertissor);
  //! destructor
  virtual ~MSK3Text();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages(int zoneId) const;

protected:

  //! sets the listener in this class and in the helper classes
  void setListener(MSKContentListenerPtr listen) {
    m_listener = listen;
  }

  //! finds the different text zones. Returns the zoneId or -1.
  int createZones(int numLines=-1, bool mainZone=false);

  // reads the header/footer string : version v1-2
  std::string readHeaderFooterString(bool header);

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //! send a zone
  void sendZone(int zoneId);

  //! send a note
  void sendNote(int zoneId, int noteId);

  //! returns a main zone id
  int getMainZone() const;

  //! returns a header zone id ( or -1 )
  int getHeader() const;

  //! returns a footer zone id ( or -1 )
  int getFooter() const;

  //! return the lines and pages height ( for v1, ...)
  bool getLinesPagesHeight(int zoneId,
                           std::vector<int> &lines,
                           std::vector<int> &pages);

  //
  // low level
  //

  //! try to read a zone header
  bool readZoneHeader(MSK3TextInternal::LineZone &zone) const;

  //! prepare a zone
  void update(MSK3TextInternal::TextZone &zone);

  //! prepare the note zones given a zone and the position of the first note
  void updateNotes(MSK3TextInternal::TextZone &zone, int firstNote);

  /** sends the zone data to the listener. You can set limit to send
      a subzone data ( like note ) */
  void send(MSK3TextInternal::TextZone &zone, Vec2i limit=Vec2i(-1,-1));

  //! tries to read a font
  bool readFont(MSK3TextInternal::Font &font, long endPos);

  //! send the font properties
  void setProperty(MSK3TextInternal::Font const &font);

  //! tries to read a paragraph
  bool readParagraph(MSK3TextInternal::LineZone &zone, MSK3TextInternal::Paragraph &parag);

  //! send the paragraph properties
  void setProperty(MSK3TextInternal::Paragraph const &para);

  //! tries to send a text zone
  bool sendText(MSK3TextInternal::LineZone &zone, int zoneId);

  //! tries to send a string (for v1-2, header/footer zone)
  bool sendString(std::string &str);

  //! returns the debug file
  libmwaw::DebugFile &ascii() {
    return m_asciiFile;
  }

private:
  MSK3Text(MSK3Text const &orig);
  MSK3Text &operator=(MSK3Text const &orig);

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
  shared_ptr<MSK3TextInternal::State> m_state;

  //! the main parser;
  MSK3Parser *m_mainParser;

  //! the debug file
  libmwaw::DebugFile &m_asciiFile;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
