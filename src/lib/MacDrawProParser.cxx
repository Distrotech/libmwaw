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
  Layer() : m_numShapes(0), m_firstShape(-1), m_isHidden(false), m_box(), m_libraryToObjectMap(), m_name("")
  {
    for (int i=0; i<3; ++i) m_N[i]=0;
  }
  //! the number of shape
  int m_numShapes;
  //! the first shape
  int m_firstShape;
  //! true if the layer is hidden
  bool m_isHidden;
  //! the layer bounding box (if computed)
  Box2f m_box;
  //! map library to pos
  std::map<int, int>  m_libraryToObjectMap;
  //! some unknown number find in the beginning of the header
  long m_N[3];
  //! the layer name
  librevenge::RVNGString m_name;
};

// generic class used to defined a library
struct Library {
  //! constructor
  Library(int id=-1) : m_id(id), m_layerList(), m_box(), m_name("")
  {
  }
  //! the library id
  int m_id;
  //! the list of layer id
  std::vector<int> m_layerList;
  //! the bounding box (if computed)
  Box2f m_box;
  //! the library name
  librevenge::RVNGString m_name;
};

// generic class used to store shape in MWAWDrawParser
struct Shape {
  //! the different shape
  enum Type { Basic, Bitmap, Group, GroupEnd, Note, Text, Unknown };

  //! constructor
  Shape() : m_type(Unknown), m_fileType(0), m_box(), m_style(), m_shape(), m_id(-1), m_nextId(-1), m_flags(0),
    m_textZoneId(-1), m_numChars(0), m_fontMap(), m_lineBreakSet(), m_paragraphMap(), m_paragraph(), m_childList(),
    m_labelBox(), m_labelEntry(),
    m_numBytesByRow(0), m_bitmapIsColor(false), m_bitmapDim(), m_bitmapFileDim(), m_bitmapEntry(), m_bitmapClutId(0), m_isSent(false)
  {
  }

  //! basic operator<<
  friend std::ostream &operator<<(std::ostream &o, Shape const &shape);

