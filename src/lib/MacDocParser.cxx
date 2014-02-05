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

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWTextListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPictData.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSubDocument.hxx"

#include "libmwaw_internal.hxx"

#include "MacDocParser.hxx"

/** Internal: the structures of a MacDocParser */
namespace MacDocParserInternal
{
////////////////////////////////////////
//! Internal: the index data of a MacDocParser
struct Index {
  //! constructor
  Index() : m_entry(), m_level(0), m_numChild(0), m_page(0), m_box(), m_extra("")
  {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Index const &index)
  {
    if (index.m_level) o << "level=" << index.m_level << ",";
    if (index.m_page) o << "page=" << index.m_page << ",";
    o << "box=" << index.m_box << ",";
    if (index.m_numChild) o << "numChild=" << index.m_numChild << ",";
    o << index.m_extra;
    return o;
  }
  //! the text entry
  MWAWEntry m_entry;
  //! the entry level
  int m_level;
  //! the number of child
  int m_numChild;
  //! the page
  int m_page;
  //! the bdbox
  Box2i m_box;
  //! extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the state of a MacDocParser
struct State {
  //! constructor
  State() : m_idPictureMap(), m_indexList(), m_idFontMap(), m_actPage(0), m_numPages(0)
  {
  }
  //! the picture page map
  std::map<int,MWAWEntry> m_idPictureMap;
  //! the index list
  std::vector<Index> m_indexList;
  //! a map id to index font
  std::map<int, MWAWFont> m_idFontMap;
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MacDocParser::MacDocParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWTextParser(input, rsrcParser, header), m_state()
{
  init();
}

MacDocParser::~MacDocParser()
{
}

void MacDocParser::init()
{
  resetTextListener();

  m_state.reset(new MacDocParserInternal::State);

  // no margins ( ie. the document is a set of picture corresponding to each page )
  getPageSpan().setMargins(0.01);
}

MWAWInputStreamPtr MacDocParser::rsrcInput()
{
  return getRSRCParser()->getInput();
}

libmwaw::DebugFile &MacDocParser::rsrcAscii()
{
  return getRSRCParser()->ascii();
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void MacDocParser::newPage(int number)
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

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void MacDocParser::parse(librevenge::RVNGTextInterface *docInterface)
{
  assert(getInput().get() != 0 && getRSRCParser());

  if (!checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = false;
  try {
    checkHeader(0L);
    ok = createZones();
    if (ok) {
      createDocument(docInterface);
      sendContents();
    }
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("MacDocParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetTextListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void MacDocParser::createDocument(librevenge::RVNGTextInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getTextListener()) {
    MWAW_DEBUG_MSG(("MacDocParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  int numPages = int(m_state->m_idPictureMap.size());
  if (!m_state->m_indexList.empty())
    numPages++;
  MWAWPageSpan ps(getPageSpan());
  ps.setPageSpan(numPages+1);
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
bool MacDocParser::createZones()
{
  MWAWRSRCParserPtr rsrcParser = getRSRCParser();
  std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::iterator it;

  // the index font zone: 1001, ...
  it = entryMap.lower_bound("MDLv");
  while (it != entryMap.end()) {
    if (it->first != "MDLv")
      break;
    MWAWEntry const &entry = it++->second;
    readFont(entry);
  }
  // index zone: 1 MDIx
  it = entryMap.lower_bound("MDIx");
  while (it != entryMap.end()) {
    if (it->first != "MDIx")
      break;
    MWAWEntry const &entry = it++->second;
    readIndex(entry);
  }
  // bookmark zone: Mdbk 1, ...
  it = entryMap.lower_bound("MDbk");
  while (it != entryMap.end()) {
    if (it->first != "MDbk")
      break;
    MWAWEntry const &entry = it++->second;
    readBookmark(entry);
  }
  // doc preference MDop:128 crypted ?

  // the picture zone: 1, ...
  bool pageSizeSet=false;
  it = entryMap.lower_bound("MDpg");
  while (it != entryMap.end()) {
    if (it->first != "MDpg")
      break;
    MWAWEntry const &entry = it++->second;
    m_state->m_idPictureMap[entry.id()]=entry;
    if (!pageSizeSet) {
      // as we do not read MDop, use picture to find page size
      librevenge::RVNGBinaryData data;
      if (!getRSRCParser()->parsePICT(entry, data))
        continue;
      MWAWInputStreamPtr pictInput=MWAWInputStream::get(data, false);
      if (!pictInput)
        continue;
      Box2f box;
      MWAWPict::ReadResult res = MWAWPictData::check(pictInput,(int) data.size(), box);
      if (res != MWAWPict::MWAW_R_BAD && box.size()[0]>0 && box.size()[1]>0) {
        pageSizeSet=true;
        getPageSpan().setFormWidth(double(box.size()[0])/72.);
        getPageSpan().setFormLength(double(box.size()[1])/72.);
      }
    }
  }
  // windows pos? 128
  it = entryMap.lower_bound("MDwp");
  while (it != entryMap.end()) {
    if (it->first != "MDwp")
      break;
    MWAWEntry const &entry = it++->second;
    readWP(entry);
  }

#ifdef DEBUG_WITH_FILES
  // the file zone: 1, ...
  it = entryMap.lower_bound("MDfi");
  while (it != entryMap.end()) {
    if (it->first != "MDfi")
      break;
    MWAWEntry const &entry = it++->second;
    readFile(entry);
  }

  // get rid of the default application resource
  libmwaw::DebugFile &ascFile = rsrcAscii();
  static char const *(appliRsrc[])= {
    "ALRT","BNDL","CNTL","CURS","CDEF", "CODE","DLOG","DLGX","DITL","FREF",
    "ICON","ICN#","MENU","MBAR","MDEF", "SIZE","TMPL","WIND",
    "acur","cicn","crsr","dctb","icl4", "icl8","ics4","ics8","ics#","ictb",
    "mstr","snd ",
    "DATA", "MDsr" /* srd: version string */
  };
  for (int r=0; r < 18+12+2; ++r) {
    it = entryMap.lower_bound(appliRsrc[r]);
    while (it != entryMap.end()) {
      if (it->first != appliRsrc[r])
        break;
      MWAWEntry const &entry = it++->second;
      if (entry.isParsed()) continue;
      entry.setParsed(true);
      ascFile.skipZone(entry.begin()-4,entry.end()-1);
    }
  }
#endif

  return !m_state->m_idPictureMap.empty();
}


bool MacDocParser::sendContents()
{
  MWAWTextListenerPtr listener=getTextListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MacDocParser::sendContents: can not find the listener\n"));
    return false;
  }
  int actPage=0;
  if (sendIndex())
    newPage(++actPage);
  listener->setParagraph(MWAWParagraph());
  std::map<int,MWAWEntry>::const_iterator it=m_state->m_idPictureMap.begin();
  for (; it!=m_state->m_idPictureMap.end(); ++it) {
    sendPicture(it->second);
    newPage(++actPage);
  }
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

// font
bool MacDocParser::readFont(MWAWEntry const &entry)
{
  if (entry.length()<12) {
    MWAW_DEBUG_MSG(("MacDocParser::readFont: the entry seems bad\n"));
    return false;
  }

  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile = rsrcAscii();
  libmwaw::DebugStream f;
  int fSz=(int) input->readULong(1);
  if (fSz<0 || 1+long(fSz)+1-(fSz%2)+10>entry.length()) {
    f << "Entries(Font):###fSz=" << fSz;
    ascFile.addPos(entry.begin()-4);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  MWAWFont font;
  std::string name("");
  for (int i=0; i<fSz; i++)
    name+=(char) input->readLong(1);
  font.setId(getParserState()->m_fontConverter->getId(name));
  if ((fSz%2)==0)
    input->seek(1, librevenge::RVNG_SEEK_CUR);
  font.setSize((float) input->readULong(2));
  int flag = (int) input->readULong(2);
  uint32_t flags=0;
  if (flag&0x1) flags |= MWAWFont::boldBit;
  if (flag&0x2) flags |= MWAWFont::italicBit;
  if (flag&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
  if (flag&0x8) flags |= MWAWFont::embossBit;
  if (flag&0x10) flags |= MWAWFont::shadowBit;
  if (flag&0x20) font.setDeltaLetterSpacing(-1);
  if (flag&0x40) font.setDeltaLetterSpacing(1);
  if (flag&0x80) f << "#flag0[0x80],";
  font.setFlags(flags);
  unsigned char col[3];
  for (int c=0; c < 3; ++c)
    col[c]=(unsigned char)(input->readULong(2)>>8);
  font.setColor(MWAWColor(col[0],col[1],col[2]));
  font.m_extra = f.str();
  f.str("");
  f << "Entries(Font)[" << entry.id() << "]:"
    << font.getDebugString(getParserState()->m_fontConverter);
  m_state->m_idFontMap[entry.id()-999]=font;
  ascFile.addPos(entry.begin()-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

//
// index functions
//
bool MacDocParser::readIndex(MWAWEntry const &entry)
{
  if (entry.length()<4) {
    MWAW_DEBUG_MSG(("MacDocParser::readIndex: the entry seems bad\n"));
    return false;
  }
  if (entry.id()!=1) {
    MWAW_DEBUG_MSG(("MacDocParser::readIndex: the entry id seems bad\n"));
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);

  libmwaw::DebugFile &ascFile = rsrcAscii();
  ascFile.addPos(entry.begin()-4);
  ascFile.addNote("Entries(Index)");
  libmwaw::DebugStream f;
  long pos;
  while (!input->isEnd()) {
    pos=input->tell();
    if (pos+21>=entry.end())
      break;

    f.str("");
    MacDocParserInternal::Index index;
    int val=(int) input->readLong(2);
    if (val) f << "#f0=" << val << ",";
    index.m_page=(int) input->readLong(2);
    if (index.m_page<=0) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    int dim[4];
    for (int i=0; i<4; ++i)
      dim[i]=(int) input->readLong(2);
    index.m_box=Box2i(Vec2i(dim[1],dim[0]),Vec2i(dim[3],dim[2]));
    val=(int) input->readLong(2);
    if (val) f << "#f1=" << val << ",";
    index.m_numChild=(int) input->readLong(2);
    for (int i=0; i<2; ++i) {
      val=(int) input->readLong(2);
      if (val) f << "#f" << i+2 << "=" << val << ",";
    }
    index.m_extra=f.str();
    f.str("");
    f << "Index:" << index;
    index.m_entry.setBegin(input->tell());
    std::string name("");
    bool ok=false;
    while (!input->isEnd()) {
      if (input->tell()>=entry.end())
        break;
      char c=(char) input->readLong(1);
      if (c==0) {
        ok = true;
        break;
      }
      name+=c;
    }
    if (!ok) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    index.m_entry.setEnd(input->tell()-1);
    m_state->m_indexList.push_back(index);
    f << name;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  f.str("");
  f << "Index[end]:";
  pos=input->tell();
  if (pos!=entry.end()-4) {
    MWAW_DEBUG_MSG(("MacDocParser::readIndex: problem reading end\n"));
    f << "###";
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}

int MacDocParser::updateIndex(int actIndex, int actLevel)
{
  int numIndex=(int) m_state->m_indexList.size();
  if (actIndex < 0 || actIndex >= numIndex) {
    MWAW_DEBUG_MSG(("MacDocParser::updateIndex: the actual index seems bad\n"));
    return -1;
  }
  MacDocParserInternal::Index &index =  m_state->m_indexList[size_t(actIndex++)];
  index.m_level=actLevel;
  for (int c=0; c < index.m_numChild; ++c) {
    actIndex=updateIndex(actIndex, actLevel+1);
    if (actIndex==-1)
      break;
  }
  return actIndex;
}

bool MacDocParser::sendIndex()
{
  MWAWTextListenerPtr listener=getTextListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MacDocParser::sendIndex: can not find the listener\n"));
    return false;
  }
  if (m_state->m_indexList.empty())
    return false;
  int id=0;
  size_t numIndex=(size_t) m_state->m_indexList.size();
  do
    id=updateIndex(id,1);
  while (id>0 && id < (int) numIndex);
  listener->setFont(MWAWFont(3,12,MWAWFont::boldBit));
  MWAWParagraph para;
  para.m_justify = MWAWParagraph::JustificationCenter;
  listener->setParagraph(para);
  listener->insertUnicodeString("Index");
  listener->insertEOL();
  listener->insertEOL();

  MWAWInputStreamPtr input = rsrcInput();
  para=MWAWParagraph();
  double w = getPageWidth();
  MWAWTabStop tab;
  tab.m_alignment = MWAWTabStop::RIGHT;
  tab.m_leaderCharacter='.';
  tab.m_position = w-0.3;
  para.m_tabs->push_back(tab);
  for (size_t i=0; i < numIndex; ++i) {
    MacDocParserInternal::Index const &index =  m_state->m_indexList[i];
    if (!index.m_entry.valid() || index.m_level<=0)
      continue;
    para.m_margins[1]=0.5*double(index.m_level);
    listener->setParagraph(para);
    if (m_state->m_idFontMap.find(index.m_level)!=m_state->m_idFontMap.end())
      listener->setFont(m_state->m_idFontMap.find(index.m_level)->second);
    else {
      MWAW_DEBUG_MSG(("MacDocParser::sendIndex: can not find font for index %d\n", int(i)));
      listener->setFont(MWAWFont());
    }
    input->seek(index.m_entry.begin(), librevenge::RVNG_SEEK_SET);
    for (long c=0; c < index.m_entry.length(); ++c) {
      unsigned char ch=(unsigned char)input->readULong(1);
      if (ch==9)
        listener->insertCharacter(' ');
      else
        listener->insertCharacter(ch);
    }
    if (index.m_page>0) {
      std::stringstream s;
      s << index.m_page;
      listener->setFont(MWAWFont());
      listener->insertTab();
      listener->insertUnicodeString(s.str().c_str());
    }
    listener->insertEOL();
  }
  return true;
}

// picture
bool MacDocParser::sendPicture(MWAWEntry const &entry)
{
  if (!getTextListener()) {
    MWAW_DEBUG_MSG(("MacDocParser::sendPicture: can not find the listener\n"));
    return false;
  }
  librevenge::RVNGBinaryData data;
  if (!getRSRCParser()->parsePICT(entry, data))
    return false;

  entry.setParsed(true);
  int dataSz=int(data.size());
  if (!dataSz)
    return false;
  MWAWInputStreamPtr pictInput=MWAWInputStream::get(data, false);
  if (!pictInput) {
    MWAW_DEBUG_MSG(("MacDocParser::sendPicture: oops can not find an input\n"));
    return false;
  }
  Box2f box;
  MWAWPict::ReadResult res = MWAWPictData::check(pictInput, dataSz,box);
  if (res == MWAWPict::MWAW_R_BAD) {
    MWAW_DEBUG_MSG(("MacDocParser::sendPicture: can not find the picture\n"));
    return false;
  }
  pictInput->seek(0,librevenge::RVNG_SEEK_SET);
  shared_ptr<MWAWPict> thePict(MWAWPictData::get(pictInput, dataSz));
  MWAWPosition pictPos=MWAWPosition(Vec2f(0,0),box.size(), librevenge::RVNG_POINT);
  pictPos.setRelativePosition(MWAWPosition::Char);
  if (thePict) {
    librevenge::RVNGBinaryData fData;
    std::string type;
    if (thePict->getBinary(fData,type))
      getTextListener()->insertPicture(pictPos, fData, type);
  }
  return true;
}

// file: unknown format: 0002 0000 0000 00 + FileInfo + DataFrk + RSRCFork ?
bool MacDocParser::readFile(MWAWEntry const &entry)
{
  entry.setParsed(true);
#ifdef DEBUG_WITH_FILES
  MWAWInputStreamPtr input = rsrcInput();
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  librevenge::RVNGBinaryData data;
  input->readDataBlock(entry.length(), data);

  libmwaw::DebugFile &ascFile = rsrcAscii();

  static int volatile fileName = 0;
  libmwaw::DebugStream f;
  f << "FILE" << ++fileName;
  libmwaw::Debug::dumpFile(data, f.str().c_str());

  ascFile.addPos(entry.begin()-4);
  ascFile.addNote(f.str().c_str());
  ascFile.skipZone(entry.begin(),entry.end()-1);
#endif

  return true;
}

// bookmark. note the name is stored as resource name
bool MacDocParser::readBookmark(MWAWEntry const &entry)
{
  if (entry.length()!=8) {
    MWAW_DEBUG_MSG(("MacDocParser::readWP: the entry seems bad\n"));
    return false;
  }

  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile = rsrcAscii();
  libmwaw::DebugStream f;
  f << "Entries(BookMark)[" << entry.id() << "]:";
  long val=input->readLong(4);
  if (val) f << "page=" << val << ",";
  val=input->readLong(4);
  if (val) f << "yPos?=" << val << ",";
  ascFile.addPos(entry.begin()-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

// unknown related to window position?
bool MacDocParser::readWP(MWAWEntry const &entry)
{
  if (entry.length()!=4) {
    MWAW_DEBUG_MSG(("MacDocParser::readWP: the entry seems bad\n"));
    return false;
  }

  entry.setParsed(true);
  MWAWInputStreamPtr input = rsrcInput();
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile = rsrcAscii();
  libmwaw::DebugStream f;
  f << "Entries(WP)[" << entry.id() << "]:";
  for (int i=0; i < 2; ++i) { // f0=0|a6|c6, f1=0|1 show index ?
    long val=input->readLong(2);
    if (val)
      f << "f" << i << "=" << val << ",";
  }
  ascFile.addPos(entry.begin()-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool MacDocParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = MacDocParserInternal::State();
  /** no data fork, may be ok, but this means
      that the file contains no text, so... */
  MWAWInputStreamPtr input = getInput();
  if (!input || !getRSRCParser())
    return false;
  if (input->hasDataFork()) {
    MWAW_DEBUG_MSG(("MacDocParser::checkHeader: find a datafork, odd!!!\n"));
  }
  if (strict) {
    // check if at least one picture zone exists
    std::multimap<std::string, MWAWEntry> &entryMap = getRSRCParser()->getEntriesMap();
    if (entryMap.find("MDpg") == entryMap.end())
      return false;
  }
  if (header)
    header->reset(MWAWDocument::MWAW_T_MACDOC, version());

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
