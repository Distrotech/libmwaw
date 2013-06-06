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
#include <map>
#include <sstream>

#include <libwpd/libwpd.h>

#include "MWAWContentListener.hxx"
#include "MWAWDebug.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWPageSpan.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPictData.hxx"
#include "MWAWPosition.hxx"
#include "MWAWRSRCParser.hxx"

#include "GWParser.hxx"

#include "GWText.hxx"

/** Internal: the structures of a GWText */
namespace GWTextInternal
{
////////////////////////////////////////
//! Internal and low level: structure which stores a text zone header for GWText
struct Zone {
  //! constructor
  Zone(): m_type(-1), m_subType(-1), m_numFonts(0), m_numRulers(0), m_numChar(0),
    m_numCharPLC(0), m_numPLC1(0), m_numPLC2(0), m_numPLC3(0), m_extra("") {
  }
  //! returns true if this is the main zone
  bool isMain() const {
    return m_type==1 && m_subType==3;
  }
  //! check if the data read are or not ok
  bool ok() const {
    if (m_type<0 || m_type > 1 || m_subType < 0 || m_subType > 100)
      return false;
    if (m_numFonts<=0 || m_numRulers<=0 || m_numChar<0 ||
        m_numCharPLC<=0 || m_numPLC1<=0 || m_numPLC2<0 || m_numPLC3<0)
      return false;
    return true;
  }
  //! returns the data size
  long size() const {
    return 22*m_numFonts+192*m_numRulers+6*m_numCharPLC
           +18*m_numPLC1+14*m_numPLC2+22*m_numPLC3+m_numChar;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Zone const &fr) {
    if (fr.m_type==1) {
      if (fr.m_subType==1)
        o << "header/footer,";
      else if (fr.m_subType==3)
        o << "main,";
      else
        o << "#subType=" << fr.m_subType << ",";
    } else {
      if (fr.m_type==0)
        o << "textbox,";
      else
        o << "#type=" << fr.m_type << ",";
      if (fr.m_subType < 1 || fr.m_subType>2) // 1 or 2
        o << "#";
      o << "subType=" << fr.m_subType << ",";
    }
    if (fr.m_numFonts)
      o << "nFonts=" << fr.m_numFonts << ",";
    if (fr.m_numRulers)
      o << "nRulers=" << fr.m_numRulers << ",";
    if (fr.m_numChar)
      o << "length[text]=" << fr.m_numChar << ",";
    if (fr.m_numCharPLC)
      o << "char[plc]=" << fr.m_numCharPLC << ",";
    if (fr.m_numPLC1)
      o << "unkn1[plc]=" << fr.m_numPLC1 << ",";
    if (fr.m_numPLC2)
      o << "unkn2[plc]=" << fr.m_numPLC2 << ",";
    if (fr.m_numPLC3)
      o << "unkn3[plc]=" << fr.m_numPLC3 << ",";
    o << fr.m_extra;
    return o;
  }
  //! the main type: 0=main, 1=textbox
  int m_type;
  //! the type: 1: header/footer, 3: main, 2: unknown
  int m_subType;
  //! the number of fonts
  int m_numFonts;
  //! the number of rulers
  int m_numRulers;
  //! the number of character
  long m_numChar;
  //! the number of char plc
  int m_numCharPLC;
  //! the number of unknown plc
  int m_numPLC1;
  //! the number of unknown plc
  int m_numPLC2;
  //! the number of unknown plc
  int m_numPLC3;
  //! extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the state of a GWText
struct State {
  //! constructor
  State() : m_version(-1), m_numPages(-1), m_actualPage(1) {
  }

  //! the file version
  mutable int m_version;
  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;
};
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
GWText::GWText(GWParser &parser) :
  m_parserState(parser.getParserState()), m_state(new GWTextInternal::State), m_mainParser(&parser)
{
}

GWText::~GWText()
{ }

int GWText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}

int GWText::numPages() const
{
  if (m_state->m_numPages >= 0)
    return m_state->m_numPages;

  int nPages=1;

  return m_state->m_numPages = nPages;
}

bool GWText::readZone()
{
  GWTextInternal::Zone zone;
  return readZone(zone);
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////
bool GWText::createZones()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;

  long pos = input->tell();
  ascFile.addPos(pos);
  ascFile.addNote("Entries(TZoneHeader):");
  pos += 68;
  input->seek(pos, WPX_SEEK_SET);

  if (!readFontNames()) {
    MWAW_DEBUG_MSG(("GWText::createZones: can not find the font names\n"));
    input->seek(pos, WPX_SEEK_SET);
  }

  bool findMainZone=false;
  while (!input->atEOS()) {
    pos=input->tell();
    GWTextInternal::Zone zone;
    if (!readZone(zone)) {
      input->seek(pos,WPX_SEEK_SET);
      if (findMainZone)
        break;

      if (!findNextZone() || !readZone(zone)) {
        input->seek(pos,WPX_SEEK_SET);
        break;
      }
    }
    if (zone.isMain())
      findMainZone=true;
  }
  return findMainZone;
}

bool GWText::findNextZone()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;

