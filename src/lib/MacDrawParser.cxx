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
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWGraphicListener.hxx"
#include "MWAWGraphicShape.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWHeader.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPictBitmap.hxx"
#include "MWAWPictData.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSubDocument.hxx"

#include "MacDrawParser.hxx"

/** Internal: the structures of a MacDrawParser */
namespace MacDrawParserInternal
{
////////////////////////////////////////
//! Internal: the state of a MacDrawParser
struct State {
  //! constructor
  State()
  {
  }
};

////////////////////////////////////////
//! Internal: the subdocument of a MacDrawParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(MacDrawParser &pars, MWAWInputStreamPtr input, int zoneId) : MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const
  {
    if (MWAWSubDocument::operator!=(doc)) return true;
    SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
    if (!sDoc) return true;
    if (m_id != sDoc->m_id) return true;
    return false;
  }

  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const
  {
    return !operator!=(doc);
  }
  //! the parser function
  void parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType type);

protected:
  //! the subdocument id
  int m_id;
private:
  SubDocument(SubDocument const &orig);
  SubDocument &operator=(SubDocument const &orig);
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType)
{
  if (!listener || !listener->canWriteText()) {
    MWAW_DEBUG_MSG(("MacDrawParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  if (!m_parser) {
    MWAW_DEBUG_MSG(("MacDrawParserInternal::SubDocument::parse: no parser\n"));
    return;
  }
  long pos = m_input->tell();
  // to do
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
}


}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MacDrawParser::MacDrawParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWGraphicParser(input, rsrcParser, header), m_state()
{
  init();
}

MacDrawParser::~MacDrawParser()
{
}

void MacDrawParser::init()
{
  resetGraphicListener();
  setAsciiName("main-1");

  m_state.reset(new MacDrawParserInternal::State);

  getPageSpan().setMargins(0.1);
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void MacDrawParser::parse(librevenge::RVNGDrawingInterface *docInterface)
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
    }
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("MacDrawParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetGraphicListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void MacDrawParser::createDocument(librevenge::RVNGDrawingInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getGraphicListener()) {
    MWAW_DEBUG_MSG(("MacDrawParser::createDocument: listener already exist\n"));
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
bool MacDrawParser::createZones()
{
  MWAWInputStreamPtr input = getInput();
  int const vers=version();
  readPrefs();
  input->seek(512,librevenge::RVNG_SEEK_SET);
  if (vers<2) {
    while (readObject()) {
    }
  }

  long pos=input->tell();
  ascii().addPos(pos);
  ascii().addNote("Entries(Data):");
  return false;
}

bool MacDrawParser::readObject()
{
  MWAWInputStreamPtr input = getInput();
  if (input->isEnd()) return false;
  long pos=input->tell();
  libmwaw::DebugStream f;
  f << "Entries(Object):";

  if (!input->checkPosition(pos+4+4)) {
    MWAW_DEBUG_MSG(("MacDrawParser::readObject: the zone seems to small\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  int type=(int) input->readULong(1);
  switch (type) {
  case 0:
    f << "end[group],";
    break;
  case 1:
    f << "text,";
    break;
  case 2:
    f << "line[axis],";
    break;
  case 3:
    f << "line,";
    break;
  case 4:
    f << "rect,";
    break;
  case 5:
    f << "roundrect,";
    break;
  case 6:
    f << "circle,";
    break;
  case 7:
    f << "arc,";
    break;
  case 8:
    f << "poly[free],";
    break;
  case 9:
    f << "poly,";
    break;
  case 10:
    f << "begin[group],";
    break;
  case 11:
    f << "bitmap,";
    break;
  default:
    MWAW_DEBUG_MSG(("MacDrawParser::readObject: find unknown object type %d\n", type));
    f << "#type=" << type << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  int val=(int) input->readULong(1);
  if (val==1) f << "locked,";
  else if (val) f << "#lock=" << val << ",";
  val=(int) input->readLong(2);
  if (val) f << "f0=" << val << ",";

  int lineType=(int) input->readULong(1);
  if (lineType>=1 && lineType<6) {
    static float const(widths[]) = { 0, 1, 2, 3.5f, 5 };
    if (lineType!=2) // default
      f << "line[width]=" << widths[lineType-1] << ",";
  }
  else {
    MWAW_DEBUG_MSG(("MacDrawParser::readObject: find unexpected line type\n"));
    f << "#line[width]=" << lineType << ",";
  }
  int linePat=(int) input->readULong(1);
  if (linePat>=1&&linePat<37)
    f << "line[pat]=" << linePat << ",";
  else {
    MWAW_DEBUG_MSG(("MacDrawParser::readObject: find unexpected line pattern\n"));
    f << "#line[pat]=" << linePat << ",";
  }
  int surfPat=(int) input->readULong(1);
  if (surfPat>=1&&surfPat<37) {
    if (surfPat!=1)
      f << "surf[pat]=" << surfPat << ",";
  }
  else {
    MWAW_DEBUG_MSG(("MacDrawParser::readObject: find unexpected surface pattern\n"));
    f << "#surf[pat]=" << surfPat << ",";
  }
  val=(int) input->readULong(1);
  if (type==2 || type==3) {
    if (val&1) f << "arrow[beg],";
    if (val&2) f << "arrow[end],";
    val &= 0xFC;
  }
  else if (type==4 || type==5) {
    if (val>=1 && val <= 5) {
      f << "corner[length]=" << float(val+1)*4.5f << ",";
      val=0;
    }
    else if (val) {
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: find unknown corner values\n"));
      f << "###";
    }
  }
  if (val) f << "#flags=" << std::hex << val << std::dec << ",";
  bool ok=true;
  long actPos=input->tell();
  switch (type) {
  case 0:
    break;
  case 1: {
    if (!input->checkPosition(actPos+20)) {
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: the zone length seems bad\n"));
      f << "###";
      ok=false;
      break;
    }
    long id=(long) input->readULong(4);
    if (id) f << "id=" << std::hex << id << std::dec << ",";
    f << "font=[";
    val=(int) input->readULong(1);
    if (val&1) f << "b:";
    if (val&2) f << "it:";
    if (val&4) f << "underline:";
    if (val&8) f << "outline:";
    if (val&0x10) f << "shadow:";
    if (val&0xE0) f << "style=" << std::hex << (val&0xE0) << std::dec << ",";
    int fId=(int) input->readULong(1);
    if (fId) f << "id=" << fId << ",";
    int fSz=(int) input->readULong(1);
    if (fSz>=1 && fSz<9) {
      static int const fontSize[]= {9,10,12,14,18,24,36,48};
      f << "sz=" << fontSize[fSz-1] << ",";
    }
    else {
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: find unknown size\n"));
      f << "#sz=" << fSz << ",";
    }
    val=(int) input->readULong(1);
    switch (val) {
    case 1:
      break;
    case 2:
      f << "interline=150%,";
      break;
    case 3:
      f << "interline=200%,";
      break;
    default:
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: find unknown interline\n"));
      f << "#interline=" << val << ",";
    }
    val=(int) input->readULong(1);
    switch (val) {
    case 1:
      break;
    case 2:
      f << "align=center,";
      break;
    case 3:
      f << "align=left,";
      break;
    default:
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: find unknown align\n"));
      f << "#align=" << val << ",";
    }
    f << "],";
    val=(int) input->readULong(1);
    if (val&3) f << "rot=" << 90*(val&3) << ",";
    if (val&4) f << "sym,";
    if (val&0xF8) f << "#rot=" << std::hex << (val&0xF8) << std::dec << ",";
    int N=(int) input->readULong(2);
    if (!input->checkPosition(actPos+20+N)) {
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: the number of character seems bad\n"));
      f << "##N=" << N << ",";
      ok=false;
      break;
    }
    int dim[4];
    for (int i=0; i<4; ++i) dim[i]=int(input->readLong(2));
    f << Vec2i(dim[1],dim[0]) << "<->" << Vec2i(dim[3],dim[2]) << ",";
    ascii().addDelimiter(input->tell(),'|');
    input->seek(actPos+20+N, librevenge::RVNG_SEEK_SET);
    break;
  }
  case 2:
  case 3:
  case 4:
  case 5:
  case 6:
  case 7: {
    if (!input->checkPosition(actPos+16+(type==7 ? 4 : 0))) {
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: the zone length seems bad\n"));
      f << "###";
      ok=false;
      break;
    }
    float dim[4];
    for (int i=0; i<4; ++i) dim[i]=float(input->readLong(4))/65536.f;
    f << Vec2f(dim[1],dim[0]) << "<->" << Vec2f(dim[3],dim[2]) << ",";
    if (type!=7) break;
    f << "angle[start]=" << input->readLong(2) << ",";
    f << "angle[w]=" << input->readLong(2) << ",";
    break;
  }
  case 8:
  case 9: {
    long dSz=(long)input->readULong(4);
    if (dSz<18||!input->checkPosition(actPos+4+dSz)) {
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: the polygon data size seems bad\n"));
      f << "###";
      ok=false;
      break;
    }
    int N=(int) input->readULong(2);
    f << "N=" << N << ",";
    float dim[4];
    for (int i=0; i<4; ++i) dim[i]=float(input->readLong(4))/65536.f;
    f << Vec2f(dim[1],dim[0]) << "<->" << Vec2f(dim[3],dim[2]) << ",";
    int coordSz=type==8 ? 1 : 4;
    if (2+16+(type==8 ? 6 : 0)+2*N*coordSz>dSz) {
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: the number of points seems bad\n"));
      f << "###";
      ok=false;
      break;
    }
    val=(int) input->readLong(2); // find a copy of the type here
    if (val!=type) f << "g0=" << val << ",";
    if (type==8) {
      for (int i=0; i<2; ++i) dim[i]=float(input->readLong(4))/65536.f;
      f << "orig=" << Vec2f(dim[1],dim[0]) << ",";
      f << "delta=[";
      for (int i=0; i<N-1; ++i)
        f << input->readLong(1) << "x" << input->readLong(1) << ",";
      f << "],";
    }
    else {
      f << "pts=[";
      for (int i=0; i<N; ++i)
        f << float(input->readLong(4))/65536.f << "x" << float(input->readLong(4))/65536.f << ",";
      f << "],";
    }
    input->seek(actPos+4+dSz, librevenge::RVNG_SEEK_SET);
    break;
  }
  case 10: {
    if (!input->checkPosition(actPos+32)) {
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: the zone length seems bad\n"));
      f << "###";
      ok=false;
      break;
    }
    float dim[4];
    for (int i=0; i<4; ++i) dim[i]=float(input->readLong(4))/65536.f;
    f << Vec2f(dim[1],dim[0]) << "<->" << Vec2f(dim[3],dim[2]) << ",";
    int N=(int) input->readULong(2);
    f << "N=" << N << ",";
    val=(int) input->readLong(2);
    if (val!=N) f << "N[child]=" << val << ",";
    long dSz=(int) input->readULong(4); // related to size (but can be only an approximation)
    f << "groupSize=" << std::hex << dSz << std::dec << ",";
    for (int i=0; i<2; ++i) {
      val=(int) input->readULong(4);
      if (val) f << "id" << i << "=" << std::hex << val << std::dec << ",";
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    for (int i=0; i<N; ++i) {
      if (!readObject()) {
        MWAW_DEBUG_MSG(("MacDrawParser::readObject: can not read an object\n"));
        return false;
      }
    }
    readObject(); // read end group
    return true;
  }
  case 11: {
    if (!input->checkPosition(actPos+42))  {
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: the zone length seems bad\n"));
      f << "###";
      ok=false;
      break;
    }
    for (int i=0; i<2; ++i) { // maybe a dim
      val=(int) input->readULong(2);
      if (val) f << "f0=" << val << ",";
    }
    int dim[4];
    for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
    Box2i bitmapBox(Vec2i(dim[1],dim[0]), Vec2i(dim[3],dim[2]));
    f << "bitmap[dim]="<< bitmapBox << ",";
    float fDim[4];
    for (int i=0; i<4; ++i) fDim[i]=float(input->readLong(4))/65536.f;
    f << Vec2f(fDim[1],fDim[0]) << "<->" << Vec2f(fDim[3],fDim[2]) << ",";

    val=(int) input->readULong(4); // find 8063989c maybe type + id
    if (val) f << "id=" << std::hex << val << std::dec << ",";
    int numBytesByRow=(int) input->readULong(2);

    for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
    Box2i fileBox(Vec2i(dim[1],dim[0]), Vec2i(dim[3],dim[2]));
    f << "bitmap[dimInFile]="<< fileBox << ",";
    long endPos=input->tell()+fileBox.size()[1]*numBytesByRow;
    if (fileBox.size()[1]<0 || numBytesByRow<0 || !input->checkPosition(endPos)) {
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: can not compute the bitmap endPos\n"));
      f << "###";
      ok=false;
      break;
    }
    if (numBytesByRow*8 < fileBox.size()[0] ||
        fileBox[0][0]>bitmapBox[0][0] || fileBox[0][1]>bitmapBox[0][1] ||
        fileBox[1][0]<bitmapBox[1][0] || fileBox[1][1]<bitmapBox[1][1]) {
      MWAW_DEBUG_MSG(("MacDrawParser::readObject: something look bad when reading a bitmap header\n"));
      f << "###";
    }
    else {
      ascii().addPos(input->tell());
      ascii().addNote("Object:bitmap[data],");
    }
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    break;
  }
  default:
    ok=false;
    break;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return ok;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool MacDrawParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = MacDrawParserInternal::State();
  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork() || !input->checkPosition(512))
    return false;

  libmwaw::DebugStream f;
  f << "FileHeader:";
  input->seek(0, librevenge::RVNG_SEEK_SET);
  int val=(int) input->readULong(2);
  int vers=0;
  if (val==0x4452) {
    if (input->readULong(2)!=0x5747) return false;
    val=(int) input->readULong(2);
    if (val==0x4d44) vers=1;
    else if (val==0x4432) vers=2;
    else {
      MWAW_DEBUG_MSG(("MacDrawParser::checkHeader: find unexpected header\n"));
      return false;
    }
    f << "version=" << vers << ",";
    f << "subVersion=" << input->readLong(2) << ",";
  }
  else if (val==0x4d44) {
    vers=0;
    val=(int) input->readLong(2); // find 4 for v0.9
    if (val!=4) f << "f0=" << val << ",";
    for (int i=0; i<2; ++i) { // always 0?
      val=(int) input->readLong(2);
      if (val) f << "f" << i+1 << "=" << val << ",";
    }
  }
  else
    return false;
  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  if (strict && !readPrintInfo()) {
    input->seek(8, librevenge::RVNG_SEEK_SET);
    for (int i=0; i<10; ++i) // allow print info to be zero
      if (input->readLong(2)) return false;
  }

  setVersion(vers);
  if (header)
    header->reset(MWAWDocument::MWAW_T_MACDRAW, vers, MWAWDocument::MWAW_K_DRAW);
  input->seek(512,librevenge::RVNG_SEEK_SET);

  return true;
}

////////////////////////////////////////////////////////////
// try to read the prefs zone
////////////////////////////////////////////////////////////
bool MacDrawParser::readPrefs()
{
  MWAWInputStreamPtr input = getInput();
  if (!input->checkPosition(512)) {
    MWAW_DEBUG_MSG(("MacDrawParser::readPrefs: the prefs zone seems too short\n"));
    ascii().addPos(14);
    ascii().addNote("Entries(Prefs):#");
    return false;
  }
  input->seek(8,librevenge::RVNG_SEEK_SET);
  if (!readPrintInfo()) {
    ascii().addPos(8);
    ascii().addNote("Entries(PrintInfo):#");
  }
  input->seek(8+120, librevenge::RVNG_SEEK_SET);
  // v2: cut in 128, 40, 3*40, remain
  long pos=input->tell();
  libmwaw::DebugStream f;
  f << "Entries(Prefs):";
  for (int i=0; i<9; ++i) { // f0=1|2|7, f1=0|75|78|7c, f2=0|48, f4=0|48, f5=0|48, f6=0|48, f7=0|48
    int val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(0x100, librevenge::RVNG_SEEK_SET);
  pos=input->tell();
  input->seek(pos+40, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote("Prefs-A:");
  for (int i=0; i<5; ++i) {
    pos=input->tell();
    f.str("");
    f << "Prefs-B" << i << ":";
    input->seek(pos+40, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  pos=input->tell();
  ascii().addPos(pos);
  ascii().addNote("Prefs-end:");

  input->seek(512, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// try to read the print info zone
////////////////////////////////////////////////////////////
bool MacDrawParser::readPrintInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  long endPos=pos+120;
  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("MacDrawParser::readPrintInfo: file seems too short\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(PrintInfo):";
  libmwaw::PrinterInfo info;
  if (!info.read(input)) {
    MWAW_DEBUG_MSG(("MacDrawParser::readPrintInfo: can not read print info\n"));
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
