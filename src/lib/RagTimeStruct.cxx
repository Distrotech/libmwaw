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
#include <iostream>

#include <RagTimeStruct.hxx>

namespace RagTimeStruct
{

bool ResourceList::read(MWAWInputStreamPtr input, MWAWEntry &entry)
{
  long pos=entry.begin();
  if (pos<=0 || !input->checkPosition(pos+2))
    return false;
  entry.setParsed(true);
  input->seek(pos, librevenge::RVNG_SEEK_SET);
  int dSz=(int) input->readULong(2);
  m_headerPos=pos+2;
  if (dSz==0)
    return true;
  m_endPos=pos+2+dSz;
  m_headerSize=(int) input->readULong(2);
  m_dataSize=(int) input->readULong(2);
  m_dataNumber=(int) input->readULong(2);
  if (m_headerSize<0x20 || dSz<m_headerSize+(m_dataNumber+1)*m_dataSize || !input->checkPosition(m_endPos))
    return false;
  libmwaw::DebugStream f;
  int unkn[3];
  for (int i=0; i<3; ++i) unkn[i]=(int) input->readLong(2);
  bool printUnknown=true;
  /* try to use the first value to retrieve the type of the zone */
  switch (unkn[0]) {
  case 0:
    if (m_dataSize==6 && unkn[1]==2 && unkn[2]==4) {
      m_type=gray;
      printUnknown=false;
    }
    else if (entry.type()=="rsrcgray")
      m_type=gray;
    break;
  case 6:
    if (m_dataSize==10 && unkn[1]==4 && unkn[2]==0) {
      m_type=SpTe;
      printUnknown=false;
    }
    else if (entry.type()=="rsrcSpTe")
      m_type=SpTe;
    break;
  case 8:
    if (m_dataSize==12 && unkn[1]==4 && unkn[2]==0) {
      m_type=SpVa;
      printUnknown=false;
    }
    else if (entry.type()=="rsrcSpVa")
      m_type=SpVa;
    break;
  case 16:
    if (m_dataSize==20 && unkn[1]==4 && unkn[2]==0) {
      m_type=SpBo;
      printUnknown=false;
    }
    else if (entry.type()=="rsrcSpBo")
      m_type=SpBo;
    break;
  case 22:
    if (m_dataSize==26 && unkn[1]==4 && unkn[2]==0) {
      m_type=BuGr;
      printUnknown=false;
    }
    else if (entry.type()=="rsrcBuGr")
      m_type=BuGr;
    break;
  case 4:
    if (m_dataSize==8 && unkn[1]==4 && unkn[2]==0) {
      m_type=SpCe;
      printUnknown=false;
      break;
    }
    if (m_dataSize==10 && unkn[1]==4 && unkn[2]==0) { // find also with unkn[1]==0
      m_type=SpDE;
      printUnknown=false;
      break;
    }
    // (m_dataSize==6 && unkn[1]==4 && unkn[2]==2) can be BuSl, colr, res_
    if (entry.type()=="rsrcSpCe")
      m_type=SpCe;
    else if (entry.type()=="rsrcSpDE")
      m_type=SpDE;
    else if (entry.type()=="rsrcBuSl")
      m_type=BuSl;
    else if (entry.type()=="rsrccolr")
      m_type=colr;
    else if (entry.type()=="rsrcres_")
      m_type=res_;
    break;
  default:
    break;
  }
  if (printUnknown) {
    f << "unkn=[";
    for (int i=0; i<3; ++i) f << unkn[i] << ",";
    f << "],";
  }
  for (int i=0; i<9; ++i) { // g3=3|5|7,g4=-1|-2|9
    int val=(int) input->readLong(2);
    if (val) f << "g" << i << "=" << val << ",";
  }
  int subN=(int) input->readLong(2);
  if (m_headerSize!=0x20+subN*6) {
    MWAW_DEBUG_MSG(("RagTimeStruct::ResourceList::read: the number of sub data seems bad\n"));
    f << "##subN=" << subN << ",";
    subN=0;
  }
  for (int i=0; i<subN; ++i) {
    f << "unkn" << i << "=[";
    // first always 0, second d|17|21|35|49|67|71|85|a3, last increasing index
    for (int j=0; j<3; ++j) {
      int val=(int)input->readLong(2);
      if (val) f << val << ",";
      else f << "_,";
    }
    f << "],";
  }
  m_extra=f.str();
  m_dataPos=pos+2+m_headerSize;
  return true;
}

std::ostream &operator<<(std::ostream &o, ResourceList &zone)
{
  o << "type=" << ResourceList::getName(zone.m_type) << ",";
  if (zone.m_headerSize)
    o << "sz[header]=" << zone.m_headerSize << ",";
  if (zone.m_dataNumber)
    o << "N[data]=" << zone.m_dataNumber << ",sz[data]=" << zone.m_dataSize << ",";
  o << zone.m_extra;
  return o;
}

}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