  //! return the shape bdbox
  Box2f getBdBox() const
  {
    if (m_type==Basic)
      return m_shape.getBdBox();
    if (m_style.m_rotate>=0 && m_style.m_rotate<=0) return m_box;
    return libmwaw::rotateBoxFromCenter(m_box,m_style.m_rotate);
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

  // text box or note

  //! the text zone ( for a text box or a note)
  int m_textZoneId;
  //! the number of caracters ( for a text box or a note)
  int m_numChars;
  //! a map position to font id ( for a text box or a note)
  std::map<int,int> m_fontMap;
  //! the list of line break position ( for a text box or a note)
  std::set<int> m_lineBreakSet;
  //! the paragraphMap ( for a text box or a note) and a Pro file
  std::map<int,int> m_paragraphMap;
  //! the paragraph ( for a text box or a note) and a II file
  MWAWParagraph m_paragraph;

  // group

  //! the child list ( for a group )
  std::vector<size_t> m_childList;

  // line
  // the label box ( for a line)
  Box2f m_labelBox;
  // the label message ( for a line )
  MWAWEntry m_labelEntry;

  // bitmap

  //! the number of bytes by row (for a bitmap)
  int m_numBytesByRow;
  //! true if the bitmap is a color bitmap
  bool m_bitmapIsColor;
  //! the bitmap dimension (in page)
  Box2i m_bitmapDim;
  //! the bitmap dimension (in the file)
  Box2i m_bitmapFileDim;
  //! the bitmap entry (data)
  MWAWEntry m_bitmapEntry;
  //! the bitmap clut rsrc id
  int m_bitmapClutId;
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
      o << "spline,";
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
  State() : m_version(0), m_isStationery(false), m_numPages(1),
    m_actualLayer(1), m_numLayers(1), m_numHiddenLayers(0), m_numVisibleLayers(0), m_createMasterPage(false), m_sendAsLibraries(false),
    m_numLibraries(0), m_numShapes(0),
    m_libraryList(), m_layerList(), m_objectDataList(), m_objectTextList(), m_shapeList()
  {
    for (int i=0; i<6; ++i) m_sizeStyleZones[i]=0;
    for (int i=0; i<2; ++i) m_sizeLayerZones[i]=0;
    for (int i=0; i<2; ++i) m_sizeLibraryZones[i]=0;
    for (int i=0; i<4; ++i) m_sizeFZones[i]=0;
  }
  //! the file version
  int m_version;
  //! flag to know if the file is a stationery document
  bool m_isStationery;
  //! the final number of pages
  int m_numPages;
  //! the actual layer
  int m_actualLayer;
  //! the number of layer
  int m_numLayers;
  //! the number of hidden layer
  int m_numHiddenLayers;
  //! the number of visible layer
  int m_numVisibleLayers;
  //! flag to know if we need or not to create a master
  bool m_createMasterPage;
  //! flag to know if we create a page by library or not
  bool m_sendAsLibraries;
  //! the number of library
  int m_numLibraries;
  //! the total number of shapes
  int m_numShapes;
  //! the size of the header zones
  long m_sizeStyleZones[6];
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
  //! constructor given an zone id
  SubDocument(MacDrawProParser &pars, MWAWInputStreamPtr input, int zoneId) : MWAWSubDocument(&pars, input, MWAWEntry()), m_id(zoneId), m_labelEntry() {}
  //! constructor given a label entry
  SubDocument(MacDrawProParser &pars, MWAWInputStreamPtr input, MWAWEntry const &labelEntry) : MWAWSubDocument(&pars, input, MWAWEntry()), m_id(-1), m_labelEntry(labelEntry) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const
  {
    if (MWAWSubDocument::operator!=(doc)) return true;
    SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
    if (!sDoc) return true;
    if (m_id != sDoc->m_id) return true;
    if (m_labelEntry != sDoc->m_labelEntry) return true;
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
  //! the label entry
  MWAWEntry m_labelEntry;
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
  if (!parser) {
    MWAW_DEBUG_MSG(("MacDrawProParserInternal::SubDocument::parse: no parser\n"));
    return;
  }
  long pos = m_input->tell();
  if (m_id >= 0)
    parser->sendText(m_id);
  else
    parser->sendLabel(m_labelEntry);
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
      sendMasterPage();
      for (int i=0; i<m_state->m_numPages; ++i)
        sendPage(i);
#ifdef DEBUG
      flushExtra();
#endif
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

  // we need one page for the master page: one page by hidden layers
  int numPages=m_state->m_sendAsLibraries ? (int) m_state->m_libraryList.size() :
               m_state->m_numHiddenLayers;
  if (numPages<=0) numPages=1;
  m_state->m_numPages = numPages;

  // create the list of page names
  std::vector<librevenge::RVNGString> namesList;
  if (m_state->m_sendAsLibraries) {
    for (size_t i=0; i<m_state->m_libraryList.size(); ++i) {
      MacDrawProParserInternal::Library const &library=m_state->m_libraryList[i];
      namesList.push_back(library.m_name);
    }
  }
  else {
    for (size_t i=0; i<m_state->m_layerList.size(); ++i) {
      MacDrawProParserInternal::Layer const &layer=m_state->m_layerList[i];
      if (!layer.m_isHidden) continue;
      namesList.push_back(layer.m_name);
    }
  }
  if ((int) namesList.size() < numPages)
    namesList.resize(size_t(numPages), "");

  // create the page list
  MWAWPageSpan ps(getPageSpan());
  m_state->m_createMasterPage=!m_state->m_sendAsLibraries &&
                              m_state->m_numHiddenLayers>1 && m_state->m_numVisibleLayers>0;
  if (m_state->m_createMasterPage)
    ps.setMasterPageName("Master");

  std::vector<MWAWPageSpan> pageList;
  int actUnamedPage=0;
  for (int i=0; i<numPages; ++i) {
    if (namesList[size_t(i)].empty()) {
      ++actUnamedPage;
      continue;
    }
    if (actUnamedPage) {
      ps.setPageSpan(actUnamedPage);
      pageList.push_back(ps);
      actUnamedPage=0;
    }
    MWAWPageSpan psNamed(ps);
    psNamed.setPageName(namesList[size_t(i)]);
    psNamed.setPageSpan(1);
    pageList.push_back(psNamed);
  }
  if (actUnamedPage) {
    ps.setPageSpan(actUnamedPage);
    pageList.push_back(ps);
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
bool MacDrawProParser::createZones()
{
  MWAWInputStreamPtr input = getInput();
  int const vers=version();
  if (getRSRCParser()) m_styleManager->readRSRCZones();
  readHeaderInfo();

  input->seek(vers==0 ? 0x1f4 : 0x1d4,librevenge::RVNG_SEEK_SET);
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

  return computeLayersAndLibrariesBoundingBox();
}

////////////////////////////////////////////////////////////
// layer/library functions
////////////////////////////////////////////////////////////
bool MacDrawProParser::readLayersInfo()
{
  int const vers=version();
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
    layer.m_firstShape=numShapes;
    if (i && m_state->m_actualLayer==i+1)
      layer.m_isHidden=true;
    pos=input->tell();
    f.str("");
    f << "Entries(Layer)[L" << i+1 << "]:";
    long val=(long) input->readULong(2);
    if (val&0x8000) {
      layer.m_isHidden=true;
      f << "hidden,";
    }
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

    input->seek(pos+(vers==0 ? 12 : 14), librevenge::RVNG_SEEK_SET);
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
    librevenge::RVNGString name("");
    for (int c=0; c<fSz; ++c) {
      char ch=(char) input->readULong(1);
      if (!ch) continue;
      f << ch;
      int unicode= getParserState()->m_fontConverter->unicode(3, (unsigned char) ch);
      if (unicode==-1)
        name.append(ch);
      else
        libmwaw::appendUnicode((uint32_t) unicode, name);
    }
    f << ",";
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
  MacDrawProParserInternal::Layer falseLayer;
  int actId=0;
  for (std::map<int, long>::const_iterator it=idToDecal.begin(); it!=idToDecal.end(); ++it) {
    long decal=it->second;
    if (decal<0 || decal+8>sz) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readLayerLibraryCorrespondance: the zone %d's decal seems bad\n", it->first));
      continue;
    }
    int id=actId++;
    f.str("");
    f << "LayToLib-L" << id+1 << ":";
    if (id<0 || id>=(int) m_state->m_layerList.size()) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readLayerLibraryCorrespondance: the layer's number:%d seems bad\n", id));
    }
    MacDrawProParserInternal::Layer &layer=(id>=0 && id<(int) m_state->m_layerList.size()) ?
                                           m_state->m_layerList[size_t(id)] : falseLayer;
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
      int obj=(int) input->readLong(4); // find 1 or 2
      int val1=(int) input->readLong(2);
      int library=(int) input->readLong(2);
      if (library<1 || library>m_state->m_numLibraries) {
        MWAW_DEBUG_MSG(("MacDrawProParser::readLayerLibraryCorrespondance: a library number seems bad\n"));
        f << "###";
      }
      else {
        if (library>int(m_state->m_libraryList.size()))
          m_state->m_libraryList.resize(size_t(library));
        m_state->m_libraryList[size_t(library-1)].m_id=library;
        m_state->m_libraryList[size_t(library-1)].m_layerList.push_back(id);
        layer.m_libraryToObjectMap[library]=obj;
      }
      f << "Li" << library;
      if (obj!=1) f << ":O" << obj-1;
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

  int const vers=version();
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
  int const fieldSize=vers==0 ? 8 : 10;
  if ((m_state->m_sizeLibraryZones[0]%fieldSize) || !m_state->m_sizeLibraryZones[0]||!m_state->m_sizeLibraryZones[1]) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readLibrariesInfo: problem with the size zone(II)\n"));
    ascii().addPos(pos);
    ascii().addNote("Entries(Library):###");
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    return true;
  }

  libmwaw::DebugStream f;
  f << "Entries(Library):";
  std::vector<long> posList;
  int N=int(m_state->m_sizeLibraryZones[0]/fieldSize);
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
    if (val) f << ":" << val;
    f << ",";
    if (vers>0) {
      val=(int) input->readLong(2);
      if (val!=i+1) f << "id=" << val << ",";
    }
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
    librevenge::RVNGString name("");
    for (int c=0; c<fSz; ++c) {
      char ch=(char) input->readULong(1);
      if (!ch) continue;
      f << ch;
      int unicode= getParserState()->m_fontConverter->unicode(3, (unsigned char) ch);
      if (unicode==-1)
        name.append(ch);
      else
        libmwaw::appendUnicode((uint32_t) unicode, name);
    }
    f << ",";
    if (int(i) < m_state->m_numLibraries) {
      if (i>=m_state->m_libraryList.size()) {
        m_state->m_libraryList.resize(i+1);
        m_state->m_libraryList[i].m_id=int(i)+1;
      }
      m_state->m_libraryList[i].m_name=name;
    }

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

bool MacDrawProParser::computeLayersAndLibrariesBoundingBox()
{
  if (m_state->m_layerList.empty()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::computeLayersAndLibrariesBoundingBox: can not find any layer\n"));
    return false;
  }
  size_t numLayers=m_state->m_layerList.size();
  for (size_t i=0; i<numLayers; ++i) {
    MacDrawProParserInternal::Layer &layer=m_state->m_layerList[i];
    if (layer.m_firstShape < 0 || layer.m_numShapes < 0 ||
        (layer.m_numShapes && layer.m_firstShape >= int(m_state->m_shapeList.size()))) {
      MWAW_DEBUG_MSG(("MacDrawProParser::computeLayersAndLibrariesBoundingBox: the layer %d seems bad\n", int(i)));
      layer.m_numShapes=0;
      layer.m_isHidden=false;
      continue;
    }
    if (layer.m_isHidden)
      ++m_state->m_numHiddenLayers;
    else
      ++m_state->m_numVisibleLayers;
    Box2f box;
    bool boxSet=false;
    for (int j=layer.m_firstShape; j<layer.m_firstShape+layer.m_numShapes; ++j) {
      if (j>=int(m_state->m_shapeList.size())) {
        MWAW_DEBUG_MSG(("MacDrawProParser::computeLayersAndLibrariesBoundingBox: the layer %d seems to contain to much shapes\n", int(i)));
        layer.m_numShapes=j-layer.m_firstShape;
        break;
      }
      MacDrawProParserInternal::Shape const &shape=m_state->m_shapeList[size_t(j)];
      if (shape.m_type==MacDrawProParserInternal::Shape::Group ||
          shape.m_type==MacDrawProParserInternal::Shape::GroupEnd ||
          shape.m_type==MacDrawProParserInternal::Shape::Unknown)
        continue;
      if (!boxSet) {
        box=shape.getBdBox();
        boxSet=true;
      }
      else
        box=box.getUnion(shape.getBdBox());
    }
    if (boxSet)
      layer.m_box=box;
    else if (layer.m_numShapes) {
      MWAW_DEBUG_MSG(("MacDrawProParser::computeLayersAndLibrariesBoundingBox: can not compute the bdbox of the layer %d\n", int(i)));
    }
  }

  if (m_state->m_libraryList.empty()) {
    // create a false library which contains all layer
    MacDrawProParserInternal::Library library(1);
    for (int i=0; i<int(numLayers); ++i)
      library.m_layerList.push_back(i);
    m_state->m_libraryList.push_back(library);
  }

  Vec2f pageSize(float(72*getPageSpan().getFormWidth()), float(72*getPageSpan().getFormLength()));
  Vec2f leftTop(72.f*float(getPageSpan().getMarginLeft()),72.f*float(getPageSpan().getMarginTop()));
  Box2f docBox;
  bool docBoxSet=false;
  int numLibraryWithGroup=0;
  for (size_t i=0; i<m_state->m_libraryList.size(); ++i) {
    MacDrawProParserInternal::Library &library=m_state->m_libraryList[i];
    if (library.m_layerList.empty()) continue;
    Box2f box;
    bool boxSet=false;
    for (size_t j=0; j<library.m_layerList.size(); ++j) {
      int id=library.m_layerList[j];
      if (id<0 || id>=int(numLayers)) {
        MWAW_DEBUG_MSG(("MacDrawProParser::computeLayersAndLibrariesBoundingBox: library %d contains bad layer\n", int(i)));
        library.m_layerList[j]=-1;
        continue;
      }
      MacDrawProParserInternal::Layer const &layer= m_state->m_layerList[size_t(id)];
      if (layer.m_box.size()[0]<0 || layer.m_box.size()[1]<0)
        continue;
      Box2f newBox(layer.m_box);
      if (layer.m_libraryToObjectMap.find(int(i+1))!=layer.m_libraryToObjectMap.end()) {
        int objId=layer.m_firstShape+layer.m_libraryToObjectMap.find(int(i+1))->second;
        if (objId<=0 || objId>int(m_state->m_shapeList.size())) {
          MWAW_DEBUG_MSG(("MacDrawProParser::computeLayersAndLibrariesBoundingBox: can not find begin library group %d\n", int(objId)));
        }
        else {
          MacDrawProParserInternal::Shape const &shape=m_state->m_shapeList[size_t(objId-1)];
          if (shape.m_type==MacDrawProParserInternal::Shape::Group) {
            newBox=shape.m_box;
            ++numLibraryWithGroup;
          }
          // else seems to happen sometimes, maybe, this may be ok...
        }
      }

      if (!boxSet) {
        box=Box2f(newBox[0]+leftTop, newBox[1]+leftTop);
        boxSet=true;
      }
      else
        box=box.getUnion(Box2f(newBox[0]+leftTop, newBox[1]+leftTop));
    }
    if (boxSet)
      library.m_box=box;
    else if (m_state->m_numShapes>0) {
      MWAW_DEBUG_MSG(("MacDrawProParser::computeLayersAndLibrariesBoundingBox: can not compute the bdbox of the library %d\n", int(i)));
    }
    // convert it to a multiple of page size
    Vec2f libRB=library.m_box[1];
    for (int c=0; c<2; ++c) {
      if (pageSize[c]<=0) continue;
      if (libRB[c]<=0)
        libRB[c]=pageSize[c];
      else {
        int numPage=int(libRB[c]/pageSize[c]-0.01);
        if (numPage<0) numPage=0;
        libRB[c]=float(numPage+1)*pageSize[c];
      }
    }
    library.m_box.setMax(libRB);
    if (!docBoxSet) {
      docBox=library.m_box;
      docBoxSet=true;
    }
    else
      docBox=docBox.getUnion(library.m_box);
  }
  if (docBoxSet) {
    getPageSpan().setFormWidth(double(docBox[1][0])/72);
    getPageSpan().setFormLength(double(docBox[1][1])/72);
  }
  if (m_state->m_isStationery && numLayers==1 && numLibraryWithGroup>1 && version()>0 &&
      numLibraryWithGroup==(int) m_state->m_libraryList.size())
    m_state->m_sendAsLibraries=true;
  return true;
}

int MacDrawProParser::readObject()
{
  int const vers=version();
  MWAWInputStreamPtr input = getInput();
  if (input->isEnd()) return -1;
  if (m_state->m_numShapes <= int(m_state->m_shapeList.size()))
    return false;

  long pos=input->tell();
  libmwaw::DebugStream f;
  size_t shapeId= m_state->m_shapeList.size();
  f << "Entries(Object)[O" << shapeId << "]:";
  int const expectedSize=vers==0 ? 32 : 34;
  if (!input->checkPosition(pos+expectedSize)) {
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
  int flags=(int) input->readULong(1);
  shape.m_flags=flags;
  if (flags & 0x8) f << "select,";
  if (flags & 0x20) f << "locked,";
  bool hasData=(flags & 0x40);
  if (flags & 0x80) f << "rotation,";
  flags &=0x17;
  if (flags) f << "fl0=" << std::hex << flags << std::dec << ",";
  int val=(int) input->readULong(1);
  // checkme
  if (val&0x20) {
    shape.m_style.m_flip[0]=true;
    f << "flipX,";
  }
  if (val&0x40) {
    shape.m_style.m_flip[1]=true;
    f << "flipY,";
  }
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
    shape.m_shape.m_type=MWAWGraphicShape::Polygon;
    f << "poly[smooth],";
    break;
  case 8:
    shape.m_type=MacDrawProParserInternal::Shape::Basic;
    shape.m_shape.m_type=MWAWGraphicShape::Polygon;
    f << "poly,";
    break;
  case 9:
    if (vers==0) {
      shape.m_type=MacDrawProParserInternal::Shape::Bitmap;
      f << "bitmap,";
    }
    else {
      shape.m_type=MacDrawProParserInternal::Shape::Basic;
      shape.m_shape.m_type=MWAWGraphicShape::Polygon;
      f << "poly[9],";
    }
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
  case 0xc:
    if (vers) {
      shape.m_type=MacDrawProParserInternal::Shape::Bitmap;
      f << "bitmap,";
      break;
    }
  // fall through intended
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
  if (vers==1) {
    val=(int) input->readULong(2);
    if (val) f << "f1=" << std::hex << val << std::dec << ",";
  }
  // reads the pattern
  int patId[2]= {0,0};
  static char const *(wh[])= {"line", "surf"};
  for (int i=0; i<2; ++i) {
    val=(int) input->readULong(2);
    if (vers>0) {
      patId[i]=val;
      continue;
    }
    if (!val) { // no color
      if (i==0) {
        shape.m_style.m_lineWidth=0;
        f << "line[no],";
      }
      continue;
    }
    if (val&0x4000) { // pattern created from colorMap
      int cId=val&0x3FFF;
      MWAWColor color;
      if (!m_styleManager->getColor(cId, color)) {
        MWAW_DEBUG_MSG(("MacDrawProParser::readObject: find unknown basic color pattern: %d\n", cId));
        f << wh[i] << "[color]=###" << cId << ",";
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
    f << wh[i] << "[pat]=[" << pattern << "],";
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

  float cornerWidth=-1;
  if (shape.m_fileType==4 && !hasData) {
    val=(int)input->readLong(4);
    if (val) {
      cornerWidth = float(val)/65536.f/36.f; // check the unit here
      f << "round[dim]=" << cornerWidth << ",";
    }
    else
      f << "round[def],";
  }
  else {
    // unknown
    val=(int) input->readLong(1); // a small number negative or positif between -4 and 3
    if (val)
      f << "f2=" << val << ",";
  }

  if (shape.m_type==MacDrawProParserInternal::Shape::Basic)
    updateGeometryShape(shape, cornerWidth);
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
    // in MacDraw Pro, the number of elements seems frequently a majorant, so ...
    if (vers>0 && val>0x40 && int(lastShapeId)>m_state->m_numShapes) {
      f << "#number[object]=" << N << ",";
      lastShapeId=size_t(m_state->m_numShapes);
    }
    if (val<0x40 || int(lastShapeId)>m_state->m_numShapes) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readObject: can not find the group number\n"));
      f << "###number[object]=" << N << ",";
    }
    else {
      f << "N=" << N << ",";
      if (val&0x1F)
        f << "N[low]="<< (val&0x1F) << ",";
      input->seek(pos+expectedSize, librevenge::RVNG_SEEK_SET);

      bool ok=true, findEnd=false;
      for (int i=0; i<N; ++i) {
        if (m_state->m_shapeList.size()>lastShapeId)
          break;
        int cId=readObject();
        if (cId<0) {
          MWAW_DEBUG_MSG(("MacDrawProParser::readObject: can not find a child\n"));
          f << "###";
          ok=false;
          break;
        }
        // in MacDraw Pro, the number of elements seems frequently a majorant, so we must check
        if (vers>0 && m_state->m_shapeList[size_t(cId)].m_type==MacDrawProParserInternal::Shape::GroupEnd) {
          f << "#[" << lastShapeId-m_state->m_shapeList.size() << "],";
          findEnd=true;
          break;
        }
        // do not use shape's childList as the vector can have grown
        m_state->m_shapeList[shapeId].m_childList.push_back(size_t(cId));
      }

      int nextId=int(m_state->m_shapeList.size());
      if (ok && !findEnd) {
        int cId=readObject(); // read end group
        if (cId<0 || m_state->m_shapeList[size_t(cId)].m_type!=MacDrawProParserInternal::Shape::GroupEnd) {
          MWAW_DEBUG_MSG(("MacDrawProParser::readObject: oops, can not find the end group data\n"));
          f << "###";
        }
        else // ok skip end group
          ++nextId;
      }
      m_state->m_shapeList[shapeId].m_nextId=nextId;

      ascii().addPos(pos);
      ascii().addNote(f.str().c_str());
      return int(shapeId);
    }
  }

  if (vers==0) {
    if (input->tell() != pos+expectedSize)
      ascii().addDelimiter(input->tell(),'|');
    input->seek(pos+expectedSize, librevenge::RVNG_SEEK_SET);
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
    return int(shapeId);
  }
  int colId[2]= {0,0};
  input->seek(pos+expectedSize-4, librevenge::RVNG_SEEK_SET);
  for (int i=0; i<2; ++i) colId[i]=(int) input->readULong(2);
  // time to set the color
  for (int i=0; i<2; ++i) {
    if (!patId[i]) { // no color
      if (i==0) {
        shape.m_style.m_lineWidth=0;
        f << "line[no],";
      }
      continue;
    }
    MWAWColor color(MWAWColor::black());
    if ((colId[i]>>14)==3) {
      if (!i || !m_styleManager->updateGradient(colId[i]&0x3FFF, shape.m_style)) {
        MWAW_DEBUG_MSG(("MacDrawProParser::readObject: find unknown gradient: %d\n", colId[i]));
        f << wh[i] << "[grad]=###" << std::hex << colId[i] << std::dec << ",";
      }
      continue;
    }
    else if ((colId[i]>>14)==1) {
      static bool first=true;
      if (first) {
        MWAW_DEBUG_MSG(("MacDrawProParser::readObject: find some 4000 color\n"));
        first=false;
      }
      f << wh[i] << "[color]=##" << std::hex << colId[i] << std::dec << ",";
      color=MWAWColor::white();
    }
    else if (colId[i] && !m_styleManager->getColor(colId[i], color)) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readObject: find unknown basic color: %d\n", colId[i]));
      f << wh[i] << "[color]=###" << std::hex << colId[i] << std::dec << ",";
      continue;
    }
    MWAWGraphicStyle::Pattern pattern;
    bool useBasicColor=false;
    // normally, we only have BWpattern
    MWAWColor patColor;
    if (patId[i]==0xC006) // used for black pattern
      useBasicColor=true;
    else if (!m_styleManager->getPattern(patId[i], pattern)) {
      f << wh[i] << "[pat]=###" << std::hex << patId[i] << std::dec << ",";
      useBasicColor=true;
    }
    else if (pattern.getUniqueColor(patColor))
      useBasicColor=true;
    if (useBasicColor) { // only color
      if (i==0) shape.m_style.m_lineColor=color;
      else shape.m_style.setSurfaceColor(color);
      if ((i==0&&!color.isBlack()) || (i==1 && !color.isWhite()))
        f << wh[i] << "[color]=" << color << ",";
      continue;
    }
    f << wh[i] << "[pat]=[" << pattern << "],";
    if (i==0 && pattern.getAverageColor(patColor)) {
      float alpha=1.0f-float(patColor.getBlue())/255.f;
      shape.m_style.m_lineColor=MWAWColor::barycenter(alpha, color, 1.f-alpha, MWAWColor::white());
    }
    else if (i==1) {
      pattern.m_colors[1]=color;
      shape.m_style.m_pattern=pattern;
    }
  }

  if (input->tell() != pos+expectedSize)
    ascii().addDelimiter(input->tell(),'|');
  input->seek(pos+expectedSize, librevenge::RVNG_SEEK_SET);
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
    bool res=readGeometryShapeData(shape, entry);
    input->seek(savePos, librevenge::RVNG_SEEK_SET);
    return res;
  }
  else if (shape.m_type==MacDrawProParserInternal::Shape::Bitmap) {
    bool res=readBitmap(shape, entry);
    input->seek(savePos, librevenge::RVNG_SEEK_SET);
    return res;
  }
  else if (shape.m_type==MacDrawProParserInternal::Shape::Text ||
           shape.m_type==MacDrawProParserInternal::Shape::Note) {
    bool res=version()==0 ? readTextII(shape, entry) : readTextPro(shape, entry);
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
  if (val) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readObjectData: find unexpected f2 position\n"));
    f << "#f2=" << val << ",";
  }
  std::string extra("");
  if (!readRotationInObjectData(shape, entry.end(), extra)) {
    f << "###rot,";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
    return false;
  }
  f << extra;

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

bool MacDrawProParser::readRotationInObjectData(MacDrawProParserInternal::Shape &shape, long endPos, std::string &extra)
{
  if ((shape.m_flags & 0x80)==0)
    return true;

  MWAWInputStreamPtr input = getInput();
  if (input->tell()+28>endPos) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readRotationInObjectData: can not find the rotation data\n"));
    extra="###rot,";
    return false;
  }

  libmwaw::DebugStream f;
  float angle= float(input->readLong(4))/65536.f; // in radians
  shape.m_style.m_rotate = float(180./M_PI*angle);
  f << "angl[rot]=" << shape.m_style.m_rotate << ",";
  float dim[4];
  for (int i=0; i<4; ++i) dim[i]=float(input->readLong(4))/65536.f;
  Box2f rect(Vec2f(dim[1],dim[0]), Vec2f(dim[3],dim[2]));
  f << "prevDim[rot]=" << rect << ",";
  f << "unkn[rot]=[";
  for (int i=0; i<2; ++i)  // another points ?
    f << float(input->readLong(4))/65536.f  << ",";
  f << "],";
  shape.m_box=Box2f(rect[0]+shape.m_box[0], rect[1]+shape.m_box[0]);
  if (shape.m_type==MacDrawProParserInternal::Shape::Basic)
    shape.m_shape.m_bdBox=shape.m_shape.m_formBox=shape.m_box;
  extra=f.str();
  return true;
}

bool MacDrawProParser::updateGeometryShape(MacDrawProParserInternal::Shape &shape, float cornerWidth)
{
  if (shape.m_type!=MacDrawProParserInternal::Shape::Basic) {
    MWAW_DEBUG_MSG(("MacDrawProParser::updateGeometryShape: called with unexpected shape\n"));
    return false;
  }
  switch (shape.m_fileType) {
  case 2: {
    int flipX=shape.m_style.m_flip[0] ? 1 : 0;
    int flipY=shape.m_style.m_flip[1] ? 1 : 0;
    shape.m_shape=MWAWGraphicShape::line
                  (Vec2f(shape.m_box[1-flipX][0],shape.m_box[1-flipY][1]),
                   Vec2f(shape.m_box[flipX][0],shape.m_box[flipY][1]));
    break;
  }
  case 3: // rect
  case 4: { // rectOval
    float cWidth=0;
    if (shape.m_fileType==4)
      cWidth=cornerWidth>0 ? cornerWidth : 25;
    shape.m_shape=MWAWGraphicShape::rectangle(shape.m_box, Vec2f(cWidth,cWidth));
    break;
  }
  case 5: // circle
    shape.m_shape=MWAWGraphicShape::circle(shape.m_box);
    break;
  case 6:
    // we need the arc angle, so to create the form
    break;
  case 7: // polygon
  case 8: // spline
  case 9: // polygon smooth
    shape.m_shape.m_bdBox=shape.m_box;
    break;
  default:
    MWAW_DEBUG_MSG(("MacDrawProParser::updateGeometryShape: called with unexpected file type\n"));
    break;
  }
  return true;
}

bool  MacDrawProParser::readTextII(MacDrawProParserInternal::Shape &shape, MWAWEntry const &entry)
{
  if ((shape.m_type!=MacDrawProParserInternal::Shape::Text &&
       shape.m_type!=MacDrawProParserInternal::Shape::Note) || version()>0 || entry.length()<28) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readTextII: the entry seems bad\n"));
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

  val=(int) input->readLong(2); // always 0?
  if (val) f << "f1=" << val << ",";
  val=(int) input->readLong(2);
  if (val>=8) {
    shape.m_textZoneId=(val-8)/4;
    f << "objText[id]=" << (val-8)/4 << ",";
    if (val&3)
      f << "objText[low]=" << (val&3) << ",";
  }
  else {
    MWAW_DEBUG_MSG(("MacDrawProParser::readTextII: find unexpected text position\n"));
    f << "###objText[pos]=" << val << ",";
  }

  std::string extra("");
  if (!readRotationInObjectData(shape, entry.end(), extra)) {
    f << "###rot,";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
    return false;
  }
  f << extra;
  int remain=int(entry.end()-input->tell());
  bool isNote=shape.m_type==MacDrawProParserInternal::Shape::Note;
  int headerSize=20;
  if (isNote) headerSize+=68;
  if (remain<headerSize) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readTextII: the text zone seems too short\n"));
    return false;
  }
  val=(int) input->readLong(1);
  if (val) f << "f0=" << val << ","; // -1|0|1
  val=(int) input->readLong(1);
  MWAWParagraph paragraph;
  switch (val) {
  case -1:
    paragraph.setInterline(2, librevenge::RVNG_PERCENT);
    f << "interline=200%,";
    break;
  case 0: // normal
    break;
  default:
    if (val>0) {
      paragraph.setInterline(val, librevenge::RVNG_POINT, MWAWParagraph::AtLeast);
      f << "interline=" << val << "pt,";
    }
    else // maybe percent
      f << "#interline=" << val << ",";
  }
  val=(int) input->readLong(1);
  if (val) f << "f1=" << val << ","; // 0|-1
  val=(int) input->readLong(1);
  switch (val) {
  case 0: // left
    break;
  case -1:
    paragraph.m_justify = MWAWParagraph::JustificationRight;
    f << "right,";
    break;
  case 1:
    paragraph.m_justify = MWAWParagraph::JustificationCenter;
    f << "center,";
    break;
  case 2:
    paragraph.m_justify = MWAWParagraph::JustificationFull;
    f << "justified,";
    break;
  default:
    f << "#align=" << val << ",";
    break;
  }
  shape.m_paragraph=paragraph;
  shape.m_numChars=(int) input->readULong(2);
  if (shape.m_numChars) f << "nChar=" << shape.m_numChars << ",";
  int N[2];
  for (int i=0; i<2; ++i) N[i]=(int) input->readULong(2);
  f << "N=[" << N[0] << "," << N[1] << "],";
  if (remain<headerSize+6*N[0]+4*N[1]) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readTextII: can not read the number of data\n"));
    f << "###,";
    return false;
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
      MWAW_DEBUG_MSG(("MacDrawProParser::readTextII: the note size seems to big\n"));
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
  }
  f << "lineBreak=[";
  for (int i=0; i<=N[0]; ++i) {
    val=(int) input->readULong(2);
    // do not store the first line: pos 0 and the last line numChars
    if (val && val< shape.m_numChars)
      shape.m_lineBreakSet.insert(val);
    f << val << ",";
  }
  f << "],";
  f << "font=[";
  for (int i=0; i<=N[1]; ++i) {
    int cPos=(int) input->readULong(2);
    val=(int) input->readLong(2);
    shape.m_fontMap[cPos]=val;
    f << cPos << ":F" << val+1 << ",";
  }
  f << "],";
  f << "line[h,?]=[";
  for (int i=0; i<=N[0]; ++i)
    f << input->readULong(2) << ":" << input->readULong(2) << ",";
  f << "],";

  if (input->tell()+4<=entry.end()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readTextII: find unexpected data\n"));
    f << "#remain,";
  }
  if (input->tell()!=entry.end()) {
    ascii().addDelimiter(input->tell(),'|');
    input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  }
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  return true;
}

bool  MacDrawProParser::readTextPro(MacDrawProParserInternal::Shape &shape, MWAWEntry const &entry)
{
  if ((shape.m_type!=MacDrawProParserInternal::Shape::Text &&
       shape.m_type!=MacDrawProParserInternal::Shape::Note) || entry.length()<28 || version()==0) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readTextPro: the entry seems bad\n"));
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

  val=(int) input->readLong(2); // always 0?
  if (val) f << "f1=" << val << ",";
  val=(int) input->readLong(2);
  if (val>=8) {
    shape.m_textZoneId=(val-8)/4;
    f << "objText[id]=" << (val-8)/4 << ",";
    if (val&3)
      f << "objText[low]=" << (val&3) << ",";
  }
  else {
    MWAW_DEBUG_MSG(("MacDrawProParser::readTextPro: find unexpected text position\n"));
    f << "###objText[pos]=" << val << ",";
  }

  std::string extra("");
  if (!readRotationInObjectData(shape, entry.end(), extra)) {
    f << "###rot,";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
    return false;
  }
  f << extra;
  int remain=int(entry.end()-input->tell());
  bool isNote=shape.m_type==MacDrawProParserInternal::Shape::Note;
  int headerSize= 16;
  if (isNote) headerSize+=20;
  if (remain<headerSize) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readTextPro: the text zone seems too short\n"));
    return false;
  }
  shape.m_numChars=(int) input->readULong(4);
  if (shape.m_numChars) f << "nChar=" << shape.m_numChars << ",";
  int N[2];
  for (int i=0; i<2; ++i) N[i]=(int) input->readULong(4);
  f << "N=[" << N[0] << "," << N[1] << "],";
  if (remain<headerSize+26*(N[0]+1)+4*(N[1]+1) && N[0]==1)
    N[0]=-1;
  if (remain<headerSize+26*(N[0]+1)+4*(N[1]+1)) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readTextPro: can not read the number of data\n"));
    f << "###remain,";
    return false;
  }
  int numVal=isNote ? 12 : 2;
  for (int i=0; i<numVal; ++i) {
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  f << "mod=[";
  for (int i=0; i<=N[1]; ++i) {
    int cPos=(int) input->readULong(2);
    f << cPos;
    val=(int) input->readULong(2);
    if (val==0xFFFF) // end
      f << ":_";
    else if (val&0x8000) {
      shape.m_paragraphMap[cPos]=(val&0x7FFF);
      f << ":P" << (val&0x7FFF) << ",";
    }
    else {
      shape.m_fontMap[cPos]=val;
      f << ":F" << val+1 << ",";
    }
  }
  f << "],";
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  for (int i=0; i<=N[0]; ++i) {
    long pos=input->tell();
    int cPos=(int) input->readULong(2);
    // do not store the first line: pos 0 and the last line numChars
    if (cPos && cPos< shape.m_numChars)
      shape.m_lineBreakSet.insert(cPos);

    f.str("");
    f << "ObjData[text-P" << i << "]:pos=" << cPos << ":";
    for (int j=0; j<5; ++j) {
      val=(int) input->readULong(2);
      if (val)
        f << "f" << j << "=" << val << ",";
    }
    float dim[2];
    for (int j=0; j<2; ++j) dim[j]=float(input->readLong(4))/65536.f;
    if (dim[0]>0 || dim[0]<0 || dim[1]>0 || dim[1]<0)
      f << "dim?=" << Vec2f(dim[0],dim[1]) << ",";
    val=(int) input->readULong(2);
    if (val) f << "f5=" << val << ",";
    f << "y=" << float(input->readLong(4))/65536.f << ",";
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  if (input->tell()+4<=entry.end()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readTextPro: find unexpected data\n"));
    ascii().addPos(input->tell());
    ascii().addNote("ObjData[text]:#remain");
  }
  else if (input->tell()!=entry.end()) {
    ascii().addPos(input->tell());
    ascii().addNote("ObjData[text]:_");
  }
  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  return true;
}

bool MacDrawProParser::readGeometryShapeData(MacDrawProParserInternal::Shape &shape, MWAWEntry const &entry)
{
  int const vers=version();
  if (shape.m_type!=MacDrawProParserInternal::Shape::Basic || entry.length()<8) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readGeometryShapeData: the entry seems bad\n"));
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

  std::string extra("");
  if (!readRotationInObjectData(shape, entry.end(), extra)) {
    f << "###rot,";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
    return false;
  }
  f << extra;

  long remain=entry.end()-input->tell();
  switch (shape.m_fileType) {
  case 2: { // line
    if (fl & 0xF8) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readGeometryShapeData: find unknown line flag\n"));
      f << "#line[flag]=" << (fl&0xF8) << ",";
    }
    if (input->tell()+20>entry.end()) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readGeometryShapeData: can not read the line header\n"));
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
      shape.m_style.m_arrows[1-i]=true;
      if (input->tell()+24>entry.end()) {
        MWAW_DEBUG_MSG(("MacDrawProParser::readGeometryShapeData: can not read the line arrow %d\n", i));
        f << "###";
        ok=false;
        break;
      }
      f << "arrow[" << (i==1 ? "beg" : "end") << "]=[";
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
      MWAW_DEBUG_MSG(("MacDrawProParser::readGeometryShapeData: can not read the line label\n"));
      f << "###";
      break;
    }
    float dim[4];
    for (int i=0; i<4; ++i) dim[i]=float(input->readLong(4))/65536.f;
    shape.m_labelBox=Box2f(Vec2f(dim[1],dim[0]), Vec2f(dim[3],dim[2]));
    f << "dim=" << shape.m_labelBox << ",";
    val=(int) input->readLong(2); // 0 or 4
    if (val) f<< "f0=" << val << ",";
    int sSz=(int) input->readULong(1);
    if (sSz>27) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readGeometryShapeData: can not read the line label size\n"));
      f << "###";
      break;
    }
    shape.m_labelEntry.setBegin(input->tell());
    shape.m_labelEntry.setLength(sSz);
    std::string label("");
    for (int i=0; i<sSz; ++i) label+=(char) input->readULong(1);
    f << label;
    input->seek(endLabelPos, librevenge::RVNG_SEEK_SET);
    break;
  }
  case 3: // rect
  case 4: { // rectOval
    if (!remain)
      break;
    if (shape.m_fileType!=4 || remain!=128) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readGeometryShapeData: can not read the round rect representation\n"));
      f << "###";
      break;
    }
    f << "pts=[";
    for (int i=0; i<16; ++i) {
      float pt[2];
      for (int j=0; j<2; ++j) pt[j]=float(input->readLong(4))/65536.f;
      f << Vec2f(pt[1],pt[0]) << ",";
    }
    f << "],";
    break;
  }
  case 5: { // circle
    if (!remain)
      break;
    if (remain!=64) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readGeometryShapeData: can not read the circle representation\n"));
      f << "###";
      break;
    }
    f << "pts=[";
    for (int i=0; i<8; ++i) {
      float pt[2];
      for (int j=0; j<2; ++j) pt[j]=float(input->readLong(4))/65536.f;
      f << Vec2f(pt[1],pt[0]) << ",";
    }
    f << "],";
    break;
  }
  case 6: { // arc
    if (remain < 8 || (remain%8))
      break;
    float fileAngle[2];
    for (int i=0; i<2; ++i) fileAngle[i]=float(input->readLong(4))/65536.f;
    f << "angle=" << fileAngle[0] << "->" << fileAngle[0]+fileAngle[1] << ",";
    float angle[2] = { 90.f-fileAngle[0]-fileAngle[1], 90.f-fileAngle[0] };
    if (fileAngle[1]<0) {
      angle[0]=90-fileAngle[0];
      angle[1]=90-fileAngle[0]-fileAngle[1];
    }
    while (angle[1] > 360) {
      angle[0]-=360;
      angle[1]-=360;
    }
    while (angle[0] < -360) {
      angle[0]+=360;
      angle[1]+=360;
    }

    Box2f box=shape.m_box;
    // we must compute the real bd box
    float minVal[2] = { 0, 0 }, maxVal[2] = { 0, 0 };
    int limitAngle[2];
    for (int i = 0; i < 2; i++)
      limitAngle[i] = (angle[i] < 0) ? int(angle[i]/90)-1 : int(angle[i]/90);
    for (int bord = limitAngle[0]; bord <= limitAngle[1]+1; bord++) {
      float ang = (bord == limitAngle[0]) ? float(angle[0]) :
                  (bord == limitAngle[1]+1) ? float(angle[1]) : float(90 * bord);
      ang *= float(M_PI/180.);
      float actVal[2] = { std::cos(ang), -std::sin(ang)};
      if (actVal[0] < minVal[0]) minVal[0] = actVal[0];
      else if (actVal[0] > maxVal[0]) maxVal[0] = actVal[0];
      if (actVal[1] < minVal[1]) minVal[1] = actVal[1];
      else if (actVal[1] > maxVal[1]) maxVal[1] = actVal[1];
    }
    Box2f circleBox=box;
    // we have the shape box, we need to reconstruct the circle box
    if (maxVal[0]>minVal[0] && maxVal[1]>minVal[1]) {
      float scaling[2]= { (box[1][0]-box[0][0])/(maxVal[0]-minVal[0]),
                          (box[1][1]-box[0][1])/(maxVal[1]-minVal[1])
                        };
      float constant[2]= { box[0][0]-minVal[0] *scaling[0], box[0][1]-minVal[1] *scaling[1]};
      circleBox=Box2f(Vec2f(constant[0]-scaling[0], constant[1]-scaling[1]),
                      Vec2f(constant[0]+scaling[0], constant[1]+scaling[1]));
    }
    if (shape.m_style.hasSurface())
      shape.m_shape = MWAWGraphicShape::pie(box, circleBox, Vec2f(float(angle[0]),float(angle[1])));
    else
      shape.m_shape = MWAWGraphicShape::arc(box, circleBox, Vec2f(float(angle[0]),float(angle[1])));

    int N=int(remain/8)-1;
    f << "N=" << N << ",pts?=["; // a smooth polygon
    for (int i=0; i<N; ++i) {
      float pt[2];
      for (int j=0; j<2; ++j) pt[j]=float(input->readLong(4))/65536.f;
      f << Vec2f(pt[1],pt[0]) << ",";
    }
    f << "],";
    break;
  }
  case 7: // polygon[smooth]
  case 8:  // polygon
    if (vers==0) {
      if (remain<=0 || (remain%8)) {
        MWAW_DEBUG_MSG(("MacDrawProParser::readGeometryShapeData: can not read compute the number of point in polygon\n"));
        f << "###";
        break;
      }
      shape.m_shape.m_bdBox=shape.m_box;
      int N=int(remain/8);
      f << "N=" << N << ",pts=[";
      std::vector<Vec2f> listVertices;
      Vec2f origin=shape.m_box[0];
      for (int i=0; i<N; ++i) {
        float pt[2];
        for (int j=0; j<2; ++j) pt[j]=float(input->readLong(4))/65536.f;
        Vec2f point(pt[1],pt[0]);
        listVertices.push_back(point+origin);
        f << point << ",";
      }
      f << "],";
      if (shape.m_fileType==8) {
        shape.m_shape.m_vertices=listVertices;
        break;
      }
      // try to smooth the curve
      shape.m_shape.m_type=MWAWGraphicShape::Path;
      shape.m_shape.m_path.push_back(MWAWGraphicShape::PathData('M', listVertices[0]));
      for (size_t i=1; i+1<listVertices.size(); ++i) {
        Vec2f dir=listVertices[i+1]-listVertices[i-1];
        shape.m_shape.m_path.push_back(MWAWGraphicShape::PathData('S', listVertices[i], listVertices[i]-0.1f*dir));
      }
      if (listVertices.size()>1)
        shape.m_shape.m_path.push_back(MWAWGraphicShape::PathData('L', listVertices.back()));
      break;
    }
  // fall through intended
  case 9: { // spline
    if (remain<=2) {
      MWAW_DEBUG_MSG(("MacDrawProParser::readGeometryShapeData: can not read compute the number of point in spline\n"));
      f << "###";
      break;
    }
    int N=(int) input->readULong(2);
    f << "N=" << N << ",";

    f << "pts=[";
    Vec2f origin=shape.m_box[0], prevPoint;
    bool hasPrevPoint=false, isSpline=false;
    std::vector<Vec2f> listVertices;
    std::vector<MWAWGraphicShape::PathData> path;
    for (int i=0; i<N; ++i) {
      long pos=input->tell();
      int type=(int) input->readULong(2);
      int nCoord=0;
      if (type>=0 && type<=9) {
        static int const(numCoord[])= {3, 3, 3, 3, 0/*4*/, 0/*5*/, 3, 3, 1, 1};
        nCoord=numCoord[type];
      }
      if (nCoord<=0 || pos+2+8*nCoord>entry.end()) {
        MWAW_DEBUG_MSG(("MacDrawProParser::readGeometryShapeData: can not determine the number of coordinate\n"));
        f << "###type=" << type << ",";
        break;
      }
      Vec2f points[3];
      for (int j=0; j<nCoord; ++j) {
        float pPos[2];
        for (int k=0; k<2; ++k) pPos[k]=float(input->readLong(4))/65536;
        points[j]=Vec2f(pPos[1],pPos[0]);
        f << points[j] << (j==nCoord-1 ? "," : ":");
        points[j]+=origin;
      }
      if (nCoord==1)
        listVertices.push_back(points[0]);
      else
        isSpline=true;
      Vec2f pt=nCoord==1 ? points[0] : points[1];
      char pType = hasPrevPoint ? 'C' : i==0 ? 'M' : nCoord==1 ? 'L' : 'S';
      path.push_back(MWAWGraphicShape::PathData(pType, pt, hasPrevPoint ? prevPoint : points[0], points[0]));
      hasPrevPoint=nCoord==3;
      if (hasPrevPoint) prevPoint=points[2];
    }
    f << "],";
    if (isSpline) {
      shape.m_shape.m_type=MWAWGraphicShape::Path;
      shape.m_shape.m_path=path;
    }
    else
      shape.m_shape.m_vertices=listVertices;
    break;
  }
  default:
    if (!remain) break;
    MWAW_DEBUG_MSG(("MacDrawProParser::readGeometryShapeData: called with unexpected file type\n"));
    break;
  }
  if (fl) f << "fl=" << std::hex << fl << std::dec << ",";
  if (input->tell()!=entry.end()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readGeometryShapeData: find unexpected data\n"));
    f << "###";
    ascii().addDelimiter(input->tell(),'|');
  }
  // if there is a rotation, we need to recompute the rect, ..., arc shape
  if (shape.m_fileType>=3 && shape.m_fileType<=6 && (shape.m_flags & 0x80))
    shape.m_shape = shape.m_shape.rotate(shape.m_style.m_rotate, shape.m_box.center());
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
  int const vers=version();
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

  std::string extra("");
  if (!readRotationInObjectData(shape, entry.end(), extra)) {
    f << "###rot,";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
    return false;
  }
  f << extra;
  int dim[4];
  for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
  Box2i &bitmapBox=shape.m_bitmapDim;
  bitmapBox=Box2i(Vec2i(dim[1],dim[0]), Vec2i(dim[3],dim[2]));
  f << "dim=" << bitmapBox << ",";
  f << "id?=" << std::hex << input->readULong(4) << std::dec << ",";
  shape.m_numBytesByRow=(int) input->readULong(2);
  if (vers>0 && shape.m_numBytesByRow&0x8000) {
    f << "color,";
    shape.m_bitmapIsColor=true;
    shape.m_numBytesByRow&=0x7FFF;
  }
  f << "rowSize=" << shape.m_numBytesByRow << ",";
  Box2i &fileBox=shape.m_bitmapFileDim;
  for (int i=0; i<4; ++i) dim[i]=(int) input->readLong(2);
  fileBox=Box2i(Vec2i(dim[1],dim[0]), Vec2i(dim[3],dim[2]));
  if (bitmapBox!=fileBox)
    f << "bitmap[dimInFile]="<< fileBox << ",";
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  if (vers) {
    long pos=input->tell();
    f.str("");
    f << "Bitmap-A:";
    for (int i=0; i<14; ++i) { // BW: all 0, color f4=f6=48, f9=f11=8, f10=1
      val=(int) input->readLong(2);
      if (val) f << "f" << i << "=" << val << ",";
    }
    val=(int) input->readULong(4); // big number in color : an unit ?
    if (val) f << "f15=" << std::hex << val << std::dec << ",";
    for (int i=0; i<2; ++i) {  // always 0
      val=(int) input->readLong(2);
      if (val) f << "g" << i << "=" << val << ",";
    }
    shape.m_bitmapClutId=(int) input->readLong(2);
    if (shape.m_bitmapClutId) f << "clut[id]=" << shape.m_bitmapClutId << ",";
    for (int i=0; i<2; ++i) {  // always 0
      val=(int) input->readLong(2);
      if (val) f << "g" << i+2 << "=" << val << ",";
    }
    ascii().addPos(pos);
    ascii().addNote(f.str().c_str());
  }
  shape.m_bitmapEntry.setBegin(input->tell());
  shape.m_bitmapEntry.setLength(fileBox.size()[1]*shape.m_numBytesByRow);

  if (fileBox.size()[1]<0 || shape.m_numBytesByRow<0 || !input->checkPosition(shape.m_bitmapEntry.end())) {
    MWAW_DEBUG_MSG(("MacDrawProParser::readBitmap: can not compute the bitmap endPos\n"));
    ascii().addPos(shape.m_bitmapEntry.begin());
    ascii().addNote("Bitmap[data]:###");
    return false;
  }
  int const numBytesByPixel=shape.m_bitmapIsColor ? 1 : 8;
  if (shape.m_numBytesByRow*numBytesByPixel < fileBox.size()[0] ||
      fileBox[0][0]>bitmapBox[0][0] || fileBox[0][1]>bitmapBox[0][1] ||
      fileBox[1][0]<bitmapBox[1][0] || fileBox[1][1]<bitmapBox[1][1]) {
    ascii().addPos(shape.m_bitmapEntry.begin());
    ascii().addNote("Bitmap[data]:###");
    MWAW_DEBUG_MSG(("MacDrawProParser::readBitmap: something look bad when reading a bitmap header\n"));
    shape.m_bitmapEntry=MWAWEntry();
  }
  else
    ascii().skipZone(shape.m_bitmapEntry.begin(), shape.m_bitmapEntry.end()-1);
  if (shape.m_bitmapEntry.end()+4<entry.end()) {
    ascii().addPos(shape.m_bitmapEntry.end());
    ascii().addNote("Bitmap[end]:###");
    MWAW_DEBUG_MSG(("MacDrawProParser::readBitmap: the bitmap data zone seems too big\n"));
  }
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
  if (val==0x4452) { // MacDraw II
    if (input->readULong(2)!=0x5747) return false;
  }
  else if (val==0x5354) { // MacDraw II template
    m_state->m_isStationery=true;
    f << "stationery,";
    if (input->readULong(2)!=0x4154) return false;
  }
  else if (val==0x6444) { // MacDraw Pro
    if (input->readULong(2)!=0x6f63) return false;
    f << "pro,";
    vers=1;
  }
  else if (val==0x644c) { // MacDraw Pro template
    m_state->m_isStationery=true;
    if (input->readULong(2)!=0x6962) return false;
    f << "pro,";
    vers=1;
  }
  else return false;

