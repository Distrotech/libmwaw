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
#include <set>
#include <sstream>

#include <libwpd/WPXString.h>

#include "MWAWCell.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWPictBasic.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"
#include "MWAWTable.hxx"

#include "CWParser.hxx"
#include "CWStruct.hxx"

#include "CWTable.hxx"

/** Internal: the structures of a CWTable */
namespace CWTableInternal
{
struct Border {
  Border() : m_flags(0) {}
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Border const &bord) {
    for (int i = 0; i < 2; i++) {
      Vec2f pos = bord.m_position[i];
      pos=1./256.*pos;
      o << pos;
      if (i == 0) o << "<->";
      else o << ",";
    }
    if (bord.m_flags)
      o << "id/flags?=" << bord.m_flags << ",";
    return o;
  }

  /* the origin and the end of edge position : unit WPX_POINT/256 */
  Vec2i m_position[2];
  // some flags : style id ?
  int m_flags;
};

struct Cell : public MWAWTableCell {
  Cell() : MWAWTableCell(), m_size(), m_zoneId(0), m_styleId(-1) {
  }

  virtual bool send(MWAWContentListenerPtr listener) {
    if (!listener) return true;
    MWAWCell cell;
    cell.position() = m_position;
    cell.setNumSpannedCells(m_numberCellSpanned);

    listener->openTableCell(cell, WPXPropertyList());
    listener->insertCharacter(' ');
    listener->closeTableCell();
    return true;
  }

  virtual bool sendContent(MWAWContentListenerPtr listener) {
    if (!listener) return true;
    listener->insertCharacter(' ');
    return true;
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Cell const &cell) {
    o << reinterpret_cast<MWAWTableCell const &>(cell);
    Vec2f sz = cell.m_size;
    o << "size=" << sz << ",";
    if (cell.m_zoneId) o << "zone=" << cell.m_zoneId << ",";
    if (cell.m_styleId >= 0)o << "style=" << cell.m_styleId << ",";
    return o;
  }

  /* the cell size : unit WPX_POINT */
  Vec2f m_size;
  /* the cell zone ( 0 is no content ) */
  int m_zoneId;
  /* the list of border id : Left, Top, Right, Bottom

     Normally, one id but merge cells can have mutiple border
  */
  std::vector<int> m_bordersId[4];
  // the style id ?
  int m_styleId;

};

////////////////////////////////////////
////////////////////////////////////////
struct Table : public CWStruct::DSET, public MWAWTable {
  //! constructor
  Table(CWStruct::DSET const dset = CWStruct::DSET()) :
    CWStruct::DSET(dset),MWAWTable(), m_bordersList(), m_parsed(false) {
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Table const &doc) {
    o << static_cast<CWStruct::DSET const &>(doc);
    return o;
  }
  //! return a cell corresponding to id
  Cell *get(int id) {
    if (id < 0 || id >= numCells()) {
      MWAW_DEBUG_MSG(("CWTableInteral::Table::get: cell %d does not exists\n",id));
      return 0;
    }
    return reinterpret_cast<Cell *>(MWAWTable::get(id).get());
  }

  std::vector<Border> m_bordersList;
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
(MWAWInputStreamPtr ip, CWParser &parser, MWAWFontConverterPtr &convert) :
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
(CWStruct::DSET const &zone, MWAWEntry const &entry, bool &complete)
{
  complete = false;
  if (!entry.valid() || zone.m_type != 6 || entry.length() < 32)
    return shared_ptr<CWStruct::DSET>();
  long pos = entry.begin();
  m_input->seek(pos+8+16, WPX_SEEK_SET); // avoid header+8 generic number
  libmwaw::DebugStream f;
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

  pos = m_input->tell();
  bool ok = readTableBorders(*tableZone);
  if (ok) {
    pos = m_input->tell();
    ok = readTableCells(*tableZone);
  }
  /** three fields which seems to follows the list of cells
      zone 0 : looks like a list of integer : related to last selected border ?
      zone 1 : looks like a list of integer : unknown meaning
      zone 2 : looks like a list of 4 pointers
   */
  for (int i = 0; ok && i < 3; i++) {
    std::stringstream s;
    s << "TableUnknown-" << i;
    pos = m_input->tell();
    ok = m_mainParser->readStructZone(s.str().c_str(), false);
  }
  if (ok) {
    pos = m_input->tell();
    ok = readTableBordersId(*tableZone);
  }

  if (!ok)
    m_input->seek(pos, WPX_SEEK_SET);

  if (m_state->m_tableMap.find(tableZone->m_id) != m_state->m_tableMap.end()) {
    MWAW_DEBUG_MSG(("CWTable::readTableZone: zone %d already exists!!!\n", tableZone->m_id));
  } else
    m_state->m_tableMap[tableZone->m_id] = tableZone;

  // fixme: in general followed by another zone
  tableZone->m_otherChilds.push_back(tableZone->m_id+1);

  return tableZone;
}

bool CWTable::sendZone(int number)
{
  std::map<int, shared_ptr<CWTableInternal::Table> >::iterator iter
    = m_state->m_tableMap.find(number);
  if (iter == m_state->m_tableMap.end())
    return false;
  shared_ptr<CWTableInternal::Table> table = iter->second;
  table->m_parsed = true;

  if (!m_listener)
    return true;

  if (table->sendTable(m_listener))
    return true;
  return table->sendAsText(m_listener);
}

void CWTable::flushExtra()
{
  std::map<int, shared_ptr<CWTableInternal::Table> >::iterator iter
    = m_state->m_tableMap.begin();
  for ( ; iter !=  m_state->m_tableMap.end(); iter++) {
    shared_ptr<CWTableInternal::Table> table = iter->second;
    if (table->m_parsed)
      continue;
    if (m_listener) m_listener->insertEOL();
#ifdef DEBUG
    sendZone(iter->first);
#endif
  }
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool CWTable::readTableBorders(CWTableInternal::Table &table)
{
  long pos = m_input->tell();
  long sz = m_input->readULong(4);
  long endPos = pos+4+sz;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWTable::readTableBorders: file is too short\n"));
    return false;
  }

  m_input->seek(pos+4, WPX_SEEK_SET);
  libmwaw::DebugStream f;
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
    MWAW_DEBUG_MSG(("CWTable::readTableBorders: find odd data size\n"));
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
    CWTableInternal::Border border;
    f.str("");
    f << "TableBorders-" << i << ":";
    int posi[4];
    for (int i = 0; i < 4; i++) posi[i] = m_input->readLong(4);
    border.m_position[0] = Vec2i(posi[1], posi[0]);
    border.m_position[1] = Vec2i(posi[3], posi[2]);
    border.m_flags = m_input->readULong(2);
    table.m_bordersList.push_back(border);
    f << border;
    if (long(m_input->tell()) != pos+fSz)
      ascii().addDelimiter(m_input->tell(), '|');
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+fSz, WPX_SEEK_SET);
  }

  m_input->seek(endPos, WPX_SEEK_SET);
  return true;
}

