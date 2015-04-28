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
#include <stack>

#include <librevenge/librevenge.h>

#include "MWAWFont.hxx"
#include "MWAWGraphicListener.hxx"
#include "MWAWGraphicShape.hxx"
#include "MWAWGraphicStyle.hxx"
#include "MWAWListener.hxx"
#include "MWAWParagraph.hxx"
#include "MWAWParser.hxx"
#include "MWAWPictBitmap.hxx"
#include "MWAWPictMac.hxx"
#include "MWAWPosition.hxx"
#include "MWAWSubDocument.hxx"

#include "ClarisWksStruct.hxx"

#include "ClarisDrawParser.hxx"
#include "ClarisDrawStyleManager.hxx"

#include "ClarisDrawGraph.hxx"

#include "libmwaw_internal.hxx"

/** Internal: the structures of a ClarisDrawGraph */
namespace ClarisDrawGraphInternal
{
//! Internal: the structure used to a point of a ClarisDrawGraph
struct CurvePoint {
  CurvePoint(MWAWVec2f point=MWAWVec2f()) : m_pos(point), m_type(1)
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
  MWAWVec2f m_pos;
  //! the control point: previous, next
  MWAWVec2f m_controlPoints[2];
  //! the point type
  int m_type;
};

//! Internal: the structure used to store a transformation ClarisDrawGraph
struct Transformation {
  //! constructor
  Transformation() : m_rotate(0), m_originalSize()
  {
    for (int i=0; i<2; ++i) m_values[i]=0;
  }
  friend std::ostream &operator<<(std::ostream &o, Transformation const &trans)
  {
    if (trans.m_rotate<0||trans.m_rotate>0)
      o << "rot=" << trans.m_rotate << ",";
    if (trans.m_originalSize!=MWAWVec2f(0,0))
      o << "size=" << trans.m_originalSize << ",";
    o << "val=[";
    for (int i=0; i<2; ++i) o << trans.m_values[i] << ",";
    o << "],";
    return o;
  }

