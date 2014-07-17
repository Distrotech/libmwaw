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

/*
 * Structures used by Claris Works parser
 *
 */
#ifndef CLARIS_WKS_STRUCT
#  define CLARIS_WKS_STRUCT

#include <iostream>
#include <set>
#include <vector>

#include "libmwaw_internal.hxx"

/** namespace to store the main structure which appears in a Claris Works file */
namespace ClarisWksStruct
{
//! main structure which correspond to a document part
struct DSET {
  struct Child;

  //! the zone position
  enum Position { P_Main=0, P_Header, P_Footer, P_Frame, P_Footnote, P_Table, P_Slide, P_Unknown};
  /** the different types of zone child */
  enum ChildType { C_Zone, C_SubText, C_Graphic, C_Unknown };

  //! constructor
  DSET() : m_size(0), m_numData(0), m_dataSz(-1), m_headerSz(-1),
    m_position(P_Unknown), m_fileType(-1),
    m_page(-1), m_box(), m_id(0), m_fathersList(), m_validedChildList(),
    m_beginSelection(0), m_endSelection(-1), m_textType(0),
    m_childs(), m_otherChilds(), m_parsed(false), m_internal(0)
  {
    for (int i = 0; i < 4; i++) m_flags[i] = 0;
  }

  //! virtual destructor
  virtual ~DSET() {}

  //! test if the zone is an header/footer
  bool isHeaderFooter() const
  {
    return m_position==P_Header||m_position==P_Footer;
  }
  //! test is a child id is valid
  bool okChildId(int zoneId) const
  {
    return m_validedChildList.find(zoneId) != m_validedChildList.end();
  }

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
  /** returns the maximum page */
  int getMaximumPage() const
  {
    if (m_position==ClarisWksStruct::DSET::P_Slide)
      return m_page;
    if (m_position!=ClarisWksStruct::DSET::P_Main)
      return 0;
    int nPages=m_page;
    for (size_t b=0; b < m_childs.size(); b++) {
      if (m_childs[b].m_page > nPages)
        nPages=m_childs[b].m_page;
    }
    return nPages;
  }

  //! try to update the child page and bounding box
  void updateChildPositions(Vec2f const &pageDim, int numHorizontalPages=1);
  //! returns the child box (ie. the union of the childs box)
  Box2i getUnionChildBox() const;

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, DSET const &doc);

  //! the size of the DSET header
  long m_size;
  //! the number of header
  long m_numData;
  //! the data size
  long m_dataSz;
  //! the header size
  long m_headerSz;

  //! the zone type
  Position m_position;
  //! the type ( 0: text, -1: graphic, ...)
  int m_fileType;

  //! the page (if known)
  int m_page;
  //! the bounding box (if known)
  Box2f m_box;

  //! the zone identificator
  int m_id;
  //! the list of fathers
  std::set<int> m_fathersList;
  //! the list of verified child
  std::set<int> m_validedChildList;

  //! the begin of selection ( at least in text header)
  int m_beginSelection;
  //! the end of selection ( at least in text header)
  int m_endSelection;

  //! the text type (header/footer,footnote, ...)
  int m_textType;

  //! some unknown flag
  int m_flags[4];

  //! the list of child zone
  std::vector<Child> m_childs;
  //! the list of other child
  std::vector<int> m_otherChilds;

  //! a flag to know if the entry is sent or not to the listener
  mutable bool m_parsed;
  //! an internal variable used to do some computation
  mutable int m_internal;

  //! structure used to define the child of a DSET structure
  struct Child {
    //! constructor
    Child() : m_type(C_Unknown), m_id(-1), m_posC(-1), m_page(-1), m_box()
    {
    }
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

    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Child const &ch)
    {
      switch (ch.m_type) {
      case C_SubText:
        o << "text,";
        break;
      case C_Zone:
        o << "zone,";
        break;
      case C_Graphic:
        o << "graphic,";
        break;
      case C_Unknown:
        o << "#type,";
      default:
        break;
      }
      if (ch.m_id != -1) o << "id=" << ch.m_id << ",";
      if (ch.m_posC != -1) o << "posC=" << ch.m_posC << ",";
      if (ch.m_page>=0) o << "pg=" << ch.m_page << ",";
      if (ch.m_box.size().x() > 0 || ch.m_box.size().y() > 0)
        o << "box=" << ch.m_box << ",";
      return o;
    }

    //! the type
    ChildType m_type;
    //! the identificator
    int m_id;
    //! a position (used in text zone to store the character )
    long m_posC;
    //! the page if known
    int m_page;
    //! the bdbox
    Box2f m_box;
  };
};
}

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
