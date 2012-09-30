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

#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>

#include <libwpd/libwpd.h>

#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWPictBasic.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSubDocument.hxx"

#include "NSParser.hxx"
#include "NSStruct.hxx"

#include "NSGraph.hxx"

/** Internal: the structures of a NSGraph */
namespace NSGraphInternal
{
//! a RSSO entry in a pict file
struct RSSOEntry {
  //! constructor
  RSSOEntry() : m_id(-1), m_position() { }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, RSSOEntry const &entry) {
    o << "id=" << entry.m_id << ",";
    o << "box=" << entry.m_position << ",";
    return o;
  }
  //! the id
  int m_id;
  //! the bdbox
  Box2f m_position;
};
////////////////////////////////////////
//! Internal: the state of a NSGraph
struct State {
  //! constructor
  State() : m_numPages(0), m_maxPageGraphic(0), m_idPictMap(), m_idRssoMap() { }

  int m_numPages /* the number of pages */;
  //! the last page containing page graphic
  int m_maxPageGraphic;
  //! the map pictId -> pictEntry
  std::map<int, MWAWEntry> m_idPictMap;
  //! the map id -> rssoEntry
  std::map<int, MWAWEntry> m_idRssoMap;
};

////////////////////////////////////////
//! Internal: the subdocument of a NSGraph
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(NSGraph &pars, MWAWInputStreamPtr input, int id, MWAWPosition const &pos, WPXPropertyList const &extras) :
    MWAWSubDocument(pars.m_mainParser, input, MWAWEntry()), m_graphParser(&pars), m_id(id), m_position(pos), m_extras(extras) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const;
  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }

  //! the parser function
  void parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  /** the graph parser */
  NSGraph *m_graphParser;
  //! the pict id
  int m_id;
  //! the pict position
  MWAWPosition m_position;
  //! the property list
  WPXPropertyList m_extras;
private:
  SubDocument(SubDocument const &orig);
  SubDocument &operator=(SubDocument const &orig);
};

