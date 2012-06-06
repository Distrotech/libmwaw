/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libmwaw
 * Copyright (C) 2009, 2011 Alonso Laurent (alonso@loria.fr)
 * Copyright (C) 2006, 2007 Andrew Ziem
 * Copyright (C) 2004-2006 Fridrich Strba (fridrich.strba@bluewin.ch)
 * Copyright (C) 2004 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2003-2005 William Lachance (william.lachance@sympatico.ca)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 * For further information visit http://libmwaw.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>

#include <libwpd/WPXString.h>

#include "MWAWCell.hxx"
#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPrinter.hxx"

#include "FWText.hxx"

#include "FWParser.hxx"

/** Internal: the structures of a FWParser */
namespace FWParserInternal
{

////////////////////////////////////////
//! Internal: the state of a FWParser
struct State {
  //! constructor
  State() : m_version(-1), m_eof(-1), m_zoneList(), m_zoneFlagsList(), m_biblioId(-1),
    m_entryMap(), m_graphicMap(), m_actPage(0), m_numPages(0),
    m_headerHeight(0), m_footerHeight(0) {
    for (int i=0; i < 3; i++) m_zoneFlagsId[i] = -1;
  }

  //! the file version
  int m_version;

  //! the last file position
  long m_eof;

  //! the list of main zone flags id
  int m_zoneFlagsId[3];

  //! the list of zone position
  shared_ptr<FWEntry> m_zoneList;

  //! the list of zone flags
  shared_ptr<FWEntry> m_zoneFlagsList;

  //! the bibliography id
  int m_biblioId;

  //! zoneId -> entry
  std::multimap<int, shared_ptr<FWEntry> > m_entryMap;

  //! zoneId -> graphic entry
  std::multimap<int, shared_ptr<FWEntry> > m_graphicMap;

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a MWParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(FWParser &pars, MWAWInputStreamPtr input, int zoneId) :
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
  int getId() const {
    return m_id;
  }
  //! sets the subdocument \a id
  void setId(int vid) {
    m_id = vid;
  }

  //! the parser function
  void parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  //! the subdocument id
  int m_id;
};

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("SubDocument::parse: no listener\n"));
    return;
  }
  FWContentListener *listen = dynamic_cast<FWContentListener *>(listener.get());
  if (!listen) {
    MWAW_DEBUG_MSG(("SubDocument::parse: bad listener\n"));
    return;
  }
  assert(m_parser);

  long pos = m_input->tell();
  reinterpret_cast<FWParser *>(m_parser)->send(m_id);
  m_input->seek(pos, WPX_SEEK_SET);
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
FWParser::FWParser(MWAWInputStreamPtr input, MWAWHeader *header) :
  MWAWParser(input, header), m_listener(), m_convertissor(), m_state(),
  m_pageSpan(), m_textParser(), m_asciiFile(), m_asciiName("")
{
  init();
}

FWParser::~FWParser()
{
  std::multimap<int, shared_ptr<FWEntry> >::iterator it;
  for (it = m_state->m_entryMap.begin(); it != m_state->m_entryMap.end(); it++) {
    shared_ptr<FWEntry> zone = it->second;
    if (zone) zone->closeDebugFile();
  }
}

