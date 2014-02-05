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

#include <librevenge/librevenge.h>

#include "MWAWTextListener.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWPictData.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSubDocument.hxx"

#include "DocMkrText.hxx"

#include "DocMkrParser.hxx"

/** Internal: the structures of a DocMkrParser */
namespace DocMkrParserInternal
{
////////////////////////////////////////
//! Internal: store a picture information in DocMkrParser
struct PictInfo {
  //! constructor
  PictInfo() : m_id(-1), m_sndId(-1), m_align(1), m_print(false), m_invert(false),
    m_action(0), m_actionString(""), m_extra("")
  {
    for (int i= 0; i < 2; i++)
      m_next[i]=0;
    for (int i = 0; i < 3; i++)
      m_appleScript[i]="";
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, PictInfo const &info);
  //! the picture id
  int m_id;
  //! the sound id
  int m_sndId;
  //! the alignement ( 1:center, ... )
  int m_align;
  //! true if the picture is printed
  bool m_print;
  //! true if we must invert the picture
  bool m_invert;
  //! the action
  int m_action;
  //! the action string
  std::string m_actionString;
  //! the next chapter/paragraph position for goChapter
  int m_next[2];
  //! the applescript type
  std::string m_appleScript[3];
  //! extra data
  std::string m_extra;
};

std::ostream &operator<<(std::ostream &o, PictInfo const &info)
{
  if (info.m_id >= 0) o << "pictId=" << info.m_id << ",";
  switch (info.m_align) {
  case 1:
    o << "center,";
    break;
  case 2:
    o << "left,";
    break;
  case 3:
    o << "right,";
    break;
  default:
    o << "#align=" << info.m_align << ",";
    break;
  }
  if (info.m_action >= 0 && info.m_action <= 16) {
    static char const *wh[]= {
      "", "goTo", "aboutDialog", "print", "quit", "launch", "sound", "QMOV",
      "note", "export[asText]", "last[chapter]", "TOC[show]", "find", "appleEvent",
      "next[chapter]", "prev[chapter]", "script"
    };
    o << wh[info.m_action];
  }
  else
    o << "#action=" << info.m_action << ",";
  switch (info.m_action) {
  case 1:
    o << "[chapter=" << info.m_next[0];
    if (info.m_next[1]) o << ",para=" << info.m_next[1] << "]";
    else o << "]";
    break;
  case 5:
  case 7:
  case 8:
  case 0x10:
    o << "[" << info.m_actionString << "]";
    break;
  case 6:
    o << "[id=" << info.m_sndId << "]";
    break;
  case 0xd:
    o << "[appli=" << info.m_appleScript[0] << ",class=" << info.m_appleScript[1]
      << ",eventid=" << info.m_appleScript[2];
    if (info.m_actionString.size())
      o << ",data=" <<info.m_actionString;
    o << "]";
    break;
  default:
    break;
  }
  o << "],";
  if (!info.m_print) o << "noPrint,";
  if (info.m_invert) o << "invert,";
  o << info.m_extra;
  return o;
}
////////////////////////////////////////
//! Internal: the state of a DocMkrParser
struct State {
  //! constructor
  State() : m_idPictEntryMap(), m_idPictInfoMap(), m_zonePictInfoUnit(100),
    m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0)
  {
  }
  //! return a pictinfo id corresponding to a zone and a local if
  int pictInfoId(int zId, int lId) const
  {
    return (zId+2)*m_zonePictInfoUnit+lId;
  }
  //! try to find the picture info unit (fixme: a hack)
  void findPictInfoUnit(int nZones);
  //! a map id->pictEntry
  std::map<int,MWAWEntry> m_idPictEntryMap;
  //! a map id->pictInfo
  std::map<int,PictInfo> m_idPictInfoMap;
  //! the zone unit to retrieve pictInfo
  int m_zonePictInfoUnit;
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

void State::findPictInfoUnit(int nZones)
{
  if (m_idPictInfoMap.empty())
    return;
  bool is100=true, is1000=true;
  std::map<int,PictInfo>::const_iterator it=m_idPictInfoMap.begin();
  for (; it != m_idPictInfoMap.end(); ++it) {
    int id=it->first;
    if (id > (nZones+3)*100 || id < 200)
      is100=false;
    if (id > (nZones+3)*1000 || id < 2000)
      is1000=false;
  }
  if (is100 && !is1000)
    m_zonePictInfoUnit=100;
  else if (is1000 && !is100)
    m_zonePictInfoUnit=1000;
  else {
    MWAW_DEBUG_MSG(("DocMkrParserInternal::State::findPictInfoUnit can not find unit\n"));
  }
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
DocMkrParser::DocMkrParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWTextParser(input, rsrcParser, header), m_state(), m_textParser()
{
  init();
}

DocMkrParser::~DocMkrParser()
{
}

void DocMkrParser::init()
{
  resetTextListener();

  m_state.reset(new DocMkrParserInternal::State);

  m_textParser.reset(new DocMkrText(*this));
}

MWAWInputStreamPtr DocMkrParser::rsrcInput()
{
  return getRSRCParser()->getInput();
}

libmwaw::DebugFile &DocMkrParser::rsrcAscii()
{
  return getRSRCParser()->ascii();
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void DocMkrParser::newPage(int number)
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
void DocMkrParser::parse(librevenge::RVNGTextInterface *docInterface)
{
  assert(getInput().get() != 0 && getRSRCParser());

  if (!checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = false;
  try {
    checkHeader(0L);
    ok = createZones();
    if (ok) {
      createDocument(docInterface);
      m_textParser->sendMainText();
      m_textParser->sendTOC();
#ifdef DEBUG
      m_textParser->flushExtra();
      flushExtra();
#endif
    }
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("DocMkrParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetTextListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void DocMkrParser::createDocument(librevenge::RVNGTextInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getTextListener()) {
    MWAW_DEBUG_MSG(("DocMkrParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  std::vector<MWAWPageSpan> pageList;
  m_textParser->updatePageSpanList(pageList);
  m_state->m_numPages = int(pageList.size());

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
bool DocMkrParser::createZones()
{
  MWAWInputStreamPtr input = getInput();
  MWAWRSRCParserPtr rsrcParser = getRSRCParser();
  std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::iterator it;

  if (!m_textParser->createZones())
    return false;
  // the different pict zones
  it = entryMap.lower_bound("PICT");
  while (it != entryMap.end()) {
    if (it->first != "PICT")
      break;
    MWAWEntry const &entry = it++->second;
    m_state->m_idPictEntryMap[entry.id()]=entry;
  }
  it = entryMap.lower_bound("conp"); // local picture 5000-...?
  while (it != entryMap.end()) {
    if (it->first != "conp")
      break;
    MWAWEntry const &entry = it++->second;
    librevenge::RVNGBinaryData data;
    rsrcParser->parsePICT(entry, data);
  }
  it = entryMap.lower_bound("pInf"); // 201|202,301..309,401..410,501..
  while (it != entryMap.end()) {
    if (it->first != "pInf")
      break;
    MWAWEntry const &entry = it++->second;
    readPictInfo(entry);
  }

  // chapiter name STR 2000..
  // entry 0: copyright
  it = entryMap.lower_bound("Dk@P");
  while (it != entryMap.end()) {
    if (it->first != "Dk@P")
      break;
    MWAWEntry const &entry = it++->second;
    std::string str;
    rsrcParser->parseSTR(entry, str);
  }
  it = entryMap.lower_bound("sTwD");
  while (it != entryMap.end()) {
    if (it->first != "sTwD")
      break;
    MWAWEntry const &entry = it++->second;
    readSTwD(entry);
  }
  it = entryMap.lower_bound("xtr2");
  while (it != entryMap.end()) {
    if (it->first != "xtr2")
      break;
    MWAWEntry const &entry = it++->second;
    readXtr2(entry);
  }
  //  1000:docname, 1001:footer name, 2001-... chapter name, others ...
  it = entryMap.lower_bound("STR ");
  while (it != entryMap.end()) {
    if (it->first != "STR ")
      break;
    MWAWEntry const &entry = it++->second;
    std::string str;
    rsrcParser->parseSTR(entry, str);
  }

  m_state->findPictInfoUnit(m_textParser->numChapters());

#ifdef DEBUG_WITH_FILES
  // get rid of the default application resource
  libmwaw::DebugFile &ascFile = rsrcAscii();
  static char const *(appliRsrc[])= {
    // default, Dialog (3000: DLOG,DITL,DLGX,dctb","ictb","STR ")
    "ALRT","BNDL","CNTL","CURS","CDEF", "DLOG","DLGX","DITL","FREF","ICON", "ICN#","MENU","SIZE",
    "crsr","dctb","icl4","icl8","ics4", "ics8","ics#","ictb","snd ",
    // local
    "mstr" /* menu string */, "aete" /* some function name?*/
  };
  for (int r=0; r < 13+9+2; r++) {
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
  return true;
}

void DocMkrParser::flushExtra()
{
  MWAWRSRCParserPtr rsrcParser = getRSRCParser();
  std::map<int,MWAWEntry>::const_iterator it = m_state->m_idPictEntryMap.begin();
  for (; it != m_state->m_idPictEntryMap.end(); ++it) {
    MWAWEntry const &entry=it->second;
    if (entry.isParsed()) continue;
    librevenge::RVNGBinaryData data;
    rsrcParser->parsePICT(entry,data);
  }
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////
bool DocMkrParser::sendPicture(int zId, int lId, double /*lineW*/)
{
  int pictId=m_state->pictInfoId(zId,lId);
  if (m_state->m_idPictInfoMap.find(pictId)==m_state->m_idPictInfoMap.end()) {
    MWAW_DEBUG_MSG(("DocMkrText::sendPicture: can not find picture for zone=%d, id=%d\n",zId,lId));
    return false;
  }
  DocMkrParserInternal::PictInfo const &info=m_state->m_idPictInfoMap.find(pictId)->second;
  if (m_state->m_idPictEntryMap.find(info.m_id)==m_state->m_idPictEntryMap.end()) {
    MWAW_DEBUG_MSG(("DocMkrText::sendPicture: can not find picture for id=%d\n",info.m_id));
    return false;
  }
  if (!getTextListener()) {
    MWAW_DEBUG_MSG(("DocMkrText::sendPicture: can not find the listener\n"));
    return false;
  }

  if (info.m_action==8 && info.m_actionString.size())
    m_textParser->sendComment(info.m_actionString);
  MWAWInputStreamPtr input = rsrcInput();
  MWAWRSRCParserPtr rsrcParser = getRSRCParser();
  MWAWEntry const &entry=m_state->m_idPictEntryMap.find(info.m_id)->second;

  librevenge::RVNGBinaryData data;
  long pos = input->tell();
  rsrcParser->parsePICT(entry,data);
  input->seek(pos,librevenge::RVNG_SEEK_SET);

  int dataSz=int(data.size());
  if (!dataSz) {
    return false;
  }
  MWAWInputStreamPtr pictInput=MWAWInputStream::get(data, false);
  if (!pictInput) {
    MWAW_DEBUG_MSG(("DocMkrText::sendPicture: oops can not find an input\n"));
    return false;
  }
  Box2f box;
  MWAWPict::ReadResult res = MWAWPictData::check(pictInput, dataSz,box);
  if (res == MWAWPict::MWAW_R_BAD) {
    MWAW_DEBUG_MSG(("DocMkrText::sendPicture: can not find the picture\n"));
    return false;
  }
  pictInput->seek(0,librevenge::RVNG_SEEK_SET);
  shared_ptr<MWAWPict> thePict(MWAWPictData::get(pictInput, dataSz));
  MWAWPosition pictPos=MWAWPosition(Vec2f(0,0),box.size(), librevenge::RVNG_POINT);
  MWAWPosition::XPos xpos= (info.m_align==1) ? MWAWPosition::XCenter :
                           (info.m_align==3) ? MWAWPosition::XRight  : MWAWPosition::XLeft;
  pictPos.setRelativePosition(MWAWPosition::Paragraph, xpos);
  pictPos.m_wrapping = MWAWPosition::WRunThrough;
  if (thePict) {
    librevenge::RVNGBinaryData fData;
    std::string type;
    if (thePict->getBinary(fData,type))
      getTextListener()->insertPicture(pictPos, fData, type);
  }
  return true;
}

bool DocMkrParser::readPictInfo(MWAWEntry const &entry)
{
  long length = entry.length();
  if (!entry.valid() || length<8) {
    MWAW_DEBUG_MSG(("DocMkrText::readPictInfo: the entry seems very short\n"));
    return false;
  }

  entry.setParsed(true);
  long pos = entry.begin();
  long endPos = entry.end();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  input->seek(pos, librevenge::RVNG_SEEK_SET);

  libmwaw::DebugStream f;
  DocMkrParserInternal::PictInfo info;
  info.m_id = (int) input->readULong(2);
  info.m_align = (int) input->readLong(2);
  int val =(int) input->readLong(2); // 0|1
  if (val) f << "unkn=" << val << ",";

  int action =(int) input->readLong(2); // 0..b
  int extraN = int(endPos-input->tell());
  if (action < 0) {
    info.m_invert=true;
    action = -action;
  }
  info.m_action=action;
  switch (action) {
  case 1:
    if (extraN < 2) {
      f << "actionArg##,";
      break;
    }
    info.m_next[0]=(int) input->readLong(2);
    if (extraN < 4)
      break;
    info.m_next[1]=(int) input->readLong(2);
    break;
  case 5:
  case 7:
  case 8:
  case 0x10: {
    if (extraN < 1) {
      f << "actionArg##,";
    }
    int fSz=(int) input->readULong(1);
    if (extraN < fSz+1) {
      f << "##[N=" << fSz << "],";
      break;
    }
    std::string name("");
    for (int i = 0; i < fSz; i++)
      name += (char) input->readULong(1);
    info.m_actionString=name;
    break;
  }
  case 6: {
    if (extraN < 4) {
      f << "actionArg##,";
      break;
    }
    info.m_sndId=(int)input->readULong(2);
    val = (int) input->readULong(2); // loop?
    if (val)
      f << "sndFlag=" << val << ",";
    break;
  }
  case 0xd: {
    if (extraN < 13) {
      f << "actionArg##,";
      break;
    }
    for (int w=0; w < 3; w++) {
      std::string name("");
      for (int i = 0; i < 4; i++)
        name += (char) input->readULong(1);
      info.m_appleScript[w]=name;
    }
    int fSz=(int) input->readULong(1);
    if (extraN < fSz+13) {
      f << "##[N=" << fSz << "],";
      break;
    }
    std::string name("");
    for (int i = 0; i < fSz; i++)
      name += (char) input->readULong(1);
    info.m_actionString=name;
    break;
  }
  default:
    break;
  }
  extraN = int(endPos-input->tell())/2;
  if (extraN==1) {
    val =(int) input->readLong(2);
    if (val==0)
      info.m_print = false;
    else if (val==1)
      info.m_print = true;
    else if (val) {
      f << "#print=" << val << ",";
    }
  }
  else {
    for (int i = 0; i < extraN; i++) { // g0=0|1
      val =(int) input->readLong(2);
      if (val)
        f << "#g" << i << "=" << val << ",";
    }
  }
  info.m_extra=f.str();
  m_state->m_idPictInfoMap[entry.id()]=info;
  f.str("");
  f << "Entries(PctInfo)[" << entry.type() << "-" << entry.id() << "]:" << info;

  if (input->tell()!=entry.end())
    ascFile.addDelimiter(input->tell(),'|');
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read some unknown zone
////////////////////////////////////////////////////////////
bool DocMkrParser::readSTwD(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<10) {
    MWAW_DEBUG_MSG(("DocMkrText::readSTwD: the entry seems very short\n"));
    return false;
  }

  entry.setParsed(true);
  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  input->seek(pos, librevenge::RVNG_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(STwD)[" << entry.type() << "-" << entry.id() << "]:";
  int val;
  for (int i=0; i < 2; i++) { // f0=2, f1=1|2
    val =(int) input->readLong(2);
    if (val)
      f << "f" << i << "=" << val << ",";
  }
  int flag =(int) input->readLong(2); // 320|7d0|1388 ?
  f << "fl=" << std::hex << flag << std::dec << ",";
  f << "dim=" << (int) input->readLong(2) << ","; // 0x1Fa|0x200
  for (int i=0; i < 2; i++) { // f0=1, f1=0|1
    val =(int) input->readLong(1);
    if (val)
      f << "f" << i+2 << "=" << val << ",";
  }
  f << "],";
  if (input->tell()!=entry.end())
    ascFile.addDelimiter(input->tell(),'|');
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool DocMkrParser::readXtr2(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<1) {
    MWAW_DEBUG_MSG(("DocMkrText::readXtr2: the entry seems very short\n"));
    return false;
  }

  entry.setParsed(true);
  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  input->seek(pos, librevenge::RVNG_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(Xtr2)[" << entry.type() << "-" << entry.id() << "]:";
  int N=1;
  if (entry.length() != 1) {
    MWAW_DEBUG_MSG(("DocMkrText::readXtr2: find more than one flag\n"));
    N = entry.length()>20 ? 20 : int(entry.length());
  }
  // f0=79|a8|b9|99
  for (int i=0; i < N; i++) {
    int val =(int) input->readULong(1);
    if (val)
      f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  if (input->tell()!=entry.end())
    ascFile.addDelimiter(input->tell(),'|');
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool DocMkrParser::checkHeader(MWAWHeader *header, bool /*strict*/)
{
  *m_state = DocMkrParserInternal::State();
  /** no data fork, may be ok, but this means
      that the file contains no text, so... */
  MWAWInputStreamPtr input = getInput();
  if (!input || !getRSRCParser())
    return false;
  if (input->hasDataFork()) {
    MWAW_DEBUG_MSG(("DocMkrParser::checkHeader: find a datafork, odd!!!\n"));
  }
  MWAWRSRCParser::Version vers;
  // read the Docmaker version
  int docmakerVersion = -1;
  MWAWEntry entry = getRSRCParser()->getEntry("vers", 2);
  if (entry.valid() && getRSRCParser()->parseVers(entry, vers))
    docmakerVersion = vers.m_majorVersion;
  else if (docmakerVersion==-1) {
    MWAW_DEBUG_MSG(("DocMkrParser::checkHeader: can not find the DocMaker version\n"));
  }
  setVersion(vers.m_majorVersion);
  if (header)
    header->reset(MWAWDocument::MWAW_T_DOCMAKER, version());

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
