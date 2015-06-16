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

#include "MWAWFontConverter.hxx"
#include "MWAWGraphicListener.hxx"
#include "MWAWGraphicShape.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWHeader.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWPictBitmap.hxx"
#include "MWAWPictData.hxx"
#include "MWAWPrinter.hxx"
#include "MWAWPosition.hxx"
#include "MWAWRSRCParser.hxx"
#include "MWAWSubDocument.hxx"

#include "MacDraft5StyleManager.hxx"

#include "MacDraft5Parser.hxx"

/** Internal: the structures of a MacDraft5Parser */
namespace MacDraft5ParserInternal
{
//!  Internal and low level: a class used to store layout definition of a MacDraf5Parser
struct Layout {
  //! constructor
  Layout() : m_entry(), m_N(0), m_objectId(0), m_name(""), m_extra("")
  {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Layout const &lay)
  {
    o << lay.m_name << ",";
    o << std::hex << lay.m_entry.begin() << "<->" << lay.m_entry.end() << std::dec << "[" << lay.m_N << "],";
    o << lay.m_extra;
    return o;
  }
  //! the layout position in the data fork
  MWAWEntry m_entry;
  //! the number of elements
  int m_N;
  //! the object number
  int m_objectId;
  //! the layout name
  std::string m_name;
  //! extra data
  std::string m_extra;
};

//! generic class used to store shape in MWAWDraftParser
struct Shape {
  //! the different shape
  enum Type { Basic, Bitmap, Group, Label, Text, Unknown };

  //! constructor
  Shape() : m_type(Unknown), m_fileType(0), m_box(), m_origin(), m_style(), m_patternId(-1), m_shape(), m_isLine(false), m_id(-1), m_nextId(-1),m_labelId(-1), m_nameId(-1),
    m_font(), m_paragraph(), m_textEntry(), m_labelWidth(0), m_childList(),
    m_bitmapIdList(), m_bitmapDimensionList(), m_isSent(false)
  {
  }

  //! return the shape bdbox
  MWAWBox2f getBdBox() const
  {
    return m_type==Basic ? m_shape.getBdBox() : m_box;
  }
  //! the graphic type
  Type m_type;
  //! the file type
  int m_fileType;
  //! the shape bdbox
  MWAWBox2f m_box;
  //! the shape origin
  MWAWVec2f m_origin;
  //! the graphic style
  MWAWGraphicStyle m_style;
  //! the pattern id
  int m_patternId;
  //! the graphic shape ( for basic geometric form )
  MWAWGraphicShape m_shape;
  //! flag to know if the shape is a line
  bool m_isLine;
  //! the shape id
  int m_id;
  //! the following id (if set)
  int m_nextId;
  //! the label id
  int m_labelId;
  //! the name id
  int m_nameId;
  //! the font ( for a text box)
  MWAWFont m_font;
  //! the paragraph ( for a text box)
  MWAWParagraph m_paragraph;
  //! the textbox entry (main text)
  MWAWEntry m_textEntry;
  //! the 1D label width in point
  float m_labelWidth;
  //! the child list id ( for a group )
  std::vector<int> m_childList;
  //! the list of bitmap id ( for a bitmap)
  std::vector<int> m_bitmapIdList;
  //! the list of bitmap dimension ( for a bitmap)
  std::vector<MWAWBox2i> m_bitmapDimensionList;
  //! a flag used to know if the object is sent to the listener or not
  mutable bool m_isSent;
};

////////////////////////////////////////
//! Internal: the state of a MacDraft5Parser
struct State {
  //! constructor
  State() : m_version(0), m_isLibrary(false), m_layoutList()
  {
  }
  //! the file version
  int m_version;
  //! flag to know if we read a library
  bool m_isLibrary;
  //! the layer list
  std::vector<Layout> m_layoutList;
  //! the color list
  std::vector<MWAWColor> m_colorList;
};

////////////////////////////////////////
//! Internal: the subdocument of a MacDraft5Parser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(MacDraft5Parser &pars, MWAWInputStreamPtr input, int zoneId) : MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId) {}

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
    MWAW_DEBUG_MSG(("MacDraft5ParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  MacDraft5Parser *parser=dynamic_cast<MacDraft5Parser *>(m_parser);
  if (!parser) {
    MWAW_DEBUG_MSG(("MacDraft5ParserInternal::SubDocument::parse: no parser\n"));
    return;
  }
  long pos = m_input->tell();
  // parser->sendText(m_id); TODO
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
}


}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MacDraft5Parser::MacDraft5Parser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWGraphicParser(input, rsrcParser, header), m_styleManager(), m_state()
{
  init();
}

MacDraft5Parser::~MacDraft5Parser()
{
}

void MacDraft5Parser::init()
{
  resetGraphicListener();
  setAsciiName("main-1");

  m_state.reset(new MacDraft5ParserInternal::State);
  m_styleManager.reset(new MacDraft5StyleManager(*this));

  getPageSpan().setMargins(0.1);
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void MacDraft5Parser::parse(librevenge::RVNGDrawingInterface *docInterface)
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
      ok=false;
    }
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetGraphicListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void MacDraft5Parser::createDocument(librevenge::RVNGDrawingInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getGraphicListener()) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::createDocument: listener already exist\n"));
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
bool MacDraft5Parser::createZones()
{
  MWAWInputStreamPtr input = getInput();
  if ((m_state->m_isLibrary && !readLibraryHeader()) ||
      (!m_state->m_isLibrary && !readDocHeader()))
    return false;
  long pos=input->tell();
  m_styleManager->readResources();
  m_styleManager->readBitmapZones();

  long dataEnd=m_styleManager->getEndDataPosition();
  if (dataEnd>0)
    input->pushLimit(dataEnd);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  long lastPosSeen=pos;
  if (m_state->m_layoutList.empty()) {
    if (!m_state->m_isLibrary) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::createZones: the layout is empty, try to continue\n"));
    }

    while (!input->isEnd()) {
      pos=input->tell();
      MacDraft5ParserInternal::Layout layout;
      layout.m_entry.setBegin(pos);
      if (dataEnd>0)
        layout.m_entry.setEnd(dataEnd);
      else
        layout.m_entry.setEnd(input->size());
      if (!readLayout(layout))
        break;
      lastPosSeen=pos=input->tell();
      if (input->isEnd())
        break;
      // check if there is another layout
      int newN=(int)input->readULong(4);
      if (newN<=0 || pos+newN*10>layout.m_entry.end() ||
          (!m_state->m_isLibrary && pos+130>=layout.m_entry.end()))
        break;
      input->seek(-4, librevenge::RVNG_SEEK_CUR);
    }
  }
  else {
    for (size_t i=0; i<m_state->m_layoutList.size(); ++i) {
      if (!m_state->m_layoutList[i].m_entry.valid())
        continue;
      if (m_state->m_layoutList[i].m_entry.end()>lastPosSeen)
        lastPosSeen=m_state->m_layoutList[i].m_entry.end();
      readLayout(m_state->m_layoutList[i]);
    }
  }
  input->seek(lastPosSeen, librevenge::RVNG_SEEK_SET);
  if (!m_state->m_isLibrary)
    readDocFooter();
  else
    readLibraryFooter();

  if (!input->isEnd()) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::createZones: find some extra zone\n"));
    ascii().addPos(input->tell());
    ascii().addNote("Entries(Extra):###");
  }
  if (dataEnd>0)
    input->popLimit();
  return false;
}

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool MacDraft5Parser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = MacDraft5ParserInternal::State();
  MWAWInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork() || !input->checkPosition(0x100))
    return false;

  libmwaw::DebugStream f;
  f << "FileHeader:";
  input->seek(0, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<3; ++i) {
    int val=(int) input->readULong(2);
    int const expected[3]= {0x4d44, 0x4443, 0x3230};
    if (val==expected[i]) continue;
    m_state->m_isLibrary=true;
    f << "f" << i << "=" << std::hex << val << std::dec << ",";
  }
  if (input->readULong(2)!=6 || !readPrintInfo())
    return false;
  if (strict && m_state->m_isLibrary) {
    bool ok=false;
    MWAWRSRCParserPtr rsrcParser = getRSRCParser();
    if (rsrcParser) {
      std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
      std::multimap<std::string, MWAWEntry>::iterator it=entryMap.find("vers");
      while (it!=entryMap.end() && it->first=="vers") {
        MWAWRSRCParser::Version vers;
        if (rsrcParser->parseVers(it++->second, vers) &&
            vers.m_string.compare(0,8,"MacDraft")==0) {
          ok=true;
          break;
        }
      }
    }
    if (!ok) {
      // normally only ok for v5 file, so v4 library without rsrc file will not be read...
      long pos=input->tell();
      input->seek(-8, librevenge::RVNG_SEEK_CUR);
      std::string name("");
      for (int i=0; i<8; ++i) name+=(char) input->readULong(1);
      if (name!="RBALRPH ") return false;
      input->seek(pos, librevenge::RVNG_SEEK_SET);
    }
  }
  int const vers=4;
  ascii().addPos(0);
  ascii().addNote(f.str().c_str());
  setVersion(vers);
  m_state->m_version=vers;
  if (header)
    header->reset(MWAWDocument::MWAW_T_MACDRAFT, vers, MWAWDocument::MWAW_K_DRAW);

  return true;
}

