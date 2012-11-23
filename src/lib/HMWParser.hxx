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
 * Parser to convert HanMac Word-K document
 */
#ifndef HMW_PARSER
#  define HMW_PARSER

#include <iostream>
#include <string>
#include <vector>

#include <libwpd/libwpd.h>

#include "MWAWPageSpan.hxx"

#include "MWAWContentListener.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

typedef class MWAWContentListener HMWContentListener;
typedef shared_ptr<HMWContentListener> HMWContentListenerPtr;
class MWAWEntry;
class MWAWFont;
class MWAWParagraph;
class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

namespace HMWParserInternal
{
struct State;
class SubDocument;
}

//! Small class used to store the decoded zone of HMWParser
struct HMWZone {
  //! constructor
  HMWZone();
  //! destructor
  ~HMWZone();

  //! return true if the zone data exists
  bool valid() const {
    return m_data.size() > 0;
  }

  //! returns the zone name
  std::string name() const {
    return name(m_type);
  }
  //! returns the zone name
  static std::string name(int type);

  //! operator <<
  friend std::ostream &operator<<(std::ostream &o, HMWZone const &zone);

  //! returns the debug file
  libmwaw::DebugFile &ascii() {
    return m_asciiFile;
  }

  //! the type : 1(text), ....
  int m_type;

  //! the zone id
  long m_id;

  //! the begin of the entry
  long m_filePos;

  //! the end of the entry
  long m_endFilePos;

  //! the storage
  WPXBinaryData m_data;

  //! the main input
  MWAWInputStreamPtr m_input;

  //! the debug file
  libmwaw::DebugFile m_asciiFile;

  //! some extra data
  std::string m_extra;

  //! true if the zone is sended
  bool m_parsed;
};

class HMWText;

/** \brief the main class to read a HanMac Word-K file
 *
 *
 *
 */
class HMWParser : public MWAWParser
{
  friend class HMWText;
  friend class HMWParserInternal::SubDocument;

public:
  //! constructor
  HMWParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~HMWParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(WPXDocumentInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! sets the listener in this class and in the helper classes
  void setListener(HMWContentListenerPtr listen);

  //! creates the listener which will be associated to the document
  void createDocument(WPXDocumentInterface *documentInterface);

  //! finds the different objects zones
  bool createZones();

  //! returns the page height, ie. paper size less margin (in inches)
  float pageHeight() const;
  //! returns the page width, ie. paper size less margin (in inches)
  float pageWidth() const;

  //! adds a new page
  void newPage(int number);

  //
  // low level
  //

  /** try to read the zones list */
  bool readZonesList();
  /** try to read a generic zone */
  bool readZone(shared_ptr<HMWZone> zone);
  /** try to decode a zone */
  shared_ptr<HMWZone> decodeZone(shared_ptr<HMWZone> zone);
  /** try to read the frame definition (type 2)*/
  bool readFrames(shared_ptr<HMWZone> zone);
  /** try to read a zone storing a list of ?, frameType*/
  bool readFramesUnkn(shared_ptr<HMWZone> zone);
  /** try to read a picture zone (type d)*/
  bool readPicture(shared_ptr<HMWZone> zone);
  /** try to read a section info zone (type 4)*/
  bool readSections(shared_ptr<HMWZone> zone);
  /** try to read a printinfo zone (type 7)*/
  bool readPrintInfo(shared_ptr<HMWZone> zone);
  /** try to read a unknown zone of type 6*/
  bool readZone6(shared_ptr<HMWZone> zone);
  /** try to read a unknown zone of type 8*/
  bool readZone8(shared_ptr<HMWZone> zone);
  /** try to read a unknown zone of type a*/
  bool readZonea(shared_ptr<HMWZone> zone);
  /** try to read a unknown zone of type b*/
  bool readZoneb(shared_ptr<HMWZone> zone);
  /** try to read a unknown zone of type c*/
  bool readZonec(shared_ptr<HMWZone> zone);
  /** check if an entry is in file */
  bool isFilePos(long pos);

protected:
  //
  // data
  //
  //! the listener
  HMWContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<HMWParserInternal::State> m_state;

  //! the actual document size
  MWAWPageSpan m_pageSpan;

  //! the text parser
  shared_ptr<HMWText> m_textParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
