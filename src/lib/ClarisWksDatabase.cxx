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

#include <librevenge/librevenge.h>

#include "MWAWCell.hxx"
#include "MWAWFont.hxx"
#include "MWAWSpreadsheetListener.hxx"
#include "MWAWParser.hxx"
#include "MWAWTable.hxx"

#include "ClarisWksDbaseContent.hxx"
#include "ClarisWksDocument.hxx"
#include "ClarisWksStruct.hxx"
#include "ClarisWksStyleManager.hxx"

#include "ClarisWksDatabase.hxx"

/** Internal: the structures of a ClarisWksDatabase */
namespace ClarisWksDatabaseInternal
{
struct Field {
  // the type
  enum Type { F_Unknown, F_Text, F_Number, F_Date, F_Time,
              F_Formula, F_FormulaSum,
              F_Checkbox, F_PopupMenu, F_RadioButton, F_ValueList,
              F_Multimedia
            };
  Field() : m_type(F_Unknown), m_defType(-1), m_resType(0), m_name(""), m_default("")
  {
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Field const &field)
  {
    switch (field.m_type) {
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
    switch (field.m_resType) {
    case 0:
      o << "text[format],";
      break;
    case 1:
      o << "number[format],";
      break;
    case 2:
      o << "date[format],";
      break;
    case 3:
      o << "time[format],";
      break;
    default:
      o << "##res[format]=" << field.m_resType << ",";
      break;
    }
    o << "'" << field.m_name << "',";
    switch (field.m_defType) {
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

  bool isText() const
  {
    return m_type == F_Text;
  }
  bool isFormula() const
  {
    return m_type == F_Formula || m_type == F_FormulaSum;
  }

  int getNumDefault(int version) const
  {
    switch (m_type) {
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
      return 2;
    case F_ValueList :
      return (version >= 3) ? 2 : 1;
    case F_Unknown :
    default:
      break;
    }
    return 0;
  }

  Type m_type;
  /** the local definition type */
  int m_defType;
  /** the result type */
  int m_resType;
  /** the field name */
  std::string m_name;
  /** the default value */
  std::string m_default;
};

////////////////////////////////////////
////////////////////////////////////////

//! Internal: the database of a ClarisWksDatabase
struct Database : public ClarisWksStruct::DSET {
  //! constructor
  Database(ClarisWksStruct::DSET const &dset = ClarisWksStruct::DSET()) :
    ClarisWksStruct::DSET(dset), m_fields(), m_content()
  {
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Database const &doc)
  {
    o << static_cast<ClarisWksStruct::DSET const &>(doc);
    return o;
  }
  //! the list of field
  std::vector<Field> m_fields;
  //! the data
  shared_ptr<ClarisWksDbaseContent> m_content;
};

////////////////////////////////////////
//! Internal: the state of a ClarisWksDatabase
struct State {
  //! constructor
  State() : m_databaseMap()
  {
  }

  std::map<int, shared_ptr<Database> > m_databaseMap;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
ClarisWksDatabase::ClarisWksDatabase(ClarisWksDocument &document) :
  m_document(document), m_parserState(document.m_parserState), m_state(new ClarisWksDatabaseInternal::State),
  m_mainParser(&document.getMainParser())
{
}

ClarisWksDatabase::~ClarisWksDatabase()
{ }

int ClarisWksDatabase::version() const
{
  return m_parserState->m_version;
}

// fixme
int ClarisWksDatabase::numPages() const
{
  return 1;
}
////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// a document part
////////////////////////////////////////////////////////////
shared_ptr<ClarisWksStruct::DSET> ClarisWksDatabase::readDatabaseZone
(ClarisWksStruct::DSET const &zone, MWAWEntry const &entry, bool &complete)
{
  complete = false;
  if (!entry.valid() || zone.m_fileType != 3 || entry.length() < 32)
    return shared_ptr<ClarisWksStruct::DSET>();
  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos+8+16, librevenge::RVNG_SEEK_SET); // avoid header+8 generic number
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  shared_ptr<ClarisWksDatabaseInternal::Database>
  databaseZone(new ClarisWksDatabaseInternal::Database(zone));

  f << "Entries(DatabaseDef):" << *databaseZone << ",";
  ascFile.addDelimiter(input->tell(), '|');
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  // read the last part
  long data0Length = zone.m_dataSz;
  long N = zone.m_numData;
  if (entry.length() -8-12 != data0Length*N + zone.m_headerSz) {
    if (data0Length == 0 && N) {
      MWAW_DEBUG_MSG(("ClarisWksDatabase::readDatabaseZone: can not find definition size\n"));
      input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
      return shared_ptr<ClarisWksStruct::DSET>();
    }

    MWAW_DEBUG_MSG(("ClarisWksDatabase::readDatabaseZone: unexpected size for zone definition, try to continue\n"));
  }

  long dataEnd = entry.end()-N*data0Length;
  int numLast = -1;
  int const vers=version();
  switch (vers) {
  case 1:
  case 2:
  case 3:
  case 4:
    numLast = 0;
    break;
  case 5:
    numLast = 4;
    break;
  case 6:
    numLast = 8;
    break;
  default:
    MWAW_DEBUG_MSG(("ClarisWksDatabase::readDatabaseZone: unexpected version\n"));
    break;
  }
  if (numLast >= 0 && long(input->tell()) + data0Length + numLast <= dataEnd) {
    ascFile.addPos(dataEnd-data0Length-numLast);
    ascFile.addNote("DatabaseDef-_");
    if (numLast) {
      ascFile.addPos(dataEnd-numLast);
      ascFile.addNote("DatabaseDef-extra");
    }
  }
  input->seek(dataEnd, librevenge::RVNG_SEEK_SET);

  for (int i = 0; i < N; i++) {
    pos = input->tell();

    f.str("");
    f << "DatabaseDef-" << i;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+data0Length, librevenge::RVNG_SEEK_SET);
  }

  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);

  if (m_state->m_databaseMap.find(databaseZone->m_id) != m_state->m_databaseMap.end()) {
    MWAW_DEBUG_MSG(("ClarisWksDatabase::readDatabaseZone: zone %d already exists!!!\n", databaseZone->m_id));
  }
  else
    m_state->m_databaseMap[databaseZone->m_id] = databaseZone;

  databaseZone->m_otherChilds.push_back(databaseZone->m_id+1);

  pos = input->tell();
  bool ok = readFields(*databaseZone);

  if (ok) {
    ok = readDefaults(*databaseZone);
    pos = input->tell();
  }
  if (ok) {
    pos = input->tell();
    ok = ClarisWksStruct::readStructZone(*m_parserState, "DatabaseListUnkn0", false);
  }
  if (ok) {
    pos = input->tell();
    // probably: field number followed by 1 : increasing, 2 : decreasing
    ok = ClarisWksStruct::readStructZone(*m_parserState, "DatabaseSortFunction", false);
  }
  if (ok) {
    pos = input->tell();
    shared_ptr<ClarisWksDbaseContent> content(new ClarisWksDbaseContent(m_document, false));
    ok = content->readContent();
    if (ok) databaseZone->m_content=content;
  }
  std::vector<int> listLayout;
  if (ok) {
    pos = input->tell();
    ok = ClarisWksStruct::readIntZone(*m_parserState, "DatabaseLayout", false, 4, listLayout);
  }
  if (ok) {
    for (size_t i=0; i<listLayout.size(); ++i) {
      pos = input->tell();
      if (!readLayout(*databaseZone)) {
        MWAW_DEBUG_MSG(("ClarisWksDatabase::readDatabaseZone: can not read some ListLayout data file\n"));
        ok=false;
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        ascFile.addPos(pos);
        ascFile.addNote("DatabaseLayout:###");
        break;
      }
    }
  }
  if (ok) {
    pos = input->tell();
    // in v1-v4 list of id block?, in v5-v6 list of block id+?
    ok = ClarisWksStruct::readStructZone(*m_parserState, "DatabaseListUnkn3", false);
  }

  if (ok) { // never seems,
    pos=input->tell();
    long sz=(long) input->readULong(4);
    if (input->checkPosition(pos+4+sz)) {
      input->seek(pos+4+sz, librevenge::RVNG_SEEK_SET);
      ascFile.addPos(pos);
      if (sz) {
        MWAW_DEBUG_MSG(("ClarisWksDatabase::readDatabaseZone: find a Unkn4 block\n"));
        ascFile.addNote("Entries(DatabaseListUnkn4):");
      }
      else
        ascFile.addNote("_");
    }
    else {
      ok=false;
      MWAW_DEBUG_MSG(("ClarisWksDatabase::readDatabaseZone: find a Unkn4 block does not know how to read it\n"));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
    }
  }
  if (ok && vers>1) {
    pos=input->tell();
    std::vector<std::string> listString;
    ok=m_document.readStringList("DatabaseListString", false, listString);
  }
  if (ok) {
    pos = input->tell();
    ok = ClarisWksStruct::readStructZone(*m_parserState, "DatabaseUnkn5", false);
  }
  if (ok && vers>=4) {
    // version 4 can contains more block: list of int+flag?
    pos=input->tell();
    ok = ClarisWksStruct::readStructZone(*m_parserState, "DatabaseUnkn6", false);
  }
  // now the following seems to be different
  if (!ok)
    input->seek(pos, librevenge::RVNG_SEEK_SET);
  return databaseZone;
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool ClarisWksDatabase::readFields(ClarisWksDatabaseInternal::Database &dBase)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  if (sz<12 || !input->checkPosition(endPos)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksDatabase::readFields: file is too short\n"));
    return false;
  }

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(DatabaseField):";
  int N = (int) input->readULong(2);
  f << "N=" << N << ",";
  int val = (int) input->readLong(2);
  if (val != -1) f << "f0=" << val << ",";
  val = (int) input->readLong(2);
  if (val) f << "f1=" << val << ",";
  int fSz = (int) input->readLong(2);
  if (sz != 12+fSz*N || fSz < 18) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksDatabase::readFields: find odd data size\n"));
    return false;
  }
  for (int i = 2; i < 4; i++) {
    val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  dBase.m_fields.resize(size_t(N));
  for (size_t i = 0; i < size_t(N); i++) {
    pos = input->tell();
    f.str("");
    f << "DatabaseField-" << i << ":";
    ClarisWksDatabaseInternal::Field &field = dBase.m_fields[i];

    int fNameMaxSz = 64;
    std::string name("");
    sz = (long) input->readULong(1);
    if ((fNameMaxSz && sz > fNameMaxSz-1) || sz > fSz-1) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      MWAW_DEBUG_MSG(("ClarisWksDatabase::readFields: find odd field name\n"));
      return false;
    }
    for (int j = 0; j < sz; j++)
      name += char(input->readULong(1));
    field.m_name = name;

    input->seek(pos+fNameMaxSz, librevenge::RVNG_SEEK_SET);
    int type = (int) input->readULong(1);
    bool ok = true;
    switch (type) {
    // or name
    case 0:
      field.m_type = ClarisWksDatabaseInternal::Field::F_Text;
      break;
    case 1:
      field.m_type = ClarisWksDatabaseInternal::Field::F_Number;
      break;
    case 2:
      field.m_type = ClarisWksDatabaseInternal::Field::F_Date;
      break;
    case 3:
      field.m_type = ClarisWksDatabaseInternal::Field::F_Time;
      break;
    case 4:
      if (version() <= 2)
        field.m_type = ClarisWksDatabaseInternal::Field::F_Formula;
      else
        field.m_type = ClarisWksDatabaseInternal::Field::F_PopupMenu;
      break;
    case 5:
      if (version() <= 2)
        field.m_type = ClarisWksDatabaseInternal::Field::F_FormulaSum;
      else
        field.m_type = ClarisWksDatabaseInternal::Field::F_Checkbox;
      break;
    case 6:
      field.m_type = ClarisWksDatabaseInternal::Field::F_RadioButton;
      break;
    case 7:
      if (version() == 4)
        field.m_type = ClarisWksDatabaseInternal::Field::F_Formula;
      else
        field.m_type = ClarisWksDatabaseInternal::Field::F_Multimedia;
      break;
    case 8:
      if (version() == 4)
        field.m_type = ClarisWksDatabaseInternal::Field::F_FormulaSum;
      else
        ok = false;
      break;
    case 10:
      field.m_type = ClarisWksDatabaseInternal::Field::F_Formula;
      break;
    case 11:
      field.m_type = ClarisWksDatabaseInternal::Field::F_FormulaSum;
      break;
    default:
      ok = false;
      break;
    }
    if (!ok)
      f << "#type=" << type << ",";
    val = (int) input->readULong(1);
    if (val)
      f << "#unkn=" << val << ",";
    unsigned long ptr = input->readULong(4);
    if (ptr) // set for formula
      f << "ptr=" << std::hex << ptr << std::dec << ",";
    field.m_resType=(int) input->readLong(1);
    f << "fl?=[" << std::hex;
    f << input->readULong(1) << ",";
    f << input->readULong(1) << ",";
    for (int j = 0; j < 6; j++) {
      // some int which seems constant on the database...
      val = (int) input->readULong(2);
      f <<  val << ",";
    }
    f << std::dec << "],";

    if (version() > 1) {
      for (int j = 0; j < 16; j++) {
        /** find f1=600 for a number
            f16 = 0[checkbox, ... ], 2[number or text],3 [name field], 82[value list],
            f16 & 8: can not be empty
        */
        val = (int) input->readLong(2);
        if (val) f << "f" << j << "=" << std::hex << val << std::dec << ",";
      }
      int subType = (int) input->readULong(2);
      if (version() == 2) {
        if ((subType & 0x80) && field.m_type == ClarisWksDatabaseInternal::Field::F_Text) {
          field.m_type = ClarisWksDatabaseInternal::Field::F_ValueList;
          subType &= 0xFF7F;
        }
        if (subType) f << "f17=" << std::hex << subType << std::dec << ",";
      }
      else {
        if ((subType & 0x80) && field.m_type == ClarisWksDatabaseInternal::Field::F_Text) {
          field.m_type = ClarisWksDatabaseInternal::Field::F_ValueList;
          subType &= 0xFF7F;
        }
        ok = true;
        switch (subType) {
        case 0:
          ok = field.m_type == ClarisWksDatabaseInternal::Field::F_Checkbox ||
               field.m_type == ClarisWksDatabaseInternal::Field::F_PopupMenu ||
               field.m_type == ClarisWksDatabaseInternal::Field::F_RadioButton ||
               field.m_type == ClarisWksDatabaseInternal::Field::F_Multimedia;
          break;
        case 2: // basic
          break;
        case 3:
          ok = field.m_type == ClarisWksDatabaseInternal::Field::F_Text;
          if (ok) f << "name[field],";
          break;
        case 6:
          ok = version() == 4 && field.m_type == ClarisWksDatabaseInternal::Field::F_ValueList;;
          break;
        default:
          ok = false;
        }
        if (!ok) f << "#unkSubType=" << std::hex << subType << std::dec << ",";
      }
      val = (int) input->readLong(2);
      if (val) f << "#unk1=" << std::hex << val << std::dec << ",";
      field.m_defType = (int) input->readULong(1);
      // default, followed by a number/ptr/... : 7fff ( mean none)
    }
    f << field << ",";
    long actPos = input->tell();
    if (actPos != pos && actPos != pos+fSz)
      ascFile.addDelimiter(actPos, '|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
  }

  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

bool ClarisWksDatabase::readDefaults(ClarisWksDatabaseInternal::Database &dBase)
{
  size_t numFields = dBase.m_fields.size();
  int vers = version();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  for (size_t v = 0; v < numFields; v++) {
    ClarisWksDatabaseInternal::Field const &field = dBase.m_fields[v];
    int numExpected = field.getNumDefault(vers);

    bool formField = field.isFormula();
    bool valueList = field.m_type == ClarisWksDatabaseInternal::Field::F_ValueList;
    for (int fi = 0; fi < numExpected; fi++) {
      // actually we guess which one are ok
      long pos = input->tell();
      long sz = (long) input->readULong(4);

      long endPos = pos+4+sz;
      if (!input->checkPosition(endPos)) {
        MWAW_DEBUG_MSG(("ClarisWksDatabase::readDefaults: can not find value for field: %d\n", fi));
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        return false;
      }
      long length = (vers <= 2 && field.isText()) ? sz : (int) input->readULong(1);
      f.str("");
      f << "Entries(DatabaseDft)[" << v << "]:";
      if (formField) {
        if (length != sz-1) {
          MWAW_DEBUG_MSG(("ClarisWksDatabase::readDefaults: can not find formula for field: %ld\n", long(v)));
          input->seek(pos, librevenge::RVNG_SEEK_SET);
          return false;
        }
        f << "formula,";
      }
      else {
        bool listField = (valueList && fi == 1) || (!valueList && fi==0 && numExpected==2);
        if (listField)
          f << "listString,";
        else
          f << "string,";
        if (vers > 2 && !listField && length != sz-1) {
          MWAW_DEBUG_MSG(("ClarisWksDatabase::readDefaults: can not find strings for field: %ld\n", long(v)));
          input->seek(pos, librevenge::RVNG_SEEK_SET);
          return false;
        }
        while (1) {
          long actPos = input->tell();
          if (actPos+length > endPos) {
            MWAW_DEBUG_MSG(("ClarisWksDatabase::readDefaults: can not find strings for field: %ld\n", long(v)));
            ascFile.addPos(pos);
            ascFile.addNote("DatabaseDft:###");

            input->seek(pos, librevenge::RVNG_SEEK_SET);
            return true;
          }
          std::string name("");
          for (int c = 0; c < length; c++)
            name += char(input->readULong(1));
          f << "'" << name << "',";
          if (long(input->tell()) == endPos)
            break;
          length = (long) input->readULong(1);
        }
      }
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      input->seek(endPos, librevenge::RVNG_SEEK_SET);
    }
  }
  return true;
}


bool ClarisWksDatabase::readLayout(ClarisWksDatabaseInternal::Database &dBase)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  if (sz<16 || !input->checkPosition(endPos)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksDatabase::readLayout: file is too short\n"));
    return false;
  }

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "DatabaseLayout-Part:";
  int N = (int) input->readULong(2);
  f << "N=" << N << ",";
  int val = (int) input->readLong(2);
  if (val != -1) f << "f0=" << val << ",";
  val = (int) input->readLong(2);
  if (val) f << "f1=" << val << ",";
  int fSz = (int) input->readLong(2);
  int hSz = (int) input->readULong(2);
  if (fSz<6 || hSz<52 || N *fSz+hSz+12 != sz) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksDatabase::readLayout: find odd data size\n"));
    return false;
  }
  for (int i = 0; i < 2; ++i) { // f2=1, f3=0
    val = (int) input->readLong(2);
    if (val!=1-i) f << "f" << i+2 << "=" << val << ",";
  }
  int childId=(int) input->readULong(2);
  f << "childId=" << childId << ",";
  dBase.m_otherChilds.push_back(childId);
  for (int i = 0; i < 2; ++i) { // f4=1-3, f5=0|c6|12a
    val = (int) input->readLong(2);
    if (val) f << "f" << i+4 << "=" << val << ",";
  }
  for (int i = 0; i<4; ++i) { // always 0|1
    val = (int) input->readLong(1);
    if (val==1) f << "fl" << i << ",";
    else if (val) f << "#fl" << i << "=" << val << ",";
  }
  int sSz=(int) input->readULong(1);
  if (sSz>31) {
    MWAW_DEBUG_MSG(("ClarisWksDatabase::readLayout: find odd string size\n"));
    f << "#sSz=" << sSz << ",";
  }
  else {
    std::string name("");
    for (int i=0; i<sSz; ++i) name+=(char) input->readULong(1);
    f << "\"" << name << "\",";
  }
  input->seek(pos+60, librevenge::RVNG_SEEK_SET);
  val = (int) input->readLong(2); // always 0
  if (val) f << "g0=" << val << ",";
  childId=(int) input->readULong(2);
  f << "childId2=" << childId << ",";
  dBase.m_otherChilds.push_back(childId);

