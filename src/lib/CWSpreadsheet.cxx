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
#include <sstream>

#include <libwpd/libwpd.h>

#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"

#include "CWParser.hxx"
#include "CWStruct.hxx"
#include "CWStyleManager.hxx"

#include "CWSpreadsheet.hxx"

/** Internal: the structures of a CWSpreadsheet */
namespace CWSpreadsheetInternal
{
//! Internal the spreadsheet
struct Spreadsheet : public CWStruct::DSET {
  // constructor
  Spreadsheet(CWStruct::DSET const &dset = CWStruct::DSET()) :
    CWStruct::DSET(dset) {
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Spreadsheet const &doc) {
    o << static_cast<CWStruct::DSET const &>(doc);
    return o;
  }
};

//! Internal: the state of a CWSpreadsheet
struct State {
  //! constructor
  State() : m_spreadsheetMap() {
  }

  std::map<int, shared_ptr<Spreadsheet> > m_spreadsheetMap;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
CWSpreadsheet::CWSpreadsheet(CWParser &parser) :
  m_parserState(parser.getParserState()), m_state(new CWSpreadsheetInternal::State),
  m_mainParser(&parser), m_styleManager(parser.m_styleManager)
{
}

CWSpreadsheet::~CWSpreadsheet()
{ }

int CWSpreadsheet::version() const
{
  return m_parserState->m_version;
}

// fixme
int CWSpreadsheet::numPages() const
{
  return 1;
}
////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// a document part
////////////////////////////////////////////////////////////
shared_ptr<CWStruct::DSET> CWSpreadsheet::readSpreadsheetZone
(CWStruct::DSET const &zone, MWAWEntry const &entry, bool &complete)
{
  complete = false;
  if (!entry.valid() || zone.m_fileType != 2 || entry.length() < 256)
    return shared_ptr<CWStruct::DSET>();
  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos+8+16, WPX_SEEK_SET); // avoid header+8 generic number
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  shared_ptr<CWSpreadsheetInternal::Spreadsheet>
  spreadsheetZone(new CWSpreadsheetInternal::Spreadsheet(zone));

  f << "Entries(SpreadsheetDef):" << *spreadsheetZone << ",";
  ascFile.addDelimiter(input->tell(), '|');
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  // read the last part
  long data0Length = zone.m_dataSz;
  long N = zone.m_numData;
  if (entry.length() -8-12 != data0Length*N + zone.m_headerSz) {
    if (data0Length == 0 && N) {
      MWAW_DEBUG_MSG(("CWSpreadsheet::readSpreadsheetZone: can not find definition size\n"));
      input->seek(entry.end(), WPX_SEEK_SET);
      return shared_ptr<CWStruct::DSET>();
    }

    MWAW_DEBUG_MSG(("CWSpreadsheet::readSpreadsheetZone: unexpected size for zone definition, try to continue\n"));
  }
  int debColSize = 0;
  int vers = version();
  switch(vers) {
  case 1:
    debColSize = 72;
    break;
  case 2:
  case 3: // checkme...
  case 4:
  case 5:
    debColSize = 76;
    break;
  case 6:
    debColSize = 72;
    break;
  default:
    break;
  }

  std::vector<int> colSize;
  if (debColSize) {
    pos = entry.begin()+debColSize;
    input->seek(pos, WPX_SEEK_SET);
    f.str("");
    f << "Entries(SpreadsheetCol):width,";
    for (int i = 0; i < 256; i++) {
      int w=(int)input->readULong(1);
      if (w!=36) // default
        f << "w" << i+1 << "=" << w << ",";
      colSize.push_back(w);
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    ascFile.addPos(input->tell());
    ascFile.addNote("SpreadsheetDef-A");
  }

  long dataEnd = entry.end()-N*data0Length;
  int numLast = version()==6 ? 4 : 0;
  if (long(input->tell()) + data0Length + numLast <= dataEnd) {
    ascFile.addPos(dataEnd-data0Length-numLast);
    ascFile.addNote("SpreadsheetDef-_");
    if (numLast) {
      ascFile.addPos(dataEnd-numLast);
      ascFile.addNote("SpreadsheetDef-extra");
    }
  }
  input->seek(dataEnd, WPX_SEEK_SET);

  for (int i = 0; i < N; i++) {
    pos = input->tell();

    f.str("");
    f << "SpreadsheetDef-" << i;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+data0Length, WPX_SEEK_SET);
  }

