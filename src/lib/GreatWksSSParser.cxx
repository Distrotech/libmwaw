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
#include <set>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWHeader.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSection.hxx"
#include "MWAWSpreadsheetListener.hxx"
#include "MWAWSubDocument.hxx"

#include "GreatWksDocument.hxx"
#include "GreatWksGraph.hxx"
#include "GreatWksText.hxx"

#include "GreatWksSSParser.hxx"

/** Internal: the structures of a GreatWksSSParser */
namespace GreatWksSSParserInternal
{
/**a style of a GreatWksSSParser */
class Style
{
public:
  /** constructor */
  Style() : m_font(3,10), m_backgroundColor(MWAWColor::white())
  {
  }
  /** the font style */
  MWAWFont m_font;
  /** the cell background color */
  MWAWColor m_backgroundColor;
};

/** a cell of a GreatWksSSParser */
class Cell : public MWAWCell
{
public:
  /// constructor
  Cell() : m_content(), m_style(-1) { }
  //! returns true if the cell do contain any content
  bool isEmpty() const
  {
    return m_content.empty() && !hasBorders();
  }

  //! the cell content
  MWAWCellContent m_content;
  /** the cell style */
  int m_style;
};

/** the spreadsheet of a of a MsWksSSParser */
class Spreadsheet
{
public:
  //! constructor
  Spreadsheet() : m_widthDefault(75), m_widthCols(), m_heightDefault(13), m_heightRows(),
    m_cells(), m_name("Sheet0")
  {
  }
  //! returns the row size in point
  int getRowHeight(int row) const
  {
    if (row>=0&&row<(int) m_heightRows.size())
      return m_heightRows[size_t(row)];
    return m_heightDefault;
  }
  //! convert the m_widthCols in a vector of of point size
  std::vector<float> convertInPoint(std::vector<int> const &list) const
  {
    size_t numCols=size_t(getRightBottomPosition()[0]+1);
    std::vector<float> res;
    res.resize(numCols);
    for (size_t i = 0; i < numCols; i++) {
      if (i>=list.size() || list[i] < 0) res[i] = float(m_widthDefault);
      else res[i] = float(list[i]);
    }
    return res;
  }
  /** the default column width */
  int m_widthDefault;
  /** the column size in points */
  std::vector<int> m_widthCols;
  /** the default row height */
  int m_heightDefault;
  /** the row height in points */
  std::vector<int> m_heightRows;
  /** the list of not empty cells */
  std::vector<Cell> m_cells;
  /** the spreadsheet name */
  std::string m_name;
protected:
  /** returns the last Right Bottom cell position */
  Vec2i getRightBottomPosition() const
  {
    int maxX = 0, maxY = 0;
    size_t numCell = m_cells.size();
    for (size_t i = 0; i < numCell; i++) {
      Vec2i const &p = m_cells[i].position();
      if (p[0] > maxX) maxX = p[0];
      if (p[1] > maxY) maxY = p[1];
    }
    return Vec2i(maxX, maxY);
  }
};

////////////////////////////////////////
//! Internal: the state of a GreatWksSSParser
struct State {
  //! constructor
  State() : m_spreadsheet(), m_styleList(), m_actPage(0), m_numPages(0),
    m_headerEntry(), m_footerEntry(), m_headerPrint(false), m_footerPrint(false), m_headerHeight(0), m_footerHeight(0)
  {
  }
  //! returns the style corresponding to an id
  Style getStyle(int id) const
  {
    if (id<0 || id>=int(m_styleList.size())) {
      MWAW_DEBUG_MSG(("GreatWksSSParserInternal::State: can not find the style %d\n", id));
      return Style();
    }
    return m_styleList[size_t(id)];
  }
  /** the spreadsheet */
  Spreadsheet m_spreadsheet;
  /** the list of style */
  std::vector<Style> m_styleList;
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  MWAWEntry m_headerEntry /** the header entry (in v2)*/, m_footerEntry/**the footer entry (in v2)*/;
  bool m_headerPrint /** the header is printed */, m_footerPrint /* the footer is printed */;
  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a GreatWksSSParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(GreatWksSSParser &pars, MWAWInputStreamPtr input, int zoneId) :
    MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const
  {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  //! the subdocument id
  int m_id;
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType type)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("GreatWksSSParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  if (type!=libmwaw::DOC_HEADER_FOOTER) {
    MWAW_DEBUG_MSG(("GreatWksSSParserInternal::SubDocument::parse: unknown type\n"));
    return;
  }

  assert(m_parser);

