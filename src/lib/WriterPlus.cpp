#include <iomanip> 
#include <iostream>
#include <sstream>
#include <string.h>

#include <math.h>
#include <stdlib.h>
#include "WriterPlus.h"
#include "WPSDataMac.h"
#include "WPSPagesXml.h"
#include "WPSRTF.h"
#include "Tools.h"

#ifndef DONT_USE_WPD
#  include <libwpd-stream/WPXStream.h>
#  include "libwps_internal.h"
#else
#  include "SimpWPXStream.h"
#endif

unsigned long WriterPlus::readULong(int num, unsigned long a) 
{
  return input->readULong(num,a);
}

long WriterPlus::readLong(int num, unsigned long a) 
{
  return input->readLong(num,a);
}

bool WriterPlus::readHeader(bool add)
{
  const int headerSize = 246;

  std::stringstream f;

  input->seek(0,WPX_SEEK_SET);

  hasHeader = hasFooter = false;
  switch (readULong(2)) {
  case 0x110: version = 1; break;
  default: return false;
  }

  if (input->seek(headerSize,WPX_SEEK_SET) != 0 || input->atEOS())
    return false;

  input->seek(2,WPX_SEEK_SET);
  f << "Entete-1: X"<<std::hex<<readULong(2) << std::dec; // 0x8033
  f << ", " << readULong(2); // 378, 472, 660
  f << ", " << readULong(2); // 2

  // s = 0: 1eec|1fdc|2008|200a|200c|2010|2012|201e, 2, i=11|12, i, 2
  // s = 1: 1f1c|1f2c|..|1fee, 0, 10*i, i=1|2|3|4|5, 2
  // s = 2: 1ef0|..|1fec, 0,16*i,i=0|2, 2
  // s = 3: 1eb8|..|1fce, 0,[324-3052], [17-174], 2
  for (int s = 0; s < 7; s++) {
    for (int i = 0; i < 5; i++) {
      int val = readULong(2);
      if (i==0) f << ", X" << std::hex << std::setw(4) << val << std::dec << "(";
      else f << ", " << val;
    }
    f << ")";
  }
  if (add) {
    int debPos = input->tell();
    ascii().setLast(debPos);
    ascii().addPos(0);
    ascii().addNote(f.str().c_str());

    f.str("");
    f << "Entete-1(II): "; 
    for (int i = 0; i < 20; i++)
      f << readLong(2) << ", ";
    ascii().addPos(debPos);
    ascii().addNote(f.str().c_str());

    debPos = input->tell();
    f.str("");
    f << "Entete-1(III): ";
    for (int i = 0; i < 20; i++)
      f << readLong(2) << ", ";
    ascii().addPos(debPos);
    ascii().addNote(f.str().c_str());
  }

  if (input->seek(246, WPX_SEEK_SET) != 0) return false;
  MacData::TPrint info;
  if (!info.read(input)) {
    input->seek(246, WPX_SEEK_SET);
    return true;
  }

  WPSData::Position pageSize = info.paper().size();
  WPSData::Position leftTMargin = -1 * info.paper().pos(0);
  WPSData::Position rightBMargin = info.paper().size() - info.page().size();
  document = WPSData::Document(pageSize, pageSize, leftTMargin, rightBMargin);
  mainFile.setDocument(document);

  f.str("");
  f << std::dec << "Document " << info;

  if (add) {
    asciiFile.setLast(input->tell());
    asciiFile.addPos(246);
    asciiFile.addNote(f.str().c_str());
    asciiFile.addPos(input->tell());
  }

  return true;
}

std::string WriterPlus::readHeadFootString() {
  std::string res;
  return res;
}

