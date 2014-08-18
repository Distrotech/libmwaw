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

#include "MacDrawProStyleManager.hxx"

#include "MacDrawProParser.hxx"

/** Internal: the structures of a MacDrawProParser */
namespace MacDrawProParserInternal
{
// generic class used to defined a layer
struct Layer {
  //! constructor
  Layer() : m_numShapes(0), m_name("")
  {
    for (int i=0; i<3; ++i) m_N[i]=0;
  }
  //! the number of shape
  int m_numShapes;
  //! some unknown number find in the beginning of the header
  long m_N[3];
  //! the layer name
  std::string m_name;
};

// generic class used to defined a library
struct Library {
  //! constructor
  Library() : m_layerList(), m_name("")
  {
  }
  //! the list of layer id
  std::vector<int> m_layerList;
  //! the library name
  std::string m_name;
};

// generic class used to store shape in MWAWDrawParser
struct Shape {
  //! the different shape
  enum Type { Basic, Bitmap, Group, GroupEnd, Note, Text, Unknown };

  //! constructor
  Shape() : m_type(Unknown), m_fileType(0), m_box(), m_style(), m_shape(), m_id(-1), m_nextId(-1), m_flags(0),
    m_font(), m_paragraph(), m_textEntry(), m_childList(),
    m_numBytesByRow(0), m_bitmapDim(), m_bitmapFileDim(), m_bitmapEntry(), m_isSent(false)
  {
  }

  //! basic operator<<
  friend std::ostream &operator<<(std::ostream &o, Shape const &shape);

  //! return the shape bdbox
  Box2f getBdBox() const
  {
    return m_type==Basic ? m_shape.getBdBox() : m_box;
  }
  //! returns true if the object is a line
  bool isLine() const
  {
    return m_type==Basic && m_shape.m_type==MWAWGraphicShape::Line;
  }
  //! the graphic type
  Type m_type;
  //! the file type
  int m_fileType;
  //! the shape bdbox
  Box2f m_box;
  //! the graphic style
  MWAWGraphicStyle m_style;
  //! the graphic shape ( for basic geometric form )
  MWAWGraphicShape m_shape;
  //! the shape id
  int m_id;
  //! the following id (if set)
  int m_nextId;
  //! the main shape flag
  int m_flags;
  //! the font ( for a text box)
  MWAWFont m_font;
  //! the paragraph ( for a text box)
  MWAWParagraph m_paragraph;
  //! the textbox entry (main text)
  MWAWEntry m_textEntry;
  //! the child list ( for a group )
  std::vector<size_t> m_childList;
  //! the number of bytes by row (for a bitmap)
  int m_numBytesByRow;
  //! the bitmap dimension (in page)
  Box2i m_bitmapDim;
  //! the bitmap dimension (in the file)
  Box2i m_bitmapFileDim;
  //! the bitmap entry (data)
  MWAWEntry m_bitmapEntry;
  //! a flag used to know if the object is sent to the listener or not
  mutable bool m_isSent;
};


std::ostream &operator<<(std::ostream &o, Shape const &shape)
{
  o << "O" << shape.m_id << "[";
  switch (shape.m_type) {
  case Shape::Basic:
    switch (shape.m_shape.m_type) {
    case MWAWGraphicShape::Line:
      o << "line,";
      break;
    case MWAWGraphicShape::Rectangle:
      o << "rect,";
      break;
    case MWAWGraphicShape::Circle:
      o << "circle,";
      break;
    case MWAWGraphicShape::Arc:
      o << "arc,";
      break;
    case MWAWGraphicShape::Pie:
      o << "pie,";
      break;
    case MWAWGraphicShape::Polygon:
      o << "polygons,";
      break;
    case MWAWGraphicShape::Path:
      o << "path,";
      break;
    case MWAWGraphicShape::ShapeUnknown:
    default:
      o << "###unknwown[shape],";
      break;
    }
    break;
  case Shape::Bitmap:
    o << "bitmap,";
    break;
  case Shape::Group:
    o << "group,";
    break;
  case Shape::GroupEnd:
    o << "group[end],";
    break;
  case Shape::Note:
    o << "note,";
    break;
  case Shape::Text:
    o << "text,";
    break;
  case Shape::Unknown:
  default:
    o << "unknown,";
    break;
  }
  o << shape.m_box << ",";
  if (shape.m_flags & 0x80) o << "rotation,";

  if (shape.m_flags & 0x3f)
    o << "fl=" << std::hex << (shape.m_flags&0x3f) << std::dec << ",";
  o << "],";
  return o;
}

////////////////////////////////////////
//! Internal: the state of a MacDrawProParser
struct State {
  //! constructor
  State() : m_version(0), m_documentSize(), m_numLayers(1), m_numLibraries(0), m_numShapes(0),
    m_libraryList(), m_layerList(), m_objectDataList(), m_objectTextList(), m_shapeList()
  {
    for (int i=0; i<5; ++i) m_sizeStyleZones[i]=0;
    for (int i=0; i<2; ++i) m_sizeLayerZones[i]=0;
    for (int i=0; i<2; ++i) m_sizeLibraryZones[i]=0;
    for (int i=0; i<4; ++i) m_sizeFZones[i]=0;
  }
  //! the file version
  int m_version;
  //! the document size (in point)
  Vec2f m_documentSize;
  //! the number of layer
  int m_numLayers;
  //! the number of library
  int m_numLibraries;
  //! the total number of shapes
  int m_numShapes;
  //! the size of the header zones
  long m_sizeStyleZones[5];
  //! the size of the layer zones
  long m_sizeLayerZones[2];
  //! the size of the library zones(checkme)
  long m_sizeLibraryZones[2];
  //! the size of the zoneF
  long m_sizeFZones[4];
  //! the library list
  std::vector<Library> m_libraryList;
  //! the layer list
  std::vector<Layer> m_layerList;
  //! the list of entries which stores the object's data
  std::vector<MWAWEntry> m_objectDataList;
  //! the list of entries which stores the object's text
  std::vector<MWAWEntry> m_objectTextList;
  //! the shape list
  std::vector<Shape> m_shapeList;
};

////////////////////////////////////////
//! Internal: the subdocument of a MacDrawProParser
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(MacDrawProParser &pars, MWAWInputStreamPtr input, int zoneId) : MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId) {}

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
    MWAW_DEBUG_MSG(("MacDrawProParserInternal::SubDocument::parse: no listener\n"));
    return;
  }
  MacDrawProParser *parser=dynamic_cast<MacDrawProParser *>(m_parser);
  if (!m_parser) {
    MWAW_DEBUG_MSG(("MacDrawProParserInternal::SubDocument::parse: no parser\n"));
    return;
  }
  long pos = m_input->tell();
  parser->sendText(m_id);
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
}


}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
MacDrawProParser::MacDrawProParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header) :
  MWAWGraphicParser(input, rsrcParser, header), m_state(), m_styleManager()
{
  init();
}

MacDrawProParser::~MacDrawProParser()
{
}

