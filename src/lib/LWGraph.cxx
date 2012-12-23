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

#include "LWParser.hxx"

#include "LWGraph.hxx"

/** Internal: the structures of a LWGraph */
namespace LWGraphInternal
{
////////////////////////////////////////
//! Internal: the state of a LWGraph
struct State {
  //! constructor
  State() : m_numPages(0) { }

  int m_numPages /* the number of pages */;
};

////////////////////////////////////////
//! Internal: the subdocument of a LWGraph
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(LWGraph &pars, MWAWInputStreamPtr input, int id, MWAWPosition const &pos, WPXPropertyList const &extras) :
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
  LWGraph *m_graphParser;
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
  LWContentListener *listen = dynamic_cast<LWContentListener *>(listener.get());
  if (!listen) {
    MWAW_DEBUG_MSG(("SubDocument::parse: bad listener\n"));
    return;
  }

  assert(m_graphParser);

  long pos = m_input->tell();
  //  m_graphParser->sendPicture(m_id, true, m_position, m_extras);
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
LWGraph::LWGraph
(MWAWInputStreamPtr ip, LWParser &parser, MWAWFontConverterPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert), m_state(new LWGraphInternal::State),
  m_mainParser(&parser), m_asciiFile(parser.ascii())
{
}

LWGraph::~LWGraph()
{ }

int LWGraph::version() const
{
  return m_mainParser->version();
}

int LWGraph::numPages() const
{
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
    // fixme stored the pict id here, ...
    WPXBinaryData data;
    rsrcParser->parsePICT(entry, data);
  }
  it = entryMap.lower_bound("JPEG");
  while (it != entryMap.end()) {
    if (it->first != "JPEG")
      break;

    MWAWEntry const &entry = it++->second;
    // fixme stored the pict id here, ...
    readJPEG(entry);
  }
  return true;
}

////////////////////////////////////////////////////////////
// low level
////////////////////////////////////////////////////////////
bool LWGraph::readJPEG(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("LWGraph::readJPEG: the entry is bad\n"));
    return false;
  }
  MWAWInputStreamPtr input = m_mainParser->rsrcInput();
  libmwaw::DebugFile &ascFile = m_mainParser->rsrcAscii();
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(JPEG):" << entry.id();

  WPXBinaryData data;
  input->seek(entry.begin(), WPX_SEEK_SET);
  input->readDataBlock(entry.length(), data);

#ifdef DEBUG_WITH_FILES
  if (!entry.isParsed()) {
    ascFile.skipZone(entry.begin(), entry.end()-1);
    libmwaw::DebugStream f2;
    f2 << "JPEG" << entry.id() << ".jpg";
    libmwaw::Debug::dumpFile(data, f2.str().c_str());
  }
#endif
  entry.setParsed(true);

  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// send data
////////////////////////////////////////////////////////////

bool LWGraph::sendPageGraphics()
{
  return true;
}

void LWGraph::flushExtra()
{
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