  ascFile.addDelimiter(input->tell(),'|');
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  input->seek(endPos-N*fSz, librevenge::RVNG_SEEK_SET);
  for (int i = 0; i < N; i++) {
    pos=input->tell();
    f.str("");
    f << "DatabaseLayout-Part" << i << ":";

    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
  }

  pos=input->tell();
  if (!ClarisWksStruct::readStructZone(*m_parserState, "DatabaseLayout", false)) {
    MWAW_DEBUG_MSG(("ClarisWksDatabase::readLayout: can not read the layout second part\n"));
    ascFile.addPos(pos);
    ascFile.addNote("DatabaseLayout-B:###");
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////
//
// send data
//
////////////////////////////////////////////////////////////
bool ClarisWksDatabase::sendDatabase(int zId, MWAWListenerPtr listener)
{
  if (!listener)
    listener=m_parserState->m_spreadsheetListener;
  if (!listener) {
    MWAW_DEBUG_MSG(("ClarisWksDatabase::sendDatabase: called without any listener\n"));
    return false;
  }
  if (listener->getType()!=MWAWListener::Spreadsheet ||
      (m_parserState->m_kind==MWAWDocument::MWAW_K_DATABASE && zId!=1)) {
    MWAW_DEBUG_MSG(("ClarisWksDatabase::sendDatabase: sending a database is not implemented\n"));
    return false;
  }

  MWAWSpreadsheetListener *sheetListener=static_cast<MWAWSpreadsheetListener *>(listener.get());
  std::map<int, shared_ptr<ClarisWksDatabaseInternal::Database> >::iterator it=
    m_state->m_databaseMap.find(zId);
  if (it == m_state->m_databaseMap.end() || !it->second) {
    MWAW_DEBUG_MSG(("ClarisWksDatabase::sendDatabase: can not find zone %d!!!\n", zId));
    return false;
  }
  ClarisWksDatabaseInternal::Database &dbase=*it->second;
  MWAWVec2i minData, maxData;
  std::vector<int> recordsPos;
  if (!dbase.m_content || !dbase.m_content->getExtrema(minData,maxData) ||
      !dbase.m_content->getRecordList(recordsPos)) {
    MWAW_DEBUG_MSG(("ClarisWksDatabase::sendDatabase: can not find content\n"));
    return false;
  }

  int numFields = maxData[0]+1>int(dbase.m_fields.size()) ?
                  maxData[0]+1 : int(dbase.m_fields.size());
  std::vector<ClarisWksStyleManager::CellFormat> formats;
  formats.resize(size_t(numFields), ClarisWksStyleManager::CellFormat());
  for (size_t f=0; f < dbase.m_fields.size(); ++f) {
    switch (dbase.m_fields[f].m_type) {
    case ClarisWksDatabaseInternal::Field::F_Number:
      formats[f].m_format=MWAWCell::F_NUMBER;
      formats[f].m_numberFormat=MWAWCell::F_NUMBER_GENERIC;
      break;
    case ClarisWksDatabaseInternal::Field::F_Date:
      formats[f].m_format=MWAWCell::F_DATE;
      break;
    case ClarisWksDatabaseInternal::Field::F_Time:
      formats[f].m_format=MWAWCell::F_TIME;
      break;
    case ClarisWksDatabaseInternal::Field::F_Unknown:
    case ClarisWksDatabaseInternal::Field::F_Text:
    case ClarisWksDatabaseInternal::Field::F_Formula:
    case ClarisWksDatabaseInternal::Field::F_FormulaSum:
    case ClarisWksDatabaseInternal::Field::F_Checkbox:
    case ClarisWksDatabaseInternal::Field::F_PopupMenu:
    case ClarisWksDatabaseInternal::Field::F_RadioButton:
    case ClarisWksDatabaseInternal::Field::F_ValueList:
    case ClarisWksDatabaseInternal::Field::F_Multimedia:
    default:
      switch (dbase.m_fields[f].m_resType) {
      case 1:
        formats[f].m_format=MWAWCell::F_NUMBER;
        formats[f].m_numberFormat=MWAWCell::F_NUMBER_GENERIC;
        break;
      case 2:
        formats[f].m_format=MWAWCell::F_DATE;
        break;
      case 3:
        formats[f].m_format=MWAWCell::F_TIME;
        break;
      default:
        break;
      }
      break;
    }
  }
  dbase.m_content->setDatabaseFormats(formats);

  std::vector<float> colSize(size_t(numFields),72);
  sheetListener->openSheet(colSize, librevenge::RVNG_POINT);
  MWAWInputStreamPtr &input= m_parserState->m_input;
  for (size_t r=0; r < recordsPos.size(); ++r) {
    sheetListener->openSheetRow((float)14, librevenge::RVNG_POINT);
    for (int c=0; c < numFields; ++c) {
      ;
      ClarisWksDbaseContent::Record rec;
      if (!dbase.m_content->get(MWAWVec2i(c,recordsPos[r]),rec)) continue;
      MWAWCell cell;
      cell.setPosition(MWAWVec2i(c,int(r)));
      cell.setFormat(rec.m_format);
      cell.setHAlignment(rec.m_hAlign);
      // change the reference date from 1/1/1904 to 1/1/1900
      if (rec.m_format.m_format==MWAWCell::F_DATE && rec.m_content.isValueSet())
        rec.m_content.setValue(rec.m_content.m_value+1460);
      sheetListener->openSheetCell(cell, rec.m_content);
      if (rec.m_content.m_textEntry.valid()) {
        long fPos = input->tell();
        input->seek(rec.m_content.m_textEntry.begin(), librevenge::RVNG_SEEK_SET);
        long endPos = rec.m_content.m_textEntry.end();
        while (!input->isEnd() && input->tell() < endPos) {
          unsigned char ch=(unsigned char) input->readULong(1);
          if (ch==9)
            sheetListener->insertTab();
          else if (ch==0xd)
            sheetListener->insertEOL();
          else
            sheetListener->insertCharacter(ch, input, endPos);
        }
        input->seek(fPos,librevenge::RVNG_SEEK_SET);
      }
      sheetListener->closeSheetCell();
    }
    sheetListener->closeSheetRow();
  }
  sheetListener->closeSheet();
  return true;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
