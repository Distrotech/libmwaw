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
#include <string>
#include <vector>

#include "libmwaw_internal.hxx"
#include "MWAWDebug.hxx"
#include "MWAWEntry.hxx"
#include "MWAWInputStream.hxx"

//! basic class used to store RagTime 5/6 structures
class RagTime5StructManager
{
public:
  struct Field;
  struct Zone;

  //! constructor
  RagTime5StructManager();
  //! destructor
  ~RagTime5StructManager();

  //! try to read a list of type definition
  bool readTypeDefinitions(Zone &zone);
  //! try to read a field
  bool readField(Zone &zone, long endPos, Field &field, long fSz=0);
  //! try to read a compressed long
  static bool readCompressedLong(MWAWInputStreamPtr &input, long endPos, long &val);

  //! a field of RagTime 5/6 structures
  struct Field {
    //! the different type
    enum Type { T_Unknown, T_Bool, T_Double, T_Long, T_2Long, T_FieldList, T_LongList, T_Unicode, T_Unstructured };

    //! constructor
    Field() : m_type(T_Unknown), m_fileType(0), m_name(""), m_doubleValue(0), m_longList(), m_numLongByData(1), m_fieldList(), m_entry(), m_extra("")
    {
      for (int i=0; i<2; ++i) m_longValue[i]=0;
    }
    //! copy constructor
    Field(Field const &orig) : m_type(orig.m_type), m_fileType(orig.m_fileType), m_name(orig.m_name), m_doubleValue(orig.m_doubleValue),
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
    //! the file type
    long m_fileType;
    //! the field type name
    std::string m_name;
    //! the long value
    long m_longValue[2];
    //! the double value
    double m_doubleValue;
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
  //! main zone in a RagTime v5-v6 document
  struct Zone {
    //! the zone type
    enum Type { Main, Data, Empty, Unknown };
    //! constructor
    Zone(MWAWInputStreamPtr input, libmwaw::DebugFile &asc):
      m_type(Unknown), m_subType(0), m_defPosition(0), m_entry(), m_name(""), m_hiLoEndian(true),
      m_entriesList(), m_extra(""), m_isParsed(false),
      m_input(input), m_defaultInput(true), m_asciiName(""), m_asciiFile(&asc), m_localAsciiFile()
    {
      for (int i=0; i<3; ++i) m_ids[i]=m_idsFlag[i]=0;
      for (int i=0; i<2; ++i) m_kinds[i]="";
      for (int i=0; i<2; ++i) m_variableD[i]=0;
      for (int i=0; i<2; ++i) m_graphicZoneId[i]=0;
    }
    //! destructor
    virtual ~Zone() {}
    //! returns the zone name
    std::string getZoneName() const;
    //! returns true if the zone is a header zone(header, list zone, ...)
    bool isHeaderZone() const
    {
      return (m_type==Data && m_ids[0]==0) ||
             (m_type==Main && (m_ids[0]==1 || m_ids[0]==4 || m_ids[0]==5));
    }
    //! returns the main type
    std::string getKindLastPart(bool main=true) const
    {
      std::string res(m_kinds[main ? 0 : 1]);
      std::string::size_type pos = res.find_last_of(':');
      if (pos == std::string::npos) return res;
      return res.substr(pos+1);
    }

    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, Zone const &z);
    //! returns the current input
    MWAWInputStreamPtr getInput()
    {
      return m_input;
    }
    //! reset the current input
    void setInput(MWAWInputStreamPtr input)
    {
      m_input = input;
      m_defaultInput = false;
    }
    //! returns true if the input correspond to the basic file
    bool isMainInput() const
    {
      return m_defaultInput;
    }
    //! returns the current ascii file
    libmwaw::DebugFile &ascii()
    {
      if (!m_defaultInput && !m_localAsciiFile)
        createAsciiFile();
      return *m_asciiFile;
    }
    //! defines the ascii name
    void setAsciiFileName(std::string const &name)
    {
      m_asciiName = name;
    }
    //! creates the ascii file
    void createAsciiFile();

    //! the zone type
    Type m_type;
    //! the zone sub type
    int m_subType;
    //! the position of the definition in the main zones
    long m_defPosition;
    //! the zone types: normal and packing
    std::string m_kinds[2];
    //! the zone entry
    MWAWEntry m_entry;
    //! the zone name ( mainly used for debugging)
    std::string m_name;
    //! true if the endian is hilo
    bool m_hiLoEndian;
    //! the zone id
    int m_ids[3];
    //! the zone flag
    int m_idsFlag[3];
    //! the list of original entries
    std::vector<MWAWEntry> m_entriesList;
    //! extra data
    std::string m_extra;
    //! the content of the zone D if it exists
    int m_variableD[2];
    //! the graphic zone id
    int m_graphicZoneId[2];
    //! a flag to know if the zone is parsed
    bool m_isParsed;
  protected:
    //! the main input
    MWAWInputStreamPtr m_input;
    //! a flag used to know if the input is or not the default input
    bool m_defaultInput;
    //! the ascii file name ( used if we need to create a ascii file)
    std::string m_asciiName;
    //! the ascii file corresponding to an input
    libmwaw::DebugFile *m_asciiFile;
    //! the local ascii file ( if we need to create a new input)
    shared_ptr<libmwaw::DebugFile> m_localAsciiFile;
  private:
    Zone(Zone const &orig);
    Zone &operator=(Zone const &orig);
  };
private:
  RagTime5StructManager(RagTime5StructManager const &orig);
  RagTime5StructManager operator=(RagTime5StructManager const &orig);
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
