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

#ifndef RAG_TIME_PARSER
#  define RAG_TIME_PARSER

#include <set>
#include <vector>

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

namespace RagTimeParserInternal
{
struct Picture;
struct State;
class SubDocument;
}

class RagTimeText;
class RagTimeSpreadsheet;

/** \brief the main class to read a RagTime v2-3 file
 *
 *
 *
 */
class RagTimeParser : public MWAWTextParser
{
  friend class RagTimeParserInternal::SubDocument;
  friend class RagTimeText;
  friend class RagTimeSpreadsheet;

public:
  //! constructor
  RagTimeParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~RagTimeParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(librevenge::RVNGTextInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //
  // interface
  //

  //! returns the ith color ( if possible)
  bool getColor(int colId, MWAWColor &color, int listId=-1) const;
  //! returns the ith date format or ""
  bool getDateTimeFormat(int dtId, std::string &dtFormat) const;
  //! returns the size of a zone data field
  int getZoneDataFieldSize(int zId) const;
  //! returns a new unique zone id
  int getNewZoneId();

  //
  // interface with text parser
  //

  //! returns a mac font id corresponding to a local id
  int getFontId(int localId) const;
  //! returns font style corresponding to a char style id
  bool getCharStyle(int charId, MWAWFont &font) const;
  //! try to read a text zone (knowing the zone width in point and the font color)
  bool readTextZone(MWAWEntry &entry, int width, MWAWColor const &fontColor=MWAWColor::black());

  //! creates the listener which will be associated to the document
  void createDocument(librevenge::RVNGTextInterface *documentInterface);
  //! try to send the different zones
  bool sendZones();

  //! adds a new page
  void newPage(int number);

  //! finds the different objects zones
  bool createZones();

  //! try to create the main data zones list
  bool findDataZones();
  //! try to read a data zone header
  bool readDataZoneHeader(int id, long endPos);
  //! try to sort the zone on each page
  bool findPagesZones();

  //! try to create the resource zones list
  bool findRsrcZones();

  //! try to send the different zones of a page
  bool sendPageZone(int page);
  //! try to send a zone
  bool send(int zId);
  //! try to send the text zone (interface of RagTimeText)
  bool sendText(int zId, MWAWListenerPtr listener);

  //! sends the picture
  bool sendPicture(int zId, MWAWPosition const &pos);
  //! sends a bitmap
  bool sendBitmap(RagTimeParserInternal::Picture const &pict, MWAWPosition const &pos);
  //! sends a basic shape
  bool sendBasicPicture(int zId, MWAWPosition const &pos);
  //! flush unsent zone (debugging function)
  void flushExtra();

  /** try to read page zone ( unknown content of size 40). A zone
      which seems to appear between each page data */
  bool readPageZone(MWAWEntry &entry);

  //! try to read pictZone ( a big zone)
  bool readPictZone(MWAWEntry &entry);

  //! try to read pictZone ( a big zone):v2
  bool readPictZoneV2(MWAWEntry &entry);

  // some rsrc zone

  //! try to read the color map:v2
  bool readColorMapV2(MWAWEntry &entry);
  //! try to read the color map:v3
  bool readColorsMap();
  //! read a printInfo block (a PREC rsrc)
  bool readPrintInfo(MWAWEntry &entry, bool inRSRCFork=false);
  /** try to read the File Link zone: FLink */
  bool readLinks(MWAWEntry &entry);
  //! try to read the format map:v3
  bool readFormatsMap();
  //! try to read the macro format zone: RTml zones
  bool readMacroFormats(MWAWEntry &entry);
  //! try to read the color table zone: CHTa zones
  bool readColorTable(MWAWEntry &entry);

  // unknown data fork zone

  //! try to read zone6 ( a big zone)
  bool readZone6(MWAWEntry &entry);

  // unknown rsrc fork zone
  //! try to read the BeDc zone ( zone of size 52, one by file with id=0);
  bool readRsrcBeDc(MWAWEntry &entry);

  //! try to read a structured zone
  bool readRsrcStructured(MWAWEntry &entry);

  //! try to read the Btch zone (zone with id=0)
  bool readRsrcBtch(MWAWEntry &entry);
  //! try to read the Calc zone (zone with id=0)
  bool readRsrcCalc(MWAWEntry &entry);
  //! try to read the fppr zone (zone with id=0)
  bool readRsrcfppr(MWAWEntry &entry);
  //! try to read the Sele zone (zone with id=0), maybe related to selection
  bool readRsrcSele(MWAWEntry &entry);
  // maybe FH=footer/header zone
  //! try to read the FHwl zone ( one by file with id=0), maybe width length?
  bool readRsrcFHwl(MWAWEntry &entry);
protected:
  //
  // data
  //
  //! the state
  shared_ptr<RagTimeParserInternal::State> m_state;
  //! the spreadsheet parser
  shared_ptr<RagTimeSpreadsheet> m_spreadsheetParser;
  //! the text parser
  shared_ptr<RagTimeText> m_textParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
