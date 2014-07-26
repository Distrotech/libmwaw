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

#include "MWAWFont.hxx"
#include "MWAWGraphicEncoder.hxx"
#include "MWAWGraphicListener.hxx"
#include "MWAWGraphicShape.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWListener.hxx"
#include "MWAWParser.hxx"
#include "MWAWPictBitmap.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSpreadsheetEncoder.hxx"
#include "MWAWSpreadsheetListener.hxx"
#include "MWAWSubDocument.hxx"

#include "ClarisWksDocument.hxx"
#include "ClarisWksStruct.hxx"
#include "ClarisWksStyleManager.hxx"

#include "ClarisWksGraph.hxx"

#include "libmwaw_internal.hxx"

/** Internal: the structures of a ClarisWksGraph */
namespace ClarisWksGraphInternal
{
//! Internal: the structure used to a point of a ClarisWksGraph
struct CurvePoint {
  CurvePoint(Vec2f point=Vec2f()) : m_pos(point), m_type(1)
  {
    for (int i = 0; i < 2; i++) m_controlPoints[i] = point;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, CurvePoint const &pt)
  {
    o << pt.m_pos;
    if (pt.m_pos != pt.m_controlPoints[0])
      o << ":prev=" << pt.m_controlPoints[0];
    if (pt.m_pos != pt.m_controlPoints[1])
      o << ":next=" << pt.m_controlPoints[1];
    switch (pt.m_type) {
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

//! Internal: the structure used to store a style of a ClarisWksGraph
struct Style : public MWAWGraphicStyle {
  //! constructor
  Style(): MWAWGraphicStyle(), m_wrapping(0), m_lineFlags(0), m_surfacePatternType(0)
  {
  }
  //! returns the wrapping
  MWAWPosition::Wrapping getWrapping() const
  {
    switch (m_wrapping&3) {
    case 0:
      return MWAWPosition::WForeground;
      break;
    case 1:
    case 2:
      return MWAWPosition::WDynamic;
    default:
      break;
    }
    return MWAWPosition::WNone;
  }
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Style const &st)
  {
    o << static_cast<MWAWGraphicStyle const &>(st);
    switch (st.m_wrapping & 3) {
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
    switch (st.m_surfacePatternType) {
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
  //! the wrap type
  int m_wrapping;
  //! the line flags
  int m_lineFlags;
  //! the surface pattern type
  int m_surfacePatternType;
};

//! Internal: the generic structure used to store a zone of a ClarisWksGraph
struct Zone {
  //! the list of types
  enum Type { T_Zone, T_Zone2, T_Shape, T_Picture, T_Chart, T_DataBox, T_Unknown,
              /* basic subtype */
              T_Line, T_Rect, T_RectOval, T_Oval, T_Arc, T_Poly,
              /* picture subtype */
              T_Pict, T_QTim, T_Movie
            };
  //! constructor
  Zone() : m_page(-1), m_box(), m_ordering(-1), m_style() {}
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Zone const &zone)
  {
    if (zone.m_page >= 0) o << "pg=" << zone.m_page << ",";
    o << "box=" << zone.m_box << ",";
    if (zone.m_ordering>0) o << "ordering=" << zone.m_ordering << ",";
    zone.print(o);
    o << "style=[" << zone.m_style << "],";
    return o;
  }
  //! destructor
  virtual ~Zone() {}
  //! return the zone bdbox
  Box2f getBdBox() const
  {
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
  virtual Type getType() const
  {
    return T_Unknown;
  }
  //! return the subtype
  virtual Type getSubType() const
  {
    return T_Unknown;
  }
  //! return the number of data to define this zone in the file
  virtual int getNumData(int /*version*/) const
  {
    return 0;
  }
  //! print the data contains
  virtual void print(std::ostream &) const { }
  //! return a child corresponding to this zone
  virtual ClarisWksStruct::DSET::Child getChild() const
  {
    ClarisWksStruct::DSET::Child child;
    child.m_box = m_box;
    return child;
  }
  //! returns the id of the reference zone
  virtual int getZoneId() const
  {
    return 0;
  }
  //! returns true if the zone can be send using a graphic listener (partial check)
  virtual bool canBeSendAsGraphic() const
  {
    ClarisWksGraphInternal::Zone::Type type=getType();
    return type==ClarisWksGraphInternal::Zone::T_Zone || type==ClarisWksGraphInternal::Zone::T_Shape ||
           type==ClarisWksGraphInternal::Zone::T_DataBox || type==ClarisWksGraphInternal::Zone::T_Chart ||
           type==ClarisWksGraphInternal::Zone::T_Unknown;
  }
  //! the page (checkme: or frame linked )
  int m_page;
  //! the bdbox
  Box2f m_box;
  //! the ordering
  int m_ordering;
  //! the style
  Style m_style;
};

//! Internal: small class to store a basic graphic zone of a ClarisWksGraph
struct ZoneShape : public Zone {
  //! constructor
  ZoneShape(Zone const &z, Type type) : Zone(z), m_type(type), m_shape(), m_rotate(0)
  {
  }
  //! print the data
  virtual void print(std::ostream &o) const
  {
    o << m_shape;
    if (m_rotate) o << "rot=" << m_rotate << ",";
  }
  //! return the main type
  virtual Type getType() const
  {
    return T_Shape;
  }
  //! return the sub type
  virtual Type getSubType() const
  {
    return m_type;
  }
  //! return the number of data
  virtual int getNumData(int /*version*/) const
  {
    if (m_shape.m_type == MWAWGraphicShape::Polygon) return 1;
    return 0;
  }
  //! return a child corresponding to this zone
  virtual ClarisWksStruct::DSET::Child getChild() const
  {
    ClarisWksStruct::DSET::Child child;
    child.m_box = m_box;
    child.m_type = ClarisWksStruct::DSET::C_Graphic;
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
  ZonePict(Zone const &z, Type type) : Zone(z), m_type(type)
  {
  }
  //! print the data
  virtual void print(std::ostream &o) const
  {
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
    case T_Zone2:
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
  virtual Type getType() const
  {
    return T_Picture;
  }
  //! return the sub type
  virtual Type getSubType() const
  {
    return m_type;
  }
  //! return the number of data in a file
  virtual int getNumData(int /*version*/) const
  {
    return 2;
  }
  //! return a child corresponding to this zone
  virtual ClarisWksStruct::DSET::Child getChild() const
  {
    ClarisWksStruct::DSET::Child child;
    child.m_box = m_box;
    child.m_type = ClarisWksStruct::DSET::C_Graphic;
    return child;
  }

  //! the sub type
  Type m_type;
  //! the picture entry followed by a ps entry or ole entry ( if defined)
  MWAWEntry m_entries[2];
};

//! Internal: structure to store a bitmap of a ClarisWksGraph
struct Bitmap : public ClarisWksStruct::DSET {
  //! constructor
  Bitmap(ClarisWksStruct::DSET const &dset = ClarisWksStruct::DSET()) :
    DSET(dset), m_numBytesPerPixel(0), m_bitmapSize(0,0), m_bitmapRowSize(0), m_entry(), m_colorMap()
  {
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Bitmap const &bt)
  {
    o << static_cast<ClarisWksStruct::DSET const &>(bt);
    if (bt.m_numBytesPerPixel > 0) o << "type=" << bt.m_numBytesPerPixel << ",";
    else if (bt.m_numBytesPerPixel < 0) o << "type=1/" << (-bt.m_numBytesPerPixel) << ",";
    return o;
  }

  //! the number of bite by pixel
  int m_numBytesPerPixel;
  //! the bitmap size
  Vec2i m_bitmapSize;
  //! the bitmap row size in the file ( with potential alignment)
  int m_bitmapRowSize;
  //! the bitmap entry
  MWAWEntry m_entry;
  //! the color map
  std::vector<MWAWColor> m_colorMap;
};

//! Internal: structure to store a link to a zone of a ClarisWksGraph
struct ZoneZone : public Zone {
  //! constructor
  ZoneZone(Zone const &z, Type fileType) : Zone(z), m_subType(fileType), m_id(-1), m_subId(-1), m_frameId(-1), m_frameSubId(-1), m_frameLast(true), m_styleId(-1), m_wrappingSep(5)
  {
    for (int i = 0; i < 9; i++)
      m_flags[i] = 0;
  }
  //! print the zone
  virtual void print(std::ostream &o) const
  {
    if (m_subType==T_Zone2) {
      o << "ZONE2" << ",";
      return;
    }
    o << "ZONE, id=" << m_id << ",";
    if (m_subId > 0) o << "subId=" << m_subId << ",";
    if (m_styleId >= 0) o << "styleId=" << m_styleId << ",";
    if (m_wrappingSep != 5) o << "wrappingSep=" << m_wrappingSep << ",";
    for (int i = 0; i < 9; i++) {
      if (m_flags[i]) o << "fl" << i << "=" << m_flags[i] << ",";
    }
  }
  //! return the main type Zone
  virtual Type getType() const
  {
    return T_Zone;
  }
  //! return the sub type Zone
  virtual Type getSubType() const
  {
    return m_subType;
  }
  //! return the number of data to define this zone in the file
  virtual int getNumData(int /*version*/) const
  {
    return m_subType==T_Zone ? 0 : 1;
  }
  //! returns true if the zone can be send using a graphic listener (partial check)
  virtual bool canBeSendAsGraphic() const
  {
    return !isLinked();
  }
  //! returns the id of the reference zone
  virtual int getZoneId() const
  {
    return m_id;
  }
  /** check if we need to send the frame is linked to another frmae */
  bool isLinked() const
  {
    return m_frameId>=0 && m_frameSubId>=0;
  }
  /** add the frame name if needed */
  bool addFrameName(MWAWGraphicStyle &style) const
  {
    if (!isLinked()) return false;
    if (m_frameSubId>0) {
      librevenge::RVNGString fName;
      fName.sprintf("Frame%d-%d", m_frameId, m_frameSubId);
      style.m_frameName=fName.cstr();
    }
    if (!m_frameLast) {
      librevenge::RVNGString fName;
      fName.sprintf("Frame%d-%d", m_frameId, m_frameSubId+1);
      style.m_frameNextName=fName.cstr();
    }
    return true;
  }

  //! return a child corresponding to this zone
  virtual ClarisWksStruct::DSET::Child getChild() const
  {
    ClarisWksStruct::DSET::Child child;
    child.m_box = m_box;
    child.m_id = m_id;
    child.m_type = ClarisWksStruct::DSET::C_Zone;
    return child;
  }

  //! the file type ( zone or zone2 )
  Type m_subType;
  //! the zoneId
  int m_id;
  //! the zoneSubId: can be page/column/frame linked number
  int m_subId;
  //! the frame id (for a linked frame)
  int m_frameId;
  //! the frame sub id (for a linked frame)
  int m_frameSubId;
  //! true if this is the last frame of a frame zone
  bool m_frameLast;
  //! the style id
  int m_styleId;
  //! the wrapping separator
  int m_wrappingSep;
  //! flag
  int m_flags[9];
};

//! Internal: structure used to store a chart zone of a ClarisWksGraph
struct Chart : public Zone {
  //! construtor
  Chart(Zone const &z) : Zone(z)
  {
  }
  //! print the zone
  virtual void print(std::ostream &o) const
  {
    o << "CHART,";
  }
  //! return the main type
  virtual Type getType() const
  {
    return T_Chart;
  }
  //! return the sub type
  virtual Type getSubType() const
  {
    return T_Chart;
  }
  //! return the number of data
  virtual int getNumData(int version) const
  {
    return version==1 ? 1 : 2;
  }
  //! return a child corresponding to this zone
  virtual ClarisWksStruct::DSET::Child getChild() const
  {
    ClarisWksStruct::DSET::Child child;
    child.m_box = m_box;
    child.m_type = ClarisWksStruct::DSET::C_Graphic;
    return child;
  }
};

//! Internal: structure used to store an unknown zone of a ClarisWksGraph
struct ZoneUnknown : public Zone {
  //! construtor
  ZoneUnknown(Zone const &z) : Zone(z), m_type(T_Unknown), m_typeId(-1)
  {
  }
  //! print the zone
  virtual void print(std::ostream &o) const
  {
    switch (m_type) {
    case T_DataBox:
      o << "BOX(database),";
      break;
    case T_Chart:
    case T_Zone:
    case T_Zone2:
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
  virtual Type getType() const
  {
    return m_type;
  }
  //! return the sub type
  virtual Type getSubType() const
  {
    return m_type;
  }
  //! return the number of data
  virtual int getNumData(int /*version*/) const
  {
    return 0;
  }
  //! return a child corresponding to this zone
  virtual ClarisWksStruct::DSET::Child getChild() const
  {
    ClarisWksStruct::DSET::Child child;
    child.m_box = m_box;
    child.m_type = ClarisWksStruct::DSET::C_Graphic;
    return child;
  }

  //! the sub type
  Type m_type;
  //! type number
  int m_typeId;
};

////////////////////////////////////////
//! Internal: class which stores a group of graphics, ...
struct Group : public ClarisWksStruct::DSET {
  //! constructor
  Group(ClarisWksStruct::DSET const &dset = ClarisWksStruct::DSET()) :
    ClarisWksStruct::DSET(dset), m_zones(), m_hasMainZone(false),
    m_zonesToSend()
  {
    m_page=0;
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Group const &doc)
  {
    o << static_cast<ClarisWksStruct::DSET const &>(doc);
    return o;
  }
  /** remove a child from a list.

      Normally, this function is not called, so optimizing it is not usefull
   */
  virtual void removeChild(int cId, bool normalChild)
  {
    DSET::removeChild(cId, normalChild);
    std::vector<shared_ptr<Zone> >::iterator it;
    for (it=m_zones.begin(); it!=m_zones.end(); ++it) {
      shared_ptr<ClarisWksGraphInternal::Zone> child = *it;
      if (!child || child->getType() != ClarisWksGraphInternal::Zone::T_Zone)
        continue;
      ClarisWksGraphInternal::ZoneZone const &childZone =
        static_cast<ClarisWksGraphInternal::ZoneZone &>(*child);
      if (childZone.m_id != cId) continue;
      m_zones.erase(it);
      return;
    }
    MWAW_DEBUG_MSG(("ClarisWksGraphInternal::Group can not detach %d\n", cId));
  }

  /** the list of child zones */
  std::vector<shared_ptr<Zone> > m_zones;

  //! a flag to know if this zone contains or no the call to zone 1
  bool m_hasMainZone;
  //! the list of block to send
  std::vector<shared_ptr<Zone> > m_zonesToSend;
};

////////////////////////////////////////
//! Internal: the state of a ClarisWksGraph
struct State {
  //! constructor
  State() : m_numPages(0), m_groupMap(), m_bitmapMap(), m_frameId(0), m_positionsComputed(false), m_ordering(0) { }
  /** returns a new ordering.

      \note: the shapes seem to appear in increasing ordering, so we can use this function.
   */
  int getOrdering() const
  {
    return ++m_ordering;
  }
  //! the number of pages
  int m_numPages;
  //! a map zoneId -> group
  std::map<int, shared_ptr<Group> > m_groupMap;
  //! a map zoneId -> group
  std::map<int, shared_ptr<Bitmap> > m_bitmapMap;
  //! a int used to defined linked frame
  int m_frameId;
  //! true if the ClarisWksGraph::computePositions was called
  bool m_positionsComputed;
  //! the last ordering used
  mutable int m_ordering;
};

////////////////////////////////////////
//! Internal: the subdocument of a ClarisWksGraph
class SubDocument : public MWAWSubDocument
{
public:
  SubDocument(ClarisWksGraph &pars, MWAWInputStreamPtr input, int zoneId, MWAWPosition pos=MWAWPosition()) :
    MWAWSubDocument(pars.m_mainParser, input, MWAWEntry()), m_graphParser(&pars), m_id(zoneId), m_position(pos) {}

  //! destructor
  virtual ~SubDocument() {}

  //! operator!=
  virtual bool operator!=(MWAWSubDocument const &doc) const
  {
    if (MWAWSubDocument::operator!=(doc)) return true;
    SubDocument const *sDoc = dynamic_cast<SubDocument const *>(&doc);
    if (!sDoc) return true;
    if (m_graphParser != sDoc->m_graphParser) return true;
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
  /** the graph parser */
  ClarisWksGraph *m_graphParser;

protected:
  //! the subdocument id
  int m_id;
  //! the position if known
  MWAWPosition m_position;
private:
  SubDocument(SubDocument const &orig);
  SubDocument &operator=(SubDocument const &orig);
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType type)
{
  if (!listener || (type==libmwaw::DOC_TEXT_BOX&&!listener->canWriteText())) {
    MWAW_DEBUG_MSG(("ClarisWksGraphInternal::SubDocument::parse: no listener\n"));
    return;
  }
  assert(m_graphParser);
  long pos = m_input->tell();
  m_graphParser->askToSend(m_id,listener,m_position);
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
}

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
ClarisWksGraph::ClarisWksGraph(ClarisWksDocument &document) :
  m_document(document), m_parserState(document.m_parserState), m_state(new ClarisWksGraphInternal::State),
  m_mainParser(&document.getMainParser())
{
}

ClarisWksGraph::~ClarisWksGraph()
{ }

int ClarisWksGraph::version() const
{
  return m_parserState->m_version;
}

void ClarisWksGraph::computePositions() const
{
  if (m_state->m_positionsComputed) return;
  m_state->m_positionsComputed=true;
  std::map<int, shared_ptr<ClarisWksGraphInternal::Group> >::iterator iter;
  for (iter=m_state->m_groupMap.begin() ; iter != m_state->m_groupMap.end() ; ++iter) {
    shared_ptr<ClarisWksGraphInternal::Group> group = iter->second;
    if (!group || group->m_position == ClarisWksStruct::DSET::P_Slide) continue;
    updateGroup(*group);
  }
}

int ClarisWksGraph::numPages() const
{
  if (m_state->m_numPages>0) return m_state->m_numPages;
  computePositions();

  int nPages = 1;
  std::map<int, shared_ptr<ClarisWksGraphInternal::Group> >::iterator iter;
  for (iter=m_state->m_groupMap.begin() ; iter != m_state->m_groupMap.end() ; ++iter) {
    shared_ptr<ClarisWksGraphInternal::Group> group = iter->second;
    if (!group) continue;
    int lastPage=group->getMaximumPage();
    if (lastPage>nPages) nPages=lastPage;
  }
  m_state->m_numPages=nPages;
  return nPages;
}

void ClarisWksGraph::askToSend(int number, MWAWListenerPtr listener, MWAWPosition const &pos)
{
  m_document.sendZone(number, listener, pos);
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////
bool ClarisWksGraph::getSurfaceColor(ClarisWksGraphInternal::Style const &style, MWAWColor &col) const
{
  if (!style.hasSurfaceColor())
    return false;
  col = style.m_surfaceColor;
  return true;
}

////////////////////////////////////////////////////////////
// a group of data mainly graphic
////////////////////////////////////////////////////////////
shared_ptr<ClarisWksStruct::DSET> ClarisWksGraph::readGroupZone
(ClarisWksStruct::DSET const &zone, MWAWEntry const &entry, bool &complete)
{
  complete = false;
  if (!entry.valid() || zone.m_fileType != 0)
    return shared_ptr<ClarisWksStruct::DSET>();
  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos+8+16, librevenge::RVNG_SEEK_SET); // avoid header+8 generic number
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  shared_ptr<ClarisWksGraphInternal::Group> group(new ClarisWksGraphInternal::Group(zone));

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
      MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupZone: can not find definition size\n"));
      input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
      return shared_ptr<ClarisWksStruct::DSET>();
    }

    MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupZone: unexpected size for zone definition, try to continue\n"));
  }

  long beginDefGroup = entry.end()-N*data0Length;
  if (long(input->tell())+42 <= beginDefGroup) {
    input->seek(beginDefGroup-42, librevenge::RVNG_SEEK_SET);
    pos = input->tell();
    if (!readGroupUnknown(*group, 42, -1)) {
      ascFile.addPos(pos);
      ascFile.addNote("GroupDef(Head-###)");
    }
  }

  input->seek(beginDefGroup, librevenge::RVNG_SEEK_SET);

  for (int i = 0; i < N; i++) {
    pos = input->tell();
    MWAWEntry gEntry;
    gEntry.setBegin(pos);
    gEntry.setLength(data0Length);
    shared_ptr<ClarisWksGraphInternal::Zone> def = readGroupDef(gEntry);
    group->m_zones.push_back(def);

    if (!def) {
      f.str("");
      f << "GroupDef#";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    input->seek(gEntry.end(), librevenge::RVNG_SEEK_SET);
  }

  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);

  if (readGroupData(*group, entry.begin())) {
    // fixme: do something here
  }

  group->m_childs.resize(group->m_zones.size());
  for (size_t i = 0; i < group->m_zones.size(); ++i) {
    if (!group->m_zones[i]) continue;
    group->m_childs[size_t(i)] = group->m_zones[i]->getChild();
  }

  if (m_state->m_groupMap.find(group->m_id) != m_state->m_groupMap.end()) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupZone: zone %d already exists!!!\n", group->m_id));
  }
  else
    m_state->m_groupMap[group->m_id] = group;

  return group;
}

////////////////////////////////////////////////////////////
// read a bitmap zone
////////////////////////////////////////////////////////////
shared_ptr<ClarisWksStruct::DSET> ClarisWksGraph::readBitmapZone
(ClarisWksStruct::DSET const &zone, MWAWEntry const &entry, bool &complete)
{
  complete = false;
  if (!entry.valid() || zone.m_fileType != 4)
    return shared_ptr<ClarisWksStruct::DSET>();
  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos+8+16, librevenge::RVNG_SEEK_SET); // avoid header+8 generic number
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  shared_ptr<ClarisWksGraphInternal::Bitmap> bitmap(new ClarisWksGraphInternal::Bitmap(zone));

  f << "Entries(BitmapDef):" << *bitmap << ",";

  ascFile.addDelimiter(input->tell(), '|');

  // read the last part
  long data0Length = zone.m_dataSz;
  long N = zone.m_numData;
  if (entry.length() -8-12 != data0Length*N + zone.m_headerSz) {
    if (data0Length == 0 && N) {
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      MWAW_DEBUG_MSG(("ClarisWksGraph::readBitmapZone: can not find definition size\n"));
      input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
      return shared_ptr<ClarisWksStruct::DSET>();
    }

    MWAW_DEBUG_MSG(("ClarisWksGraph::readBitmapZone: unexpected size for zone definition, try to continue\n"));
  }

  bool sizeSet=false;
  int sizePos = (version() == 1) ? 0: 88;
  if (sizePos && pos+sizePos+4+N*data0Length < entry.end()) {
    input->seek(pos+sizePos, librevenge::RVNG_SEEK_SET);
    ascFile.addDelimiter(pos+sizePos,'[');
    int dim[2]; // ( we must add 2 to add the border )
    for (int j = 0; j < 2; j++)
      dim[j] = (int) input->readLong(2);
    f << "sz=" << dim[1] << "x" << dim[0] << ",";
    if (dim[0] > 0 && dim[1] > 0) {
      bitmap->m_bitmapSize = Vec2i(dim[1]+2, dim[0]+2);
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

  input->seek(entry.end()-N*data0Length, librevenge::RVNG_SEEK_SET);

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
      bitmap->m_bitmapSize = Vec2i(dim[0]+2, dim[1]+2);

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
    input->seek(gEntry.end(), librevenge::RVNG_SEEK_SET);
  }

  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
  pos = entry.end();
  bool ok = readBitmapColorMap(bitmap->m_colorMap);
  if (ok) {
    pos = input->tell();
    ok = readBitmapData(*bitmap);
  }
  if (!ok)
    input->seek(pos, librevenge::RVNG_SEEK_SET);

  // fixme: in general followed by another zone
  bitmap->m_otherChilds.push_back(bitmap->m_id+1);

  if (m_state->m_bitmapMap.find(bitmap->m_id) != m_state->m_bitmapMap.end()) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupZone: zone %d already exists!!!\n", bitmap->m_id));
  }
  else
    m_state->m_bitmapMap[bitmap->m_id] = bitmap;

  return bitmap;
}

////////////////////////////////////////////////////////////
// definition of an element of a group
////////////////////////////////////////////////////////////
shared_ptr<ClarisWksGraphInternal::Zone> ClarisWksGraph::readGroupDef(MWAWEntry const &entry)
{
  shared_ptr<ClarisWksGraphInternal::Zone> res;
  if (entry.length() < 32) {
    if (version() > 1 || entry.length() < 30) {
      MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupDef: sz is too short!!!\n"));
      return res;
    }
  }
  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "GroupDef:";

  ClarisWksGraphInternal::Zone zone;
  ClarisWksGraphInternal::Style &style = zone.m_style;

  int typeId = (int) input->readULong(1);
  ClarisWksGraphInternal::Zone::Type type = ClarisWksGraphInternal::Zone::T_Unknown;
  int const vers=version();
  switch (vers) {
  case 1:
    switch (typeId) {
    case 1:
      type = ClarisWksGraphInternal::Zone::T_Zone;
      break;
    case 4:
      type = ClarisWksGraphInternal::Zone::T_Line;
      break;
    case 5:
      type = ClarisWksGraphInternal::Zone::T_Rect;
      break;
    case 6:
      type = ClarisWksGraphInternal::Zone::T_RectOval;
      break;
    case 7:
      type = ClarisWksGraphInternal::Zone::T_Oval;
      break;
    case 8:
      type = ClarisWksGraphInternal::Zone::T_Arc;
      break;
    case 9:
      type = ClarisWksGraphInternal::Zone::T_Poly;
      break;
    case 11:
      type = ClarisWksGraphInternal::Zone::T_Pict;
      break;
    case 12:
      type = ClarisWksGraphInternal::Zone::T_Chart;
      MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupDef: find some chart, not implemented!\n"));
      break;
    case 13:
      type = ClarisWksGraphInternal::Zone::T_DataBox;
      break;
    default:
      MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupDef: find unknown type=%d!!!\n", typeId));
      f << "###typeId=" << typeId << ",";
      break;
    }
    break;
  default:
    switch (typeId) {
    case 1:
      type = ClarisWksGraphInternal::Zone::T_Zone;
      break;
    case 2:
      type = ClarisWksGraphInternal::Zone::T_Line;
      break;
    case 3:
      type = ClarisWksGraphInternal::Zone::T_Rect;
      break;
    case 4:
      type = ClarisWksGraphInternal::Zone::T_RectOval;
      break;
    case 5:
      type = ClarisWksGraphInternal::Zone::T_Oval;
      break;
    case 6:
      type = ClarisWksGraphInternal::Zone::T_Arc;
      break;
    case 7:
      type = ClarisWksGraphInternal::Zone::T_Poly;
      break;
    case 8:
      type = ClarisWksGraphInternal::Zone::T_Pict;
      break;
    case 9:
      type = ClarisWksGraphInternal::Zone::T_Chart;
      MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupDef: find some chart, not implemented!\n"));
      break;
    case 10:
      type = ClarisWksGraphInternal::Zone::T_DataBox;
      break;
    case 11: // old method to define a textbox ?
      type = ClarisWksGraphInternal::Zone::T_Zone2;
      break;
    case 14:
      type = ClarisWksGraphInternal::Zone::T_Movie;
      break;
    case 18:
      type = ClarisWksGraphInternal::Zone::T_QTim;
      break;
    default:
      MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupDef: find unknown type=%d!!!\n", typeId));
      f << "###typeId=" << typeId << ",";
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
    if (!m_document.getStyleManager()->getColor(col, color)) {
      MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupDef: unknown color!!!\n"));
      f << "###col" << j << "=" << col << ",";
    }
    else if (j==0)
      style.m_lineColor=color;
    else
      style.setSurfaceColor(color);
  }

  for (int j = 0; j < 2; j++) {
    if (vers > 1) {
      val = (int) input->readULong(1);  // probably also related to surface
      if (val) f << "pat" << j << "[high]=" << val << ",";
    }
    int pat = (int) input->readULong(1);
    if (j==1 && style.m_surfacePatternType) {
      if (style.m_surfacePatternType==1) { // wall paper
        if (!m_document.getStyleManager()->updateWallPaper(pat, style))
          f << "##wallId=" << pat << ",";
      }
      else {
        f << "###surfaceType=" << style.m_surfacePatternType << ",";
        MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupDef: unknown surface type!!!\n"));
      }
      continue;
    }
    if (j==1 && (style.m_lineFlags & 0x10)) {
      m_document.getStyleManager()->updateGradient(pat, style);
      continue;
    }
    if (pat==1) {
      if (j==0) style.m_lineOpacity=0;
      else style.m_surfaceOpacity=0;
      continue;
    }
    MWAWGraphicStyle::Pattern pattern;
    float percent;
    if (!m_document.getStyleManager()->getPattern(pat,pattern,percent)) {
      MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupDef: unknown pattern!!!\n"));
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
  // checkme: look like an ordering, minimal value corresponding to front, actually unused
  zone.m_ordering = (int) input->readLong(2);

  switch (type) {
  case ClarisWksGraphInternal::Zone::T_Zone: {
    int nFlags = 0;
    ClarisWksGraphInternal::ZoneZone *z = new ClarisWksGraphInternal::ZoneZone(zone, type);
    res.reset(z);
    z->m_flags[nFlags++] = (int) input->readLong(2);
    z->m_id = (int) input->readULong(2);

    int numRemains = int(entry.end()-long(input->tell()));
    numRemains/=2;
    // v1-2:3 v4-v5:6 v6:8
    if (numRemains > 8) numRemains = 8;
    for (int j = 0; j < numRemains; j++) {
      val = (int) input->readLong(2);
      switch (j) {
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
  case ClarisWksGraphInternal::Zone::T_Zone2:  { // checkme: read the end of the zone
    ClarisWksGraphInternal::ZoneZone *z = new ClarisWksGraphInternal::ZoneZone(zone, type);
    res.reset(z);
    break;
  }
  case ClarisWksGraphInternal::Zone::T_Pict:
  case ClarisWksGraphInternal::Zone::T_QTim:
  case ClarisWksGraphInternal::Zone::T_Movie:
    res.reset(new ClarisWksGraphInternal::ZonePict(zone, type));
    break;
  case ClarisWksGraphInternal::Zone::T_Line:
  case ClarisWksGraphInternal::Zone::T_Rect:
  case ClarisWksGraphInternal::Zone::T_RectOval:
  case ClarisWksGraphInternal::Zone::T_Oval:
  case ClarisWksGraphInternal::Zone::T_Arc:
  case ClarisWksGraphInternal::Zone::T_Poly: {
    ClarisWksGraphInternal::ZoneShape *z = new ClarisWksGraphInternal::ZoneShape(zone, type);
    res.reset(z);
    readShape(entry, *z);
    break;
  }
  case ClarisWksGraphInternal::Zone::T_Chart:
    res.reset(new ClarisWksGraphInternal::Chart(zone));
    break;
  case ClarisWksGraphInternal::Zone::T_DataBox:
  case ClarisWksGraphInternal::Zone::T_Shape:
  case ClarisWksGraphInternal::Zone::T_Picture:
  case ClarisWksGraphInternal::Zone::T_Unknown:
  default: {
    ClarisWksGraphInternal::ZoneUnknown *z = new ClarisWksGraphInternal::ZoneUnknown(zone);
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
  input->seek(entry.end(), librevenge::RVNG_SEEK_SET);

  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  return res;
}

////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// extra data associated to some group's element
////////////////////////////////////////////////////////////
bool ClarisWksGraph::readGroupData(ClarisWksGraphInternal::Group &group, long beginGroupPos)
{
  //  bool complete = false;
  if (!readGroupHeader(group)) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupData: unexpected graphic1\n"));
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
    shared_ptr<ClarisWksGraphInternal::Zone> z = group.m_zones[i];
    int numZoneExpected = z ? z->getNumData(vers) : 0;

    if (numZoneExpected) {
      pos = input->tell();
      sz = (long) input->readULong(4);
      f.str("");
      if (sz == 0) {
        MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupData: find a nop zone for type: %d\n",
                        z->getSubType()));
        ascFile.addPos(pos);
        ascFile.addNote("GroupDef-before:###");
        if (!numError++) {
          ascFile.addPos(beginGroupPos);
          ascFile.addNote("###");
        }
        else {
          MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupData: too many errors, zone parsing STOPS\n"));
          return false;
        }
        pos = input->tell();
        sz = (long) input->readULong(4);
      }
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      bool parsed = true;
      switch (z->getSubType()) {
      case ClarisWksGraphInternal::Zone::T_QTim:
        if (!readQTimeData(z))
          return false;
        break;
      case ClarisWksGraphInternal::Zone::T_Movie:
      // FIXME: pict ( containing movie ) + ??? +
      case ClarisWksGraphInternal::Zone::T_Pict:
        if (!readPictData(z))
          return false;
        break;
      case ClarisWksGraphInternal::Zone::T_Poly:
        if (z->getNumData(vers) && !readPolygonData(z))
          return false;
        break;
      case ClarisWksGraphInternal::Zone::T_Zone2: {
        ClarisWksGraphInternal::ZoneZone *child=dynamic_cast<ClarisWksGraphInternal::ZoneZone *>(z.get());
        if (!child) {
          MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupData: can not retrieve the zone for type 11\n"));
          parsed=false;
          break;
        }
        if (!input->checkPosition(pos+4+sz) || sz< 0x1a) {
          MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupData: the size of the 11 zone seems bad\n"));
          parsed=false;
          break;
        }
        if (vers!=2) {
          // checkme: only find in v2, so may cause problem for other version
          MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupData: called on a type 11 zone\n"));
        }
        input->seek(pos+4, librevenge::RVNG_SEEK_SET);
        f.str("");
        f << "Entries(Zone2Data):";
        int val;
        for (int j=0; j<2; ++j) { // f0=2, f1=1
          val=(int) input->readULong(1);
          if (val) f << "f" << j << "=" << val << ",";
        }
        val=(int) input->readULong(2); // 6e
        if (val) f << "f2=" << val << ",";
        f << "ids=[";
        for (int j=0; j<2; ++j)
          f << input->readULong(4) << ",";;
        f << "],";
        val=(int) input->readULong(2); // always 0
        if (val) f << "f3=" << val << ",";
        child->m_id=(int) input->readULong(2);
        f << "child=" << child->m_id << ",";
        ascFile.addDelimiter(input->tell(),'|');
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        input->seek(pos+4+sz, librevenge::RVNG_SEEK_SET);
        break;
      }
      case ClarisWksGraphInternal::Zone::T_Chart:
        if (!readChartData(z))
          return false;
        break;
      case ClarisWksGraphInternal::Zone::T_Line:
      case ClarisWksGraphInternal::Zone::T_Rect:
      case ClarisWksGraphInternal::Zone::T_RectOval:
      case ClarisWksGraphInternal::Zone::T_Oval:
      case ClarisWksGraphInternal::Zone::T_Arc:
      case ClarisWksGraphInternal::Zone::T_Zone:
      case ClarisWksGraphInternal::Zone::T_Shape:
      case ClarisWksGraphInternal::Zone::T_Picture:
      case ClarisWksGraphInternal::Zone::T_DataBox:
      case ClarisWksGraphInternal::Zone::T_Unknown:
      default:
        parsed = false;
        break;
      }

      if (!parsed) {
        if (!input->checkPosition(pos+4+sz)) {
          input->seek(pos, librevenge::RVNG_SEEK_SET);
          MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupData: find a odd zone for type: %d\n",
                          z->getSubType()));
          return false;
        }
        f.str("");
        f << "Entries(UnknownDATA)-" << z->getSubType();
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        input->seek(pos+4+sz, librevenge::RVNG_SEEK_SET);
        if (numZoneExpected==2) {
          pos = input->tell();
          sz = (long) input->readULong(4);
          if (sz) {
            input->seek(pos, librevenge::RVNG_SEEK_SET);
            MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupData: two zones is not implemented for zone: %d\n",
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
      else if (numZoneExpected) {
        MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupData: find not null entry for a end of zone: %d\n", z->getSubType()));
        ascFile.addPos(pos);
        ascFile.addNote("GroupDef[##extra2]");
      }
      input->seek(pos, librevenge::RVNG_SEEK_SET);
    }
  }

  if (input->isEnd())
    return true;
  // sanity check: normaly no zero except maybe for the last zone
  pos=input->tell();
  sz=(long) input->readULong(4);
  int numUnparsed=0;
  while (input->checkPosition(pos+4+sz)) {
    // this can happens at the end of the file (and it is normal)
    if (!input->checkPosition(pos+4+sz+10))
      break;
    static bool isFirst=true;
    if (isFirst) {
      MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupData: find some extra block\n"));
      isFirst=false;
    }
    input->seek(pos+4+sz, librevenge::RVNG_SEEK_SET);
    f.str("");
    f << "GroupDef[end-" << numUnparsed++ << "]: ###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    pos=input->tell();
    sz=(long) input->readULong(4);
  }
  input->seek(pos, librevenge::RVNG_SEEK_SET);

  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////
bool ClarisWksGraph::readShape(MWAWEntry const &entry, ClarisWksGraphInternal::ZoneShape &zone)
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
  switch (zone.getSubType()) {
  case ClarisWksGraphInternal::Zone::T_Line: {
    int yDeb=(zone.m_style.m_lineFlags&1)?1:0;
    shape = MWAWGraphicShape::line(Vec2f(zone.m_box[0][0],zone.m_box[yDeb][1]),
                                   Vec2f(zone.m_box[1][0],zone.m_box[1-yDeb][1]));
    break;
  }
  case ClarisWksGraphInternal::Zone::T_Rect:
    shape.m_type = MWAWGraphicShape::Rectangle;
    break;
  case ClarisWksGraphInternal::Zone::T_Oval:
    shape.m_type = MWAWGraphicShape::Circle;
    break;
  case ClarisWksGraphInternal::Zone::T_Poly:
    shape.m_type = MWAWGraphicShape::Polygon;
    break; // value are defined in next zone
  case ClarisWksGraphInternal::Zone::T_Arc: {
    if (remainBytes < 4) {
      MWAW_DEBUG_MSG(("ClarisWksGraph::readSimpleGraphicZone: arc zone is too short\n"));
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
  case ClarisWksGraphInternal::Zone::T_RectOval: {
    shape.m_type = MWAWGraphicShape::Rectangle;
    if (remainBytes < 8) {
      MWAW_DEBUG_MSG(("ClarisWksGraph::readSimpleGraphicZone: arc zone is too short\n"));
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
  case ClarisWksGraphInternal::Zone::T_Zone:
  case ClarisWksGraphInternal::Zone::T_Zone2:
  case ClarisWksGraphInternal::Zone::T_Shape:
  case ClarisWksGraphInternal::Zone::T_Picture:
  case ClarisWksGraphInternal::Zone::T_Chart:
  case ClarisWksGraphInternal::Zone::T_DataBox:
  case ClarisWksGraphInternal::Zone::T_Unknown:
  case ClarisWksGraphInternal::Zone::T_Pict:
  case ClarisWksGraphInternal::Zone::T_QTim:
  case ClarisWksGraphInternal::Zone::T_Movie:
  default:
    MWAW_DEBUG_MSG(("ClarisWksGraph::readSimpleGraphicZone: unknown type\n"));
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
    input->seek(nextToRead, librevenge::RVNG_SEEK_SET);
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

bool ClarisWksGraph::readGroupHeader(ClarisWksGraphInternal::Group &group)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  // int const vers=version();
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "GroupDef(Header):";
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()!=endPos) || (sz && sz < 16)) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupHeader: zone is too short\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  if (sz == 0) {
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return true;
  }

  input->seek(pos+4, librevenge::RVNG_SEEK_SET);
  int N = (int) input->readULong(2);
  f << "N=" << N << ",";
  int type = (int) input->readLong(2);
  if (type != -1)
    f << "#type=" << type << ",";
  int val = (int) input->readLong(2);
  if (val) f << "#unkn=" << val << ",";
  int fSz = (int) input->readULong(2);
  if (!fSz || N *fSz+12 != sz) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  for (int i = 0; i < 2; i++) { // always 0, 2
    val = (int) input->readLong(2);
    if (val)
      f << "f" << i << "=" << val;
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  input->seek(pos+4+12, librevenge::RVNG_SEEK_SET);
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    if (readGroupUnknown(group, fSz, i))
      continue;
    ascFile.addPos(pos);
    ascFile.addNote("GroupDef(Head-###)");
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
  }

  int const numHeaders= N==0 ? 1 : N;
  for (int i = 0; i < numHeaders; i++) {
    pos = input->tell();
    std::vector<int> res;
    /** not frequent but we can find a list of int16 as
        00320060 00480060 0048ffe9 013a0173 01ba0173 01ea02a0
        01f8ffe7 02080295 020c012c 02140218 02ae01c1
        02ca02c9-02cc02c6-02400000
        03f801e6
        8002e3ff e0010000 ee02e6ff */
    bool ok = m_document.readStructIntZone("GroupDef", false, 2, res);
    f.str("");
    f << "[GroupDef(data" << i << ")]";
    if (ok) {
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      continue;
    }
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupHeader: can not find data for %d\n", i));
    return true;
  }
  // normally, this is often followed by another list of nop/list of int zone (but not always...)
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    sz=(long) input->readULong(4);
    f.str("");
    f << "[GroupDef(dataB" << i << ")]:";
    if (!sz) {
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      continue;
    }
    if (sz>12) { // test if this is a list of int
      input->seek(pos+10, librevenge::RVNG_SEEK_SET);
      if (input->readLong(2)==2) {
        std::vector<int> res;
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        if (sz>12 && m_document.readStructIntZone("GroupDef", false, 2, res)) {
          ascFile.addPos(pos);
          ascFile.addNote(f.str().c_str());
          continue;
        }
      }
    }
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    if (i!=0) {
      f << "###";
      MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupHeader: find some data for a dataB zone\n"));
    }
    break;
  }

  return true;
}

bool ClarisWksGraph::readGroupUnknown(ClarisWksGraphInternal::Group &group, int zoneSz, int id)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  input->seek(pos+zoneSz, librevenge::RVNG_SEEK_SET);
  if (input->tell() != pos+zoneSz) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupUnknown: zone is too short\n"));
    return false;
  }
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "GroupDef(Head-";
  if (id >= 0) f << id << "):";
  else f << "_" << "):";
  if (zoneSz < 42) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupUnknown: zone is too short\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return true;
  }

  input->seek(pos, librevenge::RVNG_SEEK_SET);
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
  m_document.checkOrdering(values16, values32);

  Vec2i dim((int)values32[0],(int)values32[1]);
  if (id < 0)
    group.m_pageDimension=dim;
  if (dim[0] || dim[1]) f << "dim=" << dim << ",";
  if (values16[0]!=1 || values16[1]!=1)
    f << "pages[num]=" << values16[0] << "x" << values16[1] << ",";
  if (values32[2])
    f << "g0=" << std::hex << values32[2] << std::dec << ",";

  if (input->tell() != pos+zoneSz) {
    ascFile.addDelimiter(input->tell(), '|');
    input->seek(pos+zoneSz, librevenge::RVNG_SEEK_SET);
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}

////////////////////////////////////////////////////////////
// read the polygon vertices
////////////////////////////////////////////////////////////
bool ClarisWksGraph::readPolygonData(shared_ptr<ClarisWksGraphInternal::Zone> zone)
{
  if (!zone || zone->getType() != ClarisWksGraphInternal::Zone::T_Shape)
    return false;
  ClarisWksGraphInternal::ZoneShape *bZone =
    static_cast<ClarisWksGraphInternal::ZoneShape *>(zone.get());
  MWAWGraphicShape &shape = bZone->m_shape;
  if (shape.m_type!=MWAWGraphicShape::Polygon)
    return false;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != endPos || sz < 12) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksGraph::readPolygonData: file is too short\n"));
    return false;
  }

  input->seek(pos+4, librevenge::RVNG_SEEK_SET);
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
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksGraph::readPolygonData: find odd data size\n"));
    return false;
  }
  for (int i = 2; i < 4; i++) {
    val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";

  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  bool isSpline=false;
  std::vector<ClarisWksGraphInternal::CurvePoint> vertices;
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    f << "PolygonData-" << i << ":";
    float position[2];
    for (int j = 0; j < 2; j++)
      position[j] = float(input->readLong(4))/256.f;
    ClarisWksGraphInternal::CurvePoint point(Vec2f(position[1], position[0]));
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
    }
    else
      f << point << ",";
    if (point.m_type >= 2) isSpline=true;
    vertices.push_back(point);

    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
  }
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
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
    ClarisWksGraphInternal::CurvePoint const &pt = vertices[i];
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
// read some chart
////////////////////////////////////////////////////////////
bool ClarisWksGraph::readChartData(shared_ptr<ClarisWksGraphInternal::Zone> zone)
{
  if (!zone || zone->getSubType() != ClarisWksGraphInternal::Zone::T_Chart)
    return false;

  MWAWInputStreamPtr input = m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  if (sz==0 || !input->checkPosition(endPos)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksGraph::readChartData: unexpected size\n"));
    return false;
  }
  libmwaw::DebugFile &ascFile=m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(ChartData):";
  int N = (int) input->readLong(2);
  f << "N=" << N << ",";
  int type = (int) input->readLong(2);
  if (type != -1)
    f << "#type=" << type << ",";
  int val = (int) input->readLong(2);
  if (val) f << "#unkn=" << val << ",";
  int fSz = (int) input->readULong(2);
  int hSz = (int) input->readULong(2);
  if (hSz<0x70 || fSz<0x10 || N *fSz+hSz+12 != sz) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksGraph::readChartData: unexpected size for chart header\n"));
    return false;
  }

  if (long(input->tell()) != pos+4+hSz)
    ascFile.addDelimiter(input->tell(), '|');
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  input->seek(endPos-N*fSz, librevenge::RVNG_SEEK_SET);
  for (int i = 0; i < N; i++) {
    pos=input->tell();
    f.str("");
    f << "ChartData-" << i << ":";

    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->seek(endPos,librevenge::RVNG_SEEK_SET);
  if (version()==1)
    return true;
  std::vector<std::string> names;
  if (!m_document.readStringList("ChartData", false, names)) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::readGroupData: find unexpected second zone\n"));
    input->seek(endPos,librevenge::RVNG_SEEK_SET);
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////
// read some picture
////////////////////////////////////////////////////////////
bool ClarisWksGraph::readPictData(shared_ptr<ClarisWksGraphInternal::Zone> zone)
{
  if (!zone || (zone->getSubType() != ClarisWksGraphInternal::Zone::T_Pict &&
                zone->getSubType() != ClarisWksGraphInternal::Zone::T_Movie))
    return false;
  ClarisWksGraphInternal::ZonePict *pZone =
    static_cast<ClarisWksGraphInternal::ZonePict *>(zone.get());
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  if (!readPICT(*pZone)) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::readPictData: find a odd pict\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  pos = input->tell();
  long sz = (long) input->readULong(4);
  input->seek(pos+4+sz, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  if (long(input->tell()) != pos+4+sz) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote("###");
    MWAW_DEBUG_MSG(("ClarisWksGraph::readPictData: find a end zone for graphic\n"));
    return false;
  }
  if (sz == 0) {
    ascFile.addPos(pos);
    ascFile.addNote("Nop");
    return true;
  }

  // fixme: use readPS for a mac file and readOLE for a pc file
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  if (readPS(*pZone))
    return true;

  input->seek(pos, librevenge::RVNG_SEEK_SET);
  if (readOLE(*pZone))
    return true;

  MWAW_DEBUG_MSG(("ClarisWksGraph::readPictData: unknown data file\n"));
#ifdef DEBUG_WITH_FILES
  if (1) {
    librevenge::RVNGBinaryData file;
    input->seek(pos+4, librevenge::RVNG_SEEK_SET);
    input->readDataBlock(sz, file);
    static int volatile pictName = 0;
    libmwaw::DebugStream f;
    f << "DATA-" << ++pictName;
    libmwaw::Debug::dumpFile(file, f.str().c_str());
  }
#endif
  ascFile.addPos(pos);
  if (zone->getSubType() == ClarisWksGraphInternal::Zone::T_Movie)
    ascFile.addNote("Entries(MovieData2):#"); // find filesignature: ALMC...
  else
    ascFile.addNote("Entries(PictData2):#");
  ascFile.skipZone(pos+4, pos+4+sz-1);

  input->seek(pos+4+sz, librevenge::RVNG_SEEK_SET);
  return true;
}

bool ClarisWksGraph::readPICT(ClarisWksGraphInternal::ZonePict &zone)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  if (sz < 12) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::readPict: file is too short\n"));
    return false;
  }

  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::readPict: file is too short\n"));
    return false;
  }
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(Graphic):";

  Box2f box;
  input->seek(pos+4, librevenge::RVNG_SEEK_SET);

  MWAWPict::ReadResult res = MWAWPictData::check(input, (int)sz, box);
  if (res == MWAWPict::MWAW_R_BAD) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::readPict: can not find the picture\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    return false;
  }

  zone.m_entries[0].setBegin(pos+4);
  zone.m_entries[0].setEnd(endPos);
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  return true;
}

bool ClarisWksGraph::readPS(ClarisWksGraphInternal::ZonePict &zone)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long header = (long) input->readULong(4);
  if (header != 0x25215053L) {
    return false;
  }
  long endPos = pos+4+sz;
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != endPos) {
    return false;
  }
  zone.m_entries[1].setBegin(pos+4);
  zone.m_entries[1].setEnd(endPos);
  zone.m_entries[1].setType("PS");
#ifdef DEBUG_WITH_FILES
  if (1) {
    librevenge::RVNGBinaryData file;
    input->seek(pos+4, librevenge::RVNG_SEEK_SET);
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
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  ascFile.skipZone(pos+4, endPos-1);

  return true;
}

bool ClarisWksGraph::readOLE(ClarisWksGraphInternal::ZonePict &zone)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long val = input->readLong(4);
  if (sz <= 24 || val != 0 || input->readULong(4) != 0x1000000)
    return false;
  long endPos = pos+4+sz;
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != endPos)
    return false;
  input->seek(pos+12, librevenge::RVNG_SEEK_SET);
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
    librevenge::RVNGBinaryData file;
    input->seek(pos+28, librevenge::RVNG_SEEK_SET);
    input->readDataBlock(sz-24, file);
    static int volatile pictName = 0;
    libmwaw::DebugStream f2;
    f2 << "OLE-" << ++pictName;
    libmwaw::Debug::dumpFile(file, f2.str().c_str());
  }
#endif

  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  ascFile.skipZone(pos+28, endPos-1);

  return true;
}

////////////////////////////////////////////////////////////
// read Qtime picture
////////////////////////////////////////////////////////////
bool ClarisWksGraph::readQTimeData(shared_ptr<ClarisWksGraphInternal::Zone> zone)
{
  if (!zone || zone->getSubType() != ClarisWksGraphInternal::Zone::T_QTim)
    return false;
  ClarisWksGraphInternal::ZonePict *pZone =
    static_cast<ClarisWksGraphInternal::ZonePict *>(zone.get());
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
    MWAW_DEBUG_MSG(("ClarisWksGraph::readQTimeData: find a odd qtim zone\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
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
    MWAW_DEBUG_MSG(("ClarisWksGraph::readQTimeData: find a odd named pict\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  return true;
}

bool ClarisWksGraph::readNamedPict(ClarisWksGraphInternal::ZonePict &zone)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  std::string name("");
  for (int i = 0; i < 4; i++) {
    char c = (char) input->readULong(1);
    if (c < ' ' || c > 'z') {
      MWAW_DEBUG_MSG(("ClarisWksGraph::readNamedPict: can not find the name\n"));
      return false;
    }
    name+=c;
  }
  long sz = (long) input->readULong(4);
  long endPos = pos+8+sz;
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != endPos || !sz) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::readNamedPict: file is too short\n"));
    return false;
  }

  zone.m_entries[0].setBegin(pos+8);
  zone.m_entries[0].setEnd(endPos);

#ifdef DEBUG_WITH_FILES
  if (1) {
    librevenge::RVNGBinaryData file;
    input->seek(pos+8, librevenge::RVNG_SEEK_SET);
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
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  ascFile.skipZone(pos+8, endPos-1);

  return true;
}

////////////////////////////////////////////////////////////
// read bitmap picture
////////////////////////////////////////////////////////////
bool ClarisWksGraph::readBitmapColorMap(std::vector<MWAWColor> &cMap)
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
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != endPos) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::readBitmapColorMap: file is too short\n"));
    return false;
  }

  input->seek(pos+4, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(BitmapColor):";
  f << "unkn=" << input->readLong(4) << ",";
  int maxColor = (int) input->readLong(4);
  if (sz != 8+8*(maxColor+1)) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::readBitmapColorMap: sz is odd\n"));
    return false;
  }
  cMap.resize(size_t(maxColor+1));
  for (int i = 0; i <= maxColor; i++) {
    int id = (int) input->readULong(2);
    if (id != i) {
      MWAW_DEBUG_MSG(("ClarisWksGraph::readBitmapColorMap: find odd index : %d\n", i));
      return false;
    }
    unsigned char col[3];
    for (int c = 0; c < 3; c++) col[c] = (unsigned char)(input->readULong(2)>>8);
    cMap[(size_t)i] = MWAWColor(col[0], col[1], col[2]);
  }

  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  return true;
}

bool ClarisWksGraph::readBitmapData(ClarisWksGraphInternal::Bitmap &zone)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != endPos || !sz) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::readBitmapData: file is too short\n"));
    return false;
  }

  long numPixels = zone.m_bitmapSize[0]*zone.m_bitmapSize[1];
  if (numPixels<=0) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::readBitmapData: unexpected empty size\n"));
    return false;
  }

  int numBytesPerPixel = int(sz/numPixels);
  int bitmapRowSize=zone.m_bitmapSize[0]*numBytesPerPixel;
  if (sz < numPixels) {
    int nHalfPixel=(zone.m_bitmapSize[0]+1)/2;
    for (int align=1; align <= 4; align*=2) {
      int diffToAlign=align==1 ? 0 : align-(nHalfPixel%align);
      if (diffToAlign==align) continue;
      if (sz == (nHalfPixel+diffToAlign)*zone.m_bitmapSize[1]) {
        bitmapRowSize=(nHalfPixel+diffToAlign);
        numBytesPerPixel=-2;
        break;
      }
    }
  }
  else if (sz > numBytesPerPixel*numPixels) {
    // check for different row alignment: 2 and 4
    for (int align=2; align <= 4; align*=2) {
      int diffToAlign=align-(zone.m_bitmapSize[0]%align);
      if (diffToAlign==align) continue;
      numPixels = (zone.m_bitmapSize[0]+diffToAlign)*zone.m_bitmapSize[1];
      numBytesPerPixel = int(sz/numPixels);
      if (sz == numBytesPerPixel*numPixels) {
        bitmapRowSize=(zone.m_bitmapSize[0]+diffToAlign)*numBytesPerPixel;
        break;
      }
    }
  }

  if (sz != bitmapRowSize*zone.m_bitmapSize[1]) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::readBitmapData: unexpected size\n"));
    return false;
  }
  zone.m_numBytesPerPixel = numBytesPerPixel;
  zone.m_bitmapRowSize = bitmapRowSize;
  zone.m_entry.setBegin(pos+4);
  zone.m_entry.setEnd(endPos);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(BitmapData):[" << numBytesPerPixel << "]";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  ascFile.skipZone(pos+4, endPos-1);

  return true;
}

