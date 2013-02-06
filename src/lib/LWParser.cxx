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

#include <libwpd/libwpd.h>

#include "MWAWContentListener.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWSubDocument.hxx"

#include "LWGraph.hxx"
#include "LWText.hxx"

#include "LWParser.hxx"

/** Internal: the structures of a LWParser */
namespace LWParserInternal
{
////////////////////////////////////////
//! Internal: the state of a LWParser
struct State {
  //! constructor
  State() : m_actPage(0), m_numPages(0), m_numCol(1), m_colSep(0), m_headerHeight(0), m_footerHeight(0) {
  }

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_numCol /** the number of columns */,
      m_colSep /** the columns separator width in point */;
  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a LWParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(LWParser &pars, MWAWInputStreamPtr input, bool header) :
    MWAWSubDocument(&pars, input, MWAWEntry()), m_isHeader(header) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const {
    if (MWAWSubDocument::operator!=(doc)) return true;
    SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
    if (!sDoc) return true;
    if (m_isHeader != sDoc->m_isHeader) return true;
    return false;
  }

  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  //! true if we need to send the parser
  int m_isHeader;
};

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("LWParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  assert(m_parser);

  reinterpret_cast<LWParser *>(m_parser)->sendHeaderFooter(m_isHeader);
}
}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
LWParser::LWParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWParser(input, rsrcParser, header), m_listener(), m_convertissor(), m_state(),
  m_pageSpan(), m_pageSpanSet(false), m_graphParser(), m_textParser()
{
  init();
}

LWParser::~LWParser()
{
  if (m_listener.get()) m_listener->endDocument();
}