  long pos = m_input->tell();
  static_cast<GreatWksSSParser *>(m_parser)->sendHF(m_id);
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_id != sDoc->m_id) return true;
  return false;
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
GreatWksSSParser::GreatWksSSParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWSpreadsheetParser(input, rsrcParser, header), m_state(), m_document()
{
  init();
}

GreatWksSSParser::~GreatWksSSParser()
{
}

void GreatWksSSParser::init()
{
  resetSpreadsheetListener();
  setAsciiName("main-1");

  m_state.reset(new GreatWksSSParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);

  m_document.reset(new GreatWksDocument(*this));
}

////////////////////////////////////////////////////////////
// interface with the text parser
////////////////////////////////////////////////////////////
bool GreatWksSSParser::sendHF(int id)
{
  return m_document->getTextParser()->sendTextbox(id==0 ? m_state->m_headerEntry : m_state->m_footerEntry);
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void GreatWksSSParser::parse(librevenge::RVNGSpreadsheetInterface *docInterface)
{
  assert(getInput().get() != 0);
  if (!checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = false;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());

    checkHeader(0L);
    ok = createZones();
    if (ok) {
      createDocument(docInterface);
      sendSpreadsheet();
    }
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("GreatWksSSParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetSpreadsheetListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void GreatWksSSParser::createDocument(librevenge::RVNGSpreadsheetInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getSpreadsheetListener()) {
    MWAW_DEBUG_MSG(("GreatWksSSParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  int numPages = 1;
  if (m_document->getGraphParser()->numPages() > numPages)
    numPages = m_document->getGraphParser()->numPages();
  if (m_document->getTextParser()->numPages() > numPages)
    numPages = m_document->getTextParser()->numPages();
  m_state->m_numPages = numPages;

  MWAWPageSpan ps(getPageSpan());
  std::vector<MWAWPageSpan> pageList;
  if (m_state->m_headerEntry.valid()) {
    MWAWHeaderFooter header(MWAWHeaderFooter::HEADER, MWAWHeaderFooter::ALL);
    header.m_subDocument.reset(new GreatWksSSParserInternal::SubDocument(*this, getInput(), 0));
    ps.setHeaderFooter(header);
  }
  if (m_state->m_footerEntry.valid()) {
    MWAWHeaderFooter footer(MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
    footer.m_subDocument.reset(new GreatWksSSParserInternal::SubDocument(*this, getInput(), 1));
    ps.setHeaderFooter(footer);
  }
  ps.setPageSpan(numPages);
  pageList.push_back(ps);
  MWAWSpreadsheetListenerPtr listen(new MWAWSpreadsheetListener(*getParserState(), pageList, documentInterface));
  setSpreadsheetListener(listen);
  listen->startDocument();
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool GreatWksSSParser::createZones()
{
  m_document->readRSRCZones();

  MWAWInputStreamPtr input = getInput();
  long pos=16;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  if (!readSpreadsheet())
    return false;

  if (!input->isEnd()) {
    pos = input->tell();
    MWAW_DEBUG_MSG(("GreatWksSSParser::createZones: find some extra data\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(Loose):");
    ascii().addPos(pos+100);
    ascii().addNote("_");
  }

  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read a spreadsheet
////////////////////////////////////////////////////////////
bool GreatWksSSParser::readSpreadsheet()
{
  MWAWInputStreamPtr input = getInput();
  int const vers=version();
  long pos = input->tell();
  libmwaw::DebugStream f;

  while (!input->isEnd()) {
    bool ok=true, printDone=false;
    pos = input->tell();
    f.str("");
    int type=(int) input->readLong(2);
    if (type==0x10) {
      if (vers==1)
        f << "Entries(DocInfo):";
      else // some patterns?
        f << "Entries(Zone10):";
    }
    else if (type>=0 && type < 0x1a) {
      static char const *(wh[0x1a]) = {
        "_", "FontNames", "Style", "Column", "Column", "Row", "Zone6", "Row",
        "Column", "Zone9", "Zonea", "Zoneb", "Row", "Screen", "DocOptions", "Selection",
        "DocInfo", "CalcMode", "Zone12", "Zone13", "Zone14", "GridOptions", "DocInfo", "Header",
        "Footer", "Chart"
      };
      f << "Entries(" << wh[type] << "):";
    }
    else if (type==0x1c)
      f << "Entries(Graphic):";
    else
      f << "Entries(Zone" << std::hex << type << std::dec << "):";
    long endPos=0;
    switch (type) {
    case 1:
      if (!m_document->getTextParser()->readFontNames()) {
        ok=false;
        break;
      }
      printDone = true;
      endPos=input->tell();
      break;
    case 2:
      if (!readStyles()) {
        ok=false;
        break;
      }
      printDone = true;
      endPos=input->tell();
      break;
    case 3: // column
    case 5: { // row :
      long sz=(long) input->readULong(2);
      endPos=pos+4+(long) sz;
      ok = (sz>=4 && input->checkPosition(endPos));
      if (!ok) break;
      int id=(int) input->readLong(2);
      int length=(int) input->readLong(2);
      if (id<1 || length < 0) {
        MWAW_DEBUG_MSG(("GreatWksSSParser::readSpreadsheet: the row/col definition seems bad\n"));
        f << "###";
      }
      else {
        std::vector<int> &lengths=type==3 ? m_state->m_spreadsheet.m_widthCols : m_state->m_spreadsheet.m_heightRows;
        if (id-1 >= (int) lengths.size())
          lengths.resize(size_t(id), -1);
        lengths[size_t(id-1)]=length;
      }
      f << "pos=" << id << ",";
      f << "size=" << length << ",";
      // readULong(1)&1 -> page break?
      break;
    }
    case 4:
      f << "end";
      endPos=pos+2;
      break;
    case 6:
      endPos=pos+10;
      if (!input->checkPosition(endPos)) {
        ok=false;
        break;
      }
      for (int i=0; i < 4; ++i) {
        int val=(int) input->readLong(2);
        static int const expected[]= {9,0,2,0x2e};
        if (val!=expected[i])
          f << "f" << i << "=" << val << ",";
      }
      break;
    // case 0x10: dim ? + string + select cell...

    case 7: // d default row size
    case 8: // 4b default column size
    case 0xb: // 0|4
    case 0xc: // d
    case 0x11: // 0
    case 0x1a: { // 1
      long sz=(long) input->readULong(4);
      endPos=pos+6+sz;
      if (!input->checkPosition(endPos)) {
        ok=false;
        break;
      }
      if (sz!=2) {
        f << "###";
        MWAW_DEBUG_MSG(("GreatWksSSParser::readSpreadsheet: size of zone %d seems odd\n", type));
        break;
      }
      int val=(int)input->readULong(2);
      switch (type) {
      case 7:
        m_state->m_spreadsheet.m_heightDefault=val;
        f << "height[def]=" << val << ",";
        break;
      case 8:
        m_state->m_spreadsheet.m_widthDefault=val;
        f << "width[def]=" << val << ",";
        break;
      case 0xc: // similar to 7 related to printing?
        f << "height[def2]=" << val << ",";
        break;
      case 0x11:
        if (val==1) f << "manual,";
        else if (val) f << "##val=" << val << ",";
        break;
      default:
        f << "f0=" << val << ",";
        break;
      }
      break;
    }
    case 0xd: {
      long sz=(long) input->readULong(4);
      endPos=pos+6+sz;
      if (!input->checkPosition(endPos)) {
        ok=false;
        break;
      }
      if ((vers==1 && sz!=0x8) || (vers==2 && sz!=0x18)) {
        f << "###";
        MWAW_DEBUG_MSG(("GreatWksSSParser::readSpreadsheet: size of screen zone seems odd\n"));
        break;
      }
      int dim[4];
      for (int i=0; i<4; ++i)
        dim[i]=(int)input->readLong(2);
      f << dim[0] << "x" << dim[1] << "<->" << dim[2] << "x" << dim[3] << ",";
      if (vers==1) break;
      for (int i=0; i<4; ++i)
        dim[i]=(int)input->readLong(2);
      f << "screen2?=" << dim[0] << "x" << dim[1] << "<->" << dim[1] << "x" << dim[2] << ",";
      // now find 00000003cccc3333
      break;
    }
    case 0xe: {
      long sz=(long) input->readULong(4);
      endPos=pos+6+sz;
      if (!input->checkPosition(endPos)) {
        ok=false;
        break;
      }
      if (sz!=0xa) {
        f << "###";
        MWAW_DEBUG_MSG(("GreatWksSSParser::readSpreadsheet: size of selection zone seems odd\n"));
        break;
      }
      int val;
      for (int i=0; i<9; ++i) {
        val=(int)input->readULong(1);
        if (!val) continue;
        // checkme: same value in v2 ?
        static char const *(what[])= {"grid[display]", "headerRC[display]", "zero[doc]", "formula[doc]", "grid[print]", "headerRC[print]", "header[print]", "footer[print]", "protected" };
        if (val==1) {
          if (i==6)
            m_state->m_headerPrint=true;
          else if (i==7)
            m_state->m_footerPrint=true;
          f << what[i] << ",";
        }
        else
          f << "###" << what[i] << "=" << val << ",";
      }
      val=(int)input->readULong(1);
      if (val!=1) f << "first[page]=" << val << ",";
      break;
    }
    case 0xf: {
      long sz=(long) input->readULong(4);
      endPos=pos+6+sz;
      if (!input->checkPosition(endPos)) {
        ok=false;
        break;
      }
      if (sz!=0xe) {
        f << "###";
        MWAW_DEBUG_MSG(("GreatWksSSParser::readSpreadsheet: size of selection zone seems odd\n"));
        break;
      }
      int val=(int)input->readULong(2);
      if (val != 4) f << "f0=" << val << ",";
      f << "cells=[";
      for (int i=0; i<3; ++i) // probably selected cell, followed by a cell range
        f << input->readLong(2) << "x" << input->readLong(2) << ",";
      f << "],";
      break;
    }
    case 0x10: {
      long sz=(long) input->readULong(4);
      endPos=pos+6+sz;
      if (!input->checkPosition(endPos)) {
        ok=false;
        break;
      }
      if (vers==2) break;
      if (sz!=0x12) {
        f << "###";
        MWAW_DEBUG_MSG(("GreatWksSSParser::readSpreadsheet: size of docinfo zone seems odd\n"));
        break;
      }
      int dim[4];
      for (int i=0; i<4; ++i)
        dim[i]=(int)input->readLong(2);
      f << "page=" << dim[0] << "x" << dim[1] << "<->" << dim[2] << "x" << dim[3] << ",";
      if (input->readLong(1)!=1) break;
      input->seek(1,librevenge::RVNG_SEEK_CUR);
      for (int i=0; i<4; ++i)
        dim[i]=(int)input->readLong(2);
      f << "select=" << dim[0] << "x" << dim[1] << "<->" << dim[2] << "x" << dim[3] << ",";
      break;
    }
    case 0x13: {
      GreatWksSSParserInternal::Cell cell;
      if (!readCell(cell)) {
        ok=false;
        break;
      }
      if (!cell.isEmpty() || !m_state->getStyle(cell.m_style).m_backgroundColor.isWhite())
        m_state->m_spreadsheet.m_cells.push_back(cell);
      printDone = true;
      endPos=input->tell();
      break;
    }
    case 0x14:
      ascii().addPos(pos);
      ascii().addNote("_");
      return true;
    case 0x15: {
      long sz=(long) input->readULong(4);
      endPos=pos+6+sz;
      if (!input->checkPosition(endPos)) {
        ok=false;
        break;
      }
      if (sz!=0xa || vers!=2) {
        f << "###";
        MWAW_DEBUG_MSG(("GreatWksSSParser::readSpreadsheet: size of grid option zone seems odd\n"));
        break;
      }
      int val=(int)input->readLong(2);
      if (val!=1) f << "unit=" << val << ",";
      val=(int)input->readLong(2);
      if (val!=5) f << "subdiv=" << val << ",";
      break;
    }
    case 0x16: {
      long sz=(long) input->readULong(4);
      endPos=pos+6+sz;
      if (!input->checkPosition(endPos)) {
        ok=false;
        break;
      }
      if (sz!=0x30 || vers!=2) {
        f << "###";
        MWAW_DEBUG_MSG(("GreatWksSSParser::readSpreadsheet: size of doc info zone seems odd\n"));
        break;
      }
      f << "margins=[";
      for (int j=0; j<4; ++j)
        f << double(input->readLong(4))/65536. << ",";
      f << "],";
      for (int c=0; c<2; ++c) {
        // two times the same values: related to header/footer ?
        f << "unkn" << c << "=[";
        for (int j=0; j<8; ++j) {
          int val=(int)input->readLong(2);
          if (val)
            f << val << ",";
          else
            f << "_,";
        }
        f << "],";
      }
      break;
    }
    case 0x17:
    case 0x18: {
      long sz=(long) input->readULong(4);
      endPos=pos+6+sz;
      if (!input->checkPosition(endPos)) {
        ok=false;
        break;
      }
      MWAWEntry entry;
      entry.setBegin(pos+6);
      entry.setEnd(endPos);
      if (type==0x17)
        m_state->m_headerEntry=entry;
      else
        m_state->m_footerEntry=entry;
      break;
    }
    case 0x19:
      if (!readChart()) {
        ok=false;
        break;
      }
      printDone = true;
      endPos=input->tell();
      break;
    case 0x1c:
      input->seek(-2, librevenge::RVNG_SEEK_CUR);
      if (!m_document->getGraphParser()->readPageFrames()) {
        ok=false;
        break;
      }
      printDone = true;
      endPos=input->tell();
      break;
    default: {
      endPos=pos+6+(long) input->readULong(4);
      if (type <= 0 || !input->checkPosition(endPos)) {
        ok=false;
        break;
      }
      break;
    }
    }
    if (!ok && type>=7) {
      // try to use the default value
      input->seek(pos+2, librevenge::RVNG_SEEK_SET);
      long sz=type==0x14 ? 0 : (long) input->readULong(4);
      endPos=pos+6+sz;
      ok=input->checkPosition(endPos);
    }
    if (!ok) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    if (input->tell()!=endPos)
      ascii().addDelimiter(input->tell(),'|');
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    if (printDone)
      continue;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
bool GreatWksSSParser::sendSpreadsheet()
{
  MWAWSpreadsheetListenerPtr listener=getSpreadsheetListener();
  MWAWInputStreamPtr &input= getInput();
  if (!listener) {
    MWAW_DEBUG_MSG(("GreatWksSSParser::sendSpreadsheet: I can not find the listener\n"));
    return false;
  }
  GreatWksSSParserInternal::Spreadsheet &sheet = m_state->m_spreadsheet;
  size_t numCell = sheet.m_cells.size();

  listener->openSheet(sheet.convertInPoint(sheet.m_widthCols), librevenge::RVNG_POINT, sheet.m_name);
  m_document->getGraphParser()->sendPageGraphics();

  int prevRow = -1;
  for (size_t i = 0; i < numCell; i++) {
    GreatWksSSParserInternal::Cell cell= sheet.m_cells[i];
    if (cell.position()[1] != prevRow) {
      while (cell.position()[1] > prevRow) {
        if (prevRow != -1)
          listener->closeSheetRow();
        prevRow++;
        listener->openSheetRow((float)sheet.getRowHeight(prevRow), librevenge::RVNG_POINT);
      }
    }

    GreatWksSSParserInternal::Style style=m_state->getStyle(cell.m_style);
    cell.setFont(style.m_font);
    if (!style.m_backgroundColor.isWhite())
      cell.setBackgroundColor(style.m_backgroundColor);
    listener->openSheetCell(cell, cell.m_content);
    if (cell.m_content.m_textEntry.valid()) {
      listener->setFont(style.m_font);
      input->seek(cell.m_content.m_textEntry.begin(), librevenge::RVNG_SEEK_SET);
      while (!input->isEnd() && input->tell()<cell.m_content.m_textEntry.end()) {
        unsigned char c=(unsigned char) input->readULong(1);
        if (c==0xd)
          listener->insertEOL();
        else
          listener->insertCharacter(c);
      }
    }
    listener->closeSheetCell();
  }
  if (prevRow!=-1) listener->closeSheetRow();
  listener->closeSheet();
  return true;
}

////////////////////////////////////////////////////////////
// read the chart zone
////////////////////////////////////////////////////////////
bool GreatWksSSParser::readChart()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;

  f << "Entries(Chart):";
  long sz=(long) input->readULong(4);
  long endPos=pos+4+sz;
  if (sz < 4 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("GreatWksSSParser::readChart: can not find the chart zone size\n"));
    f << "###";
    ascii().addPos(pos-2);
    ascii().addNote(f.str().c_str());
    return false;
  }
  int N=(int) input->readULong(2);
  int fSz=(int) input->readULong(2);
  if (N) f << "N=" << N << ",";
  if ((N && fSz!=0x14) || long(4+N*fSz)!=sz) {
    MWAW_DEBUG_MSG(("GreatWksSSParser::readChart: can not find the number of chart\n"));
    f << "###";
    ascii().addPos(pos-2);
    ascii().addNote(f.str().c_str());
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    return true;
  }
  ascii().addPos(pos-2);
  ascii().addNote(f.str().c_str());
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "Chart-" << i << ":";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+20, librevenge::RVNG_SEEK_SET);
  }
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// read a cell
////////////////////////////////////////////////////////////
bool GreatWksSSParser::readCell(GreatWksSSParserInternal::Cell &cell)
{
  cell=GreatWksSSParserInternal::Cell();

  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;

  f << "Entries(Cell):";
  long sz=(long) input->readULong(4);
  long endPos=pos+4+sz;
  if (sz < 0x12 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("GreatWksSSParser::readCell: can not find a cell\n"));
    f << "###";
    ascii().addPos(pos-2);
    ascii().addNote(f.str().c_str());
    return false;
  }
  int cPos[2];
  for (int i=0; i<2; ++i)
    cPos[i]=(int) input->readLong(2);
  Vec2i cellPos(cPos[0]-1,cPos[1]-1);
  cell.setPosition(cellPos);
  f << "cell=" << cellPos << ",";
  cell.m_style=(int) input->readLong(2);
  f << "style" << cell.m_style << ",";
  int val=(int) input->readULong(2);
  int form=(val>>8)&0x1f;
  MWAWCell::Format format;
  format.m_format=MWAWCell::F_TEXT;
  switch (form) {
  case 0:
    format.m_numberFormat=MWAWCell::F_NUMBER_GENERIC;
    break;
  case 1:
    format.m_format=MWAWCell::F_NUMBER;
    format.m_numberFormat=MWAWCell::F_NUMBER_CURRENCY;
    f << "currency,";
    break;
  case 2:
    format.m_format=MWAWCell::F_NUMBER;
    format.m_numberFormat=MWAWCell::F_NUMBER_PERCENT;
    f << "percent,";
    break;
  case 3:
    format.m_format=MWAWCell::F_NUMBER;
    format.m_numberFormat=MWAWCell::F_NUMBER_DECIMAL;
    f << "fixed,";
    break;
  case 4:
    format.m_format=MWAWCell::F_NUMBER;
    format.m_numberFormat=MWAWCell::F_NUMBER_SCIENTIFIC;
    f << "scientific,";
    break;
  case 5:
    format.m_format=MWAWCell::F_BOOLEAN;
    f << "bool,";
    break;
  case 10:
  case 11:
  case 12:
  case 13:
  case 14:
  case 15:
  case 16:
  case 17:
  case 18: {
    static char const *(wh[]) = {
      "%m/%d/%y", "%b %d, %Y", "%b %Y", "%b %d", "%B %d, %Y", "%B %Y", "%B %d", "%a, %b %d, %Y", "%A, %B %d, %Y"
    };
    format.m_format=MWAWCell::F_DATE;
    format.m_DTFormat=wh[form-10];
    f << wh[form-10] << ",";
    break;
  }
  case 20:
  case 21:
  case 22:
  case 23: {
    static char const *(wh[]) = {
      "%H:%M", "%H:%M:%S", "%I:%M %p", "%I:%M:%S %p"
    };
    format.m_format=MWAWCell::F_TIME;
    format.m_DTFormat=wh[form-20];
    f << wh[form-20] << ",";
    break;
  }
  default:
    f << "#format=" <<form << ",";
    break;
  }
  format.m_digits=(val & 0xF);
  if ((val & 0xF)!=2)
    f << "decimal=" << format.m_digits << ",";
  if (val & 0x4000) f << "parenthesis,";
  if (val & 0x8000) f << "commas,";
  val &=0x20F0;
  if (val)
    f << "fl0=" << std::hex << val << std::dec << ",";
  val=(int) input->readULong(2);
  if (val&0xf) {
    f << "bord=";
    int wh=0;
    if (val&1) {
      wh|=libmwaw::TopBit;
      f << "T";
    }
    if (val&2) {
      wh|=libmwaw::LeftBit;
      f << "L";
    }
    if (val&4) {
      wh|=libmwaw::BottomBit;
      f << "B";
    }
    if (val&8) {
      wh|=libmwaw::RightBit;
      f << "R";
    }
    f << ",";
    cell.setBorders(wh, MWAWBorder());
  }
  switch ((val>>8)&0x7) {
  case 0: // none
    break;
  case 1:
    cell.setHAlignement(MWAWCell::HALIGN_LEFT);
    f << "align=left,";
    break;
  case 2:
    cell.setHAlignement(MWAWCell::HALIGN_CENTER);
    f << "align=center,";
    break;
  case 3:
    cell.setHAlignement(MWAWCell::HALIGN_RIGHT);
    f << "align=right,";
    break;
  case 4:
    f << "align=repeat,";
    break;
  default:
    f << "#align=" << ((val>>8)&0x7) << ",";
  }
  if (val&0x8000) {
    cell.setProtected(true);
    f << "protected,";
  }
  val &= 0x78F0;
  if (val)
    f << "fl1=" << std::hex << val << std::dec << ",";
  val=(int) input->readULong(2); // small number between 0 and 255
  if (val)
    f << "f0=" << val << ",";
  int formSz=(int) input->readULong(2);
  if (formSz&0x8000) { // appear if now, rand, ... is used
    f << "formula[update],";
    formSz &= 0x7FFF;
  }
  bool ok=true;
  MWAWCellContent &content=cell.m_content;
  content.m_contentType=MWAWCellContent::C_NONE;
  if (formSz) {
    long formulaPos=input->tell();
    f << "formula,";
    if (formulaPos+formSz+4>=endPos) {
      MWAW_DEBUG_MSG(("GreatWksSSParser::readCell: formula size seems bad\n"));
      f << "##formula[size]=" << formSz << ",";
      ok=false;
    }
    else {
      ascii().addDelimiter(formulaPos,'|');
      ascii().addDelimiter(formulaPos+formSz,'|');
      std::vector<MWAWCellContent::FormulaInstruction> formula;
      std::string error("");
      if (m_document->readFormula(cellPos,formulaPos+formSz,formula, error)) {
        content.m_contentType=MWAWCellContent::C_FORMULA;
        content.m_formula=formula;
      }
      f << "[";
      for (size_t l=0; l < formula.size(); ++l)
        f << formula[l];
      f << "]" << error << ",";
      if (formSz&1) formSz+=1;
      input->seek(formulaPos+formSz, librevenge::RVNG_SEEK_SET);
    }
  }

  if (ok && input->tell()+4<=endPos) {
    int typeVal=(int) input->readLong(2);
    long valuePos=input->tell();
    switch (typeVal) {
    case 1: // no value
      break;
    case 5: {
      if (format.m_format==MWAWCell::F_TEXT)
        format.m_format=MWAWCell::F_NUMBER;
      if (content.m_contentType!=MWAWCellContent::C_FORMULA)
        content.m_contentType=MWAWCellContent::C_NUMBER;
      if (valuePos+10+2>endPos) {
        MWAW_DEBUG_MSG(("GreatWksSSParser::readCell: can not read a float value\n"));
        f << "###";
        ok=false;
        break;
      }
      double value=0;
      bool isNAN=false;
      if (!input->readDouble10(value, isNAN)) {
        MWAW_DEBUG_MSG(("GreatWksSSParser::readCell: can not read a float value(II)\n"));
        f << "###";
      }
      else if (format.m_format==MWAWCell::F_DATE) // change the reference date from 1/1/1904 to 1/1/1900
        content.setValue(value+1460.);
      else
        content.setValue(value);
      f << value << ",";
      break;
    }
    case 7: {
      if (content.m_contentType!=MWAWCellContent::C_FORMULA)
        content.m_contentType=MWAWCellContent::C_TEXT;
      int sSz=(int) input->readULong(1);
      if (valuePos+1+sSz+2>endPos) {
        MWAW_DEBUG_MSG(("GreatWksSSParser::readCell: can not read a string value\n"));
        f << "###";
        ok=false;
        break;
      }
      content.m_textEntry.setBegin(valuePos+1);
      content.m_textEntry.setLength(sSz);
      std::string text("");
      for (int i=0; i<sSz; ++i)
        text += (char) input->readULong(1);
      f << "\"" << text << "\",";
      if ((sSz%2)==0)
        input->seek(1, librevenge::RVNG_SEEK_CUR);
      break;
    }
    case 8: { // often a boolean value
      if (format.m_format==MWAWCell::F_TEXT)
        format.m_format=MWAWCell::F_BOOLEAN;
      if (content.m_contentType!=MWAWCellContent::C_FORMULA)
        content.m_contentType=MWAWCellContent::C_NUMBER;
      if (valuePos+4+2>endPos) {
        MWAW_DEBUG_MSG(("GreatWksSSParser::readCell: can not read a long value\n"));
        ok = false;
        f << "###";
        break;
      }
      long value=input->readLong(4);
      if (format.m_format==MWAWCell::F_DATE) // change the reference date from 1/1/1904 to 1/1/1900
        content.setValue(double(value)+1460.);
      else
        content.setValue(double(value));
      f << value << ",";
      break;
    }
    case 15: {
      if (format.m_format==MWAWCell::F_TEXT)
        format.m_format=MWAWCell::F_NUMBER;
      if (content.m_contentType!=MWAWCellContent::C_FORMULA)
        content.m_contentType=MWAWCellContent::C_NUMBER;
      if (valuePos+4+2>endPos) {
        MWAW_DEBUG_MSG(("GreatWksSSParser::readCell: can not read a nan value\n"));
        ok = false;
        f << "###";
        break;
      }
      input->seek(4, librevenge::RVNG_SEEK_CUR); // nan type
      double value=std::numeric_limits<double>::quiet_NaN();
      content.setValue(value);
      f << value << ",";
      break;
    }
    default:
      ok=false;
      MWAW_DEBUG_MSG(("GreatWksSSParser::readCell: find unexpected type value\n"));
      f << "#type[val]=" << typeVal << ",";
    }
  }
  if (ok && input->tell()+2<=endPos) {
    val =(int) input->readULong(2);
    if (!input->checkPosition(input->tell()+4*val)) {
      MWAW_DEBUG_MSG(("GreatWksSSParser::readCell: can not read the number of childs\n"));
      f << "###";
    }
    else if (val) {
      f << "childs=[";
      for (int i=0; i<val; ++i)
        f << input->readULong(2) << "x" << input->readULong(2) << ",";
      f << "],";
    }
  }
  cell.setFormat(format);
  if (input->tell()!=endPos) {
    ascii().addDelimiter(input->tell(),'|');
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
  }
  ascii().addPos(pos-2);
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read the fonts
////////////////////////////////////////////////////////////
bool GreatWksSSParser::readStyles()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  int const vers=version();
  libmwaw::DebugStream f;

  f << "Entries(Style):";
  long sz=(long) input->readULong(4);
  long endPos=pos+4+sz;
  int expectedSize=vers==1 ? 18 : 40;
  if (!input->checkPosition(endPos) || (sz%expectedSize)) {
    MWAW_DEBUG_MSG(("GreatWksSSParser::readStyles: can not find the font defs zone\n"));
    f << "###";
    ascii().addPos(pos-2);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pos-2);
  ascii().addNote(f.str().c_str());
  int numFonts=int(sz/expectedSize);
  for (int i=0; i <numFonts; ++i) {
    pos=input->tell();
    f.str("");
    f << "Style-" << i << ":";
    GreatWksSSParserInternal::Style style;
    MWAWFont &font=style.m_font;
    int val=(int) input->readLong(2); // always 0 ?
    if (val) f << "#unkn=" << val << ",";
    val=(int) input->readLong(2);
    if (val!=1) f << "used?=" << val << ",";

    font.setId(m_document->getTextParser()->getFontId((int) input->readULong(2)));
    int flag =(int) input->readULong(2);
    uint32_t flags=0;
    if (flag&0x1) flags |= MWAWFont::boldBit;
    if (flag&0x2) flags |= MWAWFont::italicBit;
    if (flag&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (flag&0x8) flags |= MWAWFont::embossBit;
    if (flag&0x10) flags |= MWAWFont::shadowBit;
    if (flag&0x20) font.setDeltaLetterSpacing(-1);
    if (flag&0x40) font.setDeltaLetterSpacing(1);
    if (flag&0x100) font.set(MWAWFont::Script::super100());
    if (flag&0x200) font.set(MWAWFont::Script::sub100());
    if (flag&0x800) font.setStrikeOutStyle(MWAWFont::Line::Simple);
    if (flag&0x2000) {
      font.setUnderlineStyle(MWAWFont::Line::Simple);
      font.setUnderlineType(MWAWFont::Line::Double);
    }
    flag &=0xD480;
    if (flag) f << "#fl=" << std::hex << flag << std::dec << ",";
    font.setFlags(flags);
    font.setSize((float) input->readULong(2));
    unsigned char color[3];
    for (int c=0; c<3; ++c)
      color[c] = (unsigned char)(input->readULong(2)>>8);
    font.setColor(MWAWColor(color[0],color[1],color[2]));
    f << font.getDebugString(getParserState()->m_fontConverter) << ",";
    f << "h[line]?=" << input->readULong(2) << ",";
    if (vers==1) {
      m_state->m_styleList.push_back(style);
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      continue;
    }
    MWAWColor bfColors[2];
    for (int j=0; j<2; ++j) {  // front/back color?
      for (int c=0; c<3; ++c)
        color[c] = (unsigned char)(input->readULong(2)>>8);
      MWAWColor col(color[0],color[1],color[2]);
      bfColors[j]=col;
      if ((j==0 && col.isBlack()) || (j==1 && col.isWhite())) continue;
      if (j==0) f << "col[front]=" << MWAWColor(color[0],color[1],color[2]) << ",";
      else f << "col[back]=" << MWAWColor(color[0],color[1],color[2]) << ",";
    }
    int patId=(int) input->readLong(2);
    if (!patId) {
      style.m_backgroundColor=bfColors[1];
      input->seek(8, librevenge::RVNG_SEEK_CUR);
    }
    else {
      f << "pattern[id]=" << patId << ",";
      MWAWGraphicStyle::Pattern pattern;
      pattern.m_dim=Vec2i(8,8);
      pattern.m_data.resize(8);
      for (size_t j=0; j < 8; ++j)
        pattern.m_data[j]=(unsigned char) input->readULong(1);
      pattern.m_colors[0]=bfColors[1];
      pattern.m_colors[1]=bfColors[0];
      pattern.getAverageColor(style.m_backgroundColor);
      f << "pat=[" << pattern << "],";
    }
    m_state->m_styleList.push_back(style);
    ascii().addDelimiter(input->tell(), '|');
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+expectedSize, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool GreatWksSSParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = GreatWksSSParserInternal::State();
  if (!m_document->checkHeader(header, strict)) return false;
  return getParserState()->m_kind==MWAWDocument::MWAW_K_SPREADSHEET;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