#ifndef DEBUG
  if (vers==1 && !input->hasResourceFork()) {
    // as in MacDraw pro, the color, patterns, gradients are stored in the resource fork, ...
    MWAW_DEBUG_MSG(("MacDrawProParser::checkHeader: impossible to read a MacDraw Pro files without resource fork\n"));
    return false;
  }
#endif
  val=(int) input->readULong(2);
  if (val==0) {
    f << "D2[not],";
    vers=0;
  }
  else if (val!=0x4432) {
    MWAW_DEBUG_MSG(("MacDrawProParser::checkHeader: find unexpected header\n"));
    return false;
  }
  f << "version=" << vers << ",";
  f << "subVersion=" << input->readLong(2) << ",";
  ascii().addPos(0);
  ascii().addNote(f.str().c_str());

  if (strict) {
    // try to begin the parsing
    if (!readHeaderInfo() || !m_styleManager->readStyles(m_state->m_sizeStyleZones)) return false;
    m_styleManager.reset(new MacDrawProStyleManager(*this));
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
  int const vers=version();
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
  int const headerStyleSize=vers==0 ? 24 : 12;
  if (!m_styleManager->readHeaderInfoStylePart(extra)) {
    f << "###Style[ignored],";
    input->seek(pos+4+headerStyleSize, librevenge::RVNG_SEEK_SET);
  }
  else
    f << extra;

  m_state->m_actualLayer=(int) input->readULong(2);
  if (m_state->m_actualLayer!=1) f << "layer[cur]=" << m_state->m_actualLayer << ",";
  val=(int) input->readULong(2); // h1=1|2|40
  if (val!=1) f << "h1=" << val << ",";
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

  input->seek(pos+4+headerStyleSize+12, librevenge::RVNG_SEEK_SET);
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
  int const numSizeZones=vers==0 ? 5 : 6;
  for (int i=0; i< numSizeZones; ++i) {
    m_state->m_sizeStyleZones[i]=(long) input->readULong(4);
    if (!m_state->m_sizeStyleZones[i]) continue;
    f << "sz[style" << i << "]=" << std::hex << m_state->m_sizeStyleZones[i] << std::dec << ",";
  }
  if (vers==0) {
    for (int i=0;  i< 5; ++i) {
      long lVal=(long) input->readULong(4);
      if (!lVal) continue;
      MWAW_DEBUG_MSG(("MacDrawProParser::readHeaderInfo: find some unexpected value in C zone, we may have a problem\n"));
      f << "##f" << i << "=" << std::hex << lVal << std::dec << ",";
    }
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
  if (vers==0) {
    for (int i=0; i<3; ++i) { // always 0?
      long lVal=(long) input->readULong(4);
      if (!lVal) continue;
      MWAW_DEBUG_MSG(("MacDrawProParser::readHeaderInfo: find some unexpected value in C(II) zone, we may have a problem\n"));
      f << "##f" << i << "=" << lVal << ",";
    }
  }
  else {
    f << "id?=" << std::hex << input->readULong(4) << std::dec << ",";
    for (int i=0; i<2; ++i) { // always 0, 1?
      val=(int) input->readLong(2);
      if (val!=i)
        f << "f" << i << "=" << val << ",";
    }
  }

  input->seek(vers==1 ? 0x1d4 : 0x1f4, librevenge::RVNG_SEEK_SET);
  ascii().addPos(pos);
  ascii().addNote(f.str().c_str());

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

bool MacDrawProParser::sendMasterPage()
{
  if (!m_state->m_createMasterPage)
    return true;
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MacDrawProParser::sendMasterPage: can not find the listener\n"));
    m_state->m_createMasterPage=false;
    return false;
  }
  MWAWPageSpan ps(getPageSpan());
  ps.setMasterPageName("Master");
  if (!listener->openMasterPage(ps)) {
    MWAW_DEBUG_MSG(("MacDrawProParser::sendMasterPage: can not create the master page\n"));
    m_state->m_createMasterPage=false;
    return false;
  }
  for (size_t i=0; i<m_state->m_layerList.size(); ++i) {
    MacDrawProParserInternal::Layer const &layer=m_state->m_layerList[i];
    if (layer.m_isHidden)
      continue;
    send(layer);
  }
  listener->closeMasterPage();
  return true;
}

bool MacDrawProParser::sendPage(int page)
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MacDrawProParser::sendPage: can not find the listener\n"));
    return false;
  }
  if (page>0)
    listener->insertBreak(MWAWListener::PageBreak);
  if (m_state->m_sendAsLibraries) {
    if (page<0 || page>=(int)m_state->m_libraryList.size()) {
      MWAW_DEBUG_MSG(("MacDrawProParser::sendPage: can not find a library\n"));
      return false;
    }
    send(m_state->m_libraryList[size_t(page)]);
    return true;
  }
  int actHidden=0;
  for (size_t i=0; i<m_state->m_layerList.size(); ++i) {
    MacDrawProParserInternal::Layer const &layer=m_state->m_layerList[i];
    if (layer.m_isHidden && actHidden++!=page)
      continue;
    if (!layer.m_isHidden && m_state->m_createMasterPage)
      continue;
    send(layer);
  }
  return true;
}

