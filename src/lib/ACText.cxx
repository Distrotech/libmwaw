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
#include <map>
#include <sstream>

#include <libwpd/libwpd.h>

#include "MWAWContentListener.hxx"
#include "MWAWDebug.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWPageSpan.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPictData.hxx"
#include "MWAWPosition.hxx"
#include "MWAWRSRCParser.hxx"

#include "ACParser.hxx"

#include "ACText.hxx"

/** Internal: the structures of a ACText */
namespace ACTextInternal
{
////////////////////////////////////////
//! Internal: a topic of a ACText
struct Topic {
  //! constructor
  Topic() : m_depth(0), m_type(0), m_hidden(0), m_pageBreak(false), m_font(), m_labelColor(MWAWColor::black()),
    m_data(), m_fonts(), m_auxi(), m_extra("") {
  }
  //! return true if the topic is valid
  bool valid() const {
    return m_depth>0 && (m_type==1 || m_type==2);
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Topic const &topic) {
    if (topic.m_depth > 0)
      o << "depth=" << topic.m_depth << ",";
    switch(topic.m_type) {
    case 1:
      o << "text,";
      break;
    case 2:
      o << "graphic,";
      break;
    default:
      o << "#type=" << topic.m_type << ",";
      break;
    }
    if (topic.m_pageBreak) o << "pagebreak,";
    if (topic.m_hidden) o << "hidden[" << topic.m_hidden << "],";
    if (!topic.m_labelColor.isBlack())
      o << "labelColor=" << topic.m_labelColor << ",";
    o << topic.m_extra << ",";
    return o;
  }
  //! the node depth
  int m_depth;
  //! the node type: 1=text, 2=graphic
  int m_type;
  //! the number of time a topic is hidden by its parents
  int m_hidden;
  //! true if a page break exists before the topic
  bool m_pageBreak;
  //! the line font
  MWAWFont m_font;
  //! the label color
  MWAWColor m_labelColor;
  //! the data entries(text or graphic)
  MWAWEntry m_data;
  //! the fonts entries(for text)
  MWAWEntry m_fonts;
  //! an auxialliary entry(unknown)
  MWAWEntry m_auxi;
  //! extra
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the state of a ACText
struct State {
  //! constructor
  State() : m_topicList(), m_listId(-1), m_colorList(), m_version(-1), m_numPages(-1), m_actualPage(1) {
  }

  //! set the default color map
  void setDefaultColorList(int version);
  //! the topic list
  std::vector<Topic> m_topicList;
  //! the list id
  int  m_listId;
  //! a list colorId -> color
  std::vector<MWAWColor> m_colorList;
  //! the file version
  mutable int m_version;
  int m_numPages /* the number of pages */, m_actualPage /* the actual page */;
};

void State::setDefaultColorList(int version)
{
  if (m_colorList.size()) return;
  if (version==3) {
    uint32_t const defCol[20] = {
      0x000000, 0xff0000, 0x00ff00, 0x0000ff, 0x00ffff, 0xff00db, 0xffff00, 0x8d02ff,
      0xff9200, 0x7f7f7f, 0x994914, 0x000000, 0x484848, 0x880000, 0x008600, 0x838300,
      0xff9200, 0x7f7f7f, 0x994914, 0xfffff
    };
    m_colorList.resize(20);
    for (size_t i = 0; i < 20; i++)
      m_colorList[i] = defCol[i];
    return;
  }
}
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

  int nPages=1;
  for (size_t t=0; t < m_state->m_topicList.size(); t++) {
    if (m_state->m_topicList[t].m_pageBreak)
      nPages++;
  }

