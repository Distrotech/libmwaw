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
#include <map>
#include <set>
#include <string>
#include <vector>

#include "libmwaw_internal.hxx"

class CWParser;

/** namespace to store the main structure which appears in a Claris Works file */
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
  friend std::ostream &operator<<(std::ostream &o, DSET const &doc);

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

  //! structure used to define the child of a DSET structure
  struct Child {
    /** the different types */
    enum Type { ZONE, TEXT, GRAPHIC, TABLE, UNKNOWN };

    //! constructor
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
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
