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

#include "MacDraftParser.hxx"

/** Internal: the structures of a MacDraftParser */
namespace MacDraftParserInternal
{
////////////////////////////////////////
//! Internal: the state of a MacDraftParser
struct State {
  //! constructor
  State() : m_version(0)
  {
  }
  //! the file version
  int m_version;
};

////////////////////////////////////////
//! Internal: the subdocument of a MacDraftParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(MacDraftParser &pars, MWAWInputStreamPtr input, int zoneId) : MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId) {}

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
    MWAW_DEBUG_MSG(("MacDraftParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  MacDraftParser *parser=dynamic_cast<MacDraftParser *>(m_parser);
  if (!parser) {
    MWAW_DEBUG_MSG(("MacDraftParserInternal::SubDocument::parse: no parser\n"));
    return;
  }
  long pos = m_input->tell();
  // parser->sendText(m_id);
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
}


}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MacDraftParser::MacDraftParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWGraphicParser(input, rsrcParser, header), m_state()
{
  init();
}

MacDraftParser::~MacDraftParser()
{
}

void MacDraftParser::init()
{
  resetGraphicListener();
  setAsciiName("main-1");

  m_state.reset(new MacDraftParserInternal::State);

  getPageSpan().setMargins(0.1);
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void MacDraftParser::parse(librevenge::RVNGDrawingInterface *docInterface)
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
      // TODO
    }
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("MacDraftParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetGraphicListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void MacDraftParser::createDocument(librevenge::RVNGDrawingInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getGraphicListener()) {
    MWAW_DEBUG_MSG(("MacDraftParser::createDocument: listener already exist\n"));
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
bool MacDraftParser::createZones()
{
  if (!readDocHeader())
    return false;
  MWAWInputStreamPtr input = getInput();
  while (readObject()) {
  }
  while (!input->isEnd()) {
    long pos=input->tell();
    long fSz=(long) input->readULong(2);
    if (!input->checkPosition(pos+2+fSz)) {
      input->seek(pos,librevenge::RVNG_SEEK_SET);
      break;
    }
    if (fSz==0) {
      ascii().addPos(pos);
      ascii().addNote("_");
    }
    else {
      ascii().addPos(pos);
      ascii().addNote("Entries(UnknZone):");
      input->seek(pos+2+fSz,librevenge::RVNG_SEEK_SET);
    }
  }
  if (!input->isEnd()) {
    MWAW_DEBUG_MSG(("MacDraftParser::createZones: find extra data\n"));
    ascii().addPos(input->tell());
    ascii().addNote("Entries(BAD):##");
  }
  return false;
}

////////////////////////////////////////////////////////////
// read an object
////////////////////////////////////////////////////////////
bool MacDraftParser::readObject()
{
  MWAWInputStreamPtr input = getInput();
  if (input->isEnd())
    return false;
  long pos=input->tell();
  long fSz=(long) input->readULong(2);
  if (fSz==0) {
    ascii().addPos(pos);
    ascii().addNote("_");
    return true;
  }
  if (fSz==0x1a) {
    input->seek(pos,librevenge::RVNG_SEEK_SET);
    return readLabel();
  }
  if (fSz==0x1e) {
    input->seek(pos,librevenge::RVNG_SEEK_SET);
    return readPattern();
  }
  long endPos=pos+2+fSz;
  if (fSz<0x1a || fSz==0x78 || !input->checkPosition(endPos)) {
    input->seek(pos,librevenge::RVNG_SEEK_SET);
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(Object):";
  if (fSz==0x1e) f << "pattern,";
  int val;
  for (int i=0; i<2; ++i) {
    val=(int) input->readULong(4);
    if (val) f << "id" << i << "=" << std::hex << val << std::dec << ",";
  }
  unsigned long flag=input->readULong(4);
#if 0
  int type=(flag>>26)&0x1f;
  switch (type) { // checkme
  case 0:
    f << "poly,";
    break;
  case 1: // checkme
    f << "circle1,";
    break;
  case 3:
    f << "textbox,";
    break;
  case 5:
    f << "rectangle,";
    break;
  case 6:
    f << "rectOval,";
    break;
  case 7:
    f << "ellipse,";
    break;
  case 10:
    f << "circle,";
    break;
  case 12:
    f << "pie,";
    break;
  case 14:
    f << "line/poly,";
    break;
  case 16:
    f << "group,";
    break;
  case 17:
    f << "line[h],";
    break;
  case 18:
    f << "line[v],";
    break;
  default:
    MWAW_DEBUG_MSG(("MacDraftParser::readObject: find unknown type\n"));
    f << "###type=" << type << ",";
    break;
  }
#endif
  switch ((flag>>3)&7) {
  case 0:
    break;
  case 1:
    f << "->,";
    break;
  case 2:
    f << "<-,";
    break;
  case 3:
    f << "<->,";
    break;
  case 4:
    f << "<->+length,";
    break;
  default:
    f << "##arrow=" << ((flag>>3)&7) << ",";
  }
  if (flag&0x40)
    f << "hairline,";
  switch ((flag>>9)&0x3) {
  case 0:
    f << "line[inside],";
    break;
  case 1: // centered
    break;
  case 2:
    f << "line[outside],";
    break;
  default:
    f << "line[pos]=##3,";
  }
  if ((flag>>11)&0x3f)
    f << "pat=" << ((flag>>11)&0x3f) << ",";
  if ((flag>>23)&7)
    f << "width=" << ((flag>>23)&7) << ",";
  if (flag&0x80000000)
    f << "select,";
  flag &= 0x7c7e0187;

  if (flag>>16) f << "fl0[h]=" << std::hex << (flag>>16) << std::dec << ",";
  if (flag&0xFFFF) f << "fl0[l]=" << std::hex << (flag&0xFFFF) << std::dec << ",";

  flag=input->readULong(4);
  if (flag&0x80000000) f << "lock,";
  flag &= 0x7fffffff;

  if (flag>>16) f << "fl1[h]=" << std::hex << (flag>>16) << std::dec << ",";
  if (flag&0xFFFF) f << "fl1[l]=" << std::hex << (flag&0xFFFF) << std::dec << ",";

  float dim[4];
  for (int i=0; i<4; ++i) dim[i]=float(input->readLong(2))/16.f;
  f << MWAWBox2f(MWAWVec2f(dim[1],dim[0]), MWAWVec2f(dim[3],dim[2])) << ",";
  for (int i=0; i<2; ++i) {
    val=(int) input->readULong(2);
    if (val) f << "fl" << i+1 << "=" << std::hex << val << std::dec << ",";
  }
  val=(int) input->readULong(4);
  if (val) f << "pat[id]=" << std::hex << val << ",";
  if (input->tell()!=endPos)
    ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(endPos,librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// read an label
////////////////////////////////////////////////////////////
bool MacDraftParser::readPattern()
{
  MWAWInputStreamPtr input = getInput();
  if (input->isEnd())
    return false;
  long pos=input->tell();
  long fSz=(long) input->readULong(2);
  long endPos=pos+2+fSz;
  if (fSz!=0x1e || !input->checkPosition(endPos)) {
    input->seek(pos,librevenge::RVNG_SEEK_SET);
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(Pattern):";
  long val=(long) input->readULong(4);
  if (val) f << "id=" << std::hex << val << std::dec << ",";
  val=(long) input->readULong(2); // 4|8|18|f0
  if (val) f << "fl=" << std::hex << val << std::dec << ",";
  val=(long) input->readULong(4);
  if (val) f << "id[prev?]=" << std::hex << val << std::dec << ",";
  for (int i=0; i<2; ++i) { //3,3 | 4,4 | 5,5
    val=input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  MWAWGraphicStyle::Pattern pat;
  pat.m_dim=MWAWVec2i(8,8);
  pat.m_data.resize(8);
  pat.m_colors[0]=MWAWColor::white();
  pat.m_colors[1]=MWAWColor::black();

  for (size_t i=0; i<8; ++i) pat.m_data[i]=uint8_t(input->readULong(1));
  f << pat << ",";
  f << "unkn=[";
  for (int i=0; i<4; ++i) { // either a series of 0 or -1
    val=input->readLong(2);
    if (val==-1) f << "*,";
    else if (val==0) f << "_,";
    else f << val << ",";
  }
  f << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(endPos,librevenge::RVNG_SEEK_SET);
  return true;
}
////////////////////////////////////////////////////////////
// read an label
////////////////////////////////////////////////////////////
bool MacDraftParser::readLabel()
{
  MWAWInputStreamPtr input = getInput();
  if (input->isEnd())
    return false;
  long pos=input->tell();
  long fSz=(long) input->readULong(2);
  long endPos=pos+2+fSz;
  if (fSz!=0x1a || !input->checkPosition(endPos)) {
    input->seek(pos,librevenge::RVNG_SEEK_SET);
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(Label):";
  int val;
  for (int i=0; i<2; ++i) {
    val=(int) input->readULong(4);
    if (val) f << "id" << i << "=" << std::hex << val << std::dec << ",";
  }
  unsigned long flag=input->readULong(4);
  flag &= 0x7ffffff;

  f << "font[sz]=" << (flag & 0x7F) << ",";
  f << "font[id]=" << ((flag>>8) & 0xFF) << ",";
  if (flag&0x1000000) f << "b:";
  if (flag&0x2000000) f << "it:";
  if (flag&0x4000000) f << "underline:";
  if (flag&0x8000000) f << "emboss:";
  if (flag&0x10000000) f << "shadow:";
  if (flag&0x80000000) f << "select,";
  flag &= 0x7E0FF0080;
  if (flag>>16) f << "fl0[h]=" << std::hex << (flag>>16) << std::dec << ",";
  if (flag&0xFFFF) f << "fl0[l]=" << std::hex << (flag&0xFFFF) << std::dec << ",";

  flag=input->readULong(4);
  if (flag&0x80000000) f << "lock,";
  flag &= 0x7fffffff;
  switch (flag&3) {
  case 1:
    f << "upper,";
    break;
  case 2:
    f << "lower,";
    break;
  case 3:
    f << "title,";
    break;
  case 0:
  default:
    break;
  }
  switch ((flag>>8)&3) {
  case 0:
    break;
  case 1:
    f << "interline=150%,";
    break;
  case 2:
    f << "interline=200%,";
    break;
  default:
    f << "##interline=3,";
    break;
  }
  flag &= 0x7FFFFCFC;
  if (flag>>16) f << "fl1[h]=" << std::hex << (flag>>16) << std::dec << ",";
  if (flag&0xFFFF) f << "fl1[l]=" << std::hex << (flag&0xFFFF) << std::dec << ",";

  float dim[4];
  for (int i=0; i<4; ++i) dim[i]=float(input->readLong(2))/16.f;
  f << MWAWBox2f(MWAWVec2f(dim[1],dim[0]), MWAWVec2f(dim[3],dim[2])) << ",";
  val=(int) input->readULong(2);
  if (val!=0x204)
    f << "fl2=" << std::hex << val << std::dec << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(endPos,librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool MacDraftParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = MacDraftParserInternal::State();
  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork() || !input->checkPosition(0x270))
    return false;

  libmwaw::DebugStream f;
  f << "FileHeader:";
  input->seek(0, librevenge::RVNG_SEEK_SET);
  if (input->readULong(2)!=2 || input->readULong(2)!=0 || input->readULong(2)!=2 ||
      input->readULong(2)!=0x262 || input->readULong(2)!=0x262)
    return false;
  int vers=1;
  int val=(int) input->readULong(2);
  if (val)
    f << "f0=" << val << ",";
  f << "id=" << std::hex << input->readULong(4) << std::dec << ",";
  for (int i=0; i<2; ++i) {// f0=fc[4c]0, f1=[09]200
    val=(int) input->readULong(2);
    if (val) f << "fl" << i << "=" << std::hex << val << std::dec << ",";
  }
  f << "id1=" << std::hex << input->readULong(4) << std::dec << ",";
  val=(int) input->readLong(2); // always 0
  if (val)
    f << "f1=" << val << ",";
  f << "ids=[";
  for (int i=0; i<5; ++i) { // maybe junk
    val=(int) input->readULong(4);
    if (val)
      f << std::hex << val << std::dec << ",";
    else
      f << "_,";
  }
  f << "],";
  val=(int) input->readLong(2); // 0|1
  if (val)
    f << "f2=" << val << ",";
  int sSz=(int) input->readULong(1);
  if (sSz>31) {
    if (strict)
      return false;
    MWAW_DEBUG_MSG(("MacDraftParser::checkHeader: string size seems bad\n"));
    f << "##sSz=" << sSz << ",";
  }
  else {
    std::string name("");
    for (int i=0; i<sSz; ++i) name+=(char) input->readULong(1);
    f << name << ",";
  }
  input->seek(80, librevenge::RVNG_SEEK_SET);
  val=(int) input->readULong(4);
  if (val)
    f << "id2=" << std::hex << val << std::dec << ",";

  ascii().addPos(0);
  ascii().addNote(f.str().c_str());
  setVersion(vers);
  m_state->m_version=vers;
  if (header)
    header->reset(MWAWDocument::MWAW_T_MACDRAFT, vers, MWAWDocument::MWAW_K_DRAW);

  return true;
}

////////////////////////////////////////////////////////////
// read the document header
////////////////////////////////////////////////////////////
bool MacDraftParser::readDocHeader()
{
  MWAWInputStreamPtr input = getInput();
  if (!input->checkPosition(0x262)) {
    MWAW_DEBUG_MSG(("MacDraftParser::readDocHeader: the file seems too short\n"));
    return false;
  }
  input->seek(84,librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(DocHeader):";
  int val;
  for (int i=0; i<16; ++i) {
    val=(int) input->readULong(2);
    static int const(expected[])= {0,1,0,0,0xdfff,0xffff,0,0, 0xeed7, 0xffff, 0, 0,
                                   0x9dd5, 0xbfff,0x4088,0x8000
                                  };
    if (val!=expected[i])
      f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  for (int i=0; i<7; ++i) {
    f << "unkn" << i << "=[";
    for (int j=0; j<4; ++j) {
      val=(int) input->readULong(2);
      if (val)
        f << std::hex << val << std::dec << ",";
      else
        f << "_,";
    }
    f << "],";
  }
  for (int i=0; i<14; ++i) { // always 0
    val=(int) input->readLong(2);
    if (val)
      f << "g" << i << "=" << val << ",";
  }
  ascii().addPos(84);
  ascii().addNote(f.str().c_str());

  long pos=input->tell();
  f.str("");
  f << "DocHeader-1:";
  int N=(int) input->readULong(2);
  if (N>=20) {
    MWAW_DEBUG_MSG(("MacDraftParser::readDocHeader: N seems bad\n"));
    f << "###";
    N=0;
  }
  f << "N=" << N << ",unk=[";
  for (int i=0; i<=N; ++i) {
    val=(int) input->readULong(2);
    f << input->readULong(2) << ":" << std::hex << val << std::dec << ",";
  }
  f << "],";
  input->seek(pos+42, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<6; ++i) {
    val=(int) input->readULong(2);
    static int const(expected[])= {0x144, 0xa, 0x152, 0xc, 0, 0 };
    if (val!=expected[i])
      f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  f << "unkn1=[";
  for (int i=0; i<2; ++i) { // 0,0 or 99cc6633,99cc6633
    long lVal=(long) input->readULong(4);
    if (lVal)
      f << std::hex << lVal << std::dec << ",";
    else
      f << "_,";
  }
  f << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "DocHeader-2:";
  input->seek(pos+132,librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "DocHeader-3:";
  input->seek(pos+66,librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int st=0; st<2; ++st) {
    pos=input->tell();
    f.str("");
    f << "DocHeader-4" << (st==0 ? "a" : "b") << ":";
    input->seek(pos+40,librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  pos=input->tell();
  f.str("");
  f << "DocHeader-5:";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  // ascii().addDelimiter(input->tell(),'|');

  input->seek(0x270,librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// try to read the print info zone
////////////////////////////////////////////////////////////
bool MacDraftParser::readPrintInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  long endPos=pos+120;
  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("MacDraftParser::readPrintInfo: file seems too short\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(PrintInfo):";
  libmwaw::PrinterInfo info;
  if (!info.read(input)) {
    MWAW_DEBUG_MSG(("MacDraftParser::readPrintInfo: can not read print info\n"));
    return false;
  }
  f << info;
  MWAWVec2i paperSize = info.paper().size();
  MWAWVec2i pageSize = info.page().size();
  if (pageSize.x() <= 0 || pageSize.y() <= 0 ||
      paperSize.x() <= 0 || paperSize.y() <= 0) {
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    return true;
  }

  // define margin from print info
  MWAWVec2i lTopMargin= -1 * info.paper().pos(0);
  MWAWVec2i rBotMargin=info.paper().size() - info.page().size();

  // move margin left | top
  int decalX = lTopMargin.x() > 14 ? lTopMargin.x()-14 : 0;
  int decalY = lTopMargin.y() > 14 ? lTopMargin.y()-14 : 0;
  lTopMargin -= MWAWVec2i(decalX, decalY);
  rBotMargin += MWAWVec2i(decalX, decalY);

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

////////////////////////////////////////////////////////////
//
// send data
//
////////////////////////////////////////////////////////////

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
