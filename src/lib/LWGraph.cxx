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
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSubDocument.hxx"

#include "LWParser.hxx"

#include "LWGraph.hxx"

/** Internal: the structures of a LWGraph */
namespace LWGraphInternal
{
////////////////////////////////////////
//! Internal: the state of a LWGraph
struct State {
  //! constructor
  State() : m_numPages(-1), m_idPictMap(), m_idJPEGMap() { }

  int m_numPages /* the number of pages */;
  /** a map id -> PICT entry */
  std::map<int, MWAWEntry> m_idPictMap;
  /** a map id -> JPEG entry */
  std::map<int, MWAWEntry> m_idJPEGMap;
};
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
LWGraph::LWGraph(LWParser &parser) :
  m_parserState(parser.getParserState()), m_state(new LWGraphInternal::State),
  m_mainParser(&parser)
{
}

LWGraph::~LWGraph()
{ }

int LWGraph::version() const
{
  return m_parserState->m_version;
}

int LWGraph::numPages() const
{
  if (m_state->m_numPages < 0)
    m_state->m_numPages= (m_state->m_idPictMap.size() ||m_state->m_idJPEGMap.size()) ? 1 : 0;
  return m_state->m_numPages;
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool LWGraph::createZones()
{
  MWAWRSRCParserPtr rsrcParser = m_mainParser->getRSRCParser();
  if (!rsrcParser) {
    MWAW_DEBUG_MSG(("LWGraph::createZones: can not find the entry map\n"));
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
    m_state->m_idPictMap.insert
    (std::map<int, MWAWEntry>::value_type(entry.id(), entry));
    // fixme stored the pict id here, ...
    WPXBinaryData data;
    rsrcParser->parsePICT(entry, data);
  }
  it = entryMap.lower_bound("JPEG");
  while (it != entryMap.end()) {
    if (it->first != "JPEG")
      break;

    MWAWEntry const &entry = it++->second;

    m_state->m_idJPEGMap.insert
    (std::map<int, MWAWEntry>::value_type(entry.id(), entry));
  }
  return true;
}

////////////////////////////////////////////////////////////
// low level
////////////////////////////////////////////////////////////
bool LWGraph::sendPICT(MWAWEntry const &entry)
{
  entry.setParsed(true);
  MWAWRSRCParserPtr rsrcParser = m_mainParser->getRSRCParser();

  if (!m_parserState->m_listener || !rsrcParser) {
    MWAW_DEBUG_MSG(("LWGraph::sendPICT: can not find the listener\n"));
    return false;
  }
  WPXBinaryData data;
  rsrcParser->parsePICT(entry, data);

  MWAWInputStreamPtr input=MWAWInputStream::get(data, false);
  if (!input) {
    MWAW_DEBUG_MSG(("LWGraph::sendPICT: can not find the stream\n"));
    return false;
  }
  shared_ptr<MWAWPict> pict(MWAWPictData::get(input, int(entry.length())));
  if (!pict)
    return false;

  Box2f bdBox=pict->getBdBox();
  MWAWPosition pictPos(Vec2f(0,0), bdBox.size(), WPX_POINT);
  pictPos.setRelativePosition(MWAWPosition::Char);

  WPXBinaryData pictData;
  std::string type;
  if (pict->getBinary(pictData,type))
    m_parserState->m_listener->insertPicture(pictPos, data, type);
  return true;
}

bool LWGraph::sendJPEG(MWAWEntry const &entry)
{
  if (!m_parserState->m_listener) {
    MWAW_DEBUG_MSG(("LWGraph::sendJPEG: can not find the listener\n"));
    return false;
  }
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("LWGraph::sendJPEG: the entry is bad\n"));
    return false;
  }

  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(JPEG):" << entry.id();
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());

  WPXBinaryData data;
  input->seek(entry.begin(), WPX_SEEK_SET);
  input->readDataBlock(entry.length(), data);
  MWAWPosition pictPos;
  pictPos.setRelativePosition(MWAWPosition::Char);
  Vec2i sz;
  if (findJPEGSize(data,sz)) {
    pictPos.setSize(sz);
    pictPos.setUnit(WPX_POINT);
  }
  m_parserState->m_listener->insertPicture(pictPos, data, "image/pict");

#ifdef DEBUG_WITH_FILES
  if (!entry.isParsed()) {
    ascFile.skipZone(entry.begin(), entry.end()-1);
    libmwaw::DebugStream f2;
    f2 << "JPEG" << entry.id() << ".jpg";
    libmwaw::Debug::dumpFile(data, f2.str().c_str());
  }
#endif
  entry.setParsed(true);

  return true;
}

bool LWGraph::findJPEGSize(WPXBinaryData const &data, Vec2i &sz)
{
  sz = Vec2i(100,100);
  MWAWInputStreamPtr input=MWAWInputStream::get(data, false);
  if (!input) {
    MWAW_DEBUG_MSG(("LWGraph::findJPEGSize: can not find the stream\n"));
    return false;
  }

  if (input->readULong(4)!=0xFFD8FFE0) {
    MWAW_DEBUG_MSG(("LWGraph::findJPEGSize: invalid header\n"));
    return false;
  }
  long pos = input->tell();
  int len = (int) input->readULong(2);
  if (input->readULong(4)!=0x4a464946) {
    MWAW_DEBUG_MSG(("LWGraph::findJPEGSize: not a JFIF file\n"));
    return false;
  }
  input->seek(pos+len, WPX_SEEK_SET);
  while (!input->atEOS()) {
    int header = (int) input->readULong(2);
    pos = input->tell();
    len = (int) input->readULong(2);
    if ((header&0xFF00) != 0xFF00) {
      MWAW_DEBUG_MSG(("LWGraph::findJPEGSize: oops bad data header\n"));
      break;
    }
    if (header != 0xFFC0) {
      input->seek(pos+len, WPX_SEEK_SET);
      continue;
    }
    input->seek(1, WPX_SEEK_CUR);
    int dim[2];
    for (int i = 0; i < 2; i++)
      dim[i] = (int) input->readULong(2);
    sz = Vec2i(dim[1],dim[0]);
    return true;
  }
  return false;
}

////////////////////////////////////////////////////////////
// send data
////////////////////////////////////////////////////////////

void LWGraph::send(int id)
{
  if (m_state->m_idJPEGMap.find(999+id) != m_state->m_idJPEGMap.end()) {
    sendJPEG(m_state->m_idJPEGMap.find(999+id)->second);
    return;
  }
  if (m_state->m_idPictMap.find(999+id) != m_state->m_idPictMap.end()) {
    sendPICT(m_state->m_idPictMap.find(999+id)->second);
    return;
  }
  MWAW_DEBUG_MSG(("LWGraph::send: can not find graphic %d\n", id));
}

bool LWGraph::sendPageGraphics()
{
  return true;
}

void LWGraph::flushExtra()
{
#ifdef DEBUG
  std::map<int, MWAWEntry>::const_iterator it;
  for (it = m_state->m_idPictMap.begin() ; it != m_state->m_idPictMap.end(); ++it) {
    MWAWEntry const &entry = it->second;
    if (entry.isParsed()) continue;
    sendPICT(entry);
  }
  for (it = m_state->m_idJPEGMap.begin() ; it != m_state->m_idJPEGMap.end(); ++it) {
    MWAWEntry const &entry = it->second;
    if (entry.isParsed()) continue;
    sendJPEG(entry);
  }
#endif
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
