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

#include <string.h>

#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWDebug.hxx"
#include "MWAWHeader.hxx"
#include "MWAWInputStream.hxx"
#include "MWAWParser.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWSection.hxx"

#include "ClarisWksDatabase.hxx"
#include "ClarisWksGraph.hxx"
#include "ClarisWksPresentation.hxx"
#include "ClarisWksSpreadsheet.hxx"
#include "ClarisWksStyleManager.hxx"
#include "ClarisWksTable.hxx"
#include "ClarisWksText.hxx"

#include "ClarisWksDocument.hxx"

/** Internal: the structures of a ClarisWksDocument */
namespace ClarisWksDocumentInternal
{
////////////////////////////////////////
//! Internal: the state of a ClarisWksDocument
struct State {
  //! constructor
  State() : m_pageSpanSet(false),   m_pages(0,0), m_headerId(0), m_footerId(0),  m_headerHeight(0), m_footerHeight(0),
    m_columns(1), m_columnsWidth(), m_columnsSep()
  {
  }

  //! a flag to know if pageSpan is filled
  bool m_pageSpanSet;

  //! the document number of pages ( if known )
  Vec2i m_pages;
  int m_headerId /** the header zone if known */,
      m_footerId /** the footer zone if known */;
  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;

  /** the number of columns */
  int m_columns;
  /** the columns witdh in Points*/
  std::vector<int> m_columnsWidth;
  /** the columns separator in Points*/
  std::vector<int> m_columnsSep;
};
}

ClarisWksDocument::ClarisWksDocument(MWAWParser &parser) :
  m_state(new ClarisWksDocumentInternal::State), m_parserState(parser.getParserState()),
  m_parser(&parser), m_styleManager(),
  m_databaseParser(), m_graphParser(), m_presentationParser(), m_spreadsheetParser(), m_tableParser(), m_textParser(),
  m_canSendZoneAsGraphic(0), m_forceParsed(0), m_getZone(0), m_newPage(0),
  m_sendFootnote(0), m_sendZone(0)
{
  m_styleManager.reset(new ClarisWksStyleManager(*this));

  m_databaseParser.reset(new ClarisWksDatabase(*this));
  m_graphParser.reset(new ClarisWksGraph(*this));
  m_presentationParser.reset(new ClarisWksPresentation(*this));
  m_spreadsheetParser.reset(new ClarisWksSpreadsheet(*this));
  m_tableParser.reset(new ClarisWksTable(*this));
  m_textParser.reset(new ClarisWksText(*this));
}

ClarisWksDocument::~ClarisWksDocument()
{
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
Vec2i ClarisWksDocument::getDocumentPages() const
{
  return m_state->m_pages;
}

double ClarisWksDocument::getTextHeight() const
{
  return m_parserState->m_pageSpan.getPageLength()-m_state->m_headerHeight/72.0-m_state->m_footerHeight/72.0;
}

Vec2f ClarisWksDocument::getPageLeftTop() const
{
  return Vec2f(float(m_parserState->m_pageSpan.getMarginLeft()),
               float(m_parserState->m_pageSpan.getMarginTop()+m_state->m_headerHeight/72.0));
}

void ClarisWksDocument::setHeaderFooterId(int &id, bool header)
{
  if (header)
    m_state->m_headerId=id;
  else
    m_state->m_footerId=id;
}

void ClarisWksDocument::getHeaderFooterId(int &headerId, int &footerId) const
{
  headerId = m_state->m_headerId;
  footerId = m_state->m_footerId;
}
////////////////////////////////////////////////////////////
// interface via callback
////////////////////////////////////////////////////////////
void ClarisWksDocument::newPage(int page)
{
  if (!m_newPage) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::newPage: can not find newPage callback\n"));
    return;
  }
  (m_parser->*m_newPage)(page);
}

