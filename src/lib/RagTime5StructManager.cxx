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

#include <sstream>

#include "MWAWDebug.hxx"

#include "RagTime5StructManager.hxx"

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
RagTime5StructManager::RagTime5StructManager()
{
}

RagTime5StructManager::~RagTime5StructManager()
{
}

////////////////////////////////////////////////////////////
// read basic structures
////////////////////////////////////////////////////////////
bool RagTime5StructManager::readCompressedLong(MWAWInputStreamPtr &input, long endPos, long &val)
{
  val=(long) input->readULong(1);
  if ((val&0xF0)==0xC0) {
    input->seek(-1, librevenge::RVNG_SEEK_CUR);
    val=(long)(MWAWInputStream::readULong(input->input().get(), 4, 0, false)&0xFFFFFFF);
  }
  else if (val>=0xD0) { // never seems, but may be ok
    MWAW_DEBUG_MSG(("RagTime5Struct::readCompressedLong: can not read a long\n"));
    return false;
  }
  else if (val>=0x80)
    val=((val&0x7F)<<8)+(long) input->readULong(1);
  return input->tell()<=endPos;
}

bool RagTime5StructManager::readTypeDefinitions(MWAWInputStreamPtr input, long endPos, libmwaw::DebugFile &ascFile)
{
  long debPos=input->tell();
  if (endPos-debPos<26) return false;
  libmwaw::DebugStream f;
  f << "Entries(TypeDef):";
  long N;
  if (!RagTime5StructManager::readCompressedLong(input, endPos, N) || N<20 || 12+14*N>endPos-debPos) {
    MWAW_DEBUG_MSG(("RagTime5StructManager::readTypeDefinitions: can not read the list type zone\n"));
    return false;
  }
  f << "N=" << N << ",";
  int val;
  for (int i=0; i<2; ++i) { // always 0,0
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  int sz=(int) input->readULong(1);
  if (sz) {
    f << "data1=[" << std::hex;
    for (int i=0; i<sz+1; ++i) {
      val=(int) input->readULong(1);
      if (val) f << val << ",";
      else f << "_,";
    }
    f << std::dec << "],";
  }
  long debDataPos=input->tell()+4*(N+1);
  long remain=endPos-debDataPos;
  if (remain<=0) return false;
  f << "ptr=[" << std::hex;
  std::vector<long> listPtrs((size_t)(N+1), -1);
  long lastPtr=0;
  int numOk=0;
  for (size_t i=0; i<=size_t(N); ++i) {
    long ptr=(long) input->readULong(4);
    if (ptr<0 || ptr>remain || ptr<lastPtr) {
      f << "###";
      static bool first=true;
      if (first) {
        first = false;
        MWAW_DEBUG_MSG(("RagTime5StructManager::readTypeDefinitions: problem reading some type position\n"));
      }
      listPtrs[size_t(i)]=lastPtr;
    }
    else {
      ++numOk;
      lastPtr=listPtrs[size_t(i)]=ptr;
    }
    f << ptr << ",";
  }
  f << std::dec << "],";
  ascFile.addPos(debPos);
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(endPos);
  ascFile.addNote("_");
  if (!numOk) {
    MWAW_DEBUG_MSG(("RagTime5StructManager::readTypeDefinitions: problem reading some type position\n"));
    return false;
  }

  for (size_t i=0; i+1<listPtrs.size(); ++i) {
    if (listPtrs[i]<0 || listPtrs[i]==listPtrs[i+1]) continue;
    if (listPtrs[i]==remain) break;
    f.str("");
    f << "TypeDef-" << i << "[head]:";
    int dSz=int(listPtrs[i+1]-listPtrs[i]);
    long pos=debDataPos+listPtrs[i];
    if (dSz<6+12) {
      f << "###";
      MWAW_DEBUG_MSG(("RagTime5StructManager::readTypeDefinitions: problem with some type size\n"));
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    val=(int) input->readLong(4); // a small number
    if (val!=1) f << "f0=" << val << ",";
    int hSz=(int) input->readULong(2);
    int nData=(dSz-4-hSz)/12;
    if (4+hSz>dSz || dSz!=4+hSz+12*nData || nData<0 || hSz<12+8) {
      f << "###hSz=" << hSz << ",";
      MWAW_DEBUG_MSG(("RagTime5StructManager::readTypeDefinitions: the header size seems bad\n"));
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      continue;
    }
    val=(int) input->readULong(2);
    if (val) f << "fl=" << std::hex << val << std::dec << ",";
    val=(int) input->readULong(4);
    if (val) f << "id?=" << std::hex << val << std::dec << ",";
    if (input->tell()!=pos+4+hSz-12)
      ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    input->seek(pos+4+hSz-12, librevenge::RVNG_SEEK_SET);
    for (int j=0; j<=nData; ++j) {
      pos=input->tell();
      f.str("");
      f << "TypeDef-" << i << "[" << j << "]:";
      f << "id?=" << std::hex << input->readULong(4) << std::dec << ","; // a big number
      val=(int) input->readULong(1); // 0 or small val
      if (val) f << "fl0=" << std::hex << val << std::dec << ",";
      val=(int) input->readULong(2); // small val
      if (val) f << "f0=" << std::hex << val << std::dec << ",";
      for (int k=0; k<3; ++k) {
        val=(int) input->readULong(1);
        if (val) f << "fl" << k+1 << "=" << std::hex << val << std::dec << ",";
      }
      val=(int) input->readLong(2); // small val
      if (val) f << "f1=" << val << ",";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      input->seek(pos+12, librevenge::RVNG_SEEK_SET);
    }
  }
  if (!listPtrs.empty() && listPtrs.back()!=remain) {
    ascFile.addPos(debDataPos+listPtrs.back());
    ascFile.addNote("TypeDef-end");
  }

  return true;
}

bool RagTime5StructManager::readField(RagTime5StructManager::Field &field, MWAWInputStreamPtr input, long endPos)
{
  libmwaw::DebugStream f;
  long debPos=input->tell();
  if (debPos+5>endPos) {
    MWAW_DEBUG_MSG(("RagTime5StructManager::readField: the zone seems too short\n"));
    return false;
  }
  long type=(long) input->readULong(4);
  if ((type>>16)==0) {
    input->seek(debPos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  bool complex=(int) input->readULong(1)==0xc0;
  input->seek(-1, librevenge::RVNG_SEEK_CUR);
  long fSz=0;
  if (!readCompressedLong(input, endPos, fSz) || fSz<=0 || input->tell()+fSz>endPos) {
    MWAW_DEBUG_MSG(("RagTime5StructManager::readField: can not read some data\n"));
    input->seek(debPos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  long debDataPos=input->tell();
  long endDataPos=debDataPos+fSz;
  switch (type) {
  case 0x3c057: // small int 3-9
  case 0xcf817: // bigger int dataId?
    if (fSz!=2) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for int\n"));
      f << "###int,";
      break;
    }
    field.m_type=Field::T_Long;

    if (type==0x3c057) {
      field.m_name="int";
      field.m_longValue[0]=(long) input->readLong(2);
    }
    else {
      field.m_name="uint";
      field.m_longValue[0]=(long) input->readULong(2);
    }
    return true;
  case 0x14510b7:
  case 0x15e3017:
    if (fSz!=4) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for dataId\n"));
      f << "###intx2,";
      break;
    }
    field.m_type=Field::T_2Long;
    field.m_longValue[0]=(long) input->readLong(2);
    if (type==0x14510b7) {
      field.m_name="dataId";
      field.m_longValue[1]=(long) input->readLong(2);
    }
    else {
      field.m_name="dataId2";
      field.m_longValue[1]=(long) input->readULong(2);
    }
    return true;
  case 0xc8042: { // unicode
    if ((fSz%2)!=0) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for unicode\n"));
      f << "###";
      break;
    }
    field.m_type=Field::T_Unicode;
    field.m_entry.setBegin(input->tell());
    field.m_entry.setLength(fSz);
    fSz/=2;
    f << "\"";
    for (long i=0; i<fSz; ++i) {
      int c=(int) input->readULong(2);
      if (c<0x100)
        f << char(c);
      else
        f << "[" << std::hex << c << std::dec << "]";
    }
    f << "\",";
    field.m_extra=f.str();
    return true;
  }
  case 0xce017: { // unstructured
    if (fSz<5) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for unstructured\n"));
      f << "###unstr";
      break;
    }
    field.m_type=Field::T_Unstructured;
    field.m_name="unstruct";
    field.m_longValue[0]=input->readLong(4);
    field.m_entry.setBegin(input->tell());
    field.m_entry.setEnd(endDataPos);
    f << "data=" << std::hex;
    for (long i=0; i<fSz-4; ++i)
      f << (int) input->readULong(1);
    f << std::dec << ",";
    field.m_extra=f.str();
    return true;
  }
  case 0xce842:  // list of long
  case 0x170c8e5: { // maybe list of color?
    if ((fSz%4)!=0) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for list of long\n"));
      f << "###";
      break;
    }
    fSz/=4;
    field.m_type=Field::T_LongList;
    if (type==0xce842)
      field.m_name="longList";
    else
      field.m_name="longList2";
    for (long i=0; i<fSz; ++i)
      field.m_longList.push_back((long) input->readLong(4));
    return true;
  }
  case 0xcf042: { // list of small int
    if ((fSz%2)!=0) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for list of long\n"));
      f << "###";
      break;
    }
    fSz/=2;
    field.m_type=Field::T_LongList;
    field.m_name="intList";
    for (long i=0; i<fSz; ++i)
      field.m_longList.push_back((long) input->readLong(2));
    return true;
  }
  case 0x1671817: { // list of 2 int ?
    if ((fSz%4)!=0) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for list of 2xint\n"));
      f << "###";
      break;
    }
    field.m_type=Field::T_LongList;
    field.m_name="2intList";
    field.m_numLongByData=2;
    fSz/=2;
    for (long i=0; i<fSz; ++i)
      field.m_longList.push_back((long) input->readLong(2));
    return true;
  }
  case 0xd7842: { // list of ?
    if ((fSz%6)!=0) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for 0xd7842\n"));
      f << "###";
      break;
    }
    fSz/=2;
    field.m_type=Field::T_LongList;
    field.m_name="3unknList";
    field.m_numLongByData=3;
    for (long i=0; i<fSz; ++i)
      field.m_longList.push_back((long) input->readLong(2));
    return true;
  }
  default:
    break;
  }

  input->seek(debDataPos, librevenge::RVNG_SEEK_SET);
  if (!complex) {
    static bool first=true;
    if (first) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: find some unexpected data type=%lx, ...\n", (unsigned long) type));
      first=false;
    }
    f << "#type=" << std::hex << type << std::dec << ",";
    f << "data=" << std::hex;
    for (long i=0; i<fSz; ++i) {
      f << (int) input->readULong(1);
      if (i>40) {
        f << "...";
        break;
      }
    }
    f << std::dec << ",";
    field.m_extra=f.str();
    input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
    return true;
  }
  switch (type) {
  case 0x14b5815: // increasing list
    field.m_name="lIntList";
    break;
  case 0x16be055:
    field.m_name="lUInt";
    break;
  case 0x16be065:
    field.m_name="lUInt2";
    break;
  case 0x146e815: // maybe a list of dim
    field.m_name="lLongList";
    break;
  case 0x1473815: // maybe a list of dim
    field.m_name="lLongList2";
    break;
  case 0x14e6825: // maybe a list of coldim
    field.m_name="lLongList3";
    break;
  case 0x14eb015: // maybe a list of rowdim
    field.m_name="lLongList4";
    break;
  case 0x14f1825:
    field.m_name="lLongList5";
    break;
  case 0x160f815:
    field.m_name="lLongList6";
    break;
  case 0x1671845:
    field.m_name="lLongList7";
    break;
  case 0x1451025:
    field.m_name="lUnstr";
    break;
  case 0x14e6875:
    field.m_name="lUnstr2";
    break;
  case 0x15f6815:
    field.m_name="lUnstr3";
    break;
  case 0x15f9015: // sometimes a list of 15f6815
    field.m_name="lUnstr4";
    break;
  case 0x15e0825:
    field.m_name="l3UnknList";
    break;
  default:
    MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected list type=%lx\n", (unsigned long) type));
    f << "#type=" << std::hex << type << std::dec << "[list],";
    break;
  }
  field.m_type=Field::T_FieldList;
  while (input->tell()<endDataPos) {
    long pos=input->tell();
    Field child;
    if (!readField(child, input, endDataPos)) {
      f << "###pos=" << pos-debPos;
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    field.m_fieldList.push_back(child);
  }
  if (input->tell()+4>=endDataPos) {
    input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
    return true;
  }
  MWAW_DEBUG_MSG(("RagTime5StructManager::readField: can not read some data\n"));
  input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
  field.m_extra=f.str();
  return true;
}

