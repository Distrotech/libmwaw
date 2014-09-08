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
#include <map>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWHeader.hxx"
#include "MWAWParser.hxx"
#include "MWAWPresentationListener.hxx"
#include "MWAWPosition.hxx"

#include "ClarisWksDocument.hxx"
#include "ClarisWksStruct.hxx"
#include "ClarisWksStyleManager.hxx"

#include "ClarisWksPresentation.hxx"

/** Internal: the structures of a ClarisWksPresentation */
namespace ClarisWksPresentationInternal
{
//! Internal the presentation
struct Presentation : public ClarisWksStruct::DSET {
  // constructor
  Presentation(ClarisWksStruct::DSET const &dset = ClarisWksStruct::DSET()) :
    ClarisWksStruct::DSET(dset), m_contentIdList(), m_noteIdList(), m_thumbnailsIdList(), m_masterDetached(false)
  {
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Presentation const &doc)
  {
    o << static_cast<ClarisWksStruct::DSET const &>(doc);
    return o;
  }
  /** remove a child from a list.

      Normally, this function is not called, so optimizing it is not usefull
   */
  virtual void removeChild(int cId, bool normalChild)
  {
    DSET::removeChild(cId, normalChild);
    if (m_id+1==cId) m_masterDetached=true;
  }

  //! the list of main zone id
  std::vector<int> m_contentIdList;
  //! the list of notes zone id
  std::vector<int> m_noteIdList;
  //! the list of thumbnail zone id
  std::vector<int> m_thumbnailsIdList;
  //! true if the auxiliary zone is detached
  bool m_masterDetached;
};

//! Internal: the state of a ClarisWksPresentation
struct State {
  //! constructor
  State() : m_presentation(), m_presentationMap()
  {
  }
  //! the main presentation
  shared_ptr<Presentation> m_presentation;
  //! map zId to presentation
  std::map<int, shared_ptr<Presentation> > m_presentationMap;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
ClarisWksPresentation::ClarisWksPresentation(ClarisWksDocument &document) :
  m_document(document), m_parserState(document.m_parserState), m_state(new ClarisWksPresentationInternal::State),
  m_mainParser(&document.getMainParser())
{
}

ClarisWksPresentation::~ClarisWksPresentation()
{ }

int ClarisWksPresentation::version() const
{
  return m_parserState->m_version;
}

int ClarisWksPresentation::numPages() const
{
  if (m_parserState->m_kind!=MWAWDocument::MWAW_K_PRESENTATION ||
      m_state->m_presentationMap.find(1) == m_state->m_presentationMap.end())
    return 1;
  return int(m_state->m_presentationMap.find(1)->second->m_contentIdList.size());
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////
void ClarisWksPresentation::updateSlideTypes() const
{
  std::map<int, shared_ptr<ClarisWksPresentationInternal::Presentation> >::const_iterator it =
    m_state->m_presentationMap.begin();
  while (it != m_state->m_presentationMap.end()) {
    shared_ptr<ClarisWksPresentationInternal::Presentation> pres = it++->second;
    if (!pres) continue;
    if (pres->m_id==1)
      m_state->m_presentation=pres;
    for (int step=0; step<3; ++step) {
      std::vector<int> const &content=step==0 ? pres->m_contentIdList :
                                      step==1 ? pres->m_noteIdList : pres->m_thumbnailsIdList;
      for (size_t c = 0; c < content.size(); c++) {
        shared_ptr<ClarisWksStruct::DSET> zone=m_document.getZone(content[c]);
        if (!zone) continue;
        static ClarisWksStruct::DSET::Position const positions[]= {
          ClarisWksStruct::DSET::P_Slide, ClarisWksStruct::DSET::P_SlideNote, ClarisWksStruct::DSET::P_SlideThumbnail
        };
        zone->m_position=positions[step];
        zone->m_page=int(c+1);
      }
    }
    // finally update the background type
    shared_ptr<ClarisWksStruct::DSET> zone=m_document.getZone(pres->m_id+1);
    if (!zone) continue;
    zone->m_position=ClarisWksStruct::DSET::P_SlideMaster;
  }
}

void ClarisWksPresentation::disconnectMasterFromContents() const
{
  std::map<int, shared_ptr<ClarisWksPresentationInternal::Presentation> >::const_iterator it =
    m_state->m_presentationMap.begin();
  while (it != m_state->m_presentationMap.end()) {
    shared_ptr<ClarisWksPresentationInternal::Presentation> pres = it++->second;
    if (!pres) continue;
    shared_ptr<ClarisWksStruct::DSET> background=m_document.getZone(pres->m_id+1);
    if (!background || background->m_fathersList.size()!=1) {
      MWAW_DEBUG_MSG(("ClarisWksPresentation::disconnectMasterFromContents: can not find the background zone\n"));
      continue;
    }
    int masterId=*background->m_fathersList.begin();
    shared_ptr<ClarisWksStruct::DSET> master=m_document.getZone(masterId);
    if (!master) {
      MWAW_DEBUG_MSG(("ClarisWksPresentation::disconnectMasterFromContents: can not find the master zone\n"));
      continue;
    }

    for (int step=0; step<3; ++step) {
      std::vector<int> const &content=step==0 ? pres->m_contentIdList :
                                      step==1 ? pres->m_noteIdList : pres->m_thumbnailsIdList;
      for (size_t i=0; i<content.size(); ++i) {
        int childId=content[i];
        if (master->m_fathersList.find(childId)==master->m_fathersList.end()) {
          MWAW_DEBUG_MSG(("ClarisWksPresentation::disconnectMasterFromContents: find a content zone with no link to master\n"));
          continue;
        }
        shared_ptr<ClarisWksStruct::DSET> zone=m_document.getZone(childId);
        if (!zone) continue;
        zone->removeChild(masterId, true);
        master->m_fathersList.erase(childId);
      }
    }
  }
}

////////////////////////////////////////////////////////////
// a document part
////////////////////////////////////////////////////////////
shared_ptr<ClarisWksStruct::DSET> ClarisWksPresentation::readPresentationZone
(ClarisWksStruct::DSET const &zone, MWAWEntry const &entry, bool &complete)
{
  complete = true;
  if (!entry.valid() || zone.m_fileType != 5 || entry.length() < 0x40)
    return shared_ptr<ClarisWksStruct::DSET>();
  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos+8+16, librevenge::RVNG_SEEK_SET); // avoid header+8 generic number
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  shared_ptr<ClarisWksPresentationInternal::Presentation>
  presentationZone(new ClarisWksPresentationInternal::Presentation(zone));

  f << "Entries(PresentationDef):" << *presentationZone << ",";
  ascFile.addDelimiter(input->tell(), '|');
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  // read the last part
  long data0Length = zone.m_dataSz;
  long N = zone.m_numData;
  if (entry.length() -8-12 != data0Length*N + zone.m_headerSz) {
    if (data0Length == 0 && N) {
      MWAW_DEBUG_MSG(("ClarisWksPresentation::readPresentationZone: can not find definition size\n"));
      input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
      return shared_ptr<ClarisWksStruct::DSET>();
    }

    MWAW_DEBUG_MSG(("ClarisWksPresentation::readPresentationZone: unexpected size for zone definition, try to continue\n"));
  }

  if (m_state->m_presentationMap.find(presentationZone->m_id) != m_state->m_presentationMap.end()) {
    MWAW_DEBUG_MSG(("ClarisWksPresentation::readPresentationZone: zone %d already exists!!!\n", presentationZone->m_id));
  }
  else
    m_state->m_presentationMap[presentationZone->m_id] = presentationZone;

  long dataEnd = entry.end()-N*data0Length;
  input->seek(dataEnd, librevenge::RVNG_SEEK_SET);
  for (int i = 0; i < N; i++) {
    pos = input->tell();

    f.str("");
    f << "PresentationDef-" << i;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+data0Length, librevenge::RVNG_SEEK_SET);
  }
  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);

