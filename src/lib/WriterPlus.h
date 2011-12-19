#ifndef WRITERPLUS
#  define WRITERPLUS

#include <string.h>
#include <vector>

#include "WPSAsciiDeb.h"
#include "WPSGroupFile.h"
#include "WPSRTF.h"

class WPXInputStream;

class WriterPlus {
 public:
  WriterPlus(WPXInputStream *inp) : input(inp) {}

  bool checkHeader(WPXInputStream *input);
  bool readHeader(bool add);
  bool parseInput(char const *directory);

  bool createZones();

 protected:
  struct LineInfo;

  struct DataInfo;
  struct ArrayInfo;

  struct ColumnInfo;
  struct PageInfo;
  struct ZoneInfo;
  struct PartInfo;

  // check:
  // >= 0 : entete check,
  // >= 1 : position
  // >= 2 : check also the data 
  // == 49 : check if not sure of the data
  // >= 99 : read file
  bool createInfoZones(PartInfo &part, int check);
  bool getZoneInfo(ZoneInfo &zone, int check);
  bool readPagesInfo(std::vector<PageInfo> &pages,
		     std::vector<ColumnInfo> &cols, int check);

  bool createPositions(int n, int actZone, int &actualD, DataInfo *fData);
  bool readPositions(DataInfo const *info, int decX);
  bool readData(DataInfo const *zone, int decX, int check);

  // idem for text or ident zone
  bool getNewTextZone(WPSData::Zone &zone, int check);
  bool readText(WPSData::Position const &xw, ZoneInfo const *zone,
		ArrayInfo &info, int check);
  bool readFonts(int nbChar,
		 std::vector<WPSData::Font> &fonts,
		 std::vector<int> &pos, std::vector<LineInfo> &info,
		 int check);
  
  bool readZone(ZoneInfo const *zone, ArrayInfo &info, int check);
  bool readArrayZone(ZoneInfo const *zone, ArrayInfo &info, int check);

  bool readGraphics(WPSData::Position const &pos, int py, int check);

  // read the string which can appear in entete2
  std::string WriterPlus::readHeadFootString();

  bool readZones();
  bool loopZones();

  unsigned long readULong(int num, unsigned long a = 0);
  long readLong(int num, unsigned long a = 0);

  WPSVFile &main() { return mainFile; }
  WPSAsciiFile &ascii() { return asciiFile; }
  
  struct DataInfo {
    DataInfo() {
      nextData = father = 0L;
      zone = 0L;
      y = gX = gY = h = aftH = w = 0;
      page = 1;
      column = 0;
    }
    ~DataInfo() {
      for (int i = 0; i < int(columnsData.size()); i++)
	delete columnsData[i];
      if (nextData) delete nextData;
    }

    void simplify() {mergeChildData(); }
    void updateW(int maxX) {
      w = maxX-gX;

      int numCols = columnsData.size();
      int aMax = gX;
      for (int i = 0; i < numCols; i++) {
	DataInfo *child = columnsData[i];
	if (i != numCols-1)
	  aMax += columnsSize[i];
	else
	  aMax = maxX;
	child->updateW(aMax);
      }
      if (nextData) nextData->updateW(maxX);
    }

    int minGX() const {
      int nMinX = nextData ? nextData->minGX() : -1;

      int numColumns = columnsData.size();
      if (numColumns == 0) return (nMinX >= 0 && nMinX < gX) ? nMinX : gX;

      DataInfo const *child = columnsData[0];
      int res = child->minGX();
      if (nMinX >= 0 && nMinX < res) res = nMinX;
      while (child->nextData) {
	child = child->nextData;
	int cX = child->minGX();
	if (cX < res) res = cX;
      }
      return res;
    }

