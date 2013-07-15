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
 * basic structure used to code zone when converting FullWrite document
 *
 */
#ifndef FP_MWAW_STRUCT
#  define FP_MWAW_STRUCT

#include <libwpd/libwpd.h>

#include "libmwaw_internal.hxx"

#include "MWAWDebug.hxx"
#include "MWAWEntry.hxx"

/** a namespace use to define common structure in a FullWrite file */
namespace FWStruct
{

// basic
struct Entry;
//! try to convert a file data to a color
bool getColor(int color, MWAWColor &col);
/** returns the type name */
std::string getTypeName(int type);

/** Internal: class to store a border which appear in docInfo */
struct Border {
  //! constructor
  Border() :m_frontColor(MWAWColor::black()), m_backColor(MWAWColor::white()),
    m_width(0), m_isDouble(0), m_flags(0), m_extra("") {
    for (int w=0; w < 3; w++) m_type[w]=0;
  }

  //! return a border corresponding to a type
  static MWAWBorder getBorder(int type);
  //! try to read a border definiton
  bool read(shared_ptr<FWStruct::Entry> zone, int fSz);
  //! returns the list of border order MWAWBorder::Pos
  std::vector<Variable<MWAWBorder> > getParagraphBorders() const;
  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Border const &p);
  //! the type (border, horizontal and vertical separators)
  int m_type[3];
  //! the front color (used for layout )
  MWAWColor m_frontColor;
  //! the back color (used for layout )
  MWAWColor m_backColor;
  //! the colors line + ?
  MWAWColor m_color[2];
  //! the width
  int m_width;
  //! the f1 value: isDouble ?
  int m_isDouble;
  //! the flags
  int m_flags;
  //! some extra data
  std::string m_extra;
};

/** the definition of a zone in a FullWrite file */
struct Entry : public MWAWEntry {
  Entry(MWAWInputStreamPtr input);
  ~Entry();

  //! operator<<
  friend std::ostream &operator<<(std::ostream &o, Entry const &entry);

  //! create a inputstream, ... if needed
  void update();
  //! write the debug file, ...
  void closeDebugFile();

  //! returns a reference to the ascii file
  libmwaw::DebugFile &getAsciiFile();
  //! basic operator==
  bool operator==(const Entry &a) const;
  //! basic operator!=
  bool operator!=(const Entry &a) const {
    return !operator==(a);
  }

  //! the input
  MWAWInputStreamPtr m_input;
  //! the next entry id
  int m_nextId;
  //! the zone type id find in DStruct
  int m_type;
  //! the type id (find in FZoneFlags)
  int m_typeId;
  //! some unknown values
  int m_values[3];
  //! the main data ( if the entry comes from several zone )
  WPXBinaryData m_data;
  //! the debug file
  shared_ptr<libmwaw::DebugFile> m_asciiFile;
private:
  Entry(Entry const &);
  Entry &operator=(Entry const &);
};
typedef shared_ptr<Entry> EntryPtr;

//! a structure used to store the data of a zone header in a FullWrite file
struct ZoneHeader {
  //! constructor
  ZoneHeader() : m_type(-1), m_docId(-1), m_fileId(-1), m_wrapping(-1), m_extra("") {}
  //! destructor
  virtual ~ZoneHeader() { }
  //! try to read the data header of a classical zone
  bool read(shared_ptr<FWStruct::Entry> zone);
  //! the operator<<
  friend std::ostream &operator<<(std::ostream &o, ZoneHeader const &dt);
  //! the zone type
  int m_type;
  //! the doc id
  int m_docId;
  //! the file id
  int m_fileId;
  //! the wrapping type
  int m_wrapping;
  //! some extra data
  std::string m_extra;
};
}

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
