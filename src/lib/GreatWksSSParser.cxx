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

////////////////////////////////////////
//! Internal: the state of a GreatWksSSParser
struct State {
  //! constructor
  State() : m_columnsWidth(), m_hasColSep(false), m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0)
  {
    for (int i=0; i<4; ++i)
      m_hfFlags[i]=false;
  }
  //! returns the number of expected header/footer zones
  int numHeaderFooters() const
  {
    int num=0;
    if (m_hfFlags[2]) num++; // header
    if (m_hfFlags[3]) num++; // footer
    if (m_hfFlags[1]) num*=2; // lf page
    return num;
  }

  //! returns a section
  MWAWSection getSection() const
  {
    MWAWSection sec;
    size_t numCols = m_columnsWidth.size()/2;
    if (numCols <= 1)
      return sec;
    sec.m_columns.resize(size_t(numCols));
    if (m_hasColSep)
      sec.m_columnSeparator=MWAWBorder();
    for (size_t c=0; c < numCols; c++) {
      double wSep=0;
      if (c)
        wSep += sec.m_columns[c].m_margins[libmwaw::Left]=
                  double(m_columnsWidth[2*c]-m_columnsWidth[2*c-1])/72./2.;
      if (c+1!=numCols)
        wSep+=sec.m_columns[c].m_margins[libmwaw::Right]=
                double(m_columnsWidth[2*c+2]-m_columnsWidth[2*c+1])/72./2.;
      sec.m_columns[c].m_width =
        double(m_columnsWidth[2*c+1]-m_columnsWidth[2*c])+72.*wSep;
      sec.m_columns[c].m_widthUnit = librevenge::RVNG_POINT;
    }
    return sec;
  }

  //! the columns dimension
  std::vector<double> m_columnsWidth;
  //! flags to define header/footer (titlePage, l/rPage, header, footer)
  bool m_hfFlags[4];
  //! true if columns have columns separator
  bool m_hasColSep;
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
  m_textParser.reset(new GreatWksText(*this));
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
// position and height
////////////////////////////////////////////////////////////
Vec2f GreatWksSSParser::getPageLeftTop() const
{
  return Vec2f(float(getPageSpan().getMarginLeft()),
               float(getPageSpan().getMarginTop()+m_state->m_headerHeight/72.0));
}

