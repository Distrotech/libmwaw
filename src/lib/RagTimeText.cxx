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
#include <set>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWTextListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"

#include "RagTimeParser.hxx"

#include "RagTimeText.hxx"

/** Internal: the structures of a RagTimeText */
namespace RagTimeTextInternal
{
//! Internal: a text's zone of a RagTimeText
struct TextZone {
  //! constructor
  TextZone() : m_textPos(), m_fontPosList(), m_fontList(), m_paragraphPosList(), m_paragraphList()
  {
  }
  //! the text zone
  MWAWEntry m_textPos;
  //! the beginning of character properties in the text zone
  std::vector<long> m_fontPosList;
  //! the list of character's properties
  std::vector<MWAWFont> m_fontList;
  //! the beginning of paragraph properties in the text zone
  std::vector<long> m_paragraphPosList;
  //! the list of paragraph's properties
  std::vector<MWAWParagraph> m_paragraphList;
};

////////////////////////////////////////
//! Internal: the state of a RagTimeText
struct State {
  //! constructor
  State() : m_version(-1), m_localFIdMap()
  {
  }

  //! return a mac font id corresponding to a local id
  int getFontId(int localId) const
  {
    if (m_localFIdMap.find(localId)==m_localFIdMap.end())
      return localId;
    return m_localFIdMap.find(localId)->second;
  }

