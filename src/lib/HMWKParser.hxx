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
#ifndef HMWK_PARSER
#  define HMWK_PARSER

#include <iostream>
#include <string>
#include <vector>

#include <libwpd/libwpd.h>

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"
#include "MWAWPageSpan.hxx"

#include "MWAWParser.hxx"

class MWAWEntry;
class MWAWFont;
class MWAWParagraph;

namespace HMWKParserInternal
{
struct State;
class SubDocument;
}

//! Small class used to store the decoded zone of HMWKParser
struct HMWKZone {
  //! constructor given an input and an asciiFile
  HMWKZone(MWAWInputStreamPtr input, libmwaw::DebugFile &asciiFile);
  //! constructor given an asciiFile (used for compressed zone)
  HMWKZone(shared_ptr<libmwaw::DebugFile> asciiFile);
  //! destructor
  ~HMWKZone();

  //! returns the first position in the input
  long begin() const {
    return m_asciiFilePtr ? 0 : m_filePos;
  }
  //! returns the last position in the input
  long end() const {
    return m_asciiFilePtr ? (long) m_data.size() : m_endFilePos;
  }
  //! returns the zone size
  long length() const {
    if (m_asciiFilePtr) return (long) m_data.size();
    return m_endFilePos-m_filePos;
  }
  //! returns true if the zone data exists
  bool valid() const {
    return length() > 0;
  }

  // function to define the zone in the original file

  //! returns the file begin position
  long fileBeginPos() const {
    return m_filePos;
  }
  //! returns the file begin position
  long fileEndPos() const {
    return m_endFilePos;
  }
  //! sets the begin file pos
  void setFileBeginPos(long begPos) {
    m_filePos = m_endFilePos = begPos;
  }
  //! sets the file length
  void setFileLength(long len) {
    m_endFilePos = m_filePos+len;
  }
  //! sets the begin/end file pos
  void setFilePositions(long begPos, long endPos) {
    m_filePos = begPos;
    m_endFilePos = endPos;
  }
  //! returns a pointer to the binary data
  WPXBinaryData &getBinaryData() {
    return m_data;
  }
  //! returns the zone name
  std::string name() const {
    return name(m_type);
  }
  //! returns the zone name
  static std::string name(int type);

  //! operator <<
  friend std::ostream &operator<<(std::ostream &o, HMWKZone const &zone);

  //! returns the debug file
  libmwaw::DebugFile &ascii() {
    return *m_asciiFile;
  }

  //! the type : 1(text), ....
  int m_type;

  //! the zone id
  long m_id;

  //! the zone subId
  long m_subId;

  //! the main input
  MWAWInputStreamPtr m_input;

  //! some extra data
  std::string m_extra;

  //! true if the zone is sended
  mutable bool m_parsed;

protected:
  //! the begin of the entry
  long m_filePos;

  //! the end of the entry
  long m_endFilePos;

  //! the storage (if needed)
  WPXBinaryData m_data;

  //! the debug file
  libmwaw::DebugFile *m_asciiFile;

  //! the file pointer
  shared_ptr<libmwaw::DebugFile> m_asciiFilePtr;

private:
  HMWKZone(HMWKZone const &orig);
  HMWKZone &operator=(HMWKZone const &orig);
};

class HMWKGraph;
class HMWKText;

/** \brief the main class to read a HanMac Word-K file
 *
 *
 *
 */
class HMWKParser : public MWAWParser
{
  friend class HMWKGraph;
  friend class HMWKText;
  friend class HMWKParserInternal::SubDocument;

public:
  //! constructor
  HMWKParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~HMWKParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(WPXDocumentInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! creates the listener which will be associated to the document
  void createDocument(WPXDocumentInterface *documentInterface);

  //! finds the different objects zones
  bool createZones();

  //! returns the page height, ie. paper size less margin (in inches)
  float pageHeight() const;
  //! returns the page width, ie. paper size less margin (in inches)
  float pageWidth() const;
  //! returns the page left top point ( in inches)
  Vec2f getPageLeftTop() const;

  //! adds a new page
  void newPage(int number);

  // interface with the text parser

  //! send a text zone (not implemented)
  bool sendText(long id, long subId=0);

  // interface with the graph parser

  //! send a zone
  bool sendZone(long zId);
  //! returns the color associated with a pattern
  bool getColor(int colId, int patternId, MWAWColor &color) const;

  //
  // low level
  //

  /** try to read the zones list */
  bool readZonesList();
  /** try to read a generic zone */
  bool readZone(shared_ptr<HMWKZone> zone);
  /** try to decode a zone */
  shared_ptr<HMWKZone> decodeZone(shared_ptr<HMWKZone> zone);
  /** try to read a zone storing a list of ?, frameType*/
  bool readFramesUnkn(shared_ptr<HMWKZone> zone);
  /** try to read a printinfo zone (type 7)*/
  bool readPrintInfo(HMWKZone &zone);
  /** try to read a unknown zone of type 6*/
  bool readZone6(shared_ptr<HMWKZone> zone);
  /** try to read a unknown zone of type 8*/
  bool readZone8(shared_ptr<HMWKZone> zone);
  /** try to read a unknown zone of type a*/
  bool readZonea(shared_ptr<HMWKZone> zone);
  /** try to read a unknown zone of type b*/
  bool readZoneb(HMWKZone &zone);
  /** try to read a unknown zone of type c*/
  bool readZonec(shared_ptr<HMWKZone> zone);
  /** check if an entry is in file */
  bool isFilePos(long pos);

protected:
  //
  // data
  //
  //! the state
  shared_ptr<HMWKParserInternal::State> m_state;

  //! the actual document size
  MWAWPageSpan m_pageSpan;

  //! the graph parser
  shared_ptr<HMWKGraph> m_graphParser;

  //! the text parser
  shared_ptr<HMWKText> m_textParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
