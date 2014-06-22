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

#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWChart.hxx"
#include "MWAWCell.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWList.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSpreadsheetListener.hxx"
#include "MWAWSubDocument.hxx"

#include "BeagleWksStructManager.hxx"

#include "BeagleWksSSParser.hxx"

/** Internal: the structures of a BeagleWksSSParser */
namespace BeagleWksSSParserInternal
{
//! Internal: the cell of a BeagleWksSSParser
struct Cell : public MWAWCell {
  //! constructor
  Cell(Vec2i pos=Vec2i(0,0)) : MWAWCell(), m_content(), m_formula(-1), m_isEmpty(false)
  {
    setPosition(pos);
  }
  //! the cell content
  MWAWCellContent m_content;
  //! the formula id
  int m_formula;
  //! flag to know if the cell is empty
  bool m_isEmpty;
};

//! Internal: the spreadsheet of a BeagleWksSSParser
struct Spreadsheet {
  //! constructor
  Spreadsheet() : m_numRows(0), m_widthCols(), m_heightRows(), m_cells(), m_lastReadRow(-1)
  {
  }
  //! try to associate a formula to a cell
  bool addFormula(Vec2i const &cellPos, std::vector<MWAWCellContent::FormulaInstruction> const &formula);
  //! convert the m_widthCols, m_heightRows in a vector of of point size
  static std::vector<float> convertInPoint(std::vector<int> const &list,
      float defSize)
  {
    size_t numElt = list.size();
    std::vector<float> res;
    res.resize(numElt);
    for (size_t i = 0; i < numElt; i++) {
      if (list[i] < 0) res[i] = defSize;
      else res[i] = float(list[i]);
    }
    return res;
  }
  //! update the number of columns and the width
  void updateWidthCols()
  {
    int maxCol=-1;
    for (size_t i = 0; i < m_cells.size(); ++i) {
      if (m_cells[i].position()[0]>maxCol)
        maxCol = m_cells[i].position()[0];
    }
    m_widthCols.resize(size_t(maxCol+1),-1);
  }
  //! the number of rows
  int m_numRows;
  //! the column size in points
  std::vector<int> m_widthCols;
  //! the row size in points
  std::vector<int> m_heightRows;
  //! the list of not empty cells
  std::vector<Cell> m_cells;
  //! the last read rows
  int m_lastReadRow;
};

bool Spreadsheet::addFormula(Vec2i const &cellPos, std::vector<MWAWCellContent::FormulaInstruction> const &formula)
{
  for (size_t c=0; c < m_cells.size(); ++c) {
    if (m_cells[c].position()!=cellPos)
      continue;
    m_cells[c].m_content.m_formula=formula;
    return true;
  }
  MWAW_DEBUG_MSG(("Spreadsheet::addFormula: can not find cell with position %dx%d\n", cellPos[0], cellPos[1]));
  return false;
}

//! Internal: the chart of a BeagleWksSSParser
struct Chart : public MWAWChart {
  //! constructor
  Chart(std::string const &name, BeagleWksSSParser &parser) : MWAWChart(name, parser.getParserState()->m_fontConverter),
    m_parser(&parser), m_input(parser.getInput())
  {
  }
  //! send a zone content
  void sendContent(TextZone const &zone, MWAWListenerPtr &listener);
  //! the main parser
  BeagleWksSSParser *m_parser;
  //! the input
  MWAWInputStreamPtr m_input;
private:
  Chart(Chart const &orig);
  Chart operator=(Chart const &orig);
};

void Chart::sendContent(Chart::TextZone const &zone, MWAWListenerPtr &listener)
{
  if (!listener.get() || !m_parser) {
    MWAW_DEBUG_MSG(("BeagleWksSSParserInternal::Chart::sendContent: no listener\n"));
    return;
  }
  long pos = m_input->tell();
  listener->setFont(zone.m_font);
  m_parser->sendText(zone.m_textEntry, false);
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
}

////////////////////////////////////////
//! Internal: the state of a BeagleWksSSParser
struct State {
  //! constructor
  State() :  m_spreadsheetBegin(-1), m_spreadsheet(), m_spreadsheetName("Sheet0"), m_chartList(), m_typeEntryMap(),
    m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0)
  {
  }
  //! returns a color corresponding to an id
  static bool getColor(int id, MWAWColor &color)
  {
    switch (id) {
    case 0:
      color=MWAWColor::black();
      return true;
    case 1:
      color=MWAWColor::white();
      return true;
    case 2:
      color=MWAWColor(0xFF,0,0);
      return true;
    case 3:
      color=MWAWColor(0,0xFF,0);
      return true;
    case 4:
      color=MWAWColor(0,0,0xFF);
      return true;
    case 5:
      color=MWAWColor(0,0xFF,0xFF);
      return true;
    case 6:
      color=MWAWColor(0xFF,0,0xFF);
      return true;
    case 7:
      color=MWAWColor(0xFF,0xFF,0);
      return true;
    default:
      MWAW_DEBUG_MSG(("BeagleWksSSParserInternal::State::getColor: unknown color %d\n", id));
      return false;
    }
  }
  /** the spreadsheet begin position */
  long m_spreadsheetBegin;
  /** the spreadsheet */
  Spreadsheet m_spreadsheet;
  /** the spreadsheet name */
  std::string m_spreadsheetName;
  /** the list of chart */
  std::vector<shared_ptr<Chart> > m_chartList;
  /** the type entry map */
  std::multimap<std::string, MWAWEntry> m_typeEntryMap;
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a BeagleWksSSParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(BeagleWksSSParser &pars, MWAWInputStreamPtr input, MWAWEntry const &entry) :
    MWAWSubDocument(&pars, input, entry)
  {
  }

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const
  {
    return MWAWSubDocument::operator!=(doc);
  }
  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const
  {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
private:
  SubDocument(SubDocument const &orig);
  SubDocument &operator=(SubDocument const &orig);
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("BeagleWksSSParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  BeagleWksSSParser *parser=dynamic_cast<BeagleWksSSParser *>(m_parser);
  if (!parser) {
    MWAW_DEBUG_MSG(("BeagleWksSSParserInternal::SubDocument::parse: can not find the parser\n"));
    return;
  }
  long pos = m_input->tell();
  listener->setFont(MWAWFont(3,12)); // fixme
  parser->sendText(m_zone, true);
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
}

}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
BeagleWksSSParser::BeagleWksSSParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWSpreadsheetParser(input, rsrcParser, header), m_state(), m_structureManager()
{
  init();
}

BeagleWksSSParser::~BeagleWksSSParser()
{
}

void BeagleWksSSParser::init()
{
  resetSpreadsheetListener();
  setAsciiName("main-1");

  m_state.reset(new BeagleWksSSParserInternal::State);
  m_structureManager.reset(new BeagleWksStructManager(getParserState()));

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);
}

