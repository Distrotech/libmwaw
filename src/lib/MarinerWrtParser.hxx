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
 * Parser to convert Mariner Write document
 */
#ifndef MARINER_WRT_PARSER
#  define MARINER_WRT_PARSER

#include <iostream>
#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "MWAWDebug.hxx"
#include "MWAWEntry.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

namespace MarinerWrtParserInternal
{
struct State;
class SubDocument;
}

class MarinerWrtGraph;
class MarinerWrtText;

//! a entry to store a zone structure
struct MarinerWrtEntry : public MWAWEntry {
  //! constructor
  MarinerWrtEntry() : MWAWEntry(), m_fileType(0), m_N(0), m_value(0)
  {
  }
  //! returns the entry name;
  std::string name() const;
  //! operator<<
  friend std::ostream &operator<< (std::ostream &o, MarinerWrtEntry const &ent)
  {
    if (ent.m_N || ent.m_value || ent.m_extra.length()) {
      o << "[";
      if (ent.m_N) o << "N=" << ent.m_N << ",";
      if (ent.m_value) o << "unkn=" << ent.m_value << ",";
      if (ent.m_extra.length()) o << ent.m_extra;
      o << "],";
    }
    return o;
  }
  //! the entry type
  int m_fileType;
  //! the number of value
  int m_N;
  //! a unknown value
  int m_value;
};

//! Internal: a struct used to read some field
struct MarinerWrtStruct {
  //! constructor
  MarinerWrtStruct() : m_filePos(-1), m_pos(), m_type(-1), m_data()
  {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, MarinerWrtStruct const &dt);
  //! returns the number of values
  int numValues() const
  {
    return int(m_data.size());
  }
  //! returns true if this corresponds to a simple container
  bool isBasic() const
  {
    return (m_type==1 || m_type==2) && m_data.size()<=1;
  }
  //! returns the ith value (or 0 if it does not exists )
  long value(int i) const;
  //! the file position where the field description begin
  long m_filePos;
  //! the file data position (for type=0 data )
  MWAWEntry m_pos;
  //! the data type
  int m_type;
  //! the data block
  std::vector<long> m_data;
};

/** \brief the main class to read a Mariner Write file
 *
 *
 *
 */
class MarinerWrtParser : public MWAWTextParser
{
  friend class MarinerWrtGraph;
  friend class MarinerWrtText;
  friend class MarinerWrtParserInternal::SubDocument;

public:
  //! constructor
  MarinerWrtParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~MarinerWrtParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(librevenge::RVNGTextInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! creates the listener which will be associated to the document
  void createDocument(librevenge::RVNGTextInterface *documentInterface);

  //! finds the different objects zones
  bool createZones();
  /** try to read a zone */
  bool readZone(int &actZone, bool onlyTest=false);

  //! returns the page left top point ( in inches)
  Vec2f getPageLeftTop() const;
  //! returns the section information corresponding to a zone
  MWAWSection getSection(int zoneId) const;

  //! adds a new page
  void newPage(int number);

  // interface with the text parser

  //! return a zoneid corresponding to a fileId (or -1) and set the endnote flag
  int getZoneId(uint32_t fileId, bool &endNote);
  //! ask the text parser to send a zone
  void sendText(int zoneId);

  // interface with the graph parser
  //! return the pattern percent which corresponds to an id (or -1)
  float getPatternPercent(int id) const;
  //! ask the graph parser to send a token
  void sendToken(int zoneId, long tokenId);

  //
  // low level
  //

  /** try to read an entry header */
  bool readEntryHeader(MarinerWrtEntry &entry);
  /** try to decode a zone */
  bool decodeZone(std::vector<MarinerWrtStruct> &dataList, long numData=999999);

  /** try to read the separator of differents part */
  bool readSeparator(MarinerWrtEntry const &entry);
  /** try to read the zone dimension ( normal and with margin ) */
  bool readZoneDim(MarinerWrtEntry const &entry, int zoneId);
  /** try to read the zone header */
  bool readZoneHeader(MarinerWrtEntry const &entry, int zoneId, bool onlyTest);
  /** try to read a unknown zone : one by separator?, borderdim? */
  bool readZoneb(MarinerWrtEntry const &entry, int zoneId);
  /** try to read a unknown zone of 9 int*/
  bool readZonec(MarinerWrtEntry const &entry, int zoneId);
  /** try to read a unknown zone of 23 int*/
  bool readZone13(MarinerWrtEntry const &entry, int zoneId);
  /** try to read the doc info zone */
  bool readDocInfo(MarinerWrtEntry const &entry, int zoneId);
  /** try to read a printinfo zone */
  bool readPrintInfo(MarinerWrtEntry const &entry);
  /** try to read a xml printinfo zone */
  bool readCPRT(MarinerWrtEntry const &entry);

  /** try to read a number or a list of number entries */
  bool readNumbersString(int num, std::vector<long> &res);

protected:
  //
  // data
  //
  //! the state
  shared_ptr<MarinerWrtParserInternal::State> m_state;

  //! a flag to know if page margins span are set
  bool  m_pageMarginsSpanSet;

  //! the graph parser
  shared_ptr<MarinerWrtGraph> m_graphParser;

  //! the text parser
  shared_ptr<MarinerWrtText> m_textParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