bool WriterPlus::getZoneInfo(WriterPlus::ZoneInfo &info, int check) {
  if (input->atEOS()) return false;

  long debPos = input->tell();
  info = ZoneInfo();

  info.s1 = readULong(1);
  bool shortEntete = (info.s1%2)==0;

  std::stringstream f;
  if (shortEntete) {
    if (info.s1 < 4 || info.s1 > 40) return false;

    if (readLong(1) != 0) return false;
    if (info.s1!=4) {
      if (input->seek(info.s1-4, WPX_SEEK_CUR) != 0) return false;
      f << "###";
    }
    info.i1 = readLong(2);
    if (info.i1 < 0) return false;
    if (check == 49 && info.i1 > 256) return false;
    f << "EnteteL"<<std::hex << info.s1<< std::dec << ": "<<info.i1;
  }
  else {
    info.i1 = readLong(2);
    info.numLines = readLong(1);

    f << "EnteteL"<<std::hex << info.s1<< std::dec << ": "
      <<info.i1 << ", NL=" << info.numLines;
    
    if (info.s1 == 0 || info.i1 < 0 || info.i1 > 2000 || 
	info.numLines <= 0 || info.numLines >= 0x30) return false;
    if (check == 49 && info.i1 > 256) return false;

    info.height = readLong(2);
    if (info.height < 0 || info.height > 1000) return false;
    info.pos = readULong(4);
    if (info.pos <= debPos || info.pos >= input->size()) return false;
    if (readLong(1)) {
      if (check == 49) return false; 
      f << "###";
    }
    info.w = readLong(2);
    if (info.w < 0 ||
	(info.w > 1000 && info.w > document.width())) return false;
    
    f << std::hex << ", P=" << info.pos
      << ", h=" << std::dec << info.height << ", w="
      << info.w << std::hex << ",(";
    // 4 * a counter follow by 0 ou 0x12
    for (int i = 0; i < 2; i++) {
      int v = readULong(1);
      f << v << ", ";
      info.auxi1.push_back(v);
    }
    f << "), (";
    
    int numD = info.numLines == 1 ? 1 : 1+info.numLines;
    for (int i = 0; i < numD; i++) {
      int v = readULong(1);
      info.firstXPos.push_back(v);
      f << v << ", ";
    }
    
    f << ")";
  }
    
  if (check < 99) return true;
    
  ascii().setLast(input->tell());
  ascii().addPos(debPos);
  ascii().addNote(f.str().c_str());
  ascii().addPos(input->tell());

  if (info.pos > 0) {
    ascii().setLast(info.pos);
    ascii().addPos(info.pos);
    f.str("");
    f << "_" << std::hex << debPos;
    ascii().addNote(f.str().c_str());
  }
  return true;
}

bool WriterPlus::readPagesInfo
(std::vector<WriterPlus::PageInfo> &pages, std::vector<WriterPlus::ColumnInfo> &cols, int check)
{
  pages.resize(0);
  cols.resize(0);

  long debPos = input->tell();
  PageInfo info;
  long prevPos = 0;
  bool first = true;

  while (1) {
    info.n = readLong(2);
    if (info.n <= 0) break;
    if (first && info.n != 1) break;

    if (readLong(2) != 1) break;
    info.deca = readLong(4);
    if (first) { if (info.deca != 0) break; }
    else if (info.deca <= prevPos) break;
    prevPos = info.deca;
    info.height = readLong(2);
    if (info.height < 0 || info.height > 1000)
      if (info.height > document.pageHeigth()) break;

    pages.push_back(info);

    if (check == 99) {
      std::stringstream f;
      f << "EnteteP: dec=" << std::hex << info.deca << std::dec
	<< ", N=" << info.n << ", height=" << info.height;

      ascii().setLast(debPos);
      ascii().addPos(debPos);
      ascii().addNote(f.str().c_str());
    }
    first = false;
    debPos = input->tell();
  }
  input->seek(debPos, WPX_SEEK_SET);
  if (pages.size() == 0) return false;

  bool hasColumn = readLong(1) == 0;
  input->seek(debPos, WPX_SEEK_SET);
  if (!hasColumn) return true;
  
  // maybe follow some part for the columns
  // in the only file I have, this look like :
  // 0001000000020017 0001000001950082
  // 000200000002001b 0001000001dc0086
  // can this be interleaved ?

  prevPos = -1;
  
  while(1) {
    ColumnInfo col;
    col.col = readLong(2);
    if (col.col <= 0 || col.col > 100) break;
    if (readLong(2)) break;
    col.nCol = readLong(2);
    if (col.nCol <= 0 || col.nCol > 100) break;
    col.n = readLong(2);
    col.i1 = readLong(2);
    col.deca = readLong(4);
    if (col.deca <= prevPos) break;
    prevPos = col.deca;
    col.height = readLong(2);

    if (col.height < 0 || col.height > 1000)
      if (col.height > document.pageHeigth()) break;

    cols.push_back(col);

    if (check == 99) {
      std::stringstream f;
      f << "EnteteC: dec=" << std::hex << col.deca << std::dec
	<< ", col=" << col.col << "/" << col.nCol << ", height=" << col.height
	<< ", N=" << col.n << ", " << col.i1;

      ascii().setLast(debPos);
      ascii().addPos(debPos);
      ascii().addNote(f.str().c_str());
    }
    debPos = input->tell();
  }

  input->seek(debPos, WPX_SEEK_SET);
  if (cols.size() == 0) return false;
  return true;
}