bool CWTable::readTableCells(CWTableInternal::Table &table)
{
  long pos = m_input->tell();
  long sz = m_input->readULong(4);
  long endPos = pos+4+sz;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWTable::readTableCells: file is too short\n"));
    return false;
  }

  m_input->seek(pos+4, WPX_SEEK_SET);
  libmwaw::DebugStream f;
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
    MWAW_DEBUG_MSG(("CWTable::readTableCells: find odd data size\n"));
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
    shared_ptr<CWTableInternal::Cell> cell(new CWTableInternal::Cell);
    int posi[6];
    for (int j = 0; j < 6; j++) posi[j] = m_input->readLong(4);
    Box2f box = Box2f(Vec2f(posi[1], posi[0]), Vec2f(posi[3], posi[2]));
    box.scale(1./256.);
    cell->setBox(box);
    cell->m_size = 1./256.*Vec2f(posi[5], posi[4]);
    cell->m_zoneId = m_input->readULong(4);
    cell->m_styleId = m_input->readULong(4);
    table.add(cell);
    f.str("");
    f << "TableCell-" << i << ":" << *cell;
    if (long(m_input->tell()) != pos+fSz)
      ascii().addDelimiter(m_input->tell(), '|');
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+fSz, WPX_SEEK_SET);
  }

  m_input->seek(endPos, WPX_SEEK_SET);
  return true;
}

bool CWTable::readTableBordersId(CWTableInternal::Table &table)
{
  int numCells = table.numCells();
  int numBorders = table.m_bordersList.size();
  for (int i = 0; i < 4*numCells; i++) {
    CWTableInternal::Cell *cell = table.get(i/4);
    long pos = m_input->tell();
    long sz = m_input->readULong(4);
    long endPos = pos+4+sz;
    m_input->seek(endPos, WPX_SEEK_SET);
    if (long(m_input->tell()) != endPos) {
      m_input->seek(pos, WPX_SEEK_SET);
      MWAW_DEBUG_MSG(("CWTable::readTableBordersId: file is too short\n"));
      return false;
    }

    m_input->seek(pos+4, WPX_SEEK_SET);
    libmwaw::DebugStream f;
    f << "Entries(TableBordersId)[" << i/4 << "," << i%4 << "],";
    int N = m_input->readULong(2);
    f << "N=" << N << ",";
    int val = m_input->readLong(2);
    if (val != -1) f << "f0=" << val << ",";
    val = m_input->readLong(2);
    if (val) f << "f1=" << val << ",";
    int fSz = m_input->readLong(2);
    if (N==0 || sz != 12+fSz*N || fSz < 2) {
      m_input->seek(pos, WPX_SEEK_SET);
      MWAW_DEBUG_MSG(("CWTable::readTableBordersId: find odd data size\n"));
      return false;
    }
    for (int j = 2; j < 4; j++) {
      val = m_input->readLong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }

    std::vector<int> idsList;
    for (int j = 0; j < N; j++) {
      int id = m_input->readLong(2);
      if (id < 0 || id >= numBorders) {
        m_input->seek(pos, WPX_SEEK_SET);
        MWAW_DEBUG_MSG(("CWTable::readTableBordersId: unexpected id\n"));
        return false;
      }
      idsList.push_back(id);
      if (j)
        f << "bordId" << j << "=" << id << ",";
      else
        f << "bordId=" << id << ",";
    }
    if (cell)
      cell->m_bordersId[i%4] = idsList;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(endPos, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////


// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
