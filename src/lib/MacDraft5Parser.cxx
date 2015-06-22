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
#include <stack>

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
//! generic class used to store shape in MWAWDraftParser
struct Shape {
  //! the different shape
  enum Type { Basic, Bitmap, Group, Text, Unknown };

  //! constructor
  Shape() : m_type(Unknown), m_fileType(0), m_box(), m_origin(), m_style(), m_shape(), m_otherStyleList(), m_otherShapeList(), m_isLine(false),
    m_id(-1), m_parentId(-1), m_modifierId(-1), m_nameId(-1),
    m_posToFontMap(), m_paragraph(), m_textEntry(),
    m_childList(), m_bitmapIdList(), m_bitmapDimensionList(), m_isSent(false)
  {
  }

  //! return the shape bdbox
  MWAWBox2f getBdBox() const
  {
    return m_type==Basic ? m_shape.getBdBox() : m_box;
  }
  //! translate a shape
  void translate(MWAWVec2f const &dir)
  {
    if (m_type==Basic)
      m_shape.translate(dir);
    m_box=MWAWBox2f(m_box[0]+dir, m_box[1]+dir);
    m_origin=m_origin+dir;
  }
  //! transform a shape
  void transform(float rotate, bool flipX, MWAWVec2f const &center)
  {
    if (rotate<0 || rotate>0) {
      if (m_type==Basic) {
        m_shape=m_shape.rotate(rotate, center);
        m_box=m_shape.getBdBox();
      }
      else if (m_type!=Bitmap) {
        m_style.m_rotate+=rotate;
        float angle=rotate*float(M_PI/180.);
        m_box=rotateBox(m_box, angle, center);
        MWAWVec2f decal=center-MWAWVec2f(std::cos(angle)*center[0]-std::sin(angle)*center[1],
                                         std::sin(angle)*center[0]+std::cos(angle)*center[1]);
        m_origin=MWAWVec2f(std::cos(angle)*m_origin[0]-std::sin(angle)*m_origin[1],
                           std::sin(angle)*m_origin[0]+std::cos(angle)*m_origin[1])+decal;
      }
    }
    if (flipX) {
      if (m_type==Basic) {
        m_shape.scale(MWAWVec2f(-1,1));
        m_shape.translate(MWAWVec2f(2*center[0],0));
      }
      m_style.m_flip[0]=!m_style.m_flip[0];
      m_box=MWAWBox2f(MWAWVec2f(2*center[0]-m_box[1][0],m_box[0][1]), MWAWVec2f(2*center[0]-m_box[0][0],m_box[1][1]));
      m_origin=MWAWVec2f(2*center[0]-m_origin[0],m_origin[1]);
    }
  }
  //! returns the rotation of a box
  static MWAWBox2f rotateBox(MWAWBox2f const &box, float angle, MWAWVec2f const &center)
  {
    MWAWVec2f decal=center-MWAWVec2f(std::cos(angle)*center[0]-std::sin(angle)*center[1],
                                     std::sin(angle)*center[0]+std::cos(angle)*center[1]);
    MWAWBox2f fBox;
    for (int i=0; i < 4; ++i) {
      MWAWVec2f pt=MWAWVec2f(box[i%2][0],box[i/2][1]);
      pt = MWAWVec2f(std::cos(angle)*pt[0]-std::sin(angle)*pt[1],
                     std::sin(angle)*pt[0]+std::cos(angle)*pt[1])+decal;
      if (i==0) fBox=MWAWBox2f(pt,pt);
      else fBox=fBox.getUnion(MWAWBox2f(pt,pt));
    }

    return fBox;
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
  //! the graphic shape ( for basic geometric form )
  MWAWGraphicShape m_shape;
  //! the other graphic style ( for complex basic geometric form )
  std::vector<MWAWGraphicStyle> m_otherStyleList;
  //! other graphic shapes ( for complex basic geometric form )
  std::vector<MWAWGraphicShape> m_otherShapeList;
  //! flag to know if the shape is a line
  bool m_isLine;
  //! the shape id
  long m_id;
  //! the parent id
  long m_parentId;
  //! the modifier id
  long m_modifierId;
  //! the name id
  long m_nameId;
  //! the font ( for a text box)
  std::map<long,MWAWFont> m_posToFontMap;
  //! the paragraph ( for a text box)
  MWAWParagraph m_paragraph;
  //! the textbox entry (main text)
  MWAWEntry m_textEntry;
  //! the child list id ( for a group )
  std::vector<int> m_childList;
  //! the list of bitmap id ( for a bitmap)
  std::vector<int> m_bitmapIdList;
  //! the list of bitmap dimension ( for a bitmap)
  std::vector<MWAWBox2i> m_bitmapDimensionList;
  //! a flag used to know if the object is sent to the listener or not
  mutable bool m_isSent;
};

//!  Internal and low level: a class used to store layout definition of a MacDraf5Parser
struct Layout {
  //! constructor
  Layout(int id) : m_id(id), m_entry(), m_N(0), m_objectId(0), m_name(""), m_shapeList(), m_rootList(), m_idToShapePosMap(), m_extra("")
  {
  }
  //! returns true if the layout contains no shape
  bool isEmpty() const
  {
    return m_shapeList.empty();
  }
  //! returns a child corresponding to an id
  shared_ptr<Shape> findShape(long id, bool normallyExist=true) const
  {
    if (!id) return shared_ptr<Shape>();
    if (m_idToShapePosMap.find(id)==m_idToShapePosMap.end()) {
      if (normallyExist) {
        MWAW_DEBUG_MSG(("MacDraft5ParserInternal::Layout::find shape: can not find shape %lx\n", (unsigned long) id));
      }
      return shared_ptr<Shape>();
    }
    return m_shapeList[m_idToShapePosMap.find(id)->second];
  }
  //! try to check/update the parent relations are compatible with group childs, no loop exist, ...
  void updateRelations();
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Layout const &lay)
  {
    o << lay.m_name.cstr() << ",";
    o << std::hex << lay.m_entry.begin() << "<->" << lay.m_entry.end() << std::dec << "[" << lay.m_N << "],";
    o << lay.m_extra;
    return o;
  }
  //! the layout id
  int m_id;
  //! the layout position in the data fork
  MWAWEntry m_entry;
  //! the number of elements
  int m_N;
  //! the object number
  int m_objectId;
  //! the layout name
  librevenge::RVNGString m_name;
  //! the shapes list
  std::vector<shared_ptr<Shape> > m_shapeList;
  //! the root position list
  std::vector<size_t> m_rootList;
  //! a map id to position in shapeList
  std::map<long, size_t> m_idToShapePosMap;
  //! extra data
  std::string m_extra;
};

