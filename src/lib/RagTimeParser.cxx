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
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSubDocument.hxx"

#include "RagTimeParser.hxx"

/** Internal: the structures of a RagTimeParser */
namespace RagTimeParserInternal
{
////////////////////////////////////////
//! Internal: the state of a RagTimeParser
struct State {
  //! constructor
  State() : m_dataZoneMap(), m_RSRCZoneMap(), m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0)
  {
  }

  //! the map: type->entry (datafork)
  std::multimap<std::string, MWAWEntry> m_dataZoneMap;
  //! the map: type->entry (resource fork)
  std::multimap<std::string, MWAWEntry> m_RSRCZoneMap;
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

////////////////////////////////////////
//! Internal: the subdocument of a RagTimeParser
class SubDocument : public MWAWSubDocument
{
public:
  // constructor
  SubDocument(RagTimeParser &pars, MWAWInputStreamPtr input, int zoneId) :
    MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId) {}

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
  //! the subdocument id
  int m_id;
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("RagTimeParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  assert(m_parser);

  long pos = m_input->tell();
  // TODO
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_id != sDoc->m_id) return true;
  return false;
}
}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
RagTimeParser::RagTimeParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWTextParser(input, rsrcParser, header), m_state()
{
  init();
}

RagTimeParser::~RagTimeParser()
{
}