bool MacDrawProParser::send(MacDrawProParserInternal::Library const &library)
{
  int numLayers=(int) m_state->m_layerList.size();
  Vec2f leftTop(72.f*float(getPageSpan().getMarginLeft()),72.f*float(getPageSpan().getMarginTop()));
  for (size_t i=0; i<library.m_layerList.size(); ++i) {
    int id=library.m_layerList[i];
    if (id<0 || id>=numLayers) continue;
    MacDrawProParserInternal::Layer const &layer=m_state->m_layerList[size_t(id)];
    if (layer.m_libraryToObjectMap.find(library.m_id)!=layer.m_libraryToObjectMap.end()) {
      int objId=layer.m_firstShape+layer.m_libraryToObjectMap.find(library.m_id)->second;
      if (objId>0 && objId<int(m_state->m_shapeList.size())) {
        MacDrawProParserInternal::Shape const &shape=m_state->m_shapeList[size_t(objId-1)];
        if (shape.m_type==MacDrawProParserInternal::Shape::Group) {
          send(shape, leftTop);
          continue;
        }
      }
    }
    send(layer);
  }
  return true;
}

bool MacDrawProParser::send(MacDrawProParserInternal::Layer const &layer)
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MacDrawProParser::send[layer]: can not find the listener\n"));
    return false;
  }
  if (layer.m_firstShape<0)
    return true;
  int maxShape=(int) m_state->m_shapeList.size();
  if (layer.m_firstShape+layer.m_numShapes<maxShape)
    maxShape=layer.m_firstShape+layer.m_numShapes;
  if (maxShape<=layer.m_firstShape)
    return false;
  bool openLayer=false;
  if (!layer.m_name.empty())
    openLayer=listener->openLayer(layer.m_name);
  Vec2f leftTop(72.f*float(getPageSpan().getMarginLeft()),72.f*float(getPageSpan().getMarginTop()));
  for (int i=layer.m_firstShape; i<maxShape; ++i) {
    MacDrawProParserInternal::Shape const &shape=m_state->m_shapeList[size_t(i)];
    send(shape, leftTop);
    if (shape.m_nextId>i+1)
      i+=shape.m_nextId-(i+1);
  }
  if (openLayer)
    listener->closeLayer();
  return true;
}

