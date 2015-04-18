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
#include <set>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWHeader.hxx"
#include "MWAWPictBitmap.hxx"
#include "MWAWPosition.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWGraphicListener.hxx"
#include "MWAWSubDocument.hxx"

#include "BeagleWksStructManager.hxx"

#include "BeagleWksBMParser.hxx"

/** Internal: the structures of a BeagleWksBMParser */
namespace BeagleWksBMParserInternal
{
////////////////////////////////////////
//! Internal: a bitmap of a BeagleWksBMParser
struct Bitmap {
  //! the constructor
  Bitmap() : m_colorBytes(1), m_dim(0,0), m_colorList()
  {
  }
  //! the number of color bytes (1: means black/white, 8: means color)
  int m_colorBytes;
  //! the bitmap dimension
  MWAWVec2i m_dim;
  //! the color list
  std::vector<MWAWColor> m_colorList;
};

////////////////////////////////////////
//! Internal: the state of a BeagleWksBMParser
struct State {
  //! constructor
  State() :  m_graphicBegin(-1), m_bitmap(), m_pict(), m_typeEntryMap(),
    m_actPage(0), m_numPages(0), m_headerHeight(0), m_footerHeight(0)
  {
  }

  /** the graphic begin position */
  long m_graphicBegin;
  /** the bitmap data */
  Bitmap m_bitmap;
  /// the bitmap
  shared_ptr<MWAWPict> m_pict;
  /** the type entry map */
  std::multimap<std::string, MWAWEntry> m_typeEntryMap;

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;

  int m_headerHeight /** the header height if known */,
      m_footerHeight /** the footer height if known */;
};

}


////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
BeagleWksBMParser::BeagleWksBMParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWGraphicParser(input, rsrcParser, header), m_state(), m_structureManager()
{
  init();
}

BeagleWksBMParser::~BeagleWksBMParser()
{
}

void BeagleWksBMParser::init()
{
  resetGraphicListener();
  setAsciiName("main-1");

  m_state.reset(new BeagleWksBMParserInternal::State);
  m_structureManager.reset(new BeagleWksStructManager(getParserState()));

  // reduce the margin (in case, the page is not defined)
  getPageSpan().setMargins(0.1);
}

MWAWInputStreamPtr BeagleWksBMParser::rsrcInput()
{
  return getRSRCParser()->getInput();
}

libmwaw::DebugFile &BeagleWksBMParser::rsrcAscii()
{
  return getRSRCParser()->ascii();
}

////////////////////////////////////////////////////////////
// position and height
////////////////////////////////////////////////////////////
MWAWVec2f BeagleWksBMParser::getPageLeftTop() const
{
  return MWAWVec2f(float(getPageSpan().getMarginLeft()),
                   float(getPageSpan().getMarginTop()+m_state->m_headerHeight/72.0));
}