////////////////////////////////////////////////////////////
// update the group information
////////////////////////////////////////////////////////////
void ClarisWksGraph::updateGroup(ClarisWksGraphInternal::Group &group) const
{
  if (!group.m_zonesToSend.empty())
    return;

  /* update the list of zone to be send (ie. remove remaining header/footer zone in not footer zone )
     + create a map to find linked zone: list of zone sharing the same text zone
   */
  bool isHeaderFooterBlock=group.isHeaderFooter();
  std::map<int, std::map<int, size_t> > idSIdCIdMap;
  for (size_t g = 0; g < group.m_zones.size(); g++) {
    shared_ptr<ClarisWksGraphInternal::Zone> child = group.m_zones[g];
    if (!child) continue;
    if (child->getType() == ClarisWksGraphInternal::Zone::T_Zone) {
      ClarisWksGraphInternal::ZoneZone const &childZone =
        static_cast<ClarisWksGraphInternal::ZoneZone &>(*child);
      int zId = childZone.m_id;
      shared_ptr<ClarisWksStruct::DSET> zone=m_document.getZone(zId);
      if (!zone)
        continue;
      if (!isHeaderFooterBlock && zone->isHeaderFooter())
        continue;
      if (zId==1) {
        group.m_hasMainZone = true;
        continue;
      }
      if (idSIdCIdMap.find(zId)==idSIdCIdMap.end())
        idSIdCIdMap.insert(std::map<int, std::map<int, size_t> >::value_type(zId, std::map<int, size_t>()));
      std::map<int, size_t> &sIdCIdMap=idSIdCIdMap.find(zId)->second;
      if (sIdCIdMap.find(childZone.m_subId)!=sIdCIdMap.end()) {
        MWAW_DEBUG_MSG(("ClarisWksGraph::updateGroup: zone %d already find with subId %d\n",
                        zId, childZone.m_subId));
      }
      else
        sIdCIdMap[childZone.m_subId]=g;
    }
    group.m_zonesToSend.push_back(child);
    if (g>=group.m_childs.size()) {
      MWAW_DEBUG_MSG(("ClarisWksGraph::updateGroup: the zones list is not in correspondance with the childs list\n"));
      continue;
    }
    child->m_box = group.m_childs[g].m_box;
    child->m_page = group.m_childs[g].m_page;
  }
  // update the linked frame data, ie. zone->m_frame*
  std::map<int, std::map<int, size_t> >::const_iterator it;
  for (it=idSIdCIdMap.begin(); it!=idSIdCIdMap.end(); ++it) {
    std::map<int, size_t> const &sIdCIdMap=it->second;
    if (sIdCIdMap.size()<=1) continue;
    int frameId=m_state->m_frameId++;
    int frameSubId=0;
    for (std::map<int, size_t>::const_iterator cIt=sIdCIdMap.begin(); cIt!=sIdCIdMap.end();) {
      ClarisWksGraphInternal::ZoneZone *child =
        static_cast<ClarisWksGraphInternal::ZoneZone *>(group.m_zones[cIt++->second].get());
      child->m_frameId=frameId;
      child->m_frameSubId=frameSubId++;
      child->m_frameLast=cIt==sIdCIdMap.end();
    }
  }
}