MWAWInputStreamPtr BeagleWksSSParser::rsrcInput()
{
  return getRSRCParser()->getInput();
}

libmwaw::DebugFile &BeagleWksSSParser::rsrcAscii()
{
  return getRSRCParser()->ascii();
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
Vec2f BeagleWksSSParser::getPageLeftTop() const
{
  return Vec2f(float(getPageSpan().getMarginLeft()),
               float(getPageSpan().getMarginTop()+m_state->m_headerHeight/72.0));
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void BeagleWksSSParser::newPage(int number)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!getSpreadsheetListener() || m_state->m_actPage == 1)
      continue;
    getSpreadsheetListener()->insertBreak(MWAWSpreadsheetListener::PageBreak);
  }
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void BeagleWksSSParser::parse(librevenge::RVNGSpreadsheetInterface *docInterface)
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
    MWAW_DEBUG_MSG(("BeagleWksSSParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetSpreadsheetListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void BeagleWksSSParser::createDocument(librevenge::RVNGSpreadsheetInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getSpreadsheetListener()) {
    MWAW_DEBUG_MSG(("BeagleWksSSParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  int numPages = 1;
  m_state->m_numPages = numPages;

  MWAWEntry header, footer;
  m_structureManager->getHeaderFooterEntries(header,footer);
  std::vector<MWAWPageSpan> pageList;
  MWAWPageSpan ps(getPageSpan());
  if (header.valid()) {
    shared_ptr<BeagleWksSSParserInternal::SubDocument> subDoc
    (new BeagleWksSSParserInternal::SubDocument(*this, getInput(), header));
    MWAWHeaderFooter hf(MWAWHeaderFooter::HEADER, MWAWHeaderFooter::ALL);
    hf.m_subDocument=subDoc;
    ps.setHeaderFooter(hf);
  }
  if (footer.valid()) {
    shared_ptr<BeagleWksSSParserInternal::SubDocument> subDoc
    (new BeagleWksSSParserInternal::SubDocument(*this, getInput(), footer));
    MWAWHeaderFooter hf(MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
    hf.m_subDocument=subDoc;
    ps.setHeaderFooter(hf);
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
bool BeagleWksSSParser::createZones()
{
  readRSRCZones();
  MWAWInputStreamPtr input = getInput();
  if (input->seek(66, librevenge::RVNG_SEEK_SET) || !readPrintInfo())
    return false;
  long pos = input->tell();
  if (!input->checkPosition(pos+70)) {
    MWAW_DEBUG_MSG(("BeagleWksSSParser::createZones: the file can not contains Zones\n"));
    return false;
  }

  // now read the list of zones
  libmwaw::DebugStream f;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "Entries(Zones):";
  for (int i=0; i<7; ++i) { // checkme: at least 2 zones, maybe 7
    MWAWEntry entry;
    entry.setBegin(input->readLong(4));
    entry.setLength(input->readLong(4));
    entry.setId((int) input->readLong(2));
    if (entry.length()==0) continue;
    entry.setType(i==1?"Frame":"Unknown");
    f << entry.type() << "[" << entry.id() << "]="
      << std::hex << entry.begin() << "<->" << entry.end() << ",";
    if (!entry.valid() || !input->checkPosition(entry.end())) {
      f << "###";
      if (i<2) {
        MWAW_DEBUG_MSG(("BeagleWksSSParser::createZones: can not read the header zone, stop\n"));
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());
        return false;
      }
      MWAW_DEBUG_MSG(("BeagleWksSSParser::createZones: can not zones entry %d\n",i));
      continue;
    }
    m_state->m_typeEntryMap.insert
    (std::multimap<std::string, MWAWEntry>::value_type(entry.type(),entry));
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  // now parse the different zones
  std::multimap<std::string, MWAWEntry>::iterator it;
  it=m_state->m_typeEntryMap.find("FontNames");
  if (it!=m_state->m_typeEntryMap.end())
    m_structureManager->readFontNames(it->second);
  it=m_state->m_typeEntryMap.find("Frame");
  if (it!=m_state->m_typeEntryMap.end())
    m_structureManager->readFrame(it->second);

  // now parse the different zones
  for (it=m_state->m_typeEntryMap.begin(); it!=m_state->m_typeEntryMap.end(); ++it) {
    MWAWEntry const &entry=it->second;
    if (entry.isParsed())
      continue;
    f.str("");
    f << "Entries(" << entry.type() << ")[" << entry.id() << "]:";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
    ascii().addPos(entry.end());
    ascii().addNote("_");
  }

  input->seek(m_state->m_spreadsheetBegin, librevenge::RVNG_SEEK_SET);
  pos = input->tell();
  if (!m_structureManager->readDocumentPreferences())
    input->seek(pos, librevenge::RVNG_SEEK_SET);
  pos = input->tell();
  if (!m_structureManager->readDocumentInfo())
    input->seek(pos, librevenge::RVNG_SEEK_SET);
  pos = input->tell();
  if (!readChartZone())
    input->seek(pos, librevenge::RVNG_SEEK_SET);
  if (!readSpreadsheet())
    return m_state->m_spreadsheet.m_cells.size()!=0;
  /* normally ends with a zone of size 25
     with looks like 01010000010000000000000000007cffff007d0100007c0000
                or   01010001010000000000000000000000000001000100000000
     some flags + selection?
   */
  ascii().addPos(input->tell());
  ascii().addNote("Entries(ZoneEnd)");
  return true;
}

bool BeagleWksSSParser::readRSRCZones()
{
  MWAWRSRCParserPtr rsrcParser = getRSRCParser();
  if (!rsrcParser)
    return true;

  std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::iterator it;
  // the 1 zone
  char const *(zNames[]) = {"wPos", "DMPF" };
  for (int z = 0; z < 2; ++z) {
    it = entryMap.lower_bound(zNames[z]);
    while (it != entryMap.end()) {
      if (it->first != zNames[z])
        break;
      MWAWEntry const &entry = it++->second;
      switch (z) {
      case 0: // 1001
        m_structureManager->readwPos(entry);
        break;
      case 1: // find in one file with id=4661 6a1f 4057
        m_structureManager->readFontStyle(entry);
        break;
      /* find also
         - edpt: see sendPicture
         - DMPP: the paragraph style
         - sect and alis: position?, alis=filesystem alias(dir, filename, path...)
      */
      default:
        break;
      }
    }
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool BeagleWksSSParser::readPrintInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  if (!input->checkPosition(pos+0x70))
    return false;

  libmwaw::DebugStream f;
  // print info
  libmwaw::PrinterInfo info;
  if (!info.read(input)) return false;
  f << "Entries(PrintInfo):"<< info;

  Vec2i paperSize = info.paper().size();
  Vec2i pageSize = info.page().size();
  if (pageSize.x() <= 0 || pageSize.y() <= 0 ||
      paperSize.x() <= 0 || paperSize.y() <= 0) return false;

  // define margin from print info
  Vec2i lTopMargin= -1 * info.paper().pos(0);
  Vec2i rBotMargin=info.paper().pos(1) - info.page().pos(1);

  // move margin left | top
  int decalX = lTopMargin.x() > 14 ? lTopMargin.x()-14 : 0;
  int decalY = lTopMargin.y() > 14 ? lTopMargin.y()-14 : 0;
  lTopMargin -= Vec2i(decalX, decalY);
  rBotMargin += Vec2i(decalX, decalY);

  // decrease right | bottom
  int rightMarg = rBotMargin.x() -10;
  if (rightMarg < 0) rightMarg=0;
  int botMarg = rBotMargin.y() -50;
  if (botMarg < 0) botMarg=0;

  getPageSpan().setMarginTop(lTopMargin.y()/72.0);
  getPageSpan().setMarginBottom(botMarg/72.0);
  getPageSpan().setMarginLeft(lTopMargin.x()/72.0);
  getPageSpan().setMarginRight(rightMarg/72.0);
  getPageSpan().setFormLength(paperSize.y()/72.);
  getPageSpan().setFormWidth(paperSize.x()/72.);

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(pos+0x78, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != pos+0x78) {
    MWAW_DEBUG_MSG(("BeagleWksSSParser::readPrintInfo: file is too short\n"));
    return false;
  }
  ascii().addPos(input->tell());

  return true;
}

////////////////////////////////////////////////////////////
// chart
////////////////////////////////////////////////////////////
bool BeagleWksSSParser::readChartZone()
{
  MWAWInputStreamPtr &input= getInput();
  long pos=input->tell();
  if (!input->checkPosition(pos+10)) {
    MWAW_DEBUG_MSG(("BeagleWksSSParser::readChartZone: can not find the chart zone\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(Chart):";
  pos = input->tell();
  for (int i=0; i < 4; ++i) {
    static int const expectedValues[5]= {1, 0xF, 0x4000, 0x100 };
    int val= (int) input->readULong(2);
    if (val != expectedValues[i])
      f << "f" << i+1 << "=" << std::hex << val << std::dec << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  while (!input->isEnd()) {
    pos=input->tell();
    long sz=(long) input->readULong(2);
    if (sz==0) {
      ascii().addPos(pos);
      ascii().addNote("Chart:_");
      return true;
    }
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    if (!readChart())
      return false;
  }
  return true;
}

bool BeagleWksSSParser::readChart()
{
  // find only 2 charts, so this code is not sure...
  MWAWInputStreamPtr &input= getInput();
  long pos=input->tell();
  if (!input->checkPosition(pos+9+51+0x5d)) {
    MWAW_DEBUG_MSG(("BeagleWksSSParser::readChart: the zone seems to short\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Chart-header:";
  int val=(int) input->readULong(2);
  f << "f0=" << std::hex << val << std::dec << ",";
  val=(int) input->readLong(2);
  if (val!=0x12) f << "f1=" << val << ",";
  bool findRange=false;
  shared_ptr<BeagleWksSSParserInternal::Chart> chart(new BeagleWksSSParserInternal::Chart(m_state->m_spreadsheetName, *this));
  Box2i serieRange;
  for (int i=0; i<2; ++i) {
    long actPos=input->tell();
    int dSz=(int) input->readULong(2);
    int sSz=(int) input->readULong(1);
    if (sSz+1!=dSz && sSz+2!=dSz) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      MWAW_DEBUG_MSG(("BeagleWksSSParser::readChart: can not read the chart\n"));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return false;
    }
    std::string name("");
    for (int c=0; c<sSz; ++c)
      name += (char) input->readULong(1);
    if (i==1 && sSz>=5 && (int) name.length()==sSz) {
      size_t cPos=0;
      for (int c=0; c<2; c++) {
        if (cPos+1>=(size_t) sSz) break;
        char ch=name[cPos++];
        if (ch<'A' || ch>'Z') break;
        int col=int(ch-'A')+1;
        ch=name[cPos++];
        if (ch>='A' && ch<='Z') {
          col=26*col+int(ch-'A')+1;
          if (cPos+1>=(size_t) sSz) break;
          ch=name[cPos++];
        }
        if (ch<'1' || ch>'9') break;
        int row=ch-'0';
        while (cPos<(size_t)sSz && name[cPos]>='0' && name[cPos]<='9') {
          row=row*10+int(name[cPos++]-'0');
        }
        if (c==0)
          serieRange.setMin(Vec2i(col-1,row-1));
        else {
          serieRange.setMax(Vec2i(col-1,row-1));
          findRange=true;
          break;
        }
        if (cPos>=(size_t)sSz || name[cPos++]!=':')
          break;
      }
    }
    if (i==1 && !findRange) {
      MWAW_DEBUG_MSG(("BeagleWksSSParser::readChart: can not decode the range\n"));
      f << "###";
    }
    f << "\"" << name << "\",";
    input->seek(actPos+2+dSz, librevenge::RVNG_SEEK_SET);
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "Chart-B:";
  val=(int) input->readLong(1);
  MWAWChart::Axis axis[2];
  switch (val) {
  case 1: // show
    axis[0].m_showLabel=axis[1].m_showLabel=true;
    break;
  case 2:
    axis[0].m_showLabel=axis[1].m_showLabel=false;
    f << "label[no],";
    break;
  default:
    f << "#show[label]=" << val << ",";
    break;
  }
  val=(int) input->readLong(1);
  if (val) f << "f0=" << val << ",";
  val=(int) input->readLong(1);
  f << "f1=" << val << ",";
  for (int i=0; i<7; ++i) {
    int dim[2];
    for (int j=0; j<2; ++j) dim[j]=(int) input->readLong(2);
    if (dim[0]||dim[1])
      f << "dim" << i << "?=" << dim[0] << "x" << dim[1] << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "Chart-C:";
  for (int i=0; i<3; ++i) {
    int const expectedVal[3]= {0xe, 0, 0x21};
    val=(int) input->readLong(2);
    if (val!=expectedVal[i]) f << "f" << i << "=" << val << ",";
  }
  val=(int) input->readLong(2);
  f << "f3=" << val << ",";
  val=(int) input->readLong(1);

  axis[1].m_type=MWAWChart::Axis::A_Numeric;
  switch (val) {
  case 1: // vertical scale numeric
    break;
  case 2:
    axis[1].m_type=MWAWChart::Axis::A_Logarithmic;
    f << "yScale=log,";
    break;
  default:
    f << "#yScale="<<val << ",";
    break;
  }
  val=(int) input->readLong(1);

  axis[0].m_type=MWAWChart::Axis::A_Sequence;
  switch (val) {
  case 1: // label value no skip
    break;
  case 2:
    axis[1].m_type=MWAWChart::Axis::A_Sequence;
    f << "labelX[skipEmpty],";
    break;
  case 3:
    f << "labelX[stagger],";
    break;
  default:
    f << "#labelX="<<val << ",";
    break;
  }
  axis[0].m_showGrid=axis[1].m_showGrid=false;
  for (int i=0; i<4; ++i) {
    val=(int) input->readLong(1);
    if (!val) continue;
    char const *(wh[4])= {"draw[grid]", "draw[value]", "auto[scale]", "flip[RowCol]"};
    f << wh[i];
    if (val!=1) {
      if (i==0)
        axis[0].m_showGrid=axis[1].m_showGrid=true;
      f << "=" << val << ",";
    }
    else f << ",";
  }
  for (int i=0; i<3; ++i) {
    char const *(wh[3])= {"minScale", "maxScale", "step[value]"};
    double value=0;
    bool isNAN=false;
    if (!input->readDouble8(value, isNAN))
      f << "###";
    f << wh[i] << "=" << value << ",";
  }
  val=(int) input->readULong(1); // always 0?
  if (val) f << "g0=" << val << ",";
  val=(int) input->readULong(1);
  MWAWChart::Series::Type serieType=MWAWChart::Series::S_Bar;
  switch (val) {
  case 0x19:
    serieType=MWAWChart::Series::S_Line;
    f << "line,";
    break;
  case 0x1a:
    f << "bar,";
    break;
  case 0x1b:
    serieType=MWAWChart::Series::S_Column;
    f << "column,";
    break;
  case 0x1c:
    serieType=MWAWChart::Series::S_Area;
    f << "area,";
    break;
  case 0x1d:
    // fixme (add stack)
    f << "stack,";
    break;
  case 0x1e:
    serieType=MWAWChart::Series::S_Scatter;
    f << "scatter,";
    break;
  case 0x1f:
    serieType=MWAWChart::Series::S_Pie;
    f << "pie,";
    break;
  case 0x22:
    serieType=MWAWChart::Series::S_Stock;
    f << "hi-lo,";
    break;
  default:
    f << "#type=" << std::hex << val << std::dec << ",";
    break;
  }
  ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(pos+46, librevenge::RVNG_SEEK_SET);

  pos=input->tell();
  long endPos = pos+67;
  f.str("");
  f << "Chart-D:";
  int sSz=(int) input->readULong(1);
  if (pos+1+sSz <= endPos) {
    MWAWChart::TextZone title;
    title.m_type=MWAWChart::TextZone::T_Title;
    title.m_contentType=MWAWChart::TextZone::C_Text;
    title.m_textEntry.setBegin(pos+1);
    title.m_textEntry.setLength(sSz);
    chart->add(title);
    std::string name("");
    for (int c=0; c<sSz; ++c)
      name += (char) input->readULong(1);
    f << "\"" << name << "\",";
  }
  else
    f << "###";
  if (findRange) {
    chart->add(0, axis[0]);
    chart->add(1, axis[1]);
    chart->setDataType(serieType, true);
    for (int r=serieRange[0][1]; r<serieRange[1][1]; ++r) {
      MWAWChart::Series series;
      series.m_range=Box2i(Vec2i(serieRange[0][0], r),
                           Vec2i(serieRange[1][0], r));
      series.m_type=serieType;
      unsigned char gray=(unsigned char)((r%4)*30);
      series.m_style.m_lineWidth=1;
      series.m_style.m_lineColor=MWAWColor(gray,gray,gray);
      series.m_style.setSurfaceColor(MWAWColor(gray,gray,gray));
      chart->add(series);
    }
    MWAWChart::Legend legend;
    legend.m_show=true;
    chart->set(legend);
    m_state->m_chartList.push_back(chart);
  }
  input->seek(pos+31, librevenge::RVNG_SEEK_SET);
  ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// spreadsheet
////////////////////////////////////////////////////////////
bool BeagleWksSSParser::readSpreadsheet()
{
  MWAWInputStreamPtr &input= getInput();
  long pos=input->tell();
  if (!input->checkPosition(pos+9)) {
    MWAW_DEBUG_MSG(("BeagleWksSSParser::readSpreadsheet: can not find the spreadsheet zone\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(Spreadsheet):";
  int val= (int) input->readLong(2);
  if (val!=7)
    f << "f1=" << val << ",";
  BeagleWksSSParserInternal::Spreadsheet &sheet=m_state->m_spreadsheet;
  sheet.m_numRows=(int) input->readLong(2)+1;
  f << "num[row]=" << sheet.m_numRows << ",";
  val= (int) input->readLong(2);
  if (val!=-1)
    f << "f2=" << val << ",";
  for (int i=0; i < 3; ++i) { // g0=0|73|89, other 0
    val=(int) input->readULong(1);
    if (val)
      f << "g" << i << "=" << std::hex << val << std::dec << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  while (readRowSheet(sheet))
    if (input->isEnd()) break;
  return readZone0() && readColumnWidths(sheet) && readZone0() && readFormula(sheet);
}

bool BeagleWksSSParser::readRowSheet(BeagleWksSSParserInternal::Spreadsheet &sheet)
{
  MWAWInputStreamPtr &input= getInput();
  long pos=input->tell();
  libmwaw::DebugStream f;
  f << "Entries(Row):";
  int row=(int)input->readLong(2);
  long fSz=(long) input->readULong(4);
  long endPos=pos+6+fSz;
  if (fSz<18 || row <= sheet.m_lastReadRow || row >= sheet.m_numRows || !input->checkPosition(endPos)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  sheet.m_lastReadRow=row;
  f << "row=" << row << ",";
  int val=(int) input->readLong(2);
  if (val && val!=int(fSz))
    f << "#sz=" << val << ",";
  int h=(int) input->readLong(2);
  if (h!=-1)
    f << "height=" << h << ",";
  sheet.m_heightRows.resize(size_t(row)+1, -1);
  sheet.m_heightRows[size_t(row)]=h;
  input->seek(10, librevenge::RVNG_SEEK_CUR); // junk
  int N=(int) input->readLong(2)+1;
  f << "N=" << N << ",";
  val=(int) input->readLong(2);
  if (val!=fSz && val) {
    if (val+1 > N) N=val+1;
    f << "unkn=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(pos+6+18, librevenge::RVNG_SEEK_SET);
  input->pushLimit(endPos);
  for (int i=0; i < N; ++i) {
    pos=input->tell();
    if (pos==endPos) break;
    BeagleWksSSParserInternal::Cell cell(Vec2i(i, row));
    if (!readCellSheet(cell))
      return false;
    sheet.m_cells.push_back(cell);
  }
  input->popLimit();
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

bool BeagleWksSSParser::readCellSheet(BeagleWksSSParserInternal::Cell &cell)
{
  MWAWInputStreamPtr &input= getInput();
  long pos=input->tell();
  int cSize=(int) input->readULong(1);
  long endPos=pos+2+cSize+((cSize%2)?1:0);
  libmwaw::DebugStream f;
  f << "Entries(Cell)[" << cell.position() << "]:";
  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("BeagleWksSSParser::readCellSheet: can not find some cell\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  MWAWCell::Format format;

  int val=(int) input->readULong(1);
  bool isFormula=val&0x80;
  if (isFormula) f << "formula,";
  int type=(val>>4)&7;
  if (val&0xf)
    f << "type[low]" << std::hex << (val&0xf) << std::dec << ",";
  if (cSize>=8) {
    ascii().addDelimiter(pos+2,'|');
    if (cSize!=8)
      ascii().addDelimiter(pos+10,'|');
    // first read the flags which indicated what is filled
    input->seek(pos+9, librevenge::RVNG_SEEK_SET);
    int wh=(int) input->readULong(1);
    if (wh) {
      f << "style=[";
      input->seek(pos+2, librevenge::RVNG_SEEK_SET);
      MWAWFont font(3);
      val=(int) input->readLong(2);
      if (val>0)
        font.setSize((float) val);
      val=(int) input->readLong(2);
      if (val>=0)
        font.setId(val);
      int flag = (int) input->readULong(1);
      uint32_t flags=0;
      MWAWColor col;
      if (flag&7 && m_state->getColor(flag&7, col))
        font.setColor(col);
      if (flag&0x8) flags |= MWAWFont::boldBit;
      if (flag&0x10) flags |= MWAWFont::italicBit;
      if (flag&0x20) font.setUnderlineStyle(MWAWFont::Line::Simple);
      if (flag&0x40) flags |= MWAWFont::embossBit;
      if (flag&0x80) flags |= MWAWFont::shadowBit;
      font.setFlags(flags);
      cell.setFont(font);
      f << font.getDebugString(getParserState()->m_fontConverter);
      int form=(int) input->readULong(1);
      if (form) {
        if (form & 0x10)
          format.m_thousandHasSeparator=true;
        switch (form>>5) {
        case 0: // generic
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
          format.m_format=MWAWCell::F_DATE;
          format.m_DTFormat="%m/%d/%y:";
          break;
        case 6:
          switch (form & 0x7) {
          case 0:
            format.m_format=MWAWCell::F_DATE;
            format.m_DTFormat="%b %d, %Y";
            break;
          case 1:
            format.m_format=MWAWCell::F_DATE;
            format.m_DTFormat="%B %d, %Y";
            break;
          case 2:
            format.m_format=MWAWCell::F_DATE;
            format.m_DTFormat="%a, %b %d, %Y";
            break;
          case 3:
            format.m_format=MWAWCell::F_DATE;
            format.m_DTFormat="%A, %B %d, %Y";
            break;
          case 4:
            format.m_format=MWAWCell::F_TIME;
            format.m_DTFormat="%I:%M %p";
            break;
          case 5:
            format.m_format=MWAWCell::F_TIME;
            format.m_DTFormat="%I:%M:%S %p";
            break;
          case 6:
            format.m_format=MWAWCell::F_TIME;
            format.m_DTFormat="%H:%M";
            break;
          case 7:
            format.m_format=MWAWCell::F_TIME;
            format.m_DTFormat="%H:%M:%S";
            break;
          default:
            break;
          }
          form &= 0x8;
          break;
        default:
          f << "#form=7:";
          break;
        }
        if (form & 0xf)
          format.m_digits=(form & 0xf);
        f << format;
      }
      val=(int) input->readULong(1);
      if (val>>4) {
        int cId=(val>>4);
        if (cId<5) { // first five gray colors, white, lightgray, ... black
          unsigned char gray=(unsigned char)((4-cId)*0x40);
          cell.setBackgroundColor(MWAWColor(gray,gray,gray));
        }
        else if (m_state->getColor(cId-3,col))
          cell.setBackgroundColor(col);
        f << "color[back]=" << cell.backgroundColor() << ",";
      }
      if (val&0xf) {
        int borders=0;
        f << "bord=[";
        if (val&1) {
          borders|=libmwaw::LeftBit;
          f << "left,";
        }
        if (val&2) {
          borders|=libmwaw::RightBit;
          f << "right,";
        }
        if (val&4) {
          borders|=libmwaw::TopBit;
          f << "top,";
        }
        if (val&8) {
          borders|=libmwaw::BottomBit;
          f << "bottom,";
        }
        f << "],";
        cell.setBorders(borders, MWAWBorder());
      }
      switch ((wh>>5)&3) {
      case 0:
        cell.setHAlignment(MWAWCell::HALIGN_LEFT);
        f << "left,";
        break;
      case 1:
        cell.setHAlignment(MWAWCell::HALIGN_RIGHT);
        f << "right,";
        break;
      case 2:
        cell.setHAlignment(MWAWCell::HALIGN_CENTER);
        f << "center,";
        break;
      case 3: // default
      default:
        break;
      }
      if (wh&0x9F)
        f << "wh=" << std::hex << (wh&0x9F) << std::dec;
      f << "],";
    }
    input->seek(pos+10, librevenge::RVNG_SEEK_SET);
  }
  MWAWCellContent &content=cell.m_content;
  switch (type) {
  case 0:
    f << "_";
    if (cSize) {
      MWAW_DEBUG_MSG(("BeagleWksSSParser::readCellSheet: find some data for empty cell\n"));
      f << "###";
      break;
    }
    cell.m_isEmpty=true;
    break;
  case 1:
  case 3:
  case 4: {
    if (type==1)
      f << "number,";
    else if (type==3) {
      format.m_format=MWAWCell::F_BOOLEAN;
      f << "bool,";
    }
    else {
      content.setValue(std::numeric_limits<double>::quiet_NaN());
      f << "error,"; // then followed by the nan type?
    }
    if (cSize<18+(isFormula?2:0)) {
      MWAW_DEBUG_MSG(("BeagleWksSSParser::readCellSheet: the number size seems odd\n"));
      f << "###";
      break;
    }
    if (isFormula) {
      content.m_contentType=MWAWCellContent::C_FORMULA;
      if (format.m_format==MWAWCell::F_UNKNOWN)
        format.m_format=MWAWCell::F_NUMBER;
      cell.m_formula=(int) input->readULong(2);
      f << "id[formula]=" << cell.m_formula << ",";
    }
    else
      content.m_contentType=MWAWCellContent::C_NUMBER;
    double value;
    bool isNan;
    if (!input->readDouble10(value, isNan)) {
      MWAW_DEBUG_MSG(("BeagleWksSSParser::readCellSheet: can not read a number\n"));
      f << "###";
      break;
    }
    else if (type!=4) {
      // change the reference date from 1/1/1904 to 1/1/1900
      if (format.m_format==MWAWCell::F_DATE)
        content.setValue(value+1460.);
      else if (format.m_format==MWAWCell::F_TIME)
        content.setValue(std::fmod(value,1));
      else
        content.setValue(value);
    }
    f << value << ",";
    break;
  }
  case 2: {
    f << "text,";
    if (cSize<8+(isFormula?2:0)) {
      MWAW_DEBUG_MSG(("BeagleWksSSParser::readCellSheet: text data size seems odd\n"));
      f << "###";
      break;
    }
    if (isFormula) {
      content.m_contentType=MWAWCellContent::C_FORMULA;
      if (format.m_format==MWAWCell::F_UNKNOWN)
        format.m_format=MWAWCell::F_TEXT;
      cell.m_formula=(int) input->readULong(2);
      f << "id[formula]=" << cell.m_formula << ",";
    }
    else
      content.m_contentType=MWAWCellContent::C_TEXT;
    content.m_textEntry.setBegin(input->tell());
    content.m_textEntry.setLength(cSize-8-(isFormula?2:0));
    std::string text("");
    for (long i=0; i < content.m_textEntry.length(); ++i)
      text+=(char) input->readULong(1);
    f << "\"" << text << "\",";
    break;
  }
  default:
    MWAW_DEBUG_MSG(("BeagleWksSSParser::readCellSheet: find unknown type for cell\n"));
    f << "#type=" << std::hex << type << std::dec << ",";
    break;
  }
  cell.setFormat(format);
  if (input->tell()!=endPos)
    ascii().addDelimiter(input->tell(), '|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// unknown zone
////////////////////////////////////////////////////////////
bool BeagleWksSSParser::readZone0()
{
  MWAWInputStreamPtr &input= getInput();
  long pos=input->tell();
  libmwaw::DebugStream f;
  f << "Entries(Zone0):";
  int N=(int) input->readULong(2);
  if (!input->checkPosition(pos+8+3*N)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  f << "unkn=[";
  for (int i=0; i < N; ++i) {
    f << input->readLong(2) << ":" << std::hex << input->readULong(1) << std::dec << ",";
  }
  f << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

bool BeagleWksSSParser::readColumnWidths(BeagleWksSSParserInternal::Spreadsheet &sheet)
{
  MWAWInputStreamPtr &input= getInput();
  long pos=input->tell();
  libmwaw::DebugStream f;
  f << "Entries(Columns):";
  int N=(int) input->readLong(2);
  f << "N=" << N << ",";
  int val=(int) input->readLong(2);
  if (val) f << "f0=" << val << ",";
  int dSz=(int) input->readULong(2);
  if (N<-1 || (N>=0 && dSz<=0) || !input->checkPosition(pos+6+(N+1)*dSz)) {
    MWAW_DEBUG_MSG(("BeagleWksSSParser::Zone&: header seems odd\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  sheet.m_widthCols.resize(0);
  for (int i=0; i <= N; ++i) {
    pos = input->tell();
    f.str("");
    f << "Columns-" << i << ":";
    if (dSz >= 8) {
      val=(int) input->readLong(2);
      if (val==1) f << "set,";
      else if (val) f << "#set=" << val << ",";
      int w=(int) input->readLong(2);
      sheet.m_widthCols.push_back(w);
      if (w>0)
        f << "w=" << w << ",";
      for (int j=0; j<2; ++j) {
        val=(int) input->readULong(2);
        if (val!=0xFFFF)
          f << "f" << j << "=" << std::hex << val << std::dec << ",";
      }
    }
    if (input->tell()!=pos && input->tell()!=pos+dSz)
      ascii().addDelimiter(input->tell(),'|');
    input->seek(pos+dSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool BeagleWksSSParser::readFormula(BeagleWksSSParserInternal::Spreadsheet &sheet)
{
  MWAWInputStreamPtr &input=getInput();
  libmwaw::DebugStream f;
  int id=0;
  while (!input->isEnd()) {
    long pos=input->tell();
    if (!input->checkPosition(pos+6))
      break;
    f.str("");
    if (id==0)
      f << "Entries(Formula):";
    else
      f << "Formula-" << id << ":";
    ++id;
    int row=(int) input->readULong(2);
    int col=(int) input->readULong(2);
    if (row==0x4000 && col==0x4000) {
      f << "last,";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return true;
    }
    f << "pos=" << col << "x" << row << ",";
    int dataSz=(int) input->readULong(2);
    if (!dataSz || !input->checkPosition(pos+6+dataSz)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    std::vector<MWAWCellContent::FormulaInstruction> formula;
    std::string error("");
    if (m_structureManager->readFormula(pos+6+dataSz, Vec2i(col,row), formula, error))
      sheet.addFormula(Vec2i(col,row), formula);
    else
      f << "###";
    for (size_t l=0; l < formula.size(); ++l)
      f << formula[l];
    if (input->tell()!=pos+6+dataSz)
      ascii().addDelimiter(input->tell(),'|');
    input->seek(pos+6+dataSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// send data
////////////////////////////////////////////////////////////
bool BeagleWksSSParser::sendPageFrames()
{
  std::map<int, BeagleWksStructManager::Frame> const &frameMap = m_structureManager->getIdFrameMap();
  std::map<int, BeagleWksStructManager::Frame>::const_iterator it;
  for (it=frameMap.begin(); it!=frameMap.end(); ++it)
    sendFrame(it->second);
  return true;
}

bool BeagleWksSSParser::sendFrame(BeagleWksStructManager::Frame const &frame)
{
  MWAWPosition fPos(Vec2f(0,0), frame.m_dim, librevenge::RVNG_POINT);

  fPos.setPagePos(frame.m_page > 0 ? frame.m_page : 1, frame.m_origin);
  fPos.setRelativePosition(MWAWPosition::Page);
  fPos.m_wrapping = frame.m_wrap==0 ? MWAWPosition::WNone : MWAWPosition::WDynamic;

  MWAWGraphicStyle style=MWAWGraphicStyle::emptyStyle();
  style.setBorders(frame.m_bordersSet, frame.m_border);
  return sendPicture(frame.m_pictId, fPos, style);
}

// read/send picture (edtp resource)
bool BeagleWksSSParser::sendPicture
(int pId, MWAWPosition const &pictPos, MWAWGraphicStyle const &style)
{
  MWAWSpreadsheetListenerPtr listener=getSpreadsheetListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("BeagleWksSSParser::sendPicture: can not find the listener\n"));
    return false;
  }
  MWAWRSRCParserPtr rsrcParser = getRSRCParser();
  if (!rsrcParser) {
    static bool first=true;
    if (first) {
      MWAW_DEBUG_MSG(("BeagleWksSSParser::sendPicture: need access to resource fork to retrieve picture content\n"));
      first=false;
    }
    return true;
  }

  librevenge::RVNGBinaryData data;
  if (!m_structureManager->readPicture(pId, data))
    return false;

  listener->insertPicture(pictPos, data, "image/pict", style);
  return true;
}

bool BeagleWksSSParser::sendText(MWAWEntry entry, bool /*headerFooter*/)
{
  MWAWSpreadsheetListenerPtr listener=getSpreadsheetListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("BeagleWksSSParser::sendText: can not find the listener\n"));
    return false;
  }
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("BeagleWksSSParser::sendText: can not find the entry\n"));
    return false;
  }

  MWAWInputStreamPtr &input= getInput();
  long endPos=entry.end();
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  while (!input->isEnd()) {
    long pos=input->tell();
    if (pos>=endPos) break;
    unsigned char c = (unsigned char) input->readULong(1);
    switch (c) {
    case 0x9:
      listener->insertTab();
      break;
    case 0xd:
      listener->insertEOL();
      break;
    default:
      listener->insertCharacter((unsigned char) c);
      break;
    }
  }
  return true;
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
bool BeagleWksSSParser::sendSpreadsheet()
{
  MWAWSpreadsheetListenerPtr listener=getSpreadsheetListener();
  MWAWInputStreamPtr &input= getInput();
  if (!listener) {
    MWAW_DEBUG_MSG(("BeagleWksSSParser::sendSpreadsheet: I can not find the listener\n"));
    return false;
  }
  BeagleWksSSParserInternal::Spreadsheet &sheet = m_state->m_spreadsheet;
  sheet.updateWidthCols();
  size_t numCell = sheet.m_cells.size();

  int prevRow = -1;
  listener->openSheet(sheet.convertInPoint(sheet.m_widthCols,76), librevenge::RVNG_POINT, m_state->m_spreadsheetName);
  sendPageFrames();
  // send the chart
  for (size_t i = 0; i < m_state->m_chartList.size(); ++i) {
    if (!m_state->m_chartList[i]) continue;
    BeagleWksSSParserInternal::Chart &chart=*(m_state->m_chartList[i]);
    // chart have no position, so we create one
    chart.setDimension(Vec2i(200,200));
    MWAWPosition fPos(Vec2f(0,0), Vec2f(200,200), librevenge::RVNG_POINT);
    fPos.setPagePos(1, Vec2f(float(i)*200,200));
    fPos.setRelativePosition(MWAWPosition::Page);
    listener->insertChart(fPos, chart);
  }
  std::vector<float> rowHeight = sheet.convertInPoint(sheet.m_heightRows,16);
  for (size_t i = 0; i < numCell; i++) {
    BeagleWksSSParserInternal::Cell const &cell= sheet.m_cells[i];
    // FIXME: openSheetRow/openSheetCell must do that
    if (cell.position()[1] != prevRow) {
      while (cell.position()[1] > prevRow) {
        if (prevRow != -1)
          listener->closeSheetRow();
        prevRow++;
        if (prevRow < int(rowHeight.size()))
          listener->openSheetRow(rowHeight[size_t(prevRow)], librevenge::RVNG_POINT);
        else
          listener->openSheetRow(16, librevenge::RVNG_POINT);
      }
    }
    listener->openSheetCell(cell, cell.m_content);
    if (cell.m_content.m_textEntry.valid()) {
      listener->setFont(cell.isFontSet() ? cell.getFont() : MWAWFont());
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
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool BeagleWksSSParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = BeagleWksSSParserInternal::State();
  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork() || !input->checkPosition(66))
    return false;

  libmwaw::DebugStream f;
  f << "FileHeader:";

  input->seek(0, librevenge::RVNG_SEEK_SET);
  if (input->readLong(2)!=0x4257 || input->readLong(2)!=0x6b73 ||
      input->readLong(2)!=0x4257 || input->readLong(2)!=0x7373 ||
      input->readLong(2)!=0x4257 || input->readLong(2)!=0x7373) {
    return false;
  }
  for (int i=0; i < 9; ++i) { // f2=f6=1 other 0
    int val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  setVersion(1);

  if (header)
    header->reset(MWAWDocument::MWAW_T_BEAGLEWORKS, 1, MWAWDocument::MWAW_K_SPREADSHEET);

  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  long pos=input->tell();
  f.str("");
  f << "FileHeader-II:";
  m_state->m_spreadsheetBegin=input->readLong(4);
  if (!input->checkPosition(m_state->m_spreadsheetBegin)) {
    MWAW_DEBUG_MSG(("BeagleWksSSParser::checkHeader: can not read the spreadsheet position\n"));
    return false;
  }
  f << "spreadsheet[ptr]=" << std::hex << m_state->m_spreadsheetBegin << std::dec << ",";
  for (int i=0; i < 11; ++i) { // f2=0x50c|58c|5ac f3=f5=9
    long val=input->readLong(2);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  MWAWEntry entry;
  entry.setBegin(input->readLong(4));
  entry.setLength(input->readLong(4));
  entry.setId((int) input->readLong(2)); // in fact nFonts
  entry.setType("FontNames");
  f << "fontNames[ptr]=" << std::hex << entry.begin() << "<->" << entry.end()
    << std::dec << ",nFonts=" << entry.id() << ",";
  if (entry.length() && (!entry.valid() || !input->checkPosition(entry.end()))) {
    MWAW_DEBUG_MSG(("BeagleWksSSParser::checkHeader: can not read the font names position\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }

  m_state->m_typeEntryMap.insert
  (std::multimap<std::string, MWAWEntry>::value_type(entry.type(),entry));
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  if (strict && !readPrintInfo())
    return false;
  ascii().addPos(66);
  ascii().addNote("_");

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