bool WriterPlus::createInfoZones(WriterPlus::PartInfo &part, int check) {
  part.pages.resize(0);
  part.columns.resize(0);
  part.zones.resize(0);
  part.begPos = input->tell();

  if (!readPagesInfo(part.pages, part.columns, check)) return false;
  long debPos = input->tell();

  ZoneInfo zone;
  long lastPos = -1;
  bool ok = false;

  while(!input->atEOS()) {
    debPos = input->tell();
    if (debPos == lastPos) {
      ok = true;
      break;
    }

    if (!getZoneInfo(zone,check)) return false;

    int numZ = part.zones.size();
    if (zone.pos < 0) {
      if (zone.i1 >= 0 && numZ)
	part.zones[numZ-1].fHeight = zone.i1;
      else if (check == 99) {
	ascii().setLast(debPos);
	ascii().addPos(debPos);
	ascii().addNote("###");

      }
      continue;
    }

    if (!numZ) lastPos = zone.pos;
    else if (zone.pos < part.zones[numZ-1].pos) return false;

    part.zones.push_back(zone);
    if (check < 99) continue;

    std::stringstream f;
    f << numZ++;
    
    ascii().setLast(debPos);
    ascii().addPos(debPos);
    ascii().addNote(f.str().c_str());
  }

  int numZ = part.zones.size();
  if (!ok || numZ == 0) return false;
  input->seek(debPos, WPX_SEEK_SET);

  for (int i = 0; i < int(part.pages.size()); i++) {
    int n = part.pages[i].n;
    if (n > numZ) return false;
    part.zones[n-1].lineId = i;
  }
  for (int i = 0; i < int(part.columns.size()); i++) {
    int n = part.columns[i].n;
    if (n > numZ) return false;
    part.zones[n-1].columnId = i;
  }
  part.endPos = part.zones[numZ-1].pos;

  if (check < 99) return true;
  ascii().setLast(debPos);
  ascii().addPos(debPos);

  return true;
}

bool WriterPlus::createZones() {
  listZones.resize(0);
 
  long totalPartSize = 0, maxNumZone = 0;
  while (!input->atEOS()) {
    long debPos = input->tell();

    PartInfo pInfo;
    if (!createInfoZones(pInfo, 2)) {
      input->seek(debPos+1,WPX_SEEK_SET);
      continue;
    }

    input->seek(debPos,WPX_SEEK_SET);
    if (!createInfoZones(pInfo, 99)) {
      input->seek(debPos+1,WPX_SEEK_SET);
      continue;
    }
    debPos = pInfo.endPos;
    input->seek(debPos,WPX_SEEK_SET);

    ArrayInfo info;
    if (!readZone(0L, info, 49)) input->seek(debPos,WPX_SEEK_SET);
    else pInfo.endPos = input->tell();

    totalPartSize += (pInfo.endPos-pInfo.begPos);
    if (int(pInfo.zones.size()) > maxNumZone) maxNumZone = pInfo.zones.size();
    listZones.push_back(pInfo);
  }
  if (listZones.size() == 0) return false;

  if (maxNumZone > 20) return true;
  return totalPartSize > int(input->size())/2;
}

