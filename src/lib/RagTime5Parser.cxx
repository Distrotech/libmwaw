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
#include <set>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWTextListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWGraphicShape.hxx"
#include "MWAWHeader.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPictBitmap.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWStringStream.hxx"
#include "MWAWSubDocument.hxx"

#include "RagTimeSpreadsheet.hxx"
#include "RagTimeStruct.hxx"
#include "RagTimeText.hxx"

#include "RagTime5Parser.hxx"

/** Internal: the structures of a RagTime5Parser */
namespace RagTime5ParserInternal
{
////////////////////////////////////////
//! Internal: the pattern of a RagTimeManager
struct Pattern : public MWAWGraphicStyle::Pattern {
  //! constructor ( 4 int by patterns )
  Pattern(uint16_t const *pat=0) : MWAWGraphicStyle::Pattern(), m_percent(0)
  {
    if (!pat) return;
    m_colors[0]=MWAWColor::white();
    m_colors[1]=MWAWColor::black();
    m_dim=Vec2i(8,8);
    m_data.resize(8);
    for (size_t i=0; i < 4; ++i) {
      uint16_t val=pat[i];
      m_data[2*i]=(unsigned char)(val>>8);
      m_data[2*i+1]=(unsigned char)(val&0xFF);
    }
    int numOnes=0;
    for (size_t j=0; j < 8; ++j) {
      uint8_t val=(uint8_t) m_data[j];
      for (int b=0; b < 8; b++) {
        if (val&1) ++numOnes;
        val = uint8_t(val>>1);
      }
    }
    m_percent=float(numOnes)/64.f;
  }
  //! the percentage
  float m_percent;
};


////////////////////////////////////////
//! Internal: a picture of a RagTime5Parser
struct Picture {
  //! constructor
  Picture() : m_type(0), m_pos(), m_dim(), m_headerPos(0), m_isSent(false)
  {
  }
  //! the picture type(unsure)
  int m_type;
  //! the data position
  MWAWEntry m_pos;
  //! the dimension
  Box2f m_dim;
  //! the beginning of the header(for debugging)
  long m_headerPos;
  //! a flag to know if the picture is sent
  mutable bool m_isSent;
};

////////////////////////////////////////
//! Internal: a zone of a RagTime5Parser
struct Zone {
  //! the zone type
  enum Type { Main, Data, Empty, Unknown };
  //! constructor
  Zone(MWAWInputStreamPtr input, libmwaw::DebugFile &asc):
    m_type(Unknown), m_subType(0), m_defPosition(0), m_entry(), m_entriesList(), m_name(""), m_extra(""),
    m_input(input), m_defaultInput(true), m_asciiName(""), m_asciiFile(&asc), m_localAsciiFile()
  {
    for (int i=0; i<3; ++i) m_ids[i]=m_idsFlag[i]=0;
    for (int i=0; i<2; ++i) m_kinds[i]="";
  }
  //! destructor
  ~Zone() {}
  //! returns the main type
  std::string getKindLastPart(bool main=true) const
  {
    std::string res(m_kinds[main ? 0 : 1]);
    std::string::size_type pos = res.find_last_of(':');
    if (pos == std::string::npos) return res;
    return res.substr(pos+1);
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Zone const &z);
  //! returns the current input
  MWAWInputStreamPtr getInput()
  {
    return m_input;
  }
  //! reset the current input
  void setInput(MWAWInputStreamPtr input)
  {
    m_input = input;
    m_defaultInput = false;
  }
  //! returns true if the input correspond to the basic file
  bool isMainInput() const
  {
    return m_defaultInput;
  }
  //! returns the current ascii file
  libmwaw::DebugFile &ascii()
  {
    if (!m_defaultInput && !m_localAsciiFile)
      createAsciiFile();
    return *m_asciiFile;
  }
  //! defines the ascii name
  void setAsciiFileName(std::string const &name)
  {
    m_asciiName = name;
  }
  //! creates the ascii file
  void createAsciiFile()
  {
    if (m_asciiName.empty()) {
      MWAW_DEBUG_MSG(("RagTime5ParserInternal::Zone::createAsciiFile: can not find the ascii name\n"));
      return;
    }
    if (m_localAsciiFile) {
      MWAW_DEBUG_MSG(("RagTime5ParserInternal::Zone::createAsciiFile: the ascii file already exist\n"));
    }
    m_localAsciiFile.reset(new libmwaw::DebugFile(m_input));
    m_asciiFile = m_localAsciiFile.get();
    m_asciiFile->open(m_asciiName.c_str());
  }