////////////////////////////////////////////////////////////
// try to read the document header zone
////////////////////////////////////////////////////////////
bool MacDraft5Parser::readDocHeader()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  long endPos=pos+114+6*28;
  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readDocHeader: file seems too short\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(DocHeader):";
  int dim[4];
  for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
  f << "dim=" << MWAWBox2i(MWAWVec2i(dim[1],dim[0]),MWAWVec2i(dim[3],dim[2])) << ",";
  int val;
  for (int i=0; i<5; ++i) { // f0=0|-1|2e60, f1=0|b, f2=16|3b|86, f3=1|2, f4=1|2
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  for (int i=0; i<2; ++i) { // fl0=0|4, fl1=0|1|4
    val=(int) input->readLong(1);
    if (val) f << "fl" << i << "=" << val << ",";
  }
  for (int i=0; i<7; ++i) { // f5=0|2|3, f6=0|1, f7=1, f8=f9=1c|48 (res?), f10=6-10, f11=2|3
    val=(int) input->readLong(2);
    if (val) f << "f" << i+5 << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (int i=0; i<2; ++i) {
    pos=input->tell();
    f.str("");
    f << "DocHeader-A" << i << ":"; // first dim in inch?, second metric?
    val=(int) input->readLong(2); // 0|1
    if (val!=i) f << "#id=" << val << ",";
    f << "dim=[";
    for (int j=0; j<5; ++j) f << float(input->readLong(4))/65536.f  << ",";
    f << "],";
    for (int j=0; j<4; ++j) {
      val=(int) input->readLong(2);
      int const expected[4]= {0,100,0,100};
      if (val!=expected[j]) f << "f" << j << "=" << val << ",";
    }
    f << "dim2=[";
    for (int j=0; j<3; ++j) f << float(input->readLong(4))/65536.f  << ",";
    f << "],";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+42, librevenge::RVNG_SEEK_SET);
  }
  for (int i=0; i<6; ++i) {
    pos=input->tell();
    f.str("");
    f << "DocHeader-B" << i << ":";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+28, librevenge::RVNG_SEEK_SET);
  }
  pos=input->tell();
  f.str("");
  f << "DocHeader-C0:";
  for (int i=0; i<2; ++i) {
    float fVal=float(input->readLong(4))/65536.f;
    if (fVal<0 || fVal>0)
      f << "d" << i << "=" << fVal << ",";
  }
  for (int i=0; i<4; ++i) { // f1=f2=0|1
    val=(int) input->readLong(2);
    int const expected[4]= {0x1e, 0, 0, 1};
    if (val!=expected[i]) f << "f" << i << "=" << val << ",";
  }
  for (int i=0; i<4; ++i) { // fl0=0|2, fl2=0|80
    val=(int) input->readULong(1);
    int const expected[4]= {2, 1, 0, 0};
    if (val!=expected[i]) f << "fl" << i << "=" << std::hex << val << std::dec << ",";
  }
  for (int i=0; i<5; ++i) { // f4=2|1f, f5=0|1|15|7d1 fId?, f6=b|c fSz?, f7=0|fc
    val=(int) input->readLong(2);
    int const expected[5]= {0x2, 0, 12, 0,0};
    if (val!=expected[i]) f << "f" << i+4 << "=" << val << ",";
  }
  for (int i=0; i<4; ++i) { // fl5=1|2, fl6=0|2f, fl7=0|3c
    val=(int) input->readULong(1);
    int const expected[4]= {0xfd, 1, 0, 0};
    if (val!=expected[i]) f << "fl" << i+4 << "=" << std::hex << val << std::dec << ",";
  }
  for (int i=0; i<4; ++i) { // f9=0|11,f10=0|1
    val=(int) input->readLong(2);
    int const expected[4]= {0,0,1,1};
    if (val!=expected[i]) f << "f" << i+9 << "=" << val << ",";
  }
  float fVal=float(input->readLong(4))/65536.f; // ~9
  if (fVal<0 || fVal>0)
    f << "d3=" << fVal << ",";
  val=(int) input->readLong(2); // 3|e|d
  if (val) f << "f13=" << val << ",";
  for (int i=0; i<2; ++i) { // fl8=1, fl9=0|1
    val=(int) input->readULong(1);
    if (val) f << "fl" << i+8 << "=" << std::hex << val << std::dec << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "DocHeader-C1:";
  val=(int) input->readLong(2); // 0|1
  if (val) f << "f0=" << val << ",";
  for (int i=0; i<2; ++i) dim[i]=(int) input->readLong(2);
  if (dim[0]||dim[1]) f << "dim=" << MWAWVec2i(dim[1],dim[0]) << ",";
  for (int i=0; i<4; ++i) { // always 0
    val=(int) input->readULong(2);
    if (val) f << "f" << i+1 << "=" << val << ",";
  }
  for (int i=0; i<4; ++i) { // fl0=1, fl1=0|1, fl2=0|1
    val=(int) input->readULong(1);
    if (val) f << "fl" << i << "=" << std::hex << val << std::dec << ",";
  }
  for (int i=0; i<3; ++i) { // f6=0|6e, f7=1
    val=(int) input->readULong(2);
    if (val) f << "f" << i+5 << "=" << val << ",";
  }
  for (int i=0; i<2; ++i) { // fl4=0|1|60|82,fl5=0|68|6e
    val=(int) input->readULong(1);
    if (val) f << "fl" << i+4 << "=" << std::hex << val << std::dec << ",";
  }
  f << "dim1=" << float(input->readLong(4))/65536.f << ","; // 7|19
  for (int i=0; i<2; ++i) {
    val=(int) input->readULong(1);
    static int const expected[2]= {2,1};
    if (val!=expected[i])
      f << "fl" << i+6 << "=" << std::hex << val << std::dec << ",";
  }
  for (int i=0; i<2; ++i)  // 7,7|19,19
    f << "dim" << i+2 << "=" << float(input->readLong(4))/65536.f << ",";
  for (int i=0; i<2; ++i) { // fl9=0|ea|3c
    val=(int) input->readULong(1);
    static int const expected[2]= {2,0};
    if (val!=expected[i])
      f << "fl" << i+8 << "=" << std::hex << val << std::dec << ",";
  }

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(pos+42, librevenge::RVNG_SEEK_SET);
  return true;
}

bool MacDraft5Parser::readDocFooter()
{
  MWAWInputStreamPtr input = getInput();
  if (input->isEnd())
    return false;
  long pos=input->tell();
  long endPos=pos+128;
  if (!input->checkPosition(endPos)) {
    input->seek(pos,librevenge::RVNG_SEEK_SET);
    return false;
  }

  for (int i=0; i<4; ++i) {
    pos=input->tell();
    libmwaw::DebugStream f;
    if (i==0)
      f << "Entries(DocFooter):";
    else
      f << "DocFooter-" << i << ":";
    int sSz=(int) input->readULong(1);
    if (sSz>31) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readDocFooter: string size seems bad\n"));
      f << "#sSz=" << sSz << ",";
      sSz=0;
    }
    std::string name("");
    for (int c=0; c<sSz; ++c) name+=(char) input->readULong(1);
    f << name << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+32,librevenge::RVNG_SEEK_SET);
  }
  return true;
}