bool WriterPlus::createPositions(int n, int actZone, int &actualD,
				 WriterPlus::DataInfo *fData)
{
  assert (fData != 0);
  assert ((n > 0) ^ (fData->father == 0L));

  DataInfo *prevData = 0L;

  PartInfo &part = listZones[actZone];

  long debPos = input->tell();
  bool ok = true;
  int d = actualD;
  int numZ = part.zones.size();
  int lastX = -1;
  int prevColPos = 0;

  int actualPage = fData->page;
  int actualY = fData->gY, actualLY = fData->y;
  
  for (int i = 0; n == -1 || i < n; i++) {
    if (d >= numZ) { ok = n == -1; break; }

    long actPos = input->tell();
    while (d < numZ-1 && part.zones[d+1].pos <= actPos) d++;

    ZoneInfo &zone = part.zones[d];
    bool okZone = zone.pos == actPos;
    bool newPage = okZone && zone.lineId!=-1;
    bool newCol = okZone && zone.columnId!=-1;

    if (newPage && d) {
      actualLY = 0;
      actualPage++; 
      std::cerr << actualPage << "<-" << d << "\n";
    }
    else if (newCol) actualLY = 0;

    // read the zone
    ArrayInfo nInfo;
    if (!readZone(&zone, nInfo, 2)) { ok = (n == -1 && d == numZ-1); break; }

    DataInfo *info = new DataInfo;
    info->page = actualPage;
    info->pos = actPos;
    info->father = fData;
    if (zone.pos == actPos) info->zone = &zone;

    int newX = nInfo.x;
    bool newColumn = (n > 0 && newX != lastX);
    if (prevData && !newColumn)
      prevData->nextData = info;
    else if (newColumn) {
      if (prevData) fData->columnsSize.push_back(newX-prevColPos);
      else fData->gX = newX;
      prevColPos = lastX = newX;
      actualPage = fData->page;
      actualLY = fData->y;
      actualY = fData->gY;
      fData->columnsData.push_back(info);
    }
    else 
      fData->columnsData.push_back(info);

    info->gX = nInfo.x;
    info->gY = actualY;
    info->y = actualLY;
    info->h = nInfo.height;
    if (okZone && zone.fHeight > 0)
      info->aftH = zone.fHeight;

    // and retrieve it
    if (nInfo.n) {
      if (!createPositions(nInfo.n, actZone, d, info)) { ok = false; break; }
      info->getMaxY(actualY, actualLY, actualPage);
    }
    else {
      actualY += info->h;//+info->aftH;
      actualLY += info->h;//+info->aftH;
    }
    
    prevData = info;
  }

  if (!ok) {
    input->seek(debPos, WPX_SEEK_SET);
    return false;
  }

  actualD = d;
  
  return true;
}
    
bool WriterPlus::readPositions(DataInfo const *info, int decX)
{
  if (info->pos >0) {
    std::stringstream f;
    ascii().setLast(info->pos);
    ascii().addPos(info->pos);
    f << "Global=(" << info->gX << ", " << info->gY << "), Local=("
      << info->page << "," << info->y << ")," << "W=" << info->w;
    for (int i = 0; i < int(info->columnsSize.size()); i++)
      f << ", [" << info->columnsSize[i] << "]";
    ascii().addNote(f.str().c_str());
  }

  int numC = info->columnsData.size();
  if (info->zone && info->zone->lineId != -1) 
    mainFile.addBreak(WPSVFile::PAGEBREAK, 0);

  if (!numC)
    readData(info, decX, 99);
  else {
    ArrayInfo inf;
    readArrayZone(info->zone, inf, 99);

    for(int i = 0; i < numC; i++)
      readPositions(info->columnsData[i], decX);
  }
  if (info->aftH > 0) mainFile.addBreak(WPSVFile::LINEBREAK, info->aftH);

  if (info->nextData) readPositions(info->nextData, decX);

  return true;
}

bool WriterPlus::readData(DataInfo const *zone, int decX, int check)
{
  ArrayInfo info;

  input->seek(zone->pos, WPX_SEEK_SET);
  WPSData::Position xw(decX+zone->gX,zone->w);
  if (readText(xw, zone->zone, info, check)) return true;
  input->seek(zone->pos, WPX_SEEK_SET);

  return readGraphics(WPSData::Position(decX+zone->gX, 0), zone->gY, check);
}

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////
bool WriterPlus::readZone(ZoneInfo const *zone, ArrayInfo &info, int check)
{
  long debPos = input->tell();
  WPSData::Position xw(-1,-1);
  if (readText(xw, zone, info, check)) return true;
  input->seek(debPos, WPX_SEEK_SET);
  if (readArrayZone(zone, info, check)) return true; 
  input->seek(debPos, WPX_SEEK_SET);
  bool ok = readGraphics(WPSData::Position(info.x, actualPageHeight),
			 debPageHeight, check);
  if (check >= 99) info = ArrayInfo();
  if (ok) return true;
  input->seek(debPos, WPX_SEEK_SET);
  return false;
}