void Layout::updateRelations()
{
  // first check that all id are filled are that there are no duplicated id
  std::stack<size_t> badPosStack;
  for (size_t i=0; i<m_shapeList.size(); ++i) {
    if (!m_shapeList[i]) continue;
    long id=m_shapeList[i]->m_id;
    if (!id)
      badPosStack.push(i);
    else if (m_idToShapePosMap.find(id)==m_idToShapePosMap.end())
      m_idToShapePosMap[id]=i;
    else
      badPosStack.push(i);
  }
  if (!badPosStack.empty()) {
    MWAW_DEBUG_MSG(("MacDraft5ParserInternal::Layout::updateRelations: find some bad id\n"));
    long newId= m_idToShapePosMap.empty() ? 1000 :  m_idToShapePosMap.rbegin()->first;
    while (!badPosStack.empty()) {
      size_t pos=badPosStack.top();
      badPosStack.pop();
      m_shapeList[pos]->m_id=++newId;
      m_idToShapePosMap[newId]=pos;
    }
  }
  // create the parent list and the root list
  std::map<long, long> idToParentMap;
  m_rootList.clear();
  size_t numShapes=0;
  for (size_t i=0; i<m_shapeList.size(); ++i) {
    if (!m_shapeList[i]) continue;
    ++numShapes;
    if (!m_shapeList[i]->m_parentId) {
      m_rootList.push_back(i);
      continue;
    }
    idToParentMap[m_shapeList[i]->m_id]=m_shapeList[i]->m_parentId;
  }
  // now check if all group child is correct and remove bad child
  for (size_t i=0; i<m_shapeList.size(); ++i) {
    if (!m_shapeList[i]) continue;
    Shape &shape=*m_shapeList[i];
    long id=shape.m_id;
    for (size_t c=0; c<shape.m_childList.size(); ++c) {
      if (idToParentMap.find(shape.m_childList[c])!=idToParentMap.end() &&
          idToParentMap.find(shape.m_childList[c])->second==id)
        continue;
      MWAW_DEBUG_MSG(("MacDraft5ParserInternal::Layout::updateRelations: %lx is not a child of %lx\n", (unsigned long) shape.m_childList[c], (unsigned long) id));
      shape.m_childList[c]=0;
    }
  }
  // check for loop
  std::set<size_t> seens;
  std::stack<size_t> toCheck;
  for (size_t i=0; i<m_rootList.size(); ++i) toCheck.push(m_rootList[i]);

  while (true) {
    size_t posToCheck;
    if (!toCheck.empty()) {
      posToCheck=toCheck.top();
      toCheck.pop();
    }
    else if (seens.size()==numShapes)
      break;
    else {
      bool ok=false;
      for (size_t i=0; i<m_shapeList.size(); ++i) {
        if (!m_shapeList[i] || seens.find(i)!=seens.end())
          break;
        MWAW_DEBUG_MSG(("MacDraft5ParserInternal::Layout::updateRelations: find unexpected root %d\n", (int) i));
        posToCheck=i;
        m_rootList.push_back(i);
        m_shapeList[i]->m_parentId=0;
        ok=true;
        break;
      }
      if (!ok)
        break;
    }
    seens.insert(posToCheck);
    if (!m_shapeList[posToCheck]) {
      MWAW_DEBUG_MSG(("MacDraft5ParserInternal::Layout::updateRelations: oops, can not find shape %d\n", (int) posToCheck));
      continue;
    }
    Shape &shape=*m_shapeList[posToCheck];
    for (size_t c=0; c<shape.m_childList.size(); ++c) {
      if (shape.m_childList[c]==0) continue;
      if (m_idToShapePosMap.find(shape.m_childList[c])==m_idToShapePosMap.end() ||
          seens.find(m_idToShapePosMap.find(shape.m_childList[c])->second)!=seens.end()) {
        MWAW_DEBUG_MSG(("MacDraft5ParserInternal::Layout::updateRelations: find loop for child %lx\n", (unsigned long) shape.m_childList[c]));
        shape.m_childList[c]=0;
        continue;
      }
      toCheck.push(m_idToShapePosMap.find(shape.m_childList[c])->second);
    }
  }
}

//!  Internal and low level: a image of a library used in MacDraf5Parser
struct Image {
  //! constructor
  Image() : m_id(0), m_name(""), m_extra("")
  {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Image const &im)
  {
    o << im.m_name.cstr() << ",";
    o << std::hex << im.m_id << std::dec << ",";
    o << im.m_extra;
    return o;
  }

  //! the first shape id ( in general a group )
  long m_id;
  //! the image name
  librevenge::RVNGString m_name;
  //! extra data
  std::string m_extra;
};

////////////////////////////////////////
//! Internal: the state of a MacDraft5Parser
struct State {
  //! constructor
  State() : m_version(0), m_isLibrary(false), m_origin(0,0), m_layoutList(), m_imageList()
  {
  }
  //! the file version
  int m_version;
  //! flag to know if we read a library
  bool m_isLibrary;
  //! the origin of the picture
  MWAWVec2f m_origin;
  //! the layer list
  std::vector<shared_ptr<Layout> > m_layoutList;
  //! the image list
  std::vector<shared_ptr<Image> > m_imageList;
};