  input->seek(entry.end(), WPX_SEEK_SET);

  if (m_state->m_spreadsheetMap.find(spreadsheetZone->m_id) != m_state->m_spreadsheetMap.end()) {
    MWAW_DEBUG_MSG(("CWSpreadsheet::readSpreadsheetZone: zone %d already exists!!!\n", spreadsheetZone->m_id));
  } else
    m_state->m_spreadsheetMap[spreadsheetZone->m_id] = spreadsheetZone;

  spreadsheetZone->m_otherChilds.push_back(spreadsheetZone->m_id+1);
  pos = input->tell();

  bool ok = readZone1(*spreadsheetZone);
  if (ok) {
    pos = input->tell();
    ok = m_mainParser->readStructZone("SpreadsheetZone2", false);
  }
  if (ok) {
    pos = input->tell();
    ok = readContent(*spreadsheetZone);
  }
  if (ok) {
    pos = input->tell();
    // Checkme: it is a simple list or a set of list ?
    // sometimes zero or a list of pair of int(pos?, id?)
    ok = m_mainParser->readStructZone("SpreadsheetListUnkn0", false);
  }

  if (!ok)
    input->seek(pos, WPX_SEEK_SET);
#if 0
  else {
    ascFile.addPos(input->tell());
    ascFile.addNote("Entries(UUSpreadSheetNext)");
  }
#endif
  return spreadsheetZone;
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool CWSpreadsheet::readZone1(CWSpreadsheetInternal::Spreadsheet &/*sheet*/)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos) {
    input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWSpreadsheet::readZone1: spreadsheet\n"));
    return false;
  }
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  if (sz == 0) {
    ascFile.addPos(pos);
    ascFile.addNote("Nop");
    return true;
  }
  int fSize = 0;
  switch(version()) {
  case 4:
  case 5:
    fSize = 4;
    break;
  case 6:
    fSize = 6;
    break;
  default:
    break;
  }
  if (!fSize) {
    ascFile.addPos(pos);
    ascFile.addNote("Entries(SpreadsheetZone1)");
    input->seek(endPos, WPX_SEEK_SET);
    return true;
  }
  long numElts = sz/fSize;
  if (numElts *fSize != sz) {
    input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWSpreadsheet::readZone1: unexpected size\n"));
    return false;
  }

  ascFile.addPos(pos);
  ascFile.addNote("Entries(SpreadsheetZone1)");

  libmwaw::DebugStream f;
  input->seek(pos+4, WPX_SEEK_SET);
  for (int i = 0; i < numElts; i++) {
    pos = input->tell();

    f.str("");
    f << "SpreadsheetZone1-" << i << ":";
    f << "row?=" << input->readLong(2) << ",";
    f << "col?=" << input->readLong(2) << ",";
    if (fSize == 6) {
      int val = (int) input->readLong(2);
      if (val != -1)
        f << "#unkn=" << val << ",";
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+fSize, WPX_SEEK_SET);
  }
  return true;
}