  return m_state->m_numPages = nPages;
}

bool ACText::getColor(int id, MWAWColor &col) const
{
  int numColor = (int) m_state->m_colorList.size();
  if (!numColor) {
    m_state->setDefaultColorList(version());
    numColor = int(m_state->m_colorList.size());
  }
  if (id < 0 || id >= numColor)
    return false;
  col = m_state->m_colorList[size_t(id)];
  return true;
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

//
// find/send the different zones
//
bool ACText::createZones()
{
  int vers=version();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  input->seek(vers>=3 ? 2 : 0, WPX_SEEK_SET);
  while (!input->atEOS()) {
    if (!readTopic())
      break;
  }

  long pos = input->tell();
  int val=(int) input->readLong(2);
  if (val || (vers<3 && !input->atEOS())) {
    MWAW_DEBUG_MSG(("ACText::createZones: find unexpected end data\n"));
    ascFile.addPos(pos);
    ascFile.addNote("Entries(Loose):###");
  } else {
    ascFile.addPos(pos);
    ascFile.addNote("_");
  }
  return m_state->m_topicList.size();
}

bool ACText::sendMainText()
{
  // create the main list
  shared_ptr<MWAWList> list = m_mainParser->getMainList();
  if (!list) {
    MWAW_DEBUG_MSG(("ACText::sendMainText: can retrieve the main list\n"));
  } else
    m_state->m_listId = list->getId();

  for (size_t t=0; t < m_state->m_topicList.size(); t++)
    sendTopic(m_state->m_topicList[t]);
  return true;
}

//
// read/send a topic
//
bool ACText::readTopic()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  int vers=version();
  long pos = input->tell();
  if (!input->checkPosition(pos+18+4+((vers>=3) ? 4 : 0)))
    return false;
  ACTextInternal::Topic topic;
  topic.m_depth=(int) input->readLong(2); // checkme
  topic.m_type=(int) input->readLong(2);
  if (!topic.valid()) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  int flag = (int) input->readULong(2); // 0|1|2|c01c|2002|
  if (flag & 0x100)
    f << "current,";
  if (flag & 0x2000)
    topic.m_pageBreak=true;
  flag &= 0xFEFF;
  if (flag)
    f << "fl=" << std::hex << flag << std::dec << ",";
  if (!readFont(topic.m_font, false))
    f << "foont###,";
  int color=(int) input->readLong(1);
  if (color) {
    MWAWColor fCol;
    if (getColor(color, fCol))
      topic.m_labelColor=fCol;
    else
      f << "#col="  << color << ",";
    static bool first=true;
    if (first) {
      MWAW_DEBUG_MSG(("ACText::readTopic: sending color label is not implemented\n"));
      first = false;
    }
  }
  int val;
  for (int i = 0; i < 5; i++) {
    val=(int) input->readLong(1);
    if (!val) continue;
    if (val==1 && i==2) f << "showChild|check,"; // also g2=-1
    else f << "g" << i << "=" << val << ",";
  }
  topic.m_hidden=(int) input->readLong(1);
  topic.m_extra=f.str();
  f.str("");
  f << "Entries(Topic):" << topic;
#ifdef DEBUG
  f << "font=[" << topic.m_font.getDebugString(m_parserState->m_fontConverter) << "],";
#endif

  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  input->seek(pos+18, WPX_SEEK_SET);

  int numZones= vers<3 ? 1 : topic.m_type==2 ? 2 : 3;
  for (int z=0; z < numZones; z++) {
    pos = input->tell();
    long sz=long(input->readULong(4));
    if (sz < 0 || !input->checkPosition(pos+4+sz)) {
      MWAW_DEBUG_MSG(("ACText::readTopic: can not read a topic zone\n"));
      ascFile.addPos(pos);
      ascFile.addNote("###");

      input->seek(pos, WPX_SEEK_SET);
      return false;
    }
    if (sz==0) {
      ascFile.addPos(pos);
      ascFile.addNote("_");
    }

    MWAWEntry &entry=(z==0) ? topic.m_data : (z==1&&topic.m_type==1) ? topic.m_fonts : topic.m_auxi;
    entry.setBegin(pos+4);
    entry.setLength(sz);
    input->seek(entry.end(), WPX_SEEK_SET);
  }

  m_state->m_topicList.push_back(topic);
  return true;
}

bool ACText::sendTopic(ACTextInternal::Topic const &topic)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("ACText::sendTopic: can not find a listener\n"));
    return false;
  }
  if (topic.m_pageBreak)
    m_mainParser->newPage(++m_state->m_actualPage);

  // useme: find always here a field of size 6 with 3 int=0...
  if (topic.m_auxi.valid()) {
    MWAWInputStreamPtr &input= m_parserState->m_input;
    libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
    libmwaw::DebugStream f;
    input->seek(topic.m_auxi.begin(), WPX_SEEK_SET);
    f.str("");
    f << "Entries(Data1):";
    if (topic.m_auxi.length()!=6) {
      MWAW_DEBUG_MSG(("ACText::readTopic: find unexpected size for data1\n"));
      f << "###";
    } else {
      for (int i=0; i<3; i++) {
        int val=(int)input->readLong(2);
        if (val) f << "#f" << i << "=" << val << ",";
      }
    }
    ascFile.addPos(topic.m_auxi.begin()-4);
    ascFile.addNote(f.str().c_str());
  }

  // ok, let update the data
  MWAWParagraph para = listener->getParagraph();
  if (m_state->m_listId >= 0) {
    para.m_listLevelIndex=topic.m_depth;
    para.m_listId = m_state->m_listId;
  }
  para.m_margins[1]=0.2*double(topic.m_depth-1);
  listener->setParagraph(para);
  listener->setFont(topic.m_font);

  if (topic.m_data.length()==0) {
    listener->insertEOL();
    return true;
  }

  if (topic.m_type==1)
    return sendText(topic);
  else
    return sendGraphic(topic);
}

