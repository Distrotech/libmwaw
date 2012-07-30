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

#include <libwpd/WPXString.h>

#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWPictBasic.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"

#include "CWParser.hxx"
#include "CWStruct.hxx"

#include "CWDatabase.hxx"

/** Internal: the structures of a CWDatabase */
namespace CWDatabaseInternal
{
struct Field {
  // the type
  enum Type { F_Unknown, F_Text, F_Number, F_Date, F_Time,
              F_Formula, F_FormulaSum,
              F_Checkbox, F_PopupMenu, F_RadioButton, F_ValueList,
              F_Multimedia
            };
  Field() : m_type(F_Unknown), m_defType(-1), m_name(""), m_default("") {
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Field const &field) {
    switch(field.m_type) {
    case F_Text :
      o << "text,";
      break;
    case F_Number :
      o << "number,";
      break;
    case F_Date :
      o << "date,";
      break;
    case F_Time :
      o << "time,";
      break;
    case F_Formula :
      o << "formula,";
      break;
    case F_FormulaSum :
      o << "formula(summary),";
      break;
    case F_Checkbox :
      o << "checkbox,";
      break;
    case F_PopupMenu :
      o << "popupMenu,";
      break;
    case F_RadioButton :
      o << "radioButton,";
      break;
    case F_ValueList:
      o << "valueList,";
      break;
    case F_Multimedia :
      o << "multimedia,";
      break;
    case F_Unknown :
    default:
      o << "type=#unknown,";
      break;
    }
    o << "'" << field.m_name << "',";
    switch(field.m_defType) {
    case -1:
      break;
    case 0:
      break;
    case 3:
      o << "recordInfo,";
      break;
    case 7:
      o << "serial";
      break;
    case 8:
      o << "hasDef,";
      break; // text with default
    case 9:
      o << "popup/radio/control,";
      break; // with default value ?
    default:
      o << "#defType=" << field.m_defType << ",";
      break;
    }
    if (field.m_default.length())
      o << "defaultVal='" << field.m_default << "',";
    return o;
  }

  bool isText() const {
    return m_type == F_Text;
  }
  bool isFormula() const {
    return m_type == F_Formula || m_type == F_FormulaSum;
  }

  int getNumDefault(int version) const {
    switch(m_type) {
    case F_Text :
      if (version >= 4)
        return 1;
      if (m_defType == 8) return 1;
      return 0;
    case F_Number :
    case F_Date :
    case F_Time :
    case F_Multimedia :
      return 0;
    case F_Formula :
    case F_FormulaSum :
      return 1;
    case F_Checkbox :
      return 1;
    case F_PopupMenu :
    case F_RadioButton :
    case F_ValueList :
      return 2;
    case F_Unknown :
    default:
      break;
    }
    return 0;
  }

  Type m_type;
  int m_defType;
  std::string m_name, m_default;
};

////////////////////////////////////////
////////////////////////////////////////

struct Database : public CWStruct::DSET {
  Database(CWStruct::DSET const dset = CWStruct::DSET()) :
    CWStruct::DSET(dset), m_fields(), m_parsed(false) {
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Database const &doc) {
    o << static_cast<CWStruct::DSET const &>(doc);
    return o;
  }

  std::vector<Field> m_fields;
  bool m_parsed;
};

////////////////////////////////////////
//! Internal: the state of a CWDatabase
struct State {
  //! constructor
  State() : m_databaseMap() {
  }

