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
#include <map>
#include <set>
#include <sstream>
#include <string>

#include <librevenge/librevenge.h>

#include "MWAWCell.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWList.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWSubDocument.hxx"

#include "HMWJGraph.hxx"
#include "HMWJText.hxx"

#include "HMWJParser.hxx"

/** Internal: the structures of a HMWJParser */
namespace HMWJParserInternal
{
////////////////////////////////////////
//! Internal: the state of a HMWJParser
struct State {
  //! constructor
  State() : m_zonesListBegin(-1), m_zonesMap(), m_zonesIdList(),
    m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0),
    m_headerId(0), m_footerId(0) {
  }

  //! the list of zone begin
  long m_zonesListBegin;
  //! a map of entry: filepos->zone
  std::map<long, MWAWEntry> m_zonesMap;
  //! an internal flag, used to know the actual id of a zone
  std::vector<int> m_zonesIdList;

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;
  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
  /** the header text zone id or 0*/
  long m_headerId;
  /** the footer text zone id or 0*/
  long m_footerId;
};

////////////////////////////////////////
//! Internal: the subdocument of a MWParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(HMWJParser &pars, MWAWInputStreamPtr input, long zoneId) :
    MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const {
    if (MWAWSubDocument::operator!=(doc)) return true;
    SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
    if (!sDoc) return true;
    if (m_id != sDoc->m_id) return true;
    return false;
  }

  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }

  //! returns the subdocument \a id
  long getId() const {
    return m_id;
  }
  //! sets the subdocument \a id
  void setId(long vid) {
    m_id = vid;
  }

  //! the parser function
  void parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  //! the subdocument id
  long m_id;
};

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("HMWJParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  if (type != libmwaw::DOC_HEADER_FOOTER) {
    MWAW_DEBUG_MSG(("HMWJParserInternal::SubDocument::parse: unexpected document type\n"));
    return;
  }

  assert(m_parser);
  long pos = m_input->tell();
  reinterpret_cast<HMWJParser *>(m_parser)->sendText(m_id, 0);
  m_input->seek(pos, RVNG_SEEK_SET);
}
}

////////////////////////////////////////////////////////////
// constructor/destructor + basic interface ...
////////////////////////////////////////////////////////////
HMWJParser::HMWJParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWParser(input, rsrcParser, header), m_state(), m_graphParser(), m_textParser()
{
  init();
}

HMWJParser::~HMWJParser()
{
}

void HMWJParser::init()
{
  resetListener();
  setAsciiName("main-1");

  m_state.reset(new HMWJParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);

  m_graphParser.reset(new HMWJGraph(*this));
  m_textParser.reset(new HMWJText(*this));
}

bool HMWJParser::sendText(long id, long cPos, bool asGraphic)
{
  return m_textParser->sendText(id, cPos, asGraphic);
}

bool HMWJParser::canSendTextAsGraphic(long id, long cPos)
{
  return m_textParser->canSendTextAsGraphic(id, cPos);
}

bool HMWJParser::sendZone(long zId)
{
  MWAWPosition pos(Vec2i(0,0), Vec2i(0,0), RVNG_POINT);
  pos.setRelativePosition(MWAWPosition::Char);
  return m_graphParser->sendFrame(zId, pos);
}