    void getMaxY(int &mGY, int &mY, int &mP) const {
      DataInfo const *lastCell = this;
      while (lastCell->nextData) lastCell = lastCell->nextData;
      int totH = lastCell->h+lastCell->aftH;
      mGY = lastCell->gY+totH; mY = lastCell->y+totH; mP = lastCell->page;
      int numColumns = lastCell->columnsData.size();
      for (int c = 0; c < numColumns; c++) {
	int cGY, cY, cP;
	lastCell->columnsData[c]->getMaxY(cGY, cY, cP);
	if (cGY >= mGY) mGY = cGY;
	if (cP > mP) { mY = cY; mP = cP; }
	else if (cP == mP && cY>mY) mY = cY;
      }
    }

    long pos;
    int y, gX, gY, h, aftH, w, page, column;
    std::vector<int> columnsSize; // we keep the size of the first columns
    std::vector<DataInfo *> columnsData;

    DataInfo *nextData, *father;
    ZoneInfo *zone;

    protected:
    bool mergeChildData() {
      int numColumns = columnsData.size();
      if (numColumns == 0) return true;

      for (int i = 0; i < numColumns; i++) 
	columnsData[i]->mergeChildData();

      if (numColumns != 1) return true;
      DataInfo *child = columnsData[0];

      while (child) {
	child->mergeChildData();
	child = child->nextData;
      }

      child = columnsData[0];
      pos = child->pos;
      if (child->h > 0) h = child->h;
      gY = child->gY;

      int prevPage = page;
      int decY = 0;
      if (prevPage == child->page) {
	decY = y;
	y = child->y;
      }
      else {
	page = child->page;
	y = child->y;
      }
      
      if (child->gX >= 0) gX = child->gX;

      // update x and y for the other child
      // and the next of the last child
      DataInfo *oldNext = child->nextData;

      while (child->nextData) {
	child = child->nextData;

	child->father = father;
	if (prevPage == child->page)
	  child->y += decY;
      }

      child->nextData = nextData;
      
      // finish to update the node
      child = columnsData[0];
      zone = child->zone;
      
      columnsData = child->columnsData;
      for (int i = 0; i < int(columnsData.size()); i++) {
	DataInfo *c =  columnsData[i];
	while (c) {
	  c->father = this;
	  c = c->nextData;
	}
      }
      columnsSize = child->columnsSize;
      aftH = child->aftH;

      if (oldNext) nextData = oldNext;

      // ok remove the node
      child->columnsData.resize(0);
      child->nextData = 0;
      delete child;

      return true;
    }
  };

  struct PageInfo {
    int n, height;
    long deca;
  };
  struct ColumnInfo {
    int col, nCol, n, i1, height;
    long deca;
  };
  struct LineInfo {
    int fl, height, x, pos;
    int flags[4];
  };

  struct ArrayInfo {
    ArrayInfo() : n(0), height(0), x(-1) {}
    int n, height, x;
  };

  struct ZoneInfo {
    ZoneInfo() { 
      pos = -1; w = height = fHeight = -1;
      lineId = columnId = -1;
      numLines = 0;
    }
    
    int height, fHeight, w, numLines;
    int s1, i1;
    long pos;
    int lineId, columnId;
    std::vector<int> auxi1, firstXPos; // auxi1[0] 
  };
  struct PartInfo {
    int maxHeight() const {
      int res=0;
      for (int i = 0; i < int(pages.size()); i++)
	if (pages[i].height > res) res = pages[i].height;
      return res;
    }
    std::vector<PageInfo> pages;
    std::vector<ColumnInfo> columns;
    std::vector<ZoneInfo> zones;
    long begPos, endPos; // end pos is only a minimum
  };

  
  WPXInputStream *input;
  int version;
  int phase; // 0 : read 1 : write

  std::vector<PartInfo> listZones;
  int actualZone;

  bool hasHeader, hasFooter;

  std::string directory;

  int debPageHeight, actualPageHeight;
  std::vector<int> pagesHeight;

  bool writeTextInTmpZone;
  WPSData::ZoneData tmpZone;
  WPSData::Document document;

  WPSRtf notesFile;
  WPSGroupFile mainFile;
  WPSAsciiFile asciiFile;
};

#endif
