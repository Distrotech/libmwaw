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
#include <set>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWFont.hxx"
#include "MWAWListener.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSubDocument.hxx"

#include "RagTime5Parser.hxx"
#include "RagTime5StructManager.hxx"

#include "RagTime5Text.hxx"

#include "libmwaw_internal.hxx"

/** Internal: the structures of a RagTime5Text */
namespace RagTime5TextInternal
{
////////////////////////////////////////
//! Internal: the helper to read field for a RagTime5Text
struct FieldParser : public RagTime5StructManager::FieldParser {
  //! constructor
  FieldParser(RagTime5Text &parser) : m_mainParser(parser)
  {
  }
  //! return the debug name corresponding to the zone
  std::string getZoneName() const
  {
    return "TextStyle";
  }
  //! return the debug name corresponding to a field
  std::string getZoneName(int n) const
  {
    std::stringstream s;
    s << "TextStyle-TS" << n;
    return s.str();
  }
  //! parse a field
  virtual bool parseField(RagTime5StructManager::Field &field, RagTime5StructManager::Zone &zone, int /*n*/, libmwaw::DebugStream &f)
  {
    RagTime5StructManager::TextStyle style;
    MWAWInputStreamPtr input=zone.getInput();
    if (style.read(input, field))
      f << style;
    else
      f << "#" << field;
    return true;
  }

protected:
  //! the main parser
  RagTime5Text &m_mainParser;
};

////////////////////////////////////////
//! Internal: the state of a RagTime5Text
struct State {
  //! constructor
  State() : m_numPages(0) { }
  //! the number of pages
  int m_numPages;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
RagTime5Text::RagTime5Text(RagTime5Parser &parser) :
  m_mainParser(parser), m_structManager(m_mainParser.getStructManager()), m_parserState(parser.getParserState()),
  m_state(new RagTime5TextInternal::State)
{
}

RagTime5Text::~RagTime5Text()
{ }

int RagTime5Text::version() const
{
  return m_parserState->m_version;
}

int RagTime5Text::numPages() const
{
  // TODO IMPLEMENT ME
  MWAW_DEBUG_MSG(("RagTime5Text::numPages: is not implemented\n"));
  return 0;
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// style
////////////////////////////////////////////////////////////
bool RagTime5Text::readTextStyles(RagTime5StructManager::Cluster &cluster)
{
  RagTime5TextInternal::FieldParser fieldParser(*this);
  return m_mainParser.readStructZone(cluster, fieldParser);
}

////////////////////////////////////////////////////////////
// main zone
////////////////////////////////////////////////////////////
bool RagTime5Text::readTextZone(RagTime5StructManager::Cluster &cluster)
{
  RagTime5StructManager::Link const &link=cluster.m_dataLink;
  if (link.m_ids.size()<2 || !link.m_ids[1]) {
    MWAW_DEBUG_MSG(("RagTime5Text::readTextZone: can not find the data zone\n"));
    return false;
  }

  shared_ptr<RagTime5StructManager::Zone> dataZone=m_mainParser.getDataZone(link.m_ids[0]);
  if (!dataZone || !dataZone->m_entry.valid() ||
      dataZone->getKindLastPart(dataZone->m_kinds[1].empty())!="ItemData") {
    MWAW_DEBUG_MSG(("RagTime5Text::readTextZone: can not find the first zone %d\n", link.m_ids[0]));
  }
  else {
    dataZone->m_isParsed=true;
    MWAWEntry entry=dataZone->m_entry;
    libmwaw::DebugFile &ascFile=dataZone->ascii();
    libmwaw::DebugStream f;
    f << "Entries(TextPosition)[" << *dataZone << "]:";
    ascFile.addPos(entry.end());
    ascFile.addNote("_");
    ascFile.addPos(entry.begin());
    ascFile.addNote(f.str().c_str());
  }
  int const dataId=link.m_ids[1];
  dataZone=m_mainParser.getDataZone(dataId);
  if (!dataZone || !dataZone->m_entry.valid() ||
      dataZone->getKindLastPart(dataZone->m_kinds[1].empty())!="Unicode") {
    MWAW_DEBUG_MSG(("RagTime5Text::readTextZone: the text zone %d seems bad\n", dataId));
    return false;
  }
  return m_mainParser.readUnicodeString(*dataZone);
}

////////////////////////////////////////////////////////////
// unknown
////////////////////////////////////////////////////////////
bool RagTime5Text::readTextUnknown(int typeId)
{
  if (!typeId)
    return false;

  shared_ptr<RagTime5StructManager::Zone> zone=m_mainParser.getDataZone(typeId);
  if (!zone || !zone->m_entry.valid() || (zone->m_entry.length()%6) ||
      zone->getKindLastPart(zone->m_kinds[1].empty())!="ItemData") {
    MWAW_DEBUG_MSG(("RagTime5Text::readTextUnknown: the entry of zone %d seems bad\n", typeId));
    return false;
  }
  MWAWEntry entry=zone->m_entry;
  MWAWInputStreamPtr input=zone->getInput();
  input->setReadInverted(!zone->m_hiLoEndian);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);

  libmwaw::DebugFile &ascFile=zone->ascii();
  libmwaw::DebugStream f;
  zone->m_isParsed=true;
  ascFile.addPos(entry.end());
  ascFile.addNote("_");

  f << "Entries(TextUnknown)[" << *zone << "]:";
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());

  int N=int(entry.length()/6);
  for (int i=0; i<N; ++i) {
    long pos=input->tell();
    f.str("");
    f << "TextUnknown-" << i << ":";
    f << "offset?=" << input->readULong(4) << ",";
    f << "TS" << input->readULong(2) << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  input->setReadInverted(false);
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// interface send function
////////////////////////////////////////////////////////////

void RagTime5Text::flushExtra()
{
  MWAW_DEBUG_MSG(("RagTime5Text::flushExtra: is not implemented\n"));
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