void MacDrawProParser::init()
{
  resetGraphicListener();
  setAsciiName("main-1");

  m_state.reset(new MacDrawProParserInternal::State);
  m_styleManager.reset(new MacDrawProStyleManager(*this));

  getPageSpan().setMargins(0.1);
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void MacDrawProParser::parse(librevenge::RVNGDrawingInterface *docInterface)
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
      for (size_t i=0; i<m_state->m_shapeList.size(); ++i) {
        MacDrawProParserInternal::Shape const &shape=m_state->m_shapeList[i];
        if (shape.m_isSent) continue;
        send(shape);
        if (shape.m_nextId>0 && shape.m_nextId>int(i))
          i=size_t(shape.m_nextId-1);
      }
    }
    ascii().reset();
  }
  catch (...) {
    MWAW_DEBUG_MSG(("MacDrawProParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  resetGraphicListener();
  if (!ok) throw(libmwaw::ParseException());
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void MacDrawProParser::createDocument(librevenge::RVNGDrawingInterface *documentInterface)
{
  if (!documentInterface) return;
  if (getGraphicListener()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::createDocument: listener already exist\n"));
    return;
  }

  // create the page list
  MWAWPageSpan ps(getPageSpan());
  ps.setPageSpan(1);
  /* FIXME: update the page size using m_state->m_documentSize or by
     finding the right/bottom shape */
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
bool MacDrawProParser::createZones()
{
  MWAWInputStreamPtr input = getInput();
  readHeaderInfo();

  if (getRSRCParser()) m_styleManager->readRSRCZones();

  input->seek(0x1f4,librevenge::RVNG_SEEK_SET);
  if (!m_styleManager->readStyles(m_state->m_sizeStyleZones) ||
      !readLayersInfo() || !readLayerLibraryCorrespondance() || !readLibrariesInfo() ||
      !findObjectPositions(true) || !findObjectPositions(false)) {
    MWAW_DEBUG_MSG(("MacDrawProParser::createZones: something is bad, stop.\n"));
    return false;
  }
  long pos;
  libmwaw::DebugStream f;
  if (m_state->m_sizeFZones[3]) {
    /* checkme: I am not sure if this zone contains data or if it is
       only an intermediary zone used to make possible to grow the
       previous zones */
    pos=input->tell();
    long endPos=pos+m_state->m_sizeFZones[3];
    f.str("");
    f << "Entries(Free0):";
    if (m_state->m_sizeFZones[3]<0 || !input->checkPosition(endPos)) {
      MWAW_DEBUG_MSG(("MacDrawProParser::createZones: can not read Free0 size\n"));
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return false;
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
  }
  pos=input->tell();
  if (m_state->m_numShapes<0 || !input->checkPosition(pos+32*m_state->m_numShapes)) {
    MWAW_DEBUG_MSG(("MacDrawProParser::createZones: can not read the object information\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(Object):###");
    return false;
  }
  for (int i=0; i<m_state->m_numShapes; ++i) {
    if (readObject()<0) break;
  }

  // probably a free zone (used to reserve space to make the file grow)
  if (!input->isEnd()) {
    pos=input->tell();
    ascii().addPos(pos);
    ascii().addNote("Entries(Free1):");
  }

  for (size_t i=0; i<m_state->m_objectDataList.size(); ++i) {
    MWAWEntry const &entry=m_state->m_objectDataList[i];
    if (!entry.valid() || entry.isParsed())
      continue;
    MWAW_DEBUG_MSG(("MacDrawProParser::createZones: find some unparsed data's object zones\n"));
    f.str("");
    f << "Entries(ObjData)[" << i << "]:###unparsed";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
  }
  for (size_t i=0; i<m_state->m_objectTextList.size(); ++i) {
    MWAWEntry const &entry=m_state->m_objectTextList[i];
    if (!entry.valid() || entry.isParsed())
      continue;
    MWAW_DEBUG_MSG(("MacDrawProParser::createZones: find some unparsed text's object zones\n"));
    f.str("");
    f << "Entries(ObjText)[" << i << "]:###unparsed";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
  }

  MWAW_DEBUG_MSG(("MacDrawProParser::createZones: oops is not implemented\n"));
  return false;
}

////////////////////////////////////////////////////////////
// layer/library functions
////////////////////////////////////////////////////////////
bool MacDrawProParser::readLayersInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  long begNamePos=pos+m_state->m_sizeLayerZones[0];
  long endPos=pos+m_state->m_sizeLayerZones[0]+m_state->m_sizeLayerZones[1];
  if (input->isEnd() || m_state->m_numLayers*0x80>m_state->m_sizeLayerZones[0] || !input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readLayersInfo: problem with the layer dimensions\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(Layer):###");
    return false;
  }

  libmwaw::DebugStream f;
  int numShapes=0;
  std::map<int,long> idToNamePosMap;
  m_state->m_layerList.clear();
  for (int i=0; i<m_state->m_numLayers; ++i) {
    MacDrawProParserInternal::Layer layer;
    pos=input->tell();
    f.str("");
    f << "Entries(Layer)[L" << i+1 << "]:";
    long val=(long) input->readULong(2);
    if (val&0x8000) f << "hidden,";
    val &=0x7FFF;
    if (val) f << "fl=" << std::hex << val << std::dec << ",";
    f << "id?=" << input->readULong(2) << ","; // a small number
    long delta=(long) input->readULong(4);
    if (delta) f << "name[pos]=" << std::hex << delta << std::dec << ",";
    if (delta < 0 || delta >=m_state->m_sizeLayerZones[1]) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readLayersInfo: problem with the layer position dim\n"));
      f << "###";
      delta=-1;
    }
    idToNamePosMap[i]=delta;
    layer.m_numShapes=(int) input->readULong(4);
    numShapes+=layer.m_numShapes;
    if (layer.m_numShapes) f << "N[shapes]=" << layer.m_numShapes << ",";
    m_state->m_layerList.push_back(layer);
    val=(long) input->readLong(4); // always 0?
    if (val) f << "f0=" << val << ",";
    f << "N[unkn]=[";
    for (int j=0; j<3; ++j) { // small numbers always less than numShapes. Note: f2 can be equal to -1
      val=layer.m_N[j]=(long) input->readLong(4);
      if (!val) {
        f << "_,";
        continue;
      }
      if (val>layer.m_numShapes) {
        MWAW_DEBUG_MSG(("MacDrawProParser::readLayersInfo: find an old number in header\n"));
        f << "#";
      }
      f << "f" << j << "=";
      if (val==-1)
        f << "*,";
      else
        f << val << "/" << layer.m_numShapes << ",";
    }
    f << "],";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());

    for (int j=0; j<2; ++j) {
      /* very often Layer-A0=Layer-A1.
         When they differ, Layer-A1 seems to contain more data than Layer-A0
       */
      pos=input->tell();
      f.str("");
      f << "Layer-A" << j << "[L" << i+1 << "]:";
      f << "N[unkn]=[";
      for (int k=0; k<3; ++k) { // small numbers always less than numShapes, but they can be greater than m_N[k]
        val=(long) input->readLong(4);
        if (!val) {
          f << "_,";
          continue;
        }
        if (val > layer.m_numShapes) f << "#";
        f << val << "/" << layer.m_numShapes << ",";
      }
      f << "],";
      for (int k=0; k<2; ++k) {
        float dim[4];
        for (int l=0; l<4; ++l) dim[l]=float(input->readLong(4))/65536.f;
        if (dim[0]>0||dim[1]>0||dim[2]>0||dim[3]>0)
          f << "dim" << k << "=" << Box2f(Vec2f(dim[0],dim[1]),Vec2f(dim[2],dim[3])) << ",";
      }
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
    }

    pos=input->tell();
    f.str("");
    f << "Layer-B[L" << i+1 << "]:";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());

    input->seek(pos+12, librevenge::RVNG_SEEK_SET);
  }
  m_state->m_numShapes=numShapes;

  for (std::map<int,long>::const_iterator it=idToNamePosMap.begin(); it!=idToNamePosMap.end(); ++it) {
    if (it->second < 0) continue;
    pos=begNamePos+it->second;
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    f.str("");
    f << "Layer[L" << it->first+1 << ",name]:";
    int fSz=(int)input->readULong(1);
    if (input->tell()+fSz>endPos) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readLayersInfo: oops the layer name size seems bad\n"));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      continue;
    }
    std::string name("");
    for (int c=0; c<fSz; ++c) name+=(char) input->readULong(1);
    f << name << ",";
    m_state->m_layerList[size_t(it->first)].m_name=name;
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

bool MacDrawProParser::readLayerLibraryCorrespondance()
{
  if (!m_state->m_sizeFZones[0]) return true;
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();

  MWAWEntry entry;
  entry.setBegin(pos);
  entry.setLength(m_state->m_sizeFZones[0]);
  entry.setName("LayToLib");

  if (entry.length()<0 || !input->checkPosition(entry.end())) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readLayerLibraryCorrespondance: the zone size seems bad\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(LayToLib):###");
    return false;
  }
  std::map<int, long> idToDecal;
  if (!readStructuredHeaderZone(entry, idToDecal)) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readLayerLibraryCorrespondance: can not read the header\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(LayToLib):###");
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }

  pos=input->tell();
  libmwaw::DebugStream f;
  f << "LayToLib[data]:";
  long sz=(long) input->readULong(4);
  if (sz<4 || pos+sz>entry.end()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readLayerLibraryCorrespondance: can not read the data size\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (std::map<int, long>::const_iterator it=idToDecal.begin(); it!=idToDecal.end(); ++it) {
    int id=it->first;
    long decal=it->second;
    if (decal<0 || decal+8>sz) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readLayerLibraryCorrespondance: the zone %d's decal seems bad\n", id));
      continue;
    }
    f.str("");
    f << "LayToLib-L" << id+1 << ":";
    long begPos=pos+decal;
    input->seek(begPos, librevenge::RVNG_SEEK_SET);
    int val=(int) input->readLong(2); // always 0?
    if (val) f << "f0=" << val << ",";
    val=(int) input->readLong(2); // 1|3
    if (val!=1) f << "type=" << val << ",";
    long dataSz=(long) input->readULong(4);
    if (dataSz<8 || begPos+dataSz>entry.end() || (dataSz%8)!=0) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readLayerLibraryCorrespondance: the zone %d data size seems bad\n", id));
      f << "###";
      ascii().addPos(begPos);
      ascii().addNote(f.str().c_str());
      continue;
    }
    int N=int((dataSz-8)/8);
    f << "libs=[";
    for (int i=0; i<N; ++i) {
      val=(int) input->readLong(4); // find 1 or 2
      int val1=(int) input->readLong(2);
      int library=(int) input->readLong(2);
      if (library<1 || library>m_state->m_numLibraries) {
        MWAW_DEBUG_MSG(("MacDrawProParser::readLayerLibraryCorrespondance: a library number seems bad\n"));
        f << "###";
      }
      else {
        if (library>int(m_state->m_libraryList.size()))
          m_state->m_libraryList.resize(size_t(library));
        m_state->m_libraryList[size_t(library-1)].m_layerList.push_back(id+1);
      }
      f << "Li" << library;
      if (val!=1) f << ":" << val;
      else if (val1) f << ":_";
      if (val1) f << ":" << val1;
      f << ",";
    }
    f << "],";
    ascii().addPos(begPos+dataSz);
    ascii().addNote("_");
    ascii().addPos(begPos);
    ascii().addNote(f.str().c_str());
  }
  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  return true;
}