  std::map<int, shared_ptr<Database> > m_databaseMap;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
CWDatabase::CWDatabase
(MWAWInputStreamPtr ip, CWParser &parser, MWAWFontConverterPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert), m_state(new CWDatabaseInternal::State),
  m_mainParser(&parser), m_asciiFile(parser.ascii())
{
}

CWDatabase::~CWDatabase()
{ }

int CWDatabase::version() const
{
  return m_mainParser->version();
}

// fixme
int CWDatabase::numPages() const
{
  return 1;
}
////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// a document part
////////////////////////////////////////////////////////////
shared_ptr<CWStruct::DSET> CWDatabase::readDatabaseZone
(CWStruct::DSET const &zone, MWAWEntry const &entry, bool &complete)
{
  complete = false;
  if (!entry.valid() || zone.m_type != 3 || entry.length() < 32)
    return shared_ptr<CWStruct::DSET>();
  long pos = entry.begin();
  m_input->seek(pos+8+16, WPX_SEEK_SET); // avoid header+8 generic number
  libmwaw::DebugStream f;
  shared_ptr<CWDatabaseInternal::Database>
  databaseZone(new CWDatabaseInternal::Database(zone));

  f << "Entries(DatabaseDef):" << *databaseZone << ",";
  ascii().addDelimiter(m_input->tell(), '|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  // read the last part
  long data0Length = zone.m_dataSz;
  long N = zone.m_numData;
  if (entry.length() -8-12 != data0Length*N + zone.m_headerSz) {
    if (data0Length == 0 && N) {
      MWAW_DEBUG_MSG(("CWDatabase::readDatabaseZone: can not find definition size\n"));
      m_input->seek(entry.end(), WPX_SEEK_SET);
      return shared_ptr<CWStruct::DSET>();
    }

    MWAW_DEBUG_MSG(("CWDatabase::readDatabaseZone: unexpected size for zone definition, try to continue\n"));
  }

  long dataEnd = entry.end()-N*data0Length;
  int numLast = -1;
  switch(version()) {
  case 1:
  case 2:
    numLast = 0;
    break;
  case 6:
    numLast = 8;
    break;
  default:
    MWAW_DEBUG_MSG(("CWDatabase::readDatabaseZone: unexpected version\n"));
    break;
  }
  if (numLast >= 0 && long(m_input->tell()) + data0Length + numLast <= dataEnd) {
    ascii().addPos(dataEnd-data0Length-numLast);
    ascii().addNote("DatabaseDef-_");
    if (numLast) {
      ascii().addPos(dataEnd-numLast);
      ascii().addNote("DatabaseDef-extra");
    }
  }
  m_input->seek(dataEnd, WPX_SEEK_SET);

  for (int i = 0; i < N; i++) {
    pos = m_input->tell();

    f.str("");
    f << "DatabaseDef-" << i;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+data0Length, WPX_SEEK_SET);
  }

  m_input->seek(entry.end(), WPX_SEEK_SET);

  if (m_state->m_databaseMap.find(databaseZone->m_id) != m_state->m_databaseMap.end()) {
    MWAW_DEBUG_MSG(("CWDatabase::readDatabaseZone: zone %d already exists!!!\n", databaseZone->m_id));
  } else
    m_state->m_databaseMap[databaseZone->m_id] = databaseZone;

  // fixme: in general followed by the layout zone
  databaseZone->m_otherChilds.push_back(databaseZone->m_id+1);

  pos = m_input->tell();
  bool ok = readDatabaseFields(*databaseZone);

  if (ok) {
    ok = readDatabaseDefaults(*databaseZone);
    pos = m_input->tell();
  }
  if (ok) {
    pos = m_input->tell();
    ok = m_mainParser->readStructZone("DatabaseListUnkn", false);
  }
  if (ok) {
    pos = m_input->tell();
    // probably: field number followed by 1 : increasing, 2 : decreasing
    ok = m_mainParser->readStructZone("DatabaseSortFunction", false);
  }
  if (ok) {
    pos = m_input->tell();
    ok = readDatabaseContent(*databaseZone);
  }
  if (!ok)
    m_input->seek(pos, WPX_SEEK_SET);

  return databaseZone;
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool CWDatabase::readDatabaseFields(CWDatabaseInternal::Database &dBase)
{
  long pos = m_input->tell();
  long sz = (long) m_input->readULong(4);
  long endPos = pos+4+sz;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWDatabase::readDatabaseFields: file is too short\n"));
    return false;
  }

  m_input->seek(pos+4, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(DatabaseField):";
  int N = (int) m_input->readULong(2);
  f << "N=" << N << ",";
  int val = (int) m_input->readLong(2);
  if (val != -1) f << "f0=" << val << ",";
  val = (int) m_input->readLong(2);
  if (val) f << "f1=" << val << ",";
  int fSz = (int) m_input->readLong(2);
  if (sz != 12+fSz*N || fSz < 18) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWDatabase::readDatabaseFields: find odd data size\n"));
    return false;
  }
  for (int i = 2; i < 4; i++) {
    val = (int) m_input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";

  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  dBase.m_fields.resize(size_t(N));
  for (size_t i = 0; i < size_t(N); i++) {
    pos = m_input->tell();
    f.str("");
    f << "DatabaseField-" << i << ":";
    CWDatabaseInternal::Field &field = dBase.m_fields[i];

    int fNameMaxSz = 64;
    std::string name("");
    sz = (long) m_input->readULong(1);
    if ((fNameMaxSz && sz > fNameMaxSz-1) || sz > fSz-1) {
      m_input->seek(pos, WPX_SEEK_SET);
      MWAW_DEBUG_MSG(("CWDatabase::readDatabaseFields: find odd field name\n"));
      return false;
    }
    for (int j = 0; j < sz; j++)
      name += char(m_input->readULong(1));
    field.m_name = name;

    m_input->seek(pos+fNameMaxSz, WPX_SEEK_SET);
    int type = (int) m_input->readULong(1);
    bool ok = true;
    switch(type) {
      // or name
    case 0:
      field.m_type = CWDatabaseInternal::Field::F_Text;
      break;
    case 1:
      field.m_type = CWDatabaseInternal::Field::F_Number;
      break;
    case 2:
      field.m_type = CWDatabaseInternal::Field::F_Date;
      break;
    case 3:
      field.m_type = CWDatabaseInternal::Field::F_Time;
      break;
    case 4:
      if (version() <= 2)
        field.m_type = CWDatabaseInternal::Field::F_Formula;
      else
        field.m_type = CWDatabaseInternal::Field::F_PopupMenu;
      break;
    case 5:
      if (version() <= 2)
        field.m_type = CWDatabaseInternal::Field::F_FormulaSum;
      else
        field.m_type = CWDatabaseInternal::Field::F_Checkbox;
      break;
    case 6:
      field.m_type = CWDatabaseInternal::Field::F_RadioButton;
      break;
    case 7:
      if (version() == 4)
        field.m_type = CWDatabaseInternal::Field::F_Formula;
      else
        field.m_type = CWDatabaseInternal::Field::F_Multimedia;
      break;
    case 8:
      if (version() == 4)
        field.m_type = CWDatabaseInternal::Field::F_FormulaSum;
      else
        ok = false;
      break;
    case 10:
      field.m_type = CWDatabaseInternal::Field::F_Formula;
      break;
    case 11:
      field.m_type = CWDatabaseInternal::Field::F_FormulaSum;
      break;
    default:
      ok = false;
      break;
    }
    if (!ok)
      f << "#type=" << type << ",";
    val = (int) m_input->readULong(1);
    if (val)
      f << "#unkn=" << val << ",";
    unsigned long ptr = m_input->readULong(4);
    if (ptr) // set for formula
      f << "ptr=" << std::hex << ptr << std::dec << ",";
    f << "fl?=[" << std::hex;
    f << m_input->readULong(2) << ",";
    f << m_input->readULong(1) << ",";
    for (int j = 0; j < 6; j++) {
      // some int which seems constant on the database...
      val = (int) m_input->readULong(2);
      f <<  val << ",";
    }
    f << std::dec << "],";

    if (version() > 1) {
      for (int j = 0; j < 16; j++) {
        /** find f1=600 for a number
            f16 = 0[checkbox, ... ], 2[number or text],3 [name field], 82[value list],
            f16 & 8: can not be empty
        */
        val = (int) m_input->readLong(2);
        if (val) f << "f" << j << "=" << std::hex << val << std::dec << ",";
      }
      int subType = (int) m_input->readULong(2);
      if (version() == 2) {
        if (subType) f << "f17=" << std::hex << subType << std::dec << ",";
      } else {
        if (subType & 0x80 && field.m_type == CWDatabaseInternal::Field::F_Text) {
          field.m_type = CWDatabaseInternal::Field::F_ValueList;
          subType &= 0xFF7F;
        }
        ok = true;
        switch(subType) {
        case 0:
          ok = field.m_type == CWDatabaseInternal::Field::F_Checkbox ||
               field.m_type == CWDatabaseInternal::Field::F_PopupMenu ||
               field.m_type == CWDatabaseInternal::Field::F_RadioButton ||
               field.m_type == CWDatabaseInternal::Field::F_Multimedia;
          break;
        case 2: // basic
          break;
        case 3:
          ok = field.m_type == CWDatabaseInternal::Field::F_Text;
          if (ok) f << "name[field],";
          break;
        case 6:
          ok = version() == 4 && field.m_type == CWDatabaseInternal::Field::F_ValueList;;
          break;
        default:
          ok = false;
        }
        if (!ok) f << "#unkSubType=" << std::hex << subType << std::dec << ",";
      }
      val = (int) m_input->readLong(2);
      if (val) f << "#unk1=" << std::hex << val << std::dec << ",";
      field.m_defType = (int) m_input->readULong(1);
      // default, followed by a number/ptr/... : 7fff ( mean none)
    }
    f << field << ",";
    long actPos = m_input->tell();
    if (actPos != pos && actPos != pos+fSz)
      ascii().addDelimiter(actPos, '|');
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+fSz, WPX_SEEK_SET);
  }

  m_input->seek(endPos, WPX_SEEK_SET);
  return true;
}

bool CWDatabase::readDatabaseDefaults(CWDatabaseInternal::Database &dBase)
{
  size_t numFields = dBase.m_fields.size();
  int vers = version();
  libmwaw::DebugStream f;

  for (size_t v = 0; v < numFields; v++) {
    CWDatabaseInternal::Field const &field = dBase.m_fields[v];
    int numExpected = field.getNumDefault(vers);

    bool formField = field.isFormula();
    bool valueList = field.m_type == CWDatabaseInternal::Field::F_ValueList;
    for (int fi = 0; fi < numExpected; fi++) {
      // actually we guess which one are ok
      long pos = m_input->tell();
      long sz = (long) m_input->readULong(4);

      long endPos = pos+4+sz;
      m_input->seek(endPos, WPX_SEEK_SET);
      if (long(m_input->tell()) != endPos) {
        MWAW_DEBUG_MSG(("CWDatabase::readDatabaseDefaults: can not find value for field: %d\n", fi));
        m_input->seek(pos, WPX_SEEK_SET);
        return false;
      }
      m_input->seek(pos+4, WPX_SEEK_SET);
      long length = (vers <= 2 && field.isText()) ? sz : (int) m_input->readULong(1);
      f.str("");
      f << "Entries(DatabaseDft)[" << v << "]:";
      if (formField) {
        if (length != sz-1) {
          MWAW_DEBUG_MSG(("CWDatabase::readDatabaseDefaults: can not find formula for field: %ld\n", long(v)));
          m_input->seek(pos, WPX_SEEK_SET);
          return false;
        }
        f << "formula,";
      } else {
        bool listField = (valueList && fi == 1) || (!valueList && fi==0 && numExpected==2);
        if (listField)
          f << "listString,";
        else
          f << "string,";
        if (vers > 2 && !listField && length != sz-1) {
          MWAW_DEBUG_MSG(("CWDatabase::readDatabaseDefaults: can not find strings for field: %ld\n", long(v)));
          m_input->seek(pos, WPX_SEEK_SET);
          return false;
        }
        while (1) {
          long actPos = m_input->tell();
          if (actPos+length > endPos) {
            MWAW_DEBUG_MSG(("CWDatabase::readDatabaseDefaults: can not find strings for field: %ld\n", long(v)));

            m_input->seek(pos, WPX_SEEK_SET);
            return true;
          }
          std::string name("");
          for (int c = 0; c < length; c++)
            name += char(m_input->readULong(1));
          f << "'" << name << "',";
          if (long(m_input->tell()) == endPos)
            break;
          length = (long) m_input->readULong(1);
        }
      }
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      m_input->seek(endPos, WPX_SEEK_SET);
    }
  }
  return true;
}

bool CWDatabase::readDatabaseContent(CWDatabaseInternal::Database &/*dBase*/)
{
  long pos = m_input->tell();
  long sz = (long) m_input->readULong(4);
  /** ARGHH: this zone is almost the only zone which count the header in sz ... */
  long endPos = pos+sz;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos || sz < 6) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWDatabase::readDatabaseContent: file is too short\n"));
    return false;
  }

  m_input->seek(pos+4, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(DatabaseContent):";
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
      MWAW_DEBUG_MSG(("CWDatabase::readDatabaseContent: find a odd content field\n"));
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
    f << "DatabaseContent-" << name;

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+4+sz, WPX_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////


// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
