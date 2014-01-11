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
#include <sstream>

#include <librevenge/librevenge.h>


#include "MWAWTextListener.hxx"
#include "MWAWHeader.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWSubDocument.hxx"

#include "MsWksGraph.hxx"
#include "MsWksZone.hxx"
#include "MsWks3Text.hxx"

#include "MsWks3Parser.hxx"

/** Internal: the structures of a MsWks3Parser */
namespace MsWks3ParserInternal
{
//! Internal: a zone of a MsWks3Parser ( main, header, footer )
struct Zone {
  //! the different type
  enum Type { MAIN, HEADER, FOOTER, NONE };
  //! the constructor
  Zone(Type type=NONE, int zoneId=-1) : m_type(type), m_zoneId(zoneId), m_textId(-1) {}
  //! the zone type
  Type m_type;
  //! the parser zone id
  int m_zoneId;
  //! the text internal id
  int m_textId;
};

////////////////////////////////////////
//! Internal: the state of a MsWks3Parser
struct State {
  //! constructor
  State() : m_docType(MWAWDocument::MWAW_K_TEXT), m_zoneMap(), m_actPage(0), m_numPages(0),
    m_headerText(""), m_footerText(""), m_hasHeader(false), m_hasFooter(false),
    m_pageLength(-1), m_headerHeight(0), m_footerHeight(0)
  {
  }

  //! return a zone
  Zone get(Zone::Type type)
  {
    Zone res;
    if (m_zoneMap.find(int(type)) != m_zoneMap.end())
      res = m_zoneMap[int(type)];
    return res;
  }
  //! returns true if this is a text document (hack for MSWorks 4.0 Draw)
  bool IsTextDoc() const
  {
    return m_docType == MWAWDocument::MWAW_K_TEXT;
  }
  //! the type of document
  MWAWDocument::Kind m_docType;
  //! the list of zone
  std::map<int, Zone> m_zoneMap;
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  std::string m_headerText /**header string v1-2*/, m_footerText /**footer string v1-2*/;
  bool m_hasHeader /** true if there is a header v3*/, m_hasFooter /** true if there is a footer v3*/;
  //! the page length in point (if known)
  int m_pageLength;
  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a MsWks3Parser
class SubDocument : public MWAWSubDocument
{
public:
  enum Type { Zone, Text };
  SubDocument(MsWks3Parser &pars, MWAWInputStreamPtr input, Type type,
              int zoneId, int noteId=-1) :
    MWAWSubDocument(&pars, input, MWAWEntry()), m_type(type), m_id(zoneId), m_noteId(noteId) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const
  {
    return !operator!=(doc);
  }

  //! returns the subdocument \a id
  int getId() const
  {
    return m_id;
  }
  //! sets the subdocument \a id
  void setId(int vid)
  {
    m_id = vid;
  }

  //! the parser function
  void parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  /** the type */
  Type m_type;
  /** the subdocument id*/
  int m_id;
  /** the note id */
  int m_noteId;
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("MsWks3Parser::SubDocument::parse: no listener\n"));
    return;
  }
  assert(m_parser);

  long pos = m_input->tell();
  MsWks3Parser *parser = reinterpret_cast<MsWks3Parser *>(m_parser);
  switch (m_type) {
  case Text:
    parser->sendText(m_id, m_noteId);
    break;
  case Zone:
    parser->sendZone(m_id);
    break;
  default:
    MWAW_DEBUG_MSG(("MsWks3Parser::SubDocument::parse: unexpected zone type\n"));
    break;
  }
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_id != sDoc->m_id) return true;
  if (m_type != sDoc->m_type) return true;
  if (m_noteId != sDoc->m_noteId) return true;
  return false;
}
}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MsWks3Parser::MsWks3Parser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWTextParser(input, rsrcParser, header), m_state(), m_listZones(), m_graphParser(), m_textParser(), m_zone()
{
  m_zone.reset(new MsWksZone(input, *this));
  init();
}

