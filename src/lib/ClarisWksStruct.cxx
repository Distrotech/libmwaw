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

#include <string.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"
#include "MWAWParser.hxx"

#include "ClarisWksStruct.hxx"

namespace ClarisWksStruct
{
// try to read a list of structured zone
bool readIntZone(MWAWParserState &parserState, char const *zoneName, bool hasEntete, int intSz, std::vector<int> &res)
{
  res.resize(0);
  if (intSz != 1 && intSz != 2 && intSz != 4) {
    MWAW_DEBUG_MSG(("ClarisWksStruct::readIntZone: unknown int size: %d\n", intSz));
    return false;
  }

  MWAWInputStreamPtr input = parserState.m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  input->seek(endPos,librevenge::RVNG_SEEK_SET);
  if (long(input->tell()) != endPos) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksStruct::readIntZone: unexpected size for %s\n", zoneName ? zoneName : "unamed"));
    return false;
  }
  libmwaw::DebugStream f;
  libmwaw::DebugFile &ascFile=parserState.m_asciiFile;
  if (zoneName && strlen(zoneName))
    f << "Entries(" << zoneName << "):";

  if (sz == 0) {
    if (hasEntete) {
      ascFile.addPos(pos-4);
      ascFile.addNote(f.str().c_str());
    }
    else {
      ascFile.addPos(pos);
      ascFile.addNote("NOP");
    }
    return true;
  }

  input->seek(pos+4, librevenge::RVNG_SEEK_SET);
  int N = (int) input->readLong(2);
  f << "N=" << N << ",";
  int type = (int) input->readLong(2);
  if (type != -1)
    f << "#type=" << type << ",";
  long val = input->readLong(2);
  if (val) f << "#unkn=" << val << ",";
  int fSz = (int) input->readULong(2);
  int hSz = (int) input->readULong(2);
  if (fSz != intSz || N *fSz+hSz+12 != sz) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksStruct::readIntZone: unexpected field size\n"));
    return false;
  }

  long debPos = endPos-N*fSz;
  if (long(input->tell()) != debPos) {
    ascFile.addDelimiter(input->tell(), '|');
    if (N) ascFile.addDelimiter(debPos, '|');
  }
  input->seek(debPos, librevenge::RVNG_SEEK_SET);
  f << "[";
  for (int i = 0; i < N; i++) {
    val = input->readLong(fSz);
    res.push_back((int) val);
    if (val>1000) f << "0x" << std::hex << val << std::dec << ",";
    else f << val << ",";
  }
  f << "]";

  ascFile.addPos(hasEntete ? pos-4 : pos);
  ascFile.addNote(f.str().c_str());

  input->seek(endPos,librevenge::RVNG_SEEK_SET);
  return true;
}

///////////////////////////////////////////////////////////
// try to read a unknown structured zone
////////////////////////////////////////////////////////////
bool readStructZone(MWAWParserState &parserState, char const *zoneName, bool hasEntete)
{
  MWAWInputStreamPtr input = parserState.m_input;
  long pos = input->tell();
  long sz = (long) input->readULong(4);
  long endPos = pos+4+sz;
  if (!input->checkPosition(endPos)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksStruct::readStructZone: unexpected size for %s\n", zoneName));
    return false;
  }
  libmwaw::DebugFile &ascFile= parserState.m_asciiFile;
  libmwaw::DebugStream f;
  f << "Entries(" << zoneName << "):";

  if (sz == 0) {
    if (hasEntete) {
      ascFile.addPos(pos-4);
      ascFile.addNote(f.str().c_str());
    }
    else {
      ascFile.addPos(pos);
      ascFile.addNote("NOP");
    }
    return true;
  }

  int N = (int) input->readLong(2);
  f << "N=" << N << ",";
  int type = (int) input->readLong(2);
  if (type != -1)
    f << "#type=" << type << ",";
  int val = (int) input->readLong(2);
  if (val) f << "#unkn=" << val << ",";
  int fSz = (int) input->readULong(2);
  int hSz = (int) input->readULong(2);
  if (!fSz || N *fSz+hSz+12 != sz) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    MWAW_DEBUG_MSG(("ClarisWksStruct::readStructZone: unexpected size for %s\n", zoneName));
    return false;
  }

  if (long(input->tell()) != pos+4+hSz)
    ascFile.addDelimiter(input->tell(), '|');
  ascFile.addPos(hasEntete ? pos-4 : pos);
  ascFile.addNote(f.str().c_str());

  long debPos = endPos-N*fSz;
  for (int i = 0; i < N; i++) {
    input->seek(debPos, librevenge::RVNG_SEEK_SET);
    f.str("");
    f << zoneName << "-" << i << ":";

    long actPos = input->tell();
    if (actPos != debPos && actPos != debPos+fSz)
      ascFile.addDelimiter(input->tell(),'|');
    ascFile.addPos(debPos);
    ascFile.addNote(f.str().c_str());
    debPos += fSz;
  }
  input->seek(endPos,librevenge::RVNG_SEEK_SET);
  return true;
}