////////////////////////////////////////
//! Internal: the subdocument of a MacDraft5Parser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(MacDraft5Parser &pars, MWAWInputStreamPtr input, int layoutId, long zoneId) : MWAWSubDocument(&pars, input, MWAWEntry()), m_layoutId(layoutId), m_id(zoneId) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const
  {
    if (MWAWSubDocument::operator!=(doc)) return true;
    SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
    if (!sDoc) return true;
    if (m_id != sDoc->m_id) return true;
    if (m_layoutId != sDoc->m_layoutId) return true;
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
  //! the layout id
  int m_layoutId;
  //! the subdocument id
  long m_id;
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
  parser->sendText(m_layoutId, m_id);
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

      if (!m_state->m_imageList.empty()) {
        MWAWListenerPtr listener=getGraphicListener();
        for (size_t i=0; i<m_state->m_imageList.size(); ++i) {
          if (i && listener)
            listener->insertBreak(MWAWListener::PageBreak);
          if (m_state->m_imageList[i])
            send(*m_state->m_imageList[i]);
        }
      }
      else {
        for (size_t i=0; i<m_state->m_layoutList.size(); ++i) {
          if (m_state->m_layoutList[i])
            send(*m_state->m_layoutList[i]);
        }
      }
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
  std::vector<MWAWPageSpan> pageList;
  if (m_state->m_imageList.empty())
    pageList.push_back(ps);
  else {
    for (size_t i=0; i<m_state->m_imageList.size(); ++i) {
      MWAWPageSpan page(ps);
      if (m_state->m_imageList[i])
        page.setPageName(m_state->m_imageList[i]->m_name);
      pageList.push_back(page);
    }
  }
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
      shared_ptr<MacDraft5ParserInternal::Layout> layout(new MacDraft5ParserInternal::Layout((int) m_state->m_layoutList.size()));
      layout->m_entry.setBegin(pos);
      if (dataEnd>0)
        layout->m_entry.setEnd(dataEnd);
      else
        layout->m_entry.setEnd(input->size());
      if (!readLayout(*layout))
        break;
      m_state->m_layoutList.push_back(layout);
      lastPosSeen=pos=input->tell();
      if (input->isEnd())
        break;
      // check if there is another layout
      int newN=(int)input->readULong(4);
      if (newN<=0 || pos+newN*10>layout->m_entry.end() ||
          (!m_state->m_isLibrary && pos+130>=layout->m_entry.end()))
        break;
      input->seek(-4, librevenge::RVNG_SEEK_CUR);
    }
  }
  else {
    for (size_t i=0; i<m_state->m_layoutList.size(); ++i) {
      if (!m_state->m_layoutList[i] || !m_state->m_layoutList[i]->m_entry.valid())
        continue;
      if (m_state->m_layoutList[i]->m_entry.end()>lastPosSeen)
        lastPosSeen=m_state->m_layoutList[i]->m_entry.end();
      readLayout(*m_state->m_layoutList[i]);
    }
  }
  input->seek(lastPosSeen, librevenge::RVNG_SEEK_SET);
  if (!m_state->m_isLibrary)
    readDocFooter();
  else
    readLibraryFooter();


  if (input->isEnd()) {
    if (dataEnd>0)
      input->popLimit();
    return true;
  }

  MWAW_DEBUG_MSG(("MacDraft5Parser::createZones: find some extra zone\n"));
  ascii().addPos(input->tell());
  ascii().addNote("Entries(Extra):###");
  if (dataEnd>0)
    input->popLimit();

  if (!m_state->m_imageList.empty())
    return true;
  // ok, check that we have read some shape
  for (size_t i=0; i<m_state->m_layoutList.size(); ++i) {
    if (m_state->m_layoutList[i] && !m_state->m_layoutList[i]->isEmpty())
      return true;
  }
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
  // checkme
  m_state->m_origin=MWAWVec2f(float(dim[1]), float(dim[0]));
  f << "windows[dim]=" << MWAWBox2i(MWAWVec2i(dim[1],dim[0]),MWAWVec2i(dim[3],dim[2])) << ",";
  int val;
  for (int i=0; i<3; ++i) { // f0=0|-1|2e60, f1=0|b, f2=16|3b|86, f3=1|2, f4=1|2
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  int nPages[2];
  for (int i=0; i<2; ++i) nPages[i]=(int) input->readLong(2);
  if (nPages[0]>0 && nPages[0]<20 && nPages[1]>0 && nPages[1]<20) {
    if (nPages[1]!=1)
      getPageSpan().setFormWidth(nPages[1]*getPageSpan().getFormWidth());
    if (nPages[0]!=1)
      getPageSpan().setFormLength(nPages[0]*getPageSpan().getFormLength());
  }
  else {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readDocHeader: the number of pages seems bad\n"));
    f << "###";
  }
  if (MWAWVec2i(nPages[1],nPages[0])!=MWAWVec2i(1,1))
    f << "numPages=" << MWAWVec2i(nPages[1],nPages[0]) << ",";
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
    shared_ptr<MacDraft5ParserInternal::Image> image(new MacDraft5ParserInternal::Image);
    image->m_id=(long) input->readULong(4);
    int sSz=(int) input->readULong(1);
    if (sSz>25) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readLibFooter:stringSize seems bad\n"));
      f << "##sSz=" << sSz << ",";
      sSz=0;
    }
    for (int c=0; c<sSz; ++c) {
      char ch=(char) input->readULong(1);
      if (ch==0) break;
      int unicode= getParserState()->m_fontConverter->unicode(3, (unsigned char) ch);
      if (unicode==-1)
        image->m_name.append(ch);
      else
        libmwaw::appendUnicode((uint32_t) unicode, image->m_name);
    }
    input->seek(pos+30, librevenge::RVNG_SEEK_SET);
    val=(int) input->readLong(2); // constant in a zone: 6|d
    if (val) f << "f0=" << val << ",";
    val=(int) input->readLong(2); // 0|1
    if (val) f << "f1=" << val << ",";
    image->m_extra=f.str();
    f.str("");
    f << "LibFooter-" << i << ":" << *image;
    m_state->m_imageList.push_back(image);
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
    MacDraft5ParserInternal::Shape falseShape;
    if (readModifier(falseShape)) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find unexpected modifier\n"));
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
  shared_ptr<MacDraft5ParserInternal::Shape> shape(new MacDraft5ParserInternal::Shape);
  MWAWGraphicStyle &style=shape->m_style;
  shape->m_id=(long) input->readULong(4);
  if (shape->m_id<=1) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: id seems bad\n"));
    f << "###";
  }
  f << "id=" << std::hex << shape->m_id << std::dec << ",";
  int val=(int) input->readULong(1); // 0|80
  if (val) f << "type[h]=" << std::hex << val << std::dec << ",";
  shape->m_fileType=(int) input->readULong(1); //[0-6][0-b]
  switch (shape->m_fileType) {
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
    f << "type=" << std::hex << shape->m_fileType << std::dec << ",";
  }
  shape->m_parentId=(long) input->readULong(4);
  if (shape->m_parentId)
    f << "id[parent]=" << std::hex << shape->m_parentId << std::dec << ",";
  int rotation = -(int) input->readLong(2);
  style.m_rotate=(float) rotation;
  if (rotation) f << "rotate=" << rotation << ",";
  val=(int) input->readLong(2); // 0|2|13b3|2222
  if (val) f << "f0=" << val << ",";
  shape->m_modifierId=(long) input->readULong(4);
  if (shape->m_modifierId)
    f << "modifier[id]=" << std::hex << shape->m_modifierId << std::dec << ",";
  val=(int) input->readULong(1);
  if (val) // alway 0?
    f << "f1=" << val << ",";
  val=(int) input->readULong(1);
  if (val!=1) {
    f << "width[line]=" << val << ",";
    style.m_lineWidth=float(val);
  }
  val=(int) input->readULong(1);
  if (val!=2) // alway 2?
    f << "f2=" << val << ",";
  int lineType=(int) input->readULong(1);
  // FIXME: storeme...
  int dashId=(int) input->readULong(1);
  if (dashId&0x80) {
    switch ((dashId>>4)&0x7) {
    case 0:
      f << "hairline,";
      style.m_lineWidth=0.02f;
      break;
    case 1:
      f << "width[line]=" << 0.5 << ",";
      style.m_lineWidth=0.5f;
      break;
    case 2:
      f << "width[line]=" << 0.75 << ",";
      style.m_lineWidth=0.75f;
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
  int arrowId=(int) input->readULong(1); // low|end

  int colId=(int) input->readULong(2); // 0-78
  f << m_styleManager->updateLineStyle(lineType, colId, (dashId&0xf), style);

  float pt[4];
  for (int i=0; i<2; ++i) pt[i]=float(input->readLong(4))/65536.f;
  shape->m_origin=MWAWVec2f(pt[1],pt[0]);
  f << "pos=" << shape->m_origin << ",";

  val=(int) input->readULong(2); // [02][0134][04][01]
  if (val&1) f << "selected,";
  if (val&8) f << "lock,";
  val &= 0xFFF6;
  if (val) f << "fl1=" << std::hex << val << std::dec << ",";
  shape->m_nameId=(long) input->readULong(4);
  if (shape->m_nameId)
    f << "id[name]=" << std::hex << shape->m_nameId << std::dec << ",";
  switch (shape->m_fileType) {
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
    f << m_styleManager->updateArrows(arrowId&0xf, (arrowId>>4)&0xf, style);
    shape->m_type=MacDraft5ParserInternal::Shape::Basic;
    shape->m_shape=MWAWGraphicShape::line(listPts[0], listPts[1]);
    shape->m_isLine=true;
    rotation=0;
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
    switch (shape->m_fileType) {
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
    shape->m_type=MacDraft5ParserInternal::Shape::Basic;
    val=(int) input->readULong(1);
    if (val) // alway 2?
      f << "f3=" << val << ",";
    int surfType=(int) input->readULong(1);
    colId=(int) input->readULong(2); // 0-78
    f << m_styleManager->updateSurfaceStyle(surfType, colId, style);
    if (shape->m_fileType==3 || shape->m_fileType==4) {
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
    else if (shape->m_fileType==0x14) {
      val=(int) input->readULong(1); // 0-2
      if (val) f << "orig[pos]=" << val << ",";
      val=(int) input->readULong(1); // always 0
      if (val) f << "g0=" << val << ",";
      val=(int) input->readULong(2);
      if (val!=0x181) f << "g1=" << std::hex << val << std::dec << ",";
      for (int i=0; i<3; ++i) pt[i]=float(input->readLong(4))/65536.f;
      MWAWVec2f center(pt[1],pt[0]), radius(pt[2],pt[2]);
      f << "center=" << center << ",radius=" << pt[2] << ",";
      shape->m_shape=MWAWGraphicShape::circle(MWAWBox2f(center-radius, center+radius));
      shape->m_origin=center;
    }
    else if (shape->m_fileType==0x15||shape->m_fileType==0x16||shape->m_fileType==0x17) {
      val=(int) input->readULong(1); // 0-2
      if (val) f << "orig[pos]=" << val << ",";
      val=(int) input->readULong(1); // always 0
      if (val) f << "g0=" << val << ",";
      val=(int) input->readULong(2);
      if (val!=0x181) f << "g1=" << std::hex << val << std::dec << ",";
      int numData=shape->m_fileType==0x16 ? 4 : 3;
      for (int i=0; i<numData; ++i) pt[i]=float(input->readLong(4))/65536.f;
      MWAWVec2f center(pt[1],pt[0]), radius(pt[2],numData==4 ? pt[3] : pt[2]);
      f << "center=" << center << ",radius=" << radius << ",";
      float fileAngle[2];
      for (int i=0; i<2; ++i) fileAngle[i]=float(input->readLong(4))/65536.f;
      f << "angle=" << fileAngle[0] << "x" << fileAngle[1] << ",";
      float angle[2] = { 90-fileAngle[0], 90-fileAngle[1] };
      if (angle[1]>360) {
        int numLoop=int(angle[1]/360)-1;
        angle[0]-=float(numLoop*360);
        angle[1]-=float(numLoop*360);
        while (angle[1] > 360) {
          angle[0]-=360;
          angle[1]-=360;
        }
      }
      if (angle[0] < -360) {
        int numLoop=int(angle[0]/360)+1;
        angle[0]-=float(numLoop*360);
        angle[1]-=float(numLoop*360);
        while (angle[0] < -360) {
          angle[0]+=360;
          angle[1]+=360;
        }
      }
      MWAWBox2f box=MWAWBox2f(center-radius, center+radius);
      // we must compute the real bd box
      float minVal[2] = { 0, 0 }, maxVal[2] = { 0, 0 };
      int limitAngle[2];
      for (int i = 0; i < 2; i++)
        limitAngle[i] = (angle[i] < 0) ? int(angle[i]/90)-1 : int(angle[i]/90);
      for (int bord = limitAngle[0]; bord <= limitAngle[1]+1; bord++) {
        float ang = (bord == limitAngle[0]) ? float(angle[0]) :
                    (bord == limitAngle[1]+1) ? float(angle[1]) : float(90 * bord);
        ang *= float(M_PI/180.);
        float actVal[2] = { radius[0] *std::cos(ang), -radius[1] *std::sin(ang)};
        if (actVal[0] < minVal[0]) minVal[0] = actVal[0];
        else if (actVal[0] > maxVal[0]) maxVal[0] = actVal[0];
        if (actVal[1] < minVal[1]) minVal[1] = actVal[1];
        else if (actVal[1] > maxVal[1]) maxVal[1] = actVal[1];
      }
      MWAWBox2f realBox(MWAWVec2f(center[0]+minVal[0],center[1]+minVal[1]),
                        MWAWVec2f(center[0]+maxVal[0],center[1]+maxVal[1]));
      rotation=0;
      // we must compute the real bd box
      if (!style.hasSurface())
        shape->m_shape=MWAWGraphicShape::arc(realBox, box, MWAWVec2f(angle[0],angle[1]));
      else
        shape->m_shape=MWAWGraphicShape::pie(realBox, box, MWAWVec2f(angle[0],angle[1]));
    }
    else if (shape->m_fileType==0x18) {
      val=(int) input->readULong(1); // 0-1
      if (val) f << "orig[pos]=" << val << ",";
      val=(int) input->readULong(1); // always 0
      if (val) f << "g1=" << val << ",";
      val=(int) input->readULong(2); // always 0
      if (val) f << "g2=" << val << ",";
    }
    else if (shape->m_fileType==0x1e || shape->m_fileType==0x1f) {
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
      shape->m_shape.m_type=MWAWGraphicShape::Path;
      shape->m_shape.m_path=path;
      rotation=0;
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
    shape->m_type=MacDraft5ParserInternal::Shape::Basic;
    MWAWVec2f corner(0,0);
    if (shape->m_fileType==0xb) {
      val=(int) input->readULong(1); // 1-3
      if (val!=1) f << "orig[pos]=" << val << ",";
      val=(int) input->readULong(1); // always 0
      if (val) f << "h4=" << val << ",";
      val=(int) input->readULong(2); // always 4
      if (val!=4) f << "h5=" << val << ",";
      for (int i=0; i<2; ++i) pt[i]=float(input->readLong(4))/65536.f;
      corner=MWAWVec2f(pt[1]/2.f,pt[0]/2.f);
      f << "corner=" << corner << ",";
    }
    switch (shape->m_fileType) {
    case 0x9:
    case 0xa:
      if (!rotation) {
        shape->m_shape=MWAWGraphicShape::rectangle(MWAWBox2f(listPts[0], listPts[2]), corner);
        break;
      }
    // fall through intended
    case 0x3:
    case 0x4: {
      rotation=0;
      shape->m_shape.m_type = MWAWGraphicShape::Polygon;
      std::vector<MWAWVec2f> &vertices=shape->m_shape.m_vertices;
      MWAWBox2f box;
      for (size_t i=0; i<nbPts; ++i) {
        vertices.push_back(listPts[i]);
        if (i==0)
          box=MWAWBox2f(listPts[i], listPts[i]);
        else
          box=box.getUnion(MWAWBox2f(listPts[i], listPts[i]));
      }
      shape->m_shape.m_bdBox=shape->m_shape.m_formBox=box;
      break;
    }
    case 0xb:
      shape->m_shape=MWAWGraphicShape::rectangle(MWAWBox2f(listPts[0], listPts[2]), corner);
      if (rotation) shape->m_origin=0.5f*(listPts[0]+listPts[2]);
      break;
    case 0x14: // already done
    case 0x15:
    case 0x16:
    case 0x17:
      break;
    case 0x18:
      listPts[1]=MWAWVec2f(listPts[1][1],listPts[1][0]);
      shape->m_shape=MWAWGraphicShape::circle(MWAWBox2f(listPts[0]-listPts[1], listPts[0]+listPts[1]));
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
    val=(int) input->readULong(1);
    if (val) // alway 0?
      f << "f3=" << val << ",";
    int surfType=(int) input->readULong(1);
    colId=(int) input->readULong(2); // 0-78
    f << m_styleManager->updateSurfaceStyle(surfType, colId, style);

    for (int i=0; i<2; ++i) { // g0=0|1
      val=(int) input->readULong(1);
      if (val) f << "g" << i << "=" << std::hex << val << std::dec << ",";
    }
    f << "ids?=["; // two big number, unsure
    for (int i=0; i<2; ++i) f << std::hex << input->readULong(4) << std::dec << ",";
    f << "],";
    val=(int) input->readLong(2);// 0|-1
    if (val) f << "g2=" << val << ",";
    val=(int) input->readLong(1);
    switch (val) {
    case -3:
      break;
    case -2:
      shape->m_paragraph.setInterline(1.5, librevenge::RVNG_PERCENT);
      f << "interline=150%,";
      break;
    case -1:
      shape->m_paragraph.setInterline(2., librevenge::RVNG_PERCENT);
      f << "interline=200%,";
      break;
    default:
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find unknown interline\n"));
      f << "##interline=" << val << ",";
      break;
    }
    for (int i=0; i<2; ++i) {
      val=(int) input->readULong(1); // always 0
      if (val) f << "g" << i+3 << "=" << std::hex << val << std::dec << ",";
    }
    val=(int) input->readLong(1);
    switch (val) {
    case 0: // left
      break;
    case 1:
      shape->m_paragraph.m_justify = MWAWParagraph::JustificationCenter;
      f << "align=center,";
      break;
    case -1:
      shape->m_paragraph.m_justify = MWAWParagraph::JustificationRight;
      f << "align=right,";
      break;
    default:
      MWAW_DEBUG_MSG(("MacDrawParser::readText: find unknown align\n"));
      f << "#align=" << val << ",";
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
    shape->m_type=MacDraft5ParserInternal::Shape::Text;
    if (rotation) {
      MWAWBox2f bdbox;
      MWAWVec2f center=0.5*(listPts[0]+listPts[2]);
      for (int i=0; i<4; ++i) {
        MWAWVec2f point=libmwaw::rotatePointAroundCenter(listPts[i], center, float(-rotation));
        if (i==0)
          bdbox=MWAWBox2f(point,point);
        else
          bdbox=bdbox.getUnion(MWAWBox2f(point,point));
      }
      shape->m_box=bdbox;
      rotation=0;
    }
    else
      shape->m_box=MWAWBox2f(listPts[0],listPts[2]);
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
    shape->m_bitmapDimensionList.push_back(MWAWBox2i(MWAWVec2i(dim[1],dim[0]),MWAWVec2i(dim[3],dim[2])));
    f << "dim=" << shape->m_bitmapDimensionList.back() << ",";
    shape->m_bitmapIdList.push_back((int) input->readULong(2));
    f << "id[bitmap]=" << shape->m_bitmapIdList.back() << ",";
    shape->m_type=MacDraft5ParserInternal::Shape::Bitmap;
    if (rotation) {
      MWAWBox2f bdbox;
      MWAWVec2f center=0.5*(listPts[0]+listPts[2]);
      for (int i=0; i<4; ++i) {
        MWAWVec2f point=libmwaw::rotatePointAroundCenter(listPts[i], center, float(-rotation));
        if (i==0)
          bdbox=MWAWBox2f(point,point);
        else
          bdbox=bdbox.getUnion(MWAWBox2f(point,point));
      }
      shape->m_box=bdbox;
    }
    else
      shape->m_box=MWAWBox2f(listPts[0],listPts[2]);
    break;
  }
  case 0x33:
  case 0x35: { // group
    int headerSize=shape->m_fileType==0x33 ? 0x2a : 60;
    if (fSz<headerSize) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find unknown group size\n"));
      f << "##fSz,";
      break;
    }
    if (shape->m_fileType==0x35) {
      val=(int) input->readULong(1);
      if (val) // alway 0?
        f << "f3=" << val << ",";
      int surfType=(int) input->readULong(1);
      colId=(int) input->readULong(2); // 0-78
      f << m_styleManager->updateSurfaceStyle(surfType, colId, style);
      static bool first=true;
      if (first && style.hasSurface()) {
        MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: creating surface from group is not implemented\n"));
        first=false;
        f << "#surf[group],";
      }
      for (int i=0; i<7; ++i) { // g4=0|100, g5=N
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
    shape->m_type=MacDraft5ParserInternal::Shape::Group;
    f << "child[id]=[";
    for (int i=0; i<nChild; ++i) {
      val=(int) input->readULong(4);
      shape->m_childList.push_back(val);
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
    long expectedSize=(shape->m_fileType==0x57) ? 0x106 : (shape->m_fileType==0x58) ? 0x17a : 0x176;
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
    int numPts=shape->m_fileType==0x57 ? 1 : shape->m_fileType==0x58 ? 11 : 13;
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
    if (shape->m_fileType==0x57) {
      for (int j=0; j<2; ++j) pt[j]=float(input->readLong(4))/65536.f;
      f << "axis,dim=" << MWAWVec2f(pt[1],pt[0]) << ",";
    }
    else if (shape->m_fileType==0x58) {
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
    long pId=(long) input->readULong(4);
    f << "parent[id]=" << std::hex << pId << std::dec << ",";
    if (shape->m_parentId==0) shape->m_parentId=pId;
    shape->m_type=MacDraft5ParserInternal::Shape::Group;
    shape->m_childList.push_back((int)input->readULong(4));
    f << "child[id]=" << std::hex << shape->m_childList.back() << std::dec << ",";
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
    long pId=(long) input->readULong(4);
    f << "parent[id]=" << std::hex << pId << std::dec << ",";
    if (shape->m_parentId==0) shape->m_parentId=pId;
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
    colId=(int) input->readULong(2); // 0-78
    f << m_styleManager->updateSurfaceStyle(surfType, colId, style);

    for (int i=0; i<2; ++i) pt[i]=float(input->readLong(4))/65536.f;
    f << "dim?=" << MWAWVec2f(pt[1],pt[0]) << ",";
    f << "points=[";
    std::vector<MWAWVec2f> listPts;
    for (size_t i=0; i<2; ++i) {
      for (int j=0; j<3; ++j) { // checkme: original line, line1, line2?
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
    shape->m_type=MacDraft5ParserInternal::Shape::Basic;
    if (style.hasSurface()) {
      shape->m_shape.m_type = MWAWGraphicShape::Polygon;
      std::vector<MWAWVec2f> &vertices=shape->m_shape.m_vertices;
      MWAWBox2f box;
      for (int i=0; i<4; ++i) {
        size_t const which[]= {1,4,5,2};
        MWAWVec2f point=listPts[which[i]];
        vertices.push_back(point);
        if (i==0)
          box=MWAWBox2f(point, point);
        else
          box=box.getUnion(MWAWBox2f(point, point));
      }
      shape->m_shape.m_bdBox=shape->m_shape.m_formBox=box;
    }
    else if (style.hasLine()) {
      shape->m_shape=MWAWGraphicShape::line(listPts[1], listPts[4]);

      shape->m_otherStyleList.push_back(style);
      shape->m_otherShapeList.push_back(MWAWGraphicShape::line(listPts[2], listPts[5]));
    }
    else
      shape->m_shape=MWAWGraphicShape::line(listPts[1], listPts[4]);
    rotation=0;
    break;
  }
  case 0x65: { // poly para
    if (fSz<58) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: find unknown poly[para] size\n"));
      f << "##fSz,";
      break;
    }
    MWAWGraphicStyle surfStyle;
    for (int i=0; i<2; ++i) {
      f << "style[" << (i==1 ? "inter" : "surf") << "]=[";
      val=(int) input->readULong(1);
      if (val) // alway 0?
        f << "g0=" << val << ",";
      int surfType=(int) input->readULong(1);
      colId=(int) input->readULong(2); // 0-78
      f << m_styleManager->updateSurfaceStyle(surfType, colId, i==1 ? style : surfStyle);
      f << "],";
    }

    for (int i=0; i<3; ++i) { // g0=9,g1=0,g2=2|3|e
      val=(int) input->readLong(2);
      if (val)
        f << "g" << i << "=" << val << ",";
    }
    bool isClosed=false;
    val=(int) input->readLong(1);
    if (val==1) {
      f << "closed,";
      isClosed=true;
    }
    else if (val) f << "poly[type]=" << val << ",";
    val=(int) input->readLong(1); // always 0
    if (val) f << "g7=" << val << ",";
    int nbPts=(int) input->readULong(2);
    if (fSz<58+30*nbPts) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: numbers of points of unknown poly[para] seems bad\n"));
      f << "##nbPts=" << nbPts << ",";
      break;
    }
    std::vector<MWAWVec2f> listPts[3];
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
        listPts[p].push_back(MWAWVec2f(pPos[1],pPos[0]));
        f << listPts[p].back();
        if (!f2.str().empty()) f << "[" << f2.str() << "]";
        if (j+1==nbPts) f << ",";
        else f << ":";
      }
      f << "],";
    }
    bool mainSet=false;
    shape->m_type=MacDraft5ParserInternal::Shape::Basic;
    MWAWGraphicStyle mainStyle=style;
    if (surfStyle.hasSurface()) {
      shape->m_shape.m_type = MWAWGraphicShape::Polygon;
      std::vector<MWAWVec2f> &vertices=shape->m_shape.m_vertices;
      MWAWBox2f box;
      for (size_t p=0; p<listPts[0].size(); ++p) {
        MWAWVec2f point=listPts[0][p];
        vertices.push_back(point);
        if (p==0)
          box=MWAWBox2f(point, point);
        else
          box=box.getUnion(MWAWBox2f(point, point));
      }
      shape->m_shape.m_bdBox=shape->m_shape.m_formBox=box;
      surfStyle.m_lineWidth=0;
      shape->m_style=surfStyle;
      mainSet=true;
    }

    MWAWGraphicShape newShape;
    newShape.m_type = MWAWGraphicShape::Polygon;
    std::vector<MWAWVec2f> &vertices=newShape.m_vertices;
    MWAWBox2f box;
    for (size_t p=0; p<listPts[1].size(); ++p) {
      MWAWVec2f point=listPts[1][p];
      vertices.push_back(point);
      if (p==0)
        box=MWAWBox2f(point, point);
      else
        box=box.getUnion(MWAWBox2f(point, point));
    }
    if (isClosed && !listPts[1].empty())
      vertices.push_back(listPts[1][0]);
    if (isClosed && !listPts[2].empty())
      vertices.push_back(listPts[2][0]);
    for (size_t p=listPts[2].size(); p>0;) {
      MWAWVec2f point=listPts[2][--p];
      vertices.push_back(point);
      box=box.getUnion(MWAWBox2f(point, point));
    }
    newShape.m_bdBox=newShape.m_formBox=box;
    if (mainSet) {
      shape->m_otherStyleList.push_back(mainStyle);
      shape->m_otherShapeList.push_back(newShape);
    }
    else {
      shape->m_style=mainStyle;
      shape->m_shape=newShape;
    }
    rotation=0;
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
  if (shape->m_modifierId>0 && !readModifier(*shape)) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: can not find the modifier\n"));
    input->seek(pos,librevenge::RVNG_SEEK_SET);
  }

  pos=input->tell();
  if (shape->m_nameId && !readStringList()) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readObject: can not find name list\n"));
    input->seek(pos,librevenge::RVNG_SEEK_SET);
  }
  if (rotation)
    shape->transform(float(rotation), false, shape->m_origin);
  layout.m_shapeList.push_back(shape);
  if (shape->m_fileType!=0x28)
    return true;
  pos=input->tell();
  if (!readText(*shape))
    input->seek(pos, librevenge::RVNG_SEEK_SET);
  return true;
}

bool MacDraft5Parser::readText(MacDraft5ParserInternal::Shape &shape)
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  libmwaw::DebugStream f;
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
    shape.m_textEntry.setBegin(input->tell());
    shape.m_textEntry.setLength(tSz);
    input->seek(shape.m_textEntry.end(), librevenge::RVNG_SEEK_SET);
    nData=(int) input->readULong(2);
  }
  long endPos=pos+24+tSz+nData*20;
  if (!ok || nData>100 || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::readText: can not read text\n"));
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
    long cPos=(long) input->readULong(4);
    f << "pos=" << cPos << ",";
    int val=(int) input->readLong(2); // c-2f
    if (val) f << "f0=" << val << ",";
    f << "height=" << input->readULong(2) << ",";
    MWAWFont font;
    font.setId((int) input->readULong(2));
    int flag = (int) input->readULong(1);
    uint32_t flags=0;
    if (flag&0x1) flags |= MWAWFont::boldBit;
    if (flag&0x2) flags |= MWAWFont::italicBit;
    if (flag&0x4) font.setUnderlineStyle(MWAWFont::Line::Simple);
    if (flag&0x88) f << "#fl=" << std::hex << flag << ",";
    font.setFlags(flags);
    val=(int) input->readULong(1); // always 0
    if (val) f << "f1=" << val << ",";
    font.setSize(float(input->readULong(2)));
    uint8_t col[3];
    for (int j=0; j<3; ++j) col[j]=(uint8_t)(input->readULong(2)>>8);
    font.setColor(MWAWColor(col[0],col[1],col[2]));
    f << "font=[" << font.getDebugString(getParserState()->m_fontConverter) << "],";
    if (shape.m_posToFontMap.find(cPos)==shape.m_posToFontMap.end())
      shape.m_posToFontMap[cPos]=font;
    else {
      MWAW_DEBUG_MSG(("MacDraft5Parser::readText: pos %ld already exists\n", cPos));
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(pos+20, librevenge::RVNG_SEEK_SET);
  }
  return true;
}

////////////////////////////////////////////////////////////
// read an modifier
////////////////////////////////////////////////////////////
bool MacDraft5Parser::readModifier(MacDraft5ParserInternal::Shape &shape)
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
  f << "Entries(Modifier):";
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
    int type=(int) input->readULong(2);
    long value=(long) input->readULong(4);
    f << "mod" << i << "=[";
    switch (type) {
    case 0:
      if (value==0)
        f << "nop";
      else
        f << "###nop=" << value << ",";
      break;
    case 102:
      f << "quot[child]=" << std::hex << value << std::dec << ",";
      shape.m_childList.push_back(int(value));
      break;
    case 501:
      if (value>=0 && value<=255)
        shape.m_style.m_surfaceOpacity=float(value)/255.f;
      else
        f << "###";
      f << "opacity[surf]=" << float(value)/255.f << ",";
      break;
    case 502: // checkme
      if (value>=0 && value<=255)
        shape.m_style.m_lineOpacity=float(value)/255.f;
      else
        f << "###";
      f << "opacity[line]=" << float(value)/255.f << ",";
      break;
    default:
      MWAW_DEBUG_MSG(("MacDraft5Parser::readModifier: find unknown modifier type %d\n", type));
      f << "###type=" << type << ",val=" << std::hex << value << std::dec << ",";
    }
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
  getPageSpan().setFormLength(paperSize.y()/72.);
  getPageSpan().setFormWidth(paperSize.x()/72.);

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
    if (checkN && layout.m_N==layout.m_objectId) {
      layout.updateRelations();
      return true;
    }
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
  layout.updateRelations();
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
    shared_ptr<MacDraft5ParserInternal::Layout> layout(new MacDraft5ParserInternal::Layout((int) m_state->m_layoutList.size()));
    for (int c=0; c<16; ++c) {
      char ch=(char) input->readULong(1);
      if (ch==0) break;
      int unicode= getParserState()->m_fontConverter->unicode(3, (unsigned char) ch);
      if (unicode==-1)
        layout->m_name.append(ch);
      else
        libmwaw::appendUnicode((uint32_t) unicode, layout->m_name);
    }
    input->seek(pos+16, librevenge::RVNG_SEEK_SET);
    for (int j=0; j<3; ++j) { // f0=3f|ae|be|bf, f1=4-10, f2=-1..4
      val=(int) input->readLong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }
    layout->m_N=(int) input->readULong(4);
    layout->m_entry.setBegin((long) input->readULong(4));
    layout->m_entry.setLength((long) input->readULong(4));
    layout->m_extra=f.str();

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
bool MacDraft5Parser::send(MacDraft5ParserInternal::Image const &image)
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::send[image]: can not find the listener\n"));
    return false;
  }
  if (!image.m_id) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::send[image]: can not find the image id\n"));
    return false;
  }
  bool ok=false;
  for (size_t i=0; i<m_state->m_layoutList.size(); ++i) {
    if (!m_state->m_layoutList[i]) continue;
    shared_ptr<MacDraft5ParserInternal::Shape> shape=m_state->m_layoutList[i]->findShape(image.m_id, false);
    if (!shape) continue;
    ok=true;
    bool openLayer=false;
    if (!m_state->m_layoutList[i]->m_name.empty())
      openLayer=listener->openLayer(m_state->m_layoutList[i]->m_name);
    send(*shape, *m_state->m_layoutList[i]);
    if (openLayer)
      listener->closeLayer();
  }
  if (!ok) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::send[image]: can not find any layout for the image id=%lx\n", (unsigned long) image.m_id));
    return false;
  }
  return true;
}

bool MacDraft5Parser::send(MacDraft5ParserInternal::Layout const &layout)
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::send[layout]: can not find the listener\n"));
    return false;
  }
  bool openLayer=false;
  if (!layout.m_name.empty())
    openLayer=listener->openLayer(layout.m_name);
  for (size_t i=0; i<layout.m_rootList.size(); ++i) {
    shared_ptr<MacDraft5ParserInternal::Shape> child=layout.m_shapeList[layout.m_rootList[i]];
    if (!child || child->m_isSent)
      continue;
    send(*child, layout);
  }
  if (openLayer)
    listener->closeLayer();
  return true;
}