  long searchPos=input->tell(), pos=searchPos;
  int const headerSize=24+22+184;
  if (!m_mainParser->isFilePos(pos+headerSize))
    return false;

  // first look for ruler
  input->seek(pos+headerSize, WPX_SEEK_SET);
  while (true) {
    if (input->atEOS())
      return false;
    pos = input->tell();
    unsigned long val=input->readULong(4);
    if (val==0x20FFFF)
      input->seek(pos, WPX_SEEK_SET);
    else if (val==0x20FFFFFF)
      input->seek(pos-1, WPX_SEEK_SET);
    else if (val==0xFFFFFFFF)
      input->seek(pos-2, WPX_SEEK_SET);
    else if (val==0xFFFFFF2E)
      input->seek(pos-3, WPX_SEEK_SET);
    else
      continue;
    if (input->readULong(4)!=0x20FFFF || input->readULong(4)!=0xFFFF2E00) {
      input->seek(pos+4, WPX_SEEK_SET);
      continue;
    }
    // ok a empty tabs stop
    while (!input->atEOS()) {
      pos = input->tell();
      if (input->readULong(4)!=0x20FFFF || input->readULong(4)!=0xFFFF2E00) {
        input->seek(pos, WPX_SEEK_SET);
        break;
      }
    }
    break;
  }

  pos=input->tell();
  int nFonts=0;
  GWTextInternal::Zone zone;
  while(true) {
    long hSize=headerSize+22*nFonts++;
    if (pos-hSize < searchPos)
      break;
    input->seek(pos-hSize, WPX_SEEK_SET);
    if (input->readLong(4))
      continue;
    int val=(int)input->readLong(2);
    if (val!=0&&val!=0x100) continue;
    input->seek(2,WPX_SEEK_CUR);
    if (input->readLong(2)!=nFonts)
      continue;
    input->seek(pos-hSize, WPX_SEEK_SET);
    if (readZone(zone)) {
      input->seek(pos-hSize, WPX_SEEK_SET);
      return true;
    }
  }

  MWAW_DEBUG_MSG(("GWText::findNextZone: can not find begin of zone for pos=%lx\n", pos));
  input->seek(searchPos, WPX_SEEK_SET);
  return false;
}

bool GWText::readSimpleTextbox()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  long endPos=pos+51;
  if (!m_mainParser->isFilePos(endPos))
    return false;

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(Texbox):";
  input->seek(pos, WPX_SEEK_SET);
  if (input->readLong(2)||input->readLong(1)) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  long val=input->readLong(2); // 0, 1
  if (val!=0 && val!=1) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  if (val!=0x1) f << "f0=0,";
  val= input->readLong(2); // 1, 2
  if (val<0 || val > 100) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  f << "f1=" << val << ",";
  if (input->readLong(1)!=1) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  input->seek(pos+50, WPX_SEEK_SET);
  int fSz=(int) input->readULong(1);
  if (!m_mainParser->isFilePos(endPos+fSz)) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  input->seek(endPos+fSz, WPX_SEEK_SET);
  ascFile.addPos(endPos);
  ascFile.addNote("Entries(Text)");
  return true;
}