////////////////////////////////////////////////////////////
// send data to the listener
////////////////////////////////////////////////////////////
bool ClarisWksGraph::sendGroup(std::vector<shared_ptr<ClarisWksGraphInternal::Zone> > const &lChild, MWAWGraphicListenerPtr listener)
{
  if (!listener) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::sendGroup: can not find the listener!!!\n"));
    return false;
  }
  size_t numZones = lChild.size();
  for (size_t g = 0; g < numZones; g++) {
    shared_ptr<ClarisWksGraphInternal::Zone> child = lChild[g];
    if (!child) continue;
    Box2f box=child->getBdBox();
    ClarisWksGraphInternal::Zone::Type type=child->getType();
    MWAWPosition pos(box[0], box.size(), librevenge::RVNG_POINT);
    pos.m_anchorTo=MWAWPosition::Page;
    if (type==ClarisWksGraphInternal::Zone::T_Zone) {
      int zId=child->getZoneId();
      shared_ptr<ClarisWksStruct::DSET> dset=m_document.getZone(zId);
      if (dset && dset->m_fileType==4) {
        sendBitmap(zId, listener, pos);
        continue;
      }
      shared_ptr<MWAWSubDocument> doc(new ClarisWksGraphInternal::SubDocument(*this, m_parserState->m_input, zId));
      if (dset && dset->m_fileType==1)
        listener->insertTextBox(pos, doc, child->m_style);
      else
        listener->insertGroup(box, doc);
    }
    else if (type==ClarisWksGraphInternal::Zone::T_Shape) {
      ClarisWksGraphInternal::ZoneShape const &shape=
        static_cast<ClarisWksGraphInternal::ZoneShape const &>(*child);
      MWAWGraphicStyle style(shape.m_style);
      if (shape.m_shape.m_type!=MWAWGraphicShape::Line)
        style.m_arrows[0]=style.m_arrows[1]=false;
      listener->insertPicture(pos, shape.m_shape, style);
    }
    else if (type!=ClarisWksGraphInternal::Zone::T_DataBox) {
      MWAW_DEBUG_MSG(("ClarisWksGraph::sendGroup: find unexpected type!!!\n"));
    }
  }
  return true;
}

