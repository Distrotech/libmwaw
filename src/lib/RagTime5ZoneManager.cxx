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
#include <sstream>

#include "MWAWDebug.hxx"
#include "MWAWPrinter.hxx"

#include "RagTime5Parser.hxx"
#include "RagTime5StructManager.hxx"

#include "RagTime5ZoneManager.hxx"

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
RagTime5ZoneManager::RagTime5ZoneManager(RagTime5Parser &parser) : m_mainParser(parser), m_structManager(m_mainParser.getStructManager())
{
}

RagTime5ZoneManager::~RagTime5ZoneManager()
{
}

////////////////////////////////////////////////////////////
// read basic structures
////////////////////////////////////////////////////////////

bool RagTime5ZoneManager::readDataIdList(MWAWInputStreamPtr input, int n, std::vector<int> &listIds)
{
  listIds.clear();
  long pos=input->tell();
  for (int i=0; i<n; ++i) {
    int val=(int) MWAWInputStream::readULong(input->input().get(), 2, 0, false);
    if (val==0) {
      listIds.push_back(0);
      input->seek(2, librevenge::RVNG_SEEK_CUR);
      continue;
    }
    if (val!=1) {
      // update the position
      input->seek(pos+4*n, librevenge::RVNG_SEEK_SET);
      return false;
    }
    listIds.push_back((int) MWAWInputStream::readULong(input->input().get(), 2, 0, false));
  }
  return true;
}

////////////////////////////////////////////////////////////
// zone function
////////////////////////////////////////////////////////////
void RagTime5Zone::createAsciiFile()
{
  if (m_asciiName.empty()) {
    MWAW_DEBUG_MSG(("RagTime5Zone::createAsciiFile: can not find the ascii name\n"));
    return;
  }
  if (m_localAsciiFile) {
    MWAW_DEBUG_MSG(("RagTime5Zone::createAsciiFile: the ascii file already exist\n"));
  }
  m_localAsciiFile.reset(new libmwaw::DebugFile(m_input));
  m_asciiFile = m_localAsciiFile.get();
  m_asciiFile->open(m_asciiName.c_str());
}

std::string RagTime5Zone::getZoneName() const
{
  switch (m_ids[0]) {
  case 0:
    if (m_fileType==F_Data)
      return "FileHeader";
    break;
  case 1: // with g4=1, gd=[1, lastDataZones]
    if (m_fileType==F_Main)
      return "ZoneInfo";
    break;
  case 3: // with no value or gd=[1,_] (if multiple)
    if (m_fileType==F_Main)
      return "Main3A";
    break;
  case 4:
    if (m_fileType==F_Main)
      return "ZoneLimits,";
    break;
  case 5:
    if (m_fileType==F_Main)
      return "FileLimits";
    break;
  case 6: // gd=[_,_]
    if (m_fileType==F_Main)
      return "Main6A";
    break;
  case 8: // type=UseCount, gd=[0,num>1], can be multiple
    if (m_fileType==F_Main)
      return "UnknCounter8";
    break;
  case 10: // type=SingleRef, gd=[1,id], Data id is a list types
    if (m_fileType==F_Main)
      return "Types";
    break;
  case 11: // type=SingleRef, gd=[1,id]
    if (m_fileType==F_Main)
      return "Cluster";
    break;
  default:
    break;
  }
  std::stringstream s;
  switch (m_fileType) {
  case F_Main:
    s << "Main" << m_ids[0] << "A";
    break;
  case F_Data:
    s << "Data" << m_ids[0] << "A";
    break;
  case F_Empty:
    s << "unused" << m_ids[0];
    break;
  case F_Unknown:
  default:
    s << "##zone" << m_subType << ":" << m_ids[0] << "";
    break;
  }
  return s.str();
}

std::ostream &operator<<(std::ostream &o, RagTime5Zone const &z)
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

////////////////////////////////////////////////////////////
// link to cluster
////////////////////////////////////////////////////////////
bool RagTime5ZoneManager::readFieldClusters(Link const &link)
{
  if (link.m_ids.size()!=2) {
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::readFieldClusters: call with bad ids\n"));
    return false;
  }
  for (size_t i=0; i<2; ++i) {  // fielddef and fieldpos
    if (!link.m_ids[i]) continue;
    shared_ptr<RagTime5Zone> data=m_mainParser.getDataZone(link.m_ids[i]);
    if (!data || data->m_isParsed || data->getKindLastPart(data->m_kinds[1].empty())!="Cluster") {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readFieldClusters: the child cluster id %d seems bad\n", link.m_ids[i]));
      continue;
    }
    m_mainParser.readClusterZone(*data, 0x20000+int(i));
  }
  return true;
}

bool RagTime5ZoneManager::readUnknownClusterC(Link const &link)
{
  if (link.m_ids.size()!=4) {
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterC: call with bad ids\n"));
    return false;
  }
  for (size_t i=0; i<4; ++i) {
    if (!link.m_ids[i]) continue;
    shared_ptr<RagTime5Zone> data=m_mainParser.getDataZone(link.m_ids[i]);
    if (!data || data->m_isParsed || data->getKindLastPart(data->m_kinds[1].empty())!="Cluster") {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterC: the child cluster id %d seems bad\n", link.m_ids[i]));
      continue;
    }
    m_mainParser.readClusterZone(*data, 0x30000+int(i));
  }
  return true;
}

