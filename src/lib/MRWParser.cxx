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

#include <string.h>

#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>

#include <libwpd/libwpd.h>

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

#include "MRWGraph.hxx"
#include "MRWText.hxx"

#include "MRWParser.hxx"

/** Internal: the structures of a MRWParser */
namespace MRWParserInternal
{
////////////////////////////////////////
//! Internal: the state of a MRWParser
struct State {
  //! constructor
  State() : m_eof(-1), m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0) {
  }

  //! end of file
  long m_eof;

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;
  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a MWParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(MRWParser &pars, MWAWInputStreamPtr input, int zoneId) :
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
  MRWContentListener *listen = dynamic_cast<MRWContentListener *>(listener.get());
  if (!listen) {
    MWAW_DEBUG_MSG(("SubDocument::parse: bad listener\n"));
    return;
  }
  if (m_id != 1 && m_id != 2) {
    MWAW_DEBUG_MSG(("SubDocument::parse: unknown zone\n"));
    return;
  }

  assert(m_parser);
  long pos = m_input->tell();
  //reinterpret_cast<MRWParser *>(m_parser)->sendZone(m_id);
  m_input->seek(pos, WPX_SEEK_SET);
}
}

////////////////////////////////////////////////////////////
// constructor/destructor + basic interface ...
////////////////////////////////////////////////////////////
MRWParser::MRWParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWParser(input, rsrcParser, header), m_listener(), m_convertissor(), m_state(),
  m_pageSpan(), m_pageSpanSet(false), m_graphParser(), m_textParser()
{
  init();
}

MRWParser::~MRWParser()
{
}