bool ClarisWksGraph::sendPageChild(ClarisWksGraphInternal::Group &group)
{
  group.m_parsed=true;
  MWAWListenerPtr listener=m_parserState->getMainListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::sendPageChild: can not find the listener\n"));
    return false;
  }
  updateGroup(group);
  Vec2f leftTop=72.0f*m_document.getPageLeftTop();

  for (size_t g = 0; g < group.m_zonesToSend.size(); g++) {
    shared_ptr<ClarisWksGraphInternal::Zone> child = group.m_zonesToSend[g];
    if (!child || child->m_page<=0) continue;
    if (child->getType() == ClarisWksGraphInternal::Zone::T_Zone) {
      shared_ptr<ClarisWksStruct::DSET> dset=m_document.getZone(child->getZoneId());
      if (dset && dset->m_position==ClarisWksStruct::DSET::P_Main)
        continue;
    }
    Box2f const &box=child->m_box;
    MWAWPosition pos(box[0]+leftTop, box.size(), librevenge::RVNG_POINT);
    pos.setRelativePosition(MWAWPosition::Page);
    pos.setPage(child->m_page);
    pos.m_wrapping = child->m_style.getWrapping();
    pos.setOrder(m_state->getOrdering());
    sendGroupChild(child, pos);
  }
  return true;
}