bool GWText::readZone(GWTextInternal::Zone &header)
{
  header=GWTextInternal::Zone();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  long endPos=pos+24;
  if (!m_mainParser->isFilePos(endPos))
    return false;

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  input->seek(pos, WPX_SEEK_SET);
  if (input->readLong(2)||input->readLong(1)) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  header.m_type=(int) input->readLong(2); // 0, 1
  header.m_subType=(int) input->readLong(2);
  if (input->readLong(1)) { // simple|complex field
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  header.m_numFonts=(int) input->readULong(2);
  header.m_numRulers=(int) input->readULong(2);
  header.m_numCharPLC=(int) input->readULong(2);
  header.m_numPLC1=(int) input->readULong(2); // a PLC ?
  header.m_numPLC2=(int) input->readULong(2);
  header.m_numPLC3=(int) input->readULong(2);
  header.m_numChar=(long) input->readULong(4);
  if (!header.ok() || !m_mainParser->isFilePos(endPos+header.size())) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  header.m_extra=f.str();
  f.str("");
  f << "Entries(HeaderText):" << header;
  if (input->tell()!=endPos) {
    ascFile.addDelimiter(input->tell(),'|');
    input->seek(endPos, WPX_SEEK_SET);
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  for (int i=0; i<header.m_numFonts; i++) {
    if (!readFont())
      return false;
  }
  for (int i=0; i<header.m_numRulers; i++) {
    if (!readRuler())
      return false;
  }
  for (int i=0; i < header.m_numCharPLC; i++) {
    pos=input->tell();
    f.str("");
    f << "Entries(CharPLC)-" << i << ":";
    input->seek(pos+6, WPX_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  for (int i=0; i < header.m_numPLC1; i++) {
    pos=input->tell();
    f.str("");
    f << "Entries(PLC1)-" << i << ":";
    input->seek(pos+18, WPX_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  for (int i=0; i < header.m_numPLC2; i++) {
    pos=input->tell();
    f.str("");
    f << "Entries(PLC2)-" << i << ":";
    input->seek(pos+14, WPX_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  for (int i=0; i < header.m_numPLC3; i++) {
    pos=input->tell();
    f.str("");
    f << "Entries(PLC3)-" << i << ":";
    input->seek(pos+22, WPX_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  int nPictures=0;
  if (header.m_numChar) {
    pos=input->tell();
    f.str("");
    f << "Entries(Text):";
    for (int i=0; i < header.m_numChar; i++) {
      char c=(char)input->readULong(1);
      if (c==0x4)
        nPictures++;
    }
    input->seek(pos+header.m_numChar, WPX_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  if (nPictures)
    m_mainParser->readPictureList(nPictures);
  return true;
}

bool GWText::sendMainText()
{
  return false;
}

void GWText::flushExtra()
{
}

//////////////////////////////////////////////
// Fonts
//////////////////////////////////////////////
bool GWText::readFontNames()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(FontNames):";
  long sz= (long) input->readULong(4);
  long endPos = input->tell()+sz;
  if (sz < 2 || !m_mainParser->isFilePos(endPos)) {
    MWAW_DEBUG_MSG(("GWText::readFontNames: can not read field size\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  int N=(int) input->readLong(2);
  f << "N=" << N << ",";
  if (N*5+2 > sz) {
    MWAW_DEBUG_MSG(("GWText::readFontNames: can not read N\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  for (int i=0; i < N; i++) {
    pos = input->tell();
    f.str("");
    f << "FontNames-" << i << ":";
    int fId=(int) input->readULong(2);
    f << "fId=" << fId << ",";
    int val=(int) input->readLong(2); // always 0 ?
    if (val)
      f << "unkn=" << val << ",";
    int fSz=(int) input->readULong(1);
    if (pos+5+fSz>endPos) {
      MWAW_DEBUG_MSG(("GWText::readFontNames: can not read font %d\n", i));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      input->seek(endPos, WPX_SEEK_SET);
      return i>0;
    }
    std::string name("");
    for (int c=0; c < fSz; c++)
      name+=(char) input->readULong(1);
    if ((fSz%2)==0)
      input->seek(1, WPX_SEEK_CUR);
    f << "\"" << name << "\",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  pos = input->tell();
  if (pos!=endPos) {
    MWAW_DEBUG_MSG(("GWText::readFontNames: find extra data\n"));
    ascFile.addPos(pos);
    ascFile.addNote("FontNames:###");
    input->seek(endPos, WPX_SEEK_SET);
  }
  return true;
}

bool GWText::readFont()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  long endPos=pos+22;
  if (!m_mainParser->isFilePos(endPos))
    return false;

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(FontDef):";
  input->seek(pos, WPX_SEEK_SET);

  input->seek(endPos, WPX_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}
//////////////////////////////////////////////
// Ruler
//////////////////////////////////////////////
bool GWText::readRuler()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  long endPos=pos+192;
  if (!m_mainParser->isFilePos(endPos))
    return false;

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(Ruler):";
  input->seek(pos, WPX_SEEK_SET);

  // OSNOLA: remove me when all is ok
  input->seek(endPos-2, WPX_SEEK_SET);
  char c=(char) input->readULong(1); // last tabs separator
  if (c!='.' && c!=',') {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  input->seek(endPos, WPX_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
