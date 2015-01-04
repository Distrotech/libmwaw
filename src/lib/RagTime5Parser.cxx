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

#include "RagTime5StructManager.hxx"

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
    m_type(Unknown), m_subType(0), m_defPosition(0), m_entry(), m_name(""), m_hiLoEndian(true),
    m_entriesList(), m_extra(""), m_isParsed(false), m_input(input), m_defaultInput(true),
    m_asciiName(""), m_asciiFile(&asc), m_localAsciiFile()
  {
    for (int i=0; i<3; ++i) m_ids[i]=m_idsFlag[i]=0;
    for (int i=0; i<2; ++i) m_kinds[i]="";
    for (int i=0; i<2; ++i) m_variableD[i]=0;
  }
  //! destructor
  ~Zone() {}
  //! returns the zone name
  std::string getZoneName() const;
  //! returns true if the zone is a header zone(header, list zone, ...)
  bool isHeaderZone() const
  {
    return (m_type==Data && m_ids[0]==0) ||
           (m_type==Main && (m_ids[0]==1 || m_ids[0]==4 || m_ids[0]==5));
  }
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
  //! the zone name ( mainly used for debugging)
  std::string m_name;
  //! true if the endian is hilo
  bool m_hiLoEndian;
  //! the zone id
  int m_ids[3];
  //! the zone flag
  int m_idsFlag[3];
  //! the list of original entries
  std::vector<MWAWEntry> m_entriesList;
  //! the content of the zone D if it exists
  int m_variableD[2];
  //! extra data
  std::string m_extra;
  //! a flag to know if the zone is parsed
  bool m_isParsed;
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

std::string Zone::getZoneName() const
{
  switch (m_ids[0]) {
  case 0:
    if (m_type==Zone::Data)
      return "FileHeader";
    break;
  case 1: // with g4=1, gd=[1, lastDataZones]
    if (m_type==Zone::Main)
      return "ZoneInfo";
    break;
  case 3: // with no value or gd=[1,_] (if multiple)
    if (m_type==Zone::Main)
      return "Main3A";
    break;
  case 4:
    if (m_type==Zone::Main)
      return "ZoneLimits,";
    break;
  case 5:
    if (m_type==Zone::Main)
      return "FileLimits";
    break;
  case 6: // gd=[_,_]
    if (m_type==Zone::Main)
      return "Main6A";
    break;
  case 8: // type=UseCount, gd=[0,num>1], can be multiple
    if (m_type==Zone::Main)
      return "UnknCounter8";
    break;
  case 10: // type=SingleRef, gd=[1,id], Data id is a list types
    if (m_type==Zone::Main)
      return "Types";
    break;
  case 11: // type=SingleRef, gd=[1,id], Did is a item cluster
    if (m_type==Zone::Main)
      return "Main11A";
    break;
  default:
    break;
  }
  std::stringstream s;
  switch (m_type) {
  case Zone::Main:
    s << "Main" << m_ids[0] << "A";
    break;
  case Zone::Data:
    s << "Data" << m_ids[0] << "A";
    break;
  case Zone::Empty:
    s << "unused" << m_ids[0];
    break;
  case Zone::Unknown:
  default:
    s << "##zone" << m_subType << ":" << m_ids[0] << "";
    break;
  }
  return s.str();
}

std::ostream &operator<<(std::ostream &o, Zone const &z)
{
  o << z.getZoneName();
  if (z.m_idsFlag[0])
    o << "[" << z.m_idsFlag[0] << "],";
  else
    o << ",";
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
  if (z.m_variableD[0] || z.m_variableD[1])
    o << "varD=[" << z.m_variableD[0] << "," << z.m_variableD[1] << "],";
  if (z.m_entry.valid())
    o << z.m_entry.begin() << "<->" << z.m_entry.end() << ",";
  else if (!z.m_entriesList.empty()) {
    o << "ptr=" << std::hex;
    for (size_t i=0; i< z.m_entriesList.size(); ++i) {
      o << z.m_entriesList[i].begin() << "<->" << z.m_entriesList[i].end();
      if (i+1<z.m_entriesList.size())
        o << "+";
    }
    o << std::dec << ",";
  }
  if (!z.m_hiLoEndian) o << "loHi[endian],";
  o << z.m_extra << ",";
  return o;
}

////////////////////////////////////////
//! Internal: the state of a RagTime5Parser
struct State {
  //! constructor
  State() : m_zonesEntry(), m_zonesList(), m_idToTypeMap(), m_dataIdZoneMap(), m_mainIdZoneMap(), m_idColorsMap(), m_patternList(),
    m_pageZonesIdMap(), m_idPictureMap(), m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0)
  {
  }
  //! init the pattern to default
  void initDefaultPatterns(int vers);
  //! returns the picture type corresponding to a name
  static RagTime5Parser::PictureType getPictureType(std::string const &type)
  {
    if (type=="TIFF") return RagTime5Parser::P_Tiff;
    if (type=="PICT") return RagTime5Parser::P_Pict;
    if (type=="PNG") return RagTime5Parser::P_PNG;
    if (type=="JPEG") return RagTime5Parser::P_Jpeg;
    if (type=="WMF") return RagTime5Parser::P_WMF;
    if (type=="EPSF") return RagTime5Parser::P_Epsf;
    if (type=="ScreenRep" || type=="Thumbnail") return RagTime5Parser::P_ScreenRep;
    return RagTime5Parser::P_Unknown;
  }

  //! the main zone entry
  MWAWEntry m_zonesEntry;
  //! the zone list
  std::vector<shared_ptr<Zone> > m_zonesList;
  //! a map id to type string
  std::map<int, std::string> m_idToTypeMap;
  //! a map: data id->entry (datafork)
  std::map<int, shared_ptr<Zone> > m_dataIdZoneMap;
  //! a map: main id->entry (datafork)
  std::multimap<int, shared_ptr<Zone> > m_mainIdZoneMap;
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
  MWAWTextParser(input, rsrcParser, header), m_state(), m_structManager()
{
  init();
}

