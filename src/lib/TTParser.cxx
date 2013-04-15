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
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPictData.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSubDocument.hxx"

#include "TTParser.hxx"

/** Internal: the structures of a TTParser */
namespace TTParserInternal
{
////////////////////////////////////////
//! Internal: the state of a TTParser
struct State {
  //! constructor
  State() : m_type(MWAWDocument::UNKNOWN), m_posFontMap(), m_idPictEntryMap(), m_numberSpacesForTab(0),
    m_eof(-1), m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0) {
  }
  //! the file type
  MWAWDocument::DocumentType m_type;
  //! the map of id -> font
  std::map<long, MWAWFont > m_posFontMap;
  //! a map id->pictEntry
  std::map<int,MWAWEntry> m_idPictEntryMap;
  //! number of space to used to replace tab (0: means keep tabs )
  int m_numberSpacesForTab;
  //! the last datafork position
  long m_eof;
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
TTParser::TTParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWParser(input, rsrcParser, header), m_state()
{
  init();
}

TTParser::~TTParser()
{
}

void TTParser::init()
{
  resetListener();
  setAsciiName("main-1");

  m_state.reset(new TTParserInternal::State);

  getPageSpan().setMargins(0.1);
}

void TTParser::setListener(MWAWContentListenerPtr listen)
{
  MWAWParser::setListener(listen);
}

MWAWInputStreamPtr TTParser::rsrcInput()
{
  return getRSRCParser()->getInput();
}

libmwaw::DebugFile &TTParser::rsrcAscii()
{
  return getRSRCParser()->ascii();
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void TTParser::newPage(int number)
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
void TTParser::parse(WPXDocumentInterface *docInterface)
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
      sendText();
#ifdef DEBUG
      flushExtra();
#endif
    }
    ascii().reset();
  } catch (...) {
    MWAW_DEBUG_MSG(("TTParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void TTParser::createDocument(WPXDocumentInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getListener()) {
    MWAW_DEBUG_MSG(("TTParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  std::vector<MWAWPageSpan> pageList;
  int numPages=computeNumPages();
  m_state->m_numPages = numPages;
  MWAWPageSpan ps(getPageSpan());
  for (int i = 0; i <= numPages; i++)
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
bool TTParser::createZones()
{
  MWAWInputStreamPtr input = getInput();
  std::string type, creator;
  MWAWRSRCParserPtr rsrcParser = getRSRCParser();
  std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::iterator it;

  // the chapter zone zone
  it = entryMap.lower_bound("styl");
  while (it != entryMap.end()) {
    if (it->first != "styl")
      break;
    MWAWEntry const &entry = it++->second;
    readStyles(entry);
  }
  // the different pict zones
  it = entryMap.lower_bound("PICT");
  while (it != entryMap.end()) {
    if (it->first != "PICT")
      break;
    MWAWEntry const &entry = it++->second;
    m_state->m_idPictEntryMap[entry.id()]=entry;
  }
  // a unknown zone in Tex-Edit with id=1000
  it = entryMap.lower_bound("wrct");
  while (it != entryMap.end()) {
    if (it->first != "wrct")
      break;
    MWAWEntry const &entry = it++->second;
    readWRCT(entry);
  }
  // we can have also some sound in snd:10000
  /** checkme: find also two times BBSR:0x250 with size 0x168,
      probably not in the format.. */
  return true;
}

void TTParser::flushExtra()
{
#ifdef DEBUG
  MWAWRSRCParserPtr rsrcParser = getRSRCParser();
  std::map<int,MWAWEntry>::const_iterator it = m_state->m_idPictEntryMap.begin();
  for ( ; it != m_state->m_idPictEntryMap.end(); it++) {
    MWAWEntry const &entry=it->second;
    if (entry.isParsed()) continue;
    sendPicture(entry.id());
  }
#endif
}

int TTParser::computeNumPages() const
{
  MWAWInputStreamPtr input = const_cast<TTParser *>(this)->getInput();
  input->seek(0, WPX_SEEK_SET);
  int nPages=1;
  int pageBreakChar=(m_state->m_type==MWAWDocument::TEDIT) ? 0xc : 0;

  while(!input->atEOS()) {
    if (input->readLong(1)==pageBreakChar)
      nPages++;
  }
  m_state->m_eof=input->tell();
  return nPages;
}

bool TTParser::sendText()
{
  if (!getListener()) {
    MWAW_DEBUG_MSG(("DMText::sendText: can not find the listener\n"));
    return false;
  }
  MWAWInputStreamPtr input = getInput();
  input->seek(0, WPX_SEEK_SET);
  long debPos=0;

  libmwaw::DebugStream f;
  f << "Entries(TEXT):";
  getListener()->setFont(MWAWFont(3,12));

  std::map<long, MWAWFont >::const_iterator fontIt;
  int nPict=0;
  unsigned char pageBreakChar=(m_state->m_type==MWAWDocument::TEDIT) ? 0xc : 0;

  int actPage=1;
  for (long i=0; i < m_state->m_eof; i++) {
    bool endPos = input->atEOS();
    unsigned char c=endPos ? (unsigned char)0 : (unsigned char) input->readULong(1);
    if (endPos || c==0xd || c==pageBreakChar) {
      ascii().addPos(debPos);
      ascii().addNote(f.str().c_str());
      debPos = input->tell();
      if (endPos) break;
      f.str("");
      f << "TEXT:";
    }
    fontIt=m_state->m_posFontMap.find(i);
    if (fontIt != m_state->m_posFontMap.end()) {
      f << "[Style,cPos=" << std::hex << i << std::dec << "]";
      getListener()->setFont(fontIt->second);
    }
    if (c)
      f << c;
    if (c==pageBreakChar) {
      newPage(++actPage);
      continue;
    }
    if (c==0 && m_state->m_type==MWAWDocument::TEDIT && !endPos) {
      // tex-edit accept control character, ...
      unsigned char nextC=(unsigned char) input->readULong(1);
      if (nextC < 0x20) {
        i++;
        getListener()->insertChar('^');
        getListener()->insertChar(uint8_t('@'+nextC));
        continue;
      }
      input->seek(-1, WPX_SEEK_CUR);
    }
    switch(c) {
    case 0x9:
      if (m_state->m_numberSpacesForTab>0) {
        for (int j = 0; j < m_state->m_numberSpacesForTab; j++)
          getListener()->insertChar(' ');
      } else
        getListener()->insertTab();
      break;
    case 0xd:
      getListener()->insertEOL();
      break;
    case 0x11: // command key
      getListener()->insertUnicode(0x2318);
      break;
    case 0x14: // apple logo: check me
      getListener()->insertUnicode(0xf8ff);
      break;
    case 0xca:
      sendPicture(1000+nPict++);
      break;
    default:
      if (c < 0x20) f  << "##[" << std::hex << int(c) << std::dec << "]";
      i += getListener()->insertCharacter(c, input, m_state->m_eof);
      break;
    }
  }
  return true;
}
////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

// the styles
bool TTParser::readStyles(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<2) {
    MWAW_DEBUG_MSG(("TTParser::readStyles: the entry is bad\n"));
    return false;
  }
  entry.setParsed(true);
  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  int N=(int) input->readULong(2);
  f << "Entries(Style)[" << entry.type() << "-" << entry.id() << "]:N="<<N;
  if (20*N+2 != entry.length()) {
    MWAW_DEBUG_MSG(("TTParser::readStyles: the number of values seems bad\n"));
    f << "###";
    ascFile.addPos(pos-4);
    ascFile.addNote(f.str().c_str());
  }
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  for (int i = 0; i < N; i++) {
    MWAWFont font;
    f.str("");
    pos = input->tell();
    long cPos = input->readLong(4);
    int dim[2];
    for (int j = 0; j < 2; j++)
      dim[j] = (int) input->readLong(2);
    f << "height?=" << dim[0] << ":" << dim[1] << ",";
    font.setId((int) input->readULong(2));
    int flag=(int) input->readULong(1);
    uint32_t flags = 0;
    if (flag&0x1) flags |= MWAWFont::boldBit;
    if (flag&0x2) flags |= MWAWFont::italicBit;
    if (flag&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (flag&0x8) flags |= MWAWFont::embossBit;
    if (flag&0x10) flags |= MWAWFont::shadowBit;
    if (flag&0x20) font.setDeltaLetterSpacing(-1.);
    if (flag&0x40) font.setDeltaLetterSpacing(1.);
    if (flag&0x80) f << "#flags=" << std::hex << (flag&0x80) << std::dec << ",";
    flag=(int) input->readULong(1);
    if (flag) f << "#flags1=" << std::hex << flag << std::dec << ",";
    font.setSize((float)input->readULong(2));
    font.setFlags(flags);
    unsigned char col[3];
    for (int j=0; j < 3; j++)
      col[j] = (unsigned char) (input->readULong(2)>>8);
    font.setColor(MWAWColor(col[0],col[1],col[2]));
    font.m_extra=f.str();
    if (m_state->m_posFontMap.find(cPos) != m_state->m_posFontMap.end()) {
      MWAW_DEBUG_MSG(("TTParser::readStyles: a style for pos=%lx already exist\n", cPos));
    } else
      m_state->m_posFontMap[cPos] = font;
    f.str("");
    f << "Style-" << i << ":" << "cPos=" << std::hex << cPos << std::dec << ",";
#ifdef DEBUG
    f << ",font=[" << font.getDebugString(getFontConverter()) << "]";
#endif
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

// unknown structure 4 int, related to some counter ?
bool TTParser::readWRCT(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<8) {
    MWAW_DEBUG_MSG(("TTParser::readWRCT: the entry is bad\n"));
    return false;
  }
  entry.setParsed(true);
  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  // f0=27|29|2c, f1=1|3|7, f2xf3: big number, a dim?
  f << "Entries(WRCT)[" << entry.type() << "-" << entry.id() << "]:";
  for (int i = 0; i < 4; i++)
    f << "f" << i << "=" << input->readLong(2) << ",";

  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

// the pictures
bool TTParser::sendPicture(int id)
{
  if (m_state->m_idPictEntryMap.find(id)==m_state->m_idPictEntryMap.end()) {
    MWAW_DEBUG_MSG(("TTParser::sendPicture: can not find picture for id=%d\n",id));
    return false;
  }
  if (!getListener()) {
    MWAW_DEBUG_MSG(("TTParser::sendPicture: can not find the listener\n"));
    return false;
  }

  MWAWInputStreamPtr input = rsrcInput();
  MWAWRSRCParserPtr rsrcParser = getRSRCParser();
  MWAWEntry const &entry=m_state->m_idPictEntryMap.find(id)->second;

  WPXBinaryData data;
  long pos = input->tell();
  rsrcParser->parsePICT(entry,data);
  input->seek(pos,WPX_SEEK_SET);

  int dataSz=int(data.size());
  if (!dataSz) {
    return false;
  }
  WPXInputStream *dataInput = const_cast<WPXInputStream *>(data.getDataStream());
  if (!dataInput) {
    MWAW_DEBUG_MSG(("TTParser::sendPicture: oops can not find an input\n"));
    return false;
  }
  MWAWInputStreamPtr pictInput(new MWAWInputStream(dataInput, false));
  Box2f box;
  MWAWPict::ReadResult res = MWAWPictData::check(pictInput, dataSz,box);
  if (res == MWAWPict::MWAW_R_BAD) {
    MWAW_DEBUG_MSG(("TTParser::sendPicture: can not find the picture\n"));
    return false;
  }
  dataInput->seek(0,WPX_SEEK_SET);
  shared_ptr<MWAWPict> thePict(MWAWPictData::get(pictInput, dataSz));
  MWAWPosition pictPos=MWAWPosition(Vec2f(0,0),box.size(), WPX_POINT);
  pictPos.setRelativePosition(MWAWPosition::Paragraph, MWAWPosition::XCenter);
  pictPos.m_wrapping = MWAWPosition::WRunThrough;
  if (thePict) {
    WPXBinaryData fData;
    std::string type;
    if (thePict->getBinary(fData,type))
      getListener()->insertPicture(pictPos, fData, type);
  }
  return true;
}


////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool TTParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = TTParserInternal::State();
  /** no data fork, may be ok, but this means
      that the file contains no text, so... */
  MWAWInputStreamPtr input = getInput();
  if (!input || !getRSRCParser() || !input->hasDataFork())
    return false;

  std::string type, creator;
  if (!input->getFinderInfo(type, creator))
    return false;
  MWAWDocument::DocumentType fileType(MWAWDocument::UNKNOWN);
  if (creator=="ttxt") {
    fileType=MWAWDocument::TEACH;
    m_state->m_numberSpacesForTab=2;
  } else if (creator=="TBB5")
    fileType=MWAWDocument::TEDIT;
  else
    return false;
  if (strict && fileType==MWAWDocument::TEACH && type!="ttro") {
    /** visibly, some other applications can create ttxt file,
    so check that we have at least a styl rsrc or a PICT */
    MWAWEntry entry = getRSRCParser()->getEntry("styl", 128);
    if (!entry.valid()) {
      entry = getRSRCParser()->getEntry("PICT", 1000);
      MWAW_DEBUG_MSG(("TTParser::checkHeader: can not find any basic ressource, stop\n"));
      if (!entry.valid())
        return false;
    }
  }
  m_state->m_type=fileType;
  setVersion(1);
  if (header)
    header->reset(fileType, version());

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