  //! the rotation
  float m_rotate;
  //! the original size
  MWAWVec2f m_originalSize;
  //! other values
  float m_values[2];
};
//! Internal: the structure used to store a style of a ClarisDrawGraph
struct Style : public MWAWGraphicStyle {
  //! constructor
  Style(): MWAWGraphicStyle(), m_wrapping(0), m_surfacePatternType(0)
  {
  }
  //! returns the wrapping
  MWAWPosition::Wrapping getWrapping() const
  {
    switch (m_wrapping&3) {
    case 0:
      return MWAWPosition::WBackground;
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
    return o;
  }
  //! the wrap type
  int m_wrapping;
  //! the surface pattern type
  int m_surfacePatternType;
};

//! Internal: the generic structure used to store a zone of a ClarisDrawGraph
struct Zone {
  //! the list of types
  enum Type { T_Zone, T_Pict, T_Shape, T_Unknown,
              /* basic subtype */
              T_Line, T_Rect, T_RectOval, T_Oval, T_Arc, T_Poly, T_Connector
            };
  //! constructor
  Zone() : m_zoneType(0), m_page(-1), m_box(), m_ordering(-1), m_style() {}
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Zone const &zone)
  {
    if (zone.m_page >= 0) o << "pg=" << zone.m_page << ",";
    o << "box=" << zone.m_box << ",";
    if (zone.m_ordering>0) o << "ordering=" << zone.m_ordering << ",";
    switch (zone.m_zoneType&0xF) {
    case 0:
      break;
    case 4:
      o << "header/footer,";
      break;
    case 0xa:
      o << "master,";
      break;
    default:
      o << "zoneType=" << (zone.m_zoneType&0xF) << ",";
      break;
    }
    if (zone.m_zoneType&0x20)
      o << "zoneType[0x20],";
    zone.print(o);
    o << "style=[" << zone.m_style << "],";
    return o;
  }
  //! destructor
  virtual ~Zone() {}
  //! return the zone bdbox
  MWAWBox2f getBdBox() const
  {
    MWAWVec2f minPt(m_box[0][0], m_box[0][1]);
    MWAWVec2f maxPt(m_box[1][0], m_box[1][1]);
    for (int c=0; c<2; ++c) {
      if (m_box.size()[c]>=0) continue;
      minPt[c]=m_box[1][c];
      maxPt[c]=m_box[0][c];
    }
    return MWAWBox2f(minPt,maxPt);
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
  virtual int getNumData() const
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
  //! the zone type
  int m_zoneType;
  //! the page (checkme: or frame linked )
  int m_page;
  //! the bdbox
  MWAWBox2f m_box;
  //! the ordering
  int m_ordering;
  //! the style
  Style m_style;
};

//! Internal: small class to store a basic graphic zone of a ClarisDrawGraph
struct ZoneShape : public Zone {
  //! constructor
  ZoneShape(Zone const &z, Type type) : Zone(z), m_type(type), m_shape(), m_autosize(false)
  {
  }
  //! print the data
  virtual void print(std::ostream &o) const
  {
    o << m_shape;
    if (m_autosize) o << "autosize,";
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
  virtual int getNumData() const
  {
    return (m_type==T_Connector || m_type==T_Poly) ? 1 : 0;
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
  //! true if autosize is set
  bool m_autosize;
};

//! Internal: the structure used to store a PICT
struct ZonePict : public Zone {
  //! constructor
  ZonePict(Zone const &z) : Zone(z), m_type(T_Pict)
  {
  }
  //! print the data
  virtual void print(std::ostream &o) const
  {
    o << "PICTURE,";
  }
  //! return the main type T_Picture
  virtual Type getType() const
  {
    return T_Pict;
  }
  //! return the sub type
  virtual Type getSubType() const
  {
    return T_Pict;
  }
  //! return the number of data in a file
  virtual int getNumData() const
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

//! Internal: structure to store a bitmap of a ClarisDrawGraph
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
  MWAWVec2i m_bitmapSize;
  //! the bitmap row size in the file ( with potential alignment)
  int m_bitmapRowSize;
  //! the bitmap entry
  MWAWEntry m_entry;
  //! the color map
  std::vector<MWAWColor> m_colorMap;
};

//! Internal: structure to store a link to a zone of a ClarisDrawGraph
struct ZoneZone : public Zone {
  //! constructor
  ZoneZone(Zone const &z, Type fileType) : Zone(z), m_subType(fileType), m_id(-1), m_subId(-1), m_frameId(-1), m_frameSubId(-1), m_frameLast(true), m_transformationId(-1), m_wrappingSep(5)
  {
    for (int i = 0; i < 9; i++)
      m_flags[i] = 0;
  }
  //! return true if the zone is a note
  bool isANote() const
  {
    return m_flags[4]==1;
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

  //! print the zone
  virtual void print(std::ostream &o) const
  {
    o << "ZONE, id=" << m_id << ",";
    if (m_subId > 0) o << "subId=" << m_subId << ",";
    if (m_transformationId >= 0) o << "transf=T" << m_transformationId << ",";
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
  virtual int getNumData() const
  {
    return 0;
  }
  //! returns the id of the reference zone
  virtual int getZoneId() const
  {
    return m_id;
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
  //! the transformation id
  int m_transformationId;
  //! the wrapping separator
  int m_wrappingSep;
  //! flag
  int m_flags[9];
};

//! Internal: structure used to store an unknown zone of a ClarisDrawGraph
struct ZoneUnknown : public Zone {
  //! construtor
  ZoneUnknown(Zone const &z) : Zone(z), m_type(T_Unknown), m_typeId(-1)
  {
  }
  //! print the zone
  virtual void print(std::ostream &o) const
  {
    o << "##type=" << m_typeId << ",";
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
  virtual int getNumData() const
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

  //! returns true if the group is empty
  bool isEmpty() const
  {
    return m_zones.empty();
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
      shared_ptr<ClarisDrawGraphInternal::Zone> child = *it;
      if (!child || child->getType() != ClarisDrawGraphInternal::Zone::T_Zone)
        continue;
      ClarisDrawGraphInternal::ZoneZone const &childZone =
        static_cast<ClarisDrawGraphInternal::ZoneZone &>(*child);
      if (childZone.m_id != cId) continue;
      m_zones.erase(it);
      return;
    }
    MWAW_DEBUG_MSG(("ClarisDrawGraphInternal::Group can not detach %d\n", cId));
  }

  /** the list of child zones */
  std::vector<shared_ptr<Zone> > m_zones;

  //! a flag to know if this zone contains or no the call to zone 1
  bool m_hasMainZone;
  //! the list of block to send
  std::vector<shared_ptr<Zone> > m_zonesToSend;
};

////////////////////////////////////////
//! Internal: the state of a ClarisDrawGraph
struct State {
  //! constructor
  State() : m_numPages(0), m_pageDimensions(0,0), m_masterId(-1), m_transformations(),
    m_groupMap(), m_bitmapMap(), m_positionsComputed(false), m_frameId(0) { }
  //! returns true if a group does not exist or is empty
  bool isEmptyGroup(int gId) const
  {
    return m_groupMap.find(gId)==m_groupMap.end() || !m_groupMap.find(gId)->second || m_groupMap.find(gId)->second->isEmpty();
  }
  //! the number of pages
  int m_numPages;
  //! the page dimension if known (in point)
  MWAWVec2f m_pageDimensions;
  //! the master group id ( in a draw file )
  int m_masterId;
  //! the list of transformation
  std::vector<Transformation> m_transformations;
  //! a map zoneId -> group
  std::map<int, shared_ptr<Group> > m_groupMap;
  //! a map zoneId -> group
  std::map<int, shared_ptr<Bitmap> > m_bitmapMap;
  //! true if the ClarisDrawGraph::computePositions was called
  bool m_positionsComputed;
  //! the actuel frame id
  int m_frameId;
};

////////////////////////////////////////
//! Internal: the subdocument of a ClarisDrawGraph
class SubDocument : public MWAWSubDocument
{
public:
  //! constructor for zone
  SubDocument(ClarisDrawGraph &pars, MWAWInputStreamPtr input, int zoneId, int subId) :
    MWAWSubDocument(pars.m_mainParser, input, MWAWEntry()), m_graphParser(&pars), m_id(zoneId), m_subId(subId), m_label("") {}
  //! constructor for label
  SubDocument(ClarisDrawGraph &pars, MWAWInputStreamPtr input, std::string const &label) :
    MWAWSubDocument(pars.m_mainParser, input, MWAWEntry()), m_graphParser(&pars), m_id(-1), m_subId(-1), m_label(label) {}

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
    if (m_subId != sDoc->m_subId) return true;
    if (m_label != sDoc->m_label) return true;
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
  ClarisDrawGraph *m_graphParser;

protected:
  //! the subdocument id
  int m_id;
  //! the subdocument sub id
  int m_subId;
  //! the label
  std::string m_label;
private:
  SubDocument(SubDocument const &orig);
  SubDocument &operator=(SubDocument const &orig);
};

void SubDocument::parse(MWAWListenerPtr &listener, libmwaw::SubDocumentType type)
{
  if (!listener || (type==libmwaw::DOC_TEXT_BOX&&!listener->canWriteText())) {
    MWAW_DEBUG_MSG(("ClarisDrawGraphInternal::SubDocument::parse: no listener\n"));
    return;
  }
  if (!m_graphParser) {
    MWAW_DEBUG_MSG(("ClarisDrawGraphInternal::SubDocument::parse: no graph parser\n"));
    return;
  }
  if (m_id<0) {
    if (m_label.empty()) {
      MWAW_DEBUG_MSG(("ClarisDrawGraphInternal::SubDocument::parse: can not find the label\n"));
      return;
    }
    listener->setFont(MWAWFont(3,10));
    MWAWParagraph para;
    para.m_justify = MWAWParagraph::JustificationCenter;
    listener->setParagraph(para);
    listener->insertUnicodeString(m_label.c_str());
    return;
  }
  long pos = m_input->tell();
  m_graphParser->sendTextZone(m_id, m_subId);
  m_input->seek(pos, librevenge::RVNG_SEEK_SET);
}

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
ClarisDrawGraph::ClarisDrawGraph(ClarisDrawParser &parser) :
  m_parserState(parser.getParserState()), m_state(new ClarisDrawGraphInternal::State), m_mainParser(&parser),
  m_styleManager(parser.m_styleManager)
{
}

ClarisDrawGraph::~ClarisDrawGraph()
{ }

int ClarisDrawGraph::version() const
{
  return m_parserState->m_version;
}

void ClarisDrawGraph::resetState()
{
  m_state.reset(new ClarisDrawGraphInternal::State);
}

int ClarisDrawGraph::numPages() const
{
  if (m_state->m_numPages>0) return m_state->m_numPages;

  int nPages = 1;
  std::map<int, shared_ptr<ClarisDrawGraphInternal::Group> >::iterator iter;
  for (iter=m_state->m_groupMap.begin() ; iter != m_state->m_groupMap.end() ; ++iter) {
    shared_ptr<ClarisDrawGraphInternal::Group> group = iter->second;
    if (!group) continue;
    int lastPage=group->getMaximumPage();
    if (lastPage>nPages) nPages=lastPage;
  }
  m_state->m_numPages=nPages;
  return nPages;
}

bool ClarisDrawGraph::sendTextZone(int number, int subZone)
{
  return m_mainParser->sendTextZone(number, subZone);
}

bool ClarisDrawGraph::isEmptyGroup(int gId) const
{
  return m_state->isEmptyGroup(gId);
}

////////////////////////////////////////////////////////////
// Intermediate level
////////////////////////////////////////////////////////////
void ClarisDrawGraph::updateGroup(bool isLibrary)
{
  // first looked for frame link and remove the empty link
  std::stack<int> emptyGroup;
  std::multimap<int, int> fatherMap;
  std::map<int, std::map<int, ClarisDrawGraphInternal::ZoneZone *> > idSIdCIdMap;

  std::map<int, shared_ptr<ClarisDrawGraphInternal::Group> >::iterator iter;
  for (iter=m_state->m_groupMap.begin() ; iter != m_state->m_groupMap.end() ; ++iter) {
    shared_ptr<ClarisDrawGraphInternal::Group> group = iter->second;
    if (!group) continue;
    if (group->isEmpty()) {
      emptyGroup.push(group->m_id);
      continue;
    }
    for (size_t i=0; i<group->m_zones.size(); ++i) {
      shared_ptr<ClarisDrawGraphInternal::Zone> child=group->m_zones[i];
      if (child->getType() != ClarisDrawGraphInternal::Zone::T_Zone) continue;
      ClarisDrawGraphInternal::ZoneZone *cChild = dynamic_cast<ClarisDrawGraphInternal::ZoneZone *>(child.get());
      if (!cChild) {
        MWAW_DEBUG_MSG(("ClarisDrawGraph::updateGroup: oops can not find a child\n"));
        continue;
      }
      int zId=cChild->getZoneId();
      fatherMap.insert(std::multimap<int,int>::value_type(group->m_id,zId));

      // now look for linked text box text
      if (m_mainParser->getFileType(zId)!=1) continue;
      if (idSIdCIdMap.find(zId)==idSIdCIdMap.end())
        idSIdCIdMap.insert(std::map<int, std::map<int, ClarisDrawGraphInternal::ZoneZone *> >::value_type
                           (zId, std::map<int, ClarisDrawGraphInternal::ZoneZone *>()));
      std::map<int, ClarisDrawGraphInternal::ZoneZone *> &sIdCIdMap=idSIdCIdMap.find(zId)->second;
      if (sIdCIdMap.find(cChild->m_subId)!=sIdCIdMap.end()) {
        MWAW_DEBUG_MSG(("ClarisDrawGraph::updateGroup: zone %d already find with subId %d\n",
                        zId, cChild->m_subId));
      }
      else
        sIdCIdMap[cChild->m_subId]=cChild;
    }
  }

  // update the linked frame data, ie. zone->m_frame*
  std::map<int, std::map<int, ClarisDrawGraphInternal::ZoneZone *> >::const_iterator it;
  for (it=idSIdCIdMap.begin(); it!=idSIdCIdMap.end(); ++it) {
    std::map<int, ClarisDrawGraphInternal::ZoneZone *> const &sIdCIdMap=it->second;
    if (sIdCIdMap.size()<=1) continue;
    int frameId=m_state->m_frameId++;
    int frameSubId=0;
    for (std::map<int, ClarisDrawGraphInternal::ZoneZone *>::const_iterator cIt=sIdCIdMap.begin(); cIt!=sIdCIdMap.end();) {
      ClarisDrawGraphInternal::ZoneZone *child =cIt++->second;
      child->m_frameId=frameId;
      child->m_frameSubId=frameSubId++;
      child->m_frameLast=cIt==sIdCIdMap.end();
    }
  }

  while (!emptyGroup.empty()) {
    int id=emptyGroup.top();
    emptyGroup.pop();
    std::multimap<int, int>::const_iterator fIt=fatherMap.find(id);
    while (fIt!=fatherMap.end() && fIt->first==id) {
      int fId=fIt++->second;
      if (isLibrary && fId==1) continue;
      iter=m_state->m_groupMap.find(fId);
      if (iter==m_state->m_groupMap.end() || !iter->second) {
        MWAW_DEBUG_MSG(("ClarisDrawGraph::readTransformations: oops can not find a father\n"));
        continue;
      }
      iter->second->removeChild(id, true);
      if (iter->second->isEmpty())
        emptyGroup.push(fId);
    }
  }
}

bool ClarisDrawGraph::getSurfaceColor(ClarisDrawGraphInternal::Style const &style, MWAWColor &col) const
{
  if (!style.hasSurfaceColor())
    return false;
  col = style.m_surfaceColor;
  return true;
}

bool ClarisDrawGraph::readTransformations()
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos=input->tell();
  if (!input->checkPosition(pos+12)) return false;
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(Transformation):";
  long sz = (long) input->readULong(4);
  if (sz==0) {
    ascFile.addPos(pos);
    ascFile.addNote("_");
    return true;
  }
  long endPos=pos+4+sz;
  if (!input->checkPosition(endPos)) {
    MWAW_DEBUG_MSG(("ClarisDrawGraph::readTransformations: the data size seems bad\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }

  int N = (int) input->readLong(2);
  f << "N=" << N << ",";
  int type = (int) input->readLong(2);
  if (type != -1)
    f << "#type=" << type << ",";
  int val = (int) input->readLong(2);
  if (val) f << "#unkn=" << val << ",";
  int fSz = (int) input->readULong(2);
  int hSz = (int) input->readULong(2);
  if (N*fSz+hSz+12 != sz) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisDrawGraph::readTransformations: unexpected field/header size\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    return false;
  }

  if (long(input->tell()) != pos+4+12+hSz) {
    ascFile.addDelimiter(input->tell(), '|');
    input->seek(pos+4+12+hSz, librevenge::RVNG_SEEK_SET);
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  pos=input->tell();
  if (fSz!=30) {
    input->seek(endPos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisDrawGraph::readTransformations: no sure how to read the data\n"));
    ascFile.addPos(pos);
    ascFile.addNote("Transformation-data:###");
    return true;
  }

  for (int i=0; i<N; ++i) {
    pos=input->tell();
    f.str("");
    f << "Transformation-" << i << ":";
    ClarisDrawGraphInternal::Transformation trans;
    float angle= float(input->readLong(4))/65536.f; // in radians
    trans.m_rotate = float(180./M_PI*angle);
    float dim[2];
    for (int j=0; j<2; ++j) dim[j]=float(input->readLong(4))/256.f;
    trans.m_originalSize=MWAWVec2f(dim[1],dim[0]);
    for (int j=0; j<2; ++j) trans.m_values[j]=float(input->readLong(4))/256.f;
    f << trans;
    for (int j=0; j<5; ++j) { // always 0
      val=(int) input->readLong(2);
      if (val) f << "f" << j+1 << "=" << val << ",";
    }
    m_state->m_transformations.push_back(trans);
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  return true;
}

////////////////////////////////////////////////////////////
// a group of data mainly graphic
////////////////////////////////////////////////////////////
shared_ptr<ClarisWksStruct::DSET> ClarisDrawGraph::readGroupZone
(ClarisWksStruct::DSET const &zone, MWAWEntry const &entry, bool isLibHeader)
{
  if (!entry.valid() || zone.m_fileType != 0)
    return shared_ptr<ClarisWksStruct::DSET>();
  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos+8+16, librevenge::RVNG_SEEK_SET); // avoid header+8 generic number
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  shared_ptr<ClarisDrawGraphInternal::Group> group(new ClarisDrawGraphInternal::Group(zone));

  f << "Entries(GroupDef):" << *group << ",";
  if (isLibHeader) f << "isLib,";
  int val = (int) input->readLong(2); // a small int between 0 and 3
  switch (val) {
  case 0:
    break; // normal
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
      MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupZone: can not find definition size\n"));
      input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
      return shared_ptr<ClarisWksStruct::DSET>();
    }

    MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupZone: unexpected size for zone definition, try to continue\n"));
  }

  long beginDefGroup = entry.end()-N*data0Length;
  if (long(input->tell())+42 <= beginDefGroup) {
    input->seek(beginDefGroup-42, librevenge::RVNG_SEEK_SET);
    pos = input->tell();
    if (!readGroupUnknown(*group, 42, -1)) {
      ascFile.addPos(pos);
      ascFile.addNote("GroupDef_A###:");
    }
  }

  input->seek(beginDefGroup, librevenge::RVNG_SEEK_SET);

  for (int i = 0; i < N; i++) {
    pos = input->tell();
    MWAWEntry gEntry;
    gEntry.setBegin(pos);
    gEntry.setLength(data0Length);
    shared_ptr<ClarisDrawGraphInternal::Zone> def = readGroupDef(gEntry);
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
  if (!readGroupData(*group, entry.begin(), isLibHeader)) {
    ascFile.addPos(entry.begin());
    ascFile.addNote("###");
  }

  group->m_childs.resize(group->m_zones.size());
  for (size_t i = 0; i < group->m_zones.size(); ++i) {
    if (!group->m_zones[i]) continue;
    group->m_childs[size_t(i)] = group->m_zones[i]->getChild();
  }

  if (m_state->m_groupMap.find(group->m_id) != m_state->m_groupMap.end()) {
    MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupZone: zone %d already exists!!!\n", group->m_id));
  }
  else
    m_state->m_groupMap[group->m_id] = group;

  return group;
}

////////////////////////////////////////////////////////////
// read a bitmap zone
////////////////////////////////////////////////////////////
shared_ptr<ClarisWksStruct::DSET> ClarisDrawGraph::readBitmapZone(ClarisWksStruct::DSET const &zone, MWAWEntry const &entry)
{
  if (!entry.valid() || zone.m_fileType != 4)
    return shared_ptr<ClarisWksStruct::DSET>();
  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos+8+16, librevenge::RVNG_SEEK_SET); // avoid header+8 generic number
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  shared_ptr<ClarisDrawGraphInternal::Bitmap> bitmap(new ClarisDrawGraphInternal::Bitmap(zone));

  f << "Entries(BitmapDef):" << *bitmap << ",";

  ascFile.addDelimiter(input->tell(), '|');

  // read the last part
  long data0Length = zone.m_dataSz;
  long N = zone.m_numData;
  if (entry.length() -8-12 != data0Length*N + zone.m_headerSz) {
    if (data0Length == 0 && N) {
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      MWAW_DEBUG_MSG(("ClarisDrawGraph::readBitmapZone: can not find definition size\n"));
      input->seek(entry.end(), librevenge::RVNG_SEEK_SET);
      return shared_ptr<ClarisWksStruct::DSET>();
    }

    MWAW_DEBUG_MSG(("ClarisDrawGraph::readBitmapZone: unexpected size for zone definition, try to continue\n"));
  }

  bool sizeSet=false;
  int const sizePos = 88;
  if (sizePos && pos+sizePos+4+N*data0Length < entry.end()) {
    input->seek(pos+sizePos, librevenge::RVNG_SEEK_SET);
    ascFile.addDelimiter(pos+sizePos,'[');
    int dim[2]; // ( we must add 2 to add the border )
    for (int j = 0; j < 2; j++)
      dim[j] = (int) input->readLong(2);
    f << "sz=" << dim[1] << "x" << dim[0] << ",";
    if (dim[0] > 0 && dim[1] > 0) {
      bitmap->m_bitmapSize = MWAWVec2i(dim[1]+2, dim[0]+2);
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
      bitmap->m_bitmapSize = MWAWVec2i(dim[0]+2, dim[1]+2);

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
    MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupZone: zone %d already exists!!!\n", bitmap->m_id));
  }
  else
    m_state->m_bitmapMap[bitmap->m_id] = bitmap;

  return bitmap;
}

////////////////////////////////////////////////////////////
// definition of an element of a group
////////////////////////////////////////////////////////////
shared_ptr<ClarisDrawGraphInternal::Zone> ClarisDrawGraph::readGroupDef(MWAWEntry const &entry)
{
  shared_ptr<ClarisDrawGraphInternal::Zone> res;
  if (entry.length() < 32) {
    MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupDef: sz is too short!!!\n"));
    return res;
  }
  long pos = entry.begin();
  MWAWInputStreamPtr &input= m_parserState->m_input;
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "GroupDef:";

  ClarisDrawGraphInternal::Zone zone;
  ClarisDrawGraphInternal::Style &style = zone.m_style;

  int typeId = (int) input->readULong(1);
  ClarisDrawGraphInternal::Zone::Type type = ClarisDrawGraphInternal::Zone::T_Unknown;
  switch (typeId) {
  case 1:
    type = ClarisDrawGraphInternal::Zone::T_Zone;
    break;
  case 2:
    type = ClarisDrawGraphInternal::Zone::T_Line;
    break;
  case 3:
    type = ClarisDrawGraphInternal::Zone::T_Rect;
    break;
  case 4:
    type = ClarisDrawGraphInternal::Zone::T_RectOval;
    break;
  case 5:
    type = ClarisDrawGraphInternal::Zone::T_Oval;
    break;
  case 6:
    type = ClarisDrawGraphInternal::Zone::T_Arc;
    break;
  case 7:
    type = ClarisDrawGraphInternal::Zone::T_Connector;
    break;
  case 8:
    type = ClarisDrawGraphInternal::Zone::T_Poly;
    break;
  case 10:
    type = ClarisDrawGraphInternal::Zone::T_Pict;
    break;
  default:
    MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupDef: find unknown type=%d!!!\n", typeId));
    f << "###typeId=" << typeId << ",";
    break;
  }
  int val = (int) input->readULong(1);
  style.m_wrapping = (val & 3);
  if (val>>2) f << "unk=" << (val>>2) << ","; // 0 or 8
  zone.m_zoneType = (int) input->readULong(1);
  if (zone.m_zoneType & 0x40) style.m_arrows[0]=true;
  if (zone.m_zoneType & 0x80) style.m_arrows[1]=true;
  zone.m_zoneType &= 0x3F;
  val = (int) input->readULong(1);
  if (val) f << "f0=" << std::hex << val << std::dec << ",";

  float dim[4];
  for (int j = 0; j < 4; j++) {
    dim[j] = float(input->readLong(4))/256.f;
    if (dim[j] < -100) f << "##dim?,";
  }
  zone.m_box = MWAWBox2f(MWAWVec2f(dim[1], dim[0]), MWAWVec2f(dim[3], dim[2]));
  style.m_lineWidth = float(input->readLong(2))/256.f;
  for (int j = 0; j < 2; j++) {
    int col = (int) input->readULong(1);
    MWAWColor color;
    if (!m_styleManager->getColor(col, color)) {
      MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupDef: unknown color!!!\n"));
      f << "###col" << j << "=" << col << ",";
    }
    else if (j==0)
      style.m_lineColor=color;
    else
      style.setSurfaceColor(color);
  }

  for (int j = 0; j < 2; j++) {
    // CHECKME
    val = (int) input->readULong(1);  // probably also related to surface
    if (val) f << "pat" << j << "[high]=" << val << ",";
    int pat = (int) input->readULong(1);
    if (j==1 && (zone.m_zoneType & 0x10)) {
      f << "hasGrad,";
      zone.m_zoneType &= 0xEF;
      m_styleManager->updateGradient(pat, style);
      continue;
    }
    if (pat==1) {
      if (j==0) style.m_lineOpacity=0;
      else style.m_surfaceOpacity=0;
      continue;
    }
    MWAWGraphicStyle::Pattern pattern;
    if (!m_styleManager->getPattern(pat,pattern)) {
      MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupDef: unknown pattern!!!\n"));
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
  case ClarisDrawGraphInternal::Zone::T_Zone: {
    int nFlags = 0;
    ClarisDrawGraphInternal::ZoneZone *z = new ClarisDrawGraphInternal::ZoneZone(zone, type);
    res.reset(z);
    for (int j=0; j < 3; ++j)
      z->m_flags[nFlags++] = (int) input->readLong(2);
    z->m_id = (int) input->readULong(2);

    int numRemains = int(entry.end()-long(input->tell()));
    numRemains/=2;
    if (numRemains > 9) numRemains = 9;
    for (int j = 0; j < numRemains; j++) {
      val = (int) input->readLong(2);
      switch (j) {
      case 1:
        z->m_subId = val;
        break;
      case 2:
        z->m_wrappingSep = val;
        break;
      case 6:
        z->m_transformationId = val;
        break;
      default:
        z->m_flags[nFlags++] = val;
        break;
      }
    }
    break;
  }
  case ClarisDrawGraphInternal::Zone::T_Connector:
  case ClarisDrawGraphInternal::Zone::T_Line:
  case ClarisDrawGraphInternal::Zone::T_Rect:
  case ClarisDrawGraphInternal::Zone::T_RectOval:
  case ClarisDrawGraphInternal::Zone::T_Oval:
  case ClarisDrawGraphInternal::Zone::T_Arc:
  case ClarisDrawGraphInternal::Zone::T_Poly: {
    ClarisDrawGraphInternal::ZoneShape *z = new ClarisDrawGraphInternal::ZoneShape(zone, type);
    res.reset(z);
    readShape(entry, *z);
    break;
  }
  case ClarisDrawGraphInternal::Zone::T_Pict: // CHECKME
    res.reset(new ClarisDrawGraphInternal::ZonePict(zone));
    break;
  case ClarisDrawGraphInternal::Zone::T_Shape:
  case ClarisDrawGraphInternal::Zone::T_Unknown:
  default: {
    ClarisDrawGraphInternal::ZoneUnknown *z = new ClarisDrawGraphInternal::ZoneUnknown(zone);
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
bool ClarisDrawGraph::readGroupData(ClarisDrawGraphInternal::Group &group, long beginGroupPos, bool isLibHeader)
{
  if (!readGroupHeader(group)) {
    MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupData: unexpected graphic1\n"));
    return false;
  }

  MWAWInputStreamPtr &input= m_parserState->m_input;

  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  long pos, sz;
  size_t numChilds = group.m_zones.size();
  int numError = 0;
  int numConnectors=0;
  for (size_t i = 0; i < numChilds; i++) {
    shared_ptr<ClarisDrawGraphInternal::Zone> z = group.m_zones[i];
    int numZoneExpected = z ? z->getNumData() : 0;

    if (isLibHeader || z->getSubType()==ClarisDrawGraphInternal::Zone::T_Connector) {
      ++numConnectors;
      if (!ClarisWksStruct::readStructZone(*m_parserState, "ConnectorData", false)) {
        MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupData: can not retrieve connector data\n"));
        return false;
      }
    }

    if (numZoneExpected) {
      pos = input->tell();
      sz = (long) input->readULong(4);
      f.str("");
      if (sz == 0) {
        MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupData: find a nop zone for type: %d\n",
                        z->getSubType()));
        ascFile.addPos(pos);
        ascFile.addNote("GroupDef-before:###");
        if (!numError++) {
          ascFile.addPos(beginGroupPos);
          ascFile.addNote("###");
        }
        else {
          MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupData: too many errors, zone parsing STOPS\n"));
          return false;
        }
        pos = input->tell();
        sz = (long) input->readULong(4);
      }
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      bool parsed = true;
      switch (z->getSubType()) {
      case ClarisDrawGraphInternal::Zone::T_Poly:
      case ClarisDrawGraphInternal::Zone::T_Connector:
        if (z->getNumData() && !readPolygonData(z)) {
          MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupData: can not retrieve polygon child\n"));
          return false;
        }
        break;
      case ClarisDrawGraphInternal::Zone::T_Pict: {
        if (!input->checkPosition(pos+4+sz)) {
          input->seek(pos, librevenge::RVNG_SEEK_SET);
          MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupData: find a odd zone for picture\n"));
          return false;
        }
        ClarisDrawGraphInternal::ZonePict *pict=dynamic_cast<ClarisDrawGraphInternal::ZonePict *>(z.get());
        f.str("");
        f << "Entries(PictData):";
        if (!pict) {
          MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupData: oops can not find the picture\n"));
        }
        else {
          pict->m_entries[0].setBegin(pos+4);
          pict->m_entries[0].setLength(sz);
        }
        if (sz)
          ascFile.skipZone(pos+4, pos+4+sz-1);
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        input->seek(pos+4+sz, librevenge::RVNG_SEEK_SET);

        pos = input->tell();
        sz = (long) input->readULong(4);
        if (!sz) {
          ascFile.addPos(pos);
          ascFile.addNote("_");
          break;
        }
        if (!input->checkPosition(pos+4+sz)) {
          input->seek(pos, librevenge::RVNG_SEEK_SET);
          MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupData: find a odd zone2 for picture\n"));
          return false;
        }
        if (pict) {
          pict->m_entries[1].setBegin(pos+4);
          pict->m_entries[1].setLength(sz);
        }
        MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupData: find a zone2 for picture\n"));
        ascFile.addPos(pos);
        ascFile.addNote("PictData-B:###");
        input->seek(pos+4+sz, librevenge::RVNG_SEEK_SET);
        break;
      }
      case ClarisDrawGraphInternal::Zone::T_Line:
      case ClarisDrawGraphInternal::Zone::T_Rect:
      case ClarisDrawGraphInternal::Zone::T_RectOval:
      case ClarisDrawGraphInternal::Zone::T_Oval:
      case ClarisDrawGraphInternal::Zone::T_Arc:
      case ClarisDrawGraphInternal::Zone::T_Zone:
      case ClarisDrawGraphInternal::Zone::T_Shape:
      case ClarisDrawGraphInternal::Zone::T_Unknown:
      default:
        parsed = false;
        break;
      }

      if (!parsed) {
        if (!input->checkPosition(pos+4+sz)) {
          input->seek(pos, librevenge::RVNG_SEEK_SET);
          MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupData: find a odd zone for type: %d\n",
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
            MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupData: two zones is not implemented for zone: %d\n",
                            z->getSubType()));
            return false;
          }
          ascFile.addPos(pos);
          ascFile.addNote("NOP");
        }
      }
    }
  }

  if (input->isEnd())
    return true;
  pos=input->tell();
  if (numConnectors) {
    int n=(int) input->readULong(2);
    if (n>numConnectors+1) {
      MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupData: unexcepted connector data\n"));
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      n=0;
    }
    else {
      f.str("");
      f << "Entries(ConnectorN):" << n;
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    for (int i=1; i<n; ++i) {
      pos=input->tell();
      if (!ClarisWksStruct::readStructZone(*m_parserState, "ConnectorDef", false)) {
        MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupData: unexcepted connector data\n"));
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        break;
      }
    }
  }

  return true;
}

////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////
bool ClarisDrawGraph::readShape(MWAWEntry const &entry, ClarisDrawGraphInternal::ZoneShape &zone)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  int remainBytes = int(entry.end()-input->tell());
  if (remainBytes < 4)
    return false;

  MWAWVec2f pictSz=zone.getBdBox().size();
  MWAWBox2i box(MWAWVec2f(0,0), pictSz);
  MWAWGraphicShape &shape=zone.m_shape;
  shape.m_bdBox=shape.m_formBox=box;
  libmwaw::DebugStream f;
  bool canHaveDash=false;
  int val;
  for (int i=0; i<2; ++i) {
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  remainBytes-=4;
  int decal=0;
  switch (zone.getSubType()) {
  case ClarisDrawGraphInternal::Zone::T_Line: {
    int yDeb=(zone.m_zoneType&1)?1:0;
    shape = MWAWGraphicShape::line(MWAWVec2f(zone.m_box[0][0],zone.m_box[yDeb][1]),
                                   MWAWVec2f(zone.m_box[1][0],zone.m_box[1-yDeb][1]));
    zone.m_autosize=(zone.m_zoneType&4);
    /* checkme: find also where the grid align is defined and do something like that
       MWAWVec2f vec=shape.m_vertices[1]-shape.m_vertices[0];
       float absDiff[2]={vec[0]<0 ? -vec[0]:vec[0], vec[1]<0 ? -vec[1]:vec[1]};
       if (absDiff[0]<absDiff[1]/4.f)
         shape=MWAWGraphicShape::line(shape.m_vertices[0], shape.m_vertices[0]+MWAWVec2f(0, vec[1]));
       else if (absDiff[1]<absDiff[0]/4.f)
         shape=MWAWGraphicShape::line(shape.m_vertices[0], shape.m_vertices[0]+MWAWVec2f(vec[0],0));
    */
    canHaveDash=true;
    break;
  }
  case ClarisDrawGraphInternal::Zone::T_Connector:
    canHaveDash=true;
    shape.m_type = MWAWGraphicShape::Polygon; // value are defined in second zone
    break;
  case ClarisDrawGraphInternal::Zone::T_Rect:
    shape.m_type = MWAWGraphicShape::Rectangle;
    break;
  case ClarisDrawGraphInternal::Zone::T_Oval:
    shape.m_type = MWAWGraphicShape::Circle;
    break;
  case ClarisDrawGraphInternal::Zone::T_Poly:
    shape.m_type = MWAWGraphicShape::Polygon;
    break; // value are defined in next zone
  case ClarisDrawGraphInternal::Zone::T_Arc: {
    if (remainBytes < 7) {
      MWAW_DEBUG_MSG(("ClarisDrawGraph::readSimpleGraphicZone: arc zone is too short\n"));
      return false;
    }
    decal=3;
    int fileAngle[2];
    for (int i = 0; i < 2; i++)
      fileAngle[i] = (int) input->readLong(2);
    val=(int) input->readLong(1);
    if (val==1) f<< "show[axis],";
    else if (val) f << "#show[axis]=" << val << ",";
    int angle[2] = { 90-fileAngle[0]-fileAngle[1], 90-fileAngle[0] };
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
    MWAWVec2f center = box.center();
    MWAWVec2f axis = 0.5*MWAWVec2f(box.size());
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
    MWAWBox2f realBox(MWAWVec2f(center[0]+minVal[0],center[1]+minVal[1]),
                      MWAWVec2f(center[0]+maxVal[0],center[1]+maxVal[1]));
    zone.m_box=MWAWBox2f(MWAWVec2f(zone.m_box[0])+realBox[0],MWAWVec2f(zone.m_box[0])+realBox[1]);
    shape = MWAWGraphicShape::pie(realBox, box, MWAWVec2f(float(angle[0]),float(angle[1])));
    break;
  }
  case ClarisDrawGraphInternal::Zone::T_RectOval: {
    decal=4;
    shape.m_type = MWAWGraphicShape::Rectangle;
    if (remainBytes < 8) {
      MWAW_DEBUG_MSG(("ClarisDrawGraph::readSimpleGraphicZone: arc zone is too short\n"));
      return false;
    }
    for (int i = 0; i < 2; i++) {
      float dim=float(input->readLong(2))/2.0f;
      shape.m_cornerWidth[i]=(2.f*dim <= pictSz[i]) ? dim : pictSz[i]/2.f;
      val = (int) input->readULong(2);
      if (val) f << "rRect" << i << "=" << val << ",";
    }
    break;
  }
  case ClarisDrawGraphInternal::Zone::T_Pict:
  case ClarisDrawGraphInternal::Zone::T_Zone:
  case ClarisDrawGraphInternal::Zone::T_Shape:
  case ClarisDrawGraphInternal::Zone::T_Unknown:
  default:
    MWAW_DEBUG_MSG(("ClarisDrawGraph::readSimpleGraphicZone: unknown type\n"));
    return false;
  }
  int numRemain = int(entry.end()-input->tell());
  if ((numRemain%2)==1)
    input->seek(1, librevenge::RVNG_SEEK_CUR);
  numRemain /= 2;
  int dashId=0;
  bool hasShadow=false;
  for (int i = 0; i < numRemain; i++) {
    int decalI=i+decal;
    if (decalI==9) {
      zone.m_style.m_shadowOffset=MWAWVec2i((int) input->readLong(1), (int) input->readLong(1));
      hasShadow=(zone.m_style.m_shadowOffset!=MWAWVec2i(0,0));
      continue;
    }
    if (decalI==10) {
      int col=(int) input->readULong(1);
      MWAWColor color;
      if (!m_styleManager->getColor(col, color)) {
        MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupDef: unknown shadow color!!!\n"));
        f << "###shadow[col]=" << col << ",";
      }
      else if (hasShadow)
        zone.m_style.setShadowColor(color);
      col=(int) input->readLong(1);
      if (col) {
        static bool first=true;
        if (first) {
          first=false;
          MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupDef: lighting is not implemented!!!\n"));
        }
        if (!m_styleManager->getColor(col, color)) {
          MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupDef: unknown lighting color!!!\n"));
          f << "###lighting[col]=" << col << ",";
        }
        else
          f << "lighting[col]=" << color << ",";
      }
      continue;
    }
    val = (int) input->readULong(2);
    if (decalI==8) {
      if (val==0xFFFF)
        continue;
      else {
        f << "transf=T" << val << ",";
        if (val<0 || val>=int(m_state->m_transformations.size())) {
          MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupDef: can not find the transformation!!!\n"));
          f << "###";
        }
        else if (shape.m_type!=MWAWGraphicShape::Polygon && shape.m_type!=MWAWGraphicShape::Line) {
          ClarisDrawGraphInternal::Transformation const &trans=m_state->m_transformations[size_t(val)];
          if (shape.m_type!=MWAWGraphicShape::Arc && shape.m_type!=MWAWGraphicShape::Pie)
            shape.m_bdBox.resizeFromCenter(trans.m_originalSize);
          if (shape.m_type==MWAWGraphicShape::Arc || shape.m_type==MWAWGraphicShape::Pie ||
              shape.m_type==MWAWGraphicShape::Circle)
            shape.m_formBox.resizeFromCenter(trans.m_originalSize);
          shape=shape.rotate(trans.m_rotate, shape.m_bdBox.center());
          MWAWVec2f orig=zone.m_box[0]+shape.m_bdBox[0];
          shape.translate(-1.f*shape.m_bdBox[0]);
          zone.m_box=MWAWBox2f(orig, orig+shape.m_bdBox.size());
        }
      }
    }
    if (!val) continue;
    if (decalI==3 && canHaveDash) {
      dashId=(val&0xFFFF);
      if (val>>8)
        f << "g3=" << (val>>8) << ",";
    }
    else if (decalI==4 && canHaveDash && (val>>8)==1) {
      if (!m_styleManager->getDash(dashId+1, zone.m_style.m_lineDashWidth))
        f << "###";
      f << "dashId=D" << dashId+1 << ",";
      if (val&0xFFFF)
        f << "g4=" << (val&0xFFFF) << ",";
    }
    else
      f << "g" << decalI << "=" << std::hex << val << std::dec << ",";
  }
  shape.m_extra=f.str();
  return true;
}

bool ClarisDrawGraph::readGroupHeader(ClarisDrawGraphInternal::Group &group)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  f << "GroupDef(Header):";
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()!=endPos) || (sz && sz < 16)) {
    MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupHeader: zone is too short\n"));
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
    ascFile.addNote("GroupDef-B###:");
    input->seek(pos+fSz, librevenge::RVNG_SEEK_SET);
  }

  int const numHeaders= N==0 ? 1 : N;
  for (int i = 0; i < numHeaders; i++) {
    pos = input->tell();
    std::vector<int> res;
    bool ok = ClarisWksStruct::readIntZone(*m_parserState, "GroupDef", false, 2, res);
    f.str("");
    f << "[GroupDef_B" <<i << "[data0]";
    if (ok) {
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      continue;
    }
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupHeader: can not find data for %d\n", i));
    return true;
  }
  // normally, this is often followed by another list of nop/list of int zone (but not always...)
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    sz=(long) input->readULong(4);
    f.str("");
    f << "[GroupDef_B" << i << "[data1]:";
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
        if (sz>12 && ClarisWksStruct::readIntZone(*m_parserState, "GroupDef", false, 2, res)) {
          ascFile.addPos(pos);
          ascFile.addNote(f.str().c_str());
          continue;
        }
      }
    }
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    if (i!=0) {
      f << "###";
      MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupHeader: find some data for a dataB zone\n"));
    }
    break;
  }

  return true;
}

bool ClarisDrawGraph::readGroupUnknown(ClarisDrawGraphInternal::Group &group, int zoneSz, int id)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  input->seek(pos+zoneSz, librevenge::RVNG_SEEK_SET);
  if (input->tell() != pos+zoneSz) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupUnknown: zone is too short\n"));
    return false;
  }
  libmwaw::DebugFile &ascFile = m_parserState->m_asciiFile;
  libmwaw::DebugStream f;
  if (id >= 0) f << "GroupDef_B" << id << ":";
  else f << "GroupDef_A" << ":";
  if (zoneSz < 42) {
    MWAW_DEBUG_MSG(("ClarisDrawGraph::readGroupUnknown: zone is too short\n"));
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

  MWAWVec2i dim((int)values32[0],(int)values32[1]);
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
bool ClarisDrawGraph::readPolygonData(shared_ptr<ClarisDrawGraphInternal::Zone> zone)
{
  if (!zone || zone->getType() != ClarisDrawGraphInternal::Zone::T_Shape)
    return false;
  ClarisDrawGraphInternal::ZoneShape *bZone =
    static_cast<ClarisDrawGraphInternal::ZoneShape *>(zone.get());
  MWAWGraphicShape &shape = bZone->m_shape;
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != endPos || sz < 12) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisDrawGraph::readPolygonData: file is too short\n"));
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
    MWAW_DEBUG_MSG(("ClarisDrawGraph::readPolygonData: find odd data size\n"));
    return false;
  }
  for (int i = 2; i < 4; i++) {
    val = (int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";

  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  bool isSpline=false;
  std::vector<ClarisDrawGraphInternal::CurvePoint> vertices;
  for (int i = 0; i < N; i++) {
    pos = input->tell();
    f.str("");
    f << "PolygonData-" << i << ":";
    float position[2];
    for (int j = 0; j < 2; j++)
      position[j] = float(input->readLong(4))/256.f;
    ClarisDrawGraphInternal::CurvePoint point(MWAWVec2f(position[1], position[0]));
    if (fSz >= 26) {
      for (int cPt = 0; cPt < 2; cPt++) {
        float ctrlPos[2];
        for (int j = 0; j < 2; j++)
          ctrlPos[j] = float(input->readLong(4))/256.f;
        point.m_controlPoints[cPt] = MWAWVec2f(ctrlPos[1], ctrlPos[0]);
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
    // if (zone.m_zoneType & 1) : we must close the polygon ?
    for (size_t i = 0; i < size_t(N); ++i)
      shape.m_vertices.push_back(vertices[i].m_pos);
    return true;
  }
  shape.m_type = MWAWGraphicShape::Path;
  MWAWVec2f prevPoint, pt1;
  bool hasPrevPoint = false;
  for (size_t i = 0; i < size_t(N); ++i) {
    ClarisDrawGraphInternal::CurvePoint const &pt = vertices[i];
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
// read bitmap picture
////////////////////////////////////////////////////////////
bool ClarisDrawGraph::readBitmapColorMap(std::vector<MWAWColor> &cMap)
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
    MWAW_DEBUG_MSG(("ClarisDrawGraph::readBitmapColorMap: file is too short\n"));
    return false;
  }

  input->seek(pos+4, librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(BitmapColor):";
  f << "unkn=" << input->readLong(4) << ",";
  int maxColor = (int) input->readLong(4);
  if (sz != 8+8*(maxColor+1)) {
    MWAW_DEBUG_MSG(("ClarisDrawGraph::readBitmapColorMap: sz is odd\n"));
    return false;
  }
  cMap.resize(size_t(maxColor+1));
  for (int i = 0; i <= maxColor; i++) {
    int id = (int) input->readULong(2);
    if (id != i) {
      MWAW_DEBUG_MSG(("ClarisDrawGraph::readBitmapColorMap: find odd index : %d\n", i));
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

bool ClarisDrawGraph::readBitmapData(ClarisDrawGraphInternal::Bitmap &zone)
{
  MWAWInputStreamPtr &input= m_parserState->m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  input->seek(endPos, librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != endPos || !sz) {
    MWAW_DEBUG_MSG(("ClarisDrawGraph::readBitmapData: file is too short\n"));
    return false;
  }

  long numPixels = zone.m_bitmapSize[0]*zone.m_bitmapSize[1];
  if (numPixels<=0) {
    MWAW_DEBUG_MSG(("ClarisDrawGraph::readBitmapData: unexpected empty size\n"));
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
    MWAW_DEBUG_MSG(("ClarisDrawGraph::readBitmapData: unexpected size\n"));
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

////////////////////////////////////////////////////////////
// send data to the listener
////////////////////////////////////////////////////////////

bool ClarisDrawGraph::sendMainGroupChild(int childId, MWAWPosition const &position)
{
  MWAWGraphicListenerPtr listener=m_mainParser->getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("ClarisDrawGraph::sendMainGroupChild: can not find the listener\n"));
    return false;
  }
  std::map<int, shared_ptr<ClarisDrawGraphInternal::Group> >::iterator iter= m_state->m_groupMap.find(1);
  if (iter == m_state->m_groupMap.end() || !iter->second) {
    MWAW_DEBUG_MSG(("ClarisDrawGraph::sendMainGroupChild: can not find the main group\n"));
    return false;
  }
  shared_ptr<ClarisDrawGraphInternal::Group> group = iter->second;
  group->m_parsed=true;
  if (childId<0||childId>=(int) group->m_zones.size() || !group->m_zones[size_t(childId)]) {
    MWAW_DEBUG_MSG(("ClarisDrawGraph::sendMainGroupChild: can not find child %d\n", childId));
    return false;
  }
  shared_ptr<ClarisDrawGraphInternal::Zone> child=group->m_zones[size_t(childId)];
  MWAWBox2f box=child->getBdBox();
  MWAWPosition pos(position.origin(), box.size(), librevenge::RVNG_POINT);
  pos.m_anchorTo=MWAWPosition::Page;

  ClarisDrawGraphInternal::Style cStyle = child->m_style;
  pos.m_wrapping=cStyle.getWrapping();
  pos.setOrder(child->m_ordering);
  ClarisDrawGraphInternal::Zone::Type type = child->getType();
  if (type==ClarisDrawGraphInternal::Zone::T_Shape)
    sendShape(static_cast<ClarisDrawGraphInternal::ZoneShape &>(*child), pos);
  else if (type==ClarisDrawGraphInternal::Zone::T_Pict) {
    ClarisDrawGraphInternal::ZonePict &pict=static_cast<ClarisDrawGraphInternal::ZonePict &>(*child);
    if (pict.m_entries[0].valid()) {
      MWAWInputStreamPtr input=m_parserState->m_input;
      long actPos=input->tell();
      input->seek(pict.m_entries[0].begin(), librevenge::RVNG_SEEK_SET);
      shared_ptr<MWAWPict> thePict(MWAWPictData::get(input, (int)pict.m_entries[0].length()));
      input->seek(actPos, librevenge::RVNG_SEEK_SET);
      librevenge::RVNGBinaryData data;
      std::string pictType;
      if (thePict && thePict->getBinary(data,pictType))
        listener->insertPicture(pos, data, pictType);
    }
  }
  else if (type==ClarisDrawGraphInternal::Zone::T_Zone) {
    int cId=child->getZoneId();
    switch (m_mainParser->getFileType(child->getZoneId())) {
    case 0:
      if (m_state->isEmptyGroup(cId))
        break;
      listener->openGroup(pos);
      sendGroup(cId, pos.origin());
      listener->closeGroup();
      break;
    case 1: {
      ClarisDrawGraphInternal::ZoneZone *cZone=dynamic_cast<ClarisDrawGraphInternal::ZoneZone *>(child.get());
      if (cZone && cZone->isANote()) {
        cStyle.m_lineWidth=1;

        MWAWBorder border;
        border.m_color=MWAWColor::black();
        border.m_width=1;
        cStyle.setBorders(libmwaw::LeftBit|libmwaw::BottomBit|libmwaw::RightBit, border);
        border.m_color=MWAWColor(0x60,0x60,0); // normally pattern of yellow and black
        border.m_width=20;
        cStyle.setBorders(libmwaw::TopBit, border);

        cStyle.setSurfaceColor(MWAWColor(0xff,0xff,0));
        cStyle.m_shadowOffset=MWAWVec2i(3,3);
        cStyle.setShadowColor(MWAWColor(0x80,0x80,0x80));
      }
      if (cZone->m_transformationId>=0 && cZone->m_transformationId<(int) m_state->m_transformations.size())
        cStyle.m_rotate=m_state->m_transformations[size_t(cZone->m_transformationId)].m_rotate;
      shared_ptr<MWAWSubDocument> doc(new ClarisDrawGraphInternal::SubDocument
                                      (*this, m_parserState->m_input, cId, -1));
      listener->insertTextBox(pos, doc, cStyle);
      break;
    }
    case 4:
      sendBitmap(cId, pos);
      break;
    default:
      MWAW_DEBUG_MSG(("ClarisDrawGraph::sendMainGroupChild: find unexpected group type\n"));
      break;
    }
  }

  return true;
}
bool ClarisDrawGraph::sendGroup(int number, MWAWPosition const &position)
{
  MWAWGraphicListenerPtr listener=m_mainParser->getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("ClarisDrawGraph::sendGroup: can not find the listener\n"));
    return false;
  }
  std::map<int, shared_ptr<ClarisDrawGraphInternal::Group> >::iterator iter
    = m_state->m_groupMap.find(number);
  if (iter == m_state->m_groupMap.end() || !iter->second)
    return false;
  shared_ptr<ClarisDrawGraphInternal::Group> group = iter->second;
  group->m_parsed=true;
  for (size_t i=0; i<group->m_zones.size(); ++i) {
    shared_ptr<ClarisDrawGraphInternal::Zone> child=group->m_zones[i];
    if (!child)
      continue;

    MWAWBox2f box=child->getBdBox();
    MWAWPosition pos(box[0]+position.origin(), box.size(), librevenge::RVNG_POINT);
    pos.m_anchorTo=MWAWPosition::Page;

    ClarisDrawGraphInternal::Style cStyle = child->m_style;
    pos.m_wrapping=cStyle.getWrapping();
    pos.setOrder(child->m_ordering);
    ClarisDrawGraphInternal::Zone::Type type = child->getType();
    if (type==ClarisDrawGraphInternal::Zone::T_Shape)
      sendShape(static_cast<ClarisDrawGraphInternal::ZoneShape &>(*child), pos);
    else if (type==ClarisDrawGraphInternal::Zone::T_Pict) {
      ClarisDrawGraphInternal::ZonePict &pict=static_cast<ClarisDrawGraphInternal::ZonePict &>(*child);
      if (pict.m_entries[0].valid()) {
        MWAWInputStreamPtr input=m_parserState->m_input;
        long actPos=input->tell();
        input->seek(pict.m_entries[0].begin(), librevenge::RVNG_SEEK_SET);
        shared_ptr<MWAWPict> thePict(MWAWPictData::get(input, (int)pict.m_entries[0].length()));
        input->seek(actPos, librevenge::RVNG_SEEK_SET);
        librevenge::RVNGBinaryData data;
        std::string pictType;
        if (thePict && thePict->getBinary(data,pictType))
          listener->insertPicture(pos, data, pictType);
      }
    }
    else if (type==ClarisDrawGraphInternal::Zone::T_Zone) {
      int cId=child->getZoneId();
      switch (m_mainParser->getFileType(child->getZoneId())) {
      case 0:
        if (m_state->isEmptyGroup(cId))
          break;
        listener->openGroup(pos);
        sendGroup(cId, pos.origin());
        listener->closeGroup();
        break;
      case 1: {
        ClarisDrawGraphInternal::ZoneZone *cZone=dynamic_cast<ClarisDrawGraphInternal::ZoneZone *>(child.get());
        if (cZone && cZone->isANote()) {
          cStyle.m_lineWidth=1;

          MWAWBorder border;
          border.m_color=MWAWColor::black();
          border.m_width=1;
          cStyle.setBorders(libmwaw::LeftBit|libmwaw::BottomBit|libmwaw::RightBit, border);
          border.m_color=MWAWColor(0x60,0x60,0); // normally pattern of yellow and black
          border.m_width=20;
          cStyle.setBorders(libmwaw::TopBit, border);

          cStyle.setSurfaceColor(MWAWColor(0xff,0xff,0));
          cStyle.m_shadowOffset=MWAWVec2i(3,3);
          cStyle.setShadowColor(MWAWColor(0x80,0x80,0x80));
        }
        if (cZone->m_transformationId>=0 && cZone->m_transformationId<(int) m_state->m_transformations.size())
          cStyle.m_rotate=m_state->m_transformations[size_t(cZone->m_transformationId)].m_rotate;
        /* if we can link text frame, use:
        cZone->addFrameName(cStyle);
        shared_ptr<MWAWSubDocument> doc;
        if (!cZone->isLinked()||cZone->m_subId==0)
          doc.reset(new ClarisDrawGraphInternal::SubDocument(*this, m_parserState->m_input, cId));
        */
        shared_ptr<MWAWSubDocument> doc(new ClarisDrawGraphInternal::SubDocument
                                        (*this, m_parserState->m_input, cId, cZone->isLinked() ? cZone->m_subId : -1));
        listener->insertTextBox(pos, doc, cStyle);
        break;
      }
      case 4:
        sendBitmap(cId, pos);
        break;
      default:
        MWAW_DEBUG_MSG(("ClarisDrawGraph::sendGroup: find unexpected group type\n"));
        break;
      }
    }
  }
  return true;
}

////////////////////////////////////////////////////////////
// basic shape, bitmap
////////////////////////////////////////////////////////////
bool ClarisDrawGraph::sendShape(ClarisDrawGraphInternal::ZoneShape &pict, MWAWPosition pos)
{
  MWAWListenerPtr listener=m_mainParser->getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("ClarisDrawGraph::sendShape: can not find the listener\n"));
    return false;
  }
  if (pos.size()[0] < 0 || pos.size()[1] < 0)
    pos.setSize(pict.getBdBox().size());
  MWAWGraphicStyle pStyle(pict.m_style);
  if (pict.m_shape.m_type!=MWAWGraphicShape::Line)
    pStyle.m_arrows[0]=pStyle.m_arrows[1]=false;
  listener->insertPicture(pos, pict.m_shape, pStyle);
  if (pict.m_shape.m_type!=MWAWGraphicShape::Line || !pict.m_autosize)
    return true;

  MWAWVec2f lineSz=pos.size();
  MWAWVec2f center=pos.origin() + 0.5*lineSz;
  MWAWPosition labelPos(pos);
  labelPos.setOrigin(center-MWAWVec2f(30,6));
  labelPos.setSize(MWAWVec2f(60,12));
  labelPos.setOrder(pos.order()+1);
  std::stringstream s;
  s << std::setprecision(0) << std::fixed << std::sqrt(lineSz[0]*lineSz[0]+lineSz[1]*lineSz[1]) << " pt";
  shared_ptr<MWAWSubDocument> doc(new ClarisDrawGraphInternal::SubDocument(*this, m_parserState->m_input, s.str()));
  MWAWGraphicStyle labelStyle;
  labelStyle.m_lineWidth=0;
  labelStyle.setSurfaceColor(MWAWColor::white());
  listener->insertTextBox(labelPos, doc, labelStyle);
  return true;
}

bool ClarisDrawGraph::sendBitmap(int number, MWAWPosition const &pos)
{
  std::map<int, shared_ptr<ClarisDrawGraphInternal::Bitmap> >::iterator iter
    = m_state->m_bitmapMap.find(number);
  if (iter == m_state->m_bitmapMap.end() || !iter->second) {
    MWAW_DEBUG_MSG(("ClarisDrawGraph::sendBitmap: can not find bitmap %d\n", number));
    return false;
  }
  return sendBitmap(*iter->second, pos);
}

bool ClarisDrawGraph::sendBitmap(ClarisDrawGraphInternal::Bitmap &bitmap, MWAWPosition pos)
{
  MWAWListenerPtr listener=m_mainParser->getGraphicListener();
  if (!listener) {
    MWAW_DEBUG_MSG(("ClarisDrawGraph::sendBitmap: can not find the listener\n"));
    return false;
  }
  bitmap.m_parsed=true;
  if (!bitmap.m_entry.valid() || !bitmap.m_numBytesPerPixel)
    return false;
  int bytesPerPixel = bitmap.m_numBytesPerPixel;
  if (bytesPerPixel<0 && (bytesPerPixel!=-2 && bytesPerPixel!=-4)) {
    MWAW_DEBUG_MSG(("ClarisDrawGraph::sendBitmap: unknown group of color\n"));
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
      MWAW_DEBUG_MSG(("ClarisDrawGraph::sendBitmap: unexpected mode for compressed bitmap. Bitmap ignored.\n"));
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
          MWAW_DEBUG_MSG(("ClarisDrawGraph::sendBitmap: unknown data size\n"));
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
    MWAW_DEBUG_MSG(("ClarisDrawGraph::sendBitmap: can not find bitmap size\n"));
    pos.setSize(MWAWVec2f(70,70));
  }
  if (pos.m_anchorTo==MWAWPosition::Unknown) {
    MWAW_DEBUG_MSG(("ClarisDrawGraph::sendBitmap: anchor is not set, revert to page\n"));
    pos.m_anchorTo=MWAWPosition::Page;
  }
  listener->insertPicture(pos, data, "image/pict");
  return true;
}


////////////////////////////////////////////////////////////
// basic shape, bitmap, pict
////////////////////////////////////////////////////////////
void ClarisDrawGraph::flushExtra()
{
  shared_ptr<MWAWListener> listener=m_mainParser->getGraphicListener();
  if (!listener) return;

  MWAWVec2f leftTop=72.0f*m_mainParser->getPageLeftTop();

  // first group
  std::map<int, shared_ptr<ClarisDrawGraphInternal::Group> >::iterator gIter
    = m_state->m_groupMap.begin();
  for (; gIter !=  m_state->m_groupMap.end(); ++gIter) {
    shared_ptr<ClarisDrawGraphInternal::Group> zone = gIter->second;
    // can be some footer print on the last page or an unused header/footer
    if (!zone || zone->m_parsed || zone->isHeaderFooter())
      continue;
    static bool first=true;
    if (first) {
      MWAW_DEBUG_MSG(("ClarisDrawGraph::flushExtra: find some extra graph %d\n", zone->m_id));
      first=false;
    }
    MWAWPosition pos(leftTop,MWAWVec2f(0,0),librevenge::RVNG_POINT);
    pos.setRelativePosition(MWAWPosition::Page);
    sendGroup(gIter->first, pos);
  }
  // first second
  std::map<int, shared_ptr<ClarisDrawGraphInternal::Bitmap> >::iterator bIter
    = m_state->m_bitmapMap.begin();
  for (; bIter !=  m_state->m_bitmapMap.end(); ++bIter) {
    shared_ptr<ClarisDrawGraphInternal::Bitmap> zone = bIter->second;
    // can be some footer print on the last page or an unused header/footer
    if (!zone || zone->m_parsed || zone->isHeaderFooter())
      continue;
    static bool first=true;
    if (first) {
      MWAW_DEBUG_MSG(("ClarisDrawGraph::flushExtra: find some extra bitmap %d\n", zone->m_id));
      first=false;
    }
    MWAWPosition pos(leftTop,MWAWVec2f(0,0),librevenge::RVNG_POINT);
    pos.setRelativePosition(MWAWPosition::Page);
    sendBitmap(bIter->first, pos);
  }
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