MWAWSection ClarisWksDocument::getMainSection() const
{
  MWAWSection sec;
  if (m_state->m_columns <= 1)
    return sec;
  size_t numCols = size_t(m_state->m_columns);
  bool hasSep = m_state->m_columnsSep.size()+1==numCols;
  bool hasWidth = m_state->m_columnsWidth.size()==numCols;
  double width=0.0;
  if (!hasWidth) {
    double totalWidth = 72.0*m_parserState->m_pageSpan.getPageWidth();
    for (size_t c=0; c+1 < numCols; c++)
      totalWidth -= double(m_state->m_columnsSep[c]);
    width = totalWidth/double(numCols);
  }
  sec.m_columns.resize(numCols);
  for (size_t c=0; c < numCols; c++) {
    sec.m_columns[c].m_width =
      hasWidth ? double(m_state->m_columnsWidth[c]) : width;
    sec.m_columns[c].m_widthUnit = librevenge::RVNG_POINT;
    if (!hasSep)
      continue;
    if (c)
      sec.m_columns[c].m_margins[libmwaw::Left]=
        double(m_state->m_columnsSep[c-1])/72./2.;
    if (c+1!=numCols)
      sec.m_columns[c].m_margins[libmwaw::Right]=
        double(m_state->m_columnsSep[c])/72./2.;
  }
  return sec;
}

bool ClarisWksDocument::canSendZoneAsGraphic(int number) const
{
  if (!m_canSendZoneAsGraphic) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::canSendZoneAsGraphic: can not find canSendZoneAsGraphic callback\n"));
    return false;
  }
  return (m_parser->*m_canSendZoneAsGraphic)(number);
}

shared_ptr<ClarisWksStruct::DSET> ClarisWksDocument::getZone(int zId) const
{
  if (!m_getZone) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::getZone: can not find getZone callback\n"));
    return shared_ptr<ClarisWksStruct::DSET>();
  }
  return (m_parser->*m_getZone)(zId);
}

void ClarisWksDocument::sendFootnote(int zoneId)
{
  if (!m_sendFootnote) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::sendFootnote: can not find sendFootnote callback\n"));
    return;
  }
  (m_parser->*m_sendFootnote)(zoneId);
}

void ClarisWksDocument::forceParsed(int zoneId)
{
  if (!m_forceParsed) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::forceParsed: can not find forceParsed callback\n"));
    return;
  }
  (m_parser->*m_forceParsed)(zoneId);
}

bool ClarisWksDocument::sendZone(int zoneId, bool asGraphic, MWAWPosition pos)
{
  if (!m_sendZone) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::sendZone: can not find sendZone callback\n"));
    return false;
  }
  return (m_parser->*m_sendZone)(zoneId, asGraphic, pos);
}

void ClarisWksDocument::checkOrdering(std::vector<int16_t> &vec16, std::vector<int32_t> &vec32) const
{
  if (!m_parserState || m_parserState->m_version < 4) return;
  int numSmallEndian = 0, numBigEndian = 0;
  unsigned long val;
  for (size_t i = 0; i < vec16.size(); i++) {
    val = (unsigned long)(uint16_t) vec16[i];
    if ((val & 0xFF00) && !(val & 0xFF))
      numSmallEndian++;
    else if ((val&0xFF) && !(val&0xFF00))
      numBigEndian++;
  }
  for (size_t i = 0; i < vec32.size(); i++) {
    val = (unsigned long)(uint32_t) vec32[i];
    if ((val & 0xFFFF0000) && !(val & 0xFFFF))
      numSmallEndian++;
    else if ((val&0xFFFF) && !(val&0xFFFF0000))
      numBigEndian++;
  }
  if (numBigEndian >= numSmallEndian)
    return;
  for (size_t i = 0; i < vec16.size(); i++) {
    val = (unsigned long)(uint16_t) vec16[i];
    vec16[i] = (int16_t)((val>>8) & ((val&0xFF)<<8));
  }
  for (size_t i = 0; i < vec32.size(); i++) {
    val = (unsigned long)(uint32_t) vec32[i];
    vec32[i] = (int32_t)((val>>16) & ((val&0xFFFF)<<16));
  }
}