bool CWSpreadsheet::readContent(CWSpreadsheetInternal::Spreadsheet &sheet)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  /** ARGHH: this zone is almost the only zone which count the header in sz ... */
  long endPos = pos+sz;
  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos || sz < 6) {
    input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWSpreadsheet::readContent: file is too short\n"));
    return false;
  }

  input->seek(pos+4, WPX_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(SpreadsheetContent):";
  int N = (int) input->readULong(2);
  f << "N=" << N << ",";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  while (long(input->tell()) < endPos) {
    // Normally a list of name field : CTAB (COLM CHNK+)*
    pos = input->tell();
    sz = (long) input->readULong(4);
    long zoneEnd=pos+4+sz;
    if (zoneEnd > endPos || (sz && sz < 12)) {
      input->seek(pos, WPX_SEEK_SET);
      MWAW_DEBUG_MSG(("CWSpreadsheet::readContent: find a odd content field\n"));
      return false;
    }
    if (!sz) {
      ascFile.addPos(pos);
      ascFile.addNote("Nop");
      continue;
    }
    std::string name("");
    for (int i = 0; i < 4; i++)
      name+=char(input->readULong(1));
    f.str("");
    if (name=="COLM")
      readCOLM(sheet, zoneEnd);
    else if (name=="CTAB")
      readCTAB(sheet, zoneEnd);
    else if (name=="CHNK")
      CWStruct::readCHNKZone(*m_parserState, zoneEnd);
    else {
      MWAW_DEBUG_MSG(("CWSpreadsheet::readContent: find unexpected content field\n"));
      f << "SpreadsheetContent-" << name;
      ascFile.addDelimiter(input->tell(),'|');
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    input->seek(zoneEnd, WPX_SEEK_SET);
  }

  input->seek(endPos, WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////
bool CWSpreadsheet::readCOLM(CWSpreadsheetInternal::Spreadsheet &, long endPos)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(SpreadCOLM):";
  if (pos+8 > endPos) {
    MWAW_DEBUG_MSG(("CWSpreadsheet:readCOLM: the entries size seems bad\n"));
    f << "####";
    ascFile.addPos(pos-8);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  int cPos[2];
  for (int i=0; i < 2; ++i)
    cPos[i]=(int) input->readLong(2);
  f << "ptr[" << cPos[0] << "<=>" << cPos[1] << "]";
  if (pos+8+4*(cPos[1]-cPos[0]) != endPos) {
    MWAW_DEBUG_MSG(("CWSpreadsheet:readCOLM: the entries number of elements seems bad\n"));
    f << "####";
    ascFile.addPos(pos-8);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  f << "=[";
  for (int i=cPos[0]; i <= cPos[1]; i++) {
    long ptr=(long) input->readULong(4);
    if (ptr)
      f << std::hex << ptr << std::dec << ",";
    else
      f << "_,";
  }
  f << "],";
  ascFile.addPos(pos-8);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool CWSpreadsheet::readCTAB(CWSpreadsheetInternal::Spreadsheet &, long endPos)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(SpreadCTAB):";
  if (pos+1028 > endPos) {
    MWAW_DEBUG_MSG(("CWSpreadsheet:readCTAB: the entries size seems bad\n"));
    f << "####";
    ascFile.addPos(pos-8);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  int N=(int) input->readLong(2);
  if (N) f << "N=" << N << ",";
  int val=(int) input->readLong(2); // a small number between 0 and 0xff
  if (val) f << "f0=" << val << ",";
  if (N<0 || N>255) {
    MWAW_DEBUG_MSG(("CWSpreadsheet:readCTAB: the entries number of elements seems bad\n"));
    f << "####";
    ascFile.addPos(pos-8);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  f << "ptr=[";
  long ptr;
  for (int i=0; i <= N; i++) {
    ptr=(long) input->readULong(4);
    if (ptr)
      f << std::hex << ptr << std::dec << ",";
    else
      f << "_,";
  }
  f << "],";
  for (int i=N+1; i<256; i++) { // always 0
    ptr=(long) input->readULong(4);
    if (!ptr) continue;
    static bool first=true;
    if (first) {
      MWAW_DEBUG_MSG(("CWSpreadsheet:readCTAB: find some extra values\n"));
      first=false;
    }
    f << "#g" << i << "=" << ptr << ",";
  }
  ascFile.addPos(pos-8);
  ascFile.addNote(f.str().c_str());
  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