RagTime5Parser::~RagTime5Parser()
{
}

void RagTime5Parser::init()
{
  m_structManager.reset(new RagTime5StructManager);
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
  libmwaw::DebugStream f;
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
    else {
      m_state->m_idToTypeMap[zone.m_ids[0]]=what;
      f.str("");
      f << what << ",";
      ascii().addPos(zone.m_defPosition);
      ascii().addNote(f.str().c_str());
    }
  }
  // first find the type of all zone and unpack the zone if needed...
  for (size_t i=0; i<m_state->m_zonesList.size(); ++i) {
    if (!m_state->m_zonesList[i])
      continue;
    RagTime5ParserInternal::Zone &zone=*m_state->m_zonesList[i];
    if (zone.m_isParsed)
      continue;
    if (zone.m_type==RagTime5ParserInternal::Zone::Main)
      m_state->m_mainIdZoneMap.insert
      (std::multimap<int, shared_ptr<RagTime5ParserInternal::Zone> >::value_type
       (zone.m_ids[0],m_state->m_zonesList[i]));
    else if (zone.m_type==RagTime5ParserInternal::Zone::Data) {
      if (m_state->m_dataIdZoneMap.find(zone.m_ids[0])!=m_state->m_dataIdZoneMap.end()) {
        MWAW_DEBUG_MSG(("RagTime5Parser::createZones: data zone with id=%d already exists\n", zone.m_ids[0]));
      }
      else
        m_state->m_dataIdZoneMap[zone.m_ids[0]]=m_state->m_zonesList[i];
    }
    for (int j=1; j<3; ++j) {
      if (!zone.m_ids[j]) continue;
      if (m_state->m_idToTypeMap.find(zone.m_ids[j])==m_state->m_idToTypeMap.end()) {
        // the main zone seems to point to a cluster id...
        if (zone.m_ids[0]<=6) continue;
        MWAW_DEBUG_MSG(("RagTime5Parser::createZones: can not find the type for %d:%d\n", zone.m_ids[0],j));
        ascii().addPos(zone.m_defPosition);
        ascii().addNote("###type,");
      }
      else {
        zone.m_kinds[j-1]=m_state->m_idToTypeMap.find(zone.m_ids[j])->second;
        f.str("");
        f << zone.m_kinds[j-1] << ",";
        ascii().addPos(zone.m_defPosition);
        ascii().addNote(f.str().c_str());
      }
    }
    if (!zone.m_entry.valid()) continue;
    // first unpack the packed zone
    int usedId=zone.m_kinds[1].empty() ? 0 : 1;
    std::string actType=zone.getKindLastPart(usedId==0);
    if (actType=="Pack") {
      if (!unpackZone(zone)) {
        MWAW_DEBUG_MSG(("RagTime5Parser::createZones: can not unpack the zone %d\n", zone.m_ids[0]));
        libmwaw::DebugFile &ascFile=zone.ascii();
        f.str("");
        f << "Entries(BADPACK)[" << zone << "]:###" << zone.m_kinds[usedId];
        ascFile.addPos(zone.m_entry.begin());
        ascFile.addNote(f.str().c_str());
        continue;
      }
      size_t length=zone.m_kinds[usedId].size();
      if (length>5)
        zone.m_kinds[usedId].resize(length-5);
      else
        zone.m_kinds[usedId]="";
    }

    // check hilo
    usedId=zone.m_kinds[1].empty() ? 0 : 1;
    actType=zone.getKindLastPart(usedId==0);
    if (actType=="HiLo" || actType=="LoHi") {
      zone.m_hiLoEndian=actType=="HiLo";
      size_t length=zone.m_kinds[usedId].size();
      if (length>5)
        zone.m_kinds[usedId].resize(length-5);
      else
        zone.m_kinds[usedId]="";
    }
    std::string kind=zone.getKindLastPart();
    if (kind=="Type") {
      size_t length=zone.m_kinds[0].size();
      if (length>5)
        zone.m_kinds[0].resize(length-5);
      else
        zone.m_kinds[0]="";
      zone.m_extra += "type,";
    }
  }

  std::multimap<int, shared_ptr<RagTime5ParserInternal::Zone> >::iterator it;
  // list of blocks zone
  it=m_state->m_mainIdZoneMap.lower_bound(10);
  int numZone10=0;
  while (it!=m_state->m_mainIdZoneMap.end() && it->first==10) {
    shared_ptr<RagTime5ParserInternal::Zone> zone=it++->second;
    if (!zone || zone->m_variableD[0]!=1 ||
        m_state->m_dataIdZoneMap.find(zone->m_variableD[1])==m_state->m_dataIdZoneMap.end()) {
      MWAW_DEBUG_MSG(("RagTime5Parser::createZones: the main zone 10 seems bads\n"));
      continue;
    }
    shared_ptr<RagTime5ParserInternal::Zone> dZone=
      m_state->m_dataIdZoneMap.find(zone->m_variableD[1])->second;
    if (!dZone || !dZone->m_entry.valid()) continue;
    dZone->m_name=zone->getZoneName();
    if (dZone->getKindLastPart()=="ItemData") {
      MWAWInputStreamPtr input=dZone->getInput();
      libmwaw::DebugFile &ascFile=dZone->ascii();

      input->setReadInverted(!dZone->m_hiLoEndian);
      input->seek(dZone->m_entry.begin(), librevenge::RVNG_SEEK_SET);
      bool ok=m_structManager->readTypeDefinitions(input, dZone->m_entry.end(), ascFile);
      input->setReadInverted(false);
      if (ok) {
        f.str("");
        f << "zone=" << *dZone;
        ascFile.addPos(dZone->m_entry.begin());
        ascFile.addNote(f.str().c_str());
        dZone->m_isParsed=true;
        ++numZone10;
        continue;
      }
    }
    MWAW_DEBUG_MSG(("RagTime5Parser::createZones: unexpected list of block type\n"));
  }
  if (numZone10!=1) {
    MWAW_DEBUG_MSG(("RagTime5Parser::createZones: parses %d list of type zone, we may have a problem\n", numZone10));
  }
  //! zone 11
  it=m_state->m_mainIdZoneMap.lower_bound(11);
  int numZone11=0;
  while (it!=m_state->m_mainIdZoneMap.end() && it->first==11) {
    shared_ptr<RagTime5ParserInternal::Zone> zone=it++->second;
    if (!zone || zone->m_variableD[0]!=1 ||
        m_state->m_dataIdZoneMap.find(zone->m_variableD[1])==m_state->m_dataIdZoneMap.end()) {
      MWAW_DEBUG_MSG(("RagTime5Parser::createZones: the main zone 11 seems bads\n"));
      continue;
    }
    shared_ptr<RagTime5ParserInternal::Zone> dZone=
      m_state->m_dataIdZoneMap.find(zone->m_variableD[1])->second;
    if (!dZone) continue;
    dZone->m_extra+="main11,";
    if (dZone->getKindLastPart(dZone->m_kinds[1].empty())=="Cluster" && readItemCluster(*dZone))
      ++numZone11;
    else {
      MWAW_DEBUG_MSG(("RagTime5Parser::createZones: unexpected main zone 11 type\n"));
    }
  }
  if (numZone11!=1) {
    MWAW_DEBUG_MSG(("RagTime5Parser::createZones: parses %d main11 zone, we may have a problem\n", numZone11));
  }
  for (size_t i=0; i<m_state->m_zonesList.size(); ++i) {
    if (!m_state->m_zonesList[i])
      continue;
    RagTime5ParserInternal::Zone &zone=*m_state->m_zonesList[i];
    if (zone.m_isParsed || !zone.m_entry.valid())
      continue;
    readZoneData(zone);
  }
  return false;
}

