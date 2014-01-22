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
#include "MWAWHeader.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSection.hxx"
#include "MWAWSpreadsheetListener.hxx"
#include "MWAWSubDocument.hxx"

#include "GreatWksGraph.hxx"
#include "GreatWksText.hxx"

#include "GreatWksSSParser.hxx"

/** Internal: the structures of a GreatWksSSParser */
namespace GreatWksSSParserInternal
{
/** a cell of a GreatWksSSParser */
class Cell : public MWAWCell
{
public:
  /// constructor
  Cell() : m_font(3,12), m_content(), m_style(-1) { }
  //! returns true if the cell do contain any content
  bool isEmpty() const
  {
    return m_content.empty() && !hasBorders();
  }

  /** the font */
  MWAWFont m_font;
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
  State() : m_spreadsheet(), m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0)
  {
  }
  /** the spreadsheet */
  Spreadsheet m_spreadsheet;
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

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
  reinterpret_cast<GreatWksSSParser *>(m_parser)->sendHF(m_id);
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
  MWAWSpreadsheetParser(input, rsrcParser, header), m_state(), m_graphParser(), m_textParser()
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

  m_graphParser.reset(new GreatWksGraph(*this));
  GreatWksGraph::Callback callbackGraph;
  callbackGraph.m_canSendTextBoxAsGraphic=reinterpret_cast<GreatWksGraph::Callback::CanSendTextBoxAsGraphic>(&GreatWksSSParser::canSendTextBoxAsGraphic);
  callbackGraph.m_sendTextbox=reinterpret_cast<GreatWksGraph::Callback::SendTextbox>(&GreatWksSSParser::sendTextbox);
  m_graphParser->setCallback(callbackGraph);

  m_textParser.reset(new GreatWksText(*this));
  GreatWksText::Callback callbackText;
  callbackText.m_sendPicture=reinterpret_cast<GreatWksText::Callback::SendPicture>(&GreatWksSSParser::sendPicture);
}

MWAWInputStreamPtr GreatWksSSParser::rsrcInput()
{
  return getRSRCParser()->getInput();
}

libmwaw::DebugFile &GreatWksSSParser::rsrcAscii()
{
  return getRSRCParser()->ascii();
}

////////////////////////////////////////////////////////////
// interface with the text parser
////////////////////////////////////////////////////////////
bool GreatWksSSParser::sendHF(int id)
{
  return m_textParser->sendHF(id);
}

bool GreatWksSSParser::sendTextbox(MWAWEntry const &entry, bool inGraphic)
{
  MWAWInputStreamPtr input = getInput();
  long actPos = input->tell();
  bool ok=m_textParser->sendTextbox(entry, inGraphic);
  input->seek(actPos, librevenge::RVNG_SEEK_SET);
  return ok;
}

bool GreatWksSSParser::canSendTextBoxAsGraphic(MWAWEntry const &entry)
{
  MWAWInputStreamPtr input = getInput();
  long actPos = input->tell();
  bool ok=m_textParser->canSendTextBoxAsGraphic(entry);
  input->seek(actPos, librevenge::RVNG_SEEK_SET);
  return ok;
}