bool WriterPlus::readZones() {
  WPSData::SmallText text;
  std::vector<WPSData::Font> fonts;
  std::vector<int> pos;

  int numZones = listZones.size();
  int headerZone = -1, footerZone = -1, mainZone = -1;

  if (numZones == 1) mainZone = 0;
  else if (numZones == 2) {
    if (listZones[1].maxHeight() >= listZones[0].maxHeight()) {
      headerZone = 0; // or the footer Zone
      mainZone = 1;
    }
  }
  else if (numZones == 3) {
    if (listZones[2].maxHeight() > listZones[0].maxHeight() &&
	listZones[2].maxHeight() > listZones[1].maxHeight()) {
      headerZone = 0; footerZone = 1; mainZone = 2;
    }
  }

  if (mainZone >= 0) {
    PartInfo const & part = listZones[mainZone];
    pagesHeight.resize(0);
    for (int i = 0; i < int(part.pages.size()); i++)
      pagesHeight.push_back(part.pages[i].height);
    main().setPagesHeight(pagesHeight);
  }

  for (actualZone = 0; actualZone < numZones; actualZone++) {
    PartInfo &part = listZones[actualZone];

    writeTextInTmpZone = (actualZone == headerZone) ||
      (actualZone == footerZone);
    if (writeTextInTmpZone) tmpZone.reset();

    debPageHeight = actualPageHeight = 0;
    int actualPage = 0;
    ArrayInfo posInfo;

    int numZ = part.zones.size();
    bool posOk = false;
    if (numZ) {
      DataInfo lInfo;
      int d = 0;
      input->seek(part.zones[0].pos, WPX_SEEK_SET);
      posOk = createPositions(-1, actualZone, d, &lInfo);
      if (posOk) {
	std::cerr << "Ok for zone " << actualZone << "\n";
	lInfo.simplify();
	lInfo.updateW(document.textWidth());
	std::cerr << lInfo.minGX() << "\n";
	if (!writeTextInTmpZone && readPositions(&lInfo, -lInfo.minGX())) continue;
      }
      else
	std::cerr << "Not ok for zone " << actualZone << "\n";
    }
    for (int z = 0; z < numZ; z++) {
      long lastPos = -1;
      if (z != numZ-1) lastPos = part.zones[z+1].pos;
      else if (actualZone+1 <= numZones-1)
	lastPos = listZones[actualZone+1].begPos;

      ZoneInfo &info = part.zones[z];
      if (input->seek(info.pos, WPX_SEEK_SET) != 0) break;
      
      if (z && info.lineId != -1) {
	mainFile.addBreak(WPSVFile::PAGEBREAK, 0);
	if (actualPage < int(part.pages.size()))
	  debPageHeight += part.pages[actualPage].height;
	else
	  debPageHeight += actualPageHeight;

	actualPageHeight = 0;
	actualPage++;
      }
      if (info.columnId != -1) 
	mainFile.addBreak(WPSVFile::COLUMNBREAK, 0);
      

      long debPos = input->tell();
      
      while (!input->atEOS() && input->tell() != lastPos) {
	debPos = input->tell();
	if (readZone(&info, posInfo, 99)) continue;

	ascii().setLast(debPos);
	ascii().addPos(debPos);
	if (lastPos == -1)
	  ascii().addNote("_");
	else
	  ascii().addNote("Entete###");
	break;
      }
      if (info.fHeight > 0) {
	if (writeTextInTmpZone)
	  tmpZone.addLine(WPSData::SmallText(), info.fHeight);
	else
	  mainFile.addBreak(WPSVFile::LINEBREAK, info.fHeight);
      }
	
    }
    if (writeTextInTmpZone) {
      if (actualZone == headerZone)
	main().addHeader(tmpZone, 40);
      else if (actualZone == footerZone)
	main().addFooter(tmpZone, 40);
      else if (tmpZone.getNumData())
	main().addText(tmpZone);
      tmpZone.reset();
    }
  }

  return true;

}

