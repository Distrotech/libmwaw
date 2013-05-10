/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */

/* libmwaw: tools
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

#include <string.h>

#include <cstring>
#include <iostream>
#include <list>
#include <sstream>
#include <set>
#include <string>

#include <libmwaw_internal.hxx>
#include "file_internal.h"
#include "input.h"
#include "ole.h"

namespace libmwaw_tools
{
unsigned short OLE::readU16(InputStream &input)
{
  unsigned long nRead;
  unsigned char const *data = input.read(2, nRead);
  if (!data || nRead != 2) return 0;
  return (unsigned short) (data[0]+(data[1]<<8));
}
unsigned int OLE::readU32(InputStream &input)
{
  unsigned long nRead;
  unsigned char const *data = input.read(4, nRead);
  if (!data || nRead != 4) return 0;
  return (unsigned int)  (data[0]+(data[1]<<8)+(data[2]<<16)+(data[3]<<24));
}

// header functions
bool OLE::Header::read(InputStream &input)
{
  if (input.length() < 512)
    return false;
  input.seek(0, InputStream::SK_SET);
  unsigned long nRead;
  unsigned char const *magic=input.read(8, nRead);
  if (!magic || nRead != 8)
    return false;
  const unsigned char oleMagic[] =
  { 0xd0, 0xcf, 0x11, 0xe0, 0xa1, 0xb1, 0x1a, 0xe1 };
  for (int i = 0; i < 8; i++) {
    if (magic[i] != oleMagic[i])
      return false;
  }
  input.seek(0x1e, InputStream::SK_SET);
  unsigned shift;
  shift = (unsigned) readU16(input);
  if (shift <= 6 || shift >= 31)
    return false;
  m_sizeBats[BigBat] = (unsigned) (1 << shift);
  shift = (unsigned) readU16(input);
  m_sizeBats[SmallBat] = (unsigned) (1 << shift);
  if (shift >= 31 || m_sizeBats[SmallBat] >= m_sizeBats[BigBat])
    return false;

  input.seek(0x2c, InputStream::SK_SET);
  m_numBats[BigBatAlloc] = (unsigned) readU32(input);
  m_startBats[Dirent] = (unsigned) readU32(input);
  input.seek(0x38, InputStream::SK_SET);
  m_threshold = (unsigned) readU32(input);
  m_startBats[SmallBat] = (unsigned) readU32(input);
  m_numBats[SmallBat] = (unsigned) readU32(input);
  m_startBats[MetaBat] = (unsigned) readU32(input);
  m_numBats[MetaBat] = (unsigned) readU32(input);

  if (m_threshold != 4096 || !m_numBats[BigBatAlloc] ||
      (m_numBats[BigBatAlloc]<= 109 && m_numBats[MetaBat]) ||
      (m_numBats[BigBatAlloc]> 109 && m_numBats[BigBatAlloc] > m_numBats[MetaBat] * (m_sizeBats[BigBat]/4-1) + 109))
    return false;

  size_t numBigBatInHeader=size_t(m_numBats[BigBatAlloc]>109 ? 109 : m_numBats[BigBatAlloc]);
  m_bigBatAlloc.resize(numBigBatInHeader);
  for (size_t i=0; i < numBigBatInHeader; i++)
    m_bigBatAlloc[i] = (unsigned long) readU32(input);
  return true;
}

// direntry function
bool OLE::DirEntry::read(InputStream &input)
{
  long pos = input.tell();
  input.seek(pos+128, InputStream::SK_SET);
  if (input.tell()!=pos+128) {
    MWAW_DEBUG_MSG(("OLE::DirEntry::read: file entry is too short\n"));
    return false;
  }
  input.seek(pos+0x40, InputStream::SK_SET);
  unsigned name_len = (unsigned) readU16(input);
  if( name_len > 64 ) name_len = 64;
  // 2 = file (aka stream), 1 = directory (aka storage), 5 = root
  m_type = (unsigned) input.readU8();

  input.seek(pos, InputStream::SK_SET);
  // parse name of this entry, which stored as Unicode 16-bit
  m_name=std::string("");
  for( unsigned j=0; j<name_len; j+= 2 ) {
    unsigned val = readU16(input);
    if (!val) break;
    if (val==0x5200 && name_len==2 && m_type==5) {
      m_name="R";
      m_macRootEntry=true;
      break;
    }
    m_name.append(1, char(val) );
  }
  input.seek(pos+0x44, InputStream::SK_SET);
  m_left = (unsigned) readU32(input);
  m_right = (unsigned) readU32(input);
  m_child = (unsigned) readU32(input);
  for (int i = 0; i < 4; i++)
    m_clsid[i]=(unsigned) readU32(input);

  input.seek(pos+0x74, InputStream::SK_SET);
  m_start = (unsigned long) readU32(input);
  m_size = (unsigned long) readU32(input);

  // sanity checks
  m_valid = true;
  if( (m_type != 2) && (m_type != 1 ) && (m_type != 5 ) ) m_valid = false;
  if( name_len < 1 ) m_valid = false;
  return true;
}

////////////////////////////////////////////////////////////
// dirtree function
////////////////////////////////////////////////////////////
void OLE::DirTree::DirTree::clear()
{
  m_entries.resize(1);
  m_entries[0]=DirEntry();
  m_entries[0].m_valid = true;
  m_entries[0].setName("Root Entry");
  m_entries[0].m_type = 5;
}

unsigned OLE::DirTree::index( const std::string &name) const
{

  if( name.length()==0 ) return DirEntry::End;

  // quick check for "/" (that's root)
  if( name == "/" ) return 0;

  // split the names, e.g  "/ObjectPool/_1020961869" will become:
  // "ObjectPool" and "_1020961869"
  std::list<std::string> names;
  std::string::size_type start = 0, end = 0;
  if( name[0] == '/' ) start++;
  while( start < name.length() ) {
    end = name.find_first_of( '/', start );
    if( end == std::string::npos ) end = name.length();
    names.push_back( name.substr( start, end-start ) );
    start = end+1;
  }

  // start from root
  unsigned ind = 0 ;

  // trace one by one
  std::list<std::string>::const_iterator it;
  size_t depth = 0;

  for( it = names.begin(); it != names.end(); ++it, ++depth) {
    std::string childName(*it);
    if (childName.length() && childName[0]<32)
      childName= it->substr(1);

    unsigned child = find_child( ind, childName );
    // traverse to the child
    if( child > 0 ) {
      ind = child;
      continue;
    }
    return DirEntry::End;
  }

  return ind;
}

unsigned OLE::DirTree::find_child( unsigned ind, const std::string &name ) const
{
  DirEntry const *p = entry( ind );
  if (!p || !p->m_valid) return 0;
  std::vector<unsigned> siblingsList = get_siblings(p->m_child);
  for (size_t s=0; s < siblingsList.size(); s++) {
    p  = entry( siblingsList[s] );
    if (!p) continue;
    if (p->name()==name)
      return siblingsList[s];
  }
  return 0;
}

void OLE::DirTree::get_siblings(unsigned ind, std::set<unsigned> &seens) const
{
  if (seens.find(ind) != seens.end())
    return;
  seens.insert(ind);
  DirEntry const *e = entry( ind );
  if (!e) return;
  unsigned cnt = size();
  if (e->m_left>0&& e->m_left < cnt)
    get_siblings(e->m_left, seens);
  if (e->m_right>0 && e->m_right < cnt)
    get_siblings(e->m_right, seens);
}

////////////////////////////////////////////////////////////
// alloc table function
////////////////////////////////////////////////////////////
std::vector<unsigned long> OLE::AllocTable::follow( unsigned long start ) const
{
  std::vector<unsigned long> chain;
  if( start >= count() ) return chain;

  std::set<unsigned long> seens;
  unsigned long p = start;
  while( p < count() ) {
    if( p >= 0xfffffffc) break;
    if (seens.find(p) != seens.end()) break;
    seens.insert(p);
    chain.push_back( p );
    p = m_data[ p ];
  }

  return chain;
}

////////////////////////////////////////////////////////////
// OLE
////////////////////////////////////////////////////////////
bool OLE::initAllocTables()
{
  size_t const maxN = size_t(m_header.m_sizeBats[BigBat]/4);
  size_t const numBats = size_t(m_header.m_numBats[BigBatAlloc]);
  // first we need to find the file position of the big bat alloc table
  std::vector<unsigned long> bigBatAlloc = m_header.m_bigBatAlloc;
  if (numBats > 109) {
    bigBatAlloc.resize(numBats, Eof);
    size_t k = 109;
    unsigned sector=m_header.m_startBats[MetaBat];
    for( unsigned r = 0; r < m_header.m_numBats[MetaBat]; r++ ) {
      if (sector >= 0xfffffffc) {
        MWAW_DEBUG_MSG(("OLE::initAllocTables: can not find meta block position\n"));
        return false;
      }
      m_input.seek((long) m_header.getBigBlockPos(sector), InputStream::SK_SET);
      if (m_input.atEOS()) {
        MWAW_DEBUG_MSG(("OLE::initAllocTables: can not read a meta block\n"));
        return false;
      }

      for (size_t s=0; s < maxN; s++) {
        if (k >= numBats)
          break;
        if (s==maxN-1)
          sector = (unsigned) readU32(m_input);
        else
          bigBatAlloc[k++] = (unsigned long) readU32(m_input);
      }
      if (k >= numBats)
        break;
    }
    if (k != numBats) {
      MWAW_DEBUG_MSG(("OLE::initAllocTables: can not read all bigbat position\n"));
      return false;
    }
  }

  // read the big block alloc table
  unsigned long numBBats=(unsigned long) numBats * (unsigned long) maxN;
  m_allocTable[BigBat].resize(numBBats);
  unsigned long pos = 0;
  for (size_t k=0; k < numBats; k++) {
    m_input.seek((long) m_header.getBigBlockPos((unsigned)bigBatAlloc[k]), InputStream::SK_SET);
    for (size_t s=0; s < maxN; s++) {
      if (m_input.atEOS()) {
        MWAW_DEBUG_MSG(("OLE::initAllocTables: can not read a big alloc table block\n"));
        return false;
      }
      unsigned long val = (unsigned long) readU32(m_input);
      if (val >= 0xfffffffc)
        m_allocTable[BigBat][pos++] = Eof;
      else
        m_allocTable[BigBat][pos++] = val;
    }
  }
  // read the small bat alloc table
  std::vector<unsigned long> smallBlocks = m_allocTable[BigBat].follow( m_header.m_startBats[SmallBat] );
  unsigned long numSBats=(unsigned long) smallBlocks.size() * (unsigned long) maxN;
  m_allocTable[SmallBat].resize(numSBats);
  pos = 0;
  for (size_t k=0; k < smallBlocks.size(); k++) {
    m_input.seek((long)m_header.getBigBlockPos((unsigned)smallBlocks[k]), InputStream::SK_SET);
    for (size_t s=0; s < maxN; s++) {
      if (m_input.atEOS()) {
        MWAW_DEBUG_MSG(("OLE::initAllocTables: can not read a small alloc table block\n"));
        return false;
      }
      unsigned long val = (unsigned long) readU32(m_input);
      if (val >= 0xfffffffc)
        m_allocTable[SmallBat][pos++] = Eof;
      else
        m_allocTable[SmallBat][pos++] = val;
    }
  }
  return true;
}

bool OLE::init()
{
  if (m_status!=S_Unchecked)
    return m_status==S_Ok;
  m_status=S_Bad;
  if (!m_header.read(m_input) || !initAllocTables())
    return false;

  // load directory tree
  std::vector<unsigned long> dirBlocks = m_allocTable[BigBat].follow(m_header.m_startBats[Dirent]);
  if (!dirBlocks.size())
    return false;
  size_t const maxDir = size_t(m_header.m_sizeBats[BigBat]/128);
  unsigned numDBats=(unsigned) dirBlocks.size() * (unsigned) maxDir;
  m_dirTree.resize(numDBats);
  unsigned pos = 0;
  for (size_t k=0; k < dirBlocks.size(); k++) {
    long fPos = (long) m_header.getBigBlockPos((unsigned) dirBlocks[k]);
    for (size_t s=0; s < maxDir; s++) {
      m_input.seek(fPos+long(128*s), InputStream::SK_SET);
      if (m_input.atEOS() || !m_dirTree.entry(pos++)->read(m_input)) {
        MWAW_DEBUG_MSG(("OLE::init: can not read a dir block\n"));
        return false;
      }
    }
  }
  m_smallBlockPos = m_allocTable[BigBat].follow(m_dirTree.entry(0)->m_start);
  m_status=S_Ok;
  return true;
}

std::string OLE::getCLSIDType()
{
  if (!init())
    return "";
  // get the root dir entry and the main clsid
  DirEntry *entry=m_dirTree.entry(0);
  if (!entry) return "";
  return getCLSIDType(entry->m_clsid);
}

std::string OLE::getCompObjType()
{
  if (!init())
    return "";
  std::vector<unsigned char> buf;
  if (!load("/CompObj", buf))
    return "";
  if (buf.size() < 28)
    return "";
  StringStream stream(&buf[0], (unsigned long) buf.size());
  stream.seek(12, InputStream::SK_SET);
  unsigned clsId[4];
  for (int i = 0; i < 4; i++)
    clsId[i]=(unsigned) readU32(stream);
  MWAW_DEBUG_MSG(("Find: %x %x %x %x\n", clsId[0], clsId[1], clsId[2], clsId[3]));
  return getCLSIDType(clsId);
}

bool OLE::load(std::string const &name, std::vector<unsigned char> &buffer)
{
  if (!init())
    return false;
  DirEntry *e = name.length() ? m_dirTree.entry( name ) : 0;
  if (!e || e->is_dir() || !e->m_size) {
    buffer.resize(0);
    return false;
  }
  buffer.resize(size_t(e->m_size));
  bool useBig=useBigBlockFor(e->m_size);
  std::vector<unsigned long> blocks=m_allocTable[useBig ? BigBat : SmallBat].follow(e->m_start);
  unsigned long blockSize=(unsigned long)m_header.m_sizeBats[useBig ? BigBat : SmallBat];
  unsigned long wPos = 0;
  for (size_t b=0; b < blocks.size(); b++) {
    long pos=getDataAddress((unsigned)blocks[b], useBig);
    if (pos <= 0) {
      MWAW_DEBUG_MSG(("OLE::load: oops can not find block address\n"));
      buffer.resize(0);
      return false;
    }
    m_input.seek(pos, InputStream::SK_SET);
    if (m_input.atEOS() || m_input.tell()!=pos) {
      MWAW_DEBUG_MSG(("OLE::load: oops can not go to file position\n"));
      buffer.resize(0);
      return false;
    }
    unsigned long toRead=e->m_size-wPos, read;
    if (toRead>blockSize) toRead=blockSize;
    unsigned char const *buf=m_input.read(toRead, read);
    if (!buf || read!=toRead) {
      MWAW_DEBUG_MSG(("OLE::load: oops can not read data\n"));
      buffer.resize(0);
      return false;
    }
    memcpy(&buffer[wPos], buf, size_t(read));
    wPos += read;
    if (wPos >= e->m_size) break;
  }
  return true;
}

std::string OLE::getCLSIDType(unsigned const (&clsid)[4])
{
  // do not accept not standart ole
  if (clsid[1] != 0 || clsid[2] != 0xC0 || clsid[3] != 0x46000000L)
    return "";

  switch(clsid[0]) {
  case 0x00000319:
    return "OLE file(EMH-picture?)"; // addon Enhanced Metafile ( find in some file)

  case 0x00020906:
    return "OLE file(MSWord mac)";
  case 0x00021290:
    return "OLE file(MSClipArtGalley2)";
  case 0x000212F0:
    return "OLE file(MSWordArt)"; // or MSWordArt.2
  case 0x00021302:
    return "OLE file(MSWorksWPDoc)"; // addon

    // MS Apps
  case 0x00030000:
    return "OLE file(ExcelWorksheet)";
  case 0x00030001:
    return "OLE file(ExcelChart)";
  case 0x00030002:
    return "OLE file(ExcelMacrosheet)";
  case 0x00030003:
    return "OLE file(WordDocument)";
  case 0x00030004:
    return "OLE file(MSPowerPoint)";
  case 0x00030005:
    return "OLE file(MSPowerPointSho)";
  case 0x00030006:
    return "OLE file(MSGraph)";
  case 0x00030007:
    return "OLE file(MSDraw)"; // find also ca003 ?
  case 0x00030008:
    return "OLE file(Note-It)";
  case 0x00030009:
    return "OLE file(WordArt)";
  case 0x0003000a:
    return "OLE file(PBrush)";
  case 0x0003000b:
    return "OLE file(Microsoft Equation)"; // "Microsoft Equation Editor"
  case 0x0003000c:
    return "OLE file(Package)";
  case 0x0003000d:
    return "OLE file(SoundRec)";
  case 0x0003000e:
    return "OLE file(MPlayer)";
    // MS Demos
  case 0x0003000f:
    return "OLE file(ServerDemo)"; // "OLE 1.0 Server Demo"
  case 0x00030010:
    return "OLE file(Srtest)"; // "OLE 1.0 Test Demo"
  case 0x00030011:
    return "OLE file(SrtInv)"; //  "OLE 1.0 Inv Demo"
  case 0x00030012:
    return "OLE file(OleDemo)"; //"OLE 1.0 Demo"

    // Coromandel / Dorai Swamy / 718-793-7963
  case 0x00030013:
    return "OLE file(CoromandelIntegra)";
  case 0x00030014:
    return "OLE file(CoromandelObjServer)";

    // 3-d Visions Corp / Peter Hirsch / 310-325-1339
  case 0x00030015:
    return "OLE file(StanfordGraphics)";

    // Deltapoint / Nigel Hearne / 408-648-4000
  case 0x00030016:
    return "OLE file(DGraphCHART)";
  case 0x00030017:
    return "OLE file(DGraphDATA)";

    // Corel / Richard V. Woodend / 613-728-8200 x1153
  case 0x00030018:
    return "OLE file(CorelPhotoPaint)"; // "Corel PhotoPaint"
  case 0x00030019:
    return "OLE file(CorelShow)"; // "Corel Show"
  case 0x0003001a:
    return "OLE file(CorelChart)";
  case 0x0003001b:
    return "OLE file(CorelDraw)"; // "Corel Draw"

    // Inset Systems / Mark Skiba / 203-740-2400
  case 0x0003001c:
    return "OLE file(HJWIN1.0)"; // "Inset Systems"

    // Mark V Systems / Mark McGraw / 818-995-7671
  case 0x0003001d:
    return "OLE file(MarkV ObjMakerOLE)"; // "MarkV Systems Object Maker"

    // IdentiTech / Mike Gilger / 407-951-9503
  case 0x0003001e:
    return "OLE file(IdentiTech FYI)"; // "IdentiTech FYI"
  case 0x0003001f:
    return "OLE file(IdentiTech FYIView)"; // "IdentiTech FYI Viewer"

    // Inventa Corporation / Balaji Varadarajan / 408-987-0220
  case 0x00030020:
    return "OLE file(Stickynote)";

    // ShapeWare Corp. / Lori Pearce / 206-467-6723
  case 0x00030021:
    return "OLE file(ShapewareVISIO10)";
  case 0x00030022:
    return "OLE file(Shapeware ImportServer)"; // "Spaheware Import Server"

    // test app SrTest
  case 0x00030023:
    return "OLE file(SrvrTest)"; // "OLE 1.0 Server Test"

    // test app ClTest.  Doesn't really work as a server but is in reg db
  case 0x00030025:
    return "OLE file(Cltest)"; // "OLE 1.0 Client Test"

    // Microsoft ClipArt Gallery   Sherry Larsen-Holmes
  case 0x00030026:
    return "OLE file(MS_ClipArt_Gallery)";
    // Microsoft Project  Cory Reina
  case 0x00030027:
    return "OLE file(MSProject)";

    // Microsoft Works Chart
  case 0x00030028:
    return "OLE file(MSWorksChart)";

    // Microsoft Works Spreadsheet
  case 0x00030029:
    return "OLE file(MSWorksSpreadsheet)";

    // AFX apps - Dean McCrory
  case 0x0003002A:
    return "OLE file(MinSvr)"; // "AFX Mini Server"
  case 0x0003002B:
    return "OLE file(HierarchyList)"; // "AFX Hierarchy List"
  case 0x0003002C:
    return "OLE file(BibRef)"; // "AFX BibRef"
  case 0x0003002D:
    return "OLE file(MinSvrMI)"; // "AFX Mini Server MI"
  case 0x0003002E:
    return "OLE file(TestServ)"; // "AFX Test Server"

    // Ami Pro
  case 0x0003002F:
    return "OLE file(AmiProDocument)";

    // WordPerfect Presentations For Windows
  case 0x00030030:
    return "OLE file(WPGraphics)";
  case 0x00030031:
    return "OLE file(WPCharts)";

    // MicroGrafx Charisma
  case 0x00030032:
    return "OLE file(Charisma)";
  case 0x00030033:
    return "OLE file(Charisma_30)"; // v 3.0
  case 0x00030034:
    return "OLE file(CharPres_30)"; // v 3.0 Pres
    // MicroGrafx Draw
  case 0x00030035:
    return "OLE file(MicroGrafx Draw)"; //"MicroGrafx Draw"
    // MicroGrafx Designer
  case 0x00030036:
    return "OLE file(MicroGrafx Designer_40)"; // "MicroGrafx Designer 4.0"

    // STAR DIVISION
  case 0x000424CA:
    return "OLE file(StarMath)"; // "StarMath 1.0"
  case 0x00043AD2:
    return "OLE file(Star FontWork)"; // "Star FontWork"
  case 0x000456EE:
    return "OLE file(StarMath2)"; // "StarMath 2.0"
  default:
    MWAW_DEBUG_MSG(("OLE::getCLSIDType: Find unknown clsid=%ux\n", clsid[0]));
  case 0:
    break;
  }

  return "";
}
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
