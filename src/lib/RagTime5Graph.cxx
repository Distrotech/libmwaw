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
#include "MWAWGraphicShape.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWListener.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSubDocument.hxx"

#include "RagTime5Parser.hxx"
#include "RagTime5StructManager.hxx"

#include "RagTime5Graph.hxx"

#include "libmwaw_internal.hxx"

/** Internal: the structures of a RagTime5Graph */
namespace RagTime5GraphInternal
{

////////////////////////////////////////
//! Internal: the state of a RagTime5Graph
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
RagTime5Graph::RagTime5Graph(RagTime5Parser &parser) :
  m_mainParser(parser), m_structManager(m_mainParser.getStructManager()), m_parserState(parser.getParserState()),
  m_state(new RagTime5GraphInternal::State)
{
}

RagTime5Graph::~RagTime5Graph()
{ }

int RagTime5Graph::version() const
{
  return m_parserState->m_version;
}

int RagTime5Graph::numPages() const
{
  // TODO IMPLEMENT ME
  MWAW_DEBUG_MSG(("RagTime5Graph::numPages: is not implemented\n"));
  return 0;
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// graphic
////////////////////////////////////////////////////////////
bool RagTime5Graph::readGraphicZone(RagTime5StructManager::Zone &zone)
{
  if (!zone.m_graphicZoneId[1])
    return false;
  std::vector<long> decal;
  if (zone.m_graphicZoneId[0])
    readGraphicPositions(zone.m_graphicZoneId[0], decal);
  int const dataId=zone.m_graphicZoneId[1];
  shared_ptr<RagTime5StructManager::Zone> dataZone=m_mainParser.getDataZone(dataId);
  if (!dataZone || !dataZone->m_entry.valid() ||
      dataZone->getKindLastPart(dataZone->m_kinds[1].empty())!="ItemData") {
    MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicZone: the data zone %d seems bad\n", dataId));
    return false;
  }
  dataZone->m_isParsed=true;
  MWAWEntry entry=dataZone->m_entry;
  libmwaw::DebugFile &ascFile=dataZone->ascii();
  libmwaw::DebugStream f;
  f << "Entries(GraphData)[" << *dataZone << "]:";
  ascFile.addPos(entry.end());
  ascFile.addNote("_");
  int N=int(decal.size());
  if (N==0) {
    if (entry.length()>70) {
      MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicZone: does not known to read a block %d without pos\n", dataId));
      f << "##";
      ascFile.addPos(entry.begin());
      ascFile.addNote(f.str().c_str());
      return true;
    }
    N=2;
    decal.push_back(0);
    decal.push_back(entry.length());
  }
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());

  MWAWInputStreamPtr input=dataZone->getInput();
  input->setReadInverted(!dataZone->m_hiLoEndian);
  long debPos=entry.begin();
  long endPos=entry.end();
  for (int i=0; i<N-1; ++i) {
    long pos=decal[size_t(i)];
    long nextPos=decal[size_t(i+1)];
    if (pos<0 || debPos+pos>endPos) {
      MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicZone: can not read the data zone %d-%d seems bad\n", dataId, i));
      continue;
    }
    input->seek(debPos+pos, librevenge::RVNG_SEEK_SET);
    readGraphic(*dataZone, debPos+nextPos, i);
  }
  return true;
}

bool RagTime5Graph::readGraphic(RagTime5StructManager::Zone &zone, long endPos, int n)
{
  MWAWInputStreamPtr input=zone.getInput();
  long pos=input->tell();
  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  f << "GraphData-" << n << ":";
  if (pos+42>endPos) {
    MWAW_DEBUG_MSG(("RagTime5Graph::readGraphic: a graphic seems bad\n"));
    f<<"###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }
  int val=(int) input->readULong(2);
  if (val) f << "fl0=" << std::hex << val << std::dec << ",";
  int fl=(int) input->readULong(2);
  if (fl) f << "fl1=" << std::hex << fl << std::dec << ",";
  for (int i=0; i<7; ++i) { // f1[order?]=0..5c, f3=0..16, f5=0..45, f6=0..58
    val=(int) input->readLong(2);
    if (!val) continue;
    if (i==1)
      f << "order?=" << val << ",";
    else if (i==6)
      f << "order2?=" << val << ",";
    else
      f << "f" << i << "=" << val << ",";
  }
  int type=(int) input->readLong(2);
  switch (type) {
  case 1:
    f << "rect,";
    break;
  case 2:
    f << "rectoval,";
    break;
  case 3:
    f << "circle,";
    break;
  case 4:
    f << "pie,";
    break;
  case 5:
    f << "arc,";
    break;
  case 6:
    f << "line,";
    break;
  case 7:
    f << "poly,";
    break;
  case 8:
    f << "spline,";
    break;
  case 9:
    f << "textbox,";
    break;
  case 0xa:
    f << "poly[regular],";
    break;
  // also b and c
  default:
    MWAW_DEBUG_MSG(("RagTime5Graph::readGraphic: find some unknown type\n"));
    f << "#type=" << type << ",";
    break;
  }
  for (int i=0; i<2; ++i) { // g0=0-42, g1=0-380
    val=(int) input->readLong(2);
    if (!val) continue;
    f << "g" << i << "=" << val << ",";
  }
  float dim[4];
  for (int i=0; i<4; ++i) dim[i]=float(input->readLong(4))/65536.f;
  f << "dim=" << Box2f(Vec2f(dim[0],dim[1]), Vec2f(dim[2],dim[3])) << ",";
  bool done=false;
  long dataPos=input->tell();
  switch (type) {
  case 1: // rect
  case 2: // rectoval
  case 4: // pie
  case 6: // line
  case 9: { // textbox
    bool hasEnd0=false, hasEnd1=false;
    if (fl&0xFF) {
      val=(int) input->readLong(2);
      if (val)
        f << "h0=" << val << ",";
      else
        hasEnd0=true;
      if ((fl&0xBF)==0x20) // find a size with fl0=10,fl1=0x4020|4060
        hasEnd1=true;
    }
    if (input->tell()+8+(hasEnd0 ? 4 : 0)+(hasEnd1 ? 8 : 0)>endPos) {
      MWAW_DEBUG_MSG(("RagTime5Graph::readGraphic: the data size seems too short\n"));
      f << "###sz,";
      input->seek(dataPos, librevenge::RVNG_SEEK_SET);
      break;
    }
    for (int i=0; i<4; ++i) dim[i]=float(input->readLong(4))/65536.f;
    if (dim[2]<=dim[0] || dim[3]<=dim[1]) f << "###";
    f << "dim2=" << Box2f(Vec2f(dim[0],dim[1]), Vec2f(dim[2],dim[3])) << ",";
    done=true;
    if (hasEnd0) {
      val=(int) input->readLong(2);
      if (val!=0x400) // always 400?
        f << "h1=" << val << ",";
      val=(int) input->readLong(2); // 1-3
      if (val) f << "h2=" << val << ",";
    }
    if (hasEnd1)
      f << "sz?=" << float(input->readLong(4))/65536.f << "x" << float(input->readLong(4))/65536.f << ",";
    break;
  }
  default:
    break;
  }
  if (!done || input->tell()!=endPos)
    ascFile.addDelimiter(input->tell(),'|');
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return done;
}

bool RagTime5Graph::readGraphicPositions(int posId, std::vector<long> &listPosition)
{
  if (!posId)
    return false;

  shared_ptr<RagTime5StructManager::Zone> zone=m_mainParser.getDataZone(posId);
  if (!zone || !zone->m_entry.valid() || (zone->m_entry.length()%4) ||
      zone->getKindLastPart(zone->m_kinds[1].empty())!="ItemData") {
    MWAW_DEBUG_MSG(("RagTime5Graph::readGraphicZone: the position zone %d seems bad\n", posId));
    return false;
  }
  MWAWEntry entry=zone->m_entry;
  MWAWInputStreamPtr input=zone->getInput();
  input->setReadInverted(!zone->m_hiLoEndian);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);

  zone->m_isParsed=true;
  libmwaw::DebugStream f;
  f << "Entries(GraphPos)[" << *zone << "]:";

  int N=int(entry.length()/4);
  for (int i=0; i<N; ++i) {
    long ptr=input->readLong(4);
    listPosition.push_back(ptr);
    f << ptr << ",";
  }
  input->setReadInverted(false);
  zone->ascii().addPos(entry.begin());
  zone->ascii().addNote(f.str().c_str());
  zone->ascii().addPos(entry.end());
  zone->ascii().addNote("_");
  return true;
}

////////////////////////////////////////////////////////////
// picture
////////////////////////////////////////////////////////////
bool RagTime5Graph::readPictureList(RagTime5StructManager::Zone &zone, std::vector<int> &listIds)
{
  listIds.resize(0);
  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  if (zone.m_name.empty())
    f << "Entries(PictureList)[" << zone << "]:";
  else
    f << "Entries(" << zone.m_name << ")[pictureList," << zone << "]:";
  MWAWEntry &entry=zone.m_entry;
  zone.m_isParsed=true;
  ascFile.addPos(entry.end());
  ascFile.addNote("_");
  m_mainParser.ascii().addPos(zone.m_defPosition);
  m_mainParser.ascii().addNote("picture[list]");

  if (entry.length()%4) {
    MWAW_DEBUG_MSG(("RagTime5Graph::readPictureList: the entry size seems bad\n"));
    f << "###";
    ascFile.addPos(entry.begin());
    ascFile.addNote(f.str().c_str());
    return false;
  }

  MWAWInputStreamPtr input = zone.getInput();
  input->setReadInverted(!zone.m_hiLoEndian); // checkme never seens
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);

