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

#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

#include <libwpd/libwpd.h>

#include "MWAWContentListener.hxx"
#include "MWAWDebug.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWPageSpan.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPosition.hxx"
#include "MWAWRSRCParser.hxx"

#include "ACParser.hxx"

#include "ACText.hxx"

/** Internal: the structures of a ACText */
namespace ACTextInternal
{
////////////////////////////////////////
//! Internal: the state of a ACText
struct State {
  //! constructor
  State() : m_listId(-1), m_version(-1), m_numPages(-1), m_actualPage(1) {
  }

  //! the list id
  int  m_listId;
  //! the file version
  mutable int m_version;
  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;
};
}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
ACText::ACText(ACParser &parser) :
  m_parserState(parser.getParserState()), m_state(new ACTextInternal::State), m_mainParser(&parser)
{
}

ACText::~ACText()
{ }

int ACText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}

int ACText::numPages() const
{
  if (m_state->m_numPages >= 0)
    return m_state->m_numPages;
  // FIXME: compute the positions here
  return m_state->m_numPages;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

// find the different zones
bool ACText::sendMainText()
{
  // create the main list
  shared_ptr<MWAWList> list = m_mainParser->getMainList();
  if (!list) {
    MWAW_DEBUG_MSG(("ACText::sendMainText: can create a listt\n"));
  } else
    m_state->m_listId = list->getId();

  int vers=version();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  input->seek(vers>=3 ? 2 : 0, WPX_SEEK_SET);
  while (!input->atEOS()) {
    if (!sendTopic())
      break;
  }

  long pos = input->tell();
  int val=(int) input->readLong(2);
  if (val || (vers<3 && !input->atEOS())) {
    MWAW_DEBUG_MSG(("ACText::sendMainText: find unexpected end data\n"));
    ascFile.addPos(pos);
    ascFile.addNote("Entries(Loose):###");
    return true;
  }
  ascFile.addPos(pos);
  ascFile.addNote("_");
  return true;
}

bool ACText::sendTopic()
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("ACText::sendTopic: can not find a listener\n"));
    return false;
  }

  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  int vers=version();
  long pos = input->tell();
  if (!m_mainParser->isFilePos(pos+18+4+((vers>=3) ? 4 : 0)))
    return false;
  f << "Entries(Topic):";
  int depth=(int) input->readLong(2); // checkme
  int type=(int) input->readLong(2);
  if (depth <= 0 || type < 1 || type > 2) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  f << "depth=" << depth << ",";
  if (type==2) f << "graphic,";
  int flag = (int) input->readULong(2); // 0|1|2|c01c|2002|
  if (flag & 0x100)
    f << "current,";
  flag &= 0xFEFF;
  if (flag)
    f << "fl=" << std::hex << flag << std::dec << ",";
  MWAWFont font;
  if (!readFont(font, false))
    f << "###";
#ifdef DEBUG
  f << "font=[" << font.getDebugString(m_parserState->m_fontConverter) << "],";
#endif
  int val;
  for (int i = 0; i < 6; i++) { // g0=0|5|9...
    val=(int) input->readLong(1);
    if (!val) continue;
    if (val==1 && i==3) f << "showChild|check,"; // also g3=-1
    else f << "g" << i << "=" << val << ",";
  }
  val=(int) input->readLong(1); // number time hidden
  if (val==1) f << "hidden,";
  else if (val) f << "hidden[" << val << "],";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  input->seek(pos+18, WPX_SEEK_SET);

  // ok, let update the data
  if (m_state->m_listId >= 0) {
    MWAWParagraph para = listener->getParagraph();
    para.m_listLevelIndex=depth;
    para.m_listId = m_state->m_listId;
    listener->setParagraph(para);
  }
  listener->setFont(font);

  if (type==1) {
    if (!sendText())
      return false;
  } else {
    pos = input->tell();
    long sz=long(input->readULong(4));
    if (sz < 0 || !m_mainParser->isFilePos(pos+4+sz)) {
      input->seek(pos, WPX_SEEK_SET);
      return false;
    }
    f.str("");
    f << "Entries(Graphic):";
    input->seek(pos+4+sz, WPX_SEEK_SET);

    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  if (vers<3) return true;

  pos = input->tell();
  long sz=long(input->readULong(4));
  if (sz < 0 || !m_mainParser->isFilePos(pos+4+sz)) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }

  // checkme: find always here a field of size 6 with 3 int=0...
  f.str("");
  f << "Entries(Data1):";
  if (sz!=6) {
    MWAW_DEBUG_MSG(("ACText::sendTopic: find unexpected size for data1\n"));
    f << "###";
  } else {
    for (int i=0; i<3; i++) {
      val=(int)input->readLong(2);
      if (val) f << "#f" << i << "=" << val << ",";
    }
  }
  input->seek(pos+4+sz, WPX_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  return true;
}

