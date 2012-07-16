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
 * parser for WriteNow 3.0 and 4.0
 *
 * Note: WriteNow 2.0 seems very different
 */
#ifndef WN_MWAW_PARSER
#  define WN_MWAW_PARSER

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

typedef class MWAWContentListener WNContentListener;
typedef shared_ptr<WNContentListener> WNContentListenerPtr;

class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

namespace WNParserInternal
{
struct State;
class SubDocument;
}

struct WNEntry;
struct WNEntryManager;

class WNText;

/** \brief the main class to read a WriteNow file
 *
 *
 *
 */
class WNParser : public MWAWParser
{
  friend class WNText;
  friend class WNParserInternal::SubDocument;

public:
  //! constructor
  WNParser(MWAWInputStreamPtr input, MWAWHeader *header);
  //! destructor
  virtual ~WNParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(WPXDocumentInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! sets the listener in this class and in the helper classes
  void setListener(WNContentListenerPtr listen);

  //! creates the listener which will be associated to the document
  void createDocument(WPXDocumentInterface *documentInterface);

  //! finds the different objects zones
  bool createZones();

  //! returns the page height, ie. paper size less margin (in inches)
  float pageHeight() const;
  //! returns the page width, ie. paper size less margin (in inches)
  float pageWidth() const;

  //! returns the columns information
  void getColumnInfo(int &numColumns, int &width) const;

  //! adds a new page
  void newPage(int number);

  /*
   * interface with WNText
   */
  //! returns the color which corresponds to colId
  bool getColor(int colId, Vec3uc &col) const;

  //! try to send a footnote entry
  void sendFootnote(WNEntry const &entry);

  //! try to send the graphic zone
  bool sendGraphic(int gId, Box2i const &bdbox);

  /*
   * interface with subdocument
   */

  //! try to send an entry
  void send(WNEntry const &entry);

  //
  // low level
  //

  //! try to read the document entries zone v3-v4
  bool readDocEntries();

  //! try to read the document entries zone v2
  bool readDocEntriesV2();

  /** try to read the graphic zone (unknown + list of entries )
      and to create the graphic data zone
   */
  bool parseGraphicZone(WNEntry const &entry);

  //! try to read the colormap zone
  bool readColorMap(WNEntry const &entry);

  //! try to read the print info zone
  bool readPrintInfo(WNEntry const &entry);

  //! try to read the last generic zones
  bool readGenericUnkn(WNEntry const &entry);

  //! try to send a picture to the listener
  bool sendPicture(WNEntry const &entry, Box2i const &bdbox);

  //! read a file entry
  WNEntry readEntry();

  //! check if a position is inside the file
  bool checkIfPositionValid(long pos);

protected:
  //
  // data
  //
  //! the listener
  WNContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<WNParserInternal::State> m_state;

  //! the list of entry
  shared_ptr<WNEntryManager> m_entryManager;

  //! the actual document size
  MWAWPageSpan m_pageSpan;

  //! the text parser
  shared_ptr<WNText> m_textParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
