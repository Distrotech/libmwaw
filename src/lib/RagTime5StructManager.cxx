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

#include <iomanip>
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

bool RagTime5StructManager::readTypeDefinitions(RagTime5StructManager::Zone &zone)
{
  if (zone.m_entry.length()<26) return false;
  MWAWInputStreamPtr input=zone.getInput();
  long endPos=zone.m_entry.end();
  input->setReadInverted(!zone.m_hiLoEndian);
  input->seek(zone.m_entry.begin(), librevenge::RVNG_SEEK_SET);

  libmwaw::DebugStream f;
  f << "Entries(TypeDef):[" << zone << "]";
  long N;
  if (!RagTime5StructManager::readCompressedLong(input, endPos, N) || N<20 || 12+14*N>zone.m_entry.length()) {
    MWAW_DEBUG_MSG(("RagTime5StructManager::readTypeDefinitions: can not read the list type zone\n"));
    input->setReadInverted(false);
    return false;
  }
  zone.m_isParsed=true;
  libmwaw::DebugFile &ascFile=zone.ascii();
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
  if (remain<=0) {
    input->setReadInverted(false);
    return false;
  }
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
  ascFile.addPos(zone.m_entry.begin());
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(endPos);
  ascFile.addNote("_");
  if (!numOk) {
    MWAW_DEBUG_MSG(("RagTime5StructManager::readTypeDefinitions: problem reading some type position\n"));
    input->setReadInverted(false);
    return false;
  }

  for (size_t i=0; i+1<listPtrs.size(); ++i) {
    if (listPtrs[i]<0 || listPtrs[i]==listPtrs[i+1]) continue;
    if (listPtrs[i]==remain) break;
    f.str("");
    f << "TypeDef-" << i << "[head]:";
    int dSz=int(listPtrs[i+1]-listPtrs[i]);
    long pos=debDataPos+listPtrs[i];
    if (dSz<4+20) {
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
    if (4+hSz>dSz || dSz!=4+hSz+12*nData || nData<0 || hSz<20) {
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
    long type=(long) input->readULong(4);
    if (type) f << "type=" << std::hex << type << std::dec << ",";
    for (int j=0; j<2; ++j) { // f1=0..12,
      val=(int) input->readLong(2);
      if (val) f << "f" << j+1 << "=" << val << ",";
    }
    type=(long) input->readULong(4);
    if (type) f << "type2=" << std::hex << type << std::dec << ",";
    if (hSz>20) {
      int sSz=(int) input->readULong(1);
      if ((sSz%2)!=0 || 20+1+sSz>hSz) {
        f << "###sSz=" << sSz << ",";
        MWAW_DEBUG_MSG(("RagTime5StructManager::readTypeDefinitions: the string size seems bad\n"));
      }
      else {
        std::string name("");
        for (int c=0; c<sSz/2; ++c)
          name+=(char) input->readULong(2);
        f << "name=" << name << ",";
      }
    }
    if (input->tell()!=pos+4+hSz)
      ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    input->seek(pos+4+hSz, librevenge::RVNG_SEEK_SET);
    for (int j=0; j<nData; ++j) {
      pos=input->tell();
      f.str("");
      f << "TypeDef-" << i << "[" << j << "]:";
      f << "type=" << std::hex << input->readULong(4) << std::dec << ","; // a big number
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
  input->setReadInverted(false);
  return true;
}

bool RagTime5StructManager::readField(RagTime5StructManager::Zone &zone, long endPos, RagTime5StructManager::Field &field, long fSz)
{
  MWAWInputStreamPtr input=zone.getInput();
  libmwaw::DebugStream f;
  long debPos=input->tell();
  if ((fSz && (fSz<4 || debPos+fSz<endPos)) || (!fSz && debPos+5>endPos)) {
    MWAW_DEBUG_MSG(("RagTime5StructManager::readField: the zone seems too short\n"));
    return false;
  }
  long type=(long) input->readULong(4);
  if ((type>>16)==0) {
    input->seek(debPos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  field.m_fileType=type;
  bool complex=(int) input->readULong(1)==0xc0;
  input->seek(-1, librevenge::RVNG_SEEK_CUR);
  if (fSz==0) {
    if (!readCompressedLong(input, endPos, fSz) || fSz<=0 || input->tell()+fSz>endPos) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: can not read the data size\n"));
      input->seek(debPos, librevenge::RVNG_SEEK_SET);
      return false;
    }
  }
  else
    fSz-=4;
  long debDataPos=input->tell();
  long endDataPos=debDataPos+fSz;
  switch (type) {
  case 0x360c0:
    if (fSz!=1) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for bool\n"));
      f << "###bool,";
      break;
    }
    field.m_type=Field::T_Bool;

    field.m_name="bool";
    field.m_longValue[0]=(long) input->readLong(1);
    return true;

  case 0x328c0:
    if (fSz!=1) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for bInt\n"));
      f << "###bInt,";
      break;
    }
    field.m_type=Field::T_Long;
    field.m_name="bInt";
    field.m_longValue[0]=(long) input->readLong(1);
    return true;

  case 0x3c057: // small int 3-9: not in typedef
  case 0x3b880:
  case 0x1479080:
  case 0x147b880:
  case 0x147c080:
  case 0x149d880:
  case 0x17d5880:
    if (fSz!=2) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for int\n"));
      f << "###int,";
      break;
    }
    field.m_type=Field::T_Long;
    field.m_name="int";
    field.m_longValue[0]=(long) input->readLong(2);
    return true;
  case 0x34080:
  case 0xcf817: // bigger int dataId?: not in typedef
    if (fSz!=2) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for uint\n"));
      f << "###uint,";
      break;
    }
    field.m_type=Field::T_Long;
    field.m_name="uint";
    field.m_longValue[0]=(long) input->readULong(2);
    return true;
  case 0xb6000: // color percent
  case 0x1493800: // checkme double(as int)
  case 0x1494800: // checkme double(as int)
  case 0x1495800: // checkme double(as int)
    if (fSz!=4) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for double4\n"));
      f << "###float,";
      break;
    }
    // checkme if val=0xFFFFFFFF, inf?
    field.m_type=Field::T_Double;
    field.m_name="double4";
    field.m_doubleValue=double(input->readLong(4))/65536;
    return true;
  case 0x45840: {
    if (fSz!=8) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for double\n"));
      f << "###double,";
      break;
    }
    double res;
    bool isNan;
    if (!input->readDouble8(res, isNan)) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: can not read a double\n"));
      f << "###double";
      break;
    }
    field.m_type=Field::T_Double;
    field.m_name="double";
    field.m_doubleValue=res;
    return true;
  }
  case 0x34800:
  case 0x14510b7: // 2 long, not in typedef
  case 0x147415a: // checkme, find always with 0x0
  case 0x15e3017: // 2 long, not in typedef
  case 0x1495000:
    if (fSz!=4) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for 2xint\n"));
      f << "###2xint,";
      break;
    }
    field.m_type=Field::T_2Long;
    field.m_longValue[0]=(long) input->readLong(2);
    field.m_name="2xint";
    field.m_longValue[1]=(long) input->readLong(2);
    return true;
  case 0xc8042: { // unicode: header, fl=2000|0, f2=8
    if ((fSz%2)!=0) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for unicode\n"));
      f << "###unicode";
      break;
    }
    field.m_type=Field::T_Unicode;
    field.m_name="unicode";
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
  case 0x149a940: {
    if (fSz!=6) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for intxfloat\n"));
      f << "###intxfloat";
      break;
    }
    field.m_type=Field::T_Unstructured;
    field.m_name="intxfloat";
    int val=(int) input->readLong(2);
    if (val) f << ":" << val;
    else f << ":_";
    f << double(input->readLong(4))/65536.;
    field.m_extra=f.str();
    return true;
  }
  case 0x149c940: { // checkme
    if (fSz!=6) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for floatxint\n"));
      f << "###floatxint";
      break;
    }
    field.m_type=Field::T_Unstructured;
    field.m_name="floatxint";
    f << double(input->readLong(4))/65536.;
    int val=(int) input->readLong(2);
    if (val) f << ":" << val;
    else f << ":_";
    field.m_extra=f.str();
    return true;
  }
  case 0x74040: // maybe one element: ie. 2 float
  case 0x1474040: // maybe one element: ie. 2 float
  case 0x81474040: {
    if ((fSz%8)!=0) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for floatx2\n"));
      f << "###2xfloat[list]";
      break;
    }
    field.m_type=Field::T_DoubleList;
    field.m_name="2xfloat";
    int N=int(fSz/4);
    for (int i=0; i<N; ++i)
      field.m_doubleList.push_back(double(input->readLong(4))/65536.);
    field.m_extra=f.str();
    return true;
  }

  case 0x1476840:
    if (fSz!=10) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for 3intxfloat\n"));
      f << "###3intxfloat";
      break;
    }
    field.m_type=Field::T_Unstructured;
    field.m_name="3intxfloat";
    field.m_entry.setBegin(input->tell());
    field.m_entry.setEnd(endDataPos);
    for (long i=0; i<3; ++i) { // 1|3,1,1
      int val=(int) input->readLong(2);
      if (val) f << val << ":";
      else f << "_:";
    }
    f << double(input->readLong(4))/65536. << ",";
    field.m_extra=f.str();
    return true;
  case 0x84040: {
    if (fSz!=10) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for rgba\n"));
      f << "###rgba";
      break;
    }
    field.m_type=Field::T_Color;
    field.m_name="rgba";
    field.m_longValue[0]=(long) input->readLong(2); // id or numUsed
    unsigned char col[4];
    for (long i=0; i<4; ++i) // rgba
      col[i]=(unsigned char)(input->readULong(2)>>8);
    field.m_color=MWAWColor(col[0],col[1],col[2],col[3]);
    return true;
  }
  case 0x8d000:
    if (fSz!=4) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for rsrcName\n"));
      f << "###rsrcName";
      break;
    }
    field.m_type=Field::T_Code;
    field.m_name="rsrcName";
    for (long i=0; i<4; ++i)
      field.m_code += (char) input->readULong(1);
    return true;
  case 0x148c01a: // 2 int + 8 bytes for pat ?
    if (fSz!=12) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for pat\n"));
      f << "###pat";
      break;
    }
    field.m_type=Field::T_Unstructured;
    field.m_name="pat";
    field.m_entry.setBegin(input->tell());
    field.m_entry.setEnd(endDataPos);
    field.m_extra="...";
    input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
    return true;
  case 0x226140:
    if (fSz!=21) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for font\n"));
      f << "###font";
      break;
    }
    field.m_type=Field::T_Unstructured;
    field.m_name="font";
    field.m_entry.setBegin(input->tell());
    field.m_entry.setEnd(endDataPos);
    field.m_extra="...";
    input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
    return true;
  case 0x226940: { // checkme
    if (fSz<262 || ((fSz-262)%6)!=0) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for para\n"));
      f << "###para";
      break;
    }
    field.m_type=Field::T_Unstructured;
    field.m_name="para";
    field.m_entry.setBegin(input->tell());
    field.m_entry.setEnd(endDataPos);
    field.m_extra="...";
    zone.ascii().addPos(input->tell()+70);
    zone.ascii().addNote("StructZone-para-B0:");
    zone.ascii().addPos(input->tell()+166);
    zone.ascii().addNote("StructZone-para-B1:");
    if (fSz>262) {
      zone.ascii().addPos(input->tell()+262);
      zone.ascii().addNote("StructZone-para-C:");
    }
    input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
    return true;
  }
  case 0x227140: { // border checkme
    if ((fSz%6)!=2) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for list of border\n"));
      f << "###border";
      break;
    }
    int val=(int) input->readULong(2); // c000 or c1000
    if (val!=0xc000) f << "f0=" << std::hex << val << std::dec << ",";
    field.m_type=Field::T_LongList;
    field.m_name="border";
    field.m_numLongByData=3;
    fSz/=6;
    for (long i=0; i<fSz; ++i) {
      field.m_longList.push_back((long) input->readLong(2)); // row?
      field.m_longList.push_back((long) input->readLong(2)); // col?
      field.m_longList.push_back((long) input->readULong(2)); // flags?
    }
    field.m_extra=f.str();
    return true;
  }

  case 0xce017: { // unstructured: not in typedef
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
      f << std::setfill('0') << std::setw(2) << (int) input->readULong(1);
    f << std::dec << ",";
    field.m_extra=f.str();
    return true;
  }
  case 0xce842:  // list of long : header fl=2000, f2=7
  case 0x170c8e5: { // maybe list of color: f0=418,fl1=40,fl2=8
    if ((fSz%4)!=0) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data fSz for list of long\n"));
      f << "###";
      break;
    }
    fSz/=4;
    field.m_type=Field::T_LongList;
    field.m_name="longList";
    for (long i=0; i<fSz; ++i)
      field.m_longList.push_back((long) input->readLong(4));
    return true;
  }
  case 0x80045080: // child of 14741fa
  case 0xcf042: { // list of small int: header fl=2000 with f2=9
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
  case 0x33000: // maybe one 2xint
  case 0xa4000: // maybe one 2xint
  case 0xa4840: // maybe one 4xint
  case 0x1671817:
  case 0x80033000: {
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
  case 0xa7017: // unicode
  case 0xa7027:
  case 0xa7037:
  case 0xa7047:
  case 0xa7057:
  case 0xa7067:
  case 0x146905a: // unicode

  case 0x7a047: // font definition
  case 0x7a057: // ?? definition
  case 0x7a067: // ?? definition

  case 0x146005a: // code
  case 0x146007a:
  case 0x14600aa:
  case 0x147403a:
  case 0x14740ba:
  case 0x147501a:
  case 0x148981a:

  case 0x145e0ba: // bool
  case 0x147406a:
  case 0x147550a:
  case 0x17d486a:

  case 0x147512a: // small int

  case 0xa7077: // with type=3b880
  case 0x145e01a:
  case 0x146904a: // int with type=0x149d880
  case 0x146907a: // withe type = 149d880
  case 0x146908a: // with type=0x3b880
  case 0x1469840: // with type=147b88
  case 0x146e02a:
  case 0x146e03a:
  case 0x146e04a:
  case 0x146e06a:
  case 0x146e08a:
  case 0x145e11a: // with type=0x17d5880
  case 0x145e12a: // with type=017d5880
  case 0x147407a:
  case 0x147408a:
  case 0x1474042:
  case 0x147416a:
  case 0x14741ea:
  case 0x147420a: // with type=3b880
  case 0x147e81a: // with type=3b880
  case 0x17d481a: // int

  case 0x7d04a:
  case 0x147405a:
  case 0x14741ca: // 2 long
  case 0x145e02a: // with type=b600000
  case 0x145e0ea:
  case 0x146008a:
  case 0x14752da: // with type=1495000
  case 0x14740ea: // with type=b600000
  case 0x147536a: // with type=1495000
  case 0x147538a: // with type=1495000
  case 0x146902a: // double
  case 0x146903a: // double
  case 0x17d484a: // with type=34800
  case 0x14753aa: // min word spacing
  case 0x14753ca: // optimal word spacing
  case 0x14753ea: // max word spacing

  case 0x147404a: // with type=149c94
  case 0x7d02a: // rgba color?
  case 0x145e05a:

  case 0x7a09a: // with a4000 or a4840
  case 0x7a05a: // unknown
  case 0x7a0aa:
  case 0x14600ca: // with type=80033000
  case 0x146e05a: // with type=33000
  case 0x147402a:
  case 0x14741fa: // unsure find with type=80045080
  case 0x147502a: // with type=149a940
  case 0x147505a: // with type=1493800
  case 0x147506a: // with type=1493800
  case 0x147507a: // with type=1493800
  case 0x14750aa: // with type=149a940
  case 0x14750ba: // with type=149a940
  case 0x14750ca: // with type=1474040
  case 0x147510a: // with type=81474040 or 1474040
  case 0x147513a: // with type=1493800
  case 0x14754ba: // with type=1476840
  case 0x148983a: // with type=0074040
  case 0x148985a: { // with type=1495800
    if (fSz<4) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected data type list field\n"));
      f << "###list,";
      break;
    }

    long pos=input->tell();
    Field child;
    if (!readField(zone, endDataPos, child, fSz)) {
      f << "###pos=" << pos-debPos;
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    field.m_name="container";

    field.m_type=Field::T_FieldList;
    field.m_fieldList.push_back(child);
    if (input->tell() != endDataPos) {
      MWAW_DEBUG_MSG(("RagTime5StructManager::readField: can not read some data\n"));
      f.str("");
      f << "###pos=" << pos-debPos;
      field.m_extra+=f.str();
      input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
    }
    return true;
  }

  case 0xd7842: { // list of ? : header fl=0|4000, f2=3
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
    field.m_name="#unknType";
    zone.ascii().addDelimiter(input->tell(),'|');
    input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
    return true;
  }
  switch (type) {
  case 0x14b5815: // increasing list: with fl1=3, fl2=80, f1=29
  case 0x16be055: // fl1=f, fl2=80, f1=30
  case 0x16be065: // fl1=f, fl2=80, f1=30
  case 0x146e815: // maybe a list of dim: fl1=28-2a, fl2=80, f1=34
  case 0x1473815: // maybe a list of dim, fl2=80,f1=32
  case 0x14e6825: // maybe a list of coldim
  case 0x14eb015: // maybe a list of rowdim
  case 0x14f1825:
  case 0x160f815:
  case 0x1671845:
    field.m_name="longList";
    break;
  case 0x1451025:
  case 0x14e6875:
  case 0x15f6815:
  case 0x15f9015: // sometimes a list of 15f6815
    field.m_name="unstructList";
    break;
  case 0x15e0825:
    field.m_name="3unknList";
    break;
  default:
    // find also 16c1825 with 00042040000000000001001400000006404003000008 : a list of 42040 ?
    MWAW_DEBUG_MSG(("RagTime5StructManager::readField: unexpected list type=%lx\n", (unsigned long) type));
    field.m_name="#unknList";
    break;
  }
  field.m_type=Field::T_FieldList;
  while (input->tell()<endDataPos) {
    long pos=input->tell();
    Field child;
    if (!readField(zone, endDataPos, child)) {
      f << "###pos=" << pos-debPos;
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    field.m_fieldList.push_back(child);
  }
  if (input->tell()+4>=endDataPos) {
    zone.ascii().addDelimiter(input->tell(),'|');
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
  if (!field.m_name.empty())
    o << field.m_name << ":" << std::hex << field.m_fileType << std::dec;
  else
    o << "T" << std::hex << field.m_fileType << std::dec;
  switch (field.m_type) {
  case RagTime5StructManager::Field::T_Double:
    o << "=" << field.m_doubleValue << ",";
    break;
  case RagTime5StructManager::Field::T_Bool:
    if (field.m_longValue[0]==1)
      o << ",";
    else if (field.m_longValue[0]==0)
      o << "=no,";
    else
      o << field.m_longValue[0] << ",";
    break;
  case RagTime5StructManager::Field::T_Long:
    if (field.m_longValue[0]>1000)
      o << "=0x" << std::hex << field.m_longValue[0] << std::dec << ",";
    else
      o << "=" << field.m_longValue[0] << ",";
    break;
  case RagTime5StructManager::Field::T_2Long:
    o << "=" << field.m_longValue[0] << ":" <<  field.m_longValue[1] << ",";
    break;
  case RagTime5StructManager::Field::T_Color:
    o << "=" << field.m_color;
    if (field.m_longValue[0])
      o << "[" << field.m_longValue[0] << "]";
    o << ",";
    return o;
  case RagTime5StructManager::Field::T_Code:
    o << "=" << field.m_code << ",";
    return o;
  case RagTime5StructManager::Field::T_Unicode:
    o << "=" << field.m_extra << ",";
    return o;
  case RagTime5StructManager::Field::T_Unstructured:
    o << "=" << field.m_extra << ",";
    return o;
  case RagTime5StructManager::Field::T_FieldList:
    if (!field.m_fieldList.empty()) {
      o << "=[";
      for (size_t i=0; i< field.m_fieldList.size(); ++i)
        o << "[" << field.m_fieldList[i] << "],";
      o << "]";
    }
    o << ",";
    break;
  case RagTime5StructManager::Field::T_DoubleList:
    if (!field.m_doubleList.empty()) {
      o << "=[";
      for (size_t i=0; i<field.m_doubleList.size(); ++i)
        o << field.m_doubleList[i] << ",";
      o << "],";
    }
    break;
  case RagTime5StructManager::Field::T_LongList:
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
          else if (val==(long) 0x80000000)
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
    o << "[###unkn],";
    break;
  }
  o << field.m_extra;
  return o;
}

////////////////////////////////////////////////////////////
// style functions
////////////////////////////////////////////////////////////
bool RagTime5StructManager::GraphicStyle::read(MWAWInputStreamPtr &input, RagTime5StructManager::Field const &field)
{
  std::stringstream s;
  if (field.m_type==RagTime5StructManager::Field::T_FieldList) {
    switch (field.m_fileType) {
    case 0x7d02a:
    case 0x145e05a: {
      int wh=field.m_fileType==0x7d02a ? 0 : 1;
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Color && child.m_fileType==0x84040) {
          m_colors[wh]=child.m_color;
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StructManager::GraphicStyle::read: find some unknown color %d block\n", wh));
        s << "##col[" << wh << "]=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    }
    case 0x145e02a:
    case 0x145e0ea: {
      int wh=field.m_fileType==0x145e02a ? 0 : 1;
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Double && child.m_fileType==0xb6000) {
          m_colorsAlpha[wh]=float(child.m_doubleValue);
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StructManager::GraphicStyle::read: find some unknown colorAlpha[%d] block\n", wh));
        s << "###colorAlpha[" << wh << "]=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    }
    case 0x145e01a: {
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Long && child.m_fileType==0x147c080) {
          m_parentId=(int) child.m_longValue[0];
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StructManager::GraphicStyle::read: find some unknown parent block\n"));
        s << "###parent=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    }
    case 0x7d04a:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Double && child.m_fileType==0x1494800) {
          m_width=float(child.m_doubleValue);
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StructManager::GraphicStyle::read: find some unknown width block\n"));
        s << "###w=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x145e0ba: {
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Bool && child.m_fileType==0x360c0) {
          m_hidden=child.m_longValue[0]!=0;
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StructManager::GraphicStyle::read: find some unknown no print block\n"));
        s << "###hidden=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    }

    case 0x14600ca:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_LongList && child.m_fileType==(long)0x80033000) {
          m_dash=child.m_longList;
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StructManager::GraphicStyle::read: find some unknown dash block\n"));
        s << "###dash=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x146005a:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Code && child.m_fileType==0x8d000) {
          if (child.m_code=="LiOu")
            m_position=3;
          else if (child.m_code=="LiIn")
            m_position=1;
          else if (child.m_code=="LiRo")
            m_position=4;
          else {
            MWAW_DEBUG_MSG(("RagTime5StructManager::GraphicStyle::read: find some unknown position string %s\n", child.m_code.c_str()));
            s << "##pos=" << child.m_code << ",";
          }
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StructManager::GraphicStyle::read: find some unknown position block\n"));
        s << "###pos=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x146007a:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Code && child.m_fileType==0x8d000) {
          if (child.m_code=="LiRo")
            m_mitter=2;
          else if (child.m_code=="LiBe")
            m_mitter=3;
          else {
            MWAW_DEBUG_MSG(("RagTime5StructManager::GraphicStyle::read: find some unknown mitter string %s\n", child.m_code.c_str()));
            s << "##mitter=" << child.m_code << ",";
          }
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StructManager::GraphicStyle::read: find some unknown mitter block\n"));
        s << "###mitter=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x148981a:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Code && child.m_fileType==0x8d000) {
          if (child.m_code=="GrNo")
            m_gradient=1;
          else if (child.m_code=="GrRa")
            m_gradient=2;
          else {
            MWAW_DEBUG_MSG(("RagTime5StructManager::GraphicStyle::read: find some unknown gradient string %s\n", child.m_code.c_str()));
            s << "##gradient=" << child.m_code << ",";
          }
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StructManager::GraphicStyle::read: find some unknown gradient block\n"));
        s << "###gradient=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x14600aa:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Code && child.m_fileType==0x8d000) {
          if (child.m_code=="CaRo")
            m_cap=2;
          else if (child.m_code=="CaSq")
            m_cap=3;
          else {
            MWAW_DEBUG_MSG(("RagTime5StructManager::GraphicStyle::read: find some unknown cap string %s\n", child.m_code.c_str()));
            s << "##cap=" << child.m_code << ",";
          }
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StructManager::GraphicStyle::read: find some unknown cap block\n"));
        s << "###cap=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x148985a: // checkme
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Double && child.m_fileType==0x1495800) {
          m_gradientRotation=float(360*child.m_doubleValue);
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StructManager::GraphicStyle::read: find some unknown grad rotation block\n"));
        s << "###rot[grad]=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x148983a: // checkme
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_DoubleList && child.m_doubleList.size()==2 && child.m_fileType==0x74040) {
          m_gradientCenter=Vec2f((float) child.m_doubleList[0], (float) child.m_doubleList[1]);
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StructManager::GraphicStyle::read: find some unknown grad center block\n"));
        s << "###rot[center]=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    case 0x146008a:
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Double && child.m_fileType==0xb6000) {
          m_limitPercent=(float) child.m_doubleValue;
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StructManager::GraphicStyle::read: find some unknown limit percent block\n"));
        s << "###limitPercent=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    // unknown small id
    case 0x145e11a: { // frequent
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Long && child.m_fileType==0x17d5880) {
          s << "#unkn0=" << child.m_longValue[0] << ",";
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StructManager::GraphicStyle::read: find some unknown unkn0 block\n"));
        s << "###unkn0=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    }
    case 0x145e12a: // unknown small int 2|3
      for (size_t i=0; i<field.m_fieldList.size(); ++i) {
        RagTime5StructManager::Field const &child=field.m_fieldList[i];
        if (child.m_type==RagTime5StructManager::Field::T_Long && child.m_fileType==0x17d5880) {
          MWAW_DEBUG_MSG(("RagTime5StructManager::GraphicStyle::read: find some unkn1 block\n"));
          s << "#unkn1=" << child.m_longValue[0];
          continue;
        }
        MWAW_DEBUG_MSG(("RagTime5StructManager::GraphicStyle::read: find some unknown unkn1 block\n"));
        s << "###unkn1=" << child << ",";
      }
      m_extra+=s.str();
      return true;
    default:
      break;
    }
  }
  else if (field.m_type==RagTime5StructManager::Field::T_Unstructured) {
    switch (field.m_fileType) {
    case 0x148c01a: {
      if (field.m_entry.length()!=12) {
        MWAW_DEBUG_MSG(("RagTime5StructManager::GraphicStyle::read: find some odd size for pattern\n"));
        s << "##pattern=" << field << ",";
        m_extra+=s.str();
        return true;
      }
      input->seek(field.m_entry.begin(), librevenge::RVNG_SEEK_SET);
      for (int i=0; i<2; ++i) {
        static int const(expected[])= {0xb, 0x40};
        int val=(int) input->readULong(2);
        if (val!=expected[i])
          s << "pat" << i << "=" << std::hex << val << std::dec << ",";
      }
      m_pattern.reset(new MWAWGraphicStyle::Pattern);
      m_pattern->m_colors[0]=MWAWColor::white();
      m_pattern->m_colors[1]=MWAWColor::black();
      m_pattern->m_dim=Vec2i(8,8);
      m_pattern->m_data.resize(8);
      for (size_t i=0; i<8; ++i)
        m_pattern->m_data[i]=(unsigned char) input->readULong(1);
      m_extra+=s.str();
      return true;
    }
    default:
      break;
    }
  }
  return false;
}

std::ostream &operator<<(std::ostream &o, RagTime5StructManager::GraphicStyle const &style)
{
  if (style.m_parentId>=0) o << "parent=S" << style.m_parentId << ",";
  if (style.m_width>=0) o << "w=" << style.m_width << ",";
  if (!style.m_colors[0].isBlack()) o << "color0=" << style.m_colors[0] << ",";
  if (!style.m_colors[1].isWhite()) o << "color1=" << style.m_colors[1] << ",";
  for (int i=0; i<2; ++i) {
    if (style.m_colorsAlpha[i]<1)
      o << "color" << i << "[alpha]=" << style.m_colorsAlpha[i] << ",";
  }
  if (!style.m_dash.empty()) {
    o << "dash=";
    for (size_t i=0; i<style.m_dash.size(); ++i)
      o << style.m_dash[i] << ":";
    o << ",";
  }
  if (style.m_pattern)
    o << "pattern=[" << *style.m_pattern << "],";
  switch (style.m_gradient) {
  case 0:
    break;
  case 1:
    o << "grad[normal],";
    break;
  case 2:
    o << "grad[radial],";
    break;
  default:
    o<< "##gradient=" << style.m_gradient;
    break;
  }
  if (style.m_gradientRotation<0 || style.m_gradientRotation>0)
    o << "rot[grad]=" << style.m_gradientRotation << ",";
  if (style.m_gradientCenter!=Vec2f(0.5f,0.5f))
    o << "center[grad]=" << style.m_gradientCenter << ",";
  switch (style.m_position) {
  case 1:
    o << "pos[inside],";
    break;
  case 2:
    break;
  case 3:
    o << "pos[outside],";
    break;
  case 4:
    o << "pos[round],";
    break;
  default:
    o << "#pos=" << style.m_position << ",";
    break;
  }
  switch (style.m_cap) {
  case 1: // triangle
    break;
  case 2:
    o << "cap[round],";
    break;
  case 3:
    o << "cap[square],";
    break;
  default:
    o << "#cap=" << style.m_cap << ",";
    break;
  }
  switch (style.m_mitter) {
  case 1: // no add
    break;
  case 2:
    o << "mitter[round],";
    break;
  case 3:
    o << "mitter[out],";
    break;
  default:
    o << "#mitter=" << style.m_mitter << ",";
    break;
  }
  if (style.m_limitPercent<1||style.m_limitPercent>1)
    o << "limit=" << 100*style.m_limitPercent << "%";
  if (style.m_hidden)
    o << "hidden,";
  o << style.m_extra;
  return o;
}
////////////////////////////////////////////////////////////
// zone function
////////////////////////////////////////////////////////////
void RagTime5StructManager::Zone::createAsciiFile()
{
  if (m_asciiName.empty()) {
    MWAW_DEBUG_MSG(("RagTime5StructManager::Zone::createAsciiFile: can not find the ascii name\n"));
    return;
  }
  if (m_localAsciiFile) {
    MWAW_DEBUG_MSG(("RagTime5StructManager::Zone::createAsciiFile: the ascii file already exist\n"));
  }
  m_localAsciiFile.reset(new libmwaw::DebugFile(m_input));
  m_asciiFile = m_localAsciiFile.get();
  m_asciiFile->open(m_asciiName.c_str());
}

std::string RagTime5StructManager::Zone::getZoneName() const
{
  switch (m_ids[0]) {
  case 0:
    if (m_type==Data)
      return "FileHeader";
    break;
  case 1: // with g4=1, gd=[1, lastDataZones]
    if (m_type==Main)
      return "ZoneInfo";
    break;
  case 3: // with no value or gd=[1,_] (if multiple)
    if (m_type==Main)
      return "Main3A";
    break;
  case 4:
    if (m_type==Main)
      return "ZoneLimits,";
    break;
  case 5:
    if (m_type==Main)
      return "FileLimits";
    break;
  case 6: // gd=[_,_]
    if (m_type==Main)
      return "Main6A";
    break;
  case 8: // type=UseCount, gd=[0,num>1], can be multiple
    if (m_type==Main)
      return "UnknCounter8";
    break;
  case 10: // type=SingleRef, gd=[1,id], Data id is a list types
    if (m_type==Main)
      return "Types";
    break;
  case 11: // type=SingleRef, gd=[1,id]
    if (m_type==Main)
      return "Cluster";
    break;
  default:
    break;
  }
  std::stringstream s;
  switch (m_type) {
  case Main:
    s << "Main" << m_ids[0] << "A";
    break;
  case Data:
    s << "Data" << m_ids[0] << "A";
    break;
  case Empty:
    s << "unused" << m_ids[0];
    break;
  case Unknown:
  default:
    s << "##zone" << m_subType << ":" << m_ids[0] << "";
    break;
  }
  return s.str();
}

std::ostream &operator<<(std::ostream &o, RagTime5StructManager::Zone const &z)
{
  o << z.getZoneName();
  if (z.m_idsFlag[0])
    o << "[" << z.m_idsFlag[0] << "],";
  else
    o << ",";
  for (int i=1; i<3; ++i) {
    if (!z.m_kinds[i-1].empty()) {
      o << z.m_kinds[i-1] << ",";
      continue;
    }
    if (!z.m_ids[i] && !z.m_idsFlag[i]) continue;
    o << "id" << i << "=" << z.m_ids[i];
    if (z.m_idsFlag[i]==0)
      o << "*";
    else if (z.m_idsFlag[i]!=1)
      o << ":" << z.m_idsFlag[i] << ",";
    o << ",";
  }
  if (z.m_variableD[0] || z.m_variableD[1])
    o << "varD=[" << z.m_variableD[0] << "," << z.m_variableD[1] << "],";
  if (z.m_entry.valid())
    o << z.m_entry.begin() << "<->" << z.m_entry.end() << ",";
  else if (!z.m_entriesList.empty()) {
    o << "ptr=" << std::hex;
    for (size_t i=0; i< z.m_entriesList.size(); ++i) {
      o << z.m_entriesList[i].begin() << "<->" << z.m_entriesList[i].end();
      if (i+1<z.m_entriesList.size())
        o << "+";
    }
    o << std::dec << ",";
  }
  if (!z.m_hiLoEndian) o << "loHi[endian],";
  o << z.m_extra << ",";
  return o;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
