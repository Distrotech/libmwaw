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
#include "MWAWPosition.hxx"

#include "GreatWksBMParser.hxx"

/** Internal: the structures of a GreatWksBMParser */
namespace GreatWksBMParserInternal
{
////////////////////////////////////////
//! Internal: the state of a GreatWksBMParser
struct State {
  //! constructor
  State() : m_picture(), m_bitmap()
  {
  }
  //! the picture entry (v2)
  MWAWEntry m_picture;
  /// the bitmap (v1)
  shared_ptr<MWAWPict> m_bitmap;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
GreatWksBMParser::GreatWksBMParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWGraphicParser(input, rsrcParser, header), m_state()
{
  init();
}

GreatWksBMParser::~GreatWksBMParser()
{
}

void GreatWksBMParser::init()
{
  resetGraphicListener();
  setAsciiName("main-1");

  m_state.reset(new GreatWksBMParserInternal::State);

  getPageSpan().setMargins(0.1);
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void GreatWksBMParser::parse(librevenge::RVNGDrawingInterface *docInterface)
{
  assert(getInput().get() != 0 && getRSRCParser());

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
      if (version()==1)
        sendBitmap();
      else
        sendPicture();
    }
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("GreatWksBMParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetGraphicListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void GreatWksBMParser::createDocument(librevenge::RVNGDrawingInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getGraphicListener()) {
    MWAW_DEBUG_MSG(("GreatWksBMParser::createDocument: listener already exist\n"));
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
bool GreatWksBMParser::createZones()
{
  MWAWInputStreamPtr input = getInput();
  if (input->size()<512) return false;
#ifdef DEBUG
  libmwaw::DebugStream f;
  f << "FileHeader:";
  input->seek(0, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<256; i++) { // normally 0
    int val=(int) input->readLong(2);
    if (val)
      f << "f" << i << "=" << val << ",";
  }
  ascii().addPos(0);
  ascii().addNote(f.str().c_str());
#endif
  if (version()==2) {
    m_state->m_picture.setBegin(512);
    m_state->m_picture.setEnd(input->size());
    return true;
  }
  if (!readBitmap()) return false;
  if (!input->isEnd()) {
    MWAW_DEBUG_MSG(("GreatWksBMParser::createZones: find some extra data\n"));
    ascii().addPos(input->tell());
    ascii().addNote("Entries(End):###");
  }
  return true;
}

////////////////////////////////////////////////////////////
// send data
////////////////////////////////////////////////////////////
bool GreatWksBMParser::sendBitmap()
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("GreatWksBMParser::sendBitmap: can not find the listener\n"));
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

// the picture
bool GreatWksBMParser::sendPicture()
{
  MWAWListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("GreatWksBMParser::sendPicture: can not find the listener\n"));
    return false;
  }
  MWAWEntry const &entry=m_state->m_picture;
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("GreatWksBMParser::sendPicture: can not find the picture entry\n"));
    return false;
  }

  MWAWInputStreamPtr input = getInput();
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  shared_ptr<MWAWPict> thePict(MWAWPictData::get(input, (int)entry.length()));
  if (!thePict) {
    MWAW_DEBUG_MSG(("GreatWksBMParser::sendPicture: can not retrieve the picture\n"));
    return false;
  }
  librevenge::RVNGBinaryData data;
  std::string type;
  if (!thePict->getBinary(data,type)) {
    MWAW_DEBUG_MSG(("GreatWksBMParser::sendPicture: can not retrieve the picture data\n"));
    return false;
  }
  MWAWPageSpan const &page=getPageSpan();
  MWAWPosition pos(Vec2f((float)page.getMarginLeft(),(float)page.getMarginRight()),
                   Vec2f((float)page.getPageWidth(),(float)page.getPageLength()), librevenge::RVNG_INCH);
  pos.setRelativePosition(MWAWPosition::Page);
  pos.m_wrapping = MWAWPosition::WNone;
  listener->insertPicture(pos, data, "image/pict");