void SubDocument::parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType /*type*/)
{
  if (!listener.get()) {
    MWAW_DEBUG_MSG(("SubDocument::parse: no listener\n"));
    return;
  }
  NSContentListener *listen = dynamic_cast<NSContentListener *>(listener.get());
  if (!listen) {
    MWAW_DEBUG_MSG(("SubDocument::parse: bad listener\n"));
    return;
  }

  assert(m_graphParser);

  long pos = m_input->tell();
  m_graphParser->sendPicture(m_id, true, m_position, m_extras);
  m_input->seek(pos, WPX_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_graphParser != sDoc->m_graphParser) return true;
  if (m_id != sDoc->m_id) return true;
  if (m_position != sDoc->m_position) return true;
  return false;
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
NSGraph::NSGraph
(MWAWInputStreamPtr ip, NSParser &parser, MWAWFontConverterPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert), m_state(new NSGraphInternal::State),
  m_mainParser(&parser), m_asciiFile(parser.ascii())
{
}

NSGraph::~NSGraph()
{ }

int NSGraph::version() const
{
  return m_mainParser->version();
}

int NSGraph::numPages() const
{
  return m_state->m_maxPageGraphic;
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool NSGraph::createZones()
{
  MWAWRSRCParserPtr rsrcParser = m_mainParser->getRSRCParser();
  if (!rsrcParser) {
    MWAW_DEBUG_MSG(("NSGraph::createZones: can not find the entry map\n"));
    return false;
  }
  std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::iterator it;

  // the different pict zones
  it = entryMap.lower_bound("PICT");
  while (it != entryMap.end()) {
    if (it->first != "PICT")
      break;
    MWAWEntry const &entry = it++->second;
    m_state->m_idPictMap[entry.id()]=entry;
  }
  it = entryMap.lower_bound("RSSO");
  while (it != entryMap.end()) {
    if (it->first != "RSSO")
      break;
    MWAWEntry &entry = it++->second;
    m_state->m_idRssoMap[entry.id()]=entry;
  }

  // number of page graphic
  it = entryMap.lower_bound("PGRA");
  while (it != entryMap.end()) {
    if (it->first != "PGRA")
      break;
    MWAWEntry &entry = it++->second;
    readPGRA(entry);
  }

  // a picture position ?
  it = entryMap.lower_bound("PLAC");
  while (it != entryMap.end()) {
    if (it->first != "PLAC")
      break;
    MWAWEntry &entry = it++->second;
    readPLAC(entry);
  }
  it = entryMap.lower_bound("PLDT");
  while (it != entryMap.end()) {
    if (it->first != "PLDT")
      break;
    MWAWEntry &entry = it++->second;
    entry.setName("PLDT");
    NSStruct::RecursifData data(NSStruct::Z_Main);
    data.read(*m_mainParser, entry);
    readPLDT(data);
  }

  return true;
}

// read the PLAC zone ( a list of picture placement ? )
bool NSGraph::readPLAC(MWAWEntry const &entry)
{
  if ((!entry.valid()&&entry.length()) || (entry.length()%202)) {
    MWAW_DEBUG_MSG(("NSGraph::readPLAC: the entry is bad\n"));
    return false;
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  int numElt = int(entry.length()/202);
  libmwaw::DebugStream f;
  f << "Entries(PLAC)[" << entry.id() << "]:N=" << numElt;
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());

  long val;
  for (int i = 0; i < numElt; i++) {
    pos = input->tell();
    f.str("");
    f << "PLAC" << i << ":";
    val = (int) input->readULong(2);
    f << "pictId=" << val;
    ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+202, WPX_SEEK_SET);
  }
  return true;
}

// read PLDT zone: a unknown zone (a type, an id/anchor type? and a bdbox )
bool NSGraph::readPLDT(NSStruct::RecursifData const &data)
{
  if (!data.m_info || data.m_info->m_zoneType < 0 || data.m_info->m_zoneType >= 3) {
    MWAW_DEBUG_MSG(("NSGraph::readPLDT: find unexpected zoneType\n"));
    return false;
  }

  if (!data.m_childList.size())
    return true;

  if (data.m_childList.size() > 1) {
    MWAW_DEBUG_MSG(("NSGraph::readPLDT: level 0 node contains more than 1 node\n"));
  }
  if (data.m_childList[0].isLeaf()) {
    MWAW_DEBUG_MSG(("NSGraph::readPLDT: level 1 node is a leaf\n"));
    return false;
  }
  NSStruct::RecursifData const &mainData = *data.m_childList[0].m_data;
  size_t numData = mainData.m_childList.size();
  //  NSGraphInternal::Zone &zone = m_state->m_zones[(int) data.m_info->m_zoneType];
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  libmwaw::DebugStream f;

  long val;
  for (size_t n = 0 ; n < numData; n++) {
    if (mainData.m_childList[n].isLeaf()) {
      MWAW_DEBUG_MSG(("NSGraph::readPLDT: oops some level 2 node are leaf\n"));
      continue;
    }
    NSStruct::RecursifData const &dt=*mainData.m_childList[n].m_data;
    /* type == 7fffffff and wh = 2 */
    if (dt.m_childList.size() != 1) {
      MWAW_DEBUG_MSG(("NSGraph::readPLDT: find an odd number of 3 leavers\n"));
      continue;
    }
    NSStruct::RecursifData::Node const &child= dt.m_childList[0];
    if (!child.isLeaf() || child.m_entry.length() < 14) {
      MWAW_DEBUG_MSG(("NSGraph::readPLDT: find an odd level 3 leaf\n"));
      continue;
    }

    long pos = child.m_entry.begin();
    input->seek(pos, WPX_SEEK_SET);
    f.str("");
    std::string type("");   // find different small string here
    for (int i = 0; i < 4; i++)
      type += (char) input->readULong(1);
    f << type << ",";
    val = input->readLong(2); // a small number find 4,5,b,d
    if (val) f << "f0=" << val << ",";
    int dim[4];
    for (int i = 0; i < 4; i++) dim[i] = (int) input->readLong(2);
    f << "bdbox=(" << dim[1] << "x" << dim[0] << "<->"
      << dim[3] << "x" << dim[2] << "),";
    ascFile.addPos(pos-12);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

//! read the PGRA resource: the number of page graphic ? (id 20000)
bool NSGraph::readPGRA(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() < 2) {
    MWAW_DEBUG_MSG(("NSGraph::readPGRA: the entry is bad\n"));
    return false;
  }
  if (entry.id() != 20000) {
    MWAW_DEBUG_MSG(("NSGraph::readPGRA: the entry id %d is odd\n", entry.id()));
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  if (entry.id() != 20000)
    f << "Entries(PGRA)[#" << entry.id() << "]:";
  else
    f << "Entries(PGRA):";
  // a number between 0 and 2: seems related to PICT20000 -> PICT20000+N-1
  m_state->m_maxPageGraphic = (int) input->readLong(2);
  f << "lastPage[withGraphic]=" << m_state->m_maxPageGraphic << ",";

  if (entry.length()!=2)
    f << "###size=" << entry.length() << ",";

  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// low level
////////////////////////////////////////////////////////////
std::vector<NSGraphInternal::RSSOEntry> NSGraph::findRSSOEntry(MWAWInputStreamPtr input) const
{
  std::vector<NSGraphInternal::RSSOEntry> listRSSO;
  if (!input) {
    MWAW_DEBUG_MSG(("NSGraph::findRSSOEntry: can not find the input\n"));
    return listRSSO;
  }

  input->seek(10, WPX_SEEK_SET);
  int header = (int) input->readULong(2);
  if (header==0x0011) {
    if (input->readULong(2) != 0x2ff)
      return listRSSO;
    // ok look like a pict2
  } else if (header != 0x1101)
    return listRSSO;

  // look for: 00a1006400104e495349000900b3000901dc01f90002
  while (!input->atEOS()) {
    long pos = input->tell();
    int val=(int) input->readULong(4);
    int depl = 0;
    if (val == 0x104e4953) ;
    else if (val == 0x4e495349) depl=-1;
    else if (val == 0x49534900) depl=-2;
    else if (val == 0x53490009) depl=-3;
    else continue;
    input->seek(depl-8, WPX_SEEK_CUR);
    bool ok = input->readULong(1) == 0xa1;
    ok = ok && input->readULong(4) == 0x00640010;
    ok = ok && input->readULong(4) == 0x4e495349;
    ok = ok && input->readULong(2) == 0x0009;
    if (!ok) {
      input->seek(pos+4, WPX_SEEK_SET);
      continue;
    }
    float dim[4];
    for (int i=0; i < 4; i++) dim[i] = float(input->readLong(2));
    if (input->atEOS()) break;
    NSGraphInternal::RSSOEntry rsso;
    rsso.m_id = (int) input->readLong(2);
    if (input->atEOS()) break;
    rsso.m_position=Box2f(Vec2f(dim[1], dim[0]), Vec2f(dim[3], dim[2]));
    if (rsso.m_id > 0)
      listRSSO.push_back(rsso);
    else if (version() > 3) {
      MWAW_DEBUG_MSG(("NSGraph::findRSSOEntry: find odd rsso id%d\n", rsso.m_id));
    }
  }

  return listRSSO;
}

////////////////////////////////////////////////////////////
// send data
////////////////////////////////////////////////////////////
bool NSGraph::sendPicture(int pictId, bool inPictRsrc, MWAWPosition pictPos,
                          WPXPropertyList extras)
{
  MWAWRSRCParserPtr rsrcParser = m_mainParser->getRSRCParser();
  if (!m_listener) {
    MWAW_DEBUG_MSG(("NSGraph::sendPicture: can not find the listener\n"));
    return true;
  }
  std::map<int, MWAWEntry> &pictMap = inPictRsrc ?
                                      m_state->m_idPictMap : m_state->m_idRssoMap;
  if (pictMap.find(pictId) == pictMap.end()) {
    if (version() <= 3 && !inPictRsrc)
      return true;
    MWAW_DEBUG_MSG(("NSGraph::sendPicture: can not find the picture\n"));
    return false;
  }
  MWAWEntry &entry = pictMap.find(pictId)->second;
  WPXBinaryData data;
  bool ok = rsrcParser->parsePICT(entry, data) && data.size();
  if (!ok) {
    MWAW_DEBUG_MSG(("NSGraph::sendPicture: can not read the picture\n"));
  }
  entry.setParsed(true);
  if (!ok) return true;

  std::vector<NSGraphInternal::RSSOEntry> listRSSO;
  if (inPictRsrc) {
    // we must first look for RSSO entry
    WPXInputStream *dataStream = const_cast<WPXInputStream *>(data.getDataStream());
    if (dataStream) {
      MWAWInputStreamPtr dataInput(new MWAWInputStream(dataStream, false));
      listRSSO=findRSSOEntry(dataInput);
    }
  }

  if (listRSSO.size() && (pictPos.m_anchorTo == MWAWPosition::Char ||
                          pictPos.m_anchorTo == MWAWPosition::CharBaseLine)) {
    // we need to create a frame
    MWAWPosition framePos(pictPos.origin(), pictPos.size(), WPX_POINT);;
    framePos.setRelativePosition(MWAWPosition::Char,
                                 MWAWPosition::XLeft, MWAWPosition::YTop);
    framePos.m_wrapping =  MWAWPosition::WRunThrough;
    pictPos.setRelativePosition(MWAWPosition::Frame);
    pictPos.setOrigin(Vec2f(0,0));
    MWAWSubDocumentPtr subdoc
    (new NSGraphInternal::SubDocument(*this, m_mainParser->rsrcInput(), pictId, pictPos, extras));
    m_listener->insertTextBox(framePos, subdoc);
    return true;
  }
  // first the picture
  m_listener->insertPicture(pictPos, data, "image/pict", extras);
  // then the author possible picture
  pictPos.setClippingPosition(Vec2f(), Vec2f());
  for (size_t i=0; i < listRSSO.size(); i++) {
    NSGraphInternal::RSSOEntry const &rssoEntry = listRSSO[i];
    MWAWPosition rssoPos(pictPos);
    rssoPos.setOrigin(pictPos.origin()+rssoEntry.m_position.min());
    rssoPos.setSize(rssoEntry.m_position.size());
    sendPicture(rssoEntry.m_id, false, rssoPos, extras);
  }
  return true;
}

bool NSGraph::sendPageGraphics()
{
  MWAWRSRCParserPtr rsrcParser = m_mainParser->getRSRCParser();
  if (!m_listener) {
    MWAW_DEBUG_MSG(("NSGraph::sendPageGraphics: can not find the listener\n"));
    return true;
  }
  Vec2f LT = 72.f*m_mainParser->getPageLeftTop();
  for (int i = 0; i < m_state->m_maxPageGraphic; i++) {
    if (m_state->m_idPictMap.find(20000+i)==m_state->m_idPictMap.end())
      continue;
    MWAWEntry &entry = m_state->m_idPictMap.find(20000+i)->second;
    WPXBinaryData data;
    if (!rsrcParser->parsePICT(entry, data) || !data.size()) {
      MWAW_DEBUG_MSG(("NSGraph::sendPageGraphics: can not read the file picture\n"));
      continue;
    }
    WPXInputStream *dataStream = const_cast<WPXInputStream *>(data.getDataStream());
    if (!dataStream)
      continue;
    MWAWInputStreamPtr dataInput(new MWAWInputStream(dataStream, false));
    dataInput->seek(0, WPX_SEEK_SET);
    Box2f box;
    if (MWAWPictData::check(dataInput, (int)data.size(), box) == MWAWPict::MWAW_R_BAD) {
      MWAW_DEBUG_MSG(("NSGraph::sendPageGraphics: can not determine the picture type\n"));
      continue;
    }
    MWAWPosition pictPos(box.min()+LT, box.size(), WPX_POINT);
    pictPos.setRelativePosition(MWAWPosition::Page);
    pictPos.m_wrapping = MWAWPosition::WRunThrough;
    pictPos.setPage(i+1);
    sendPicture(20000+i, true, pictPos);
  }
  return true;
}

void NSGraph::flushExtra()
{
  for (std::map<int, MWAWEntry>::iterator it = m_state->m_idPictMap.begin();
       it != m_state->m_idPictMap.end(); it++) {
    MWAWEntry &entry = it->second;
    if (entry.isParsed()) continue;
    MWAW_DEBUG_MSG(("NSGraph::sendPicture: picture unparsed: %d\n", entry.id()));
    MWAWPosition pictPos(Vec2f(0,0), Vec2f(1.,1.));
    pictPos.setRelativePosition(MWAWPosition::Char);
    sendPicture(entry.id(), true, pictPos);
  }
  for (std::map<int, MWAWEntry>::iterator it = m_state->m_idRssoMap.begin();
       it != m_state->m_idRssoMap.end(); it++) {
    MWAWEntry &entry = it->second;
    if (entry.isParsed()) continue;
    MWAW_DEBUG_MSG(("NSGraph::sendPicture: rsso picture unparsed: %d\n", entry.id()));
    MWAWPosition pictPos(Vec2f(0,0), Vec2f(1.,1.));
    pictPos.setRelativePosition(MWAWPosition::Char);
    sendPicture(entry.id(), false, pictPos, WPXPropertyList());
  }
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