////////////////////////////////////////////////////////////
// interface with the graph parser
////////////////////////////////////////////////////////////
bool GreatWksSSParser::sendPicture(MWAWEntry const &entry, MWAWPosition pos)
{
  MWAWInputStreamPtr input = getInput();
  long actPos = input->tell();
  bool ok=m_graphParser->sendPicture(entry, pos);
  input->seek(actPos, librevenge::RVNG_SEEK_SET);
  return ok;
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
    ok = false;
    if (ok) {
      createDocument(docInterface);
      m_graphParser->sendPageGraphics();
      m_textParser->sendMainText();
#ifdef DEBUG
      m_textParser->flushExtra();
#endif
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
  if (m_graphParser->numPages() > numPages)
    numPages = m_graphParser->numPages();
  if (m_textParser->numPages() > numPages)
    numPages = m_textParser->numPages();
  m_state->m_numPages = numPages;

  MWAWPageSpan ps(getPageSpan());
  std::vector<MWAWPageSpan> pageList;
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
  readRSRCZones();

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

bool GreatWksSSParser::readRSRCZones()
{
  MWAWRSRCParserPtr rsrcParser = getRSRCParser();
  if (!rsrcParser)
    return true;

  std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::iterator it;
  // the 1 zone
  char const *(zNames[]) = {"PRNT", "PAT#", "WPSN", "PlTT", "ARRs", "DaHS", "GrDS", "NxEd" };
  for (int z = 0; z < 8; ++z) {
    it = entryMap.lower_bound(zNames[z]);
    while (it != entryMap.end()) {
      if (it->first != zNames[z])
        break;
      MWAWEntry const &entry = it++->second;
      switch (z) {
      case 0:
        readPrintInfo(entry);
        break;
      case 1:
        m_graphParser->readPatterns(entry);
        break;
      case 2:
        readWPSN(entry);
        break;
      case 3: // only in v2
        m_graphParser->readPalettes(entry);
        break;
      case 4: // only in v2?
        readARRs(entry);
        break;
      case 5: // only in v2?
        readDaHS(entry);
        break;
      case 6: // only in v2?
        readGrDS(entry);
        break;
      case 7: // only in v2?
        readNxEd(entry);
        break;
      default:
        break;
      }
    }
  }

  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read the windows position blocks
////////////////////////////////////////////////////////////
bool GreatWksSSParser::readWPSN(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%24) != 2) {
    MWAW_DEBUG_MSG(("GreatWksSSParser::readWPSN: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  libmwaw::DebugStream f;
  entry.setParsed(true);

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "Entries(Windows):";
  int N=(int) input->readLong(2);
  f << "N=" << N << ",";
  if (2+24*N!=int(entry.length())) {
    f << "###";
    MWAW_DEBUG_MSG(("GreatWksSSParser::readWPSN: the number of entries seems bad\n"));
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  for (int i=0; i < N; ++i) {
    pos = input->tell();
    f.str("");
    f << "Windows-" << i << ":";
    int width[2];
    for (int j=0; j < 2; ++j)
      width[j]=(int) input->readLong(2);
    f << "w=" << width[1] << "x" << width[0] << ",";
    int LT[2];
    for (int j=0; j < 2; ++j)
      LT[j]=(int) input->readLong(2);
    f << "LT=" << LT[1] << "x" << LT[0] << ",";
    for (int st=0; st < 2; ++st) {
      int dim[4];
      for (int j=0; j < 4; ++j)
        dim[j]=(int) input->readLong(2);
      if (dim[0]!=LT[0] || dim[1]!=LT[1] || dim[2]!=LT[0]+width[0])
        f << "dim" << st << "=" << dim[1] << "x" << dim[0] << "<->"
          << dim[3] << "x" << dim[2] << ",";
    }
    input->seek(pos+24, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool GreatWksSSParser::readPrintInfo(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() != 120) {
    MWAW_DEBUG_MSG(("GreatWksSSParser::readPrintInfo: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  libmwaw::DebugStream f;

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::PrinterInfo info;
  if (!info.read(input)) return false;
  f << "Entries(PrintInfo):"<< info;
  entry.setParsed(true);

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

  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read some unknown zone in rsrc fork
////////////////////////////////////////////////////////////
bool GreatWksSSParser::readARRs(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%32)) {
    MWAW_DEBUG_MSG(("GreatWksSSParser::readARRs: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  libmwaw::DebugStream f;
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  ascFile.addPos(pos-4);
  ascFile.addNote("Entries(ARRs)");
  int N=int(entry.length()/32);
  for (int i=0; i < N; ++i) {
    pos = input->tell();
    f.str("");
    f << "ARRs-" << i << ":";
    input->seek(pos+32, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool GreatWksSSParser::readDaHS(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() < 44 || (entry.length()%12) != 8) {
    MWAW_DEBUG_MSG(("GreatWksSSParser::readDaHS: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  libmwaw::DebugStream f;
  entry.setParsed(true);

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "Entries(DaHS):";
  int val=(int) input->readLong(2);
  if (val!=2)
    f << "#f0=" << val << ",";
  val=(int) input->readLong(2);
  if (val!=9)
    f << "#f1=" << val << ",";
  ascFile.addDelimiter(input->tell(), '|');
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());

  pos=entry.begin()+44;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int N=int((entry.length()-44))/12;

  for (int i=0; i < N; ++i) {
    pos = input->tell();
    f.str("");
    f << "DaHS-" << i << ":";
    input->seek(pos+12, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  return true;
}

bool GreatWksSSParser::readGrDS(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%16)) {
    MWAW_DEBUG_MSG(("GreatWksSSParser::readGrDS: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  libmwaw::DebugStream f;
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  ascFile.addPos(pos-4);
  ascFile.addNote("Entries(GrDS)");
  int N=int(entry.length()/16);
  for (int i=0; i < N; ++i) {
    pos = input->tell();
    f.str("");
    f << "GrDS-" << i << ":";
    int val=(int)input->readLong(2); // 1,2,3
    f << "unkn=" << val << ",";
    for (int st=0; st < 2; ++st) {
      unsigned char col[3];
      for (int j=0; j < 3; ++j)
        col[j]=(unsigned char)(input->readULong(2)>>8);
      MWAWColor color(col[0], col[1], col[2]);
      if (st==0) {
        if (!color.isWhite()) f << "backColor=" << color << ",";
      }
      else if (!color.isBlack()) f << "frontColor=" << color << ",";
    }
    val = (int) input->readULong(2);
    if (val) f << "ptr?=" << std::hex << val << std::dec << ",";
    input->seek(pos+16, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(i==0?pos-4:pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool GreatWksSSParser::readNxEd(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<4) {
    MWAW_DEBUG_MSG(("GreatWksSSParser::readNxEd: the entry is bad\n"));
    return false;
  }

  if (entry.length()!=4) {
    MWAW_DEBUG_MSG(("GreatWksSSParser::readNxEd: OHHHH the entry is filled\n"));
  }

  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  libmwaw::DebugStream f;
  entry.setParsed(true);

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "Entries(NxED):";
  for (int i = 0; i < 2; ++i) { // always 0
    int val=(int) input->readLong(2);
    if (val)
      f << "f" << i << "=" << val << ",";
  }
  ascFile.addPos(pos-4);
  ascFile.addNote("Entries(NxED):");
  return true;
}

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
      if (!m_textParser->readFontNames()) {
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
      f << "pos=" << input->readLong(2) << ",";
      f << "size=" << input->readLong(2) << ",";
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
        f << "height[def]=" << val << ",";
        break;
      case 8:
        f << "width[def]=" << val << ",";
        break;
      case 0xc: // similar to 7 related to header?
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
        if (val==1)
          f << what[i] << ",";
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
      /* TODO:  a simple or a complex text zone
         need to store the entry and then call m_textParser->sendTextbox
      */
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
      if (!m_graphParser->readPageFrames()) {
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
  /* fixme: find the limit of the last zone
     case 0x14: end of cell... */
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
  if ((val & 0xF)!=2) {
    format.m_digits=(val & 0xF);
    f << "decimal=" << format.m_digits << ",";
  }
  if (val & 0x4000) f << "parenthesis,";
  if (val & 0x8000) f << "commas,";
  val &=0x20F0;
  if (val)
    f << "fl0=" << std::hex << val << std::dec << ",";
  val=(int) input->readULong(2);
  if (val&0xf) {
    cell.setBorders(val & 0xF, MWAWBorder()); // checkme
    f << "bord=";
    if (val&1) f << "T";
    if (val&2) f << "R";
    if (val&4) f << "B";
    if (val&8) f << "L";
    f << ",";
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
      if (readFormula(cellPos,formulaPos+formSz,formula, error)) {
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
    f << "FontDef-" << i << ":";
    MWAWFont font;
    int val=(int) input->readLong(2); // always 0 ?
    if (val) f << "#unkn=" << val << ",";
    val=(int) input->readLong(2);
    if (val!=1) f << "used?=" << val << ",";

    font.setId(m_textParser->getFontId((int) input->readULong(2)));
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
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      continue;
    }
    for (int j=0; j<2; ++j) {  // front/back color?
      for (int c=0; c<3; ++c)
        color[c] = (unsigned char)(input->readULong(2)>>8);
      MWAWColor col(color[0],color[1],color[2]);
      if ((j==0 && col.isBlack()) || (j==1 && col.isWhite())) continue;
      if (j==0) f << "col[front]=" << MWAWColor(color[0],color[1],color[2]) << ",";
      else f << "col[back]=" << MWAWColor(color[0],color[1],color[2]) << ",";
    }
    val=(int) input->readLong(2);
    if (val) f << "pattern[id]=" << val << ",";
    int pattern[4];
    bool hasPattern=false;
    for (int j=0; j<4; ++j) {
      pattern[j]=(int) input->readULong(2);
      if (pattern[j]) hasPattern=true;
    }
    if (hasPattern) {
      f << "pat=[" << std::hex;
      for (int j=0; j<4; ++j)
        f << pattern[j] << ",";
      f << "]," << std::dec;
    }
    // followed by pattern(readme)
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
  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork() || !input->checkPosition(0x4c))
    return false;

  libmwaw::DebugStream f;
  f << "FileHeader:";

  input->seek(0,librevenge::RVNG_SEEK_SET);
  int vers=(int) input->readLong(1);
  if (vers < 1 || vers > 2)
    return false;
  if (input->readLong(1))
    return false;
  setVersion(vers);
  std::string type("");
  for (int i=0; i < 4; ++i)
    type+=(char) input->readLong(1);
  if (type!="ZCAL")
    return false;

  if (strict) {
    // check that the fonts table is in expected position
    long fontPos=18;
    if (input->seek(fontPos, librevenge::RVNG_SEEK_SET) || !m_textParser->readFontNames()) {
      MWAW_DEBUG_MSG(("GreatWksSSParser::checkHeader: can not find fonts table\n"));
      return false;
    }
  }
  ascii().addPos(0);
  ascii().addNote(f.str().c_str());
  ascii().addPos(6);
  ascii().addNote("FileHeader-II:");

  if (header)
    header->reset(MWAWDocument::MWAW_T_GREATWORKS, vers, MWAWDocument::MWAW_K_SPREADSHEET);
  else
    getParserState()->m_kind=MWAWDocument::MWAW_K_SPREADSHEET;
  return true;
}

////////////////////////////////////////////////////////////
// read a formula
////////////////////////////////////////////////////////////
bool GreatWksSSParser::readCellInFormula(Vec2i const &pos, MWAWCellContent::FormulaInstruction &instr)
{
  MWAWInputStreamPtr input=getInput();
  instr=MWAWCellContent::FormulaInstruction();
  instr.m_type=MWAWCellContent::FormulaInstruction::F_Cell;
  bool absolute[2] = { true, true};
  int cPos[2];
  for (int i=0; i<2; ++i) {
    int val = (int) input->readULong(2);
    if (val & 0x8000) {
      absolute[i]=0;
      if (val&0x4000)
        cPos[i] = pos[i]+(val-0xFFFF);
      else
        cPos[i] = pos[i]+(val-0x7FFF);
    }
    else
      cPos[i]=val;
  }

  if (cPos[0] < 1 || cPos[1] < 1) {
    MWAW_DEBUG_MSG(("GreatWksSSParser::readCellInFormula: can not read cell position\n"));
    return false;
  }
  instr.m_position[0]=Vec2i(cPos[1]-1,cPos[0]-1);
  instr.m_positionRelative[0]=Vec2b(!absolute[1],!absolute[0]);
  return true;
}

bool GreatWksSSParser::readString(long endPos, std::string &res)
{
  res="";
  MWAWInputStreamPtr input=getInput();
  long pos=input->tell();
  int fSz=(int) input->readULong(1);
  if (pos+1+fSz>endPos) {
    MWAW_DEBUG_MSG(("GreatWksSSParser::readString: can not read string size\n"));
    return false;
  }
  for (int i=0; i<fSz; ++i)
    res += (char) input->readULong(1);
  return true;
}

bool GreatWksSSParser::readNumber(long endPos, double &res, bool &isNan)
{
  MWAWInputStreamPtr input=getInput();
  long pos=input->tell();
  if (pos+10>endPos) {
    MWAW_DEBUG_MSG(("GreatWksSSParser::readNumber: can not read a number\n"));
    return false;
  }
  return input->readDouble10(res, isNan);
}

namespace GreatWksSSParserInternal
{
struct Functions {
  char const *m_name;
  int m_arity;
};

static Functions const s_listFunctions[] = {
  { "=", 1}, {"", -1} /*UNKN*/, {"", 0}/*SPEC:long*/, {"", -1} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "", 0} /*SPEC:double*/,{ "", -2} /*UNKN*/,{ "", 0} /*SPEC:text*/,
  { "", 0} /*SPEC:short*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,

  { "", 0} /*SPEC:cell*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,

  { ":", 2} /*SPEC:concatenate cell*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,
  { "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,{ "", -2} /*UNKN*/,

  { "(", 1}, { "-", 1}, { "+", 1} /*checkme*/,{ "^", 2},
  { "*", 2}, { "/", 2}, { "+", 2}, { "-", 2},
  { "", -2} /*UNKN*/,{ "=", 2}, { "<", 2}, { "<=", 2},
  { ">", 2}, { ">=", 2}, { "<>", 2}, { "", -2} /*UNKN*/,

};
}

bool GreatWksSSParser::readFormula(Vec2i const &cPos, long endPos, std::vector<MWAWCellContent::FormulaInstruction> &formula, std::string &error)
{
  MWAWInputStreamPtr input=getInput();
  libmwaw::DebugStream f;
  std::vector<std::vector<MWAWCellContent::FormulaInstruction> > stack;
  bool ok=true;
  while (!input->isEnd()) {
    long pos=input->tell();
    if (pos > endPos)
      break;
    int arity=0, val, type=(int) input->readULong(1);
    MWAWCellContent::FormulaInstruction instr;
    switch (type) {
    case 2:
      if (pos+1+2 > endPos) {
        ok = false;
        break;
      }
      val = (int) input->readLong(2);
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Long;
      instr.m_longValue=val;
      break;
    case 5: {
      double value;
      bool isNan;
      ok=readNumber(endPos, value, isNan);
      if (!ok) break;
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Double;
      instr.m_doubleValue=value;
      break;
    }
    case 7: {
      std::string text;
      ok=readString(endPos, text);
      if (!ok) break;
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Text;
      instr.m_content=text;
      break;
    }
    case 8:
      if (pos+1+1 > endPos) {
        ok = false;
        break;
      }
      val = (int) input->readLong(1);
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Long;
      instr.m_longValue=val;
      break;
    case 0x10: {
      ok=readCellInFormula(cPos,instr);
      if (!ok) break;
      f << instr << ",";
      break;
    }
    case 0x40: {
      if (pos+1+1 > endPos) {
        ok = false;
        break;
      }
      val = (int) input->readULong(1);
      arity= (int) input->readULong(1);
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Function;

      static char const* (s_functions[]) = {
        "", "Abs", "Exp", "Fact", "Int", "Ln", "Log", "Log10",
        "Mod", "Pi", "Product", "Rand", "Round", "Sign", "Sqrt", "Trunc",

        "Average", "Count", "CountA", "Max", "Min", "StDev", "StDevP", "Sum",
        "Var", "VarP", "Acos", "Asin", "Atan", "Atan2", "Cos", "Sin",

        "Tan", "Degree", "Radians", "And", "Choose", "False", "If", "IsBlanck",
        "IsErr", "IsError", "IsLogical", "IsNa", "IsNonText", "IsNum", "IsRef", "IsText",

        "Not", "Or", "True", "Char", "Clean", "Code", "Dollar", "Exact",
        "Find", "Fixed", "Left", "Len", "Lower", "Mid", "Proper"/*checkme: first majuscule*/, "Replace",

        "Repeat"/*checkme*/, "Right", "Search", "Substitute", "Trim", "Upper", "DDB", "FV",
        "IPMT", "IRR", "MIRR", "NPER", "NPV", "PMT", "PPMT", "PV",

        "Rate", "SLN", "SYD", "Annuity", "Compound", "Date", "Day", "Hour",
        "Minute", "Month", "Now", "Second", "Time", "Weekday", "Year", "HLookup",

        "Index", "Lookup", "Match", "N", "Na", "T", "Type", "VLookup",
        "", "", "", "", "", "", "", "",

      };
      std::string functName("");
      if (val < 0x70) functName=s_functions[val];
      if (!functName.empty())
        instr.m_content=functName;
      else {
        std::stringstream s;
        s << "Funct" << std::hex << val << std::dec << "#";
        instr.m_content=s.str();
      }
      break;
    }
    default:
      if (type >= 0x40 || GreatWksSSParserInternal::s_listFunctions[type].m_arity == -2) {
        f.str("");
        f << "##Funct" << std::hex << type << std::dec;
        ok = false;
        break;
      }
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Function;
      instr.m_content=GreatWksSSParserInternal::s_listFunctions[type].m_name;
      ok=!instr.m_content.empty();
      arity = GreatWksSSParserInternal::s_listFunctions[type].m_arity;
      if (arity == -1) arity = (int) input->readLong(1);
      if (arity<0) ok=false;
      break;
    }
    if (!ok) break;
    std::vector<MWAWCellContent::FormulaInstruction> child;
    if (instr.m_type!=MWAWCellContent::FormulaInstruction::F_Function) {
      child.push_back(instr);
      stack.push_back(child);
      continue;
    }
    size_t numElt = stack.size();
    if ((int) numElt < arity) {
      f.str("");
      f << instr.m_content << "[##" << arity << "]";
      ok = false;
      break;
    }
    if ((instr.m_content[0] >= 'A' && instr.m_content[0] <= 'Z') || instr.m_content[0] == '(') {
      if (instr.m_content[0] != '(')
        child.push_back(instr);

      instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
      instr.m_content="(";
      child.push_back(instr);
      for (int i = 0; i < arity; i++) {
        if (i) {
          instr.m_content=";";
          child.push_back(instr);
        }
        std::vector<MWAWCellContent::FormulaInstruction> const &node=
          stack[size_t((int)numElt-arity+i)];
        child.insert(child.end(), node.begin(), node.end());
      }
      instr.m_content=")";
      child.push_back(instr);

      stack.resize(size_t((int) numElt-arity+1));
      stack[size_t((int)numElt-arity)] = child;
      continue;
    }
    if (arity==1) {
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
      stack[numElt-1].insert(stack[numElt-1].begin(), instr);
      if (type==0 && pos+1==endPos)
        break;
      continue;
    }
    if (arity==2 && instr.m_content==":") {
      if (stack[numElt-2].size()!=1 || stack[numElt-2][0].m_type!=MWAWCellContent::FormulaInstruction::F_Cell ||
          stack[numElt-1].size()!=1 || stack[numElt-1][0].m_type!=MWAWCellContent::FormulaInstruction::F_Cell) {
        f << "### unexpected type of concatenate argument";
        ok=false;
        break;
      }
      instr=stack[numElt-2][0];
      MWAWCellContent::FormulaInstruction instr2=stack[numElt-1][0];
      instr.m_type=MWAWCellContent::FormulaInstruction::F_CellList;
      instr.m_position[1]=instr2.m_position[0];
      instr.m_positionRelative[1]=instr2.m_positionRelative[0];
      stack[numElt-2][0]=instr;
      stack.resize(numElt-1);
      continue;
    }
    if (arity==2) {
      instr.m_type=MWAWCellContent::FormulaInstruction::F_Operator;
      stack[numElt-2].push_back(instr);
      stack[numElt-2].insert(stack[numElt-2].end(), stack[numElt-1].begin(), stack[numElt-1].end());
      stack.resize(numElt-1);
      continue;
    }
    ok=false;
    f << "### unexpected arity";
    break;
  }

  if (!ok) ;
  else if (stack.size()==1 && stack[0].size()>1 && stack[0][0].m_content=="=") {
    formula.insert(formula.begin(),stack[0].begin()+1,stack[0].end());
    return true;
  }
  else
    f << "###stack problem";

  ascii().addDelimiter(input->tell(),'#');
  static bool first = true;
  if (first) {
    MWAW_DEBUG_MSG(("BeagleWksSSParser::readFormula: I can not read some formula\n"));
    first = false;
  }

  error = f.str();
  f.str("");
  for (size_t i = 0; i < stack.size(); ++i) {
    for (size_t j=0; j < stack[i].size(); ++j)
      f << stack[i][j] << ",";
  }
  f << error;
  error = f.str();
  return false;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
