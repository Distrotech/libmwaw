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

#include "MWAWContentListener.hxx"
#include "MWAWHeader.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWSection.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSubDocument.hxx"

#include "GWGraph.hxx"
#include "GWText.hxx"

#include "GWParser.hxx"

/** Internal: the structures of a GWParser */
namespace GWParserInternal
{

////////////////////////////////////////
//! Internal: the state of a GWParser
struct State {
  //! constructor
  State() : m_docType(GWParser::TEXT), m_columnsWidth(), m_hasColSep(false), m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0) {
    for (int i=0; i<4; ++i)
      m_hfFlags[i]=false;
  }
  //! returns the number of expected header/footer zones
  int numHeaderFooters() const {
    int num=0;
    if (m_hfFlags[2]) num++; // header
    if (m_hfFlags[3]) num++; // footer
    if (m_hfFlags[1]) num*=2; // lf page
    return num;
  }

  //! returns a section
  MWAWSection getSection() const {
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

  //! the document type
  GWParser::DocType m_docType;
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
//! Internal: the subdocument of a GWParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(GWParser &pars, MWAWInputStreamPtr input, int zoneId) :
    MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  //! the subdocument id
  int m_id;
};

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("GWParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  if (type!=libmwaw::DOC_HEADER_FOOTER) {
    MWAW_DEBUG_MSG(("GWParserInternal::SubDocument::parse: unknown type\n"));
    return;
  }

  assert(m_parser);

  long pos = m_input->tell();
  reinterpret_cast<GWParser *>(m_parser)->sendHF(m_id);
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
GWParser::GWParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWParser(input, rsrcParser, header), m_state(), m_graphParser(), m_textParser()
{
  init();
}

GWParser::~GWParser()
{
}

void GWParser::init()
{
  resetListener();
  setAsciiName("main-1");

  m_state.reset(new GWParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);

  m_graphParser.reset(new GWGraph(*this));
  m_textParser.reset(new GWText(*this));
}

MWAWInputStreamPtr GWParser::rsrcInput()
{
  return getRSRCParser()->getInput();
}

libmwaw::DebugFile &GWParser::rsrcAscii()
{
  return getRSRCParser()->ascii();
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
Vec2f GWParser::getPageLeftTop() const
{
  return Vec2f(float(getPageSpan().getMarginLeft()),
               float(getPageSpan().getMarginTop()+m_state->m_headerHeight/72.0));
}

GWParser::DocType GWParser::getDocumentType() const
{
  return m_state->m_docType;
}

////////////////////////////////////////////////////////////
// interface with the text parser
////////////////////////////////////////////////////////////
MWAWSection GWParser::getMainSection() const
{
  return m_state->getSection();
}

bool GWParser::sendHF(int id)
{
  return m_textParser->sendHF(id);
}

bool GWParser::sendTextbox(MWAWEntry const &entry, bool inGraphic)
{
  MWAWInputStreamPtr input = getInput();
  long actPos = input->tell();
  bool ok=m_textParser->sendTextbox(entry, inGraphic);
  input->seek(actPos, librevenge::RVNG_SEEK_SET);
  return ok;
}

bool GWParser::canSendTextBoxAsGraphic(MWAWEntry const &entry)
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
bool GWParser::sendPicture(MWAWEntry const &entry, MWAWPosition pos)
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
void GWParser::newPage(int number)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!getListener() || m_state->m_actPage == 1)
      continue;
    getListener()->insertBreak(MWAWContentListener::PageBreak);
  }
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void GWParser::parse(librevenge::RVNGTextInterface *docInterface)
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
      m_graphParser->sendPageGraphics();
      m_textParser->sendMainText();
#ifdef DEBUG
      m_textParser->flushExtra();
