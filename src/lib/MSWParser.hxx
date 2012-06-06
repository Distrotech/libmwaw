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
 * parser for Microsoft Word ( version 4.0-5.1 )
 */
#ifndef MSW_MWAW_PARSER
#  define MSW_MWAW_PARSER

#include <list>
#include <map>
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

typedef class MWAWContentListener MSWContentListener;
typedef shared_ptr<MSWContentListener> MSWContentListenerPtr;

class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

namespace MSWParserInternal
{
struct Object;
struct State;
class SubDocument;
}

class MSWText;
class MSWTextStyles;

//! the entry of MSWParser
struct MSWEntry : public MWAWEntry {
  MSWEntry() : MWAWEntry(), m_textId(-1) {
  }
  /** \brief returns the text id
   *
   * This field is used to differentiate main text, header, ...)
   */
  int textId() const {
    return m_textId;
  }
  //! sets the text id
  void setTextId(int newId) {
    m_textId = newId;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, MSWEntry const &entry);
  //! the text identificator
  int m_textId;
};

/** \brief the main class to read a Microsoft Word file
 *
 *
 *
 */
class MSWParser : public MWAWParser
{
  friend class MSWText;
  friend class MSWTextStyles;
  friend class MSWParserInternal::SubDocument;

public:
  //! constructor
  MSWParser(MWAWInputStreamPtr input, MWAWHeader *header);
  //! destructor
  virtual ~MSWParser();

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
  void setListener(MSWContentListenerPtr listen);

  //! creates the listener which will be associated to the document
  void createDocument(WPXDocumentInterface *documentInterface);

  //! finds the different zones
  bool createZones();

  //! read the list of zones
  bool readZoneList();

  //! read the print info zone
  bool readPrintInfo(MSWEntry &entry);

  //! read the printer name
  bool readPrinter(MSWEntry &entry);

  //! read the document sumary
  bool readDocSum(MSWEntry &entry);

  //! read a zone which consists in a list of string
  bool readStringsZone(MSWEntry &entry, std::vector<std::string> &list);

  //! read the objects
  bool readObjects();

  //! read the object list
  bool readObjectList(MSWEntry &entry);

  //! read the object flags
  bool readObjectFlags(MSWEntry &entry);

  //! read an object
  bool readObject(MSWParserInternal::Object &obj);

  //! read the page dimensions + ?
  bool readDocumentInfo(MSWEntry &entry);

  //! read the zone 17( some bdbox + text position ?)
  bool readZone17(MSWEntry &entry);

  //! check if a position corresponds or not to a picture entry
  bool checkPicturePos(long pos, int type);

  //! read a picture data
  bool readPicture(MSWEntry &entry);

  //! returns the page height, ie. paper size less margin (in inches)
  float pageHeight() const;
  //! returns the page width, ie. paper size less margin (in inches)
  float pageWidth() const;
  //! returns the color corresponding to an id
  bool getColor(int id, Vec3uc &col) const;

  //! adds a new page
  void newPage(int number);

  /*
   * interface with subdocument
   */
  //! try to send a footnote id
  void sendFootnote(int id);

  //! try to send a bookmark field id
  void sendFieldComment(int id);

  //! try to send a date
  void send(int id, libmwaw::SubDocumentType type);

  //
  // low level
  //

  /** check if an entry is in file */
  bool isFilePos(long pos);

  //! read a file entry
  MSWEntry readEntry(std::string type, int id=-1);

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
  MSWContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<MSWParserInternal::State> m_state;

  //! the list of entries
  std::multimap<std::string, MSWEntry> m_entryMap;

  //! the actual document size
  MWAWPageSpan m_pageSpan;

  //! the text parser
  shared_ptr<MSWText> m_textParser;

  //! the debug file
  libmwaw::DebugFile m_asciiFile;

  //! the debug file name
  std::string m_asciiName;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
