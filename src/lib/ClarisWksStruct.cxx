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

#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

#include <librevenge/librevenge.h>

#include "ClarisWksStruct.hxx"

namespace ClarisWksStruct
{
//------------------------------------------------------------
// DSET
//------------------------------------------------------------
Box2i DSET::getUnionChildBox() const
{
  Box2i res;
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

void DSET::updateChildPositions(Vec2f const &pageDim, int numHorizontalPages)
{
  float const &textWidth=pageDim[0];
  float const &textHeight=pageDim[1];
  if (textHeight<=0) {
    MWAW_DEBUG_MSG(("DSET::updateChildPositions: the height can not be null\n"));
    return;
  }
  if (numHorizontalPages>1 && textWidth<=0) {
    MWAW_DEBUG_MSG(("DSET::updateChildPositions: the width can not be null\n"));
    numHorizontalPages=1;
  }
  Box2f groupBox;
  int groupPage=-1;
  bool firstGroupFound=false;
  for (size_t i=0; i < m_childs.size(); i++) {
    Child &child=m_childs[i];
    Box2f childBdBox=child.getBdBox();
    int pageY=int(float(childBdBox[1].y())/textHeight);
    if (pageY < 0)
      continue;
    if (++pageY > 1) {
      Vec2f orig = child.m_box[0];
      Vec2f sz = child.m_box.size();
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
          MWAW_DEBUG_MSG(("ClarisWksGraph::updateGroup: can not find the page\n"));
          continue;
        }
      }
      child.m_box = Box2f(orig, orig+sz);
    }
    int pageX=1;
    if (numHorizontalPages>1) {
      pageX=int(float(childBdBox[1].x())/textWidth);
      Vec2f orig = child.m_box[0];
      Vec2f sz = child.m_box.size();
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
          MWAW_DEBUG_MSG(("ClarisWksGraph::updateGroup: can not find the horizontal page\n"));
          continue;
        }
      }
      child.m_box = Box2f(orig, orig+sz);
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

std::ostream &operator<<(std::ostream &o, DSET const &doc)
{
  switch (doc.m_type) {
  case DSET::T_Unknown:
    break;
  case DSET::T_Frame:
    o << "frame,";
    break;
  case DSET::T_Header:
    o << "header,";
    break;
  case DSET::T_Footer:
    o << "footer,";
    break;
  case DSET::T_Footnote:
    o << "footnote,";
    break;
  case DSET::T_Main:
    o << "main,";
    break;
  case DSET::T_Slide:
    o << "slide,";
    break;
  case DSET::T_Table:
    o << "table,";
    break;
  default:
    o << "#type=" << doc.m_type << ",";
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
  o << "id=" << doc.m_id << ",";
  if (!doc.m_fathersList.empty()) {
    o << "fathers=[";
    std::set<int>::const_iterator it = doc.m_fathersList.begin();
    for (; it != doc.m_fathersList.end(); ++it)
      o << *it << ",";
    o << "],";
  }
  if (!doc.m_validedChildList.empty()) {
    o << "child[valided]=[";
    std::set<int>::const_iterator it = doc.m_validedChildList.begin();
    for (; it != doc.m_validedChildList.end(); ++it)
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
