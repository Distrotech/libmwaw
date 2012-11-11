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
  Spreadsheet(CWStruct::DSET const dset = CWStruct::DSET()) :
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
CWSpreadsheet::CWSpreadsheet
(MWAWInputStreamPtr ip, CWParser &parser, MWAWFontConverterPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert), m_state(new CWSpreadsheetInternal::State),
  m_mainParser(&parser), m_styleManager(parser.m_styleManager), m_asciiFile(parser.ascii())
{
}

CWSpreadsheet::~CWSpreadsheet()
{ }

int CWSpreadsheet::version() const
{
  return m_mainParser->version();
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
  m_input->seek(pos+8+16, WPX_SEEK_SET); // avoid header+8 generic number
  libmwaw::DebugStream f;
  shared_ptr<CWSpreadsheetInternal::Spreadsheet>
  spreadsheetZone(new CWSpreadsheetInternal::Spreadsheet(zone));

  f << "Entries(SpreadsheetDef):" << *spreadsheetZone << ",";
  ascii().addDelimiter(m_input->tell(), '|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  // read the last part
  long data0Length = zone.m_dataSz;
  long N = zone.m_numData;
  if (entry.length() -8-12 != data0Length*N + zone.m_headerSz) {
    if (data0Length == 0 && N) {
      MWAW_DEBUG_MSG(("CWSpreadsheet::readSpreadsheetZone: can not find definition size\n"));
      m_input->seek(entry.end(), WPX_SEEK_SET);
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
    m_input->seek(pos, WPX_SEEK_SET);
    f.str("");
    f << "Entries(SpreadsheetCol):";
    for (int i = 0; i < 256; i++)
      colSize.push_back((int)m_input->readULong(1));
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());

    ascii().addPos(m_input->tell());
    ascii().addNote("SpreadsheetDef-A");
  }

  long dataEnd = entry.end()-N*data0Length;
  int numLast = version()==6 ? 4 : 0;
  if (long(m_input->tell()) + data0Length + numLast <= dataEnd) {
    ascii().addPos(dataEnd-data0Length-numLast);
    ascii().addNote("SpreadsheetDef-_");
    if (numLast) {
      ascii().addPos(dataEnd-numLast);
      ascii().addNote("SpreadsheetDef-extra");
    }
  }
  m_input->seek(dataEnd, WPX_SEEK_SET);

  for (int i = 0; i < N; i++) {
    pos = m_input->tell();

    f.str("");
    f << "SpreadsheetDef-" << i;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+data0Length, WPX_SEEK_SET);
  }

  m_input->seek(entry.end(), WPX_SEEK_SET);

  if (m_state->m_spreadsheetMap.find(spreadsheetZone->m_id) != m_state->m_spreadsheetMap.end()) {
    MWAW_DEBUG_MSG(("CWSpreadsheet::readSpreadsheetZone: zone %d already exists!!!\n", spreadsheetZone->m_id));
  } else
    m_state->m_spreadsheetMap[spreadsheetZone->m_id] = spreadsheetZone;

  spreadsheetZone->m_otherChilds.push_back(spreadsheetZone->m_id+1);
  pos = m_input->tell();

  bool ok = readZone1(*spreadsheetZone);
  if (ok) {
    pos = m_input->tell();
    ok = m_mainParser->readStructZone("SpreadsheetZone2", false);
  }
  if (ok) {
    pos = m_input->tell();
    ok = readContent(*spreadsheetZone);
  }
  if (ok) {
    pos = m_input->tell();
    // Checkme: it is a simple list or a set of list ?
    // sometimes zero or a list of pair of int(pos?, id?)
    ok = m_mainParser->readStructZone("SpreadsheetListUnkn0", false);
  }

  if (!ok)
    m_input->seek(pos, WPX_SEEK_SET);
#if 0
  else {
    ascii().addPos(m_input->tell());
    ascii().addNote("Entries(UUSpreadSheetNext)");
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
  long pos = m_input->tell();
  long sz = (long) m_input->readULong(4);
  long endPos = pos+4+sz;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWSpreadsheet::readZone1: spreadsheet\n"));
    return false;
  }
  if (sz == 0) {
    ascii().addPos(pos);
    ascii().addNote("Nop");
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
    ascii().addPos(pos);
    ascii().addNote("Entries(SpreadsheetZone1)");
    m_input->seek(endPos, WPX_SEEK_SET);
    return true;
  }
  long numElts = sz/fSize;
  if (numElts *fSize != sz) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWSpreadsheet::readZone1: unexpected size\n"));
    return false;
  }

  ascii().addPos(pos);
  ascii().addNote("Entries(SpreadsheetZone1)");

  libmwaw::DebugStream f;
  m_input->seek(pos+4, WPX_SEEK_SET);
  for (int i = 0; i < numElts; i++) {
    pos = m_input->tell();

    f.str("");
    f << "SpreadsheetZone1-" << i << ":";
    f << "row?=" << m_input->readLong(2) << ",";
    f << "col?=" << m_input->readLong(2) << ",";
    if (fSize == 6) {
      int val = (int) m_input->readLong(2);
      if (val != -1)
        f << "#unkn=" << val << ",";
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+fSize, WPX_SEEK_SET);
  }
  return true;
}

bool CWSpreadsheet::readContent(CWSpreadsheetInternal::Spreadsheet &/*sheet*/)
{
  long pos = m_input->tell();
  long sz = (long) m_input->readULong(4);
  /** ARGHH: this zone is almost the only zone which count the header in sz ... */
  long endPos = pos+sz;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos || sz < 6) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWSpreadsheet::readContent: file is too short\n"));
    return false;
  }

  m_input->seek(pos+4, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(SpreadsheetContent):";
  int N = (int) m_input->readULong(2);
  f << "N=" << N << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  while (long(m_input->tell()) < endPos) {
    // Normally a list of name field : CTAB (COLM CHNK+)*
    pos = m_input->tell();
    sz = (long) m_input->readULong(4);
    if (pos+4+sz > endPos || (sz && sz < 12)) {
      m_input->seek(pos, WPX_SEEK_SET);
      MWAW_DEBUG_MSG(("CWSpreadsheet::readContent: find a odd content field\n"));
      return false;
    }
    if (!sz) {
      ascii().addPos(pos);
      ascii().addNote("Nop");
      continue;
    }
    std::string name("");
    for (int i = 0; i < 4; i++)
      name+=char(m_input->readULong(1));
    f.str("");
    f << "SpreadsheetContent-" << name;

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+4+sz, WPX_SEEK_SET);
  }

  m_input->seek(endPos, WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////


// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
