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
#include "MWAWDebug.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSubDocument.hxx"

#include "MRWParser.hxx"

#include "MRWText.hxx"

/** Internal: the structures of a MRWText */
namespace MRWTextInternal
{

////////////////////////////////////////
//! Internal: the state of a MRWText
struct State {
  //! constructor
  State() : m_version(-1), m_numPages(-1), m_actualPage(0) {
  }

  //! the file version
  mutable int m_version;
  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;
};

////////////////////////////////////////
//! Internal: the subdocument of a MRWText
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(MRWText &pars, MWAWInputStreamPtr input, int id, libmwaw::SubDocumentType type) :
    MWAWSubDocument(pars.m_mainParser, input, MWAWEntry()), m_textParser(&pars), m_id(id), m_type(type) {}

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
  /** the text parser */
  MRWText *m_textParser;
  //! the subdocument id
  int m_id;
  //! the subdocument type
  libmwaw::SubDocumentType m_type;
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
  MRWContentListener *listen = dynamic_cast<MRWContentListener *>(listener.get());
  if (!listen) {
    MWAW_DEBUG_MSG(("SubDocument::parse: bad listener\n"));
    return;
  }

  assert(m_textParser);

  long pos = m_input->tell();
  m_input->seek(pos, WPX_SEEK_SET);
}

bool SubDocument::operator!=(MWAWSubDocument const &doc) const
{
  if (MWAWSubDocument::operator!=(doc)) return true;
  SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
  if (!sDoc) return true;
  if (m_textParser != sDoc->m_textParser) return true;
  if (m_id != sDoc->m_id) return true;
  if (m_type != sDoc->m_type) return true;
  return false;
}
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MRWText::MRWText(MWAWInputStreamPtr ip, MRWParser &parser, MWAWFontConverterPtr &convert) :
  m_input(ip), m_listener(), m_convertissor(convert),
  m_state(new MRWTextInternal::State), m_mainParser(&parser), m_asciiFile(parser.ascii())
{
}

MRWText::~MRWText()
{
}

int MRWText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_mainParser->version();
  return m_state->m_version;
}