void RagTimeParser::init()
{
  resetTextListener();
  setAsciiName("main-1");

  m_state.reset(new RagTimeParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void RagTimeParser::newPage(int number)
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
void RagTimeParser::parse(librevenge::RVNGTextInterface *docInterface)
{
  assert(getInput().get() != 0);

  if (!checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = true;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());
    checkHeader(0L);
    ok=createZones();
    if (ok) {
      createDocument(docInterface);
      // TODO
    }

    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("RagTimeParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetTextListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void RagTimeParser::createDocument(librevenge::RVNGTextInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getTextListener()) {
    MWAW_DEBUG_MSG(("RagTimeParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  MWAWPageSpan ps(getPageSpan());
  m_state->m_numPages=1;
  std::vector<MWAWPageSpan> pageList;
  ps.setPageSpan(m_state->m_numPages);
  pageList.push_back(ps);
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


bool RagTimeParser::createZones()
{
  int const vers=version();
  if (!findDataZones())
    return false;
  if (vers>=2)
    findRsrcZones();

  libmwaw::DebugStream f;
  std::multimap<std::string,MWAWEntry>::iterator it;

  it=m_state->m_dataZoneMap.lower_bound("ZoneText");
  while (it!=m_state->m_dataZoneMap.end() && it->first=="ZoneText")
    readTextZone(it++->second);
  it=m_state->m_dataZoneMap.lower_bound("PictZone");
  while (it!=m_state->m_dataZoneMap.end() && it->first=="PictZone") {
    if (vers==1)
      readPictZoneV2(it++->second);
    else
      readPictZone(it++->second);
  }
  if (vers<2) {
    it=m_state->m_dataZoneMap.lower_bound("ColorMap");
    while (it!=m_state->m_dataZoneMap.end() && it->first=="ColorMap")
      readColorMapV2(it++->second);
  }

  // now unknown zone
  it=m_state->m_dataZoneMap.lower_bound("Zone0");
  while (it!=m_state->m_dataZoneMap.end() && it->first=="Zone0")
    readZone0(it++->second);

  it=m_state->m_dataZoneMap.lower_bound("Zone3");
  while (it!=m_state->m_dataZoneMap.end() && it->first=="Zone3") {
    if (vers==1)
      readZone3V2(it++->second);
    else
      readZone3(it++->second);
  }
  it=m_state->m_dataZoneMap.lower_bound("Zone6");
  while (it!=m_state->m_dataZoneMap.end() && it->first=="Zone6")
    readZone6(it++->second);


  // print info
  it=m_state->m_RSRCZoneMap.lower_bound("rsrcPREC");
  while (it!=m_state->m_RSRCZoneMap.end() && it->first=="rsrcPREC")
    readPrintInfo(it++->second);
  // the font
  it=m_state->m_RSRCZoneMap.lower_bound("rsrcFHFont");
  while (it!=m_state->m_RSRCZoneMap.end() && it->first=="rsrcFHFont")
    readRsrcFont(it++->second);
  // the numbering format
  it=m_state->m_RSRCZoneMap.lower_bound("rsrcFormat");
  while (it!=m_state->m_RSRCZoneMap.end() && it->first=="rsrcFormat")
    readRsrcFormat(it++->second);
  // the file link
  it=m_state->m_RSRCZoneMap.lower_bound("rsrcLink");
  while (it!=m_state->m_RSRCZoneMap.end() && it->first=="rsrcLink")
    readRsrcFont(it++->second);
  it=m_state->m_RSRCZoneMap.lower_bound("rsrcItem");
  while (it!=m_state->m_RSRCZoneMap.end() && it->first=="rsrcItem")
    readRsrcItemFormat(it++->second);

  it=m_state->m_RSRCZoneMap.lower_bound("rsrcfppr");
  while (it!=m_state->m_RSRCZoneMap.end() && it->first=="rsrcfppr")
    readRsrcfppr(it++->second);
  it=m_state->m_RSRCZoneMap.lower_bound("rsrcBeDc");
  while (it!=m_state->m_RSRCZoneMap.end() && it->first=="rsrcBeDc")
    readRsrcBeDc(it++->second);
  it=m_state->m_RSRCZoneMap.lower_bound("rsrcBtch");
  while (it!=m_state->m_RSRCZoneMap.end() && it->first=="rsrcBtch")
    readRsrcBtch(it++->second);
  it=m_state->m_RSRCZoneMap.lower_bound("rsrcCalc");
  while (it!=m_state->m_RSRCZoneMap.end() && it->first=="rsrcCalc")
    readRsrcCalc(it++->second);
  it=m_state->m_RSRCZoneMap.lower_bound("rsrcCHTa");
  while (it!=m_state->m_RSRCZoneMap.end() && it->first=="rsrcCHTa")
    readRsrcCHTa(it++->second);
  it=m_state->m_RSRCZoneMap.lower_bound("rsrcUnamed");
  while (it!=m_state->m_RSRCZoneMap.end() && it->first=="rsrcUnamed")
    readRsrcUnamed(it++->second);

  it=m_state->m_RSRCZoneMap.lower_bound("rsrcFHsl");
  while (it!=m_state->m_RSRCZoneMap.end() && it->first=="rsrcFHsl")
    readRsrcFHsl(it++->second);
  it=m_state->m_RSRCZoneMap.lower_bound("rsrcFHwl");
  while (it!=m_state->m_RSRCZoneMap.end() && it->first=="rsrcFHwl")
    readRsrcFHwl(it++->second);
  it=m_state->m_RSRCZoneMap.lower_bound("rsrcSpDI");
  while (it!=m_state->m_RSRCZoneMap.end() && it->first=="rsrcSpDI")
    readRsrcSpDI(it++->second);
  it=m_state->m_RSRCZoneMap.lower_bound("rsrcSpDo");
  while (it!=m_state->m_RSRCZoneMap.end() && it->first=="rsrcSpDo")
    readRsrcSpDo(it++->second);
  for (int i=0; i<10; ++i) {
    /* gray [2bytes]:color,0,26
       color [2bytes]*3:color
       res_ [2bytes]*3: sometimes string?
       BuSl: [small 2bytes int]*3
       BuGr : structure with many data of size 0x1a

       SpBo :  structure with many data of size 0x14
       SpCe :  structure with many data of size 0x8
       SpDE :  structure with many data of size 0xa ( 2int, unknown, 1int)
       SpTe :  structure with many data of size 0xa
       SpVa:  structure with many data of size 0xc
     */
    static char const *(what[])= {
      "rsrcgray", "rsrccolr", "rsrcres_",
      "rsrcBuSl", "rsrcBuGr",
      "rsrcSpBo", "rsrcSpCe", "rsrcSpDE", "rsrcSpTe", "rsrcSpVa"
    };
    it=m_state->m_RSRCZoneMap.lower_bound(what[i]);
    while (it!=m_state->m_RSRCZoneMap.end() && it->first==what[i])
      readRsrcStructured(it++->second);
  }

  for (it=m_state->m_dataZoneMap.begin(); it!=m_state->m_dataZoneMap.end(); ++it) {
    MWAWEntry const &entry=it->second;
    if (entry.isParsed()) continue;
    f.str("");
    f << "Entries(" << entry.type() << "):";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
  }
  for (it=m_state->m_RSRCZoneMap.begin(); it!=m_state->m_RSRCZoneMap.end(); ++it) {
    MWAWEntry const &entry=it->second;
    if (entry.isParsed()) continue;
    f.str("");
    f << "Entries(" << entry.type() << "):";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
  }

  return false;
}

////////////////////////////////////////////////////////////
// find the different zones
////////////////////////////////////////////////////////////

bool RagTimeParser::findDataZones()
{
  MWAWInputStreamPtr input = getInput();
  int const vers=version();
  int const headerSize=vers>=2 ? 156 : 196;
  int const zoneLength=vers>=2 ? 54 : 40;
  libmwaw::DebugStream f;
  f << "Entries(Zones)[header]:";
  long pos = input->tell();
  input->seek(pos+(vers>=2 ? 48 : 72), librevenge::RVNG_SEEK_SET);
  ascii().addDelimiter(input->tell(),'|');
  int nZones=(int) input->readULong(2) ;
  long endPos=pos+headerSize+nZones*zoneLength;
  f << "num[zones]=" << nZones << ",";
  if (nZones==0 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::findDataZones: can not find the number of zones\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addDelimiter(input->tell(),'|');
  if (vers==1) {
    input->seek(pos+186, librevenge::RVNG_SEEK_SET);
    MWAWEntry entry;
    entry.setBegin((long) input->readULong(2));
    entry.setType("ColorMap");
    if (entry.begin() && input->checkPosition(entry.begin()))
      m_state->m_dataZoneMap.insert(std::multimap<std::string,MWAWEntry>::value_type(entry.type(),entry));
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(pos+headerSize, librevenge::RVNG_SEEK_SET);
  for (int n=0; n<nZones; ++n) {
    pos=input->tell();
    if (!input->checkPosition(pos+zoneLength))
      break;
    int type=(int) input->readULong(1);
    f.str("");
    f << "Zones-" << n << ":";
    std::string what("");
    bool hasPointer=true;
    if (type>=0 && type<=6) { // checkme
      static const char *(wh[]) = { "Zone0", "Zone1", "ZoneText", "Zone3", "PictZone", "Zone5", "Zone6" };
      what=wh[type];
      if (type==5) hasPointer=false;
    }
    else if ((type & 0xF)==0xF) { // find 0xF and 0xFF with no data
      std::stringstream s;
      s << "Undef" << (type>>4) << "A";
      hasPointer=false;
    }
    else {
      MWAW_DEBUG_MSG(("RagTimeParser::findDataZones: find some unknown zone, type=%d\n", type));
      std::stringstream s;
      s << "Zone" << type;
      what=s.str();
    }
    f << what << ",";
    if (hasPointer) {
      input->seek(pos+zoneLength-4, librevenge::RVNG_SEEK_SET);
      long filePos=(long) input->readULong(4);
      if (filePos && (filePos<endPos || !input->checkPosition(filePos))) {
        f << "###";
        MWAW_DEBUG_MSG(("RagTimeParser::findDataZones: find an odd zone\n"));
      }
      else if (filePos) {
        MWAWEntry entry;
        entry.setBegin(filePos+(vers==1 ? 4 : 0));
        // checkme: some time in v3 the zones pos is decaled by 2, if *(pos+6)&0x80?
        entry.setType(what);
        m_state->m_dataZoneMap.insert
        (std::multimap<std::string,MWAWEntry>::value_type(entry.type(),entry));
      }
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+zoneLength, librevenge::RVNG_SEEK_SET);
  }
  ascii().addPos(endPos);
  ascii().addNote("_");

  return true;
}

bool RagTimeParser::findRsrcZones()
{
  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  long pos=input->tell();
  int dSz=(int) input->readULong(2);
  f << "Entries(RsrcMap):";
  if ((dSz%10)!=2 || !input->checkPosition(pos+2+dSz)) {
    MWAW_DEBUG_MSG(("RagTimeParser::findRsrcZones: find odd size\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  // the resource map
  int N=dSz/10;
  f << "N=" << N << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (int i=0; i<N; ++i) {
    long fPos=input->tell();
    f.str("");
    f << "RsrcMap[" << i << "]:";
    MWAWEntry entry;
    long depl=(long) input->readULong(4);
    entry.setBegin(pos+depl+2);
    std::string what("rsrc");
    for (int c=0; c<4; ++c) {
      char ch=(char) input->readULong(1);
      if (ch==' ') what += '_';
      else what+=ch;
    }
    if (what=="rsrc____") what="rsrcUnamed";
    else if (what=="rsrcFHfl") what="rsrcFHFont";
    else if (what=="rsrcFoTa") what="rsrcFormat";
    else if (what=="rsrcFLin") what="rsrcLink";
    else if (what=="rsrcRTml") what="rsrcItem";
    entry.setType(what);
    entry.setId((int) input->readLong(2));
    f << what << "[" << entry.id() << "],";
    // no data rsrc seems to exists, so...
    if (depl) {
      f << std::hex << entry.begin() << std::dec << ",";
      if (!entry.begin() || !input->checkPosition(entry.begin())) {
        f << "###";
        MWAW_DEBUG_MSG(("RagTimeParser::findRsrcZones: the entry position seems bad\n"));
      }
      else
        m_state->m_RSRCZoneMap.insert
        (std::multimap<std::string,MWAWEntry>::value_type(entry.type(),entry));
    }
    input->seek(fPos+10, librevenge::RVNG_SEEK_SET);
    ascii().addPos(fPos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}
////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read a zone of text
////////////////////////////////////////////////////////////
bool RagTimeParser::readTextZone(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  int const vers=version();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+5+2+2+6)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readTextZone: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(TextZone):";
  int dSz=(int) input->readULong(2);
  int numChar=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  f << "N=" << numChar << ",";
  if (!input->checkPosition(endPos) || numChar>dSz) {
    MWAW_DEBUG_MSG(("RagTimeParser::readTextZone: the numChar seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  int val=(int) input->readLong(1); // always 0?
  if (val) f << "g0=" << val << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  ascii().addPos(endPos);
  ascii().addNote("_");

  pos = input->tell();
  f.str("");
  f << "TextZone[text]:";
  std::string text("");
  for (int i=0; i<numChar-1; ++i) text+=(char) input->readULong(1);
  f << text << ",";
  if (vers>=2 && (numChar%2)==1)
    input->seek(1, librevenge::RVNG_SEEK_CUR);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos = input->tell();
  f.str("");
  f << "TextZone:";
  int numStyles=(int) input->readULong(2);
  f << "numStyles=" << numStyles << ",";
  int const styleSize=vers>=2 ? 10:8;
  if (pos+2+styleSize*numStyles>endPos+2+4) {
    MWAW_DEBUG_MSG(("RagTimeParser::readTextZone: the number of styles seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i=0; i < numStyles; ++i) {
    pos = input->tell();
    f.str("");
    f << "TextZone[C" << i << "]:";
    input->seek(pos+styleSize, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  pos=input->tell();
  int numPara=(int) input->readULong(2);
  f.str("");
  f << "TextZone:";
  f << "numPara=" << numPara << ",";
  if (vers==1) {
    // no sure how to read this data, ...
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  if (pos+2+48*numPara+4>endPos) {
    MWAW_DEBUG_MSG(("RagTimeParser::readTextZone: the number of styles seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i=0; i < numPara; ++i) {
    pos = input->tell();
    f.str("");
    f << "TextZone[P" << i << "]:";
    input->seek(pos+48, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  pos=input->tell();
  dSz=(int) input->readULong(2);
  f.str("");
  f << "TextZone[A]:";
  if (pos+2+dSz+2>endPos) {
    MWAW_DEBUG_MSG(("RagTimeParser::readTextZone: the zoneA size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return true;
  }
  if (dSz==0) {
    ascii().addPos(pos);
    ascii().addNote("_");
  }
  else {
    // never seems
    MWAW_DEBUG_MSG(("RagTimeParser::readTextZone: find a zoneA zone\n"));
    f << "#";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  // now the token?
  pos=input->tell();
  dSz=(int) input->readULong(2);
  f.str("");
  f << "TextZone[B]:";
  if (pos+2+dSz!=endPos) {
    MWAW_DEBUG_MSG(("RagTimeParser::readTextZone: the zoneB size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return true;
  }
  if (dSz==0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  int n=0;
  while (!input->isEnd()) {
    // find either: "," or format?=[2,id=[1-19],1,1,sSz,0],"text"
    pos=input->tell();
    if (pos>=endPos) break;
    f.str("");
    f << "TextZone[B" << n++ << "],";
    dSz=(int) input->readULong(2);
    long fEndPos=pos+dSz;
    if (dSz<3 || fEndPos>endPos) {
      MWAW_DEBUG_MSG(("RagTimeParser::readTextZone: the zoneB size seems bad\n"));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return true;
    }
    int sSz=(int) input->readULong(1);
    if (sSz+3!=dSz && dSz<14) {
      MWAW_DEBUG_MSG(("RagTimeParser::readTextZone: can not find the zoneB format\n"));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      input->seek(fEndPos, librevenge::RVNG_SEEK_SET);
      continue;
    }
    if (sSz+3!=dSz) {
      sSz=dSz-14;
      input->seek(pos+2, librevenge::RVNG_SEEK_SET);
      f << "format?=[";
      for (int i=0; i< 6; ++i) { // small number
        val=(int) input->readLong(2);
        if (val) f << val << ",";
        else f << "_";
      }
      f << "],";
    }
    text="";
    for (int i=0; i<sSz; ++i)
      text+=(char) input->readULong(1);
    f << "\"" << text << "\",";
    input->seek(fEndPos, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// picture zone
////////////////////////////////////////////////////////////
bool RagTimeParser::readPictZone(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+0x4a)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readPictZone: the position seems bad\n"));
    return false;
  }
  if (version()<2) {
    MWAW_DEBUG_MSG(("RagTimeParser::readPictZone: must not be called for v1-2... file\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(PictZone):";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  if (dSz<76 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readPictZone: the header size seems bad\n"));
    f << "###dSz=" << dSz << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  int headerSz=(int) input->readULong(2);
  if ((headerSz&0x8000)) // default
    headerSz&=0x7FFF;
  else
    f << "noHeaderflags,";
  if (headerSz>dSz) {
    MWAW_DEBUG_MSG(("RagTimeParser::readPictZone: the header size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }

  input->seek(pos+74, librevenge::RVNG_SEEK_SET);
  ascii().addDelimiter(input->tell(),'|');
  long pictSz=(long) input->readULong(2);
  if (headerSz+pictSz>dSz) {
    MWAW_DEBUG_MSG(("RagTimeParser::readPictZone: the pict size seems bad\n"));
    f << "###pictSz=" << std::hex << pictSz << std::dec << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(pos+2+headerSz, librevenge::RVNG_SEEK_SET);
  pos=input->tell();
  //TODO: store the picture
  ascii().skipZone(pos, pos+pictSz-1);

  input->seek(pos+pictSz, librevenge::RVNG_SEEK_SET);
  pos=input->tell();
  if (pos!=endPos) {
    // find one time another struct: a label?
    MWAW_DEBUG_MSG(("RagTimeParser::readPictZone: find some extra data\n"));
    ascii().addPos(pos);
    ascii().addNote("PictZone[end]");
  }
  return true;
}

bool RagTimeParser::readPictZoneV2(MWAWEntry &entry)
{
  if (version()>=2) {
    MWAW_DEBUG_MSG(("RagTimeParser::readPictZoneV2: must not be called for v3... file\n"));
    return false;
  }
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0) {
    MWAW_DEBUG_MSG(("RagTimeParser::readPictZoneV2: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(PictZone):";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  if (dSz<0x14 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readPictZoneV2: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(endPos);
  ascii().addNote("_");
  int headerSz=(int) input->readULong(2);
  int val=(int) input->readLong(2); // always 0?
  if (val) f << "f0=" << val << ",";
  if (!input->checkPosition(pos+2+headerSz+10)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readPictZoneV2: the header size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(pos+2+headerSz, librevenge::RVNG_SEEK_SET);

  /* TO: store the picture position +
     we need probably to read the bitmap: format:
     804289ce[001c]:rowSize[00000000][0046]:numRow[00d1]:numCols
  */
  pos=input->tell();
  ascii().skipZone(pos, endPos-1);
  return true;
}

////////////////////////////////////////////////////////////
// color map
////////////////////////////////////////////////////////////
bool RagTimeParser::readColorMapV2(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+8)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readColorMapV2: the position seems bad\n"));
    return false;
  }
  if (version()>=2) {
    MWAW_DEBUG_MSG(("RagTimeParser::readColorMapV2: must only be called for v1-2... file\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(ColorMap):";
  int val=(int) input->readLong(2); // always 0?
  if (val) f << "f0=" << val << ",";
  int dSz=(int) input->readULong(2);
  int N[3];
  for (int i=0; i<3; ++i)
    N[i]=(int) input->readLong(2);
  f << "numColors=" << N[1] << ",";
  if (N[0]+1!=N[1]) {
    MWAW_DEBUG_MSG(("RagTimeParser::readColorMapV2: arghhs N[0]+1!=N[1]\n"));
    f << "###num?=" << N[0] << ",";
  }
  if (N[2]>=0) f << "maxptr=" << N[2] << ",";
  long endPos=pos+4+dSz;
  if (dSz<6+10*N[1]+(N[2]+1)*4+N[1] || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readColorMapV2: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (int i=0; i<=N[0]; ++i) {
    //-2: white, -1: black, ...
    pos=input->tell();
    f.str("");
    f << "ColorMap-" << i << ":";
    unsigned char col[3];
    for (int j=0; j < 3; j++)
      col[j] = (unsigned char)(input->readULong(2)>>8);
    f << "col=" << MWAWColor(col[0],col[1],col[2]) << ",";
    val=(int) input->readULong(2); // 1|2|3|4|22
    f << "f0=" << std::hex << val << std::dec << ",";
    f << "id=" << input->readLong(2) << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  for (int i=0; i<=N[2]; ++i) {
    pos=input->tell();
    f.str("");
    f << "ColorMap-A" << i << ":";
    val=(int) input->readULong(2); // the index in the text?
    f << "text[pos]?=" << std::hex << val << std::dec << ",";
    val=(int) input->readULong(2); // 1|2|4 color used?
    f << "id?=" << val << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  for (int i=0; i< N[1]; ++i) { // the color name
    pos=input->tell();
    f.str("");
    f << "ColorMap-" << i << ":name=";
    int fSz=(int) input->readULong(1);
    if (pos+1+fSz>endPos) {
      MWAW_DEBUG_MSG(("RagTimeParser::readColorMapV2: can not read a string\n"));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return false;
    }
    std::string text("");
    for (int c=0; c<fSz; ++c) text+=(char) input->readULong(1);
    f << text;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  pos=input->tell();
  if (pos!=endPos) {
    MWAW_DEBUG_MSG(("RagTimeParser::readColorMapV2: find some extra data\n"));
    ascii().addPos(pos);
    ascii().addNote("ColorMap-end:###");
  }
  return true;
}

////////////////////////////////////////////////////////////
// print info
////////////////////////////////////////////////////////////
bool RagTimeParser::readPrintInfo(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+120)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readPrintInfo: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(rsrcPREC):";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  libmwaw::PrinterInfo info;
  if (dSz<120 || !input->checkPosition(endPos) || !info.read(input)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readPrintInfo: the zone seems too short\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  f << info;

  Vec2i paperSize = info.paper().size();
  Vec2i pageSize = info.page().size();
  if (pageSize.x() <= 0 || pageSize.y() <= 0 ||
      paperSize.x() <= 0 || paperSize.y() <= 0) return false;

  // define margin from print info
  Vec2i lTopMargin= -1 * info.paper().pos(0);
  Vec2i rBotMargin=info.paper().pos(1) - info.page().pos(1);

  // move margin left | top
  int decalX = lTopMargin.x() > 14 ? lTopMargin.x()-14 : 0;
  int decalY = lTopMargin.y() > 14 ? lTopMargin.y()-14 : 0;
  lTopMargin -= Vec2i(decalX, decalY);
  rBotMargin += Vec2i(decalX, decalY);

  // decrease right | bottom
  int rightMarg = rBotMargin.x() -10;
  if (rightMarg < 0) rightMarg=0;
  int botMarg = rBotMargin.y() -50;
  if (botMarg < 0) botMarg=0;

  getPageSpan().setMarginTop(lTopMargin.y()/72.0);
  getPageSpan().setMarginBottom(botMarg/72.0);
  getPageSpan().setMarginLeft(lTopMargin.x()/72.0);
  getPageSpan().setMarginRight(rightMarg/72.0);
  getPageSpan().setFormLength(paperSize.y()/72.);
  getPageSpan().setFormWidth(paperSize.x()/72.);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  if (pos==endPos) return true;
  // TODO read the last part 0x20(size?) + page dim + some flags
  f.str("");
  f << "rsrcPREC[data]:";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  return true;
}

////////////////////////////////////////////////////////////
// unknown zone
////////////////////////////////////////////////////////////
bool RagTimeParser::readZone3(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+0x66)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readZone3: the position seems bad\n"));
    return false;
  }
  if (version()<2) {
    MWAW_DEBUG_MSG(("RagTimeParser::readZone3: must not be called for v1-2... file\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(Zone3):";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  if (dSz<0x62 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readZone3: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  int val;
  for (int i=0; i<6; ++i) { // f0=0, f1=4|6|44, f2=1-8, f3=1-f, f4=1-5, f5=1-3
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  for (int i=0; i<6; ++i) // g0~40, g1=g2=2, g3~16, g4=[b-10], g5=[8-c]|800d
    f << "g" << i << "=" << double(input->readLong(4))/65536. << ",";
  long zoneBegin[11];
  zoneBegin[10]=endPos;
  for (int i=0; i<10; ++i) {
    zoneBegin[i]=(long) input->readULong(4);
    if (!zoneBegin[i]) continue;
    f << "zone" << i << "=" << std::hex << pos+2+zoneBegin[i] << std::dec << ",";
    if (pos+2+zoneBegin[i]>endPos) {
      MWAW_DEBUG_MSG(("RagTimeParser::readZone3: the zone %d seems bad\n",i));
      zoneBegin[i]=0;
      f << "###";
      continue;
    }
    zoneBegin[i]+=pos+2;
  }
  f << "fl?=["; // or some big number
  for (int i=0; i<8; ++i) {
    val=(int) input->readULong(2);
    if (val)
      f << std::hex << val << std::dec << ",";
    else
      f << "_,";
  }
  f << "],";
  for (int i=0; i<3; ++i) { // h0=0-4, h1=h2=0
    val=(int) input->readULong(2);
    if (val)
      f << "h" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  // now read the different zone, first set the endPosition
  for (int i=9; i>=0; --i) {
    if (zoneBegin[i]==0)
      zoneBegin[i]=zoneBegin[i+1];
    else if (zoneBegin[i]>zoneBegin[i+1]) {
      MWAW_DEBUG_MSG(("RagTimeParser::readZone3: the zone %d seems bad(II)\n",i));
      zoneBegin[i]=zoneBegin[i+1];
    }
  }
  for (int i=0; i<10; ++i) {
    if (zoneBegin[i+1]<=zoneBegin[i]) continue;
    f.str("");
    f << "Zone3-" << i << ":";
    // Zone3-3: sz+[32bytes]+(N+1)*12
    // Zone3-9: sz+N+N*14
    ascii().addPos(zoneBegin[i]);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool RagTimeParser::readZone3V2(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+6)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readZone3V2: the position seems bad\n"));
    return false;
  }
  if (version()>=2) {
    MWAW_DEBUG_MSG(("RagTimeParser::readZone3V2: must not be called for v3... file\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(Zone3):";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  if (dSz<4 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readZone3V2: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  long zoneBegin[3];
  zoneBegin[2]=endPos;
  for (int i=0; i<2; ++i) {
    zoneBegin[i]=pos+6+(long) input->readULong(2);
    if (zoneBegin[i]>=endPos) {
      f << "###";
      MWAW_DEBUG_MSG(("RagTimeParser::readZone3V2: the zone begin seems bad%d\n", i));
    }
    f << "ptr[" << i << "]=" << std::hex << zoneBegin[i] << std::dec << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  // now read the different zone, first set the endPosition
  for (int i=1; i>=0; --i) {
    if (zoneBegin[i]==0)
      zoneBegin[i]=zoneBegin[i+1];
    else if (zoneBegin[i]>zoneBegin[i+1]) {
      MWAW_DEBUG_MSG(("RagTimeParser::readZone3V2: the zone %d seems bad(II)\n",i));
      zoneBegin[i]=zoneBegin[i+1];
    }
  }

  if (zoneBegin[0]<zoneBegin[1]) {
    long zoneEndPos=zoneBegin[1];
    int n=0;
    input->seek(zoneBegin[0], librevenge::RVNG_SEEK_SET);
    while (!input->isEnd()) {
      pos=input->tell();
      if (pos+2>zoneEndPos) break;
      f.str("");
      f << "Zone3[A" << n++ << "]:";
      int val=(int) input->readLong(1);
      f << "type=" << val << ",";
      dSz=(int) input->readULong(1);
      if (pos+6+dSz>zoneEndPos) {
        MWAW_DEBUG_MSG(("RagTimeParser::readZone3V2: problem reading some zone3[A] field\n"));
        f << "###";
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());
        break;
      }
      if ((dSz%2)==1) ++dSz;
      input->seek(pos+6+dSz, librevenge::RVNG_SEEK_SET);
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
    }
  }
  if (zoneBegin[1]<endPos) {
    input->seek(zoneBegin[1], librevenge::RVNG_SEEK_SET);
    bool ok=true;
    for (int i=0; i<2; ++i) {
      pos=input->tell();
      f.str("");
      f << "Zone3[B" << i << "]:";
      int n=(int) input->readULong(2);
      f << "N=" << n << ",";
      static int const dataSize[]= {20,14};
      if (pos+2+dataSize[i]*n>endPos) {
        MWAW_DEBUG_MSG(("RagTimeParser::readZone3V2: problem reading some zone3[B%d] field\n", i));
        f << "###";
        ok=false;
      }
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      if (!ok) break;
      for (int j=0; j<n; ++j) {
        pos=input->tell();
        f.str("");
        f << "Zone3[B" << i << "-" << j << "]:";
        input->seek(pos+dataSize[i], librevenge::RVNG_SEEK_SET);
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());
      }
    }
    if (ok) {
      /* finally something like
         86000000200201000c000014003c00030c090000
         or
         86000000204201000a000003004000060d0a000000000ce5410000010000000000000000688f688f688f0000000000000000688f688f688f000000000000
         font + ?
       */
      ascii().addPos(input->tell());
      ascii().addNote("Zone3[B-end]:");
    }
  }
  return true;
}

bool RagTimeParser::readZone6(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+0x66)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readZone6: the position seems bad\n"));
    return false;
  }
  if (version()<2) {
    MWAW_DEBUG_MSG(("RagTimeParser::readZone6: must not be called for v1-2... file\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(Zone6):";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  if (dSz<0x62 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readZone6: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  std::set<long> beginZones;
  long zonesList[4]= {0,0,0,0};
  int val;
  beginZones.insert(endPos);
  long ptr=(long) input->readULong(4);
  val=(int) input->readULong(2);
  if (ptr) {
    f << "zone0=" << std::hex << pos+2+ptr << std::dec << "[" << val << "],";
    if (pos+2+ptr>endPos) {
      MWAW_DEBUG_MSG(("RagTimeParser::readZone6: the zone 0 seems bad\n"));
      f << "###";
    }
    else {
      beginZones.insert(pos+2+ptr);
      zonesList[0]=pos+2+ptr;
    }
  }
  for (int i=0; i<2; ++i) {
    val=(int) input->readULong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }

  for (int i=0; i<3; ++i) {
    ptr=(long) input->readULong(4);
    val=(int) input->readULong(2);
    if (!ptr) continue;
    f << "zone" << i+1 << "=" << std::hex << pos+2+ptr << std::dec << "[" << val << "],";
    if (pos+2+ptr>endPos) {
      MWAW_DEBUG_MSG(("RagTimeParser::readZone6: the zone %d seems bad\n",i+1));
      f << "###";
      continue;
    }
    beginZones.insert(pos+2+ptr);
    zonesList[i+1]=pos+2+ptr;
  }
  ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i=0; i<4; ++i) {
    pos=zonesList[i];
    if (!pos || pos==endPos) continue;
    f.str("");
    f << "Zone6-" << i << ":";
    std::set<long>::const_iterator it=beginZones.find(pos);
    if (it==beginZones.end() || ++it==beginZones.end()) {
      MWAW_DEBUG_MSG(("RagTimeParser::readZone6: can not find the end of zone\n"));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      continue;
    }
    long zEndPos=*it;
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    while (!input->isEnd()) {
      pos=input->tell();
      if (pos+4>zEndPos) break;
      dSz=(int) input->readULong(4);
      if (pos+4+dSz>zEndPos) {
        MWAW_DEBUG_MSG(("RagTimeParser::readZone6: can not determine the end of sub zone\n"));
        f << "###";
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());
        break;
      }
      input->seek(pos+4+dSz, librevenge::RVNG_SEEK_SET);
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
    }
  }
  return true;
}

bool RagTimeParser::readZone0(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+22)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readZone0: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(Zone0):";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  if (dSz<20 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readZone0: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  int val;
  for (int i=0; i<4; i++) { // always 0?
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  int fl=(int) input->readULong(4); // 71606|e2c60|e2c68|e6658
  f << "flag?=" << std::hex << fl << std::dec << ",";
  val=(int) input->readULong(4); // always equal to fl?
  if (val!=fl)
    f << "flag1?=" << std::hex << val << std::dec << ",";
  val=(int) input->readLong(2); // 4|5
  if (val!=4) f << "f4=" << val << ",";
  val=(int) input->readLong(2); // always 0
  if (val) f << "f5=" << val << ",";
  if (input->tell()!=endPos)
    ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// unknown rsrc zone: table + name
////////////////////////////////////////////////////////////
bool RagTimeParser::readRsrcFont(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+0x26)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcFont: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(" << entry.type() << ")[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  int headerSz=(int) input->readULong(2);
  int fSz=(int) input->readULong(2);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (headerSz<0x20 || fSz<0x10 || dSz<headerSz+(N+1)*fSz || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcFont: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  input->seek(pos+2+headerSz, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  std::set<long> posSet;
  posSet.insert(endPos);
  for (int i=0; i<=N; ++i) {
    pos=input->tell();
    f.str("");
    f << entry.type() << "-" << i << ":";
    int val=(int) input->readLong(2); // small number
    if (val) f << "f0=" << val << ",";
    val=(int) input->readLong(2); // always 0?
    if (val) f << "f1=" << val << ",";
    int fPos=(int) input->readULong(2);
    f << "pos[name]=" << std::hex << entry.begin()+2+fPos << std::dec << ",";
    posSet.insert(entry.begin()+2+fPos);
    val=(int) input->readLong(2);
    if (val) f << "fId=" << val << ",";
    ascii().addDelimiter(input->tell(),'|');
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  for (std::set<long>::const_iterator it=posSet.begin(); it!=posSet.end();) {
    pos=*(it++);
    if (pos>=endPos) break;
    long nextPos=it==posSet.end() ? endPos : *it;
    f.str("");
    f << entry.type() << "[name]:";
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    std::string name("");
    while (!input->isEnd() && input->tell()<nextPos) {
      char c=(char) input->readULong(1);
      if (c=='\0') break;
      name+=c;
    }
    f << name;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool RagTimeParser::readRsrcFormat(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+0x26)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcFormat: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(" << entry.type() << ")[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  int headerSz=(int) input->readULong(2);
  int fSz=(int) input->readULong(2);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (headerSz<0x20 || fSz<6 || dSz<headerSz+(N+1)*fSz || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcFormat: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  input->seek(pos+2+headerSz, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  std::set<long> posSet;
  posSet.insert(endPos);
  for (int i=0; i<=N; ++i) {
    pos=input->tell();
    f.str("");
    f << entry.type() << "-" << i << ":";
    int val=(int) input->readLong(2); // 0 (except last)
    if (val) f << "f0=" << val << ",";
    val=(int) input->readLong(2); // always 1 ?
    if (val!=1) f << "f1=" << val << ",";
    int fPos=(int) input->readULong(2);
    f << "pos[def]=" << std::hex << entry.begin()+2+fPos << std::dec << ",";
    posSet.insert(entry.begin()+2+fPos);
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  for (std::set<long>::const_iterator it=posSet.begin(); it!=posSet.end();) {
    pos=*(it++);
    if (pos>=endPos) break;
    long nextPos=it==posSet.end() ? endPos : *it;
    f.str("");
    f << entry.type() << "[def]:";
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    int depl[4];
    f << "depl=[";
    for (int i=0; i<4; ++i) {
      depl[i]=(int) input->readULong(1);
      if (depl[i]) f << depl[i] << ",";
      else f << "_,";
    }
    f << "],";
    // fixme: parser the 4 potential header zones
    headerSz=depl[3];
    if ((headerSz&1)) ++headerSz;
    input->seek(pos+headerSz, librevenge::RVNG_SEEK_SET);
    int cSz=(int) input->readULong(1);
    if (headerSz<4||cSz<=0||pos+headerSz+1+cSz!=nextPos) {
      MWAW_DEBUG_MSG(("RagTimeParser::readRsrcFormat: can not read a format\n"));
      f << "###";
    }
    else {
      ascii().addDelimiter(input->tell()-1,'|');
      std::string name("");
      for (int i=0; i<cSz; ++i) name += (char) input->readULong(1);
      f << name << ",";
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool RagTimeParser::readRsrcCHTa(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+0x26)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcCHTa: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(" << entry.type() << ")[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  int headerSz=(int) input->readULong(2);
  int fSz=(int) input->readULong(2);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (headerSz<0x20 || fSz<0x8 || dSz<headerSz+(N+1)*fSz || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcCHTa: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  input->seek(pos+2+headerSz, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  std::set<long> posSet;
  posSet.insert(endPos);
  for (int i=0; i<=N; ++i) {
    pos=input->tell();
    f.str("");
    f << entry.type() << "-" << i << ":";
    int val=(int) input->readLong(2); // 0 (except last)
    if (val) f << "f0=" << val << ",";
    // CHTa[0](fSz=0xc) does contains name ptr, but not CHTa[1](fSz=0x8)
    if (fSz>=0xc) {
      int fPos=(int) input->readULong(2);
      if (fPos) {
        f << "pos[def]=" << std::hex << entry.begin()+2+fPos << std::dec << ",";
        posSet.insert(entry.begin()+2+fPos);
      }
      // then 3*int for color
      // 1 int for index or col number ?
    }
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  for (std::set<long>::const_iterator it=posSet.begin(); it!=posSet.end();) {
    pos=*(it++);
    if (pos>=endPos) break;
    long nextPos=it==posSet.end() ? endPos : *it;
    f.str("");
    f << entry.type() << "[name]:";
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    std::string name("");
    while (!input->isEnd() && input->tell()<nextPos) {
      char c=(char) input->readULong(1);
      if (c=='\0') break;
      name+=c;
    }
    f << name;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool RagTimeParser::readRsrcItemFormat(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+0x26)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcItemFormat: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(" << entry.type() << ")[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  int headerSz=(int) input->readULong(2);
  int fSz=(int) input->readULong(2);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (headerSz<0x20 || fSz<0x10 || dSz<headerSz+(N+1)*fSz || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcItemFormat: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  input->seek(pos+2+headerSz, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  std::set<long> posSet;
  posSet.insert(endPos);
  for (int i=0; i<=N; ++i) {
    pos=input->tell();
    f.str("");
    f << entry.type() << "-" << i << ":";
    int fPos=(int) input->readULong(2);
    f << "pos[name]=" << std::hex << entry.begin()+2+fPos << std::dec << ",";
    posSet.insert(entry.begin()+2+fPos);
    ascii().addDelimiter(input->tell(),'|');
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  for (std::set<long>::const_iterator it=posSet.begin(); it!=posSet.end();) {
    pos=*(it++);
    if (pos>=endPos) break;
    long nextPos=it==posSet.end() ? endPos : *it;
    f.str("");
    f << entry.type() << "[name]:";
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    std::string name("");
    while (!input->isEnd() && input->tell()<nextPos) {
      char c=(char) input->readULong(1);
      if (c=='\0') break;
      name+=c;
    }
    f << name;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// other unknown rsrc zone
////////////////////////////////////////////////////////////
bool RagTimeParser::readRsrcCalc(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+0x26)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcCalc: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(rsrcCalc)[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  int val=(int) input->readLong(2); // always 0
  if (val) f << "f0=" << val << ",";
  val=(int) input->readLong(2); // a small number
  if (val) f << "f1=" << val << ",";
  int N=(int) input->readLong(2);
  f << "N=" << N << ",";
  long endPos=pos+2+dSz;
  if (dSz!=24+N*26 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcCalc: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  input->seek(pos+2+24, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "rsrcCalc-" << i << ":";

    input->seek(pos+26, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool RagTimeParser::readRsrcFHsl(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+0x26)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcFHsl: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(rsrcFHsl)[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  int headerSz=(int) input->readULong(2);
  int fSz=(int) input->readULong(2);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (headerSz<0x2c || dSz!=headerSz+(N+1)*fSz || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcFHsl: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  input->seek(pos+2+headerSz, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i=0; i<=N; ++i) {
    pos=input->tell();
    f.str("");
    f << "rsrcFHsl-" << i << ":";
    int val=(int) input->readLong(2); // 01?
    if (val==-1) f << "unused,";
    else if (val) f << "f0=" << val << ",";
    // then 2 small number

    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool RagTimeParser::readRsrcFHwl(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+0x26)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcFHwl: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(rsrcFHwl)[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  int headerSz=(int) input->readULong(2);
  int fSz=(int) input->readULong(2);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (headerSz<0x26 || dSz!=headerSz+(N+1)*fSz || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcFHwl: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  for (int i=0; i<8; ++i) {
    int val=(int) input->readLong(2);
    static int const expected[]= {0x200, 2, 0, 0, 0, 0, 5, 0/* small number*/};
    if (val!=expected[i])
      f << "f" << i+2 << "=" << val << ",";
  }
  for (int i=0; i<8; ++i) { // g4=1, g6=17|21|2b|7b, g7=0x208
    int val=(int) input->readLong(2);
    f << "g" << i << "=" << val << ",";
  }
  input->seek(pos+2+headerSz, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i=0; i<=N; ++i) {
    pos=input->tell();
    f.str("");
    f << "rsrcFHwl-" << i << ":";
    /* a small number, except for the last value
       so, maybe the interesting data are at the end of the bloc, in pos+0x210-4 ?
       or the block are subdivided differently
     */
    f << "N=" << input->readLong(2) << ",";
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool RagTimeParser::readRsrcSpDI(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+0x20)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcSpDI: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(" << entry.type() << ")[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  int headerSz=(int) input->readULong(2);
  int fSz=(int) input->readULong(2);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (headerSz<0x20 || fSz<0x8 || dSz<headerSz+(N+1)*fSz || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcSpDI: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  input->seek(pos+2+headerSz, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  std::set<long> posSet;
  posSet.insert(endPos);
  for (int i=0; i<=N; ++i) {
    pos=input->tell();
    f.str("");
    f << entry.type() << "-" << i << ":";
    int val=(int) input->readLong(2); // 0 (except last)
    if (val) f << "f0=" << val << ",";
    int fPos=(int) input->readULong(2);
    if (fPos) {
      f << "pos[def]=" << std::hex << entry.begin()+2+fPos << std::dec << ",";
      posSet.insert(entry.begin()+2+fPos);
    }
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  for (std::set<long>::const_iterator it=posSet.begin(); it!=posSet.end();) {
    pos=*(it++);
    if (pos>=endPos) break;
    f.str("");
    f << entry.type() << "[data]:";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool RagTimeParser::readRsrcBeDc(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+52)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcBeDc: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(rsrcBeDc)[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  if (dSz<52 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcBeDc: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  int val=(int) input->readLong(2); // alway 1?
  if (val!=1) f << "f0=" << val << ",";
  val=(int) input->readLong(2); // alway 0?
  if (val) f << "f1=" << val << ",";
  for (int i=0; i<2; ++i) { // f2=0|1|11a0|349c|5c74|a250|e09c, f3=0|1|a|17|..2014
    val=(int) input->readULong(2);
    if (val) f << "f" << i+2 << "=" << std::hex << val << std::dec << ",";
  }
  f << "id?=" << std::hex << input->readULong(4) << std::dec << ","; // a really big number
  for (int i=0; i<7; ++i) { // all 0, or g2=1,g3=157,g4=13,g5=1f5|275: (g2,g3,g4,g5) a dim?
    val=(int) input->readULong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  for (int i=0; i<13; ++i) { // begin by small int, h2,h5: maybe two bool,
    val=(int) input->readLong(2);
    if (val) f << "h" << i << "=" << val << ",";
  }
  if (input->tell()!=endPos)
    ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

bool RagTimeParser::readRsrcBtch(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+6)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcBtch: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(rsrcBtch)[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  if (dSz<6 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcBtch: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  int val=(int) input->readLong(2); // an id?
  f << "id=" << val << ",";
  for (int i=0; i<2; ++i) { // alway 0?
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

bool RagTimeParser::readRsrcfppr(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+0x4a)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcfppr: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(rsrcfppr)[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  if (dSz<0x1a || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcfppr: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  for (int i=0; i<5; ++i) {
    int const expected[]= {2, 0x2d, 0x3c, 0, 0x64 };
    int val=(int) input->readLong(2);
    if (val!=expected[i]) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  for (int i=0; i<4; ++i) { // g0-g2: find small number of form 10*k+3 and often g1=g0+10, g2=g1+10 ; g3=0
    int val = (int) input->readLong(4);
    if (val)
      f << "g" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

bool RagTimeParser::readRsrcSele(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+4)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcSele: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(rsrcSele)[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  // checkme: what are the posible size value, I find either 4 or 18, 26, 74
  if ((dSz!=4 && (dSz%8)!=2) || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcSele: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  int val=(int) input->readLong(2);
  f << "f0=" << val << ",";
  if (dSz==4) {
    val=(int) input->readLong(2); // maybe 2 bool
    f << "f1=" << val << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return true;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  int N=(dSz/8);
  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "rsrcSele-" << i << ":";
    input->seek(pos+8, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool RagTimeParser::readRsrcSpDo(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  int const vers=version();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+0x4a)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcSpDo: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(rsrcSpDo)[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  int const expectedSz=vers>2 ? 0x4e : 0x4a;
  long endPos=pos+2+dSz;
  if (dSz!=expectedSz || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcSpDo: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  for (int i=0; i<2; ++i) { // f1=0|80
    int val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  int val0;
  for (int i=0; i<10; ++i) { // find g0=3+10*k, g1=10+g0, g2=10+g1, g3=10+g2, other 0|3+10*k1
    int val = (int) input->readLong(4);
    if (i==0) {
      val0=val;
      f << "g0=" << val << ",";
    }
    else if (i<4 && val!=val0+10*i)
      f << "g" << i << "=" << val << ",";
    else if (i>=4 && val)
      f << "g" << i << "=" << val << ",";
  }
  for (int i=0; i<9; ++i) {
    int val = (int) input->readULong(2);
    static int const expected[]= {0,0,0,0x64,0x3ff5,0x8312,0x6e97,0x8d4f,0xdf3b};
    if (val!=expected[i])
      f << "h" << i << "=" << std::hex << val << std::dec << ",";
  }
  int const numVal=vers>2 ? 6:4;
  for (int i=0; i<numVal; ++i) { // k0=small int, k1=k0+1, k2=k3=k4=0, k5=0|b7|e9|f3
    int val = (int) input->readLong(2);
    if (val) f << "k" << i << "=" << val << ",";
  }
  f << "id?=" << std::hex << input->readULong(4) << std::dec << ","; // a big number
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

bool RagTimeParser::readRsrcStructured(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcStructured: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(" << entry.type() << ")[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  if (dSz==0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }
  long endPos=pos+2+dSz;
  int headerSz=(int) input->readULong(2);
  int fSz=(int) input->readULong(2);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (headerSz<6 || dSz<headerSz+(N+1)*fSz || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcStructured: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  input->seek(pos+2+headerSz, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i=0; i<=N; ++i) {
    pos=input->tell();
    f.str("");
    f << entry.type() << "-" << i << ":";
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

/* this field has often a standart formula: header[size], field[size], N but sometimes
   it is a string or something else. Ie. we must find where the field format is stored...
 */
bool RagTimeParser::readRsrcUnamed(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcUnamed: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(rsrcUnamed)[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  if (dSz==0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }
  long endPos=pos+2+dSz;
  int headerSz=(int) input->readULong(2);
  int fSz=(int) input->readULong(2);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (headerSz<6 || dSz<headerSz+(N+1)*fSz || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeParser::readRsrcUnamed: the size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  input->seek(pos+2+headerSz, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i=0; i<=N; ++i) {
    pos=input->tell();
    f.str("");
    f << "rsrcUnamed-" << i << ":";
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  if (input->tell()!=endPos) {
    ascii().addPos(input->tell());
    ascii().addNote("rsrcUnamed-end");
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool RagTimeParser::checkHeader(MWAWHeader *header, bool /*strict*/)
{
  *m_state = RagTimeParserInternal::State();

  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork())
    return false;

  libmwaw::DebugStream f;
  f << "FileHeader:";
  int headerSize=48;
  if (!input->checkPosition(headerSize)) {
    MWAW_DEBUG_MSG(("RagTimeParser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0,librevenge::RVNG_SEEK_SET);
  if (input->readLong(2)) return false;
  int dim[2];
  for (int i=0; i< 2; ++i) dim[i]=(int) input->readLong(2);
  f << "dim=" << dim[1] << "x" << dim[0] << ",";
  int val=(int) input->readLong(2);
  if (val!=1) f << "first[page]?=" << val << ",";
  for (int i=0; i< 2; ++i) {
    f << "unkn" << i << "[";
    for (int j=0; j<7; ++j) {
      val=(int) input->readLong(2);
      static int const expected[]= {0,0,0,0,0x688f,0x688f,0x688f};
      // unkn0: always as expected
      // unkn1: find f0=-13:602, f1=0|...|431, f2=0|228|296
      if (val!=expected[j])
        f << "f" << j << "=" << val << ",";
    }
    f << "],";
  }
  for (int i=0; i<3; ++i) { // f0=0|4|9|a|14|23|29, other 0
    val=(int) input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  // finally a very big number, which seems a multiple of 4 as in 0xb713cfc
  f << "unkn2=" << std::hex << input->readULong(4) << std::dec << ",";
  int vers=(int) input->readULong(1);
  val=(int) input->readULong(1);
  switch (vers) {
  case 0:
    setVersion(1);
    if (val==0x64)
      f << "v2.1[classic],";
    else // or v1?
      f << "v2[" << std::hex << val << std::dec << "],";
    break;
  case 1:
    setVersion(3);
    if (val==0x2c) {
      f << "v3.0,";
      setVersion(2);
    }
    else if (val==0x90) f << "v3.[12],";
    else
      f << "v3[" << std::hex << val << std::dec << "],";
    break;
  default:
    MWAW_DEBUG_MSG(("RagTimeParser::checkHeader: unknown version=%d\n", vers));
    return false;
  }
  input->seek(headerSize, librevenge::RVNG_SEEK_SET);

  // ok, we can finish initialization
  if (header)
    header->reset(MWAWDocument::MWAW_T_RAGTIME, version());

  ascii().addPos(0);
  ascii().addNote(f.str().c_str());
  ascii().addPos(headerSize);

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