MsWks3Parser::~MsWks3Parser()
{
}

void MsWks3Parser::init()
{
  resetTextListener();
  setAsciiName("main-1");

  m_state.reset(new MsWks3ParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);

  m_graphParser.reset(new MsWksGraph(*this, *m_zone));
  m_textParser.reset(new MsWks3Text(*this, *m_zone));
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
double MsWks3Parser::getTextHeight() const
{
  if (m_state->m_pageLength > 0)
    return (m_state->m_pageLength-m_state->m_headerHeight-m_state->m_footerHeight)/72.0;
  return getPageSpan().getPageLength()-m_state->m_headerHeight/72.0-m_state->m_footerHeight/72.0;
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void MsWks3Parser::newPage(int number, bool softBreak)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!getTextListener() || m_state->m_actPage == 1)
      continue;
    if (softBreak)
      getTextListener()->insertBreak(MWAWTextListener::SoftPageBreak);
    else
      getTextListener()->insertBreak(MWAWTextListener::PageBreak);
  }
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void MsWks3Parser::parse(librevenge::RVNGTextInterface *docInterface)
{
  assert(m_zone && m_zone->getInput() != 0);

  if (!checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = true;
  try {
    // create the asciiFile
    m_zone->initAsciiFile(asciiName());

    checkHeader(0L);
    ok = createZones();
    if (ok) {
      createDocument(docInterface);
      sendZone(MsWks3ParserInternal::Zone::MAIN);
      m_textParser->flushExtra();
      m_graphParser->flushExtra();
    }
    m_zone->ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("MsWks3Parser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetTextListener();
  if (!ok) throw(libmwaw::ParseException());
}

void MsWks3Parser::sendText(int id, int noteId)
{
  if (noteId < 0)
    m_textParser->sendZone(id);
  else
    m_textParser->sendNote(id, noteId);
}

void MsWks3Parser::sendZone(int zoneType)
{
  if (!getTextListener()) return;
  MsWks3ParserInternal::Zone zone=m_state->get(MsWks3ParserInternal::Zone::Type(zoneType));
  if (zone.m_zoneId >= 0)
    m_graphParser->sendAll(zone.m_zoneId, zoneType==MsWks3ParserInternal::Zone::MAIN);
  if (zone.m_textId >= 0)
    m_textParser->sendZone(zone.m_textId);
}

bool MsWks3Parser::sendFootNote(int zoneId, int noteId)
{
  MWAWSubDocumentPtr subdoc
  (new MsWks3ParserInternal::SubDocument(*this, m_zone->getInput(), MsWks3ParserInternal::SubDocument::Text, zoneId, noteId));
  if (getTextListener())
    getTextListener()->insertNote(MWAWNote(MWAWNote::FootNote), subdoc);
  return true;
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void MsWks3Parser::createDocument(librevenge::RVNGTextInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getTextListener()) {
    MWAW_DEBUG_MSG(("MsWks3Parser::createDocument: listener already exist\n"));
    return;
  }

  int vers = version();

  MsWks3ParserInternal::Zone mainZone=m_state->get(MsWks3ParserInternal::Zone::MAIN);
  // update the page
  int numPage = 1;
  if (mainZone.m_textId >= 0 && m_textParser->numPages(mainZone.m_textId) > numPage)
    numPage = m_textParser->numPages(mainZone.m_textId);
  if (mainZone.m_zoneId >= 0 && m_graphParser->numPages(mainZone.m_zoneId) > numPage)
    numPage = m_graphParser->numPages(mainZone.m_zoneId);
  m_state->m_numPages = numPage;
  m_state->m_actPage = 0;

  // create the page list
  MWAWPageSpan ps(getPageSpan());
  int id = m_textParser->getHeader();
  if (id >= 0) {
    if (vers <= 2) m_state->m_headerHeight = 12;
    MWAWHeaderFooter header(MWAWHeaderFooter::HEADER, MWAWHeaderFooter::ALL);
    header.m_subDocument.reset
    (new MsWks3ParserInternal::SubDocument
     (*this, m_zone->getInput(), MsWks3ParserInternal::SubDocument::Text, id));
    ps.setHeaderFooter(header);
  }
  else if (m_state->get(MsWks3ParserInternal::Zone::HEADER).m_zoneId >= 0) {
    MWAWHeaderFooter header(MWAWHeaderFooter::HEADER, MWAWHeaderFooter::ALL);
    header.m_subDocument.reset
    (new MsWks3ParserInternal::SubDocument
     (*this, m_zone->getInput(), MsWks3ParserInternal::SubDocument::Zone, int(MsWks3ParserInternal::Zone::HEADER)));
    ps.setHeaderFooter(header);
  }
  id = m_textParser->getFooter();
  if (id >= 0) {
    if (vers <= 2) m_state->m_footerHeight = 12;
    MWAWHeaderFooter footer(MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
    footer.m_subDocument.reset
    (new MsWks3ParserInternal::SubDocument
     (*this, m_zone->getInput(), MsWks3ParserInternal::SubDocument::Text, id));
    ps.setHeaderFooter(footer);
  }
  else if (m_state->get(MsWks3ParserInternal::Zone::FOOTER).m_zoneId >= 0) {
    MWAWHeaderFooter footer(MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
    footer.m_subDocument.reset
    (new MsWks3ParserInternal::SubDocument
     (*this, m_zone->getInput(), MsWks3ParserInternal::SubDocument::Zone, int(MsWks3ParserInternal::Zone::FOOTER)));
    ps.setHeaderFooter(footer);
  }
  ps.setPageSpan(m_state->m_numPages+1);
  std::vector<MWAWPageSpan> pageList(1,ps);
  //
  MWAWTextListenerPtr listen(new MWAWTextListener(*getParserState(), pageList, documentInterface));
  setTextListener(listen);
  listen->startDocument();
  // update the graphic left top position
  m_graphParser->setPageLeftTop
  (Vec2f(72.f*float(getPageSpan().getMarginLeft()),
         72.f*float(getPageSpan().getMarginTop())+float(m_state->m_headerHeight)));
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool MsWks3Parser::createZones()
{
  MWAWInputStreamPtr input = m_zone->getInput();
  long pos = input->tell();

  if (version()>=3) {
    bool ok = true;
    if (m_state->m_hasHeader)
      ok = readGroupHeaderInfo(true,99);
    if (ok) pos = input->tell();
    else input->seek(pos, librevenge::RVNG_SEEK_SET);
    if (ok && m_state->m_hasFooter)
      ok = readGroupHeaderInfo(false,99);
    if (ok) pos = input->tell();
    else input->seek(pos, librevenge::RVNG_SEEK_SET);
  }

  MsWks3ParserInternal::Zone::Type const type = MsWks3ParserInternal::Zone::MAIN;
  MsWks3ParserInternal::Zone newZone(type, int(m_state->m_zoneMap.size()));
  m_state->m_zoneMap.insert(std::map<int,MsWks3ParserInternal::Zone>::value_type(int(type),newZone));
  MsWks3ParserInternal::Zone &mainZone = m_state->m_zoneMap.find(int(type))->second;
  while (!input->isEnd()) {
    pos = input->tell();
    if (!readZone(mainZone)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
  }

  mainZone.m_textId = m_textParser->createZones(-1, true);

  pos = input->tell();

  if (!input->isEnd())
    m_zone->ascii().addPos(input->tell());
  m_zone->ascii().addNote("Entries(End)");
  m_zone->ascii().addPos(pos+100);
  m_zone->ascii().addNote("_");

  // ok, prepare the data
  m_state->m_numPages = 1;
  std::vector<int> linesH, pagesH;
  if (m_textParser->getLinesPagesHeight(mainZone.m_textId, linesH, pagesH))
    m_graphParser->computePositions(mainZone.m_zoneId, linesH, pagesH);

  return true;
}


////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read a generic zone
////////////////////////////////////////////////////////////
bool MsWks3Parser::readZone(MsWks3ParserInternal::Zone &zone)
{
  MWAWInputStreamPtr input = m_zone->getInput();
  if (input->isEnd()) return false;
  long pos = input->tell();
  MWAWEntry pict;
  int val = (int) input->readLong(1);
  input->seek(-1, librevenge::RVNG_SEEK_CUR);
  switch (val) {
  case 0: {
    if (m_graphParser->getEntryPicture(zone.m_zoneId, pict)>=0) {
      input->seek(pict.end(), librevenge::RVNG_SEEK_SET);
      return true;
    }
    break;
  }
  case 1: {
    if (m_graphParser->getEntryPictureV1(zone.m_zoneId, pict)>=0) {
      input->seek(pict.end(), librevenge::RVNG_SEEK_SET);
      return true;
    }
    break;
  }
  case 2:
    if (readDocumentInfo())
      return true;
    break;
  case 3: {
    MWAWEntry group;
    if (readGroup(zone, group, 2))
      return true;
    break;
  }
  default:
    break;
  }

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  return false;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool MsWks3Parser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = MsWks3ParserInternal::State();
  MWAWInputStreamPtr input = m_zone->getInput();
  if (!input || !input->hasDataFork())
    return false;

  int numError = 0, val;

  const int headerSize = 0x20;

  libmwaw::DebugStream f;

  input->seek(0,librevenge::RVNG_SEEK_SET);

  m_state->m_hasHeader = m_state->m_hasFooter = false;
  int vers = (int) input->readULong(4);
  switch (vers) {
  case 11:
#ifndef DEBUG
    return false;
#else
    setVersion(4);
    break; // no-text Works4 file have classic header
#endif
  case 9:
    setVersion(3);
    break;
  case 8:
    setVersion(2);
    break;
  case 4:
    setVersion(1);
    break;
  default:
    if (strict) return false;

    MWAW_DEBUG_MSG(("MsWks3Parser::checkHeader: find unknown version 0x%x\n", vers));
    // must we stop in this case, or can we continue ?
    if (vers < 0 || vers > 14) {
      MWAW_DEBUG_MSG(("MsWks3Parser::checkHeader: version too big, we stop\n"));
      return false;
    }
    setVersion((vers < 4) ? 1 : (vers < 8) ? 2 : (vers < 11) ? 3 : 4);
  }
  if (input->seek(headerSize,librevenge::RVNG_SEEK_SET) != 0 || input->isEnd())
    return false;

  if (input->seek(12,librevenge::RVNG_SEEK_SET) != 0) return false;

  for (int i = 0; i < 3; i++) {
    val = (int)(int) input->readLong(1);
    if (val < -10 || val > 10) {
      MWAW_DEBUG_MSG(("MsWks3Parser::checkHeader: find odd val%d=0x%x: not implemented\n", i, val));
      numError++;
    }
  }
  input->seek(1,librevenge::RVNG_SEEK_CUR);
  int type = (int) input->readLong(2);
  switch (type) {
  // Text document
  case 1:
    break;
  case 2:
    m_state->m_docType = MWAWDocument::MWAW_K_DATABASE;
    break;
  case 3:
    m_state->m_docType = MWAWDocument::MWAW_K_SPREADSHEET;
    break;
  case 12:
    m_state->m_docType = MWAWDocument::MWAW_K_DRAW;
    break;
  default:
    MWAW_DEBUG_MSG(("MsWks3Parser::checkHeader: find odd type=%d: not implemented\n", type));
    return false;
  }

#ifndef DEBUG
  // I have never seen this file, so...
  if (strict && version() == 1 && m_state->m_docType != MWAWDocument::MWAW_K_TEXT)
    return false;

  if (m_state->m_docType != MWAWDocument::MWAW_K_TEXT)
    return false;

  if (version() < 1 || version() > 3)
    return false;
#endif

  // ok, we can finish initialization
  MWAWEntry headerZone;
  headerZone.setBegin(0);
  headerZone.setEnd(headerSize);
  headerZone.setType("FileHeader");
  m_listZones.push_back(headerZone);

  //
  input->seek(0,librevenge::RVNG_SEEK_SET);
  f << "FileHeader: ";
  f << "version= " << input->readULong(4);
  long dim[4];
  for (int i = 0; i < 4; i++) dim[i] = input->readLong(2);
  if (dim[2] <= dim[0] || dim[3] <= dim[1]) {
    MWAW_DEBUG_MSG(("MsWks3Parser::checkHeader: find odd bdbox\n"));
    numError++;
  }
  f << ", windowdbdbox?=(";
  for (int i = 0; i < 4; i++) f << dim[i]<<",";
  f << "),";
  for (int i = 0; i < 4; i++) {
    val = (int) input->readULong(1);
    if (!val) continue;
    f << "##v" << i << "=" << std::hex << val <<",";
  }
  type = (int) input->readULong(2);
  f << std::dec;
  switch (type) {
  case 1:
    f << "doc,";
    break;
  case 2:
    f << "database,";
    break; // with ##v3=50
  case 3:
    f << "spreadsheet,";
    break; // with ##v2=5,##v3=6c
  case 12:
    f << "draw,";
    break;
  default:
    f << "###type=" << type << ",";
    break;
  }
  f << "numlines?=" << input->readLong(2) << ",";
  val = (int) input->readLong(1); // 0, v2: 0, 4 or -4
  if (val)  f << "f0=" << val << ",";
  val = (int) input->readLong(1); // almost always 1
  if (val != 1) f << "f1=" << val << ",";
  for (int i = 11; i < headerSize/2; i++) { // v1: 0, 0, v2: 0, 0|1
    val = (int) input->readULong(2);
    if (!val) continue;
    f << "f" << i << "=" << std::hex << val << std::dec;
    if (m_state->IsTextDoc() && version() >= 3 && i == 12) {
      if (val & 0x100) {
        m_state->m_hasHeader = true;
        f << "(Head)";
      }
      if (val & 0x200) {
        m_state->m_hasFooter = true;
        f << "(Foot)";
      }
    }
    f << ",";
  }

  if (header)
    header->reset(MWAWDocument::MWAW_T_MICROSOFTWORKS, version(), m_state->m_docType);

  m_zone->ascii().addPos(0);
  m_zone->ascii().addNote(f.str().c_str());
  m_zone->ascii().addPos(headerSize);

  input->seek(headerSize,librevenge::RVNG_SEEK_SET);
  return strict ? (numError==0) : (numError < 3);
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool MsWks3Parser::readDocumentInfo()
{
  MWAWInputStreamPtr input = m_zone->getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;

  if (input->readLong(1) != 2)
    return false;

  int vers = version();
  int docId = (int) input->readULong(1);
  int docExtra = (int) input->readULong(1);
  int flag = (int) input->readULong(1);
  long sz = (long) input->readULong(2);
  long endPos = pos+6+sz;
  if (!input->checkPosition(endPos))
    return false;

  int expectedSz = vers<=2 ? 0x15e : 0x9a;
  if (sz < expectedSz) {
    if (sz < 0x78+8) {
      MWAW_DEBUG_MSG(("MsWks3Parser::readDocumentInfo: size is too short\n"));
      return false;
    }
    MWAW_DEBUG_MSG(("MsWks3Parser::readDocumentInfo: size is too short: try to continue\n"));
  }

  f << "Entries(DocInfo):";
  if (docId) f << "id=0x"<< std::hex << docId << ",";
  if (docExtra) f << "unk=" << docExtra << ","; // in v3: find 3, 7, 1x
  if (flag) f << "fl=" << flag << ","; // in v3: find 80, 84, e0
  m_zone->ascii().addPos(pos);
  m_zone->ascii().addNote(f.str().c_str());

  if (!readPrintInfo()) {
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    return true;
  }

  pos = input->tell();
  if (sz < 0x9a) {
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    return true;
  }
  pos = input->tell();
  f.str("");
  f << "DocInfo-1:";
  int val = (int) input->readLong(2);
  if ((val & 0x0400) && vers >= 3) {
    f << "titlepage,";
    val &= 0xFBFF;
  }
  if (val) f << "unkn=" << val << ",";
  if (vers <= 2) {
    for (int wh = 0; wh < 2; wh++) {
      long debPos = input->tell();
      std::string name(wh==0 ? "header" : "footer");
      std::string text = m_textParser->readHeaderFooterString(wh==0);
      if (text.size()) f << name << "="<< text << ",";

      long remain = debPos+100 - input->tell();
      for (long i = 0; i < remain; i++) {
        unsigned char c = (unsigned char) input->readULong(1);
        if (c == 0) continue;
        f << std::dec << "f"<< i << "=" << (int) c << ",";
      }
    }
    f << "defFid=" << input->readULong(2) << ",";
    f << "defFsz=" << input->readULong(2)/2 << ",";
    val = (int) input->readULong(2); // 0 or 8
    if (val) f << "#unkn=" << val << ",";
    int dim[2];
    for (int i = 0; i < 2; i++) dim[i] = (int) input->readULong(2);
    f << "dim=" << dim[0] << "x" << dim[1] << ",";
    /* followed by 0 (v1) or 0|0x21|0* (v2)*/
    m_zone->ascii().addPos(pos);
    m_zone->ascii().addNote(f.str().c_str());
    pos = input->tell();
    f.str("");
    f << "DocInfo-2:";
  }

  // last data ( normally 26)
  int numData = int((endPos - input->tell())/2);
  for (int i = 0; i < numData; i++) {
    val = (int) input->readLong(2);
    switch (i) {
    case 2:
      if (val!=1) f << "firstPageNumber=" << val << ",";
      break;
    case 3:
      if (val!=1) f << "firstNoteNumber=" << val << ",";
      break;
    default:
      if (val)
        f << "g" << i << "=" << val << ",";
      break;
    }
  }
  m_zone->ascii().addPos(pos);
  m_zone->ascii().addNote(f.str().c_str());

  input->seek(endPos, librevenge::RVNG_SEEK_SET);

  return true;
}

////////////////////////////////////////////////////////////
// read a basic zone info
////////////////////////////////////////////////////////////
bool MsWks3Parser::readGroup(MsWks3ParserInternal::Zone &zone, MWAWEntry &entry, int check)
{
  entry = MWAWEntry();
  MWAWInputStreamPtr input=m_zone->getInput();
  if (input->isEnd()) return false;

  long pos = input->tell();
  if (input->readULong(1) != 3) return false;

  libmwaw::DebugStream f;
  int docId = (int) input->readULong(1);
  int docExtra = (int) input->readULong(1);
  int flag = (int) input->readULong(1);
  long size = (long) input->readULong(2)+6;

  int blockSize = version() <= 2 ? 340 : 360;
  if (size < blockSize) return false;

  f << "Entries(GroupHeader):";
  if (docId) f << "id=0x"<< std::hex << docId << std::dec << ",";
  if (docExtra) f << "unk=" << docExtra << ",";
  if (flag) f << "fl=" << flag << ",";
  if (size != blockSize)
    f << "end=" << std::hex << pos+size << std::dec << ",";

  entry.setBegin(pos);
  entry.setLength(size);
  entry.setType("GroupHeader");

  if (!input->checkPosition(entry.end())) {
    if (!input->checkPosition(pos+blockSize)) {
      MWAW_DEBUG_MSG(("MsWks3Parser::readGroup: can not determine group %d size \n", docId));
      return false;
    }
    entry.setLength(blockSize);
  }

  if (check <= 0) return true;
  input->seek(pos+8, librevenge::RVNG_SEEK_SET);
  for (int i = 0; i < 52; i++) {
    int v = (int) input->readLong(2);
    if (i < 8 && (v < -100 || v > 100)) return false;
    if (v) {
      f << "f" << i << "=";
      if (v > 0 && v < 1000)
        f << v;
      else
        f << std::hex << "X" << v << std::dec;
      f << ",";
    }
  }
  m_zone->ascii().addPos(pos);
  m_zone->ascii().addNote(f.str().c_str());

  pos = pos+blockSize;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int N=(int) input->readLong(2);

  f.str("");
  f << "GroupHeader:N=" << N << ",";
  m_zone->ascii().addPos(pos);
  m_zone->ascii().addNote(f.str().c_str());

#if 0
  if (check < 99) return true;
  if (m_state->m_docId >= 0 && docId != m_state->m_docId)
    MWAW_DEBUG_MSG(("MsWks3Parser::readGroup: find a different zone id 0x%x: not implemented\n", docId));
  else
    m_state->m_docId = docId;
#endif
  MWAWEntry pictZone;
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    if (m_graphParser->getEntryPicture(zone.m_zoneId, pictZone)>=0)
      continue;
    MWAW_DEBUG_MSG(("MsWks3Parser::readGroup: can not find the end of group \n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    break;
  }
  if (input->tell() < entry.end()) {
    m_zone->ascii().addPos(input->tell());
    m_zone->ascii().addNote("Entries(GroupData)");
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  }

  return true;
}

////////////////////////////////////////////////////////////
// read a header/footer zone info
////////////////////////////////////////////////////////////
bool MsWks3Parser::readGroupHeaderInfo(bool header, int check)
{
  if (version() < 3) return false;

  MWAWInputStreamPtr input=m_zone->getInput();
  long debPos = input->tell();

  long ptr = (long) input->readULong(2);
  if (input->isEnd()) return false;
  if (ptr) {
    if (check == 49) return false;
    if (check == 99) {
      MWAW_DEBUG_MSG(("MsWks3Parser::readGroupHeaderInfo: find ptr=0x%lx\n", ptr));
    }
  }

  libmwaw::DebugStream f;

  int size = (int) input->readLong(2)+4;
  int realSize = 0x11;
  if (size < realSize) return false;
  if (input->readLong(2) != 0) return false;
  f << "Entries(GroupHInfo)";
  if (header)
    f << "[header]";
  else
    f << "[footer]";
  f << ": size=" << std::hex << size << std::dec << " BTXT";

  if (!input->checkPosition(debPos+size)) return false;

  input->seek(debPos+6, librevenge::RVNG_SEEK_SET);
  int N=(int) input->readLong(2);
  f << ", N=" << N;
  int dim[4];
  for (int i = 0; i < 4; i++)
    dim[i] = (int) input->readLong(2);

  Box2i box(Vec2i(dim[1], dim[0]), Vec2i(dim[3], dim[2]));
  if (box.size().x() < -2000 || box.size().y() < -2000 ||
      box.size().x() > 2000 || box.size().y() > 2000 ||
      box.min().x() < -200 || box.min().y() < -200) return false;
  if (check == 49 && box.size().x() == 0 &&  box.size().y() == 0) return false;
  f << ", BDBox =" << box;
  int val = (int) input->readULong(1);
  if (val) f << ", flag=" << val;

  input->seek(debPos+size, librevenge::RVNG_SEEK_SET);
  if (check < 99) return true;
  if (header) m_state->m_headerHeight = box.size().y();
  else m_state->m_footerHeight = box.size().y();
  MsWks3ParserInternal::Zone::Type type=
    header ? MsWks3ParserInternal::Zone::HEADER : MsWks3ParserInternal::Zone::FOOTER;
  MsWks3ParserInternal::Zone zone(type, int(m_state->m_zoneMap.size()));

  m_zone->ascii().addPos(debPos);
  m_zone->ascii().addNote(f.str().c_str());
  m_zone->ascii().addPos(input->tell());

  input->seek(debPos+realSize, librevenge::RVNG_SEEK_SET);
  input->pushLimit(debPos+size);
  bool limitSet = true;
  for (int i = 0; i < N; i++) {
    long pos = input->tell();
    if (limitSet && pos==debPos+size) {
      limitSet = false;
      input->popLimit();
    }
    if (readZone(zone)) continue;
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    zone.m_textId = m_textParser->createZones(N-i, false);
    if (zone.m_textId >= 0)
      break;
    MWAW_DEBUG_MSG(("MsWks3Parser::readGroupHeaderInfo: can not find end of group\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
  }
  if (limitSet) input->popLimit();
  if (long(input->tell()) < debPos+size) {
    m_zone->ascii().addPos(input->tell());
    m_zone->ascii().addNote("GroupHInfo-II");

    input->seek(debPos+size, librevenge::RVNG_SEEK_SET);

    m_zone->ascii().addPos(debPos + size);
    m_zone->ascii().addNote("_");
  }
  //  m_graphParser->addDeltaToPositions(zone.m_zoneId, -1*box[0]);
  if (m_state->m_zoneMap.find(int(type)) != m_state->m_zoneMap.end()) {
    MWAW_DEBUG_MSG(("MsWks3Parser::readGroupHeaderInfo: the zone already exists\n"));
  }
  else
    m_state->m_zoneMap.insert(std::map<int,MsWks3ParserInternal::Zone>::value_type(int(type),zone));
  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool MsWks3Parser::readPrintInfo()
{
  MWAWInputStreamPtr input = m_zone->getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;
  // print info
  libmwaw::PrinterInfo info;
  if (!input->checkPosition(pos+0x78+8) || !info.read(input)) return false;
  f << "Entries(PrintInfo):"<< info;

  Vec2i paperSize = info.paper().size();
  Vec2i pageSize = info.page().size();
  if (pageSize.x() <= 0 || pageSize.y() <= 0 ||
      paperSize.x() <= 0 || paperSize.y() <= 0) return false;

  // now read the margin
  int margin[4];
  int maxSize = paperSize.x() > paperSize.y() ? paperSize.x() : paperSize.y();
  f << ", margin=(";
  for (int i = 0; i < 4; i++) {
    margin[i] = int(72.f/120.f*(float)input->readLong(2));
    if (margin[i] < -maxSize || margin[i] > maxSize) return false;
    f << margin[i];
    if (i != 3) f << ", ";
  }
  f << ")";

  // fixme: compute the real page length here...
  // define margin from print info
  Vec2i lTopMargin(margin[0],margin[1]), rBotMargin(margin[2],margin[3]);
  lTopMargin += paperSize - pageSize;

  int leftMargin = lTopMargin.x();
  int topMargin = lTopMargin.y();

  // decrease a little right and bottom margins Margin
  int rightMarg = rBotMargin.x()-50;
  if (rightMarg < 0) {
    leftMargin -= (-rightMarg);
    if (leftMargin < 0) leftMargin=0;
    rightMarg=0;
  }
  int botMarg = rBotMargin.y()-50;
  if (botMarg < 0) {
    topMargin -= (-botMarg);
    if (topMargin < 0) topMargin=0;
    botMarg=0;
  }

  getPageSpan().setMarginTop(topMargin/72.0);
  getPageSpan().setMarginBottom(botMarg/72.0);
  getPageSpan().setMarginLeft(leftMargin/72.0);
  getPageSpan().setMarginRight(rightMarg/72.0);
  getPageSpan().setFormLength(paperSize.y()/72.);
  getPageSpan().setFormWidth(paperSize.x()/72.);

  m_zone->ascii().addPos(pos);
  m_zone->ascii().addNote(f.str().c_str());
  input->seek(pos+0x78+8, librevenge::RVNG_SEEK_SET);

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