//
// send the text
//
bool ACText::sendText(ACTextInternal::Topic const &topic)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("ACText::sendText: can not find a listener\n"));
    return false;
  }
  if (!topic.m_data.valid()) {
    MWAW_DEBUG_MSG(("ACText::sendText: can not find my data\n"));
    listener->insertEOL();
    return false;
  }
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  // first read the font list if it exists
  std::map<long,MWAWFont> fontMap;
  if (topic.m_fonts.valid()) {
    input->seek(topic.m_fonts.begin(), WPX_SEEK_SET);
    int n= (int) input->readULong(2);

    f.str("");
    f << "Entries(CharPLC):n=" << n << ",";
    if (2+20*n!=long(topic.m_fonts.length())) {
      MWAW_DEBUG_MSG(("ACText::sendText: find unexpected number of font\n"));
      f << "###";
      ascFile.addPos(topic.m_fonts.begin()-4);
      ascFile.addNote(f.str().c_str());
    } else {
      ascFile.addPos(topic.m_fonts.begin()-4);
      ascFile.addNote(f.str().c_str());

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
        else
          fontMap[cPos]=font;
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
  }

  input->seek(topic.m_data.begin(), WPX_SEEK_SET);
  long sz=topic.m_data.length();
  f.str("");
  f << "Entries(Text):";
  std::map<long,MWAWFont>::const_iterator fIt;
  for (long i=0; i < sz; i++) {
    fIt = fontMap.find(i);
    if (fIt!=fontMap.end())
      listener->setFont(fIt->second);
    char c=(char) input->readULong(1);
    switch(c) {
    case 0x9:
      listener->insertTab();
      break;
    case 0xd:
      listener->insertEOL(true);
      break;
    default:
      listener->insertCharacter((unsigned char)c);
    }
    f << c;
  }
  listener->insertEOL();
  ascFile.addPos(topic.m_data.begin()-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

//
// send a graphic
//
bool ACText::sendGraphic(ACTextInternal::Topic const &topic)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("ACText::sendGraphic: can not find a listener\n"));
    return false;
  }
  if (!topic.m_data.valid()) {
    MWAW_DEBUG_MSG(("ACText::sendGraphic: can not find my data\n"));
    listener->insertEOL();
    return false;
  }
  long dataSz=topic.m_data.length();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  long pos = topic.m_data.begin();

  ascFile.addPos(pos-4);
  ascFile.addNote("Entries(Graphic):");
  ascFile.skipZone(pos, pos+dataSz-1);

  Box2f box;
  input->seek(pos, WPX_SEEK_SET);
  MWAWPict::ReadResult res = MWAWPictData::check(input, (int)dataSz, box);
  if (res == MWAWPict::MWAW_R_BAD) {
    MWAW_DEBUG_MSG(("ACText::sendGraphic: can not find the picture\n"));
    ascFile.addPos(pos);
    ascFile.addNote("###");

    return true;
  }

  WPXBinaryData file;
  input->seek(pos, WPX_SEEK_SET);
  input->readDataBlock(dataSz, file);

  MWAWPosition posi(Vec2f(0,0), box.size(), WPX_POINT);
  posi.setRelativePosition(MWAWPosition::Char);
  listener->insertPicture(posi, file, "image/pict");
  listener->insertEOL();
#ifdef DEBUG_WITH_FILES
  static int volatile pictName = 0;
  libmwaw::DebugStream f;
  f << "DATA-" << ++pictName;
  libmwaw::Debug::dumpFile(file, f.str().c_str());
#endif

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
  font.setSize((float) input->readLong(2));

  font.m_extra=f.str();
  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
