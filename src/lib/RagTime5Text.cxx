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
bool RagTime5Text::readTextStyles(RagTime5StructManager::Zone &/*zone*/, RagTime5StructManager::ZoneLink const &link)
{
  if (link.m_ids.size()<2 || !link.m_ids[1])
    return false;

  std::vector<long> decal;
  if (link.m_ids[0])
    m_mainParser.readPositions(link.m_ids[0], decal);
  if (decal.empty())
    decal=link.m_longList;
  int const dataId=link.m_ids[1];
  shared_ptr<RagTime5StructManager::Zone> dataZone=m_mainParser.getDataZone(dataId);
  if (!dataZone || !dataZone->m_entry.valid() ||
      dataZone->getKindLastPart(dataZone->m_kinds[1].empty())!="ItemData") {
    MWAW_DEBUG_MSG(("RagTime5Text::readTextStyles: the data zone %d seems bad\n", dataId));
    return false;
  }

  dataZone->m_isParsed=true;
  MWAWEntry entry=dataZone->m_entry;
  libmwaw::DebugFile &ascFile=dataZone->ascii();
  libmwaw::DebugStream f;
  f << "Entries(TextStyle)[" << *dataZone << "]:";
  ascFile.addPos(entry.end());
  ascFile.addNote("_");
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());

  int N=int(decal.size());
  MWAWInputStreamPtr input=dataZone->getInput();
  input->setReadInverted(!dataZone->m_hiLoEndian);
  long debPos=entry.begin();
  long endPos=entry.end();
  if (N==0) {
    MWAW_DEBUG_MSG(("RagTime5Text::readTextStyles: can not find decal list for zone %d, let try to continue\n", dataId));
    input->seek(debPos, librevenge::RVNG_SEEK_SET);
    int n=0;
    while (input->tell()+14 < endPos) {
      long pos=input->tell();
      if (!readTextStyle(*dataZone, endPos, n++)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        break;
      }
    }
    if (input->tell()!=endPos) {
      static bool first=true;
      if (first) {
        MWAW_DEBUG_MSG(("RagTime5Text::readTextStyles: can not read some block\n"));
        first=false;
      }
      ascFile.addPos(debPos);
      ascFile.addNote("###");
    }
  }
  else {
    for (int i=0; i<N-1; ++i) {
      long pos=decal[size_t(i)];
      long nextPos=decal[size_t(i+1)];
      if (pos==nextPos) continue;
      if (pos<0 || debPos+pos>endPos) {
        MWAW_DEBUG_MSG(("RagTime5Text::readTextStyles: can not read the data zone %d-%d seems bad\n", dataId, i));
        continue;
      }
      input->seek(debPos+pos, librevenge::RVNG_SEEK_SET);
      readTextStyle(*dataZone, debPos+nextPos, i);
      if (input->tell()!=debPos+nextPos) {
        static bool first=true;
        if (first) {
          MWAW_DEBUG_MSG(("RagTime5Text::readTextStyles: can not read some block\n"));
          first=false;
        }
        ascFile.addPos(debPos+pos);
        ascFile.addNote("###");
      }
    }
  }
  return true;
}

bool RagTime5Text::readTextStyle(RagTime5StructManager::Zone &zone, long endPos, int n)
{
  MWAWInputStreamPtr input=zone.getInput();
  long pos=input->tell();
  if (pos+14>endPos) return false;
  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  f << "TextStyle-S" << n+1 << "[A]:";
  int val=(int) input->readLong(4);
  if (val!=1) f << "numUsed=" << val << ",";
  f << "f1=" << std::hex << input->readULong(2) << std::dec << ",";
  val=(int) input->readLong(2); // sometimes form an increasing sequence but not always
  if (val!=n+1) f << "id=" << val << ",";
  f << "type=" << std::hex << input->readULong(4) << std::dec << ","; // 0 or 0x14[5-b][0-f][08]42 or 17d5042
  val=(int) input->readLong(2); // small number
  if (val) f << "f3=" << val << ",";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  int m=0;
  while (!input->isEnd()) {
    pos=input->tell();
    if (pos>=endPos) break;
    RagTime5StructManager::Field field;
    if (!m_structManager->readField(zone, endPos, field)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f.str("");
    f << "TextStyle-A" << n << "-B" << ++m << ":" << field;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
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