////////////////////////////////////////////////////////////
// style cluster
////////////////////////////////////////////////////////////
bool RagTime5ZoneManager::readStyleCluster(RagTime5Zone &zone, Cluster &cluster)
{
  MWAWEntry &entry=zone.m_entry;
  if (entry.length()==0) return true;
  if (entry.length()<13) return false;

  MWAWInputStreamPtr input=zone.getInput();
  long endPos=entry.end();
  input->setReadInverted(!zone.m_hiLoEndian);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  std::string zoneName(f.str());
  f.str("");
  f << "Entries(ClustStyle)[" << zone << "]:";
  int val;
  for (int i=0; i<4; ++i) { // f0=f1=0, f2=1, f3=small number
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  zone.m_isParsed=true;
  libmwaw::DebugFile &ascFile=zone.ascii();
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(endPos);
  ascFile.addNote("_");

  cluster.m_hiLoEndian=zone.m_hiLoEndian;
  // normally 5 zone, zone0: link to data zone, zone4: unicode list
  for (int n=0; n<6; ++n) {
    long pos=input->tell();
    if (pos>=endPos || input->isEnd()) break;
    long lVal;
    if (!m_structManager->readCompressedLong(input, endPos, lVal)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f.str("");
    f << "ClustStyle-" << n << ":";
    f << "f0=" << lVal << ",";
    // always in big endian
    long sz;
    if (!m_structManager->readCompressedLong(input,endPos,sz) || sz <= 7 || input->tell()+sz>endPos) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f << "sz=" << sz << ",";
    long endDataPos=input->tell()+sz;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    pos=input->tell();
    f.str("");
    f << "ClustStyle-" << n << "-A:";
    if (!zone.m_hiLoEndian) f << "lohi,";
    long fSz;
    if (!m_structManager->readCompressedLong(input, endDataPos, fSz) || fSz<6 || input->tell()+fSz>endDataPos) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readStyleCluster: can not read item A\n"));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
      continue;
    }
    long debSubDataPos=input->tell();
    long endSubDataPos=debSubDataPos+fSz;
    int fl=(int) input->readULong(2); // [01][13][0139b]
    f << "fl=" << std::hex << fl << std::dec << ",";
    int N=(int) input->readLong(4);
    std::stringstream s;
    s << "type" << std::hex << N << std::dec;

    enum What { W_Unknown, W_UnicodeLink,
                W_MainStructZones, W_Formats
              };
    What what=W_Unknown;
    std::string name=s.str();
    Link link;
    bool isMainLink=false, isNamedLink=false;

    switch (N) {
    case -5: // n=0
      name="header";
      if (fSz<12) {
        f << "###data,";
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readStyleCluster: can not read the data id\n"));
        break;
      }
      for (int i=0; i<2; ++i) { // always 0?
        val=(int) input->readLong(2);
        if (val) f << "f" << i+1 << "=" << val << ",";
      }
      val=(int) input->readLong(2);
      f << "id=" << val << ",";
      if ((fSz!=58 && fSz!=64 && fSz!=66 && fSz!=68) || n!=0) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readStyleCluster: find unknown block\n"));
        f << "###unknown,";
        break;
      }
      val=(int) input->readULong(2);
      if (val!=0x480) {
        f << "###type=" << std::hex << val << ",";
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readStyleCluster: the field format seems bad\n"));
      }
      link.m_N=val;
      for (int i=0; i<13; ++i) { // g3=2, g4=10, g6 and g8 2 small int
        val=(int) input->readLong(2);
        if (val) f << "g" << i << "=" << val << ",";
      }
      link.m_fileType[0]=(int) input->readULong(4);
      if (link.m_fileType[0] != 0x01473857 && link.m_fileType[0] != 0x0146e827) {
        f << "###fileType=" << std::hex << link.m_fileType[0] << std::dec << ",";
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readStyleCluster: the field type seems bad\n"));
      }
      link.m_fileType[1]=(int) input->readULong(2); // c018|c030|c038 or type ?
      if (!readDataIdList(input, 2, link.m_ids) || link.m_ids[1]==0) {
        f << "###noData,";
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readStyleCluster: can not find any data\n"));
        break;
      }

      link.m_type=Link::L_FieldsList;
      what=W_MainStructZones;
      isMainLink=true;
      if (fSz==58) {
        if (link.m_fileType[0] == 0x0146e827) {
          what=W_Formats;
          link.m_name=name="formats";
          cluster.m_type=Cluster::C_Formats;
        }
        else {
          link.m_name=name="units";
          cluster.m_type=Cluster::C_Units;
        }
      }
      else if (fSz==64) {
        link.m_name=name="graphColor";
        cluster.m_type=Cluster::C_GraphicColors;
      }
      else if (fSz==66) {
        link.m_name=name="textStyle";
        cluster.m_type=Cluster::C_TextStyles;
      }
      else {
        link.m_name=name="graphStyle";
        cluster.m_type=Cluster::C_GraphicStyles;
      }
      ascFile.addDelimiter(input->tell(), '|');
      break;
    case -2:
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readStyleCluster: find unexpected header -2\n"));
      break;
    default: {
      if (what!=W_Unknown) break;

      input->seek(debSubDataPos+18, librevenge::RVNG_SEEK_SET);
      long type=(long) input->readULong(4);
      if (type) f << "type=" << std::hex << type << std::dec << ",";
      input->seek(debSubDataPos+6, librevenge::RVNG_SEEK_SET);
      // n=2,3 with fSz=28, type=0x3e800 : some time a data
      if (fSz==28) {
        link.m_fileType[0]=(int) input->readULong(4);
        if (link.m_fileType[0]!=0x35800 && link.m_fileType[0]!=0x3e800) {
          f << "###fType=" << std::hex << link.m_fileType[0] << std::dec << ",";
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readStyleCluster: the field 2,3 type seems bad\n"));
          break;
        }
        if (n!=2 && n!=3) {
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readStyleCluster: n seems bad\n"));
          f << "##n=" << n << ",";
        }
        for (int i=0; i<6; ++i) { // always 0?
          val=(int) input->readLong(2);
          if (val) f << "f" << i << "=" << val << ",";
        }
        val=(int) input->readLong(2); // always 20?
        if (val!=32) f << "f6=" << val << ",";
        if (readDataIdList(input, 1, link.m_ids) && link.m_ids[0]) {
          if (cluster.m_type!=Cluster::C_Formats) {
            f << "###unexpected";
            MWAW_DEBUG_MSG(("RagTime5ZoneManager::readStyleCluster: find link for not format data\n"));
          }
          link.m_fieldSize=4;
          link.m_N=N;
          break;
        }

        break;
      }
      // n=4
      if (fSz==32) {
        if (n!=4) {
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readStyleCluster: n seems bad\n"));
          f << "##n=" << n << ",";
        }
        name="unicodeList";
        what=W_UnicodeLink;
        isNamedLink=true;
        link.m_type=Link::L_UnicodeList;

        for (int i=0; i<8; ++i) { // always 0?
          val=(int) input->readULong(2);
          if (val) f << "f" << i << "=" << val << ",";
        }
        link.m_fileType[1]=(long) input->readULong(2);
        if (link.m_fileType[1]!=0x220 && link.m_fileType[1]!=0x200 && link.m_fileType[1]!=0x620)
          f << "###type=" << std::hex << link.m_fileType[1] << std::dec << ",";
        if (!readDataIdList(input, 2, link.m_ids) || !link.m_ids[1]) {
          f << "###";
          link=Link();
          break;
        }
        break;
      }
      // n=1 with fSz=36, type=7d0a
      if (fSz==36) {
        if (n!=1) {
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readStyleCluster: n seems bad\n"));
          f << "##n=" << n << ",";
        }
        for (int i=0; i<2; ++i) { // always 0
          val=(int) input->readLong(2);
          if (val) f << "f" << i << "=" << val << ",";
        }
        link.m_fileType[0]=(int) input->readULong(4);
        if (link.m_fileType[0]!=0x7d01a) {
          f << "###fType=" << std::hex << link.m_fileType[0] << std::dec << ",";
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readStyleCluster: the field 1 type seems bad\n"));
          break;
        }
        for (int i=0; i<4; ++i) { // always 0
          val=(int) input->readLong(2);
          if (val) f << "f" << i+2 << "=" << val << ",";
        }
        link.m_fileType[1]=(int) input->readULong(2);
        if (link.m_fileType[1]!=0x10 && link.m_fileType[1]!=0x18) {
          f << "###fType1=" << std::hex << link.m_fileType[1] << std::dec << ",";
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readStyleCluster: the field 1 type1 seems bad\n"));
          break;
        }
        for (int i=0; i<3; ++i) { // always 3,4,5 ?
          val=(int) input->readLong(4);
          if (val!=i+3) f << "g" << i << "=" << val << ",";
        }
      }
      break;
    }
    }
    if (link.empty())
      f << name << "-" << lVal << ",";
    else
      f << link << ",";
    if (input->tell()!=pos && input->tell()!=endSubDataPos)
      ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(endSubDataPos, librevenge::RVNG_SEEK_SET);

    int m=0;

    while (!input->isEnd()) {
      pos=input->tell();
      if (pos+4>endDataPos)
        break;
      RagTime5StructManager::Field field;
      if (!m_structManager->readField(input, endDataPos, ascFile, field)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        break;
      }
      f.str("");
      f << "ClustStyle-" << n << "-B" << m++ << "[" << name << "]:";
      if (!zone.m_hiLoEndian) f << "lohi,";
      bool done=false;
      switch (what) {
      case W_UnicodeLink: {
        if (field.m_type==RagTime5StructManager::Field::T_LongList && field.m_fileType==0xce842) {
          f << "pos=[";
          for (size_t i=0; i<field.m_longList.size(); ++i)
            f << field.m_longList[i] << ",";
          f << "],";
          link.m_longList=field.m_longList;
          done=true;
          break;
        }
        if (field.m_type==RagTime5StructManager::Field::T_Unstructured && field.m_fileType==0xce017) {
          // a small value 2|4|a|1c|40
          f << "unkn="<<field.m_extra << ",";
          done=true;
          break;
        }
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readStyleCluster: find unexpected list link field\n"));
        f << "###";
        break;
      }
      case W_Formats: {
        if (field.m_type==RagTime5StructManager::Field::T_FieldList && field.m_fileType==0x146e815) {
          f << "decal=[";
          for (size_t i=0; i<field.m_fieldList.size(); ++i) {
            RagTime5StructManager::Field const &child=field.m_fieldList[i];
            if (child.m_type==RagTime5StructManager::Field::T_LongList && child.m_fileType==0xce842) {
              for (size_t j=0; j<child.m_longList.size(); ++j)
                f << child.m_longList[j] << ",";
              link.m_longList=child.m_longList;
              continue;
            }
            if (child.m_type==RagTime5StructManager::Field::T_Unstructured && child.m_fileType==0xce017) {
              // a list of small int 0104|0110|22f8ffff7f3f
              f << "unkn0=" << child.m_extra << ",";
              continue;
            }
            f << "#[" << child << "],";
          }
          f << "],";
          done=true;
          break;
        }
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readStyleCluster: find unexpected child[format]\n"));
        break;
      }
      case W_MainStructZones: {
        if (field.m_type==RagTime5StructManager::Field::T_FieldList && field.m_fileType==0x1473815) {
          f << "decal=[";
          for (size_t i=0; i<field.m_fieldList.size(); ++i) {
            RagTime5StructManager::Field const &child=field.m_fieldList[i];
            if (child.m_type==RagTime5StructManager::Field::T_LongList && child.m_fileType==0xce842) {
              for (size_t j=0; j<child.m_longList.size(); ++j)
                f << child.m_longList[j] << ",";
              link.m_longList=child.m_longList;
              continue;
            }
            else if (child.m_type==RagTime5StructManager::Field::T_Unstructured && child.m_fileType==0xce017) {
              /* find 00000003,00000007,00000008,0000002000,000002,000042,000400,000400000000000000000000,0008
                 000810,004000,00400000,800a00 : a list of bool, one for each style ?
                 checkme: maybe related to style with name
              */
              f << "unkn=[" << child.m_extra << "],";
              continue;
            }
            MWAW_DEBUG_MSG(("RagTime5ZoneManager::readStyleCluster: find unexpected decal child[%s]\n", link.m_name.c_str()));
            f << "#[" << child << "],";
          }
          f << "],";
          done=true;
          break;
        }
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readStyleCluster: find unexpected child[%s]\n", link.m_name.c_str()));
        break;
      }
      case W_Unknown:
      default:
        break;
      }
      if (done) {
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        continue;
      }
      f << field;
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    if (!link.empty()) {
      if (isMainLink) {
        if (cluster.m_dataLink.empty())
          cluster.m_dataLink=link;
        else {
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readStyleCluster: oops the main link is already set\n"));
          cluster.m_linksList.push_back(link);
        }
      }
      else if (isNamedLink) {
        if (cluster.m_nameLink.empty())
          cluster.m_nameLink=link;
        else {
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readStyleCluster: oops the name link is already set\n"));
          cluster.m_linksList.push_back(link);
        }
      }
      else
        cluster.m_linksList.push_back(link);
    }
    pos=input->tell();
    if (pos!=endDataPos) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readStyleCluster: find some extra data\n"));
      f.str("");
      f << "ClustStyle-" << n << ":" << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
  }
  long pos=input->tell();
  if (pos!=endPos) {
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::readStyleCluster: find some extra data\n"));
    f.str("");
    f << "ClustStyle-###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->setReadInverted(false);

  return true;
}

////////////////////////////////////////////////////////////
// color pattern cluster
////////////////////////////////////////////////////////////
bool RagTime5ZoneManager::readColPatCluster(RagTime5Zone &zone, Cluster &cluster)
{
  MWAWEntry &entry=zone.m_entry;
  if (entry.length()==0) return true;
  if (entry.length()<13) return false;

  MWAWInputStreamPtr input=zone.getInput();
  long endPos=entry.end();
  input->setReadInverted(!zone.m_hiLoEndian);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  std::string zoneName(f.str());
  f.str("");
  f << "Entries(ClustColPat)[" << zone << "]:";
  int val;
  for (int i=0; i<4; ++i) { // f0=f1=0, f2=1, f3=0|1|4
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  zone.m_isParsed=true;
  libmwaw::DebugFile &ascFile=zone.ascii();
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(endPos);
  ascFile.addNote("_");

  cluster.m_hiLoEndian=zone.m_hiLoEndian;
  cluster.m_type=Cluster::C_ColorPattern;
  // normally 2: zone 0 link to data zone + zone 1 link to a field zone with 10 bytes data
  for (int n=0; n<3; ++n) {
    long pos=input->tell();
    if (pos>=endPos || input->isEnd()) break;
    long lVal;
    if (!m_structManager->readCompressedLong(input, endPos, lVal)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f.str("");
    f << "ClustColPat-" << n << ":";
    // n=0, 42|44|146|152, n=1, f
    if (n!=1 || lVal!=0xf)
      f << "f0=" << lVal << ",";
    // always in big endian
    long sz;
    if (!m_structManager->readCompressedLong(input,endPos,sz) || sz <= 7 || input->tell()+sz>endPos) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f << "sz=" << sz << ",";
    long endDataPos=input->tell()+sz;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    pos=input->tell();
    f.str("");
    f << "ClustColPat-" << n << "-A:";
    if (!zone.m_hiLoEndian) f << "lohi,";
    long fSz;
    if (!m_structManager->readCompressedLong(input, endDataPos, fSz) || fSz<6 || input->tell()+fSz>endDataPos) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readColPatCluster: can not read item A\n"));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
      continue;
    }
    long debSubDataPos=input->tell();
    long endSubDataPos=debSubDataPos+fSz;
    int fl=(int) input->readULong(2);
    if ((n==0&&fl!=0x30) || (n==1&&fl!=0x10) || n>=2)
      f << "fl=" << std::hex << fl << std::dec << ",";
    int N=(int) input->readLong(4);
    Link link;

    switch (N) {
    case -5: { // n=0
      if (n || (fSz!=82 && fSz!=86)) {
        f << "###data,";
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readColPatCluster: find unexpected field\n"));
        break;
      }
      for (int i=0; i<2; ++i) { // always 0?
        val=(int) input->readLong(2);
        if (val) f << "f" << i+1 << "=" << val << ",";
      }
      val=(int) input->readULong(4);
      if (val!=0x16a8042) f << "#fileType=" << std::hex << val << std::dec << ",";
      for (int i=0; i<2; ++i) {
        val=(int) input->readLong(2);
        if (val) f << "f" << i+3 << "=" << val << ",";
      }

      for (int wh=0; wh<2; ++wh) {
        long actPos=input->tell();
        link=Link();
        f << "link" << wh << "=[";
        val=(int) input->readLong(2);
        if (val!= 0x10)
          f << "f0=" << val << ",";
        val=(int) input->readLong(2);
        if (val) // always 0?
          f << "f1=" << val << ",";
        link.m_N=(int) input->readLong(2);
        link.m_fileType[1]=(long) input->readULong(4);
        if ((wh==0 && link.m_fileType[1]!=0x84040) ||
            (wh==1 && link.m_fileType[1]!=0x16de842))
          f << "#fileType=" << std::hex << link.m_fileType[1] << std::dec << ",";
        for (int i=0; i<7; ++i) {
          val=(int) input->readLong(2); // always 0?
          if (val) f << "f" << i+2 << "=" << val << ",";
        }
        link.m_fieldSize=(int) input->readULong(2);
        std::vector<int> listIds;
        if (!m_structManager->readDataIdList(input, 1, listIds)) {
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readColPatCluster: can not read the data id\n"));
          link=Link();
          f << "##link=" << link << "],";
          input->seek(actPos+30, librevenge::RVNG_SEEK_SET);
          continue;
        }
        if (listIds[0]) {
          link.m_ids.push_back(listIds[0]);
          cluster.m_linksList.push_back(link);
        }
        f << link;
        f << "],";
      }
      std::vector<int> listIds;
      if (!m_structManager->readDataIdList(input, 1, listIds)) {
        f << "##clusterIds";
        break;
      }
      if (listIds[0]) {
        cluster.m_clusterIdsList.push_back(listIds[0]);
        f << "clusterId1=data" << listIds[0] << "A,";
      }
      if (fSz==82)
        break;
      val=(int) input->readLong(4);
      if (val!=2)
        f << "g0=" << val << ",";
      break;
    }
    default: {
      if (N<=0) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readColPatCluster: find unexpected header -2\n"));
        f << "###N=" << N << ",";
        break;
      }
      if (fSz!=30 || n!=1) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readColPatCluster: find unexpected data size\n"));
        f << "###fSz=" << fSz << ",";
        break;
      }
      for (int i=0; i<8; ++i) { // f1=2b|2d|85|93
        val=(int) input->readLong(2);
        if (val) f << "f" << i << "=" << val << ",";
      }

      link.m_fileType[1]=(int) input->readULong(2);
      if (link.m_fileType[1]!=0x40)
        f << "#fileType=" << std::hex << link.m_fileType[1] << ",";
      link.m_fieldSize=(int) input->readULong(2);
      if (m_structManager->readDataIdList(input, 1, link.m_ids) && link.m_ids[0]) {
        link.m_N=N;
        cluster.m_linksList.push_back(link);
      }
      f << link << ",";
      break;
    }
    }
    if (input->tell()!=pos && input->tell()!=endSubDataPos)
      ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(endSubDataPos, librevenge::RVNG_SEEK_SET);

    int m=0;

    while (!input->isEnd()) {
      pos=input->tell();
      if (pos+4>endDataPos)
        break;
      RagTime5StructManager::Field field;
      if (!m_structManager->readField(input, endDataPos, ascFile, field)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        break;
      }
      f.str("");
      f << "ClustColPat-" << n << "-B" << m++ << ":";
      if (!zone.m_hiLoEndian) f << "lohi,";
      if (n==0 && field.m_type==RagTime5StructManager::Field::T_FieldList && (field.m_fileType==0x16be055 || field.m_fileType==0x16be065)) {
        f << "unk" << (field.m_fileType==0x16be055 ? "0" : "1") << "=";
        for (size_t i=0; i<field.m_fieldList.size(); ++i) {
          RagTime5StructManager::Field const &child=field.m_fieldList[i];
          if (child.m_type==RagTime5StructManager::Field::T_Long && child.m_fileType==0xcf817) {
            f << child.m_longValue[0] << ",";
            continue;
          }
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readColPatCluster: find unexpected color/pattern child field\n"));
          f << "#[" << child << "],";
        }
      }
      else {
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readColPatCluster: find unexpected sub field\n"));
        f << "#" << field;
      }
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    pos=input->tell();
    if (pos!=endDataPos) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readColPatCluster: find some extra data\n"));
      f.str("");
      f << "ClustColPat-" << n << ":" << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
  }
  long pos=input->tell();
  if (pos!=endPos) {
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::readColPatCluster: find some extra data\n"));
    f.str("");
    f << "ClustColPat-###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->setReadInverted(false);

  return true;
}

////////////////////////////////////////////////////////////
// field cluster (field def or field pos)
////////////////////////////////////////////////////////////
bool RagTime5ZoneManager::readFieldCluster(RagTime5Zone &zone, Cluster &cluster, int type)
{
  MWAWEntry &entry=zone.m_entry;
  if (entry.length()==0) return true;
  if (entry.length()<13) return false;

  MWAWInputStreamPtr input=zone.getInput();
  long endPos=entry.end();
  input->setReadInverted(!zone.m_hiLoEndian);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  std::string zoneName;
  switch (type) {
  case 0x20000:
    zoneName="ClustField_Def";
    break;
  case 0x20001:
    zoneName="ClustField_Pos";
    break;
  default:
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::readFieldCluster: find unexpected type\n"));
    zoneName="ClustField_BAD";
    break;
  }
  f.str("");
  f << "Entries(" << zoneName << ")[" << zone << "]:";
  int val;
  for (int i=0; i<4; ++i) { // f0=f1=0, f2=1, f3=0|1|3|4
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  zone.m_isParsed=true;
  libmwaw::DebugFile &ascFile=zone.ascii();
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(endPos);
  ascFile.addNote("_");

  cluster.m_hiLoEndian=zone.m_hiLoEndian;
  cluster.m_type=Cluster::C_Fields;
  // normally 1 zone
  for (int n=0; n<2; ++n) {
    long pos=input->tell();
    if (pos>=endPos || input->isEnd()) break;
    long lVal;
    if (!m_structManager->readCompressedLong(input, endPos, lVal)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f.str("");
    f << zoneName << "-" << n << ":";
    f << "f0=" << lVal << ","; // 39-94
    long sz;
    if (!m_structManager->readCompressedLong(input,endPos,sz) || sz <= 7 || input->tell()+sz>endPos) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f << "sz=" << sz << ",";
    long endDataPos=input->tell()+sz;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    pos=input->tell();
    f.str("");
    f << zoneName << "-" << n << "-A:";
    if (!zone.m_hiLoEndian) f << "lohi,";
    long fSz;
    if (!m_structManager->readCompressedLong(input, endDataPos, fSz) || fSz<6 || input->tell()+fSz>endDataPos) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readFieldCluster: can not read item A\n"));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
      continue;
    }
    long debSubDataPos=input->tell();
    long endSubDataPos=debSubDataPos+fSz;
    int fl=(int) input->readULong(2);
    if (fl!=0x30)
      f << "fl=" << std::hex << fl << std::dec << ",";
    int N=(int) input->readLong(4);

    Link link;
    link.m_fileType[0]=type;
    if (N<=0) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readFieldCluster: find unexpected header\n"));
      f << "###type" << std::hex << N << std::dec;
    }
    else if (n==0 && ((type==0x20000 && fSz==41) || (type==0x20001 && fSz==32))) {
      for (int i=0; i<8; ++i) {
        val=(int) input->readLong(2);
        if (val) f << "f" << i << "=" << val << ",";
      }
      link.m_type= Link::L_List;
      link.m_fileType[1]=(long) input->readULong(2);
      if ((link.m_fileType[1]&0xFFD7)!=0x1010)
        f << "#fileType=" << std::hex << link.m_fileType[1] << std::dec << ",";
      if (!m_structManager->readDataIdList(input, 2, link.m_ids) || !link.m_ids[1]) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readFieldCluster: can not read the data def\n"));
        f << "###noData,";
        link=Link();
      }
      else
        f << link << ",";
      if (fSz!=32) {
        std::vector<int> listIds;
        bool hasCluster=false;
        if (m_structManager->readDataIdList(input, 1, listIds) && listIds[0]) {
          cluster.m_clusterIdsList.push_back(listIds[0]);
          f << "clusterId1=data" << listIds[0] << "A,";
          hasCluster=true;
        }
        val=(int) input->readLong(1);
        if ((hasCluster && val!=1) || (!hasCluster && val))
          f << "#hasCluster=" << val << ",";
        for (int i=0; i<2; ++i) { // always 0
          val=(int) input->readLong(2);
          if (val) f << "g" << i << "=" << val << ",";
        }
      }
    }
    else {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readFieldCluster: find unknown block\n"));
      f << "###unknown,";
    }

    if (input->tell()!=pos && input->tell()!=endSubDataPos)
      ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(endSubDataPos, librevenge::RVNG_SEEK_SET);

    int m=0;

    while (!input->isEnd()) {
      pos=input->tell();
      if (pos+4>endDataPos)
        break;
      RagTime5StructManager::Field field;
      if (!m_structManager->readField(input, endDataPos, ascFile, field)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        break;
      }
      f.str("");
      f << zoneName << "-" << n << "-B" << m++ << ":";
      if (!zone.m_hiLoEndian) f << "lohi,";

      if (n==0) {
        if (field.m_type==RagTime5StructManager::Field::T_LongList && field.m_fileType==0xce842) {
          f << "pos=[";
          for (size_t i=0; i<field.m_longList.size(); ++i)
            f << field.m_longList[i] << ",";
          f << "],";
          link.m_longList=field.m_longList;
          ascFile.addPos(pos);
          ascFile.addNote(f.str().c_str());
          continue;
        }
        if (field.m_type==RagTime5StructManager::Field::T_Unstructured && field.m_fileType==0xce017) {
          // pos find 2|4|8
          // def find f801|000f00
          f << "unkn="<<field.m_extra << ",";
          continue;
        }
      }
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readFieldCluster: find some unknown fields\n"));
      f << "###" << field;
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    if (!link.empty()) {
      if (n==0)
        cluster.m_dataLink=link;
      else
        cluster.m_linksList.push_back(link);
    }

    pos=input->tell();
    if (pos!=endDataPos) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readFieldCluster: find some extra data\n"));
      f.str("");
      f << zoneName << "-" << n << ":" << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
  }

  long pos=input->tell();
  if (pos!=endPos) {
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::readFieldCluster: find some extra data\n"));
    f.str("");
    f << zoneName << "-###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->setReadInverted(false);

  return true;
}

////////////////////////////////////////////////////////////
// unknown cluster A
////////////////////////////////////////////////////////////
bool RagTime5ZoneManager::readUnknownClusterA(RagTime5Zone &zone, Cluster &cluster)
{
  MWAWEntry &entry=zone.m_entry;
  if (entry.length()==0) return true;
  if (entry.length()<13) return false;

  MWAWInputStreamPtr input=zone.getInput();
  long endPos=entry.end();
  input->setReadInverted(!zone.m_hiLoEndian);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f.str("");
  f << "Entries(ClustUnkA)[" << zone << "]:";
  int val;
  for (int i=0; i<4; ++i) { // f0=f1=0, f2=1, f3=0|1|4
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  zone.m_isParsed=true;
  libmwaw::DebugFile &ascFile=zone.ascii();
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(endPos);
  ascFile.addNote("_");

  cluster.m_hiLoEndian=zone.m_hiLoEndian;
  cluster.m_type=Cluster::C_ClusterA;
  // normally 1 zone, zone0: link to data zone
  for (int n=0; n<2; ++n) {
    long pos=input->tell();
    if (pos>=endPos || input->isEnd()) break;
    long lVal;
    if (!m_structManager->readCompressedLong(input, endPos, lVal)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f.str("");
    f << "ClustUnkA-" << n << ":";
    f << "f0=" << lVal << ",";
    // always in big endian
    long sz;
    if (!m_structManager->readCompressedLong(input,endPos,sz) || sz <= 7 || input->tell()+sz>endPos) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f << "sz=" << sz << ",";
    long endDataPos=input->tell()+sz;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    pos=input->tell();
    f.str("");
    f << "ClustUnkA-" << n << "-A:";
    if (!zone.m_hiLoEndian) f << "lohi,";
    long fSz;
    if (!m_structManager->readCompressedLong(input, endDataPos, fSz) || fSz<6 || input->tell()+fSz>endDataPos) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterA: can not read item A\n"));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
      continue;
    }
    long debSubDataPos=input->tell();
    long endSubDataPos=debSubDataPos+fSz;
    int fl=(int) input->readULong(2);
    if (fl!=0x31)
      f << "fl=" << std::hex << fl << std::dec << ",";
    int N=(int) input->readLong(4);

    switch (N) {
    case -5: { // n=0
      if (fSz<12) {
        f << "###data,";
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterA: can not read the data id\n"));
        break;
      }
      for (int i=0; i<2; ++i) { // always 0?
        val=(int) input->readLong(2);
        if (val) f << "f" << i+1 << "=" << val << ",";
      }
      val=(int) input->readLong(2);
      f << "id=" << val << ",";
      if ((fSz!=76 && fSz!=110) || n!=0) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterA: find unknown block\n"));
        f << "###unknown,";
        break;
      }
      Link link;
      link.m_fileType[0]=(int) input->readULong(2);
      if ((link.m_fileType[0]&0xBCFF)!=4) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterA: the type seems odd\n"));
        f << "##fileType=" << std::hex << link.m_fileType[0] << std::dec << ",";
      }
      else if (link.m_fileType[0]!=0x204)
        f << "fileType=" << std::hex << link.m_fileType[0] << std::dec << ",";
      val=(int) input->readLong(2); // always 0?
      if (val) f << "f4=" << val << ",";
      for (int i=0; i<7; ++i) { // g1, g2, g3 small int other 0
        val=(int) input->readLong(4);
        if (i==2)
          link.m_N=val;
        else if (val) f << "g" << i << "=" << val << ",";
      }
      link.m_fileType[1]=(long) input->readULong(2);
      link.m_fieldSize=(int) input->readULong(2);

      f << "mainData,";
      std::vector<int> listIds;
      if (!m_structManager->readDataIdList(input, 2, listIds)) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterA: can not read the first list id\n"));
        f << "##listIds,";
        break;
      }
      if (listIds[0]) {
        link.m_ids.push_back(listIds[0]);
        cluster.m_dataLink=link;
        f << "data1=data" << listIds[0] << "A,";
      }
      cluster.m_clusterIdsList.push_back(listIds[1]);
      if (listIds[1])
        f << "clusterId1=data" << listIds[1] << "A,";
      unsigned long ulVal=input->readULong(4);
      if (ulVal) {
        f << "h0=" << (ulVal&0x7FFFFFFF);
        if (ulVal&0x80000000) f << "[h],";
        else f << ",";
      }
      val=(int) input->readLong(2); // always 1?
      if (val!=1) f << "h1=" << val << ",";
      listIds.clear();
      if (!m_structManager->readDataIdList(input, 2, listIds)) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterA: can not read the cluster list id\n"));
        f << "##listClusterIds,";
        break;
      }
      cluster.m_clusterIdsList.push_back(listIds[0]);
      if (listIds[0]) // find some 4104 cluster
        f << "cluster[nextId]=data" << listIds[0] << "A,";
      if (listIds[1]) { // find some 4001 cluster
        cluster.m_clusterIdsList.push_back(listIds[1]);
        f << "cluster[id]=data" << listIds[1] << "A,";
      }
      val=(int) input->readULong(2); // 2[08a][01]
      f << "fl=" << std::hex << val << std::dec << ",";
      for (int i=0; i<2; ++i) { // h2=0|4|a, h3=small number
        val=(int) input->readLong(2);
        if (val) f << "h" << i+2 << "=" << val << ",";
      }
      if (fSz==76) break;

      for (int i=0; i<7; ++i) { // g1, g2, g3 small int other 0
        val=(int) input->readLong(i==0 ? 2 : 4);
        if (i==2)
          link.m_N=val;
        else if (val) f << "g" << i << "=" << val << ",";
      }
      link.m_fileType[1]=(long) input->readULong(2);
      link.m_fieldSize=(int) input->readULong(2);

      listIds.clear();
      if (!m_structManager->readDataIdList(input, 1, listIds)) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterA: can not read the second list id\n"));
        f << "##listIds2,";
        break;
      }
      if (listIds[0]) {
        link.m_ids.clear();
        link.m_ids.push_back(listIds[0]);
        f << "data2=data" << listIds[0] << "A,";
        cluster.m_linksList.push_back(link);
      }
      break;
    }
    default:
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterA: find unexpected header\n"));
      f << "###type" << std::hex << N << std::dec;
      break;
    }
    if (input->tell()!=pos && input->tell()!=endSubDataPos)
      ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(endSubDataPos, librevenge::RVNG_SEEK_SET);

    int m=0;

    while (!input->isEnd()) {
      pos=input->tell();
      if (pos+4>endDataPos)
        break;
      RagTime5StructManager::Field field;
      if (!m_structManager->readField(input, endDataPos, ascFile, field)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        break;
      }
      f.str("");
      f << "ClustUnkA-" << n << "-B" << m++ << ":";
      if (!zone.m_hiLoEndian) f << "lohi,";
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterA: find some unknown fields\n"));
      f << "###" << field;
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }

    pos=input->tell();
    if (pos!=endDataPos) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterA: find some extra data\n"));
      f.str("");
      f << "ClustUnkA-" << n << ":" << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
  }

  long pos=input->tell();
  if (pos!=endPos) {
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterA: find some extra data\n"));
    f.str("");
    f << "ClustUnkA-###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->setReadInverted(false);

  return true;
}

////////////////////////////////////////////////////////////
// unknown cluster B ( first internal child of a root cluster )
////////////////////////////////////////////////////////////
bool RagTime5ZoneManager::readUnknownClusterB(RagTime5Zone &zone, Cluster &cluster)
{
  MWAWEntry &entry=zone.m_entry;
  if (entry.length()==0) return true;
  if (entry.length()<13) return false;

  MWAWInputStreamPtr input=zone.getInput();
  long endPos=entry.end();
  input->setReadInverted(!zone.m_hiLoEndian);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(ClustUnkB)[" << zone << "]:";
  int val;
  for (int i=0; i<4; ++i) { // f0=f1=0, f2=1, f3=0|1|4
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  zone.m_isParsed=true;
  libmwaw::DebugFile &ascFile=zone.ascii();
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(endPos);
  ascFile.addNote("_");

  cluster.m_hiLoEndian=zone.m_hiLoEndian;
  cluster.m_type=Cluster::C_ClusterB;
  // normally 1 zone
  for (int n=0; n< 2; ++n) {
    long pos=input->tell();
    if (pos>=endPos || input->isEnd()) break;
    long lVal;
    if (!m_structManager->readCompressedLong(input, endPos, lVal)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f.str("");
    f << "ClustUnkB-" << n << ":";
    f << "f0=" << lVal << ","; // 13|16|31|32|47...
    // always in big endian
    long sz;
    if (!m_structManager->readCompressedLong(input,endPos,sz) || sz <= 7 || input->tell()+sz>endPos) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f << "sz=" << sz << ",";
    long endDataPos=input->tell()+sz;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    pos=input->tell();
    f.str("");
    f << "ClustUnkB-" << n << "-A:";
    if (!zone.m_hiLoEndian) f << "lohi,";
    long fSz;
    if (!m_structManager->readCompressedLong(input, endDataPos, fSz) || fSz<6 || input->tell()+fSz>endDataPos) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterB: can not read item A\n"));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
      continue;
    }
    long debSubDataPos=input->tell();
    long endSubDataPos=debSubDataPos+fSz;
    int fl=(int) input->readULong(2);
    if ((n==0 && fl!=0x30) || (n==1 && fl!=0x10))
      f << "fl=" << std::hex << fl << std::dec << ",";
    int N=(int) input->readLong(4);

    Link link;
    if (N<=0) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterB: find unexpected header\n"));
      f << "###type" << std::hex << N << std::dec;
    }
    else if (n==0 && fSz==32) {
      for (int i=0; i<8; ++i) { // find g0=3c|60 when type==0x30002, other 0
        val=(int) input->readLong(2);
        if (val) f << "f" << i << "=" << val << ",";
      }
      link.m_type=Link::L_List;
      link.m_fileType[1]=(long) input->readULong(2);
      if (link.m_fileType[1]!=0x4030 && link.m_fileType[1]!=0x4038)
        f << "#fileType=" << std::hex << link.m_fileType[1] << std::dec << ",";
      if (!m_structManager->readDataIdList(input, 2, link.m_ids) || !link.m_ids[1]) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterB: can not read the data pos\n"));
        f << "###noData,";
        link=Link();
      }
      else
        f << link << ",";
    }
    else {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterB: find unknown block\n"));
      f << "###unknown,";
    }

    if (input->tell()!=pos && input->tell()!=endSubDataPos)
      ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(endSubDataPos, librevenge::RVNG_SEEK_SET);

    int m=0;

    while (!input->isEnd()) {
      pos=input->tell();
      if (pos+4>endDataPos)
        break;
      RagTime5StructManager::Field field;
      if (!m_structManager->readField(input, endDataPos, ascFile, field)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        break;
      }
      f.str("");
      f << "ClustUnkB-" << n << "-B" << m++ << ":";
      if (!zone.m_hiLoEndian) f << "lohi,";

      if (n==0) {
        if (field.m_type==RagTime5StructManager::Field::T_LongList && field.m_fileType==0xce842) {
          f << "pos=[";
          for (size_t i=0; i<field.m_longList.size(); ++i)
            f << field.m_longList[i] << ",";
          f << "],";
          link.m_longList=field.m_longList;
          ascFile.addPos(pos);
          ascFile.addNote(f.str().c_str());
          continue;
        }
        if (field.m_type==RagTime5StructManager::Field::T_Unstructured && field.m_fileType==0xce017) {
          // find 2
          f << "unkn="<<field.m_extra << ",";
          continue;
        }
      }
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterB: find some unknown fields\n"));
      f << "###" << field;
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    if (!link.empty()) {
      if (n==0)
        cluster.m_dataLink=link;
      else
        cluster.m_linksList.push_back(link);
    }

    pos=input->tell();
    if (pos!=endDataPos) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterB: find some extra data\n"));
      f.str("");
      f << "ClustUnkB-" << n << ":" << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
  }

  long pos=input->tell();
  if (pos!=endPos) {
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterB: find some extra data\n"));
    f.str("");
    f << "ClustUnkB-###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->setReadInverted(false);

  return true;
}

////////////////////////////////////////////////////////////
// unknown cluster C
////////////////////////////////////////////////////////////
bool RagTime5ZoneManager::readUnknownClusterC(RagTime5Zone &zone, Cluster &cluster, int type)
{
  MWAWEntry &entry=zone.m_entry;
  if (entry.length()==0) return true;
  if (entry.length()<13) return false;

  MWAWInputStreamPtr input=zone.getInput();
  long endPos=entry.end();
  input->setReadInverted(!zone.m_hiLoEndian);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  std::string zoneName;
  switch (type) {
  case 0x30000:
    zoneName="ClustUnkC_A";
    break;
  case 0x30001:
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterC: find zone ClustUnkC_B\n"));
    zoneName="ClustUnkC_B";
    break;
  case 0x30002:
    zoneName="ClustUnkC_C";
    break;
  case 0x30003:
    zoneName="ClustUnkC_D";
    break;
  default:
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterC: find unexpected type\n"));
    zoneName="ClustUnkC_BAD";
    break;
  }
  f.str("");
  f << "Entries(" << zoneName << ")[" << zone << "]:";
  int val;
  for (int i=0; i<4; ++i) { // f0=f1=0, f2=1, f3=0|1|3|4
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  zone.m_isParsed=true;
  libmwaw::DebugFile &ascFile=zone.ascii();
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(endPos);
  ascFile.addNote("_");

  cluster.m_hiLoEndian=zone.m_hiLoEndian;
  cluster.m_type=Cluster::C_ClusterC;
  // normally 1 zone for ClustUnkC_A and ClustUnkC_D, 2 zone for ClustUnkC_C, ??? zone for ClustUnkC_B
  for (int n=0; n<(type==0x30002) ? 3 : 2; ++n) {
    long pos=input->tell();
    if (pos>=endPos || input->isEnd()) break;
    long lVal;
    if (!m_structManager->readCompressedLong(input, endPos, lVal)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f.str("");
    f << zoneName << "-" << n << ":";
    f << "f0=" << lVal << ","; // 59, 7b...
    // always in big endian
    long sz;
    if (!m_structManager->readCompressedLong(input,endPos,sz) || sz <= 7 || input->tell()+sz>endPos) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f << "sz=" << sz << ",";
    long endDataPos=input->tell()+sz;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    pos=input->tell();
    f.str("");
    f << zoneName << "-" << n << "-A:";
    if (!zone.m_hiLoEndian) f << "lohi,";
    long fSz;
    if (!m_structManager->readCompressedLong(input, endDataPos, fSz) || fSz<6 || input->tell()+fSz>endDataPos) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterC: can not read item A\n"));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
      continue;
    }
    long debSubDataPos=input->tell();
    long endSubDataPos=debSubDataPos+fSz;
    int fl=(int) input->readULong(2);
    if ((n==0 && fl!=0x30) || (n==1 && fl!=0x10))
      f << "fl=" << std::hex << fl << std::dec << ",";
    int N=(int) input->readLong(4);

    Link link;
    link.m_fileType[0]=(type-0x30000);
    if (N<=0) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterC: find unexpected header\n"));
      f << "###type" << std::hex << N << std::dec;
    }
    else if (n==0 && ((fSz==40 && type==0x30002) || (fSz==32 && type==0x30003))) {
      for (int i=0; i<8; ++i) { // find g0=3c|60 when type==0x30002, other 0
        val=(int) input->readLong(2);
        if (val) f << "f" << i << "=" << val << ",";
      }
      link.m_type=Link::L_List;
      link.m_fileType[1]=(long) input->readULong(2);
      if ((type==0x30002 && link.m_fileType[1]!=0x8030) || (type==0x30003 && link.m_fileType[1]!=0x330))
        f << "#fileType=" << std::hex << link.m_fileType[1] << std::dec << ",";
      if (!m_structManager->readDataIdList(input, 2, link.m_ids) || !link.m_ids[1]) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterC: can not read the data pos\n"));
        f << "###noData,";
        link=Link();
      }
      else
        f << link << ",";
      if (fSz==40) {
        for (int i=0; i<2; ++i) { // find g0=2, g1=0
          val=(int) input->readLong(4);
          if (val) f << "g" << i << "=" << val << ",";
        }
      }
    }
    else if ((n==0 && fSz==34 && type==0x30000) || (fSz==30 && type==0x30002 && n>0)) {
      for (int i=0; i<8; ++i) { // g1=54, other 0?
        val=(int) input->readLong(2);
        if (val) f << "f" << i << "=" << val << ",";
      }
      link.m_N=N;
      link.m_fileType[1]=(long) input->readULong(2);
      if (link.m_fileType[1]!=0x50)
        f << "#fileType=" << std::hex << link.m_fileType[1] << ",";
      link.m_fieldSize=(int) input->readULong(2);
      if (!m_structManager->readDataIdList(input, 1, link.m_ids) || !link.m_ids[0]) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterC: can not read the data pos\n"));
        f << "###noData,";
        link=Link();
      }
      else
        f << link << ",";
      if (fSz==34) {
        for (int i=0; i<2; ++i) { // find 0
          val=(int) input->readLong(2);
          if (val) f << "g" << i << "=" << val << ",";
        }
      }
    }
    else {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterC: find unknown block\n"));
      f << "###unknown,";
    }

    if (input->tell()!=pos && input->tell()!=endSubDataPos)
      ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(endSubDataPos, librevenge::RVNG_SEEK_SET);

    int m=0;

    while (!input->isEnd()) {
      pos=input->tell();
      if (pos+4>endDataPos)
        break;
      RagTime5StructManager::Field field;
      if (!m_structManager->readField(input, endDataPos, ascFile, field)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        break;
      }
      f.str("");
      f << zoneName << "-" << n << "-B" << m++ << ":";
      if (!zone.m_hiLoEndian) f << "lohi,";

      if (n==0) {
        if ((type==0x30002 || type==0x30003) && field.m_type==RagTime5StructManager::Field::T_LongList && field.m_fileType==0xce842) {
          f << "pos=[";
          for (size_t i=0; i<field.m_longList.size(); ++i)
            f << field.m_longList[i] << ",";
          f << "],";
          link.m_longList=field.m_longList;
          ascFile.addPos(pos);
          ascFile.addNote(f.str().c_str());
          continue;
        }
      }
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterC: find some unknown fields\n"));
      f << "###" << field;
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    if (!link.empty()) {
      if (n==0)
        cluster.m_dataLink=link;
      else
        cluster.m_linksList.push_back(link);
    }

    pos=input->tell();
    if (pos!=endDataPos) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterC: find some extra data\n"));
      f.str("");
      f << zoneName << "-" << n << ":" << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
  }

  long pos=input->tell();
  if (pos!=endPos) {
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterC: find some extra data\n"));
    f.str("");
    f << zoneName << "-###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->setReadInverted(false);

  return true;
}

////////////////////////////////////////////////////////////
// root cluster
////////////////////////////////////////////////////////////
bool RagTime5ZoneManager::readRootCluster(RagTime5Zone &zone, RagTime5ZoneManager::ClusterRoot &cluster)
{
  MWAWEntry &entry=zone.m_entry;
  if (entry.length()==0) return true;
  if (entry.length()<13) return false;

  MWAWInputStreamPtr input=zone.getInput();
  long endPos=entry.end();
  input->setReadInverted(!zone.m_hiLoEndian);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  f << "Entries(ClustRoot)[" << zone << "]:";
  int val;
  for (int i=0; i<4; ++i) { // f0=f1=0, f2=1, f3=small number
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  zone.m_isParsed=true;
  libmwaw::DebugFile &ascFile=zone.ascii();
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(endPos);
  ascFile.addNote("_");

  cluster.m_type=Cluster::C_Root;
  cluster.m_hiLoEndian=zone.m_hiLoEndian;
  int expectedN=-1, n=0;
  while (!input->isEnd()) {
    long pos=input->tell();
    if (pos>=endPos) break;
    ++expectedN;
    /* expected:
       n=0: N=-2, fSz=215 | 220 : main data ( with auxilliar col/pattern id + int + a potential bool zone?)
       n=1: fSz=32, zones' name, ie. unicode list
       n=2: fSz=28, zones' long list, unknown
       n=3: fSz=30, zones' data ids
       n=4: fSz=38, unknown zone with 3 long
       n=5/6: fSz=32, unknown data id to struct zone? note: field5 is optional
       n=7: fileName

       after n=8, no more ordering? : find at most fSz=26(1), fSz=28(2), fSz=30(2), fSz=32(3), fSz=52(1), fSz=78(1)
    */
    long lVal;
    if (!RagTime5StructManager::readCompressedLong(input, endPos, lVal)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f.str("");
    f << "ClustRoot-" << n << "[exp" << expectedN << "]:";
    f << "f0=" << lVal << ",";
    // always in big endian
    long sz;
    if (!RagTime5StructManager::readCompressedLong(input,endPos,sz) || sz <= 7 || input->tell()+sz>endPos) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f << "sz=" << sz << ",";
    long endDataPos=input->tell()+sz;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    pos=input->tell();
    f.str("");
    f << "ClustRoot-" << n << "-A[exp" << expectedN << "]:";;
    if (!zone.m_hiLoEndian) f << "lohi,";
    long fSz;
    if (!RagTime5StructManager::readCompressedLong(input, endDataPos, fSz) || fSz<6 || input->tell()+fSz>endDataPos) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: can not read item A\n"));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      ++n;
      input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
      continue;
    }
    long debSubDataPos=input->tell();
    long endSubDataPos=debSubDataPos+fSz;
    int fl=(int) input->readULong(2); // [01][13][0139b]
    f << "fl=" << std::hex << fl << std::dec << ",";
    int N=(int) input->readLong(4);
    std::stringstream s;
    s << "type" << std::hex << N << std::dec;

    enum What { W_Unknown, W_FileName,
                W_ListZoneNames, W_ListLink,
                W_ListLong,
                W_GraphTypes,
                W_MainZone
              };
    What what=W_Unknown;
    std::string name=s.str();
    Link link;
    int linkItemId=-1, linkOtherId=-1;
    if ((zone.m_hiLoEndian && N==int(0x80000000)) || (!zone.m_hiLoEndian && N==0x8000)) {
      if (expectedN<5 || expectedN>7)  {
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: expected N seems bad\n"));
        f << "#expectedN=" << expectedN << ",";
      }
      if (expectedN<7)
        expectedN=7;
      name="filename";
      what=W_FileName;
    }
    switch (N) {
    case -2: {
      name="head2";
      what=W_MainZone;
      if ((fSz!=215 && fSz!=220) || expectedN!=0) {
        f << "###sz,";
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: can not read the head2 size seems bad\n"));
        break;
      }
      val=(int) input->readLong(4); // 8|9|a
      f << "f1=" << val << ",";
      for (int i=0; i<4; ++i) { // f2=0-7, f3=1|3
        val=(int) input->readLong(2);
        if (val) f << "f" << i+2 << "=" << val << ",";
      }
      val=(int) input->readLong(4); // 7|8
      f << "N1=" << val << ",";
      std::vector<int> listIds;
      long actPos=input->tell();
      if (!m_structManager->readDataIdList(input, 1, listIds) || !listIds[0]) {
        f << "###cluster[child],";
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster[head2]: can not find the cluster's child\n"));
        input->seek(actPos+4, librevenge::RVNG_SEEK_SET);
      }
      else { // link to unknown cluster zone
        cluster.m_clusterIds[0]=listIds[0];
        f << "unknClustB=data" << listIds[0] << "A,";
      }
      for (int i=0; i<21; ++i) { // always g0=g11=g18=16, other 0 ?
        val=(int) input->readLong(2);
        if (val) f << "g" << i << "=" << val << ",";
      }
      val=(int) input->readULong(4);
      if (val!=0x3c052)
        f << "#fileType=" << std::hex << val << std::dec << ",";
      for (int i=0; i<9; ++i) { // always h6=6
        val=(int) input->readLong(2);
        if (val) f << "h" << i << "=" << val << ",";
      }
      for (int i=0; i<3; ++i) { // can be 1,11,10
        val=(int) input->readULong(1);
        if (val)
          f << "fl" << i << "=" << std::hex << val << std::dec << ",";
      }
      if (fSz==220) {
        for (int i=0; i<2; ++i) { // h10=1, h11=16
          val=(int) input->readLong(2);
          if (val) f << "h" << i+9 << "=" << val << ",";
        }
        val=(int) input->readLong(1);
        if (val) f << "h11=" << val << ",";
      }
      val=(int) input->readLong(4); // e-5a
      if (val) f << "N2=" << val << ",";
      for (int i=0; i<9; ++i) { // j8=18
        val=(int) input->readLong(2);
        if (val) f << "j" << i << "=" << val << ",";
      }
      for (int i=0; i<3; ++i) {
        val=(int) input->readLong(4);
        if (val!=i+2)
          f << "j" << 9+i << "=" << val << ",";
      }
      actPos=input->tell();
      listIds.clear();
      if (!m_structManager->readDataIdList(input, 4, listIds)) {
        f << "###style[child],";
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster[head2]: can not find the style's child\n"));
        input->seek(actPos+16, librevenge::RVNG_SEEK_SET);
      }
      else {
        for (size_t i=0; i<4; ++i) {
          if (listIds[i]==0) continue;
          cluster.m_styleClusterIds[i]=listIds[i];
          static char const *(wh[])= { "graph", "units", "units2", "text" };
          f << wh[i] << "Style=data" << listIds[i] << "A,";
        }
      }
      val=(int) input->readLong(4); // always 5?
      if (val!=80) f << "N3=" << val << ",";
      actPos=input->tell();
      listIds.clear();
      if (!m_structManager->readDataIdList(input, 3, listIds)) {
        f << "###style[child],";
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster[head2]: can not find the style2's child\n"));
        input->seek(actPos+12, librevenge::RVNG_SEEK_SET);
      }
      else {
        for (size_t i=0; i<3; ++i) {
          if (listIds[i]==0) continue;
          cluster.m_styleClusterIds[i+4]=listIds[i];
          static char const *(wh[])= { "format", "#unk", "graphColor" };
          f << wh[i] << "Style=data" << listIds[i] << "A,";
        }
      }
      for (int i=0; i<9; ++i) { // k6=0|6, k7=0|7
        val=(int) input->readULong(4); // maybe some dim
        static int const(expected[])= {0xc000, 0x2665, 0xc000, 0x2665, 0xc000, 0xc000, 0, 0, 0};
        if (val!=expected[i])
          f << "k" << i << "=" << std::hex << val << std::dec << ",";
      }
      for (int i=0; i<2; ++i) { // l0=0|1|2, l1=0|1
        val=(int) input->readLong(2); // 0|1|2
        if (val)
          f << "l" << i <<"=" << val << ",";
      }
      // a very big number
      f << "ID=" << std::hex << input->readULong(4) << std::dec << ",";
      for (int i=0; i<6; ++i) { // always 0
        val=(int) input->readLong(2); // 0|1|2
        if (val)
          f << "l" << i+2 <<"=" << val << ",";
      }
      break;
    }
    default: {
      if (what==W_FileName) break;
      if (N<0) {
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: find unexpected data\n"));
        f << "###N=" << N << ",";
        break;
      }
      if (what!=W_Unknown) break;

      if (fSz==26) { // in general block8 or 9 no auxiallary data
        if (expectedN<8)
          f << "##expectedN=" << expectedN << ",";
        name="block1a";
        val=(int) input->readULong(4); // small number or 0
        if (val) f << "N2=" << val << ",";
        link.m_fileType[0]=(int) input->readULong(4);
        if (link.m_fileType[0]!=0x14b4042) {
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: unexpected type for block1a\n"));
          f << "##fileType=" << std::hex << link.m_fileType[0] << std::dec << ",";
        }
        for (int i=0; i<6; ++i) { // f4=c
          val=(int) input->readLong(2);
          if (val) f << "f" << i << "=" << val << ",";
        }
        break;
      }
      // expectedN==2 or near 9 or 10...
      if (fSz==28) {
        link.m_fileType[0]=(long) input->readULong(4);
        if (link.m_fileType[0]!=0x35800 && link.m_fileType[0]!=0x3e800) {
          what=W_Unknown;
          break;
        }
        what=W_ListLong;
        link.m_N=N;
        link.m_fieldSize=4;
        link.m_type=Link::L_LongList;
        link.m_name="LongListRoot";
        if (expectedN<=2 && link.m_fileType[0]!=0x35800) {
          if (expectedN!=2) {
            MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: unexpected expectedN for list[long]\n"));
            f << "##expectedN=" << expectedN << ",";
            expectedN=2;
          }
          f << "zone[longs],";
          name="zone:longs";
        }
        else if (link.m_fileType[0]==0x35800) {
          f << "list[long0],";
          name="list:longs0";
        }
        else {
          f << "list[long1],";
          name="list:longs1";
        }
        for (int i=0; i<7; ++i) { // always 0
          val=(int) input->readLong(2);
          if (val) f << "f" << i << "=" << val << ",";
        }
        if (link.m_fileType[1]!=0 && link.m_fileType[1]!=0x20) {
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: unexpected type1 for list[long]\n"));
          f << "##fileType1=" << std::hex << link.m_fileType[1] << std::dec << ",";
        }
        if (!m_structManager->readDataIdList(input, 1, link.m_ids)) {
          f << "###noData," << link;
          link=Link();
        }
        else if (link.m_ids[0])
          f << link << ",";
        break;
      }

      // field3 and other near 10
      if (fSz==30) {
        val=(int) input->readULong(4);
        if (val) f << "N2=" << val << ",";
        link.m_fileType[0]=(int) input->readULong(4);
        if (link.m_fileType[0]==0) {
          for (int i=0; i<4; ++i) { // always 0
            val=(int) input->readLong(2);
            if (val) f << "f" << i << "=" << val << ",";
          }

          link.m_fileType[1]=(int) input->readULong(2);
          link.m_fieldSize=(int) input->readULong(2);
          link.m_N=N;
          if (!m_structManager->readDataIdList(input, 1, link.m_ids)) {
            MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: can not read cluster for size 1e\n"));
            f << "##noData,";
          }
          else if (!link.m_ids[0])
            f << "##noCluster,";
          else if (expectedN<=3) { // no auxilliar data expected
            if (expectedN!=3) {
              MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: bad expected N for cluster list id\n"));
              f << "##expectedN=" << expectedN << ",";
              expectedN=3;
            }
            if (link.m_fileType[1]!=0x40 || link.m_fieldSize!=8) {
              MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: find odd definition for cluster list id\n"));
              f << "##[" << std::hex << link.m_fileType[1] << std::dec << ":" << link.m_fieldSize << "],";
            }
            cluster.m_listClusterId=link.m_ids[0];
            name="clusterList";
            link=Link();
          }
          else  { // no auxilliar data expected
            if (link.m_fileType[1]!=0 || link.m_fieldSize!=4) {
              MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: find odd definition for cluster id and fSz=1e\n"));
              f << "##[" << std::hex << link.m_fileType[1] << std::dec << ":" << link.m_fieldSize << "],";
            }
            f << "unknDataC,";
            name="unknDataC";
          }
          break;
        }

        if (link.m_fileType[0]==0x15e5042) {
          // first near n=9, second near n=15 with no other data
          // no auxilliar data expected
          name="unknDataD";
          for (int i=0; i<4; ++i) { // f0, f3: small number
            val=(int) input->readULong(4);
            if (val) f << "f" << i << "=" << val << ",";
          }
          break;
        }

        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: unexpected type for structure with size 1e\n"));
        f << "##fileType=" << std::hex << link.m_fileType[0] << std::dec << ",";
        break;
      }
      // expectedN=1|(5|6)| after 8
      if (fSz==32 && (expectedN==1 || expectedN>=4)) {
        val=(int) input->readLong(4); // 0 for n=1, small value for n=5|6
        if (expectedN==1 && val)
          f << "f0=" << val << ",";
        link.m_fileType[0]=(long) input->readULong(4);
        if (expectedN==1) {
          link.m_type=Link::L_List;
          f << "zone[names],";
          name="zone:names";
          what=W_ListZoneNames;
          if (link.m_fileType[0]!=0x7d01a) {
            MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: unexpected type for zone[name]\n"));
            f << "##fileType=" << std::hex << link.m_fileType[0] << std::dec << ",";
          }
        }
        else {
          if (link.m_fileType[0]) {
            MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: unexpected type with fSz=32\n"));
            f << "##fileType=" << std::hex << link.m_fileType[0] << std::dec << ",";
          }
          if (val==0x47040)
            link.m_fileType[0]=val;
          else if (val)
            f << "f0=" << val << ",";
        }
        for (int i=0; i<4; ++i) { // always 0
          val=(int) input->readLong(2);
          if (val) f << "f" << i << "=" << val << ",";
        }
        link.m_N=N;
        link.m_fileType[1]=(long) input->readULong(2);
        if (expectedN==1) {
          if (link.m_fileType[1]!=0x220 && link.m_fileType[1]!=0x200) {
            MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: unexpected type1 for zone[name]\n"));
            f << "##fileType1=" << std::hex << link.m_fileType[1] << std::dec << ",";
          }
        }
        else if ((link.m_fileType[1]&0xFFD7)==0x8010) {
          what=W_ListLink;
          link.m_type=Link::L_List;
          linkOtherId=0;
          expectedN=5;
          f << "field5,";
          link.m_name=name="docInfo";
        }
        else if ((link.m_fileType[1]&0xFFD7)==0xc010) {
          what=W_ListLink;
          link.m_type=Link::L_List;
          linkOtherId=1;
          expectedN=6;
          f << "field6,";
          link.m_name=name="unknZoneC";
        }
        else if (link.m_fileType[0]==0x47040 && (link.m_fileType[1]&0xFFD7)==0) {
          what=W_ListLink;
          link.m_type=Link::L_List;
          linkItemId=1;
          link.m_name=name="settings";
        }
        else if (link.m_fileType[0]==0 && (link.m_fileType[1]&0xFFD7)==0x10) {
          what=W_ListLink;
          link.m_type=Link::L_List;
          linkItemId=0;
          link.m_name=name="condFormula";
        }
        else if (link.m_fileType[0]==0 && (link.m_fileType[1]&0xFFD7)==0x310) {
          // TODO: check what is it: an unicode list ?
          what=W_ListLink;
          f << "unknDataE,";
          name="unknDataE";
          link.m_type=Link::L_List;
        }
        else {
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: unexpected type1 for zone with fSz=32\n"));
          f << "##fileType1=" << std::hex << link.m_fileType[0] << std::dec << ",";
        }
        if (!m_structManager->readDataIdList(input, 2, link.m_ids) || !link.m_ids[1]) {
          f << "###noData," << link;
          link=Link();
        }
        break;
      }
      // n==4
      if (fSz==38) { // no auxilliary data
        link.m_fileType[0]=(int) input->readULong(4);
        if (expectedN>4 || link.m_fileType[0]!=0x47040) {
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: unexpected data of size 38\n"));
          f << "##fileType=" << std::hex << link.m_fileType[0] << std::dec << ",";
          break;
        }
        if (expectedN!=4) {
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: bad expected N for field 4\n"));
          f << "##expectedN=" << expectedN << ",";
          expectedN=4;
        }
        name="field4";
        for (int i=0; i<6; ++i) { // always 0
          val=(int) input->readLong(2);
          if (val) f << "f" << i << "=" << val << ",";
        }

        link.m_fileType[1]=(int) input->readULong(2);
        if (link.m_fileType[1]!=0x10 && link.m_fileType[1]!=0x18) {
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: unexpected fileType1 for field 4\n"));
          f << "##fileType1=" << std::hex << link.m_fileType[0] << std::dec << ",";
        }
        f << "val=[";
        for (int i=0; i<3; ++i) { // small int, often with f1=f0+1, f2=f1+1
          val=(int) input->readULong(4);
          if (val)
            f << val << ",";
          else
            f << "_,";
        }
        f << "],";
        val=(int) input->readULong(2); // always 0?
        if (val) f << "f0=" << val << ",";
        break;
      }

      if (fSz==52) {
        f << "graphTypes,";
        name="graphTypes";
        what=W_GraphTypes;
        if (N!=1) f << "##N=" << N << ",";
        for (int i=0; i<2; ++i) { // always 0 and small val
          val=(int) input->readLong(2);
          if (val) f << "f" << i+1 << "=" << val << ",";
        }
        link.m_fileType[0]=(int) input->readLong(4);
        if (link.m_fileType[0]!=0x14e6042) {
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster[graphType]: find unexpected fileType\n"));
          f << "###fileType=" << std::hex << link.m_fileType[0] << std::dec << ",";
        }
        for (int i=0; i<14; ++i) { // g1=0-2, g2=10[size?], g4=1-8[N], g13=30
          val=(int) input->readLong(2);
          if (val) f << "g" << i << "=" << val << ",";
        }
        if (m_structManager->readDataIdList(input, 2, link.m_ids) && link.m_ids[1]) {
          link.m_fileType[1]=0x30;
          link.m_fieldSize=16;
        }
        else
          link.m_ids.push_back(0);
        val=(int) input->readLong(2);
        if (val) // small number
          f << "h0=" << val << ",";
        break;
      }
      if (fSz==0x4e) {
        name="fieldLists";
        if (N!=1) f << "##N=" << N << ",";
        val=(int) input->readULong(4);
        if (val) f << "id=" << val << ",";
        link.m_fileType[0]=(long) input->readULong(4);
        if (link.m_fileType[0]!=0x154a042) {
          f << "###";
          break;
        }
        for (int i=0; i<2; ++i) { // always 0
          val=(int) input->readULong(2);
          if (val)
            f << "f" << i << "=" << val << ",";
        }
        std::vector<int> listIds;
        if (!m_structManager->readDataIdList(input, 2, listIds)) {
          f << "###";
          break;
        }
        if (listIds[0] || listIds[1]) { // fielddef and fieldpos
          Link fieldLink;
          fieldLink.m_type=Link::L_ClusterLink;
          fieldLink.m_ids=listIds;
          cluster.m_fieldClusterLink=fieldLink;
          f << "fields," << fieldLink << ",";
        }
        val=(int) input->readULong(4);
        if (val) f << "id2=" << val << ",";
        for (int i=0; i<4; ++i) { // always 0
          val=(int) input->readULong(2);
          if (val)
            f << "f" << i+2 << "=" << val << ",";
        }
        for (int i=0; i<2; ++i) { // always 1,0
          val=(int) input->readULong(1);
          if (val!=1-i)
            f << "fl" << i << "=" << val << ",";
        }
        val=(int) input->readLong(2);
        if (val!=100)
          f << "f6=" << val << ",";
        f << "ID=[";
        for (int i=0; i<4; ++i) { // very big number
          val=(int) input->readULong(4);
          if (val)
            f << std::hex << val << std::dec << ",";
          else
            f << "_,";
        }
        f << "],";
        listIds.clear();
        if (!m_structManager->readDataIdList(input, 4, listIds)) {
          f << "###";
          break;
        }
        if (listIds[0] || listIds[1] || listIds[2] || listIds[3]) {
          Link fieldLink;
          fieldLink.m_type=Link::L_UnknownClusterC;
          fieldLink.m_ids=listIds;
          cluster.m_linksList.push_back(fieldLink);
          f << fieldLink << ",";
        }

        val=(int) input->readULong(4);
        if (val) f << "id3=" << val << ",";
        break;
      }
      f << "##unparsed,";
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: find an unparsed zone\n"));
      break;
    }
    }
    if (link.empty())
      f << name << "-" << lVal << ",";
    else
      f << link << ",";
    if (input->tell()!=pos && input->tell()!=endSubDataPos)
      ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(endSubDataPos, librevenge::RVNG_SEEK_SET);

    int m=0;

    while (!input->isEnd()) {
      pos=input->tell();
      if (pos+4>endDataPos)
        break;
      RagTime5StructManager::Field field;
      if (!m_structManager->readField(input, endDataPos, ascFile, field)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        break;
      }
      f.str("");
      f << "ClustRoot-" << n << "-B" << m++ << "[" << name << "]:";
      if (!zone.m_hiLoEndian) f << "lohi,";
      bool done=false;
      switch (what) {
      case W_FileName: // expectedN=7
        if (field.m_type==RagTime5StructManager::Field::T_Unicode && field.m_fileType==0xc8042) {
          cluster.m_fileName=field.m_string.cstr();
          f << field.m_string.cstr();
          done=true;
          break;
        }
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: find unexpected filename field\n"));
        f << "##[" << field << "]";
        break;
      case W_ListZoneNames: // expectedN=1, find with ce842
      case W_ListLink: // expectedN=5|6, with ce842
      case W_ListLong: {  // expectedN=2 + other
        if (field.m_type==RagTime5StructManager::Field::T_LongList && field.m_fileType==0xce842) {
          f << "pos=[";
          for (size_t i=0; i<field.m_longList.size(); ++i) {
            if (field.m_longList[i]==0)
              f << "_,";
            else if (field.m_longList[i]>1000)
              f << std::hex << field.m_longList[i] << std::dec << ",";
            else
              f << field.m_longList[i] << ",";
          }
          f << "],";
          link.m_longList=field.m_longList;
          done=true;
          break;
        }
        if (field.m_type==RagTime5StructManager::Field::T_Unstructured && field.m_fileType==0xce017) {
          // a small value 2|4|a|1c|40
          f << "unkn="<<field.m_extra << ",";
          done=true;
          break;
        }
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: find unexpected list link field\n"));
        f << "##[" << field << "]";
        break;
      }
      case W_GraphTypes: {
        if (field.m_type==RagTime5StructManager::Field::T_FieldList && field.m_fileType==0x14eb015) {
          f << "decal=[";
          for (size_t i=0; i<field.m_fieldList.size(); ++i) {
            RagTime5StructManager::Field const &child=field.m_fieldList[i];
            if (child.m_type==RagTime5StructManager::Field::T_LongList && child.m_fileType==0xce842) {
              for (size_t j=0; j<child.m_longList.size(); ++j) {
                if (child.m_longList[j]==0)
                  f << "_,";
                else if (child.m_longList[j]>1000)
                  f << std::hex << child.m_longList[j] << std::dec << ",";
                else
                  f << child.m_longList[j] << ",";
              }
              link.m_longList=child.m_longList;
              continue;
            }
            MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: find unexpected decal child[graphType]\n"));
            f << "#[" << child << "],";
          }
          f << "],";
          done=true;
          break;
        }
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: find unexpected child[graphType]\n"));
        f << "##[" << field << "]";
        break;
      }
      case W_MainZone: // expectedN=0
        if (field.m_type==RagTime5StructManager::Field::T_ZoneId && field.m_fileType==0x14510b7) {
          if (field.m_longValue[0]) {
            Link fieldLink;
            cluster.m_styleClusterIds[7]=(int) field.m_longValue[0];
            f << "col/pattern[id]=dataA" << field.m_longValue[0] << ",";
          }
          done=true;
          break;
        }
        if (field.m_type==RagTime5StructManager::Field::T_Long && field.m_fileType==0x3c057) {
          // small number between 8 and 10
          if (field.m_longValue[0])
            f << "unkn0=" << field.m_longValue[0] << ",";
          done=true;
          break;
        }
        if (field.m_type==RagTime5StructManager::Field::T_FieldList && field.m_fileType==0x1451025) {
          f << "decal=[";
          for (size_t i=0; i<field.m_fieldList.size(); ++i) {
            RagTime5StructManager::Field const &child=field.m_fieldList[i];
            if (child.m_type==RagTime5StructManager::Field::T_Unstructured && child.m_fileType==0xce017) {
              // can be very long, seems to contain more 0 than 1
              f << "unkn1="<<child.m_extra << ",";
              continue;
            }
            MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: find unexpected decal child[graphType]\n"));
            f << "##[" << child << "],";
          }
          f << "],";
          done=true;
          break;
        }
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: find unexpected child[main]\n"));
        f << "##[" << field << "]";
        break;
      // expectedN=3|4,
      case W_Unknown:
      default:
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: find unexpected child[unknown]\n"));
        f << "##[" << field << "]";
        break;
      }
      if (done) {
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        continue;
      }
      f << field;
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    if (!link.empty()) {
      if (n==0) {
        if (cluster.m_dataLink.empty())
          cluster.m_dataLink=link;
        else {
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: oops the main link is already set\n"));
          cluster.m_linksList.push_back(link);
        }
      }
      else if (what==W_GraphTypes)
        cluster.m_graphicTypeLink=link;
      else if (what==W_ListZoneNames)
        cluster.m_listClusterName=link;
      else if (what==W_ListLong && n==2)
        cluster.m_listClusterUnkn=link;
      else if (link.m_type==Link::L_List && linkItemId==0)
        cluster.m_conditionFormulaLinks.push_back(link);
      else if (link.m_type==Link::L_List && linkItemId==1)
        cluster.m_settingLinks.push_back(link);
      else if (what==W_ListLink && linkOtherId==0)
        cluster.m_docInfoLink=link;
      else if (what==W_ListLink && linkOtherId==1)
        cluster.m_linkUnknown=link;
      else
        cluster.m_linksList.push_back(link);
    }
    pos=input->tell();
    if (pos!=endDataPos) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: find some extra data\n"));
      f.str("");
      f << "ClustRoot-" << n << ":" << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    ++n;
    input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
  }
  long pos=input->tell();
  if (pos!=endPos) {
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::readRootCluster: find some extra data\n"));
    f.str("");
    f << "ClustRoot###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->setReadInverted(false);

  return true;
}

bool RagTime5ZoneManager::readClusterZone(RagTime5Zone &zone, RagTime5ZoneManager::Cluster &cluster, int zoneType)
{
  switch (zoneType) {
  case 0:
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: must not be called for a root cluster\n"));
    return false;
  case 0x480:
    if (readStyleCluster(zone, cluster))
      return true;
    break;
  case 0x104:
  case 0x204:
  case 0x4104:
  case 0x4204:
    if (readUnknownClusterA(zone, cluster))
      return true;
    break;
  case 0x10000:
    if (readUnknownClusterB(zone, cluster))
      return true;
    break;
  case 0x20000:
  case 0x20001:
    if (readFieldCluster(zone, cluster, zoneType))
      return true;
    break;
  case 0x30000:
  case 0x30001:
  case 0x30002:
  case 0x30003:
    if (readUnknownClusterC(zone, cluster, zoneType))
      return true;
    break;
  case 0x40000:
    if (readColPatCluster(zone, cluster))
      return true;
    break;
  default:
    break;
  }
  cluster=Cluster();
  MWAWEntry &entry=zone.m_entry;
  if (entry.length()==0) return true;
  if (entry.length()<13) return false;

  MWAWInputStreamPtr input=zone.getInput();
  long endPos=entry.end();
  input->setReadInverted(!zone.m_hiLoEndian);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  if (zoneType==-1)
    f << "ClustUnkn";
  else
    f << "Clust" << std::hex << zoneType << std::dec << "A";
  std::string zoneName(f.str());
  f.str("");
  f << "Entries(" << zoneName << ")[" << zone << "]:";
  int val;
  for (int i=0; i<4; ++i) { // f0=f1=0, f2=1, f3=small number
    val=(int) input->readLong(2);
    if (val) f << "f" << i << "=" << val << ",";
  }
  zone.m_isParsed=true;
  libmwaw::DebugFile &ascFile=zone.ascii();
  ascFile.addPos(entry.begin());
  ascFile.addNote(f.str().c_str());
  ascFile.addPos(endPos);
  ascFile.addNote("_");

  cluster.m_hiLoEndian=zone.m_hiLoEndian;
  int n=0;
  while (!input->isEnd()) {
    long pos=input->tell();
    if (pos>=endPos) break;
    long lVal;
    if (!RagTime5StructManager::readCompressedLong(input, endPos, lVal)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f.str("");
    f << zoneName << "-" << n << ":";
    f << "f0=" << lVal << ",";
    // always in big endian
    long sz;
    if (!RagTime5StructManager::readCompressedLong(input,endPos,sz) || sz <= 7 || input->tell()+sz>endPos) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f << "sz=" << sz << ",";
    long endDataPos=input->tell()+sz;
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    pos=input->tell();
    f.str("");
    f << zoneName << "-" << n << "-A:";
    if (!zone.m_hiLoEndian) f << "lohi,";
    long fSz;
    if (!RagTime5StructManager::readCompressedLong(input, endDataPos, fSz) || fSz<6 || input->tell()+fSz>endDataPos) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: can not read item A\n"));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());

      ++n;
      input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
      continue;
    }
    long debSubDataPos=input->tell();
    long endSubDataPos=debSubDataPos+fSz;
    int fl=(int) input->readULong(2); // [01][13][0139b]
    f << "fl=" << std::hex << fl << std::dec << ",";
    int N=(int) input->readLong(4);
    std::stringstream s;
    s << "type" << std::hex << N << std::dec;

    enum What { W_Unknown, W_ListLink, W_FixedListLink,
                W_GraphZones,
                W_LinkDef,
                W_TextUnknown, W_TextZones
              };
    What what=W_Unknown;
    std::string name=s.str();
    Link link;
    int linkItemId=-1;
    bool isMainLink=false, isNamedLink=false;

    switch (N) {
    case -5:
      name="header";
      if (fSz<12) {
        f << "###data,";
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: can not read the data id\n"));
        break;
      }
      for (int i=0; i<2; ++i) { // always 0?
        val=(int) input->readLong(2);
        if (val) f << "f" << i+1 << "=" << val << ",";
      }
      val=(int) input->readLong(2);
      f << "id=" << val << ",";
      if (fSz==118) {
        what=W_GraphZones;
        name="graphZone";
        input->seek(debSubDataPos+42, librevenge::RVNG_SEEK_SET);
        link.m_type=Link::L_Graphic;
        link.m_fileType[0]=(long) input->readULong(4);
        link.m_fileType[1]=(long) input->readULong(2);
        link.m_fieldSize=(int) input->readULong(2);
        bool ok=link.m_fileType[1]==0 && (link.m_fieldSize==0x8000 || link.m_fieldSize==0x8020);
        ok=ok && m_structManager->readDataIdList(input, 2, link.m_ids) && link.m_ids[1]!=0;
        input->seek(debSubDataPos+106, librevenge::RVNG_SEEK_SET);
        std::vector<int> listIds;
        if (ok && m_structManager->readDataIdList(input, 3, listIds)) {
          link.m_ids.push_back(listIds[0]);
          for (size_t i=1; i<3; ++i) {
            if (!listIds[i]) continue;
            cluster.m_clusterIdsList.push_back(listIds[i]);
            f << "clusterId" << i << "=data" << listIds[i] << "A,";
          }
        }
        else if (ok)
          link.m_ids.push_back(0);

        if (ok) {
          cluster.m_type=Cluster::C_GraphicData;
          isMainLink=true;
          f << "graph,";
          ascFile.addDelimiter(debSubDataPos+42, '|');
          ascFile.addDelimiter(debSubDataPos+58, '|');
          ascFile.addDelimiter(debSubDataPos+106, '|');
        }
        else
          link=Link();
        input->seek(debSubDataPos+6, librevenge::RVNG_SEEK_SET);
        break;
      }
      if (fSz==134) {
        val=(int) input->readULong(2);
        if (val!=zoneType)
          f << "#type=" << std::hex << val << std::dec << ",";
        for (int i=0; i<9; ++i) { // f1=0|7-b, f3=0|8-b
          val=(int) input->readULong(2);
          if (val)
            f << "f" << i << "=" << val << ",";
        }
        for (int i=0; i<2; ++i) { // g0=0|1, g1=0|1
          val=(int) input->readULong(1);
          if (val)
            f << "g" << i << "=" << val << ",";
        }
        val=(int) input->readULong(2); // [02][08]0[12c]
        if (val) f << "fl=" << std::hex << val << std::dec << ",";
        for (int i=0; i<12; ++i) { // h1=2, h3=0|3, h5=3|4, h7=h5+1, h9=h7+1, h11=h9+1
          val=(int) input->readULong(2);
          if (val)
            f << "h" << i << "=" << val << ",";
        }
        std::vector<int> listIds;
        if (!m_structManager->readDataIdList(input, 2, listIds)) {
          f << "##field,";
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: can not read the field definitions\n"));
          break;
        }
        if (listIds[0] || listIds[1]) { // fielddef and fieldpos
          Link fieldLink;
          fieldLink.m_type=Link::L_ClusterLink;
          fieldLink.m_ids=listIds;
          cluster.m_fieldClusterLink=fieldLink;
          f << "fields," << fieldLink << ",";
        }
        break;
      }
      break;
    default: {
      if (what!=W_Unknown) break;

      input->seek(debSubDataPos+18, librevenge::RVNG_SEEK_SET);
      long type=(long) input->readULong(4);
      if (type==0x15f3817||type==0x15e4817) {
        link.m_fileType[0]=type;
        link.m_fileType[1]=(long) input->readULong(2);
        link.m_fieldSize=(int) input->readULong(2);
        if (m_structManager->readDataIdList(input, 1, link.m_ids) && link.m_ids[0]) {
          link.m_N=N;
          shared_ptr<RagTime5Zone> data=m_mainParser.getDataZone(link.m_ids[0]);
          if (data) {
            if (link.m_fileType[0]==0x15e4817) {
              name="textUnknown";
              what=W_TextUnknown;
              link.m_type=Link::L_TextUnknown;
            }
            else {
              name="linkDef";
              what=W_LinkDef;
              link.m_type=Link::L_LinkDef;
              if (fSz>=71) { // the definitions
                ascFile.addDelimiter(input->tell(),'|');
                input->seek(debSubDataPos+63, librevenge::RVNG_SEEK_SET);
                ascFile.addDelimiter(input->tell(),'|');
                std::vector<int> dataId;
                if (m_structManager->readDataIdList(input, 2, dataId) && dataId[1]) {
                  link.m_ids.push_back(dataId[0]);
                  link.m_ids.push_back(dataId[1]);
                }
              }
            }
            break;
          }
        }
      }

      input->seek(debSubDataPos+6, librevenge::RVNG_SEEK_SET);
      if (fSz==28) {
        link.m_fileType[0]=(int) input->readULong(4);
        if (link.m_fileType[0]!=0x100004) {
          f << "fType=" << std::hex << link.m_fileType[0] << std::dec << ",";
          break;
        }
        link.m_type=Link::L_Text;
        name="textZone";
        what=W_TextZones;
        cluster.m_type=Cluster::C_TextData;
        isMainLink=true;
        f << "f0=" << N << ",";
        val=(int) input->readLong(2); // always 0?
        if (val) f << "f1=" << val << ",";
        val=(int) input->readLong(2); // always f?
        if (val!=15) f << "f2=" << val << ",";
        std::vector<int> listIds;
        if (m_structManager->readDataIdList(input, 1, listIds))
          link.m_ids.push_back((int) listIds[0]);
        else {
          f << "#link0,";
          link.m_ids.push_back(0);
        }
        link.m_N=(int) input->readULong(4);
        val=(int) input->readLong(1); // always 0?
        if (val) f << "f3=" << val << ",";
        listIds.clear();
        if (m_structManager->readDataIdList(input, 1, listIds) && listIds[0])
          link.m_ids.push_back(listIds[0]);
        else {
          f << "#link1,";
          link.m_ids.push_back(0);
        }
        val=(int) input->readLong(1); // always 1?
        if (val) f << "f4=" << val << ",";
        break;
      }
      if (fSz==30) {
        link.m_fileType[0]=(int) input->readULong(4);
        input->seek(debSubDataPos+22, librevenge::RVNG_SEEK_SET);
        link.m_fileType[1]=(int) input->readULong(2);
        link.m_fieldSize=(int) input->readULong(2);
        if (m_structManager->readDataIdList(input, 1, link.m_ids) && link.m_ids[0]) {
          link.m_N=N;
          shared_ptr<RagTime5Zone> data=m_mainParser.getDataZone(link.m_ids[0]);

          if (data) {
            if (link.m_fileType[0]==0x9f840) {
              what=W_FixedListLink;
              if (link.m_fileType[1]!=0x10) // 10 or 18
                f << "f1=" << link.m_fileType[1] << ",";
              link.m_fileType[1]=0;
              link.m_type=Link::L_GraphicTransform;
              name="graphTransform";
            }
            else if (link.m_fileType[0]==0x14b9800 && (data->m_entry.length()%8)==0) {
              what=W_FixedListLink;
              if (link.m_fileType[1]&8) {
                link.m_fileType[1]&=0xFFF7;
                f << "type1[8],";
              }
              name="linkLn3";
              if (data->m_entry.length()!=N*link.m_fieldSize) {
                f << "#N=" << N << ",";
                link.m_N=int(data->m_entry.length()/8);
                link.m_fieldSize=8;
              }
            }
            else if (!link.m_fieldSize || data->m_entry.length()!=N*link.m_fieldSize)
              link=Link();
            else if ((link.m_fileType[1]==0xd0||link.m_fileType[1]==0xd8) && link.m_fieldSize==12) {
              name="clusterLink";
              link.m_type=Link::L_ClusterLink;
            }
            else if (link.m_ids[0]==14) { // fixme
              name="clusterList";
              link=Link();
              break;
            }
            else {
              what=W_FixedListLink;
              link.m_fileType[0]=0;
              name="listLn2";
              ascFile.addDelimiter(debSubDataPos+22,'|');
            }
          }
          else
            link=Link();
        }
        else
          link=Link();
        input->seek(debSubDataPos+6, librevenge::RVNG_SEEK_SET);
        break;
      }
      if (fSz==32 || fSz==36 || fSz==41) {
        what=W_ListLink;
        long unknType=(long) input->readULong(4);
        if (unknType) f << "unknType=" << std::hex << unknType << std::dec << ",";
        link.m_fileType[0]=(long) input->readULong(4);
        input->seek(debSubDataPos+20, librevenge::RVNG_SEEK_SET);
        link.m_type=Link::L_List;
        val=(int) input->readLong(2);
        if (val) f << "f3=" << val << ",";
        link.m_fileType[1]=(long) input->readULong(2);
        if (!m_structManager->readDataIdList(input, 2, link.m_ids) || !link.m_ids[1]) {
          if (fSz==41 || fSz==36)
            what=W_Unknown;
          else
            f << "###";
          link=Link();
          break;
        }
        if (link.m_fileType[1]==0x220 || link.m_fileType[1]==0x200 || link.m_fileType[1]==0x620) {
          name="unicodeList";
          if (link.m_fileType[0]==0x7d01a) name+="[layout]";
          isNamedLink=true;
          link.m_type=Link::L_UnicodeList;
        }
        else if (unknType==0x47040) {
          if (link.m_fileType[1]==0x20)
            f << "hasNoPos,";
          else if (link.m_fileType[1]) {
            MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone[settings]: find unexpected flags\n"));
            f << "###flags=" << std::hex << link.m_fileType[1] << std::dec << ",";
          }
          linkItemId=1;
          link.m_name=name="settings";
        }
        // fSz=41 is the field list
        // todo 0x80045080 is a list of 2 int
        // fileType[1]==4030: layout list
        else if (link.m_fileType[1]==0x30 && fSz==32) {
          linkItemId=0;
          link.m_name=name="condFormula";
        }
        else
          name="listLink";
        /* checkme:
           link.m_fileType[0]==20|0 is a list of fields
           link.m_fileType[0]==1010|1038 is a list of sequence of 16 bytes
        */
        ascFile.addDelimiter(debSubDataPos+20,'|');
        break;
      }
      if (fSz==0x4e  && N==1) {
        name="unknClustChild";
        val=(int) input->readULong(4);
        if (val) f << "id=" << val << ",";
        link.m_fileType[0]=(long) input->readULong(4);
        if (link.m_fileType[0]!=0x154a042) {
          f << "###";
          break;
        }
        for (int i=0; i<2; ++i) { // always 0
          val=(int) input->readULong(2);
          if (val)
            f << "f" << i << "=" << val << ",";
        }
        std::vector<int> listIds;
        if (!m_structManager->readDataIdList(input, 2, listIds)) {
          f << "###";
          break;
        }
        if (listIds[0] || listIds[1]) { // fielddef and fieldpos
          Link fieldLink;
          fieldLink.m_type=Link::L_ClusterLink;
          fieldLink.m_ids=listIds;
          cluster.m_fieldClusterLink=fieldLink;
          f << "fields," << fieldLink << ",";
        }
        val=(int) input->readULong(4);
        if (val) f << "id2=" << val << ",";
        for (int i=0; i<4; ++i) { // always 0
          val=(int) input->readULong(2);
          if (val)
            f << "f" << i+2 << "=" << val << ",";
        }
        for (int i=0; i<2; ++i) { // always 1,0
          val=(int) input->readULong(1);
          if (val!=1-i)
            f << "fl" << i << "=" << val << ",";
        }
        val=(int) input->readLong(2);
        if (val!=100)
          f << "f6=" << val << ",";
        f << "ID=[";
        for (int i=0; i<4; ++i) { // very big number
          val=(int) input->readULong(4);
          if (val)
            f << std::hex << val << std::dec << ",";
          else
            f << "_,";
        }
        f << "],";
        listIds.clear();
        if (!m_structManager->readDataIdList(input, 4, listIds)) {
          f << "###";
          break;
        }
        if (listIds[0] || listIds[1] || listIds[2] || listIds[3]) {
          Link fieldLink;
          fieldLink.m_type=Link::L_UnknownClusterC;
          fieldLink.m_ids=listIds;
          cluster.m_linksList.push_back(fieldLink);
          f << fieldLink << ",";
        }

        val=(int) input->readULong(4);
        if (val) f << "id3=" << val << ",";
        break;
      }
      if (fSz==0x50  && N==1) {
        name="graphDim";
        val=(int) input->readULong(2); // always 0?
        if (val) f << "f0=" << val << ",";
        val=(int) input->readULong(2);
        if (val) f << "id=" << val << ",";
        val=(int) input->readULong(2); //[04][01248a][01][23]
        if (val) f << "fl=" << std::hex << val << std::dec << ",";
        for (int i=0; i<2; ++i) {
          val=(int) input->readULong(2); //f1=0-1e, f2=0|3ffe
          if (val) f << "f" << i+1 << "=" << val << ",";
        }
        float dim[4];
        for (int i=0; i<4; ++i) dim[i]=float(input->readLong(4))/65536.f;
        Box2f box(Vec2f(dim[0],dim[1]), Vec2f(dim[2],dim[3]));
        f << "box=" << box << ",";
        for (int i=0; i<4; ++i) dim[i]=float(input->readLong(4))/65536.f;
        Box2f box2(Vec2f(dim[0],dim[1]), Vec2f(dim[2],dim[3]));
        if (box!=box2)
          f << "boxA=" << box2 << ",";
        for (int i=0; i<7; ++i) { // g1=0|2, g3=9|7
          val=(int) input->readLong(2);
          if (val) f << "g" << i << "=" << val << ",";
        }
        for (int i=0; i<9; ++i) { // h0=a flag?, h2= a flag?, h5=h6=0|-1
          val=(int) input->readLong(2);
          if (val) f << "h" << i << "=" << val << ",";
        }
        break;
      }
      break;
    }
    }
    if (link.empty())
      f << name << "-" << lVal << ",";
    else
      f << link << ",";
    if (fSz>32 && N>0 && what==W_Unknown) {
      long actPos=input->tell();
      input->seek(debSubDataPos+18, librevenge::RVNG_SEEK_SET);
      link.m_fileType[0]=(long) input->readULong(4);
      link.m_fileType[1]=(long) input->readULong(2);
      link.m_fieldSize=(int) input->readULong(2);
      if (m_structManager->readDataIdList(input, 1, link.m_ids) && link.m_ids[0]) {
        link.m_N=N;
        shared_ptr<RagTime5Zone> data=m_mainParser.getDataZone(link.m_ids[0]);
        if (data && data->m_entry.length()==N*link.m_fieldSize) {
          name+="-fixZone";
          what=W_FixedListLink;

          f << link << ",";
          ascFile.addDelimiter(debSubDataPos+18,'|');
          ascFile.addDelimiter(debSubDataPos+30,'|');
        }
        else
          link=Link();
      }
      else
        link=Link();
      input->seek(actPos, librevenge::RVNG_SEEK_SET);
    }
    if (input->tell()!=pos && input->tell()!=endSubDataPos)
      ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(endSubDataPos, librevenge::RVNG_SEEK_SET);

    int m=0;

    while (!input->isEnd()) {
      pos=input->tell();
      if (pos+4>endDataPos)
        break;
      RagTime5StructManager::Field field;
      if (!m_structManager->readField(input, endDataPos, ascFile, field)) {
        input->seek(pos, librevenge::RVNG_SEEK_SET);
        break;
      }
      f.str("");
      f << zoneName << "-" << n << "-B" << m++ << "[" << name << "]:";
      if (!zone.m_hiLoEndian) f << "lohi,";
      bool done=false;
      switch (what) {
      case W_ListLink: {
        if (field.m_type==RagTime5StructManager::Field::T_LongList && field.m_fileType==0xce842) {
          f << "pos=[";
          for (size_t i=0; i<field.m_longList.size(); ++i)
            f << field.m_longList[i] << ",";
          f << "],";
          link.m_longList=field.m_longList;
          done=true;
          break;
        }
        if (field.m_type==RagTime5StructManager::Field::T_Unstructured && field.m_fileType==0xce017) {
          // a small value 2|4|a|1c|40
          f << "unkn="<<field.m_extra << ",";
          done=true;
          break;
        }
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find unexpected list link field\n"));
        f << "###";
        break;
      }
      case W_LinkDef: {
        if (field.m_type==RagTime5StructManager::Field::T_FieldList &&
            (field.m_fileType==0x15f4815 /* v5?*/ || field.m_fileType==0x160f815 /* v6? */)) {
          f << "decal=[";
          for (size_t i=0; i<field.m_fieldList.size(); ++i) {
            RagTime5StructManager::Field const &child=field.m_fieldList[i];
            if (child.m_type==RagTime5StructManager::Field::T_LongList && child.m_fileType==0xce842) {
              for (size_t j=0; j<child.m_longList.size(); ++j)
                f << child.m_longList[j] << ",";
              link.m_longList=child.m_longList;
              continue;
            }
            MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find unexpected decal child[list]\n"));
            f << "#[" << child << "],";
          }
          f << "],";
          done=true;
          break;
        }
        else if (field.m_type==RagTime5StructManager::Field::T_Unstructured && field.m_fileType==0xce017) {
          f << "unkn0=" << field.m_extra; // always 2: next value ?
          done=true;
          break;
        }
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find unexpected child[list]\n"));
        break;
      }

      case W_GraphZones: {
        if (field.m_type==RagTime5StructManager::Field::T_Long && field.m_fileType==0x3c057) {
          // a small value 3|4
          f << "f0="<<field.m_longValue[0] << ",";
          done=true;
          break;
        }
        if (field.m_type==RagTime5StructManager::Field::T_FieldList && field.m_fileType==0x14e6825) {
          f << "decal=[";
          for (size_t i=0; i<field.m_fieldList.size(); ++i) {
            RagTime5StructManager::Field const &child=field.m_fieldList[i];
            if (child.m_type==RagTime5StructManager::Field::T_LongList && child.m_fileType==0xce842) {
              for (size_t j=0; j<child.m_longList.size(); ++j)
                f << child.m_longList[j] << ",";
              link.m_longList=child.m_longList;
              continue;
            }
            MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find unexpected decal child[graph]\n"));
            f << "#[" << child << "],";
          }
          f << "],";
          done=true;
          break;
        }
        if (field.m_type==RagTime5StructManager::Field::T_FieldList && field.m_fileType==0x14e6875) {
          f << "listFlag?=[";
          for (size_t i=0; i<field.m_fieldList.size(); ++i) {
            RagTime5StructManager::Field const &child=field.m_fieldList[i];
            if (child.m_type==RagTime5StructManager::Field::T_Unstructured && child.m_fileType==0xce017)
              f << child.m_extra << ","; // find data with different length there
            else {
              MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find unexpected unstructured child[graphZones]\n"));
              f << "#" << child << ",";
            }
          }
          f << "],";
          done=true;
          break;
        }
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find unexpected child[graphZones]\n"));
        break;
      }
      case W_TextUnknown:
      case W_TextZones:
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find unexpected text zones child\n"));
        f << "###";
        break;
      case W_FixedListLink:
      case W_Unknown:
      default:
        break;
      }
      if (done) {
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        continue;
      }
      f << field;
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    if (!link.empty()) {
      if (isMainLink) {
        if (cluster.m_dataLink.empty())
          cluster.m_dataLink=link;
        else {
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: oops the main link is already set\n"));
          cluster.m_linksList.push_back(link);
        }
      }
      else if (link.m_type==Link::L_List && linkItemId==0)
        cluster.m_conditionFormulaLinks.push_back(link);
      else if (link.m_type==Link::L_List && linkItemId==1)
        cluster.m_settingLinks.push_back(link);
      else if (isNamedLink) {
        if (cluster.m_nameLink.empty())
          cluster.m_nameLink=link;
        else {
          MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: oops the name link is already set\n"));
          cluster.m_linksList.push_back(link);
        }
      }
      else
        cluster.m_linksList.push_back(link);
    }
    pos=input->tell();
    if (pos!=endDataPos) {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find some extra data\n"));
      f.str("");
      f << zoneName << "-" << n << ":" << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
    }
    ++n;
    input->seek(endDataPos, librevenge::RVNG_SEEK_SET);
  }
  long pos=input->tell();
  if (pos!=endPos) {
    MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find some extra data\n"));
    f.str("");
    f << zoneName << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
  }
  input->setReadInverted(false);

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