  //! the zone type
  Type m_type;
  //! the zone sub type
  int m_subType;
  //! the position of the definition in the main zones
  long m_defPosition;
  //! the zone types: normal and packing
  std::string m_kinds[2];
  //! the zone entry
  MWAWEntry m_entry;
  //! the zone id
  int m_ids[3];
  //! the zone flag
  int m_idsFlag[3];
  //! the list of original entries
  std::vector<MWAWEntry> m_entriesList;
  //! the zone name (used mainly for debugging)
  std::string m_name;
  //! extra data
  std::string m_extra;
protected:
  //! the main input
  MWAWInputStreamPtr m_input;
  //! a flag used to know if the input is or not the default input
  bool m_defaultInput;
  //! the ascii file name ( used if we need to create a ascii file)
  std::string m_asciiName;
  //! the ascii file corresponding to an input
  libmwaw::DebugFile *m_asciiFile;
  //! the local ascii file ( if we need to create a new input)
  shared_ptr<libmwaw::DebugFile> m_localAsciiFile;
private:
  Zone(Zone const &orig);
  Zone &operator=(Zone const &orig);
};

std::ostream &operator<<(std::ostream &o, Zone const &z)
{
  /*
    main0: header,
    main1: zones info
    main4: zones
    main5: file information
  */
  switch (z.m_type) {
  case Zone::Main:
    if (z.m_idsFlag[0]==0)
      o << "main" << z.m_ids[0] << ",";
    else if (z.m_idsFlag[0]==1)
      o << "mZone" << z.m_ids[0] << ",";
    else
      o << "mZone" << z.m_ids[0] << ":#" << z.m_idsFlag[0] << ",";
    break;
  case Zone::Data:
    if (z.m_idsFlag[0]==0)
      o << "main" << z.m_ids[0] << ",";
    else if (z.m_idsFlag[0]==1)
      o << "data" << z.m_ids[0] << ",";
    else
      o << "data" << z.m_ids[0] << ":#" << z.m_idsFlag[0] << ",";
    break;
  case Zone::Empty:
    o << "unused" << z.m_ids[0] << ",";
    break;
  case Zone::Unknown:
  default:
    o << "##zone" << z.m_subType << "[" << z.m_ids[0] << "],";
    break;
  }
  for (int i=1; i<3; ++i) {
    if (!z.m_kinds[i-1].empty()) {
      o << z.m_kinds[i-1] << ",";
      continue;
    }
    if (!z.m_ids[i] && !z.m_idsFlag[i]) continue;
    o << "id" << i << "=" << z.m_ids[i];
    if (z.m_idsFlag[i]==0)
      o << "*";
    else if (z.m_idsFlag[i]!=1)
      o << ":" << z.m_idsFlag[i] << ",";
    o << ",";
  }
  if (!z.m_entriesList.empty()) {
    o << "ptr=" << std::hex;
    for (size_t i=0; i< z.m_entriesList.size(); ++i) {
      o << z.m_entriesList[i].begin() << "<->" << z.m_entriesList[i].end();
      if (i+1<z.m_entriesList.size())
        o << "+";
    }
    o << std::dec << ",";
  }
  o << z.m_extra << ",";
  return o;
}

////////////////////////////////////////
//! Internal: the state of a RagTime5Parser
struct State {
  //! constructor
  State() : m_zonesEntry(), m_zonesList(), m_idToTypeMap(), m_dataZoneMap(), m_idColorsMap(), m_patternList(),
    m_pageZonesIdMap(), m_idPictureMap(), m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0)
  {
  }
  //! init the pattern to default
  void initDefaultPatterns(int vers);
  //! the main zone entry
  MWAWEntry m_zonesEntry;
  //! the zone list
  std::vector<shared_ptr<Zone> > m_zonesList;
  //! a map id to type string
  std::map<int, std::string> m_idToTypeMap;
  //! a map: type->entry (datafork)
  std::multimap<std::string, MWAWEntry> m_dataZoneMap;
  //! the color map
  std::map<int, std::vector<MWAWColor> > m_idColorsMap;
  //! a list patternId -> pattern
  std::vector<Pattern> m_patternList;
  //! a map: page->main zone id
  std::map<int, std::vector<int> > m_pageZonesIdMap;
  //! a map: zoneId->picture (datafork)
  std::map<int, Picture> m_idPictureMap;
  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

void State::initDefaultPatterns(int vers)
{
  if (!m_patternList.empty()) return;
  if (vers <= 2) {
    static uint16_t const(s_pattern[4*40]) = {
      0x0, 0x0, 0x0, 0x0, 0x8000, 0x800, 0x8000, 0x800, 0x8800, 0x2200, 0x8800, 0x2200, 0x8822, 0x8822, 0x8822, 0x8822,
      0xaa55, 0xaa55, 0xaa55, 0xaa55, 0xdd77, 0xdd77, 0xdd77, 0xdd77, 0xddff, 0x77ff, 0xddff, 0x77ff, 0xffff, 0xffff, 0xffff, 0xffff,
      0x8888, 0x8888, 0x8888, 0x8888, 0xff00, 0x0, 0xff00, 0x0, 0xaaaa, 0xaaaa, 0xaaaa, 0xaaaa, 0xff00, 0xff00, 0xff00, 0xff00,
      0xeedd, 0xbb77, 0xeedd, 0xbb77, 0x1122, 0x4488, 0x1122, 0x4488, 0x102, 0x408, 0x1020, 0x4080, 0x102, 0x0, 0x0, 0x4080,
      0x77bb, 0xddee, 0x77bb, 0xddee, 0x8844, 0x2211, 0x8844, 0x2211, 0x8040, 0x2010, 0x804, 0x201, 0x8040, 0x0, 0x0, 0x201,
      0x55ff, 0x55ff, 0x55ff, 0x55ff, 0xaa00, 0xaa00, 0xaa00, 0xaa00, 0x8010, 0x220, 0x108, 0x4004, 0xb130, 0x31b, 0xd8c0, 0xc8d,
      0xff88, 0x8888, 0xff88, 0x8888, 0xaa00, 0x8000, 0x8800, 0x8000, 0xff80, 0x8080, 0x8080, 0x8080, 0xf0f0, 0xf0f0, 0xf0f, 0xf0f,
      0xc300, 0x0, 0x3c00, 0x0, 0x808, 0x8080, 0x8080, 0x808, 0x8040, 0x2000, 0x204, 0x800, 0x2, 0x588, 0x5020, 0x0,
      0x8000, 0x0, 0x0, 0x0, 0x40a0, 0x0, 0x40a, 0x0, 0x8142, 0x2418, 0x1824, 0x4281, 0x2050, 0x8888, 0x8888, 0x502,
      0xff80, 0x8080, 0xff08, 0x808, 0x44, 0x2810, 0x2844, 0x0, 0x103c, 0x5038, 0x1478, 0x1000, 0x8, 0x142a, 0x552a, 0x1408
    };
    m_patternList.resize(40);
    for (size_t i = 0; i < 40; i++)
      m_patternList[i]=Pattern(&s_pattern[i*4]);
  }
}

////////////////////////////////////////
//! Internal: the subdocument of a RagTime5Parser
class SubDocument : public MWAWSubDocument
{
public:
  // constructor
  SubDocument(RagTime5Parser &pars, MWAWInputStreamPtr input, int zoneId) :
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
    MWAW_DEBUG_MSG(("RagTime5ParserInternal::SubDocument::parse: no listener\n"));
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
RagTime5Parser::RagTime5Parser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWTextParser(input, rsrcParser, header), m_state()
{
  init();
}

RagTime5Parser::~RagTime5Parser()
{
}

void RagTime5Parser::init()
{
  resetTextListener();
  setAsciiName("main-1");

  m_state.reset(new RagTime5ParserInternal::State);

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);
}

bool RagTime5Parser::getColor(int colId, MWAWColor &color, int listId) const
{
  if (listId==-1) listId=version()>=2 ? 1 : 0;
  if (m_state->m_idColorsMap.find(listId)==m_state->m_idColorsMap.end()) {
    MWAW_DEBUG_MSG(("RagTime5Parser::getColor: can not find the color list %d\n", listId));
    return false;
  }
  std::vector<MWAWColor> const &colors=m_state->m_idColorsMap.find(listId)->second;
  if (colId<0 || colId>=(int)colors.size()) {
    MWAW_DEBUG_MSG(("RagTime5Parser::getColor: can not find color %d\n", colId));
    return false;
  }
  color=colors[size_t(colId)];
  return true;
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void RagTime5Parser::newPage(int number)
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
void RagTime5Parser::parse(librevenge::RVNGTextInterface *docInterface)
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
      //sendZones();
#if defined(DEBUG) && 0
      m_textParser->flushExtra();
      m_spreadsheetParser->flushExtra();
      flushExtra();
#endif
    }

    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("RagTime5Parser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetTextListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void RagTime5Parser::createDocument(librevenge::RVNGTextInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getTextListener()) {
    MWAW_DEBUG_MSG(("RagTime5Parser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  int numPages=1;
  // TODO
  m_state->m_actPage = 0;
  m_state->m_numPages=numPages;

  // create the page list
  MWAWPageSpan ps(getPageSpan());
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

bool RagTime5Parser::createZones()
{
  int const vers=version();
  if (vers<5) {
    MWAW_DEBUG_MSG(("RagTime5Parser::createZones: must be called for %d document\n", vers));
    return false;
  }
  if (!findDataZones(m_state->m_zonesEntry))
    return false;
  ascii().addPos(m_state->m_zonesEntry.end());
  ascii().addNote("FileHeader-End");

  if (m_state->m_zonesList.size()<20) {
    // even an empty file seems to have almost ~80 zones, so...
    MWAW_DEBUG_MSG(("RagTime5Parser::createZones: the zone list seems too short\n"));
    return false;
  }
  // we need first to join dissociate zone and to read the type data
  for (size_t i=0; i<m_state->m_zonesList.size(); ++i) {
    if (!m_state->m_zonesList[i])
      continue;
    RagTime5ParserInternal::Zone &zone=*m_state->m_zonesList[i];
    if (!update(zone))
      continue;

    std::string what("");
    if (zone.m_idsFlag[1]!=0 || (zone.m_ids[1]!=23 && zone.m_ids[1]!=24) || zone.m_ids[2]!=21 ||
        !readString(zone, what) || what.empty())
      continue;

    if (m_state->m_idToTypeMap.find(zone.m_ids[0])!=m_state->m_idToTypeMap.end()) {
      MWAW_DEBUG_MSG(("RagTime5Parser::createZones: a type with id=%d already exists\n", zone.m_ids[0]));
    }
    else
      m_state->m_idToTypeMap[zone.m_ids[0]]=what;
  }
  // first create the zone list
  for (size_t i=0; i<m_state->m_zonesList.size(); ++i) {
    if (!m_state->m_zonesList[i])
      continue;
    RagTime5ParserInternal::Zone &zone=*m_state->m_zonesList[i];
    if (zone.m_entry.isParsed() || !zone.m_entry.valid())
      continue;
    for (int j=1; j<3; ++j) {
      if (!zone.m_ids[j]) continue;
      if (m_state->m_idToTypeMap.find(zone.m_ids[j])==m_state->m_idToTypeMap.end()) {
        MWAW_DEBUG_MSG(("RagTime5Parser::createZones: can not find the type for %d:%d\n", zone.m_ids[0],j));
      }
      else
        zone.m_kinds[j-1]=m_state->m_idToTypeMap.find(zone.m_ids[j])->second;
    }
    // first unpack the packed zone
    int packPos=zone.m_kinds[1].empty() ? 0 : 1;
    size_t packLength=zone.m_kinds[packPos].size();
    if (packLength>5 && zone.m_kinds[packPos].compare(packLength-5,5,":Pack")==0) {
      if (!unpackZone(zone)) {
        MWAW_DEBUG_MSG(("RagTime5Parser::createZones: can not unpack the zone %d\n", zone.m_ids[0]));
        libmwaw::DebugFile &ascFile=zone.ascii();
        libmwaw::DebugStream f;
        f << "Entries(BADPACK)[" << zone << "]:###" << zone.m_kinds[packPos];
        ascFile.addPos(zone.m_entry.begin());
        ascFile.addNote(f.str().c_str());
        continue;
      }
      zone.m_kinds[packPos].resize(packLength-5);
    }

    // the "RagTime" string
    std::string what;
    if (zone.m_kinds[1]=="BESoftware:7BitASCII:Type" && readString(zone, what))
      continue;

    std::string kind=zone.getKindLastPart();
    PictureType type=P_Unknown;
    if (kind=="TIFF") type=P_Tiff;
    else if (kind=="PICT") type=P_Pict;
    else if (kind=="JPEG") type=P_Jpeg;
    else if (kind=="WMF") type=P_WMF;
    else if (kind=="EPSF") type=P_Epsf;
    else if (kind=="ScreenRep") type=P_ScreenRep;

    if (type!=P_Unknown) {
      if (readPicture(zone, zone.m_entry, type))
        continue;
      MWAW_DEBUG_MSG(("RagTime5Parser::createZones: can not read a picture zone %d\n", zone.m_ids[0]));
      libmwaw::DebugFile &ascFile=zone.ascii();
      libmwaw::DebugStream f;
      f << "Entries(BADPICT)[" << zone << "]:###" << zone.m_kinds[0] << "," << zone.m_kinds[1];
      ascFile.addPos(zone.m_entry.begin());
      ascFile.addNote(f.str().c_str());
      continue;
    }

    if (kind=="Type") {
      if (zone.m_kinds[0]=="BESoftware:OSAScript:Type")
        zone.m_name="OSAScript";
      else if (zone.m_kinds[0]=="BESoftware:DocuVersion:Type")
        zone.m_name="DocuVersion";
    }
    else if (kind=="ScriptComment") // an unicode string
      zone.m_name="ScriptComment";
    else if (kind=="ItemData")
      zone.m_name="ItemData";
    if (readListZone(zone)) continue;
    libmwaw::DebugFile &ascFile=zone.ascii();
    libmwaw::DebugStream f;
    f << "Entries(" << zone.m_name << "):" << zone;
    ascFile.addPos(zone.m_entry.begin());
    ascFile.addNote(f.str().c_str());
    ascFile.addPos(zone.m_entry.end());
    ascFile.addNote("_");
  }

  return false;
}

////////////////////////////////////////////////////////////
// parse the different zones
////////////////////////////////////////////////////////////
bool RagTime5Parser::readString(RagTime5ParserInternal::Zone &zone, std::string &text)
{
  if (!zone.m_entry.valid()) return false;
  MWAWInputStreamPtr input=zone.getInput();
  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  f << "Entries(StringZone)[" << zone << "]:";
  input->seek(zone.m_entry.begin(), librevenge::RVNG_SEEK_SET);
  text="";
  for (long i=0; i<zone.m_entry.length(); ++i) {
    char c=(char) input->readULong(1);
    if (c==0 && i+1==zone.m_entry.length()) break;
    if (c<0x1f)
      return false;
    text+=c;
  }
  f << "\"" << text << "\",";
  if (input->tell()!=zone.m_entry.end()) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readString: find extra data\n"));
    f << "###";
    ascFile.addDelimiter(input->tell(),'|');
  }
  zone.m_entry.setParsed(true);
  ascFile.addPos(zone.m_entry.begin());
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(zone.m_entry.end());
  ascFile.addNote("_");
  return true;
}

bool RagTime5Parser::readListZone(RagTime5ParserInternal::Zone &zone)
{
  MWAWInputStreamPtr input=zone.getInput();
  libmwaw::DebugStream f;
  f << "Entries(ListZone)[" << zone << "]:";
  input->seek(zone.m_entry.begin(), librevenge::RVNG_SEEK_SET);
  long N=(long) input->readULong(1);
  if (!N || N>0x83) return false;
  if (N>=0x80) // find 0x80, never 0x81
    N=(long) input->readULong(int(N-0x7F));
  f << "N=" << N << ",";
  if (N<=20 || 12+14*N>zone.m_entry.length()) return false;
  int val;
  for (int i=0; i<2; ++i) { // always 0,0
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  int sz=(int) input->readULong(1);
  if (sz) {
    f << "data1=[" << std::hex;
    for (int i=0; i<sz+1; ++i) {
      val=(int) input->readULong(1);
      if (val) f << val << ",";
      else f << "_,";
    }
    f << std::dec << "],";
  }
  long debPos=input->tell()+4*(N+1);
  long remain=zone.m_entry.end()-debPos;
  if (remain<=0) return false;
  f << "ptr=[" << std::hex;
  std::vector<long> listPtrs((size_t)(N+1));
  for (size_t i=0; i<=size_t(N); ++i) {
    long ptr=(long) input->readULong(4);
    if (ptr<0 || ptr>remain || (i && ptr<listPtrs[i-1]))
      return false;
    f << ptr << ",";
    listPtrs[size_t(i)]=ptr;
  }
  f << std::dec << "],";
  libmwaw::DebugFile &ascFile=zone.ascii();
  ascFile.addPos(zone.m_entry.begin());
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(zone.m_entry.end());
  ascFile.addNote("_");

  for (size_t i=0; i<listPtrs.size(); ++i) {
    if (listPtrs[i]==remain) break;
    f.str("");
    f << "ListZone-" << i << ":";
    ascFile.addPos(debPos+listPtrs[i]);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool RagTime5Parser::update(RagTime5ParserInternal::Zone &zone)
{
  if (zone.m_entriesList.empty())
    return true;
  if (zone.m_ids[0]==0) // the header zone
    return true;
  if (zone.m_idsFlag[0]==0) {
    // avoid normal main zone: zones, file
    if (zone.m_ids[0]==1 || zone.m_ids[0]==4 || zone.m_ids[0]==5)
      return false;
    std::stringstream s;
    s << "Main" << zone.m_ids[0];
    zone.m_name=s.str();
  }
  else if (zone.m_type==RagTime5ParserInternal::Zone::Main)
    zone.m_name="MZone";
  else {
    std::stringstream s;
    s << "ZoneA" << zone.m_ids[1] << "A";
    zone.m_name=s.str();
  }

  std::stringstream s;
  s << "Zone" << std::hex << zone.m_entriesList[0].begin() << std::dec;
  zone.setAsciiFileName(s.str());

  if (zone.m_entriesList.size()==1) {
    zone.m_entry=zone.m_entriesList[0];
    return true;
  }

  libmwaw::DebugStream f;
  f << "Entries(" << zone.m_name << "):";
  MWAWInputStreamPtr input = getInput();
  shared_ptr<MWAWStringStream> newStream;
  for (size_t z=0; z<zone.m_entriesList.size(); ++z) {
    MWAWEntry const &entry=zone.m_entriesList[z];
    if (!entry.valid() || !input->checkPosition(entry.end())) {
      MWAW_DEBUG_MSG(("RagTime5Parser::update: can not read some data\n"));
      f << "###";
      ascii().addPos(entry.begin());
      ascii().addNote(f.str().c_str());
      return false;
    }
    input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);

    unsigned long read;
    const unsigned char *dt = input->read((unsigned long)entry.length(), read);
    if (!dt || long(read) != entry.length()) {
      MWAW_DEBUG_MSG(("RagTime5Parser::update: can not read some data\n"));
      f << "###";
      ascii().addPos(entry.begin());
      ascii().addNote(f.str().c_str());
      return false;
    }
    ascii().skipZone(entry.begin(), entry.end()-1);
    if (z==0)
      newStream.reset(new MWAWStringStream(dt, (unsigned int) entry.length()));
    else
      newStream->append(dt, (unsigned int) entry.length());
  }

  MWAWInputStreamPtr newInput(new MWAWInputStream(newStream, false));
  zone.setInput(newInput);
  zone.m_entry.setBegin(0);
  zone.m_entry.setLength(newInput->size());

  return true;
}

bool RagTime5Parser::unpackZone(RagTime5ParserInternal::Zone &zone)
{
  if (!zone.m_entry.valid())
    return false;

  long pos=zone.m_entry.begin(), endPos=zone.m_entry.end();
  MWAWInputStreamPtr input=zone.getInput();
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  if (zone.m_entry.length()<=6) {
    // check for an empty zone
    if (zone.m_entry.length()==4 && input->readLong(4)==0) {
      zone.ascii().addPos(pos);
      zone.ascii().addNote("_");
      zone.m_entry.setLength(0);
      return true;
    }
    return false;
  }
  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTime5Parser::unpackZone: the input seems bad\n"));
    return false;
  }

  unsigned long sz=(unsigned long) input->readULong(4);
  int flag=int(sz>>24);
  sz &= 0xFFFFFF;
  if ((flag&0xf) || (flag&0xf0)==0 || !(sz&0xFFFFFF)) return false;

  int nBytesRead=0, szField=9;
  unsigned int read=0;
  size_t mapPos=0;
  std::vector<unsigned char> newData;
  newData.reserve(size_t(sz));
  std::vector<std::vector<unsigned char> > mapToString;
  mapToString.reserve(size_t(zone.m_entry.length()-6));
  bool ok=false;
  while (!input->isEnd()) {
    if ((int) mapPos==(1<<szField)-0x102)
      ++szField;
    if (input->tell()>=endPos) {
      MWAW_DEBUG_MSG(("RagTime5Parser::unpackZone: oops can not find last data\n"));
      ok=false;
      break;
    }
    do {
      read = (read<<8)+(unsigned int) input->readULong(1);
      nBytesRead+=8;
    }
    while (nBytesRead<szField);
    unsigned int val=(read >> (nBytesRead-szField));
    nBytesRead-=szField;
    read &= ((1<<nBytesRead)-1);

    if (val<0x100) {
      unsigned char c=(unsigned char) val;
      newData.push_back(c);
      if (mapPos>= mapToString.size())
        mapToString.resize(mapPos+1);
      mapToString[mapPos++]=std::vector<unsigned char>(1,c);
      continue;
    }
    if (val==0x100) { // begin
      if (!newData.empty()) {
        // data are reset when mapPos=3835, so it is ok
        mapPos=0;
        mapToString.resize(0);
        szField=9;
      }
      continue;
    }
    if (val==0x101) {
      ok=input->tell()==endPos && read==0;
      if (!ok) {
        MWAW_DEBUG_MSG(("RagTime5Parser::unpackZone: find 0x101 in bad position\n"));
      }
      break;
    }
    size_t readPos=size_t(val-0x102);
    if (readPos >= mapToString.size()) {
      MWAW_DEBUG_MSG(("RagTime5Parser::unpackZone: find bad position\n"));
      ok = false;
      break;
    }
    std::vector<unsigned char> final=mapToString[readPos++];
    if (readPos==mapToString.size())
      final.push_back(final[0]);
    else
      final.push_back(mapToString[readPos][0]);
    newData.insert(newData.end(), final.begin(), final.end());
    if (mapPos>= mapToString.size())
      mapToString.resize(mapPos+1);
    mapToString[mapPos++]=final;
  }

  if (ok && newData.size()!=(size_t) sz) {
    MWAW_DEBUG_MSG(("RagTime5Parser::unpackZone: oops the data file is bad\n"));
    ok=false;
  }
  if (!ok) {
    MWAW_DEBUG_MSG(("RagTime5Parser::unpackZone: stop with mapPos=%ld and totalSize=%ld/%ld\n", long(mapPos), long(newData.size()), long(sz)));
    return false;
  }

  if (input.get()==getInput().get())
    ascii().skipZone(pos, endPos-1);

  shared_ptr<MWAWStringStream> newStream(new MWAWStringStream(&newData[0], (unsigned int) newData.size()));
  MWAWInputStreamPtr newInput(new MWAWInputStream(newStream, false));
  zone.setInput(newInput);
  zone.m_entry.setBegin(0);
  zone.m_entry.setLength(newInput->size());
  zone.m_extra += "packed,";
  return ok;
}

////////////////////////////////////////////////////////////
// find the different zones
////////////////////////////////////////////////////////////
bool RagTime5Parser::readPicture(RagTime5ParserInternal::Zone &zone, MWAWEntry &entry, PictureType type)
{
  if (entry.length()<=40)
    return false;
  MWAWInputStreamPtr input = zone.getInput();
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  long val;
  std::string extension("");
  switch (type) {
  case P_Epsf:
    val=(long) input->readULong(4);
    if (val!=0xc5d0d3c6 && val != 0x25215053) return false;
    extension="eps";
#if 0
    // when header==0xc5d0d3c6, we may want to decompose the data
    input->setReadInverted(true);
    MWAWEntry fEntry[3];
    for (int i=0; i<3; ++i) {
      fEntry[i].setBegin((long) input->readULong(4));
      fEntry[i].setLength((long) input->readULong(4));
      if (!fEntry[i].length()) continue;
      f << "decal" << i << "=" << std::dec << fEntry[i].begin() << "<->" << fEntry[i].end() << std::hex << ",";
      if (fEntry[i].begin()<0x1c ||fEntry[i].end()>zone.m_entry.length()) {
        MWAW_DEBUG_MSG(("RagTime5Parser::readPicture: the address %d seems too big\n", i));
        fEntry[i]=MWAWEntry();
        f << "###";
        continue;
      }
    }
    for (int i=0; i<2; ++i) { // always -1,0
      if (input->tell()>=pos+fDataPos)
        break;
      val=(int) input->readLong(2);
      if (val!=i-1)
        f << "f" << i << "=" << val << ",";
    }
    // now first fEntry=eps file, second WMF?, third=tiff file
#endif
    break;
  case P_Jpeg:
    val=(long) input->readULong(2);
    if (val!=0xffd8) return false;
    extension="jpg";
    break;
  case P_Pict:
    input->seek(10, librevenge::RVNG_SEEK_CUR);
    val=(long) input->readULong(2);
    if (val!=0x1101 && val !=0x11) return false;
    extension="pct";
    break;
  case P_ScreenRep:
    val=(long) input->readULong(1);
    if (val!=0x49 && val!=0x4d) return false;
    extension="sRep";
    break;
  case P_Tiff:
    val=(long) input->readULong(2);
    if (val!=0x4949 && val != 0x4d4d) return false;
    extension="tiff";
    break;
  case P_WMF: // checkme: can we find a signature
    break;
  case P_Unknown:
  default:
    return false;
  }
#ifdef DEBUG_WITH_FILES
  ascii().addPos(zone.m_defPosition);
  ascii().addNote(extension.c_str());
  if (type==P_ScreenRep) {
    libmwaw::DebugFile &ascFile=zone.ascii();
    libmwaw::DebugStream f;
    f << "Entries(ScreenRep)[" << zone << "]:";
    ascFile.addPos(zone.m_entry.begin());
    ascFile.addNote(f.str().c_str());
    return true;
  }
  if (zone.isMainInput())
    ascii().skipZone(entry.begin(), entry.end()-1);
  librevenge::RVNGBinaryData file;
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  input->readDataBlock(entry.length(), file);
  static int volatile pictName = 0;
  libmwaw::DebugStream f;
  f << "Pict-" << ++pictName << extension;
  libmwaw::Debug::dumpFile(file, f.str().c_str());
#endif
  return true;
}

////////////////////////////////////////////////////////////
// find the different zones
////////////////////////////////////////////////////////////
bool RagTime5Parser::findDataZones(MWAWEntry const &entry)
{
  libmwaw::DebugStream f;
  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  if (!input->checkPosition(entry.end())) {
    MWAW_DEBUG_MSG(("RagTime5Parser::findDataZones: main entry seems too bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }

  int n=0;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  while (!input->isEnd()) {
    pos=input->tell();
    if (pos>=entry.end()) break;
    int type=(int) input->readULong(1);
    if (type==0x18) {
      while (input->tell()<entry.end()) {
        if (input->readULong(1)==0xFF)
          continue;
        input->seek(-1, librevenge::RVNG_SEEK_CUR);
        break;
      }
      ascii().addPos(pos);
      ascii().addNote("_");
      continue;
    }
    f.str("");
    shared_ptr<RagTime5ParserInternal::Zone> zone(new RagTime5ParserInternal::Zone(input, ascii()));
    zone->m_defPosition=pos;
    switch (type) {
    case 1:
      zone->m_type=RagTime5ParserInternal::Zone::Data;
      break;
    case 2:
      zone->m_type=RagTime5ParserInternal::Zone::Main;
      break;
    case 3:
      zone->m_type=RagTime5ParserInternal::Zone::Empty;
      break;
    default:
      zone->m_subType=type;
      break;
    }
    // type=3: 0001, 59-78 + sometimes g4=[_,1]
    if (pos+4>entry.end() || type < 1 || type > 3) {
      zone->m_extra=f.str();
      if (n++==0)
        f << "Entries(Zones)[1]:";
      else
        f << "Zones-" << n << ":";
      f << zone << "###";
      MWAW_DEBUG_MSG(("RagTime5Parser::findDataZones: find unknown type\n"));
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      break;
    }
    int val;
    for (int i=0; i<4-type; ++i) {
      zone->m_idsFlag[i]=(int) input->readULong(2); // alway 0/1?
      zone->m_ids[i]=(int) input->readULong(2);
    }
    bool ok=true;
    do {
      int type2=(int)  input->readULong(1);
      switch (type2) {
      case 4:
        ok = input->tell()+4+1<=entry.end();
        if (!ok) break;
        f << "g4=[";
        for (int i=0; i<2; ++i) { // always 0,1 ?
          val=(int) input->readULong(2);
          if (val) f << val << ",";
          else f << "_,";
        }
        f << "],";
        break;
      case 5:
      case 6: {
        ok = input->tell()+8+(type2==6 ? 1 : 0)<=entry.end();
        if (!ok) break;
        MWAWEntry zEntry;
        zEntry.setBegin((long) input->readULong(4));
        zEntry.setLength((long) input->readULong(4));
        zone->m_entriesList.push_back(zEntry);
        break;
      }
      case 9:
        ok=input->tell()<=entry.end();
        break;
      case 0xd: // always 0?
        ok = input->tell()+4<=entry.end();
        if (!ok) break;
        f << "gD=[";
        for (int i=0; i<2; ++i) {
          val=(int) input->readULong(2);
          if (val) f << val << ",";
          else f << "_,";
        }
        f << "],";
        break;
      case 0x18:
        while (input->tell()<entry.end()) {
          if (input->readULong(1)==0xFF)
            continue;
          input->seek(-1, librevenge::RVNG_SEEK_CUR);
          break;
        }
        ok=input->tell()+1<entry.end();
        break;
      default:
        ok=false;
        MWAW_DEBUG_MSG(("RagTime5Parser::findDataZones: find unknown type2=%d\n", type2));
        f << "type2=" << type2 << ",";
        break;
      }
      if (!ok || (type2&1))
        break;
    }
    while (1);
    zone->m_extra=f.str();
    m_state->m_zonesList.push_back(zone);
    f.str("");
    if (n++==0)
      f << "Entries(Zones)[1]:";
    else
      f << "Zones-" << n << ":";
    f << *zone;
    if (!ok) {
      MWAW_DEBUG_MSG(("RagTime5Parser::findDataZones: find unknown data\n"));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      break;
    }
    ascii().addPos(pos);
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
// picture zone
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// color map
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// print info
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool RagTime5Parser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = RagTime5ParserInternal::State();

  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork())
    return false;

  libmwaw::DebugStream f;
  f << "FileHeader:";
  if (!input->checkPosition(32)) {
    MWAW_DEBUG_MSG(("RagTime5Parser::checkHeader: file is too short\n"));
    return false;
  }
  input->seek(0,librevenge::RVNG_SEEK_SET);
  if (input->readULong(4)!=0x43232b44 || input->readULong(4)!=0xa4434da5
      || input->readULong(4)!=0x486472d7)
    return false;
  f << "vers=5.5,";
  int val;
  for (int i=0; i<3; ++i) {
    val=(int) input->readLong(2);
    if (val!=i) f << "f" << i << "=" << val << ",";
  }
  val=(int) input->readLong(2); // always 0?
  if (val) f << "f3=" << val << ",";
  m_state->m_zonesEntry.setBegin((long) input->readULong(4));
  m_state->m_zonesEntry.setLength((long) input->readULong(4));
  if (m_state->m_zonesEntry.length()<137 ||
      !input->checkPosition(m_state->m_zonesEntry.begin()+137))
    return false;
  if (strict && !input->checkPosition(m_state->m_zonesEntry.end()))
    return false;
  val=(int) input->readLong(1);
  if (val==1)
    f << "compacted,";
  else if (val)
    f << "g0=" << val << ",";

  for (int i=0; i<3; ++i) {
    val=(int) input->readLong(1);
    if (val) f << "g" << i+1 << "=" << val << ",";
  }
  setVersion(5);
  // ok, we can finish initialization
  if (header)
    header->reset(MWAWDocument::MWAW_T_RAGTIME, version());
  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  ascii().addPos(input->tell());
  ascii().addNote("_");

  return true;
}

////////////////////////////////////////////////////////////
// send data to the listener
////////////////////////////////////////////////////////////

void RagTime5Parser::flushExtra()
{
  MWAWListenerPtr listener=getMainListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("RagTime5Parser::flushExtra: can not find the listener\n"));
    return;
  }
  // todo
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