bool RagTime5Parser::readZoneData(RagTime5ParserInternal::Zone &zone)
{
  if (!zone.m_entry.valid()) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readZoneData: can not find the entry\n"));
    return false;
  }
  libmwaw::DebugStream f;
  int usedId=zone.m_kinds[1].empty() ? 0 : 1;
  std::string actType=zone.getKindLastPart(usedId==0);

  std::string kind=zone.getKindLastPart();
  // the "RagTime" string
  if (kind=="CodeName") {
    std::string what;
    if (zone.m_kinds[1]=="BESoftware:7BitASCII:Type" && readString(zone, what))
      return true;
    MWAW_DEBUG_MSG(("RagTime5Parser::readZoneData: can not read codename for zone %d\n", zone.m_ids[0]));
    f << "Entries(CodeName)[" << zone << "]:###";
    libmwaw::DebugFile &ascFile=zone.ascii();
    ascFile.addPos(zone.m_entry.begin());
    ascFile.addNote(f.str().c_str());
    return true;
  }
  //
  // first test for picture data
  //

  // list of picture id
  if (kind=="ScreenRepList") {
    std::vector<int> listIds;
    return readPictureList(zone, listIds);
  }
  if (kind=="ScreenRepMatchData" || kind=="ScreenRepMatchDataColor")
    return readPictureMatch(zone, kind=="ScreenRepMatchDataColor");
  // other pict
  PictureType pictType=m_state->getPictureType(kind);
  if (pictType==P_ScreenRep && !zone.m_kinds[1].empty()) {
    pictType=m_state->getPictureType(zone.getKindLastPart(false));
    if (pictType != P_Unknown && readPicture(zone, zone.m_entry, pictType))
      return false;
    pictType=P_ScreenRep;
  }
  if (pictType!=P_Unknown) {
    if (readPicture(zone, zone.m_entry, pictType))
      return true;
    MWAW_DEBUG_MSG(("RagTime5Parser::readZoneData: can not read a picture zone %d\n", zone.m_ids[0]));
    f << "Entries(BADPICT)[" << zone << "]:###";
    libmwaw::DebugFile &ascFile=zone.ascii();
    ascFile.addPos(zone.m_entry.begin());
    ascFile.addNote(f.str().c_str());
    return true;
  }

  if (kind=="DocuVersion")
    return readDocumentVersion(zone);
  std::string name("");
  if (kind=="OSAScript" || kind=="TCubics")
    name=kind;
  else if (kind=="ItemData" || kind=="ScriptComment" || kind=="Unicode") {
    actType=zone.getKindLastPart(zone.m_kinds[1].empty());
    if (actType=="Unicode" || kind=="ScriptComment" || kind=="Unicode") {
      if (readUnicodeString(zone))
        return true;
      MWAW_DEBUG_MSG(("RagTime5Parser::readZoneData: can not read a unicode zone %d\n", zone.m_ids[0]));
      f << "Entries(StringUnicode)[" << zone << "]:###";
      libmwaw::DebugFile &ascFile=zone.ascii();
      ascFile.addPos(zone.m_entry.begin());
      ascFile.addNote(f.str().c_str());
      return true;
    }
    if (actType=="Cluster") {
      if (readItemCluster(zone)) return true;
      MWAW_DEBUG_MSG(("RagTime5Parser::readZoneData: can not read a cluster of item %d\n", zone.m_ids[0]));
      f << "Entries(ItemClster)[" << zone << "]:###";
      libmwaw::DebugFile &ascFile=zone.ascii();
      ascFile.addPos(zone.m_entry.begin());
      ascFile.addNote(f.str().c_str());
      return true;
    }
    if (zone.m_entry.length()>=240 && (zone.m_entry.length()%6)==0 && readUnknBlock6(zone))
      return true;
    if (zone.m_entry.length()>=40 && readStructZone(zone))
      return true;
    // checkme: some basic ItemData seems very often be unicode strings for instance data19
    name="ItemDta";
  }
  else {
    MWAW_DEBUG_MSG(("RagTime5Parser::readZoneData: find a unknown type for zone=%d\n", zone.m_ids[0]));
    name="UnknownZone";
  }
  libmwaw::DebugFile &ascFile=zone.ascii();
  f << "Entries(" << name << "):" << zone;
  ascFile.addPos(zone.m_entry.begin());
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(zone.m_entry.end());
  ascFile.addNote("_");
  return true;
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
  zone.m_isParsed=true;
  ascFile.addPos(zone.m_entry.begin());
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(zone.m_entry.end());
  ascFile.addNote("_");
  return true;
}

