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

////////////////////////////////////////////////////////////
// spreadsheet
////////////////////////////////////////////////////////////
bool WingzParser::readSpreadsheet()
{
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  while (!input->isEnd()) {
    long pos=input->tell();
    int type=(int) input->readULong(1);
    int val=(int) input->readULong(1);
    int dSz=(int) input->readULong(2);
    if (type!=0xFF && input->isEnd()) {
      MWAW_DEBUG_MSG(("WingzParser::readSpreadsheet: can not read some zone\n"));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    std::string name("");
    if (type<=0x10) {
      static char const *(wh[])= {
        "", "SheetSize", "SheetSize", "", "", "", "", "" /* cell content or maybe in 12*/,
        "Formula", "SheetFont", "", "", "", "", "Graphic", "",
        "PrintInfo"
      };
      name=wh[type];
    }
    if (name.empty()) {
      std::stringstream s;
      s << "ZSheet" << type;
      name = s.str();
    }
    f.str("");
    f << "Entries(" << name << "):";
    if (val!=0x80) f << "fl=" << std::hex << val << std::dec << ",";

    bool ok=true;
    switch (type) {
    case 1: // col size
    case 2: // row size
    case 18: // ?
    case 19: // ?
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      ok=readSpreadsheetSize();
      break;
    case 3: // never find this block with data ...
    case 4: // idem
      ok=input->checkPosition(pos+6+dSz);
      if (!ok) break;
      val=(int) input->readLong(2);
      if (val) f << "id=" << val << ",";
      if (dSz) {
        MWAW_DEBUG_MSG(("WingzParser::readSpreadsheet: find some data in zone %d\n", type));
        f << "###";
        ascii().addDelimiter(pos+6,'|');
      }
      input->seek(pos+6+dSz, librevenge::RVNG_SEEK_SET);
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      break;
    case 9:
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      ok=readSpreadsheetFont();
      break;
    case 0xc:
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      ok=readSpreadsheetRow();
      break;
    case 0x10:
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      ok=readPrintInfo();
      break;
    case 0xff: // end
      if (val==0xf && dSz==0) {
        ascii().addPos(pos);
        ascii().addNote("_");
        return true;
      }
      ok=false;
      break;
    default:
      ok=false;
    }
    if (ok) continue;
    input->seek(pos+4, librevenge::RVNG_SEEK_SET);
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

bool WingzParser::readSpreadsheetRow()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  int type=(int) input->readULong(1);
  if (type!=12) return false;
  int val=(int) input->readULong(1);
  int dSz= (int) input->readULong(2);
  int id=(int) input->readLong(2);
  long endPos=pos+10+dSz;
  libmwaw::DebugStream f;
  f << "Entries(SheetCell)[" << id << "]:";
  if (val!=0x40) f << "fl=" << std::hex << val << std::dec << ",";
  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("WingzParser::readSpreadsheetRow: find bad size for data\n"));
    return false;
  }
  for (int i=0; i<2; ++i) {
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  while (!input->isEnd()) {
    pos=input->tell();
    if (pos>=endPos) break;
    type=(int) input->readULong(1);
    f.str("");
    f << "SheetCell:typ=" << (type&0xf);
    if (type&0xf0) f << "[high=" << (type>>4) << "]";
    f << ",";
    bool ok=true;
    switch (type &0xf) {
    case 0: //nothing
      break;
    case 1: // style
      if (pos+6>endPos) {
        ok=false;
        break;
      }
      input->seek(pos+6, librevenge::RVNG_SEEK_SET);
      break;
    case 2:
    case 3: {
      ok=false;
      if (pos+9>endPos)
        break;
      input->seek(pos+8, librevenge::RVNG_SEEK_SET);
      int fSz=(int) input->readULong(1);
      if (pos+9+fSz>endPos)
        break;
      std::string text("");
      for (int i=0; i<fSz; ++i) text += (char) input->readULong(1);
      f << text;
      input->seek(pos+9+fSz, librevenge::RVNG_SEEK_SET);
      ok=true;
      break;
    }
    case 5: { // style + double
      if (pos+16>endPos) {
        ok=false;
        break;
      }
      input->seek(pos+8, librevenge::RVNG_SEEK_SET);
      double value;
      bool isNAN;
      input->readDoubleReverted8(value, isNAN);
      f << value;
      input->seek(pos+16, librevenge::RVNG_SEEK_SET);
      break;
    }
    default:
      ok=false;
      break;
    }
    if (!ok) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  pos=input->tell();
  if (pos==endPos) return true;
  MWAW_DEBUG_MSG(("WingzParser::readSpreadsheetRow: find some extra data\n"));
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote("SheetCell-end:");
  return true;
}

bool WingzParser::readSpreadsheetFont()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  int type=(int) input->readULong(1);
  if (type!=9) return false;
  int val=(int) input->readULong(1);
  int dSz= (int) input->readULong(2);
  int id=(int) input->readLong(2);

  libmwaw::DebugStream f;
  f << "Entries(SheetFont)[" << id << "]:";
  if (val!=0x40) f << "fl=" << std::hex << val << std::dec << ",";
  if (dSz<26 || !input->checkPosition(pos+6+dSz)) {
    MWAW_DEBUG_MSG(("WingzParser::readSpreadsheetFont: find bad size for data\n"));
    return false;
  }
  val=(int) input->readLong(2);
  if (val) f << "used?=" << val << ",";
  val=(int) input->readLong(2); // always 0?
  if (val) f << "f0=" << val << ",";
  int fSz=(int) input->readLong(2);
  f << "font[sz]=" << fSz << ",";
  int flag=(int) input->readLong(2); // bold, ..?
  if (flag) f << "font[flag]=" << std::hex << flag << std::dec << ",";
  for (int i=0; i<2; ++i) { // always 0?
    val=(int) input->readLong(2);
    if (val) f << "f" << i+1 << "=" << val << ",";
  }
  val=(int) input->readLong(1); // 0|1|0x14
  if (val) f << "f3=" << val << ",";
  for (int i=0; i<3; ++i) { // back, unknown,front color
    val=(int) input->readULong(4);
    int col=int(val&0xFFFFFF);
    int high=(val>>24);
    if ((i==0&&col!=0xFFFFFF) || i==1 || (i==2&&col))
      f << "col" << i << "=" << std::hex << col << std::dec << ",";
    if (high) f << "col" << i << "[high]=" << high << ","; // 0|1
  }
  int nSz=(int) input->readULong(1);
  if (26+nSz>dSz) {
    MWAW_DEBUG_MSG(("WingzParser::readSpreadsheetFont: the name size seems bad\n"));
    f << "###";
  }
  else {
    std::string name("");
    for (int i=0; i < nSz; ++i) name+=(char) input->readULong(1);
    f << name << ",";
  }
  if (input->tell()!=pos+6+dSz) {
    MWAW_DEBUG_MSG(("WingzParser::readSpreadsheetFont: find some extra data\n"));
    ascii().addDelimiter(input->tell(), '|');
    input->seek(pos+6+dSz, librevenge::RVNG_SEEK_SET);
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

bool WingzParser::readSpreadsheetSize()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  int type=(int) input->readULong(1);
  if (type!=1 && type!=2 && type!=18 && type!=19) return false;
  libmwaw::DebugStream f;
  if (type <= 2)
    f << "Entries(SheetSize)[" << (type==1 ? "col" : "row") << "]:";
  else // related to page break?
    f << "Entries(SheetPbrk)[" << (type==18 ? "col" : "row") << "]:";
  int val=(int) input->readULong(1);
  if (val!=0x80) f << "fl=" << std::hex << val << std::dec << ",";
  int dSz= (int) input->readULong(2);
  if (dSz%4 || !input->checkPosition(pos+6+dSz)) {
    MWAW_DEBUG_MSG(("WingzParser::readSpreadsheetSize: find bad size for data\n"));
    return false;
  }
  int id=(int) input->readLong(2);
  if (id) f << "id=" << id << ",";
  f << "pos=[";
  for (int i=0; i<dSz/4; ++i) {
    int cell=(int) input->readULong(2); // the row/col number
    if (cell==0xFFFF) f << "-inf";
    else if (cell==0x7FFF) f << "inf";
    else f << cell;
    val=(int) input->readULong(2);
    if (type<=2)
      f << ":" << double(val)/20. << "pt,"; // a dim TWIP
    else
      f << "[sz=" << val << "],"; // num row/column
  }
  f << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  return true;
}

// retrieve a next spreadheet zone (used when parsing stop for some problem )
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
  int type=(int) input->readULong(1);
  if (type!=0x10) return false;
  int val=(int) input->readULong(1);
  int dSz=(int) input->readULong(2);
  int id=(int) input->readULong(2);
  if (dSz!=0x7c || !input->checkPosition(pos+20+0x7c)) {
    MWAW_DEBUG_MSG(("WingzParser::readPrintInfo: the header seem bad\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(PrintInfo):";
  if (val!=0x80) f << "fl=" << std::hex << val << std::dec << ",";
  if (id) f << "id=" << id << ",";
  for (int i=0; i<3; ++i) {
    int dim[2];
    for (int j=0; j<2; ++j) dim[j]=(int) input->readULong(2);
    if (i==2)
      f << "unit=" << dim[0] << "x" << dim[1] << ",";
    else
      f << "dim" << i << "=" << dim[0] << "x" << dim[1] << ",";
  }
  // 3 small number 0x78,4,6|7
  for (int i=0; i<3; ++i) {
    val=(int) input->readULong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  // print info
  libmwaw::PrinterInfo info;
  input->setReadInverted(false);
  bool ok=info.read(input);
  input->setReadInverted(true);
  if (!ok) return false;
  f << info;

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

  input->seek(pos+20+0x7c, librevenge::RVNG_SEEK_SET);
  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