//------------------------------------------------------------
// DSET
//------------------------------------------------------------
MWAWBox2i DSET::getUnionChildBox() const
{
  MWAWBox2i res;
  long maxX=1000;
  for (size_t i=0; i<m_childs.size(); ++i) {
    Child const &child=m_childs[i];
    // highly spurious, better to ignore
    if ((long) child.m_box[1][0]>3*maxX)
      continue;
    if ((long) child.m_box[1][0]>maxX)
      maxX=(long) child.m_box[1][0];
    res=child.m_box.getUnion(res);
  }
  return res;
}

void DSET::removeChild(int cId)
{
  removeChild(cId, std::find(m_otherChilds.begin(), m_otherChilds.end(), cId)==m_otherChilds.end());
}

void DSET::removeChild(int cId, bool normalChild)
{
  if (normalChild) {
    std::vector<Child>::iterator it;
    for (it=m_childs.begin(); it!=m_childs.end(); ++it) {
      if (it->m_type != C_Zone || it->m_id != cId) continue;
      m_childs.erase(it);
      return;
    }
  }
  else {
    std::vector<int>::iterator it;
    for (it=m_otherChilds.begin(); it!=m_otherChilds.end(); ++it) {
      if (*it != cId) continue;
      m_otherChilds.erase(it);
      return;
    }
  }
  MWAW_DEBUG_MSG(("ClarisWksStruct::DSET::removeChild can not detach %d\n", cId));
}