bool MacDrawProParser::send(MacDrawProParserInternal::Shape const &shape, Vec2f const &orig)
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MacDrawProParser::send[shape]: can not find the listener\n"));
    return false;
  }
  shape.m_isSent=true;
  // for not basic shape, we need the box before the rotation, so compute the box by hand
  Box2f box=(shape.m_type==MacDrawProParserInternal::Shape::Basic) ? shape.m_shape.getBdBox() : shape.m_box;
  box=Box2f(box[0]+orig, box[1]+orig);
  MWAWPosition pos(box[0], box.size(), librevenge::RVNG_POINT);
  pos.m_anchorTo = MWAWPosition::Page;
  switch (shape.m_type) {
  case MacDrawProParserInternal::Shape::Basic:
    listener->insertPicture(pos, shape.m_shape, shape.m_style);
    if (shape.m_shape.m_type==MWAWGraphicShape::Line && shape.m_labelEntry.valid()) {
      MWAWGraphicStyle style;
      style.m_lineWidth=0;
      style.setSurfaceColor(MWAWColor::white());
      shared_ptr<MWAWSubDocument> doc(new MacDrawProParserInternal::SubDocument(*this, getInput(), shape.m_labelEntry));
      MWAWPosition labelPos(box[0]+shape.m_labelBox[0], shape.m_labelBox.size(), librevenge::RVNG_POINT);
      labelPos.m_anchorTo = MWAWPosition::Page;
      listener->insertTextBox(labelPos, doc, style);
    }
    break;
  case MacDrawProParserInternal::Shape::Bitmap:
    return sendBitmap(shape, pos);
  case MacDrawProParserInternal::Shape::Group: {
    size_t numShapes=m_state->m_shapeList.size();
    if (!numShapes) break;
    listener->openGroup(pos);
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
      send(child, orig);
    }
    listener->closeGroup();
    break;
  }
  case MacDrawProParserInternal::Shape::GroupEnd:
    break;
  case MacDrawProParserInternal::Shape::Note: {
    MWAWGraphicStyle style=shape.m_style;
    style.m_lineWidth=1;

    MWAWBorder border;
    border.m_color=MWAWColor::black();
    border.m_width=1;
    style.setBorders(libmwaw::LeftBit|libmwaw::BottomBit|libmwaw::RightBit, border);
    border.m_color=MWAWColor(0x60,0x60,0); // normally pattern of yellow and black
    border.m_width=20;
    style.setBorders(libmwaw::TopBit, border);

    style.setSurfaceColor(MWAWColor(0xff,0xff,0));
    style.m_shadowOffset=Vec2i(3,3);
    style.setShadowColor(MWAWColor(0x80,0x80,0x80));

    shared_ptr<MWAWSubDocument> doc(new MacDrawProParserInternal::SubDocument(*this, getInput(), shape.m_id));
    listener->insertTextBox(pos, doc, style);
    return true;
  }
  case MacDrawProParserInternal::Shape::Text: {
    shared_ptr<MWAWSubDocument> doc(new MacDrawProParserInternal::SubDocument(*this, getInput(), shape.m_id));
    listener->insertTextBox(pos, doc, shape.m_style);
    return true;
  }
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
  int const numPixelByBytes=shape.m_bitmapIsColor ? 1 : 8;
  Vec2i pictDim=shape.m_bitmapDim.size();
  if (shape.m_type!=MacDrawProParserInternal::Shape::Bitmap || numBytesByRow<=0 ||
      pictDim[0]<0 || pictDim[1]<0 ||
      numBytesByRow*shape.m_bitmapFileDim.size()[1]<shape.m_bitmapEntry.length() ||
      shape.m_bitmapDim[0][0]<0 || shape.m_bitmapDim[0][1]<0 ||
      shape.m_bitmapDim[0][0]<shape.m_bitmapFileDim[0][0] ||
      shape.m_bitmapFileDim.size()[0]<=0 || shape.m_bitmapFileDim.size()[1]<=0 ||
      numPixelByBytes*numBytesByRow<shape.m_bitmapFileDim.size()[0]) {
    MWAW_DEBUG_MSG(("MacDrawProParser::sendBitmap: the bitmap seems bad\n"));
    return false;
  }
  shared_ptr<MWAWPictBitmap> pict;
  if (shape.m_bitmapIsColor) {
    MWAWRSRCParserPtr rsrcParser=getRSRCParser();
    if (!rsrcParser) {
      MWAW_DEBUG_MSG(("MacDrawProParser::sendBitmap: need access to resource fork to read colormap\n"));
      return false;
    }

    std::multimap<std::string, MWAWEntry> &entryMap = rsrcParser->getEntriesMap();
    std::multimap<std::string, MWAWEntry>::iterator it=entryMap.find("clut");
    MWAWEntry entry;
    while (it!=entryMap.end() && it->first=="clut") {
      if (it->second.id()==shape.m_bitmapClutId) {
        entry=it->second;
        break;
      }
      ++it;
    }

    std::vector<MWAWColor> colList;
    if (!entry.valid() || !rsrcParser->parseClut(entry, colList) || colList.empty()) {
      MWAW_DEBUG_MSG(("MacDrawProParser::sendBitmap: can not read the color list\n"));
      return false;
    }

    MWAWPictBitmapIndexed *pictIndexed=new MWAWPictBitmapIndexed(pictDim);
    pict.reset(pictIndexed);
    pictIndexed->setColors(colList);
    std::vector<int> data;
    data.resize(size_t(pictDim[0]), 0);

    MWAWInputStreamPtr input=getInput();
    input->seek(shape.m_bitmapEntry.begin(), librevenge::RVNG_SEEK_SET);
    int numCol=int(colList.size());
    for (int r=shape.m_bitmapFileDim[0][1]; r<shape.m_bitmapFileDim[1][1]; ++r) {
      long pos=input->tell();
      if (r<shape.m_bitmapDim[0][1]||r>=shape.m_bitmapDim[1][1]) { // must not appear, but...
        input->seek(pos+numBytesByRow, librevenge::RVNG_SEEK_SET);
        continue;
      }
      int wPos=shape.m_bitmapDim[0][0]-shape.m_bitmapFileDim[0][0];
      for (int col=shape.m_bitmapFileDim[0][0]; col<shape.m_bitmapFileDim[1][0]; ++col) {
        int c=(int) input->readULong(1);
        if (wPos>=pictDim[0]) break;
        if (c>=numCol) {
          MWAW_DEBUG_MSG(("MacDrawProParser::sendBitmap: find some eroneous index, reset to 0\n"));
          c=0;
        }
        data[size_t(wPos++)]=c;
      }
      pictIndexed->setRow(r-shape.m_bitmapDim[0][1], &data[0]);
      input->seek(pos+numBytesByRow, librevenge::RVNG_SEEK_SET);
    }
  }
  else {
    // change: implement indexed transparent color, replaced this code
    MWAWPictBitmapColor *pictColor=new MWAWPictBitmapColor(pictDim, true);
    pict.reset(pictColor);
    MWAWColor transparent(255,255,255,0);
    MWAWColor black(MWAWColor::black());
    std::vector<MWAWColor> data;
    data.resize(size_t(pictDim[0]), transparent);
    // first set unseen row to zero (even if this must not appear)
    for (int r=shape.m_bitmapDim[0][1]; r<shape.m_bitmapFileDim[0][1]; ++r) pictColor->setRow(r-shape.m_bitmapDim[0][1], &data[0]);
    for (int r=shape.m_bitmapFileDim[1][1]; r<shape.m_bitmapDim[1][1]; ++r) pictColor->setRow(r-shape.m_bitmapDim[0][1], &data[0]);

    MWAWInputStreamPtr input=getInput();
    input->seek(shape.m_bitmapEntry.begin(), librevenge::RVNG_SEEK_SET);
    for (int r=shape.m_bitmapFileDim[0][1]; r<shape.m_bitmapFileDim[1][1]; ++r) {
      long pos=input->tell();
      if (r<shape.m_bitmapDim[0][1]||r>=shape.m_bitmapDim[1][1]) { // must not appear, but...
        input->seek(pos+numBytesByRow, librevenge::RVNG_SEEK_SET);
        continue;
      }
      int wPos=shape.m_bitmapDim[0][0]-shape.m_bitmapFileDim[0][0];
      for (int col=shape.m_bitmapFileDim[0][0]; col<shape.m_bitmapFileDim[1][0]; ++col) {
        unsigned char c=(unsigned char) input->readULong(1);
        for (int j=0, bit=0x80; j<8 ; ++j, bit>>=1) {
          if (wPos>=pictDim[0]) break;
          data[size_t(wPos++)]=(c&bit) ? black : transparent;
        }
      }
      pictColor->setRow(r-shape.m_bitmapDim[0][1], &data[0]);
      input->seek(pos+numBytesByRow, librevenge::RVNG_SEEK_SET);
    }
  }

  librevenge::RVNGBinaryData binary;
  std::string type;
  if (!pict || !pict->getBinary(binary,type)) return false;