////////////////////////////////////////////////////////////
// read the document information
////////////////////////////////////////////////////////////
bool ClarisWksDocument::readDocInfo()
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  int const vers=m_parserState->m_version;
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  f << "Entries(DocInfo):";
  long expectedSize=vers==1 ? 352 : vers < 6 ? 372 : 374;
  long pos = input->tell();
  long endPos=pos+expectedSize;
  if (!input->checkPosition(endPos)) return false;
  f << "ptr=" << std::hex << input->readULong(4) << std::dec << ",";
  int val;
  for (int i = 0; i < 6; i++) {
    val = (int) input->readULong(2);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  m_state->m_headerId = (int) input->readLong(2);
  if (m_state->m_headerId) f << "headerId=" << m_state->m_headerId << ",";
  val = (int) input->readLong(2);
  if (val) f << "unkn=" << val << ",";
  m_state->m_footerId = (int) input->readLong(2);
  if (m_state->m_footerId) f << "footerId=" << m_state->m_footerId << ",";
  for (int i=0; i < 4; ++i) {
    val = (int) input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  int pages[2];
  for (int i=0; i < 2; ++i)
    pages[i]=(int) input->readLong(2);
  if (pages[1]>=1 && pages[1] < 1000 &&
      (pages[0]==1 || (pages[0]>1 && pages[0]<100 && m_parserState->m_kind == MWAWDocument::MWAW_K_DRAW)))
    m_state->m_pages=Vec2i(pages[0],pages[1]);
  else {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readDocInfo: the number of pages seems bad\n"));
    f << "###";
  }
  if (pages[0]!=1 || pages[1]!=1)
    f << "pages[num]=" << pages[0] << "x" << pages[1] << ",";
  if (vers==1) {
    ascFile.addDelimiter(input->tell(), '|');
    input->seek(8, librevenge::RVNG_SEEK_CUR);
    ascFile.addDelimiter(input->tell(), '|');

    int numCols = (int) input->readLong(2);
    if (numCols < 1 || numCols > 9) {
      MWAW_DEBUG_MSG(("ClarisWksDocument::readDocInfo: pb reading number of columns\n"));
      f << "###numCols=" << numCols;
      numCols = 1;
    }
    if (numCols != 1)
      f << "numCols=" << numCols << ",";
    m_state->m_columns = numCols;
    if (numCols > 1) {
      int colSep = (int) input->readLong(2);
      m_state->m_columnsSep.resize(size_t(numCols-1), colSep);
      f << "colSep=" << colSep << ",";
    }
    else
      input->seek(2, librevenge::RVNG_SEEK_CUR);
  }
  ascFile.addDelimiter(input->tell(), '|');
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(pos+100);
  ascFile.addNote("DocInfo-2");
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// read the document header
////////////////////////////////////////////////////////////
bool ClarisWksDocument::readDocHeader()
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  int const vers=m_parserState->m_version;
  long debPos = input->tell();
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  f << "Entries(DocHeader):";

  int val;
  if (vers >= 6) {
    f << "unkn=[";
    for (int i = 0; i < 4; i++) {
      val = (int) input->readLong(1);
      if (val) f << val << ", ";
      else f << "_, ";
    }
    f << "],";
    for (int i = 0; i < 4; i++) {
      val = (int) input->readLong(2);
      if (val) f << "e" << i << "=" << val << ",";
    }
  }
  long pos = input->tell();
  int zone0Length = 52, zone1Length=0;
  switch (vers) {
  case 1:
    zone0Length = 114;
    zone1Length=50;
    break;
  case 2:
  case 3: // checkme: never see a v3 file
    zone0Length = 116;
    zone1Length=112;
    break;
  case 4:
    zone0Length = 120;
    zone1Length=92;
    break;
  case 5:
    zone0Length = 132;
    zone1Length = 92;
    break;
  case 6:
    zone0Length = 124;
    zone1Length = 1126;
    break;
  default:
    break;
  }
  int totalLength = zone0Length+zone1Length;

  input->seek(totalLength, librevenge::RVNG_SEEK_CUR);
  if (input->tell() != pos+totalLength) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readDocHeader: file is too short\n"));
    return false;
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  val = (int) input->readLong(2); // always find 1
  if (val != 1)
    f << "#unkn=" << std::hex << val << std::dec << ",";
  for (int i = 0; i < 4; i++) {
    val = (int) input->readULong(2);
    if (val)
      f << std::hex << "f" << i << "="  << std::hex << val << std::dec << ",";
  }
  int dim[2];
  for (int i = 0; i < 2; i++)
    dim[i] = (int) input->readLong(2);
  f << "dim?=" << dim[1] << "x" << dim[0] << ",";
  int margin[6];
  f << "margin?=[";
  for (int i = 0; i < 6; i++) {
    margin[i] = (int) input->readLong(2);
    f << margin[i] << ",";
  }
  f << "],";
  if (dim[0] > 0 && dim[1] > 0 &&
      margin[0] >= 0 && margin[1] >= 0 && margin[2] >= 0 && margin[3] >= 0 &&
      dim[0] > margin[0]+margin[2] && dim[1] > margin[1]+margin[3]) {

    Vec2i paperSize(dim[1],dim[0]);
    Vec2i lTopMargin(margin[1], margin[0]);
    Vec2i rBotMargin(margin[3], margin[2]);

    m_parser->getPageSpan().setMarginTop(lTopMargin.y()/72.0);
    m_parser->getPageSpan().setMarginBottom(rBotMargin.y()/72.0);
    m_parser->getPageSpan().setMarginLeft(lTopMargin.x()/72.0);
    m_parser->getPageSpan().setMarginRight(rBotMargin.x()/72.0);
    m_parser->getPageSpan().setFormLength(paperSize.y()/72.);
    m_parser->getPageSpan().setFormWidth(paperSize.x()/72.);
    m_state->m_pageSpanSet = true;
  }
  int dim2[2];
  for (int i = 0; i < 2; i++)
    dim2[i] = (int) input->readLong(2);
  f << "dim2?=" << dim2[1] << "x" << dim2[0] << ",";
  int fl[4];
  f << "fl?=[";
  for (int i = 0; i < 4; i++) {
    fl[i] = (int) input->readULong(1);
    if (fl[i])
      f << fl[i] << ",";
    else
      f << "_,";
  }
  f << "],";
  for (int i = 0; i < 9; i++) {
    val = (int) input->readLong(2);
    if (val)
      f << "g" << i << "="  << val << ",";
  }

  if (long(input->tell()) != pos+zone0Length)
    ascFile.addDelimiter(input->tell(), '|');
  input->seek(pos+zone0Length, librevenge::RVNG_SEEK_SET);
  ascFile.addPos(debPos);
  ascFile.addNote(f.str().c_str());

  /* zone 1 actual font, actul pos, .. */
  if (!getTextParser()->readParagraph())
    return false;
  pos = input->tell();
  f.str("");
  f << "DocHeader:zone?=" << input->readULong(2) << ",";
  if (vers >= 4) f << "unkn=" << input->readULong(2) << ",";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  MWAWFont font;
  int posChar;
  if (!getTextParser()->readFont(-1, posChar, font))
    return false;

  /* zone 2, type, unknown */
  pos = input->tell();
  f.str("");
  f << "DocHeader-1:";
  for (int i = 0; i < 6; i++) {
    val = (int) input->readULong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  input->seek(4, librevenge::RVNG_SEEK_CUR);
  int type = (int) input->readULong(1);
  f << "type=" << type << ",";
  val = (int) input->readULong(1);
  if (type != val) f << "#unkn=" << val << ",";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  if (vers <= 2) {
    // the document font ?
    if (!getTextParser()->readFont(-1, posChar, font))
      return false;
    ascFile.addPos(input->tell());
    ascFile.addNote("DocHeader-2");
    if (vers==2) {
      input->seek(46, librevenge::RVNG_SEEK_CUR);
      long actPos = input->tell();
      f.str("");
      f << "DocHeader(Col):";
      int numCols = (int) input->readLong(2);
      if (numCols < 1 || numCols > 9) {
        MWAW_DEBUG_MSG(("ClarisWksDocument::readDocHeader: pb reading number of columns\n"));
        f << "###numCols=" << numCols;
        numCols = 1;
      }
      if (numCols != 1)
        f << "numCols=" << numCols << ",";
      m_state->m_columns = numCols;
      f << "colsW=[";
      for (int i = 0; i < numCols; i++) {
        val = (int) input->readULong(2);
        m_state->m_columnsWidth.push_back(val);
        f << val << ",";
      }
      f << "],";
      input->seek(actPos+20, librevenge::RVNG_SEEK_SET);
      if (numCols > 1) {
        f << "colsS=[";
        for (int i = 0; i < numCols-1; i++) {
          val = (int) input->readULong(2);
          m_state->m_columnsSep.push_back(val);
          f << input->readULong(2) << ",";
        }
        f << "],";
      }
      input->seek(actPos+36, librevenge::RVNG_SEEK_SET);
      val = (int) input->readLong(2);
      if (val) f << "unkn=" << val << ",";
      ascFile.addPos(actPos);
      ascFile.addNote(f.str().c_str());
    }
  }
  else if (long(input->tell()) != pos+zone1Length)
    ascFile.addDelimiter(input->tell(), '|');
  input->seek(pos+zone1Length, librevenge::RVNG_SEEK_SET);
  if (input->isEnd()) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readDocHeader: file is too short\n"));
    return false;
  }
  switch (vers) {
  case 1:
  case 2: {
    pos = input->tell();
    if (!getTextParser()->readParagraphs())
      return false;
    pos = input->tell();
    if (!readPrintInfo()) {
      MWAW_DEBUG_MSG(("ClarisWksDocument::readDocHeader: can not find print info\n"));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    if (vers==1)
      break;
    pos = input->tell();
    if (!m_styleManager->readPatternList() ||
        !m_styleManager->readGradientList()) {
      input->seek(pos+8, librevenge::RVNG_SEEK_SET);
      return false;
    }
    pos=input->tell();
    ascFile.addPos(pos);
    ascFile.addNote("Entries(DocUnkn0)");
    input->seek(12, librevenge::RVNG_SEEK_CUR);
    if (!readStructZone("DocH0", false)) {
      input->seek(pos+12, librevenge::RVNG_SEEK_SET);
      return false;
    }
    pos=input->tell();
    f.str("");
    f << "Entries(DocUnkn1):";
    long sz=input->readLong(4);
    if (sz) {
      MWAW_DEBUG_MSG(("ClarisWksDocument::readDocHeader: oops find a size for DocUnkn2, we may have a problem\n"));
      f << sz << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    else {
      ascFile.addPos(pos);
      ascFile.addNote("_");
    }
    break;
  }
  case 4:
  case 5:
  case 6: {
    pos = input->tell();
    MWAWEntry entry;
    entry.setBegin(pos);
    entry.setLength(6*260);
    if (!readDSUM(entry, true))
      return false;
    pos = input->tell();
    long sz = (long) input->readULong(4);
    if (!sz) {
      ascFile.addPos(pos);
      ascFile.addNote("Nop");
    }
    else {
      long endPos = pos+4+sz;
      if (!input->checkPosition(endPos)) {
        MWAW_DEBUG_MSG(("ClarisWksDocument::readDocHeader: unexpected LinkInfo size\n"));
        return false;
      }
      ascFile.addPos(pos);
      ascFile.addNote("Entries(LinkInfo)");
      input->seek(endPos, librevenge::RVNG_SEEK_SET);
    }

    if (vers > 4) {
      val = (int) input->readULong(4);
      if (val != long(input->tell())) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        MWAW_DEBUG_MSG(("ClarisWksDocument::readDocHeader: can not find local position\n"));
        ascFile.addPos(pos);
        ascFile.addNote("#");

        return false;
      }
      pos = input->tell(); // series of data with size 42 or 46
      if (!readStructZone("DocUnkn1", false)) {
        input->seek(pos,librevenge::RVNG_SEEK_SET);
        return false;
      }
    }

    pos = input->tell(); // series of data with size 42 or 46
    int expectedSize = 0;
    switch (vers) {
    case 5:
      expectedSize=34;
      break;
    case 6:
      expectedSize=32;
      break;
    default:
      break;
    }
    if (expectedSize) {
      ascFile.addPos(pos);
      ascFile.addNote("DocHeader-3");
      input->seek(pos+expectedSize, librevenge::RVNG_SEEK_SET);
    }

    if (!readPrintInfo()) {
      MWAW_DEBUG_MSG(("ClarisWksDocument::readDocHeader: can not find print info\n"));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }

    for (int z = 0; z < 4; z++) { // zone0, zone1 : color palette, zone2 (val:2, id:2)
      if (z==3 && vers!=4) break;
      pos = input->tell();
      sz = (long) input->readULong(4);
      if (!sz) {
        ascFile.addPos(pos);
        ascFile.addNote("Nop");
        continue;
      }
      entry.setBegin(pos);
      entry.setLength(4+sz);
      if (!input->checkPosition(entry.end())) {
        MWAW_DEBUG_MSG(("ClarisWksDocument::readDocHeader: can not read final zones\n"));
        return false;
      }
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      switch (z) {
      case 0:
        ascFile.addPos(pos);
        ascFile.addNote("DocUnkn2");
        break;
      case 1:
        if (!m_styleManager->readColorList(entry)) {
          input->seek(pos, librevenge::RVNG_SEEK_SET);
          return false;
        }
        break;
      case 2: // a serie of id? num
        if (!readStructZone("DocH0", false)) {
          input->seek(pos, librevenge::RVNG_SEEK_SET);
          return false;
        }
        break;
      case 3: // checkme
        ascFile.addPos(pos);
        ascFile.addNote("DocUnkn4");
        break;
      default:
        break;
      }
      input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    }
    break;
  }
  default:
    break;
  }
  return true;
}

////////////////////////////////////////////////////////////
// a list the document property
////////////////////////////////////////////////////////////
bool ClarisWksDocument::readDSUM(MWAWEntry const &entry, bool inHeader)
{
  if (!entry.valid() || (!inHeader && entry.type() != "DSUM"))
    return false;
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos = entry.begin();
  long debStrings = inHeader ? pos : pos+8;
  input->seek(debStrings, librevenge::RVNG_SEEK_SET);

  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  f << "Entries(DSUM):";
  for (int entete = 0; entete < 6; entete++) {
    char const *(entryNames[]) = { "Title",  "Category", "Description", "Author", "Version", "Keywords"};
    pos = input->tell();
    long sz = (int) input->readULong(4);
    if (!sz) continue;
    int strSize = (int) input->readULong(1);
    if (strSize != sz-1 || pos+4+sz > entry.end()) {
      MWAW_DEBUG_MSG(("ClarisWksDocument::readDSUM: unexpected string size\n"));
      if (pos+4+sz > entry.end() || strSize > sz-1) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        return false;
      }
      f << "###";
    }
    std::string name("");
    for (int i = 0; i < strSize; i++) {
      char c = (char) input->readULong(1);
      if (c) {
        name += c;
        continue;
      }
      MWAW_DEBUG_MSG(("ClarisWksDocument::readDSUM: unexpected string char\n"));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      return false;
    }
    if (name.length())
      f << entryNames[entete] << "=" << name << ",";
    input->seek(pos+4+sz, librevenge::RVNG_SEEK_SET);
  }

  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool ClarisWksDocument::readPrintInfo()
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos = input->tell();
  if (input->readULong(2) != 0) return false;
  long sz = (long) input->readULong(2);
  if (sz < 0x78)
    return false;
  long endPos = pos+4+sz;
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readPrintInfo: file is too short\n"));
    return false;
  }
  input->seek(pos+4, librevenge::RVNG_SEEK_SET);

  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  // print info
  libmwaw::PrinterInfo info;
  if (!info.read(input)) {
    if (sz == 0x78) {
      // the size is ok, so let try to continue
      ascFile.addPos(pos);
      ascFile.addNote("Entries(PrintInfo):##");
      input->seek(endPos, librevenge::RVNG_SEEK_SET);
      MWAW_DEBUG_MSG(("ClarisWksDocument::readPrintInfo: can not read print info, continue\n"));
      return true;
    }
    return false;
  }
  f << "Entries(PrintInfo):"<< info;

  Vec2i paperSize = info.paper().size();
  Vec2i pageSize = info.page().size();
  if (pageSize.x() <= 0 || pageSize.y() <= 0 ||
      paperSize.x() <= 0 || paperSize.y() <= 0) return false;

  if (!m_state->m_pageSpanSet) {
    // define margin from print info
    Vec2i lTopMargin= -1 * info.paper().pos(0);
    Vec2i rBotMargin=info.paper().size() - info.page().size();

    // move margin left | top
    int decalX = lTopMargin.x() > 14 ? lTopMargin.x()-14 : 0;
    int decalY = lTopMargin.y() > 14 ? lTopMargin.y()-14 : 0;
    lTopMargin -= Vec2i(decalX, decalY);
    rBotMargin += Vec2i(decalX, decalY);

    m_parser->getPageSpan().setMarginTop(lTopMargin.y()/72.0);
    m_parser->getPageSpan().setMarginBottom(rBotMargin.y()/72.0);
    m_parser->getPageSpan().setMarginLeft(lTopMargin.x()/72.0);
    m_parser->getPageSpan().setMarginRight(rBotMargin.x()/72.0);
    m_parser->getPageSpan().setFormLength(paperSize.y()/72.);
    m_parser->getPageSpan().setFormWidth(paperSize.x()/72.);
  }

  if (long(input->tell()) !=endPos) {
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    f << ", #endPos";
    ascFile.addDelimiter(input->tell(), '|');
  }

  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  return true;
}