bool MacDraft5Parser::send(MacDraft5ParserInternal::Shape const &shape,
                           MacDraft5ParserInternal::Layout const &layout)
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::send[shape]: can not find the listener\n"));
    return false;
  }
  shape.m_isSent=true;
  MWAWBox2f box=shape.getBdBox();
  MWAWPosition pos(box[0]+m_state->m_origin, box.size(), librevenge::RVNG_POINT);
  pos.m_anchorTo = MWAWPosition::Page;

  if (!shape.m_childList.empty())
    listener->openGroup(pos);
  MWAWGraphicStyle style=shape.m_style;
  switch (shape.m_type) {
  case MacDraft5ParserInternal::Shape::Basic:
    listener->insertPicture(pos, shape.m_shape, style);
    for (size_t i=0; i<shape.m_otherShapeList.size(); ++i) {
      if (i>=shape.m_otherStyleList.size()) {
        MWAW_DEBUG_MSG(("MacDraft5Parser::send[shape]: can not find some style\n"));
        break;
      }
      box=shape.m_otherShapeList[i].getBdBox();
      pos=MWAWPosition(box[0]+m_state->m_origin, box.size(), librevenge::RVNG_POINT);
      pos.m_anchorTo = MWAWPosition::Page;
      listener->insertPicture(pos, shape.m_otherShapeList[i], shape.m_otherStyleList[i]);
    }
    break;
  case MacDraft5ParserInternal::Shape::Text: {
    shared_ptr<MWAWSubDocument> doc(new MacDraft5ParserInternal::SubDocument(*this, getInput(), layout.m_id, shape.m_id));
    listener->insertTextBox(pos, doc, style);
    break;
  }
  case MacDraft5ParserInternal::Shape::Bitmap:
    sendBitmap(shape, pos);
    break;
  case MacDraft5ParserInternal::Shape::Group:
  case MacDraft5ParserInternal::Shape::Unknown:
  default:
    break;
  }

  for (size_t i=0; i<shape.m_childList.size(); ++i) {
    shared_ptr<MacDraft5ParserInternal::Shape> child=layout.findShape(shape.m_childList[i]);
    if (!child) continue;
    if (child->m_isSent) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::send[shape]: the child is already sent\n"));
      continue;
    }
    send(*child, layout);
  }
  if (!shape.m_childList.empty())
    listener->closeGroup();

  return true;
}