#ifdef DEBUG_WITH_FILES
  static int volatile pictName = 0;
  libmwaw::DebugStream f;
  f << "PICT-" << ++pictName << ".bmp";
  libmwaw::Debug::dumpFile(binary, f.str().c_str());
#endif

  // bitmap have no border
  MWAWGraphicStyle style=shape.m_style;
  style.m_lineWidth=0;
  listener->insertPicture(position, binary, type, style);

  return true;
}

bool MacDrawProParser::sendText(int zId)
{
  int const vers=version();
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MacDrawProParser::sendText: can not find the listener\n"));
    return false;
  }
  if (zId<0||zId>=(int) m_state->m_shapeList.size() ||
      (m_state->m_shapeList[size_t(zId)].m_type != MacDrawProParserInternal::Shape::Text &&
       m_state->m_shapeList[size_t(zId)].m_type != MacDrawProParserInternal::Shape::Note)) {
    MWAW_DEBUG_MSG(("MacDrawProParser::sendText: can not find the text shape %d\n", zId));
    return false;
  }
  MacDrawProParserInternal::Shape const &shape=m_state->m_shapeList[size_t(zId)];
  shape.m_isSent = true;
  if (shape.m_textZoneId<0 || shape.m_textZoneId>=int(m_state->m_objectTextList.size())) {
    MWAW_DEBUG_MSG(("MacDrawProParser::sendText: can not find the text zone %d\n", shape.m_textZoneId));
    return false;
  }
  MWAWEntry const &entry=m_state->m_objectTextList[size_t(shape.m_textZoneId)];
  entry.setParsed(true);

  MWAWInputStreamPtr input=getInput();
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Object[text]:shape=[" << shape << "],";
  int numUsed=(int) input->readULong(4);
  if (numUsed!=1) f << "numUsed=" << numUsed << ",";
  // skip size
  input->seek(4, librevenge::RVNG_SEEK_CUR);
  int N=shape.m_numChars;
  if (int(entry.length()-8)<N) {
    MWAW_DEBUG_MSG(("MacDrawProParser::sendText: the zone size seems short\n"));
    N=int(entry.length()-8);
  }
  long endPos=input->tell()+N;
  if (vers==0)
    listener->setParagraph(shape.m_paragraph);
  for (int i=0; i<N; ++i) {
    if (input->isEnd())
      break;
    if (vers>=0 && shape.m_paragraphMap.find(i)!=shape.m_paragraphMap.end()) {
      int id=shape.m_paragraphMap.find(i)->second;
      f << "[P" << id+1 << "]";
      MWAWParagraph para;
      if (id>=0 && !m_styleManager->getParagraph(id, para))
        f << "###";
      listener->setParagraph(para);
    }
    /* note: it is also possible to add a soft linebreak here when the
       previous character is not a EOL. Sometimes this is better but
       other times not, ie. if the line slightly overlaps the right
       border, we will end with some "empty" lines */
    if (shape.m_lineBreakSet.find(i)!=shape.m_lineBreakSet.end())
      f << "[L]";
    if (shape.m_fontMap.find(i)!=shape.m_fontMap.end()) {
      int id=shape.m_fontMap.find(i)->second;
      f << "[F" << id+1 << "]";
      MWAWFont font;
      if (id>=0 && !m_styleManager->getFont(id+1, font))
        f << "###";
      listener->setFont(font);
    }
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
      if (i+1!=N)
        listener->insertEOL();
      break;
    default:
      listener->insertCharacter((unsigned char)c, input, endPos);
      break;
    }
  }
  ascii().addPos(entry.begin());
  ascii().addNote(f.str().c_str());
  return true;
}

