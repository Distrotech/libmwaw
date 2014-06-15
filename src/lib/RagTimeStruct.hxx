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
 * main structure of a RagTime document
 *
 */
#ifndef RAGTIME_STRUCT
#  define RAGTIME_STRUCT

#include <string>

#include "libmwaw_internal.hxx"

#include "MWAWDebug.hxx"
#include "MWAWEntry.hxx"
#include "MWAWInputStream.hxx"

/** \brief some structure used to parse a RagTime document
 *
 *
 *
 */
namespace RagTimeStruct
{
/** a structure used to store list in a resource fork */
struct ResourceList {
  /** the different resource type which can be stored has list */
  enum Type { BuSl=0, BuGr, SpBo, SpCe, SpDE, SpTe, SpVa, gray, colr, res_, Undef};
  /** constructor */
  ResourceList() : m_type(Undef), m_headerPos(0), m_headerSize(0), m_dataPos(0), m_dataNumber(0), m_dataSize(0), m_endPos(0), m_extra("")
  {
  }
  /** try to read the header block */
  bool read(MWAWInputStreamPtr input, MWAWEntry &entry);
  /** returns a string corresponding to a type */
  static std::string getName(Type type)
  {
    static char const *(wh[])= { "BuSl", "BuGr", "SpBo", "SpCe", "SpDE", "SpTe", "SpVa", "gray", "colr", "res_", "#Undef"};
    return wh[int(type)];
  }
  /** operator<< */
  friend std::ostream &operator<<(std::ostream &o, ResourceList &zone);
  /** the resource type */
  Type m_type;
  /** the begin position of the header */
  long m_headerPos;
  /** the header size */
  int m_headerSize;
  /** the begin position of the first data */
  long m_dataPos;
  /** the number of data */
  int m_dataNumber;
  /** the data size */
  int m_dataSize;
  /** the end pos */
  long m_endPos;
  /** extra data */
  std::string m_extra;
};
}
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