////////////////////////////////////////////////////////////
// new page
////////////////////////////////////////////////////////////
void BeagleWksBMParser::newPage(int number)
{
  if (number <= m_state->m_actPage || number > m_state->m_numPages)
    return;

  while (m_state->m_actPage < number) {
    m_state->m_actPage++;
    if (!getGraphicListener() || m_state->m_actPage == 1)
      continue;
    getGraphicListener()->insertBreak(MWAWGraphicListener::PageBreak);
  }
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void BeagleWksBMParser::parse(librevenge::RVNGDrawingInterface *docInterface)
{
  if (!getInput().get() || !checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = false;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());

    checkHeader(0L);
    ok = createZones();
    if (ok) {
      createDocument(docInterface);
      sendPageFrames();
      sendBitmap();
    }
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("BeagleWksBMParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetGraphicListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void BeagleWksBMParser::createDocument(librevenge::RVNGDrawingInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getGraphicListener()) {
    MWAW_DEBUG_MSG(("BeagleWksBMParser::createDocument: listener already exist\n"));
    return;
  }

  // update the page
  m_state->m_actPage = 0;

  // create the page list
  int numPages = 1;
  m_state->m_numPages = numPages;

  std::vector<MWAWPageSpan> pageList;
  MWAWPageSpan ps(getPageSpan());
  ps.setPageSpan(numPages);
  pageList.push_back(ps);

  MWAWGraphicListenerPtr listen(new MWAWGraphicListener(*getParserState(), pageList, documentInterface));
  setGraphicListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool BeagleWksBMParser::createZones()
{
  readRSRCZones();
  MWAWInputStreamPtr input = getInput();
  if (input->seek(66, librevenge::RVNG_SEEK_SET) || !readPrintInfo())
    return false;
  long pos = input->tell();
  if (!input->checkPosition(pos+70)) {
    MWAW_DEBUG_MSG(("BeagleWksBMParser::createZones: the file can not contains zones\n"));
    return false;
  }

  // now read the list of zones
  libmwaw::DebugStream f;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  f << "Entries(Zones):";
  for (int i=0; i<7; ++i) { // checkme: at least 2 zones, i=2 is not a zone, maybe 7
    MWAWEntry entry;
    entry.setBegin(input->readLong(4));
    entry.setLength(input->readLong(4));
    entry.setId((int) input->readLong(2));
    if (entry.length()==0) continue;
    entry.setType(i==1?"Frame":"Unknown");
    f << entry.type() << "[" << entry.id() << "]="
      << std::hex << entry.begin() << "<->" << entry.end() << ",";
    if (!entry.valid() || !input->checkPosition(entry.end())) {
      f << "###";
      if (i<2) {
        MWAW_DEBUG_MSG(("BeagleWksBMParser::createZones: can not read the header zone, stop\n"));
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());
        return false;
      }
      if (i!=2) {
        MWAW_DEBUG_MSG(("BeagleWksBMParser::createZones: can not zones entry %d\n",i));
      }
      continue;
    }
    m_state->m_typeEntryMap.insert
    (std::multimap<std::string, MWAWEntry>::value_type(entry.type(),entry));
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  // now parse the different zones
  std::multimap<std::string, MWAWEntry>::iterator it;
  it=m_state->m_typeEntryMap.find("FontNames");
  if (it!=m_state->m_typeEntryMap.end())
    m_structureManager->readFontNames(it->second);
  it=m_state->m_typeEntryMap.find("Frame");
  if (it!=m_state->m_typeEntryMap.end())
    m_structureManager->readFrame(it->second);

  // now parse the different zones
  for (it=m_state->m_typeEntryMap.begin(); it!=m_state->m_typeEntryMap.end(); ++it) {
    MWAWEntry const &entry=it->second;
    if (entry.isParsed())
      continue;
    f.str("");
    f << "Entries(" << entry.type() << ")[" << entry.id() << "]:";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
    ascii().addPos(entry.end());
    ascii().addNote("_");
  }

  input->seek(m_state->m_graphicBegin, librevenge::RVNG_SEEK_SET);
  if (!readPrefColorMap()) {
    ascii().addPos(pos);
    ascii().addNote("Entries(ColorMap)[Pref]:###");
    return false;
  }

  if (!readBitmap() || !m_state->m_pict) return false;
  if (!input->isEnd()) {
    MWAW_DEBUG_MSG(("BeagleWksBMParser::createZones: find some extra data\n"));
    ascii().addPos(input->tell());
    ascii().addNote("Entries(ZoneEnd)");
  }
  return true;
}

bool BeagleWksBMParser::readRSRCZones()
{
  MWAWRSRCParserPtr rsrcParser = getRSRCParser();
  if (!rsrcParser)
    return true;

  std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
  std::multimap<std::string, MWAWEntry>::iterator it;
  // the 1 zone
  char const *(zNames[]) = {"wPos", "DMPF" };
  for (int z = 0; z < 2; ++z) {
    it = entryMap.lower_bound(zNames[z]);
    while (it != entryMap.end()) {
      if (it->first != zNames[z])
        break;
      MWAWEntry const &entry = it++->second;
      switch (z) {
      case 0: // 1001
        m_structureManager->readwPos(entry);
        break;
      case 1: // find in one file with id=4661 6a1f 4057
        m_structureManager->readFontStyle(entry);
        break;
      /* find also
         - edpt: see sendPicture
         - DMPP: the paragraph style
         - sect and alis: position?, alis=filesystem alias(dir, filename, path...)
      */
      default:
        break;
      }
    }
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the bitmap data
////////////////////////////////////////////////////////////
bool BeagleWksBMParser::readBitmap()
{
  MWAWInputStreamPtr input = getInput();
  BeagleWksBMParserInternal::Bitmap &bitmap=m_state->m_bitmap;
  bool isColor=bitmap.m_colorBytes==8;

  long pos = input->tell();
  long dSz=(long) input->readULong(2);
  long endPos=pos+dSz;
  if (dSz<38 || !input->checkPosition(endPos)) {
    ascii().addPos(pos);
    ascii().addNote("Entries(Bitmap):###");
    return false;
  }

  libmwaw::DebugStream f;
  f << "Entries(Bitmap):";
  int dim[4];
  for (int i=0; i<4; ++i) dim[i]=(int)input->readULong(2);
  if (dim[0]||dim[1]||dim[2]!=bitmap.m_dim[1]||dim[3]!=bitmap.m_dim[0])
    f << "dim=" << dim[1] << "x" << dim[0] << "<->" << dim[3] << "x" << dim[2] << ",";
  int val;
  for (int i=0; i<8; ++i) {
    int const(expected[])= {0x11,0x2ff, 0xc00, -1, -1, 0, 0, 0 };
    val=(int) input->readLong(2);
    if (val!=expected[i])
      f << "f" << i << "=" << val << ",";
  }
  for (int i=0; i<4; ++i) dim[i]=(int)input->readULong(2);
  if (dim[0]||dim[2]||dim[1]!=bitmap.m_dim[0]||dim[3]!=bitmap.m_dim[1])
    f << "dim1=" << dim[0] << "x" << dim[2] << "<->" << dim[1] << "x" << dim[3] << ",";
  for (int i=0; i<5; ++i) {
    int const(expected[])= {0, 0, 0, 0xa0, 0x82 };
    val=(int) input->readLong(2);
    if (val!=expected[i])
      f << "g" << i << "=" << val << ",";
  }
  val=(int) input->readULong(2);
  if (val==0x1e) {
    f << "f1e,";
    val=(int) input->readULong(2);
  }
  if (val!=1) {
    MWAW_DEBUG_MSG(("BeagleWksBMParser::readBitmap: we may have a problem\n"));
    f << "h0=" << val << ",";
  }
  val=(int) input->readULong(2);
  if (val != 0xa) f << "h1=" << val << ",";
  for (int i=0; i<4; ++i) dim[i]=(int)input->readULong(2);
  if (dim[0]||dim[1]||dim[2]!=bitmap.m_dim[1]||dim[3]!=bitmap.m_dim[0])
    f << "dim2=" << dim[1] << "x" << dim[0] << "<->" << dim[3] << "x" << dim[2] << ",";
  val=(int) input->readULong(2);
  if (val != 0x98) f << "h2=" << val << ",";
  f << "fl?=" << std::hex << input->readULong(2) << std::dec << ","; // 48[BW]|8240[Color]
  for (int i=0; i<4; ++i) dim[i]=(int)input->readULong(2);
  if (dim[0]||dim[1]||dim[2]!=bitmap.m_dim[1]||dim[3]!=bitmap.m_dim[0])
    f << "dim3=" << dim[1] << "x" << dim[0] << "<->" << dim[3] << "x" << dim[2] << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  if (isColor) {
    pos=input->tell();
    if (!readColorMap() || bitmap.m_colorList.empty()) {
      ascii().addPos(pos);
      ascii().addNote("ColorMap:###");
      return false;
    }
  }

  pos=input->tell();
  if (pos+18>endPos || !input->checkPosition(pos+17)) {
    MWAW_DEBUG_MSG(("BeagleWksBMParser::readBitmap: the zone B seems too short\n"));
    return false;
  }
  f.str("");
  f << "Bitmap-A:";
  for (int j=0; j < 2; ++j) {
    for (int i=0; i<4; ++i) dim[i]=(int)input->readULong(2);
    if (dim[0]||dim[1]||dim[2]!=bitmap.m_dim[1]||dim[3]!=bitmap.m_dim[0])
      f << "dim" << j << "=" << dim[1] << "x" << dim[0] << "<->" << dim[3] << "x" << dim[2] << ",";
  }
  val=(int) input->readLong(2);
  if (val) f << "f0=" << val << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  int sSz=isColor ? 2 : 1;
  int numColors=(int) bitmap.m_colorList.size();
  shared_ptr<MWAWPictBitmapIndexed> pict(new MWAWPictBitmapIndexed(bitmap.m_dim));
  if (isColor)
    pict->setColors(bitmap.m_colorList);
  else {
    std::vector<MWAWColor> colors(2);
    colors[0]=MWAWColor::white();
    colors[1]=MWAWColor::black();
    pict->setColors(colors);
  }
  for (int r=0; r<bitmap.m_dim[1]; ++r) {
    pos=input->tell();
    int sz=(int) input->readULong(sSz);
    f.str("");
    f << "Bitmap-R" << r << ":";
    long rEndPos=pos+sSz+sz;
    if (!sz || rEndPos>endPos) {
      MWAW_DEBUG_MSG(("BeagleWksBMParser::readBitmap: can read row %d\n", r));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return false;
    }
    int col=0;
    while (input->tell()<rEndPos) {
      int wh=(int) input->readULong(1);
      if (wh>=0x81) {
        int color=(int) input->readULong(1);
        for (int j=0; j < 0x101-wh; ++j) {
          if (col>=bitmap.m_dim[0]) break;
          if (isColor) {
            if (color>=numColors) break;
            pict->set(col++, r, color);
          }
          else {
            for (int b=7; b>=0; --b) {
              if (col>=bitmap.m_dim[0]) break;
              pict->set(col++, r, (color>>b)&1);
            }
          }
        }
      }
      else {
        for (int j=0; j < wh+1; ++j) {
          int color=(int) input->readULong(1);
          if (col>=bitmap.m_dim[0]) continue;
          if (isColor) {
            if (color>=numColors) break;
            pict->set(col++, r, color);
          }
          else {
            for (int b=7; b>=0; --b) {
              if (col>=bitmap.m_dim[0]) break;
              pict->set(col++, r, (color>>b)&1);
            }
          }
        }
      }
      if (input->tell()>rEndPos) {
        MWAW_DEBUG_MSG(("BeagleWksBMParser::readBitmap: can read row data %d\n", r));
        f << "###";
        ascii().addPos(pos);
        ascii().addNote(f.str().c_str());
        return false;
      }
    }
    if (col!=bitmap.m_dim[0]) {
      MWAW_DEBUG_MSG(("BeagleWksBMParser::readBitmap: row %d has a unexpected number of pixels\n", r));
      f << "#" << col << "!=" << bitmap.m_dim[0] << ",";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return false;
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(rEndPos, librevenge::RVNG_SEEK_SET);
  }
  m_state->m_pict=pict;
  pos=input->tell();
  f.str("");
  f << "Bitmap-Data[end]:";
  if (pos+7==endPos) { // no sure when it exists
    val=(int)input->readLong(1);
    if (val) f << "unkn=" << val << ",";
  }
  if (input->tell()+6!=endPos) {
    MWAW_DEBUG_MSG(("BeagleWksBMParser::readBitmap: the end seems bad\n"));
    f << "###";
  }
  else {
    for (int i=0; i<3; ++i) // f0=a0, f1=83, f2=ff
      f << "f" << i << "=" << std::hex << input->readLong(2) << std::dec << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

bool BeagleWksBMParser::readPrefColorMap()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  if (!input->checkPosition(pos+38)) {
    MWAW_DEBUG_MSG(("BeagleWksBMParser::readPrefColorMap: the bitmap header seems too short\n"));
    ascii().addPos(input->tell());
    ascii().addNote("Entries(ColorMap)[pref]:###");
    return false;
  }
  BeagleWksBMParserInternal::Bitmap &bitmap=m_state->m_bitmap;
  libmwaw::DebugStream f;
  f << "Entries(ColorMap)[pref,header]:";
  int val=(int) input->readLong(2);
  if (val) f << "f0=" << val << ",";
  f << "sz[bitmap]=" << std::hex << input->readULong(2) << std::dec << ",";
  for (int i=0; i<4; ++i) { // 0,0,8,8 ? f2-f3=colorMap size?
    val=(int) input->readLong(1);
    if (val) f << "f" << i+1 << "=" << val << ",";
  }
  int dim[2];
  for (int i=0; i<2; ++i) dim[i]=(int) input->readLong(2);
  bitmap.m_dim=MWAWVec2i(dim[1], dim[0]);
  f << "dim=" << bitmap.m_dim << ",";
  bitmap.m_colorBytes=(int) input->readLong(2);
  switch (bitmap.m_colorBytes) {
  case 1:
    f << "BW,";
    break;
  case 8:
    f << "color,";
    break;
  default:
    MWAW_DEBUG_MSG(("BeagleWksBMParser::readPrefColorMap: the number of bytes seems bad\n"));
    f << "#bytes=" << bitmap.m_colorBytes << ",";
    break;
  }
  for (int i=0; i<11; ++i) {
    val=(int) input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  int maxColor=(int) input->readULong(2);
  f << "N="<< maxColor+1 << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  if (!input->checkPosition(pos+(1+maxColor)*8))
    return false;

  for (int i=0; i<= maxColor; ++i) {
    f.str("");
    f << "ColorMap-P" << i << ":";
    pos = input->tell();
    int id=(int) input->readLong(2);
    if (id!=i) f << "#id=" << id << ",";
    unsigned char col[3];
    for (int c = 0; c < 3; c++) col[c] = (unsigned char)(input->readULong(2)>>8);
    MWAWColor color(col[0], col[1], col[2]);
    f << color << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool BeagleWksBMParser::readColorMap()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  if (!input->checkPosition(pos+44)) {
    MWAW_DEBUG_MSG(("BeagleWksBMParser::readColorMap: the bitmap header seems too short\n"));
    ascii().addPos(input->tell());
    ascii().addNote("ColorMap:###");
    return false;
  }
  libmwaw::DebugStream f;
  f << "ColorMap[header]:";
  int val;
  for (int i=0; i<14; ++i) { // always f4=72,f6=72,f9=8,f10=1,f11=8 ?
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  f << "id=" << std::hex << input->readULong(4) << std::dec << ",";
  for (int i=0; i<5; ++i) { // 0,0,0,sz?, 0
    val=(int) input->readLong(2);
    if (val) f << "g" << i << "=" << std::hex << val << std::dec << ",";
  }
  int maxColor=(int) input->readULong(2);
  f << "N="<< maxColor+1 << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  if (!input->checkPosition(pos+(1+maxColor)*8))
    return false;

  BeagleWksBMParserInternal::Bitmap &bitmap=m_state->m_bitmap;
  bitmap.m_colorList.resize(size_t(maxColor+1));
  for (int i=0; i<= maxColor; ++i) {
    f.str("");
    f << "ColorMap-" << i << ":";
    pos = input->tell();
    int id=(int) input->readLong(2);
    if (id!=i) f << "#id=" << id << ",";
    unsigned char col[3];
    for (int c = 0; c < 3; c++) col[c] = (unsigned char)(input->readULong(2)>>8);
    MWAWColor color(col[0], col[1], col[2]);
    bitmap.m_colorList[size_t(i)]=color;
    f << color << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// read the print info
////////////////////////////////////////////////////////////
bool BeagleWksBMParser::readPrintInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos = input->tell();
  if (!input->checkPosition(pos+0x70))
    return false;

  libmwaw::DebugStream f;
  // print info
  libmwaw::PrinterInfo info;
  if (!info.read(input)) return false;
  f << "Entries(PrintInfo):"<< info;

  MWAWVec2i paperSize = info.paper().size();
  MWAWVec2i pageSize = info.page().size();
  if (pageSize.x() <= 0 || pageSize.y() <= 0 ||
      paperSize.x() <= 0 || paperSize.y() <= 0) return false;

  // define margin from print info
  MWAWVec2i lTopMargin= -1 * info.paper().pos(0);
  MWAWVec2i rBotMargin=info.paper().pos(1) - info.page().pos(1);

  // move margin left | top
  int decalX = lTopMargin.x() > 14 ? lTopMargin.x()-14 : 0;
  int decalY = lTopMargin.y() > 14 ? lTopMargin.y()-14 : 0;
  lTopMargin -= MWAWVec2i(decalX, decalY);
  rBotMargin += MWAWVec2i(decalX, decalY);

  // decrease right | bottom
  int rightMarg = rBotMargin.x() -10;
  if (rightMarg < 0) rightMarg=0;
  int botMarg = rBotMargin.y() -50;
  if (botMarg < 0) botMarg=0;

  getPageSpan().setMarginTop(lTopMargin.y()/72.0);
  getPageSpan().setMarginBottom(botMarg/72.0);
  getPageSpan().setMarginLeft(lTopMargin.x()/72.0);
  getPageSpan().setMarginRight(rightMarg/72.0);
  getPageSpan().setFormLength(paperSize.y()/72.);
  getPageSpan().setFormWidth(paperSize.x()/72.);

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(pos+0x78, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != pos+0x78) {
    MWAW_DEBUG_MSG(("BeagleWksBMParser::readPrintInfo: file is too short\n"));
    return false;
  }
  ascii().addPos(input->tell());

  return true;
}

////////////////////////////////////////////////////////////
// send data
////////////////////////////////////////////////////////////
bool BeagleWksBMParser::sendBitmap()
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("BeagleWksBMParser::sendBitmap: can not find the listener\n"));
    return false;
  }

  librevenge::RVNGBinaryData data;
  std::string type;
  if (!m_state->m_pict || !m_state->m_pict->getBinary(data,type)) return false;

  MWAWPageSpan const &page=getPageSpan();
  MWAWPosition pos(MWAWVec2f((float)page.getMarginLeft(),(float)page.getMarginRight()),
                   MWAWVec2f((float)page.getPageWidth(),(float)page.getPageLength()), librevenge::RVNG_INCH);
  pos.setRelativePosition(MWAWPosition::Page);
  pos.m_wrapping = MWAWPosition::WNone;
  listener->insertPicture(pos, data, "image/pict");
  return true;
}

bool BeagleWksBMParser::sendPageFrames()
{
  std::map<int, BeagleWksStructManager::Frame> const &frameMap = m_structureManager->getIdFrameMap();
  std::map<int, BeagleWksStructManager::Frame>::const_iterator it;
  for (it=frameMap.begin(); it!=frameMap.end(); ++it)
    sendFrame(it->second);
  return true;
}

bool BeagleWksBMParser::sendFrame(BeagleWksStructManager::Frame const &frame)
{
  MWAWPosition fPos(MWAWVec2f(0,0), frame.m_dim, librevenge::RVNG_POINT);

  fPos.setPagePos(frame.m_page > 0 ? frame.m_page : 1, frame.m_origin);
  fPos.setRelativePosition(MWAWPosition::Page);
  fPos.m_wrapping = frame.m_wrap==0 ? MWAWPosition::WNone : MWAWPosition::WDynamic;

  MWAWGraphicStyle style=MWAWGraphicStyle::emptyStyle();
  style.setBorders(frame.m_bordersSet, frame.m_border);
  return sendPicture(frame.m_pictId, fPos, style);
}

// read/send picture (edtp resource)
bool BeagleWksBMParser::sendPicture
(int pId, MWAWPosition const &pictPos, MWAWGraphicStyle const &style)
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("BeagleWksBMParser::sendPicture: can not find the listener\n"));
    return false;
  }
  MWAWRSRCParserPtr rsrcParser = getRSRCParser();
  if (!rsrcParser) {
    static bool first=true;
    if (first) {
      MWAW_DEBUG_MSG(("BeagleWksBMParser::sendPicture: need access to resource fork to retrieve picture content\n"));
      first=false;
    }
    return true;
  }

  librevenge::RVNGBinaryData data;
  if (!m_structureManager->readPicture(pId, data))
    return false;

  listener->insertPicture(pictPos, data, "image/pict", style);
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool BeagleWksBMParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = BeagleWksBMParserInternal::State();
  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork() || !input->checkPosition(66))
    return false;

  libmwaw::DebugStream f;
  f << "FileHeader:";

  input->seek(0, librevenge::RVNG_SEEK_SET);
  if (input->readLong(2)!=0x4257 || input->readLong(2)!=0x6b73 ||
      input->readLong(2)!=0x4257 || input->readLong(2)!=0x7074 ||
      input->readLong(2)!=0x4257 || input->readLong(2)!=0x7074) {
    return false;
  }
  for (int i=0; i < 9; ++i) { // f2=f6=1 other 0
    int val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  setVersion(1);

  if (header)
    header->reset(MWAWDocument::MWAW_T_BEAGLEWORKS, 1, MWAWDocument::MWAW_K_PAINT);

  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  long pos=input->tell();
  f.str("");
  f << "FileHeader-II:";
  m_state->m_graphicBegin=input->readLong(4);
  if (!input->checkPosition(m_state->m_graphicBegin)) {
    MWAW_DEBUG_MSG(("BeagleWksBMParser::checkHeader: can not read the graphic position\n"));
    return false;
  }
  f << "graphic[ptr]=" << std::hex << m_state->m_graphicBegin << std::dec << ",";
  for (int i=0; i < 11; ++i) { // f2=0x50c|58c|5ac f3=f5=9
    long val=input->readLong(2);
    if (val) f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  MWAWEntry entry;
  entry.setBegin(input->readLong(4));
  entry.setLength(input->readLong(4));
  entry.setId((int) input->readLong(2)); // in fact nFonts
  entry.setType("FontNames");
  f << "fontNames[ptr]=" << std::hex << entry.begin() << "<->" << entry.end()
    << std::dec << ",nFonts=" << entry.id() << ",";
  if (entry.length() && (!entry.valid() || !input->checkPosition(entry.end()))) {
    MWAW_DEBUG_MSG(("BeagleWksBMParser::checkHeader: can not read the font names position\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }

  m_state->m_typeEntryMap.insert
  (std::multimap<std::string, MWAWEntry>::value_type(entry.type(),entry));
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  if (strict && !readPrintInfo())
    return false;
  ascii().addPos(66);
  ascii().addNote("_");

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