#endif
    }
    ascii().reset();
  } catch (...) {
    MWAW_DEBUG_MSG(("GWParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void GWParser::createDocument(librevenge::RVNGTextInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getListener()) {
    MWAW_DEBUG_MSG(("GWParser::createDocument: listener already exist\n"));
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
    MWAW_DEBUG_MSG(("GWParser::createDocument: header/footer will be ignored\n"));
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
        hF.m_subDocument.reset(new GWParserInternal::SubDocument(*this, getInput(), id++));
        ps.setHeaderFooter(hF);
        continue;
      }
      hF=MWAWHeaderFooter(type, MWAWHeaderFooter::ODD);
      hF.m_subDocument.reset(new GWParserInternal::SubDocument(*this, getInput(), id++));
      ps.setHeaderFooter(hF);
      hF=MWAWHeaderFooter(type, MWAWHeaderFooter::EVEN);
      hF.m_subDocument.reset(new GWParserInternal::SubDocument(*this, getInput(), id++));
      ps.setHeaderFooter(hF);
    }
  }
  ps.setPageSpan(numPages);
  pageList.push_back(ps);
  MWAWContentListenerPtr listen(new MWAWContentListener(*getParserState(), pageList, documentInterface));
  setListener(listen);
  listen->startDocument();
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool GWParser::createZones()
{
  readRSRCZones();
  if (getDocumentType()==DRAW)
    return createDrawZones();

  MWAWInputStreamPtr input = getInput();
  long pos=36;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  if (!readDocInfo()) {
    ascii().addPos(pos);
    ascii().addNote("Entries(DocInfo):###");
    return false;
  }

  bool ok=m_textParser->createZones(m_state->numHeaderFooters());
  if (input->isEnd()) // v1 file end here
    return ok;

  pos = input->tell();
  if (!m_graphParser->readGraphicZone())
    input->seek(pos, librevenge::RVNG_SEEK_SET);
  if (!input->isEnd()) {
    pos = input->tell();
    MWAW_DEBUG_MSG(("GWParser::createZones: find some extra data\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(Loose):");
    ascii().addPos(pos+200);
    ascii().addNote("_");
  }

  return ok;
}

bool GWParser::createDrawZones()
{
  MWAWInputStreamPtr input = getInput();
  long pos;
  ascii().addPos(40);
  ascii().addNote("Entries(GZoneHeader)");
  ascii().addDelimiter(68,'|');
  pos = 74;
  input->seek(74, librevenge::RVNG_SEEK_SET);
  if (!m_textParser->readFontNames())
    input->seek(pos, librevenge::RVNG_SEEK_SET);
  else
    pos = input->tell();

  bool ok=m_graphParser->readGraphicZone();
  if (!input->isEnd()) {
    pos = input->tell();
    MWAW_DEBUG_MSG(("GWParser::createZones: find some extra data\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(Loose):");
    ascii().addPos(pos+200);
    ascii().addNote("_");
  }
  return ok;
}

bool GWParser::readRSRCZones()
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
      switch(z) {
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
bool GWParser::readWPSN(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%24) != 2) {
    MWAW_DEBUG_MSG(("GWParser::readWPSN: the entry is bad\n"));
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
    MWAW_DEBUG_MSG(("GWParser::readWPSN: the number of entries seems bad\n"));
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
bool GWParser::readPrintInfo(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() != 120) {
    MWAW_DEBUG_MSG(("GWParser::readPrintInfo: the entry is bad\n"));
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
bool GWParser::readARRs(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%32)) {
    MWAW_DEBUG_MSG(("GWParser::readARRs: the entry is bad\n"));
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

bool GWParser::readDaHS(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() < 44 || (entry.length()%12) != 8) {
    MWAW_DEBUG_MSG(("GWParser::readDaHS: the entry is bad\n"));
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

bool GWParser::readGrDS(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%16)) {
    MWAW_DEBUG_MSG(("GWParser::readGrDS: the entry is bad\n"));
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
      } else if (!color.isBlack()) f << "frontColor=" << color << ",";
    }
    val = (int) input->readULong(2);
    if (val) f << "ptr?=" << std::hex << val << std::dec << ",";
    input->seek(pos+16, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(i==0?pos-4:pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool GWParser::readNxEd(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<4 ) {
    MWAW_DEBUG_MSG(("GWParser::readNxEd: the entry is bad\n"));
    return false;
  }

  if (entry.length()!=4) {
    MWAW_DEBUG_MSG(("GWParser::readNxEd: OHHHH the entry is filled\n"));
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
// read some unknown zone in data fork
////////////////////////////////////////////////////////////
bool GWParser::readDocInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  int const vers=version();
  if (!input->checkPosition(pos+46+(vers==2?6:0)+12+38*16)) {
    MWAW_DEBUG_MSG(("GWParser::readDocInfo: the zone is too short\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(DocInfo):";
  int val;
  for (int i=0; i < 4; ++i) {
    static char const *(wh[])= {"fl0", "fl1", "smartquote","hidepict"};
    val =(int) input->readLong(1);
    if (!val) continue;
    if (val==1) f << wh[i] << ",";
    else f << "#" << wh[i] << "=" << val << ",";
  }
  val =(int) input->readLong(2);
  if (val!=1) f << "first[page]=" << val << ",";
  for (int i=0; i < 19; ++i) { // always 0
    val =(int) input->readLong(2);
    if (val)
      f << "f" << i+1 << "=" << val << ",";
  }
  ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos+=46+(vers==2?6:0);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f.str("");
  f << "DocInfo-II:";
  for (int i=0; i < 4; ++i) {
    val=(int) input->readLong(1);
    if (!val) continue;
    static char const *(wh[])= {"titlePage", "left/rightPage", "header","footer"};
    if (val!=1) {
      f << "#" << wh[i] << "=" << val << ",";
      continue;
    }
    f << wh[i] << ",";
    m_state->m_hfFlags[i]=true;
  }

  val=(int) input->readLong(2); // f1=1|2
  if (val)
    f << "f0=" << val << ",";
  f << "colSep[w]=" << float(input->readLong(4))/65536.f << ",";
  val=(int) input->readLong(1);
  if (val==1) f << "same[colW]?,";
  else if (val) f << "#same[colW]=" << val << ",";
  val=(int) input->readLong(1);
  if (val==1) {
    f << "hasColSep,";
    m_state->m_hasColSep = true;
  } else if (val) f << "#hasColSep=" << val << ",";
  input->seek(pos+12, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (int i=0; i < 14; ++i) {
    pos = input->tell();
    f.str("");
    if (i<4) {
      static char const *wh[]= {"margins", "header/footer", "1", "pageDim" };
      f << "DocInfo[" << wh[i] << "]:";
    } else
      f << "DocInfo[" << i << "]:";

    double dim[4];
    for (int j=0; j<4; ++j)
      dim[j]=double(input->readLong(4))/65536.;
    if (dim[0]>0 || dim[1]>0||dim[2]>0||dim[3]>0) {
      f << "dim=" << dim[1] << "x" << dim[0] << "<->" << dim[3] << "x" << dim[2] << ",";
      if (i==0) {
        getPageSpan().setMarginTop(dim[0]/72.0);
        getPageSpan().setMarginBottom(dim[2]/72.0);
        getPageSpan().setMarginLeft(dim[1]/72.0);
        getPageSpan().setMarginRight(dim[3]/72.0);
      }
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+16, librevenge::RVNG_SEEK_SET);
  }
  for (int st=0; st < 2; ++st) {
    pos=input->tell();
    f.str("");
    if (st==0)
      f << "DocInfo[leftPage]:";
    else
      f << "DocInfo[rightPage]:";
    for (int i=0; i < 12; ++i) {
      double dim[4];
      for (int j=0; j<4; ++j)
        dim[j]=double(input->readLong(4))/65536.;
      if (dim[0]>0 || dim[1]>0||dim[2]>0||dim[3]>0) {
        switch(i) {
        case 0:
          f << "header=";
          break;
        case 11:
          f << "footer=";
          break;
        default:
          f << "col" << i-1 << "=";
          if (st==1)
            continue;
          m_state->m_columnsWidth.push_back(dim[1]);
          m_state->m_columnsWidth.push_back(dim[3]);
          break;
        }
        f << dim[1] << "x" << dim[0] << "<->" << dim[3] << "x" << dim[2] << ",";
      }
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  return true;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool GWParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = GWParserInternal::State();
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
  if (type=="ZOBJ") {
    m_state->m_docType=GWParser::DRAW;
    MWAW_DEBUG_MSG(("GWParser::checkHeader: find a draw file\n"));
  } else if (type!="ZWRT")
    return false;

  if (strict) {
    // check that the fonts table is in expected position
    long fontPos=vers==1 ? 0x302 : 0x308;
    if (m_state->m_docType==GWParser::DRAW)
      fontPos = 0x4a;
    if (input->seek(fontPos, librevenge::RVNG_SEEK_SET) || !m_textParser->readFontNames()) {
      MWAW_DEBUG_MSG(("GWParser::checkHeader: can not find fonts table\n"));
      return false;
    }
  }
  ascii().addPos(0);
  ascii().addNote(f.str().c_str());
  ascii().addPos(6);
  ascii().addNote("FileHeader-II:");

  if (header)
    header->reset(MWAWDocument::MWAW_T_GREATWORKS, vers);
  return true;
}



// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
