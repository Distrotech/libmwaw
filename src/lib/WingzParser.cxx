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

#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWCell.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWSpreadsheetListener.hxx"
#include "MWAWSubDocument.hxx"

#include "WingzParser.hxx"

/** Internal: the structures of a WingzParser */
namespace WingzParserInternal
{
////////////////////////////////////////
//! Internal: the state of a WingzParser
struct State {
  //! constructor
  State() : m_numPages(0), m_headerHeight(0), m_footerHeight(0)
  {
  }

  int m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
WingzParser::WingzParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWSpreadsheetParser(input, rsrcParser, header), m_state()
{
  init();
}

WingzParser::~WingzParser()
{
}

void WingzParser::init()
{
  resetSpreadsheetListener();
  setAsciiName("main-1");

  m_state.reset(new WingzParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);
}


////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void WingzParser::parse(librevenge::RVNGSpreadsheetInterface *docInterface)
{
  assert(getInput().get() != 0);

  if (!checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = true;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());

    checkHeader(0L);
    ok = createZones();
    ascii().addPos(getInput()->tell());
    ascii().addNote("_");
    ok=false;
    if (ok) {
      createDocument(docInterface);
    }
  }
  catch (...) {
    MWAW_DEBUG_MSG(("WingzParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  ascii().reset();
  resetSpreadsheetListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void WingzParser::createDocument(librevenge::RVNGSpreadsheetInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getSpreadsheetListener()) {
    MWAW_DEBUG_MSG(("WingzParser::createDocument: listener already exist\n"));
    return;
  }

  // create the page list
  m_state->m_numPages = 1;
  MWAWPageSpan ps(getPageSpan());
  ps.setPageSpan(1);
  std::vector<MWAWPageSpan> pageList(1,ps);
  //
  MWAWSpreadsheetListenerPtr listen(new MWAWSpreadsheetListener(*getParserState(), pageList, documentInterface));
  setSpreadsheetListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool WingzParser::createZones()
{
  MWAWInputStreamPtr input = getInput();
  input->setReadInverted(true);
  input->seek(12, librevenge::RVNG_SEEK_SET);
  if (!readZoneA() || !readFontsList() || !readZoneB()) {
    ascii().addPos(input->tell());
    ascii().addNote("Entries(Loose)");
    if (!findNextZone())
      return false;
  }
  if (!readSpreadsheet()) return false;
  if (!input->isEnd()) {
    ascii().addPos(input->tell());
    ascii().addNote("Entries(Loose)");
  }
  return false;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read a font list
////////////////////////////////////////////////////////////
bool WingzParser::readFontsList()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  libmwaw::DebugStream f;
  f << "Entries(FontList):";
  int val=(int) input->readULong(1);
  if (val) f << "f0=" << val << ",";
  long sz=(long) input->readULong(1);
  long endPos=pos+sz;
  if (!input->checkPosition(pos+sz)) {
    MWAW_DEBUG_MSG(("WingzParser::readFontsList: the zone seems to short\n"));
    return false;
  }
  int N=(int) input->readULong(1);
  f << "N=" << N << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (int i=0; i < N; ++i) {
    f.str("");
    f << "FontList-" << i << ":";
    pos=input->tell();
    int fSz=(int) input->readULong(1);
    if (pos+1+fSz>endPos) {
      MWAW_DEBUG_MSG(("WingzParser::readFontsList: the %d font size seems bad\n", i));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      input->seek(endPos, librevenge::RVNG_SEEK_SET);
      return true;
    }
    std::string name("");
    for (int c=0; c<fSz; ++c) name+=(char) input->readULong(1);
    f << name;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  if (input->tell()!=endPos) {
    ascii().addPos(input->tell());
    ascii().addNote("ListFont-end");
    MWAW_DEBUG_MSG(("WingzParser::readFontsList: find extra data\n"));
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the some unknown zone
////////////////////////////////////////////////////////////

// the first unknown zone
bool WingzParser::readZoneA()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  if (!input->checkPosition(pos+176)) {
    MWAW_DEBUG_MSG(("WingzParser::readZoneA: the zone seems to short\n"));
    return false;
  }

  libmwaw::DebugStream f;
  f << "Entries(ZoneA):";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(pos+23, librevenge::RVNG_SEEK_SET);
  for (int i=0; i < 4; ++i) {
    pos=input->tell();
    f.str("");
    f << "ZoneA-" << i << ":";
    int const(sz[]) = { 42, 42, 30, 51};
    input->seek(pos+sz[i], librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

// the second unknown zone
bool WingzParser::readZoneB()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  if (!input->checkPosition(pos+237)) {
    MWAW_DEBUG_MSG(("WingzParser::readZoneB: the zone seems to short\n"));
    return false;
  }

  libmwaw::DebugStream f;
  f << "Entries(ZoneB):";
  int val=(int) input->readLong(2);
  if (val) f << "f0=" << val << ",";
  val=(int) input->readLong(2);
  if (val) f << "graph[num]=" << val << ",";
  for (int i=0; i < 10; ++i) { // f7,f8 related to page size?
    val=(int) input->readLong(2);
    if (val)
      f << "f" << i+1 << "=" << val << ",";
  }
  int sSz=(int) input->readULong(1);
  if (!input->checkPosition(pos+25+sSz)) {
    MWAW_DEBUG_MSG(("WingzParser::readZoneB: auto save name seems bad\n"));
    f << "####";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  std::string name("");
  for (int i=0; i < sSz; ++i)
    name += (char) input->readULong(1);
  f << name << ",";
  for (int i=0; i < 8; ++i) {
    val=(int) input->readLong(2);
    if (val)
      f << "g" << i << "=" << val << ",";
  }
  int dim[4];
  for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
  f << "select?=" << dim[0] << "x" << dim[1] << "<->" << dim[2] << "x" << dim[3] << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  ascii().addPos(pos);
  ascii().addNote("ZoneB-A");
  input->seek(pos+96, librevenge::RVNG_SEEK_SET);

  pos=input->tell();
  ascii().addPos(pos);
  ascii().addNote("ZoneB-B");
  input->seek(pos+92, librevenge::RVNG_SEEK_SET);

  return true;
}

bool WingzParser::readSpreadsheet()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  if (!input->checkPosition(pos+4*6)) {
    MWAW_DEBUG_MSG(("WingzParser::readSpreadsheet: the zone seems to short\n"));
    return false;
  }
  libmwaw::DebugStream f;
  while (!input->isEnd()) {
    pos=input->tell();
    int type=(int) input->readULong(1);
    int val=(int) input->readULong(1);
    int dSz=(int) input->readULong(2);
    if (type==0xFF && val==0xf && dSz==0) {
      ascii().addPos(pos);
      ascii().addNote("_");
      return true;
    }
    f.str("");
    if (type==0xe) f << "Entries(Graphic)";
    else if (type==0x10) f << "Entries(DocInfo)";
    else f << "Entries(Zone" << std::hex << type << std::dec << "A)";
    if (val != 0x80) f << "[" << std::hex << val << std::dec << "]";
    f << ":";
    if (type==0xc) dSz+=4;
    else if (type==0x10) dSz+=14;
    else if (type==0xe) { // graphic
      if (dSz>0x80) {
        // ff->?, 2xx -> textbox ? , 4xx -> chart ?
        MWAW_DEBUG_MSG(("WingzParser::readSpreadsheet: find some complex graphic zone\n"));
        if (!findNextZone(0xe) || input->tell()<pos+0x20) {
          f << "###";
          ascii().addPos(pos);
          ascii().addNote(f.str().c_str());
          input->seek(pos, librevenge::RVNG_SEEK_END);
          break;
        }
        dSz = int(input->tell()-6-pos);
      }
      else
        dSz+=2;
    }
    // zone 16: page size?
    if (!type || !val || (val&0x3F) || !input->checkPosition(pos+6+dSz)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    int id=(int) input->readLong(2);
    if (id) f << "id=" << id << ",";
    if (input->tell() != pos+6+dSz)
      ascii().addDelimiter(input->tell(), '|');
    input->seek(pos+6+dSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool WingzParser::findNextZone(int lastType)
{
  MWAWInputStreamPtr input = getInput();
  bool lastCheck=true;
  while (!input->isEnd()) {
    long pos=input->tell();
    int val=(int)input->readULong(2);
    int type=val&0xFF;
    if (type==0x80) {
      if (!lastCheck) {
        input->seek(-1, librevenge::RVNG_SEEK_CUR);
        lastCheck=true;
      }
      continue;
    }
    lastCheck=false;
    if ((val&0xff00)!=0x8000 || (lastType==0 && type!=1) || type>=0x14 || type<lastType)
      continue;
    long dSz=(long) input->readULong(2);
    if (type==0xc) dSz+=4;
    else if (type==0x10) dSz+=4;
    else if (type==0xe) {
      if (dSz < 0x80) dSz += 2;
      else if (input->checkPosition(pos+0x40)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        return true;
      }
      else {
        input->seek(pos+2, librevenge::RVNG_SEEK_SET);
        continue;
      }
    }
    if (input->checkPosition(pos+6+dSz+2)) {
      input->seek(pos+6+dSz+1,librevenge::RVNG_SEEK_SET);
      val=(int) input->readULong(1);
      if ((val&0xC0) && !(val&0x3F)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        return true;
      }
    }
    input->seek(pos+2, librevenge::RVNG_SEEK_SET);
  }
  return false;
}
////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool WingzParser::checkHeader(MWAWHeader *header, bool /*strict*/)
{
  *m_state = WingzParserInternal::State();

  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork())
    return false;

  int const headerSize=12;
  if (!input->checkPosition(headerSize)) {
    MWAW_DEBUG_MSG(("WingzParser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0,librevenge::RVNG_SEEK_SET);
  if (input->readULong(2)!=0x574e || input->readULong(2)!=0x475a || // WNGZ
      input->readULong(2)!=0x575a || input->readULong(2)!=0x5353) // WZSS
    return false;
  input->setReadInverted(true);
  libmwaw::DebugStream f;
  f << "FileHeader:";
  std::string name(""); // 0110: version number
  for (int i=0; i<4; ++i)
    name += (char) input->readULong(1);
  f << "vers=" << name << ",";
  ascii().addPos(0);
  ascii().addNote(f.str().c_str());
  if (header)
    header->reset(MWAWDocument::MWAW_T_WINGZ, 1, MWAWDocument::MWAW_K_SPREADSHEET);
  input->seek(12,librevenge::RVNG_SEEK_SET);
  input->setReadInverted(false);
  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool WingzParser::readPrintInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  if (!input->checkPosition(pos+0x78)) {
    MWAW_DEBUG_MSG(("WingzParser::readPrintInfo: file is too short\n"));
    return false;
  }

  libmwaw::DebugStream f;
  // print info
  libmwaw::PrinterInfo info;
  if (!info.read(input)) return false;
  f << "Entries(PrintInfo):"<< info;

  Vec2i paperSize = info.paper().size();
  Vec2i pageSize = info.page().size();
  if (pageSize.x() <= 0 || pageSize.y() <= 0 ||
      paperSize.x() <= 0 || paperSize.y() <= 0) return false;

  // define margin from print info
  Vec2i lTopMargin= -1 * info.paper().pos(0);
  Vec2i rBotMargin=info.paper().size() - info.page().size();

  // move margin left | top
  int decalX = lTopMargin.x() > 14 ? lTopMargin.x()-14 : 0;
  int decalY = lTopMargin.y() > 14 ? lTopMargin.y()-14 : 0;
  lTopMargin -= Vec2i(decalX, decalY);
  rBotMargin += Vec2i(decalX, decalY);

  // decrease right | bottom
  int rightMarg = rBotMargin.x() -50;
  if (rightMarg < 0) rightMarg=0;
  int botMarg = rBotMargin.y() -50;
  if (botMarg < 0) botMarg=0;

  getPageSpan().setMarginTop(lTopMargin.y()/72.0);
  getPageSpan().setMarginBottom(botMarg/72.0);
  getPageSpan().setMarginLeft(lTopMargin.x()/72.0);
  getPageSpan().setMarginRight(rightMarg/72.0);
  getPageSpan().setFormLength(paperSize.y()/72.);
  getPageSpan().setFormWidth(paperSize.x()/72.);

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(pos+0x78, librevenge::RVNG_SEEK_SET);
  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
