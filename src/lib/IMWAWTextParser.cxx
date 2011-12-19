/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libmwaw
 * Copyright (C) 2009, 2011 Alonso Laurent (alonso@loria.fr)
 * Copyright (C) 2006, 2007 Andrew Ziem
 * Copyright (C) 2004-2006 Fridrich Strba (fridrich.strba@bluewin.ch)
 * Copyright (C) 2004 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2003-2005 William Lachance (william.lachance@sympatico.ca)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 * For further information visit http://libmwaw.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

#include <stdlib.h>
#include <string.h>

#include "libmwaw_internal.hxx"

#include "IMWAWParser.hxx"

#include "IMWAWTextParser.hxx"

////////////////////////////////////////////////////////////
// Constructor
////////////////////////////////////////////////////////////
IMWAWTextParser::IMWAWTextParser(IMWAWParser &parser) :
  m_textPositions(), m_listFODs(), m_mainParser(0)
{
  reset(parser);
}

IMWAWTextParser::~IMWAWTextParser() {}


void IMWAWTextParser::reset(IMWAWParser &parser)
{
  m_mainParser = &parser;

  m_textPositions = IMWAWEntry();
  m_listFODs.resize(0);
}


////////////////////////////////////////////////////////////
// read data
////////////////////////////////////////////////////////////
bool IMWAWTextParser::readFDP(TMWAWInputStreamPtr &input, IMWAWEntry const &entry,
                              std::vector<DataFOD> &fods,
                              IMWAWTextParser::FDPParser parser)
{
  if (entry.length() <= 0 || entry.begin() <= 0) {
    MWAW_DEBUG_MSG(("IMWAWTextParser: warning: FDP entry unintialized"));
    return false;
  }

  entry.setParsed();
  uint32_t page_offset = entry.begin();
  long length = entry.length();
  long endPage = entry.end();

  bool smallFDP = version() < 5;
  int deplSize = smallFDP ? 1 : 2;
  int headerSize = smallFDP ? 4 : 8;

  if (length < headerSize) {
    MWAW_DEBUG_MSG(("IMWAWTextParser: warning: FDP offset=0x%X, length=0x%lx\n",
                    page_offset, length));
    return false;
  }

  libmwaw_tools::DebugStream f;
  if (smallFDP) {
    endPage--;
    input->seek(endPage, WPX_SEEK_SET);
  } else
    input->seek(page_offset, WPX_SEEK_SET);
  uint16_t cfod = input->readULong(deplSize);

  f << "FDP: N="<<(int) cfod;
  if (smallFDP) input->seek(page_offset, WPX_SEEK_SET);
  else f << ", unk=" << input->readLong(2);

  if (headerSize+(4+deplSize)*cfod > length) {
    MWAW_DEBUG_MSG(("IMWAWTextParser: error: cfod = %i (0x%X)\n", cfod, cfod));
    return false;
  }

  int firstFod = fods.size();
  long lastLimit = firstFod ? fods.back().m_pos : 0;

  long lastReadPos = 0L;

  DataFOD::Type type = DataFOD::ATTR_UNKN;
  if (entry.hasType("FDPC")) type = DataFOD::ATTR_TEXT;
  else if (entry.hasType("FDPP")) type = DataFOD::ATTR_PARAG;
  else {
    MWAW_DEBUG_MSG(("IMWAWTextParser: FDP error: unknown type = '%s'\n", entry.type().c_str()));
  }

  /* Read array of fcLim of FODs.  The fcLim refers to the offset of the
     last character covered by the formatting. */
  for (int i = 0; i <= cfod; i++) {
    DataFOD fod;
    fod.m_type = type;
    fod.m_pos = input->readULong(4);
    if (fod.m_pos == 0) fod.m_pos=m_textPositions.begin();

    /* check that fcLim is not too large */
    if (fod.m_pos > m_textPositions.end()) {
      MWAW_DEBUG_MSG(("IMWAWTextParser: error: length of 'text selection' %ld > "
                      "total text length %ld\n", fod.m_pos, m_textPositions.end()));
      return false;
    }

    /* check that pos is monotonic */
    if (lastLimit > fod.m_pos) {
      MWAW_DEBUG_MSG(("IMWAWTextParser: error: character position list must "
                      "be monotonic, but found %ld, %ld\n", lastLimit, fod.m_pos));
      return false;
    }

    lastLimit = fod.m_pos;

    if (i != cfod)
      fods.push_back(fod);
    else // ignore the last text position
      lastReadPos = fod.m_pos;
  }

  std::vector<DataFOD>::iterator fods_iter;
  /* Read array of bfprop of FODs.  The bfprop is the offset where
     the FPROP is located. */
  f << ", Tpos:defP=(" << std::hex;
  for (fods_iter = fods.begin() + firstFod; fods_iter!= fods.end(); fods_iter++) {
    int depl = input->readULong(deplSize);
    /* check size of bfprop  */
    if ((depl < headerSize+(4+deplSize)*cfod && depl > 0) ||
        long(page_offset)+depl  > endPage) {
      MWAW_DEBUG_MSG(("IMWAWTextParser: error: pos of bfprop is bad "
                      "%i (0x%X)\n", depl, depl));
      return false;
    }

    if (depl)
      (*fods_iter).m_defPos = depl + page_offset;

    f << (*fods_iter).m_pos << ":";
    if (depl) f << (*fods_iter).m_defPos << ", ";
    else f << "_, ";
  }
  f << "), lstPos=" << lastReadPos << ", ";

  ascii().addPos(page_offset);
  ascii().addNote(f.str().c_str());
  ascii().addPos(input->tell());

  std::map<long,int> mapPtr;
  bool smallSzInProp = smallFDP ? true :version() < 100 ? false : true;
  for (fods_iter = fods.begin() + firstFod; fods_iter!= fods.end(); fods_iter++) {
    long pos = (*fods_iter).m_defPos;
    if (pos == 0) continue;

    std::map<long,int>::iterator it= mapPtr.find(pos);
    if (it != mapPtr.end()) {
      (*fods_iter).m_id = mapPtr[pos];
      continue;
    }

    input->seek(pos, WPX_SEEK_SET);
    int szProp = input->readULong(smallSzInProp ? 1 : 2);
    if (smallSzInProp) szProp++;
    if (szProp == 0) {
      MWAW_DEBUG_MSG(("Works: error: 0 == szProp at file offset 0x%lx\n", (input->tell())-1));
      return false;
    }
    long endPos = pos+szProp;
    if (endPos > endPage) {
      MWAW_DEBUG_MSG(("Works: error: cch = %d, too large\n", szProp));
      return false;
    }

    ascii().addPos(endPos);
    ascii().addPos(pos);
    int id;
    std::string mess;
    if (parser &&(this->*parser) (input, endPos, id, mess) ) {
      (*fods_iter).m_id = mapPtr[pos] = id;

      f.str("");
      f << entry.type()  << std::dec << id <<":" << mess;
      ascii().addNote(f.str().c_str());
      pos = input->tell();
    }

    if (pos != endPos) {
      ascii().addPos(pos);
      f.str("");
      f << entry.type() << "###";
    }
  }

  /* go to end of page */
  input->seek(endPage, WPX_SEEK_SET);

  return m_textPositions.end() > lastReadPos;
}

std::vector<IMWAWTextParser::DataFOD> IMWAWTextParser::mergeSortedLists
(std::vector<IMWAWTextParser::DataFOD> const &lst1,
 std::vector<IMWAWTextParser::DataFOD> const &lst2) const
{
  std::vector<IMWAWTextParser::DataFOD> res;
  // we regroup these two lists in one list
  int num1 = lst1.size(), i1 = 0;
  int num2 = lst2.size(), i2 = 0;

  while (i1 < num1 || i2 < num2) {
    DataFOD val;
    if (i2 == num2) val = lst1[i1++];
    else if (i1 == num1 || lst2[i2].m_pos < lst1[i1].m_pos)
      val = lst2[i2++];
    else val = lst1[i1++];

    if (val.m_pos < m_textPositions.begin() || val.m_pos > m_textPositions.end())
      continue;

    res.push_back(val);
  }
  return res;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