int MRWText::numPages() const
{
  return m_state->m_numPages;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
//     Text
////////////////////////////////////////////////////////////
bool MRWText::readTextZone(MRWEntry const &entry, int)
{
  if (entry.length() < 0x3) {
    MWAW_DEBUG_MSG(("MRWText::readTextZone: data seems to short\n"));
    return false;
  }

  m_input->seek(entry.begin(), WPX_SEEK_SET);
  m_input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  m_mainParser->decodeZone(dataList);
  m_input->popLimit();

  if (dataList.size() != 1) {
    MWAW_DEBUG_MSG(("MRWText::readTextZone: can find my data\n"));
    return false;
  }

  libmwaw::DebugStream f;
  f << entry.name() << "[data]:";
  MRWStruct const &data = dataList[0];
  if (data.m_type) {
    MWAW_DEBUG_MSG(("MRWText::readTextZone: find unexpected type zone\n"));
    return false;
  }

  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  if (data.m_pos.length() <= 0)
    return true;

  long pos = data.m_pos.begin();
  long endPos = data.m_pos.end();
  m_input->seek(pos, WPX_SEEK_SET);

  f.str("");
  f << entry.name() << ":";
  while (!m_input->atEOS()) {
    char c = (char) m_input->readULong(1);
    f << c;
    long actPos = m_input->tell();
    if (c==0xa || c==0xd || actPos==endPos) {
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());

      pos = actPos;
      f.str("");
      f << entry.name() << ":";
    }
    if (actPos==endPos)
      break;
  }

  m_input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

bool MRWText::readPLCZone(MRWEntry const &entry, int)
{
  if (entry.length() < 2*entry.m_N-1) {
    MWAW_DEBUG_MSG(("MRWText::readPLCZone: data seems to short\n"));
    return false;
  }

  m_input->seek(entry.begin(), WPX_SEEK_SET);
  m_input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  m_mainParser->decodeZone(dataList,1+2*entry.m_N);
  m_input->popLimit();

  if (int(dataList.size()) != 2*entry.m_N) {
    MWAW_DEBUG_MSG(("MRWText::readPLCZone: find unexpected number of data\n"));
    return false;
  }

  libmwaw::DebugStream f;
  for (size_t d=0; d < dataList.size(); d+=2) {
    f.str("");
    f << entry.name() << "-" << d/2 << ":";
    f << std::hex << dataList[d].value(0) << std::dec << ":"; //pos
    f << dataList[d+1].value(0); // id

    ascii().addPos(dataList[d].m_filePos);
    ascii().addNote(f.str().c_str());
  }
  m_input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
//     Fonts
////////////////////////////////////////////////////////////
bool MRWText::readFontNames(MRWEntry const &entry, int)
{
  if (entry.length() < entry.m_N) {
    MWAW_DEBUG_MSG(("MRWText::readFontNames: data seems to short\n"));
    return false;
  }

  m_input->seek(entry.begin(), WPX_SEEK_SET);
  m_input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  m_mainParser->decodeZone(dataList,1+19*entry.m_N);
  m_input->popLimit();

  if (int(dataList.size()) != 19*entry.m_N) {
    MWAW_DEBUG_MSG(("MRWText::readFontNames: find unexpected number of data\n"));
    return false;
  }

  libmwaw::DebugStream f;
  size_t d = 0;
  int val;
  for (int i = 0; i < entry.m_N; i++) {
    f.str("");
    f << entry.name() << "-" << i << ":";
    ascii().addPos(dataList[d].m_filePos);
    std::string fontName("");
    for (int j = 0; j < 2; j++, d++) {
      MRWStruct const &data = dataList[d];
      if (data.m_type!=0 || !data.m_pos.valid()) {
        MWAW_DEBUG_MSG(("MRWText::readFontNames: name %d seems bad\n", j));
        f << "###" << data << ",";
        continue;
      }
      long pos = data.m_pos.begin();
      m_input->seek(pos, WPX_SEEK_SET);
      int fSz = int(m_input->readULong(1));
      if (fSz+1 > data.m_pos.length()) {
        MWAW_DEBUG_MSG(("MRWText::readFontNames: field name %d seems bad\n", j));
        f << data << "[###fSz=" << fSz << ",";
        continue;
      }
      std::string name("");
      for (int c = 0; c < fSz; c++)
        name+=(char) m_input->readULong(1);
      if (j == 0) {
        fontName = name;
        f << name << ",";
      } else
        f << "nFont=" << name << ",";
    }
    val = (int) dataList[d++].value(0);
    if (val != 4) // always 4
      f << "f0=" << val << ",";
    val = (int) dataList[d++].value(0);
    if (val) // always 0
      f << "f1=" << val << ",";
    int fId = (int) (uint16_t) dataList[d++].value(0);
    f << "fId=" << fId << ",";
    for (int j = 5; j < 19; j++) { // f14=1,f15=0|3
      MRWStruct const &data = dataList[d++];
      if (data.m_type==0 || data.numValues() > 1)
        f << "f" << j-3 << "=" << data << ",";
      else if (data.value(0))
        f << "f" << j-3 << "=" << data.value(0) << ",";
    }
    if (fontName.length() && fId)
      m_convertissor->setCorrespondance(fId, fontName);
    ascii().addNote(f.str().c_str());
  }
  m_input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

bool MRWText::readFonts(MRWEntry const &entry, int)
{
  if (entry.length() < entry.m_N+1) {
    MWAW_DEBUG_MSG(("MRWText::readFonts: data seems to short\n"));
    return false;
  }

  m_input->seek(entry.begin(), WPX_SEEK_SET);
  m_input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  m_mainParser->decodeZone(dataList,1+1+77*entry.m_N);
  m_input->popLimit();

  if (int(dataList.size()) != 1+77*entry.m_N) {
    MWAW_DEBUG_MSG(("MRWText::readFonts: find unexpected number of data\n"));
    return false;
  }

  libmwaw::DebugStream f;
  f << entry.name() << ":unkn=" << dataList[0].value(0);
  ascii().addPos(dataList[0].m_filePos);
  ascii().addNote(f.str().c_str());

  size_t d = 1;
  for (int i = 0; i < entry.m_N; i++) {
    f.str("");
    f << entry.name() << "-" << i << ":";
    ascii().addPos(dataList[d].m_filePos);

    for (int j = 0; j < 77; j++, d++)
      f << dataList[d] << ",";
    ascii().addNote(f.str().c_str());
  }
  m_input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
//     Style
////////////////////////////////////////////////////////////
bool MRWText::readStyleNames(MRWEntry const &entry, int)
{
  if (entry.length() < entry.m_N) {
    MWAW_DEBUG_MSG(("MRWText::readStyleNames: data seems to short\n"));
    return false;
  }

  m_input->seek(entry.begin(), WPX_SEEK_SET);
  m_input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  m_mainParser->decodeZone(dataList,1+2*entry.m_N);
  m_input->popLimit();

  if (int(dataList.size()) != 2*entry.m_N) {
    MWAW_DEBUG_MSG(("MRWText::readStyleNames: find unexpected number of data\n"));
    return false;
  }

  libmwaw::DebugStream f;
  size_t d = 0;
  for (int i = 0; i < entry.m_N; i++) {
    f.str("");
    f << entry.name() << "-" << i << ":";
    ascii().addPos(dataList[d].m_filePos);
    if (!dataList[d].isBasic()) {
      MWAW_DEBUG_MSG(("MRWText::readStyleNames: bad id field for style %d\n", i));
      f << "###" << dataList[d] << ",";
    } else
      f << "id=" << dataList[d].value(0) << ",";
    d++;

    std::string name("");
    MRWStruct const &data = dataList[d++];
    if (data.m_type!=0 || !data.m_pos.valid()) {
      MWAW_DEBUG_MSG(("MRWText::readStyleNames: name %d seems bad\n", i));
      f << "###" << data << ",";
    } else {
      long pos = data.m_pos.begin();
      m_input->seek(pos, WPX_SEEK_SET);
      int fSz = int(m_input->readULong(1));
      if (fSz+1 > data.m_pos.length()) {
        MWAW_DEBUG_MSG(("MRWText::readStyleNames: field name %d seems bad\n", i));
        f << data << "[###fSz=" << fSz << ",";
      } else {
        for (int c = 0; c < fSz; c++)
          name+=(char) m_input->readULong(1);
        f << name << ",";
      }
    }
    ascii().addNote(f.str().c_str());
  }
  m_input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
//     Paragraph
////////////////////////////////////////////////////////////
bool MRWText::readRulers(MRWEntry const &entry, int)
{
  if (entry.length() < entry.m_N+1) {
    MWAW_DEBUG_MSG(("MRWText::readRulers: data seems to short\n"));
    return false;
  }

  m_input->seek(entry.begin(), WPX_SEEK_SET);
  m_input->pushLimit(entry.end());
  std::vector<MRWStruct> dataList;
  m_mainParser->decodeZone(dataList,1+3*68*entry.m_N);
  m_input->popLimit();

  int numDatas = int(dataList.size());
  if (numDatas < 68*entry.m_N) {
    MWAW_DEBUG_MSG(("MRWText::readRulers: find unexpected number of data\n"));
    return false;
  }

  libmwaw::DebugStream f;
  size_t d = 0;
  for (int i = 0; i < entry.m_N; i++) {
    f.str("");
    f << entry.name() << "-" << i << ":";
    ascii().addPos(dataList[d].m_filePos);

    if (int(d+68) > numDatas) {
      MWAW_DEBUG_MSG(("MRWText::readRulers: ruler %d is too short\n", i));
      f << "###";
      ascii().addNote(f.str().c_str());
      return true;
    }
    for (int j = 0; j < 57; j++, d++) {
      MRWStruct const &data = dataList[d];
      f << data << ",";
    }
    MRWStruct const &tabData = dataList[d++];
    if (tabData.m_data.size() > 1) {
      MWAW_DEBUG_MSG(("MRWText::readRulers: numtabs as unexpected type\n"));
      f << "###";
      ascii().addNote(f.str().c_str());
      return true;
    }
    int nTabs = (int) tabData.value(0);
    if (nTabs < 0 || int(d)+4*nTabs+10 > numDatas) {
      MWAW_DEBUG_MSG(("MRWText::readRulers: can not read numtabs\n"));
      f << "###";
      ascii().addNote(f.str().c_str());
      return true;
    }
    if (nTabs) {
      for (int j = 0; j < nTabs; j++) {
        f << "tabs" << j << "=[";
        for (int k = 0; k < 4; k++, d++)
          f << dataList[d] << ",";
        f << "],";
      }
    }
    for (int j = 58; j < 68; j++, d++) {
      MRWStruct const &data = dataList[d];
      f << data << ",";
    }
    ascii().addNote(f.str().c_str());
  }
  m_input->seek(entry.end(), WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
//     the token
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
//     the sections
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

//! send data to the listener

void MRWText::flushExtra()
{
  if (!m_listener) return;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
