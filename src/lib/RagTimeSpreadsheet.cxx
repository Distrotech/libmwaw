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

#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWTextListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"

#include "RagTimeParser.hxx"

#include "RagTimeSpreadsheet.hxx"

/** Internal: the structures of a RagTimeSpreadsheet */
namespace RagTimeSpreadsheetInternal
{
//! Internal: a spreadsheet's zone of a RagTimeSpreadsheet
struct SpreadsheetZone {
  //! constructor
  SpreadsheetZone()
  {
  }
};

////////////////////////////////////////
//! Internal: the state of a RagTimeSpreadsheet
struct State {
  //! constructor
  State() : m_version(-1)
  {
  }

  //! the file version
  mutable int m_version;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
RagTimeSpreadsheet::RagTimeSpreadsheet(RagTimeParser &parser) :
  m_parserState(parser.getParserState()), m_state(new RagTimeSpreadsheetInternal::State), m_mainParser(&parser)
{
}

RagTimeSpreadsheet::~RagTimeSpreadsheet()
{ }

int RagTimeSpreadsheet::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}


////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// spreadsheet zone
////////////////////////////////////////////////////////////
bool RagTimeSpreadsheet::readSpreadsheet(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+0x66)) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheet: the position seems bad\n"));
    return false;
  }
  if (version()<2) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheet: must not be called for v1-2... file\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(SpreadsheetZone):";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  if (dSz<0x62 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheet: the size seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  int val;
  for (int i=0; i<6; ++i) { // f0=0, f1=4|6|44, f2=1-8, f3=1-f, f4=1-5, f5=1-3
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  for (int i=0; i<6; ++i) // g0~40, g1=g2=2, g3~16, g4=[b-10], g5=[8-c]|800d
    f << "g" << i << "=" << double(input->readLong(4))/65536. << ",";
  long zoneBegin[11];
  zoneBegin[10]=endPos;
  for (int i=0; i<10; ++i) {
    zoneBegin[i]=(long) input->readULong(4);
    if (!zoneBegin[i]) continue;
    f << "zone" << i << "=" << std::hex << pos+2+zoneBegin[i] << std::dec << ",";
    if (pos+2+zoneBegin[i]>endPos) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheet: the zone %d seems bad\n",i));
      zoneBegin[i]=0;
      f << "###";
      continue;
    }
    zoneBegin[i]+=pos+2;
  }
  f << "fl?=["; // or some big number
  for (int i=0; i<8; ++i) {
    val=(int) input->readULong(2);
    if (val)
      f << std::hex << val << std::dec << ",";
    else
      f << "_,";
  }
  f << "],";
  for (int i=0; i<3; ++i) { // h0=0-4, h1=h2=0
    val=(int) input->readULong(2);
    if (val)
      f << "h" << i << "=" << val << ",";
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  // now read the different zone, first set the endPosition
  for (int i=9; i>=0; --i) {
    if (zoneBegin[i]==0)
      zoneBegin[i]=zoneBegin[i+1];
    else if (zoneBegin[i]>zoneBegin[i+1]) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheet: the zone %d seems bad(II)\n",i));
      zoneBegin[i]=zoneBegin[i+1];
    }
  }
  for (int i=0; i<10; ++i) {
    if (zoneBegin[i+1]<=zoneBegin[i]) continue;
    f.str("");
    f << "SpreadsheetZone-" << i << ":";
    // SpreadsheetZone-3: sz+[32bytes]+(N+1)*12
    // SpreadsheetZone-9: sz+N+N*14
    ascFile.addPos(zoneBegin[i]);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool RagTimeSpreadsheet::readSpreadsheetV2(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+6)) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetV2: the position seems bad\n"));
    return false;
  }
  if (version()>=2) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetV2: must not be called for v3... file\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  f << "Entries(SpreadsheetZone):";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  if (dSz<4 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetV2: the size seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  ascFile.addPos(endPos);
  ascFile.addNote("_");
  long zonesList[2]= {0,endPos};
  for (int i=0; i<2; ++i) {
    long ptr=pos+6+(long) input->readULong(2);
    f << "ptr[" << i << "]=" << std::hex << ptr << std::dec << ",";
    if (ptr>=endPos) {
      f << "###";
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetV2: the zone begin seems bad%d\n", i));
      continue;
    }
    zonesList[i]=ptr;
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  MWAWEntry cells;
  cells.setBegin(zonesList[0]);
  cells.setEnd(zonesList[1]);
  readSpreadsheetCellsV2(cells);

  MWAWEntry extra;
  extra.setBegin(zonesList[1]);
  extra.setEnd(endPos);
  return readSpreadsheetExtraV2(extra);
}

bool RagTimeSpreadsheet::readSpreadsheetCellsV2(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=entry.begin();
  long endPos=entry.end();
  if (pos<=0 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellsV2: the position seems bad\n"));
    return false;
  }

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  ascFile.addPos(endPos);
  ascFile.addNote("_");

  int n=0;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  Vec2i cellPos(0,0);
  while (!input->isEnd()) {
    pos=input->tell();
    if (pos+2>endPos) break;
    f.str("");
    f << "Entries(SpreadsheetCell)[" << n++ << "]:";
    int col=(int) input->readLong(1);
    if (col<=cellPos[0]) ++cellPos[1];
    cellPos[0]=col;
    f << "C" << cellPos << ":";
    int dSz=(int) input->readULong(1);
    long zEndPos=pos+6+dSz;
    if (zEndPos>endPos) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellsV2: problem reading some cells\n"));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      return false;
    }
    int val=(int) input->readULong(1);
    int type=(val>>4);
    switch (type) {
    case 0:
      f << "empty,";
      break;
    case 3:
      f << "number,";
      break;
    case 9:
      f << "text,";
      break;
    default:
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellsV2: find unknown type %d\n", type));
      f << "##type=" << type << ",";
      break;
    }
    bool hasFormat=(val&1);
    bool hasAlign=(val&2);
    bool hasFormula=(val&4);
    bool hasFlag80=(val&8);
    if (hasFormula)
      f << "formula,";
    if (val&8)
      f << "flag80,";

    val=(int) input->readULong(1);
    bool hasNumberFormat=false;
    if (val&0x80) {
      f << "digits[set],";
      val&=0x7F;
      hasNumberFormat=true;
    }
    // fl0&30: no change
    if (val) f << "fl0=" << std::hex << val << std::dec << ",";
    val=(int) input->readULong(1);
    if (val&0xF0) f << "bord=" << std::hex << (val>>4) << std::dec << ",";
    if (val&0xF) f << "fl1=" << std::hex << (val&0xf) << std::dec << ",";
    val=(int) input->readULong(1);
    if (val) f << "fl2=" << std::hex << val << std::dec << ",";
    bool ok=true;
    long actPos;
    if (ok && hasNumberFormat) {
      val=(int) input->readULong(1);
      actPos=input->tell();
      switch (val>>5) {
      case 1:
        if (actPos+1>zEndPos) {
          MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellsV2: problem reading type1\n"));
          f << "#type 1,";
          ok=false;
          break;
        }
        // find 82
        f << "type1[" << std::hex << input->readULong(1) << std::dec << "],";
        break;
      case 3:
        if (actPos+1>zEndPos) {
          MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellsV2: problem reading currency\n"));
          f << "#currency,";
          ok=false;
          break;
        }
        // find c2
        f << "currency[" << std::hex << input->readULong(1) << std::dec << "],";
        break;
      case 6:
        if (actPos+1>zEndPos) {
          MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellsV2: problem reading currency\n"));
          f << "#percent,";
          ok=false;
          break;
        }
        // find 0
        f << "percent[" << std::hex << input->readULong(1) << std::dec << "],";
        break;
      case 4:
      case 2:
        if ((val>>5)==4) f << "scientific,";
        if (actPos+1>zEndPos) {
          MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellsV2: problem reading a digits\n"));
          f << "#digits,";
          ok=false;
          break;
        }
        f << "digits=" << input->readULong(1) << ",";
        break;
      case 0:
        break;
      default:
        MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellsV2: problem read numbering flags\n"));
        ok=false;
        break;
      }
      if (ok)
        val &= 0x1F;
      else
        f << "##";
      if (val)
        f << "fl3=" << std::hex << val << std::dec << ",";
    }
    if (ok && hasFormat) {
      if (input->tell()+4>zEndPos) {
        MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellsV2: problem reading font format\n"));
        f << "#format,";
        ok=false;
      }
      f << "Font=[";
      f << "size=" << input->readULong(1) << ",";
      f << "flags=" << std::hex << input->readULong(1) << std::dec << ",";
      f << "id=" << input->readULong(2) << ",";
      f << "],";
    }
    if (ok && hasFlag80) {
      actPos=input->tell();
      int fSz=(int) input->readULong(1);
      if (!fSz||actPos+1+fSz>zEndPos) {
        MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellsV2: can not read a flag 80\n"));
        f << "###flag80,";
        ok=false;
      }
      else {
        // find some zone with size 22 as 59cb050e030304058380fd04050e86050d030104030d
        ascFile.addDelimiter(input->tell(),'|');
        input->seek(actPos+1+fSz, librevenge::RVNG_SEEK_SET);
        if (actPos+1+fSz!=zEndPos)
          ascFile.addDelimiter(input->tell(),'|');
      }
    }
    val= (ok && hasAlign) ? (int) input->readULong(1) : 0;
    int align= val&7;
    switch (align) {
    case 0:
      break;
    case 2:
      f << "left,";
      break;
    case 3:
      f << "center,";
      break;
    case 4:
      f << "right,";
      break;
    case 5:
      f << "full,";
      break;
    default:
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellsV2: find unknown alignment\n"));
      f << "##align=" << align << ",";
      break;
    }
    val&=0xF8;
    if (ok && hasFormula) {
      actPos=input->tell();
      int fSz=(int) input->readULong(1);
      if (!fSz||actPos+1+fSz>zEndPos) {
        MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellsV2: can not read a formula\n"));
        f << "###formula,";
        ok=false;
      }
      else {
        ascFile.addDelimiter(input->tell(),'|');
        input->seek(actPos+1+fSz, librevenge::RVNG_SEEK_SET);
        if (actPos+1+fSz!=zEndPos)
          ascFile.addDelimiter(input->tell(),'|');
      }
    }
    if (ok) {
      actPos=input->tell();
      switch (type) {
      case 0:
        if (actPos!=zEndPos) {
          MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellsV2: something look bad\n"));
          f << "###data";
          break;
        }
        break;
      case 3: {
        if (actPos+10!=zEndPos) {
          MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellsV2: can not read a number\n"));
          f << "###number";
          break;
        }
        double res;
        bool isNan;
        if (!input->readDouble10(res, isNan))
          f << "#value,";
        else
          f << res << ",";
        break;
      }
      case 9: {
        int sSz=(int) input->readULong(1);
        if (actPos+1+sSz!=zEndPos) {
          MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetCellsV2: can not read a text\n"));
          f << "###text";
          break;
        }
        std::string text("");
        for (int i=0; i<sSz; ++i) text+=(char) input->readULong(1);
        f << text << ",";
        break;
      }
      default:
        break;
      }
    }

    actPos=input->tell();
    if (actPos!=zEndPos)
      ascFile.addDelimiter(actPos,'|');
    if ((dSz%2)==1) ++zEndPos;
    input->seek(zEndPos, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool RagTimeSpreadsheet::readSpreadsheetExtraV2(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=entry.begin();
  long endPos=entry.end();
  if (pos<=0 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetExtraV2: the position seems bad\n"));
    return false;
  }

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  ascFile.addPos(endPos);
  ascFile.addNote("_");

  for (int i=0; i<2; ++i) {
    pos=input->tell();
    f.str("");
    static char const *(what[])= {"SpreadsheetB", "SpreadsheetCol"};
    f << "Entries(" << what[i] << "):";
    int n=(int) input->readULong(2);
    f << "N=" << n << ",";
    static int const dataSize[]= {20,14};
    if (pos+2+dataSize[i]*n>endPos) {
      MWAW_DEBUG_MSG(("RagTimeSpreadsheet::readSpreadsheetExtraV2: problem reading some spreadsheetZone[B%d] field\n", i));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      return false;
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    for (int j=0; j<n; ++j) {
      pos=input->tell();
      f.str("");
      f << what[i] << "-" << j << ":";
      input->seek(pos+dataSize[i], librevenge::RVNG_SEEK_SET);
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
  }

  /* finally something like
     86000000200201000c000014003c00030c090000
     or
     86000000204201000a000003004000060d0a000000000ce5410000010000000000000000688f688f688f0000000000000000688f688f688f000000000000
     font + ?
  */
  ascFile.addPos(input->tell());
  ascFile.addNote("SpreadsheetZone[B-end]:");
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// read a zone of spreadsheet
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// send data to the listener

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
