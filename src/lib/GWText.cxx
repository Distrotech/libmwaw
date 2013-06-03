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


////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

//
// find/send the different zones
//
bool GWText::createZones()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;

  long pos=input->tell();
  if (!readFontNames()) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  if (!readZone()) {
    MWAW_DEBUG_MSG(("GWText::createZones: can not find the zone header\n"));
    input->seek(pos, WPX_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote("Entries(ZoneHeader):###");
    return false;
  }
  if (input->atEOS())
    return true;

  while (!input->atEOS()) {
    pos = input->tell();
    ascFile.addPos(pos);
    ascFile.addNote("ZoneC:");

    if (!findNextZone())
      break;
  }

  return false;
}

bool GWText::findNextZone()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  while(1) {
    long searchPos=input->tell(), pos=searchPos;
    if (!m_mainParser->isFilePos(pos+24+22+192))
      return false;

    // first look for ruler
    input->seek(pos+24+22+184, WPX_SEEK_SET);
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
    long actPos=input->tell();
    for (int i=1; i < 20; i++) {
      if (actPos-192-24-22*i < searchPos)
        break;
      input->seek(actPos-192-24-22*i, WPX_SEEK_SET);
      if (input->readLong(4))
        continue;
      int val=(int)input->readLong(2);
      if (val!=0&&val!=0x100) continue;
      input->seek(2,WPX_SEEK_CUR);
      if (input->readLong(2)!=i)
        continue;
      input->seek(actPos-192-24-22*i, WPX_SEEK_SET);
      if (readZone())
        return true;
    }
    MWAW_DEBUG_MSG(("GWText::findNextZone: can not find begin of zone for pos=%lx\n", actPos));
    input->seek(actPos+1, WPX_SEEK_SET);
  }
  return false;
}

