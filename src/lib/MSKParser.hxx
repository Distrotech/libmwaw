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

#ifndef MSK_MWAW_PARSER
#  define MSK_MWAW_PARSER

#include <list>
#include <string>
#include <vector>

#include "MWAWPageSpan.hxx"

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

namespace MSKParserInternal
{
struct State;
class SubDocument;
}

class MSKGraph;
class MSKText;

/** \brief the main class to read a Microsoft Works file
 *
 *
 *
 */
class MSKParser : public MWAWParser
{
  friend class MSKParserInternal::SubDocument;
  friend class MSKGraph;
  friend class MSKText;
public:
  //! constructor
  MSKParser(MWAWInputStreamPtr input, MWAWHeader *header);
  //! destructor
  virtual ~MSKParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  /** returns the file version.
   *
   * this version is only correct after the header is parsed */
  int version() const;

  // the main parse function
  void parse(WPXDocumentInterface *documentInterface);

  //! Debugging: change the default ascii file
  void setAsciiName(char const *name) {
    m_asciiName = name;
  }

protected:
  //! inits all internal variables
  void init();

  //! sets the listener in this class and in the helper classes
  void setListener(MSKContentListenerPtr listen);

  //! creates the listener which will be associated to the document
  void createDocument(WPXDocumentInterface *documentInterface);

  //! finds the different objects zones
  bool createZones();

  //! returns the page height, ie. paper size less margin (in inches)
  float pageHeight() const;
  //! returns the page width, ie. paper size less margin (in inches)
  float pageWidth() const;

  //! returns the page top left point
  Vec2f getPageTopLeft() const;

  //! adds a new page
  void newPage(int number, bool softBreak=false);
  //
  // intermediate level
  //

  //! return the color which correspond to an index
  bool getColor(int id, Vec3uc &col) const;

  //! try to read the documentinfo ( zone2)
  bool readDocumentInfo();
  //! try to read a group zone (zone3)
  bool readGroup(MWAWEntry &zone, int check);
  //! try to send a textbox
  bool sendTextBox(int id, MWAWPosition const &pos, WPXPropertyList &extras);

  /** try to send an entry

  \note If id >= 0, the text parser zone id. If id < 0, the graph parser zone (-id-1)
   */
  void send(int id);

  //
  // low level
  //

  //! read the print info zone
  bool readPrintInfo();

  //! check if a position is inside the file
  bool checkIfPositionValid(long pos);

  //! returns the debug file
  libmwaw::DebugFile &ascii() {
    return m_asciiFile;
  }

  //! return the ascii file name
  std::string const &asciiName() const {
    return m_asciiName;
  }

protected:
  //
  // data
  //
  //! the listener
  MSKContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<MSKParserInternal::State> m_state;

  //! the actual document size
  MWAWPageSpan m_pageSpan;

  //! the list of different Zones
  std::vector<MWAWEntry> m_listZones;

  //! the graph parser
  shared_ptr<MSKGraph> m_graphParser;

  //! the text parser
  shared_ptr<MSKText> m_textParser;

  //! the debug file
  libmwaw::DebugFile m_asciiFile;

  //! the debug file name
  std::string m_asciiName;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