  pos = input->tell();
  bool ok = readZone1(*presentationZone);
  if (ok) {
    pos = input->tell();
    ok = readZone2(*presentationZone);
  }
  if (!ok)
    input->seek(pos, librevenge::RVNG_SEEK_SET);

  return presentationZone;
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool ClarisWksPresentation::readZone1(ClarisWksPresentationInternal::Presentation &pres)
{
  long val;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  for (int st = 0; st < 3; st++) {
    long pos = input->tell();
    long N = (long) input->readULong(4);
    long endPos = pos+16*N+4;
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    if (N < 0 || long(input->tell()) != endPos) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      MWAW_DEBUG_MSG(("ClarisWksPresentation::readZone1: zone seems too short\n"));
      return false;
    }
    f.str("");
    f << "Entries(PresentationStr)[" << st << "]" << ":N=" << N << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    input->seek(pos+4, librevenge::RVNG_SEEK_SET);
    for (int i = 0; i < N; i++) {
      f.str("");
      f << "PresentationStr" << st << "-" << i << ":";
      pos = input->tell();
      int zoneId = (int) input->readLong(4);
      if (zoneId > 0) {
        if (st == 1)
          pres.m_contentIdList.push_back(zoneId);
        else if (st == 2)
          pres.m_noteIdList.push_back(zoneId);
        else
          pres.m_thumbnailsIdList.push_back(zoneId);
        pres.m_otherChilds.push_back(zoneId);
      }
      else
        f << "###";
      f << "zId=" << zoneId << ",";
      f << "f1=" << input->readLong(4) << ","; // always 8 ?
      int sSz = (int) input->readLong(4);
      input->seek(pos+16+sSz, librevenge::RVNG_SEEK_SET);
      if (sSz < 0 || input->tell() != pos+16+sSz) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        MWAW_DEBUG_MSG(("ClarisWksPresentation::readZone1: can not read string %d\n", i));
        return false;
      }
      input->seek(pos+12, librevenge::RVNG_SEEK_SET);
      std::string name("");
      for (int s = 0; s < sSz; s++)
        name += (char) input->readULong(1);
      f << name << ",";
      val = input->readLong(4); // always 0 ?
      if (val)
        f << "f2=" << val << ",";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
  }