bool HMWJParser::getColor(int colId, int patternId, MWAWColor &color) const
{
  return m_graphParser->getColor(colId, patternId, color);
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
Vec2f HMWJParser::getPageLeftTop() const
{
  return Vec2f(float(getPageSpan().getMarginLeft()),
               float(getPageSpan().getMarginTop()+m_state->m_headerHeight/72.0));
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void HMWJParser::newPage(int number)
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

bool HMWJParser::readClassicHeader(HMWJZoneHeader &header, long endPos)
{
  header=HMWJZoneHeader(header.m_isMain);
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  header.m_length = (long) input->readULong(4);
  long headerEnd=pos+4+header.m_length;

  if ((endPos>0&&headerEnd>endPos) || (endPos<0&&!input->checkPosition(headerEnd)))
    return false;
  header.m_n = (int) input->readLong(2);
  header.m_values[0]=(int) input->readLong(2);
  header.m_fieldSize=(int) input->readLong(2);
  if (header.m_length < 16+header.m_n*header.m_fieldSize)
    return false;
  for (int i = 0; i < 3; i++)
    header.m_values[i+1]=(int) input->readLong(2);
  header.m_id=(long) input->readULong(4);
  return true;
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void HMWJParser::parse(RVNGTextInterface *docInterface)
{
  assert(getInput().get() != 0);

  if (!checkHeader(0L)) throw(libmwaw::ParseException());
  bool ok = true;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());

    checkHeader(0L);
    ok = createZones();
    if (ok) {
      createDocument(docInterface);
      std::vector<long> tokenIds = m_textParser->getTokenIdList();
      m_graphParser->sendPageGraphics(tokenIds);
      m_textParser->sendMainText();
#ifdef DEBUG
      m_textParser->flushExtra();
      m_graphParser->flushExtra();
#endif
    }
    ascii().reset();
  } catch (...) {
    MWAW_DEBUG_MSG(("HMWJParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void HMWJParser::createDocument(RVNGTextInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getListener()) {
    MWAW_DEBUG_MSG(("HMWJParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  int numPage = m_textParser->numPages();
  if (m_graphParser->numPages() > numPage)
    numPage = m_graphParser->numPages();
  m_state->m_numPages = numPage;

  // create the page list
  MWAWPageSpan ps(getPageSpan());
  if (m_state->m_headerId) {
    MWAWHeaderFooter header(MWAWHeaderFooter::HEADER, MWAWHeaderFooter::ALL);
    header.m_subDocument.reset(new HMWJParserInternal::SubDocument(*this, getInput(), m_state->m_headerId));
    ps.setHeaderFooter(header);
  }
  if (m_state->m_footerId) {
    MWAWHeaderFooter footer(MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
    footer.m_subDocument.reset(new HMWJParserInternal::SubDocument(*this, getInput(), m_state->m_footerId));
    ps.setHeaderFooter(footer);
  }
  ps.setPageSpan(m_state->m_numPages+1);
  std::vector<MWAWPageSpan> pageList(1,ps);

  //
  MWAWContentListenerPtr listen(new MWAWContentListener(*getParserState(), pageList, documentInterface));
  setListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool HMWJParser::createZones()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  if (!readHeaderEnd())
    input->seek(pos+34, RVNG_SEEK_SET);
  if (!readZonesList())
    return false;
  m_state->m_zonesIdList.clear();
  m_state->m_zonesIdList.resize(16,0);
  /* some zones do not seem to appear in this list, so we must track them */
  std::map<long,MWAWEntry>::iterator it;
  std::vector<MWAWEntry> newEntriesList;
  for (it = m_state->m_zonesMap.begin()  ; it != m_state->m_zonesMap.end(); ++it) {
    MWAWEntry const &entry = it->second;
    if (!entry.valid()) continue;
    if (m_state->m_zonesMap.find(entry.end()) != m_state->m_zonesMap.end())
      continue;

    MWAWEntry newEntry;
    newEntry.setBegin(entry.end());
    while (checkEntry(newEntry)) {
      if (!newEntry.valid()) break;
      newEntriesList.push_back(newEntry);

      long newBeginPos=newEntry.end();
      if (m_state->m_zonesMap.find(newBeginPos) != m_state->m_zonesMap.end())
        break;
      newEntry=MWAWEntry();
      newEntry.setBegin(newBeginPos);
    }
  }
  for (size_t i=0; i < newEntriesList.size(); i++) {
    MWAWEntry const &zone=newEntriesList[i];
    if (!zone.valid())
      continue;
    m_state->m_zonesMap.insert
    (std::map<long,MWAWEntry>::value_type(zone.begin(),zone));
  }

  // now parse the different zones
  for (it =  m_state->m_zonesMap.begin(); it != m_state->m_zonesMap.end(); ++it) {
    if (it->second.begin()<=0) continue;
    readZone(it->second);
  }

  // retrieve the text type, look for header/footer and pass information to text parser
  std::map<long,int> idTypeMap = m_graphParser->getTextFrameInformations();
  std::map<long,int>::const_iterator typeIt=idTypeMap.begin();
  for ( ; typeIt!=idTypeMap.end() ; ++typeIt) {
    if (typeIt->second==1)
      m_state->m_headerId = typeIt->first;
    else if (typeIt->second==2)
      m_state->m_footerId = typeIt->first;
  }
  m_textParser->updateTextZoneTypes(idTypeMap);

  // and the footnote
  long fntTextId;
  std::vector<long> fntFirstPosList;
  if (m_graphParser->getFootnoteInformations(fntTextId, fntFirstPosList))
    m_textParser->updateFootnoteInformations(fntTextId, fntFirstPosList);

  // finish graphparser preparation
  m_graphParser->prepareStructures();

  libmwaw::DebugStream f;
  for (it = m_state->m_zonesMap.begin(); it !=m_state->m_zonesMap.end(); ++it) {
    if (it->second.begin()<=0) continue;
    MWAWEntry const &zone = it->second;
    if (zone.isParsed()) continue;
    f.str("");
    f << "Entries(" << zone.name() << "):";
    ascii().addPos(zone.begin());
    ascii().addNote(f.str().c_str());
  }

  return m_state->m_zonesMap.size();
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////
bool HMWJParser::checkEntry(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  if (entry.begin()<=0 || !input->checkPosition(entry.begin()))
    return false;
  long pos = input->tell();
  input->seek(entry.begin(), RVNG_SEEK_SET);

  int type = (int) input->readULong(2);
  long val = input->readLong(2); // always 0?
  long length = (long) input->readULong(4);
  if (type >= 32 || length < 8 || !input->checkPosition(entry.begin()+length)) {
    input->seek(pos, RVNG_SEEK_SET);
    return false;
  }

  entry.setId(type);
  entry.setLength(length);

  static char const *(what[]) = {
    "FontDef", "Ruler", "Style", "FrameDef", "TZoneList",
    "TextZone", "Picture", "Table", "GraphData", "GroupData",
    "ZoneA", "ZoneB", "Section", "FtnDef", "ZoneE", "FontsName"
  };
  if (type <= 15)
    entry.setName(what[type]);
  else {
    std::stringstream s;
    s << "Zone" << std::hex << type << std::dec;
    entry.setName(s.str());
  }

  libmwaw::DebugStream f;
  f << "Entries(" << entry.name() << ":";
  if (val) f << "#unkn=" << val << ",";
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  input->seek(pos, RVNG_SEEK_SET);
  return true;
}

bool HMWJParser::readZonesList()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  if (!input->checkPosition(pos+82))
    return false;

  libmwaw::DebugStream f;
  f << "Entries(Zones):";
  for (int i = 0; i < 7; i++) { // f0=a000
    long val = (long) input->readULong(2);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "Zones(A):";

  for (int i = 0; i < 20; i++) {
    // sure up to Zonesb=FontNames Zones19=EOF
    long ptr = (long) input->readULong(4);
    if (!ptr) continue;
    if (!input->checkPosition(ptr))
      f << "###";
    else if (i != 19) { // i==19: is end of file
      MWAWEntry zone;
      zone.setBegin(ptr);
      if (checkEntry(zone))
        m_state->m_zonesMap.insert
        (std::map<long,MWAWEntry>::value_type(zone.begin(),zone));
      else
        f << "###";
    }
    f << "Zone" << i << "=" << std::hex << ptr << std::dec << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  ascii().addPos(input->tell());
  ascii().addNote("_");

  return m_state->m_zonesMap.size();
}

bool HMWJParser::readZone(MWAWEntry &entry)
{
  if (entry.begin()<=0) {
    MWAW_DEBUG_MSG(("HMWJParser::readZone: can not find the zone\n"));
    return false;
  }

  int localId = 0;
  if (entry.id() >= 0 && entry.id() <= 15)
    localId = m_state->m_zonesIdList[size_t(entry.id())]++;
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  long pos = entry.begin();
  input->seek(pos, RVNG_SEEK_SET);

  f << "Entries(" << entry.name() << "):";
  int type = (int) input->readULong(2); // number between 0 and f
  f << "type=" << type << ",";
  long val = input->readLong(2);
  if (val) f << "f0=" << val << ",";
  entry.setLength((long) input->readULong(4));
  if (entry.length() < 12 || !input->checkPosition(entry.end())) {
    MWAW_DEBUG_MSG(("HMWJParser::readZone: header seems to short\n"));
    return false;
  }
  entry.setParsed(true);

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  bool done = false;
  switch(entry.id()) {
  case 0:
    done = m_textParser->readFonts(entry);
    break;
  case 1:
    done = m_textParser->readParagraphs(entry);
    break;
  case 2:
    done = m_textParser->readStyles(entry);
    break;
  case 3:
    done = m_graphParser->readFrames(entry);
    break;
  case 4:
    done = m_textParser->readTextZonesList(entry);
    break;
  case 5:
    done = m_textParser->readTextZone(entry, localId);
    break;
  case 6:
    done = m_graphParser->readPicture(entry, localId);
    break;
  case 7:
    done = m_graphParser->readTable(entry, localId);
    break;
  case 8:
    done = m_graphParser->readGraphData(entry, localId);
    break;
  case 9:
    done = m_graphParser->readGroupData(entry, localId);
    break;
  case 10: // always 5 zones with N=0? the preference ?
    done = readZoneA(entry);
    break;
  case 11:
    done = readZoneB(entry);
    break;
  case 12:
    done = m_textParser->readSections(entry);
    break;
  case 13:
    done = m_textParser->readFtnPos(entry);
    break;
  case 15:
    done = m_textParser->readFontNames(entry);
    break;
  default:
    break;
  }

  if (done) return true;

  f.str("");
  f << entry.name() << "[data]:";
  ascii().addPos(pos+8);
  ascii().addNote(f.str().c_str());

  return true;
}


// read the print info data
bool HMWJParser::readPrintInfo(MWAWEntry const &entry)
{
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugFile &asciiFile = ascii();
  long pos = entry.begin();

  if (!input->checkPosition(entry.end())) {
    MWAW_DEBUG_MSG(("HMWJParser::readPrintInfo: the zone seems too short\n"));
    return false;
  }

  input->seek(pos, RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  entry.setParsed(true);

  f << "Entries(PrintInfo):";
  float margins[4] = {0,0,0,0}; // L, T, R, B
  int dim[4] = {0,0,0,0};
  long val;

  val = (long) input->readULong(2);
  if (val != 1) f << "firstSectNumber=" << val << ",";
  val = (long) input->readULong(2);
  if (val) f << "f0=" << val << ",";
  for (int i = 0; i < 4; i++) dim[i]=int(input->readLong(2));
  f << "paper=[" << dim[1] << "x" << dim[0] << " " << dim[3] << "x" << dim[2] << "],";
  f << "margins?=[";
  for (int i= 0; i < 4; i++) {
    margins[i] = float(input->readLong(4))/65536.f;
    f << margins[i] << ",";
  }
  f << "],";

  // after unknown
  asciiFile.addDelimiter(input->tell(),'|');
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  pos += 44;
  input->seek(pos, RVNG_SEEK_SET);
  f.str("");
  f << "PrintInfo(B):";

  // print info
  libmwaw::PrinterInfo info;
  if (!info.read(input)) {
    f << "###";
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    return false;
  }

  Vec2i paperSize = info.paper().size();
  Vec2i pageSize = info.page().size();

  bool useDocInfo = (dim[3]-dim[1]>margins[2]+margins[0]) &&
                    (dim[2]-dim[0]>margins[2]+margins[0]);
  bool usePrintInfo = pageSize.x() > 0 && pageSize.y() > 0 &&
                      paperSize.x() > 0 && paperSize.y() > 0;

  Vec2f lTopMargin(margins[0],margins[1]), rBotMargin(margins[2],margins[3]);
  // define margin from print info
  if (useDocInfo)
    paperSize = Vec2i(dim[3]-dim[1],dim[2]-dim[0]);
  else if (usePrintInfo) {
    lTopMargin= Vec2f(-float(info.paper().pos(0)[0]), -float(info.paper().pos(0)[1]));
    rBotMargin=info.paper().pos(1) - info.page().pos(1);

    // move margin left | top
    float decalX = lTopMargin.x() > 14 ? 14 : 0;
    float decalY = lTopMargin.y() > 14 ? 14 : 0;
    lTopMargin -= Vec2f(decalX, decalY);
    rBotMargin += Vec2f(decalX, decalY);
  }

  // decrease right | bottom
  float rightMarg = rBotMargin.x() -10;
  if (rightMarg < 0) rightMarg=0;
  float botMarg = rBotMargin.y() -50;
  if (botMarg < 0) botMarg=0;

  if (useDocInfo || usePrintInfo) {
    getPageSpan().setMarginTop(lTopMargin.y()/72.0);
    getPageSpan().setMarginBottom(botMarg/72.0);
    getPageSpan().setMarginLeft(lTopMargin.x()/72.0);
    getPageSpan().setMarginRight(rightMarg/72.0);
    getPageSpan().setFormLength(paperSize.y()/72.);
    getPageSpan().setFormWidth(paperSize.x()/72.);

    f << info;
  } else
    f << "###";

  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  if (input->tell()!=entry.end()) {
    asciiFile.addDelimiter(input->tell(),'|');
    input->seek(entry.end(), RVNG_SEEK_SET);
  }
  return true;
}

/* a unknown zone find always with N=0, so probably bad when N\neq 0 */
bool HMWJParser::readZoneA(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("HMWJParser::readZoneA: called without any entry\n"));
    return false;
  }
  if (entry.length() == 8) {
    MWAW_DEBUG_MSG(("HMWJParser::readZoneA: find an empty zone\n"));
    entry.setParsed(true);
    return true;
  }
  if (entry.length() < 12) {
    MWAW_DEBUG_MSG(("HMWJParser::readZoneA: the entry seems too short\n"));
    return false;
  }
  long pos = entry.begin()+8; // skip header
  long endPos = entry.end();

  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugFile &asciiFile = ascii();
  libmwaw::DebugStream f;
  entry.setParsed(true);
  input->seek(pos, RVNG_SEEK_SET);
  // first read the header (supposing that fieldSize=4)
  f << entry.name() << "[header]:";
  HMWJZoneHeader mainHeader(true);
  if (!readClassicHeader(mainHeader,endPos) ||
      (mainHeader.m_n && mainHeader.m_fieldSize!=4)) {
    MWAW_DEBUG_MSG(("HMWJParser::readZoneA: can not read an entry\n"));
    f << "###sz=" << mainHeader.m_length;
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    return false;
  }
  if (mainHeader.m_n != 0) {
    MWAW_DEBUG_MSG(("HMWJParser::readZoneA: Arggh, find unexpected N\n"));
    f << "###";
  }
  long headerEnd=pos+4+mainHeader.m_length;
  f << mainHeader;
  f << "listId=[" << std::hex;
  std::vector<long> listIds;
  for (int i = 0; i < mainHeader.m_n; i++) {
    long val = (long) input->readULong(4);
    listIds.push_back(val);
    f << val << ",";
  }
  f << std::dec << "],";
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  if (input->tell()!=headerEnd) {
    asciiFile.addDelimiter(input->tell(),'|');
    input->seek(headerEnd, RVNG_SEEK_SET);
  }

  // find now 5 zone with size 2a, 10, 24, 1ea, 10 or 0x38
  /* ZoneA-4:
     000000100000000000040000000a000100000000
     000000380002000800040000000a000100000000
       0000000400000006000a0000000000000000022c00000034004e15c400000000004e2100018d00b4
     size, N, ...???
   */
  long const expectedSize[]= {0x2a, 0x10, 0x24, 0x1ea, 0x10};
  for (int i = 0; i < 5; i++) {
    pos = input->tell();
    if (pos==endPos)
      return true;
    f.str("");
    f << entry.name() << "-" << i << ":";
    long dataSz = (long) input->readULong(4);
    long zoneEnd=pos+4+dataSz;

    if (zoneEnd>endPos) {
      MWAW_DEBUG_MSG(("HMWJParser::readZoneA: can not read an entry\n"));
      f << "###sz=" << dataSz;
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
      return false;
    }
    if (dataSz != expectedSize[i] && dataSz!=0) {
      MWAW_DEBUG_MSG(("HMWJParser::readZoneA: find unexpected size for zone %d\n", i));
      f << "###sz=" << dataSz;
    }
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(zoneEnd, RVNG_SEEK_SET);
  }
  pos = input->tell();
  if (pos!=endPos) {
    MWAW_DEBUG_MSG(("HMWJParser::readZoneA: find unexpected end data\n"));
    f.str("");
    f << entry.name() << "###:";
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
  }

  return true;
}

/* a unknown zone */
bool HMWJParser::readZoneB(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("HMWJParser::readZoneB: called without any entry\n"));
    return false;
  }
  if (entry.length() == 8) {
    MWAW_DEBUG_MSG(("HMWJParser::readZoneB: find an empty zone\n"));
    entry.setParsed(true);
    return true;
  }
  if (entry.length() < 12) {
    MWAW_DEBUG_MSG(("HMWJParser::readZoneB: the entry seems too short\n"));
    return false;
  }
  long pos = entry.begin()+8; // skip header
  long endPos = entry.end();

  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugFile &asciiFile = ascii();
  libmwaw::DebugStream f;
  entry.setParsed(true);
  input->seek(pos, RVNG_SEEK_SET);
  // first read the header (supposing that fieldSize=4)
  f << entry.name() << "[header]:";
  HMWJZoneHeader mainHeader(true);
  if (!readClassicHeader(mainHeader,endPos) ||
      (mainHeader.m_n && mainHeader.m_fieldSize!=44)) {
    MWAW_DEBUG_MSG(("HMWJParser::readZoneB: can not read an entry\n"));
    f << "###sz=" << mainHeader.m_length;
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    return false;
  }
  long headerEnd=pos+4+mainHeader.m_length;
  f << mainHeader;
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  for (int i = 0; i < mainHeader.m_n; i++) {
    pos = input->tell();
    f.str("");
    f << entry.name() << "-data" << i << ":";
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos+44, RVNG_SEEK_SET);
  }
  if (input->tell()!=headerEnd) {
    asciiFile.addDelimiter(input->tell(),'|');
    input->seek(headerEnd, RVNG_SEEK_SET);
  }

  for (int i = 0; i < mainHeader.m_n; i++) {
    pos = input->tell();
    f.str("");
    f << entry.name() << "-" << i << ":";
    long dataSz = (long) input->readULong(4);
    if (pos+4+dataSz>endPos) {
      MWAW_DEBUG_MSG(("HMWJParser::readZoneB: can not read an entry\n"));
      f << "###sz=" << dataSz;
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
      return false;
    }
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    if (dataSz) input->seek(dataSz, RVNG_SEEK_CUR);
  }
  pos = input->tell();
  if (pos!=endPos) {
    MWAW_DEBUG_MSG(("HMWJParser::readZoneB: find unexpected end data\n"));
    f.str("");
    f << entry.name() << "###:";
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
  }
  return true;
}

// a small unknown zone
bool HMWJParser::readHeaderEnd()
{
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugFile &asciiFile = ascii();
  long pos=input->tell();
  long endPos=pos+34;

  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("HMWJParser::readHeaderEnd: the zone seems too short\n"));
    return false;
  }

  libmwaw::DebugStream f;
  f << "Entries(HeaderEnd):";

  long val = input->readLong(4); // 1c58b4
  f << "dim?=" << float(val)/65536.f << ",";

  for (int i = 0; i < 4; i++) { // always 7,7,0,0
    val = input->readLong(2);
    if (!val) continue;
    f << "f" << i << "=" << val << ",";
  }
  val = input->readLong(4); // 2d5ab ~dim/10.
  f << "dim2?=" << float(val)/65536.f << ",";
  for (int i = 0; i < 4; i++) { // 0,4,0, 0
    val = (long) input->readULong(2);
    if (!val) continue;
    f << "g" << i << "=" << val << ",";
  }
  for (int i = 0; i < 4; i++) { // 1,1,1,0
    val = input->readLong(1);
    if (!val) continue;
    f << "h" << i << "=" << val << ",";
  }
  for (int i = 0; i < 3; i++) { // always 6,0,0
    val = input->readLong(2);
    if (!val) continue;
    f << "j" << i << "=" << val << ",";
  }

  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  if (input->tell()!=endPos) {
    asciiFile.addDelimiter(input->tell(),'|');
    input->seek(endPos, RVNG_SEEK_SET);
  }
  return true;
}

#ifdef DEBUG
bool HMWJParser::readZoneWithHeader(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("HMWJParser::readZoneWithHeader: called without any entry\n"));
    return false;
  }
  if (entry.length() < 12) {
    MWAW_DEBUG_MSG(("HMWJParser::readZoneWithHeader: the entry seems too short\n"));
    return false;
  }

  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugFile &asciiFile = ascii();
  libmwaw::DebugStream f;
  entry.setParsed(true);

  long pos = entry.begin()+8; // skip header
  long endPos = entry.end();
  input->seek(pos, RVNG_SEEK_SET);

  // first read the header
  f << entry.name() << "[header]:";
  HMWJZoneHeader mainHeader(false);
  if (!readClassicHeader(mainHeader,endPos)) {
    MWAW_DEBUG_MSG(("HMWJParser::readZoneWithHeader: can not read an entry\n"));
    f << "###sz=" << mainHeader.m_length;
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    return false;
  }
  long headerEnd=pos+4+mainHeader.m_length;
  f << mainHeader;

  int val;
  f << "unk=[";
  for (int i = 0; i < mainHeader.m_n; i++) {
    f << "[";
    for (int j=0; j < mainHeader.m_fieldSize; j++) {
      val = (int) input->readULong(1);
      if (val) f << std::hex << val << std::dec << ",";
      else f << "_,";
    }
    f << "]";
  }
  f << "],";
  if (input->tell()!=headerEnd) {
    asciiFile.addDelimiter(input->tell(),'|');
    input->seek(headerEnd, RVNG_SEEK_SET);
  }
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  for (int i = 0; i < mainHeader.m_n; i++) {
    pos = input->tell();
    f.str("");
    f << entry.name() << "-" << i << ":";
    long dataSz = (long) input->readULong(4);
    if (pos+4+dataSz>endPos) {
      MWAW_DEBUG_MSG(("HMWJParser::readZoneWithHeader: can not read an entry\n"));
      f << "###sz=" << dataSz;
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
      return false;
    }
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    if (dataSz) input->seek(dataSz, RVNG_SEEK_CUR);
  }
  asciiFile.addPos(endPos);
  asciiFile.addNote("_");
  if (input->tell()==endPos)
    return true;

  // often another list of data
  int i=0;
  while (!input->isEnd()) {
    pos = input->tell();
    if (pos==endPos)
      return true;
    f.str("");
    f << entry.name() << "-A" << i++ << ":";
    long dataSz = (long) input->readULong(4);
    if (pos+4+dataSz>endPos) {
      MWAW_DEBUG_MSG(("HMWJParser::readZoneWithHeader: can not read an entry\n"));
      f << "###sz=" << dataSz;
      asciiFile.addPos(pos);
      asciiFile.addNote(f.str().c_str());
      return false;
    }
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    if (dataSz) input->seek(dataSz, RVNG_SEEK_CUR);
  }
  return true;
}
#endif

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool HMWJParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = HMWJParserInternal::State();

  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork())
    return false;
  libmwaw::DebugStream f;
  f << "FileHeader:";
  long const headerSize=0x33c;
  if (!input->checkPosition(headerSize)) {
    MWAW_DEBUG_MSG(("HMWJParser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0,RVNG_SEEK_SET);
  int head[3];
  for (int i = 0; i < 3; i++)
    head[i] = (int) input->readULong(2);
  if (head[0] != 0x594c || head[1] != 0x5953 || head[2] != 0x100)
    return false;
  int val = (int) input->readLong(1);
  if (val==1) f << "hasPassword,";
  else if (val) {
    if (strict) return false;
    f << "#hasPassword=" << val << ",";
  }
  val = (int) input->readLong(1);
  if (val) {
    if (strict && (val<0||val>2)) return false;
    f << "f0=" << val << ",";
  }

  m_state->m_zonesListBegin = 0x460;
  for (int i = 0; i < 4; i++) { // always 0?
    val = (int) input->readLong(2);
    if (val) f << "f" << i+1 << "=" << val << ",";
  }
  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  long pos;
  // title, subject, author, revision, remark, [2 documents tags], mail:
  int fieldSizes[] = { 128, 128, 32, 32, 256, 36, 64, 64, 64 };
  for (int i = 0; i < 9; i++) {
    pos=input->tell();
    if (i == 5) {
      ascii().addPos(pos);
      ascii().addNote("FileHeader[DocTags]:");
      input->seek(pos+fieldSizes[i], RVNG_SEEK_SET);
      pos=input->tell();
      MWAWEntry printInfo;
      printInfo.setBegin(pos);
      printInfo.setLength(164);
      if (!readPrintInfo(printInfo))
        input->seek(pos+164, RVNG_SEEK_SET);

      pos=input->tell();
      ascii().addPos(pos);
      ascii().addNote("FileHeader[DocEnd]");
      input->seek(pos+60, RVNG_SEEK_SET);
      continue;
    }
    int fSz = (int) input->readULong(1);
    if (fSz >= fieldSizes[i]) {
      if (strict)
        return false;
      MWAW_DEBUG_MSG(("HMWJParser::checkHeader: can not read field size %i\n", i));
      ascii().addPos(pos);
      ascii().addNote("FileHeader#");
      input->seek(pos+fieldSizes[i], RVNG_SEEK_SET);
      continue;
    }
    f.str("");
    if (fSz == 0)
      f << "_";
    else {
      std::string name("");
      for (int c = 0; c < fSz; c++)
        name+=(char) input->readULong(1);
      f.str("");
      f << "FileHeader[field"<<i<< "]:" << name;
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+fieldSizes[i], RVNG_SEEK_SET);
  }

  pos=input->tell();
  f.str("");
  f << "FileHeader(B):"; // unknown 76 bytes
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(m_state->m_zonesListBegin, RVNG_SEEK_SET);
  if (header)
    header->reset(MWAWDocument::MWAW_T_HANMACWORDJ, 1);

  return true;
}

////////////////////////////////////////////////////////////
// code to uncompress a zone
////////////////////////////////////////////////////////////
/* implementation of a basic splay tree to decode a block
   freely inspired from: ftp://ftp.cs.uiowa.edu/pub/jones/compress/minunsplay.c :

   Author: Douglas Jones, Dept. of Comp. Sci., U. of Iowa, Iowa City, IA 52242.
   Date: Nov. 5, 1990.
         (derived from the Feb. 14 1990 version by stripping out irrelevancies)
         (minor revision of Feb. 20, 1989 to add exit(0) at end of program).
         (minor revision of Nov. 14, 1988 to detect corrupt input better).
         (minor revision of Aug. 8, 1988 to eliminate unused vars, fix -c).
   Copyright:  This material is derived from code Copyrighted 1988 by
         Jeffrey Chilton and Douglas Jones.  That code contained a copyright
         notice allowing copying for personal or research purposes, so long
         as copies of the code were not sold for direct commercial advantage.
         This version of the code has been stripped of most of the material
         added by Jeff Chilton, and this release of the code may be used or
         copied for any purpose, public or private.
   Patents:  The algorithm central to this code is entirely the invention of
         Douglas Jones, and it has not been patented.  Any patents claiming
         to cover this material are invalid.
   Exportability:  Splay-tree based compression algorithms may be used for
         cryptography, and when used as such, they may not be exported from
         the United States without appropriate approval.  All cryptographic
         features of the original version of this code have been removed.
   Language: C
   Purpose: Data uncompression program, a companion to minsplay.c
   Algorithm: Uses a splay-tree based prefix code.  For a full understanding
          of the operation of this data compression scheme, refer to the paper
          "Applications of Splay Trees to Data Compression" by Douglas W. Jones
          in Communications of the ACM, Aug. 1988, pages 996-1007.
*/
bool HMWJParser::decodeZone(MWAWEntry const &entry, RVNGBinaryData &dt)
{
  if (!entry.valid() || entry.length() <= 4) {
    MWAW_DEBUG_MSG(("HMWJParser::decodeZone: called with an invalid zone\n"));
    return false;
  }
  short const maxChar=256;
  short const maxSucc=maxChar+1;
  short const twoMaxChar=2*maxChar+1;
  short const twoMaxSucc=2*maxSucc;

  // first build the tree data
  short left[maxSucc];
  short right[maxSucc];
  short up[twoMaxSucc];
  for (short i = 0; i <= twoMaxChar; ++i)
    up[i] = i/2;
  for (short j = 0; j <= maxChar; ++j) {
    left[j] = short(2 * j);
    right[j] = short(2 * j + 1);
  }

  short const root = 0;
  short const sizeBit = 8;
  short const highBit=128; /* mask for the most sig bit of 8 bit byte */

  short bitbuffer = 0;       /* buffer to hold a byte for unpacking bits */
  short bitcounter = 0;  /* count of remaining bits in buffer */

  MWAWInputStreamPtr input = getInput();
  input->seek(entry.begin()+4, RVNG_SEEK_SET);
  dt.clear();
  while (!input->isEnd() && input->tell() < entry.end()) {
    short a = root;
    bool ok = true;
    do {  /* once for each bit on path */
      if(bitcounter == 0) {
        if (input->isEnd() || input->tell() >= entry.end()) {
          MWAW_DEBUG_MSG(("HMWJParser::decodeZone: find some uncomplete data for zone%lx\n", entry.begin()));
          dt.append((unsigned char)a);
          ok = false;
          break;
        }

        bitbuffer = (short) input->readULong(1);
        bitcounter = sizeBit;
      }
      --bitcounter;
      if ((bitbuffer & highBit) != 0)
        a = right[a];
      else
        a = left[a];
      bitbuffer = short(bitbuffer << 1);
    } while (a <= maxChar);
    if (!ok)
      break;
    dt.append((unsigned char)(a - maxSucc));

    /* now splay tree about leaf a */
    do {    /* walk up the tree semi-rotating pairs of nodes */
      short c;
      if ((c = up[a]) != root) {      /* a pair remains */
        short d = up[c];
        short b = left[d];
        if (c == b) {
          b = right[d];
          right[d] = a;
        } else
          left[d] = a;
        if (left[c] == a)
          left[c] = b;
        else
          right[c] = b;
        up[a] = d;
        up[b] = c;
        a = d;
      } else
        a = c;
    } while (a != root);
  }
  if (dt.size()==0) {
    MWAW_DEBUG_MSG(("HMWJParser::decodeZone: oops an empty zone\n"));
    return false;
  }

  ascii().skipZone(entry.begin()+4, entry.end()-1);
  return true;
}


// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