bool ClarisWksGraph::sendGroup(ClarisWksGraphInternal::Group &group, MWAWPosition const &position)
{
  group.m_parsed=true;
  MWAWListenerPtr listener=m_parserState->getMainListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::sendGroup: can not find the listener\n"));
    return false;
  }
  updateGroup(group);
  bool mainGroup = group.m_position == ClarisWksStruct::DSET::P_Main;
  bool isSlide = group.m_position == ClarisWksStruct::DSET::P_Slide;
  Vec2f leftTop(0,0);
  float textHeight = 0.0;
  if (mainGroup)
    leftTop = 72.0f*m_document.getPageLeftTop();
  textHeight = 72.0f*float(m_mainParser->getFormLength());

  libmwaw::SubDocumentType inDocType;
  if (!listener->isSubDocumentOpened(inDocType))
    inDocType = libmwaw::DOC_NONE;
  MWAWPosition::AnchorTo suggestedAnchor = MWAWPosition::Char;
  switch (inDocType) {
  case libmwaw::DOC_TEXT_BOX:
    suggestedAnchor=MWAWPosition::Char;
    break;
  case libmwaw::DOC_CHART:
  case libmwaw::DOC_CHART_ZONE:
  case libmwaw::DOC_HEADER_FOOTER:
  case libmwaw::DOC_NOTE:
    suggestedAnchor=MWAWPosition::Frame;
    break;
  case libmwaw::DOC_SHEET:
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
  // CHECKME
  if (0 && position.m_anchorTo==MWAWPosition::Unknown) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::sendGroup: position is not set\n"));
  }
  if (!mainGroup && canSendAsGraphic(group)) {
    Box2f box=group.m_box;
    MWAWGraphicEncoder graphicEncoder;
    MWAWGraphicListenerPtr graphicListener(new MWAWGraphicListener(*m_parserState, box, &graphicEncoder));
    graphicListener->startDocument();
    sendGroup(group.m_zonesToSend, graphicListener);
    graphicListener->endDocument();
    librevenge::RVNGBinaryData data;
    std::string type;
    if (graphicEncoder.getBinaryResult(data,type)) {
      MWAWPosition pos(position);
      //pos.setOrigin(box[0]);
      if (pos.size()[0]<=0 || pos.size()[1]<=0)
        pos.setSize(box.size());
      pos.m_wrapping =  MWAWPosition::WForeground;
      if (pos.m_anchorTo==MWAWPosition::Unknown) {
        pos=MWAWPosition(box[0], box.size(), librevenge::RVNG_POINT);
        pos.setRelativePosition(suggestedAnchor);
        if (suggestedAnchor == MWAWPosition::Page) {
          int pg = isSlide ? 0 : group.m_page > 0 ? group.m_page : 1;
          Vec2f orig = pos.origin()+leftTop;
          pos.setPagePos(pg, orig);
          pos.m_wrapping = MWAWPosition::WForeground;
        }
        else if (suggestedAnchor == MWAWPosition::Char)
          pos.setOrigin(Vec2f(0,0));
      }
      listener->insertPicture(pos, data, type);
    }
    return true;
  }
  if (group.m_zonesToSend.size() > 1 &&
      (position.m_anchorTo==MWAWPosition::Char ||
       position.m_anchorTo==MWAWPosition::CharBaseLine)) {
    // we need to a frame, ...
    MWAWPosition lPos;
    lPos.m_anchorTo=MWAWPosition::Paragraph;
    MWAWSubDocumentPtr doc(new ClarisWksGraphInternal::SubDocument(*this, m_parserState->m_input, group.m_id, lPos));
    MWAWGraphicStyle style(MWAWGraphicStyle::emptyStyle());
    style.m_backgroundOpacity=0;
    MWAWPosition pos(position);
    pos.m_wrapping=MWAWPosition::WForeground;
    listener->insertTextBox(pos, doc, style);
    return true;
  }

  /* first sort the different zone: ie. we must first send the page block, then the main zone */
  std::vector<shared_ptr<ClarisWksGraphInternal::Zone> > listJobs[2];
  for (size_t g = 0; g < group.m_zonesToSend.size(); g++) {
    shared_ptr<ClarisWksGraphInternal::Zone> child = group.m_zonesToSend[g];
    if (!child) continue;
    if (position.m_anchorTo==MWAWPosition::Unknown && suggestedAnchor == MWAWPosition::Page) {
      Vec2f RB = Vec2f(child->m_box[1])+leftTop;
      if (RB[1] >= textHeight || listener->isSectionOpened()) {
        listJobs[1].push_back(child);
        continue;
      }
    }
    if (child->getType() == ClarisWksGraphInternal::Zone::T_Zone) {
      shared_ptr<ClarisWksStruct::DSET> dset=m_document.getZone(child->getZoneId());
      if (dset && dset->m_position==ClarisWksStruct::DSET::P_Main) {
        listJobs[1].push_back(child);
        continue;
      }
    }
    listJobs[0].push_back(child);
  }
  for (int st = 0; st < 2; st++) {
    if (st == 1) {
      suggestedAnchor = MWAWPosition::Char;
      if (group.m_hasMainZone)
        m_document.sendZone(1);
    }
    size_t numJobs=listJobs[st].size();
    for (size_t g = 0; g < numJobs; g++) {
      Box2f box;
      std::vector<shared_ptr<ClarisWksGraphInternal::Zone> > groupList;
      int page = 0;
      size_t lastOk=g;

      if (st==0 && !mainGroup) {
        for (size_t h = g; h < numJobs; ++h) {
          shared_ptr<ClarisWksGraphInternal::Zone> child = listJobs[st][h];
          if (!child) continue;
          if (groupList.empty()) page=child->m_page;
          else if (page != child->m_page) break;
          if (!child->canBeSendAsGraphic()) break;
          if (child->getType()==ClarisWksGraphInternal::Zone::T_Zone &&
              !m_document.canSendZoneAsGraphic(child->getZoneId()))
            break;
          if (groupList.empty())
            box=child->m_box;
          else
            box=box.getUnion(child->m_box);
          groupList.push_back(child);
          lastOk=h;
        }
      }
      if (groupList.empty()) {
        shared_ptr<ClarisWksGraphInternal::Zone> child = listJobs[st][g];
        if (!child) continue;
        groupList.push_back(child);
      }
      if (groupList.size()<= 1) {
        if (!groupList[0]) continue;
        box = groupList[0]->m_box;
        page = groupList[0]->m_page;
      }
      MWAWPosition pos(position);
      pos.setOrder(m_state->getOrdering());
      pos.setOrigin(box[0]);
      pos.setSize(box.size());
      pos.setUnit(librevenge::RVNG_POINT);
      if (pos.m_anchorTo==MWAWPosition::Unknown) {
        pos=MWAWPosition(box[0], box.size(), librevenge::RVNG_POINT);
        pos.setRelativePosition(suggestedAnchor);
        if (suggestedAnchor == MWAWPosition::Page) {
          int pg = page > 0 ? page : 1;
          Vec2f orig = pos.origin()+leftTop;
          pos.setPagePos(pg, orig);
          pos.m_wrapping = MWAWPosition::WForeground;
        }
        else if (st==1 || suggestedAnchor == MWAWPosition::Char)
          pos.setOrigin(Vec2f(0,0));
      }
      // groupList can not be empty
      if (groupList.size() <= 1) {
        sendGroupChild(groupList[0], pos);
        continue;
      }
      MWAWGraphicEncoder graphicEncoder;
      MWAWGraphicListenerPtr graphicListener(new MWAWGraphicListener(*m_parserState, box, &graphicEncoder));
      graphicListener->startDocument();
      sendGroup(groupList, graphicListener);
      graphicListener->endDocument();
      librevenge::RVNGBinaryData data;
      std::string type;
      if (graphicEncoder.getBinaryResult(data,type)) {
        MWAWGraphicStyle style(MWAWGraphicStyle::emptyStyle());
        style.m_backgroundOpacity=0;
        pos.m_wrapping =  MWAWPosition::WForeground;
        listener->insertPicture(pos, data, type, style);
      }
      g=lastOk;
    }
  }
  return true;
}

