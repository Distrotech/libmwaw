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
 * Parser to convert HanMac Word-J document
 */
#ifndef HAN_MAC_WRD_J_PARSER
#  define HAN_MAC_WRD_J_PARSER

#include <iostream>
#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "MWAWDebug.hxx"

#include "MWAWParser.hxx"

namespace HanMacWrdJParserInternal
{
struct State;
class SubDocument;
}

class HanMacWrdJGraph;
class HanMacWrdJText;

/** a class use to store the classic header find before file zone */
struct HanMacWrdJZoneHeader {
  //! constructor
  HanMacWrdJZoneHeader(bool isMain) : m_length(0), m_n(0), m_fieldSize(0), m_id(0), m_isMain(isMain)
  {
    for (int i=0; i < 4; i++) m_values[i] = 0;
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, HanMacWrdJZoneHeader const &h)
  {
    if (h.m_n) o << "N=" << h.m_n << ",";
    if (h.m_id) o << "zId=" << std::hex << h.m_id << std::dec << ",";
    bool toPrint[4]= {true, true, true, true};
    if (h.m_isMain) {
      if (h.m_values[0]+h.m_n == h.m_values[1])
        toPrint[0]=toPrint[1]=false;
      else if (h.m_values[0]+h.m_n == h.m_values[2])
        toPrint[0]=toPrint[2]=false;
      else
        o << "###N,";
    }
    for (int i=0; i < 4; i++)
      if (toPrint[i] && h.m_values[i]) o << "h" << i << "=" << h.m_values[i] << ",";
    return o;
  }
  //! the zone size
  long m_length;
  //! the number of item
  int m_n;
  //! the field size
  int m_fieldSize;
  //! the zone id
  long m_id;
  //! 4 unknown field : freeN?, totalN?, totalN1?, ?
  int m_values[4];
  //! true if this is the main header
  bool m_isMain;
};

/** \brief the main class to read a HanMac Word-J file
 *
 *
 *
 */
class HanMacWrdJParser : public MWAWTextParser
{
  friend class HanMacWrdJGraph;
  friend class HanMacWrdJText;
  friend class HanMacWrdJParserInternal::SubDocument;

public:
  //! constructor
  HanMacWrdJParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~HanMacWrdJParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(librevenge::RVNGTextInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! creates the listener which will be associated to the document
  void createDocument(librevenge::RVNGTextInterface *documentInterface);

  //! finds the different objects zones in a Hapanese File
  bool createZones();

  //! returns the page left top point ( in inches)
  Vec2f getPageLeftTop() const;

  //! adds a new page
  void newPage(int number);

  // interface with the text parser

  //! send a text zone
  bool sendText(long id, long cPos, bool asGraphic=false);
  //! check if we can send a textzone as graphic
  bool canSendTextAsGraphic(long id, long cPos);

  // interface with the graph parser

  //! send a zone
  bool sendZone(long zId);
  //! returns the color associated with a pattern
  bool getColor(int colId, int patternId, MWAWColor &color) const;

  //
  // low level
  //

  /** look in entry.begin() to see if a entry exists at this position,
      if so fills entry.end(), entry.id(), ...*/
  bool checkEntry(MWAWEntry &entry);

  /** try to read the zones list */
  bool readZonesList();
  /** try to read a generic zone */
  bool readZone(MWAWEntry &entry);

  /** try to read a header of classic zone */
  bool readClassicHeader(HanMacWrdJZoneHeader &header, long endPos=-1);
  /** try to decode a zone */
  bool decodeZone(MWAWEntry const &entry, librevenge::RVNGBinaryData &data);

  /** try to read a printinfo zone*/
  bool readPrintInfo(MWAWEntry const &entry);
  /** try to read a unknown zone, just after the header (simillar to HanMacWrd Zoneb) */
  bool readHeaderEnd();
#ifdef DEBUG
  /** try to read a unknown zones with header data */
  bool readZoneWithHeader(MWAWEntry const &entry);
#endif
  /** try to read the zone A ( a big zone containing 5 sub zone ? )*/
  bool readZoneA(MWAWEntry const &entry);
  /** try to read the zone B*/
  bool readZoneB(MWAWEntry const &entry);

protected:
  //
  // data
  //
  //! the state
  shared_ptr<HanMacWrdJParserInternal::State> m_state;

  //! the graph parser
  shared_ptr<HanMacWrdJGraph> m_graphParser;

  //! the text parser
  shared_ptr<HanMacWrdJText> m_textParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
