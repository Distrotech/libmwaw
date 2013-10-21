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

#include <libwpd/libwpd.h>

#include "MWAWContentListener.hxx"
#include "MWAWFont.hxx"
#include "MWAWFontConverter.hxx"
#include "MWAWGraphicListener.hxx"
#include "MWAWGraphicShape.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWPictBitmap.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSubDocument.hxx"

#include "CWParser.hxx"
#include "CWStruct.hxx"
#include "CWStyleManager.hxx"

#include "CWGraph.hxx"

#include "libmwaw_internal.hxx"

/** Internal: the structures of a CWGraph */
namespace CWGraphInternal
{
//! Internal: the structure used to a point of a CWGraph
struct CurvePoint {
  CurvePoint(Vec2f point=Vec2f()) : m_pos(point), m_type(1) {
    for (int i = 0; i < 2; i++) m_controlPoints[i] = point;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, CurvePoint const &pt) {
    o << pt.m_pos;
    if (pt.m_pos != pt.m_controlPoints[0])
      o << ":prev=" << pt.m_controlPoints[0];
    if (pt.m_pos != pt.m_controlPoints[1])
      o << ":next=" << pt.m_controlPoints[1];
    switch(pt.m_type) {
    case 0:
      o << ":point2";
      break;
    case 1:
      break;
    case 2:
      o << ":spline";
      break;
    case 3:
      o << ":spline2";
      break;
    default:
      o << ":#type=" << pt.m_type;
    }
    return o;
  }
  //! the main position
  Vec2f m_pos;
  //! the control point: previous, next
  Vec2f m_controlPoints[2];
  //! the point type
  int m_type;
};

//! Internal: the structure used to store a style of a CWGraph
struct Style : public MWAWGraphicStyle {
  //! constructor
  Style(): MWAWGraphicStyle(), m_id(-1), m_wrapping(0), m_lineFlags(0), m_surfacePatternType(0) {
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Style const &st) {
    if (st.m_id >= 0) o << "id=" << st.m_id << ",";
    o << reinterpret_cast<MWAWGraphicStyle const &>(st);
    switch(st.m_wrapping & 3) {
    case 0:
      o << "wrap=none,";
      break; // RunThrough
    case 1:
      o << "wrap=regular,";
      break; // Page Wrap
    case 2:
      o << "wrap=irregular,";
      break; // Optimal page Wrap
    default:
      o << "#wrap=3,";
      break;
    }
    switch(st.m_surfacePatternType) {
    case 0:
      break; // pattern
    case 1:
      o << "wallPattern,";
      break;
    default:
      o << "pattType=" << st.m_surfacePatternType << ",";
    }
    if (st.m_lineFlags & 0x10)
      o << "useSurfGradient";
    if (st.m_lineFlags & 0x2F)
      o << "lineFlags(?)=" << std::hex << int(st.m_lineFlags & 0x2F) << std::dec << ",";
    return o;
  }
  //! the identificator
  int m_id;
  //! the wrap type
  int m_wrapping;
  //! the line flags
  int m_lineFlags;
  //! the surface pattern type
  int m_surfacePatternType;
};

//! Internal: the generic structure used to store a zone of a CWGraph
struct Zone {
  //! the list of types
  enum Type { T_Zone, T_Shape, T_Picture, T_Chart, T_DataBox, T_Unknown,
              /* basic subtype */
              T_Line, T_Rect, T_RectOval, T_Oval, T_Arc, T_Poly,
              /* picture subtype */
              T_Pict, T_QTim, T_Movie
            };
  //! constructor
  Zone() : m_page(-1), m_box(), m_style() {}
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Zone const &zone) {
    if (zone.m_page >= 0) o << "pg=" << zone.m_page << ",";
    o << "box=" << zone.m_box << ",";
    zone.print(o);
    o << "style=[" << zone.m_style << "],";
    return o;
  }
  //! destructor
  virtual ~Zone() {}
  //! return the zone bdbox
  Box2f getBdBox() const {
    Vec2f minPt(m_box[0][0], m_box[0][1]);
    Vec2f maxPt(m_box[1][0], m_box[1][1]);
    for (int c=0; c<2; ++c) {
      if (m_box.size()[c]>=0) continue;
      minPt[c]=m_box[1][c];
      maxPt[c]=m_box[0][c];
    }
    return Box2f(minPt,maxPt);
  }
  //! return the main type
  virtual Type getType() const {
    return T_Unknown;
  }
  //! return the subtype
  virtual Type getSubType() const {
    return T_Unknown;
  }
  //! return the number of data to define this zone in the file
  virtual int getNumData() const {
    return 0;
  }
  //! print the data contains
  virtual void print(std::ostream &) const { }
  //! return a child corresponding to this zone
  virtual CWStruct::DSET::Child getChild() const {
    CWStruct::DSET::Child child;
    child.m_box = m_box;
    return child;
  }
  //! the page (checkme: or frame linked )
  int m_page;
  //! the bdbox
  Box2f m_box;
  //! the style
  Style m_style;
};

//! Internal: small class to store a basic graphic zone of a CWGraph
struct ZoneShape : public Zone {
  //! constructor
  ZoneShape(Zone const &z, Type type) : Zone(z), m_type(type), m_shape(), m_rotate(0) {
  }
  //! print the data
  virtual void print(std::ostream &o) const {
    o << m_shape;
    if (m_rotate) o << "rot=" << m_rotate << ",";
  }
  //! return the main type
  virtual Type getType() const {
    return T_Shape;
  }
  //! return the sub type
  virtual Type getSubType() const {
    return m_type;
  }
  //! return the number of data
  virtual int getNumData() const {
    if (m_shape.m_type == MWAWGraphicShape::Polygon) return 1;
    return 0;
  }
  //! return a child corresponding to this zone
  virtual CWStruct::DSET::Child getChild() const {
    CWStruct::DSET::Child child;
    child.m_box = m_box;
    child.m_type = CWStruct::DSET::Child::GRAPHIC;
    return child;
  }

  //! the sub type
  Type m_type;
  //! the shape
  MWAWGraphicShape m_shape;
  //! the rotation
  int m_rotate;
};

//! Internal: the structure used to store a PICT or a MOVIE
struct ZonePict : public Zone {
  //! constructor
  ZonePict(Zone const &z, Type type) : Zone(z), m_type(type) {
  }
  //! print the data
  virtual void print(std::ostream &o) const {
    switch (m_type) {
    case T_Pict:
      o << "PICTURE,";
      break;
    case T_QTim:
      o << "QTIME,";
      break;
    case T_Movie:
      o << "MOVIE,";
      break;
    case T_Zone:
    case T_Shape:
    case T_Picture:
    case T_Chart:
    case T_DataBox:
    case T_Line:
    case T_Rect:
    case T_RectOval:
    case T_Oval:
    case T_Arc:
    case T_Poly:
    case T_Unknown:
    default:
      o << "##type=" << m_type << ",";
      break;
    }
  }
  //! return the main type T_Picture
  virtual Type getType() const {
    return T_Picture;
  }
  //! return the sub type
  virtual Type getSubType() const {
    return m_type;
  }
  //! return the number of data in a file
  virtual int getNumData() const {
    return 2;
  }
  //! return a child corresponding to this zone
  virtual CWStruct::DSET::Child getChild() const {
    CWStruct::DSET::Child child;
    child.m_box = m_box;
    child.m_type = CWStruct::DSET::Child::GRAPHIC;
    return child;
  }

  //! the sub type
  Type m_type;
  //! the picture entry followed by a ps entry or ole entry ( if defined)
  MWAWEntry m_entries[2];
};

//! Internal: structure to store a bitmap of a CWGraph
struct Bitmap : public CWStruct::DSET {
  //! constructor
  Bitmap(CWStruct::DSET const &dset = CWStruct::DSET()) :
    DSET(dset), m_bitmapType(-1), m_size(0,0), m_entry(), m_colorMap() {
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Bitmap const &bt) {
    o << static_cast<CWStruct::DSET const &>(bt);
    if (bt.m_bitmapType >= 0) o << "type=" << bt.m_bitmapType << ",";
    return o;
  }

  //! the bitmap type
  int m_bitmapType;
  //! the bitmap size
  Vec2i m_size;
  //! the bitmap entry
  MWAWEntry m_entry;
  //! the color map
  std::vector<MWAWColor> m_colorMap;
};

//! Internal: structure to store a link to a zone of a CWGraph
struct ZoneZone : public Zone {
  //! constructor
  ZoneZone(Zone const &z) : Zone(z), m_id(-1), m_subId(-1), m_styleId(-1), m_wrappingSep(5) {
    for (int i = 0; i < 9; i++)
      m_flags[i] = 0;
  }
  //! print the zone
  virtual void print(std::ostream &o) const {
    o << "ZONE, id=" << m_id << ",";
    if (m_subId > 0) o << "subId=" << m_subId << ",";
    if (m_styleId >= 0) o << "styleId=" << m_styleId << ",";
    if (m_wrappingSep != 5) o << "wrappingSep=" << m_wrappingSep << ",";
    for (int i = 0; i < 9; i++) {
      if (m_flags[i]) o << "fl" << i << "=" << m_flags[i] << ",";
    }
  }
  //! return the main type Zone
  virtual Type getType() const {
    return T_Zone;
  }
  //! return the sub type Zone
  virtual Type getSubType() const {
    return T_Zone;
  }

  //! return a child corresponding to this zone
  virtual CWStruct::DSET::Child getChild() const {
    CWStruct::DSET::Child child;
    child.m_box = m_box;
    child.m_id = m_id;
    child.m_type = CWStruct::DSET::Child::ZONE;
    return child;
  }

  //! the zoneId
  int m_id;
  //! the zoneSubId: can be page/column/frame linked number
  int m_subId;
  //! the style id
  int m_styleId;
  //! the wraping separator
  int m_wrappingSep;
  //! flag
  int m_flags[9];
};

//! Internal: structure used to store an unknown zone of a CWGraph
struct ZoneUnknown : public Zone {
  //! construtor
  ZoneUnknown(Zone const &z) : Zone(z), m_type(T_Unknown), m_typeId(-1) {
  }
  //! print the zone
  virtual void print(std::ostream &o) const {
    switch(m_type) {
    case T_DataBox:
      o << "BOX(database),";
      break;
    case T_Chart:
      o << "CHART,";
      break;
    case T_Zone:
    case T_Shape:
    case T_Picture:
    case T_Line:
    case T_Rect:
    case T_RectOval:
    case T_Oval:
    case T_Arc:
    case T_Poly:
    case T_Unknown:
    case T_Pict:
    case T_QTim:
    case T_Movie:
    default:
      o << "##type=" << m_typeId << ",";
      break;
    }
  }
  //! return the main type
  virtual Type getType() const {
    return m_type;
  }
  //! return the sub type
  virtual Type getSubType() const {
    return m_type;
  }
  //! return the number of data
  virtual int getNumData() const {
    return m_type == T_Chart ? 2 : 0;
  }
  //! return a child corresponding to this zone
  virtual CWStruct::DSET::Child getChild() const {
    CWStruct::DSET::Child child;
    child.m_box = m_box;
    child.m_type = CWStruct::DSET::Child::GRAPHIC;
    return child;
  }

  //! the sub type
  Type m_type;
  //! type number
  int m_typeId;
};

////////////////////////////////////////
//! Internal: class which stores a group of graphics, ...
struct Group : public CWStruct::DSET {
  struct LinkedZones;
  //! constructor
  Group(CWStruct::DSET const &dset = CWStruct::DSET()) :
    CWStruct::DSET(dset), m_zones(), m_headerDim(0,0), m_hasMainZone(false), m_box(), m_page(0), m_totalNumber(0),
    m_blockToSendList(), m_idLinkedZonesMap() {
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Group const &doc) {
    o << static_cast<CWStruct::DSET const &>(doc);
    return o;
  }