bool MacDrawProParser::readLibrariesInfo()
{
  if (!m_state->m_sizeLibraryZones[0] && !m_state->m_sizeLibraryZones[1])
    return true;

  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  long begNamePos=pos+m_state->m_sizeLibraryZones[0];
  long endPos=pos+m_state->m_sizeLibraryZones[0]+m_state->m_sizeLibraryZones[1];
  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readLibrariesInfo: problem with the library dimensions\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(Library):###");
    return false;
  }
  if ((m_state->m_sizeLibraryZones[0]%8) || !m_state->m_sizeLibraryZones[0]||!m_state->m_sizeLibraryZones[1]) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readLibrariesInfo: problem with the size zone(II)\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(Library):###");
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    return true;
  }

  libmwaw::DebugStream f;
  f << "Entries(Library):";
  std::vector<long> posList;
  int N=int(m_state->m_sizeLibraryZones[0]/8);
  for (int i=0; i<N; ++i) {
    long cPos=(long) input->readULong(4);
    if (cPos<0 || cPos>m_state->m_sizeLibraryZones[1]) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readLayersInfo: oops the library name positions seems bad\n"));
      f << "###";
      posList.push_back(-1);
    }
    else
      posList.push_back(cPos);
    f << std::hex << cPos << std::dec;
    int val=(int) input->readLong(4);
    if (val) f << ":" << val << ",";
    else f << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (size_t i=0; i<posList.size(); ++i) {
    if (posList[i]<0) continue;

    pos=begNamePos+posList[i];
    f.str("");
    f << "Library-name[" << i << "]:";
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    int fSz=(int)input->readULong(1);
    if (input->tell()+fSz>endPos) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readLayersInfo: oops the library name size seems bad\n"));
      f << "###";
      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      continue;
    }
    std::string name("");
    for (int c=0; c<fSz; ++c) name+=(char) input->readULong(1);
    f << name << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// generic function