bool MacDraft5Parser::readLibraryHeader()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  long endPos=pos+22;
  if (!input->checkPosition(endPos+4)) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readLibraryHeader: file seems too short\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(LibHeader):";
  int val;
  for (int i=0; i<2; ++i) { // f0=-1|0|4, f1=0|e4
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  int dim[4];
  for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
  f << "dim=" << MWAWBox2i(MWAWVec2i(dim[1],dim[0]),MWAWVec2i(dim[3],dim[2])) << ",";
  for (int i=0; i<5; ++i) { // f2=5|6, f3=f2+2
    val=(int) input->readLong(2);
    if (val) f << "f" << i+2 << "=" << val << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(endPos, librevenge::RVNG_SEEK_SET);

  return true;
}

bool MacDraft5Parser::readLibraryFooter()
{
  MWAWInputStreamPtr input = getInput();
  if (input->isEnd())
    return false;
  long pos=input->tell();
  long fSz=(long) input->readULong(4);
  long endPos=pos+4+fSz;
  if (!fSz || !input->checkPosition(endPos)|| fSz<30 || (fSz%34)<30 || (fSz%34)>31) {
    input->seek(pos,librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("MacDraft5Parser::readLibraryFooter: the zone size seems bad\n"));
    return false;
  }

  libmwaw::DebugStream f;
  f << "Entries(LibFooter):";
  int val;
  for (int i=0; i<3; ++i) { // always 0
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  int N=(int) input->readULong(2);
  if (30+34*N!=fSz && 31+34*N!=fSz) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readLibFooter:N seems bad\n"));
    f << "##N=" << N << ",";
    if (30+34*N>fSz)
      N=int((fSz-30)/34);
  }
  val=(int) input->readLong(2); // always 0 ?
  if (val) f << "f3=" << val << ",";
  val=(int) input->readLong(2); // always c
  if (val!=34) f << "#fSz=" << val << ",";
  long dataSz=input->readLong(4);
  if (dataSz && dataSz!=34*N) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readLibFooter:dataSize seems bad\n"));
    f << "##dataSz=" << dataSz << ",";
  }
  for (int i=0; i<7; ++i) {
    val=(int) input->readLong(2);
    static int const expected[]= {0,0x22,0,4,0,0,0};
    if (val!=expected[i]) f << "f" << i+4 << "=" << val << ",";
  }

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (long i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "LibFooter-" << i << ":";
    f << "ID=" << std::hex << input->readULong(4) << std::dec << ",";
    int sSz=(int) input->readULong(1);
    if (sSz>25) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readLibFooter:stringSize seems bad\n"));
      f << "##sSz=" << sSz << ",";
      sSz=0;
    }
    std::string text("");
    for (int c=0; c<sSz; ++c) text+=(char) input->readULong(1);
    f << text << ",";
    input->seek(pos+30, librevenge::RVNG_SEEK_SET);
    val=(int) input->readLong(2); // constant in a zone: 6|d
    if (val) f << "f0=" << val << ",";
    val=(int) input->readLong(2); // 0|1
    if (val) f << "f1=" << val << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  input->seek(endPos,librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// read an object
////////////////////////////////////////////////////////////
bool MacDraft5Parser::readObject(MacDraft5ParserInternal::Layout &layout)
{
  MWAWInputStreamPtr input = getInput();
  if (input->isEnd())
    return false;
  long pos=input->tell();
  long fSz=(long) input->readULong(2);
  if (fSz>=0x25 && (fSz%6)==1) {
    input->seek(pos,librevenge::RVNG_SEEK_SET);
    if (readLabel()) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find unexpected label\n"));
      return true;
    }
  }
  long endPos=pos+2+fSz;
  if (fSz<0x2e || !input->checkPosition(endPos)) {
    input->seek(pos,librevenge::RVNG_SEEK_SET);
    return false;
  }
  input->seek(pos+2,librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(Object):";
  MacDraft5ParserInternal::Shape shape;
  MWAWGraphicStyle &style=shape.m_style;
  shape.m_id=(int) input->readULong(4);
  if (shape.m_id<=1) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: id seems bad\n"));
    f << "###";
  }
  f << "id=" << std::hex << shape.m_id << std::dec << ",";
  int val=(int) input->readULong(1); // 0|80
  if (val) f << "type[h]=" << std::hex << val << std::dec << ",";
  shape.m_fileType=(int) input->readULong(1); //[0-6][0-b]
  switch (shape.m_fileType) {
  case 1:
    f << "line[single],";
    break;
  case 3:
    f << "poly1,";
    break;
  case 4:
    f << "poly2,";
    break;
  case 0x9: // orig=center
  case 0xa: // orig=corner
    f << "rectangle,";
    break;
  case 0xb:
    f << "rectOval,";
    break;
  case 0x14:
    f << "circle,";
    break;
  case 0x15:
    f << "arc1,";
    break;
  case 0x16:
    f << "arc2,";
    break;
  case 0x17: // checkme
    f << "arc3,";
    break;
  case 0x18:
    f << "circle[cent&rad],";
    break;
  case 0x1e:
    f << "spline1,";
    break;
  case 0x1f:
    f << "spline2,";
    break;
  case 0x28:
    f << "textbox,";
    break;
  case 0x30:
    f << "bitmap,";
    break;
  case 0x33:
    f << "group1,";
    break;
  case 0x35:
    f << "group2,";
    break;
  case 0x51:
    f << "quotation[h],";
    break;
  case 0x52:
    f << "quotation[v],";
    break;
  case 0x53:
    f << "quotation[d],";
    break;
  case 0x54:
    f << "quotation[para],";
    break;
  case 0x55: // circle center
    f << "quotation[circleC],";
    break;
  case 0x56: // circle diameter
    f << "quotation[circleD],";
    break;
  case 0x57: // axis
    f << "quotation[axis],";
    break;
  case 0x58: // angle
    f << "quotation[angle],";
    break;
  case 0x5e:
    f << "correspondance[quot/parent],";
    break;
  case 0x63:
    f << "quotation[surf],";
    break;
  case 0x64:
    f << "line[para],";
    break;
  case 0x65:
    f << "poly[para],";
    break;
  default:
    f << "type=" << std::hex << shape.m_fileType << std::dec << ",";
  }
  val=(int) input->readULong(4);
  if (val)
    f << "id2=" << std::hex << val << std::dec << ",";
  int rotation=(int) input->readLong(2);
  if (rotation) f << "rotate=" << rotation << ",";
  val=(int) input->readLong(2); // 0|2|13b3|2222
  if (val) f << "f0=" << val << ",";
  shape.m_labelId=(int) input->readULong(4);
  if (shape.m_labelId)
    f << "label[id]=" << std::hex << shape.m_labelId << std::dec << ",";
  val=(int) input->readULong(1);
  if (val) // alway 0?
    f << "f1=" << val << ",";
  val=(int) input->readULong(1);
  if (val!=1) {
    f << "width[line]=" << val << ",";
    style.m_lineWidth=val;
  }
  val=(int) input->readULong(1);
  if (val!=2) // alway 2?
    f << "f2=" << val << ",";
  int lineType=(int) input->readULong(1);
  switch (lineType) {
  case 0:
    f << "no[line],";
    style.m_lineWidth=0;
    break;
  case 1: // use color
    break;
  case 3: // use pattern
    f << "usePattern[line],";
    break;
  default:
    MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find unknown line type\n"));
    f << "#line[type]=" << lineType << ",";
    break;
  }
  // FIXME: storeme...
  int dashId=(int) input->readULong(1);
  if (dashId&0xf)
    f << "dash[id]=" << (dashId&0xf) << ",";
  if (dashId&0x80) {
    switch ((dashId>>4)&0x7) {
    case 0:
      f << "hairline,";
      style.m_lineWidth=0.;
      break;
    case 1:
      f << "width[line]=" << 0.5 << ",";
      style.m_lineWidth=0.5;
      break;
    case 2:
      f << "width[line]=" << 0.75 << ",";
      style.m_lineWidth=0.75;
      break;
    default:
      f <<"###width[line]=" << ((dashId>>4)&0x7) << ",";
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find unknown line width\n"));
      break;
    }
  }
  else if (dashId&0xf0) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find unknown dash[high]\n"));
    f << "##dash[high]=" << (dashId>>6) << ",";
  }
  val=(int) input->readULong(1); // low|end
  if (val) f << "arrow=" << std::hex << val << std::dec << ",";

  int colId=(int) input->readULong(2); // 0-78
  if (colId==0) {
    style.m_lineWidth=0;
    f << "no[line],";
  }
  else if (lineType==3)
    f << "patId=" << colId << ",";
  else if (!m_styleManager->getColor(colId, style.m_lineColor))
    f << "##colId=" << colId << ",";
  else if (!style.m_lineColor.isBlack())
    f << "col[line]=" << style.m_lineColor << ",";

  float pt[4];
  for (int i=0; i<2; ++i) pt[i]=float(input->readLong(4))/65536.f;
  f << "pos=" << MWAWVec2f(pt[1],pt[0]) << ",";
  val=(int) input->readULong(2); // [02][0134][04][01]
  if (val&1) f << "selected,";
  if (val&8) f << "lock,";
  val &= 0xFFF6;
  if (val) f << "fl1=" << std::hex << val << std::dec << ",";
  shape.m_nameId=(int) input->readULong(4);
  if (shape.m_nameId)
    f << "id[name]=" << std::hex << shape.m_nameId << std::dec << ",";
  switch (shape.m_fileType) {
  case 1: { // line
    if (fSz!=0x3e && fSz!=0x50) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find unknown line size\n"));
      f << "##fSz,";
      break;
    }
    val=(int) input->readLong(2);
    if (val) f << "angle[def]=" << val << ",";
    MWAWVec2f listPts[2];
    for (int i=0; i<2; ++i) {
      val=(int) input->readULong(1);
      if (val!=1-i) f << "g" << i << "=" << val << ",";
      val=(int) input->readULong(1);
      if (val!=0x81) f << "fl" << 2+i << "=" << std::hex << val << std::dec << ",";
      for (int j=0; j<2; ++j) pt[j]=float(input->readLong(4))/65536.f;
      listPts[i]=MWAWVec2f(pt[1],pt[0]);
      f << "pt" << i << "=" << listPts[i] << ",";
    }
    shape.m_type=MacDraft5ParserInternal::Shape::Basic;
    shape.m_shape=MWAWGraphicShape::line(listPts[0], listPts[1]);
    shape.m_isLine=true;
    if (fSz==0x3e) break;
    for (int i=0; i<9; ++i) { // always 0
      val=(int) input->readLong(2);
      if (val) f << "g" << i+2 << "=" << val << ",";
    }
    break;
  }
  case 3:
  case 4: // poly
  case 0x9: // rect1
  case 0xa: // rect2
  case 0xb: // rectOval : rect + corner
  case 0x14: // circle
  case 0x15: // arc1
  case 0x16: // arc2
  case 0x17: // arc3
  case 0x18: // circle[radius,oval]
  case 0x1e: // spline1
  case 0x1f: { // spline2
    int expectedSz=0;
    size_t nbPts=0;
    int nodeHeaderSz=0;
    int expectedNodeType=0;
    switch (shape.m_fileType) {
    case 3:
    case 4:
      if (fSz<48) {
        MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find unknown size\n"));
        f << "##fSz,";
        break;
      }
      expectedSz=-1;
      nodeHeaderSz=2;
      expectedNodeType=0x81;
      break;
    case 9:
    case 0xa:
      expectedSz=0x54;
      nbPts=4;
      nodeHeaderSz=2;
      expectedNodeType=0x81;
      break;
    case 0xb:
      expectedSz=0x60;
      nbPts=4;
      nodeHeaderSz=2;
      expectedNodeType=0x80;
      break;
    case 0x14:
      expectedSz=0x5a;
      nbPts=3;
      nodeHeaderSz=2;
      expectedNodeType=0x81;
      break;
    case 0x15:
    case 0x17:
      expectedSz=0x62;
      nbPts=3;
      nodeHeaderSz=2;
      expectedNodeType=0x81;
      break;
    case 0x16:
      expectedSz=0x66;
      nbPts=3;
      nodeHeaderSz=2;
      expectedNodeType=0x81;
      break;
    case 0x18:
      expectedSz=0x40;
      nbPts=2;
      break;
    case 0x1e:
    case 0x1f:
      if (fSz<58) {
        MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find unknown size\n"));
        f << "##fSz,";
        break;
      }
      expectedSz=-1;
      break;
    default:
      break;
    }
    if (expectedSz>=0 && fSz!=expectedSz) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find unknown size\n"));
      f << "##fSz,";
      break;
    }
    shape.m_type=MacDraft5ParserInternal::Shape::Basic;
    val=(int) input->readULong(1);
    if (val) // alway 2?
      f << "f3=" << val << ",";
    int surfType=(int) input->readULong(1);
    switch (surfType) {
    case 0:
      f << "no[surf],";
      break;
    case 1: // use color
      break;
    case 2: // use pattern
      f << "usePattern[surf],";
      break;
    case 3: // checkme
      f << "useGradiant[surf],";
      break;
    default:
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find unknown surf type\n"));
      f << "#surf[type]=" << surfType << ",";
      break;
    }
    colId=(int) input->readULong(2); // 0-78
    MWAWColor color;
    if (colId==0) // none
      ;
    else if (surfType==2)
      f << "patId[surf]=" << colId << ",";
    else if (surfType==3)
      f << "gradId[surf]=" << colId << ",";
    else if (m_styleManager->getColor(colId, color)) {
      if (!color.isWhite())
        f << "col[surf]=" << color << ",";
      style.setSurfaceColor(color);
    }
    else
      f << "##colId=" << colId << ",";
    if (shape.m_fileType==3 || shape.m_fileType==4) {
      val=(int) input->readULong(1);
      if (val==1) f << "closed,";
      else if (val) f << "##orig[pos]=" << val << ",";
      val=(int) input->readULong(1); // always 0
      if (val) f << "g0=" << val << ",";
      nbPts=(size_t) input->readULong(2);
      if (fSz<48+10*(int) nbPts) {
        MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: the number of points seems bad\n"));
        f << "##N=" << nbPts << ",";
        break;
      }
    }
    else if (shape.m_fileType==0x14) {
      val=(int) input->readULong(1); // 0-2
      if (val) f << "orig[pos]=" << val << ",";
      val=(int) input->readULong(1); // always 0
      if (val) f << "g0=" << val << ",";
      val=(int) input->readULong(2);
      if (val!=0x181) f << "g1=" << std::hex << val << std::dec << ",";
      for (int i=0; i<3; ++i) pt[i]=float(input->readLong(4))/65536.f;
      MWAWVec2f center(pt[1],pt[0]), radius(pt[2],pt[2]);
      f << "center=" << center << ",radius=" << pt[2] << ",";
      shape.m_shape=MWAWGraphicShape::circle(MWAWBox2f(center-radius, center+radius));
    }
    else if (shape.m_fileType==0x15||shape.m_fileType==0x16||shape.m_fileType==0x17) {
      val=(int) input->readULong(1); // 0-2
      if (val) f << "orig[pos]=" << val << ",";
      val=(int) input->readULong(1); // always 0
      if (val) f << "g0=" << val << ",";
      val=(int) input->readULong(2);
      if (val!=0x181) f << "g1=" << std::hex << val << std::dec << ",";
      int numData=shape.m_fileType==0x16 ? 4 : 3;
      for (int i=0; i<numData; ++i) pt[i]=float(input->readLong(4))/65536.f;
      MWAWVec2f center(pt[1],pt[0]), radius(pt[2],numData==4 ? pt[3] : pt[2]);
      f << "center=" << center << ",radius=" << radius << ",";
      float fileAngle[2];
      for (int i=0; i<2; ++i) fileAngle[i]=float(input->readLong(4))/65536.f;
      f << "angle=" << fileAngle[0] << "x" << fileAngle[0]+fileAngle[1] << ",";
      if (fileAngle[1]<0) {
        fileAngle[0]+=fileAngle[1];
        fileAngle[1]*=-1;
      }
      float angle[2] = { 90-fileAngle[0]-fileAngle[1], 90-fileAngle[0] };
      if (angle[1]>360) {
        int numLoop=int(angle[1]/360)-1;
        angle[0]-=numLoop*360;
        angle[1]-=numLoop*360;
        while (angle[1] > 360) {
          angle[0]-=360;
          angle[1]-=360;
        }
      }
      if (angle[0] < -360) {
        int numLoop=int(angle[0]/360)+1;
        angle[0]-=numLoop*360;
        angle[1]-=numLoop*360;
        while (angle[0] < -360) {
          angle[0]+=360;
          angle[1]+=360;
        }
      }
      // fixme
      shape.m_shape=MWAWGraphicShape::pie(MWAWBox2f(center-radius, center+radius),
                                          MWAWBox2f(center-radius, center+radius),
                                          MWAWVec2f(angle[0],angle[1]));
    }
    else if (shape.m_fileType==0x18) {
      val=(int) input->readULong(1); // 0-1
      if (val) f << "orig[pos]=" << val << ",";
      val=(int) input->readULong(1); // always 0
      if (val) f << "g1=" << val << ",";
      val=(int) input->readULong(2); // always 0
      if (val) f << "g2=" << val << ",";
    }
    else if (shape.m_fileType==0x1e || shape.m_fileType==0x1f) {
      val=(int) input->readULong(1);
      if (val==1) f << "closed,";
      else if (val) f << "##orig[pos]=" << val << ",";
      val=(int) input->readULong(1); // always 0
      if (val) f << "g0=" << val << ",";
      nbPts=(size_t) input->readULong(2);
      if (fSz<48+30*(int) nbPts+10) {
        MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: the number of points seems bad\n"));
        f << "##N=" << nbPts << ",";
        break;
      }

      f << "points=[";
      std::vector<MWAWGraphicShape::PathData> path;
      MWAWVec2f prevPoint[2];
      for (size_t i=0; i<=nbPts; ++i) {
        MWAWVec2f points[3];
        int nCoord=(i==nbPts) ? 1 : 3;
        for (int j=0; j<nCoord; ++j) {
          libmwaw::DebugStream f2;
          val=(int) input->readULong(1);
          if (val!=((i||j)?0:1)) f2 << "h0=" << val << ",";
          val=(int) input->readULong(1);
          if (val!=(j==0 ? 0x81 : 0)) f2 << "fl=" << std::hex << val << std::dec << ",";
          float pPos[2];
          for (int k=0; k<2; ++k) pPos[k]=float(input->readLong(4))/65536;
          points[j]=MWAWVec2f(pPos[1],pPos[0]);
          f << points[j];
          if (!f2.str().empty()) f << "[" << f2.str() << "]";
          if (j+1==nCoord) f << ",";
          else f << ":";
        }
        // checkme
        char pType = i==0 ? 'M' : 'C';
        path.push_back(MWAWGraphicShape::PathData(pType, points[0], (i==0) ? points[0] : prevPoint[0],
                       (i==0) ? points[0] : prevPoint[1]));
        prevPoint[0]=points[1];
        prevPoint[1]=points[2];
      }
      f << "],";
      shape.m_shape.m_type=MWAWGraphicShape::Path;
      shape.m_shape.m_path=path;
      break;
    }
    std::vector<MWAWVec2f> listPts;
    listPts.resize(nbPts);
    f << "pts=[";
    for (size_t i=0; i<nbPts; ++i) {
      libmwaw::DebugStream f2;
      if (nodeHeaderSz==2) {
        val=(int) input->readULong(1);
        if (val!=(i?0:1)) f2 << "h0=" << val << ",";
        val=(int) input->readULong(1);
        if (val!=expectedNodeType) f2 << "fl=" << std::hex << val << std::dec << ",";
      }
      for (int j=0; j<2; ++j) pt[j]=float(input->readLong(4))/65536.f;
      listPts[i]=MWAWVec2f(pt[1],pt[0]);
      f << listPts[i];
      if (!f2.str().empty())
        f << ":[" << f2.str() << "],";
      else
        f << ",";
    }
    f << "],";
    shape.m_type=MacDraft5ParserInternal::Shape::Basic;
    MWAWVec2f corner(0,0);
    if (shape.m_fileType==0xb) {
      val=(int) input->readULong(1); // 1-3
      if (val!=1) f << "orig[pos]=" << val << ",";
      val=(int) input->readULong(1); // always 0
      if (val) f << "h4=" << val << ",";
      val=(int) input->readULong(2); // always 4
      if (val!=4) f << "h5=" << val << ",";
      for (int i=0; i<2; ++i) pt[i]=float(input->readLong(4))/65536.f;
      corner=MWAWVec2f(pt[1],pt[0]);
      f << "corner=" << corner << ",";
    }
    switch (shape.m_fileType) {
    case 0x3:
    case 0x4: {
      shape.m_shape.m_type = MWAWGraphicShape::Polygon;
      std::vector<MWAWVec2f> &vertices=shape.m_shape.m_vertices;
      MWAWBox2f box;
      for (size_t i=0; i<nbPts; ++i) {
        vertices.push_back(listPts[i]);
        if (i==0)
          box=MWAWBox2f(listPts[i], listPts[i]);
        else
          box=box.getUnion(MWAWBox2f(listPts[i], listPts[i]));
      }
      shape.m_shape.m_bdBox=shape.m_shape.m_formBox=box;
      break;
    }
    case 0x9:
    case 0xa:
    case 0xb:
      shape.m_shape=MWAWGraphicShape::rectangle(MWAWBox2f(listPts[0], listPts[2]), corner);
      break;
    case 0x14: // already done
    case 0x15:
    case 0x16:
    case 0x17:
      break;
    case 0x18:
      listPts[1]=MWAWVec2f(listPts[1][1],listPts[1][0]);
      shape.m_shape=MWAWGraphicShape::circle(MWAWBox2f(listPts[0]-listPts[1], listPts[0]+listPts[1]));
      break;
    default: {
      static bool first=true;
      f << "###";
      if (first) {
        first=false;
        MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: oops can not store a shape\n"));
      }
      break;
    }
    }
    break;
  }
  case 0x28: { // text
    if (fSz!=0x64) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find unknown text size\n"));
      f << "##fSz,";
      break;
    }
    for (int i=0; i<2; ++i) { // always 0
      val=(int) input->readLong(2);
      if (val) f << "g" << i << "=" << val << ",";
    }
    for (int i=0; i<2; ++i) { // g2=Ø|1
      val=(int) input->readULong(1);
      if (val) f << "g" << i+2 << "=" << std::hex << val << std::dec << ",";
    }
    f << "ids?=["; // two big number, unsure
    for (int i=0; i<2; ++i) f << std::hex << input->readULong(4) << std::dec << ",";
    f << "],";
    val=(int) input->readLong(2);// 0|-1
    if (val) f << "g4=" << val << ",";
    for (int i=0; i<4; ++i) { // g5=1|fd, g8=0|1
      val=(int) input->readULong(1);
      if (val) f << "g" << i+5 << "=" << std::hex << val << std::dec << ",";
    }
    MWAWVec2f listPts[4];
    f << "pts=[";
    for (size_t i=0; i<4; ++i) {
      libmwaw::DebugStream f2;
      val=(int) input->readULong(1);
      if (val!=(i?0:1)) f2 << "h0=" << val << ",";
      val=(int) input->readULong(1);
      if (val) f2 << "fl=" << std::hex << val << std::dec << ",";
      for (int j=0; j<2; ++j) pt[j]=float(input->readLong(4))/65536.f;
      listPts[i]=MWAWVec2f(pt[1],pt[0]);
      f << listPts[i];
      if (!f2.str().empty())
        f << ":[" << f2.str() << "],";
      else
        f << ",";
    }
    f << "],";
    shape.m_type=MacDraft5ParserInternal::Shape::Text;
    shape.m_box=MWAWBox2f(listPts[0],listPts[2]);
    break;
  }
  case 0x30: { // bitmap
    if (fSz!=0x5e) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find unknown bitmap size\n"));
      f << "##fSz,";
      break;
    }
    for (int i=0; i<2; ++i) { // always 0
      val=(int) input->readLong(2);
      if (val) f << "g" << i << "=" << val << ",";
    }
    MWAWVec2f listPts[4];
    f << "pts=[";
    for (size_t i=0; i<4; ++i) {
      libmwaw::DebugStream f2;
      val=(int) input->readULong(1);
      if (val!=(i?0:1)) f2 << "h0=" << val << ",";
      val=(int) input->readULong(1);
      if (val) f2 << "fl=" << std::hex << val << std::dec << ",";
      for (int j=0; j<2; ++j) pt[j]=float(input->readLong(4))/65536.f;
      listPts[i]=MWAWVec2f(pt[1],pt[0]);
      f << listPts[i];
      if (!f2.str().empty())
        f << ":[" << f2.str() << "],";
      else
        f << ",";
    }
    f << "],";
    int dim[4];
    for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
    shape.m_bitmapDimensionList.push_back(MWAWBox2i(MWAWVec2i(dim[1],dim[0]),MWAWVec2i(dim[3],dim[2])));
    f << "dim=" << shape.m_bitmapDimensionList.back() << ",";
    shape.m_bitmapIdList.push_back((int) input->readULong(2));
    f << "id[bitmap]=" << shape.m_bitmapIdList.back() << ",";
    shape.m_type=MacDraft5ParserInternal::Shape::Bitmap;
    shape.m_box=MWAWBox2f(listPts[0],listPts[2]);
    break;
  }
  case 0x33:
  case 0x35: { // group
    int headerSize=shape.m_fileType==0x33 ? 0x2a : 60;
    if (fSz<headerSize) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find unknown group size\n"));
      f << "##fSz,";
      break;
    }
    if (shape.m_fileType==0x35) {
      for (int i=0; i<9; ++i) { // g0=0-2, g1=0-77, g6=0|100, g7=N
        val=(int) input->readLong(2);
        if (val) f << "g" << i << "=" << val << ",";
      }
    }
    int nChild=(int) input->readULong(2);
    if (fSz<headerSize+4*nChild) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find unknown group size\n"));
      f << "##fSz,";
      break;
    }
    shape.m_type=MacDraft5ParserInternal::Shape::Group;
    f << "child[id]=[";
    for (int i=0; i<nChild; ++i) {
      val=(int) input->readULong(4);
      shape.m_childList.push_back(val);
      f << std::hex << val << std::dec << ",";
    }
    f << "],";
    break;
  }
  case 0x51:  // quotation
  case 0x52:
  case 0x53:
  case 0x54:
  case 0x55:
  case 0x56:
  case 0x57:
  case 0x58: {
    long expectedSize=(shape.m_fileType==0x57) ? 0x106 : (shape.m_fileType==0x58) ? 0x17a : 0x176;
    if (fSz!=expectedSize) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find unknown quotation size\n"));
      f << "##fSz,";
      break;
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+52, librevenge::RVNG_SEEK_SET);

    pos=input->tell();
    f.str("");
    f << "Object-A:quotation,";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+82, librevenge::RVNG_SEEK_SET);

    pos=input->tell();
    f.str("");
    f << "Object-B:quotation,";
    f << "unkn=[";
    for (size_t i=0; i<8; ++i) {
      for (int j=0; j<2; ++j) pt[j]=float(input->readLong(4))/65536.f;
      f << MWAWVec2f(pt[1],pt[0]) << ",";
    }
    f << "],";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+64, librevenge::RVNG_SEEK_SET);

    pos=input->tell();
    f.str("");
    f << "Object-C:quotation,";
    int sSz=(int) input->readULong(1);
    if (sSz>31) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find unknown string size\n"));
      f << "##sSz=" << sSz << ",";
      break;
    }
    std::string text("");
    for (int i=0; i<sSz; ++i) text+=(char) input->readULong(1);
    f << text << ",";
    input->seek(pos+32, librevenge::RVNG_SEEK_SET);
    for (int i=0; i<8; ++i) { // always 0
      val=(int) input->readLong(2);
      if (val)
        f << "f" << i << "=" << val << ",";
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+48, librevenge::RVNG_SEEK_SET);

    pos=input->tell();
    f.str("");
    f << "Object-D:quotation,";
    MWAWVec2f listPts[13];
    f << "pts=[";
    int numPts=shape.m_fileType==0x57 ? 1 : shape.m_fileType==0x58 ? 11 : 13;
    for (int i=0; i<numPts; ++i) {
      libmwaw::DebugStream f2;
      val=(int) input->readULong(1);
      if (val!=(i?0:1)) f2 << "h0=" << val << ",";
      val=(int) input->readULong(1);
      if (val!=0x81) f2 << "fl=" << std::hex << val << std::dec << ",";
      for (int j=0; j<2; ++j) pt[j]=float(input->readLong(4))/65536.f;
      listPts[i]=MWAWVec2f(pt[1],pt[0]);
      f << listPts[i];
      if (!f2.str().empty())
        f << ":[" << f2.str() << "],";
      else
        f << ",";
    }
    f << "],";
    if (shape.m_fileType==0x57) {
      for (int j=0; j<2; ++j) pt[j]=float(input->readLong(4))/65536.f;
      f << "axis,dim=" << MWAWVec2f(pt[1],pt[0]) << ",";
    }
    else if (shape.m_fileType==0x58) {
      f << "angle,unkn=[";
      for (int i=0; i<3; ++i) f << float(input->readLong(4))/65536.f << ",";
      f << "],";
      // then 0004002d0000000311050000
    }
    static bool first=true;
    if (first) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find some quotations, unimplemented\n"));
      first=false;
    }
    break;
  }
  case 0x5e: { // correspondance between surface quotation and parent
    if (fSz!=0x3e) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find unknown correspondance quotation size\n"));
      f << "##fSz,";
      break;
    }
    for (int i=0; i<7; ++i) { // g0=1, g1=4b40, g2=dc00
      val=(int) input->readULong(2);
      if (val) f << "g" << i << "=" << std::hex << val << std::dec << ",";
    }
    f << "parent[id]=" << std::hex << input->readULong(4) << std::dec << ",";
    shape.m_type=MacDraft5ParserInternal::Shape::Group;
    shape.m_childList.push_back((int)input->readULong(4));
    f << "child[id]=" << std::hex << shape.m_childList.back() << std::dec << ",";
    break;
  }
  case 0x63: { // surface quotation
    if (fSz!=0x62) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find unknown surface quotation size\n"));
      f << "##fSz,";
      break;
    }
    for (int i=0; i<6; ++i) { // g0=1, g1=1, g2=7, g4=c, g5=0: font definition
      val=(int) input->readLong(2);
      if (val) f << "g" << i << "=" << val << ",";
    }
    for (int i=0; i<2; ++i) pt[i]=float(input->readLong(4))/65536.f;
    f << "pos1=" << MWAWVec2f(pt[1],pt[0]) << ",";;
    for (int i=0; i<5; ++i) { // g6=4
      val=(int) input->readLong(2);
      if (val) f << "g" << i+6 << "=" << val << ",";
    }
    f << "parent[id]=" << input->readULong(4) << ",";
    int sSz=(int) input->readULong(1);
    if (sSz>23) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find unexpected text size\n"));
      sSz=0;
    }
    std::string text("");
    for (int i=0; i<sSz; ++i) text+=(char) input->readULong(1);
    f << text << ",";
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    static bool first=true;
    if (first) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find some surface quotations, unimplemented\n"));
      first=false;
    }
    break;
  }
  case 0x64: { // line para
    if (fSz!=0x70) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find unknown line[para] size\n"));
      f << "##fSz,";
      break;
    }
    val=(int) input->readULong(1);
    if (val) // alway 0?
      f << "g0=" << val << ",";
    int surfType=(int) input->readULong(1);
    switch (surfType) {
    case 0:
      f << "no[surf],";
      break;
    case 1: // use color
      break;
    case 2: // use pattern
      f << "usePattern[surf],";
      break;
    case 3: // checkme
      f << "useGradiant[surf],";
      break;
    default:
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find unknown surf type\n"));
      f << "#surf[type]=" << surfType << ",";
      break;
    }
    colId=(int) input->readULong(2); // 0-78
    MWAWColor color;
    if (colId==0) // none
      ;
    else if (surfType==2)
      f << "patId[surf]=" << colId << ",";
    else if (surfType==3)
      f << "gradId[surf]=" << colId << ",";
    else if (m_styleManager->getColor(colId, color)) {
      if (!color.isWhite())
        f << "col[surf]=" << color << ",";
      style.setSurfaceColor(color);
    }
    else
      f << "##colId=" << colId << ",";

    for (int i=0; i<2; ++i) pt[i]=float(input->readLong(4))/65536.f;
    f << "dim?=" << MWAWVec2f(pt[1],pt[0]) << ",";
    f << "points=[";
    std::vector<MWAWVec2f> listPts;
    for (size_t i=0; i<2; ++i) {
      for (int j=0; j<3; ++j) { // checkme: orignal line, line1, line2?
        libmwaw::DebugStream f2;
        val=(int) input->readULong(1);
        if (val!=((i||j)?0:1)) f2 << "h0=" << val << ",";
        val=(int) input->readULong(1);
        if (val!=(j==0 ? 0x81 : 0x90)) f2 << "fl=" << std::hex << val << std::dec << ",";
        float pPos[2];
        for (int k=0; k<2; ++k) pPos[k]=float(input->readLong(4))/65536;
        listPts.push_back(MWAWVec2f(pPos[1],pPos[0]));
        f << listPts.back();
        if (!f2.str().empty()) f << "[" << f2.str() << "]";
        if (j+1==3) f << ",";
        else f << ":";
      }
    }
    static bool first=true;
    if (first) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find some line[para], unimplemented\n"));
      first=false;
    }
    break;
  }
  case 0x65: { // poly para
    if (fSz<58) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find unknown poly[para] size\n"));
      f << "##fSz,";
      break;
    }
    for (int i=0; i<2; ++i) {
      static char const *(wh[])= {"inter", "surf"};
      val=(int) input->readULong(1);
      if (val) // alway 0?
        f << "g0=" << val << ",";
      int surfType=(int) input->readULong(1);
      switch (surfType) {
      case 0:
        f << "no[" << wh[i] << "],";
        break;
      case 1: // use color
        break;
      case 2: // use pattern
        f << "usePattern[" << wh[i] << "],";
        break;
      case 3: // checkme
        f << "useGradiant[" << wh[i] << "],";
        break;
      default:
        MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find unknown surf type\n"));
        f << "#" << wh[i] << "[type]=" << surfType << ",";
        break;
      }
      colId=(int) input->readULong(2); // 0-78
      MWAWColor color;
      if (colId==0) // none
        ;
      else if (surfType==2)
        f << "patId[" << wh[i] << "]=" << colId << ",";
      else if (surfType==3)
        f << "gradId[" << wh[i] << "]=" << colId << ",";
      else if (m_styleManager->getColor(colId, color)) {
        if (!color.isWhite())
          f << "col[" << wh[i] << "]=" << color << ",";
        if (i==1)
          style.setSurfaceColor(color);
      }
      else
        f << "##colId=" << colId << ",";
    }

    for (int i=0; i<3; ++i) { // g0=9,g1=0,g2=2|3|e
      val=(int) input->readLong(2);
      if (val)
        f << "g" << i << "=" << val << ",";
    }
    val=(int) input->readLong(1);
    if (val==1) f << "closed,";
    else if (val) f << "poly[type]=" << val << ",";
    val=(int) input->readLong(1); // always 0
    if (val) f << "g7=" << val << ",";
    int nbPts=(int) input->readULong(2);
    if (fSz<58+30*nbPts) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: numbers of points of unknown poly[para] seems bad\n"));
      f << "##nbPts=" << nbPts << ",";
      break;
    }
    std::vector<MWAWVec2f> listPts;
    for (int p=0; p<3; ++p) {
      f << "poly" << p << "=[";
      for (int j=0; j<nbPts; ++j) {
        libmwaw::DebugStream f2;
        val=(int) input->readULong(1);
        if (val!=((p||j)?0:1)) f2 << "h0=" << val << ",";
        val=(int) input->readULong(1);
        if (val!=(p==0 ? 0x81 : 0x90)) f2 << "fl=" << std::hex << val << std::dec << ",";
        float pPos[2];
        for (int k=0; k<2; ++k) pPos[k]=float(input->readLong(4))/65536;
        listPts.push_back(MWAWVec2f(pPos[1],pPos[0]));
        f << listPts.back();
        if (!f2.str().empty()) f << "[" << f2.str() << "]";
        if (j+1==nbPts) f << ",";
        else f << ":";
      }
      f << "],";
    }
    static bool first=true;
    if (first) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find some poly[para], unimplemented\n"));
      first=false;
    }
    break;
  }
  default:
    MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find some unknown data\n"));
    f << "#unparsed,";
  }

  if (input->tell()!=pos && input->tell()!=endPos)
    ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(endPos,librevenge::RVNG_SEEK_SET);
  ++layout.m_objectId;

  pos=input->tell();
  if (shape.m_labelId>0 && !readLabel()) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: can not find the label\n"));
    input->seek(pos,librevenge::RVNG_SEEK_SET);
  }

  pos=input->tell();
  if (shape.m_nameId && !readStringList()) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: can not find name list\n"));
    input->seek(pos,librevenge::RVNG_SEEK_SET);
  }

  if (shape.m_fileType!=0x28)
    return true;
  pos=input->tell();
  f.str("");
  f << "Entries(Text):";
  bool ok=input->checkPosition(pos+24);
  int tSz=0;
  if (ok) {
    for (int i=0; i<2; ++i) {
      int dim[4];
      for (int j=0; j<4; ++j) dim[j]=(int) input->readLong(2);
      f << "dim" << i << "=" << MWAWBox2i(MWAWVec2i(dim[1],dim[0]),MWAWVec2i(dim[3],dim[2])) << ",";
    }
    tSz=(int) input->readULong(2);
    ok=(tSz<20000 && input->checkPosition(pos+24+tSz));
  }
  int nData=0;
  if (ok) {
    f << "f0=" << input->readULong(4) << ",";
    std::string text("");
    for (int i=0; i<tSz; ++i) text+=(char) input->readULong(1);
    f << text << ",";
    nData=(int) input->readULong(2);
  }
  endPos=pos+24+tSz+nData*20;
  if (!ok || nData>100 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: can not read text\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote("###Text");
    return true;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  for (int i=0; i<nData; ++i) {
    pos=input->tell();
    f.str("");
    f << "Text-C" << i << ":";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+20, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// read an label
////////////////////////////////////////////////////////////
bool MacDraft5Parser::readLabel()
{
  MWAWInputStreamPtr input = getInput();
  if (input->isEnd())
    return false;
  long pos=input->tell();
  long fSz=(long) input->readULong(2);
  long endPos=pos+2+fSz;
  if (fSz<0x25 || (fSz%6)!=1 || !input->checkPosition(endPos)) {
    input->seek(pos,librevenge::RVNG_SEEK_SET);
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(Label):";
  long val;
  for (int i=0; i<3; ++i) { // always 0
    val=input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  int N=(int) input->readLong(2);
  if (6*N+0x1f!=fSz) {
    input->seek(pos,librevenge::RVNG_SEEK_SET);
    return false;
  }
  for (int i=0; i<4; ++i) {
    val=input->readLong(4);
    static long const expected[4]= {6,6,6,2 };
    if (val!=expected[i]) f << "f" << i+3 << "=" << val << ",";
  }
  for (int i=0; i<3; ++i) { // always 0
    val=input->readLong(2);
    if (val) f << "f" << i+7 << "=" << val << ",";
  }
  for (int i=0; i<N; ++i) {
    f << "unkn" << i << "=[";
    f << input->readLong(2) << ","; // 0 or small int
    f << std::hex << input->readULong(4) << std::dec; // id or not
    f << "],";
  }
  val=(int) input->readULong(1); // always 0
  if (val) f << "fl=" << std::hex << val << std::dec << ",";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(endPos,librevenge::RVNG_SEEK_SET);
  return true;
}

bool MacDraft5Parser::readStringList()
{
  MWAWInputStreamPtr input = getInput();
  if (input->isEnd())
    return false;
  long pos=input->tell();
  long fSz=(long) input->readULong(2);
  long endPos=pos+2+fSz;
  if (fSz<14 || !input->checkPosition(endPos)) {
    input->seek(pos,librevenge::RVNG_SEEK_SET);
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(StringLists):";
  int val;
  for (int i=0; i<2; ++i) { // always 1,1
    val=(int) input->readLong(2);
    if (val==1)
      continue;
    if (fSz>=0x2e) {
      input->seek(pos,librevenge::RVNG_SEEK_SET);
      return false;
    }
    f << "f" << i << "=" << val << ",";
  }
  for (int i=0; i<10; ++i) { // find s0, s2, s4
    int sSz=(int) input->readULong(1);
    if (input->tell()+sSz>endPos) {
      input->seek(pos,librevenge::RVNG_SEEK_SET);
      return false;
    }
    if (!sSz) continue;
    std::string text("");
    for (int j=0; j<sSz; ++j) text += (char) input->readULong(1);
    f << "s" << i << "=" << text << ",";
  }
  if (input->tell()!=endPos)
    ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(endPos,librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// try to read the print info zone
////////////////////////////////////////////////////////////
bool MacDraft5Parser::readPrintInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  long endPos=pos+120;
  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readPrintInfo: file seems too short\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(PrintInfo):";
  libmwaw::PrinterInfo info;
  if (!info.read(input)) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readPrintInfo: can not read print info\n"));
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

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// layout
////////////////////////////////////////////////////////////
bool MacDraft5Parser::readLayout(MacDraft5ParserInternal::Layout &layout)
{
  MWAWEntry const &entry=layout.m_entry;
  MWAWInputStreamPtr input = getInput();
  if (!entry.valid() || !input->checkPosition(entry.begin())) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readLayout: the entry seems bad\n"));
    return false;
  }
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Layout[def]:";
  int val=(int) input->readULong(4);
  bool checkN=false;
  if (m_state->m_isLibrary) {
    layout.m_N=val;
    checkN=true;
  }
  else if (val!=layout.m_N) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readLayout: N seems bad\n"));
    f << "###";
    if (layout.m_N==0) {
      layout.m_N=val;
      checkN=true;
    }
  }
  f << "N=" << val << ",";
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  while (!input->isEnd()) {
    if (checkN && layout.m_N==layout.m_objectId)
      return true;
    long pos=input->tell();
    if (pos>=entry.end())
      break;
    if (!readObject(layout)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
  }

  long pos=input->tell();
  if (pos<entry.end()) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readLayout: find some extra data\n"));
    ascii().addPos(input->tell());
    ascii().addNote("Layout[extra]:###");
  }
  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// last zone
////////////////////////////////////////////////////////////
bool MacDraft5Parser::readPICT(MWAWEntry const &entry, librevenge::RVNGBinaryData &pict)
{
  MWAWInputStreamPtr input = getInput();
  pict.clear();
  if (!input || !entry.valid() || entry.length()<0xd) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readPict: entry is invalid\n"));
    return false;
  }

  libmwaw::DebugStream f;
  f << "Entries(RSRC" << entry.type() << ")[" << entry.id() << "]:";
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  input->readDataBlock(entry.length(), pict);

#ifdef DEBUG_WITH_FILES
  if (!entry.isParsed()) {
    ascii().skipZone(entry.begin(), entry.end()-1);
    libmwaw::DebugStream f2;
    f2 << "RSRC-" << entry.type() << "_" << entry.id() << ".pct";
    libmwaw::Debug::dumpFile(pict, f2.str().c_str());
  }
#endif

  ascii().addPos(entry.begin()-16);
  ascii().addNote(f.str().c_str());

  entry.setParsed(true);
  return true;
}

bool MacDraft5Parser::readViews(MWAWEntry const &entry, bool inRsrc)
{
  if (inRsrc && !getRSRCParser()) return false;
  MWAWInputStreamPtr input = inRsrc ? getRSRCParser()->getInput() : getInput();
  if (!input || !entry.valid() || entry.length()<38 || (entry.length()%38)<30 || (entry.length()%38)>31) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readViews: entry is invalid\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile = inRsrc ? getRSRCParser()->ascii() : ascii();
  libmwaw::DebugStream f;
  f << "Entries(Views):";
  if (entry.id()!=1) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readViews: entry id seems odd\n"));
    f << "##id=" << entry.id() << ",";
  }
  int val;
  for (int i=0; i<3; ++i) { // always 0
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  int N=(int) input->readULong(2);
  if (30+38*N!=entry.length() && 31+38*N!=entry.length()) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readViews:N seems bad\n"));
    f << "##N=" << N << ",";
    if (30+38*N>entry.length())
      N=int((entry.length()-30)/38);
  }
  val=(int) input->readLong(2); // always 0 ?
  if (val) f << "f3=" << val << ",";
  val=(int) input->readLong(2); // always 38
  if (val!=38) f << "#fSz=" << val << ",";
  long dataSz=input->readLong(4);
  if (dataSz!=38*N) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readViews:dataSize seems bad\n"));
    f << "##dataSz=" << dataSz << ",";
  }
  for (int i=0; i<7; ++i) {
    val=(int) input->readLong(2);
    static int const expected[]= {0,0x26,0,0x1e,0,0,0};
    if (val!=expected[i]) f << "f" << i+4 << "=" << val << ",";
  }

  ascFile.addPos(entry.begin()-(inRsrc ? 4 : 16));
  ascFile.addNote(f.str().c_str());

  for (long i=0; i<N; ++i) {
    long pos=input->tell();
    f.str("");
    f << "Views-" << i << ":";
    int sSz=(int) input->readULong(1);
    if (sSz>31) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readViews:stringSize seems bad\n"));
      f << "##sSz=" << sSz << ",";
      sSz=0;
    }
    std::string name("");
    for (int c=0; c<sSz; ++c) name += (char) input->readULong(1);
    f << name << ",";
    input->seek(pos+32, librevenge::RVNG_SEEK_SET);
    int dim[2];
    for (int j=0; j<2; ++j) dim[j]=(int) input->readULong(2);
    f << MWAWVec2i(dim[0],dim[1]) << ",";
    val=(int) input->readULong(2); // 0 or 1
    if (val) f << "page=?" << val << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// PICT list
////////////////////////////////////////////////////////////
bool MacDraft5Parser::readPICTList(MWAWEntry const &entry, bool inRsrc)
{
  if (inRsrc && !getRSRCParser()) return false;
  MWAWInputStreamPtr input = inRsrc ? getRSRCParser()->getInput() : getInput();
  if (!input || !entry.valid() || entry.length()<12 || (entry.length()%12)!=0) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readPICTLists: entry is invalid\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile = inRsrc ? getRSRCParser()->ascii() : ascii();
  libmwaw::DebugStream f;
  f << "Entries(PICTNot):";
  if (entry.id()!=0) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readPICTLists: entry id seems odd\n"));
    f << "##id=" << entry.id() << ",";
  }
  int N=int(entry.length()/12);
  for (long i=0; i<N; ++i) {
    f << "[";
    f << std::hex << input->readULong(4) << std::dec << ","; // big number, maybe flag
    int val=(int) input->readLong(2); // always 0
    if (val)
      f << val << ",";
    else
      f << "_,";
    std::string type(""); // PICT
    for (int c=0; c<4; ++c) type+=(char) input->readULong(1);
    f << type << ":" << input->readLong(2); // ID
    f << "],";
  }
  ascFile.addPos(entry.begin()-(inRsrc ? 4 : 16));
  ascFile.addNote(f.str().c_str());
  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// RSRC layout
////////////////////////////////////////////////////////////
bool MacDraft5Parser::readLayoutDefinitions(MWAWEntry const &entry, bool inRsrc)
{
  if (inRsrc && !getRSRCParser()) return false;
  MWAWInputStreamPtr input = inRsrc ? getRSRCParser()->getInput() : getInput();
  if (!input || !entry.valid() || entry.length()<30 || (entry.length()%34)<30 || (entry.length()%34)>31) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readLayoutDefinitions: entry is invalid\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile = inRsrc ? getRSRCParser()->ascii() : ascii();
  libmwaw::DebugStream f;
  f << "Entries(Layout):";
  if (entry.id()!=0) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readLayoutDefinitions: entry id seems odd\n"));
    f << "##id=" << entry.id() << ",";
  }
  int val;
  for (int i=0; i<3; ++i) { // always 0
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  int N=(int) input->readULong(2);
  if (30+34*N!=entry.length() && 31+34*N!=entry.length()) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readLayoutDefinitions:N seems bad\n"));
    f << "##N=" << N << ",";
    if (30+34*N>entry.length())
      N=int((entry.length()-30)/34);
  }
  val=(int) input->readLong(2); // always 0 ?
  if (val) f << "f3=" << val << ",";
  val=(int) input->readLong(2); // always c
  if (val!=34) f << "#fSz=" << val << ",";
  long dataSz=input->readLong(4);
  if (dataSz && dataSz!=34*N) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readLayoutDefinitions:dataSize seems bad\n"));
    f << "##dataSz=" << dataSz << ",";
  }
  for (int i=0; i<7; ++i) {
    val=(int) input->readLong(2);
    static int const expected[]= {0,0x22,0,0x10,0,0,0};
    if (val!=expected[i]) f << "f" << i+4 << "=" << val << ",";
  }

  ascFile.addPos(entry.begin()-(inRsrc ? 4 : 16));
  ascFile.addNote(f.str().c_str());

  for (long i=0; i<N; ++i) {
    long pos=input->tell();
    f.str("");
    MacDraft5ParserInternal::Layout layout;
    for (int c=0; c<16; ++c) {
      char ch=(char) input->readULong(1);
      if (ch==0) break;
      layout.m_name+=ch;
    }
    input->seek(pos+16, librevenge::RVNG_SEEK_SET);
    for (int j=0; j<3; ++j) { // f0=3f|ae|be|bf, f1=4-10, f2=-1..4
      val=(int) input->readLong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }
    layout.m_N=(int) input->readULong(4);
    layout.m_entry.setBegin((long) input->readULong(4));
    layout.m_entry.setLength((long) input->readULong(4));
    layout.m_extra=f.str();
    m_state->m_layoutList.push_back(layout);
    f.str("");
    f << "Layout-" << i << ":" << layout;
    input->seek(pos+34, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// RSRC unknown
////////////////////////////////////////////////////////////
bool MacDraft5Parser::readLinks(MWAWEntry const &entry, bool inRsrc)
{
  if (inRsrc && !getRSRCParser()) return false;
  MWAWInputStreamPtr input = inRsrc ? getRSRCParser()->getInput() : getInput();
  if (!input || !entry.valid() || entry.length()!=0x30) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readLinks: entry is invalid\n"));
    return false;
  }
  entry.setParsed(true);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile = inRsrc ? getRSRCParser()->ascii() : ascii();
  libmwaw::DebugStream f;
  f << "Entries(Links)[" << entry.id() << "]:";
  std::string name(""); // MD40
  for (int i=0; i<4; ++i) name+=(char) input->readULong(1);
  f << name << ",";
  for (int i=0; i<21; ++i) { // f0=3b0, f11=137|145, fl13=1, fl17=fl11+1
    int val=(int) input->readULong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascFile.addPos(entry.begin()-(inRsrc ? 4 : 16));
  ascFile.addNote(f.str().c_str());
  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
//
// send data
//
////////////////////////////////////////////////////////////

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