bool WriterPlus::loopZones() {
  WPSData::SmallText text;
  std::vector<WPSData::Font> fonts;
  std::vector<int> pos;
  bool prevOk = false;
  writeTextInTmpZone = false;
  debPageHeight = actualPageHeight = -1;

  ArrayInfo posInfo;
  WPSData::Position xw(-1,-1);
  while (!input->atEOS()) {
    long debPos = input->tell();

    if (readText(xw, 0L, posInfo, 99)) { prevOk = true; continue; }
    input->seek(debPos, WPX_SEEK_SET);
    if (prevOk && readZone(0L, posInfo, 99)) continue;
    if (prevOk) {
      ascii().setLast(debPos);
      ascii().addPos(debPos);
      ascii().addNote("Entete###");
    }
    input->seek(debPos+1, WPX_SEEK_SET);
    posInfo = ArrayInfo();
    prevOk = false;
  }
  return true;

}

////////////////////////////////////////////////////////////
//
// Graphics
//
////////////////////////////////////////////////////////////
bool WriterPlus::readGraphics
(WPSData::Position const &pos, int pY, int check) {
  long debPos = input->tell();
  if (readLong(2) != 0) return false;

  int size = readULong(2);
  
  WPSData::Position pictOrig, pictSize;
  bool setPos;
  int type, version;
  if (!Tools::getDataFileType(input, size, type, version,
			      pictOrig, pictSize, setPos))
    return false;

  if (check == 49 && type == Tools::EMPTY) return false;

  input->seek(debPos+4+size, WPX_SEEK_SET);
  if (check < 99) return true;
  std::stringstream f;
   
  f << "EnteteG("<< type <<")";

  if (setPos) {
    f << ": pos=(" << pictOrig.x() << ", " << pictOrig.y() << ")"
      << ", size=(" << pictSize.x() << ", " << pictSize.y() << ")";
    if (phase == 0 && type != Tools::EMPTY) {
      std::string bName = Tools::getUniqueName(type);
      std::string fName(directory); fName+='/'; fName+=bName;
      input->seek(debPos+4, WPX_SEEK_SET);
      Tools::saveBinaryFile(input, fName, size);

      if (pY >= 0) {
	if (pos.y()+pictSize.y() <= document.pageHeigth())
	  pictOrig.moveToY(pY+pos.y());
	else
	  pictOrig.moveToY(pY+pos.y()+(document.pageHeigth()-pictSize.y()));
      } 
      if (pos.x() >= 0) pictOrig.moveToX(pos.x());

      main().addAuxilliary(WPSData::FileAux(bName,pictOrig, pictSize, pictSize));
    }
    input->seek(debPos+4+size, WPX_SEEK_SET);
  }

  ascii().setLast(debPos+4+size);
  ascii().addPos(debPos);
  ascii().addNote(f.str().c_str());
  ascii().addPos(debPos+4+size);

  asciiFile.skipZone(debPos+4,debPos+3+size);
  return true;
}

////////////////////////////////////////////////////////////
//
// 0x30: A cell, a table ?
// 0x40: ?
//
////////////////////////////////////////////////////////////
bool WriterPlus::readArrayZone(ZoneInfo const */*zone*/, ArrayInfo &info, int check) {
  long debPos = input->tell();
  int size = readLong(4);
  if (size != 0x30 && size != 0x40) return false;

  std::stringstream f;
  f << "Entete"<<size<<":";

  ArrayInfo newInfo;
  for (int i = 0; i < size/8; i++) {
    f << "(";
    for (int v = 0; v < 4; v++) {
      int val = readLong(2);
      if (i == 1 && v == 0) { newInfo.x = val; f << "x=";}
      else if (i == 0 && v == 1 ) { newInfo.height = val; f << "height=";}
      else if (i == 1 && v == 1) { newInfo.n = val; f << " n="; }
      if (val < 0 || val > 10000) {
	if (check == 49 || (4*i+v) < 10) return false;
      }
      f << val;
      if (v != 3) f << "x";
    }
    f << "), ";
  }

  info = newInfo;
  if (check < 99) return true;
  long endPos = input->tell();
  ascii().setLast(endPos);
  ascii().addPos(debPos);
  ascii().addNote(f.str().c_str());
  ascii().addPos(endPos);
  return true;
}