  //! the file version
  mutable int m_version;
  //! a map local fontId->fontId
  std::map<int, int> m_localFIdMap;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
RagTimeText::RagTimeText(RagTimeParser &parser) :
  m_parserState(parser.getParserState()), m_state(new RagTimeTextInternal::State), m_mainParser(&parser)
{
}

RagTimeText::~RagTimeText()
{ }

int RagTimeText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}

////////////////////////////////////////////////////////////
// rsrc zone: fonts
////////////////////////////////////////////////////////////
bool RagTimeText::readFontNames(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2+0x26)) {
    MWAW_DEBUG_MSG(("RagTimeText::readFontNames: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(" << entry.type() << ")[" << entry.id() << "]:";
  int dSz=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  int headerSz=(int) input->readULong(2);
  int fSz=(int) input->readULong(2);
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  if (headerSz<0x20 || fSz<0x10 || dSz<headerSz+(N+1)*fSz || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("RagTimeText::readFontNames: the size seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  input->seek(pos+2+headerSz, librevenge::RVNG_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  std::set<long> posSet;
  std::map<int, long> fontIdPosMap;
  posSet.insert(endPos);
  for (int i=0; i<=N; ++i) {
    pos=input->tell();
    f.str("");
    f << entry.type() << "-" << i << ":";
    int val=(int) input->readLong(2); // small number
    if (val) f << "f0=" << val << ",";
    val=(int) input->readLong(2); // always 0?
    if (val) f << "f1=" << val << ",";
    int fPos=(int) input->readULong(2);
    f << "pos[name]=" << std::hex << entry.begin()+2+fPos << std::dec << ",";
    posSet.insert(entry.begin()+2+fPos);
    int fId=(int) input->readLong(2);
    if (fId) f << "fId=" << fId << ",";
    fontIdPosMap[fId]=entry.begin()+2+fPos;
    ascFile.addDelimiter(input->tell(),'|');
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  for (std::map<int, long>::const_iterator it=fontIdPosMap.begin(); it!=fontIdPosMap.end(); ++it) {
    pos=it->second;
    int fId=it->first;
    if (pos>=endPos) continue;
    std::set<long>::const_iterator pIt=posSet.find(pos);
    f.str("");
    f << entry.type() << "[name]:id=" << fId << ",";
    if (pIt==posSet.end()|| ++pIt==posSet.end()) {
      MWAW_DEBUG_MSG(("RagTimeText::readFontNames: can not find the end name position\n"));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    std::string name("");
    long nextPos=*pIt;
    while (!input->isEnd() && input->tell()<nextPos) {
      char c=(char) input->readULong(1);
      if (c=='\0') break;
      name+=c;
    }
    f << name;
    // ok, let update the conversion map
    m_state->m_localFIdMap[fId]=m_parserState->m_fontConverter->getId(name);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read a zone of text
////////////////////////////////////////////////////////////
bool RagTimeText::readTextZone(MWAWEntry &entry)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  int const vers=version();
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+5+2+2+6)) {
    MWAW_DEBUG_MSG(("RagTimeText::readTextZone: the position seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(TextZone):";
  int dSz=(int) input->readULong(2);
  int numChar=(int) input->readULong(2);
  long endPos=pos+2+dSz;
  f << "N=" << numChar << ",";
  if (!input->checkPosition(endPos) || numChar>dSz) {
    MWAW_DEBUG_MSG(("RagTimeText::readTextZone: the numChar seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  int val=(int) input->readLong(1); // always 0?
  if (val) f << "g0=" << val << ",";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(endPos);
  ascFile.addNote("_");

  RagTimeTextInternal::TextZone zone;
  pos = input->tell();
  f.str("");
  f << "TextZone[text]:";
  zone.m_textPos.setBegin(pos);
  zone.m_textPos.setLength(numChar);
  std::string text("");
  // 01: date, 02: time, ...
  for (int i=0; i<numChar-1; ++i) text+=(char) input->readULong(1);
  f << text << ",";
  if (vers>=2 && (numChar%2)==1)
    input->seek(1, librevenge::RVNG_SEEK_CUR);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  if (!readFonts(zone, endPos) || !readParagraphs(zone, endPos))
    return false;

  pos=input->tell();
  if (vers==1) {
    if (pos!=endPos) {
      MWAW_DEBUG_MSG(("RagTimeText::readTextZone: find some extra data\n"));
      ascFile.addPos(pos);
      ascFile.addNote("TextZone[end]:###");
    }
    return true;
  }
  dSz=(int) input->readULong(2);
  f.str("");
  f << "TextZone[A]:";
  if (pos+2+dSz+2>endPos) {
    MWAW_DEBUG_MSG(("RagTimeText::readTextZone: the zoneA size seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  if (dSz==0) {
    ascFile.addPos(pos);
    ascFile.addNote("_");
  }
  else {
    // never seems
    MWAW_DEBUG_MSG(("RagTimeText::readTextZone: find a zoneA zone\n"));
    f << "#";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  // now the token?
  pos=input->tell();
  dSz=(int) input->readULong(2);
  f.str("");
  f << "TextZone[B]:";
  if (pos+2+dSz!=endPos) {
    MWAW_DEBUG_MSG(("RagTimeText::readTextZone: the zoneB size seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  if (dSz==0) {
    ascFile.addPos(pos);
    ascFile.addNote("_");
    return true;
  }

  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  int n=0;
  while (!input->isEnd()) {
    // find either: "," or format?=[2,id=[1-19],1,1,sSz,0],"text"
    pos=input->tell();
    if (pos>=endPos) break;
    f.str("");
    f << "TextZone[B" << n++ << "],";
    dSz=(int) input->readULong(2);
    long fEndPos=pos+dSz;
    if (dSz<3 || fEndPos>endPos) {
      MWAW_DEBUG_MSG(("RagTimeText::readTextZone: the zoneB size seems bad\n"));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      return true;
    }
    int sSz=(int) input->readULong(1);
    if (sSz+3!=dSz && dSz<14) {
      MWAW_DEBUG_MSG(("RagTimeText::readTextZone: can not find the zoneB format\n"));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      input->seek(fEndPos, librevenge::RVNG_SEEK_SET);
      continue;
    }
    if (sSz+3!=dSz) {
      sSz=dSz-14;
      input->seek(pos+2, librevenge::RVNG_SEEK_SET);
      f << "format?=[";
      for (int i=0; i< 6; ++i) { // small number
        val=(int) input->readLong(2);
        if (val) f << val << ",";
        else f << "_";
      }
      f << "],";
    }
    text="";
    for (int i=0; i<sSz; ++i)
      text+=(char) input->readULong(1);
    f << "\"" << text << "\",";
    input->seek(fEndPos, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool RagTimeText::readFonts(RagTimeTextInternal::TextZone &zone, long endPos)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  int const vers=version();
  long pos=input->tell();

  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(TextChar):";
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  int const fSz=vers>=2 ? 10:8;
  if (pos+2+fSz*N>endPos+2+4) {
    MWAW_DEBUG_MSG(("RagTimeText::readFonts: the number of styles seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  for (int i=0; i < N; ++i) {
    pos = input->tell();
    f.str("");
    f << "TextChar-C" << i << ":";
    long textPos=(long) input->readULong(2);
    f << "pos=" << textPos << ",";
    if (vers>=2) {
      // TODO
      input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    MWAWFont font;
    int size= (int) input->readULong(1);
    int flag = (int) input->readULong(1);
    uint32_t flags=0;
    if (flag&0x1) flags |= MWAWFont::boldBit;
    if (flag&0x2) flags |= MWAWFont::italicBit;
    if (flag&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (flag&0x8) flags |= MWAWFont::embossBit;
    if (flag&0x10) flags |= MWAWFont::shadowBit;
    if (flag&0x20) font.setDeltaLetterSpacing(-1);
    if (flag&0x40) font.setDeltaLetterSpacing(1);
    if (flag&0x80) font.set(MWAWFont::Script::super100());
    if (size&0x80) {
      font.set(MWAWFont::Script::sub100());
      size&=0x7f;
    }
    font.setSize((float)size);
    font.setFlags(flags);
    font.setId(m_state->getFontId((int) input->readULong(2)));
    int val=(int) input->readLong(1);
    if (val) font.set(MWAWFont::Script(float(val)));
    val=(int) input->readLong(1);
    if (val) font.setDeltaLetterSpacing(float(val)/16.0f);
    zone.m_fontPosList.push_back(textPos);
    zone.m_fontList.push_back(font);
    f << font.getDebugString(m_parserState->m_fontConverter);

    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool RagTimeText::readParagraphs(RagTimeTextInternal::TextZone &zone, long endPos)
{
  MWAWInputStreamPtr input = m_parserState->m_input;
  int const vers=version();
  long pos=input->tell();

  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  f << "Entries(TextPara):";
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  int const paraSz=vers>=2 ? 48 : 34;
  if (pos+2+paraSz*N>endPos) {
    MWAW_DEBUG_MSG(("RagTimeText::readParagraphs: the number of paragrphs seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  for (int i=0; i < N; ++i) {
    pos = input->tell();
    f.str("");
    f << "TextPara-P" << i << ":";
    long textPos=(long) input->readULong(2);
    f << "pos=" << textPos << ",";
    if (vers>=2) {
      //TODO:
      input->seek(pos+paraSz, librevenge::RVNG_SEEK_SET);
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      continue;
    }
    MWAWParagraph para;
    para.m_marginsUnit=librevenge::RVNG_POINT;
    para.m_margins[1]=(double)input->readLong(2);
    // FIXME: margins from left...
    para.m_margins[2]=(double)input->readULong(2);
    int align=(int) input->readULong(1);
    switch (align) {
    case 0: // left
      break;
    case 1:
      para.m_justify = MWAWParagraph::JustificationCenter;
      break;
    case 2:
      para.m_justify = MWAWParagraph::JustificationRight;
      break;
    case 3:
      para.m_justify = MWAWParagraph::JustificationFull;
      break;
    default:
      MWAW_DEBUG_MSG(("RagTimeText::readParagraphs: find unknown align value\n"));
      f << "###align=" << align << ",";
      break;
    }
    int numTabs=(int) input->readULong(1);
    if (numTabs>10) {
      MWAW_DEBUG_MSG(("RagTimeText::readParagraphs: the number of tabs seems odd\n"));
      f << "###tabs[num]=" << numTabs << ",";
      numTabs=0;
    }
    int interlinePoint=(int) input->readLong(1);
    int interline=(int) input->readULong(1);
    if (interline & 0xF8)
      f << "interline[high]=" << std::hex << (interline & 0xFC) << std::dec << ",";
    interline &= 0x7;
    switch (interline) {
    case 0:
    case 1:
    case 2:
      para.setInterline(1.+interline*0.5, librevenge::RVNG_PERCENT);
      break;
    case 3: // 1line +/- nbPt
      para.setInterline(1.+interlinePoint/12., librevenge::RVNG_PERCENT, MWAWParagraph::AtLeast);
      break;
    case 4:
      para.setInterline(interlinePoint, librevenge::RVNG_POINT);
      break;
    default:
      MWAW_DEBUG_MSG(("RagTimeText::readParagraphs: unknown interline type\n"));
      f << "#interline=" << interline << ",";
      break;
    }

    para.m_margins[0]=(double)input->readLong(2);
    for (int j=0; j<numTabs; ++j) {
      int tabPos=(int) input->readLong(2);
      MWAWTabStop tab;
      if (tabPos<0) {
        tab.m_alignment=MWAWTabStop::DECIMAL;
        tabPos*=-1;
      }
      else if (tabPos&0x4000) {
        tab.m_alignment=MWAWTabStop::CENTER;
        tabPos &= 0x1FFF;
      }
      else if (tabPos&0x2000) {
        tab.m_alignment=MWAWTabStop::RIGHT;
        tabPos &= 0x1FFF;
      }
      tab.m_position=double(tabPos)/72.;
      para.m_tabs->push_back(tab);
    }
    input->seek(pos+32, librevenge::RVNG_SEEK_SET);
    int prev=(int) input->readULong(1);
    int next=(int) input->readULong(1);
    int wh=0;
    if (prev&0x80) {
      wh=2;
      prev&=0x7f;
    }
    if (next&0x80) {
      wh|=1;
      next&=0x7f;
    }
    switch (wh) {
    default:
    case 0: // normal;
      break;
    case 1: // +0.5 interline
    case 2: // +1 interline
      para.m_spacings[1]=(wh-1)*0.5*12./72.;
      break;
    case 3:
      para.m_spacings[1]=prev/72.;
      para.m_spacings[2]=next/72.;
      break;
    }
    f << para;
    zone.m_paragraphPosList.push_back(textPos);
    zone.m_paragraphList.push_back(para);

    input->seek(pos+paraSz, librevenge::RVNG_SEEK_SET);
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
// send data to the listener

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
