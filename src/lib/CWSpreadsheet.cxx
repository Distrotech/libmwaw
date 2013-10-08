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

#include "MWAWCell.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWTable.hxx"

#include "CWDbaseContent.hxx"
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
    CWStruct::DSET(dset), m_colWidths(), m_rowHeightMap(), m_content() {
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Spreadsheet const &doc) {
    o << static_cast<CWStruct::DSET const &>(doc);
    return o;
  }
  //! the columns width
  std::vector<int> m_colWidths;
  //! a map row to height
  std::map<int, int> m_rowHeightMap;
  //! the data
  shared_ptr<CWDbaseContent> m_content;
};

//! Internal: the state of a CWSpreadsheet
struct State {
  //! constructor
  State() : m_spreadsheetMap() {
  }
  //! a map zoneId to spreadsheet
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
  sheet(new CWSpreadsheetInternal::Spreadsheet(zone));

  f << "Entries(SpreadsheetDef):" << *sheet << ",";
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

  sheet->m_colWidths.resize(0);
  sheet->m_colWidths.resize(256,36);
  if (debColSize) {
    pos = entry.begin()+debColSize;
    input->seek(pos, WPX_SEEK_SET);
    f.str("");
    f << "Entries(SpreadsheetCol):width,";
    for (size_t i = 0; i < 256; ++i) {
      int w=(int)input->readULong(1);
      sheet->m_colWidths[i]=w;
      if (w!=36) // default
        f << "w" << i << "=" << w << ",";
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

  if (m_state->m_spreadsheetMap.find(sheet->m_id) != m_state->m_spreadsheetMap.end()) {
    MWAW_DEBUG_MSG(("CWSpreadsheet::readSpreadsheetZone: zone %d already exists!!!\n", sheet->m_id));
  } else
    m_state->m_spreadsheetMap[sheet->m_id] = sheet;

  sheet->m_otherChilds.push_back(sheet->m_id+1);
  pos = input->tell();

  bool ok = readZone1(*sheet);
  if (ok) {
    pos = input->tell();
    ok = m_mainParser->readStructZone("SpreadsheetZone2", false);
  }
  if (ok) {
    pos = input->tell();
    shared_ptr<CWDbaseContent> content(new CWDbaseContent(m_parserState, m_styleManager, true));
    ok = content->readContent();
    if (ok) sheet->m_content=content;
  }
  if (ok) {
    pos = input->tell();
    if (!readRowHeightZone(*sheet)) {
      input->seek(pos, WPX_SEEK_SET);
      ok = m_mainParser->readStructZone("SpreadsheetRowHeight", false);
    }
  }

  if (!ok)
    input->seek(pos, WPX_SEEK_SET);
#if 0
  else {
    ascFile.addPos(input->tell());
    ascFile.addNote("Entries(UUSpreadSheetNext)");
  }
#endif
  return sheet;
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

bool CWSpreadsheet::readRowHeightZone(CWSpreadsheetInternal::Spreadsheet &sheet)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos=pos+4+sz;
  if (!input->checkPosition(endPos)) {
    input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWSpreadsheet::readRowHeightZone: unexpected size\n"));
    return false;
  }

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  if (sz == 0) {
    ascFile.addPos(pos);
    ascFile.addNote("NOP");
    return true;
  }

  f << "Entries(SpreadsheetRowHeight):";
  int N = (int) input->readLong(2);
  f << "N=" << N << ",";
  int type = (int) input->readLong(2);
  if (type != -1)
    f << "#type=" << type << ",";
  int val = (int) input->readLong(2);
  if (val) f << "#unkn=" << val << ",";
  int fSz = (int) input->readULong(2);
  int hSz = (int) input->readULong(2);
  if (fSz!=4 || N *fSz+hSz+12 != sz) {
    input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWSpreadsheet::readRowHeightZone: unexpected size for fieldSize\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(endPos, WPX_SEEK_SET);
    return true;
  }
  if (long(input->tell()) != pos+4+hSz)
    ascFile.addDelimiter(input->tell(), '|');
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  long debPos = endPos-N*4;
  input->seek(debPos, WPX_SEEK_SET);
  for (int i = 0; i < N; i++) {
    pos = input->tell();

    f.str("");
    f << "SpreadsheetRowHeightZone-" << i << ":";
    int row=(int) input->readLong(2);
    int h=(int) input->readLong(2);
    sheet.m_rowHeightMap[row]=h;
    f << "row=" << row << ", height=" << h << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
//
// send data
//
////////////////////////////////////////////////////////////
bool CWSpreadsheet::sendSpreadsheet(int zId)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("CWSpreadsheet::sendSpreadsheet: called without any listener\n"));
    return false;
  }
  std::map<int, shared_ptr<CWSpreadsheetInternal::Spreadsheet> >::iterator it=
    m_state->m_spreadsheetMap.find(zId);
  if (it == m_state->m_spreadsheetMap.end() || !it->second) {
    MWAW_DEBUG_MSG(("CWSpreadsheet::readSpreadsheetZone: can not find zone %d!!!\n", zId));
    return false;
  }
  CWSpreadsheetInternal::Spreadsheet &sheet=*it->second;
  Vec2i minData, maxData;
  if (!sheet.m_content || !sheet.m_content->getExtrema(minData,maxData)) {
    MWAW_DEBUG_MSG(("CWSpreadsheet::sendSpreadsheet: can not find content\n"));
    return false;
  }
  std::vector<float> colSize((size_t)(maxData[0]-minData[0]+1),36);
  for (int c=minData[0], fC=0; c <= maxData[0]; ++c, ++fC) {
    if (c>=0 && c < int(sheet.m_colWidths.size()))
      colSize[size_t(fC)]=(float) sheet.m_colWidths[size_t(c)];
  }
  MWAWTable table(MWAWTable::TableDimBit);
  table.setColsSize(colSize);
  listener->openTable(table);
  for (int r=minData[1], fR=0; r <= maxData[1]; ++r, ++fR) {
    if (sheet.m_rowHeightMap.find(r)!=sheet.m_rowHeightMap.end())
      listener->openTableRow((float)sheet.m_rowHeightMap.find(r)->second, WPX_POINT);
    else
      listener->openTableRow((float)12, WPX_POINT);
    for (int c=minData[0], fC=0; c <= maxData[0]; ++c, ++fC) {
      MWAWCell cell;
      cell.setPosition(Vec2i(fC,fR));
      listener->openTableCell(cell);
      sheet.m_content->send(Vec2i(c, r));
      listener->closeTableCell();
    }
    listener->closeTableRow();
  }
  listener->closeTable();
  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
