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

#include "MORParser.hxx"

#include "MORText.hxx"

/** Internal: the structures of a MORText */
namespace MORTextInternal
{
////////////////////////////////////////
//! Internal: the state of a MORText
struct State {
  //! constructor
  State() : m_colorList(), m_version(-1), m_numPages(-1), m_actualPage(1) {
  }

  //! set the default color map
  void setDefaultColorList(int version);
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
MORText::MORText(MORParser &parser) :
  m_parserState(parser.getParserState()), m_state(new MORTextInternal::State), m_mainParser(&parser)
{
}

MORText::~MORText()
{ }

int MORText::version() const
{
  if (m_state->m_version < 0)
    m_state->m_version = m_parserState->m_version;
  return m_state->m_version;
}

int MORText::numPages() const
{
  if (m_state->m_numPages >= 0)
    return m_state->m_numPages;

  int nPages=1;
  // fixme
  return m_state->m_numPages = nPages;
}

bool MORText::getColor(int id, MWAWColor &col) const
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

bool MORText::check(MWAWEntry &entry)
{
  if (entry.begin()<0 || !m_mainParser->isFilePos(entry.begin()+4))
    return false;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long actPos=input->tell();
  input->seek(entry.begin(), WPX_SEEK_SET);
  entry.setLength(4+(long) input->readULong(4));
  input->seek(actPos,WPX_SEEK_SET);
  return m_mainParser->isFilePos(entry.end());
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////

//
// find/send the different zones
//
bool MORText::createZones()
{
  return false;
}

bool MORText::sendMainText()
{
  return true;
}

//! read a text entry
bool MORText::readText(MWAWEntry const &entry)
{
  if (entry.length()<4) {
    MWAW_DEBUG_MSG(("MORText::readText: the entry is bad\n"));
    return false;
  }
  long pos = entry.begin();
  long endPos = entry.end();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  input->seek(pos+4, WPX_SEEK_SET);
  f << "Entries(Text):";
  int val;
  while (!input->atEOS()) {
    long actPos = input->tell();
    if (actPos >= endPos)
      break;
    unsigned char c=(unsigned char)input->readULong(1);
    if (c!=0x1b) {
      f << c;
      continue;
    }
    if (actPos+1 >= endPos) {
      f << "@[#]";
      MWAW_DEBUG_MSG(("MORText::readText: text end by 0x1b\n"));
      continue;
    }
    int nextC=(int)input->readULong(1);
    if (nextC!=0xb9) {
      // find @[55]@[42]path@[75]@[62]
      f << "@[" << std::hex << nextC << std::dec << "]";
      continue;
    }
    // field?
    if (actPos+11 >= endPos) {
      f << "@[#b9]";
      MWAW_DEBUG_MSG(("MORText::readText: field b9 seems too short\n"));
      continue;
    }
    f << "@[b9:";
    for (int i=0; i < 4; i++) {
      val=(int)input->readLong(2);
      if (val)
        f << std::hex << val << std::dec << ",";
      else
        f << ",";
    }
    val=(int)input->readLong(2);
    if (val!=0x1bb9) {
      MWAW_DEBUG_MSG(("MORText::readText: field b9 seems odds\n"));
      f << "###" << std::hex << val << std::dec;
    }
    f << "@]";
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}

// try read to the file text position
bool MORText::readTopic(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%10)) {
    MWAW_DEBUG_MSG(("MORText::readTopic: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  input->seek(pos, WPX_SEEK_SET);
  entry.setParsed(true);

  ascFile.addPos(pos);
  ascFile.addNote("Entries(Topic)");

  int N=int(entry.length()/10);
  int val;
  std::vector<MWAWEntry> textPositions;
  for (int i=0; i < N; i++) {
    pos=input->tell();
    f.str("");
    f << "Topic-" << i << ":";
    val = (int) input->readLong(2); // a small number betwen -1 and 3
    f << "f0=" << val << ",";
    val = (int) input->readULong(2); // some flag ?
    if (val) f << "fl=" << std::hex << val << std::dec << ",";
    long fPos = input->readLong(4);
    f << "pos=" << std::hex << fPos << std::dec << ",";
    MWAWEntry tEntry;
    tEntry.setBegin(fPos);
    if (!check(tEntry)) {
      MWAW_DEBUG_MSG(("MORText::readTopic: can not read a text position\n"));
      f << "###";
    } else
      textPositions.push_back(tEntry);
    val = (int) input->readLong(2); // a small number 1 or 2
    if (val)
      f << "f1=" << val << ",";

    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+10, WPX_SEEK_SET);
  }
  for (size_t i=0; i < textPositions.size(); i++) {
    MWAWEntry const &tEntry=textPositions[i];
    ascFile.addPos(tEntry.end());
    ascFile.addNote("_");
    if (readText(tEntry))
      continue;
    f.str("");
    f << "Topic-" << i << "[data]:";
    ascFile.addPos(tEntry.begin());
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

bool MORText::readComment(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%8)) {
    MWAW_DEBUG_MSG(("MORText::readComment: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  input->seek(pos, WPX_SEEK_SET);
  entry.setParsed(true);

  ascFile.addPos(pos);
  ascFile.addNote("Entries(Comment)");

  int N=int(entry.length()/8);
  int val;
  std::vector<MWAWEntry> filePositions;
  for (int i=0; i < N; i++) {
    pos=input->tell();

    f.str("");
    f << "Comment-" << i << ":";
    long fPos = input->readLong(4);
    f << "pos=" << std::hex << fPos << std::dec << ",";
    MWAWEntry tEntry;
    tEntry.setBegin(fPos);
    if (!check(tEntry)) {
      MWAW_DEBUG_MSG(("MORText::readComment: can not read a file position\n"));
      f << "###";
    } else
      filePositions.push_back(tEntry);
    val = (int) input->readLong(2); // always 4 ?
    if (val != 4)
      f << "f0=" << val << ",";
    val = (int) input->readULong(2); // some flag ? find 0x3333 0x200d ...
    if (val) f << "fl=" << std::hex << val << std::dec << ",";

    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+8, WPX_SEEK_SET);
  }
  for (size_t i=0; i < filePositions.size(); i++) {
    MWAWEntry const &tEntry=filePositions[i];
    ascFile.addPos(tEntry.end());
    ascFile.addNote("_");
    if (readText(tEntry))
      continue;
    f.str("");
    f << "Comment-" << i << "[data]:";
    ascFile.addPos(tEntry.begin());
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

//
// send the text
//

//
// send a graphic
//

//////////////////////////////////////////////
// Fonts
//////////////////////////////////////////////
bool MORText::readFonts(MWAWEntry const &entry)
{
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("MORText::readFonts: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  long endPos = entry.end();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  input->seek(pos, WPX_SEEK_SET);
  entry.setParsed(true);

  int n=0;
  while (1) {
    pos=input->tell();
    if (pos+1 > endPos) {
      MWAW_DEBUG_MSG(("MORText::readFonts: problem reading a font\n"));
      break;
    }
    int fSz=int(input->readULong(1));
    if (fSz==0)
      break;
    if (pos+1+fSz+2 > endPos) {
      input->seek(-1, WPX_SEEK_CUR);
      break;
    }
    f.str("");
    if (n==0)
      f << "Entries(Fonts)-" << n++ << ",";
    else
      f << "Fonts-"  << n++ << ":";
    std::string name("");
    for (int i=0; i < fSz; i++)
      name+=(char) input->readULong(1);
    if ((fSz&1)==0) input->seek(1, WPX_SEEK_CUR);
    int id=(int) input->readULong(2);
    f << name << ",id=" << id << ",";
    if (name.length())
      m_parserState->m_fontConverter->setCorrespondance(id, name);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }

  pos = input->tell();
  if (pos != endPos) {
    MWAW_DEBUG_MSG(("MORText::readFonts: problem reading a font\n"));
    ascFile.addPos(pos);
    ascFile.addNote("Fonts:###");
  }

  return true;
}

//////////////////////////////////////////////
// unknown
//////////////////////////////////////////////
bool MORText::readUnknown5(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%8)) {
    MWAW_DEBUG_MSG(("MORText::readUnknown5: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  input->seek(pos, WPX_SEEK_SET);
  entry.setParsed(true);

  ascFile.addPos(pos);
  ascFile.addNote("Entries(Unknown5)");

  int N=int(entry.length()/8);
  int val;
  std::vector<MWAWEntry> filePositions;
  for (int i=0; i < N; i++) {
    pos=input->tell();

    f.str("");
    f << "Unknown5-" << i << ":";
    long fPos = input->readLong(4);
    f << "pos=" << std::hex << fPos << std::dec << ",";
    MWAWEntry tEntry;
    tEntry.setBegin(fPos);
    if (fPos==0x50) // checkme: default or related to filePosition 0x50 ?
      ;
    else if (!check(tEntry)) {
      MWAW_DEBUG_MSG(("MORText::readUnknown5: can not read a file position\n"));
      f << "###";
    } else
      filePositions.push_back(tEntry);
    val = (int) input->readLong(2); // always -1 ?
    if (val != -1)
      f << "f0=" << val << ",";
    val = (int) input->readLong(2); // always 0 ?
    if (val)
      f << "f1=" << val << ",";

    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+8, WPX_SEEK_SET);
  }
  for (size_t i=0; i < filePositions.size(); i++) {
    MWAWEntry const &tEntry=filePositions[i];
    if (readUnknown5Data(tEntry))
      continue;
    f.str("");
    f << "Unknown5-###" << i << "[data]:";
    ascFile.addPos(tEntry.begin());
    ascFile.addNote(f.str().c_str());
    ascFile.addPos(tEntry.end());
    ascFile.addNote("_");
  }
  return true;
}

bool MORText::readUnknown5Data(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<16) {
    MWAW_DEBUG_MSG(("MORText::readUnknown5Data: the entry is bad\n"));
    return false;
  }
  long pos = entry.begin();
  long endPos = entry.end();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  input->seek(pos+4, WPX_SEEK_SET); // skip size
  entry.setParsed(true);

  f << "Unknown5[data]:";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  input->seek(pos+16, WPX_SEEK_SET);

  int n=0;
  while(1) {
    pos = input->tell();
    if (pos+2 > endPos)
      break;
    int type=(int) input->readLong(2);
    int dataSz=0;
    if (type & 0x1)
      dataSz=4;
    else {
      switch(type) {
      case 0x66: // group: arg num group
      case 0x68: // group of num 6a,70* ?
      case 0x72: // group of num 74* ?
      case 0x74: // [ id : val ]
        dataSz=4;
        break;
      case 0x6a: // pattern?, text, id, ...
      case 0x70: // size=0x4a ?
        dataSz=4+(int) input->readULong(4);
        break;
      default:
        MWAW_DEBUG_MSG(("MORText::readUnknown5Data: argh... find unexpected type %d\n", type));
        break;
      }
    }
    if (!dataSz || pos+2+dataSz > endPos) {
      input->seek(pos, WPX_SEEK_SET);
      break;
    }

    f.str("");
    f << "Unknown5-" << n++ << "[data]:";
    f << "type=" << std::hex << (type&0xFFFE) << std::dec;
    if (type&1) f << "*";
    f << ",";
    if (dataSz==4)
      f << "N=" << input->readLong(4) << ",";
    if (type==0x6a) {
      MWAWEntry dEntry;
      dEntry.setBegin(pos+2+4);
      dEntry.setLength(dataSz-4);
      // can also be some text and ?
      if (!readValue(dEntry,-6))
        f << "#";
    }
    input->seek(pos+2+dataSz, WPX_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  pos = input->tell();
  if (pos!=endPos) {
    ascFile.addPos(pos);
    ascFile.addNote("Unknown5-###[data]:");
  }

  ascFile.addPos(endPos);
  ascFile.addNote("_");

  return true;
}

bool MORText::readUnknown6(MWAWEntry const &entry)
{
  if (!entry.valid() || (entry.length()%4)) {
    MWAW_DEBUG_MSG(("MORText::readUnknown6: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  input->seek(pos, WPX_SEEK_SET);
  entry.setParsed(true);

  f << "Entries(Unknown6):";
  int N=int(entry.length()/4);
  std::vector<MWAWEntry> filePositions;
  for (int i=0; i < N; i++) {
    long fPos = input->readLong(4);
    f << std::hex << fPos << std::dec << ",";
    MWAWEntry tEntry;
    tEntry.setBegin(fPos);
    if (!check(tEntry)) {
      MWAW_DEBUG_MSG(("MORText::readUnknown6: can not read a file position\n"));
      f << "###";
    } else
      filePositions.push_back(tEntry);
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  for (size_t i=0; i < filePositions.size(); i++) {
    MWAWEntry const &tEntry=filePositions[i];
    if (readUnknown6Data(tEntry))
      continue;

    ascFile.addPos(tEntry.begin());
    ascFile.addNote("Unknown6-data:###");
    ascFile.addPos(tEntry.end());
    ascFile.addNote("_");
  }
  return true;
}

bool MORText::readUnknown6Data(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length()<8) {
    MWAW_DEBUG_MSG(("MORText::readUnknown6Data: the entry is bad\n"));
    return false;
  }
  int vers = version();
  long pos = entry.begin();
  long endPos = entry.end();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  input->seek(pos+4, WPX_SEEK_SET); // skip size
  entry.setParsed(true);

  f << "Unknown6[data]:";
  int val=(int) input->readULong(2);
  if (val!=6*(vers-1)) {
    MWAW_DEBUG_MSG(("MORText::readUnknown6Data: find unexpected type\n"));
    f << "#f0=" << val << ",";
  }
  int N=(int) input->readULong(2);
  f << "N=" << N << ",";
  long lastListPos = pos+8+N*16;
  if (lastListPos > endPos) {
    MWAW_DEBUG_MSG(("MORText::readUnknown6Data: can not read length\n"));
    return false;
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  std::vector<MWAWEntry> listDataEntries;
  for (int n=0; n<N; n++) {
    pos = input->tell();
    f.str("");
    f << "Unknown6[data-" << n << "]:";
    val=int(input->readLong(1));
    if (val!=6*(vers-1))
      f << "#f0=" << val << ",";
    val=int(input->readULong(1));
    if (val) // [0|1|2|4]|[0..7]
      f << "fl=" << std::hex << val << std::dec << ",";
    for (int i=0; i <2; i++) { // f0=0|1|2, f1=0|1|999
      val=int(input->readLong(2));
      if (val)
        f << "f" << i+1 << "=" << val << ",";
    }
    for (int i=0; i<2; i++) { // number or flag?
      val=int(input->readULong(2));
      if (val)
        f << "g" << i << "=" << std::hex << val << std::dec << ",";
    }
    // FIXME: from here, experimental: a dim, some flags ?
    int unkn=(int) input->readLong(2);
    if (unkn) f << "unkn=" << unkn << ",";
    int values[2];
    for (int i=0; i < 2; i++)
      values[i] = (int) input->readLong(2);
    if (unkn==0 && values[1]>0 && values[0]>=0 &&
        lastListPos+values[0]+values[1] <= endPos) {
      MWAWEntry dEntry;
      dEntry.setBegin(lastListPos+values[0]);
      dEntry.setLength(values[1]);
      listDataEntries.push_back(dEntry);
      f << "ptr=" << std::hex << dEntry.begin() << "<->" << dEntry.end() << std::dec << ",";
    } else {
      for (int i=0; i < 2; i++) {
        if (values[i])
          f << "g" << i+2 << "=" << std::hex << values[i] << std::dec << ",";
      }
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(pos+16, WPX_SEEK_SET);
  }

  // I find here a pattern or a font name or a font name+id or 0 followed by 360|720|-720 (a dim?)
  for (size_t n=0; n < listDataEntries.size(); n++) {
    MWAWEntry const &dEntry=listDataEntries[n];
    f.str("");
    f << "Unknown6[dataA-" << n << "]:";
    if (!readValue(dEntry, 0))
      f << "###";
    ascFile.addPos(dEntry.begin());
    ascFile.addNote(f.str().c_str());
  }
  ascFile.addPos(endPos);
  ascFile.addNote("_");
  return true;
}

bool MORText::readValue(MWAWEntry const &entry, long fDecal)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;

  MORStruct::Pattern pattern;
  long pos = entry.begin();
  input->seek(pos, WPX_SEEK_SET);
  if (m_mainParser->readPattern(entry.end(),pattern)) {
    f << pattern;
    if (input->tell()!=entry.end())
      ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos+fDecal);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  // can we find a backsidde here
  input->seek(pos, WPX_SEEK_SET);
  std::string extra("");
  if (m_mainParser->readBackside(entry.end(), extra)) {
    f << extra;
    if (input->tell()!=entry.end())
      ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos+fDecal);
    ascFile.addNote(f.str().c_str());
    return true;
  }

  input->seek(pos, WPX_SEEK_SET);
  int c=(int) input->readULong(1);
  if (c==0 && entry.length()==4 && input->readULong(1)==0) {
    f << "val=" << input->readLong(2) << ",";
    ascFile.addPos(pos+fDecal);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  long strLength=long(1+c+(1-(c%2)));
  if (c > 0 && (strLength == entry.length()||strLength+2 == entry.length())) {
    input->seek(pos+1, WPX_SEEK_SET);
    std::string name("");
    for (int i=0; i < c; i++)
      name +=(char) input->readULong(1);
    if ((c%2)==0) input->seek(1,WPX_SEEK_CUR);
    f << "name=" << name << ",";
    if (input->tell()!=entry.end()) {
      if (input->tell()+2==entry.end())
        f << "id=" << input->readULong(2) << ",";
      else
        ascFile.addDelimiter(input->tell(),'|');
    }
    ascFile.addPos(pos+fDecal);
    ascFile.addNote(f.str().c_str());
    return true;
  }
  return false;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