bool ClarisWksGraph::sendGroupChild(shared_ptr<ClarisWksGraphInternal::Zone> child, MWAWPosition pos)
{
  if (!child) return false;
  MWAWListenerPtr listener=m_parserState->getMainListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::sendGroupChild: can not find the listener\n"));
    return false;
  }
  ClarisWksGraphInternal::Style const &cStyle = child->m_style;
  pos.m_wrapping=cStyle.getWrapping();
  ClarisWksGraphInternal::Zone::Type type = child->getType();
  if (type==ClarisWksGraphInternal::Zone::T_Picture)
    return sendPicture(static_cast<ClarisWksGraphInternal::ZonePict &>(*child), pos);
  if (type==ClarisWksGraphInternal::Zone::T_Shape)
    return sendShape(static_cast<ClarisWksGraphInternal::ZoneShape &>(*child), pos);
  if (type==ClarisWksGraphInternal::Zone::T_DataBox || type==ClarisWksGraphInternal::Zone::T_Chart ||
      type==ClarisWksGraphInternal::Zone::T_Unknown)
    return true;
  if (type!=ClarisWksGraphInternal::Zone::T_Zone) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::sendGroupChild: find unknown zone\n"));
    return false;
  }

  ClarisWksGraphInternal::ZoneZone const &childZone = static_cast<ClarisWksGraphInternal::ZoneZone &>(*child);
  int zId = childZone.m_id;
  shared_ptr<ClarisWksStruct::DSET> dset=m_document.getZone(zId);
  // if this is a group, try to send it as a picture
  bool isLinked=childZone.isLinked();
  bool isGroup = dset && dset->m_fileType==0;
  if (!isLinked && isGroup && canSendGroupAsGraphic(zId))
    return sendGroup(zId, MWAWListenerPtr(), pos);
  if (!isLinked && dset && dset->m_fileType==4)
    return sendBitmap(zId, MWAWListenerPtr(), pos);
  if (!isLinked && (cStyle.hasPattern() || cStyle.hasGradient()) &&
      (dset && dset->m_fileType==1) && m_document.canSendZoneAsGraphic(zId)) {
    Box2f box=Box2f(Vec2f(0,0), childZone.m_box.size());
    MWAWGraphicEncoder graphicEncoder;
    MWAWGraphicListener graphicListener(*m_parserState, box, &graphicEncoder);
    graphicListener.startDocument();
    shared_ptr<MWAWSubDocument> doc(new ClarisWksGraphInternal::SubDocument(*this, m_parserState->m_input, zId));
    MWAWPosition textPos(box[0], box.size(), librevenge::RVNG_POINT);
    textPos.m_anchorTo=MWAWPosition::Page;
    textPos.m_wrapping=pos.m_wrapping;
    graphicListener.insertTextBox(textPos, doc, cStyle);
    graphicListener.endDocument();
    librevenge::RVNGBinaryData data;
    std::string mime;
    if (graphicEncoder.getBinaryResult(data,mime))
      listener->insertPicture(pos, data, mime);
    return true;
  }
  // now check if we need to create a frame
  ClarisWksStruct::DSET::Position cPos=dset ? dset->m_position : ClarisWksStruct::DSET::P_Unknown;
  bool createFrame=
    cPos == ClarisWksStruct::DSET::P_Frame || cPos == ClarisWksStruct::DSET::P_Table ||
    (cPos == ClarisWksStruct::DSET::P_Unknown && pos.m_anchorTo == MWAWPosition::Page &&
     (!dset || dset->m_fileType!=2));
  if (!isLinked && childZone.m_subId) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::sendGroup: find odd subs zone\n"));
    return false;
  }
  MWAWGraphicStyle style=MWAWGraphicStyle::emptyStyle();
  if (dset && dset->m_fileType==1) { // checkme: use style for textbox
    MWAWColor color;
    if (cStyle.hasSurfaceColor() && getSurfaceColor(cStyle, color))
      style.setBackgroundColor(color);
    else
      style.m_backgroundOpacity=0;
    if (cStyle.hasLine()) {
      MWAWBorder border;
      border.m_color=cStyle.m_lineColor;
      style.setBorders(15, border);
      // extend the frame to add border
      float extend = float(cStyle.m_lineWidth*0.85);
      pos.setOrigin(pos.origin()-Vec2f(extend,extend));
      pos.setSize(pos.size()+2.0*Vec2f(extend,extend));
    }
  }
  else
    style.m_backgroundOpacity=0;
  if (createFrame) {
    childZone.addFrameName(style);
    shared_ptr<MWAWSubDocument> doc;
    if (!isLinked || childZone.m_subId==0) {
      MWAWPosition childPos;
      childPos.setUnit(librevenge::RVNG_POINT);
      if (dset && dset->m_fileType==0) {
        childPos.setRelativePosition(MWAWPosition::Paragraph);
        pos.m_wrapping=MWAWPosition::WForeground;
        style.m_backgroundOpacity=0;
      }
      doc.reset(new ClarisWksGraphInternal::SubDocument(*this, m_parserState->m_input, zId, childPos));
    }
    if (!isLinked && dset && dset->m_fileType==1 && pos.size()[1]>0) // use min-height for text
      pos.setSize(Vec2f(pos.size()[0],-pos.size()[1]));
    listener->insertTextBox(pos, doc, style);
    return true;
  }
  if (dset && dset->m_fileType==2) { // spreadsheet
    Box2f box=Box2f(Vec2f(0,0), childZone.m_box.size());
    MWAWSpreadsheetEncoder spreadsheetEncoder;
    MWAWSpreadsheetListener *spreadsheetListener=new MWAWSpreadsheetListener(*m_parserState, box, &spreadsheetEncoder);
    MWAWListenerPtr sheetListener(spreadsheetListener);
    spreadsheetListener->startDocument();
    m_document.sendZone(zId, sheetListener);
    spreadsheetListener->endDocument();
    librevenge::RVNGBinaryData data;
    std::string mime;
    if (spreadsheetEncoder.getBinaryResult(data,mime))
      listener->insertPicture(pos, data, mime);
    return true;
  }
  return m_document.sendZone(zId, listener, pos);
}