  /** check if we need to send the frame is linked to another frmae */
  bool isLinked(int id) const {
    return m_idLinkedZonesMap.find(id) != m_idLinkedZonesMap.end() &&
           m_idLinkedZonesMap.find(id)->second.isLinked();
  }
  /** add the frame name if needed */
  bool addFrameName(int id, int subId, WPXPropertyList &framePList,
                    WPXPropertyList &textboxPList) const {
    if (!isLinked(id)) return false;
    LinkedZones const &lZones = m_idLinkedZonesMap.find(id)->second;
    std::map<int, size_t>::const_iterator it = lZones.m_mapIdChild.find(subId);
    if (it == lZones.m_mapIdChild.end()) {
      MWAW_DEBUG_MSG(("CWGraphInternal::Group::addFrameName: can not find frame %d[%d]\n", id, subId));
      return false;
    }
    if (it != lZones.m_mapIdChild.begin()) {
      WPXString fName;
      fName.sprintf("Frame%d-%d", id, subId);
      framePList.insert("libwpd:frame-name",fName);
    }
    ++it;
    if (it != lZones.m_mapIdChild.end()) {
      WPXString fName;
      fName.sprintf("Frame%d-%d", id, it->first);
      textboxPList.insert("libwpd:next-frame-name",fName);
    }
    return true;
  }
  /** the list of child zones */
  std::vector<shared_ptr<Zone> > m_zones;

  //! the header dimension ( if defined )
  Vec2i m_headerDim;
  //! a flag to know if this zone contains or no the call to zone 1
  bool m_hasMainZone;
  //! the group bdbox ( if known )
  Box2f m_box;
  //! the group page ( if known )
  int m_page;
  //! the number of zone to send
  int m_totalNumber;
  //! the list of block to send
  std::vector<size_t> m_blockToSendList;
  //! a map zone id to the list of zones
  std::map<int, LinkedZones> m_idLinkedZonesMap;

  //! a small class of Group used to store a list a set of text zone
  struct LinkedZones {
    //! constructor
    LinkedZones(int frameId) :  m_frameId(frameId), m_mapIdChild() {
    }
    //! returns true if we have many linked
    bool isLinked() const {
      return  m_mapIdChild.size() > 1;
    }
    //! the frame basic id
    int m_frameId;
    //! map zoneId -> group child
    std::map<int, size_t> m_mapIdChild;
  };
};

////////////////////////////////////////
//! Internal: the state of a CWGraph
struct State {
  //! constructor
  State() : m_numAccrossPages(-1), m_groupMap(), m_bitmapMap(), m_frameId(0) { }

  //! the number of accross pages ( draw document)
  int m_numAccrossPages;
  //! a map zoneId -> group
  std::map<int, shared_ptr<Group> > m_groupMap;
  //! a map zoneId -> group
  std::map<int, shared_ptr<Bitmap> > m_bitmapMap;
  //! a int used to defined linked frame
  int m_frameId;
};

////////////////////////////////////////
//! Internal: the subdocument of a CWGraph
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(CWGraph &pars, MWAWInputStreamPtr input, int zoneId, MWAWPosition pos=MWAWPosition()) :
    MWAWSubDocument(pars.m_mainParser, input, MWAWEntry()), m_graphParser(&pars), m_id(zoneId), m_position(pos) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const {
    if (MWAWSubDocument::operator!=(doc)) return true;
    SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
    if (!sDoc) return true;
    if (m_graphParser != sDoc->m_graphParser) return true;
    if (m_id != sDoc->m_id) return true;
    return false;
  }

  //! operator!==
  virtual bool operator==(MWAWSubDocument const &doc) const {
    return !operator!=(doc);
  }
  //! the parser function
  void parse(MWAWContentListenerPtr &listener, libmwaw::SubDocumentType type) {
    parse(listener, type, false);
  }
  //! the graphic parser function
  void parseGraphic(MWAWGraphicListenerPtr &listener, libmwaw::SubDocumentType type) {
    parse(listener, type, true);
  }
  //! the main parser function
  void parse(MWAWListenerPtr listener, libmwaw::SubDocumentType type, bool asGraphic);
  /** the graph parser */
  CWGraph *m_graphParser;

protected:
  //! the subdocument id
  int m_id;
  //! the position if known
  MWAWPosition m_position;
private:
  SubDocument(SubDocument const &orig);
  SubDocument &operator=(SubDocument const &orig);
};

void SubDocument::parse(MWAWListenerPtr listener, libmwaw::SubDocumentType type, bool asGraphic)
{
  if (!listener || (type==libmwaw::DOC_TEXT_BOX&&!listener->canWriteText())) {
    MWAW_DEBUG_MSG(("CWGraphInternal::SubDocument::parse: no listener\n"));
    return;
  }
  assert(m_graphParser);
  long pos = m_input->tell();
  if ((asGraphic && (type==libmwaw::DOC_TEXT_BOX || type==libmwaw::DOC_GRAPHIC_GROUP))
      || (!asGraphic && type==libmwaw::DOC_TEXT_BOX))
    m_graphParser->askToSend(m_id,asGraphic,m_position);
  else {
    MWAW_DEBUG_MSG(("CWGraphInternal::SubDocument::parse: find unexpected type\n"));
  }
  m_input->seek(pos, WPX_SEEK_SET);
}

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
CWGraph::CWGraph(CWParser &parser) :
  m_parserState(parser.getParserState()), m_state(new CWGraphInternal::State),
  m_mainParser(&parser), m_styleManager(parser.m_styleManager)
{
}

CWGraph::~CWGraph()
{ }

int CWGraph::version() const
{
  return m_parserState->m_version;
}

int CWGraph::numPages() const
{
  int nPages = 1;
  std::map<int, shared_ptr<CWGraphInternal::Group> >::iterator iter;

  if (m_state->m_numAccrossPages<=0) {
    m_state->m_numAccrossPages=1;
    if (m_mainParser->getHeader() &&
        m_mainParser->getHeader()->getKind()==MWAWDocument::MWAW_K_DRAW) {
      m_state->m_numAccrossPages=m_mainParser->getDocumentPages()[0];
      if (m_state->m_numAccrossPages<=1) {
        // info not always fill so we must check it
        for (iter=m_state->m_groupMap.begin() ; iter != m_state->m_groupMap.end() ; ++iter) {
          shared_ptr<CWGraphInternal::Group> group = iter->second;
          if (!group || group->m_type != CWStruct::DSET::T_Main)
            continue;
          checkNumberAccrossPages(*group);
        }
      }
    }
  }
  for (iter=m_state->m_groupMap.begin() ; iter != m_state->m_groupMap.end() ; ++iter) {
    shared_ptr<CWGraphInternal::Group> group = iter->second;
    if (!group) continue;
    if (group->m_type == CWStruct::DSET::T_Slide) {
      if (group->m_page > nPages)
        nPages = group->m_page;
      continue;
    }
    if (group->m_type != CWStruct::DSET::T_Main)
      continue;
    updateInformation(*group);
    size_t numBlock = group->m_blockToSendList.size();
    for (size_t b=0; b < numBlock; b++) {
      size_t bId=group->m_blockToSendList[b];
      CWGraphInternal::Zone *child = group->m_zones[bId].get();
      if (!child) continue;
      if (child->m_page > nPages)
        nPages = child->m_page;
    }
  }
  return nPages;
}

void CWGraph::askToSend(int number, bool asGraphic, MWAWPosition const& pos)
{
  m_mainParser->sendZone(number, asGraphic, pos);
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////
bool CWGraph::getSurfaceColor(CWGraphInternal::Style const style, MWAWColor &col) const
{
  if (!style.hasSurfaceColor())
    return false;
  col = style.m_surfaceColor;
  return true;
}

////////////////////////////////////////////////////////////
// a group of data mainly graphic
////////////////////////////////////////////////////////////
shared_ptr<CWStruct::DSET> CWGraph::readGroupZone
(CWStruct::DSET const &zone, MWAWEntry const &entry, bool &complete)
{
  complete = false;
  if (!entry.valid() || zone.m_fileType != 0)
    return shared_ptr<CWStruct::DSET>();
  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos+8+16, WPX_SEEK_SET); // avoid header+8 generic number
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  shared_ptr<CWGraphInternal::Group> group(new CWGraphInternal::Group(zone));

  f << "Entries(GroupDef):" << *group << ",";
  int val = (int) input->readLong(2); // a small int between 0 and 3
  switch (val) {
  case 0:
    break; // normal
  case 3:
    f << "database/spreadsheet,";
    break;
  default:
    f << "#type?=" << val << ",";
    break;
  }
  val = (int) input->readLong(2); // a small number between 0 and 1e8
  if (val) f << "f1=" << val << ",";

  ascFile.addDelimiter(input->tell(), '|');
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  // read the last part
  long data0Length = zone.m_dataSz;
  long N = zone.m_numData;
  if (entry.length() -8-12 != data0Length*N + zone.m_headerSz) {
    if (data0Length == 0 && N) {
      MWAW_DEBUG_MSG(("CWGraph::readGroupZone: can not find definition size\n"));
      input->seek(entry.end(), WPX_SEEK_SET);
      return shared_ptr<CWStruct::DSET>();
    }

    MWAW_DEBUG_MSG(("CWGraph::readGroupZone: unexpected size for zone definition, try to continue\n"));
  }

  long beginDefGroup = entry.end()-N*data0Length;
  if (long(input->tell())+42 <= beginDefGroup) {
    input->seek(beginDefGroup-42, WPX_SEEK_SET);
    pos = input->tell();
    if (!readGroupUnknown(*group, 42, -1)) {
      ascFile.addPos(pos);
      ascFile.addNote("GroupDef(Head-###)");
    }
  }

  input->seek(beginDefGroup, WPX_SEEK_SET);

  group->m_childs.resize(size_t(N));
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    MWAWEntry gEntry;
    gEntry.setBegin(pos);
    gEntry.setLength(data0Length);
    shared_ptr<CWGraphInternal::Zone> def = readGroupDef(gEntry);
    group->m_zones.push_back(def);

    if (def)
      group->m_childs[size_t(i)] = def->getChild();
    else {
      f.str("");
      f << "GroupDef#";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    input->seek(gEntry.end(), WPX_SEEK_SET);
  }

  input->seek(entry.end(), WPX_SEEK_SET);

  if (readGroupData(*group, entry.begin())) {
    // fixme: do something here
  }

  if (m_state->m_groupMap.find(group->m_id) != m_state->m_groupMap.end()) {
    MWAW_DEBUG_MSG(("CWGraph::readGroupZone: zone %d already exists!!!\n", group->m_id));
  } else
    m_state->m_groupMap[group->m_id] = group;

  return group;
}