  return true;
}

bool ClarisWksPresentation::readZone2(ClarisWksPresentationInternal::Presentation &/*pres*/)
{
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long endPos = pos+16;
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != endPos) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksPresentation::readZone2: zone seems too short\n"));
    return false;
  }

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "Entries(PresentationTitle):";
  // checkme this also be 1 times : [ 0, f2] or 1, str0, f2, or a mixt
  for (int i = 0; i < 3; i++) { // find f0=1, f1=0, f2=[0|1|2|4]
    long val = input->readLong(4);
    if (val)
      f << "f" << i << "=" << val << ",";
  }
  int sSz = (int) input->readLong(4);
  input->seek(pos+16+sSz, librevenge::RVNG_SEEK_SET);
  if (sSz < 0 || input->tell() != pos+16+sSz) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksPresentation::readZone2: can not read title\n"));
    return false;
  }
  input->seek(pos+16, librevenge::RVNG_SEEK_SET);
  std::string title("");
  for (int s = 0; s < sSz; s++)
    title += (char) input->readULong(1);
  f << title << ",";

  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

bool ClarisWksPresentation::sendMaster()
{
  if (!m_state->m_presentation) {
    MWAW_DEBUG_MSG(("ClarisWksPresentation::sendMaster: oops can not find main presentation\n"));
    return false;
  }

  return  m_document.sendZone(2);
}

bool ClarisWksPresentation::sendZone(int number)
{
  if (number != 1) {
    MWAW_DEBUG_MSG(("ClarisWksPresentation::sendZone: sending embeded presentation is not implemented\n"));
    return false;
  }
  shared_ptr<ClarisWksPresentationInternal::Presentation> presentation = m_state->m_presentation;
  MWAWPresentationListenerPtr listener=m_parserState->m_presentationListener;
  if (!presentation || !listener) {
    MWAW_DEBUG_MSG(("ClarisWksPresentation::sendZone: can not find the presentation listener\n"));
    return false;
  }
  presentation->m_parsed = true;
  if (!presentation->m_masterDetached)
    m_document.forceParsed(number+1);
  for (size_t p = 0; p < presentation->m_contentIdList.size(); p++) {
    if (p)
      listener->insertBreak(MWAWListener::PageBreak);
    int id = presentation->m_contentIdList[p];
    if (id > 0)
      m_document.sendZone(id);
    if (p>=presentation->m_noteIdList.size())
      continue;
    id = presentation->m_noteIdList[p];
    if (id > 0)
      m_document.sendZone(id);
  }
  return true;
}

void ClarisWksPresentation::flushExtra()
{
  shared_ptr<MWAWListener> listener=m_parserState->getMainListener();
  if (!listener || listener->getType()!=MWAWListener::Presentation) return;
  std::map<int, shared_ptr<ClarisWksPresentationInternal::Presentation> >::iterator iter
    = m_state->m_presentationMap.begin();
  for (; iter !=  m_state->m_presentationMap.end(); ++iter) {
    shared_ptr<ClarisWksPresentationInternal::Presentation> presentation = iter->second;
    if (presentation->m_parsed)
      continue;
    listener->insertEOL();
    sendZone(iter->first);
  }
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
