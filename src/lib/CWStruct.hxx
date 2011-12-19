/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libmwaw
 * Copyright (C) 2009, 2011 Alonso Laurent (alonso@loria.fr)
 * Copyright (C) 2006, 2007 Andrew Ziem
 * Copyright (C) 2004-2006 Fridrich Strba (fridrich.strba@bluewin.ch)
 * Copyright (C) 2004 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2003-2005 William Lachance (william.lachance@sympatico.ca)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 * For further information visit http://libwps.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

/*
 * Structures used by Claris Works parser
 *
 */
#ifndef CW_MWAW_STRUCT
#  define CW_MWAW_STRUCT

#include <iostream>
#include <vector>

#include "libmwaw_tools.hxx"

namespace CWStruct
{
//! main structure which correspond to a document part
struct DSET {
  struct Child;

  //! constructor
  DSET() : m_size(0), m_numData(0), m_dataSz(-1), m_headerSz(-1),
    m_type(-1), m_id(0), m_fatherId(0),
    m_beginSelection(0), m_endSelection(-1),
    m_childs(),  m_otherChilds(), m_internal() {
    for (int i = 0; i < 4; i++) m_flags[i] = 0;
  }

  //! virtual destructor
  virtual ~DSET() {}

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, DSET const &doc) {
    switch(doc.m_type) {
    case 0:
      o << "normal,";
      break;
    case 1:
      o << "text,";
      break;
    case 2:
      o << "spreadsheet,";
      break;
    case 3:
      o << "database,";
      break;
    case 4:
      o << "bitmap??,";
      break;
    case 6:
      o << "table??,";
      break;
    default:
      o << "type=" << doc.m_type << ",";
      break;
    }
    o << "id=" << doc.m_id << ",";
    if (doc.m_fatherId) o << "fatherId=" << doc.m_fatherId << ",";
    o << "N=" << doc.m_numData << ",";
    if (doc.m_dataSz >=0) o << "dataSz=" << doc.m_dataSz << ",";
    if (doc.m_headerSz >= 0) o << "headerSz=" << doc.m_headerSz << ",";
    if (doc.m_beginSelection) o << "begSel=" << doc.m_beginSelection << ",";
    if (doc.m_endSelection >= 0) o << "endSel=" << doc.m_endSelection << ",";
    for (int i = 0; i < 4; i++) {
      if (doc.m_flags[i])
        o << "fl" << i << "=" << std::hex << doc.m_flags[i] << std::dec << ",";
    }
    for (int i = 0; i < int(doc.m_childs.size()); i++)
      o << "child" << i << "=[" << doc.m_childs[i] << "],";
    for (int i = 0; i < int(doc.m_otherChilds.size()); i++)
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

  //! the type ( 0: text, -1: graphic, ...)
  int m_type;

  //! the identificator
  int m_id;

  //! the father identifactor (if known )
  int m_fatherId;

  //! the begin of selection ( at least in text header)
  int m_beginSelection;

  //! the end of selection ( at least in text header)
  int m_endSelection;

  //! some unknown flag
  int m_flags[4];

  //! the list of child zone
  std::vector<Child> m_childs;

  //! the list of other child
  std::vector<int> m_otherChilds;

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
