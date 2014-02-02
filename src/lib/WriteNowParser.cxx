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
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWTextListener.hxx"
#include "MWAWHeader.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPrinter.hxx"

#include "WriteNowEntry.hxx"
#include "WriteNowText.hxx"

#include "WriteNowParser.hxx"

/** Internal: the structures of a WriteNowParser */
namespace WriteNowParserInternal
{
////////////////////////////////////////
//! Internal: the state of a WriteNowParser
struct State {
  //! constructor
  State() : m_endPos(-1), m_colorMap(), m_picturesList(),
    m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0),
    m_numColumns(1), m_columnWidth(-1)
  {
  }

  //! the last position
  long m_endPos;
  //! the color map
  std::vector<MWAWColor> m_colorMap;
  //! the list of picture entries
  std::vector<WriteNowEntry> m_picturesList;

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;

  int m_numColumns /** the number of columns */, m_columnWidth /** the columns size */;
};

////////////////////////////////////////
//! Internal: the subdocument of a WriteNowParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(WriteNowParser &pars, MWAWInputStreamPtr input, WriteNowEntry pos) :
    MWAWSubDocument(&pars, input, MWAWEntry()), m_pos(pos) {}

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
  //! the subdocument file position
  WriteNowEntry m_pos;
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("SubDocument::parse: no listener\n"));
    return;
  }
  assert(m_parser);

  long pos = m_input->tell();
  static_cast<WriteNowParser *>(m_parser)->send(m_pos);
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
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
WriteNowParser::WriteNowParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWTextParser(input, rsrcParser, header), m_state(), m_entryManager(), m_textParser()
{
  init();
}

WriteNowParser::~WriteNowParser()
{
}

void WriteNowParser::init()
{
  resetTextListener();
  setAsciiName("main-1");

  m_state.reset(new WriteNowParserInternal::State);
  m_entryManager.reset(new WriteNowEntryManager);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);

  m_textParser.reset(new WriteNowText(*this));
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
void WriteNowParser::getColumnInfo(int &numColumns, int &width) const
{
  numColumns = m_state->m_numColumns;
  width = m_state->m_columnWidth;
}

////////////////////////////////////////////////////////////
// new page and color
////////////////////////////////////////////////////////////
void WriteNowParser::newPage(int number)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!getTextListener() || m_state->m_actPage == 1)
      continue;
    getTextListener()->insertBreak(MWAWTextListener::PageBreak);
  }
}

bool WriteNowParser::getColor(int colId, MWAWColor &col) const
{
  if (colId >= 0 && colId < int(m_state->m_colorMap.size())) {
    col = m_state->m_colorMap[(size_t)colId];
    return true;
  }
  return false;
}

void WriteNowParser::sendFootnote(WriteNowEntry const &entry)
{
  if (!getTextListener()) return;

  MWAWSubDocumentPtr subdoc(new WriteNowParserInternal::SubDocument(*this, getInput(), entry));
  getTextListener()->insertNote(MWAWNote(MWAWNote::FootNote), subdoc);
}

void WriteNowParser::send(WriteNowEntry const &entry)
{
  m_textParser->send(entry);
}

bool WriteNowParser::sendGraphic(int gId, Box2i const &bdbox)
{
  if (gId < 8 || (gId-8) >= int(m_state->m_picturesList.size())) {
    MWAW_DEBUG_MSG(("WriteNowParser::sendGraphic: called with bad id=%d\n", gId));
    return false;
  }
  if (!m_state->m_picturesList[(size_t)gId-8].isZone()) {
    MWAW_DEBUG_MSG(("WriteNowParser::sendGraphic: called with a no zone id=%d\n", gId));
    return false;
  }

  sendPicture(m_state->m_picturesList[(size_t)gId-8], bdbox);
  return true;
}