bool RagTime5Parser::readUnicodeString(RagTime5ParserInternal::Zone &zone)
{
  long length=zone.m_entry.length();
  if (length==0) return true;
  if ((length%2)!=0) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readUnicodeString: find unexpected data length\n"));
    return false;
  }
  MWAWInputStreamPtr input=zone.getInput();
  // unicode string seems to ignore hilo/lohi flag, so...
  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  f << "Entries(StringUnicode)[" << zone << "]:";
  /* checkme: the HiLo flag does not seem well set, we must probably used the document version, here
     input->setReadInverted(!zone.m_hiLoEndian);
   */
  input->seek(zone.m_entry.begin(), librevenge::RVNG_SEEK_SET);
  length/=2;
  f << "\"";
  for (long i=0; i<length; ++i) {
    int c=(int) input->readULong(2);
    if (c<0x100)
      f << char(c);
    else
      f << "[" << std::hex << c << std::dec << "]";
  }
  f << "\",";
  zone.m_isParsed=true;
  ascFile.addPos(zone.m_entry.begin());
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(zone.m_entry.end());
  ascFile.addNote("_");
  input->setReadInverted(false);
  return true;
}

bool RagTime5Parser::readItemCluster(RagTime5ParserInternal::Zone &zone)
{
  MWAWEntry &entry=zone.m_entry;
  if (entry.length()==0) return true;
  if (entry.length()<13) return false;

  MWAWInputStreamPtr input=zone.getInput();
  long endPos=entry.end();
  input->setReadInverted(!zone.m_hiLoEndian);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  std::string name= !zone.m_name.empty() ? zone.m_name : "ItemCluster";
  f << "Entries(" << name << ")[itemClster," << zone << "]:";
  int val;
  for (int i=0; i<4; ++i) { // f0=f1=0, f2=1, f3=small number
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  zone.m_isParsed=true;
  libmwaw::DebugFile &ascFile=zone.ascii();
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(endPos);
  ascFile.addNote("_");

  int n=0;
  while (!input->isEnd()) {
    long pos=input->tell();
    if (pos>=endPos) break;
    // N
    long lVal;
    if (!RagTime5StructManager::readCompressedLong(input, endPos, lVal)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f.str("");
    f << name << "-" << n << ":";
    f << "f0=" << lVal << ",";
    // always in big endian
    long sz;
    if (!RagTime5StructManager::readCompressedLong(input,endPos,sz) || sz <= 7 || input->tell()+sz>endPos) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f << "sz=" << sz << ",";
    long endDataPos=input->tell()+sz;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    pos=input->tell();
    f.str("");
    f << name << "-" << n << "-A:";
    if (!zone.m_hiLoEndian) f << "lohi,";
    if (!RagTime5StructManager::readCompressedLong(input, endDataPos, lVal) || lVal<6 || input->tell()+lVal>endDataPos) {
      MWAW_DEBUG_MSG(("RagTime5Parser::readItemCluster: can not read item A\n"));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      ++n;
      input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
      continue;
    }
    long endSubDataPos=input->tell()+lVal;
    int fl=(int) input->readULong(2); // [01][13][0139b]
    f << "fl=" << std::hex << fl << std::dec << ",";
    long type=(long) input->readLong(4); // a small number, or 80000000
    std::string what("");
    switch (type) {
    case -2147483648: // 80000000
      what="filename";
      break;
    case -5: {
      what="header";
      if (lVal<12) {
        f << "###data,";
        MWAW_DEBUG_MSG(("RagTime5Parser::readItemCluster: can not read the data id\n"));
        break;
      }
      for (int i=0; i<2; ++i) { // always 0?
        val=(int) input->readLong(2);
        if (val) f << "f" << i+1 << "=" << val << ",";
      }
      f << "id=" << input->readULong(2) << ",";
      break;
    }
    default: {
      f << "#";
      std::stringstream s;
      s << "type" << type;
      what=s.str();
      break;
    }
    }
    f << what << ",";
    if (input->tell()!=endSubDataPos)
      ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(endSubDataPos, librevenge::RVNG_SEEK_SET);

    int m=0;

    while (!input->isEnd()) {
      pos=input->tell();
      if (pos+4>endDataPos)
        break;
      RagTime5StructManager::Field field;
      if (!m_structManager->readField(field, input, endDataPos)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        break;
      }
      f.str("");
      f << name << "-" << n << "-B" << m++ << "[" << what << "]:";
      if (!zone.m_hiLoEndian) f << "lohi,";
      f << field;
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }

    pos=input->tell();
    if (pos!=endDataPos) {
      MWAW_DEBUG_MSG(("RagTime5Parser::readItemCluster: find some extra data\n"));
      f.str("");
      f << name << "-" << n << ":" << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    ++n;
    input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
  }
  long pos=input->tell();
  if (pos!=endPos) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readItemCluster: find some extra data\n"));
    f.str("");
    f << name << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->setReadInverted(false);
  return true;
}

bool RagTime5Parser::readUnknBlock6(RagTime5ParserInternal::Zone &zone)
{
  if (zone.m_entry.length()<240 || (zone.m_entry.length()%6)!=0) return false;
  MWAWInputStreamPtr input=zone.getInput();
  int N=int(zone.m_entry.length()/6);
  // encoding seems always hiLo
  input->seek(zone.m_entry.begin(), librevenge::RVNG_SEEK_SET);

  int bad=0;
  std::set<int> seens;
  for (int i=0; i<N; ++i) {
    // 0, 1, 1001, 1801, 4001, 5801
    int fl=(int) input->readULong(2);
    if ((fl&0xff)>4) return false;
    int id=(int) input->readULong(2);
    input->seek(2, librevenge::RVNG_SEEK_CUR);
    if (fl) {
      if ((fl&0x7fe))
        ++bad;
      continue;
    }
    if (seens.find(id)!=seens.end())
      return false;
    else
      seens.insert(id);
  }
  if (bad>10)
    return false;
  if (bad) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readUnknBlock6: find %d bad\n", bad));
  }
  // in general N seems a multiple of but sometimes...
  if ((N%128)) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readUnknBlock6: find N=%d\n", N));
  }
  zone.m_isParsed=true;

  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  input->seek(zone.m_entry.begin(), librevenge::RVNG_SEEK_SET);

  for (int i=0; i<N; ++i) {
    long pos=input->tell();
    f.str("");
    if (i==0)
      f << "Entries(Block6)[" << zone << "]:";
    else
      f << "Block6-" << i << ":";
    int fl=(int) input->readULong(2);
    int id=(int) input->readULong(2);
    int val=(int) input->readLong(2);
    if (id)
      f << "id=" << id << ",";
    if ((fl&0x7fe))
      f << "#";
    if (fl) f << "fl=" << std::hex << fl << std::dec << ",";
    if (val!=-1)
      f << "f0=" << val << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  ascFile.addPos(zone.m_entry.end());
  ascFile.addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// spreadsheet zone
////////////////////////////////////////////////////////////
bool RagTime5Parser::readStructZone(RagTime5ParserInternal::Zone &zone)
{
  if (zone.m_entry.length()<40) return false;
  MWAWInputStreamPtr input=zone.getInput();
  long pos=zone.m_entry.begin(), endPos=zone.m_entry.end();
  input->setReadInverted(!zone.m_hiLoEndian);
  libmwaw::DebugStream f;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  if (input->readLong(2)) {
    input->setReadInverted(false);
    return false;
  }
  input->seek(4, librevenge::RVNG_SEEK_CUR);
  if (input->readLong(2)!=1) return false;
  input->seek(pos+14, librevenge::RVNG_SEEK_SET);
  RagTime5StructManager::Field field;
  if (!m_structManager->readField(field, input, endPos)) {
    input->setReadInverted(false);
    return false;
  }
  libmwaw::DebugFile &ascFile=zone.ascii();
  zone.m_isParsed=true;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int n=0;
  while (!input->isEnd()) {
    pos=input->tell();
    if (pos+14>endPos) break;
    f.str("");
    if (n==0)
      f << "Entries(StructZone)[A0][" << zone << "]:";
    else
      f << "StructZone-A" << n << ":";
    if (input->readLong(2)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f << "f0=" << input->readLong(2) << ",";
    f << "f1=" << std::hex << input->readULong(2) << std::dec << ",";
    int val=(int) input->readLong(2);
    if (val) f << "id?=" << val << ",";
    ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    if (pos+14==endPos) break;
    int m=0;
    input->seek(pos+14, librevenge::RVNG_SEEK_SET);
    while (!input->isEnd()) {
      pos=input->tell();
      if (!m_structManager->readField(field, input, endPos)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        break;
      }
      f.str("");
      f << "StructZone-" << n << "-B" << ++m << ":" << field;
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    ++n;
  }
  pos=input->tell();
  if (pos!=endPos) {
    ascFile.addPos(pos);
    ascFile.addNote("StructZone-end:###");
  }
  ascFile.addPos(zone.m_entry.end());
  ascFile.addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// zone unpack/create ascii file, ...
////////////////////////////////////////////////////////////
bool RagTime5Parser::update(RagTime5ParserInternal::Zone &zone)
{
  if (zone.m_entriesList.empty())
    return true;
  if (zone.isHeaderZone()) {
    zone.m_isParsed=true;
    return false;
  }
  std::stringstream s;
  s << "Zone" << std::hex << zone.m_entriesList[0].begin() << std::dec;
  zone.setAsciiFileName(s.str());

  if (zone.m_entriesList.size()==1) {
    zone.m_entry=zone.m_entriesList[0];
    return true;
  }

  libmwaw::DebugStream f;
  f << "Entries(" << zone.getZoneName() << "):";
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

bool RagTime5Parser::unpackZone(RagTime5ParserInternal::Zone &zone, MWAWEntry const &entry, std::vector<unsigned char> &data)
{
  if (!entry.valid())
    return false;

  MWAWInputStreamPtr input=zone.getInput();
  long pos=entry.begin(), endPos=entry.end();
  if (entry.length()<4 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTime5Parser::unpackZone: the input seems bad\n"));
    return false;
  }

  bool actEndian=input->readInverted();
  input->setReadInverted(false);
  input->seek(pos, librevenge::RVNG_SEEK_SET);

  data.resize(0);
  unsigned long sz=(unsigned long) input->readULong(4);
  if (sz==0) {
    input->setReadInverted(actEndian);
    return true;
  }
  int flag=int(sz>>24);
  sz &= 0xFFFFFF;
  if ((flag&0xf) || (flag&0xf0)==0 || !(sz&0xFFFFFF)) {
    input->setReadInverted(actEndian);
    return false;
  }

  int nBytesRead=0, szField=9;
  unsigned int read=0;
  size_t mapPos=0;
  data.reserve(size_t(sz));
  std::vector<std::vector<unsigned char> > mapToString;
  mapToString.reserve(size_t(entry.length()-6));
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
      data.push_back(c);
      if (mapPos>= mapToString.size())
        mapToString.resize(mapPos+1);
      mapToString[mapPos++]=std::vector<unsigned char>(1,c);
      continue;
    }
    if (val==0x100) { // begin
      if (!data.empty()) {
        // data are reset when mapPos=3835, so it is ok
        mapPos=0;
        mapToString.resize(0);
        szField=9;
      }
      continue;
    }
    if (val==0x101) {
      ok=read==0;
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
    data.insert(data.end(), final.begin(), final.end());
    if (mapPos>= mapToString.size())
      mapToString.resize(mapPos+1);
    mapToString[mapPos++]=final;
  }

  if (ok && data.size()!=(size_t) sz) {
    MWAW_DEBUG_MSG(("RagTime5Parser::unpackZone: oops the data file is bad\n"));
    ok=false;
  }
  if (!ok) {
    MWAW_DEBUG_MSG(("RagTime5Parser::unpackZone: stop with mapPos=%ld and totalSize=%ld/%ld\n", long(mapPos), long(data.size()), long(sz)));
  }
  input->setReadInverted(actEndian);
  return ok;
}

bool RagTime5Parser::unpackZone(RagTime5ParserInternal::Zone &zone)
{
  if (!zone.m_entry.valid())
    return false;

  std::vector<unsigned char> newData;
  if (!unpackZone(zone, zone.m_entry, newData))
    return false;
  long pos=zone.m_entry.begin(), endPos=zone.m_entry.end();
  MWAWInputStreamPtr input=zone.getInput();
  if (input->tell()!=endPos) {
    MWAW_DEBUG_MSG(("RagTime5Parser::unpackZone: find some extra data\n"));
    return false;
  }
  if (newData.empty()) {
    // empty zone
    zone.ascii().addPos(pos);
    zone.ascii().addNote("_");
    zone.m_entry.setLength(0);
    zone.m_extra += "packed,";
    return true;
  }

  if (input.get()==getInput().get())
    ascii().skipZone(pos, endPos-1);

  shared_ptr<MWAWStringStream> newStream(new MWAWStringStream(&newData[0], (unsigned int) newData.size()));
  MWAWInputStreamPtr newInput(new MWAWInputStream(newStream, false));
  zone.setInput(newInput);
  zone.m_entry.setBegin(0);
  zone.m_entry.setLength(newInput->size());
  zone.m_extra += "packed,";
  return true;
}

////////////////////////////////////////////////////////////
// read the different zones
////////////////////////////////////////////////////////////
bool RagTime5Parser::readDocumentVersion(RagTime5ParserInternal::Zone &zone)
{
  MWAWInputStreamPtr input = zone.getInput();
  MWAWEntry &entry=zone.m_entry;

  zone.m_isParsed=true;
  ascii().addPos(zone.m_defPosition);
  ascii().addNote("doc[version],");

  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  f << "Entries(DocVersion):";
  ascFile.addPos(entry.end());
  ascFile.addNote("_");
  if ((entry.length())%6!=2) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readDocumentVersion: the entry size seem bads\n"));
    f << "###";
    ascFile.addPos(entry.begin());
    ascFile.addNote(f.str().c_str());
    return true;
  }
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  int val=(int) input->readLong(1); // find 2-4
  f << "f0=" << val << ",";
  val=(int) input->readLong(1); // always 0
  if (val)
    f << "f1=" << val << ",";
  int N=int(entry.length()/6);
  for (int i=0; i<N; ++i) {
    // v0: last used version, v1: first used version, ... ?
    f << "v" << i << "=" << input->readLong(1);
    val = (int) input->readULong(1);
    if (val)
      f << "." << val;
    val = (int) input->readULong(1); // 20|60|80
    if (val != 0x80)
      f << ":" << std::hex << val << std::dec;
    for (int j=0; j<3; ++j) { // often 0 or small number
      val = (int) input->readULong(1);
      if (val)
        f << ":" << val << "[" << j << "]";
    }
    f << ",";
  }
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  return true;
}

bool RagTime5Parser::readPictureList(RagTime5ParserInternal::Zone &zone, std::vector<int> &listIds)
{
  listIds.resize(0);
  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  if (zone.m_name.empty())
    f << "Entries(PictureList)[" << zone << "]:";
  else
    f << "Entries(" << zone.m_name << ")[pictureList," << zone << "]:";
  MWAWEntry &entry=zone.m_entry;
  zone.m_isParsed=true;
  ascFile.addPos(entry.end());
  ascFile.addNote("_");
  ascii().addPos(zone.m_defPosition);
  ascii().addNote("picture[list]");

  if (entry.length()%4) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readPictureList: the entry size seems bad\n"));
    f << "###";
    ascFile.addPos(entry.begin());
    ascFile.addNote(f.str().c_str());
    return false;
  }

  MWAWInputStreamPtr input = zone.getInput();
  input->setReadInverted(!zone.m_hiLoEndian); // checkme never seens
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);

  int N=int(entry.length()/4);
  for (int i=0; i<N; ++i) {
    int val=(int) input->readLong(2); // always 1
    int id=(int) input->readLong(2);
    listIds.push_back(id);
    f << id;
    if (val!=1) f << ":" << val << ",";
    else f << ",";
  }
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  input->setReadInverted(false);
  return true;
}

bool RagTime5Parser::readPictureMatch(RagTime5ParserInternal::Zone &zone, bool color)
{
  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  if (zone.m_name.empty())
    f << "Entries(" << (color ? "PictureColMatch" : "PictureMatch") << ")[" << zone << "]:";
  else
    f << "Entries(" << zone.m_name << "[" << (color ? "pictureColMatch" : "pictureMatch") << ")[" << zone << "]:";
  MWAWEntry &entry=zone.m_entry;
  zone.m_isParsed=true;
  ascFile.addPos(entry.end());
  ascFile.addNote("_");
  ascii().addPos(zone.m_defPosition);
  ascii().addNote(color ? "picture[matchCol]" : "picture[match]");

  int const expectedSz=color ? 42 : 32;
  if (entry.length() != expectedSz) {
    MWAW_DEBUG_MSG(("RagTime5Parser::readPictureMatch: the entry size seems bad\n"));
    f << "###";
    ascFile.addPos(entry.begin());
    ascFile.addNote(f.str().c_str());
    return false;
  }

  MWAWInputStreamPtr input = zone.getInput();
  input->setReadInverted(!zone.m_hiLoEndian); // checkme never seens
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);

  int val;
  for (int i=0; i<4; ++i) {
    static int const(expected[])= {0,0,0x7fffffff,0x7fffffff};
    val=(int) input->readLong(4);
    if (val!=expected[i])
      f << "f" << i << "=" << val << ",";
  }
  int dim[2];
  for (int i=0; i<2; ++i)
    dim[i]=(int) input->readLong(2);
  f << "dim=" << dim[0] << "x" << dim[1] << ",";
  for (int i=0; i<2; ++i) { // f2=0-3, f4=0-1
    val=(int) input->readLong(2);
    if (val)
      f << "f" << i+2 << "=" << val << ",";
  }
  // a very big number
  f << "ID?=" << std::hex << input->readULong(4) << std::dec << ",";
  for (int i=0; i<2; ++i) { // f5=f6=0
    val=(int) input->readLong(2);
    if (val)
      f << "f" << i+4 << "=" << val << ",";
  }
  if (color) {
    for (int i=0; i<5; ++i) { // g0=a|32, g1=0|1, other 0, color and pattern ?
      val=(int) input->readLong(2);
      if (val)
        f << "g" << i << "=" << val << ",";
    }
  }
  input->setReadInverted(false);
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());

  return true;
}

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
    if (val!=(long) 0xc5d0d3c6 && val != (long) 0x25215053) return false;
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
    // jpeg format begin by 0xffd8 and jpeg-2000 format begin by 0000 000c 6a50...
    if (val!=0xffd8 && (val!=0 || input->readULong(4)!=0xc6a50 || input->readULong(4)!=0x20200d0a))
      return false;
    extension="jpg";
    break;
  case P_Pict:
    input->seek(10, librevenge::RVNG_SEEK_CUR);
    val=(long) input->readULong(2);
    if (val!=0x1101 && val !=0x11) return false;
    extension="pct";
    break;
  case P_PNG:
    if (input->readULong(4) != 0x89504e47) return false;
    extension="png";
    break;
  case P_ScreenRep:
    val=(long) input->readULong(1);
    if (val!=0x49 && val!=0x4d) return false;
    MWAW_DEBUG_MSG(("RagTime5Parser::readPicture: find unknown picture format for zone %d\n", zone.m_ids[0]));
    extension="sRep";
    break;
  case P_Tiff:
    val=(long) input->readULong(2);
    if (val!=0x4949 && val != 0x4d4d) return false;
    val=(long) input->readULong(2);
    /* find also frequently 4d4d 00dd b300 d61e here ?
       and one time 4d 00 b3 2a d6 */
    if (val!=0x2a00 && val!=0x002a) {
      static bool first=true;
      if (first) {
        MWAW_DEBUG_MSG(("RagTime5Parser::readPicture: some tiffs seem bad, zone %d, ...\n", zone.m_ids[0]));
        first=false;
      }
      extension="check.tiff";
    }
    else
      extension="tiff";
    break;
  case P_WMF:
    if (input->readULong(4)!=0x01000900) return false;
    extension="wmf";
    break;
  case P_Unknown:
  default:
    return false;
  }
  zone.m_isParsed=true;
  libmwaw::DebugStream f;
  f << "picture[" << extension << "],";
  ascii().addPos(zone.m_defPosition);
  ascii().addNote(f.str().c_str());
