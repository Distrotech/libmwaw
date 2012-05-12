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
#include <sstream>

#include <libwpd/WPXString.h>

#include "MWAWPosition.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPrinter.hxx"

#include "MWAWHeader.hxx"

#include "MWAWStruct.hxx"
#include "MWAWTools.hxx"
#include "MWAWContentListener.hxx"

#include "WNParser.hxx"

#include "WNEntry.hxx"
#include "WNText.hxx"

/** Internal: the structures of a WNParser */
namespace WNParserInternal
{
////////////////////////////////////////
//! Internal: the state of a WNParser
struct State {
  //! constructor
  State() : m_version(-1), m_endPos(-1), m_colorMap(), m_picturesList(),
    m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0),
    m_numColumns(1), m_columnWidth(-1) {
  }

  //! the file version
  int m_version;

  //! the last position
  long m_endPos;
  //! the color map
  std::vector<Vec3uc> m_colorMap;
  //! the list of picture entries
  std::vector<WNEntry> m_picturesList;

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;

  int m_numColumns /** the number of columns */, m_columnWidth /** the columns size */;
};

////////////////////////////////////////
//! Internal: the subdocument of a WNParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(WNParser &pars, MWAWInputStreamPtr input, WNEntry pos) :
    MWAWSubDocument(&pars, input, MWAWEntry()), m_pos(pos) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWContentListenerPtr &listener, MWAWSubDocumentType type);

protected:
  //! the subdocument file position
  WNEntry m_pos;
};

void SubDocument::parse(MWAWContentListenerPtr &listener, MWAWSubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("SubDocument::parse: no listener\n"));
    return;
  }
  WNContentListener *listen = dynamic_cast<WNContentListener *>(listener.get());
  if (!listen) {
    MWAW_DEBUG_MSG(("SubDocument::parse: bad listener\n"));
    return;
  }

  assert(m_parser);

  long pos = m_input->tell();
  reinterpret_cast<WNParser *>(m_parser)->send(m_pos);
  m_input->seek(pos, WPX_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_pos != sDoc->m_pos) return true;
  return false;
}
}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
WNParser::WNParser(MWAWInputStreamPtr input, MWAWHeader *header) :
  MWAWParser(input, header), m_listener(), m_convertissor(), m_state(),
  m_entryManager(), m_pageSpan(), m_textParser(), m_listSubDocuments(),
  m_asciiFile(), m_asciiName("")
{
  init();
}

WNParser::~WNParser()
{
  if (m_listener.get()) m_listener->endDocument();
}

void WNParser::init()
{
  m_convertissor.reset(new MWAWTools::Convertissor);
  m_listener.reset();
  m_asciiName = "main-1";

  m_state.reset(new WNParserInternal::State);
  m_entryManager.reset(new WNEntryManager);

  // reduce the margin (in case, the page is not defined)
  m_pageSpan.setMarginTop(0.1);
  m_pageSpan.setMarginBottom(0.1);
  m_pageSpan.setMarginLeft(0.1);
  m_pageSpan.setMarginRight(0.1);

  m_textParser.reset(new WNText(getInput(), *this, m_convertissor));
}

void WNParser::setListener(WNContentListenerPtr listen)
{
  m_listener = listen;
  m_textParser->setListener(listen);
}

int WNParser::version() const
{
  return m_state->m_version;
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
float WNParser::pageHeight() const
{
  return m_pageSpan.getFormLength()-m_pageSpan.getMarginTop()-m_pageSpan.getMarginBottom()-m_state->m_headerHeight/72.0-m_state->m_footerHeight/72.0;
}

float WNParser::pageWidth() const
{
  return m_pageSpan.getFormWidth()-m_pageSpan.getMarginLeft()-m_pageSpan.getMarginRight();
}

void WNParser::getColumnInfo(int &numColumns, int &width) const
{
  numColumns = m_state->m_numColumns;
  width = m_state->m_columnWidth;
}

////////////////////////////////////////////////////////////
// new page and color
////////////////////////////////////////////////////////////
void WNParser::newPage(int number)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!m_listener || m_state->m_actPage == 1)
      continue;
    m_listener->insertBreak(DMWAW_PAGE_BREAK);
  }
}

bool WNParser::getColor(int colId, Vec3uc &col) const
{
  if (colId >= 0 && colId < int(m_state->m_colorMap.size())) {
    col = m_state->m_colorMap[colId];
    return true;
  }
  return false;
}

