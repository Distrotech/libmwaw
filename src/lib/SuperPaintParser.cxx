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

#include "MWAWGraphicListener.hxx"
#include "MWAWHeader.hxx"
#include "MWAWPictBitmap.hxx"
#include "MWAWPictData.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWPosition.hxx"

#include "SuperPaintParser.hxx"

/** Internal: the structures of a SuperPaintParser */
namespace SuperPaintParserInternal
{
////////////////////////////////////////
//! Internal: the state of a SuperPaintParser
struct State {
  //! constructor
  State() : m_kind(MWAWDocument::MWAW_K_DRAW), m_bitmap()
  {
  }
  //! the file type
  MWAWDocument::Kind m_kind;
  /// the bitmap (v1)
  shared_ptr<MWAWPict> m_bitmap;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
SuperPaintParser::SuperPaintParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWGraphicParser(input, rsrcParser, header), m_state()
{
  init();
}

SuperPaintParser::~SuperPaintParser()
{
}

void SuperPaintParser::init()
{
  resetGraphicListener();
  setAsciiName("main-1");

  m_state.reset(new SuperPaintParserInternal::State);

  getPageSpan().setMargins(0.1);
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void SuperPaintParser::parse(librevenge::RVNGDrawingInterface *docInterface)
{
  assert(getInput().get() != 0);

  if (!checkHeader(0L))  throw(libmwaw::ParseException());
  bool ok = false;
  try {
    // create the asciiFile
    ascii().setStream(getInput());
    ascii().open(asciiName());
    checkHeader(0L);
    ok = createZones();
    if (ok) {
      createDocument(docInterface);
      if (m_state->m_kind==MWAWDocument::MWAW_K_PAINT)
        sendBitmap();
      else {
        MWAW_DEBUG_MSG(("SuperPaintParser::parse: sending picture is not implemented\n"));
        ok=false;
      }
    }
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("SuperPaintParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetGraphicListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void SuperPaintParser::createDocument(librevenge::RVNGDrawingInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getGraphicListener()) {
    MWAW_DEBUG_MSG(("SuperPaintParser::createDocument: listener already exist\n"));
    return;
  }

  // create the page list
  MWAWPageSpan ps(getPageSpan());
  ps.setPageSpan(1);
  std::vector<MWAWPageSpan> pageList(1,ps);
  MWAWGraphicListenerPtr listen(new MWAWGraphicListener(*getParserState(), pageList, documentInterface));
  setGraphicListener(listen);
  listen->startDocument();
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool SuperPaintParser::createZones()
{
  MWAWInputStreamPtr input = getInput();
  readHeader();
  input->seek(512,librevenge::RVNG_SEEK_SET);
  bool ok=true;
  if (m_state->m_kind==MWAWDocument::MWAW_K_DRAW)
    ok = readPicture() && false; // not yet implemented
  else
    ok = readBitmap();

  long pos=input->tell();
  if (input->size()==pos+2 && input->readLong(2)==0) {
    ascii().addPos(pos);
    ascii().addNote("_");
  }
  else if (input->size()!=pos) {
    MWAW_DEBUG_MSG(("SuperPaintParser::createZones: find some extra data\n"));
    ascii().addPos(input->tell());
    ascii().addNote("Entries(End):###");
  }
  return ok;
}

////////////////////////////////////////////////////////////
// send data
////////////////////////////////////////////////////////////
bool SuperPaintParser::sendBitmap()
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("SuperPaintParser::sendBitmap: can not find the listener\n"));
    return false;
  }

  librevenge::RVNGBinaryData data;
  std::string type;
  if (!m_state->m_bitmap || !m_state->m_bitmap->getBinary(data,type)) return false;

  MWAWPageSpan const &page=getPageSpan();
  MWAWPosition pos(Vec2f((float)page.getMarginLeft(),(float)page.getMarginRight()),
                   Vec2f((float)page.getPageWidth(),(float)page.getPageLength()), librevenge::RVNG_INCH);
  pos.setRelativePosition(MWAWPosition::Page);
  pos.m_wrapping = MWAWPosition::WNone;
  listener->insertPicture(pos, data, "image/pict");
  return true;
}

////////////////////////////////////////////////////////////
// read the data
////////////////////////////////////////////////////////////
bool SuperPaintParser::readPicture()
{
  MWAWInputStreamPtr input = getInput();
  input->seek(512, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  /* finishme: this code may find the first picture zones, but it will
     probably fail on textbox zones... */
  while (!input->isEnd()) {
    f.str("");
    f << "Entries(Picture):";
    long pos=input->tell();
    int dSz=(int) input->readULong(2);
    if (!input->checkPosition(pos+2)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    input->seek(pos+2+dSz, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool SuperPaintParser::readBitmap(bool onlyCheck)
{
  MWAWInputStreamPtr input = getInput();
  input->seek(512, librevenge::RVNG_SEEK_SET);
  long pos=input->tell();
  libmwaw::DebugStream f;
  f << "Entries(Bitmap):";
  if (!input->checkPosition(pos+48)) {
    MWAW_DEBUG_MSG(("SuperPaintParser::readBitmap: the bitmap header seems too short\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  int val;
  for (int i=0; i<11; ++i) {
    static int const expected[]= {0x11,1,0xa0,0x62,0xc,0xa0,0,0x8e,1,0,0xa};
    val=(int) input->readULong(1);
    if (val!=expected[i])
      f << "f" << i << "=" << val << ",";
  }
  int pictDim[4];
  for (int i=0; i<4; ++i) pictDim[i]=(int) input->readLong(2);
  f << "dim=" << pictDim[1] << "x" << pictDim[0] << "<->" << pictDim[3] << "x" << pictDim[2] << ",";
  if (pictDim[0]<0 || pictDim[1]<0 || pictDim[0]>pictDim[2] || pictDim[1]>pictDim[3]) {
    MWAW_DEBUG_MSG(("SuperPaintParser::readBitmap: the picture dimension seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(pos+19, librevenge::RVNG_SEEK_SET);

  shared_ptr<MWAWPictBitmapIndexed> pict;
  if (!onlyCheck) {
    pict.reset(new MWAWPictBitmapIndexed(Vec2i(pictDim[3],pictDim[2])));
    std::vector<MWAWColor> colors(2);
    colors[0]=MWAWColor::white();
    colors[1]=MWAWColor::black();
    pict->setColors(colors);
  }

  while (!input->isEnd()) {
    pos=input->tell();
    f.str("");
    f << "Bitmap-Row:";
    val=(int) input->readULong(1);
    if (val!=0x98||!input->checkPosition(pos+29)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    int numColByRow=(int) input->readLong(2); // 20|2c|2a
    f << "numCol[byRow]=" << numColByRow << ",";
    int dim[4];
    for (int i=0; i<4; ++i) dim[i]=(int) input->readULong(2);
    f << "dim?=" << dim[1] << "x" << dim[0] << "<->" <<  dim[3] << "x" << dim[2] << ",";
    if (dim[2]<dim[0] || numColByRow*8 < dim[3]-dim[1] || dim[1]<0 || dim[3]>pictDim[3]+8) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    for (int j=0; j<2; ++j) {
      // normally the real picture position (before alignment to 8)
      int dim2[4];
      for (int i=0; i<4; ++i) dim2[i]=(int) input->readULong(2);
      if (dim[0]!=dim2[0]||dim[2]!=dim2[2]||(dim[1]>dim2[1]||dim[1]+8<dim2[1])||
          (dim[3]<dim2[3]||dim[3]>dim2[3]+8))
        f << "dim" << j << "?=" << dim2[1] << "x" << dim2[0] << "<->" <<  dim2[3] << "x" << dim2[2] << ",";
    }
    val=(int) input->readLong(2);
    if (val!=1) f << "f1=" << val << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    for (int r=dim[0]; r<dim[2]; ++r) {
      pos=input->tell();
      int len=(int) input->readULong(1);
      long rEndPos=pos+1+len;
      if (!len || !input->checkPosition(rEndPos)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        break;
      }
      f.str("");
      f << "Bitmap-R" << r << ":";
      int col=dim[1];
      while (input->tell()<rEndPos) {
        int wh=(int) input->readULong(1);
        if (wh>=0x81) {
          int color=(int) input->readULong(1);
          if (onlyCheck) {
            col+=8*(0x101-wh);
          }
          else {
            for (int j=0; j < 0x101-wh; ++j) {
              for (int b=7; b>=0; --b) {
                if (col<pictDim[3])
                  pict->set(col, r, (color>>b)&1);
                ++col;
              }
            }
          }
        }
        else {
          if (input->tell()+wh+1>rEndPos) {
            MWAW_DEBUG_MSG(("SuperPaintParser::readBitmap: can not read row %d\n", r));
            f << "###";
            ascii().addPos(pos);
            ascii().addNote(f.str().c_str());
            return false;
          }
          if (onlyCheck) {
            col+=8*(wh+1);
            input->seek(wh+1, librevenge::RVNG_SEEK_CUR);
          }
          else {
            for (int j=0; j < wh+1; ++j) {
              int color=(int) input->readULong(1);
              for (int b=7; b>=0; --b) {
                if (col<pictDim[3])
                  pict->set(col, r, (color>>b)&1);
                ++col;
              }
            }
          }
        }
        if (col>pictDim[3]+8) {
          MWAW_DEBUG_MSG(("SuperPaintParser::readBitmap: can not read row %d\n", r));
          f << "###";
          ascii().addPos(pos);
          ascii().addNote(f.str().c_str());
          return false;
        }
      }
      f << "col=" << col << ",";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      input->seek(rEndPos, librevenge::RVNG_SEEK_SET);
    }
  }
  pos=input->tell();
  f.str("");
  f << "Bitmap-End:";
  if (!input->checkPosition(pos+6) || input->readULong(1)!=0xa0) {
    MWAW_DEBUG_MSG(("SuperPaintParser::readBitmap: problem when reading the bitmap\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  if (!onlyCheck)
    m_state->m_bitmap=pict;
  for (int i=0; i<3; ++i) {
    int const expected[]= {0,0x8F,0xFF};
    val=(int) input->readULong(1);
    if (val!=expected[i])
      f << "f" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  if (!onlyCheck)
    m_state->m_bitmap=pict;
  return true;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool SuperPaintParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = SuperPaintParserInternal::State();
  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork() || !input->checkPosition(512))
    return false;

  int const vers=1;
  libmwaw::DebugStream f;
  f << "FileHeader:";
  input->seek(0, librevenge::RVNG_SEEK_SET);
  if (input->readULong(2)!=0x1000) return false;
  int dim[4];
  for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
  if (dim[0]||dim[1]||dim[2]||dim[3])
    f << "bitmap[content]=" << dim[1] << "x" << dim[0] << "<->" << dim[3] << "x" << dim[2] << ",";
  int val=(int) input->readLong(2);
  switch (val) {
  case 1:
    m_state->m_kind=MWAWDocument::MWAW_K_PAINT;
    f << "paint,";
    break;
  case 2: // not yet implemented
#ifdef DEBUG
    f << "draw,";
    break;
#else
    return false;
#endif
  default:
    return false;
  }
  val=(int) input->readLong(1);
  if (val==1) f << "hasPrintInfo?,";
  else if (val) f << "#f0=" << val << ",";
  val=(int) input->readLong(1);
  if (val==8) f << "color?,";
  else if (val) f << "#f1=" << val << ",";
  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  if (strict) {
    if (m_state->m_kind==MWAWDocument::MWAW_K_PAINT) {
      if (!readBitmap(true))
        return false;
    }
  }

  setVersion(vers);
  if (header)
    header->reset(MWAWDocument::MWAW_T_SUPERPAINT, vers, m_state->m_kind);
  input->seek(512,librevenge::RVNG_SEEK_SET);

  return true;
}

////////////////////////////////////////////////////////////
// try to read the header zone
////////////////////////////////////////////////////////////
bool SuperPaintParser::readHeader()
{
  MWAWInputStreamPtr input = getInput();
  if (!input->checkPosition(512)) {
    MWAW_DEBUG_MSG(("SuperPaintParser::readHeader: the header zone seems too short\n"));
    ascii().addPos(14);
    ascii().addNote("Entries(Header):#");
    return false;
  }
  input->seek(14,librevenge::RVNG_SEEK_SET);
  if (!readPrintInfo()) {
    ascii().addPos(14);
    ascii().addNote("Entries(PrintInfo):#");
    input->seek(14+120, librevenge::RVNG_SEEK_SET);
  }
  long pos=input->tell();
  libmwaw::DebugStream f;
  f << "Entries(Header):";
  int val;
  for (int i=0; i<6; ++i) {
    int const expected[]= {0x24,1,0,0/*or3*/,0,0x24 /* or 32*/};
    val=(int) input->readLong(1);
    if (val != expected[i]) f << "f" << i << "=" << val << ",";
  }
  for (int i=0; i<60; ++i) { // always 0?
    val=(int) input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "Header-II:";
  for (int i=0; i<126; ++i) { // always 0?
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// try to read the print info zone
////////////////////////////////////////////////////////////
bool SuperPaintParser::readPrintInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  long endPos=pos+120;
  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("SuperPaintParser::readPrintInfo: file seems too short\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(PrintInfo):";
  libmwaw::PrinterInfo info;
  if (!info.read(input)) {
    MWAW_DEBUG_MSG(("SuperPaintParser::readPrintInfo: can not read print info\n"));
    return false;
  }
  f << info;
  Vec2i paperSize = info.paper().size();
  Vec2i pageSize = info.page().size();
  if (pageSize.x() <= 0 || pageSize.y() <= 0 ||
      paperSize.x() <= 0 || paperSize.y() <= 0) {
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    return true;
  }

  // define margin from print info
  Vec2i lTopMargin= -1 * info.paper().pos(0);
  Vec2i rBotMargin=info.paper().size() - info.page().size();

  // move margin left | top
  int decalX = lTopMargin.x() > 14 ? lTopMargin.x()-14 : 0;
  int decalY = lTopMargin.y() > 14 ? lTopMargin.y()-14 : 0;
  lTopMargin -= Vec2i(decalX, decalY);
  rBotMargin += Vec2i(decalX, decalY);

  // decrease right | bottom
  int rightMarg = rBotMargin.x() -50;
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
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