void DSET::updateChildPositions(MWAWVec2f const &pageDim, float formLength, int numHorizontalPages)
{
  float const &textWidth=pageDim[0];
  float textHeight=pageDim[1];
  if (float(m_pageDimension[1])>0.5f*formLength && float(m_pageDimension[1])<formLength)
    textHeight=float(m_pageDimension[1]);
  if (textHeight<=0) {
    MWAW_DEBUG_MSG(("ClarisWksStruct::DSET::updateChildPositions: the height can not be null\n"));
    return;
  }
  if (numHorizontalPages>1 && textWidth<=0) {
    MWAW_DEBUG_MSG(("ClarisWksStruct::DSET::updateChildPositions: the width can not be null\n"));
    numHorizontalPages=1;
  }
  MWAWBox2f groupBox;
  int groupPage=-1;
  bool firstGroupFound=false;
  for (size_t i=0; i < m_childs.size(); i++) {
    Child &child=m_childs[i];
    MWAWBox2f childBdBox=child.getBdBox();
    int pageY=int(float(childBdBox[1].y())/textHeight);
    if (pageY < 0)
      continue;
    if (++pageY > 1) {
      MWAWVec2f orig = child.m_box[0];
      MWAWVec2f sz = child.m_box.size();
      orig[1]-=float(pageY-1)*textHeight;
      if (orig[1] < 0) {
        if (orig[1]>=-textHeight*0.1f)
          orig[1]=0;
        else if (orig[1]>-1.1*textHeight) {
          orig[1]+=textHeight;
          if (orig[1]<0) orig[1]=0;
          pageY--;
        }
        else {
          // can happen in a drawing document if a form is on several vertical page
          if (m_position!=P_Main) { // can be normal, if this corresponds to the mainZone
            MWAW_DEBUG_MSG(("ClarisWksStruct::DSET::updateChildPositions: data on several vertical page(move it on the first page)\n"));
          }
          // better to move it on the first page, ie. if the position is problematic, we do no create a big number of empty page
          pageY=int(float(childBdBox[0].y())/textHeight);
          if (++pageY<0) pageY=0;
          if (sz[1]>textHeight) {
            orig[1]=0;
            sz[1]=textHeight;
          }
          else
            orig[1]=textHeight-sz[1];
        }
      }
      child.m_box = MWAWBox2f(orig, orig+sz);
    }
    int pageX=1;
    if (numHorizontalPages>1) {
      pageX=int(float(childBdBox[1].x())/textWidth);
      MWAWVec2f orig = child.m_box[0];
      MWAWVec2f sz = child.m_box.size();
      orig[0]-=float(pageX)*textWidth;
      if (orig[0] < 0) {
        if (orig[0]>=-textWidth*0.1f)
          orig[0]=0;
        else if (orig[0]>-1.1*textWidth) {
          orig[0]+=textWidth;
          if (orig[0]<0) orig[0]=0;
          pageX--;
        }
        else {
          // can happen if a form is on several horizontal page
          MWAW_DEBUG_MSG(("ClarisWksStruct::DSET::updateChildPositions: data on several horizontal page(move it on the first page)\n"));
          // better to move it on the first page, ie. if the position is problematic, we do no create a big number of empty page
          pageX=int(float(childBdBox[0].x())/textWidth);
          if (pageX<0) pageX=0;
          if (sz[0]>textWidth) {
            orig[0]=0;
            sz[0]=textWidth;
          }
          else
            orig[0]=textWidth-sz[0];
        }
      }
      child.m_box = MWAWBox2f(orig, orig+sz);
      pageX++;
    }
    int page=pageX+(pageY-1)*numHorizontalPages;
    if (!firstGroupFound) {
      groupPage=page;
      groupBox=child.getBdBox();
      firstGroupFound=true;
    }
    else if (groupPage==page)
      groupBox=groupBox.getUnion(child.getBdBox());
    else
      groupPage=-1;
    child.m_page = page;
  }
  if (groupPage>=0) {
    m_page=groupPage;
    m_box=groupBox;
  }
}

void DSET::findForbiddenPagesBreaking(float pageDim, float formDim, int dim, std::set<int> &forbiddenPageBreak) const
{
  if (isHeaderFooter() || m_position==P_Frame)
    return;

  if (dim<0||dim>1) {
    MWAW_DEBUG_MSG(("ClarisWksStruct::DSET::findForbiddenPagesBreaking: the height can not be null\n"));
    return;
  }
  float length=pageDim;
  if (float(m_pageDimension[dim])>0.5f*formDim && float(m_pageDimension[dim])<formDim)
    length=float(m_pageDimension[dim]);
  if (length<=0) {
    MWAW_DEBUG_MSG(("ClarisWksStruct::DSET::findForbiddenPagesBreaking: the length can not be null\n"));
    return;
  }
  float const eps=0.1f*length;
  for (size_t i=0; i < m_childs.size(); i++) {
    Child const &child=m_childs[i];
    MWAWBox2f childBdBox=child.getBdBox();
    // as the recomputation of page position is not accurate, just ignore the small size
    if (childBdBox.size()[dim]<=length)
      continue;
    int pageMax=int(float(childBdBox[1][dim])/length);
    if (pageMax <= 0)
      continue;
    float diff = child.m_box[1][dim]-float(pageMax)*length;
    if (diff <= eps)
      --pageMax;
    int pageMin=int(float(childBdBox[0][dim])/length);
    if (pageMin<0) pageMin=0;
    else {
      diff = child.m_box[0][dim]-float(pageMin)*length;
      if (diff >= length-eps)
        ++pageMin;
    }
    for (int j=pageMin+1; j<=pageMax; ++j)
      forbiddenPageBreak.insert(j);
  }
}