bool MacDrawProParser::sendLabel(MWAWEntry const &entry)
{
  MWAWGraphicListenerPtr listener=getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("MacDrawProParser::sendLabel: can not find the listener\n"));
    return false;
  }
  if (!entry.valid()) {
    MWAW_DEBUG_MSG(("MacDrawProParser::sendLabel: can not find the label entry\n"));
    return false;
  }
  MWAWInputStreamPtr input=getInput();
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  long endPos=entry.end();
  for (long i=0; i<entry.length(); ++i) {
    if (input->isEnd()) {
      MWAW_DEBUG_MSG(("MacDrawProParser::sendLabel: the zone seems too short\n"));
      break;
    }
    char c=(char) input->readULong(1);
    if (c==0) {
      MWAW_DEBUG_MSG(("MacDrawProParser::sendText: find char 0\n"));
      continue;
    }
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
  return true;
}

void MacDrawProParser::flushExtra()
{
  Vec2f leftTop(72.f*float(getPageSpan().getMarginLeft()),72.f*float(getPageSpan().getMarginRight()));
  for (size_t i=0; i<m_state->m_shapeList.size(); ++i) {
    MacDrawProParserInternal::Shape const &shape=m_state->m_shapeList[i];
    if (shape.m_isSent || shape.m_type==MacDrawProParserInternal::Shape::GroupEnd)
      continue;
    static bool first=true;
    if (first) {
      MWAW_DEBUG_MSG(("MacDrawProParser::flushExtra: find some unsent zone\n"));
      first=false;
    }
    send(shape, leftTop);
  }
  libmwaw::DebugStream f;
  for (size_t i=0; i<m_state->m_objectTextList.size(); ++i) {
    MWAWEntry const &entry=m_state->m_objectTextList[i];
    if (!entry.valid() || entry.isParsed())
      continue;
    static bool first=false;
    if (first) {
      MWAW_DEBUG_MSG(("MacDrawProParser::flushExtra: find some unparsed text's object zones\n"));
      first=false;
    }
    f.str("");
    f << "Entries(ObjText)[" << i << "]:###unparsed";
    ascii().addPos(entry.begin());
    ascii().addNote(f.str().c_str());
  }
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