////////////////////////////////////////////////////////////
bool MacDrawProParser::readStructuredHeaderZone(MWAWEntry const &entry, std::map<int, long> &idToDeltaPosMap)
{
  idToDeltaPosMap.clear();
  if (!entry.length())
    return true;

  MWAWInputStreamPtr input = getInput();
  long pos=entry.begin();
  libmwaw::DebugStream f;
  f << "Entries(" << entry.name() << "):";

  if (entry.length()<4+4 || !input->checkPosition(entry.end())) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readStructuredHeaderZone: can not read %s size\n", entry.name().c_str()));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }

  input->seek(pos,librevenge::RVNG_SEEK_SET);
  long sz=(long) input->readULong(4);
  long endPos=pos+sz;
  if (sz<8 || endPos>entry.end()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readStructuredHeaderZone: can not read %s-ptr size\n", entry.name().c_str()));
    f << "ptr###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }
  long fFree=(int) input->readULong(4);
  f << "fFree=" << fFree << ",";
  if (fFree!=sz)
    ascii().addDelimiter(pos+fFree,'|');
  int numDatas=int((sz-8)/4);
  f << "ptrs=[";
  for (int i=0; i<numDatas; ++i) {
    long ptr=(long) input->readULong(4);
    if (!ptr) continue;
    idToDeltaPosMap[i]=ptr;
    f << std::hex << ptr << std::dec << ":" << i << ",";
  }
  f << "],";
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  input->seek(endPos,  librevenge::RVNG_SEEK_SET);

  return true;
}

////////////////////////////////////////////////////////////
// object functions
////////////////////////////////////////////////////////////
bool MacDrawProParser::findObjectPositions(bool dataZone)
{
  if (!m_state->m_sizeFZones[dataZone ? 1 : 2]) return true;
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();

  MWAWEntry entry;
  entry.setBegin(pos);
  entry.setLength(m_state->m_sizeFZones[dataZone ? 1 : 2]);
  entry.setName(dataZone ? "ObjData" : "ObjText");

  libmwaw::DebugStream f;
  f << entry.name() << "[data]:";
  if (entry.length()<0 || !input->checkPosition(entry.end())) {
    MWAW_DEBUG_MSG(("MacDrawProParser::findObjectPositions: the zone size seems bad\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return false;
  }
  std::map<int, long> idToDecal;
  if (!readStructuredHeaderZone(entry, idToDecal)) {
    MWAW_DEBUG_MSG(("MacDrawProParser::findObjectPositions: can not read the header\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }

  pos=input->tell();
  long sz=(long) input->readULong(4);
  if (sz<4 || pos+sz>entry.end()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::findObjectPositions: can not read the data size\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    return true;
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  std::vector<MWAWEntry> &positionList=dataZone ? m_state->m_objectDataList : m_state->m_objectTextList;
  positionList.clear();
  for (std::map<int, long>::const_iterator it=idToDecal.begin(); it!=idToDecal.end(); ++it) {
    int id=it->first;
    long decal=it->second;
    if (decal<4 || decal+8>sz) {
      MWAW_DEBUG_MSG(("MacDrawProParser::findObjectPositions: the zone %d's decal seems bad\n", id));
      continue;
    }

    f.str("");
    f << entry.name() << "[" << id << "]:";
    long begPos=pos+decal;
    input->seek(begPos+4, librevenge::RVNG_SEEK_SET);
    long dataSz=(long) input->readULong(4);
    if (dataSz<8 || begPos+dataSz>entry.end()) {
      MWAW_DEBUG_MSG(("MacDrawProParser::findObjectPositions: the zone %d data size seems bad\n", id));
      f << "###";
      ascii().addPos(begPos);
      ascii().addNote(f.str().c_str());
      continue;
    }
    MWAWEntry dEntry;
    dEntry.setBegin(begPos);
    dEntry.setLength(dataSz);
    dEntry.setId(id);
    if (id>=int(positionList.size()))
      positionList.resize(size_t(id+1));
    if (id>=0 && id < int(positionList.size()))
      positionList[size_t(id)]=dEntry;
    else {
      MWAW_DEBUG_MSG(("MacDrawProParser::findObjectPositions: can not store entry %d\n", id));
      f << "###";
      ascii().addPos(begPos);
      ascii().addNote(f.str().c_str());
    }
    ascii().addPos(dEntry.end());
    ascii().addNote("_");
  }
  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  return true;
}

int MacDrawProParser::readObject()
{
  MWAWInputStreamPtr input = getInput();
  if (input->isEnd()) return -1;
  if (m_state->m_numShapes <= int(m_state->m_shapeList.size()))
    return false;

  long pos=input->tell();
  libmwaw::DebugStream f;
  size_t shapeId= m_state->m_shapeList.size();
  f << "Entries(Object)[O" << shapeId << "]:";

  if (!input->checkPosition(pos+32)) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readObject: the zone seems to small\n"));
    f << "###";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return -1;
  }
  m_state->m_shapeList.push_back(MacDrawProParserInternal::Shape());
  MacDrawProParserInternal::Shape &shape=m_state->m_shapeList.back();
  shape.m_id=int(shapeId);
  shape.m_nextId=shape.m_id+1; // default value
  float dim[4];
  for (int i=0; i<4; ++i) dim[i]=float(input->readLong(4))/65536.f;
  shape.m_box=Box2f(Vec2f(dim[1],dim[0]),Vec2f(dim[3],dim[2]));
  f << shape.m_box << ",";
  int val=(int) input->readULong(1);
  shape.m_flags=val;
  if (val & 0x8) f << "select,";
  if (val & 0x20) f << "locked,";
  bool hasData=(val & 0x40);
  if (val & 0x80) f << "rotation,";
  val &=0x17;
  if (val) f << "fl0=" << std::hex << val << std::dec << ",";
  val=(int) input->readULong(1);
  // checkme
  if (val&0x20) f << "flipX,";
  if (val&0x40) f << "flipY,";
  if (val&0x90) f << "fl1=" << std::hex << ((val>>4)&9) << std::dec << ",";
  shape.m_fileType = (val & 0xF);
  switch (shape.m_fileType) {
  case 1:
    shape.m_type=MacDrawProParserInternal::Shape::Text;
    f << "text,";
    break;
  case 2:
    shape.m_type=MacDrawProParserInternal::Shape::Basic;
    shape.m_shape.m_type=MWAWGraphicShape::Line;
    f << "line,";
    break;
  case 3:
    shape.m_type=MacDrawProParserInternal::Shape::Basic;
    shape.m_shape.m_type=MWAWGraphicShape::Rectangle;
    f << "rect,";
    break;
  case 4:
    shape.m_type=MacDrawProParserInternal::Shape::Basic;
    shape.m_shape.m_type=MWAWGraphicShape::Rectangle;
    f << "rectOval,";
    break;
  case 5:
    shape.m_type=MacDrawProParserInternal::Shape::Basic;
    shape.m_shape.m_type=MWAWGraphicShape::Circle;
    f << "circle,";
    break;
  case 6:
    shape.m_type=MacDrawProParserInternal::Shape::Basic;
    shape.m_shape.m_type=MWAWGraphicShape::Arc;
    f << "arc,";
    break;
  case 7:
    shape.m_type=MacDrawProParserInternal::Shape::Basic;
    shape.m_shape.m_type=MWAWGraphicShape::Path;
    f << "spline,";
    break;
  case 8:
    shape.m_type=MacDrawProParserInternal::Shape::Basic;
    shape.m_shape.m_type=MWAWGraphicShape::Polygon;
    f << "poly,";
    break;
  case 9:
    shape.m_type=MacDrawProParserInternal::Shape::Bitmap;
    f << "bitmap,";
    break;
  case 0xa:
    if (hasData) {
      shape.m_type=MacDrawProParserInternal::Shape::Note;
      f << "note,";
    }
    else {
      shape.m_type=MacDrawProParserInternal::Shape::Group;
      f << "group,";
    }
    break;
  case 0xb:
    shape.m_type=MacDrawProParserInternal::Shape::GroupEnd;
    f << "group[end],";
    break;
  default:
    MWAW_DEBUG_MSG(("MacDrawProParser::readObject: find unknown type %d\n", val));
    f << "###type=" << std::hex << val << std::dec << ",";
  }
  val=(int) input->readLong(1); // 0|1
  if (val==0) f << "f0[no],";
  else if (val!=1) f << "f0=" << val << ",";

  // read the linewidth
  val=(int) input->readULong(1);
  if (val) {
    float penSize=1;
    if (!m_styleManager->getPenSize(val, penSize))
      f << "###P" << val << ",";
    else {
      shape.m_style.m_lineWidth=penSize;
      if (penSize<1 || penSize>1)
        f << "lineWidth=" <<  penSize << ",";
    }
  }

  // reads the pattern
  for (int i=0; i<2; ++i) {
    val=(int) input->readULong(2);
    if (val==0) { // transparent
      if (i==0) {
        shape.m_style.m_lineWidth=0;
        f << "line[no],";
      }
      continue;
    }
    char const *(wh[])= {"line", "surf"};
    if (val&0x4000) {
      val &= 0x3FFF;
      MWAWColor color;
      if (!m_styleManager->getColor(val, color)) {
        MWAW_DEBUG_MSG(("MacDrawProParser::readObject: find unknown basic color pattern: %d\n", val));
        f << wh[i] << "[color]=###" << val << ",";
      }
      else {
        if (i==0) shape.m_style.m_lineColor=color;
        else shape.m_style.setSurfaceColor(color);
        if ((i==0&&!color.isBlack()) || (i==1 && !color.isWhite()))
          f << wh[i] << "[color]=" << color << ",";
      }
      continue;
    }
    MWAWGraphicStyle::Pattern pattern;
    if (!m_styleManager->getPattern(val, pattern)) {
      f << wh[i] << "[pat]=###" << std::hex << val << std::dec << ",";
      continue;
    }
    MWAWColor color;
    if (pattern.getUniqueColor(color)) {
      if (i==0) shape.m_style.m_lineColor=color;
      else shape.m_style.setSurfaceColor(color);
      if ((i==0&&!color.isBlack()) || (i==1 && !color.isWhite()))
        f << wh[i] << "[color]=" << color << ",";
      continue;
    }
    f << wh[i] <<  "[pat]=" << pattern << ",";
    if (i==0 && pattern.getAverageColor(color))
      shape.m_style.m_lineColor=color;
    else if (i==1)
      shape.m_style.m_pattern=pattern;
  }

  // read the dash
  val=(int) input->readULong(1);
  if (val) {
    if (!m_styleManager->getDash(val, shape.m_style.m_lineDashWidth))
      f << "###";
    f << "dash=D" << val << ",";
  }

  // unknown
  if (shape.m_fileType==4 && !hasData) {
    val=(int)input->readLong(4);
    if (val)
      f << "round[dim]=" << float(val)/256.f/65536.f << ",";
    else
      f << "round[def],";
  }
  else {
    val=(int) input->readLong(1); // a small number negative or positif
    if (val)
      f << "f1=" << val << ",";
  }
  if (hasData) {
    val=(int) input->readULong(2);
    if (val<8) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readObject: can not find the data position\n"));
      f << "###objData[pos],";
    }
    else {
      long actPos=input->tell();
      if (!readObjectData(shape, (val-8)/4))
        f << "###";
      input->seek(actPos, librevenge::RVNG_SEEK_SET);
      f << "objData[id]=" << (val-8)/4 << ",";
    }
    if (val&3)
      f << "objData[low]=" << (val&3) << ",";
  }
  else if (shape.m_type==MacDrawProParserInternal::Shape::Group) {
    val=(int) input->readULong(2);
    int N=(val/0x20)-2;
    size_t lastShapeId=shapeId+size_t(N);
    if (val<0x40 || int(lastShapeId)>m_state->m_numShapes) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readObject: can not find the group number\n"));
      f << "###number[object]=" << N << ",";
    }
    else {
      f << "N=" << N << ",";
      if (val&0x1F)
        f << "N[low]="<< (val&0x1F) << ",";
      input->seek(pos+32, librevenge::RVNG_SEEK_SET);

      bool ok=true;
      for (int i=0; i<N; ++i) {
        if (m_state->m_shapeList.size()>lastShapeId)
          break;
        int cId=readObject();
        if (cId<0) {
          MWAW_DEBUG_MSG(("MacDrawParser::readObject: can not find a child\n"));
          f << "###";
          ok=false;
          break;
        }
        // do not use shape's childList as the vector can have grown
        m_state->m_shapeList[shapeId].m_childList.push_back(size_t(cId));
      }

      int nextId=int(m_state->m_shapeList.size())+1;
      if (ok) {
        int cId=readObject(); // read end group
        if (cId<0 || m_state->m_shapeList[size_t(cId)].m_type!=MacDrawProParserInternal::Shape::GroupEnd) {
          MWAW_DEBUG_MSG(("MacDrawParser::readObject: oops, can not find the end group data\n"));
          f << "###";
          --nextId;
        }
      }
      m_state->m_shapeList[shapeId].m_nextId=nextId;

      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return int(shapeId);
    }
  }
  ascii().addDelimiter(input->tell(),'|');
  input->seek(pos+32, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  return int(shapeId);
}

bool MacDrawProParser::readObjectData(MacDrawProParserInternal::Shape &shape, int zId)
{
  if (zId<0 || zId>=(int) m_state->m_objectDataList.size() || !m_state->m_objectDataList[size_t(zId)].valid()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readObjectData: can not find the data for zone %d\n", zId));
    return false;
  }
  MWAWEntry const &entry=m_state->m_objectDataList[size_t(zId)];
  entry.setParsed(true);
  ascii().addPos(entry.end());
  ascii().addNote("_");

  MWAWInputStreamPtr input = getInput();
  long savePos=input->tell();
  if (shape.m_type==MacDrawProParserInternal::Shape::Basic) {
    bool res=readBasicObjectData(shape, entry);
    input->seek(savePos, librevenge::RVNG_SEEK_SET);
    return res;
  }
  else if (shape.m_type==MacDrawProParserInternal::Shape::Bitmap) {
    bool res=readBitmap(shape, entry);
    input->seek(savePos, librevenge::RVNG_SEEK_SET);
    return res;
  }
  libmwaw::DebugStream f;
  f << "ObjData[" << shape << "]:";
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  int val=(int) input->readLong(2); // always 0?
  if (val) f << "f0=" << val << ",";
  val=(int) input->readLong(2);
  if (val!=1) f << "numUsed=" << val << ",";
  input->seek(4, librevenge::RVNG_SEEK_CUR); // skip the size field

  val=(int) input->readLong(2); // always 0?
  if (val) f << "f1=" << val << ",";
  val=(int) input->readLong(2);
  int textId=-1;
  if (val) {
    if (val>=8 && (shape.m_type== MacDrawProParserInternal::Shape::Text || shape.m_type== MacDrawProParserInternal::Shape::Note)) {
      textId=(val-8)/4;
      f << "objText[id]=" << (val-8)/4 << ",";
      if (val&3)
        f << "objText[low]=" << (val&3) << ",";
    }
    else {
      MWAW_DEBUG_MSG(("MacDrawProParser::readObjectData: find unexpected text position\n"));
      f << "###objText[pos]=" << val << ",";
    }
  }
  // first read the rotation data
  if (shape.m_flags & 0x80) {
    if (input->tell()+28>entry.end()) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readObjectData: can not find the rotation data\n"));
      f << "###rot,";
      ascii().addPos(entry.begin());
      ascii().addNote(f.str().c_str());
      return false;
    }
    f << "angl[rot]=" << float(input->readLong(4))/65536.f  << ","; // in radians
    f << "rot[value?]=[";
    for (int i=0; i<6; ++i)  // 3 points ?
      f << float(input->readLong(4))/65536.f  << ",";
    f << "],";
  }

  int remain=int(entry.end()-input->tell());
  switch (shape.m_type) {
  case MacDrawProParserInternal::Shape::Note:
  case MacDrawProParserInternal::Shape::Text: {
    bool isNote=shape.m_type==MacDrawProParserInternal::Shape::Note;
    int const headerSize=isNote ? 20+68 : 20;
    if (remain<headerSize) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readObjectData: the text zone seems too short\n"));
      break;
    }
    for (int i=0; i<4; ++i) { // f0=-1|0|1, f1=0|2, f3=0|-1, f4=-1|0|1
      val=(int) input->readLong(1);
      if (val) f << "f" << i << "=" << val << ",";
    }
    int nChar=(int) input->readULong(2);
    if (nChar) f << "nChar=" << nChar << ",";
    int N[2];
    for (int i=0; i<2; ++i) N[i]=(int) input->readULong(2);
    f << "N=[" << N[0] << "," << N[1] << "],";
    if (remain<headerSize+6*N[0]+4*N[1]) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readObjectData: can not read the number of data\n"));
      f << "###,";
      break;
    }
    if (isNote) {
      long debPos=input->tell();
      f << "note=[";
      for (int i=0; i<2; ++i) { // always 0
        val=(int) input->readLong(2);
        if (val) f << "f" << i << "=" << val << ",";
      }
      f << "flgs?=[";
      for (int i=0; i<2; ++i) // two time the same big number ?
        f << std::hex << input->readULong(4) << std::dec << ",";
      f << "],";
      for (int i=0; i<8; ++i) { // always 0
        val=(int) input->readLong(2);
        if (val) f << "f" << i+2 << "=" << val << ",";
      }
      int sSz=(int) input->readULong(1);
      if (sSz>31) {
        MWAW_DEBUG_MSG(("MacDrawProParser::readObjectData: the note size seems to big\n"));
        f << "#sSz=" << sSz << ",";
      }
      else {
        std::string name("");
        for (int i=0; i<sSz; ++i) name+=(char) input->readULong(1);
        f << name << ",";
      }
      input->seek(debPos+60, librevenge::RVNG_SEEK_SET);
      for (int i=0; i<4; ++i) { // always 0
        val=(int) input->readLong(2);
        if (val) f << "g" << i << "=" << val << ",";
      }
      f << "],";
      input->seek(debPos+68, librevenge::RVNG_SEEK_SET);
    }
    f << "lineBreak=[";
    for (int i=0; i<=N[0]; ++i)
      f << input->readULong(2) << ",";
    f << "],";
    f << "font=[";
    for (int i=0; i<=N[1]; ++i)
      f << input->readULong(2) << ":F" << input->readLong(2)+1 << ",";
    f << "],";
    f << "line[h,?]=[";
    for (int i=0; i<=N[0]; ++i)
      f << input->readULong(2) << ":" << input->readULong(2) << ",";
    f << "],";
    if (input->tell()+4<=entry.end())
      f << "#remain,";
    if (input->tell()!=entry.end()) {
      ascii().addDelimiter(input->tell(),'|');
      input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
    }
    break;
  }
  case MacDrawProParserInternal::Shape::Bitmap:
  case MacDrawProParserInternal::Shape::Basic:
  case MacDrawProParserInternal::Shape::Group:
  case MacDrawProParserInternal::Shape::GroupEnd:
  case MacDrawProParserInternal::Shape::Unknown:
    break;
  }
  if (textId>=0)
    readObjectText(shape, textId);

  if (input->tell()!=entry.end()) {
    ascii().addDelimiter(input->tell(),'|');
    MWAW_DEBUG_MSG(("MacDrawProParser::readObjectData: find unexpected data\n"));
    f << "###";
  }
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());

  input->seek(savePos, librevenge::RVNG_SEEK_SET);
  return true;
}

bool MacDrawProParser::readBasicObjectData(MacDrawProParserInternal::Shape &shape, MWAWEntry const &entry)
{
  if (shape.m_type!=MacDrawProParserInternal::Shape::Basic || entry.length()<8) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readBasicObjectData: the entry seems bad\n"));
    return false;
  }
  entry.setParsed(true);

  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  f << "ObjData[" << shape << "]:";
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  int val;
  val=(int) input->readLong(2);
  if (val) f << "f0=" << val << ",";
  val=(int) input->readLong(2);
  if (val!=1) f << "numUsed=" << val << ",";
  input->seek(4, librevenge::RVNG_SEEK_CUR); // skip the size field

  val=(int) input->readLong(2); // find 0|1|2 for line, other 0
  if (val) f << "f1=" << val << ",";
  int fl=(int) input->readLong(2);

  // first read the rotation data
  if (shape.m_flags & 0x80) {
    if (input->tell()+28>entry.end()) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readBasicObjectData: can not find the rotation data\n"));
      f << "###rot,";
      ascii().addPos(entry.begin());
      ascii().addNote(f.str().c_str());
      return false;
    }
    float angle= float(input->readLong(4))/65536.f; // in radians
    shape.m_style.m_rotate = float(360./M_PI*angle);
    f << "angl[rot]=" << shape.m_style.m_rotate << ",";
    f << "rot[value?]=[";
    for (int i=0; i<6; ++i)  // 3 points ?
      f << float(input->readLong(4))/65536.f  << ",";
    f << "],";
  }

  long remain=entry.end()-input->tell();
  switch (shape.m_shape.m_type) {
  case MWAWGraphicShape::Line: {
    if (fl & 0xF8) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readBasicObjectData: find unknown line flag\n"));
      f << "#line[flag]=" << (fl&0xF8) << ",";
    }
    if (input->tell()+20>entry.end()) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readBasicObjectData: can not read the line header\n"));
      f << "###";
      break;
    }
    f << "angl=" << float(input->readLong(4))/65536.f  << ","; // in radians
    f << "dim=[";
    for (int i=0; i<4; ++i)
      f << float(input->readLong(4))/65536.f  << ",";
    f << "],";
    bool ok=true;
    for (int i=0; i<2; ++i) { // the arrow data
      if ((fl&(i+1))==0) continue;
      shape.m_style.m_arrows[i]=true;
      if (input->tell()+24>entry.end()) {
        MWAW_DEBUG_MSG(("MacDrawProParser::readBasicObjectData: can not read the line arrow %d\n", i));
        f << "###";
        ok=false;
        break;
      }
      f << "arrow[" << (i==0 ? "beg" : "end") << "]=[";
      for (int j=0; j<6; ++j) { // arrow points
        f << float(input->readLong(4))/65536.f;
        if (j%2) f << ",";
        else f << "x";
      }
      f << "],";
    }
    if (!ok) break;
    if ((fl&4)==0) {
      fl &= 0xFC;
      break;
    }

    fl &= 0xF8;
    long endLabelPos=input->tell()+44;
    f << "label=[";
    if (endLabelPos>entry.end()) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readBasicObjectData: can not read the line label\n"));
      f << "###";
      break;
    }
    f << "dim=[";
    for (int i=0; i<4; ++i)
      f << float(input->readLong(4))/65536.f  << ",";
    f << "],";
    val=(int) input->readLong(2); // 0 or 4
    if (val) f<< "f0=" << val << ",";
    int sSz=(int) input->readULong(1);
    if (sSz>27) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readBasicObjectData: can not read the line label size\n"));
      f << "###";
      break;
    }

    std::string label("");
    for (int i=0; i<sSz; ++i) label+=(char) input->readULong(1);
    f << label;
    input->seek(endLabelPos, librevenge::RVNG_SEEK_SET);
    break;
  }
  case MWAWGraphicShape::Path:
  case MWAWGraphicShape::Polygon: {
    if (remain<0 || (remain%8)) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readBasicObjectData: can not read compute the number of point in polygon\n"));
      f << "###";
      break;
    }
    int N=int(remain/8);
    f << "N=" << N << ",pts=[";
    for (int i=0; i<N; ++i) {
      float pt[2];
      for (int j=0; j<2; ++j) pt[j]=float(input->readLong(4))/65536.f;
      f << Vec2f(pt[0],pt[1]) << ",";
    }
    f << "],";
    break;
  }
  case MWAWGraphicShape::Rectangle: {
    if (!remain)
      break;
    if (shape.m_fileType!=4 || remain!=128) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readBasicObjectData: can not read the round rect representation\n"));
      f << "###";
      break;
    }
    f << "pts=[";
    for (int i=0; i<16; ++i) {
      float pt[2];
      for (int j=0; j<2; ++j) pt[j]=float(input->readLong(4))/65536.f;
      f << Vec2f(pt[0],pt[1]) << ",";
    }
    f << "],";
    break;
  }
  case MWAWGraphicShape::Circle: {
    if (!remain)
      break;
    if (remain!=64) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readBasicObjectData: can not read the circle representation\n"));
      f << "###";
      break;
    }
    f << "pts=[";
    for (int i=0; i<8; ++i) {
      float pt[2];
      for (int j=0; j<2; ++j) pt[j]=float(input->readLong(4))/65536.f;
      f << Vec2f(pt[0],pt[1]) << ",";
    }
    f << "],";
    break;
  }
  case MWAWGraphicShape::Arc:
  case MWAWGraphicShape::Pie: {
    if (remain < 8 || (remain%8))
      break;
    float angle[2];
    for (int i=0; i<2; ++i) angle[i]=float(input->readLong(4))/65536.f;
    f << "angle=" << angle[0] << "->" << angle[0]+angle[1] << ",";
    int N=int(remain/8)-1;
    f << "N=" << N << ",pts?=["; // it is a spline ?
    for (int i=0; i<N; ++i) {
      float pt[2];
      for (int j=0; j<2; ++j) pt[j]=float(input->readLong(4))/65536.f;
      f << Vec2f(pt[0],pt[1]) << ",";
    }
    f << "],";
    break;
  }
  case MWAWGraphicShape::ShapeUnknown:
  default:
    break;
  }
  if (fl) f << "fl=" << std::hex << fl << std::dec << ",";
  if (input->tell()!=entry.end()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readBasicObjectData: find unexpected data\n"));
    f << "###";
    ascii().addDelimiter(input->tell(),'|');
  }
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  return true;
}