//////////////////////////////////////////////////////////////
//
// Read a paragraph (font+linebreak) then the text
//
//////////////////////////////////////////////////////////////
bool WriterPlus::readFonts(int nbChar, std::vector<WPSData::Font> &fonts, std::vector<int> &pos,
			   std::vector<WriterPlus::LineInfo> &linesInfo, int check) {
  fonts.resize(0);
  pos.resize(0);
  linesInfo.resize(0);
  std::stringstream f;
  f << "Fonts(";
  long debPos = input->tell();
  int id[3];
  for (int i = 0; i < 3; i++) {
    id[i] = input->readLong(2);
    if (id[i] > 100 || id[i] <= 0) return false;
    f << id[i] << ",";
  }
  f << "):";

  int posi = 0;
  for (int i = 0; i < id[1]; i++) {
    int v = input->readLong(2);
    if (v !=0) {
      if (check == 49) return false;
      f << "[" << v << "]";
    }
    if (input->readLong(2) !=0 ) return false;
    int fId = input->readLong(2);
    if (fId < 0 || fId > 255) return false;
    f << " (" << fId;
    int flags[2];
    for (int i = 0; i < 2; i++) {
      flags[i] = input->readULong(1);
      f << std::hex << ", " << flags[i] << std::dec;
    }
    int fSize = input->readLong(2);
    int nChar = input->readLong(2);
    if (fSize <= 0 || nChar < 0 || fSize > 50) return false;
    f << ", " << fSize << " ) ";

    for (int i = 0; i < 2; i++) {
      int val = input->readLong(2);
      if (check == 49 && (val < 0 || val > 255)) return false;
    }
    fonts.push_back(WPSData::Font(fId, fSize, flags[0]));
    pos.push_back(posi);
    posi += nChar;
  }
  if (posi != nbChar) return false;

  posi = 0;
  for (int i = 0; i < id[2]; i++) {
    long pos = input->tell();
    std::stringstream fU;
    fU << "Line" << i << ": ";

    LineInfo info;
    info.fl = readLong(2);
    info.height = readLong(2);
    info.x = readLong(2);
    info.pos = readLong(2);
    if (info.fl < 0 || info.pos < 0 || info.height > 100) return false;
    fU << "char=" << info.pos << ", h=" << info.height
       << ", fl=" << info.fl << ", xPos=" << info.x;
    posi = (info.pos += posi);
    for (int v = 0; v < 4; v++) {
      info.flags[v] = readLong(2);
      if (info.flags[v] == 0) continue;
      if (check == 49) return false;
      fU << ", i" << v <<"=" << info.flags[v];
    }
    linesInfo.push_back(info);

    if (check >= 99) {
      ascii().setLast(pos);
      ascii().addPos(pos);
      ascii().addNote(fU.str().c_str());
    }
  }
  if (posi != nbChar) return false;

  if (check < 99) return true;

  long endPos = input->tell();
  ascii().setLast(endPos);
  ascii().addPos(debPos);
  ascii().addNote(f.str().c_str());
  ascii().addPos(endPos);
  return true;
}

