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
 * Parser to convert FullWrite document
 *
 */
#ifndef FP_MWAW_PARSER
#  define FP_MWAW_PARSER

#include <list>
#include <string>
#include <vector>

#include "DMWAWPageSpan.hxx"

#include "MWAWPosition.hxx"

#include "MWAWEntry.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

typedef class MWAWContentListener FWContentListener;
typedef shared_ptr<FWContentListener> FWContentListenerPtr;

class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

/** the definition of a zone in the file */
struct FWEntry : public MWAWEntry {
  FWEntry(MWAWInputStreamPtr input);
  ~FWEntry();

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, FWEntry const &entry);

  //! create a inputstream, ... if needed
  void update();
  //! write the debug file, ...
  void closeDebugFile();

  //! returns a reference to the ascii file
  libmwaw::DebugFile &getAsciiFile();
  //! basic operator==
  bool operator==(const FWEntry &a) const;
  //! basic operator!=
  bool operator!=(const FWEntry &a) const {
    return !operator==(a);
  }

  //! the input
  MWAWInputStreamPtr m_input;
  //! the flags definition id
  int m_flagsId;
  //! the next entry id
  int m_nextId;
  //! the type id
  int m_typeId;
  //! some unknown values
  int m_values[3];
  //! the main data ( if the entry comes from several zone )
  WPXBinaryData m_data;
  //! the debug file
  shared_ptr<libmwaw::DebugFile> m_asciiFile;
private:
  FWEntry(FWEntry const &);
  FWEntry &operator=(FWEntry const &);
};

namespace FWParserInternal
{
struct State;
struct Font;
class SubDocument;
}

class FWText;

/** \brief the main class to read a FullWrite file
 *
 *
 *
 */
class FWParser : public MWAWParser
{
  friend class FWText;
  friend class FWParserInternal::SubDocument;

public:
  //! constructor
  FWParser(MWAWInputStreamPtr input, MWAWHeader *header);
  //! destructor
  virtual ~FWParser();

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
  void setListener(FWContentListenerPtr listen);

  //! creates the listener which will be associated to the document
  void createDocument(WPXDocumentInterface *documentInterface);

  //! finds the different objects zones
  bool createZones();

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  //! read the print info zone
  bool readPrintInfo();

  //! returns the page height, ie. paper size less margin (in inches)
  float pageHeight() const;
  //! returns the page width, ie. paper size less margin (in inches)
  float pageWidth() const;

  //! adds a new page
  void newPage(int number);

  //! try to send a footnote/endnote entry
  void sendText(int id, MWAWSubDocumentType type, int which=0);

  //! find the last position of the document and read data
  bool readDocPosition();

  //! try to read the zones main flags
  bool readZoneFlags(shared_ptr<FWEntry> zone);

  //! try to read the zone position
  bool readZonePos(shared_ptr<FWEntry> zone);

  //! check if a zone is a graphic zone, ...
  bool readGraphic(shared_ptr<FWEntry> zone);

  //! send a graphic to a listener (if it exists)
  bool sendGraphic(shared_ptr<FWEntry> zone);

  //! check if a zone is a unknown zone, ...
  bool readUnkn0(shared_ptr<FWEntry> zone);

  //! check if a zone is the document information zone, ...
  bool readDocInfo(shared_ptr<FWEntry> zone);

  //
  // interface to the text parser
  //
  bool send(int zId);

  //
  // low level
  //

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
  FWContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<FWParserInternal::State> m_state;

  //! the actual document size
  DMWAWPageSpan m_pageSpan;

  //! the text parser
  shared_ptr<FWText> m_textParser;

  //! a list of created subdocuments
  std::vector<shared_ptr<FWParserInternal::SubDocument> > m_listSubDocuments;

  //! the debug file
  libmwaw::DebugFile m_asciiFile;

  //! the debug file name
  std::string m_asciiName;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