////////////////////////////////////////////////////////////
// basic shape, bitmap, pict
////////////////////////////////////////////////////////////
bool ClarisWksGraph::sendShape(ClarisWksGraphInternal::ZoneShape &pict, MWAWPosition pos)
{
  MWAWListenerPtr listener=m_parserState->getMainListener();
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

bool ClarisWksGraph::canSendBitmapAsGraphic(int number) const
{
  std::map<int, shared_ptr<ClarisWksGraphInternal::Bitmap> >::iterator iter
    = m_state->m_bitmapMap.find(number);
  if (iter == m_state->m_bitmapMap.end() || !iter->second) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::canSendBitmapAsGraphic: can not find bitmap %d\n", number));
    return false;
  }
  return true;
}

bool ClarisWksGraph::sendBitmap(int number, MWAWListenerPtr listener, MWAWPosition const &pos)
{
  std::map<int, shared_ptr<ClarisWksGraphInternal::Bitmap> >::iterator iter
    = m_state->m_bitmapMap.find(number);
  if (iter == m_state->m_bitmapMap.end() || !iter->second) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::sendBitmap: can not find bitmap %d\n", number));
    return false;
  }
  if (!listener)
    listener=m_parserState->getMainListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::sendBitmap: can not find a listener\n"));
    return false;
  }
  return sendBitmap(*iter->second, *listener, pos);
}

