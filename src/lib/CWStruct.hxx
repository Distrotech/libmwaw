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
#ifndef CW_MWAW_STRUCT
#  define CW_MWAW_STRUCT

#include <iostream>
#include <set>
#include <vector>

#include "libmwaw_internal.hxx"

namespace CWStruct
{
//! main structure which correspond to a document part
struct DSET {
  struct Child;

  //! the document type
  enum Type { T_Main=0, T_Header, T_Footer, T_Frame, T_Footnote, T_Table, T_Slide, T_Unknown};

  //! constructor
  DSET() : m_size(0), m_numData(0), m_dataSz(-1), m_headerSz(-1),
    m_type(T_Unknown), m_fileType(-1), m_id(0), m_fathersList(), m_validedChildList(),
    m_beginSelection(0), m_endSelection(-1), m_textType(0),
    m_childs(), m_otherChilds(), m_parsed(false), m_internal(0) {
    for (int i = 0; i < 4; i++) m_flags[i] = 0;
  }

  //! virtual destructor
  virtual ~DSET() {}

  //! test is a child id is valid
  bool okChildId(int zoneId) const {
    return m_validedChildList.find(zoneId) != m_validedChildList.end();
  }

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, DSET const &doc) {
    switch(doc.m_type) {
    case T_Unknown:
      break;
    case T_Frame:
      o << "frame,";
      break;
    case T_Header:
      o << "header,";
      break;
    case T_Footer:
      o << "footer,";
      break;
    case T_Footnote:
      o << "footnote,";
      break;
    case T_Main:
      o << "main,";
      break;
    case T_Slide:
      o << "slide,";
      break;
    case T_Table:
      o << "table,";
      break;
    default:
      o << "#type=" << doc.m_type << ",";
      break;
    }
    switch(doc.m_fileType) {
    case 0:
      o << "normal,";
      break;
    case 1:
      o << "text";
      if (doc.m_textType==0xFF)
        o << "*,";
      else if (doc.m_textType)
        o << "[#type=" << std::hex << doc.m_textType<< std::dec << "],";
      else
        o << ",";
      break;
    case 2:
      o << "spreadsheet,";
      break;
    case 3:
      o << "database,";
      break;
    case 4:
      o << "bitmap,";
      break;
    case 5:
      o << "presentation,";
      break;
    case 6:
      o << "table,";
      break;
    default:
      o << "#type=" << doc.m_fileType << ",";
      break;
    }
    o << "id=" << doc.m_id << ",";
    if (doc.m_fathersList.size()) {
      o << "fathers=[";
      std::set<int>::const_iterator it = doc.m_fathersList.begin();
      for ( ; it != doc.m_fathersList.end(); it++)
        o << *it << ",";
      o << "],";
    }
    if (doc.m_validedChildList.size()) {
      o << "child[valided]=[";
      std::set<int>::const_iterator it = doc.m_validedChildList.begin();
      for ( ; it != doc.m_validedChildList.end(); it++)
        o << *it << ",";
      o << "],";
    }
    o << "N=" << doc.m_numData << ",";
    if (doc.m_dataSz >=0) o << "dataSz=" << doc.m_dataSz << ",";
    if (doc.m_headerSz >= 0) o << "headerSz=" << doc.m_headerSz << ",";
    if (doc.m_beginSelection) o << "begSel=" << doc.m_beginSelection << ",";
    if (doc.m_endSelection >= 0) o << "endSel=" << doc.m_endSelection << ",";
    for (int i = 0; i < 4; i++) {
      if (doc.m_flags[i])
        o << "fl" << i << "=" << std::hex << doc.m_flags[i] << std::dec << ",";
    }
    for (size_t i = 0; i < doc.m_childs.size(); i++)
      o << "child" << i << "=[" << doc.m_childs[i] << "],";
    for (size_t i = 0; i < doc.m_otherChilds.size(); i++)
      o << "otherChild" << i << "=" << doc.m_otherChilds[i] << ",";
    return o;
  }

  //! the size of the DSET header
  long m_size;

  //! the number of header
  long m_numData;

  //! the data size
  long m_dataSz;

  //! the header size
  long m_headerSz;

  //! the document type
  Type m_type;

  //! the type ( 0: text, -1: graphic, ...)
  int m_fileType;

  //! the identificator
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

  //! contructor
  struct Child {
    enum Type { ZONE, TEXT, GRAPHIC, TABLE, UNKNOWN };

    Child() : m_type(UNKNOWN), m_id(-1), m_posC(-1), m_box() {
    }

    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Child const &ch) {
      switch(ch.m_type) {
      case TEXT:
        o << "text,";
        break;
      case ZONE:
        o << "zone,";
        break;
      case GRAPHIC:
        o << "graphic,";
        break;
      case TABLE:
        o << "table,";
        break;
      case UNKNOWN:
        o << "#type,";
      default:
        break;
      }
      if (ch.m_id != -1) o << "id=" << ch.m_id << ",";
      if (ch.m_posC != -1) o << "posC=" << ch.m_posC << ",";
      if (ch.m_box.size().x() > 0 || ch.m_box.size().y() > 0)
        o << "box=" << ch.m_box << ",";
      return o;
    }

    //! the type
    int m_type;
    //! the identificator
    int m_id;
    //! a position (used in text zone to store the character )
    long m_posC;
    //! the bdbox
    Box2i m_box;
  };
};
}

#endif
