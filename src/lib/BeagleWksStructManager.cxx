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
#include <set>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWDebug.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWParser.hxx"
#include "MWAWRSRCParser.hxx"

#include "BeagleWksStructManager.hxx"

/** Internal: the structures of a BeagleWksStructManager */
namespace BeagleWksStructManagerInternal
{

////////////////////////////////////////
//! Internal: the state of a BeagleWksStructManager
struct State {
  //! constructor
  State() :  m_fileIdFontIdList(), m_header(), m_footer(), m_idFrameMap()
  {
  }
  //! a list to get the correspondance between fileId and fontId
  std::vector<int> m_fileIdFontIdList;
  //! the header
  MWAWEntry m_header;
  //! the footer
  MWAWEntry m_footer;
  /** the map id to frame */
  std::map<int, BeagleWksStructManager::Frame> m_idFrameMap;
};
}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
BeagleWksStructManager::BeagleWksStructManager(MWAWParserStatePtr parserState) :
  m_parserState(parserState), m_state(new BeagleWksStructManagerInternal::State())
{
}

BeagleWksStructManager::~BeagleWksStructManager()
{
}

int BeagleWksStructManager::getFontId(int fId) const
{
  if (fId<0||fId>=int(m_state->m_fileIdFontIdList.size())) {
    MWAW_DEBUG_MSG(("BeagleWksStructManager::getFontId can not find the final font id\n"));
    return 3;
  }

  return m_state->m_fileIdFontIdList[size_t(fId)];
}

void BeagleWksStructManager::getHeaderFooterEntries(MWAWEntry &header, MWAWEntry &footer) const
{
  header=m_state->m_header;
  footer=m_state->m_footer;
}

std::map<int,BeagleWksStructManager::Frame> const &BeagleWksStructManager::getIdFrameMap() const
{
  return m_state->m_idFrameMap;
}

bool BeagleWksStructManager::getFrame(int fId, Frame &frame) const
{
  if (m_state->m_idFrameMap.find(fId)==m_state->m_idFrameMap.end()) {
    MWAW_DEBUG_MSG(("BeagleWksStructManager::getFrame: can not find frame for id=%d\n",fId));
    return false;
  }
  frame=m_state->m_idFrameMap.find(fId)->second;
  return true;
}

MWAWInputStreamPtr BeagleWksStructManager::getInput()
{
  return m_parserState->m_input;
}

libmwaw::DebugFile &BeagleWksStructManager::ascii()
{
  return m_parserState->m_asciiFile;
}

MWAWInputStreamPtr BeagleWksStructManager::rsrcInput()
{
  return m_parserState->m_rsrcParser->getInput();
}

libmwaw::DebugFile &BeagleWksStructManager::rsrcAscii()
{
  return m_parserState->m_rsrcParser->ascii();
}