void MRWParser::init()
{
  m_convertissor.reset(new MWAWFontConverter);
  m_listener.reset();
  setAsciiName("main-1");

  m_state.reset(new MRWParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  m_pageSpan.setMarginTop(0.1);
  m_pageSpan.setMarginBottom(0.1);
  m_pageSpan.setMarginLeft(0.1);
  m_pageSpan.setMarginRight(0.1);

  m_graphParser.reset(new MRWGraph(getInput(), *this, m_convertissor));
  m_textParser.reset(new MRWText(getInput(), *this, m_convertissor));
}

void MRWParser::setListener(MRWContentListenerPtr listen)
{
  m_listener = listen;
  m_graphParser->setListener(listen);
  m_textParser->setListener(listen);
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
float MRWParser::pageHeight() const
{
  return float(m_pageSpan.getFormLength()-m_pageSpan.getMarginTop()-m_pageSpan.getMarginBottom()-m_state->m_headerHeight/72.0-m_state->m_footerHeight/72.0);
}

float MRWParser::pageWidth() const
{
  return float(m_pageSpan.getFormWidth()-m_pageSpan.getMarginLeft()-m_pageSpan.getMarginRight());
}

Vec2f MRWParser::getPageLeftTop() const
{
  return Vec2f(float(m_pageSpan.getMarginLeft()),
               float(m_pageSpan.getMarginTop()+m_state->m_headerHeight/72.0));
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void MRWParser::newPage(int number)
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

bool MRWParser::isFilePos(long pos)
{
  if (pos <= m_state->m_eof)
    return true;

  MWAWInputStreamPtr input = getInput();
  long actPos = input->tell();
  input->seek(pos, WPX_SEEK_SET);
  bool ok = long(input->tell()) == pos;
  if (ok) m_state->m_eof = pos;
  input->seek(actPos, WPX_SEEK_SET);
  return ok;
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void MRWParser::parse(WPXDocumentInterface *docInterface)
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
      m_graphParser->sendPageGraphics();

      m_textParser->flushExtra();
      m_graphParser->flushExtra();
      if (m_listener) m_listener->endDocument();
      m_listener.reset();
    }
    ascii().reset();
  } catch (...) {
    MWAW_DEBUG_MSG(("MRWParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void MRWParser::createDocument(WPXDocumentInterface *documentInterface)
{
  if (!documentInterface) return;
  if (m_listener) {
    MWAW_DEBUG_MSG(("MRWParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  int numPage = m_textParser->numPages();
  if (m_graphParser->numPages() > numPage)
    numPage = m_graphParser->numPages();
  m_state->m_numPages = numPage;

  // create the page list
  std::vector<MWAWPageSpan> pageList;
  MWAWPageSpan ps(m_pageSpan);

  for (int i = 0; i <= m_state->m_numPages; i++) pageList.push_back(ps);

  //
  MRWContentListenerPtr listen(new MRWContentListener(pageList, documentInterface));
  setListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////

// ------ read the different zones ---------
bool MRWParser::createZones()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  int actZone=-1;
  while (readZone(actZone))
    pos = input->tell();
  ascii().addPos(pos);
  ascii().addNote("Entries(Loose)");
  return false;
}

bool MRWParser::readZone(int &actZone)
{
  MWAWInputStreamPtr input = getInput();
  if (input->atEOS())
    return false;
  long pos = input->tell();
  MRWEntry zone;
  if (!readEntryHeader(zone)) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  libmwaw::DebugStream f;
  f << "Entries(" << zone.name() << "):" << zone;

  bool done = false;
  switch(zone.m_fileType) {
  case -1: // separator
  case -2: // last file
    done = readSeparator(zone);
    actZone++;
    break;
  case 0x1:
    done = readZone1(zone, actZone);
    break;
  case 2:
    done = m_textParser->readTextZone(zone, actZone);
    break;
  case 4:
  case 5:
    done = m_textParser->readPLCZone(zone, actZone);
    break;
  case 6:
    done = m_textParser->readFonts(zone, actZone);
    break;
  case 7:
    done = m_textParser->readRulers(zone, actZone);
    break;
  case 8:
    done = m_textParser->readFontNames(zone, actZone);
    break;
  case 9:
    done = readDimSeparator(zone, actZone);
    break;
  case 0xa:
    done = readDimSeparator(zone, actZone);
    break;
  case 0xb: // border dim?
    done = readZoneb(zone, actZone);
    break;
  case 0xc:
    done = readZonec(zone, actZone);
    break;
  case 0x14:
    done = m_graphParser->readToken(zone, actZone);
    break;
  case 0x1a:
    done = m_textParser->readStyleNames(zone, actZone);
    break;
  case 0x1f:
    done = readPrintInfo(zone);
    break;
  case 0x24:
    done = readCPRT(zone);
    break;
    /* 0x41a: docInfo */
  case 0x420:
    done = m_graphParser->readPostscript(zone, actZone);
    break;
  default:
    break;
  }
  if (done) {
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(zone.end(), WPX_SEEK_SET);
    return true;
  }

  input->seek(zone.begin(), WPX_SEEK_SET);
  input->pushLimit(zone.end());
  std::vector<MRWStruct> dataList;
  decodeZone(dataList);
  input->popLimit();

  size_t numData = dataList.size();
  f << "numData=" << numData << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  int numDataByField = zone.m_fileType==1 ? 22 : 10;
  for (size_t d = 0; d < numData; d++) {
    MRWStruct const &dt = dataList[d];
    if ((int(d)%numDataByField)==0) {
      if (d)
        ascii().addNote(f.str().c_str());
      f.str("");
      f << zone.name() << "-" << d << ":";
      ascii().addPos(dt.m_filePos);
    }
    f << dt << ",";
  }
  if (numData)
    ascii().addNote(f.str().c_str());

  if (input->tell() != zone.end()) {
    f.str("");
    if (input->tell() == zone.end()-1)
      f << "_";
    else
      f << zone.name() << ":###";
    ascii().addPos(input->tell());
    ascii().addNote(f.str().c_str());
  }
  input->seek(zone.end(), WPX_SEEK_SET);
  return true;
}

// --------- zone separator ----------

// read the zone separator
bool MRWParser::readSeparator(MRWEntry const &entry)
{
  if (entry.length() < 0x3) {
    MWAW_DEBUG_MSG(("MRWParser::readSeparator: data seems to short\n"));
    return false;
  }
  MWAWInputStreamPtr input = getInput();
  input->seek(entry.begin(), WPX_SEEK_SET);
  input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  decodeZone(dataList);
  input->popLimit();

  if (dataList.size() != 1) {
    MWAW_DEBUG_MSG(("MRWParser::readSeparator: can find my data\n"));
    return false;
  }

  MRWStruct const &data = dataList[0]; // always 0x77aa
  libmwaw::DebugStream f;
  f << entry.name() << "[data]:";
  if (data.m_data.size() != 1 || data.m_data[0] != 0x77aa)
    f << "#" << data;
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());

  return true;
}

bool MRWParser::readDimSeparator(MRWEntry const &entry, int )
{
  if (entry.length() < entry.m_N) {
    MWAW_DEBUG_MSG(("MRWParser::readDimSeparator: data seems to short\n"));
    return false;
  }
  MWAWInputStreamPtr input = getInput();
  input->seek(entry.begin(), WPX_SEEK_SET);
  input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  decodeZone(dataList, 1+4*entry.m_N);
  input->popLimit();

  if (int(dataList.size()) != 4*entry.m_N) {
    MWAW_DEBUG_MSG(("MRWParser::readDimSeparator: find unexpected number of data\n"));
    return false;
  }

  libmwaw::DebugStream f;
  size_t d = 0;
  for (int i = 0; i < entry.m_N; i++) {
    f.str("");
    f << entry.name() << "-" << i << ":";
    ascii().addPos(dataList[d].m_filePos);

    int dim[4] = { 0, 0, 0, 0 };
    for (int j = 0; j < 4; j++) {
      MRWStruct const &data = dataList[d++];
      if (!data.isBasic()) {
        MWAW_DEBUG_MSG(("MRWParser::readDimSeparator: find unexpected dim data type\n"));
        f << "###dim" << j << "=" << data << ",";
      } else
        dim[j] = (int) data.value(0);
    }
    f << "dim?=" << dim[1] << "x" << dim[0] << "<->" << dim[3] << "x" << dim[2] << ",";
    ascii().addNote(f.str().c_str());
  }
  input->seek(entry.end(), WPX_SEEK_SET);

  return true;
}

bool MRWParser::readZone1(MRWEntry const &entry, int)
{
  if (entry.length() < entry.m_N) {
    MWAW_DEBUG_MSG(("MRWParser::readZone1: data seems to short\n"));
    return false;
  }
  MWAWInputStreamPtr input = getInput();
  input->seek(entry.begin(), WPX_SEEK_SET);
  input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  decodeZone(dataList, 1+22*entry.m_N);
  input->popLimit();

  if (int(dataList.size()) != 22*entry.m_N) {
    MWAW_DEBUG_MSG(("MRWParser::readZone1: find unexpected number of data\n"));
    return false;
  }

  libmwaw::DebugStream f;
  size_t d = 0;
  for (int i = 0; i < entry.m_N; i++) {
    f.str("");
    f << entry.name() << "-" << i << ":";
    ascii().addPos(dataList[d].m_filePos);

    long cPos[2];
    for (int j = 0; j < 2; j++) {
      MRWStruct const &data = dataList[d++];
      if (!data.isBasic()) {
        MWAW_DEBUG_MSG(("MRWParser::readZone1: find unexpected pos data type\n"));
        f << "###pos" << j << "=" << data << ",";
        cPos[j]=0;
      } else
        cPos[j]=data.value(0);
    }
    f << "cPos=" << std::hex << cPos[0] << "<->" << cPos[1] << ",";
    MRWStruct dt = dataList[d++];
    if (!dt.isBasic()) {
      MWAW_DEBUG_MSG(("MRWParser::readZone1: find unexpected fl0 type\n"));
      f << "###fl0=" << dt << ",";
    } else
      f << "fl0=" << std::hex << dt.value(0) << std::dec << ",";
    int posi[4];
    for (int j = 0; j < 4; j++) { // first,length, first,length
      MRWStruct const &data = dataList[d++];
      if (!data.isBasic()) {
        MWAW_DEBUG_MSG(("MRWParser::readZone1: find unexpected posi data type\n"));
        f << "###pos" << j << "=" << data << ",";
        posi[j]=0;
      } else
        posi[j]=int(data.value(0));
    }
    if (posi[0] || posi[1])
      f << "pos0=" << posi[0] << ":" << posi[1] << ",";
    if (posi[2] || posi[3])
      f << "pos1=" << posi[2] << ":" << posi[3] << ",";

    dt = dataList[d++];
    if (!dt.isBasic()) {
      MWAW_DEBUG_MSG(("MRWParser::readZone1: find unexpected f0 type\n"));
      f << "###f0=" << dt << ",";
    } else if (dt.value(0)) // alway 0
      f << "f0=" << dt.value(0) << ",";
    long pos[4];
    for (int j = 0; j < 4; j++) { // ymin?, xmin, ymax?, xmax
      MRWStruct const &data = dataList[d++];
      if (!data.isBasic()) {
        MWAW_DEBUG_MSG(("MRWParser::readZone1: find unexpected posi data type\n"));
        f << "###pos" << j << "=" << data << ",";
        pos[j] = 0;
      } else
        pos[j]=data.value(0);
    }
    if (pos[0]||pos[1]||pos[2]||pos[3])
      f << "pos2=" << pos[1] << "x" << pos[0] << "<->" << pos[3] << "x" << pos[2] << ",";
    dt = dataList[d++];
    if (!dt.isBasic()) {
      MWAW_DEBUG_MSG(("MRWParser::readZone1: find unexpected numChar type\n"));
      f << "###nChar=" << dt << ",";
    } else if (dt.value(0)) // seems ~cPos[1]-cPos[0]
      f << "nChar?=" << std::hex << dt.value(0) << std::dec << ",";

    dt = dataList[d++];
    if (!dt.isBasic()) {
      MWAW_DEBUG_MSG(("MRWParser::readZone1: find unexpected f0 type\n"));
      f << "###f0=" << dt << ",";
    } else if (dt.value(0)) // alway 0
      f << "f0=" << dt.value(0) << ",";
    for (int j = 14; j < 17; j++) { // f1 a small number(cst), f2=some flag, f3=id?
      MRWStruct const &data = dataList[d++];
      if (!data.isBasic()) {
        MWAW_DEBUG_MSG(("MRWParser::readZone1: find unexpected dim data type\n"));
        f << "###f" << j-13 << "=" << data << ",";
      } else if (data.value(0)) {
        if (j!=15)
          f << "f" << j-13 << "=" << data.value(0) << ",";
        else
          f << "f" << j-13 << "=" << std::hex << data.value(0) << std::dec << ",";
      }
    }
    for (int j = 0; j < 4; j++) { // ymin?, xmin, ymax?, xmax \in pos2
      MRWStruct const &data = dataList[d++];
      if (!data.isBasic()) {
        MWAW_DEBUG_MSG(("MRWParser::readZone1: find unexpected posi data type\n"));
        f << "###pos3" << j << "=" << data << ",";
        pos[j] = 0;
      } else
        pos[j]=data.value(0);
    }
    if (pos[0]||pos[1]||pos[2]||pos[3])
      f << "pos3=" << pos[1] << "x" << pos[0] << "<->" << pos[3] << "x" << pos[2] << ",";

    dt = dataList[d++];
    if (!dt.isBasic()) {
      MWAW_DEBUG_MSG(("MRWParser::readZone1: find unexpected f4 type\n"));
      f << "###f4=" << dt << ",";
    } else if (dt.value(0)) // alway 0
      f << "f4=" << dt.value(0) << ",";
    ascii().addNote(f.str().c_str());
  }
  input->seek(entry.end(), WPX_SEEK_SET);

  return true;
}

bool MRWParser::readZoneb(MRWEntry const &entry, int)
{
  if (entry.length() < entry.m_N) {
    MWAW_DEBUG_MSG(("MRWParser::readZoneb: data seems to short\n"));
    return false;
  }
  MWAWInputStreamPtr input = getInput();
  input->seek(entry.begin(), WPX_SEEK_SET);
  input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  decodeZone(dataList, 1+4*entry.m_N);
  input->popLimit();

  if (int(dataList.size()) != 4*entry.m_N) {
    MWAW_DEBUG_MSG(("MRWParser::readZoneb: find unexpected number of data\n"));
    return false;
  }

  libmwaw::DebugStream f;
  size_t d = 0;
  for (int i = 0; i < entry.m_N; i++) {
    f.str("");
    f << entry.name() << "-" << i << ":";
    ascii().addPos(dataList[d].m_filePos);

    for (int j = 0; j < 4; j++) { // always 0, 0, 0, 0?
      MRWStruct const &data = dataList[d++];
      if (!data.isBasic()) {
        MWAW_DEBUG_MSG(("MRWParser::readZoneb: find unexpected dim data type\n"));
        f << "###dim" << j << "=" << data << ",";
      } else if (data.value(0))
        f << "f" << j << "=" << data.value(0) << ",";
    }
    ascii().addNote(f.str().c_str());
  }
  input->seek(entry.end(), WPX_SEEK_SET);

  return true;
}

bool MRWParser::readZonec(MRWEntry const &entry, int)
{
  if (entry.length() < entry.m_N) {
    MWAW_DEBUG_MSG(("MRWParser::readZonec: data seems to short\n"));
    return false;
  }
  MWAWInputStreamPtr input = getInput();
  input->seek(entry.begin(), WPX_SEEK_SET);
  input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  decodeZone(dataList, 1+9*entry.m_N);
  input->popLimit();

  if (int(dataList.size()) != 9*entry.m_N) {
    MWAW_DEBUG_MSG(("MRWParser::readZonec: find unexpected number of data\n"));
    return false;
  }

  libmwaw::DebugStream f;
  size_t d = 0;
  for (int i = 0; i < entry.m_N; i++) {
    f.str("");
    f << entry.name() << "-" << i << ":";
    ascii().addPos(dataList[d].m_filePos);

    for (int j = 0; j < 9; j++) {
      MRWStruct const &data = dataList[d++];
      if (!data.isBasic()) {
        MWAW_DEBUG_MSG(("MRWParser::readZonec: find unexpected dim data type\n"));
        f << "###dim" << j << "=" << data << ",";
      } else if (data.value(0))
        f << "f" << j << "=" << data.value(0) << ",";
    }
    ascii().addNote(f.str().c_str());
  }
  input->seek(entry.end(), WPX_SEEK_SET);

  return true;
}

// --------- print info ----------

// read the print info xml data
bool MRWParser::readCPRT(MRWEntry const &entry)
{
  if (entry.length() < 0x10) {
    MWAW_DEBUG_MSG(("MRWParser::readCPRT: data seems to short\n"));
    return false;
  }

  MWAWInputStreamPtr input = getInput();
  input->seek(entry.begin(), WPX_SEEK_SET);
#ifdef DEBUG_WITH_FILES
  WPXBinaryData file;
  input->readDataBlock(entry.length(), file);

  static int volatile cprtName = 0;
  libmwaw::DebugStream f;
  f << "CPRT" << ++cprtName << ".plist";
  libmwaw::Debug::dumpFile(file, f.str().c_str());

  ascii().skipZone(entry.begin(),entry.end()-1);
#endif

  input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

// read the print info data
bool MRWParser::readPrintInfo(MRWEntry const &entry)
{
  if (entry.length() < 0x77) {
    MWAW_DEBUG_MSG(("MRWParser::readPrintInfo: data seems to short\n"));
    return false;
  }

  MWAWInputStreamPtr input = getInput();
  input->seek(entry.begin(), WPX_SEEK_SET);

  libmwaw::PrinterInfo info;
  if (!info.read(input))
    return false;

  libmwaw::DebugStream f;
  f << "PrintInfo:"<< info;
  Vec2i paperSize = info.paper().size();
  Vec2i pageSize = info.page().size();
  if (pageSize.x() <= 0 || pageSize.y() <= 0 ||
      paperSize.x() <= 0 || paperSize.y() <= 0) return false;

  if (!m_pageSpanSet) {
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
  }

  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());

  input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// low level
////////////////////////////////////////////////////////////

// ---- field decoder ---------
bool MRWParser::readEntryHeader(MRWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  std::vector<long> dataList;
  if (!readNumbersString(4,dataList)||dataList.size()<5) {
    MWAW_DEBUG_MSG(("MRWParser::readEntryHeader: oops can not find header entry\n"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  long length = (dataList[1]<<16)+dataList[2];
  if (length < 0 || !isFilePos(input->tell()+length)) {
    MWAW_DEBUG_MSG(("MRWParser::readEntryHeader: the header data seems to short\n"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  entry.setBegin(input->tell());
  entry.setLength(length);
  entry.m_fileType = (int) int16_t(dataList[0]);
  entry.m_N = (int) dataList[4];
  entry.m_value = (int) dataList[3];

  return true;
}

bool MRWParser::readNumbersString(int num, std::vector<long> &res)
{
  res.resize(0);
  // first read the string
  MWAWInputStreamPtr input = getInput();
  std::string str("");
  while (!input->atEOS()) {
    int ch = int(input->readULong(1));
    if (ch=='-' || (ch >= 'A' && ch <= 'F') || (ch >= '0' && ch <= '9')) {
      str += char(ch);
      continue;
    }
    input->seek(-1, WPX_SEEK_CUR);
    break;
  }
  if (!str.length()) return false;

  // ok we have the string, let decodes it
  size_t sz = str.length(), i = sz;
  int nBytes = 0;
  long val=0;
  while(1) {
    if (i==0) {
      if (nBytes)
        res.insert(res.begin(),val);
      break;
    }
    char c = str[--i];
    if (c=='-') {
      if (!nBytes) {
        MWAW_DEBUG_MSG(("MRWParser::readNumbersString find '-' with no val\n"));
        break;
      }
      res.insert(res.begin(),-val);
      val = 0;
      nBytes = 0;
      continue;
    }

    if (nBytes==num) {
      res.insert(res.begin(),val);
      val = 0;
      nBytes = 0;
    }

    if (c >= '0' && c <= '9')
      val += (long(c-'0')<<(4*nBytes));
    else if (c >= 'A' && c <= 'F')
      val += (long(c+10-'A')<<(4*nBytes));
    else {
      MWAW_DEBUG_MSG(("MRWParser::readNumbersString find odd char %x\n", int(c)));
      break;
    }
    nBytes++;
  }
  return true;
}

bool MRWParser::decodeZone(std::vector<MRWStruct> &dataList, long numData)
{
  dataList.clear();

  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  while (dataList.size() < size_t(numData)) {
    if (input->atEOS())
      break;
    MRWStruct data;
    data.m_filePos = pos;
    int type = int(input->readULong(1));
    data.m_type = (type&3);
    if (type == 3)
      return true;
    if ((type & 0x3c) || (type && !(type&0x3)))
      break;
    if ((type>>4)==0xc) {
      if (input->atEOS()) break;
      int num = int(input->readULong(1));
      if (!num) break;
      for (int j = 0; j < num; j++)
        dataList.push_back(data);
      pos = input->tell();
      continue;
    }
    if ((type>>4)==0x8) {
      dataList.push_back(data);
      pos = input->tell();
      continue;
    }
    std::vector<long> &numbers = data.m_data;
    if (!readNumbersString(data.m_type==1 ? 4: 8, numbers))
      break;
    if (type==0) {
      if (numbers.size() != 1 || numbers[0] < 0 || input->readULong(1) != 0x2c)
        break;
      data.m_pos.setBegin(input->tell());
      data.m_pos.setLength(numbers[0]);
      if (!isFilePos(data.m_pos.end()))
        break;
      input->seek(data.m_pos.end(), WPX_SEEK_SET);
      numbers.resize(0);
    }

    dataList.push_back(data);
    pos = input->tell();
  }
  input->seek(pos, WPX_SEEK_SET);
  return dataList.size();
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool MRWParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = MRWParserInternal::State();

  long const headerSize=0x2e;
  if (!isFilePos(headerSize)) {
    MWAW_DEBUG_MSG(("MRWParser::checkHeader: file is too short\n"));
    return false;
  }
  MWAWInputStreamPtr input = getInput();
  input->seek(0,WPX_SEEK_SET);

  int actZone = -1;
  if (!readZone(actZone))
    return false;
  if (strict && !readZone(actZone))
    return false;

  input->seek(0, WPX_SEEK_SET);
  if (header)
    header->reset(MWAWDocument::MARIW, 1);

  return true;
}

////////////////////////////////////////////////////////////
// MRWEntry/MRWStruct function
////////////////////////////////////////////////////////////
std::string MRWEntry::name() const
{
  switch(m_fileType) {
  case -1:
    return "Separator";
  case -2:
    return "EndZone";
  case 2:
    return "TEXT";
  case 4:
    return "CharPLC";
  case 5:
    return "ParagPLC";
  case 6:
    return "Fonts";
  case 7:
    return "Paragraphs";
  case 8:
    return "FontNames";
  case 9:
    return "SepDim0";
  case 0xa:
    return "SepDim1";
  case 0x14: // token, picture, ...
    return "Token";
  case 0x1a:
    return "StyleNames";
  case 0x1f:
    return "PrintInfo";
  case 0x24:
    return "CPRT";
  case 0x41a:
    return "DocInfo";
  case 0x420:
    return "PSFile";
  default:
    break;
  }
  std::stringstream s;
  if (m_fileType >= 0)
    s << "Zone" << std::hex << std::setfill('0') << std::setw(2) << m_fileType << std::dec;
  else
    s << "Zone-" << std::hex << std::setfill('0') << std::setw(2) << -m_fileType << std::dec;

  return s.str();
}

long MRWStruct::value(int i) const
{
  if (i < 0 || i >= int(m_data.size())) {
    if (i) {
      MWAW_DEBUG_MSG(("MRWStruct::value: can not find value %d\n", i));
    }
    return 0;
  }
  return m_data[size_t(i)];
}
std::ostream &operator<<(std::ostream &o, MRWStruct const &dt)
{
  switch(dt.m_type) {
  case 0: // data
    o << "sz=" << std::hex << dt.m_pos.length() << std::dec;
    return o;
  case 3: // end of data
    return o;
  case 1: // int?
  case 2: // long?
    break;
  default:
    if (dt.m_type) o << ":" << dt.m_type;
    break;
  }
  size_t numData = dt.m_data.size();
  if (!numData) {
    o << "_";
    return o;
  }
  if (numData > 1) o << "[";
  for (size_t d = 0; d < numData; d++) {
    long val = dt.m_data[d];
    if (val > -100 && val < 100)
      o << val;
    else if (val > 0)
      o << "0x" << std::hex << val << std::dec;
    else
      o << "-0x" << std::hex << -val << std::dec;
    if (d+1 != numData) o << ",";
  }
  if (numData > 1) o << "]";
  return o;
}


// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