#ifdef DEBUG_WITH_FILES
  ascii().skipZone(entry.begin(), entry.end()-1);
  librevenge::RVNGBinaryData file;
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  input->readDataBlock(entry.length(), file);
  static int volatile pictName = 0;
  libmwaw::DebugStream f;
  f << "PICT-" << ++pictName;
  libmwaw::Debug::dumpFile(file, f.str().c_str());
#endif
  return true;
}

bool GreatWksBMParser::readBitmap(bool onlyCheck)
{
  MWAWInputStreamPtr input = getInput();
  long endPos=input->size();
  input->seek(512, librevenge::RVNG_SEEK_SET);

  libmwaw::DebugStream f;
  // a bitmap is composed of 720 rows of (72x8bytes)
  shared_ptr<MWAWPictBitmapIndexed> pict;
  if (!onlyCheck) {
    pict.reset(new MWAWPictBitmapIndexed(Vec2f(576,720)));
    std::vector<MWAWColor> colors(2);
    colors[0]=MWAWColor::white();
    colors[1]=MWAWColor::black();
    pict->setColors(colors);
  }

  for (int r=0; r<720; ++r) {
    long rowPos=input->tell();
    f.str("");
    f << "Entries(Bitmap)-" << r << ":";
    int col=0;
    while (col<72*8) {
      if (input->tell()+2>endPos) {
        MWAW_DEBUG_MSG(("GreatWksBMParser::readBitmap: can not read row %d\n", r));
        f << "###";
        ascii().addPos(rowPos);
        ascii().addNote(f.str().c_str());
        return false;
      }
      int wh=(int) input->readULong(1);
      if (wh>=0x81) {
        int color=(int) input->readULong(1);
        if (onlyCheck) {
          col+=8*(wh+1);
          continue;
        }
        for (int j=0; j < 0x101-wh; ++j) {
          if (col>=72*8) {
            MWAW_DEBUG_MSG(("GreatWksBMParser::readBitmap: can not read row %d\n", r));
            f << "###";
            ascii().addPos(rowPos);
            ascii().addNote(f.str().c_str());
            return false;
          }
          for (int b=7; b>=0; --b)
            pict->set(col++, r, (color>>b)&1);
        }
      }
      else {
        if (input->tell()+wh+1>endPos) {
          MWAW_DEBUG_MSG(("GreatWksBMParser::readBitmap: can not read row %d\n", r));
          f << "###";
          ascii().addPos(rowPos);
          ascii().addNote(f.str().c_str());
          return false;
        }
        for (int j=0; j < wh+1; ++j) {
          int color=(int) input->readULong(1);
          if (col>=72*8) {
            MWAW_DEBUG_MSG(("GreatWksBMParser::readBitmap: can not read row %d\n", r));
            f << "###";
            ascii().addPos(rowPos);
            ascii().addNote(f.str().c_str());
            return false;
          }
          if (onlyCheck) {
            col+=8;
            continue;
          }
          for (int b=7; b>=0; --b)
            pict->set(col++, r, (color>>b)&1);
        }
      }
    }
    ascii().addPos(rowPos);
    ascii().addNote(f.str().c_str());
  }
  if (!onlyCheck)
    m_state->m_bitmap=pict;
  return true;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool GreatWksBMParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = GreatWksBMParserInternal::State();
  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork() || !input->checkPosition(512+10))
    return false;

  std::string type, creator;
  /** no finder info, may be ok, but this means
      that we may have a basic pict file, so... */
  if (!input->getFinderInfo(type, creator))
    return false;
  if (creator!="ZEBR")
    return false;
  int vers=1;
  if (type=="PNTG")
    vers=1;
  else if (type=="ZPNT")
    vers=2;
  else return false;
  if (strict) {
    input->seek(512, librevenge::RVNG_SEEK_SET);
    if (vers==2) {
      Box2f box;
      if (MWAWPictData::check(input, (int)(input->size()-512), box)==MWAWPict::MWAW_R_BAD)
        return false;
    }
    else if (!readBitmap(true))
      return false;
  }
  setVersion(vers);
  if (header)
    header->reset(MWAWDocument::MWAW_T_GREATWORKS, vers, MWAWDocument::MWAW_K_PAINT);

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