bool MacDrawProParser::readBitmap(MacDrawProParserInternal::Shape &shape, MWAWEntry const &entry)
{
  if (shape.m_type!=MacDrawProParserInternal::Shape::Bitmap || entry.length()<10) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readBitmap: the entry seems bad\n"));
    return false;
  }
  entry.setParsed(true);

  MWAWInputStreamPtr input = getInput();
  libmwaw::DebugStream f;
  f << "Entries(Bitmap)[" << shape << "]:";
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  int val;
  val=(int) input->readLong(2);
  if (val) f << "f0=" << val << ",";
  val=(int) input->readLong(2);
  if (val!=1) f << "numUsed=" << val << ",";
  input->seek(4, librevenge::RVNG_SEEK_CUR); // skip the size field

  for (int i=0; i<2; ++i) {
    val=(int) input->readLong(2);
    if (val) f << "f" << i+1 << "=" << val << ",";
  }

  // first read the rotation data
  if (shape.m_flags & 0x80) {
    if (input->tell()+28>entry.end()) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readBitmap: can not find the rotation data\n"));
      f << "###rot,";
      ascii().addPos(entry.begin());
      ascii().addNote(f.str().c_str());
      return false;
    }
    float angle= float(input->readLong(4))/65536.f; // in radians
    shape.m_style.m_rotate = float(360./M_PI*angle);
    f << "angl[rot]=" << shape.m_style.m_rotate << ",";
    f << "rot[value?]=[";
    for (int i=0; i<6; ++i)  // 3 points ?
      f << float(input->readLong(4))/65536.f  << ",";
    f << "],";
  }
  int dim[4];
  for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
  Box2i &bitmapBox=shape.m_bitmapDim;
  bitmapBox=Box2i(Vec2i(dim[1],dim[0]), Vec2i(dim[3],dim[2]));
  f << "dim=" << bitmapBox << ",";
  f << "id?=" << std::hex << input->readULong(4) << std::dec << ",";
  shape.m_numBytesByRow=(int) input->readULong(2);
  f << "rowSize=" << shape.m_numBytesByRow << ",";
  Box2i &fileBox=shape.m_bitmapFileDim;
  for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
  fileBox=Box2i(Vec2i(dim[1],dim[0]), Vec2i(dim[3],dim[2]));
  if (bitmapBox!=fileBox)
    f << "bitmap[dimInFile]="<< fileBox << ",";
  shape.m_bitmapEntry.setBegin(input->tell());
  shape.m_bitmapEntry.setLength(fileBox.size()[1]*shape.m_numBytesByRow);
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());

  if (fileBox.size()[1]<0 || shape.m_numBytesByRow<0 || !input->checkPosition(shape.m_bitmapEntry.end())) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readObject: can not compute the bitmap endPos\n"));
    ascii().addPos(shape.m_bitmapEntry.begin());
    ascii().addNote("Bitmap[data]:###");
    return false;
  }
  if (shape.m_numBytesByRow*8 < fileBox.size()[0] ||
      fileBox[0][0]>bitmapBox[0][0] || fileBox[0][1]>bitmapBox[0][1] ||
      fileBox[1][0]<bitmapBox[1][0] || fileBox[1][1]<bitmapBox[1][1]) {
    ascii().addPos(shape.m_bitmapEntry.begin());
    ascii().addNote("Bitmap[data]:###");
    MWAW_DEBUG_MSG(("MacDrawProParser::readObject: something look bad when reading a bitmap header\n"));
    shape.m_bitmapEntry=MWAWEntry();
  }
  else
    ascii().skipZone(shape.m_bitmapEntry.begin(), shape.m_bitmapEntry.end()-1);
  if (shape.m_bitmapEntry.end()+4<entry.end()) {
    ascii().addPos(shape.m_bitmapEntry.end());
    ascii().addNote("Bitmap[end]:###");
    MWAW_DEBUG_MSG(("MacDrawProParser::readObject: the bitmap data zone seems too big\n"));
  }
  return true;
}