///////////////////////////////////////////////////////////
// try to read a unknown structured zone
////////////////////////////////////////////////////////////
bool ClarisWksDocument::readStructZone(char const *zoneName, bool hasEntete)
{
  if (!m_parserState) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readStructZone: can not find the parser state\n"));
    return false;
  }
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  if (!input->checkPosition(endPos)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksDocument::readStructZone: unexpected size for %s\n", zoneName));
    return false;
  }
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(" << zoneName << "):";

  if (sz == 0) {
    if (hasEntete) {
      ascFile.addPos(pos-4);
      ascFile.addNote(f.str().c_str());
    }
    else {
      ascFile.addPos(pos);
      ascFile.addNote("NOP");
    }
    return true;
  }

  int N = (int) input->readLong(2);
  f << "N=" << N << ",";
  int type = (int) input->readLong(2);
  if (type != -1)
    f << "#type=" << type << ",";
  int val = (int) input->readLong(2);
  if (val) f << "#unkn=" << val << ",";
  int fSz = (int) input->readULong(2);
  int hSz = (int) input->readULong(2);
  if (!fSz || N *fSz+hSz+12 != sz) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksDocument::readStructZone: unexpected size for %s\n", zoneName));
    return false;
  }

  if (long(input->tell()) != pos+4+hSz)
    ascFile.addDelimiter(input->tell(), '|');
  ascFile.addPos(hasEntete ? pos-4 : pos);
  ascFile.addNote(f.str().c_str());

  long debPos = endPos-N*fSz;
  for (int i = 0; i < N; i++) {
    input->seek(debPos, librevenge::RVNG_SEEK_SET);
    f.str("");
    f << zoneName << "-" << i << ":";

    long actPos = input->tell();
    if (actPos != debPos && actPos != debPos+fSz)
      ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(debPos);
    ascFile.addNote(f.str().c_str());
    debPos += fSz;
  }
  input->seek(endPos,librevenge::RVNG_SEEK_SET);
  return true;
}