void FWParser::init()
{
  m_convertissor.reset(new MWAWFontConverter);
  m_listener.reset();
  m_asciiName = "main-1";

  m_state.reset(new FWParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  m_pageSpan.setMarginTop(0.1);
  m_pageSpan.setMarginBottom(0.1);
  m_pageSpan.setMarginLeft(0.1);
  m_pageSpan.setMarginRight(0.1);

  m_textParser.reset(new FWText(getInput(), *this, m_convertissor));
}

void FWParser::setListener(FWContentListenerPtr listen)
{
  m_listener = listen;
  m_textParser->setListener(listen);
}

int FWParser::version() const
{
  return m_state->m_version;
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
float FWParser::pageHeight() const
{
  return float(m_pageSpan.getFormLength()-m_pageSpan.getMarginTop()-m_pageSpan.getMarginBottom()-m_state->m_headerHeight/72.0-m_state->m_footerHeight/72.0);
}

float FWParser::pageWidth() const
{
  return float(m_pageSpan.getFormWidth()-m_pageSpan.getMarginLeft()-m_pageSpan.getMarginRight());
}


////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void FWParser::newPage(int number)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!m_listener || m_state->m_actPage == 1)
      continue;
    m_listener->insertBreak(MWAW_PAGE_BREAK);
  }
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void FWParser::parse(WPXDocumentInterface *docInterface)
{
  assert(getInput().get() != 0);

  if (!checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = true;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());

    checkHeader(0L);
    ok = createZones();
    if (ok) {
      createDocument(docInterface);
      m_textParser->sendMainText();
      flushExtra();
      if (m_listener) m_listener->endDocument();
      m_listener.reset();
    }
    bool first = true;
    std::multimap<int, shared_ptr<FWEntry> >::iterator it;
    libmwaw::DebugStream f;
    for (it = m_state->m_entryMap.begin(); it != m_state->m_entryMap.end(); it++) {
      shared_ptr<FWEntry> &zone = it->second;
      if (!zone || !zone->valid() || zone->isParsed()) continue;

      if (first) {
        first = false;
        MWAW_DEBUG_MSG(("FWParser::parse: find some unparsed zone!!!\n"));
      }
      f << "Entries(" << zone->type() << ")";
      if (zone->m_nextId != -2) f << "[" << zone->m_nextId << "]";

      libmwaw::DebugFile &asciiFile = zone->getAsciiFile();

      asciiFile.addPos(zone->begin());
      asciiFile.addNote(f.str().c_str());
      asciiFile.addPos(zone->end());
      asciiFile.addNote("_");

      zone->closeDebugFile();
    }

    ascii().reset();
  } catch (...) {
    MWAW_DEBUG_MSG(("FWParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void FWParser::createDocument(WPXDocumentInterface *documentInterface)
{
  if (!documentInterface) return;
  if (m_listener) {
    MWAW_DEBUG_MSG(("FWParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  std::vector<MWAWPageSpan> pageList;
  MWAWPageSpan ps(m_pageSpan);

  int numPage = m_textParser->numPages();
  m_state->m_numPages = numPage;
  for (int i = 0; i <= m_state->m_numPages; i++) pageList.push_back(ps);

  //
  FWContentListenerPtr listen(new FWContentListener(pageList, documentInterface));
  setListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool FWParser::createZones()
{
  std::multimap<int, shared_ptr<FWEntry> >::iterator it;
  if (m_state->m_zoneList)
    readZonePos(m_state->m_zoneList);
  if (m_state->m_zoneFlagsList)
    readZoneFlags(m_state->m_zoneFlagsList);

  for (it = m_state->m_entryMap.begin(); it != m_state->m_entryMap.end(); it++) {
    shared_ptr<FWEntry> &zone = it->second;
    if (!zone || !zone->valid() || zone->isParsed()) continue;
    if (zone->m_typeId >= 0) {
      if (readGraphic(zone)) continue;
      if (m_textParser->readTextData(zone)) continue;
      continue;
    } else if (zone->m_typeId == -1) {
      switch (zone->m_flagsId) {
      case 0:
        if (!m_textParser->readStyle(zone)) {
          MWAW_DEBUG_MSG(("FWParser::createZones: can not read the style zone\n"));
        }
        break;
      case 1:
        if (!readUnkn0(zone)) {
          MWAW_DEBUG_MSG(("FWParser::createZones: can not read the unk0 zone\n"));
        }
        break;
      case 2:
        if (!readDocInfo(zone)) {
          MWAW_DEBUG_MSG(("FWParser::createZones: can not read the document zone\n"));
        }
        break;
      default:
        if (zone->id() == m_state->m_biblioId) {
          MWAW_DEBUG_MSG(("FWParser::createZones: find a bibliography zone: unparsed\n"));
          zone->setType("Biblio");
        } else {
          MWAW_DEBUG_MSG(("FWParser::createZones: find unexpected general zone\n"));
        }
        break;
      }
      continue;
    }
  }
  m_textParser->sortZones();
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
bool FWParser::checkHeader(MWAWHeader *header, bool /*strict*/)
{
  *m_state = FWParserInternal::State();

  MWAWInputStreamPtr input = getInput();

  int const minSize=50;
  input->seek(minSize,WPX_SEEK_SET);
  if (int(input->tell()) != minSize) {
    MWAW_DEBUG_MSG(("FWParser::checkHeader: file is too short\n"));
    return false;
  }

  if (!readDocPosition())
    return false;

  input->seek(0,WPX_SEEK_SET);

  if (header)
    header->reset(MWAWDocument::FULLW, 1);

  return true;
}

bool FWParser::readDocInfo(shared_ptr<FWEntry> zone)
{
  if (zone->length() < 0x4b2)
    return false;
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->getAsciiFile();
  libmwaw::DebugStream f;

  long pos = zone->begin();
  input->seek(pos, WPX_SEEK_SET);
  int N = (int)input->readLong(2);
  if (!N) return false;
  input->seek(4, WPX_SEEK_CUR);
  int val = (int)input->readLong(2);
  if (N < val-2 || N > val+2) return false;
  input->seek(2, WPX_SEEK_CUR);
  int nC = (int)input->readULong(1);
  if (!nC || nC > 70) return false;
  for (int i = 0; i < nC; i++) {
    int c = (int)input->readULong(1);
    if (c < 0x20) return false;
  }

  zone->setParsed(true);
  input->seek(pos+2, WPX_SEEK_SET);
  f << "Entries(DocInfo)|" << *zone << ":";
  f << "N0=" << N << ",";
  f << "unkn0=" << std::hex << input->readULong(2) << std::dec << ","; // big number
  for (int i = 0; i < 2; i++) { // almost always ° + small number
    val = (int)input->readULong(1);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  val = (int)input->readLong(2); // almost always N
  if (val != N) f << "N1=" << val << ",";
  f << "unkn1=" << std::hex << input->readULong(2) << std::dec << ","; // big number
  std::string title("");
  nC = (int)input->readULong(1);
  for (int i = 0; i < nC; i++)
    title += (char) input->readLong(1);
  f << "title=" << title << ",";
  asciiFile.addDelimiter(input->tell(),'|');
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  pos += 10+70; // checkme
  input->seek(pos, WPX_SEEK_SET);
  f.str("");
  f << "DocInfo-A:";
  for (int i = 0; i < 4; i++) { // 0, 1, 2, 80
    val = (int)input->readULong(1);
    if (val)
      f << "fl" << i << "=" << std::hex << val << std::dec << ",";
  }
  val = (int)input->readLong(2); // almost always 0, but find also 53, 64, ba, f4
  if (val) f << "f0=" << val << ",";
  f << "unkn=["; // 2 big number which seems realted
  for (int i = 0; i < 2; i++)
    f << std::hex << input->readULong(4) << std::dec << ",";
  f << "],";
  for (int i = 4; i < 6; i++) { // always 1, 0 ?
    val = (int)input->readULong(1);
    if (val!=5-i) f << "fl" << i << "=" << std::hex << val << std::dec << ",";
  }
  // 5 relative big number : multiple of 4
  long ptr = (long) input->readULong(4);
  f << "unkn2=[" << std::hex << ptr << "," << std::dec;
  for (int i = 0; i < 4; i++)
    f << long(input->readULong(4))-ptr << ",";
  f << "],";
  val = (int)input->readLong(2); // almost always -2, but find also 6
  if (val != -2)
    f << "f1=" << val << ",";
  for (int i = 0; i < 2; i++) { // always 0,0
    val = (int)input->readULong(2);
    if (val) f << "f" << i+2 << "=" << std::hex << val << std::dec << ",";
  }
  for (int st=0; st < 2; st++) {
    long actPos = input->tell();
    val = (int)input->readULong(2); // big number
    if (val) f << "g" << st << "=" << std::hex << val << std::dec << ",";

    nC = (int)input->readULong(1);
    if (nC > 32) {
      MWAW_DEBUG_MSG(("FWParser::readDocInfo: can not read user name\n"));
      nC = 0;
    }
    std::string s("");
    for (int i = 0; i < nC; i++) s+=char(input->readLong(1));
    if (nC)
      f << "Username" << st << "=" << s << ",";
    input->seek(actPos+36, WPX_SEEK_SET);
  }

  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  pos = input->tell();
  asciiFile.addPos(pos);
  asciiFile.addNote("DocInfo-B");
  pos+=196;
  input->seek(pos, WPX_SEEK_SET);
  for (int i = 0; i < 6; i++) {
    pos = input->tell();
    f.str("");
    f << "DocInfo-C" << i << ":" << input->readLong(2);

    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos+48, WPX_SEEK_SET);
  }
  for (int i = 0; i < 9; i++) {
    pos = input->tell();
    f.str("");
    f << "DocInfo-D" << i << ":";

    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
    input->seek(pos+38, WPX_SEEK_SET);
  }
  pos = input->tell();
  asciiFile.addPos(pos);
  asciiFile.addNote("DocInfo-E");
  input->seek(pos+182, WPX_SEEK_SET);

  // this part in v1 and v2
  pos = input->tell();
  asciiFile.addPos(pos);
  asciiFile.addNote("DocInfo-F");
  if (version()==2) {
    input->seek(pos+436, WPX_SEEK_SET);
    pos = input->tell();
    asciiFile.addPos(pos);
    asciiFile.addNote("DocInfo-G");
    // if all is ok 3*[Name:4bytes, 0:4 bytes, sz:4 bytes, sz bytes ]
  }

  zone->closeDebugFile();
  return true;
}

bool FWParser::readUnkn0(shared_ptr<FWEntry> zone)
{
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->getAsciiFile();
  libmwaw::DebugStream f;

  long pos = zone->begin();
  input->seek(pos, WPX_SEEK_SET);
  int N = (int)input->readLong(2);
  if ((N & 0xF)||N<=0) return false;
  input->seek(pos+6, WPX_SEEK_SET);
  for (int i = 0; i < N-1; i++) {
    if (input->tell() >= zone->end())
      return false;
    long v = input->readLong(1);
    if (v==0) continue;
    if (v!=1) {
      if (2*i > N) {
        MWAW_DEBUG_MSG(("FWParser::readUnkn0: find only %d/%d entries\n", i, N));
        break;
      }
      return false;
    }
    input->seek(5, WPX_SEEK_CUR);
  }
  if (input->tell() > zone->end())
    return false;

  zone->setParsed(true);
  f << "Entries(Unkn0)|" << *zone <<":";
  f << "N=" << N << ",";
  input->seek(pos+2, WPX_SEEK_SET);
  f << "unkn=" << std::hex << input->readULong(4) << std::dec << ",";
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  for (int i = 0; i < N-1; i++) {
    pos = input->tell();
    long v = input->readLong(1);
    if (v==1)
      input->seek(pos+6, WPX_SEEK_SET);
    else if (v)
      break;
    f.str("");
    f << "Unkn0-" << i << ":";
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
  }
  if (input->tell() != zone->end()) {
    MWAW_DEBUG_MSG(("FWParser::readUnkn0: end seems odd\n"));
  }

  zone->closeDebugFile();
  return true;
}

////////////////////////////////////////////////////////////
// read/send a graphic zone
////////////////////////////////////////////////////////////
bool FWParser::readGraphic(shared_ptr<FWEntry> zone)
{
  int vers = version();

  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->getAsciiFile();
  libmwaw::DebugStream f;

  long pos = zone->begin();
  input->seek(pos, WPX_SEEK_SET);
  long sz = (long) input->readULong(4);
  int expectedSz = vers==1 ? 0x5c : 0x54;
  if (sz != expectedSz || pos+sz > zone->end()) return false;
  input->seek(sz, WPX_SEEK_CUR);
  f << "Entries(Graphic)";
  if (zone->m_nextId != -2) f << "[" << zone->m_nextId << "]";
  f << "|" << *zone << ":";
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());

  pos = input->tell();
  sz = (long) input->readULong(4);
  if (!sz || pos+4+sz > zone->end()) {
    MWAW_DEBUG_MSG(("FWParser::readGraphic: can not read graphic size\n"));
    return false;
  }
  f.str("");
  f << "Graphic:sz=" << std::hex << sz << std::dec << ",";
  asciiFile.addPos(pos);
  asciiFile.addNote(f.str().c_str());
  asciiFile.skipZone(pos+4, pos+4+sz-1);
  input->seek(sz, WPX_SEEK_CUR);

  m_state->m_graphicMap.insert
  (std::multimap<int, shared_ptr<FWEntry> >::value_type(zone->id(), zone));

  pos = input->tell();
  if (pos == zone->end())
    return true;

  sz = (long)input->readULong(4);
  if (sz)
    input->seek(sz, WPX_SEEK_CUR);
  if (pos+4+sz!=zone->end()) {
    MWAW_DEBUG_MSG(("FWParser::readGraphic: end graphic seems odds\n"));
  }
  asciiFile.addPos(pos);
  asciiFile.addNote("Graphic-A");

  asciiFile.addPos(input->tell());
  asciiFile.addNote("_");

  return true;
}

bool FWParser::sendGraphic(shared_ptr<FWEntry> zone)
{
  if (!m_listener) return true;
  zone->setParsed(true);

  // int vers = version();

  MWAWInputStreamPtr input = zone->m_input;

  long pos = zone->begin();
  input->seek(pos, WPX_SEEK_SET);
  int sz = (int)input->readULong(4);
  input->seek(sz, WPX_SEEK_CUR);

  // header
  pos = input->tell();
  sz = (int)input->readULong(4);

#ifdef DEBUG_WITH_FILES
  if (1) {
    WPXBinaryData file;
    input->seek(pos+4, WPX_SEEK_SET);
    input->readDataBlock(sz, file);
    static int volatile pictName = 0;
    libmwaw::DebugStream f;
    f << "DATA-" << ++pictName;
    libmwaw::Debug::dumpFile(file, f.str().c_str());
  }
#endif

  input->seek(pos+4, WPX_SEEK_SET);
  Box2f box;
  MWAWPict::ReadResult res =
    MWAWPictData::check(input, sz, box);
  if (res == MWAWPict::MWAW_R_BAD) {
    MWAW_DEBUG_MSG(("FWParser::sendGraphic: can not find the picture\n"));
    return false;
  }

  Vec2f actualSize, naturalSize;
  if (box.size().x() > 0 && box.size().y()  > 0) {
    actualSize = naturalSize = box.size();
  } else if (actualSize.x() <= 0 || actualSize.y() <= 0) {
    MWAW_DEBUG_MSG(("WPParser::sendGraphic: can not find the picture size\n"));
    actualSize = naturalSize = Vec2f(100,100);
  }
  MWAWPosition pictPos=MWAWPosition(Vec2f(0,0),actualSize, WPX_POINT);
  pictPos.setRelativePosition(MWAWPosition::Char);
  pictPos.setNaturalSize(naturalSize);

  input->seek(pos+4, WPX_SEEK_SET);
  shared_ptr<MWAWPict> pict
  (MWAWPictData::get(input, sz));
  if (pict) {
    WPXBinaryData data;
    std::string type;
    if (pict->getBinary(data,type)) {
      m_listener->insertPicture(pictPos, data, type);
      return true;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////
// read the zone flag and positions
////////////////////////////////////////////////////////////
bool FWParser::readZoneFlags(shared_ptr<FWEntry> zone)
{
  int vers = version();
  int dataSz = vers==1 ? 22 : 16;
  if (!zone || zone->length()%dataSz) {
    MWAW_DEBUG_MSG(("FWParser::readZoneFlags: size seems odd\n"));
    return false;
  }
  zone->setParsed(true);
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->getAsciiFile();

  libmwaw::DebugStream f;
  long numElt = zone->length()/dataSz;
  input->seek(zone->begin(), WPX_SEEK_SET);
  int numPosZone = 0;
  int numNegZone = 0;
  std::multimap<int, shared_ptr<FWEntry> >::iterator it;
  for (long i = 0; i < numElt; i++) {
    long pos = input->tell();
    int id = (int)input->readLong(2);
    it = m_state->m_entryMap.find(id);
    shared_ptr<FWEntry> entry;
    if (it == m_state->m_entryMap.end()) {
      if (id != -2) {
        MWAW_DEBUG_MSG(("FWParser::readZoneFlags: can not find entry %d\n",id));
      }
      entry.reset(new FWEntry(input));
      entry->setId(id);
    } else
      entry = it->second;
    f.str("");
    entry->setType("UnknownZone");
    int val = (int)input->readLong(2); // always -2 ?
    if (val != -2) f << "g0=" << val << ",";
    val  = (int)input->readLong(2); // always 0 ?
    if (val) f << "g1=" << val << ",";
    // a small number between 1 and 0x100
    entry->m_values[0] =  (int)input->readLong(2);
    for (int j = 0; j < 2; j++) { // always 0
      val  = (int)input->readLong(2);
      if (val) f << "g" << j+2 << "=" << val << ",";
    }
    /** -1: generic, -2: null, other fId */
    entry->m_typeId =  (int)input->readLong(2);
    if (entry->m_typeId >= 0)
      entry->m_flagsId = numPosZone++;
    else if (entry->m_typeId == -1) {
      bool find = false;
      for (int j = 0; j < 3; j++) {
        if (i!=m_state->m_zoneFlagsId[j]) continue;
        find = true;
        entry->m_flagsId=j;
        break;
      }
      if (!find) {
        MWAW_DEBUG_MSG(("FWParser::readZoneFlags: can not find generic zone id %ld\n",i));
        entry->m_flagsId = numNegZone;
        f << "#";
      }
      numNegZone++;
    }
    // v2: always  0|0x14, v1 two small number or 0x7FFF
    entry->m_values[1] =  (int)input->readLong(1);
    entry->m_values[2] =  (int)input->readLong(1);
    if (vers == 1) {
      for (int j = 0; j < 3; j++) { // always 0, -2|0, 0 ?
        val  = (int)input->readLong(2);
        if ((j==1 && val !=-2) || (j!=1 && val))
          f << "g" << j+4 << "=" << val << ",";
      }
    }

    std::string extra = f.str();
    f.str("");
    if (i==0) f << "Entries(ZoneFlags):";
    else f << "ZoneFlags-" << i << ":";
    f << *entry << ",";
    f << extra;

    if (entry->id() < 0) {
      if (entry->m_typeId != -2) {
        MWAW_DEBUG_MSG(("FWParser::readZoneFlags: find a null zone with unexpected type\n"));
      }
    }

    input->seek(pos+dataSz, WPX_SEEK_SET);
    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());
  }
  asciiFile.addPos(zone->end());
  asciiFile.addNote("Entries(ZoneAfter)");
  return true;
}

bool FWParser::readZonePos(shared_ptr<FWEntry> zone)
{
  int vers = version();
  int dataSz = vers==1 ? 10 : 8;
  if (zone->length()%dataSz) {
    MWAW_DEBUG_MSG(("FWParser::readZonePos: size seems odd\n"));
    return false;
  }
  zone->setParsed(true);
  MWAWInputStreamPtr input = zone->m_input;
  libmwaw::DebugFile &asciiFile = zone->getAsciiFile();

  libmwaw::DebugStream f;
  int numElt = int(zone->length()/dataSz);
  input->seek(zone->begin(), WPX_SEEK_SET);
  int val;

  // read data
  std::set<long> filePositions;
  std::vector<shared_ptr<FWEntry> > listEntry;
  if (numElt>0)
    listEntry.resize(size_t(numElt));
  for (int i = 0; i < numElt; i++) {
    long pos = input->tell();
    long fPos = input->readLong(4);

    shared_ptr<FWEntry> entry(new FWEntry(input));
    entry->setType("Unknown");
    entry->setId(i);
    entry->m_nextId = (int) input->readLong(2);
    int id = (int) input->readLong(2);
    if (fPos >= 0) {
      filePositions.insert(fPos);
      entry->setBegin(fPos);
    }
    f.str("");
    if (id != i+1 && id !=-2) f << "id2=" << id << ",";

    if (entry->begin()>=0)
      f << "pos=" << std::hex << entry->begin() << std::dec << ",";
    if (entry->m_nextId != -2) f << "nextId=" << entry->m_nextId << ",";
    if (vers==1) {
      val = (int) input->readLong(2);
      if (val) f << "f0=" << val << ",";
    }

    input->seek(pos+dataSz, WPX_SEEK_SET);

    entry->setExtra(f.str());
    f.str("");
    if (i == 0) f << "Entries(ZonePos):";
    else f << "ZonePos" << i << ":";
    f << *entry;

    asciiFile.addPos(pos);
    asciiFile.addNote(f.str().c_str());

    if (id != -2 && (id < 1 || id >= numElt)) {
      MWAW_DEBUG_MSG(("FWParser::readZonePos: entry id seems bad\n"));
    }
    listEntry[size_t(i)] = entry;
  }
  filePositions.insert(zone->begin());

  // compute end of each entry
  for (int i = 0; i < numElt; i++) {
    shared_ptr<FWEntry> entry = listEntry[size_t(i)];
    if (!entry || entry->begin() < 0)
      continue;
    std::set<long>::iterator it=filePositions.find(entry->begin());
    if (it == filePositions.end()) {
      MWAW_DEBUG_MSG(("FWParser::readZonePos: can not find my entry\n"));
      continue;
    }
    it++;
    if (it == filePositions.end()) {
      MWAW_DEBUG_MSG(("FWParser::readZonePos: can not find my entry\n"));
      continue;
    }

    entry->setEnd(*it);
    if (entry->m_nextId < 0) continue;
    if (entry->m_nextId >= numElt) {
      entry->m_nextId = -1;
      MWAW_DEBUG_MSG(("FWParser::readZonePos: can not find the next entry\n"));
      continue;
    }
    if (!listEntry[size_t(entry->m_nextId)] || listEntry[size_t(entry->m_nextId)]->isParsed()) {
      entry->m_nextId = -1;
      MWAW_DEBUG_MSG(("FWParser::readZonePos:  next entry %d is already used\n",
                      entry->m_nextId));
      continue;
    }
    listEntry[size_t(entry->m_nextId)]->setParsed(true);
  }

  for (int i = 0; i < numElt; i++) {
    shared_ptr<FWEntry> entry = listEntry[size_t(i)];
    if (!entry || !entry->valid() || entry->isParsed()) continue;

    m_state->m_entryMap.insert
    (std::multimap<int, shared_ptr<FWEntry> >::value_type(i, entry));

    if (entry->m_nextId < 0) {
      entry->m_input = input;
      entry->m_asciiFile = shared_ptr<libmwaw::DebugFile>
                           (&asciiFile, MWAW_shared_ptr_noop_deleter<libmwaw::DebugFile>());
      continue;
    }
    // ok we must reconstruct a file
    shared_ptr<FWEntry> actEnt = entry;
    WPXBinaryData &data = entry->m_data;
    while (1) {
      if (!actEnt->valid()) break;
      input->seek(actEnt->begin(), WPX_SEEK_SET);
      unsigned long read;
      const unsigned char *dt = input->read((size_t)actEnt->length(), read);
      data.append(dt, read);
      asciiFile.skipZone(actEnt->begin(), actEnt->end()-1);
      if (actEnt->m_nextId < 0) break;
      actEnt = listEntry[size_t(actEnt->m_nextId)];
      if (actEnt) actEnt->setParsed(true);
    }
    entry->update();
  }

  asciiFile.addPos(zone->end());
  asciiFile.addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// read the document header
////////////////////////////////////////////////////////////
bool FWParser::readDocPosition()
{
  MWAWInputStreamPtr input = getInput();
  if (m_state->m_eof < 0) {
    // find eof
    while (!input->atEOS())
      input->seek(1000, WPX_SEEK_CUR);
    m_state->m_eof = input->tell();
  }
  if (m_state->m_eof < 48)
    return false;

  libmwaw::DebugStream f;
  long pos = m_state->m_eof-48;
  input->seek(m_state->m_eof-48, WPX_SEEK_SET);
  f << "Entries(DocPosition):";

  long val;
  m_state->m_biblioId = (int) input->readLong(2);
  if (m_state->m_biblioId != -2)
    f << "bibId=" << m_state->m_biblioId << ",";
  for (int i = 0; i < 4; i++) { // always 0?
    val = input->readLong(2);
    if (val) f << "f" << i << "=" << val <<",";
  }
  long sz[2];
  for (int i = 0; i < 2; i++) {
    shared_ptr<FWEntry> zone(new FWEntry(input));
    zone->m_asciiFile = shared_ptr<libmwaw::DebugFile>
                        (&ascii(), MWAW_shared_ptr_noop_deleter<libmwaw::DebugFile>());
    zone->setBegin((long)input->readULong(4));
    zone->setLength((sz[i]=(long)input->readULong(4)));
    if (zone->end() > m_state->m_eof || !zone->valid())
      return false;
    if (i == 1) m_state->m_zoneList = zone;
    else m_state->m_zoneFlagsList = zone;
  }
  f << "flZones=[";
  for (int i = 0; i < 3; i++) {
    m_state->m_zoneFlagsId[2-i] = (int)input->readLong(2);
    f << m_state->m_zoneFlagsId[2-i] << ",";
  }
  f << "],";
  val = input->readLong(2); // always 0 ?
  if (val) f << "g0=" << val << ",";
  // a big number
  f << std::hex << "unkn=" << input->readULong(2) << std::dec << ",";
  val = (long) input->readULong(4);
  if (val != 1 && val != 0xbeecf54L) // always 1 in v1 and 0xbeecf54L in v2 ?
    f << std::hex << "unkn2=" << val << std::dec << ",";
  // always 1 ?
  val = (long) input->readULong(4);
  if (val != 1) // always 1 in v1
    f << "g1=" << val << ",";
  val = (long) input->readULong(4);
  if (val==0x46575254L) {
    if ((sz[0]%16)==0 && (sz[1]%8)==0)
      m_state->m_version=2;
    else if ((sz[0]%22)==0 && (sz[1]%10)==0)
      m_state->m_version=1;
    else
      return false;
  } else {
    if (val != 1) f << "g2=" << val << ",";
    if ((sz[0]%22)==0 && (sz[1]%10)==0)
      m_state->m_version=1;
    else
      return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool FWParser::readPrintInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
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
  Vec2i rBotMargin=info.paper().size() - info.page().size();

  // move margin left | top
  int decalX = lTopMargin.x() > 14 ? lTopMargin.x()-14 : 0;
  int decalY = lTopMargin.y() > 14 ? lTopMargin.y()-14 : 0;
  lTopMargin -= Vec2i(decalX, decalY);
  rBotMargin += Vec2i(decalX, decalY);

  // decrease right | bottom
  int rightMarg = rBotMargin.x() -50;
  if (rightMarg < 0) rightMarg=0;
  int botMarg = rBotMargin.y() -50;
  if (botMarg < 0) botMarg=0;

  m_pageSpan.setMarginTop(lTopMargin.y()/72.0);
  m_pageSpan.setMarginBottom(botMarg/72.0);
  m_pageSpan.setMarginLeft(lTopMargin.x()/72.0);
  m_pageSpan.setMarginRight(rightMarg/72.0);
  m_pageSpan.setFormLength(paperSize.y()/72.);
  m_pageSpan.setFormWidth(paperSize.x()/72.);

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(pos+0x78, WPX_SEEK_SET);
  if (long(input->tell()) != pos+0x78) {
    MWAW_DEBUG_MSG(("FWParser::readPrintInfo: file is too short\n"));
    return false;
  }

  return true;
}

void FWParser::sendText(int id, libmwaw::SubDocumentType type, int wh)
{
  if (!m_listener) return;

  MWAWSubDocumentPtr subdoc(new FWParserInternal::SubDocument(*this, getInput(), id));
  switch(type) {
  case libmwaw::DOC_NOTE:
    m_listener->insertNote(MWAWContentListener::NoteType(wh), subdoc);
    break;
  case libmwaw::DOC_COMMENT_ANNOTATION:
    m_listener->insertComment(subdoc);
    break;
  case libmwaw::DOC_HEADER_FOOTER:
  case libmwaw::DOC_TABLE:
  case libmwaw::DOC_TEXT_BOX:
  case libmwaw::DOC_NONE:
  default:
    MWAW_DEBUG_MSG(("FWParser::sendText: unexpected type\n"));
  }
}

bool FWParser::send(int zId)
{
  return m_textParser->send(zId);
}

////////////////////////////////////////
//  send no send zone
////////////////////////////////////////
void FWParser::flushExtra()
{
  m_textParser->flushExtra();
  std::multimap<int, shared_ptr<FWEntry> >::iterator it;
  for (it = m_state->m_graphicMap.begin(); it != m_state->m_graphicMap.end(); it++) {
    shared_ptr<FWEntry> zone = it->second;
    if (!zone || zone->isParsed())
      continue;
    sendGraphic(zone);
  }
}

////////////////////////////////////////
//  the definition of a zone in the file
////////////////////////////////////////
FWEntry::FWEntry(MWAWInputStreamPtr input) : MWAWEntry(), m_input(input), m_flagsId(-1), m_nextId(-2), m_typeId(-1), m_data(), m_asciiFile()
{
  for (int i = 0; i < 3; i++)
    m_values[i] = 0;
}
FWEntry::~FWEntry()
{
  closeDebugFile();
}

std::ostream &operator<<(std::ostream &o, FWEntry const &entry)
{
  if (entry.type().length()) {
    o << entry.type();
    if (entry.id() >= 0) o << "[" << entry.id() << "]";
    o << ",";
  }
  if (entry.m_flagsId != -1) {
    o << "fId=" << entry.m_flagsId << ",";
  }
  if (entry.m_typeId != -1) {
    if (entry.m_typeId >= 0) o << "text/graphic,";
    else o << "null,";
  }
  for (int i = 0; i < 3; i++)
    if (entry.m_values[i])
      o << "f" << i << "=" << entry.m_values[i] << ",";
  if (entry.m_extra.length())
    o << entry.m_extra << ",";
  return o;
}

void FWEntry::update()
{
  if (!m_data.size()) return;

  setBegin(0);
  setLength((long)m_data.size());
  WPXInputStream *dataInput =
    const_cast<WPXInputStream *>(m_data.getDataStream());
  if (!dataInput) {
    MWAW_DEBUG_MSG(("FWEntry::update: can not create entry\n"));
    return;
  }
  m_input.reset(new MWAWInputStream(dataInput, false));
  m_input->setResponsable(false);

  m_asciiFile.reset(new libmwaw::DebugFile(m_input));
  std::stringstream s;
  s << "DataZone" << m_id;
  m_asciiFile->open(s.str());
}

void FWEntry::closeDebugFile()
{
  if (!m_data.size()) return;
  m_asciiFile->reset();
}

libmwaw::DebugFile &FWEntry::getAsciiFile()
{
  return *m_asciiFile;
}

bool FWEntry::operator==(const FWEntry &a) const
{
  if (MWAWEntry::operator!=(a)) return false;
  if (m_input.get() != a.m_input.get()) return false;
  if (id() != a.id()) return false;
  if (m_nextId != a.m_nextId) return false;
  if (m_typeId != a.m_typeId) return false;
  if (m_flagsId != a.m_flagsId) return false;
  for (int i  = 0; i < 3; i++)
    if (m_values[i] != a.m_values[i]) return false;
  return true;
}


// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
