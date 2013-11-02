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
#include <limits>
#include <map>
#include <sstream>

#include <librevenge/librevenge.h>

#include "MWAWDebug.hxx"
#include "MWAWHeader.hxx"
#include "MWAWInputStream.hxx"

#include "CWParser.hxx"

#include "CWStruct.hxx"

namespace CWStruct
{
//
// DSET
//
std::ostream &operator<<(std::ostream &o, DSET const &doc)
{
  switch(doc.m_type) {
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
  switch(doc.m_fileType) {
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
  o << "id=" << doc.m_id << ",";
  if (doc.m_fathersList.size()) {
    o << "fathers=[";
    std::set<int>::const_iterator it = doc.m_fathersList.begin();
    for ( ; it != doc.m_fathersList.end(); ++it)
      o << *it << ",";
    o << "],";
  }
  if (doc.m_validedChildList.size()) {
    o << "child[valided]=[";
    std::set<int>::const_iterator it = doc.m_validedChildList.begin();
    for ( ; it != doc.m_validedChildList.end(); ++it)
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