#ifdef DEBUG_WITH_FILES
  if (type==P_ScreenRep) {
    libmwaw::DebugFile &ascFile=zone.ascii();
    f.str("");
    f << "Entries(ScrRep)[" << zone << "]:";
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
  f.str("");
  f << "Pict-" << ++pictName << "." << extension;
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
    for (int i=0; i<4-type; ++i) {
      zone->m_idsFlag[i]=(int) input->readULong(2); // alway 0/1?
      zone->m_ids[i]=(int) input->readULong(2);
    }
    bool ok=true;
    do {
      int type2=(int)  input->readULong(1);
      switch (type2) {
      case 4: // always 0, 1
      case 0xa: { // always 0, 0: never seens in v5 but frequent in v6
        ok = input->tell()+4+(type2==4 ? 1 : 0)<=entry.end();
        if (!ok) break;
        int data[2];
        for (int i=0; i<2; ++i)
          data[i]=(int) input->readULong(2);
        if (type2==4) {
          if (data[0]==0 && data[1]==1)
            f << "selected,";
          else if (data[0]==0)
            f << "#selected=" << data[1] << ",";
          else
            f << "#selected=[" << data[0] << "," << data[1] << "],";
        }
        else
          f << "g" << std::hex << type2 << std::dec << "=[" << data[0] << "," << data[1] << "],";
        break;
      }
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
      case 0xd: // always 0 || c000
        ok = input->tell()+4<=entry.end();
        if (!ok) break;
        for (int i=0; i<2; ++i)
          zone->m_variableD[i]=(int) input->readULong(2);
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
      if (!ok || (type2&1) || (type2==0xa))
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
      ascii().addDelimiter(input->tell(),'|');
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
  val=(int) input->readLong(1);
  setVersion(5);
  switch (val) {
  case 0:
    f << "vers=5,";
    break;
  case 4:
    f << "vers=6.5,";
    setVersion(6);
    break;
  default:
    f << "#vers=" << val << ",";
    break;
  }
  for (int i=0; i<2; ++i) {
    val=(int) input->readLong(1);
    if (val) f << "g" << i+1 << "=" << val << ",";
  }
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