std::ostream &operator<<(std::ostream &o, DSET const &doc)
{
  switch (doc.m_position) {
  case DSET::P_Unknown:
    break;
  case DSET::P_Frame:
    o << "frame,";
    break;
  case DSET::P_Header:
    o << "header,";
    break;
  case DSET::P_Footer:
    o << "footer,";
    break;
  case DSET::P_Footnote:
    o << "footnote,";
    break;
  case DSET::P_Main:
    o << "main,";
    break;
  case DSET::P_GraphicMaster:
    o << "graphic[master],";
    break;
  case DSET::P_Slide:
    o << "slide,";
    break;
  case DSET::P_SlideMaster:
    o << "slide[master],";
    break;
  case DSET::P_SlideNote:
    o << "slide[note],";
    break;
  case DSET::P_SlideThumbnail:
    o << "slide[thumbnail],";
    break;
  case DSET::P_Table:
    o << "table,";
    break;
  default:
    o << "#position=" << doc.m_position << ",";
    break;
  }
  switch (doc.m_fileType) {
  case 0:
    o << "normal,";
    break;
  case 1:
    o << "text";
    if (doc.m_textType==0xFF)
      o << "*,";
    else if (doc.m_textType==0xa) // appear in graphic file
      o << "[textbox],";
    else if (doc.m_textType)
      o << "[#type=" << std::hex << doc.m_textType<< std::dec << "],";
    else
      o << ",";
    break;
  case 2:
    o << "spreadsheet,";
    break;
  case 3:
    o << "database,";
    break;
  case 4:
    o << "bitmap,";
    break;
  case 5:
    o << "presentation,";
    break;
  case 6:
    o << "table,";
    break;
  default:
    o << "#type=" << doc.m_fileType << ",";
    break;
  }
  if (doc.m_page>= 0) o << "pg=" << doc.m_page << ",";
  if (doc.m_box.size()[0]>0||doc.m_box.size()[1]>0)
    o << "box=" << doc.m_box << ",";
  if (doc.m_pageDimension[0]>0 || doc.m_pageDimension[1]>0)
    o << "zone[dim]=" << doc.m_pageDimension << ",";
  o << "id=" << doc.m_id << ",";
  if (!doc.m_fathersList.empty()) {
    o << "fathers=[";
    std::set<int>::const_iterator it = doc.m_fathersList.begin();
    for (; it != doc.m_fathersList.end(); ++it)
      o << *it << ",";
    o << "],";
  }
  o << "N=" << doc.m_numData << ",";
  if (doc.m_dataSz >=0) o << "dataSz=" << doc.m_dataSz << ",";
  if (doc.m_headerSz >= 0) o << "headerSz=" << doc.m_headerSz << ",";
  if (doc.m_beginSelection) o << "begSel=" << doc.m_beginSelection << ",";
  if (doc.m_endSelection >= 0) o << "endSel=" << doc.m_endSelection << ",";
  for (int i = 0; i < 4; i++) {
    if (doc.m_flags[i])
      o << "fl" << i << "=" << std::hex << doc.m_flags[i] << std::dec << ",";
  }
  for (size_t i = 0; i < doc.m_childs.size(); i++)
    o << "child" << i << "=[" << doc.m_childs[i] << "],";
  for (size_t i = 0; i < doc.m_otherChilds.size(); i++)
    o << "otherChild" << i << "=" << doc.m_otherChilds[i] << ",";
  return o;
}

}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
