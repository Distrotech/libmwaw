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
#include <map>
#include <sstream>

#include <libwpd/libwpd.h>

#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWPictBasic.hxx"
#include "MWAWPictMac.hxx"

#include "CWParser.hxx"
#include "CWStruct.hxx"

#include "CWPresentation.hxx"

/** Internal: the structures of a CWPresentation */
namespace CWPresentationInternal
{
//! Internal the presentation
struct Presentation : public CWStruct::DSET {
  // constructor
  Presentation(CWStruct::DSET const dset = CWStruct::DSET()) :
    CWStruct::DSET(dset), m_zoneIdList() {
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Presentation const &doc) {
    o << static_cast<CWStruct::DSET const &>(doc);
    return o;
  }

  //! the list of main zone id
  std::vector<int> m_zoneIdList;
};

//! Internal: the state of a CWPresentation
struct State {
  //! constructor
  State() : m_presentationMap() {
  }

  std::map<int, shared_ptr<Presentation> > m_presentationMap;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
CWPresentation::CWPresentation
(MWAWInputStreamPtr ip, CWParser &parser, MWAWFontConverterPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert), m_state(new CWPresentationInternal::State),
  m_mainParser(&parser), m_asciiFile(parser.ascii())
{
}

CWPresentation::~CWPresentation()
{ }

int CWPresentation::version() const
{
  return m_mainParser->version();
}

int CWPresentation::numPages() const
{
  if (!m_mainParser->getHeader() ||
      m_mainParser->getHeader()->getKind()!=MWAWDocument::K_PRESENTATION ||
      m_state->m_presentationMap.find(1) == m_state->m_presentationMap.end())
    return 1;
  return int(m_state->m_presentationMap.find(1)->second->m_zoneIdList.size());
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////
std::vector<int> CWPresentation::getSlidesList() const
{
  std::vector<int> res;
  std::map<int, shared_ptr<CWPresentationInternal::Presentation> >::const_iterator it =
    m_state->m_presentationMap.begin();
  while(it != m_state->m_presentationMap.end()) {
    shared_ptr<CWPresentationInternal::Presentation> pres = it++->second;
    if (!pres) continue;
    for (size_t c = 0; c < pres->m_otherChilds.size(); c++)
      res.push_back(pres->m_otherChilds[c]);
  }
  return res;
}

////////////////////////////////////////////////////////////
// a document part
////////////////////////////////////////////////////////////
shared_ptr<CWStruct::DSET> CWPresentation::readPresentationZone
(CWStruct::DSET const &zone, MWAWEntry const &entry, bool &complete)
{
  complete = true;
  if (!entry.valid() || zone.m_fileType != 5 || entry.length() < 0x40)
    return shared_ptr<CWStruct::DSET>();
  long pos = entry.begin();
  m_input->seek(pos+8+16, WPX_SEEK_SET); // avoid header+8 generic number
  libmwaw::DebugStream f;
  shared_ptr<CWPresentationInternal::Presentation>
  presentationZone(new CWPresentationInternal::Presentation(zone));

  f << "Entries(PresentationDef):" << *presentationZone << ",";
  ascii().addDelimiter(m_input->tell(), '|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  // read the last part
  long data0Length = zone.m_dataSz;
  long N = zone.m_numData;
  if (entry.length() -8-12 != data0Length*N + zone.m_headerSz) {
    if (data0Length == 0 && N) {
      MWAW_DEBUG_MSG(("CWPresentation::readPresentationZone: can not find definition size\n"));
      m_input->seek(entry.end(), WPX_SEEK_SET);
      return shared_ptr<CWStruct::DSET>();
    }

    MWAW_DEBUG_MSG(("CWPresentation::readPresentationZone: unexpected size for zone definition, try to continue\n"));
  }

  if (m_state->m_presentationMap.find(presentationZone->m_id) != m_state->m_presentationMap.end()) {
    MWAW_DEBUG_MSG(("CWPresentation::readPresentationZone: zone %d already exists!!!\n", presentationZone->m_id));
  } else
    m_state->m_presentationMap[presentationZone->m_id] = presentationZone;

  long dataEnd = entry.end()-N*data0Length;
  m_input->seek(dataEnd, WPX_SEEK_SET);
  for (int i = 0; i < N; i++) {
    pos = m_input->tell();

    f.str("");
    f << "PresentationDef-" << i;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    m_input->seek(pos+data0Length, WPX_SEEK_SET);
  }
  m_input->seek(entry.end(), WPX_SEEK_SET);

  pos = m_input->tell();
  bool ok = readZone1(*presentationZone);
  if (ok) {
    pos = m_input->tell();
    ok = readZone2(*presentationZone);
  }
  if (!ok)
    m_input->seek(pos, WPX_SEEK_SET);

  return presentationZone;
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool CWPresentation::readZone1(CWPresentationInternal::Presentation &pres)
{
  long pos, val;
  libmwaw::DebugStream f;

  for (int st = 0; st < 3; st++) {
    pos = m_input->tell();
    long N = (long) m_input->readULong(4);
    long endPos = pos+16*N+4;
    m_input->seek(endPos, WPX_SEEK_SET);
    if (N < 0 || long(m_input->tell()) != endPos) {
      m_input->seek(pos, WPX_SEEK_SET);
      MWAW_DEBUG_MSG(("CWPresentation::readZone1: zone seems too short\n"));
      return false;
    }
    f.str("");
    f << "Entries(PresentationStr)[" << st << "]" << ":N=" << N << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());

    m_input->seek(pos+4, WPX_SEEK_SET);
    for (int i = 0; i < N; i++) {
      f.str("");
      f << "PresentationStr" << st << "-" << i << ":";
      pos = m_input->tell();
      int zoneId = (int) m_input->readLong(4);
      if (zoneId > 0) {
        if (st == 1)
          pres.m_zoneIdList.push_back(zoneId);
        pres.m_otherChilds.push_back(zoneId);
      } else
        f << "###";
      f << "zId=" << zoneId << ",";
      f << "f1=" << m_input->readLong(4) << ","; // always 8 ?
      int sSz = (int) m_input->readLong(4);
      m_input->seek(pos+16+sSz, WPX_SEEK_SET);
      if (sSz < 0 || m_input->tell() != pos+16+sSz) {
        m_input->seek(pos, WPX_SEEK_SET);
        MWAW_DEBUG_MSG(("CWPresentation::readZone1: can not read string %d\n", i));
        return false;
      }
      m_input->seek(pos+12, WPX_SEEK_SET);
      std::string name("");
      for (int s = 0; s < sSz; s++)
        name += (char) m_input->readULong(1);
      f << name << ",";
      val = m_input->readLong(4); // always 0 ?
      if (val)
        f << "f2=" << val << ",";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
    }
  }

  return true;
}

bool CWPresentation::readZone2(CWPresentationInternal::Presentation &/*pres*/)
{
  libmwaw::DebugStream f;

  long pos = m_input->tell();
  long endPos = pos+16;
  m_input->seek(endPos, WPX_SEEK_SET);
  if (long(m_input->tell()) != endPos) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWPresentation::readZone2: zone seems too short\n"));
    return false;
  }

  m_input->seek(pos, WPX_SEEK_SET);
  f << "Entries(PresentationTitle):";
  long val;
  // checkme this also be 1 times : [ 0, f2] or 1, str0, f2, or a mixt
  for (int i = 0; i < 3; i++) { // find f0=1, f1=0, f2=[0|1|2|4]
    val = m_input->readLong(4);
    if (val)
      f << "f" << i << "=" << val << ",";
  }
  int sSz = (int) m_input->readLong(4);
  m_input->seek(pos+16+sSz, WPX_SEEK_SET);
  if (sSz < 0 || m_input->tell() != pos+16+sSz) {
    m_input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWPresentation::readZone2: can not read title\n"));
    return false;
  }
  m_input->seek(pos+16, WPX_SEEK_SET);
  std::string title("");
  for (int s = 0; s < sSz; s++)
    title += (char) m_input->readULong(1);
  f << title << ",";

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

bool CWPresentation::sendZone(int number)
{
  std::map<int, shared_ptr<CWPresentationInternal::Presentation> >::iterator iter
    = m_state->m_presentationMap.find(number);
  if (iter == m_state->m_presentationMap.end())
    return false;
  shared_ptr<CWPresentationInternal::Presentation> presentation = iter->second;
  if (!presentation || !m_listener)
    return true;
  presentation->m_parsed = true;
  if (presentation->okChildId(number+1))
    m_mainParser->forceParsed(number+1);
  bool main = number == 1;
  int actPage = 1;
  for (size_t p = 0; p < presentation->m_zoneIdList.size(); p++) {
    if (main) m_mainParser->newPage(actPage++);
    int id = presentation->m_zoneIdList[p];
    if (id > 0 && presentation->okChildId(id))
      m_mainParser->sendZone(id);
  }
  return true;
}

void CWPresentation::flushExtra()
{
  std::map<int, shared_ptr<CWPresentationInternal::Presentation> >::iterator iter
    = m_state->m_presentationMap.begin();
  for ( ; iter !=  m_state->m_presentationMap.end(); iter++) {
    shared_ptr<CWPresentationInternal::Presentation> presentation = iter->second;
    if (presentation->m_parsed)
      continue;
    if (m_listener) m_listener->insertEOL();
    sendZone(iter->first);
  }
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
