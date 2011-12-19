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

#include "CWTable.hxx"

#include "CWParser.hxx"
#include "CWStruct.hxx"

/** Internal: the structures of a CWTable */
namespace CWTableInternal
{
struct Zone {
};

////////////////////////////////////////
////////////////////////////////////////

struct Table : public CWStruct::DSET {
  Table(CWStruct::DSET const dset = CWStruct::DSET()) :
    CWStruct::DSET(dset), m_parsed(false) {
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Table const &doc) {
    o << static_cast<CWStruct::DSET const &>(doc);
    return o;
  }

  bool m_parsed;
};

////////////////////////////////////////
//! Internal: the state of a CWTable
struct State {
  //! constructor
  State() : m_tableMap() {
  }

  std::map<int, shared_ptr<Table> > m_tableMap;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
CWTable::CWTable
(TMWAWInputStreamPtr ip, CWParser &parser, MWAWTools::ConvertissorPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert), m_state(new CWTableInternal::State),
  m_mainParser(&parser), m_asciiFile(parser.ascii())
{
}

CWTable::~CWTable()
{ }

int CWTable::version() const
{
  return m_mainParser->version();
}

// fixme
int CWTable::numPages() const
{
  return 1;
}
////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// a document part
////////////////////////////////////////////////////////////
shared_ptr<CWStruct::DSET> CWTable::readTableZone
(CWStruct::DSET const &zone, IMWAWEntry const &entry, bool &complete)
{
  complete = false;
  if (!entry.valid() || zone.m_type != 6 || entry.length() < 32)
    return shared_ptr<CWStruct::DSET>();
  long pos = entry.begin();
  m_input->seek(pos+8+16, WPX_SEEK_SET); // avoid header+8 generic number
  libmwaw_tools::DebugStream f;
  shared_ptr<CWTableInternal::Table>
  tableZone(new CWTableInternal::Table(zone));

  f << "Entries(TableDef):" << *tableZone << ",";
  float dim[2];
  for (int i = 0; i < 2; i++) dim[i] = m_input->readLong(4)/256.;
  f << "dim=" << dim[0] << "x" << dim[1] << ",";
  int val;
  for (int i = 0; i < 3; i++) {
    // f1=parentZoneId ?
    val = m_input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascii().addDelimiter(m_input->tell(), '|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  // read the last part
  int data0Length = zone.m_dataSz;
  int N = zone.m_numData;
  if (entry.length() -8-12 != data0Length*N + zone.m_headerSz) {
    if (data0Length == 0 && N) {
      MWAW_DEBUG_MSG(("CWTable::readTableZone: can not find definition size\n"));
      m_input->seek(entry.end(), WPX_SEEK_SET);
      return shared_ptr<CWStruct::DSET>();
    }

    MWAW_DEBUG_MSG(("CWTable::readTableZone: unexpected size for zone definition, try to continue\n"));
  }

  if (long(m_input->tell())+N*data0Length > entry.end()) {
    MWAW_DEBUG_MSG(("CWTable::readTableZone: file is too short\n"));
    return shared_ptr<CWStruct::DSET>();
  }
  if (N) {
    MWAW_DEBUG_MSG(("CWTable::readTableZone: find some tabledef !!!\n"));
    m_input->seek(entry.end()-N*data0Length, WPX_SEEK_SET);

    for (int i = 0; i < N; i++) {
      pos = m_input->tell();

      f.str("");
      f << "TableDef#";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      m_input->seek(pos+data0Length, WPX_SEEK_SET);
    }
  }

  m_input->seek(entry.end(), WPX_SEEK_SET);

  if (m_state->m_tableMap.find(tableZone->m_id) != m_state->m_tableMap.end()) {
    MWAW_DEBUG_MSG(("CWTable::readTableZone: zone %d already exists!!!\n", tableZone->m_id));
  } else
    m_state->m_tableMap[tableZone->m_id] = tableZone;

  // fixme: in general followed by another zone
  tableZone->m_otherChilds.push_back(tableZone->m_id+1);

  pos = m_input->tell();
  bool ok = readTableBorders();
  if (ok) {
    pos = m_input->tell();
    ok = readTableCells();
  }

  if (!ok)
    m_input->seek(pos, WPX_SEEK_SET);

  return tableZone;
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool CWTable::readTableBorders()
{
  long pos = m_input->tell();
  long sz = m_input->readULong(4);
  long endPos = pos+4+sz;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWGraph::readTableBorders: file is too short\n"));
    return false;
  }

  m_input->seek(pos+4, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "Entries(TableBorders):";
  int N = m_input->readULong(2);
  f << "N=" << N << ",";
  int val = m_input->readLong(2);
  if (val != -1) f << "f0=" << val << ",";
  val = m_input->readLong(2);
  if (val) f << "f1=" << val << ",";
  int fSz = m_input->readLong(2);
  if (sz != 12+fSz*N || fSz < 18) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWGraph::readTableBorders: find odd data size\n"));
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
    f << "TableBorders-" << i << ":";
    float posi[4];
    for (int i = 0; i < 4; i++) posi[i] = m_input->readLong(4)/256.;
    f << posi[0] << "x" << posi[1] << "<->" << posi[2] << "x" << posi[3] << ",";
    int flags = m_input->readULong(2);
    f << "fl=" << std::hex << flags << std::dec << ",";
    if (long(m_input->tell()) != pos+fSz)
      ascii().addDelimiter(m_input->tell(), '|');
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+fSz, WPX_SEEK_SET);
  }

  m_input->seek(endPos, WPX_SEEK_SET);
  return true;
}

bool CWTable::readTableCells()
{
  long pos = m_input->tell();
  long sz = m_input->readULong(4);
  long endPos = pos+4+sz;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWGraph::readTableCells: file is too short\n"));
    return false;
  }

  m_input->seek(pos+4, WPX_SEEK_SET);
  libmwaw_tools::DebugStream f;
  f << "Entries(TableCell):";
  int N = m_input->readULong(2);
  f << "N=" << N << ",";
  int val = m_input->readLong(2);
  if (val != -1) f << "f0=" << val << ",";
  val = m_input->readLong(2);
  if (val) f << "f1=" << val << ",";
  int fSz = m_input->readLong(2);
  if (sz != 12+fSz*N || fSz < 32) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWGraph::readTableCells: find odd data size\n"));
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
    f << "TableCell-" << i << ":";
    float posi[6];
    for (int i = 0; i < 6; i++) posi[i] = m_input->readLong(4)/256.;
    f << posi[0] << "x" << posi[1] << "<->" << posi[2] << "x" << posi[3] << ",";
    f << "sz=" << posi[4] << "x" << posi[5] << ",";
    f << "zoneId=" << m_input->readULong(4) << ",";
    f << "styleId?=" << m_input->readULong(4) << ",";

    if (long(m_input->tell()) != pos+fSz)
      ascii().addDelimiter(m_input->tell(), '|');
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+fSz, WPX_SEEK_SET);
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