bool ClarisWksGraph::sendBitmap(ClarisWksGraphInternal::Bitmap &bitmap, MWAWListener &listener, MWAWPosition pos)
{
  bitmap.m_parsed=true;
  if (!bitmap.m_entry.valid() || !bitmap.m_numBytesPerPixel)
    return false;
  int bytesPerPixel = bitmap.m_numBytesPerPixel;
  if (bytesPerPixel<0 && (bytesPerPixel!=-2 && bytesPerPixel!=-4)) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::sendBitmap: unknown group of color\n"));
    return false;
  }
  int numColors = int(bitmap.m_colorMap.size());
  shared_ptr<MWAWPictBitmap> bmap;

  MWAWPictBitmapIndexed *bmapIndexed = 0;
  MWAWPictBitmapColor *bmapColor = 0;
  bool indexed = false;
  if (numColors > 2) {
    bmapIndexed =  new MWAWPictBitmapIndexed(bitmap.m_bitmapSize);
    bmapIndexed->setColors(bitmap.m_colorMap);
    bmap.reset(bmapIndexed);
    indexed = true;
  }
  else {
    if (bytesPerPixel<0) {
      MWAW_DEBUG_MSG(("ClarisWksGraph::sendBitmap: unexpected mode for compressed bitmap. Bitmap ignored.\n"));
      return false;
    }
    bmap.reset((bmapColor=new MWAWPictBitmapColor(bitmap.m_bitmapSize)));
  }

  bool const isCompressed =  bytesPerPixel<0;
  int const numColorByData= isCompressed ? -bytesPerPixel : 1;
  long const colorMask= !isCompressed ? 0 : numColorByData==2 ? 0xF : 0x3;
  int const numColorBytes = isCompressed ? 8/numColorByData : 8*bytesPerPixel;
  //! let go
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(bitmap.m_entry.begin(), librevenge::RVNG_SEEK_SET);
  for (int r = 0; r < bitmap.m_bitmapSize[1]; r++) {
    long rPos=input->tell();
    int numRead=0;
    long read=0;
    for (int c = 0; c < bitmap.m_bitmapSize[0]; c++) {
      if (numRead==0) {
        read=(long) input->readULong(isCompressed ? 1 : bytesPerPixel);
        numRead=numColorByData;
      }
      --numRead;
      long val=!isCompressed ? read : (read>>(numColorBytes*numRead))&colorMask;
      if (indexed) {
        bmapIndexed->set(c,r,(int)val);
        continue;
      }
      switch (bytesPerPixel) {
      case 1:
        bmapColor->set(c,r, MWAWColor((unsigned char)val,(unsigned char)val,(unsigned char)val));
        break;
      case 2: // rgb compressed ?
        bmapColor->set(c,r, MWAWColor((unsigned char)(((val>>10)&0x1F) << 3),(unsigned char)(((val>>5)&0x1F) << 3),(unsigned char)(((val>>0)&0x1F) << 3)));
        break;
      case 4:
        bmapColor->set(c,r, MWAWColor(uint32_t(val)|0xFF000000)); // checkme
        break;
      default: {
        static bool first = true;
        if (first) {
          MWAW_DEBUG_MSG(("ClarisWksGraph::sendBitmap: unknown data size\n"));
          first = false;
        }
        break;
      }
      }
    }
    input->seek(rPos+bitmap.m_bitmapRowSize, librevenge::RVNG_SEEK_SET);
  }

  librevenge::RVNGBinaryData data;
  std::string type;
  if (!bmap->getBinary(data,type)) return false;
  if (pos.size()[0] <= 0 || pos.size()[1] <= 0) {
    pos.m_anchorTo=MWAWPosition::Char;
    if (m_parserState->m_kind==MWAWDocument::MWAW_K_PAINT)// fixme
      pos.setSize(Vec2f(0.9f*float(m_mainParser->getPageWidth()),
                        0.9f*float(m_mainParser->getPageLength())));
    else {
      MWAW_DEBUG_MSG(("ClarisWksGraph::sendBitmap: can not find bitmap size\n"));
      pos.setSize(Vec2f(1,1));
    }
  }
  if (pos.m_anchorTo==MWAWPosition::Unknown) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::sendBitmap: anchor is not set, revert to char\n"));
    pos.m_anchorTo=MWAWPosition::Char;
  }
  listener.insertPicture(pos, data, "image/pict");
  return true;
}

bool ClarisWksGraph::sendPicture(ClarisWksGraphInternal::ZonePict &pict, MWAWPosition pos)
{
  bool send = false;
  bool posOk = pos.size()[0] > 0 && pos.size()[1] > 0;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  MWAWListenerPtr listener=m_parserState->getMainListener();
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
    input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);

    switch (pict.getSubType()) {
    case ClarisWksGraphInternal::Zone::T_Movie:
    case ClarisWksGraphInternal::Zone::T_Pict: {
      shared_ptr<MWAWPict> thePict(MWAWPictData::get(input, (int)entry.length()));
      if (thePict) {
        if (!send && listener) {
          librevenge::RVNGBinaryData data;
          std::string type;
          if (thePict->getBinary(data,type))
            listener->insertPicture(pos, data, type);
        }
        send = true;
      }
      break;
    }
    case ClarisWksGraphInternal::Zone::T_Line:
    case ClarisWksGraphInternal::Zone::T_Rect:
    case ClarisWksGraphInternal::Zone::T_RectOval:
    case ClarisWksGraphInternal::Zone::T_Oval:
    case ClarisWksGraphInternal::Zone::T_Arc:
    case ClarisWksGraphInternal::Zone::T_Poly:
    case ClarisWksGraphInternal::Zone::T_Zone:
    case ClarisWksGraphInternal::Zone::T_Zone2:
    case ClarisWksGraphInternal::Zone::T_Shape:
    case ClarisWksGraphInternal::Zone::T_Picture:
    case ClarisWksGraphInternal::Zone::T_Chart:
    case ClarisWksGraphInternal::Zone::T_DataBox:
    case ClarisWksGraphInternal::Zone::T_Unknown:
    case ClarisWksGraphInternal::Zone::T_QTim:
    default:
      if (!send && listener) {
        librevenge::RVNGBinaryData data;
        input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
        input->readDataBlock(entry.length(), data);
        listener->insertPicture(pos, data, "image/pict");
      }
      send = true;
      break;
    }

#ifdef DEBUG_WITH_FILES
    m_parserState->m_asciiFile.skipZone(entry.begin(), entry.end()-1);
    librevenge::RVNGBinaryData file;
    input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
    input->readDataBlock(entry.length(), file);
    static int volatile pictName = 0;
    libmwaw::DebugStream f;
    f << "PICT-" << ++pictName;
    libmwaw::Debug::dumpFile(file, f.str().c_str());
#endif
  }
  return send;
}

////////////////////////////////////////////////////////////
// interface send function
////////////////////////////////////////////////////////////
bool ClarisWksGraph::canSendGroupAsGraphic(int number) const
{
  std::map<int, shared_ptr<ClarisWksGraphInternal::Group> >::iterator iter
    = m_state->m_groupMap.find(number);
  if (iter == m_state->m_groupMap.end() || !iter->second)
    return false;
  return canSendAsGraphic(*iter->second);
}

bool ClarisWksGraph::canSendAsGraphic(ClarisWksGraphInternal::Group &group) const
{
  updateGroup(group);
  if ((group.m_position != ClarisWksStruct::DSET::P_Frame && group.m_position != ClarisWksStruct::DSET::P_Unknown)
      || group.m_page <= 0)
    return false;
  size_t numZones = group.m_zonesToSend.size();
  for (size_t g = 0; g < numZones; g++) {
    shared_ptr<ClarisWksGraphInternal::Zone> child = group.m_zonesToSend[g];
    if (!child) continue;
    if (!child->canBeSendAsGraphic()) return false;
    if (child->getType()==ClarisWksGraphInternal::Zone::T_Zone &&
        !m_document.canSendZoneAsGraphic(child->getZoneId()))
      return false;
  }
  return true;
}

bool ClarisWksGraph::sendPageGraphics(int groupId)
{
  std::map<int, shared_ptr<ClarisWksGraphInternal::Group> >::iterator iter
    = m_state->m_groupMap.find(groupId);
  if (iter == m_state->m_groupMap.end() || !iter->second) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::sendPageGraphics: can not find the group %d\n", groupId));
    return false;
  }
  shared_ptr<ClarisWksGraphInternal::Group> group = iter->second;
  group->m_parsed=true;
  return sendPageChild(*group);
}

bool ClarisWksGraph::sendGroup(int number, MWAWListenerPtr listener, MWAWPosition const &position)
{
  std::map<int, shared_ptr<ClarisWksGraphInternal::Group> >::iterator iter
    = m_state->m_groupMap.find(number);
  if (iter == m_state->m_groupMap.end() || !iter->second)
    return false;
  shared_ptr<ClarisWksGraphInternal::Group> group = iter->second;
  group->m_parsed=true;
  MWAWGraphicListener *graphicListener=dynamic_cast<MWAWGraphicListener *>(listener.get());
  if (graphicListener) {
    shared_ptr<MWAWGraphicListener> ptr(graphicListener, MWAW_shared_ptr_noop_deleter<MWAWGraphicListener>());
    return sendGroup(group->m_zonesToSend, ptr);
  }
  if (!m_parserState->getMainListener()) {
    MWAW_DEBUG_MSG(("ClarisWksGraph::sendGroup: can not find the listener\n"));
    return false;
  }
  return sendGroup(*group, position);
}

void ClarisWksGraph::flushExtra()
{
  shared_ptr<MWAWListener> listener=m_parserState->getMainListener();
  if (!listener) return;
  std::map<int, shared_ptr<ClarisWksGraphInternal::Group> >::iterator iter
    = m_state->m_groupMap.begin();
  for (; iter !=  m_state->m_groupMap.end(); ++iter) {
    shared_ptr<ClarisWksGraphInternal::Group> zone = iter->second;
    // can be some footer print on the last page or an unused header/footer
    if (!zone || zone->m_parsed || zone->isHeaderFooter())
      continue;
    static bool first=true;
    if (first) {
      MWAW_DEBUG_MSG(("ClarisWksGraph::flushExtra: find some extra graph %d\n", zone->m_id));
      first=false;
    }
    listener->insertEOL();
    MWAWPosition pos(Vec2f(0,0),Vec2f(0,0),librevenge::RVNG_POINT);
    pos.setRelativePosition(MWAWPosition::Char);
    sendGroup(iter->first, MWAWListenerPtr(), pos);
  }
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