// try to read a list of structured zone
bool ClarisWksDocument::readStructIntZone(char const *zoneName, bool hasEntete, int intSz, std::vector<int> &res)
{
  if (!m_parserState) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readStructZone: can not find the parser state\n"));
    return false;
  }
  res.resize(0);
  if (intSz != 1 && intSz != 2 && intSz != 4) {
    MWAW_DEBUG_MSG(("ClarisWksDocument::readStructIntZone: unknown int size: %d\n", intSz));
    return false;
  }

  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  input->seek(endPos,librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != endPos) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksDocument::readStructIntZone: unexpected size for %s\n", zoneName));
    return false;
  }
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  if (zoneName && strlen(zoneName))
    f << "Entries(" << zoneName << "):";

  if (sz == 0) {
    if (hasEntete) {
      ascFile.addPos(pos-4);
      ascFile.addNote(f.str().c_str());
    }
    else {
      ascFile.addPos(pos);
      ascFile.addNote("NOP");
    }
    return true;
  }

  input->seek(pos+4, librevenge::RVNG_SEEK_SET);
  int N = (int) input->readLong(2);
  f << "N=" << N << ",";
  int type = (int) input->readLong(2);
  if (type != -1)
    f << "#type=" << type << ",";
  long val = input->readLong(2);
  if (val) f << "#unkn=" << val << ",";
  int fSz = (int) input->readULong(2);
  int hSz = (int) input->readULong(2);
  if (fSz != intSz || N *fSz+hSz+12 != sz) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksDocument::readStructIntZone: unexpected field size\n"));
    return false;
  }

  long debPos = endPos-N*fSz;
  if (long(input->tell()) != debPos) {
    ascFile.addDelimiter(input->tell(), '|');
    if (N) ascFile.addDelimiter(debPos, '|');
  }
  input->seek(debPos, librevenge::RVNG_SEEK_SET);
  f << "[";
  for (int i = 0; i < N; i++) {
    val = input->readLong(fSz);
    res.push_back((int) val);
    f << val << ",";
  }
  f << "]";

  ascFile.addPos(hasEntete ? pos-4 : pos);
  ascFile.addNote(f.str().c_str());

  input->seek(endPos,librevenge::RVNG_SEEK_SET);
  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