void WNParser::sendFootnote(WNEntry const &entry)
{
  if (!m_listener) return;

  MWAWSubDocumentPtr subdoc(new WNParserInternal::SubDocument(*this, getInput(), entry));
  m_listener->insertNote(FOOTNOTE, subdoc);
}

void WNParser::send(WNEntry const &entry)
{
  m_textParser->send(entry);
}

bool WNParser::sendGraphic(int gId, Box2i const &bdbox)
{
  if (gId < 8 || (gId-8) >= int(m_state->m_picturesList.size())) {
    MWAW_DEBUG_MSG(("WNParser::sendGraphic: called with bad id=%d\n", gId));
    return false;
  }
  if (!m_state->m_picturesList[gId-8].isZone()) {
    MWAW_DEBUG_MSG(("WNParser::sendGraphic: called with a no zone id=%d\n", gId));
    return false;
  }

  sendPicture(m_state->m_picturesList[gId-8], bdbox);
  return true;
}


////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void WNParser::parse(WPXDocumentInterface *docInterface)
{
  assert(getInput().get() != 0);

  if (!checkHeader(0L))  throw(libmwaw_libwpd::ParseException());
  bool ok = true;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());

    m_entryManager->reset();
    checkHeader(0L);
    ascii().addPos(getInput()->tell());
    ascii().addNote("_");

    ok = createZones();
    if (ok) {
      createDocument(docInterface);
      m_textParser->sendZone(0);

      // ok, we can now send what is not send.
      m_textParser->flushExtra();
      Box2i emptyBdBox;
      for (int i = 0; i < int(m_state->m_picturesList.size()); i++) {
        if (m_state->m_picturesList[i].isParsed() ||
            !m_state->m_picturesList[i].isZone()) continue;
        sendPicture(m_state->m_picturesList[i], emptyBdBox);
      }
    }

    ascii().reset();
  } catch (...) {
    MWAW_DEBUG_MSG(("WNParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  if (!ok) throw(libmwaw_libwpd::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void WNParser::createDocument(WPXDocumentInterface *documentInterface)
{
  if (!documentInterface) return;
  if (m_listener) {
    MWAW_DEBUG_MSG(("WNParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  std::list<DMWAWPageSpan> pageList;
  DMWAWPageSpan ps(m_pageSpan);

  WNEntry entry = m_textParser->getHeader();
  if (entry.valid()) {
    shared_ptr<WNParserInternal::SubDocument> subdoc
    (new WNParserInternal::SubDocument(*this, getInput(), entry));
    m_listSubDocuments.push_back(subdoc);
    ps.setHeaderFooter(HEADER, 0, ALL, subdoc.get());
  }

  entry = m_textParser->getFooter();
  if (entry.valid()) {
    shared_ptr<WNParserInternal::SubDocument> subdoc
    (new WNParserInternal::SubDocument(*this, getInput(), entry));
    m_listSubDocuments.push_back(subdoc);
    ps.setHeaderFooter(FOOTER, 0, ALL, subdoc.get());
  }

  int numPage = m_textParser->numPages();
  m_state->m_numPages = numPage;

  for (int i = 0; i <= m_state->m_numPages; i++) pageList.push_back(ps);

  //
  WNContentListenerPtr listen =
    WNContentListener::create(pageList, documentInterface);
  setListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// try to find the different zone
////////////////////////////////////////////////////////////
bool WNParser::createZones()
{
  if (version() < 3) {
    if (!readDocEntriesV2())
      return false;
  } else if (!readDocEntries())
    return false;

  std::map<std::string, WNEntry const *>::const_iterator iter;

  // the Color map zone
  iter = m_entryManager->m_typeMap.find("ColMap");
  if (iter != m_entryManager->m_typeMap.end())
    readColorMap(*iter->second);

  // the graphic zone
  iter = m_entryManager->m_typeMap.find("GraphZone");
  if (iter != m_entryManager->m_typeMap.end())
    parseGraphicZone(*iter->second);


  iter = m_entryManager->m_typeMap.find("UnknZone1");
  if (iter != m_entryManager->m_typeMap.end())
    readGenericUnkn(*iter->second);

  iter = m_entryManager->m_typeMap.find("PrintZone");
  if (iter != m_entryManager->m_typeMap.end())
    readPrintInfo(*iter->second);

  // checkme: never seens, but probably also a list of zone...
  iter = m_entryManager->m_typeMap.find("UnknZone2");
  if (iter != m_entryManager->m_typeMap.end())
    readGenericUnkn(*iter->second);

  bool ok = m_textParser->createZones();

  // fixme: we must continue....
  libmwaw::DebugStream f;
  iter = m_entryManager->m_typeMap.begin();
  for (; iter != m_entryManager->m_typeMap.end(); iter++) {
    WNEntry ent = *iter->second;
    if (ent.isParsed()) continue;
    ascii().addPos(ent.begin());
    f.str("");
    f << "Entries(" << iter->first << ")";
    if (ent.m_id >= 0) f << "[" << ent.m_id << "]";
    ascii().addNote(f.str().c_str());
    ascii().addPos(ent.end());
    ascii().addNote("_");
  }
  return ok;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// try to read the document main zone
////////////////////////////////////////////////////////////
bool WNParser::readDocEntries()
{
  MWAWInputStreamPtr input = getInput();

  std::multimap<std::string, WNEntry const *>::iterator it =
    m_entryManager->m_typeMap.find("DocTable");
  if (it == m_entryManager->m_typeMap.end()) {
    MWAW_DEBUG_MSG(("WNParser::readDocEntries: can not find last zone\n"));
    return false;
  }
  WNEntry const &entry = *(it->second);
  if (!entry.valid() || entry.length() < 148) {
    MWAW_DEBUG_MSG(("WNParser::readDocEntries: last entry is invalid\n"));
    return false;
  }

  input->seek(entry.begin(), WPX_SEEK_SET);
  bool ok = input->readLong(4) == entry.length();
  if (!ok || input->readLong(4) != entry.begin()) {
    MWAW_DEBUG_MSG(("WNParser::readDocEntries: bad begin of last zone\n"));
    return false;
  }
  entry.setParsed(true);
  libmwaw::DebugStream f;
  f << "Entries(DocTable):";

  long val;
  long expectedVal[] = { 0, 0x80, 0x40000000L};
  long freePos = 0;
  for (int i = 0; i < 7; i++) { // 0, 80, 4000
    val = input->readULong(4);
    if (i == 3) {
      freePos = val;
      continue;
    }
    if ((i < 3 && val!= expectedVal[i]) || (i >=3 && val))
      f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  if (freePos) { // checkme
    if (freePos > m_state->m_endPos) {
      MWAW_DEBUG_MSG(("WNParser::readDocEntries: find odd freePos\n"));
    } else {
      f << "freeZone?=" << std::hex << freePos << std::dec << ",";
      ascii().addPos(freePos);
      ascii().addNote("Entries(Free)");
    }
  }
  char const *(entryName[]) = { "TextZone", "TextZone", "TextZone", "UnknZone0",
                                "GraphZone", "ColMap", "StylZone", "FontZone",
                                "UnknZone1", "UnknZone2"
                              };
  for (int i = 0; i < 10; i++) {
    WNEntry entry = readEntry();
    entry.setType(entryName[i]);
    if (i < 3) entry.m_id=i;
    if (entry.isZone())
      m_entryManager->add(entry);
    f << entry;
  }
  ascii().addDelimiter(input->tell(), '|');
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  if (entry.length() > 386) {
    long pos = entry.begin()+376;
    input->seek(pos, WPX_SEEK_SET);
    f.str("");
    f << "DocTable-II:";
    m_state->m_numColumns = input->readLong(1);
    f << "nCol=" << m_state->m_numColumns << ",";
    val = input->readLong(1);
    if (val != 1) f << "unkn=" << val << ",";
    m_state->m_columnWidth = input->readLong(2);
    f << "colWidth=" << m_state->m_columnWidth << ",";

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());

    if (long(input->tell()) != entry.end())
      ascii().addDelimiter(input->tell(), '|');
  }
  ascii().addPos(entry.end());
  ascii().addNote("_");

  return true;
}

////////////////////////////////////////////////////////////
// try to read the document main zone : v2
////////////////////////////////////////////////////////////
bool WNParser::readDocEntriesV2()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  libmwaw::DebugStream f;
  std::stringstream s;
  f << "Entries(DocEntries):";
  for (int i = 0; i < 5; i++) {
    if (input->readLong(1) != 4) {
      MWAW_DEBUG_MSG(("WNParser::readDocEntriesV2: can not find entries header:%d\n",i));
      return false;
    }
    long dataPos = input->readULong(1);
    dataPos = (dataPos<<16)+input->readULong(2);
    if (!checkIfPositionValid(dataPos)) {
      MWAW_DEBUG_MSG(("WNParser::readDocEntriesV2:  find an invalid position for entry:%d\n",i));
      continue;
    }
    WNEntry entry;
    entry.setBegin(dataPos);
    switch(i) {
    case 0:
    case 1:
    case 2:
      entry.setType("TextZone");
      entry.m_id=i;
      break;
    case 4:
      entry.setType("PrintZone");
      break;
    default: { // find 2 time 0006000800000000
      std::stringstream s;
      s << "Unknown" << i;
      entry.setType(s.str());
    }
    }
    long actPos = input->tell();
    input->seek(dataPos, WPX_SEEK_SET);
    entry.setLength(input->readULong(2)+2);
    input->seek(actPos, WPX_SEEK_SET);
    m_entryManager->add(entry);
  }
  ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  ascii().addPos(input->tell()+32);
  ascii().addNote("_");
  return true;
}
////////////////////////////////////////////////////////////
// try to read a graphic list zone
////////////////////////////////////////////////////////////
bool WNParser::parseGraphicZone(WNEntry const &entry)
{
  MWAWInputStreamPtr input = getInput();

  if (!entry.valid() || entry.length() < 24) {
    MWAW_DEBUG_MSG(("WNParser::parseGraphicZone: zone size is invalid\n"));
    return false;
  }

  input->seek(entry.begin(), WPX_SEEK_SET);
  if (input->readLong(4) != entry.length()) {
    MWAW_DEBUG_MSG(("WNParser::parseGraphicZone: bad begin of last zone\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(GraphicZone):";
  f << "ptr?=" << std::hex << input->readULong(4) << std::dec << ",";
  f << "ptr2?=" << std::hex << input->readULong(4) << std::dec << ",";
  long val;
  for (int i = 0; i < 3; i++) { // always 3, 80, 0 ?
    val = input->readLong(2);
    if (val)  f << "f" << i << "=" << val << ",";
  }
  int N = input->readLong(2);
  f << "N?=" << N << ",";
  for (int i = 4; i < 6; i++) { // alway 0 ?
    val = input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());

  if (entry.length() != 24+12*N) {
    MWAW_DEBUG_MSG(("WNParser::parseGraphicZone: zone size is invalid(II)\n"));
    return false;
  }
  for (int i = 0; i < N; i++) {
    long pos = input->tell();
    WNEntry entry = readEntry();
    f.str("");
    if (i < 8)
      f << "GraphicZoneA-" << i << ":";
    else
      f << "GraphicZone-" << i-8 << ":";

    entry.m_id=(i < 8) ? i : i-8;
    if (entry.isZone()) {
      if (i == 0)
        entry.setType("PrintZone");
      else if (i < 8) {
        std::stringstream s;
        s << "GraphicUnkn" << i;
        entry.setType(s.str());
      } else
        entry.setType("GraphicData");
      if (i < 8)
        m_entryManager->add(entry);
    } else if (entry.m_val[2]==-1 && entry.m_val[3]==0x76543210L) {
      entry.m_val[2]= entry.m_val[3]=0;
      f << "*";
    }
    if (i >= 8)
      m_state->m_picturesList.push_back(entry);
    f << entry;

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  entry.setParsed(true);
  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// try to read a graphic zone
////////////////////////////////////////////////////////////
bool WNParser::sendPicture(WNEntry const &entry, Box2i const &bdbox)
{
  MWAWInputStreamPtr input = getInput();

  if (!entry.valid() || entry.length() < 24) {
    MWAW_DEBUG_MSG(("WNParser::sendPicture: zone size is invalid\n"));
    return false;
  }

  input->seek(entry.begin(), WPX_SEEK_SET);
  if (input->readLong(4) != entry.length()) {
    MWAW_DEBUG_MSG(("WNParser::sendPicture: bad begin of last zone\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(GraphicData):";
  f << "ptr?=" << std::hex << input->readULong(4) << std::dec << ",";
  f << "ptr2?=" << std::hex << input->readULong(4) << std::dec << ",";
  int type = input->readLong(2);
  if (type != 14) f << "#type=" << type << ",";
  long val;
  /* fl0 : 0[pict1] or 1[pict2] : graphic type ?, fl1 : always 0
   */
  for (int i = 0; i < 2; i++) {
    val = input->readLong(1);
    if (val) f << "fl" << i << "=" << val << ",";
  }

  f << "ptr3?=" << std::hex << input->readULong(4) << std::dec << ",";
  for (int i = 0; i < 2; i++) { // alway 0 ?
    val = input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }

  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());

  int sz = entry.length()-24;
  if (sz) {
    long pos = input->tell();
    shared_ptr<MWAWPict> pict
    (MWAWPictData::get(input, sz));
    if (!pict) {
      MWAW_DEBUG_MSG(("WNParser::sendPicture: can not read the picture\n"));
      ascii().addDelimiter(pos, '|');
    } else {
      if (m_listener) {
        WPXBinaryData data;
        std::string type;
        MWAWPosition pictPos;
        if (bdbox.size().x() > 0 && bdbox.size().y() > 0) {
          pictPos=MWAWPosition(Vec2f(0,0),bdbox.size(), WPX_POINT);
          pictPos.setNaturalSize(pict->getBdBox().size());
        } else
          pictPos=MWAWPosition(Vec2f(0,0),pict->getBdBox().size(), WPX_POINT);

        if (pict->getBinary(data,type))
          m_listener->insertPicture(pictPos, data, type);
      }

#ifdef DEBUG_WITH_FILES
      if (!entry.isParsed()) {
        ascii().skipZone(pos, entry.end()-1);
        WPXBinaryData file;
        input->seek(entry.begin(), WPX_SEEK_SET);
        input->readDataBlock(entry.length(), file);
        static int volatile pictName = 0;
        libmwaw::DebugStream f;
        f << "PICT-" << ++pictName << ".pct";
        libmwaw::Debug::dumpFile(file, f.str().c_str());
      }
#endif
    }
  }

  entry.setParsed(true);
  ascii().addPos(entry.end());
  ascii().addNote("_");

  return true;
}


////////////////////////////////////////////////////////////
// try to read the color map zone
////////////////////////////////////////////////////////////
bool WNParser::readColorMap(WNEntry const &entry)
{
  m_state->m_colorMap.resize(0);

  MWAWInputStreamPtr input = getInput();

  if (!entry.valid() || entry.length() < 0x10) {
    MWAW_DEBUG_MSG(("WNParser::readColorMap: zone size is invalid\n"));
    return false;
  }

  input->seek(entry.begin(), WPX_SEEK_SET);
  if (input->readLong(4) != entry.length()) {
    MWAW_DEBUG_MSG(("WNParser::readColorMap: bad begin of last zone\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(ColorMap):";
  f << "ptr?=" << std::hex << input->readULong(4) << std::dec << ",";
  f << "ptr2?=" << std::hex << input->readULong(4) << std::dec << ",";
  long pos, val;
  for (int i = 0; i < 3; i++) { // always 4, 0, 0 ?
    val = input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  int N=input->readULong(2);
  f << "N=" << N << ",";
  for (int i = 0; i < 2; i++) { // 0
    val = input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  if (long(input->tell())+N*8 > entry.end()) {
    MWAW_DEBUG_MSG(("WNParser::readColorMap: the zone is too short\n"));
    return false;
  }
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  std::vector<long> defPos;
  for (int n = 0; n < N; n++) {
    pos = input->tell();
    f.str("");
    f << "ColorMap[" << n << "]:";
    int type = input->readLong(1);
    switch(type) {
    case 1:
      f << "named(RGB),";
      break;
    case 2:
      f << "unamed,";
      break; // check me : are the color in RGB ?
    case 3:
      f << "unamed(RGB),";
      break;
    default:
      MWAW_DEBUG_MSG(("WNParser::readColorMap: find unknown type %d\n", type));
      f << "#type=" << type << ",";
      break;
    }
    for (int i = 0; i < 3; i++) { // always 0
      val = input->readLong(1);
      if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
    }
    val = input->readULong(4);
    defPos.push_back(pos+val);
    // fixme: used this to read the data...
    f << "defPos=" << std::hex << pos+val << std::dec << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  for (int n = 0; n < N; n++) {
    pos = defPos[n];
    if (pos+12 > entry.end()) {
      MWAW_DEBUG_MSG(("WNParser::readColorMap: can not read entry : %d\n", n));
      return false;
    }

    input->seek(pos, WPX_SEEK_SET);
    f.str("");
    f << "ColorMapData[" << n << "]:";
    int col[4];
    for (int i = 0; i < 4; i++) col[i] = input->readULong(2)/256;
    f << "col=" << col[0] << "x" << col[1] << "x" << col[2];
    if (col[3]) f << "x" << col[3];
    f << ",";
    m_state->m_colorMap.push_back(Vec3uc(col[0],col[1],col[2]));

    int sz = input->readULong(1);
    if (pos+8+1+sz > entry.end()) {
      MWAW_DEBUG_MSG(("WNParser::readColorMap: can not read end of entry : %d\n", n));
      return false;
    }
    std::string name("");
    for (int i = 0; i < sz; i++) name += char(input->readULong(1));

    if (name.length()) f << name;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  entry.setParsed(true);
  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// try to read the print info zone
////////////////////////////////////////////////////////////
bool WNParser::readPrintInfo(WNEntry const &entry)
{
  MWAWInputStreamPtr input = getInput();
  int expectedLength = version() <= 2 ? 0x78+2 : 0x88;
  if (!entry.valid() || entry.length() < expectedLength) {
    MWAW_DEBUG_MSG(("WNParser::readPrintInfo: zone size is invalid\n"));
    return false;
  }

  input->seek(entry.begin(), WPX_SEEK_SET);
  long sz = version() <= 2 ? 2+input->readULong(2) : input->readULong(4);
  if (sz != entry.length()) {
    MWAW_DEBUG_MSG(("WNParser::readPrintInfo: bad begin of last zone\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(PrintInfo):";
  if (version()>=3) {
    f << "ptr?=" << std::hex << input->readULong(4) << std::dec << ",";
    f << "ptr2?=" << std::hex << input->readULong(4) << std::dec << ",";
    long val;
    for (int i = 0; i < 4; i++) { // 15, 0, ??, ???
      val = input->readLong(2);
      if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
    }
    for (int i = 0; i < 2; i++) { // 0
      val = input->readLong(2);
      if (val) f << "g" << i << "=" << val << ",";
    }
  }
  libmwaw::PrinterInfo info;
  if (!info.read(input)) {
    MWAW_DEBUG_MSG(("WNParser::readPrintInfo: can not read print info\n"));
    return false;
  }
  f << info;

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

  entry.setParsed(true);
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// try to read the last generic zone ( always find N=0 :-~)
////////////////////////////////////////////////////////////
bool WNParser::readGenericUnkn(WNEntry const &entry)
{
  MWAWInputStreamPtr input = getInput();

  if (!entry.valid() || entry.length() < 0x10) {
    MWAW_DEBUG_MSG(("WNParser::readGenericUnkn: zone size is invalid\n"));
    return false;
  }

  input->seek(entry.begin(), WPX_SEEK_SET);
  if (input->readLong(4) != entry.length()) {
    MWAW_DEBUG_MSG(("WNParser::readGenericUnkn: bad begin of last zone\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(" << entry.type() << "):";
  f << "ptr?=" << std::hex << input->readULong(4) << std::dec << ",";
  f << "ptr2?=" << std::hex << input->readULong(4) << std::dec << ",";
  long pos, val;
  for (int i = 0; i < 3; i++) { // 7, 0, 0
    val = input->readLong(2);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  int N=input->readULong(2);
  f << "N=" << N << ",";
  for (int i = 0; i < 2; i++) { // 0
    val = input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  if (long(input->tell())+N*8 > entry.end()) {
    MWAW_DEBUG_MSG(("WNParser::readGenericUnkn: the zone is too short\n"));
    return false;
  }
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  std::vector<long> defPos;
  for (int n = 0; n < N; n++) {
    pos = input->tell();
    f.str("");
    f << entry.type() << "[" << n << "]:";
    int type = input->readULong(1);
    switch(type) {
    case 0:
      f << "def,";
      break;
    default:
      MWAW_DEBUG_MSG(("WNParser::readGenericUnkn: find unknown type %d\n", type));
      f << "#type=" << type << ",";
      break;
    }
    for (int i = 0; i < 3; i++) { // always 0
      val = input->readLong(1);
      if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
    }
    val = input->readULong(4);
    defPos.push_back(pos+val);
    // fixme: used this to read the data...
    f << "defPos=" << std::hex << pos+val << std::dec << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  for (int n = 0; n < N; n++) {
    pos = defPos[n];
    if (pos == entry.end()) continue;
    if (pos+12 > entry.end()) {
      MWAW_DEBUG_MSG(("WNParser::readGenericUnkn: can not read entry : %d\n", n));
      return false;
    }

    input->seek(pos, WPX_SEEK_SET);
    f.str("");
    f << entry.type() << "Data[" << n << "]:";

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  entry.setParsed(true);
  ascii().addPos(entry.end());
  ascii().addNote("_");
  return true;
}

bool WNParser::checkIfPositionValid(long pos)
{
  if (pos <= m_state->m_endPos)
    return true;
  MWAWInputStreamPtr input = getInput();
  long actPos = input->tell();
  input->seek(pos, WPX_SEEK_SET);
  bool ok = long(input->tell())==pos;
  if (ok) m_state->m_endPos = pos;

  input->seek(actPos, WPX_SEEK_SET);
  return ok;
}

WNEntry WNParser::readEntry()
{
  WNEntry res;
  MWAWInputStreamPtr input = getInput();
  int val = input->readULong(2);
  res.m_fileType = (val >> 12);
  res.m_val[0]= val & 0x0FFF;
  res.m_val[1]= input->readLong(2);
  if (res.isZoneType()) {
    res.setBegin(input->readULong(4));
    res.setLength(input->readULong(4));
    if (!checkIfPositionValid(res.end())) {
      MWAW_DEBUG_MSG(("WNParser::readEntry: find an invalid entry\n"));
      res.setLength(0);
    }
  } else {
    res.m_val[2]= input->readLong(4);
    res.m_val[3]= input->readLong(4);
  }
  return res;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool WNParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = WNParserInternal::State();

  MWAWInputStreamPtr input = getInput();

  libmwaw::DebugStream f;
  int headerSize=28;
  input->seek(headerSize,WPX_SEEK_SET);
  if (int(input->tell()) != headerSize) {
    MWAW_DEBUG_MSG(("WNParser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0, WPX_SEEK_SET);
  long val = input->readULong(4);
  int version = 0;
  switch(val) {
  case 0:
    if (input->readULong(4) != 0)
      return false;
    version = 2;
    break;
  case 0x57726974: // Writ
    if (input->readULong(4) != 0x654e6f77) // eNow
      return false;
    version = 3;
    break;
  default:
    return false;
  }
  m_state->m_version = version;
  f << "FileHeader:";

  if (version < 3) {
    if (strict) {
      if (input->readLong(1)!=4) return false;
      input->seek(-1, WPX_SEEK_CUR);
    }

    ascii().addPos(0);
    ascii().addNote(f.str().c_str());
    ascii().addPos(input->tell());

    return true;
  }

  val = input->readULong(2);
  if (strict && val > 3)
    return false;
#ifndef DEBUG
  if (val != 2) return false;
#endif
  f << "f0=" << val << ",";

  for (int i = 1; i < 4; i++) {
    // all zero, excepted f1=1 in one file...
    val = input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }

  // a flag ??
  val = input->readULong(2);
  if (val != 0x4000) f << "fl=" << std::hex << val << std::dec << ",";
  val = input->readLong(2);
  if (val) f << "f4=" << val << ",";

  // last entry
  WNEntry entry;
  entry.setBegin(input->readULong(4));
  entry.setLength(input->readULong(4));
  entry.m_fileType = 4;

  if (!checkIfPositionValid(entry.end())) {
    MWAW_DEBUG_MSG(("WNParser::checkHeader: can not find final zone\n"));
    return false;
  }
  entry.setType("DocTable");
  m_entryManager->add(entry);

  f << "entry=" << std::hex << entry.begin() << ":" << entry.end() << std::dec << ",";

  // ok, we can finish initialization
  if (header)
    header->reset(MWAWDocument::WNOW, m_state->m_version);

  //
  input->seek(headerSize, WPX_SEEK_SET);

  ascii().addPos(0);
  ascii().addNote(f.str().c_str());
  ascii().addPos(headerSize);

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