  int N=int(entry.length()/4);
  for (int i=0; i<N; ++i) {
    int val=(int) input->readLong(2); // always 1
    int id=(int) input->readLong(2);
    if (val==1) {
      f << "Data" << id << ",";
      listIds.push_back(id);
    }
    else if (val)
      f << "#" << i << ":" << val << ",";
  }
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  input->setReadInverted(false);
  return true;
}

bool RagTime5Graph::readPicture(RagTime5StructManager::Zone &zone, MWAWEntry &entry, PictureType type)
{
  if (entry.length()<=40)
    return false;
  MWAWInputStreamPtr input = zone.getInput();
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  long val;
  std::string extension("");
  switch (type) {
  case P_Epsf:
    val=(long) input->readULong(4);
    if (val!=(long) 0xc5d0d3c6 && val != (long) 0x25215053) return false;
    extension="eps";
#if 0
    // when header==0xc5d0d3c6, we may want to decompose the data
    input->setReadInverted(true);
    MWAWEntry fEntry[3];
    for (int i=0; i<3; ++i) {
      fEntry[i].setBegin((long) input->readULong(4));
      fEntry[i].setLength((long) input->readULong(4));
      if (!fEntry[i].length()) continue;
      f << "decal" << i << "=" << std::dec << fEntry[i].begin() << "<->" << fEntry[i].end() << std::hex << ",";
      if (fEntry[i].begin()<0x1c ||fEntry[i].end()>zone.m_entry.length()) {
        MWAW_DEBUG_MSG(("RagTime5Graph::readPicture: the address %d seems too big\n", i));
        fEntry[i]=MWAWEntry();
        f << "###";
        continue;
      }
    }
    for (int i=0; i<2; ++i) { // always -1,0
      if (input->tell()>=pos+fDataPos)
        break;
      val=(int) input->readLong(2);
      if (val!=i-1)
        f << "f" << i << "=" << val << ",";
    }
    // now first fEntry=eps file, second WMF?, third=tiff file
#endif
    break;
  case P_Jpeg:
    val=(long) input->readULong(2);
    // jpeg format begin by 0xffd8 and jpeg-2000 format begin by 0000 000c 6a50...
    if (val!=0xffd8 && (val!=0 || input->readULong(4)!=0xc6a50 || input->readULong(4)!=0x20200d0a))
      return false;
    extension="jpg";
    break;
  case P_Pict:
    input->seek(10, librevenge::RVNG_SEEK_CUR);
    val=(long) input->readULong(2);
    if (val!=0x1101 && val !=0x11) return false;
    extension="pct";
    break;
  case P_PNG:
    if (input->readULong(4) != 0x89504e47) return false;
    extension="png";
    break;
  case P_ScreenRep:
    val=(long) input->readULong(1);
    if (val!=0x49 && val!=0x4d) return false;
    MWAW_DEBUG_MSG(("RagTime5Graph::readPicture: find unknown picture format for zone %d\n", zone.m_ids[0]));
    extension="sRep";
    break;
  case P_Tiff:
    val=(long) input->readULong(2);
    if (val!=0x4949 && val != 0x4d4d) return false;
    val=(long) input->readULong(2);
    /* find also frequently 4d4d 00dd b300 d61e here ?
       and one time 4d 00 b3 2a d6 */
    if (val!=0x2a00 && val!=0x002a) {
      static bool first=true;
      if (first) {
        MWAW_DEBUG_MSG(("RagTime5Graph::readPicture: some tiffs seem bad, zone %d, ...\n", zone.m_ids[0]));
        first=false;
      }
      extension="check.tiff";
    }
    else
      extension="tiff";
    break;
  case P_WMF:
    if (input->readULong(4)!=0x01000900) return false;
    extension="wmf";
    break;
  case P_Unknown:
  default:
    return false;
  }
  zone.m_isParsed=true;
  libmwaw::DebugStream f;
  f << "picture[" << extension << "],";
  m_mainParser.ascii().addPos(zone.m_defPosition);
  m_mainParser.ascii().addNote(f.str().c_str());
#ifdef DEBUG_WITH_FILES
  if (type==P_ScreenRep) {
    libmwaw::DebugFile &ascFile=zone.ascii();
    f.str("");
    f << "Entries(ScrRep)[" << zone << "]:";
    ascFile.addPos(zone.m_entry.begin());
    ascFile.addNote(f.str().c_str());
    return true;
  }
  if (zone.isMainInput())
    m_mainParser.ascii().skipZone(entry.begin(), entry.end()-1);
  librevenge::RVNGBinaryData file;
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  input->readDataBlock(entry.length(), file);
  static int volatile pictName = 0;
  f.str("");
  f << "Pict-" << ++pictName << "." << extension;
  libmwaw::Debug::dumpFile(file, f.str().c_str());
#endif
  return true;
}

