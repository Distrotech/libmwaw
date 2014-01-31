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
  if (ok && ord!=0x20) {
    MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSSV1: find unexpected order\n"));
    f << "###ord=" << std::hex << ord << std::dec;
    ok = false;
  }
  MWAWCellContent &content=record.m_content;
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
      content.m_contentType=MWAWCellContent::C_NUMBER;
      content.setValue(double(2+2*type));
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
      content.m_contentType=MWAWCellContent::C_NUMBER;
      double value;
      if (input->readDouble10(value, record.m_hasNaNValue)) {
        content.setValue(value);
        f << value << ",";
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
      content.m_contentType=MWAWCellContent::C_TEXT;
      content.m_textEntry.setBegin(input->tell());
      content.m_textEntry.setLength((long) fSz);
      std::string data("");
      for (int c=0; c < fSz; ++c) data+=(char) input->readULong(1);
      f << data << ",";
      break;
    }
    case 4: {
      f << "formula,";
      int formSz=(int) input->readULong(1);
      if (!input->checkPosition(actPos+2+formSz)) {
        MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSSV1: the formula seems bad\n"));
        f << "###";
        break;
      }
      ascFile.addDelimiter(input->tell(),'|');
      input->seek(formSz+1-(formSz%2), librevenge::RVNG_SEEK_CUR);
      ascFile.addDelimiter(input->tell(),'|');
      val=(int) input->readULong(1);
      int rType=(val>>4);
      if (val&0xF) f << "unkn1=" << (val&0xF) << ",";
      val=(int) input->readULong(1);
      if (val) f << "unkn1=" << (val) << ",";
      long resPos=input->tell();
      switch (rType) {
      case 0:
      case 1:
        if (input->checkPosition(resPos+2+2*rType)) {
          content.m_contentType=MWAWCellContent::C_NUMBER;
          content.setValue(double(input->readLong(2+2*rType)));
          f << "=" << content.m_value << ",";
          break;
        }
        MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSSV1: can not read a long[formula]\n"));
        f << "###int" << rType << "[res],";
        break;
      case 2: {
        double value;
        if (input->checkPosition(resPos+10) && input->readDouble10(value, record.m_hasNaNValue)) {
          content.m_contentType=MWAWCellContent::C_NUMBER;
          content.setValue(value);
          f << "=" << value << ",";
          break;
        }
        MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSSV1: can not read a double[formula]\n"));
        f << "###float[res],";
        break;
      }
      case 3: {
        int fSz = (int) input->readULong(1);
        if (!input->checkPosition(resPos+fSz+1)) {
          MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSSV1: the res string(II) seems bad\n"));
          f << "###string[res]";
          break;
        }
        content.m_contentType=MWAWCellContent::C_TEXT;
        content.m_textEntry.setBegin(input->tell());
        content.m_textEntry.setLength((long) fSz);
        std::string data("");
        for (int c=0; c < fSz; ++c) data+=(char) input->readULong(1);
        f << "=" << data << ",";
        break;
      }
      case 5: // bool
        if (input->checkPosition(resPos+1)) {
          if (!fileFormat)
            format.m_format=MWAWCell::F_BOOLEAN;
          content.m_contentType=MWAWCellContent::C_NUMBER;
          content.setValue(double(input->readLong(1)));
          f << "=" << content.m_value << ",";
          break;
        }
        MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSSV1: can not read a bool[formula]\n"));
        f << "###bool[res],";
        break;
      case 6: // nan
        if (input->checkPosition(resPos+1)) {
          content.m_contentType=MWAWCellContent::C_NUMBER;
          content.setValue(std::numeric_limits<double>::quiet_NaN());
          record.m_hasNaNValue = true;
          f << "=nan" << input->readLong(1) << ",";
          break;
        }
        MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSSV1: can not read a nanc[formula]\n"));
        f << "###nan[res],";
        break;
      case 7: // empty
        break;
      // type 8: find with 206b*: maybe to indicate an empty cell recovered by neighbour...
      default:
        f << "##type=" << rType << "[res],";
        ok = false;
        break;
      }
      break;
    }
    case 5: // bool
      if (input->checkPosition(actPos+1)) {
        if (!fileFormat)
          format.m_format=MWAWCell::F_BOOLEAN;
        content.m_contentType=MWAWCellContent::C_NUMBER;
        content.setValue(double(input->readLong(1)));
        f << "=" << content.m_value << ",";
        break;
      }
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSSV1: can not read a bool\n"));
      f << "###bool,";
      break;
    case 6:
      if (input->checkPosition(actPos+1)) {
        content.m_contentType=MWAWCellContent::C_NUMBER;
        content.setValue(std::numeric_limits<double>::quiet_NaN());
        record.m_hasNaNValue = true;
        f << "=nan" << input->readLong(1) << ",";
        break;
      }
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSSV1: can not read a nanc[formula]\n"));
      f << "###nan[res],";
      break;
    case 7: // empty
      break;
    default:
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSSV1: unexpected type\n"));
      f << "###type=" << type << ",";
      ok=false;
      break;
    }
  }
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
  if (record.m_style) f << "style" << record.m_style << ",";
  ClarisWksStyleManager::Style style;
  ClarisWksStyleManager::CellFormat format;
  if (m_document.getStyleManager()->get(record.m_style, style) &&
      m_document.getStyleManager()->get(style.m_cellFormatId, format))
    f << format << ",";
  MWAWCellContent &content=record.m_content;
  switch (type) {
  case 0:
  case 1:
    if (type==0)
      f << "int,";
    else
      f << "long,";
    if (sz!=6+2*type) {
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSS: unexpected size for a int\n"));
      f << "###";
      break;
    }
    content.m_contentType=MWAWCellContent::C_NUMBER;
    content.setValue(double(input->readLong(2+2*type)));
    f << "val=" << content.m_value << ",";
    break;
  case 2: {
    f << "float,";
    if (sz!=0xe) {
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSS: unexpected size for a float\n"));
      f << "###";
      break;
    }
    content.m_contentType=MWAWCellContent::C_NUMBER;
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
    if (fSz+5!=sz && fSz+6!=sz) {
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSS: the string(II) seems bad\n"));
      f << "###";
      break;
    }
    content.m_contentType=MWAWCellContent::C_TEXT;
    content.m_textEntry.setBegin(input->tell());
    content.m_textEntry.setLength((long) fSz);
    std::string data("");
    for (int c=0; c < fSz; ++c) data+=(char) input->readULong(1);
    f << data << ",";
    break;
  }
  case 4: {
    f << "formula,";
    if (sz<7) {
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSS: the formula seems bad\n"));
      f << "###";
      break;
    }
    int rType=(int) input->readULong(1);
    int formSz=(int) input->readULong(2);
    if (8+formSz > sz) {
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSS: the formula seems bad\n"));
      f << "###";
      break;
    }
    ascFile.addDelimiter(input->tell(),'|');
    /** checkme: there does not seem to be alignment, but another
        variable before the result */
    input->seek(formSz+1, librevenge::RVNG_SEEK_CUR);
    ascFile.addDelimiter(input->tell(),'|');
    int remainSz=int(endPos-input->tell());
    switch (rType) {
    case 0:
    case 1:
      if (remainSz>=2+2*rType) {
        content.m_contentType=MWAWCellContent::C_NUMBER;
        content.setValue(double(input->readLong(2+2*rType)));
        f << "=" << content.m_value << ",";
        break;
      }
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSS: can not formula res\n"));
      f << "###int" << rType << "[res],";
      break;
    case 2: {
      double value;
      if (remainSz>=10 && input->readDouble10(value, record.m_hasNaNValue)) {
        content.m_contentType=MWAWCellContent::C_NUMBER;
        content.setValue(value);
        f << "=" << value << ",";
        break;
      }
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSS: can not formula res\n"));
      f << "###float[res],";
      break;
    }
    case 3: {
      if (remainSz<1) {
        MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSS: the res string seems bad\n"));
        f << "###string[res]";
        break;
      }
      int fSz = (int) input->readULong(1);
      if (fSz+1>remainSz) {
        MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSS: the res string(II) seems bad\n"));
        f << "###string[res]";
        break;
      }
      content.m_contentType=MWAWCellContent::C_TEXT;
      content.m_textEntry.setBegin(input->tell());
      content.m_textEntry.setLength((long) fSz);
      std::string data("");
      for (int c=0; c < fSz; ++c) data+=(char) input->readULong(1);
      f << "=" << data << ",";
      break;
    }
    default:
      f << "##type=" << rType << "[res],";
      break;
    }
    break;
  }
  case 7: // link/anchor/goto
    if (sz!=4) {
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSS: the mark size seems bad\n"));
      f << "###mark";
      break;
    }
    f << "mark,";
    break;
  case 8:
  case 9:
    f << "type" << type << ",";
    if (sz!=4) {
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::readRecordSS: the type%d seems bad\n", type));
      f << "###";
      break;
    }
    break;
  default:
    f << "#type=" << type << ",";
  }
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
        (m_version>3 && val+2!=sz && val+3!=sz)) {
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
    if (val) f << "unkn=" << std::hex << val << std::dec << ",";
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
  if (!m_isSpreadsheet) {
    static bool first=true;
    if (pos[0]>=0&&pos[0]<int(m_dbFormatList.size())) {
      ClarisWksStyleManager::CellFormat const &format=m_dbFormatList[size_t(pos[0])];
      record.m_format=format;
      record.m_fileFormat=format.m_fileFormat;
      record.m_hAlign=format.m_hAlign;
    }
    else if (first) {
      MWAW_DEBUG_MSG(("ClarisWksDbaseContent::send: can not find format for field %d\n", pos[0]));
      first=false;
    }
  }
  else if (m_version<=3)
    return true;
  else {
    MWAWFont font;
    ClarisWksStyleManager::Style style;
    if (record.m_style>=0)
      m_document.getStyleManager()->get(record.m_style, style);
    if (style.m_fontId>=0)
      m_document.getStyleManager()->get(style.m_fontId, record.m_font);
    ClarisWksStyleManager::CellFormat format;
    if (style.m_cellFormatId>=0 && m_document.getStyleManager()->get(style.m_cellFormatId, format)) {
      record.m_format=format;
      record.m_fileFormat=format.m_fileFormat;
      record.m_hAlign=format.m_hAlign;
    }
  }
  // time to update the cell format
  MWAWCell::Format &format=record.m_format;
  switch (record.m_fileFormat) {
  case -1: // unset
    break;
  case 0: // general
    if (!record.m_content.isValueSet())
      break;
    format.m_format=MWAWCell::F_NUMBER;
    format.m_numberFormat=MWAWCell::F_NUMBER_GENERIC;
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
    format.m_DTFormat=wh[record.m_fileFormat-5];
    break;
  }
  case 10: // unknown
  case 11: // unknown
    break;
  case 12:
  case 13:
  case 14:
  case 15: {
    static char const *(wh[])= {"%H:%M", "%H:%M:%S", "%I:%M %p", "%I:%M:%S %p"};
    format.m_format=MWAWCell::F_TIME;
    format.m_DTFormat=wh[record.m_fileFormat-12];
    break;
  }
  default: // unknown
    break;
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
  MWAWParagraph para;
  para.m_justify =
    record.m_hAlign==MWAWCell::HALIGN_LEFT ? MWAWParagraph::JustificationLeft :
    record.m_hAlign==MWAWCell::HALIGN_CENTER ? MWAWParagraph::JustificationCenter :
    record.m_hAlign==MWAWCell::HALIGN_RIGHT ? MWAWParagraph::JustificationRight :
    content.m_contentType==MWAWCellContent::C_TEXT ? MWAWParagraph::JustificationLeft :
    MWAWParagraph::JustificationRight;
  listener->setParagraph(para);
  switch (content.m_contentType) {
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
  // change the reference date from 1/1/1904 to 1/1/1900
  if (MWAWCellContent::double2String(format.m_format==MWAWCell::F_DATE ? val+1462 : val, format, value))
    s << value;
  else {
    MWAW_DEBUG_MSG(("ClarisWksDbaseContent::send: can not convert the actual value\n"));
    s << val;
  }
  listener->insertUnicodeString(s.str().c_str());
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