bool MacDrawProParser::readObjectText(MacDrawProParserInternal::Shape &shape, int zId)
{
  if (zId<0 || zId>=(int) m_state->m_objectTextList.size() || !m_state->m_objectTextList[size_t(zId)].valid()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readObjectText: can not find the data for zone %d\n", zId));
    return false;
  }
  MWAWEntry const &entry=m_state->m_objectTextList[size_t(zId)];
  entry.setParsed(true);
  ascii().addPos(entry.end());
  ascii().addNote("_");

  libmwaw::DebugStream f;
  f << "ObjText[" << shape << "]:";
  if (entry.length()<8) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readObjectText: the zone seems too short \n"));
    f << "###";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
  }

  MWAWInputStreamPtr input = getInput();
  long savePos=input->tell();
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  int val=(int) input->readLong(2); // always 0?
  if (val) f << "f0=" << val << ",";
  val=(int) input->readLong(2); // 1|3
  if (val!=1) f << "type=" << val << ",";
  input->seek(4, librevenge::RVNG_SEEK_CUR); // skip the size field

  std::string text("");
  long N=int(entry.length()-8);
  for (long i=0; i<N; ++i) {
    char c=(char) input->readULong(1);
    if (c) {
      text+=c;
      continue;
    }
    // this zone seems aligned to a multiple of 4, so 0 can appear in the last position
    if (i<N-3) {
      f << "###";
      MWAW_DEBUG_MSG(("MacDrawProParser::readObjectText: find an unexpected char 0\n"));
    }
    break;
  }
  f << text;
  if (input->tell()!=entry.end())
    ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());

  input->seek(savePos, librevenge::RVNG_SEEK_SET);
  return true;
}