bool RagTime5Graph::readPictureMatch(RagTime5StructManager::Zone &zone, bool color)
{
  libmwaw::DebugFile &ascFile=zone.ascii();
  libmwaw::DebugStream f;
  if (zone.m_name.empty())
    f << "Entries(" << (color ? "PictureColMatch" : "PictureMatch") << ")[" << zone << "]:";
  else
    f << "Entries(" << zone.m_name << "[" << (color ? "pictureColMatch" : "pictureMatch") << ")[" << zone << "]:";
  MWAWEntry &entry=zone.m_entry;
  zone.m_isParsed=true;
  ascFile.addPos(entry.end());
  ascFile.addNote("_");
  m_mainParser.ascii().addPos(zone.m_defPosition);
  m_mainParser.ascii().addNote(color ? "picture[matchCol]" : "picture[match]");

  int const expectedSz=color ? 42 : 32;
  if (entry.length() != expectedSz) {
    MWAW_DEBUG_MSG(("RagTime5Graph::readPictureMatch: the entry size seems bad\n"));
    f << "###";
    ascFile.addPos(entry.begin());
    ascFile.addNote(f.str().c_str());
    return false;
  }

  MWAWInputStreamPtr input = zone.getInput();
  input->setReadInverted(!zone.m_hiLoEndian); // checkme never seens
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);

  int val;
  for (int i=0; i<4; ++i) {
    static int const(expected[])= {0,0,0x7fffffff,0x7fffffff};
    val=(int) input->readLong(4);
    if (val!=expected[i])
      f << "f" << i << "=" << val << ",";
  }
  int dim[2];
  for (int i=0; i<2; ++i)
    dim[i]=(int) input->readLong(2);
  f << "dim=" << dim[0] << "x" << dim[1] << ",";
  for (int i=0; i<2; ++i) { // f2=0-3, f4=0-1
    val=(int) input->readLong(2);
    if (val)
      f << "f" << i+2 << "=" << val << ",";
  }
  // a very big number
  f << "ID?=" << std::hex << input->readULong(4) << std::dec << ",";
  for (int i=0; i<2; ++i) { // f5=f6=0
    val=(int) input->readLong(2);
    if (val)
      f << "f" << i+4 << "=" << val << ",";
  }
  if (color) {
    for (int i=0; i<5; ++i) { // g0=a|32, g1=0|1, other 0, color and pattern ?
      val=(int) input->readLong(2);
      if (val)
        f << "g" << i << "=" << val << ",";
    }
  }
  input->setReadInverted(false);
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());

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

void RagTime5Graph::flushExtra()
{
  MWAW_DEBUG_MSG(("RagTime5Graph::flushExtra: is not implemented\n"));
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
