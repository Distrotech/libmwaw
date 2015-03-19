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
  std::string zoneName(f.str());
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
        f << "clustAData=data" << listIds[0] << "A,";
      }
      cluster.m_clusterIds.push_back(listIds[1]);
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
      cluster.m_clusterIds.push_back(listIds[0]);
      if (listIds[0]) // find some 4104 cluster
        f << "cluster[nextId]=data" << listIds[0] << "A,";
      if (listIds[1]) { // find some 4001 cluster
        cluster.m_clusterIds.push_back(listIds[1]);
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
        f << "clustADatB=data" << listIds[0] << "A,";
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
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterA: find some fields\n"));
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

  // check cluster id
  for (size_t j=0; j<cluster.m_clusterIds.size(); ++j) {
    int cId=cluster.m_clusterIds[j];
    if (cId==0) continue;
    shared_ptr<RagTime5Zone> data=m_mainParser.getDataZone(cId);
    if (!data || !data->m_entry.valid() || data->getKindLastPart(data->m_kinds[1].empty())!="Cluster") {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readUnknownClusterA: the cluster zone %d seems bad\n", cId));
      continue;
    }
  }

  return true;
}

bool RagTime5ZoneManager::readClusterZone(RagTime5Zone &zone, RagTime5ZoneManager::Cluster &cluster, int zoneType)
{
  if (zoneType==0x480) {
    if (readStyleCluster(zone, cluster))
      return true;
    cluster=Cluster();
  }
  else if (zoneType==0x104 || zoneType==0x204 || zoneType==0x4104 || zoneType==0x4204) {
    if (readUnknownClusterA(zone, cluster))
      return true;
    cluster=Cluster();
  }
  MWAWEntry &entry=zone.m_entry;
  if (entry.length()==0) return true;
  if (entry.length()<13) return false;

  MWAWInputStreamPtr input=zone.getInput();
  long endPos=entry.end();
  input->setReadInverted(!zone.m_hiLoEndian);
  input->seek(entry.begin(), librevenge::RVNG_SEEK_SET);
  libmwaw::DebugStream f;
  switch (zoneType) {
  case 0:
    f << "ClustRoot";
    break;
  case 0x480:
    f << "ClustStyle";
    break;
  case 0x104:
  case 0x204:
  case 0x4104:
  case 0x4204:
    f << "ClustUnkA";
    break;
  case -1:
    f << "ClustUnkn";
    break;
  default:
    f << "Clust" << std::hex << zoneType << std::dec << "A";
    break;
  }
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

    enum What { W_Unknown, W_FileName, W_ListLink, W_FixedListLink,
                W_ColorPattern,
                W_GraphTypes, W_GraphZones,
                W_LinkDef,
                W_TextUnknown, W_TextZones
              };
    What what=W_Unknown;
    std::string name=s.str();
    Link link;
    bool isMainLink=false, isNamedLink=false;

    if ((zone.m_hiLoEndian && N==int(0x80000000)) || (!zone.m_hiLoEndian && N==0x8000)) {
      name="filename";
      what=W_FileName;
    }
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
      if (fSz==82 || fSz==86) {
        val=(int) input->readULong(2);
        if (val!=0x8042) f << "f3=" << std::hex << val << std::dec << ",";
        for (int i=0; i<5; ++i) { // g1, g2=10
          val=(int) input->readLong(2);
          if (i==4)
            link.m_N=val;
          else if (val) f << "g" << i << "=" << val << ",";
        }
        link.m_fileType[1]=(long) input->readULong(2);
        ascFile.addDelimiter(input->tell(),'|');
        input->seek(debSubDataPos+42, librevenge::RVNG_SEEK_SET);
        ascFile.addDelimiter(input->tell(),'|');
        link.m_fieldSize=(int) input->readULong(2);
        std::vector<int> listIds;
        if (!m_structManager->readDataIdList(input, 1, listIds)) {
          link=Link();
          break;
        }
        link.m_ids.push_back(listIds[0]);
        ascFile.addDelimiter(input->tell(),'|');
        input->seek(debSubDataPos+74, librevenge::RVNG_SEEK_SET);
        ascFile.addDelimiter(input->tell(),'|');
        if (!m_structManager->readDataIdList(input, 2, listIds)) {
          link=Link();
          break;
        }
        link.m_ids.push_back(listIds[0]);
        if (listIds[1]) {
          cluster.m_clusterIds.push_back(listIds[1]);
          f << "clusterId1=data" << listIds[1] << "A,";
        }
        name="color/pattern";
        cluster.m_type=Cluster::C_ColorPattern;
        link.m_type=Link::L_ColorPattern;
        what=W_ColorPattern;
        isMainLink=true;
        break;
      }
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
            cluster.m_clusterIds.push_back(listIds[i]);
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
      break;
    case -2: {
      name="head2";
      if (fSz<215) {
        f << "###sz,";
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: can not read the head2 size seems bad\n"));
        break;
      }
      val=(int) input->readLong(4); // 8|9|a
      f << "f1=" << val << ",";
      for (int i=0; i<4; ++i) { // f2=0-7, f3=1|3
        val=(int) input->readLong(2);
        if (val) f << "f" << i+2 << "=" << val << ",";
      }
      val=(int) input->readLong(4); // 7|8
      std::vector<int> listIds;
      f << "f6=" << val << ",";
      if (!m_structManager->readDataIdList(input, 1, listIds) || !listIds[0]) {
        f << "###cluster[child],";
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone[head2]: can not find the cluster's child\n"));
      }
      else {
        cluster.m_childClusterIds.push_back(listIds[0]);
        f << "cluster[child]=data" << listIds[0] << "A,";
      }
      for (int i=0; i<21; ++i) { // always g0=g11=g18=16, other 0 ?
        val=(int) input->readLong(2);
        if (val) f << "g" << i << "=" << val << ",";
      }
      break;
    }
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
            MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone[settins]: find unexpected flags\n"));
            f << "###flags=" << std::hex << link.m_fileType[1] << std::dec << ",";
          }
          name="settings";
          link.m_type=Link::L_SettingsList;
        }
        else if ((link.m_fileType[1]&0xFFD7)==0x1010) {
          if (link.m_fileType[1]==0x20)
            f << "hasNoPos,";
          if (fSz>=36) {
            name="field[def]";
            link.m_type=Link::L_FieldDef;
            std::vector<int> listIds;
            if (m_structManager->readDataIdList(input, 1, listIds) && listIds[0]) {
              cluster.m_clusterIds.push_back(listIds[0]);
              f << "clusterId1=data" << listIds[0] << "A,";
            }
          }
          else {
            name="field[pos]";
            link.m_type=Link::L_FieldPos;
          }
        }
        // fSz=41 is the field list
        // todo 0x80045080 is a list of 2 int
        // fileType[1]==4030: layout list
        else if (link.m_fileType[1]==0x30 && fSz==32) {
          name="condFormula";
          link.m_type=Link::L_ConditionFormula;
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
      if (fSz==52 && N==1) {
        name="graphTypes";
        what=W_GraphTypes;
        for (int i=0; i<2; ++i) { // always 0 and small val
          val=(int) input->readLong(2);
          if (val) f << "f" << i+1 << "=" << val << ",";
        }
        if (input->readULong(4)!=0x14e6042)
          break;
        for (int i=0; i<16; ++i) { // g1=0-2, g2=10[size?], g4=1-8[N], g13=30
          val=(int) input->readLong(2);
          if (val) f << "g" << i << "=" << val << ",";
        }
        if (m_structManager->readDataIdList(input, 1, link.m_ids) && link.m_ids[0]) {
          link.m_type=Link::L_GraphicType;
          link.m_fileType[0]=0x30;
          link.m_fileType[1]=0;
          link.m_fieldSize=16;
        }
        else
          link.m_ids.push_back(0);
        val=(int) input->readLong(2);
        if (val) // small number
          f << "h0=" << val << ",";
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
        for (size_t i=0; i<2; ++i) {
          if (!listIds[i])
            continue;
          cluster.m_childClusterIds.push_back(listIds[i]);
          f << "clusterId" << i << "=data" << listIds[i] << "A,";
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
        for (size_t i=0; i<4; ++i) {
          if (!listIds[i])
            continue;
          cluster.m_childClusterIds.push_back(listIds[i]);
          f << "clusterId" << i+2 << "=data" << listIds[i] << "A,";
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
      case W_FileName:
        if (field.m_type==RagTime5StructManager::Field::T_Unicode && field.m_fileType==0xc8042) {
          f << field.m_string.cstr();
          done=true;
          break;
        }
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find unexpected filename field\n"));
        f << "###";
        break;
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
      case W_ColorPattern: {
        if (field.m_type==RagTime5StructManager::Field::T_FieldList && (field.m_fileType==0x16be055 || field.m_fileType==0x16be065)) {
          f << "unk" << (field.m_fileType==0x16be055 ? "0" : "1") << "=";
          for (size_t i=0; i<field.m_fieldList.size(); ++i) {
            RagTime5StructManager::Field const &child=field.m_fieldList[i];
            if (child.m_type==RagTime5StructManager::Field::T_Long && child.m_fileType==0xcf817) {
              f << child.m_longValue[0] << ",";
              continue;
            }
            MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find unexpected color/pattern child field\n"));
            f << "#[" << child << "],";
          }
          f << "],";
          done=true;
          break;
        }
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find unexpected color/pattern link field\n"));
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

      case W_GraphTypes: {
        if (field.m_type==RagTime5StructManager::Field::T_FieldList && field.m_fileType==0x14eb015) {
          f << "decal=[";
          for (size_t i=0; i<field.m_fieldList.size(); ++i) {
            RagTime5StructManager::Field const &child=field.m_fieldList[i];
            if (child.m_type==RagTime5StructManager::Field::T_LongList && child.m_fileType==0xce842) {
              for (size_t j=0; j<child.m_longList.size(); ++j)
                f << child.m_longList[j] << ",";
              link.m_longList=child.m_longList;
              continue;
            }
            MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find unexpected decal child[graphType]\n"));
            f << "#[" << child << "],";
          }
          f << "],";
          done=true;
          break;
        }
        MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: find unexpected child[graphType]\n"));
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

  // check cluster id
  for (size_t j=0; j<cluster.m_clusterIds.size(); ++j) {
    int cId=cluster.m_clusterIds[j];
    if (cId==0) continue;
    shared_ptr<RagTime5Zone> data=m_mainParser.getDataZone(cId);
    if (!data || !data->m_entry.valid() || data->getKindLastPart(data->m_kinds[1].empty())!="Cluster") {
      MWAW_DEBUG_MSG(("RagTime5ZoneManager::readClusterZone: the cluster zone %d seems bad\n", cId));
      continue;
    }
  }

  return true;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