////////////////////////////////////////////////////////////
// a group of data mainly graphic
////////////////////////////////////////////////////////////
shared_ptr<CWStruct::DSET> CWGraph::readBitmapZone
(CWStruct::DSET const &zone, MWAWEntry const &entry, bool &complete)
{
  complete = false;
  if (!entry.valid() || zone.m_fileType != 4)
    return shared_ptr<CWStruct::DSET>();
  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos+8+16, WPX_SEEK_SET); // avoid header+8 generic number
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  shared_ptr<CWGraphInternal::Bitmap> bitmap(new CWGraphInternal::Bitmap(zone));

  f << "Entries(BitmapDef):" << *bitmap << ",";

  ascFile.addDelimiter(input->tell(), '|');

  // read the last part
  long data0Length = zone.m_dataSz;
  long N = zone.m_numData;
  if (entry.length() -8-12 != data0Length*N + zone.m_headerSz) {
    if (data0Length == 0 && N) {
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      MWAW_DEBUG_MSG(("CWGraph::readBitmapZone: can not find definition size\n"));
      input->seek(entry.end(), WPX_SEEK_SET);
      return shared_ptr<CWStruct::DSET>();
    }

    MWAW_DEBUG_MSG(("CWGraph::readBitmapZone: unexpected size for zone definition, try to continue\n"));
  }

  bool sizeSet=false;
  int sizePos = (version() == 1) ? 0: 88;
  if (sizePos && pos+sizePos+4+N*data0Length < entry.end()) {
    input->seek(pos+sizePos, WPX_SEEK_SET);
    ascFile.addDelimiter(pos+sizePos,'[');
    int dim[2]; // ( we must add 2 to add the border )
    for (int j = 0; j < 2; j++)
      dim[j] = (int) input->readLong(2);
    f << "sz=" << dim[1] << "x" << dim[0] << ",";
    if (dim[0] > 0 && dim[1] > 0) {
      bitmap->m_size = Vec2i(dim[1]+2, dim[0]+2);
      sizeSet = true;
    }
    ascFile.addDelimiter(input->tell(),']');
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  /** the end of this block is very simillar to a bitmapdef, excepted
      maybe the first integer  .... */
  if (long(input->tell())+(N+1)*data0Length <= entry.end())
    N++;

  input->seek(entry.end()-N*data0Length, WPX_SEEK_SET);

  for (int i = 0; i < N; i++) {
    pos = input->tell();
    MWAWEntry gEntry;
    gEntry.setBegin(pos);
    gEntry.setLength(data0Length);
    f.str("");
    f << "BitmapDef-" << i << ":";
    long val = (long) input->readULong(4);
    if (val) {
      if (i == 0)
        f << "unkn=" << val << ",";
      else
        f << "ptr=" << std::hex << val << std::dec << ",";
    }
    // f0 : 0 true color, if not number of bytes
    for (int j = 0; j < 3; j++) {
      val = (int) input->readLong(2);
      if (val) f << "f" << j << "=" << val << ",";
    }
    int dim[2]; // ( we must add 2 to add the border )
    for (int j = 0; j < 2; j++)
      dim[j] = (int) input->readLong(2);
    if (i == N-1 && !sizeSet)
      bitmap->m_size = Vec2i(dim[0]+2, dim[1]+2);

    f << "dim?=" << dim[0] << "x" << dim[1] << ",";
    for (int j = 3; j < 6; j++) {
      val = (int) input->readLong(2);
      if ((j != 5 && val!=1) || (j==5 && val)) // always 1, 1, 0
        f << "f" << j << "=" << val << ",";
    }
    if (long(input->tell()) != gEntry.end())
      ascFile.addDelimiter(input->tell(), '|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(gEntry.end(), WPX_SEEK_SET);
  }

  input->seek(entry.end(), WPX_SEEK_SET);
  pos = entry.end();
  bool ok = readBitmapColorMap( bitmap->m_colorMap);
  if (ok) {
    pos = input->tell();
    ok = readBitmapData(*bitmap);
  }
  if (!ok)
    input->seek(pos, WPX_SEEK_SET);

  // fixme: in general followed by another zone
  bitmap->m_otherChilds.push_back(bitmap->m_id+1);

  if (m_state->m_bitmapMap.find(bitmap->m_id) != m_state->m_bitmapMap.end()) {
    MWAW_DEBUG_MSG(("CWGraph::readGroupZone: zone %d already exists!!!\n", bitmap->m_id));
  } else
    m_state->m_bitmapMap[bitmap->m_id] = bitmap;

  return bitmap;
}

shared_ptr<CWGraphInternal::Zone> CWGraph::readGroupDef(MWAWEntry const &entry)
{
  shared_ptr<CWGraphInternal::Zone> res;
  if (entry.length() < 32) {
    if (version() > 1 || entry.length() < 30) {
      MWAW_DEBUG_MSG(("CWGraph::readGroupDef: sz is too short!!!\n"));
      return res;
    }
  }
  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos, WPX_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "GroupDef:";

  CWGraphInternal::Zone zone;
  CWGraphInternal::Style &style = zone.m_style;

  int typeId = (int) input->readULong(1);
  CWGraphInternal::Zone::Type type = CWGraphInternal::Zone::T_Unknown;
  int const vers=version();
  switch(vers) {
  case 1:
    switch(typeId) {
    case 1:
      type = CWGraphInternal::Zone::T_Zone;
      break;
    case 4:
      type = CWGraphInternal::Zone::T_Line;
      break;
    case 5:
      type = CWGraphInternal::Zone::T_Rect;
      break;
    case 6:
      type = CWGraphInternal::Zone::T_RectOval;
      break;
    case 7:
      type = CWGraphInternal::Zone::T_Oval;
      break;
    case 8:
      type = CWGraphInternal::Zone::T_Arc;
      break;
    case 9:
      type = CWGraphInternal::Zone::T_Poly;
      break;
    case 11:
      type = CWGraphInternal::Zone::T_Pict;
      break;
    case 13:
      type = CWGraphInternal::Zone::T_DataBox;
      break;
    default:
      break;
    }
    break;
  default:
    switch(typeId) {
    case 1:
      type = CWGraphInternal::Zone::T_Zone;
      break;
    case 2:
      type = CWGraphInternal::Zone::T_Line;
      break;
    case 3:
      type = CWGraphInternal::Zone::T_Rect;
      break;
    case 4:
      type = CWGraphInternal::Zone::T_RectOval;
      break;
    case 5:
      type = CWGraphInternal::Zone::T_Oval;
      break;
    case 6:
      type = CWGraphInternal::Zone::T_Arc;
      break;
    case 7:
      type = CWGraphInternal::Zone::T_Poly;
      break;
    case 8:
      type = CWGraphInternal::Zone::T_Pict;
      break;
    case 9:
      type = CWGraphInternal::Zone::T_Chart;
      break;
    case 10:
      type = CWGraphInternal::Zone::T_DataBox;
      break;
    case 14:
      type = CWGraphInternal::Zone::T_Movie;
      break;
    case 18:
      type = CWGraphInternal::Zone::T_QTim;
      break;
    default:
      break;
    }
    break;
  }
  int val = (int) input->readULong(1);
  style.m_wrapping = (val & 3);
  if (vers > 1)
    style.m_surfacePatternType = (val >> 2);
  else if (val>>2)
    f << "g0=" << (val>>2) << ",";
  style.m_lineFlags = (int) input->readULong(1);
  if (style.m_lineFlags & 0x40) style.m_arrows[0]=true;
  if (style.m_lineFlags & 0x80) style.m_arrows[1]=true;
  val = (int) input->readULong(1);
  if (val) f << "f0=" << val << ",";

  float dim[4];
  for (int j = 0; j < 4; j++) {
    dim[j] = float(input->readLong(4))/256.f;
    if (dim[j] < -100) f << "##dim?,";
  }
  zone.m_box = Box2f(Vec2f(dim[1], dim[0]), Vec2f(dim[3], dim[2]));
  style.m_lineWidth = (float) input->readLong(1);
  val = (int) input->readULong(1);
  if (val) f << "f1=" << val << ",";
  for (int j = 0; j < 2; j++) {
    int col = (int) input->readULong(1);
    MWAWColor color;
    if (!m_styleManager->getColor(col, color)) {
      MWAW_DEBUG_MSG(("CWGraph::readGroupDef: unknown color!!!\n"));
      f << "###col" << j << "=" << col << ",";
    } else if (j==0)
      style.m_lineColor=color;
    else
      style.setSurfaceColor(color);
  }

  for (int j = 0; j < 2; j++) {
    if (vers > 1) {
      val =  (int) input->readULong(1); // probably also related to surface
      if (val) f << "pat" << j << "[high]=" << val << ",";
    }
    int pat = (int) input->readULong(1);
    if (j==1 && style.m_surfacePatternType) {
      if (style.m_surfacePatternType==1) { // wall paper
        if (!m_styleManager->updateWallPaper(pat, style))
          f << "##wallId=" << pat << ",";
      } else {
        f << "###surfaceType=" << style.m_surfacePatternType << ",";
        MWAW_DEBUG_MSG(("CWGraph::readGroupDef: unknown surface type!!!\n"));
      }
      continue;
    }
    if (j==1 && (style.m_lineFlags & 0x10)) {
      m_styleManager->updateGradient(pat, style);
      continue;
    }
    if (pat==1) {
      if (j==0) style.m_lineOpacity=0;
      else style.m_surfaceOpacity=0;
      continue;
    }
    MWAWGraphicStyle::Pattern pattern;
    float percent;
    if (!m_styleManager->getPattern(pat,pattern,percent)) {
      MWAW_DEBUG_MSG(("CWGraph::readGroupDef: unknown pattern!!!\n"));
      f << "###pat" << j << "=" << pat << ",";
    }
    pattern.m_colors[1]=j==0 ? style.m_lineColor : style.m_surfaceColor;
    MWAWColor color;
    if (!pattern.getUniqueColor(color)) {
      pattern.getAverageColor(color);
      if (j==1) style.m_pattern = pattern;
    }
    if (j==0)
      style.m_lineColor=color;
    else
      style.m_surfaceColor=color;
  }
  style.m_id = (int) input->readLong(2);

  switch (type) {
  case CWGraphInternal::Zone::T_Zone: {
    int nFlags = 0;
    CWGraphInternal::ZoneZone *z = new CWGraphInternal::ZoneZone(zone);
    res.reset(z);
    z->m_flags[nFlags++] = (int) input->readLong(2);
    z->m_id = (int) input->readULong(2);

    int numRemains = int(entry.end()-long(input->tell()));
    numRemains/=2;
    // v1-2:3 v4-v5:6 v6:8
    if (numRemains > 8) numRemains = 8;
    for (int j = 0; j < numRemains; j++) {
      val = (int) input->readLong(2);
      switch(j) {
      case 1:
        z->m_subId = val;
        break;
      case 2:
        z->m_wrappingSep = val;
        break;
      case 3: // checkme
        z->m_styleId = val;
        break;
      default:
        z->m_flags[nFlags++] = val;
        break;
      }
    }
    break;
  }
  case CWGraphInternal::Zone::T_Pict:
  case CWGraphInternal::Zone::T_QTim:
  case CWGraphInternal::Zone::T_Movie:
    res.reset(new CWGraphInternal::ZonePict(zone, type));
    break;
  case CWGraphInternal::Zone::T_Line:
  case CWGraphInternal::Zone::T_Rect:
  case CWGraphInternal::Zone::T_RectOval:
  case CWGraphInternal::Zone::T_Oval:
  case CWGraphInternal::Zone::T_Arc:
  case CWGraphInternal::Zone::T_Poly: {
    CWGraphInternal::ZoneShape *z = new CWGraphInternal::ZoneShape(zone, type);
    res.reset(z);
    readShape(entry, *z);
    break;
  }
  case CWGraphInternal::Zone::T_DataBox:
  case CWGraphInternal::Zone::T_Chart:
  case CWGraphInternal::Zone::T_Shape:
  case CWGraphInternal::Zone::T_Picture:
  case CWGraphInternal::Zone::T_Unknown:
  default: {
    CWGraphInternal::ZoneUnknown *z = new CWGraphInternal::ZoneUnknown(zone);
    res.reset(z);
    z->m_type = type;
    z->m_typeId = typeId;
    break;
  }
  }

  f << *res;

  long actPos = input->tell();
  if (actPos != entry.begin() && actPos != entry.end())
    ascFile.addDelimiter(input->tell(),'|');
  input->seek(entry.end(), WPX_SEEK_SET);

  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  return res;
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool CWGraph::readGroupData(CWGraphInternal::Group &group, long beginGroupPos)
{
  //  bool complete = false;
  if (!readGroupHeader(group)) {
    MWAW_DEBUG_MSG(("CWGraph::readGroupData: unexpected graphic1\n"));
    return false;
  }

  MWAWInputStreamPtr &input= m_parserState->m_input;

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  int const vers = version();
  long pos, sz;
  size_t numChilds = group.m_zones.size();
  int numError = 0;
  for (size_t i = 0; i < numChilds; i++) {
    shared_ptr<CWGraphInternal::Zone> z = group.m_zones[i];
    int numZoneExpected = z ? z->getNumData() : 0;

    if (numZoneExpected) {
      pos = input->tell();
      sz = (long) input->readULong(4);
      f.str("");
      if (sz == 0) {
        MWAW_DEBUG_MSG(("CWGraph::readGroupData: find a nop zone for type: %d\n",
                        z->getSubType()));
        ascFile.addPos(pos);
        ascFile.addNote("#Nop");
        if (!numError++) {
          ascFile.addPos(beginGroupPos);
          ascFile.addNote("###");
        } else {
          MWAW_DEBUG_MSG(("CWGraph::readGroupData: too many errors, zone parsing STOPS\n"));
          return false;
        }
        pos = input->tell();
        sz = (long) input->readULong(4);
      }
      input->seek(pos, WPX_SEEK_SET);
      bool parsed = true;
      switch(z->getSubType()) {
      case CWGraphInternal::Zone::T_QTim:
        if (!readQTimeData(z))
          return false;
        break;
      case CWGraphInternal::Zone::T_Movie:
        // FIXME: pict ( containing movie ) + ??? +
      case CWGraphInternal::Zone::T_Pict:
        if (!readPictData(z))
          return false;
        break;
      case CWGraphInternal::Zone::T_Poly:
        if (z->getNumData() && !readPolygonData(z))
          return false;
        break;
      case CWGraphInternal::Zone::T_Line:
      case CWGraphInternal::Zone::T_Rect:
      case CWGraphInternal::Zone::T_RectOval:
      case CWGraphInternal::Zone::T_Oval:
      case CWGraphInternal::Zone::T_Arc:
      case CWGraphInternal::Zone::T_Zone:
      case CWGraphInternal::Zone::T_Shape:
      case CWGraphInternal::Zone::T_Picture:
      case CWGraphInternal::Zone::T_Chart:
      case CWGraphInternal::Zone::T_DataBox:
      case CWGraphInternal::Zone::T_Unknown:
      default:
        parsed = false;
        break;
      }

      if (!parsed) {
        input->seek(pos+4+sz, WPX_SEEK_SET);
        if (long(input->tell()) != pos+4+sz) {
          input->seek(pos, WPX_SEEK_SET);
          MWAW_DEBUG_MSG(("CWGraph::readGroupData: find a odd zone for type: %d\n",
                          z->getSubType()));
          return false;
        }
        f.str("");
        if (z->getSubType() == CWGraphInternal::Zone::T_Chart)
          f << "Entries(ChartData)";
        else
          f << "Entries(UnknownDATA)-" << z->getSubType();
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        input->seek(pos+4+sz, WPX_SEEK_SET);
        if (numZoneExpected==2) {
          pos = input->tell();
          sz = (long) input->readULong(4);
          if (sz) {
            input->seek(pos, WPX_SEEK_SET);
            MWAW_DEBUG_MSG(("CWGraph::readGroupData: two zones is not implemented for zone: %d\n",
                            z->getSubType()));
            return false;
          }
          ascFile.addPos(pos);
          ascFile.addNote("NOP");
        }
      }
    }
    if (vers>=6) {
      pos = input->tell();
      sz = (long) input->readULong(4);
      if (sz == 0) {
        ascFile.addPos(pos);
        ascFile.addNote("Nop");
        continue;
      }
      MWAW_DEBUG_MSG(("CWGraph::readGroupData: find not null entry for a end of zone: %d\n", z->getSubType()));
      input->seek(pos, WPX_SEEK_SET);
    }
  }

  if (input->atEOS())
    return true;
  // sanity check: normaly no zero except maybe for the last zone
  pos = input->tell();
  sz = (long) input->readULong(4);
  if (sz == 0 && !input->atEOS()) {
    MWAW_DEBUG_MSG(("CWGraph::readGroupData: find unexpected nop data at end of zone\n"));
    ascFile.addPos(beginGroupPos);
    ascFile.addNote("###");
  }
  input->seek(pos, WPX_SEEK_SET);
  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////
bool CWGraph::readShape(MWAWEntry const &entry, CWGraphInternal::ZoneShape &zone)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long actPos = input->tell();
  int remainBytes = int(entry.end()-actPos);
  if (remainBytes < 0)
    return false;

  Vec2f pictSz=zone.getBdBox().size();
  Box2i box(Vec2f(0,0), pictSz);
  MWAWGraphicShape &shape=zone.m_shape;
  shape.m_bdBox=shape.m_formBox=box;
  libmwaw::DebugStream f;
  switch(zone.getSubType()) {
  case CWGraphInternal::Zone::T_Line: {
    int yDeb=(zone.m_style.m_lineFlags&1)?1:0;
    shape = MWAWGraphicShape::line(Vec2f(zone.m_box[0][0],zone.m_box[yDeb][1]),
                                   Vec2f(zone.m_box[1][0],zone.m_box[1-yDeb][1]));
    break;
  }
  case CWGraphInternal::Zone::T_Rect:
    shape.m_type = MWAWGraphicShape::Rectangle;
    break;
  case CWGraphInternal::Zone::T_Oval:
    shape.m_type = MWAWGraphicShape::Circle;
    break;
  case CWGraphInternal::Zone::T_Poly:
    shape.m_type = MWAWGraphicShape::Polygon;
    break; // value are defined in next zone
  case CWGraphInternal::Zone::T_Arc: {
    if (remainBytes < 4) {
      MWAW_DEBUG_MSG(("CWGraph::readSimpleGraphicZone: arc zone is too short\n"));
      return false;
    }
    int fileAngle[2];
    for (int i = 0; i < 2; i++)
      fileAngle[i] = (int) input->readLong(2);
    int val=(int) input->readLong(1);
    if (val==1) f<< "show[axis],";
    else if (val) f << "#show[axis]=" << val << ",";
    int angle[2] = { 90-fileAngle[0]-fileAngle[1], 90-fileAngle[0] };
    while (angle[1] > 360) {
      angle[0]-=360;
      angle[1]-=360;
    }
    while (angle[0] < -360) {
      angle[0]+=360;
      angle[1]+=360;
    }
    Vec2f center = box.center();
    Vec2f axis = 0.5*Vec2f(box.size());
    // we must compute the real bd box
    float minVal[2] = { 0, 0 }, maxVal[2] = { 0, 0 };
    int limitAngle[2];
    for (int i = 0; i < 2; i++)
      limitAngle[i] = (angle[i] < 0) ? int(angle[i]/90)-1 : int(angle[i]/90);
    for (int bord = limitAngle[0]; bord <= limitAngle[1]+1; bord++) {
      float ang = (bord == limitAngle[0]) ? float(angle[0]) :
                  (bord == limitAngle[1]+1) ? float(angle[1]) : float(90 * bord);
      ang *= float(M_PI/180.);
      float actVal[2] = { axis[0] *std::cos(ang), -axis[1] *std::sin(ang)};
      if (actVal[0] < minVal[0]) minVal[0] = actVal[0];
      else if (actVal[0] > maxVal[0]) maxVal[0] = actVal[0];
      if (actVal[1] < minVal[1]) minVal[1] = actVal[1];
      else if (actVal[1] > maxVal[1]) maxVal[1] = actVal[1];
    }
    Box2f realBox(Vec2f(center[0]+minVal[0],center[1]+minVal[1]),
                  Vec2f(center[0]+maxVal[0],center[1]+maxVal[1]));
    zone.m_box=Box2f(Vec2f(zone.m_box[0])+realBox[0],Vec2f(zone.m_box[0])+realBox[1]);
    shape = MWAWGraphicShape::pie(realBox, box, Vec2f(float(angle[0]),float(angle[1])));
    break;
  }
  case CWGraphInternal::Zone::T_RectOval: {
    shape.m_type = MWAWGraphicShape::Rectangle;
    if (remainBytes < 8) {
      MWAW_DEBUG_MSG(("CWGraph::readSimpleGraphicZone: arc zone is too short\n"));
      return false;
    }
    for (int i = 0; i < 2; i++) {
      float dim=float(input->readLong(2))/2.0f;
      shape.m_cornerWidth[i]=(2.f*dim <= pictSz[i]) ? dim : pictSz[i]/2.f;
      int val = (int) input->readULong(2);
      if (val) f << "rRect" << i << "=" << val << ",";
    }
    break;
  }
  case CWGraphInternal::Zone::T_Zone:
  case CWGraphInternal::Zone::T_Shape:
  case CWGraphInternal::Zone::T_Picture:
  case CWGraphInternal::Zone::T_Chart:
  case CWGraphInternal::Zone::T_DataBox:
  case CWGraphInternal::Zone::T_Unknown:
  case CWGraphInternal::Zone::T_Pict:
  case CWGraphInternal::Zone::T_QTim:
  case CWGraphInternal::Zone::T_Movie:
  default:
    MWAW_DEBUG_MSG(("CWGraph::readSimpleGraphicZone: unknown type\n"));
    return false;
  }

  int const vers=version();
  long nextToRead = entry.end()+(vers==6 ? -6 : vers>=4 ? -2 : 0);
  int numRemain = int(nextToRead-input->tell());
  numRemain /= 2;
  for (int i = 0; i < numRemain; i++) {
    int val = (int) input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  if (vers>=4 && input->tell() <= nextToRead) {
    input->seek(nextToRead, WPX_SEEK_SET);
    zone.m_rotate=(int) input->readLong(2);
    if (zone.m_rotate) {
      shape=shape.rotate(float(-zone.m_rotate), shape.m_bdBox.center());
      Vec2f orig=zone.m_box[0]+shape.m_bdBox[0];
      shape.translate(-1.f*shape.m_bdBox[0]);
      zone.m_box=Box2f(orig, orig+shape.m_bdBox.size());
    }
    if (vers==6) {
      for (int i=0; i < 2; i++) { // always 0
        int val = (int) input->readLong(2);
        if (val) f << "h" << i << "=" << val << ",";
      }
    }
  }
  shape.m_extra=f.str();
  return true;
}

bool CWGraph::readGroupHeader(CWGraphInternal::Group &group)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  // int const vers=version();
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "GroupDef(Header):";
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()!=endPos) || (sz && sz < 16)) {
    MWAW_DEBUG_MSG(("CWGraph::readGroupHeader: zone is too short\n"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  if (sz == 0) {
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return true;
  }

  input->seek(pos+4, WPX_SEEK_SET);
  int N = (int) input->readULong(2);
  f << "N=" << N << ",";
  int type = (int) input->readLong(2);
  if (type != -1)
    f << "#type=" << type << ",";
  int val = (int) input->readLong(2);
  if (val) f << "#unkn=" << val << ",";
  int fSz = (int) input->readULong(2);
  if (!fSz || N *fSz+12 != sz) {
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  for (int i = 0; i < 2; i++) { // always 0, 2
    val = (int) input->readLong(2);
    if (val)
      f << "f" << i << "=" << val;
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  input->seek(pos+4+12, WPX_SEEK_SET);
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    if (readGroupUnknown(group, fSz, i))
      continue;
    ascFile.addPos(pos);
    ascFile.addNote("GroupDef(Head-###)");
    input->seek(pos+fSz, WPX_SEEK_SET);
  }

  /** a list of int16 : find
      00320060 00480060 0048ffe9 013a0173 01ba0173 01ea02a0
      01f8ffe7 02080295 020c012c 02140218 02ae01c1
      02ca02c9-02cc02c6-02400000
      03f801e6
      8002e3ff e0010000 ee02e6ff */
  int numHeader = N+1;//vers >=6 ? N+1 : 2*N;
  for (int i = 0; i < numHeader; i++) {
    pos = input->tell();
    std::vector<int> res;
    bool ok = m_mainParser->readStructIntZone("", false, 2, res);
    f.str("");
    f << "[GroupDef(data" << i << ")]";
    if (ok) {
      if (input->tell() != pos+4) {
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
      }
      continue;
    }
    input->seek(pos, WPX_SEEK_SET);
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    MWAW_DEBUG_MSG(("CWGraph::readGroupHeader: can not find data for %d\n", i));
    return true;
  }

  return true;
}

bool CWGraph::readGroupUnknown(CWGraphInternal::Group &group, int zoneSz, int id)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  input->seek(pos+zoneSz, WPX_SEEK_SET);
  if (input->tell() != pos+zoneSz) {
    input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWGraph::readGroupUnknown: zone is too short\n"));
    return false;
  }
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "GroupDef(Head-";
  if (id >= 0) f << id << "):";
  else f << "_" << "):";
  if (zoneSz < 42) {
    MWAW_DEBUG_MSG(("CWGraph::readGroupUnknown: zone is too short\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return true;
  }

  input->seek(pos, WPX_SEEK_SET);
  int type = (int) input->readLong(2); // find -1, 0, 3
  if (type) f << "f0=" << type << ",";
  for (int i = 0; i < 6; i++) {
    /** find f1=8|9|f|14|15|2a|40|73|e9, f2=0|d4, f5=0|80, f6=0|33 */
    long val = (long) input->readULong(1);
    if (val) f << "f" << i+1 << "=" << std::hex << val << std::dec << ",";
  }
  std::vector<int16_t> values16; // some values can be small or little endian, so...
  std::vector<int32_t> values32;
  for (int i = 0; i < 2; i++)
    values32.push_back((int32_t) input->readLong(4));
  // now two small number also in big or small endian
  for (int i = 0; i < 2; i++)
    values16.push_back((int16_t) input->readLong(2));
  // a very big number
  values32.push_back((int32_t) input->readLong(4));
  m_mainParser->checkOrdering(values16, values32);

  Vec2i dim((int)values32[0],(int)values32[1]);
  if (id < 0)
    group.m_headerDim=dim;
  if (dim[0] || dim[1]) f << "dim=" << dim << ",";
  if (values16[0]!=1 || values16[1]!=1)
    f << "pages[num]=" << values16[0] << "x" << values16[1] << ",";
  if (values32[2])
    f << "g0=" << std::hex << values32[2] << std::dec << ",";

  if (input->tell() != pos+zoneSz) {
    ascFile.addDelimiter(input->tell(), '|');
    input->seek(pos+zoneSz, WPX_SEEK_SET);
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read the polygon vertices
////////////////////////////////////////////////////////////
bool CWGraph::readPolygonData(shared_ptr<CWGraphInternal::Zone> zone)
{
  if (!zone || zone->getType() != CWGraphInternal::Zone::T_Shape)
    return false;
  CWGraphInternal::ZoneShape *bZone =
    reinterpret_cast<CWGraphInternal::ZoneShape *>(zone.get());
  MWAWGraphicShape &shape = bZone->m_shape;
  if (shape.m_type!=MWAWGraphicShape::Polygon)
    return false;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos || sz < 12) {
    input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWGraph::readPolygonData: file is too short\n"));
    return false;
  }

  input->seek(pos+4, WPX_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(PolygonData):";
  int N = (int) input->readULong(2);
  f << "N=" << N << ",";
  int val = (int) input->readLong(2);
  if (val != -1) f << "f0=" << val << ",";
  val = (int) input->readLong(2);
  if (val) f << "f1=" << val << ",";
  int fSz = (int) input->readLong(2);
  if (sz != 12+fSz*N) {
    input->seek(pos, WPX_SEEK_SET);
    MWAW_DEBUG_MSG(("CWGraph::readPolygonData: find odd data size\n"));
    return false;
  }
  for (int i = 2; i < 4; i++) {
    val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";

  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  bool isSpline=false;
  std::vector<CWGraphInternal::CurvePoint> vertices;
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    f << "PolygonData-" << i << ":";
    float position[2];
    for (int j = 0; j < 2; j++)
      position[j] = float(input->readLong(4))/256.f;
    CWGraphInternal::CurvePoint point(Vec2f(position[1], position[0]));
    if (fSz >= 26) {
      for (int cPt = 0; cPt < 2; cPt++) {
        float ctrlPos[2];
        for (int j = 0; j < 2; j++)
          ctrlPos[j] = float(input->readLong(4))/256.f;
        point.m_controlPoints[cPt] = Vec2f(ctrlPos[1], ctrlPos[0]);
      }
      int fl = (int) input->readULong(2);
      point.m_type = (fl>>14);
      f << point << ",";
      if (fl&0x3FFF)
        f << "unkn=" << std::hex << int(fl&0x3FFF) << std::dec << ",";
    } else
      f << point << ",";
    if (point.m_type >= 2) isSpline=true;
    vertices.push_back(point);

    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    input->seek(pos+fSz, WPX_SEEK_SET);
  }
  input->seek(endPos, WPX_SEEK_SET);
  if (!isSpline) {
    // if (m_style.m_lineFlags & 1) : we must close the polygon ?
    for (size_t i = 0; i < size_t(N); ++i)
      shape.m_vertices.push_back(vertices[i].m_pos);
    return true;
  }
  shape.m_type = MWAWGraphicShape::Path;
  Vec2f prevPoint, pt1;
  bool hasPrevPoint = false;
  for (size_t i = 0; i < size_t(N); ++i) {
    CWGraphInternal::CurvePoint const &pt = vertices[i];
    if (pt.m_type >= 2) pt1 = pt.m_controlPoints[0];
    else pt1 = pt.m_pos;
    char type = hasPrevPoint ? 'C' : i==0 ? 'M' : pt.m_type<2 ? 'L' : 'S';
    shape.m_path.push_back(MWAWGraphicShape::PathData(type, pt.m_pos, hasPrevPoint ? prevPoint : pt1, pt1));
    hasPrevPoint = pt.m_type >= 2;
    if (hasPrevPoint) prevPoint =  pt.m_controlPoints[1];
  }

  return true;
}

////////////////////////////////////////////////////////////
// read some picture
////////////////////////////////////////////////////////////
bool CWGraph::readPictData(shared_ptr<CWGraphInternal::Zone> zone)
{
  if (!zone || (zone->getSubType() != CWGraphInternal::Zone::T_Pict &&
                zone->getSubType() != CWGraphInternal::Zone::T_Movie))
    return false;
  CWGraphInternal::ZonePict *pZone =
    reinterpret_cast<CWGraphInternal::ZonePict *>(zone.get());
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  if (!readPICT(*pZone)) {
    MWAW_DEBUG_MSG(("CWGraph::readPictData: find a odd pict\n"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  pos = input->tell();
  long sz = (long) input->readULong(4);
  input->seek(pos+4+sz, WPX_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  if (long(input->tell()) != pos+4+sz) {
    input->seek(pos, WPX_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote("###");
    MWAW_DEBUG_MSG(("CWGraph::readPictData: find a end zone for graphic\n"));
    return false;
  }
  if (sz == 0) {
    ascFile.addPos(pos);
    ascFile.addNote("Nop");
    return true;
  }

  // fixme: use readPS for a mac file and readOLE for a pc file
  input->seek(pos, WPX_SEEK_SET);
  if (readPS(*pZone))
    return true;

  input->seek(pos, WPX_SEEK_SET);
  if (readOLE(*pZone))
    return true;

  MWAW_DEBUG_MSG(("CWGraph::readPictData: unknown data file\n"));
#ifdef DEBUG_WITH_FILES
  if (1) {
    WPXBinaryData file;
    input->seek(pos+4, WPX_SEEK_SET);
    input->readDataBlock(sz, file);
    static int volatile pictName = 0;
    libmwaw::DebugStream f;
    f << "DATA-" << ++pictName;
    libmwaw::Debug::dumpFile(file, f.str().c_str());
  }
#endif
  ascFile.addPos(pos);
  if (zone->getSubType() == CWGraphInternal::Zone::T_Movie)
    ascFile.addNote("Entries(MovieData2):#"); // find filesignature: ALMC...
  else
    ascFile.addNote("Entries(PictData2):#");
  ascFile.skipZone(pos+4, pos+4+sz-1);

  input->seek(pos+4+sz, WPX_SEEK_SET);
  return true;
}

bool CWGraph::readPICT(CWGraphInternal::ZonePict &zone)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  if (sz < 12) {
    MWAW_DEBUG_MSG(("CWGraph::readPict: file is too short\n"));
    return false;
  }

  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("CWGraph::readPict: file is too short\n"));
    return false;
  }
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(Graphic):";

  Box2f box;
  input->seek(pos+4, WPX_SEEK_SET);

  MWAWPict::ReadResult res = MWAWPictData::check(input, (int)sz, box);
  if (res == MWAWPict::MWAW_R_BAD) {
    MWAW_DEBUG_MSG(("CWGraph::readPict: can not find the picture\n"));
    input->seek(pos, WPX_SEEK_SET);
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    return false;
  }

  zone.m_entries[0].setBegin(pos+4);
  zone.m_entries[0].setEnd(endPos);
  input->seek(endPos, WPX_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  return true;
}

bool CWGraph::readPS(CWGraphInternal::ZonePict &zone)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long header = (long) input->readULong(4);
  if (header != 0x25215053L) {
    return false;
  }
  long endPos = pos+4+sz;
  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos) {
    return false;
  }
  zone.m_entries[1].setBegin(pos+4);
  zone.m_entries[1].setEnd(endPos);
  zone.m_entries[1].setType("PS");
#ifdef DEBUG_WITH_FILES
  if (1) {
    WPXBinaryData file;
    input->seek(pos+4, WPX_SEEK_SET);
    input->readDataBlock(sz, file);
    static int volatile pictName = 0;
    libmwaw::DebugStream f;
    f << "PostScript-" << ++pictName << ".ps";
    libmwaw::Debug::dumpFile(file, f.str().c_str());
  }
#endif

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(PostScript):";
  input->seek(endPos, WPX_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  ascFile.skipZone(pos+4, endPos-1);

  return true;
}

bool CWGraph::readOLE(CWGraphInternal::ZonePict &zone)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long val = input->readLong(4);
  if (sz <= 24 || val != 0 || input->readULong(4) != 0x1000000)
    return false;
  long endPos = pos+4+sz;
  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos)
    return false;
  input->seek(pos+12, WPX_SEEK_SET);
  // now a dim in little endian
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(OLE):";
  int dim[4];
  for (int i = 0; i < 4; i++)
    dim[i] = (int)(int32_t)MWAWInputStream::readULong(input->input().get(), 4, 0, true);
  if (dim[0] >= dim[2] || dim[1] >= dim[3]) return false;
  f << "dim=" << dim[1] << "x" << dim[0] << "<->" << dim[3] << "x" << dim[2] << ",";
  zone.m_entries[1].setBegin(pos+28);
  zone.m_entries[1].setEnd(endPos);
  zone.m_entries[1].setType("OLE");
#ifdef DEBUG_WITH_FILES
  if (1) {
    WPXBinaryData file;
    input->seek(pos+28, WPX_SEEK_SET);
    input->readDataBlock(sz-24, file);
    static int volatile pictName = 0;
    libmwaw::DebugStream f2;
    f2 << "OLE-" << ++pictName;
    libmwaw::Debug::dumpFile(file, f2.str().c_str());
  }
#endif

  input->seek(endPos, WPX_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  ascFile.skipZone(pos+28, endPos-1);

  return true;
}

////////////////////////////////////////////////////////////
// read Qtime picture
////////////////////////////////////////////////////////////
bool CWGraph::readQTimeData(shared_ptr<CWGraphInternal::Zone> zone)
{
  if (!zone || zone->getSubType() != CWGraphInternal::Zone::T_QTim)
    return false;
  CWGraphInternal::ZonePict *pZone =
    reinterpret_cast<CWGraphInternal::ZonePict *>(zone.get());
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  bool ok = true;
  std::string name("");
  for (int i = 0; i < 4; i++) {
    char c = (char) input->readULong(1);
    if (c == 0) ok = false;
    name += c;
  }
  if (!ok) {
    MWAW_DEBUG_MSG(("CWGraph::readQTimeData: find a odd qtim zone\n"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(QTIM):"<< name << ":";
  for (int i = 0; i < 2; i++) f << "f" << i << "=" << input->readULong(2) << ",";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  pos = input->tell();
  if (!readNamedPict(*pZone)) {
    MWAW_DEBUG_MSG(("CWGraph::readQTimeData: find a odd named pict\n"));
    input->seek(pos, WPX_SEEK_SET);
    return false;
  }
  return true;
}


bool CWGraph::readNamedPict(CWGraphInternal::ZonePict &zone)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  std::string name("");
  for (int i = 0; i < 4; i++) {
    char c = (char) input->readULong(1);
    if (c < ' ' || c > 'z') {
      MWAW_DEBUG_MSG(("CWGraph::readNamedPict: can not find the name\n"));
      return false;
    }
    name+=c;
  }
  long sz = (long) input->readULong(4);
  long endPos = pos+8+sz;
  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos || !sz) {
    MWAW_DEBUG_MSG(("CWGraph::readNamedPict: file is too short\n"));
    return false;
  }

  zone.m_entries[0].setBegin(pos+8);
  zone.m_entries[0].setEnd(endPos);

#ifdef DEBUG_WITH_FILES
  if (1) {
    WPXBinaryData file;
    input->seek(pos+8, WPX_SEEK_SET);
    input->readDataBlock(sz, file);
    static int volatile pictName = 0;
    libmwaw::DebugStream f;
    f << "PICT2-" << ++pictName << "." << name;
    libmwaw::Debug::dumpFile(file, f.str().c_str());
  }
#endif

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(" << name << "):";
  input->seek(endPos, WPX_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  ascFile.skipZone(pos+8, endPos-1);

  return true;
}

////////////////////////////////////////////////////////////
// read bitmap picture
////////////////////////////////////////////////////////////
bool CWGraph::readBitmapColorMap(std::vector<MWAWColor> &cMap)
{
  cMap.resize(0);
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  if (!sz) {
    ascFile.addPos(pos);
    ascFile.addNote("Nop");
    return true;
  }
  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("CWGraph::readBitmapColorMap: file is too short\n"));
    return false;
  }

  input->seek(pos+4, WPX_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(BitmapColor):";
  f << "unkn=" << input->readLong(4) << ",";
  int maxColor = (int) input->readLong(4);
  if (sz != 8+8*(maxColor+1)) {
    MWAW_DEBUG_MSG(("CWGraph::readBitmapColorMap: sz is odd\n"));
    return false;
  }
  cMap.resize(size_t(maxColor+1));
  for (int i = 0; i <= maxColor; i++) {
    int id = (int) input->readULong(2);
    if (id != i) {
      MWAW_DEBUG_MSG(("CWGraph::readBitmapColorMap: find odd index : %d\n", i));
      return false;
    }
    unsigned char col[3];
    for (int c = 0; c < 3; c++) col[c] = (unsigned char)(input->readULong(2)>>8);
    cMap[(size_t)i] = MWAWColor(col[0], col[1], col[2]);
  }

  input->seek(endPos, WPX_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  return true;
}

bool CWGraph::readBitmapData(CWGraphInternal::Bitmap &zone)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  input->seek(endPos, WPX_SEEK_SET);
  if (long(input->tell()) != endPos || !sz) {
    MWAW_DEBUG_MSG(("CWGraph::readBitmapData: file is too short\n"));
    return false;
  }
  /* Fixme: this code can not works for the packed bitmap*/
  long numColors = zone.m_size[0]*zone.m_size[1];
  int numBytes = numColors ? int(sz/numColors) : 0;
  if (sz != numBytes*numColors) {
    // check for different row alignement: 2 and 4
    for (int align=2; align <= 4; align*=2) {
      int diffToAlign=align-(zone.m_size[0]%align);
      if (diffToAlign==align) continue;
      numColors = (zone.m_size[0]+diffToAlign)*zone.m_size[1];
      numBytes = numColors ? int(sz/numColors) : 0;
      if (sz == numBytes*numColors) {
        zone.m_size[0]+=diffToAlign;
        MWAW_DEBUG_MSG(("CWGraph::readBitmapData: increase width to %d\n",zone.m_size[0]));
        break;
      }
    }
  }
  if (sz != numBytes*numColors) {
    MWAW_DEBUG_MSG(("CWGraph::readBitmapData: unexpected size\n"));
    return false;
  }
  zone.m_bitmapType = numBytes;
  zone.m_entry.setBegin(pos+4);
  zone.m_entry.setEnd(endPos);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(BitmapData):nBytes=" << numBytes;
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  ascFile.skipZone(pos+4, endPos-1);

  return true;
}

////////////////////////////////////////////////////////////
// update the group information
////////////////////////////////////////////////////////////
void CWGraph::checkNumberAccrossPages(CWGraphInternal::Group &group) const
{
  m_state->m_numAccrossPages=1;
  float textWidth=72.0f*(float)m_mainParser->getPageWidth();
  for (size_t b=0; b < group.m_zones.size(); b++) {
    CWGraphInternal::Zone *child = group.m_zones[b].get();
    if (!child) continue;
    if (child->m_box[1].y() >= 2000) // a little to suspicious
      continue;
    int page=int(child->m_box[1].x()/textWidth-0.2)+1;
    if (page > m_state->m_numAccrossPages && page < 100) {
      MWAW_DEBUG_MSG(("CWGraph::checkNumberAccrossPages: increase num page accross to %d\n", page));
      m_state->m_numAccrossPages = page;
    }
  }
}

void CWGraph::updateInformation(CWGraphInternal::Group &group) const
{
  if (group.m_blockToSendList.size() || group.m_idLinkedZonesMap.size())
    return;
  std::set<int> forbiddenZone;

  if (group.m_type == CWStruct::DSET::T_Main || group.m_type == CWStruct::DSET::T_Slide) {
    int headerId=0, footerId=0;
    m_mainParser->getHeaderFooterId(headerId, footerId);
    if (headerId) forbiddenZone.insert(headerId);
    if (footerId) forbiddenZone.insert(footerId);
  }

  for (size_t g = 0; g < group.m_zones.size(); g++) {
    CWGraphInternal::Zone *child = group.m_zones[g].get();
    if (!child) continue;
    if (child->getType() != CWGraphInternal::Zone::T_Zone) {
      group.m_blockToSendList.push_back(g);
      group.m_totalNumber++;
      continue;
    }

    CWGraphInternal::ZoneZone const &childZone =
      reinterpret_cast<CWGraphInternal::ZoneZone &>(*child);
    int zId = childZone.m_id;
    if (!group.okChildId(zId) || forbiddenZone.find(zId) != forbiddenZone.end())
      continue;

    group.m_totalNumber++;
    if (zId==1) {
      group.m_hasMainZone = true;
      continue;
    }

    group.m_blockToSendList.push_back(g);

    if (group.m_idLinkedZonesMap.find(zId) == group.m_idLinkedZonesMap.end())
      group.m_idLinkedZonesMap.insert
      (std::map<int,CWGraphInternal::Group::LinkedZones>::value_type
       (zId,CWGraphInternal::Group::LinkedZones(m_state->m_frameId++)));
    CWGraphInternal::Group::LinkedZones &lZone = group.m_idLinkedZonesMap.find(zId)->second;
    if (lZone.m_mapIdChild.find(childZone.m_subId) != lZone.m_mapIdChild.end()) {
      MWAW_DEBUG_MSG(("CWGraph::updateInformation: zone %d already find with subId %d\n",
                      zId, childZone.m_subId));
      continue;
    }
    lZone.m_mapIdChild[childZone.m_subId] = g;
  }

  // try to fix the page position corresponding to the main zone
  int numPagesAccross=m_state->m_numAccrossPages;
  if (numPagesAccross <= 0) {
    MWAW_DEBUG_MSG(("CWGraph::updateInformation: the number of accross pages is not set\n"));
    numPagesAccross=1;
  }
  float textWidth=72.0f*(float)m_mainParser->getPageWidth();
  float textHeight = 0.0;
  if (double(group.m_headerDim[1])>36.0*m_mainParser->getFormLength() &&
      double(group.m_headerDim[1])<72.0*m_mainParser->getFormLength())
    textHeight=float(group.m_headerDim[1]);
  else
    textHeight=72.0f*(float)m_mainParser->getTextHeight();
  if (textHeight <= 0) {
    MWAW_DEBUG_MSG(("CWGraph::updateInformation: can not retrieve the form length\n"));
    return;
  }
  size_t numBlock = group.m_blockToSendList.size();
  Box2f groupBox;
  int groupPage=-1;
  bool firstGroupFound=false;
  for (size_t b=0; b < numBlock; b++) {
    size_t bId=group.m_blockToSendList[b];
    CWGraphInternal::Zone *child = group.m_zones[bId].get();
    if (!child) continue;
    int pageY=int(float(child->m_box[1].y())/textHeight);
    if (pageY < 0)
      continue;
    if (++pageY > 1) {
      Vec2f orig = child->m_box[0];
      Vec2f sz = child->m_box.size();
      orig[1]-=float(pageY-1)*textHeight;
      if (orig[1] < 0) {
        if (orig[1]>=-textHeight*0.1f)
          orig[1]=0;
        else if (orig[1]>-1.1*textHeight) {
          orig[1]+=textHeight;
          if (orig[1]<0) orig[1]=0;
          pageY--;
        } else {
          MWAW_DEBUG_MSG(("CWGraph::updateInformation: can not find the page\n"));
          continue;
        }
      }
      child->m_box = Box2f(orig, orig+sz);
    }
    int pageX=1;
    if (numPagesAccross>1) {
      pageX=int(float(child->m_box[1].x())/textWidth);
      Vec2f orig = child->m_box[0];
      Vec2f sz = child->m_box.size();
      orig[0]-=float(pageX)*textWidth;
      if (orig[0] < 0) {
        if (orig[0]>=-textWidth*0.1f)
          orig[0]=0;
        else if (orig[0]>-1.1*textWidth) {
          orig[0]+=textWidth;
          if (orig[0]<0) orig[0]=0;
          pageX--;
        } else {
          MWAW_DEBUG_MSG(("CWGraph::updateInformation: can not find the horizontal page\n"));
          continue;
        }
      }
      child->m_box = Box2f(orig, orig+sz);
      pageX++;
    }
    int page=pageX+(pageY-1)*numPagesAccross;
    if (!firstGroupFound) {
      groupPage=page;
      groupBox=child->getBdBox();
      firstGroupFound=true;
    } else if (groupPage==page)
      groupBox=groupBox.getUnion(child->getBdBox());
    else
      groupPage=-1;
    child->m_page = page;
  }
  if (groupPage>=0) {
    group.m_page=groupPage;
    group.m_box=groupBox;
  }
}

////////////////////////////////////////////////////////////
// send data to the listener
////////////////////////////////////////////////////////////
bool CWGraph::canSendGroupAsGraphic(int number) const
{
  std::map<int, shared_ptr<CWGraphInternal::Group> >::iterator iter
    = m_state->m_groupMap.find(number);
  if (iter == m_state->m_groupMap.end() || !iter->second)
    return false;
  return canSendAsGraphic(*iter->second);
}

bool CWGraph::sendGroup(int number, bool asGraphic, MWAWPosition const &position)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("CWGraph::sendGroup: can not find the listener\n"));
    return false;
  }
  std::map<int, shared_ptr<CWGraphInternal::Group> >::iterator iter
    = m_state->m_groupMap.find(number);
  if (iter == m_state->m_groupMap.end() || !iter->second)
    return false;
  shared_ptr<CWGraphInternal::Group> group = iter->second;
  group->m_parsed=true;
  if (asGraphic) {
    MWAWGraphicListenerPtr graphicListener=m_parserState->m_graphicListener;
    if (!graphicListener) {
      MWAW_DEBUG_MSG(("CWGraph::sendGroup: can not find the graphiclistener\n"));
      return false;
    }
    return sendGroup(*group, group->m_blockToSendList, *graphicListener);
  }
  return sendGroup(*group, position);
}

bool CWGraph::canSendAsGraphic(CWGraphInternal::Group &group) const
{
  updateInformation(group);
  if ((group.m_type != CWStruct::DSET::T_Frame && group.m_type != CWStruct::DSET::T_Unknown)
      || group.m_page <= 0)
    return false;
  size_t numZones = group.m_blockToSendList.size();
  for (size_t g = 0; g < numZones; g++) {
    CWGraphInternal::Zone const *child = group.m_zones[group.m_blockToSendList[g]].get();
    if (!child) continue;
    CWGraphInternal::Zone::Type type=child->getType();
    if (type==CWGraphInternal::Zone::T_Zone) {
      CWGraphInternal::ZoneZone const &childZone =
        reinterpret_cast<CWGraphInternal::ZoneZone const &>(*child);
      if (group.isLinked(childZone.m_id) || !m_mainParser->canSendZoneAsGraphic(childZone.m_id))
        return false;
      continue;
    }
    if (type==CWGraphInternal::Zone::T_Shape || type==CWGraphInternal::Zone::T_DataBox ||
        type==CWGraphInternal::Zone::T_Chart || type==CWGraphInternal::Zone::T_Unknown)
      continue;
    return false;
  }
  return true;
}

bool CWGraph::sendGroup(CWGraphInternal::Group &group, std::vector<size_t> const &lChild, MWAWGraphicListener &listener)
{
  group.m_parsed=true;
  size_t numZones = lChild.size();
  for (size_t g = 0; g < numZones; g++) {
    CWGraphInternal::Zone const *child = group.m_zones[lChild[g]].get();
    if (!child) continue;
    Box2f box=child->getBdBox();
    CWGraphInternal::Zone::Type type=child->getType();
    if (type==CWGraphInternal::Zone::T_Zone) {
      CWGraphInternal::ZoneZone const &zone=
        reinterpret_cast<CWGraphInternal::ZoneZone const &>(*child);
      shared_ptr<CWStruct::DSET> dset=m_mainParser->getZone(zone.m_id);
      if (dset && dset->m_fileType==4) {
        MWAWPosition pos(box[0], box.size(), WPX_POINT);
        sendBitmap(zone.m_id, true, pos);
        continue;
      }
      shared_ptr<MWAWSubDocument> doc(new CWGraphInternal::SubDocument(*this, m_parserState->m_input, zone.m_id));
      if (dset && dset->m_fileType==1)
        listener.insertTextBox(box, doc, zone.m_style);
      else
        listener.insertGroup(box, doc);
    } else if (type==CWGraphInternal::Zone::T_Shape) {
      CWGraphInternal::ZoneShape const &shape=
        reinterpret_cast<CWGraphInternal::ZoneShape const &>(*child);
      MWAWGraphicStyle style(shape.m_style);
      if (shape.m_shape.m_type!=MWAWGraphicShape::Line)
        style.m_arrows[0]=style.m_arrows[1]=false;
      listener.insertPicture(box, shape.m_shape, style);
    } else if (type!=CWGraphInternal::Zone::T_DataBox) {
      MWAW_DEBUG_MSG(("CWGraph::sendGroup: find unexpected type!!!\n"));
    }
  }
  return true;
}

bool CWGraph::sendGroup(CWGraphInternal::Group &group, MWAWPosition const &position)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("CWGraph::sendGroup: can not find the listener\n"));
    return false;
  }
  updateInformation(group);
  bool mainGroup = group.m_type == CWStruct::DSET::T_Main;
  bool isSlide = group.m_type == CWStruct::DSET::T_Slide;
  Vec2f leftTop(0,0);
  float textHeight = 0.0;
  if (mainGroup)
    leftTop = 72.0f*m_mainParser->getPageLeftTop();
  textHeight = 72.0f*float(m_mainParser->getFormLength());

  libmwaw::SubDocumentType inDocType;
  if (!listener->isSubDocumentOpened(inDocType))
    inDocType = libmwaw::DOC_NONE;
  MWAWPosition::AnchorTo suggestedAnchor = MWAWPosition::Char;
  switch(inDocType) {
  case libmwaw::DOC_TEXT_BOX:
    suggestedAnchor=MWAWPosition::Char;
    break;
  case libmwaw::DOC_HEADER_FOOTER:
  case libmwaw::DOC_NOTE:
    suggestedAnchor=MWAWPosition::Frame;
    break;
  case libmwaw::DOC_TABLE:
  case libmwaw::DOC_COMMENT_ANNOTATION:
    suggestedAnchor=MWAWPosition::Char;
    break;
  case libmwaw::DOC_GRAPHIC_GROUP:
    suggestedAnchor=MWAWPosition::Page;
    break;
  default:
  case libmwaw::DOC_NONE:
    suggestedAnchor= (mainGroup || isSlide) ? MWAWPosition::Page : MWAWPosition::Char;
    break;
  }
  if (0 && position.m_anchorTo==MWAWPosition::Unknown) {
    MWAW_DEBUG_MSG(("CWGraph::sendGroup: position is not set\n"));
  }
  MWAWGraphicListenerPtr graphicListener=m_parserState->m_graphicListener;
  bool canUseGraphic=graphicListener && !graphicListener->isDocumentStarted();
  if (!mainGroup && canUseGraphic && canSendAsGraphic(group)) {
    Box2f box=group.m_box;
    graphicListener->startGraphic(box);
    sendGroup(group, group.m_blockToSendList, *graphicListener);
    WPXBinaryData data;
    std::string type;
    if (graphicListener->endGraphic(data,type)) {
      MWAWPosition pos(position);
      //pos.setOrigin(box[0]);
      if (pos.size()[0]<=0 || pos.size()[1]<=0)
        pos.setSize(box.size());
      if (pos.m_anchorTo==MWAWPosition::Unknown) {
        pos=MWAWPosition(box[0], box.size(), WPX_POINT);
        pos.setRelativePosition(suggestedAnchor);
        if (suggestedAnchor == MWAWPosition::Page) {
          int pg = isSlide ? 0 : group.m_page > 0 ? group.m_page : 1;
          Vec2f orig = pos.origin()+leftTop;
          pos.setPagePos(pg, orig);
          pos.m_wrapping =  MWAWPosition::WBackground;
        } else if (suggestedAnchor == MWAWPosition::Char)
          pos.setOrigin(Vec2f(0,0));
      }
      listener->insertPicture(pos, data, type);
    }
    return true;
  }
  if (group.m_totalNumber > 1 &&
      (position.m_anchorTo==MWAWPosition::Char ||
       position.m_anchorTo==MWAWPosition::CharBaseLine)) {
    // we need to a frame, ...
    MWAWPosition lPos;
    lPos.m_anchorTo=MWAWPosition::Frame;
    MWAWSubDocumentPtr doc(new CWGraphInternal::SubDocument(*this, m_parserState->m_input, group.m_id, lPos));
    WPXPropertyList extras;
    extras.insert("style:background-transparency", "100%");
    listener->insertTextBox(position, doc, extras);
    return true;
  }

  /* first sort the different zone: ie. we must first send the page block, then the main zone */
  std::vector<size_t> listJobs[2];
  for (size_t g = 0; g < group.m_blockToSendList.size(); g++) {
    CWGraphInternal::Zone *child = group.m_zones[g].get();
    if (!child) continue;
    if (position.m_anchorTo==MWAWPosition::Unknown && suggestedAnchor == MWAWPosition::Page) {
      Vec2f RB = Vec2f(child->m_box[1])+leftTop;
      if (RB[1] >= textHeight || listener->isSectionOpened()) {
        listJobs[1].push_back(g);
        continue;
      }
    }
    if (child->getType() == CWGraphInternal::Zone::T_Zone) {
      CWGraphInternal::ZoneZone const &childZone =
        reinterpret_cast<CWGraphInternal::ZoneZone &>(*child);
      int zId = childZone.m_id;
      shared_ptr<CWStruct::DSET> dset=m_mainParser->getZone(zId);
      if (dset && dset->m_type==CWStruct::DSET::T_Main) {
        listJobs[1].push_back(g);
        continue;
      }
    }
    listJobs[0].push_back(g);
  }
  for (int st = 0; st < 2; st++) {
    if (st == 1) {
      suggestedAnchor = MWAWPosition::Char;
      if (group.m_hasMainZone)
        m_mainParser->sendZone(1, false);
    }
    size_t numJobs=listJobs[st].size();
    for (size_t g = 0; g < numJobs; g++) {
      size_t cId;
      Box2f box;
      std::vector<size_t> groupList;
      int page = 0;
      size_t lastOk=g;

      if (st==0 && !mainGroup && canUseGraphic) {
        for (size_t h = g; h < numJobs; ++h) {
          cId=listJobs[st][h];
          CWGraphInternal::Zone *child = group.m_zones[cId].get();
          if (!child) continue;
          CWGraphInternal::Zone::Type type=child->getType();
          if (groupList.empty()) page=child->m_page;
          else if (page != child->m_page) break;
          if (type==CWGraphInternal::Zone::T_Zone) {
            CWGraphInternal::ZoneZone const &childZone =
              reinterpret_cast<CWGraphInternal::ZoneZone const &>(*child);
            if (group.isLinked(childZone.m_id) ||
                !m_mainParser->canSendZoneAsGraphic(childZone.m_id))
              break;
          } else if (type==CWGraphInternal::Zone::T_DataBox ||
                     type==CWGraphInternal::Zone::T_Chart ||
                     type==CWGraphInternal::Zone::T_Unknown)
            continue;
          else if (type!=CWGraphInternal::Zone::T_Shape)
            break;
          if (groupList.empty())
            box=child->m_box;
          else
            box=box.getUnion(child->m_box);
          groupList.push_back(cId);
          lastOk=h;
        }
      }

      if (groupList.size() <= 1) {
        cId=listJobs[st][g];
        CWGraphInternal::Zone *child = group.m_zones[cId].get();
        if (!child) continue;
        box = child->m_box;
        page = child->m_page;
      }
      MWAWPosition pos(position);
      pos.setOrder(int(cId)+1);
      pos.setOrigin(box[0]);
      pos.setSize(box.size());
      if (pos.m_anchorTo==MWAWPosition::Unknown) {
        pos=MWAWPosition(box[0], box.size(), WPX_POINT);
        pos.setRelativePosition(suggestedAnchor);
        if (suggestedAnchor == MWAWPosition::Page) {
          int pg = page > 0 ? page : 1;
          Vec2f orig = pos.origin()+leftTop;
          pos.setPagePos(pg, orig);
          pos.m_wrapping =  MWAWPosition::WBackground;
          pos.setOrder(-int(cId)-1);
        } else if (st==1 || suggestedAnchor == MWAWPosition::Char)
          pos.setOrigin(Vec2f(0,0));
      }
      if (groupList.size() <= 1) {
        sendGroupChild(group, cId, pos);
        continue;
      }
      graphicListener->startGraphic(box);
      sendGroup(group, groupList, *graphicListener);
      WPXBinaryData data;
      std::string type;
      if (graphicListener->endGraphic(data,type)) {
        WPXPropertyList extras;
        extras.insert("style:background-transparency", "100%");
        listener->insertPicture(pos, data, type, extras);
      }
      g=lastOk;
    }
  }
  return true;
}

bool CWGraph::sendGroupChild(CWGraphInternal::Group &group, size_t cId, MWAWPosition pos)
{
  CWGraphInternal::Zone *child = group.m_zones[cId].get();
  if (!child) return false;
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) {
    MWAW_DEBUG_MSG(("CWGraph::sendGroupChild: can not find the listener\n"));
    return false;
  }
  CWGraphInternal::Zone::Type type = child->getType();
  if (type==CWGraphInternal::Zone::T_Picture)
    return sendPicture(reinterpret_cast<CWGraphInternal::ZonePict &>(*child), pos);
  if (type==CWGraphInternal::Zone::T_Shape)
    return sendShape(reinterpret_cast<CWGraphInternal::ZoneShape &>(*child), pos);
  if (type==CWGraphInternal::Zone::T_DataBox || type==CWGraphInternal::Zone::T_Chart ||
      type==CWGraphInternal::Zone::T_Unknown)
    return true;
  if (type!=CWGraphInternal::Zone::T_Zone) {
    MWAW_DEBUG_MSG(("CWGraph::sendGroupChild: find unknown zone\n"));
    return false;
  }

  CWGraphInternal::ZoneZone const &childZone = reinterpret_cast<CWGraphInternal::ZoneZone &>(*child);
  int zId = childZone.m_id;
  shared_ptr<CWStruct::DSET> dset=m_mainParser->getZone(zId);
  CWGraphInternal::Style const cStyle = childZone.m_style;
  switch (cStyle.m_wrapping&3) {
  case 0:
    pos.m_wrapping = MWAWPosition::WRunThrough;
    break;
  case 1:
  case 2:
    pos.m_wrapping = MWAWPosition::WDynamic;
    break;
  default:
    break;
  }
  // if this is a group, try to send it as a picture
  bool isLinked=group.isLinked(zId);
  bool isGroup = dset && dset->m_fileType==0;
  MWAWGraphicListenerPtr graphicListener=m_parserState->m_graphicListener;
  bool canUseGraphic=graphicListener && !graphicListener->isDocumentStarted();
  if (!isLinked && isGroup && canUseGraphic && canSendGroupAsGraphic(zId))
    return sendGroup(zId, false, pos);
  if (!isLinked && dset && dset->m_fileType==4)
    return sendBitmap(zId, false, pos);
  if (!isLinked && (cStyle.hasPattern() || cStyle.hasGradient()) && canUseGraphic &&
      (dset && dset->m_fileType==1) && m_mainParser->canSendZoneAsGraphic(zId)) {
    Box2f box=Box2f(Vec2f(0,0), childZone.m_box.size());
    graphicListener->startGraphic(box);
    shared_ptr<MWAWSubDocument> doc(new CWGraphInternal::SubDocument(*this, m_parserState->m_input, zId));
    graphicListener->insertTextBox(box, doc, cStyle);
    WPXBinaryData data;
    std::string mime;
    if (graphicListener->endGraphic(data,mime))
      listener->insertPicture(pos, data, mime);
    return true;
  }
  // now check if we need to create a frame
  CWStruct::DSET::Type cType=dset ? dset->m_type : CWStruct::DSET::T_Unknown;
  bool createFrame=
    cType == CWStruct::DSET::T_Frame || cType == CWStruct::DSET::T_Table ||
    (cType == CWStruct::DSET::T_Unknown && pos.m_anchorTo == MWAWPosition::Page);
  if (!isLinked && childZone.m_subId) {
    MWAW_DEBUG_MSG(("find old subs zone\n"));
    return false;
  }
  WPXPropertyList extras;
  if (dset && dset->m_fileType==1) { // checkme: use style for textbox
    MWAWColor color;
    if (cStyle.hasSurfaceColor() && getSurfaceColor(cStyle, color))
      extras.insert("fo:background-color", color.str().c_str());
    else
      extras.insert("style:background-transparency", "100%");
    if (cStyle.hasLine()) {
      std::stringstream stream;
      stream << cStyle.m_lineWidth*0.03 << "cm solid "
             << cStyle.m_lineColor.str();
      extras.insert("fo:border", stream.str().c_str());
      // extend the frame to add border
      float extend = float(cStyle.m_lineWidth*0.85);
      pos.setOrigin(pos.origin()-Vec2f(extend,extend));
      pos.setSize(pos.size()+2.0*Vec2f(extend,extend));
    }
  } else
    extras.insert("style:background-transparency", "100%");
  if (createFrame) {
    WPXPropertyList textboxExtras;
    group.addFrameName(zId, childZone.m_subId, extras, textboxExtras);
    shared_ptr<MWAWSubDocument> doc;
    if (!isLinked || childZone.m_subId==0) {
      MWAWPosition lPos;
      if (0 && (cType == CWStruct::DSET::T_Frame || cType == CWStruct::DSET::T_Table))
        lPos.m_anchorTo=MWAWPosition::Frame;
      doc.reset(new CWGraphInternal::SubDocument(*this, m_parserState->m_input, zId, lPos));
    }
    if (!isLinked && dset && dset->m_fileType==1 && pos.size()[1]>0) // use min-height for text
      pos.setSize(Vec2f(pos.size()[0],-pos.size()[1]));
    listener->insertTextBox(pos, doc, extras, textboxExtras);
    return true;
  }
  return m_mainParser->sendZone(zId, false);
}

bool CWGraph::sendShape(CWGraphInternal::ZoneShape &pict, MWAWPosition pos)
{
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  if (!listener) return true;
  if (pos.size()[0] < 0 || pos.size()[1] < 0)
    pos.setSize(pict.getBdBox().size());

  MWAWGraphicStyle pStyle(pict.m_style);
  if (pict.m_shape.m_type!=MWAWGraphicShape::Line)
    pStyle.m_arrows[0]=pStyle.m_arrows[1]=false;
  pos.setOrigin(pos.origin()-Vec2f(2,2));
  pos.setSize(pos.size()+Vec2f(4,4));
  listener->insertPicture(pos, pict.m_shape, pStyle);
  return true;
}

bool CWGraph::canSendBitmapAsGraphic(int number) const
{
  std::map<int, shared_ptr<CWGraphInternal::Bitmap> >::iterator iter
    = m_state->m_bitmapMap.find(number);
  if (iter == m_state->m_bitmapMap.end() || !iter->second) {
    MWAW_DEBUG_MSG(("CWGraph::canSendBitmapAsGraphic: can not find bitmap %d\n", number));
    return false;
  }
  return true;
}

bool CWGraph::sendBitmap(int number, bool asGraphic, MWAWPosition const &pos)
{
  std::map<int, shared_ptr<CWGraphInternal::Bitmap> >::iterator iter
    = m_state->m_bitmapMap.find(number);
  if (iter == m_state->m_bitmapMap.end() || !iter->second) {
    MWAW_DEBUG_MSG(("CWGraph::sendBitmap: can not find bitmap %d\n", number));
    return false;
  }
  return sendBitmap(*iter->second, asGraphic, pos);
}

bool CWGraph::sendBitmap(CWGraphInternal::Bitmap &bitmap, bool asGraphic, MWAWPosition pos)
{
  if (!bitmap.m_entry.valid() || !bitmap.m_bitmapType)
    return false;

  if (asGraphic) {
    if  (!m_parserState->m_graphicListener ||
         !m_parserState->m_graphicListener->isDocumentStarted()) {
      MWAW_DEBUG_MSG(("CWGraph::sendBitmap: can not access to the graphic listener\n"));
      return true;
    }
  } else if (!m_parserState->m_listener)
    return true;
  int numColors = int(bitmap.m_colorMap.size());
  shared_ptr<MWAWPictBitmap> bmap;

  MWAWPictBitmapIndexed *bmapIndexed = 0;
  MWAWPictBitmapColor *bmapColor = 0;
  bool indexed = false;
  if (numColors > 2) {
    bmapIndexed =  new MWAWPictBitmapIndexed(bitmap.m_size);
    bmapIndexed->setColors(bitmap.m_colorMap);
    bmap.reset(bmapIndexed);
    indexed = true;
  } else
    bmap.reset((bmapColor=new MWAWPictBitmapColor(bitmap.m_size)));

  //! let go
  int fSz = bitmap.m_bitmapType;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(bitmap.m_entry.begin(), WPX_SEEK_SET);
  for (int r = 0; r < bitmap.m_size[1]; r++) {
    for (int c = 0; c < bitmap.m_size[0]; c++) {
      long val = (long) input->readULong(fSz);
      if (indexed) {
        bmapIndexed->set(c,r,(int)val);
        continue;
      }
      switch(fSz) {
      case 1:
        bmapColor->set(c,r, MWAWColor((unsigned char)val,(unsigned char)val,(unsigned char)val));
        break;
      case 2: // rgb compressed ?
        bmapColor->set(c,r, MWAWColor((unsigned char)(((val>>10)&0x1F) << 3),(unsigned char)(((val>>5)&0x1F) << 3),(unsigned char)(((val>>0)&0x1F) << 3)));
        break;
      case 4:
        bmapColor->set(c,r, MWAWColor(uint32_t(val)));
        break;
      default: {
        static bool first = true;
        if (first) {
          MWAW_DEBUG_MSG(("CWGraph::sendBitmap: unknown data size\n"));
          first = false;
        }
        break;
      }
      }
    }
  }

  WPXBinaryData data;
  std::string type;
  if (!bmap->getBinary(data,type)) return false;
  if (pos.size()[0] <= 0 || pos.size()[1] <= 0) {
    MWAW_DEBUG_MSG(("CWGraph::sendBitmap: can not find bitmap size\n"));
    pos.setSize(Vec2f(0,0));
  }
  if (asGraphic) {
    MWAWGraphicStyle style;
    style.m_lineWidth=0;
    m_parserState->m_graphicListener->insertPicture(Box2f(pos.origin(),pos.origin()+pos.size()), style, data, "image/pict");
  } else
    m_parserState->m_listener->insertPicture(pos, data, "image/pict");

  return true;
}

bool CWGraph::sendPicture(CWGraphInternal::ZonePict &pict,
                          MWAWPosition pos, WPXPropertyList extras)
{
  bool send = false;
  bool posOk = pos.size()[0] > 0 && pos.size()[1] > 0;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  MWAWContentListenerPtr listener=m_parserState->m_listener;
  for (int z = 0; z < 2; z++) {
    MWAWEntry entry = pict.m_entries[z];
    if (!entry.valid())
      continue;
    if (!posOk) {
      Vec2f sz=pict.m_box.size();
      // recheck that all is ok now
      if (sz[0]<0) sz[0]=0;
      if (sz[1]<0) sz[1]=0;
      pos.setSize(sz);
    }
    input->seek(entry.begin(), WPX_SEEK_SET);

    switch(pict.getSubType()) {
    case CWGraphInternal::Zone::T_Movie:
    case CWGraphInternal::Zone::T_Pict: {
      shared_ptr<MWAWPict> thePict(MWAWPictData::get(input, (int)entry.length()));
      if (thePict) {
        if (!send && listener) {
          WPXBinaryData data;
          std::string type;
          if (thePict->getBinary(data,type))
            listener->insertPicture(pos, data, type, extras);
        }
        send = true;
      }
      break;
    }
    case CWGraphInternal::Zone::T_Line:
    case CWGraphInternal::Zone::T_Rect:
    case CWGraphInternal::Zone::T_RectOval:
    case CWGraphInternal::Zone::T_Oval:
    case CWGraphInternal::Zone::T_Arc:
    case CWGraphInternal::Zone::T_Poly:
    case CWGraphInternal::Zone::T_Zone:
    case CWGraphInternal::Zone::T_Shape:
    case CWGraphInternal::Zone::T_Picture:
    case CWGraphInternal::Zone::T_Chart:
    case CWGraphInternal::Zone::T_DataBox:
    case CWGraphInternal::Zone::T_Unknown:
    case CWGraphInternal::Zone::T_QTim:
    default:
      if (!send && listener) {
        WPXBinaryData data;
        input->seek(entry.begin(), WPX_SEEK_SET);
        input->readDataBlock(entry.length(), data);
        listener->insertPicture(pos, data, "image/pict", extras);
      }
      send = true;
      break;
    }

#ifdef DEBUG_WITH_FILES
    m_parserState->m_asciiFile.skipZone(entry.begin(), entry.end()-1);
    WPXBinaryData file;
    input->seek(entry.begin(), WPX_SEEK_SET);
    input->readDataBlock(entry.length(), file);
    static int volatile pictName = 0;
    libmwaw::DebugStream f;
    f << "PICT-" << ++pictName;
    libmwaw::Debug::dumpFile(file, f.str().c_str());
#endif
  }
  return send;
}

void CWGraph::flushExtra()
{
  std::map<int, shared_ptr<CWGraphInternal::Group> >::iterator iter
    = m_state->m_groupMap.begin();
  for ( ; iter !=  m_state->m_groupMap.end(); ++iter) {
    shared_ptr<CWGraphInternal::Group> zone = iter->second;
    if (zone->m_parsed)
      continue;
    if (m_parserState->m_listener) m_parserState->m_listener->insertEOL();
    MWAWPosition pos(Vec2f(0,0),Vec2f(0,0),WPX_POINT);
    pos.setRelativePosition(MWAWPosition::Char);
    sendGroup(iter->first, false, pos);
  }
}

void CWGraph::setSlideList(std::vector<int> const &slideList)
{
  std::map<int, shared_ptr<CWGraphInternal::Group> >::iterator iter;
  for (size_t s=0; s < slideList.size(); s++) {
    iter = m_state->m_groupMap.find(slideList[s]);
    if (iter==m_state->m_groupMap.end() || !iter->second) {
      MWAW_DEBUG_MSG(("CWGraph::setSlideList: can find group %d\n", slideList[s]));
      continue;
    }
    iter->second->m_page=int(s+1);
  }
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