////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool MacDrawProParser::checkHeader(MWAWHeader *header, bool strict)
{
  *m_state = MacDrawProParserInternal::State();
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
  }
  else if (val==0x5354) { // template
    f << "stationery,";
    if (input->readULong(2)!=0x4154) return false;
  }
  else return false;

  val=(int) input->readULong(2);
  if (val==0x4432)
    vers=0;
  else if (val==0) {
    f << "D2[not],";
    vers=0;
  }
  else {
    MWAW_DEBUG_MSG(("MacDrawProParser::checkHeader: find unexpected header\n"));
    return false;
  }
  f << "version=" << vers << ",";
  f << "subVersion=" << input->readLong(2) << ",";
  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  if (strict && !readPrintInfo()) {
    input->seek(8, librevenge::RVNG_SEEK_SET);
    for (int i=0; i<10; ++i) // allow print info to be zero
      if (input->readLong(2)) return false;
  }

  if (strict) {
    // we must check that this is not a basic pict file
    input->seek(512+2, librevenge::RVNG_SEEK_SET);
    int dim[4];
    for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
    val=(int) input->readLong(2);
    if (dim[0]<dim[2] && dim[1]<dim[3] && (val==0x1101 || (val==0x11 && input->readLong(2)==0x2ff))) {
      // posible
      input->seek(512, librevenge::RVNG_SEEK_SET);
      Box2f box;
      if (MWAWPictData::check(input, (int)(input->size()-512), box) != MWAWPict::MWAW_R_BAD)
        return false;
    }
  }

  setVersion(vers);
  m_state->m_version=vers;
  if (header)
    header->reset(MWAWDocument::MWAW_T_MACDRAWPRO, vers, MWAWDocument::MWAW_K_DRAW);
  input->seek(512,librevenge::RVNG_SEEK_SET);

  return true;
}