bool ACText::sendText()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  int vers=version();
  long pos = input->tell();
  long sz=long(input->readULong(4));
  long endPos = pos+4+sz;
  if (sz < 0 || !m_mainParser->isFilePos(endPos)) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  if (vers >= 3) {
    // first read the text properties
    input->seek(endPos, WPX_SEEK_SET);
    long pSz=long(input->readULong(4));
    int n= pSz ? (int) input->readULong(2) : 0;
    if (pSz && (2+20*n!=pSz || !m_mainParser->isFilePos(endPos+4+pSz))) {
      input->seek(pos, WPX_SEEK_SET);
      return false;
    }
    f.str("");
    f << "Entries(CharPLC):n=" << n << ",";

    ascFile.addPos(endPos);
    ascFile.addNote(f.str().c_str());

    endPos+=4+pSz;
    for (int i = 0; i < n; i++) {
      long pPos = input->tell();
      f.str("");
      f << "CharPLC-" << i << ":";

      long cPos = (long) input->readULong(4);
      if (cPos) f << "cPos=" << cPos << ",";
      int dim[2];
      for (int j = 0; j < 2; j++)
        dim[j]=(int) input->readLong(2);
      f << "h=" << dim[0] << ","; // checkme, simillar to font.size()+2
      f << "f0=" << dim[1] << ","; // seems slightly less than font.size()

      MWAWFont font;
      if (!readFont(font, true))
        f << "###";
#ifdef DEBUG
      f << "font=[" << font.getDebugString(m_parserState->m_fontConverter) << "],";
#endif

      for (int j = 0; j < 3; j++) { // always 0
        int val = (int) input->readLong(2);
        if (val)
          f << "f" << j+1 << "=" << val << ",";
      }
      input->seek(pPos+20, WPX_SEEK_SET);
      ascFile.addPos(pPos);
      ascFile.addNote(f.str().c_str());
    }
  }

  input->seek(pos+4, WPX_SEEK_SET);
  f.str("");
  f << "Entries(Text):";
  for (int i=0; i < sz; i++)
    f << (char) input->readULong(1);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  input->seek(endPos, WPX_SEEK_SET);
  return true;
}

//////////////////////////////////////////////
// Fonts
//////////////////////////////////////////////
bool ACText::readFont(MWAWFont &font, bool inPLC)
{
  font = MWAWFont();

  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugStream f;

  font.setId((int) input->readLong(2));
  int flag[2];
  for (int i = 0; i < 2; i++)
    flag[inPLC ? i : 1-i] = (int) input->readULong(1);
  uint32_t flags = 0;
  if (flag[0]&0x1) flags |= MWAWFont::boldBit;
  if (flag[0]&0x2) flags |= MWAWFont::italicBit;
  if (flag[0]&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
  if (flag[0]&0x8) flags |= MWAWFont::embossBit;
  if (flag[0]&0x10) flags |= MWAWFont::shadowBit;
  flag[0] &= 0xE0;
  for (int i = 0; i < 2; i++) {
    if (flag[i])
      f << "#fl" << i << "=" << std::hex << flag << std::dec << ",";
  }
  font.setFlags(flags);
  font.setSize((int) input->readLong(2));

  font.m_extra=f.str();
  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