////////////////////////////////////////////////////////////
// the frame
////////////////////////////////////////////////////////////
bool BeagleWksStructManager::readFrame(MWAWEntry const &entry)
{
  if (entry.length()!=156*(long)entry.id()) {
    MWAW_DEBUG_MSG(("BeagleWksStructManager::readFrame: the entry seems bad\n"));
    return false;
  }
  entry.setParsed(true);
  MWAWInputStreamPtr input = getInput();
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  for (int i=0; i<entry.id(); ++i) {
    Frame frame;
    long pos=input->tell(), begPos=pos;
    libmwaw::DebugStream f;
    f << "Entries(Frame)[" << i << "]:";
    int type=(int) input->readULong(2);
    int val;
    switch (type) {
    case 0x8000: {
      f << "picture,";
      val=(int) input->readLong(2); // 1|8
      if (val) f << "f0=" << val << ",";
      ascii().addDelimiter(input->tell(),'|');
      input->seek(pos+40, librevenge::RVNG_SEEK_SET);
      ascii().addDelimiter(input->tell(),'|');
      for (int j=0; j < 5; ++j) { // f1=5, f3=2, f5=e|13
        val=(int) input->readLong(2);
        if (val) f << "f" << j+1 << "=" << val << ",";
      }
      double dim[4];
      for (int j=0; j<4; ++j)
        dim[j]=double(input->readLong(4))/65536.;
      f << "dim?=" << dim[1] << "x" << dim[0] << "<->" << dim[3] << "x" << dim[2] << ",";
      val =(int) input->readLong(2);
      if (val) f << "f6=" << val << ",";
      break;
    }
    case 0xffff: {
      f << "attachment,";
      for (int j=0; j<2; ++j) { // f0=0, f1=4ef8
        val=(int) input->readLong(2);
        if (val) f << "f" << j << "=" << val << ",";
      }
      int fSz=(int)input->readULong(1);
      if (fSz>0 && fSz<32) {
        std::string name("");
        for (int c=0; c < fSz; c++)
          name+=(char) input->readLong(1);
        f << name << ",";
      }
      else {
        MWAW_DEBUG_MSG(("BeagleWksStructManager::readFrame: the size seems bad\n"));
        f << "#fSz=" << fSz << ",";
      }
      input->seek(pos+44, librevenge::RVNG_SEEK_SET);
      for (int j=0; j<6; ++j)
        f << "dim" << j << "?=" << input->readLong(2) << "x"
          << input->readLong(2) << ",";
      break;
    }
    default:
      MWAW_DEBUG_MSG(("BeagleWksStructManager::readFrame: unknown frame type\n"));
      f << "type=" << std::hex << type << std::dec << ",";
      break;
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());

    pos=input->tell();
    f.str("");
    val=(int) input->readULong(2); // afc, 1df, 1dd, f34, a5a8
    f << "f0=" << std::hex << val << std::dec << ",";
    f << "PTR=" << std::hex << input->readULong(4) << std::dec << ",";
    float orig[2];
    for (int j=0; j<2; ++j)
      orig[j]=float(input->readLong(4))/65536.f;
    frame.m_origin=Vec2f(orig[1],orig[0]);
    f << "PTR1=" << std::hex << input->readULong(4) << std::dec << ",";

    frame.m_page=(int) input->readLong(2);
    float dim[2];
    for (int j=0; j<2; ++j)
      dim[j]=float(input->readLong(2));
    frame.m_dim=Vec2f(dim[1],dim[0]);
    f << "dim=" << dim[1] << "x" << dim[0] << ",";
    for (int j=0; j<4; ++j) { // f1=0|05b1 other 0
      val=(int) input->readLong(2);
      if (val) f << "f" << j+1 << "=" << std::hex << val << std::dec << ",";
    }
    frame.m_id=(int) input->readLong(2);
    for (int j=0; j<2; ++j) { // 0
      val=(int) input->readLong(2);
      if (val) f << "g" << j << "=" << std::hex << val << std::dec << ",";
    }
    frame.m_extra=f.str();
    f.str("");
    f << "Frame-II[" << i << "]:" << frame;

    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());

    pos=input->tell();
    f.str("");
    f << "Frame-III[" << i << "]:";
    val=int(input->readLong(2)); // 0|6|8|9
    if (val) f << "f0=" << val << ",";
    val=int(input->readULong(4)); // big number
    f << "PTR=" << std::hex << val << std::dec << ",";
    frame.m_border.m_width = (double) input->readLong(2);
    if (frame.m_border.m_width > 0)
      f << "borderSize=" << frame.m_border.m_width << ",";
    val=int(input->readLong(2)); // 0
    if (val) f << "f1=" << val << ",";
    val=int(input->readLong(4));
    if (val) f << "offset=" << double(val)/65536. << ",";
    val=int(input->readLong(2)); // 0
    if (val) f << "f2=" << val << ",";
    int flags=(int) input->readLong(2);
    frame.m_wrap=(flags&3);
    switch (frame.m_wrap) { // textaround
    case 0: // none
      f << "wrap=none,";
      break;
    case 1:
      f << "wrap=rectangle,";
      break;
    case 2:
      f << "wrap=irregular,";
      break;
    default:
      f << "#wrap=3,";
      break;
    }
    if (flags&0x8) {
      frame.m_charAnchor = false;
      f << "anchor=page,";
    }
    if (flags&0x10) {
      f << "bord[all],";
      frame.m_bordersSet=libmwaw::LeftBit|libmwaw::RightBit|
                         libmwaw::BottomBit|libmwaw::TopBit;
    }
    else if (flags&0x1E0) {
      f << "bord[";
      if (flags&0x20) {
        f << "T";
        frame.m_bordersSet |= libmwaw::TopBit;
      }
      if (flags&0x40) {
        f << "L";
        frame.m_bordersSet |= libmwaw::LeftBit;
      }
      if (flags&0x80) {
        f << "B";
        frame.m_bordersSet |= libmwaw::BottomBit;
      }
      if (flags&0x100) {
        f << "R";
        frame.m_bordersSet |= libmwaw::RightBit;
      }
      f << "],";
    }
    flags &= 0xFE04;
    if (flags) f << "fl=" << std::hex << flags << std::dec << ",";
    frame.m_pictId=(int)input->readULong(2);
    f << "pId=" << frame.m_pictId << ",";
    ascii().addDelimiter(input->tell(),'|');
    input->seek(18, librevenge::RVNG_SEEK_CUR);
    ascii().addDelimiter(input->tell(),'|');
    val=int(input->readLong(4));
    if (val) f << "textAround[offsT/B]=" << double(val)/65536. << ",";
    val=int(input->readLong(4));
    if (val) f << "textAround[offsR/L]=" << double(val)/65536. << ",";
    for (int j=0; j<2; ++j) { // g0,g1=0 or g0,g1=5c0077c (dim?)
      val=(int) input->readLong(2);
      if (val) f << "g" << j << "=" << val << ",";
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    if (m_state->m_idFrameMap.find(frame.m_id)!=m_state->m_idFrameMap.end()) {
      MWAW_DEBUG_MSG(("BeagleWksStructManager::readFrame: frame %d already exists\n", frame.m_id));
    }
    else
      m_state->m_idFrameMap[frame.m_id]=frame;
    input->seek(begPos+156, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// the fonts
////////////////////////////////////////////////////////////
bool BeagleWksStructManager::readFontNames(MWAWEntry const &entry)
{
  if (!entry.valid())
    return (entry.length()==0&&entry.id()==0);

  entry.setParsed(true);
  MWAWInputStreamPtr input= getInput();
  long pos=entry.begin(), endPos=entry.end();
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  m_state->m_fileIdFontIdList.resize(0);
  for (int i=0; i < entry.id(); ++i) {
    pos = input->tell();
    f.str("");
    f << "Entries(FontNames)[" << i << "]:";
    int fSz=(int) input->readULong(1);
    if (pos+1+fSz>endPos) {
      MWAW_DEBUG_MSG(("BeagleWksStructManager::readFontNames: can not read font %d\n", i));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      input->seek(endPos, librevenge::RVNG_SEEK_SET);
      return i>0;
    }
    std::string name("");
    for (int c=0; c < fSz; ++c)
      name+=(char) input->readULong(1);
    if (!name.empty())
      m_state->m_fileIdFontIdList.push_back(m_parserState->m_fontConverter->getId(name));

    f << "\"" << name << "\",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  pos = input->tell();
  if (pos!=endPos) {
    MWAW_DEBUG_MSG(("BeagleWksStructManager::readFontNames: find extra data\n"));
    ascii().addPos(pos);
    ascii().addNote("FontNames:###");
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
  }
  ascii().addPos(endPos);
  ascii().addNote("_");

  return true;
}

////////////////////////////////////////////////////////////
// the document info and preferences
////////////////////////////////////////////////////////////
bool BeagleWksStructManager::readDocumentInfo()
{
  MWAWInputStreamPtr input= getInput();
  long pos=input->tell();
  libmwaw::DebugStream f;
  f << "Entries(DocInfo):";
  long dSz=(int) input->readULong(2);
  long endPos=pos+dSz+4;
  if (dSz<0x226 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("BeagleWksStructManager::readDocumentInfo: can not find the database zone\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  int id=(int) input->readLong(2);
  if (id!=1) f << "id=" << id << ",";
  std::string what("");
  for (int i=0; i < 4; ++i) // dosu
    what+=(char) input->readLong(1);
  f << what << ",";
  int val;
  for (int i=0; i < 3; ++i) { // 961/2101, 0, 0
    val=(int) input->readLong(2);
    if ((i==0 && val!=0x961) || (i&&val))
      f << "f" << i+2 << "=" << val << ",";
  }
  f << "ids=[";
  for (int i=0; i < 2; ++i)
    f << std::hex << (long) input->readULong(4) << std::dec << ",";
  f << "],";
  double margins[4];
  f << "margins=[";
  for (int i=0; i < 4; ++i) {
    margins[i]=double(input->readLong(4))/72.;
    f << margins[i] << ",";
  }
  f << "],";
  MWAWPageSpan &pageSpan=m_parserState->m_pageSpan;
  if (margins[0]>=0&&margins[1]>=0&&margins[2]>=0&&margins[3]>=0&&
      margins[0]+margins[1]<0.5*pageSpan.getFormLength() &&
      margins[2]+margins[3]<0.5*pageSpan.getFormWidth()) {
    pageSpan.setMarginTop(margins[0]);
    pageSpan.setMarginBottom(margins[1]);
    pageSpan.setMarginLeft(margins[3]);
    pageSpan.setMarginRight(margins[2]);
  }
  else {
    MWAW_DEBUG_MSG(("BeagleWksStructManager::readDocumentInfo: the page margins seem bad\n"));
    f << "###";
  }
  int numRemains=int(endPos-512-input->tell());
  f << "fls=[";
  for (int i=0; i < numRemains; ++i) { // [_,_,_,_,_,1,]
    val = (int) input->readLong(1);
    if (val) f << val << ",";
    else f << "_,";
  }
  f << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int st=0; st<2; ++st) {
    pos=input->tell();
    f.str("");
    if (st==0)
      f << "DocInfo[header]:";
    else
      f << "DocInfo[footer]:";
    int fSz = (int) input->readULong(1);
    MWAWEntry &entry=st==0 ? m_state->m_header : m_state->m_footer;
    entry.setBegin(input->tell());
    entry.setLength(fSz);
    std::string name("");
    for (int i=0; i<fSz; ++i)
      name+=(char) input->readULong(1);
    f << name;
    input->seek(pos+256, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  return true;
}

bool BeagleWksStructManager::readDocumentPreferences()
{
  MWAWInputStreamPtr input= getInput();
  long pos=input->tell();
  libmwaw::DebugStream f;
  f << "Entries(Preferences):";
  long dSz=(long) input->readULong(2);
  long endPos=pos+dSz+4;
  if (dSz<0x2e || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("BeagleWksStructManager::readDocumentInfo: can not find the database zone\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  int id=(int) input->readLong(2);
  if (id!=0xa) f << "id=" << id << ",";
  std::string what="";
  for (int i=0; i < 4; ++i) // pref
    what+=(char) input->readLong(1);
  f << what << ",";
  for (int i=0; i < 3; ++i) { // always 0
    int val=(int) input->readLong(2);
    if (val) f << "f" << i+2 << "=" << val << ",";
  }
  f << "ids=[";
  for (int i=0; i < 2; i++)
    f << std::hex << (long) input->readULong(4) << std::dec << ",";
  f << "],";
  int val=(int) input->readULong(2); // 0|22d8|4ead|e2c8
  if (val)
    f << "fl?=" << std::hex << val << std::dec << ",";
  for (int i=0; i < 8; i++) {
    static int const(expectedValues[])= {1,4/*or 2*/,3,2,2,1,1,1 };
    val=(int) input->readLong(1);
    if (val!=expectedValues[i])
      f << "g" << i << "=" << val << ",";
  }
  for (int i=0; i < 8; ++i) { // 1,a|e, 0, 21, 3|4, 6|7|9, d|13, 3|5: related to font?
    val=(int) input->readLong(2);
    if (val)
      f << "h" << i << "=" << val << ",";
  }
  val=(int) input->readULong(2); //0|10|3e|50|c8|88|98
  if (val)
    f << "h8=" <<  std::hex << val << std::dec << ",";
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  return true;
}

////////////////////////////////////////////////////////////
// resource fork data
////////////////////////////////////////////////////////////

// read the windows position blocks
bool BeagleWksStructManager::readwPos(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() != 8) {
    MWAW_DEBUG_MSG(("BeagleWksStructManager::readwPos: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  libmwaw::DebugStream f;
  entry.setParsed(true);

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "Entries(Windows):";
  int dim[4];
  for (int i=0; i < 4; ++i)
    dim[i]=(int) input->readLong(2);

  f << "dim=" << dim[1] << "x" << dim[0] << "<->"
    << dim[3] << "x" << dim[2] << ",";
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool BeagleWksStructManager::readFontStyle(MWAWEntry const &entry)
{
  if (!entry.valid() || entry.length() != 8) {
    MWAW_DEBUG_MSG(("BeagleWksStructManager::readFontStyle: the entry is bad\n"));
    return false;
  }

  long pos = entry.begin();
  MWAWInputStreamPtr input = rsrcInput();
  libmwaw::DebugFile &ascFile = rsrcAscii();
  libmwaw::DebugStream f;
  entry.setParsed(true);

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "Entries(FontStyle)[" << std::hex << entry.id() << std::dec << "]:";
  int fSz=(int) input->readLong(2);
  if (fSz) f << "fSz=" << fSz << ",";
  int fl=(int) input->readLong(2);
  if (fl) f << "flags=" << std::hex << fl << std::dec << ",";
  int id=(int) input->readLong(2);
  if (id) f << "fId=" << id << ",";
  int val=(int) input->readLong(2);
  if (val) f << "color?=" << val << ",";
  ascFile.addPos(pos-4);
  ascFile.addNote(f.str().c_str());
  return true;
}

bool BeagleWksStructManager::readPicture(int pId, librevenge::RVNGBinaryData &pict)
{
  MWAWRSRCParserPtr rsrcParser = m_parserState->m_rsrcParser;
  if (!rsrcParser) {
    static bool first=true;
    if (first) {
      MWAW_DEBUG_MSG(("BeagleWksStructManager::readPicture: need access to resource fork to retrieve picture content\n"));
      first=false;
    }
    return true;
  }

  std::multimap<std::string, MWAWEntry> &entryMap =
    rsrcParser->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::const_iterator it
    =entryMap.find("edtp");
  MWAWEntry pictEntry;
  while (it!=entryMap.end()) {
    if (it->first!="edtp")
      break;
    MWAWEntry const &entry=it++->second;
    if (entry.id()!=pId)
      continue;
    entry.setParsed(true);
    pictEntry=entry;
    break;
  }
  if (!pictEntry.valid()) {
    MWAW_DEBUG_MSG(("BeagleWksStructManager::readPicture: can not find picture %d\n", pId));
    return false;
  }

  MWAWInputStreamPtr input = rsrcInput();
  input->seek(pictEntry.begin(), librevenge::RVNG_SEEK_SET);
  pict.clear();
  input->readDataBlock(pictEntry.length(), pict);

  libmwaw::DebugFile &ascFile = rsrcAscii();
  libmwaw::DebugStream f;
#ifdef DEBUG_WITH_FILES
  static int volatile pictName = 0;
  f << "PICT" << ++pictName << ".pct";
  libmwaw::Debug::dumpFile(pict, f.str().c_str());
#endif
  ascFile.addPos(pictEntry.begin()-4);
  ascFile.addNote(f.str().c_str());
  ascFile.skipZone(pictEntry.begin(),pictEntry.end()-1);

  return true;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