bool WriterPlus::readText(WPSData::Position const &xw, ZoneInfo const *zone,
			  ArrayInfo &info, int check) {
  WPSData::SmallText txt;
  long debPos = input->tell();
  int size = input->readLong(2);
  int size2 = input->readLong(2);

  std::stringstream f;
  f << "Text-"<<size;
  if (size < 0 || size2 < 0) return false;

  f << "(" << size2 << ")" << std::hex;
  std::string str;
  for (int i = 0; i < size; i++) {
    if (input->atEOS()) return false;
    unsigned char c = input->readULong(1);
    if (c < 9) return false;
    str += c;
  }
  f << "[";
  int fl[5];
  for (int i = 0; i < 5; i++) {
    fl[i] = input->readLong(2);
    f << fl[i] << ",";
  }
  ArrayInfo newInfo;
  newInfo.n = 0;
  newInfo.height = fl[1] > 0 ? fl[1] : 0;
  newInfo.x = fl[4] > 0 ? fl[4] : 0;

  std::vector<WPSData::Font> fonts;
  std::vector<int> pos, fontN;
  std::vector<LineInfo> linesInfo;
  if (!readFonts(size, fonts, pos, linesInfo, check)) return false;

  if (input->tell() != debPos + size+size2+4)
    return false;
  info = newInfo;
  if (check < 99) return true;

  int nbFonts = fonts.size();
  fontN.resize(nbFonts);
  for (int i = 0; i < nbFonts; i++) fontN[i] = i;
  txt = WPSData::SmallText(str, fonts, pos, fontN);

  int nbChar = str.length();

  std::vector<int> auxIndents;
  int debX = xw.x() >= 0 ? xw.x() : newInfo.x;

  int w = xw.y() > 0 ? xw.x()+xw.y() : zone ? zone->w : -1;
  if (w <= 0) w = document.textWidth();

  int numLines = linesInfo.size();
  for (int i = 0; i < numLines; i++) {
    int fPos = i == 0 ? 0 : linesInfo[i-1].pos;
    int lPos = i == numLines-1 ? nbChar-1 : linesInfo[i].pos-1;

    // update the indents
    if (zone && i < int(zone->firstXPos.size())) {
      int x = debX+zone->firstXPos[i];
      WPSData::Indent idt (x,x,w,auxIndents);

      if (writeTextInTmpZone)
	tmpZone.addIndent(idt);
      else
	main().setIndents(idt);
    }
      
    // put the text
    if (writeTextInTmpZone)
      tmpZone.addLine(txt.getSubText(fPos,lPos), linesInfo[i].height);
    else {
      main().addText(txt.getSubText(fPos,lPos));
      main().addBreak(WPSVFile::LINEBREAK, linesInfo[i].height); 
    }
  }
  
  f << "],'" << str << "'";

  if (actualPageHeight>=0) actualPageHeight+= newInfo.height;
  long endPos = input->tell();
  ascii().setLast(endPos);
  ascii().addPos(debPos);
  ascii().addNote(f.str().c_str());
  ascii().addPos(endPos);
  return true;
}

bool WriterPlus::parseInput(char const *direc)
{
  directory=std::string(direc);
  phase = 0;

  mainFile.addChild(new WPSRtf);
  mainFile.addChild(new WPSXml);

  if (!readHeader(false)) return false;
  long debPos = input->tell();
  if (createZones()) readZones();
  else {
    input->seek(debPos, WPX_SEEK_SET);
    loopZones();
  }

  phase = 1;
  // create the note file
  std::string noteName(directory);noteName+="/notes";
  notesFile.open(noteName);
  notesFile.header(0,0);
  notesFile.setFont(WPSData::Font(20,12,1));
  notesFile.addString(std::string("\t\t\tDIVERS NON COMPRIS\n\n\n\n"));
  notesFile.setFont(WPSData::Font(20,12,0));

  // create the rtf file
  std::string mainName(directory);mainName+="/text";
  mainFile.open(mainName);
  mainFile.header(0,0);

  // create the asciiFile
  std::string name(directory);name+="/auxillary/main-1";
  asciiFile.setStream(input);
  asciiFile.open(name);

  readHeader(true);
  if (createZones()) readZones();
  else {
    input->seek(debPos, WPX_SEEK_SET);
    loopZones();
  }

  if (WPSData::DebugMessage::message().size()) {
    std::string mes("EnteteFont: ");
    mes += WPSData::DebugMessage::message();
    ascii().addPos(0);
    ascii().writeAndAddRem(mes);
  }
  ascii().setLast(-1);
  ascii().reset();

  return true;

}

bool WriterPlus::checkHeader(WPXInputStream *input)
{
  input->seek(0,WPX_SEEK_SET);
  
  int val = readU8(input)<<8;
  val += readU8(input);
  switch (val) {
  case 0x110:
    version = 1;
    return true;
  }
  return false;
}
