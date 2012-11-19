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

#include "HMWParser.hxx"

/** Internal: the structures of a HMWParser */
namespace HMWParserInternal
{
////////////////////////////////////////
//! Internal: class to store a small zone of HMWParser
struct Zone {
  //! constructor
  Zone() : m_type(-1), m_filePos(-1), m_data(), m_input(), m_asciiFile(), m_parsed(false) {
  }
  //! destructor
  ~Zone() {
    ascii().reset();
  }

  //! returns the debug file
  libmwaw::DebugFile &ascii() {
    return m_asciiFile;
  }

  //! the type : 1(text), ....
  int m_type;

  //! the begin of the entry
  long m_filePos;

  //! the storage
  WPXBinaryData m_data;

  //! the main input
  MWAWInputStreamPtr m_input;

  //! the debug file
  libmwaw::DebugFile m_asciiFile;

  //! true if the zone is sended
  bool m_parsed;
};


////////////////////////////////////////
//! Internal: the state of a HMWParser
struct State {
  //! constructor
  State() : m_zonesListBegin(-1), m_eof(-1), m_zonesMap(),
    m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0) {
  }

  //! the list of zone begin
  long m_zonesListBegin;
  //! end of file
  long m_eof;
  //! a map of entry: type->entry
  std::multimap<int,MWAWEntry> m_zonesMap;

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;
  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a MWParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(HMWParser &pars, MWAWInputStreamPtr input, int zoneId) :
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
  HMWContentListener *listen = dynamic_cast<HMWContentListener *>(listener.get());
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
  //reinterpret_cast<HMWParser *>(m_parser)->sendZone(m_id);
  m_input->seek(pos, WPX_SEEK_SET);
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
HMWParser::HMWParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWParser(input, rsrcParser, header), m_listener(), m_convertissor(), m_state(),
  m_pageSpan()
{
  init();
}

HMWParser::~HMWParser()
{
}

void HMWParser::init()
{
  m_convertissor.reset(new MWAWFontConverter);
  m_listener.reset();
  setAsciiName("main-1");

  m_state.reset(new HMWParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  m_pageSpan.setMarginTop(0.1);
  m_pageSpan.setMarginBottom(0.1);
  m_pageSpan.setMarginLeft(0.1);
  m_pageSpan.setMarginRight(0.1);
}

void HMWParser::setListener(HMWContentListenerPtr listen)
{
  m_listener = listen;
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
float HMWParser::pageHeight() const
{
  return float(m_pageSpan.getFormLength()-m_pageSpan.getMarginTop()-m_pageSpan.getMarginBottom()-m_state->m_headerHeight/72.0-m_state->m_footerHeight/72.0);
}

float HMWParser::pageWidth() const
{
  return float(m_pageSpan.getFormWidth()-m_pageSpan.getMarginLeft()-m_pageSpan.getMarginRight());
}


////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void HMWParser::newPage(int number)
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

bool HMWParser::isFilePos(long pos)
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
void HMWParser::parse(WPXDocumentInterface *docInterface)
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
      if (m_listener) m_listener->endDocument();
      m_listener.reset();
    }
    ascii().reset();
  } catch (...) {
    MWAW_DEBUG_MSG(("HMWParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void HMWParser::createDocument(WPXDocumentInterface *documentInterface)
{
  if (!documentInterface) return;
  if (m_listener) {
    MWAW_DEBUG_MSG(("HMWParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  int numPage = 0;
  m_state->m_numPages = numPage+1;

  // create the page list
  std::vector<MWAWPageSpan> pageList;
  MWAWPageSpan ps(m_pageSpan);

  for (int i = 0; i <= m_state->m_numPages; i++) pageList.push_back(ps);

  //
  HMWContentListenerPtr listen(new HMWContentListener(pageList, documentInterface));
  setListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool HMWParser::createZones()
{
  if (!HMWParser::readZonesList())
    return false;

  libmwaw::DebugStream f;
  std::multimap<int,MWAWEntry>::iterator it;
  for (it = m_state->m_zonesMap.begin(); it !=m_state->m_zonesMap.end(); it++)
    readZone(it->second);
  for (it = m_state->m_zonesMap.begin(); it !=m_state->m_zonesMap.end(); it++) {
    MWAWEntry &entry = it->second;
    if (entry.isParsed())
      continue;
    f.str("");
    f << "Entries(Zone" << std::hex << entry.id() << std::dec << "):";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
  }
  return false;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////
bool HMWParser::readZonesList()
{
  if (m_state->m_zonesListBegin <= 0 || !isFilePos(m_state->m_zonesListBegin)) {
    MWAW_DEBUG_MSG(("HMWParser::readZonesList: the list entry is not set \n"));
    return false;
  }
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;

  long debZone = m_state->m_zonesListBegin;
  std::set<long> seeDebZone;
  while (debZone) {
    if (seeDebZone.find(debZone) != seeDebZone.end()) {
      MWAW_DEBUG_MSG(("HMWParser::readZonesList: oops, we have already see this zone\n"));
      break;
    }
    seeDebZone.insert(debZone);
    long pos = debZone;
    input->seek(pos, WPX_SEEK_SET);
    int numZones = int(input->readULong(1));
    f.str("");
    f << "Entries(Zones):";
    f << "N=" << numZones << ",";
    if (!numZones || !isFilePos(pos+16*(numZones+1))) {
      MWAW_DEBUG_MSG(("HMWParser::readZonesList: can not read the number of zones\n"));
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      break;
    }
    int val;
    for (int i = 0; i < 3; i++) {
      val = int(input->readLong(1));
      if (val) f << "f" << i << "=" << val << ",";
    }
    long ptr = long(input->readULong(4));
    if (ptr != debZone) {
      MWAW_DEBUG_MSG(("HMWParser::readZonesList: can not read the zone begin ptr\n"));
      f << "#ptr=" << std::hex << ptr << std::dec << ",";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      break;
    }
    long nextPtr = long(input->readULong(4));
    if (nextPtr) {
      f << "nextPtr=" << std::hex << nextPtr << std::dec;
      if (!isFilePos(nextPtr)) {
        MWAW_DEBUG_MSG(("HMWParser::readZonesList: can not read the next zone begin ptr\n"));
        nextPtr = 0;
        f << "###";
      }
      f << ",";
    }
    for (int i = 0; i < 2; i++) { // always 0,0?
      val = int(input->readLong(2));
      if (val) f << "f" << i+3 << "=" << val << ",";
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+16, WPX_SEEK_SET);

    for (int i = 0; i < numZones; i++) {
      pos = input->tell();
      f.str("");
      f << "Zones-" << i << ":";
      int type = int(input->readLong(2));
      f << "type=" << type << ",";
      val = int(input->readLong(2));
      if (val) f << "f0=" << val << ",";
      ptr = long(input->readULong(4));
      f << "ptr=" << std::hex << ptr << ",";
      if (!isFilePos(ptr)) {
        MWAW_DEBUG_MSG(("HMWParser::readZonesList: can not read the %d zone address\n", i));
        f << "#";
      } else {
        MWAWEntry entry;
        entry.setBegin(ptr);
        entry.setId(type);
        m_state->m_zonesMap.insert
        (std::multimap<int,MWAWEntry>::value_type(type,entry));
      }
      ascii().addDelimiter(input->tell(), '|');
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      input->seek(pos+16, WPX_SEEK_SET);
    }

    ascii().addPos(input->tell());
    ascii().addNote("_");
    if (!nextPtr) break;
    debZone = nextPtr;
  }
  return m_state->m_zonesMap.size();
}

bool HMWParser::readZone(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);
  f << "Entries(Zone" << std::hex << entry.id() << std::dec << "):";
  int n = int(input->readLong(2));
  f << "n?=" << n << ",";
  long val = input->readLong(2);
  if (val) f << "unkn=" << val << ",";

  entry.setParsed(true);
  long totalSz = (long) input->readULong(4);
  long dataSz = (long) input->readULong(4);
  if (totalSz != dataSz+12 || !isFilePos(pos+totalSz)) {
    MWAW_DEBUG_MSG(("HMWParser::readZone: can not read the zone size\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  entry.setLength(totalSz);
  shared_ptr<HMWParserInternal::Zone> zone=decodeZone(entry);
  if (!zone)
    return false;

  /** type1: text, type7: printInfo: typed: graphic */
  f.str("");
  f << "Zone" << std::hex << entry.id() << std::dec << "[data]:";
  zone->ascii().addPos(0);
  zone->ascii().addNote(f.str().c_str());

  return true;
}

/* implementation of a basic splay tree to decode a block
   freely inspired from: ftp://ftp.cs.uiowa.edu/pub/jones/compress/minunsplay.c

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
shared_ptr<HMWParserInternal::Zone> HMWParser::decodeZone(MWAWEntry const &entry)
{
  shared_ptr<HMWParserInternal::Zone> zone;
  if (!entry.valid() || entry.length() <= 12) {
    MWAW_DEBUG_MSG(("HMWParser::decodeZone: called with invalid entry\n"));
    return zone;
  }
  zone.reset(new HMWParserInternal::Zone);
  zone->m_type = entry.id();
  zone->m_filePos = entry.begin();
  short const maxChar=256;
  short const maxSucc=maxChar+1;
  short const twoMaxChar=2*maxChar+1;
  short const twoMaxSucc=2*maxSucc;

  // first build the tree data
  short int left[maxSucc];
  short int right[maxSucc];
  short int up[twoMaxSucc];
  for (short i = 0; i <= twoMaxChar; ++i)
    up[i] = i/2;
  for (short j = 0; j <= maxChar; ++j) {
    left[j] = 2 * j;
    right[j] = 2 * j + 1;
  }

  short const root = 0;
  short const sizeBit = 8;
  short const highBit=128; /* mask for the most sig bit of 8 bit byte */

  short bitbuffer;       /* buffer to hold a byte for unpacking bits */
  short bitcounter = 0;  /* count of remaining bits in buffer */

  MWAWInputStreamPtr input = getInput();
  input->seek(entry.begin()+12, WPX_SEEK_SET);
  WPXBinaryData &dt = zone->m_data;
  while (!input->atEOS() && input->tell() < entry.end()) {
    short a = root;
    bool ok = true;
    do {  /* once for each bit on path */
      if(bitcounter == 0) {
        if (input->atEOS() || input->tell() >= entry.end()) {
          MWAW_DEBUG_MSG(("HMWParser::decodeZone: find some uncomplete data for zone%lx\n", entry.begin()));
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
      short b,c,d;
      if ((c = up[a]) != root) {      /* a pair remains */
        d = up[c]; //# ->-2(A6)
        b = left[d];
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
    MWAW_DEBUG_MSG(("HMWParser::decodeZone: oops an empty zone\n"));
    zone.reset();
    return zone;
  }

  WPXInputStream *dataInput =
    const_cast<WPXInputStream *>(zone->m_data.getDataStream());
  if (!dataInput) {
    MWAW_DEBUG_MSG(("HMWParser::decodeZone: can not find my input\n"));
    zone.reset();
    return zone;
  }

  zone->m_input.reset(new MWAWInputStream(dataInput, false));
  zone->m_asciiFile.setStream(zone->m_input);
  std::stringstream s;
  s << "Block" << std::hex << entry.begin() << std::dec;
  zone->m_asciiFile.open(s.str());

  ascii().skipZone(entry.begin()+12, entry.length()-1);
  return zone;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool HMWParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = HMWParserInternal::State();

  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  f << "FileHeader:";
  long const headerSize=0x33c;
  if (!isFilePos(headerSize)) {
    MWAW_DEBUG_MSG(("HMWParser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0,WPX_SEEK_SET);
  bool ok = input->readULong(2) == 0x4859;
  ok = ok && input->readULong(2) == 0x4c53;
  ok = ok && input->readULong(2) == 0x0210;
  if (!ok)
    return false;
  int val = (int) input->readLong(1);
  if (val) {
    if (strict) return false;
    if (val==1) f << "hasPassword,";
    else f << "#hasPassword=" << val << ",";
  }
  val = (int) input->readLong(1);
  if (val) {
    if (strict) return false;
    f << "f0=" << val << ",";
  }
  m_state->m_zonesListBegin = (int) input->readULong(4); // always 0x042c ?
  if (m_state->m_zonesListBegin<0x14 || !isFilePos(m_state->m_zonesListBegin))
    return false;
  if (m_state->m_zonesListBegin < 0x33c) {
    MWAW_DEBUG_MSG(("HMWParser::checkHeader: the header size seems short\n"));
  }
  f << "zonesBeg=" << std::hex << m_state->m_zonesListBegin << std::dec << ",";
  long fLength = long(input->readULong(4));
  if (fLength < m_state->m_zonesListBegin)
    return false;
  if (!isFilePos(fLength)) {
    if (!isFilePos(fLength/2)) return false;
    MWAW_DEBUG_MSG(("HMWParser::checkHeader: file seems incomplete, try to continue\n"));
    f << "#len=" << std::hex << fLength << std::dec << ",";
  }
  long tLength = long(input->readULong(4));
  f << "textLength=" << tLength << ",";

  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  long pos;
  // title, subject, author, revision, remark, [2 documents tags], mail:
  int const fieldSizes[] = { 128, 128, 32, 32, 256, 40, 64, 64, 64 };
  for (int i = 0; i < 9; i++) {
    pos=input->tell();
    if (i == 5) {
      ascii().addPos(pos);
      ascii().addNote("FileHeader[DocTags]:");
      input->seek(pos+fieldSizes[i], WPX_SEEK_SET);
      continue;
    }
    int fSz = (int) input->readULong(1);
    if (fSz >= fieldSizes[i]) {
      if (strict)
        return false;
      MWAW_DEBUG_MSG(("HMWParser::checkHeader: can not read field size %i\n", i));
      ascii().addPos(pos);
      ascii().addNote("FileHeader#");
      input->seek(pos+fieldSizes[i], WPX_SEEK_SET);
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
    input->seek(pos+fieldSizes[i], WPX_SEEK_SET);
  }
  pos=input->tell();
  f.str("");
  f << "FileHeader(II):"; // unknown 240 bytes
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(m_state->m_zonesListBegin, WPX_SEEK_SET);
  if (header)
    header->reset(MWAWDocument::HMAC, 1);

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
