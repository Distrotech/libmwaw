/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
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


#include "CSVGenerator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <iostream>

#include <librevenge/librevenge.h>
#include <librevenge-stream/librevenge-stream.h>
#include <libmwaw/libmwaw.hxx>

CSVGenerator::CSVGenerator(char const *fName) : m_output(), m_outputInit(false), m_dataStarted(false), m_firstFieldSend(false)
{
  if (!fName) return;
  m_output.open(fName, std::ios::out|std::ios::binary|std::ios::trunc);
  if (!m_output.good())
    throw MWAWDocument::Result(MWAWDocument::MWAW_R_FILE_ACCESS_ERROR);
  m_outputInit=true;
}

CSVGenerator::~CSVGenerator()
{
}

std::ostream &CSVGenerator::getOutput()
{
  if (m_outputInit) return m_output;
  return std::cout;
}

void CSVGenerator::insertTab()
{
  if (!m_dataStarted)
    return;
  getOutput() << char(0x9);
}

void CSVGenerator::insertText(const librevenge::RVNGString &text)
{
  if (!m_dataStarted)
    return;
  if (text.len()==0)
    return;
  char const *data=text.cstr();
  int sz=int(strlen(data));
  // we must escape the character \"
  for (int c=0; c<sz; c++) {
    if (data[c]=='\"')
      getOutput() << '\"';
    getOutput() << data[c];
  }
}

void CSVGenerator::insertSpace()
{
  if (!m_dataStarted)
    return;
  getOutput() << ' ';
}

void CSVGenerator::insertLineBreak()
{
  if (!m_dataStarted)
    return;
  getOutput() << '\n';
}

void CSVGenerator::openTable(const librevenge::RVNGPropertyList &propList)
{
  if (!propList["libmwaw:main_spreadsheet"] && !propList["libmwaw:main_database"])
    return;
  const librevenge::RVNGPropertyListVector *columns = propList.child("librevenge:table-columns");
  int nCol=int(columns ? columns->count() : 0);
  for (int i=0; i < nCol; ++i) {
    if (i)
      getOutput() << ",Col" << i+1;
    else
      getOutput() << "Col" << i+1;
  }
  getOutput() << "\n";
  m_dataStarted=true;
}

void CSVGenerator::closeTable()
{
  m_dataStarted=false;
}

void CSVGenerator::openTableRow(const librevenge::RVNGPropertyList & /* propList */)
{
  if (!m_dataStarted)
    return;
  m_firstFieldSend=false;
}

void CSVGenerator::closeTableRow()
{
  if (!m_dataStarted)
    return;
  getOutput() << "\n";
}

void CSVGenerator::openTableCell(const librevenge::RVNGPropertyList & /* propList */)
{
  if (!m_dataStarted)
    return;
  if (m_firstFieldSend)
    getOutput() << ",\"";
  else {
    getOutput() << "\"";
    m_firstFieldSend=true;
  }
}

void CSVGenerator::closeTableCell()
{
  if (!m_dataStarted)
    return;
  getOutput() << "\"";
}

/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