////////////////////////////////////////////////////////////
// interface with the text parser
////////////////////////////////////////////////////////////
MWAWSection GreatWksSSParser::getMainSection() const
{
  return m_state->getSection();
}

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
// new page
////////////////////////////////////////////////////////////
void GreatWksSSParser::newPage(int number)
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
  int numHF=m_state->numHeaderFooters();
  if (numHF!=m_textParser->numHFZones()) {
    MWAW_DEBUG_MSG(("GreatWksSSParser::createDocument: header/footer will be ignored\n"));
    numHF=0;
  }
  std::vector<MWAWPageSpan> pageList;
  if (numHF && m_state->m_hfFlags[0]) // title page have no header/footer
    pageList.push_back(ps);
  else
    numPages++;
  if (numHF) {
    int id=0;
    for (int w=0; w<2; ++w) {
      if (!m_state->m_hfFlags[w+2])
        continue;
      MWAWHeaderFooter::Type type=
        w==0 ? MWAWHeaderFooter::HEADER : MWAWHeaderFooter::FOOTER;
      MWAWHeaderFooter hF;
      if (m_state->m_hfFlags[1]==false) {
        hF=MWAWHeaderFooter(type, MWAWHeaderFooter::ALL);
        hF.m_subDocument.reset(new GreatWksSSParserInternal::SubDocument(*this, getInput(), id++));
        ps.setHeaderFooter(hF);
        continue;
      }
      hF=MWAWHeaderFooter(type, MWAWHeaderFooter::ODD);
      hF.m_subDocument.reset(new GreatWksSSParserInternal::SubDocument(*this, getInput(), id++));
      ps.setHeaderFooter(hF);
      hF=MWAWHeaderFooter(type, MWAWHeaderFooter::EVEN);
      hF.m_subDocument.reset(new GreatWksSSParserInternal::SubDocument(*this, getInput(), id++));
      ps.setHeaderFooter(hF);
    }
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
  readRSRCZones();

  MWAWInputStreamPtr input = getInput();
  long pos=16;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  if (!readSpreadsheet())
    return false;

  if (input->isEnd())
    return true;

  pos = input->tell();
  if (!m_graphParser->readGraphicZone())
    input->seek(pos, librevenge::RVNG_SEEK_SET);
  if (!input->isEnd()) {
    pos = input->tell();
    MWAW_DEBUG_MSG(("GreatWksSSParser::createZones: find some extra data\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(Loose):");
    ascii().addPos(pos+2000);
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
  long pos = input->tell();
  libmwaw::DebugStream f;

  while (!input->isEnd()) {
    bool ok=true, printDone=false;
    pos = input->tell();
    f.str("");
    int type=(int) input->readLong(2);
    if (type>=0 && type < 0x12) {
      static char const *(wh[0x12]) = {
        "_", "FontNames", "FontDef", "Column", "Column", "Row", "Zone6", "Row",
        "Column", "Zone9", "Zonea", "Zoneb", "Row", "Screen", "DocOptions", "Selection",
        "DocInfo", "CalcMode"
      };
      f << "Entries(" << wh[type] << "):";
    }
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
      if (!readFonts()) {
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
      if (sz<2) {
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
    case 0x13:
      if (!readCell()) {
        ok=false;
        break;
      }
      printDone = true;
      endPos=input->tell();
      break;
    case 0x14:
      ascii().addPos(pos);
      ascii().addNote("_");
      return true;
    default: {
      endPos=pos+6+(long) input->readULong(4);
      if (type <= 0 || !input->checkPosition(endPos)) {
        ok=false;
        break;
      }
      break;
    }
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
// read a cell
////////////////////////////////////////////////////////////
bool GreatWksSSParser::readCell()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;

  f << "Entries(Cell):";
  long sz=(long) input->readULong(4);
  long endPos=pos+4+sz;
  if (sz < 12 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("GreatWksSSParser::readCell: can not find a cell\n"));
    f << "###";
    ascii().addPos(pos-2);
    ascii().addNote(f.str().c_str());
    return false;
  }
  int cPos[2];
  for (int i=0; i<2; ++i)
    cPos[i]=(int) input->readLong(2);
  f << "cell" << cPos[0] << "x" << cPos[1] << ",";
  f << "style" << (int) input->readLong(2) << ",";
  int val=(int) input->readULong(2);
  switch ((val>>8)&0x1f) {
  case 0:
    break;
  case 1:
    f << "currency,";
    break;
  case 2:
    f << "percent,";
    break;
  case 3:
    f << "fixed,";
    break;
  case 4:
    f << "scientific,";
    break;
  case 5:
    f << "bool,";
    break;
  default:
    f << "#format=" << ((val>>8)&0x1f) << ",";
    break;
  }
  val &=0xE0FF;
  if (val)
    f << "fl0=" << std::hex << val << std::dec << ",";
  val=(int) input->readULong(2);
  if (val&0xf) {
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
    f << "align=left,";
    break;
  case 2:
    f << "align=center,";
    break;
  case 3:
    f << "align=right,";
    break;
  case 4:
    f << "align=repeat,";
    break;
  default:
    f << "#align=" << ((val>>8)&0x7) << ",";
  }
  if (val&0x8000) f << "protected,";
  val &= 0x78F0;
  if (val)
    f << "fl1=" << std::hex << val << std::dec << ",";
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
bool GreatWksSSParser::readFonts()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  int const vers=version();
  libmwaw::DebugStream f;

  f << "Entries(FontDef):";
  long sz=(long) input->readULong(4);
  long endPos=pos+4+sz;
  int expectedSize=vers==1 ? 18 : 40;
  if (!input->checkPosition(endPos) || (sz%expectedSize)) {
    MWAW_DEBUG_MSG(("GreatWksSSParser::readFonts: can not find the font defs zone\n"));
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
      f << "col" << j << "=" << MWAWColor(color[0],color[1],color[2]) << ",";
    }
    val=(int) input->readLong(2);
    if (val) f << "pattern[id]=" << val << ",";
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

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