////////////////////////////////////////////////////////////
// try to read the prefs zone
////////////////////////////////////////////////////////////
bool MacDrawProParser::readHeaderInfo()
{
  MWAWInputStreamPtr input = getInput();
  if (!input->checkPosition(512)) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readHeaderInfo: the prefs zone seems too short\n"));
    ascii().addPos(14);
    ascii().addNote("Entries(HeaderInfo):#");
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
  f << "Entries(HeaderInfo):";
  for (int i=0; i<9; ++i) { // f0=1|2|7, f1=0|75|78|7c, f2=0|48, f4=0|48, f5=0|48, f6=0|48, f7=0|48
    int val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  ascii().addDelimiter(input->tell(),'|');
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(0x100, librevenge::RVNG_SEEK_SET);
  pos=input->tell();
  f.str("");
  f << "HeaderInfo-A:";
  int val=(int) input->readULong(2); // 0 or 8
  if (val) f << "f0=" << val << ",";
  m_state->m_numLayers=(int) input->readULong(2); // 1|2|9
  if (m_state->m_numLayers!=1) f << "num[layers]=" << m_state->m_numLayers << ",";
  std::string extra("");
  if (!m_styleManager->readHeaderInfoStylePart(extra)) {
    f << "###Style[ignored],";
    input->seek(pos+4+24, librevenge::RVNG_SEEK_SET);
  }
  else
    f << extra;
  for (int i=0; i<2; ++i) { // h0=1|2|9 ( maybe related to layer), h1=1|2|40
    val=(int) input->readULong(2);
    if (val!=1) f << "h" << i << "=" << val << ",";
  }
  val=(int) input->readULong(2); // 0|45
  if (val) f << "h2=" << val << ",";
  val=(int) input->readULong(2); // actual shape ?
  if (val!=1) f << "shape[actual]?=" << val << ",";
  for (int i=0; i<2; ++i) { // h3=0, h4=2|3|7|a
    val=(int) input->readULong(2);
    if (val) f << "h" << i+3 << "=" << val << ",";
  }
  m_state->m_numLibraries=(int) input->readULong(2);
  if (m_state->m_numLibraries) f << "num[libraries]=" << m_state->m_numLibraries << ",";

  input->seek(pos+40, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());
  for (int i=0; i<3; ++i) {
    pos=input->tell();
    f.str("");
    f << "HeaderInfo-B" << i << ":";
    val=(int) input->readULong(2); // always 0 ?
    if (val) f << "f0=" << val << ",";

    val=(int) input->readULong(2);
    if (val && i==0) {
      m_state->m_numLibraries=val;
      f << "num[libraries]=" << m_state->m_numLibraries << ",";
    }
    else if (val)
      f << "num[shape?]=" << val << ",";

    ascii().addDelimiter(input->tell(),'|');
    input->seek(pos+40, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }

  pos=input->tell();
  f.str("");
  f << "HeaderInfo-C:";
  for (int i=0; i<5; ++i) {
    m_state->m_sizeStyleZones[i]=(long) input->readULong(4);
    if (!m_state->m_sizeStyleZones[i]) continue;
    f << "sz[style" << i << "]=" << std::hex << m_state->m_sizeStyleZones[i] << std::dec << ",";
  }
  for (int i=0;  i<5; ++i) {
    long lVal=(long) input->readULong(4);
    if (!lVal) continue;
    MWAW_DEBUG_MSG(("MacDrawProParser::readHeaderInfo: find some unexpected value in C zone, we may have a problem\n"));
    f << "##f" << i << "=" << std::hex << lVal << std::dec << ",";
  }
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  pos=input->tell();
  f.str("");
  f << "HeaderInfo-C(II):";
  for (int i=0; i<2; ++i)
    m_state->m_sizeLayerZones[i]=(long) input->readULong(4);
  f << "sz[layers]=" << std::hex << m_state->m_sizeLayerZones[0] << "+" << m_state->m_sizeLayerZones[1] << std::dec << ",";
  m_state->m_sizeFZones[0]=(long) input->readULong(4);
  if (m_state->m_sizeFZones[0])
    f << "sz[ZoneF0]=" << std::hex << m_state->m_sizeFZones[0] << std::dec << ",";
  for (int i=0; i<2; ++i)
    m_state->m_sizeLibraryZones[i]=(long) input->readULong(4);
  if (m_state->m_sizeLibraryZones[0] || m_state->m_sizeLibraryZones[1])
    f << "sz[libraries]=" << std::hex << m_state->m_sizeLibraryZones[0] << "+" << m_state->m_sizeLibraryZones[1] << std::dec << ",";
  for (int i=1; i<4; ++i) {
    m_state->m_sizeFZones[i]=(long) input->readULong(4);
    if (!m_state->m_sizeFZones[i]) continue;
    f << "sz[ZoneF" << i << "]=" << std::hex << m_state->m_sizeFZones[i] << std::dec << ",";
  }
  for (int i=0; i<3; ++i) { // always 0?
    long lVal=(long) input->readULong(4);
    if (!lVal) continue;
    MWAW_DEBUG_MSG(("MacDrawProParser::readHeaderInfo: find some unexpected value in C(II) zone, we may have a problem\n"));
    f << "##f" << i << "=" << lVal << ",";
  }

  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

  input->seek(0x1f4, librevenge::RVNG_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
// try to read the print info zone
////////////////////////////////////////////////////////////
bool MacDrawProParser::readPrintInfo()
{
  MWAWInputStreamPtr input = getInput();
  long pos=input->tell();
  long endPos=pos+120;
  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readPrintInfo: file seems too short\n"));
    return false;
  }
  libmwaw::DebugStream f;
  f << "Entries(PrintInfo):";
  libmwaw::PrinterInfo info;
  if (!info.read(input)) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readPrintInfo: can not read print info\n"));
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

////////////////////////////////////////////////////////////
//
// send data
//
////////////////////////////////////////////////////////////

bool MacDrawProParser::send(MacDrawProParserInternal::Shape const &shape)
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MacDrawProParser::send: can not find the listener\n"));
    return false;
  }
  shape.m_isSent=true;
  Box2f box=shape.getBdBox();
  MWAWPosition pos(box[0], box.size(), librevenge::RVNG_POINT);
  pos.m_anchorTo = MWAWPosition::Page;
  switch (shape.m_type) {
  case MacDrawProParserInternal::Shape::Basic:
    listener->insertPicture(pos, shape.m_shape, shape.m_style);
    break;
  case MacDrawProParserInternal::Shape::Bitmap:
    return sendBitmap(shape, pos);
  case MacDrawProParserInternal::Shape::Group: {
    size_t numShapes=m_state->m_shapeList.size();
    if (!numShapes) break;
    listener->openLayer(pos);
    for (size_t i=0; i<shape.m_childList.size(); ++i) {
      if (shape.m_childList[i]>=numShapes) {
        MWAW_DEBUG_MSG(("MacDrawProParser::send: can not find a child\n"));
        continue;
      }
      MacDrawProParserInternal::Shape const &child=m_state->m_shapeList[shape.m_childList[i]];
      if (child.m_isSent) {
        MWAW_DEBUG_MSG(("MacDrawProParser::send: the child is already sent\n"));
        continue;
      }
      send(child);
    }
    listener->closeLayer();
    break;
  }
  case MacDrawProParserInternal::Shape::GroupEnd:
    break;
  case MacDrawProParserInternal::Shape::Text: {
    shared_ptr<MWAWSubDocument> doc(new MacDrawProParserInternal::SubDocument(*this, getInput(), shape.m_id));
    listener->insertTextBox(pos, doc, shape.m_style);
    return true;
  }
  case MacDrawProParserInternal::Shape::Note:
  case MacDrawProParserInternal::Shape::Unknown:
  default:
    return false;
  }
  return true;
}

bool MacDrawProParser::sendBitmap(MacDrawProParserInternal::Shape const &shape, MWAWPosition const &position)
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MacDrawProParser::sendBitmap: can not find the listener\n"));
    return false;
  }
  if (!shape.m_bitmapEntry.valid()) return false;
  int const numBytesByRow=shape.m_numBytesByRow;
  if (shape.m_type!=MacDrawProParserInternal::Shape::Bitmap || numBytesByRow<=0 ||
      numBytesByRow*shape.m_bitmapFileDim.size()[1]<shape.m_bitmapEntry.length() ||
      shape.m_bitmapDim[0][0]<0 || shape.m_bitmapDim[0][1]<0 ||
      shape.m_bitmapDim[0][0]<shape.m_bitmapFileDim[0][0] ||
      shape.m_bitmapFileDim.size()[0]<=0 || shape.m_bitmapFileDim.size()[1]<=0 ||
      8*numBytesByRow<shape.m_bitmapFileDim.size()[0]) {
    MWAW_DEBUG_MSG(("MacDrawProParser::sendBitmap: the bitmap seems bad\n"));
    return false;
  }
  // change: implement indexed transparent color, replaced this code
  MWAWPictBitmapColor pict(shape.m_bitmapDim[1], true);
  MWAWColor transparent(255,255,255,0);
  MWAWColor black(MWAWColor::black());
  std::vector<MWAWColor> data(size_t(shape.m_bitmapDim[1][0]), transparent);
  // first set unseen row to zero (even if this must not appear)
  for (int r=shape.m_bitmapDim[0][1]; r<shape.m_bitmapFileDim[0][1]; ++r) pict.setRow(r, &data[0]);
  for (int r=shape.m_bitmapFileDim[1][1]; r<shape.m_bitmapDim[1][1]; ++r) pict.setRow(r, &data[0]);

  MWAWInputStreamPtr input=getInput();
  input->seek(shape.m_bitmapEntry.begin(), librevenge::RVNG_SEEK_SET);
  for (int r=shape.m_bitmapFileDim[0][1]; r<shape.m_bitmapFileDim[1][1]; ++r) {
    long pos=input->tell();
    if (r<shape.m_bitmapDim[0][1]||r>=shape.m_bitmapDim[1][1]) { // must not appear, but...
      input->seek(pos+numBytesByRow, librevenge::RVNG_SEEK_SET);
      continue;
    }
    int wPos=shape.m_bitmapFileDim[0][0];
    for (int col=shape.m_bitmapFileDim[0][0]; col<shape.m_bitmapFileDim[1][0]; ++col) {
      unsigned char c=(unsigned char) input->readULong(1);
      for (int j=0, bit=0x80; j<8 ; ++j, bit>>=1) {
        if (wPos>=shape.m_bitmapDim[1][0]) break;
        data[size_t(wPos++)]=(c&bit) ? black : transparent;
      }
    }
    pict.setRow(r, &data[0]);
    input->seek(pos+numBytesByRow, librevenge::RVNG_SEEK_SET);
  }

  librevenge::RVNGBinaryData binary;
  std::string type;
  if (!pict.getBinary(binary,type)) return false;
#ifdef DEBUG_WITH_FILES
  static int volatile pictName = 0;
  libmwaw::DebugStream f;
  f << "PICT-" << ++pictName << ".bmp";
  libmwaw::Debug::dumpFile(binary, f.str().c_str());
#endif

  listener->insertPicture(position, binary, type);

  return true;
}

bool MacDrawProParser::sendText(int zId)
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MacDrawProParser::sendText: can not find the listener\n"));
    return false;
  }
  if (zId<0||zId>=(int) m_state->m_shapeList.size() ||
      m_state->m_shapeList[size_t(zId)].m_type != MacDrawProParserInternal::Shape::Text) {
    MWAW_DEBUG_MSG(("MacDrawProParser::sendText: can not find the text shape\n"));
    return false;
  }
  MacDrawProParserInternal::Shape const &shape=m_state->m_shapeList[size_t(zId)];
  shape.m_isSent = true;
  if (!shape.m_textEntry.valid())
    return true;

  listener->setParagraph(shape.m_paragraph);
  listener->setFont(shape.m_font);

  MWAWInputStreamPtr input=getInput();
  input->seek(shape.m_textEntry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Object[text]:";
  long endPos=shape.m_textEntry.end();
  while (!input->isEnd()) {
    if (input->tell()>=shape.m_textEntry.end())
      break;
    char c = (char) input->readULong(1);
    if (c==0) {
      MWAW_DEBUG_MSG(("MacDrawProParser::sendText: find char 0\n"));
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
  ascii().addPos(shape.m_textEntry.begin());
  ascii().addNote(f.str().c_str());
  return true;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
