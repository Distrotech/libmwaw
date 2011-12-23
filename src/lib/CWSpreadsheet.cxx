/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libmwaw
 * Copyright (C) 2009, 2011 Alonso Laurent (alonso@loria.fr)
 * Copyright (C) 2006, 2007 Andrew Ziem
 * Copyright (C) 2004-2006 Fridrich Strba (fridrich.strba@bluewin.ch)
 * Copyright (C) 2004 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2003-2005 William Lachance (william.lachance@sympatico.ca)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 * For further information visit http://libmwaw.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>

#include <libwpd/WPXString.h>

#include "TMWAWPictBasic.hxx"
#include "TMWAWPictMac.hxx"
#include "TMWAWPosition.hxx"

#include "MWAWStruct.hxx"
#include "MWAWTools.hxx"
#include "MWAWContentListener.hxx"

#include "CWSpreadsheet.hxx"

#include "CWParser.hxx"
#include "CWStruct.hxx"

/** Internal: the structures of a CWSpreadsheet */
namespace CWSpreadsheetInternal
{
////////////////////////////////////////
////////////////////////////////////////

struct Spreadsheet : public CWStruct::DSET {
  Spreadsheet(CWStruct::DSET const dset = CWStruct::DSET()) :
    CWStruct::DSET(dset), m_parsed(false) {
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Spreadsheet const &doc) {
    o << static_cast<CWStruct::DSET const &>(doc);
    return o;
  }

  bool m_parsed;
};

////////////////////////////////////////
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
(TMWAWInputStreamPtr ip, CWParser &parser, MWAWTools::ConvertissorPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert), m_state(new CWSpreadsheetInternal::State),
  m_mainParser(&parser), m_asciiFile(parser.ascii())
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
(CWStruct::DSET const &zone, IMWAWEntry const &entry, bool &complete)
{
  complete = false;
  if (!entry.valid() || zone.m_type != 2 || entry.length() < 256)
    return shared_ptr<CWStruct::DSET>();
  long pos = entry.begin();
  m_input->seek(pos+8+16, WPX_SEEK_SET); // avoid header+8 generic number
  libmwaw_tools::DebugStream f;
  shared_ptr<CWSpreadsheetInternal::Spreadsheet>
  spreadsheetZone(new CWSpreadsheetInternal::Spreadsheet(zone));

  f << "Entries(SpreadsheetDef):" << *spreadsheetZone << ",";
  ascii().addDelimiter(m_input->tell(), '|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  // read the last part
  int data0Length = zone.m_dataSz;
  int N = zone.m_numData;
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
      colSize.push_back(m_input->readULong(1));
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

  // fixme: in general followed by the layout zone
  spreadsheetZone->m_otherChilds.push_back(spreadsheetZone->m_id+1);

  pos = m_input->tell();

  bool ok = readZone1(*spreadsheetZone);
  if (ok) {
    pos = m_input->tell();
    ok = readZone2(*spreadsheetZone);
  }
  if (ok) {
    pos = m_input->tell();
    ok = readContent(*spreadsheetZone);
  }

  if (!ok)
    m_input->seek(pos, WPX_SEEK_SET);

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
  long sz = m_input->readULong(4);
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
  int numElts = sz/fSize;
  if (numElts*fSize != sz) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWSpreadsheet::readZone1: unexpected size\n"));
    return false;
  }

  ascii().addPos(pos);
  ascii().addNote("Entries(SpreadsheetZone1)");

  libmwaw_tools::DebugStream f;
  m_input->seek(pos+4, WPX_SEEK_SET);
  for (int i = 0; i < numElts; i++) {
    pos = m_input->tell();

    f.str("");
    f << "SpreadsheetZone1-" << i << ":";
    f << "row?=" << m_input->readLong(2) << ",";
    f << "col?=" << m_input->readLong(2) << ",";
    if (fSize == 6) {
      int val = m_input->readLong(2);
      if (val != -1)
        f << "#unkn=" << val << ",";
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+fSize, WPX_SEEK_SET);
  }
  return true;
}

bool CWSpreadsheet::readZone2(CWSpreadsheetInternal::Spreadsheet &/*sheet*/)
{
  long pos = m_input->tell();
  long sz = m_input->readULong(4);
  long endPos = pos+4+sz;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWSpreadsheet::readZone2: spreadsheet\n"));
    return false;
  }
  if (sz == 0) {
    ascii().addPos(pos);
    ascii().addNote("Nop");
    return true;
  }

  m_input->seek(pos+4, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "Entries(SpreadsheetZone2):";
  int N = m_input->readULong(2);
  f << "N=" << N << ",";
  int val = m_input->readLong(2);
  if (val != -1) f << "f0=" << val << ",";
  val = m_input->readLong(2);
  if (val) f << "f1=" << val << ",";
  int fSz = m_input->readLong(2);
  if (sz != 12+fSz*N) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWSpreadsheet::readZone2: find odd data size\n"));
    return false;
  }
  for (int i = 2; i < 4; i++) {
    val = m_input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";

  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i = 0; i < N; i++) {
    pos = m_input->tell();
    f.str("");
    f << "SpreadsheetZone2-" << i << ":";

    m_input->seek(pos+fSz, WPX_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  m_input->seek(endPos, WPX_SEEK_SET);
  return true;
}

bool CWSpreadsheet::readContent(CWSpreadsheetInternal::Spreadsheet &/*sheet*/)
{
  long pos = m_input->tell();
  long sz = m_input->readULong(4);
  /** ARGHH: this zone is almost the only zone which count the header in sz ... */
  long endPos = pos+sz;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos || sz < 6) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWSpreadsheet::readContent: file is too short\n"));
    return false;
  }

  m_input->seek(pos+4, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "Entries(SpreadsheetContent):";
  int N = m_input->readULong(2);
  f << "N=" << N << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  while (long(m_input->tell()) < endPos) {
    // Normally a list of name field : CTAB (COLM CHNK+)*
    pos = m_input->tell();
    sz = m_input->readULong(4);
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