bool GWText::readZone()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  long endPos=pos+24;
  if (!m_mainParser->isFilePos(endPos))
    return false;

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(ZoneHeader):";
  input->seek(pos, WPX_SEEK_SET);
  if (input->readLong(4)) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  long val=input->readLong(2); // 0, 100
  if (val!=0 && val!=0x100) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  if (val!=0x100) f << "unkn0=" << std::hex << val << std::dec << ",";
  val= input->readLong(2); // 100, 300, ...
  f << "unkn1=" << std::hex << val << std::dec << ",";
  int nFonts=(int) input->readULong(2);
  f << "nFonts=" << nFonts << ",";
  int nRulers=(int) input->readULong(2);
  f << "nRulers=" << nRulers << ",";
  int nCharPLC=(int) input->readULong(2);
  f << "nCharPLC=" << nCharPLC << ",";
  int nPLC1=(int) input->readULong(2); // a PLC ?
  f << "nPLC1=" << nPLC1 << ",";
  int nPLC2=(int) input->readULong(2);
  f << "nPLC2=" << nPLC2 << ",";
  int nPLC3=(int) input->readULong(2);
  f << "nPLC3=" << nPLC2 << ",";
  int nChar=(int) input->readULong(4);
  f << "nChar=" << nChar << ",";
  if (nPLC1 <= 0 || nRulers <= 0 || nChar < 0 ||
      !m_mainParser->isFilePos(endPos+22*nFonts+192*nRulers+6*nCharPLC+18*nPLC1+14*nPLC2+22*nPLC3+nChar)) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  if (input->tell()!=endPos) {
    ascFile.addDelimiter(input->tell(),'|');
    input->seek(endPos, WPX_SEEK_SET);
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  for (int i=0; i<nFonts; i++) {
    if (!readFont())
      return false;
  }
  for (int i=0; i<nRulers; i++) {
    if (!readRuler())
      return false;
  }
  for (int i=0; i < nCharPLC; i++) {
    pos=input->tell();
    f.str("");
    f << "Entries(CharPLC)-" << i << ":";
    input->seek(pos+6, WPX_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  endPos=input->tell();
  for (int i=0; i < nPLC1; i++) {
    pos=input->tell();
    f.str("");
    f << "Entries(PLC1)-" << i << ":";
    input->seek(pos+18, WPX_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  endPos=input->tell();
  for (int i=0; i < nPLC2; i++) {
    pos=input->tell();
    f.str("");
    f << "Entries(PLC2)-" << i << ":";
    input->seek(pos+14, WPX_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  endPos=input->tell();
  for (int i=0; i < nPLC3; i++) {
    pos=input->tell();
    f.str("");
    f << "Entries(PLC3)-" << i << ":";
    input->seek(pos+22, WPX_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  endPos=input->tell();
  if (nChar) {
    pos=input->tell();
    f.str("");
    f << "Entries(Text):";
    input->seek(pos+nChar, WPX_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  endPos=input->tell();
  // in v1: a list of sz<dataSz>
  int n=0;
  while(!input->atEOS()) {
    pos = input->tell();
    unsigned long value= input->readULong(4);
    int decal=-1;
    // first tabs
    if (value==0x20FFFF)
      decal = 0;
    else if (value==0x20FFFFFF)
      decal = 1;
    else if (value==0xFFFFFFFF)
      decal = 2;
    else if (value==0xFFFFFF2E)
      decal = 3;
    if (decal>=0) {
      input->seek(pos-decal, WPX_SEEK_SET);
      if (input->readULong(4)==0x20FFFF && input->readULong(4)==0xFFFF2E00)
        break;
      input->seek(pos+4, WPX_SEEK_SET);
      continue;
    }
    // graphic size
    if ((value>>24)==0x36)
      decal = 3;
    else if ((value>>16)==0x36)
      decal = 2;
    else if (((value>>8)&0xFFFF)==0x36)
      decal = 1;
    else if ((value&0xFFFF)==0x36)
      decal = 0;
    if (decal==-1)
      continue;
    input->seek(pos-decal, WPX_SEEK_SET);
    int N=(int) input->readULong(2);
    if (N==0 || !m_mainParser->isFilePos(pos-decal+4+54*N) ||
        input->readLong(2)!=0x36 || !readGraphic()) {
      input->seek(pos+4, WPX_SEEK_SET);
      continue;
    }
    input->seek(pos-decal, WPX_SEEK_SET);
    if (!readGraphicList()) {
      input->seek(pos+4, WPX_SEEK_SET);
      continue;
    }
    if (pos-decal!=endPos) {
      f.str("");
      f << "ZoneC-" << n++ << ":";
      ascFile.addPos(endPos);
      ascFile.addNote(f.str().c_str());
    }
    endPos=input->tell();
  }
  input->seek(endPos, WPX_SEEK_SET);

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
  long endPos = pos+4+sz;
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

//////////////////////////////////////////////
// Graphic
//////////////////////////////////////////////
bool GWText::readGraphicList()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  long endPos=pos+4;
  if (!m_mainParser->isFilePos(endPos))
    return false;

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(Graphic):";
  int N=(int) input->readLong(2);
  int fSz=(int) input->readULong(2);
  if (N<0 || !fSz || !m_mainParser->isFilePos(endPos+N*fSz)) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  endPos+=N*fSz;
  f << "N=" << N << ",fSz=" << fSz << ",";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  for (int i=0; i < N; i++) {
    pos = input->tell();
    if (!readGraphic()) {
      MWAW_DEBUG_MSG(("GWText::readGraphicList: oops graphic detection is probably bad\n"));
      ascFile.addPos(pos);
      ascFile.addNote("Graphic:###");
      input->seek(endPos+N*fSz, WPX_SEEK_SET);
      return true;
    }
  }
  ascFile.addPos(endPos);
  ascFile.addNote("_");
  pos = endPos;
  input->seek(pos, WPX_SEEK_SET);

  endPos = pos+4;
  if (!m_mainParser->isFilePos(endPos))
    return true;
  N=(int) input->readLong(2);
  fSz=(int) input->readULong(2);
  if (N<0 || !fSz || !m_mainParser->isFilePos(endPos+N*fSz)) {
    input->seek(pos, WPX_SEEK_SET);
    return true;
  }
  endPos+=N*fSz;
  f.str("");
  f << "Entries(GraphA): N=" << N << ",fSz=" << fSz << ",";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  for (int i=0; i < N; i++) {
    pos = input->tell();
    ascFile.addPos(pos);
    ascFile.addNote("GraphA:");
    input->seek(pos+fSz, WPX_SEEK_SET);
  }
  ascFile.addPos(endPos);
  ascFile.addNote("_");
  pos = endPos;
  input->seek(pos, WPX_SEEK_SET);

  endPos = pos+4;
  if (!m_mainParser->isFilePos(endPos))
    return true;
  N=(int) input->readLong(2);
  fSz=(int) input->readULong(2);
  if (N<0 || !fSz || !m_mainParser->isFilePos(endPos+N*fSz)) {
    input->seek(pos, WPX_SEEK_SET);
    return true;
  }
  endPos+=N*fSz;
  f.str("");
  f << "Entries(GraphB): N=" << N << ",fSz=" << fSz << ",";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  for (int i=0; i < N; i++) {
    pos = input->tell();
    ascFile.addPos(pos);
    ascFile.addNote("GraphB:");
    input->seek(pos+fSz, WPX_SEEK_SET);
  }
  ascFile.addPos(endPos);
  ascFile.addNote("_");
  pos = endPos;
  input->seek(pos, WPX_SEEK_SET);

  return true;
}

bool GWText::readGraphic()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  long endPos=pos+54;
  if (!m_mainParser->isFilePos(endPos))
    return false;

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Graphic:";
  int type=(int) input->readLong(1);
  if (type<0||type>16||input->readLong(1)) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  float dim[4];
  for (int i=0; i<4; i++)
    dim[i]=float(input->readLong(4))/65536.f;
  if (dim[0]<=0 || dim[1]<=0 || dim[2]<dim[0] || dim[3]<dim[1]) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  f << "dim=" << dim[1] << "x" << dim[0] << "<->"
    << dim[3] << "x" << dim[2] << ",";
  ascFile.addDelimiter(input->tell(),'|');
  input->seek(endPos, WPX_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
