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

#include <time.h>

#include <cmath>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <limits>

#include <librevenge/librevenge.h>

#include "MWAWDebug.hxx"
#include "MWAWHeader.hxx"
#include "MWAWInputStream.hxx"
#include "MWAWListener.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWParser.hxx"

#include "ClarisWksDocument.hxx"
#include "ClarisWksStyleManager.hxx"

#include "ClarisWksDbaseContent.hxx"

ClarisWksDbaseContent::ClarisWksDbaseContent(ClarisWksDocument &document, bool spreadsheet) :
  m_version(0), m_isSpreadsheet(spreadsheet), m_document(document), m_parserState(document.m_parserState), m_idColumnMap(), m_dbFormatList()
{
  if (!m_parserState || !m_parserState->m_header) {
    MWAW_DEBUG_MSG(("ClarisWksDbaseContent::ClarisWksDbaseContent: can not find the file header\n"));
    return;
  }
  m_version = m_parserState->m_header->getMajorVersion();
}

ClarisWksDbaseContent::~ClarisWksDbaseContent()
{
}

void ClarisWksDbaseContent::setDatabaseFormats(std::vector<ClarisWksStyleManager::CellFormat> const &format)
{
  if (m_isSpreadsheet) {
    MWAW_DEBUG_MSG(("ClarisWksDbaseContent::setDatabaseFormats: called with spreadsheet\n"));
    return;
  }
  m_dbFormatList=format;
}

bool ClarisWksDbaseContent::getExtrema(Vec2i &min, Vec2i &max) const
{
  if (m_idColumnMap.empty())
    return false;
  bool first=true;
  std::map<int, Column>::const_iterator it=m_idColumnMap.begin();
  for (; it!=m_idColumnMap.end() ; ++it) {
    int col=it->first;
    Column const &column=it->second;
    if (column.m_idRecordMap.empty())
      continue;
    max[0]=col;
    std::map<int, Record>::const_iterator rIt=column.m_idRecordMap.begin();
    for (; rIt!=column.m_idRecordMap.end(); ++rIt) {
      int row=rIt->first;
      if (first) {
        min[0]=col;
        min[1]=max[1]=row;
        first=false;
      }
      else if (row < min[1])
        min[1]=row;
      else if (row > max[1])
        max[1]=row;
    }
  }
  return !first;
}

bool ClarisWksDbaseContent::getRecordList(std::vector<int> &list) const
{
  list.resize(0);
  if (m_isSpreadsheet || m_idColumnMap.empty())
    return false;
  std::set<int> set;
  std::map<int, Column>::const_iterator it=m_idColumnMap.begin();
  for (; it!=m_idColumnMap.end() ; ++it) {
    Column const &column=it->second;
    std::map<int, Record>::const_iterator rIt=column.m_idRecordMap.begin();
    for (; rIt!=column.m_idRecordMap.end(); ++rIt) {
      int row=rIt->first;
      if (set.find(row)==set.end())
        set.insert(row);
    }
  }
  if (set.empty())
    return false;
  list = std::vector<int>(set.begin(), set.end());

  return true;
}

bool ClarisWksDbaseContent::readContent()
{
  if (!m_parserState) return false;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  /** ARGHH: this zone is almost the only zone which count the header in sz ... */
  long endPos = pos+sz;
  std::string zoneName(m_isSpreadsheet ? "spread" : "dbase");
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != endPos || sz < 6) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readContent: file is too short\n"));
    return false;
  }

  input->seek(pos+4, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(DBHeader)[" << zoneName << "]:";
  int N = (int) input->readULong(2);
  f << "N=" << N << ",";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  input->pushLimit(endPos);
  readColumnList();

  if (input->tell() == endPos) {
    input->popLimit();
    return true;
  }
  /* can we have more than one sheet ? If so, going into the while
     loop may be ok, we will not read the data...*/
  MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readContent: find extra data\n"));
  bool ok=true;
  while (input->tell() < endPos) {
    pos = input->tell();
    sz = (long) input->readULong(4);
    long zoneEnd=pos+4+sz;
    if (zoneEnd > endPos || (sz && sz < 12)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readContent: find a odd content field\n"));
      ok=false;
      break;
    }
    if (!sz) {
      ascFile.addPos(pos);
      ascFile.addNote("Nop");
      continue;
    }
    std::string name("");
    for (int i = 0; i < 4; i++)
      name+=char(input->readULong(1));
    MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readContent: find unexpected content field\n"));
    f << "DBHeader[" << zoneName << "]:###" << name;
    ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(zoneEnd, librevenge::RVNG_SEEK_SET);
  }
  input->popLimit();
  return ok;
}