////////////////////////////////////////////////////////////
// field function
////////////////////////////////////////////////////////////
std::ostream &operator<<(std::ostream &o, RagTime5StructManager::Field const &field)
{
  switch (field.m_type) {
  case RagTime5StructManager::Field::T_Long:
    if (!field.m_name.empty())
      o << field.m_name;
    else
      o << "long";
    o << "=" << field.m_longValue[0] << ",";
    break;
  case RagTime5StructManager::Field::T_2Long:
    if (!field.m_name.empty())
      o << field.m_name;
    else
      o << "long[2]";
    o << "=" << field.m_longValue[0] << ",";
    break;
  case RagTime5StructManager::Field::T_Unicode:
    if (!field.m_name.empty())
      o << field.m_name;
    else
      o << "str";
    o << "=" << field.m_extra << ",";
    return o;
  case RagTime5StructManager::Field::T_Unstructured:
    if (!field.m_name.empty())
      o << field.m_name;
    else
      o << "unstructured";
    o << "=" << field.m_extra << ",";
    return o;
  case RagTime5StructManager::Field::T_FieldList:
    if (!field.m_name.empty())
      o << field.m_name;
    else
      o << "child";
    if (!field.m_fieldList.empty()) {
      o << "=[";
      for (size_t i=0; i< field.m_fieldList.size(); ++i)
        o << "[" << field.m_fieldList[i] << "],";
      o << "]";
    }
    o << ",";
    break;
  case RagTime5StructManager::Field::T_LongList:
    if (!field.m_name.empty())
      o << field.m_name;
    else
      o << "data=";
    if (!field.m_longList.empty() && field.m_numLongByData>0) {
      o << "=[";
      size_t pos=0;
      while (pos+size_t(field.m_numLongByData-1)<field.m_longList.size()) {
        for (int i=0; i<field.m_numLongByData; ++i) {
          long val=field.m_longList[pos++];
          if (!val)
            o << "_";
          else if (val>-1000 && val <1000)
            o << val;
          else if (val==0x80000000)
            o << "inf";
          // find sometime 0x3e7f0001
          else
            o << "0x" << std::hex << val << std::dec;
          o << ((i+1==field.m_numLongByData) ? "," : ":");
        }
      }
      o << "]";
    }
    o << ",";
    break;
  case RagTime5StructManager::Field::T_Unknown:
  default:
    if (!field.m_name.empty())
      o << field.m_name << "[###unkn],";
    else
      o << "###unkn,";
    break;
  }
  o << field.m_extra;
  return o;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