bool MacDraft5Parser::sendBitmap(MacDraft5ParserInternal::Shape const &bitmap, MWAWPosition const &position)
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::sendBitmap: can not find the listener\n"));
    return false;
  }
  bitmap.m_isSent=true;

  if (bitmap.m_bitmapIdList.empty() || bitmap.m_bitmapIdList.size()!=1) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::sendBitmap: the bitmap seems bad\n"));
    return false;
  }

  librevenge::RVNGBinaryData binary;
  std::string type;
  if (!m_styleManager->getBitmap(bitmap.m_bitmapIdList[0], binary,type)) return false;
  MWAWGraphicStyle style=bitmap.m_style;
  style.m_lineWidth=0;
  listener->insertPicture(position, binary, type, style);

  return true;
}

bool MacDraft5Parser::sendText(int layoutId, long zId)
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::sendText: can not find the listener\n"));
    return false;
  }
  if (layoutId<0 || layoutId>=(int) m_state->m_layoutList.size() || !m_state->m_layoutList[size_t(layoutId)]) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::sendText: can not find the layout\n"));
    return false;
  }
  MacDraft5ParserInternal::Layout const &layout=*m_state->m_layoutList[size_t(layoutId)];
  shared_ptr<MacDraft5ParserInternal::Shape> shape=layout.findShape(zId);
  if (!shape)
    return false;
  shape->m_isSent = true;

  listener->setParagraph(shape->m_paragraph);
  listener->setFont(MWAWFont(3,12));
  if (shape->m_type != MacDraft5ParserInternal::Shape::Text) {
    MWAW_DEBUG_MSG(("MacDraft5Parser::sendText: unexpected shape type\n"));
    return false;
  }
  if (!shape->m_textEntry.valid())
    return true;
  MWAWInputStreamPtr input=getInput();
  input->seek(shape->m_textEntry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Text-data:";
  long endPos=shape->m_textEntry.end();
  long cPos=0;
  while (!input->isEnd()) {
    if (input->tell()>=shape->m_textEntry.end())
      break;
    if (shape->m_posToFontMap.find(cPos)!=shape->m_posToFontMap.end()) {
      listener->setFont(shape->m_posToFontMap.find(cPos)->second);
      f << "[F]";
    }
    ++cPos;
    char c = (char) input->readULong(1);
    if (c==0) {
      MWAW_DEBUG_MSG(("MacDraft5Parser::sendText: find char 0\n"));
      f << "#[0]";
      continue;
    }
    f << c;
    switch (c) {
    case 9:
      listener->insertTab();
      break;
    case 0xd:
      listener->insertEOL();
      break;
    default:
      listener->insertCharacter((unsigned char)c, input, endPos);
      break;
    }
  }
  ascii().addPos(shape->m_textEntry.begin());
  ascii().addNote(f.str().c_str());
  return true;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
