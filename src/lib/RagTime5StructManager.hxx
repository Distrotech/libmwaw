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

#ifndef RAG_TIME_5_STRUCT_MANAGER
#  define RAG_TIME_5_STRUCT_MANAGER

#include <ostream>

#include <vector>

#include "MWAWDebug.hxx"
#include "MWAWEntry.hxx"
#include "MWAWInputStream.hxx"

//! basic class used to store RagTime 5/6 structures
class RagTime5StructManager
{
public:
  struct Field;

  //! constructor
  RagTime5StructManager();
  //! destructor
  ~RagTime5StructManager();

  //! try to read a list of type definition
  bool readTypeDefinitions(MWAWInputStreamPtr input, long endPos, libmwaw::DebugFile &ascFile);
  //! try to read a field
  bool readField(Field &field, MWAWInputStreamPtr input, long endPos);
  //! try to read a compressed long
  static bool readCompressedLong(MWAWInputStreamPtr &input, long endPos, long &val);

  //! a field of RagTime 5/6 structures
  struct Field {
    //! the different type
    enum Type { T_Unknown, T_Long, T_2Long, T_FieldList, T_LongList, T_Unicode, T_Unstructured };

    //! constructor
    Field() : m_type(T_Unknown), m_name(""), m_longList(), m_numLongByData(1), m_fieldList(), m_entry(), m_extra("")
    {
      for (int i=0; i<2; ++i) m_longValue[i]=0;
    }
    //! copy constructor
    Field(Field const &orig) : m_type(orig.m_type), m_name(orig.m_name),
      m_longList(orig.m_longList), m_numLongByData(orig.m_numLongByData), m_fieldList(orig.m_fieldList), m_entry(orig.m_entry), m_extra(orig.m_extra)
    {
      for (int i=0; i<2; ++i)
        m_longValue[i]=orig.m_longValue[i];
    }
    //! destructor
    ~Field()
    {
    }
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Field const &field);
    //! the field type
    Type m_type;
    //! the field type name
    std::string m_name;
    //! the long value
    long m_longValue[2];
    //! the list of long value
    std::vector<long> m_longList;
    //! the number of long by data (in m_longList)
    int m_numLongByData;
    //! the list of field
    std::vector<Field> m_fieldList;
    //! entry to defined the position of a String or Unstructured data
    MWAWEntry m_entry;
    //! extra data
    std::string m_extra;
  };
private:
  RagTime5StructManager(RagTime5StructManager const &orig);
  RagTime5StructManager operator=(RagTime5StructManager const &orig);
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
