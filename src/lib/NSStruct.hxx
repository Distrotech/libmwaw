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

#ifndef NS_STRUCT
#  define NS_STRUCT

#include <iostream>
#include <vector>

#include "libmwaw_internal.hxx"

#include "MWAWEntry.hxx"

class NSParser;

/** a namespace used to regroup the different structure used to parse a Nisus File */
namespace NSStruct
{
/** the different zone */
enum ZoneType { Z_Main=0, Z_Footnote, Z_HeaderFooter };

/** the different variable type */
enum VariableType { V_None=0, V_Numbering, V_Variable, V_Version };

/** a position */
struct Position {
  //! the constructor
  Position() : m_paragraph(0), m_word(0), m_char(0) {
  }
  //! operator<<: prints data in form "XxYxZ"
  friend std::ostream &operator<< (std::ostream &o, Position const &pos);

  //! operator==
  bool operator==(Position const &p2) const {
    return cmp(p2)==0;
  }
  //! operator!=
  bool operator!=(Position const &p2) const {
    return cmp(p2)!=0;
  }
  //! a small compare operator
  int cmp(Position const &p2) const {
    if (m_paragraph < p2.m_paragraph) return -1;
    if (m_paragraph > p2.m_paragraph) return 1;
    if (m_word < p2.m_word) return -1;
    if (m_word > p2.m_word) return 1;
    if (m_char < p2.m_char) return -1;
    if (m_char > p2.m_char) return 1;
    return 0;
  }
  /** the paragraph */
  int m_paragraph;
  /** the word */
  int m_word;
  /** the character position */
  int m_char;

  //! a comparaison structure used to sort the position
  struct Compare {
    //! comparaison function
    bool operator()(Position const &p1, Position const &p2) const {
      return p1.cmp(p2) < 0;
    }
  };
};

////////////////////////////////////////
// Internal: low level

/** Internal: low level a structure helping to store the footnote information */
struct FootnoteInfo {
  //! constructor
  FootnoteInfo() : m_flags(0), m_distToDocument(5), m_distSeparator(36),
    m_separatorLength(108), m_unknown(0) {
  }
  //! operator<<: prints data
  friend std::ostream &operator<< (std::ostream &o, FootnoteInfo const &fnote);

  //! returns true if we have endnote
  bool endNotes() const {
    return (m_flags&0x8);
  }
  //! returns true if we have to reset index at the beginning of a page
  bool resetNumberOnNewPage() const {
    return (m_flags&0x8)==0 && (m_flags&0x10);
  }
  //! the footnote flags
  int m_flags;
  //! the distance between the footnote and the document
  int m_distToDocument;
  //! the distance between two footnotes ( or between a footnote and the line sep)
  int m_distSeparator;
  //! the separator length
  int m_separatorLength;
  //! a unknown value
  int m_unknown;
};

/** Internal: low level a structure helping to read recursifList */
struct RecursifData {
  struct Node;
  struct Info;
  //! constructor
  RecursifData(NSStruct::ZoneType zone, NSStruct::VariableType vType=NSStruct::V_None, int level=0) :
    m_info(new Info(zone, vType)), m_level(level), m_childList() {
  }
  //! copy constructor
  RecursifData(RecursifData const &orig) :
    m_info(orig.m_info), m_level(-1), m_childList() {
  }
  //! copy operator
  RecursifData &operator=(RecursifData const &orig) {
    if (this != &orig) {
      m_info = orig.m_info;
      m_level = orig.m_level;
      m_childList = orig.m_childList;
    }
    return *this;
  }
  //! read the data
  bool read(NSParser &parser, MWAWEntry const &entry);

  //! zone information
  shared_ptr<Info> m_info;
  //! the node level
  int m_level;
  //! the list of data entry
  std::vector<Node> m_childList;

  //! the zone information
  struct Info {
    //! the constructor
    Info(NSStruct::ZoneType zType, NSStruct::VariableType vType=NSStruct::V_None) :
      m_zoneType(zType), m_variableType(vType) {
    }
    //! the zone id
    NSStruct::ZoneType m_zoneType;
    //! the variable type
    NSStruct::VariableType m_variableType;
  };
  //! the data data
  struct Node {
    //! constructor
    Node() : m_type(0), m_entry(), m_data() {
    }
    //! returns true if the node is a final node
    bool isLeaf() const {
      return !m_data;
    }

    //! the variable type
    int m_type;
    //! the entry
    MWAWEntry m_entry;
    //! the recursif data
    shared_ptr<RecursifData> m_data;
  };
};
}

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