////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void WriteNowParser::parse(librevenge::RVNGTextInterface *docInterface)
{
  assert(getInput().get() != 0);

  if (!checkHeader(0L))  throw(libmwaw::ParseException());
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
      for (size_t i = 0; i < m_state->m_picturesList.size(); i++) {
        if (m_state->m_picturesList[i].isParsed() ||
            !m_state->m_picturesList[i].isZone()) continue;
        sendPicture(m_state->m_picturesList[i], emptyBdBox);
      }
    }

    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("WriteNowParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetTextListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void WriteNowParser::createDocument(librevenge::RVNGTextInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getTextListener()) {
    MWAW_DEBUG_MSG(("WriteNowParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  MWAWPageSpan ps(getPageSpan());

  WriteNowEntry entry = m_textParser->getHeader();
  if (entry.valid()) {
    MWAWHeaderFooter header(MWAWHeaderFooter::HEADER, MWAWHeaderFooter::ALL);
    header.m_subDocument.reset(new WriteNowParserInternal::SubDocument(*this, getInput(), entry));
    ps.setHeaderFooter(header);
  }

  entry = m_textParser->getFooter();
  if (entry.valid()) {
    MWAWHeaderFooter footer(MWAWHeaderFooter::FOOTER, MWAWHeaderFooter::ALL);
    footer.m_subDocument.reset(new WriteNowParserInternal::SubDocument(*this, getInput(), entry));
    ps.setHeaderFooter(footer);
  }

  int numPage = m_textParser->numPages();
  m_state->m_numPages = numPage;
  ps.setPageSpan(m_state->m_numPages+1);
  std::vector<MWAWPageSpan> pageList(1,ps);
  //
  MWAWTextListenerPtr listen(new MWAWTextListener(*getParserState(), pageList, documentInterface));
  setTextListener(listen);
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
bool WriteNowParser::createZones()
{
  if (version() < 3) {
    if (!readDocEntriesV2())
      return false;
  }
  else if (!readDocEntries())
    return false;

  std::multimap<std::string, WriteNowEntry const *>::const_iterator iter;

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
  for (; iter != m_entryManager->m_typeMap.end(); ++iter) {
    WriteNowEntry ent = *iter->second;
    if (ent.isParsed()) continue;
    ascii().addPos(ent.begin());
    f.str("");
    f << "Entries(" << iter->first << ")";
    if (ent.id() >= 0) f << "[" << ent.id() << "]";
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
bool WriteNowParser::readDocEntries()
{
  MWAWInputStreamPtr input = getInput();

  std::multimap<std::string, WriteNowEntry const *>::iterator it =
    m_entryManager->m_typeMap.find("DocEntries");
  if (it == m_entryManager->m_typeMap.end()) {
    MWAW_DEBUG_MSG(("WriteNowParser::readDocEntries: can not find last zone\n"));
    return false;
  }
  WriteNowEntry const &entry = *(it->second);
  if (!entry.valid() || entry.length() < 148) {
    MWAW_DEBUG_MSG(("WriteNowParser::readDocEntries: last entry is invalid\n"));
    return false;
  }

  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  bool ok = input->readLong(4) == entry.length();
  if (!ok || input->readLong(4) != entry.begin()) {
    MWAW_DEBUG_MSG(("WriteNowParser::readDocEntries: bad begin of last zone\n"));
    return false;
  }
  entry.setParsed(true);
  libmwaw::DebugStream f;
  f << "Entries(DocEntries):";

  long val;
  long expectedVal[] = { 0, 0x80, 0x40000000L};
  long freePos = 0;
  for (int i = 0; i < 7; i++) { // 0, 80, 4000
    val = (long) input->readULong(4);
    if (i == 3) {
      freePos = val;
      continue;
    }
    if ((i < 3 && val!= expectedVal[i]) || (i >=3 && val))
      f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  if (freePos) { // checkme
    if (freePos > m_state->m_endPos) {
      MWAW_DEBUG_MSG(("WriteNowParser::readDocEntries: find odd freePos\n"));
    }
    else {
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
    WriteNowEntry zone = readEntry();
    zone.setType(entryName[i]);
    if (i < 3) zone.setId(i);
    if (zone.isZone())
      m_entryManager->add(zone);
    f << zone;
  }
  ascii().addDelimiter(input->tell(), '|');
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  if (entry.length() > 386) {
    long pos = entry.begin()+376;
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    f.str("");
    f << "DocEntries-II:";
    m_state->m_numColumns = (int) input->readLong(1);
    f << "nCol=" << m_state->m_numColumns << ",";
    val = input->readLong(1);
    if (val != 1) f << "unkn=" << val << ",";
    m_state->m_columnWidth = (int) input->readLong(2);
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
bool WriteNowParser::readDocEntriesV2()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell(), debPos=pos;
  libmwaw::DebugStream f;
  std::stringstream s;
  f << "Entries(DocEntries):";
  for (int i = 0; i < 5; i++) {
    int val=(int) input->readLong(1);
    if (val != 4 && val != 0x44) {
      MWAW_DEBUG_MSG(("WriteNowParser::readDocEntriesV2: can not find entries header:%d\n",i));
      return false;
    }
    long dataPos = (long) input->readULong(1);
    dataPos = (dataPos<<16)+(long) input->readULong(2);
    if (!checkIfPositionValid(dataPos)) {
      MWAW_DEBUG_MSG(("WriteNowParser::readDocEntriesV2:  find an invalid position for entry:%d\n",i));
      continue;
    }
    WriteNowEntry entry;
    entry.setBegin(dataPos);
    switch (i) {
    case 0:
    case 1:
    case 2:
      entry.setType("TextZone");
      entry.setId(i);
      break;
    case 4:
      entry.setType("PrintZone");
      break;
    default: { // find 2 time 0006000800000000
      std::stringstream name;
      s << "Unknown" << i;
      entry.setType(name.str());
    }
    }
    long actPos = input->tell();
    input->seek(dataPos, librevenge::RVNG_SEEK_SET);
    entry.setLength((long) input->readULong(2)+2);
    input->seek(actPos, librevenge::RVNG_SEEK_SET);
    m_entryManager->add(entry);
  }
  f << "ptr=[";
  for (int i = 0; i < 5; i++)
    f << std::hex << input->readULong(4) << std::dec << ",";
  f << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(debPos+0x6E,librevenge::RVNG_SEEK_SET);
  pos=input->tell();
  f.str("");
  f << "DocEntries-II:";

  if (version()==2) {
    m_state->m_numColumns = (int) input->readLong(1);
    f << "nCol=" << m_state->m_numColumns << ",";
    long val = input->readLong(1);
    if (val != 1) f << "unkn=" << val << ",";
    m_state->m_columnWidth = (int) input->readLong(2);
    f << "colWidth=" << m_state->m_columnWidth << ",";
    ascii().addDelimiter(input->tell(),'|');
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  // another copy (visibly the previous version)
  ascii().addPos(debPos+0xFc);
  ascii().addNote("DocEntries[Old]:");
  ascii().addPos(debPos+0x16a);
  ascii().addNote("DocEntries-II[Old]:");
  ascii().addPos(debPos+0x1F8);
  ascii().addNote("_");
  return true;
}
////////////////////////////////////////////////////////////
// try to read a graphic list zone
////////////////////////////////////////////////////////////
bool WriteNowParser::parseGraphicZone(WriteNowEntry const &entry)
{
  MWAWInputStreamPtr input = getInput();

  if (!entry.valid() || entry.length() < 24) {
    MWAW_DEBUG_MSG(("WriteNowParser::parseGraphicZone: zone size is invalid\n"));
    return false;
  }

  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  if (input->readLong(4) != entry.length()) {
    MWAW_DEBUG_MSG(("WriteNowParser::parseGraphicZone: bad begin of last zone\n"));
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
  int N = (int) input->readLong(2);
  f << "N?=" << N << ",";
  for (int i = 4; i < 6; i++) { // alway 0 ?
    val = input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());

  if (entry.length() != 24+12*N) {
    MWAW_DEBUG_MSG(("WriteNowParser::parseGraphicZone: zone size is invalid(II)\n"));
    return false;
  }
  for (int i = 0; i < N; i++) {
    long pos = input->tell();
    WriteNowEntry zone = readEntry();
    f.str("");
    if (i < 8)
      f << "GraphicZoneA-" << i << ":";
    else
      f << "GraphicZone-" << i-8 << ":";

    zone.setId((i < 8) ? i : i-8);
    if (zone.isZone()) {
      if (i == 0)
        zone.setType("PrintZone");
      else if (i < 8) {
        std::stringstream s;
        s << "GraphicUnkn" << i;
        zone.setType(s.str());
      }
      else
        zone.setType("GraphicData");
      if (i < 8)
        m_entryManager->add(zone);
    }
    else if (zone.m_val[2]==-1 && zone.m_val[3]==0x76543210L) {
      zone.m_val[2]= zone.m_val[3]=0;
      f << "*";
    }
    if (i >= 8)
      m_state->m_picturesList.push_back(zone);
    f << zone;

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
bool WriteNowParser::sendPicture(WriteNowEntry const &entry, Box2i const &bdbox)
{
  MWAWInputStreamPtr input = getInput();

  if (!entry.valid() || entry.length() < 24) {
    MWAW_DEBUG_MSG(("WriteNowParser::sendPicture: zone size is invalid\n"));
    return false;
  }

  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  if (input->readLong(4) != entry.length()) {
    MWAW_DEBUG_MSG(("WriteNowParser::sendPicture: bad begin of last zone\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(GraphicData):";
  f << "ptr?=" << std::hex << input->readULong(4) << std::dec << ",";
  f << "ptr2?=" << std::hex << input->readULong(4) << std::dec << ",";
  int type = (int) input->readLong(2);
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

  int sz = (int) entry.length()-24;
  if (sz) {
    long pos = input->tell();
    shared_ptr<MWAWPict> pict(MWAWPictData::get(input, sz));
    if (!pict) {
      MWAW_DEBUG_MSG(("WriteNowParser::sendPicture: can not read the picture\n"));
      ascii().addDelimiter(pos, '|');
    }
    else {
      if (getTextListener()) {
        librevenge::RVNGBinaryData data;
        std::string pictType;
        MWAWPosition pictPos;
        if (bdbox.size().x() > 0 && bdbox.size().y() > 0) {
          pictPos=MWAWPosition(Vec2f(0,0),bdbox.size(), librevenge::RVNG_POINT);
          pictPos.setNaturalSize(pict->getBdBox().size());
        }
        else
          pictPos=MWAWPosition(Vec2f(0,0),pict->getBdBox().size(), librevenge::RVNG_POINT);
        pictPos.setRelativePosition(MWAWPosition::Char);

        if (pict->getBinary(data,pictType))
          getTextListener()->insertPicture(pictPos, data, pictType);
      }

#ifdef DEBUG_WITH_FILES
      if (!entry.isParsed()) {
        ascii().skipZone(pos, entry.end()-1);
        librevenge::RVNGBinaryData file;
        input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
        input->readDataBlock(entry.length(), file);
        static int volatile pictName = 0;
        libmwaw::DebugStream f2;
        f2 << "PICT-" << ++pictName << ".pct";
        libmwaw::Debug::dumpFile(file, f2.str().c_str());
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
bool WriteNowParser::readColorMap(WriteNowEntry const &entry)
{
  m_state->m_colorMap.resize(0);

  MWAWInputStreamPtr input = getInput();

  if (!entry.valid() || entry.length() < 0x10) {
    MWAW_DEBUG_MSG(("WriteNowParser::readColorMap: zone size is invalid\n"));
    return false;
  }

  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  if (input->readLong(4) != entry.length()) {
    MWAW_DEBUG_MSG(("WriteNowParser::readColorMap: bad begin of last zone\n"));
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
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  for (int i = 0; i < 2; i++) { // 0
    val = (int) input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  if (long(input->tell())+N*8 > entry.end()) {
    MWAW_DEBUG_MSG(("WriteNowParser::readColorMap: the zone is too short\n"));
    return false;
  }
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  std::vector<long> defPos;
  for (int n = 0; n < N; n++) {
    pos = input->tell();
    f.str("");
    f << "ColorMap[" << n << "]:";
    int type = (int) input->readLong(1);
    switch (type) {
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
      MWAW_DEBUG_MSG(("WriteNowParser::readColorMap: find unknown type %d\n", type));
      f << "#type=" << type << ",";
      break;
    }
    for (int i = 0; i < 3; i++) { // always 0
      val = input->readLong(1);
      if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
    }
    val = (long) input->readULong(4);
    defPos.push_back(pos+val);
    // fixme: used this to read the data...
    f << "defPos=" << std::hex << pos+val << std::dec << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  for (int n = 0; n < N; n++) {
    pos = defPos[(size_t) n];
    if (pos+12 > entry.end()) {
      MWAW_DEBUG_MSG(("WriteNowParser::readColorMap: can not read entry : %d\n", n));
      return false;
    }

    input->seek(pos, librevenge::RVNG_SEEK_SET);
    f.str("");
    f << "ColorMapData[" << n << "]:";
    unsigned char col[4];
    for (int i = 0; i < 4; i++) col[i] = (unsigned char)(input->readULong(2)/256);
    f << "col=" << MWAWColor(col[0],col[1],col[2],col[3]) << ",";
    m_state->m_colorMap.push_back(MWAWColor((unsigned char)col[0],(unsigned char)col[1],(unsigned char)col[2]));

    int sz = (int) input->readULong(1);
    if (pos+8+1+sz > entry.end()) {
      MWAW_DEBUG_MSG(("WriteNowParser::readColorMap: can not read end of entry : %d\n", n));
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
bool WriteNowParser::readPrintInfo(WriteNowEntry const &entry)
{
  MWAWInputStreamPtr input = getInput();
  int expectedLength = version() <= 2 ? 0x78+2 : 0x88;
  if (!entry.valid() || entry.length() < expectedLength) {
    MWAW_DEBUG_MSG(("WriteNowParser::readPrintInfo: zone size is invalid\n"));
    return false;
  }

  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  long sz = version() <= 2 ? 2+(long) input->readULong(2) : (long) input->readULong(4);
  if (sz != entry.length()) {
    MWAW_DEBUG_MSG(("WriteNowParser::readPrintInfo: bad begin of last zone\n"));
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
    MWAW_DEBUG_MSG(("WriteNowParser::readPrintInfo: can not read print info\n"));
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

  getPageSpan().setMarginTop(lTopMargin.y()/72.0);
  getPageSpan().setMarginBottom(botMarg/72.0);
  getPageSpan().setMarginLeft(lTopMargin.x()/72.0);
  getPageSpan().setMarginRight(rightMarg/72.0);
  getPageSpan().setFormLength(paperSize.y()/72.);
  getPageSpan().setFormWidth(paperSize.x()/72.);

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
bool WriteNowParser::readGenericUnkn(WriteNowEntry const &entry)
{
  MWAWInputStreamPtr input = getInput();

  if (!entry.valid() || entry.length() < 0x10) {
    MWAW_DEBUG_MSG(("WriteNowParser::readGenericUnkn: zone size is invalid\n"));
    return false;
  }

  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  if (input->readLong(4) != entry.length()) {
    MWAW_DEBUG_MSG(("WriteNowParser::readGenericUnkn: bad begin of last zone\n"));
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
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  for (int i = 0; i < 2; i++) { // 0
    val = input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  if (long(input->tell())+N*8 > entry.end()) {
    MWAW_DEBUG_MSG(("WriteNowParser::readGenericUnkn: the zone is too short\n"));
    return false;
  }
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  std::vector<long> defPos;
  for (int n = 0; n < N; n++) {
    pos = input->tell();
    f.str("");
    f << entry.type() << "[" << n << "]:";
    int type = (int) input->readULong(1);
    switch (type) {
    case 0:
      f << "def,";
      break;
    default:
      MWAW_DEBUG_MSG(("WriteNowParser::readGenericUnkn: find unknown type %d\n", type));
      f << "#type=" << type << ",";
      break;
    }
    for (int i = 0; i < 3; i++) { // always 0
      val = input->readLong(1);
      if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
    }
    val = (long) input->readULong(4);
    defPos.push_back(pos+val);
    // fixme: used this to read the data...
    f << "defPos=" << std::hex << pos+val << std::dec << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  for (int n = 0; n < N; n++) {
    pos = defPos[(size_t) n];
    if (pos == entry.end()) continue;
    if (pos+12 > entry.end()) {
      MWAW_DEBUG_MSG(("WriteNowParser::readGenericUnkn: can not read entry : %d\n", n));
      return false;
    }

    input->seek(pos, librevenge::RVNG_SEEK_SET);
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

bool WriteNowParser::checkIfPositionValid(long pos)
{
  if (pos <= m_state->m_endPos)
    return true;
  MWAWInputStreamPtr input = getInput();
  long actPos = input->tell();
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  bool ok = long(input->tell())==pos;
  if (ok) m_state->m_endPos = pos;

  input->seek(actPos, librevenge::RVNG_SEEK_SET);
  return ok;
}

WriteNowEntry WriteNowParser::readEntry()
{
  WriteNowEntry res;
  MWAWInputStreamPtr input = getInput();
  int val = (int) input->readULong(2);
  res.m_fileType = (val >> 12);
  res.m_val[0]= val & 0x0FFF;
  res.m_val[1]= (int) input->readLong(2);
  if (res.isZoneType()) {
    res.setBegin((long) input->readULong(4));
    res.setLength((long) input->readULong(4));
    if (!checkIfPositionValid(res.end())) {
      MWAW_DEBUG_MSG(("WriteNowParser::readEntry: find an invalid entry\n"));
      res.setLength(0);
    }
  }
  else {
    res.m_val[2]= (int) input->readLong(4);
    res.m_val[3]= (int) input->readLong(4);
  }
  return res;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool WriteNowParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = WriteNowParserInternal::State();

  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork())
    return false;

  libmwaw::DebugStream f;
  int headerSize=28;
  input->seek(headerSize,librevenge::RVNG_SEEK_SET);
  if (int(input->tell()) != headerSize) {
    MWAW_DEBUG_MSG(("WriteNowParser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0, librevenge::RVNG_SEEK_SET);
  long val = (long) input->readULong(4);
  int vers = 0;
  switch (val) {
  case 0:
    if (input->readULong(4) != 0)
      return false;
    vers = 2;
    break;
  case 0x57726974: // Writ
    if (input->readULong(4) != 0x654e6f77) // eNow
      return false;
    vers = 3;
    break;
  default:
    return false;
  }
  setVersion(vers);
  f << "FileHeader:";

  if (vers < 3) {
    if (strict) {
      for (int i=0; i < 4; ++i) {
        val = long(input->readLong(1));
        if (val!=4 && val!=0x44) return false;
        input->seek(3, librevenge::RVNG_SEEK_CUR);
      }
      input->seek(8, librevenge::RVNG_SEEK_SET);
    }

    ascii().addPos(0);
    ascii().addNote(f.str().c_str());
    ascii().addPos(input->tell());

    return true;
  }

  val = (long) input->readULong(2);
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
  val = (long) input->readULong(2);
  if (val != 0x4000) f << "fl=" << std::hex << val << std::dec << ",";
  val = (long) input->readLong(2);
  if (val) f << "f4=" << val << ",";

  // last entry
  WriteNowEntry entry;
  entry.setBegin((long) input->readULong(4));
  entry.setLength((long) input->readULong(4));
  entry.m_fileType = 4;

  if (!checkIfPositionValid(entry.end())) {
    MWAW_DEBUG_MSG(("WriteNowParser::checkHeader: can not find final zone\n"));
    return false;
  }
  entry.setType("DocEntries");
  m_entryManager->add(entry);

  f << "entry=" << std::hex << entry.begin() << ":" << entry.end() << std::dec << ",";

  // ok, we can finish initialization
  if (header)
    header->reset(MWAWDocument::MWAW_T_WRITENOW, version());

  //
  input->seek(headerSize, librevenge::RVNG_SEEK_SET);

  ascii().addPos(0);
  ascii().addNote(f.str().c_str());
  ascii().addPos(headerSize);

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