bool ClarisWksDbaseContent::readColumnList()
{
  if (!m_parserState) return false;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  long sz=input->readLong(4);
  std::string hName("");
  for (int i=0; i < 4; ++i) hName+=(char) input->readULong(1);
  if (sz!=0x408 || hName!="CTAB" || !input->checkPosition(pos+4+sz)) {
    MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readCOLM: the entry seems bad\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  if (m_isSpreadsheet)
    f << "Entries(DBCTAB)[spread]:";
  else
    f << "Entries(DBCTAB)[dbase]:";
  int N=(int) input->readLong(2);
  if (N) f << "Ncols=" << N << ",";
  int val=(int) input->readLong(2);
  if (val) f << "Nrows=" << val << ",";
  if (N<0 || N>255) {
    MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readColumnList: the entries number of elements seems bad\n"));
    f << "####";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  f << "ptr=[";
  long ptr;
  std::vector<long> listIds;
  for (int i=0; i <= N; i++) {
    ptr=(long) input->readULong(4);
    listIds.push_back(ptr);
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
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readColumnList: find some extra values\n"));
      first=false;
    }
    f << "#g" << i << "=" << ptr << ",";
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  for (size_t c=0; c<listIds.size(); c++) {
    if (!listIds[c]) continue;
    pos=input->tell();
    if (readColumn(int(c)))
      continue;
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  return true;
}

bool ClarisWksDbaseContent::readColumn(int c)
{
  if (!m_parserState) return false;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  long sz=input->readLong(4);
  std::string hName("");
  for (int i=0; i < 4; ++i) hName+=(char) input->readULong(1);
  int cPos[2];
  for (int i=0; i < 2; ++i)
    cPos[i]=(int) input->readLong(2);
  if (sz!=8+4*(cPos[1]-cPos[0]+1) || hName!="COLM" || !input->checkPosition(pos+4+sz)) {
    MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readCOLM: the entry seems bad\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  if (m_isSpreadsheet)
    f << "Entries(DBCOLM)[spread]:";
  else
    f << "Entries(DBCOLM)[dbase]:";
  f << "ptr[" << cPos[0] << "<=>" << cPos[1] << "]=[";
  std::vector<long> listIds;
  listIds.resize(size_t(cPos[0]),0);
  for (int i=cPos[0]; i <= cPos[1]; i++) {
    long ptr=(long) input->readULong(4);
    listIds.push_back(ptr);
    if (ptr)
      f << std::hex << ptr << std::dec << ",";
    else
      f << "_,";
  }
  f << "],";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  // now read the chnk
  ClarisWksDbaseContent::Column col;
  bool ok=true;
  for (size_t i=0; i < listIds.size(); ++i) {
    pos=input->tell();
    if (!listIds[i] || readRecordList(Vec2i(c,64*int(i)), col))
      continue;
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    ok=false;
    break;
  }
  if (!col.m_idRecordMap.empty())
    m_idColumnMap[c]=col;
  return ok;
}

bool ClarisWksDbaseContent::readRecordList(Vec2i const &where, Column &col)
{
  if (!m_parserState) return false;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  long sz=input->readLong(4);
  long endPos=pos+4+sz;
  std::string hName("");
  for (int i=0; i < 4; ++i) hName+=(char) input->readULong(1);
  int N=(int) input->readULong(2);
  if (sz<6+134 || hName!="CHNK" || !input->checkPosition(pos+4+sz) || N>0x40) {
    MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordList: the entry seems bad\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  std::string zoneName(m_isSpreadsheet ? "spread" : "dbase");
  f << "Entries(DBCHNK)[" << zoneName << "]:N=" << N << ",";
  int type=(int) input->readULong(2); // often 400, 800...
  f << "type=" << std::hex << type << std::dec << ",";
  int dim[2]; // checkme
  for (int i=0; i<2; i++) dim[i]=(int) input->readLong(2);
  f << "dim=" << dim[0] << "x" << dim[1] << ",";

  f << "depl=[";
  std::vector<long> ptrLists(64,0);
  int find=0;
  for (size_t i=0; i < 64; ++i) {
    long depl=(long) input->readLong(2);
    if (depl==0) {
      f << "_,";
      continue;
    }
    find++;
    long fPos=pos+4+depl;
    if (fPos > endPos) {
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordList: the %d ptr seems bad\n", (int) i));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      return false;
    }
    f << std::hex << depl << std::dec << ",";
    ptrLists[i]=fPos;
  }
  f << "],";
  if (find!=N) {
    MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordList: the number of find data seems bad\n"));
    f << "###find=" << find << "!=" << N << ",";
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  for (size_t i=0; i<64; ++i) {
    if (!ptrLists[i]) continue;
    Record record;
    Vec2i wh(where[0],where[1]+(int) i);
    if ((m_isSpreadsheet && readRecordSS(wh, ptrLists[i], record)) ||
        (!m_isSpreadsheet && readRecordDB(wh, ptrLists[i], record))) {
      col.m_idRecordMap[wh[1]]=record;
      continue;
    }

    f.str("");
    f << "DBCHNK[" << zoneName << wh << "]:#";
    input->seek(ptrLists[i], librevenge::RVNG_SEEK_SET);
    int fType=(int) input->readULong(1);
    f << "type=" << std::hex << fType << std::dec << ",";
    ascFile.addPos(ptrLists[i]);
    ascFile.addNote(f.str().c_str());
    col.m_idRecordMap[wh[1]]=record;
  }

  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

bool ClarisWksDbaseContent::readRecordSSV1(Vec2i const &id, long pos, ClarisWksDbaseContent::Record &record)
{
  record=ClarisWksDbaseContent::Record();
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "DBCHNK[spread" << id << "]:";
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int val=(int) input->readULong(1);
  int type=(val>>4);
  int fileFormat=(val&0xF);
  MWAWCell::Format &format=record.m_format;
  switch (fileFormat) {
  case 0: // general
    break;
  case 1:
    format.m_format=MWAWCell::F_NUMBER;
    format.m_numberFormat=MWAWCell::F_NUMBER_CURRENCY;
    break;
  case 2:
    format.m_format=MWAWCell::F_NUMBER;
    format.m_numberFormat=MWAWCell::F_NUMBER_PERCENT;
    break;
  case 3:
    format.m_format=MWAWCell::F_NUMBER;
    format.m_numberFormat=MWAWCell::F_NUMBER_SCIENTIFIC;
    break;
  case 4:
    format.m_format=MWAWCell::F_NUMBER;
    format.m_numberFormat=MWAWCell::F_NUMBER_DECIMAL;
    break;
  case 5:
  case 6:
  case 7:
  case 8:
  case 9: {
    static char const *(wh[])= {"%m/%d/%y", "%B %d, %y", "%B %d, %Y", "%a, %b %d %y", "%A, %B %d %Y" };
    format.m_format=MWAWCell::F_DATE;
    format.m_DTFormat=wh[fileFormat-5];
    break;
  }
  case 10:
  case 11:
  case 12:
  case 13: {
    static char const *(wh[])= {"%I:%M %p", "%I:%M:%S %p", "%H:%M", "%H:%M:%S" };
    format.m_format=MWAWCell::F_TIME;
    format.m_DTFormat=wh[fileFormat-10];
    break;
  }
  default: // unknown
    MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSSV1: find unknown format\n"));
    f << "format=##" << fileFormat << ",";
    break;
  }

  bool ok=true;
  int ord=(int) input->readULong(1);
  if (ord&8) {
    f << "commas[thousand],";
    ord &= 0xF7;
  }
  if (ord&4) {
    f << "parenthese[negative],";
    ord &= 0xFB;
  }
  if (ord&2) {
    f << "lock,";
    ord &= 0xFD;
  }
  if (ord&1) {
    if (!input->checkPosition(pos+8)) {
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSSV1: can not read format\n"));
      f << "###";
      ok = false;
    }
    else {
      MWAWFont &font = record.m_font;
      font = MWAWFont();
      int fId= (int) input->readULong(2);
      if (fId!=0xFFFF)
        font.setId(m_document.getStyleManager()->getFontId((int)fId));
      font.setSize((float) input->readULong(1));
      int flag =(int) input->readULong(1);
      uint32_t flags=0;
      if (flag&0x1) flags |= MWAWFont::boldBit;
      if (flag&0x2) flags |= MWAWFont::italicBit;
      if (flag&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
      if (flag&0x8) flags |= MWAWFont::embossBit;
      if (flag&0x10) flags |= MWAWFont::shadowBit;
      if (flag&0x20) font.setDeltaLetterSpacing(-1);
      if (flag&0x40) font.setDeltaLetterSpacing(1);
      if (flag&0x80) font.setStrikeOutStyle(MWAWFont::Line::Simple);
      font.setFlags(flags);
      int colId = (int) input->readULong(1);
      if (colId!=1) {
        MWAWColor col;
        if (m_document.getStyleManager()->getColor(colId, col))
          font.setColor(col);
        else {
          MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSSV1: unknown color %d\n", colId));
        }
      }
      f << "font=[" << font.getDebugString(m_parserState->m_fontConverter) << "],";
      val=(int) input->readULong(1);
      record.m_borders = (val>>4);
      if (record.m_borders) {
        f << "border=";
        if (record.m_borders&1) f << "L";
        if (record.m_borders&2) f << "T";
        if (record.m_borders&4) f << "R";
        if (record.m_borders&8) f << "B";
        f << ",";
      }
      val &=0xF;
      switch ((val>>2)) {
      case 1:
        record.m_hAlign=MWAWCell::HALIGN_LEFT;
        f << "left,";
        break;
      case 2:
        record.m_hAlign=MWAWCell::HALIGN_CENTER;
        f << "center,";
        break;
      case 3:
        record.m_hAlign=MWAWCell::HALIGN_RIGHT;
        f << "right,";
        break;
      default:
        break;
      }
      val &=0x3;
      if (val) f << "#unk=" << val << ",";
      ascFile.addDelimiter(pos+2,'|');
      input->seek(pos+8, librevenge::RVNG_SEEK_SET);
      ascFile.addDelimiter(pos+8,'|');
      ord &= 0xFE;
    }
  }
  format.m_digits=(ord>>4);
  ord &= 0xF;
  if (ok && ord) {
    MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSSV1: find unexpected order\n"));
    f << "###ord=" << std::hex << ord << std::dec;
    ok = false;
  }
  MWAWCellContent &content=record.m_content;
  if (ok && type==4) {
    f << "formula,";
    long actPos=input->tell();
    int formSz=(int) input->readULong(1);
    if (!input->checkPosition(actPos+2+formSz)) {
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSSV1: the formula seems bad\n"));
      f << "###";
      ok = false;
    }
    else {
      ascFile.addDelimiter(input->tell(),'|');
      std::vector<MWAWCellContent::FormulaInstruction> formula;
      std::string error;
      if (readFormula(id, actPos+1+formSz, formula, error)) {
        content.m_contentType=MWAWCellContent::C_FORMULA;
        content.m_formula=formula;
      }
      else {
        MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSSV1: can not read a formula\n"));
        f << "###";
      }
      f << "form=";
      for (size_t i=0; i < formula.size(); ++i)
        f << formula[i];
      f << error << ",";
      if ((formSz%2)==0) ++formSz;
      input->seek(actPos+1+formSz, librevenge::RVNG_SEEK_SET);
      ascFile.addDelimiter(input->tell(),'|');
      val=(int) input->readULong(1);
      type=(val>>4);
      if (val&0xF) f << "unkn0=" << (val&0xF) << ",";
      val=(int) input->readULong(1);
      if (val) f << "unkn1=" << (val) << ",";
    }
  }
  if (ok) {
    long actPos=input->tell();
    switch (type) {
    case 0:
    case 1:
      if (type==0)
        f << "int,";
      else
        f << "long,";
      if (!input->checkPosition(actPos+2+2*type)) {
        MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSSV1: unexpected size for a int\n"));
        f << "###";
        ok = false;
        break;
      }
      record.m_valueType=MWAWCellContent::C_NUMBER;
      content.setValue(double(input->readLong(2+2*type)));
      f << "val=" << content.m_value << ",";
      break;
    case 2: {
      f << "float,";
      if (!input->checkPosition(actPos+10)) {
        MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSSV1: unexpected size for a float\n"));
        f << "###";
        ok=false;
        break;
      }
      record.m_valueType=MWAWCellContent::C_NUMBER;
      double value;
      if (input->readDouble10(value, record.m_hasNaNValue)) {
        content.setValue(value);
        f << "val=" << value << ",";
      }
      else {
        MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSSV1: can not read a float\n"));
        f << "###,";
      }
      break;
    }
    case 3: {
      f << "string,";
      int fSz = (int) input->readULong(1);
      if (!input->checkPosition(actPos+1+fSz)) {
        MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSSV1: the string(II) seems bad\n"));
        f << "###";
        ok=false;
        break;
      }
      record.m_valueType=MWAWCellContent::C_TEXT;
      content.m_textEntry.setBegin(input->tell());
      content.m_textEntry.setLength((long) fSz);
      std::string data("");
      for (int c=0; c < fSz; ++c) data+=(char) input->readULong(1);
      f << "val=" << data << ",";
      break;
    }
    // case 4: the formula is already read
    case 5: // bool
      if (input->checkPosition(actPos+1)) {
        if (!fileFormat)
          format.m_format=MWAWCell::F_BOOLEAN;
        record.m_valueType=MWAWCellContent::C_NUMBER;
        content.setValue(double(input->readLong(1)));
        f << "val=" << content.m_value << ",";
        break;
      }
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSSV1: can not read a bool\n"));
      f << "###bool,";
      break;
    case 6:
      if (input->checkPosition(actPos+1)) {
        record.m_valueType=MWAWCellContent::C_NUMBER;
        content.setValue(std::numeric_limits<double>::quiet_NaN());
        record.m_hasNaNValue = true;
        f << "val=nan" << input->readLong(1) << ",";
        break;
      }
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSSV1: can not read a nanc[formula]\n"));
      f << "###nan[res],";
      break;
    case 7: // empty
      break;
    case 8: // checkme: does such cell can have data?
      f << "recovered,";
      break;
    default:
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSSV1: unexpected type\n"));
      f << "###type=" << type << ",";
      ok=false;
      break;
    }
  }
  if (content.m_contentType!=MWAWCellContent::C_FORMULA)
    content.m_contentType=record.m_valueType;
  if (format.m_format==MWAWCell::F_UNKNOWN && content.isValueSet()) {
    format.m_format=MWAWCell::F_NUMBER;
    format.m_numberFormat=MWAWCell::F_NUMBER_GENERIC;
  }
  f << format << ",";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  if (ok) {
    ascFile.addPos(input->tell());
    ascFile.addNote("_");
  }
  return ok;
}

bool ClarisWksDbaseContent::readRecordSS(Vec2i const &id, long pos, ClarisWksDbaseContent::Record &record)
{
  if (m_version <= 3)
    return readRecordSSV1(id, pos, record);
  record=ClarisWksDbaseContent::Record();
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "DBCHNK[spread" << id << "]:";
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  long sz=(long) input->readULong(2);
  long endPos=pos+sz+2;
  if (!input->checkPosition(endPos) || sz < 4) {
    f << "###sz=" << sz;
    MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSS: the sz seems bad\n"));
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  int type=(int) input->readULong(1);
  // next some format ?
  int val= (int) input->readULong(1); // 0-46
  if (val) f << "format=" << std::hex << val << std::dec << ",";
  record.m_style=(int) input->readLong(2);
  if (record.m_style) f << "Style-" << record.m_style << ",";

  int fileFormat=0;
  ClarisWksStyleManager::Style style;
  ClarisWksStyleManager::CellFormat format;
  if (m_document.getStyleManager()->get(record.m_style, style)) {
    if (m_document.getStyleManager()->get(style.m_cellFormatId, format)) {
      f << format << ",";
      fileFormat=format.m_fileFormat;
    }
    if (style.m_fontId>=0)
      m_document.getStyleManager()->get(style.m_fontId, record.m_font);
    MWAWGraphicStyle graphStyle;
    if (style.m_graphicId>=0 && m_document.getStyleManager()->get(style.m_graphicId, graphStyle)) {
      if (graphStyle.hasSurfaceColor())
        record.m_backgroundColor=graphStyle.m_surfaceColor;
    }
  }
  switch (fileFormat) {
  case 0: // general
    break;
  case 1:
    format.m_format=MWAWCell::F_NUMBER;
    format.m_numberFormat=MWAWCell::F_NUMBER_CURRENCY;
    break;
  case 2:
    format.m_format=MWAWCell::F_NUMBER;
    format.m_numberFormat=MWAWCell::F_NUMBER_PERCENT;
    break;
  case 3:
    format.m_format=MWAWCell::F_NUMBER;
    format.m_numberFormat=MWAWCell::F_NUMBER_SCIENTIFIC;
    break;
  case 4:
    format.m_format=MWAWCell::F_NUMBER;
    format.m_numberFormat=MWAWCell::F_NUMBER_DECIMAL;
    break;
  case 5:
  case 6:
  case 7:
  case 8:
  case 9: {
    static char const *(wh[])= {"%m/%d/%y", "%B %d, %y", "%B %d, %Y", "%a, %b %d %y", "%A, %B %d %Y" };
    format.m_format=MWAWCell::F_DATE;
    format.m_DTFormat=wh[fileFormat-5];
    break;
  }
  case 10:  // unknown
  case 11:
    break;
  case 12:
  case 13:
  case 14:
  case 15: {
    static char const *(wh[])= {"%H:%M", "%H:%M:%S", "%I:%M %p", "%I:%M:%S %p" };
    format.m_format=MWAWCell::F_TIME;
    format.m_DTFormat=wh[fileFormat-12];
    break;
  }
  default: // unknown
    MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSS: find unknown format\n"));
    f << "format=##" << fileFormat << ",";
    break;
  }

  MWAWCellContent &content=record.m_content;
  bool ok=true;
  if (type==4) {
    f << "formula,";
    if (sz<7) {
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSS: the formula seems bad\n"));
      f << "##sz,";
      ok = false;
    }
    else {
      type=(int) input->readULong(1);
      val=(int) input->readLong(1);
      if (val) f << "unkn=" << val << ",";
      int formSz=(int) input->readULong(1);
      long actPos=input->tell();
      ascFile.addDelimiter(actPos,'|');

      if (8+formSz > sz) {
        MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSS: the formula seems bad\n"));
        f << "###";
        ok=false;
      }
      else {
        std::vector<MWAWCellContent::FormulaInstruction> formula;
        std::string error;
        if (readFormula(id, actPos+1+formSz, formula, error)) {
          content.m_contentType=MWAWCellContent::C_FORMULA;
          content.m_formula=formula;
        }
        else {
          MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSS: can not read a formule\n"));
          f << "###";
        }
        f << "form=";
        for (size_t i=0; i < formula.size(); ++i)
          f << formula[i];
        f << error << ",";

        /** checkme: there does not seem to be alignment, but another
            variable before the result */
        input->seek(actPos+formSz+1, librevenge::RVNG_SEEK_SET);
        ascFile.addDelimiter(input->tell(),'|');
        sz=4+int(endPos-input->tell());
      }
    }
  }
  if (ok) {
    switch (type) {
    case 0:
    case 1:
      if (type==0)
        f << "int,";
      else
        f << "long,";
      if (sz<6+2*type) {
        MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSS: unexpected size for a int\n"));
        f << "###";
        break;
      }
      record.m_valueType=MWAWCellContent::C_NUMBER;
      content.setValue(double(input->readLong(2+2*type)));
      f << "val=" << content.m_value << ",";
      break;
    case 2: {
      f << "float,";
      if (sz<0xe) {
        MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSS: unexpected size for a float\n"));
        f << "###";
        break;
      }
      record.m_valueType=MWAWCellContent::C_NUMBER;
      double value;
      if (input->readDouble10(value, record.m_hasNaNValue)) {
        content.setValue(value);
        f << value << ",";
      }
      else {
        MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSS: can not read a float\n"));
        f << "###,";
      }
      break;
    }
    case 3: {
      f << "string,";
      if (sz<5) {
        MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSS: the string seems bad\n"));
        f << "###";
        break;
      }
      int fSz = (int) input->readULong(1);
      if (fSz+5>sz) {
        MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSS: the string(II) seems bad\n"));
        f << "###";
        break;
      }
      record.m_valueType=MWAWCellContent::C_TEXT;
      content.m_textEntry.setBegin(input->tell());
      content.m_textEntry.setLength((long) fSz);
      std::string data("");
      for (int c=0; c < fSz; ++c) data+=(char) input->readULong(1);
      f << data << ",";
      break;
    }
    // 4: formula
    case 5: // bool
      if (sz>=4+1) {
        if (format.m_format==MWAWCell::F_UNKNOWN)
          format.m_format=MWAWCell::F_BOOLEAN;
        record.m_valueType=MWAWCellContent::C_NUMBER;
        content.setValue(double(input->readLong(1)));
        f << "val=" << content.m_value << ",";
        break;
      }
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSS: can not read a bool\n"));
      f << "###bool,";
      break;
    case 6:
      if (sz>=4+1) {
        record.m_valueType=MWAWCellContent::C_NUMBER;
        content.setValue(std::numeric_limits<double>::quiet_NaN());
        record.m_hasNaNValue = true;
        f << "val=nan" << input->readLong(1) << ",";
        break;
      }
      break;
    case 7: // link/anchor/goto
      if (sz<4) {
        MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSS: the mark size seems bad\n"));
        f << "###mark";
        break;
      }
      f << "mark,";
      break;
    case 8:
    case 9:
      f << "type" << type << ",";
      if (sz<4) {
        MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSS: the type%d seems bad\n", type));
        f << "###";
        break;
      }
      break;
    default:
      f << "#type=" << type << ",";
    }
  }
  if (content.m_contentType!=MWAWCellContent::C_FORMULA)
    content.m_contentType=record.m_valueType;
  record.m_format=format;
  record.m_hAlign=format.m_hAlign;
  record.m_borders=format.m_borders;
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(endPos);
  ascFile.addNote("_");
  return true;
}

bool ClarisWksDbaseContent::readRecordDB(Vec2i const &id, long pos, ClarisWksDbaseContent::Record &record)
{
  record=ClarisWksDbaseContent::Record();
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "DBCHNK[dbase" << id << "]:";
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  long sz=0;
  long endPos=-1;
  if (m_version>3) {
    sz=(long) input->readULong(2);
    endPos=pos+sz+2;
    if (!input->checkPosition(endPos) || sz < 2) {
      f << "###sz=" << sz;
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordDB: the sz seems bad\n"));
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      return true;
    }
  }
  int val=(int) input->readULong(2);
  int type=(val>>12);
  val = int(val&0xFFF);
  MWAWCellContent &content=record.m_content;
  switch (type) {
  case 0: {
    f << "string,";
    if ((m_version<=3&&!input->checkPosition(pos+2+val)) ||
        (m_version>3 && (val+2>sz || val+4<sz))) {
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordDB: the string(II) seems bad\n"));
      f << "###";
      break;
    }
    content.m_contentType=MWAWCellContent::C_TEXT;
    content.m_textEntry.setBegin(input->tell());
    content.m_textEntry.setLength((long) val);
    std::string data("");
    for (int c=0; c < val; ++c) data+=(char) input->readULong(1);
    f << data << ",";
    break;
  }
  case 4:
    // find also some string here when val is not null so let test
    if (val && m_version>3 && val+2<=sz && val+4>=sz) {
      content.m_contentType=MWAWCellContent::C_TEXT;
      content.m_textEntry.setBegin(input->tell());
      content.m_textEntry.setLength((long) val);
      std::string data("");
      for (int c=0; c < val; ++c) data+=(char) input->readULong(1);
      f << data << ",";
      break;
    }
    if (val)
      f << "unkn=" << std::hex << val << std::dec << ",";
    f << "int,";
    if ((m_version<=3&&!input->checkPosition(pos+2)) || (m_version>3 && sz!=2)) {
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordDB: unexpected size for a int\n"));
      f << "###";
      break;
    }
    content.m_contentType=MWAWCellContent::C_NUMBER;
    content.setValue(double(input->readLong(1)));
    break;
  case 8:
  case 9: {
    if (val) f << "unkn=" << std::hex << val << std::dec << ",";
    f << "float" << type << ",";
    if ((m_version<=3&&!input->checkPosition(pos+12)) || (m_version>3 && sz!=12)) {
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordDB: unexpected size for a float\n"));
      f << "###";
      break;
    }
    double value;
    if (input->readDouble10(value, record.m_hasNaNValue)) {
      content.m_contentType=MWAWCellContent::C_NUMBER;
      content.setValue(value);
      f << value << ",";
    }
    else {
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordDB: can not read a float\n"));
      f << "###,";
    }
    break;
  }
  default:
    if (val) f << "unkn=" << std::hex << val << std::dec << ",";
    f << "#type=" << type << ",";
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  if (m_version>3) {
    ascFile.addPos(endPos);
    ascFile.addNote("_");
  }
  return true;
}

bool ClarisWksDbaseContent::get(Vec2i const &pos, ClarisWksDbaseContent::Record &record) const
{
  std::map<int, Column>::const_iterator it=m_idColumnMap.find(pos[0]);
  if (it==m_idColumnMap.end()) return false;
  Column const &col=it->second;
  std::map<int, Record>::const_iterator rIt=col.m_idRecordMap.find(pos[1]);
  if (rIt==col.m_idRecordMap.end()) return false;
  record=rIt->second;
  if (m_isSpreadsheet) return true;

  static bool first=true;
  if (pos[0]>=0&&pos[0]<int(m_dbFormatList.size())) {
    ClarisWksStyleManager::CellFormat const &format=m_dbFormatList[size_t(pos[0])];
    record.m_format=format;
    record.m_fileFormat=format.m_fileFormat;
    record.m_hAlign=format.m_hAlign;
  }
  else if (first) {
    MWAW_DEBUG_MSG(("ClarisWksDbaseContent::get: can not find format for field %d\n", pos[0]));
    first=false;
  }
  return true;
}

bool ClarisWksDbaseContent::send(Vec2i const &pos)
{
  MWAWListenerPtr listener=m_parserState->getMainListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("ClarisWksDbaseContent::send: can not find the listener\n"));
    return false;
  }
  Record record;
  if (!get(pos, record)) return true;
  MWAWCellContent const &content=record.m_content;
  listener->setFont(record.m_font);
  MWAWCellContent::Type contentType=
    content.m_contentType==MWAWCellContent::C_FORMULA ? record.m_valueType : content.m_contentType;
  MWAWParagraph para;
  para.m_justify =
    record.m_hAlign==MWAWCell::HALIGN_LEFT ? MWAWParagraph::JustificationLeft :
    record.m_hAlign==MWAWCell::HALIGN_CENTER ? MWAWParagraph::JustificationCenter :
    record.m_hAlign==MWAWCell::HALIGN_RIGHT ? MWAWParagraph::JustificationRight :
    contentType==MWAWCellContent::C_TEXT ? MWAWParagraph::JustificationLeft :
    MWAWParagraph::JustificationRight;
  listener->setParagraph(para);
  switch (contentType) {
  case MWAWCellContent::C_NUMBER:
    if (record.m_fileFormat)
      send(content.m_value, record.m_hasNaNValue, record.m_format);
    else {
      std::stringstream s;
      s << content.m_value;
      listener->insertUnicodeString(s.str().c_str());
    }
    break;
  case MWAWCellContent::C_TEXT:
    if (content.m_textEntry.valid()) {
      MWAWInputStreamPtr &input= m_parserState->m_input;
      long fPos = input->tell();
      input->seek(content.m_textEntry.begin(), librevenge::RVNG_SEEK_SET);
      long endPos = content.m_textEntry.end();
      while (!input->isEnd() && input->tell() < endPos) {
        unsigned char c=(unsigned char) input->readULong(1);
        listener->insertCharacter(c, input, endPos);
      }
      input->seek(fPos,librevenge::RVNG_SEEK_SET);
    }
    break;
  case MWAWCellContent::C_FORMULA:
  case MWAWCellContent::C_NONE:
  case MWAWCellContent::C_UNKNOWN:
  default:
    break;
  }
  return true;
}

void ClarisWksDbaseContent::send(double val, bool isNotANumber, ClarisWksStyleManager::CellFormat const &format)
{
  MWAWListenerPtr listener=m_parserState->getMainListener();
  if (!listener)
    return;
  std::stringstream s;
  int const &type=format.m_fileFormat;
  // note: if val*0!=0, val is a NaN so better so simply print NaN
  if (type <= 0 || type >=16 || type==10 || type==11 || isNotANumber) {
    s << val;
    listener->insertUnicodeString(s.str().c_str());
    return;
  }
  std::string value("");
  // FIXME: must not be here, change the reference date from 1/1/1904 to 1/1/1900
  if (MWAWCellContent::double2String(format.m_format==MWAWCell::F_DATE ? val+1460 : val, format, value))
    s << value;
  else {
    MWAW_DEBUG_MSG(("ClarisWksDbaseContent::send: can not convert the actual value\n"));
    s << val;
  }
  listener->insertUnicodeString(s.str().c_str());
}

////////////////////////////////////////////////////////////
// formula
////////////////////////////////////////////////////////////
bool ClarisWksDbaseContent::readCellInFormula(Vec2i const &pos, MWAWCellContent::FormulaInstruction &instr)
{
  MWAWInputStreamPtr input=m_parserState->m_input;
  instr=MWAWCellContent::FormulaInstruction();
  instr.m_type=MWAWCellContent::FormulaInstruction::F_Cell;
  bool absolute[2] = { true, true};
  int cPos[2];
  for (int i=0; i<2; ++i) {
    int val = (int) input->readULong(2);
    if (val & 0x8000) {
      absolute[1-i]=false;
      if (val&0x4000)
        cPos[1-i] = pos[1-i]-1+(val-0xFFFF);
      else
        cPos[1-i] = pos[1-i]-1+(val-0x7FFF);
    }
    else
      cPos[1-i]=val;
  }
  if (m_version==6) {
    // checkme: what is this number
    int val=(int) input->readLong(2);
    static bool first = true;
    if (val!=-1 && first) {
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readCellInFormula: ARGHHH value after cell is %d\n", val));
      first=false;
    }
  }

  if (cPos[0] < 0 || cPos[1] < 0) {
    MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readCellInFormula: can not read cell position\n"));
    return false;
  }
  instr.m_position[0]=Vec2i(cPos[0],cPos[1]);
  instr.m_positionRelative[0]=Vec2b(!absolute[0],!absolute[1]);
  return true;
}

bool ClarisWksDbaseContent::readString(long endPos, std::string &res)
{
  res="";
  MWAWInputStreamPtr input=m_parserState->m_input;
  long pos=input->tell();
  int fSz=(int) input->readULong(1);
  if (pos+1+fSz>endPos) {
    MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readString: can not read string size\n"));
    return false;
  }
  for (int i=0; i<fSz; ++i)
    res += (char) input->readULong(1);
  return true;
}

bool ClarisWksDbaseContent::readNumber(long endPos, double &res, bool &isNan)
{
  MWAWInputStreamPtr input=m_parserState->m_input;
  long pos=input->tell();
  if (pos+10>endPos) {
    MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readNumber: can not read a number\n"));
    return false;
  }
  return input->readDouble10(res, isNan);
}

namespace ClarisWksDbaseContentInternal
{
struct Operators {
  char const *m_name;
  int m_arity;
};

static Operators const s_listOperators[] = {
  { "<", 2}, { ">", 2}, { "=", 2}, { "<=", 2},
  { ">=", 2}, { "<>", 2}, { "", -2} /*UNKN*/,{ "+", 2},

  { "-", 2}, { "*", 2}, { "/", 2}, { "^", 2},
  { "+", 1} /*checkme*/, { "-", 1}, { "(", 1}, { "", -2} /*UNKN*/,
};
}

bool ClarisWksDbaseContent::readFormula(Vec2i const &cPos, long endPos, std::vector<MWAWCellContent::FormulaInstruction> &formula, std::string &error)
{
  MWAWInputStreamPtr input=m_parserState->m_input;
  libmwaw::DebugStream f;
  bool ok=true;
  long pos=input->tell();
  if (input->isEnd() || pos >= endPos)
    return false;
  int arity=0, type=(int) input->readULong(1);
  bool isFunction=false, isOperator=false;
  MWAWCellContent::FormulaInstruction instr;
  switch (type) {
  case 0x10:
    if (pos+1+2>endPos) {
      ok=false;
      break;
    }
    instr.m_type=MWAWCellContent::FormulaInstruction::F_Long;
    instr.m_longValue=(double) input->readLong(2);
    break;
  case 0x11:
    if (pos+1+4>endPos) {
      ok=false;
      break;
    }
    instr.m_type=MWAWCellContent::FormulaInstruction::F_Long;
    instr.m_longValue=(double) input->readLong(4);
    break;
  case 0x12: {
    double value;
    bool isNan;
    ok=readNumber(endPos, value, isNan);
    if (!ok) break;
    instr.m_type=MWAWCellContent::FormulaInstruction::F_Double;
    instr.m_doubleValue=value;
    break;
  }
  case 0x13: {
    ok=readCellInFormula(cPos,instr);
    break;
  }
  case 0x1b:
    if (pos+1+8 > endPos) {
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readFormula: find instruction 0x1b, unknown size\n"));
      f << "##[code=1b,short],";
      ok = false;
      break;
    }
    /* found in some web files followed by a bad cell list */
    MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readFormula: find instruction 0x1b\n"));
    f << "##[code=1b],";
    instr.m_type=MWAWCellContent::FormulaInstruction::F_CellList;
    input->seek(8, librevenge::RVNG_SEEK_SET);
    break;
  case 0x14: {
    if (pos+1+8 > endPos || !readCellInFormula(cPos, instr)) {
      f << "###list cell short";
      ok = false;
      break;
    }
    MWAWCellContent::FormulaInstruction instr2;
    if (!readCellInFormula(cPos, instr2)) {
      f << "###list cell short2";
      ok = false;
      break;
    }
    instr.m_type=MWAWCellContent::FormulaInstruction::F_CellList;
    instr.m_position[1]=instr2.m_position[0];
    instr.m_positionRelative[1]=instr2.m_positionRelative[0];
    break;
  }
  case 0x15: {
    std::string text;
    ok=readString(endPos, text);
    if (!ok) break;
    instr.m_type=MWAWCellContent::FormulaInstruction::F_Text;
    instr.m_content=text;
    break;
  }
  case 0x16: {
    if (pos+1+2>endPos) {
      ok=false;
      break;
    }
    isFunction=true;
    int val=(int) input->readULong(1);
    arity=(int) input->readULong(1);
    std::string name("");
    if (val<0x70) {
      static char const *(wh[]) = {
        "Abs", "Acos", "Alert", "And", "Code", "Asin", "Atan", "Atan2",
        "Average", "Choose", "Char", "Concatenate", "Cos", "Count", "Date", "DateToText",

        "DateValue", "Day", "DayName", "DayOfYear", "Degrees", "NA"/* error*/, "Exact", "Exp",
        "Frac", "FV", "HLookup", "Hour", "If", "Index", "Int", "IRR",

        "IsBlank", "IsError", "IsNA", "IsNumber", "IsText", "Left", "Ln", "Log",
        "Log10", "LookUp", "Lower", "Match", "Max", "Mid", "Min", "Minute",

        "MIRR", "Mod", "MonthName", "NA", "Not", "Now", "NPER", "NPV",
        "N" /*NumToText*/, "Or", "Pi", "PMT", "Product", "Proper", "PV", "Radians",

        "Rand", "Rate", "Replace", "Right", "Round", "Second", "Sign", "Sqrt",
        "StDev", "Sum", "Tan", "Rept", "Value"/*TextToNum*/, "Time", "TimeToText", "TimeValue",

        "", "Trim", "Type", "Upper", "Var", "VLookUp", "WeekDay", "WeekOfYear",
        "Year", "Find", "Column", "Row", "Fact", "Len", "Sin", "Month",

        "Trunc", "Count2", "Macro", "Beep", "", "", "", "",
        "", "", "", "", "", "", "", "",
      };
      name=wh[val];
    }
    if (name.empty()) {
      f << "###";
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readFormula: find unknown function\n"));
      std::stringstream s;
      s << "Funct" << std::hex << val << std::dec;
      name=s.str();
    }
    instr.m_type=MWAWCellContent::FormulaInstruction::F_Function;
    instr.m_content=name;
    formula.push_back(instr);
    instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
    instr.m_content="(";
    break;
  }
  case 0x18: // bool
    if (pos+1+1>endPos) {
      ok=false;
      break;
    }
    instr.m_type=MWAWCellContent::FormulaInstruction::F_Long;
    instr.m_longValue=(int) input->readLong(1);
    break;
  case 0x1f: // final equal sign
    return true;
  default: {
    std::string op("");
    if (type<0x10) {
      op=ClarisWksDbaseContentInternal::s_listOperators[type].m_name;
      arity=ClarisWksDbaseContentInternal::s_listOperators[type].m_arity;
    }
    if (!op.empty()&&arity>0) {
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
      instr.m_content=op;
      isOperator=true;
      break;
    }
    f << "##type=" << std::hex << type << std::dec << ",";
    ok=false;
    break;
  }
  }
  error+=f.str();
  if (!ok) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  if (!isOperator || arity==1)
    formula.push_back(instr);
  if (isFunction && arity>1) {
    instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
    instr.m_content=";";
  }
  for (int i=0; i < arity; ++i) {
    if (!readFormula(cPos, endPos, formula, error))
      return false;
    if (i+1==arity) break;
    formula.push_back(instr);
  }
  if (isFunction || instr.m_content=="(") {
    instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
    instr.m_content=")";
    formula.push_back(instr);
  }

  return true;
}

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////
void ClarisWksDbaseContent::Record::updateFormulaCells(Vec2i const &removeDelta)
{
  if (m_content.m_contentType!=MWAWCellContent::C_FORMULA)
    return;
  std::vector<MWAWCellContent::FormulaInstruction> &formula=m_content.m_formula;
  for (size_t i=0; i<formula.size(); ++i) {
    MWAWCellContent::FormulaInstruction &instr=formula[i];
    int numCell=instr.m_type==MWAWCellContent::FormulaInstruction::F_Cell ? 1 :
                instr.m_type==MWAWCellContent::FormulaInstruction::F_CellList ? 2 : 0;
    for (int c=0; c<numCell; ++c) {
      instr.m_position[c]-=removeDelta;
      if (instr.m_position[c][0]<0 || instr.m_position[c][1]<0) {
        static bool first=true;
        if (first) {
          MWAW_DEBUG_MSG(("ClarisWksDbaseContent::Record::updateFormulaCells: some cell's positions are bad, remove formula\n"));
          first=false;
        }
        // revert to the basic cell type
        m_content.m_contentType=m_valueType;
        return;
      }
    }
  }
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