void LWParser::init()
{
  m_convertissor.reset(new MWAWFontConverter);
  m_listener.reset();
  setAsciiName("main-1");

  m_state.reset(new LWParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  m_pageSpan.setMarginTop(0.1);
  m_pageSpan.setMarginBottom(0.1);
  m_pageSpan.setMarginLeft(0.1);
  m_pageSpan.setMarginRight(0.1);

  m_graphParser.reset(new LWGraph(getInput(), *this, m_convertissor));
  m_textParser.reset(new LWText(getInput(), *this, m_convertissor));
}

void LWParser::setListener(MWAWContentListenerPtr listen)
{
  m_listener = listen;
  m_textParser->setListener(listen);
  m_graphParser->setListener(listen);
}

MWAWInputStreamPtr LWParser::rsrcInput()
{
  return getRSRCParser()->getInput();
}

libmwaw::DebugFile &LWParser::rsrcAscii()
{
  return getRSRCParser()->ascii();
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
float LWParser::pageHeight() const
{
  return float(m_pageSpan.getFormLength()-m_pageSpan.getMarginTop()-m_pageSpan.getMarginBottom()-m_state->m_headerHeight/72.0-m_state->m_footerHeight/72.0);
}

float LWParser::pageWidth() const
{
  return float(m_pageSpan.getFormWidth()-m_pageSpan.getMarginLeft()-m_pageSpan.getMarginRight());
}

Vec2f LWParser::getPageLeftTop() const
{
  return Vec2f(float(m_pageSpan.getMarginLeft()),
               float(m_pageSpan.getMarginTop()+m_state->m_headerHeight/72.0));
}

////////////////////////////////////////////////////////////
// interface with the text parser
////////////////////////////////////////////////////////////
bool LWParser::getColumnInfo(int &numCols, int &colSep) const
{
  if (m_state->m_numCol < 1) {
    numCols = 1;
    colSep = 0;
    return false;
  }
  numCols = m_state->m_numCol;
  colSep = m_state->m_colSep;
  return true;
}

bool LWParser::sendHeaderFooter(bool header)
{
  MWAWInputStreamPtr input = getInput();
  MWAWInputStreamPtr rsrc = rsrcInput();
  long pos = input->tell(), rsrcPos = rsrc ? rsrc->tell() : 0;
  m_textParser->sendHeaderFooter(header);
  input->seek(pos, WPX_SEEK_SET);
  if (rsrc)
    rsrc->seek(rsrcPos, WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// interface with the graph parser
////////////////////////////////////////////////////////////
void LWParser::sendGraphic(int graphId)
{
  MWAWInputStreamPtr input = getInput();
  MWAWInputStreamPtr rsrc = rsrcInput();
  long pos = input->tell(), rsrcPos = rsrc ? rsrc->tell() : 0;
  m_graphParser->send(graphId);
  input->seek(pos, WPX_SEEK_SET);
  if (rsrc)
    rsrc->seek(rsrcPos, WPX_SEEK_SET);
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void LWParser::newPage(int number)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!m_listener || m_state->m_actPage == 1)
      continue;
    m_listener->insertBreak(MWAWContentListener::PageBreak);
  }
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void LWParser::parse(WPXDocumentInterface *docInterface)
{
  assert(getInput().get() != 0 && getRSRCParser());

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
      m_graphParser->flushExtra();
      m_textParser->flushExtra();
#endif
    }
    ascii().reset();
  } catch (...) {
    MWAW_DEBUG_MSG(("LWParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void LWParser::createDocument(WPXDocumentInterface *documentInterface)
{
  if (!documentInterface) return;
  if (m_listener) {
    MWAW_DEBUG_MSG(("LWParser::createDocument: listener already exist\n"));
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

  std::vector<MWAWPageSpan> pageList;
  MWAWPageSpan ps(m_pageSpan);

  if (m_textParser->hasHeaderFooter(true)) {
    shared_ptr<MWAWSubDocument> subdoc(new LWParserInternal::SubDocument(*this, getInput(), true));
    ps.setHeaderFooter(MWAWPageSpan::HEADER, MWAWPageSpan::ALL, subdoc);
  }
  if (m_textParser->hasHeaderFooter(false)) {
    shared_ptr<MWAWSubDocument> subdoc(new LWParserInternal::SubDocument(*this, getInput(), false));
    ps.setHeaderFooter(MWAWPageSpan::FOOTER, MWAWPageSpan::ALL, subdoc);
  }

  for (int i = 0; i <= m_state->m_numPages; i++) pageList.push_back(ps);

  MWAWContentListenerPtr listen(new MWAWContentListener(m_convertissor, pageList, documentInterface));
  setListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool LWParser::createZones()
{
  MWAWRSRCParserPtr rsrcParser = getRSRCParser();
  if (!rsrcParser) {
    MWAW_DEBUG_MSG(("LWParser::createZones: can not find the entry map\n"));
    return false;
  }
  std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::iterator it;

  // the different zones
  it = entryMap.lower_bound("LWSR");
  while (it != entryMap.end()) {
    if (it->first != "LWSR")
      break;

    MWAWEntry const &entry = it++->second;
    switch(entry.id()) {
    case 1000:
      readDocument(entry);
      break;
    case 1001:
      readPrintInfo(entry);
      break;
    case 1002: // a list of int ?
      readLWSR2(entry);
      break;
    case 1003:
      readDocInfo(entry);
      break;
    case 1007:
      readTOCPage(entry);
      break;
    default:
      break;
    }
  }
  it = entryMap.lower_bound("MPSR");
  while (it != entryMap.end()) {
    if (it->first != "MPSR")
      break;

    MWAWEntry const &entry = it++->second;
    switch(entry.id()) {
    case 1005: // a constant block which contains a default font?
      readMPSR5(entry);
      break;
    case 1007:
      readTOC(entry);
      break;
    default:
      break;
    }
  }
  if (!m_textParser->createZones())
    return false;
  m_graphParser->createZones();
  return true;
}

////////////////////////////////////////////////////////////
// read the doc/print info
////////////////////////////////////////////////////////////
bool LWParser::readDocInfo(MWAWEntry const &entry)
{
  if (entry.id() != 1003)
    return false;
  if (!entry.valid() || (entry.length()%0x40)) {
    MWAW_DEBUG_MSG(("LWParser::readDocInfo: the entry seems bad\n"));
    return false;
  }
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  input->seek(entry.begin(), WPX_SEEK_SET);
  entry.setParsed(true);

  long pos;
  int N=int(entry.length()/0x40);
  libmwaw::DebugStream f;
  for (int n = 0; n < N; n++) {
    pos = input->tell();
    f.str("");
    if (n==0)
      f << "Entries(DocInfo):";
    else
      f << "DocInfo-" << n << ":";

    int fl=int(input->readULong(1)); // 0|28
    if (fl) f << "fl0=" << fl << ",";
    long val=int(input->readULong(1)); // a small number less than 12
    if (val) f << "f0=" << val << ",";
    val = input->readLong(2); // 0|3|4|c
    if (val) f << "f1=" << val << ",";
    int dim[2];
    for (int i = 0; i < 2; i++)
      dim[i] = int(input->readLong(2));
    f << "dim=" << dim[0] << "x" << dim[1] << ",";
    int margins[4];
    f << "margins=[";
    for (int i = 0; i < 4; i++) {
      margins[i] = int(input->readLong(2));
      f << margins[i] << ",";
    }
    f << "],";
    for (int i = 0; i < 6; i++) { // f2=0|1c, f3=7|9|f, f4=f6=f7=0|e, f5=0|e|54|78
      val = input->readLong(2);
      if (val) f << "f" << i+2 << "=" << val << ",";
    }
    for (int i = 0; i < 6; i++) { // f1=0|1, f2=1, f3=0|1, f4=0|1, f5=0, f6=1
      val = (int) input->readULong(1);
      if (val) f << "fl" << i+1 << "=" << val << ",";
    }
    for (int i = 0; i < 5; i++) { // g0=0|18, g1=0|14, g4=0|..|6
      val = input->readLong(2);
      if (val) f << "g" << i << "=" << val << ",";
    }
    f << "col?=[" << std::hex;
    for (int i = 0; i <3; i++)
      f << input->readULong(2) << ",";
    f << "]," << std::dec;
    for (int i = 0; i < 6; i++) { // fl1=0|1, fl2=0|1, fl4=0|1, fl5=0|1
      val = (int) input->readULong(1);
      if (val) f << "fl" << i << "(2)=" << val << ",";
    }
    for (int i = 0; i < 4; i++) { // alway 0?
      val = input->readLong(2);
      if (val) f << "h" << i << "=" << val << ",";
    }

    ascFile.addPos(n==0 ? pos-4 : pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+0x40, WPX_SEEK_SET);
  }
  return true;
}

bool LWParser::readPrintInfo(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() < 0x78) {
    MWAW_DEBUG_MSG(("LWParser::readPrintInfo: the entry is bad\n"));
    return false;
  }
  if (entry.id() != 1001) {
    MWAW_DEBUG_MSG(("LWParser::readPrintInfo: the entry id %d is odd\n", entry.id()));
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  // print info
  libmwaw::PrinterInfo info;
  if (!info.read(input)) return false;
  if (entry.id() != 1001)
    f << "Entries(PrintInfo)[#" << entry.id() << "]:" << info;
  else
    f << "Entries(PrintInfo):" << info;
  if (entry.length() != 0x78)
    f << "###size=" << entry.length() << ",";
  rsrcAscii().addPos(pos-4);
  rsrcAscii().addNote(f.str().c_str());
  if (m_pageSpanSet)
    return true;

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

  m_pageSpan.setMarginTop(lTopMargin.y()/72.0);
  m_pageSpan.setMarginBottom(botMarg/72.0);
  m_pageSpan.setMarginLeft(lTopMargin.x()/72.0);
  m_pageSpan.setMarginRight(rightMarg/72.0);
  m_pageSpan.setFormLength(paperSize.y()/72.);
  m_pageSpan.setFormWidth(paperSize.x()/72.);

  return true;
}

////////////////////////////////////////////////////////////
// read the TOC data
////////////////////////////////////////////////////////////
bool LWParser::readTOCPage(MWAWEntry const &entry)
{
  if (entry.id() != 1007)
    return false;
  if (!entry.valid() || entry.length()<0x24) {
    MWAW_DEBUG_MSG(("LWParser::readTOCPage: the entry is bad\n"));
    return false;
  }
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(TOCpage)[" << entry.id() << "]:";
  entry.setParsed(true);
  int dim[4];
  for (int i = 0; i < 4; i++)
    dim[i] = (int) input->readLong(4);
  f << "dim?=" << dim[0] << "x" << dim[1]
    << "<->"  << dim[2] << "x" << dim[3] << ",";
  int val;
  for (int i = 0; i < 9; i++) { // f5=1|2|21, f8=256
    val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  int N = (int) input->readLong(2);
  f << "N=" << N << ",";
  if (input->tell()+N>entry.end()) {
    MWAW_DEBUG_MSG(("LWParser::readTOCPage: the page seems bead\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  f << "pages=[";
  for (int i = 0; i < N; i++)
    f << (int) input->readULong(1) << ",";
  f << "],";
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool LWParser::readTOC(MWAWEntry const &entry)
{
  if (entry.id() != 1007)
    return false;
  if (!entry.valid() || entry.length()<2) {
    MWAW_DEBUG_MSG(("LWParser::readTOC: the entry is bad\n"));
    return false;
  }
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(TOCdata)[" << entry.id() << "]:";
  entry.setParsed(true);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (long(N*9+2) > entry.length()) {
    MWAW_DEBUG_MSG(("LWParser::readTOC: the number of entry seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());

  bool ok = true;
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    if (pos+9 > entry.end()) {
      ok = false;
      break;
    }
    f.str("");
    f << "TOCdata-" << i << ":";
    long cPos[2];
    for (int j = 0; j < 2; j++)
      cPos[j] = (long) input->readULong(4);
    f << "cpos?=" << cPos[0] << "<->" << cPos[1] << ",";
    int nC = (int) input->readULong(1);
    if (pos+9+nC > entry.end()) {
      ok = false;
      break;
    }
    std::string name("");
    for (int c = 0; c < nC; c++)
      name += (char) input->readULong(1);
    f << name;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    f.str("");
    f << "[TOC" << i << "]";
    ascii().addPos(cPos[0]);
    ascii().addNote(f.str().c_str());
  }
  if (!ok) {
    f << "###";
    MWAW_DEBUG_MSG(("LWParser::readTOC: can not read end\n"));
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the main document information data then unknown data
////////////////////////////////////////////////////////////
bool LWParser::readDocument(MWAWEntry const &entry)
{
  if (entry.id() != 1000)
    return false;
  if (!entry.valid() || entry.length()<0x28) {
    MWAW_DEBUG_MSG(("LWParser::readDocument: the entry seems bad\n"));
    return false;
  }
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(Document):";
  entry.setParsed(true);
  long val;
  for (int i=0; i<3; i++) { // fl1=0|2|6, fl2=0|80
    val = (long) input->readULong(1);
    if (val) f << "fl" << i << std::hex << "=" << val << std::dec << ",";
  }
  for (int i=0; i<2; i++) { // f0=0|1, f1=0|1
    val = (long) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  for (int i=0; i<3; i++) { // fl3=0|1, fl4=0|1, fl5=1
    val = (long) input->readULong(1);
    if (val) f << "fl" << i+3 << "=" << val << ",";
  }
  int pageDim[2];
  for(int i=0; i<2; i++)
    pageDim[i] = (int) input->readLong(2);
  f << "dim=" << pageDim[1] << "x" << pageDim[0] << ",";
  int dim[4];
  for (int s=0; s<2; s++) {
    for(int i=0; i<4; i++)
      dim[i] = (int) input->readULong(2);
    f << "pos" << s << "=" << dim[1] << "x" << dim[0]
      << "<->" << dim[3] << "x" << dim[2] << ",";
    if (s==1) break;

    int margins[4]= {dim[0], dim[1], pageDim[0]-dim[2], pageDim[1]-dim[3]};
    if (margins[2] > 0 && 2*(margins[0]+margins[2]) < pageDim[0] &&
        margins[3] > 0 && 2*(margins[1]+margins[3]) < pageDim[1]) {
      m_pageSpan.setMarginTop(double(margins[0])/72.0);
      m_pageSpan.setMarginBottom(double(margins[2])/72.0);
      m_pageSpan.setMarginLeft(double(margins[1])/72.0);
      m_pageSpan.setMarginRight(double(margins[3])/72.0);
      m_pageSpan.setFormLength(double(pageDim[0])/72.);
      m_pageSpan.setFormWidth(double(pageDim[1])/72.);
      m_pageSpanSet = true;
    }
  }
  m_state->m_numCol = (int) input->readLong(2);
  if (m_state->m_numCol > 1) f << "numCols=" << m_state->m_numCol << ",";
  m_state->m_colSep = (int) input->readLong(2);
  if (m_state->m_colSep) f << "colSep=" << m_state->m_colSep << ",";

  for (int i=0; i<3; i++) { // gl0=3|3fff|4000|9001, gl1=3|4000|9001,gl2=d[12]|f[12]|1d2
    val = (long) input->readULong(2);
    if (val) f << "gl" << i << "=" << std::hex << val << std::dec << ",";
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  if (entry.length()==0x28)
    return true;
  m_textParser->readDocumentHF(entry);
  return true;
}

bool LWParser::readLWSR2(MWAWEntry const &entry)
{
  if (entry.id() != 1002)
    return false;
  if (!entry.valid() || entry.length()%4) {
    MWAW_DEBUG_MSG(("LWParser::readLWSR2: the entry seems bad\n"));
    return false;
  }
  int N = int(entry.length()/4);
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(LWSR2):";
  entry.setParsed(true);
  f << "pos?=[" << std::hex;
  for (int i = 0; i < N; i++)
    f << input->readLong(4) << ",";
  f << std::dec << "],";
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool LWParser::readMPSR5(MWAWEntry const &entry)
{
  if (entry.id() != 1005)
    return false;
  if (!entry.valid() || entry.length() != 0x48) {
    MWAW_DEBUG_MSG(("LWParser::readMPSR5: the entry is bad\n"));
    return false;
  }
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(MPSR5):";
  entry.setParsed(true);
  long val = input->readLong(2); // a|c
  if (val)
    f << "f0=" << val << ",";
  std::string name("");
  for (int i = 0; i < 32; i++) {
    char c = (char) input->readULong(1);
    if (!c)
      break;
    name += c;
  }
  f << "defFont?=\"" << name << "\",";
  input->seek(pos+34, WPX_SEEK_SET);
  for (int i = 0; i < 2; i++) { // f1=3|4|6, f2=4
    val = input->readLong(2);
    if (val)
      f << "f" << i+1 << "=" << val << ",";
  }
  int dim[4];
  for (int s=0; s<2; s++) {
    for(int i=0; i<4; i++)
      dim[i] = (int) input->readLong(2);
    f << "pos" << s << "=" << dim[1] << "x" << dim[0]
      << "<->" << dim[3] << "x" << dim[2] << ",";
  }
  val = (long) input->readULong(4); // a very big number
  if (val)
    f << "unkn=" << std::hex << val << std::dec << ",";
  long sel[2]; // checkme
  for(int i=0; i<2; i++)
    sel[i] = input->readLong(4);
  if (sel[0]==sel[1])
    f << "sel?=" << std::hex << sel[0] << std::dec << ",";
  else
    f << "sel?=" << std::hex << sel[0] << "x" << sel[1] << std::dec << ",";
  for (int i = 0; i < 2; i++) { // g1=0|6
    val = input->readLong(2);
    if (val)
      f << "g" << i << "=" << val << ",";
  }
  for (int i = 0; i < 2; i++) { // fl0=0|1, fl1=0|1
    val = (long) input->readULong(1);
    if (val)
      f << "fl" << i << "=" << val << ",";
  }

  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
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
bool LWParser::checkHeader(MWAWHeader *header, bool /*strict*/)
{
  *m_state = LWParserInternal::State();
  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork() || !getRSRCParser())
    return false;
  // check if the LWSR string exists
  MWAWEntry entry = getRSRCParser()->getEntry("LWSR", 1000);
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("LWParser::checkHeader: can not find the LWSR[1000] resource, not a Mac File!!!\n"));
    return false;
  }
  if (header)
    header->reset(MWAWDocument::LWTEXT, 1);

  return true;
}


// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
