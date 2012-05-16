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

#include <string.h>
#include <iostream>

#include "libmwaw_internal.hxx"

#include "MWAWInputStream.hxx"

#include "MWAWHeader.hxx"

MWAWHeader::MWAWHeader(MWAWDocument::DocumentType DocumentType, int vers) :
  m_version(vers),
  m_docType(DocumentType),
  m_docKind(MWAWDocument::K_TEXT)
{
}

MWAWHeader::~MWAWHeader()
{
}


/**
 * So far, we have identified
 */
std::vector<MWAWHeader> MWAWHeader::constructHeader(MWAWInputStreamPtr input)
{
  std::vector<MWAWHeader> res;

  input->seek(8, WPX_SEEK_SET);
  if (input->atEOS() || input->tell() != 8)
    return res;

  input->seek(0, WPX_SEEK_SET);
  int val[4];
  for (int i = 0; i < 4; i++)
    val[i] = input->readULong(2);

  // ----------- clearly discriminant ------------------
  if (val[2] == 0x424F && val[3] == 0x424F && (val[0]>>8) < 8) {
    MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a Claris Works file[Limited parsing]\n"));
    res.push_back(MWAWHeader(MWAWDocument::CW, (val[0]) >> 8));
  }
  if (val[0]==0x5772 && val[1]==0x6974 && val[2]==0x654e && val[3]==0x6f77) {
    input->seek(8, WPX_SEEK_SET);
    int version = input->readLong(2);

#ifdef DEBUG
    bool ok = (version >= 0 && version <= 3);
    if (ok)
      MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a WriteNow file version 3.0 or 4.0\n"));
    else
      MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a WriteNow file (unknown version %d)\n", version));
#else
    bool ok = version == 2;
#endif

    if (ok)
      res.push_back(MWAWHeader(MWAWDocument::WNOW, 3));
  }

  if ((val[0]==0xfe34 && val[1]==0) ||
      (val[0] == 0xfe37 && (val[1] == 0x23 || val[1] == 0x1c))) {
    int vers = -1;
    switch (val[1]) {
    case 0:
      MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a Word 3.0 file[no parsing]\n"));
      vers = 3;
      break;
    case 0x1c:
      MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a Word 4.0 file[minimal parsing]\n"));
      vers = 4;
      break;
    case 0x23:
      MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a Word 5.0 file[minimal parsing]\n"));
      vers = 5;
      break;
    default:
      break;
    }
    if (vers >= 0)
      res.push_back(MWAWHeader(MWAWDocument::MSWORD, vers));
  }

  // ----------- less discriminant ------------------
  if (val[0] == 0x2e && val[1] == 0x2e) {
    MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a MacWrite II file\n"));
    res.push_back(MWAWHeader(MWAWDocument::MWPRO, 0));
  }
  if (val[0] == 4 && val[1] == 4) {
    MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a MacWritePro file\n"));
    res.push_back(MWAWHeader(MWAWDocument::MWPRO, 1));
  }

  // ----------- other ------------------
  if (val[0]==0 && val[1]==0 && val[2]==0 && val[3]==0) {
    input->seek(8, WPX_SEEK_SET);
    if (input->readULong(1) == 0x4) {
      MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a WriteNow 1.0 or 2.0 file\n"));
      res.push_back(MWAWHeader(MWAWDocument::WNOW, 2));
    }
  }
  if (val[0]==0) {
    int vers = -1;
    switch(val[1]) {
    case 4:
      vers = 1;
      break;
    case 8:
      vers = 2;
      break;
#ifdef DEBUG
    case 9:
      vers = 3;
      break;
    case 11: // a msworks 4 file ( but not a text file)
      vers = 4;
      break;
#endif
    default:
      break;
    }
    if (vers > 0) {
      if (vers <= 2) {
        MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a Microsoft Works %d.0 file\n", vers));
      } else {
        MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a Microsoft Works %d.0 file[no parsing]\n", vers));
      }
      res.push_back(MWAWHeader(MWAWDocument::MSWORKS, vers));
    }
  }
  if (val[0] == 3 || val[0] == 6) {
    // version will be print by MWParser::check
    res.push_back(MWAWHeader(MWAWDocument::MW, val[0]));
  }
  if (val[0] == 0x110) {
    MWAW_DEBUG_MSG(("MWAWHeader::constructHeader: find a Writerplus file\n"));
    res.push_back(MWAWHeader(MWAWDocument::WPLUS, 1));
  }
  //ok now look at the end of file
  input->seek(0, WPX_SEEK_SET);
  while(!input->atEOS()) {
    if (input->seek(1000, WPX_SEEK_CUR) != 0) break;
  }
  input->seek(-4, WPX_SEEK_CUR);
  for (int i = 0; i < 2; i++)
    val[i]=input->readULong(2);
  if (val[0] == 0x4657 && val[1]==0x5254) // FWRT
    res.push_back(MWAWHeader(MWAWDocument::FULLW, 2));
  if (val[0] == 0 && val[1]==1) { // not probable, but
    res.push_back(MWAWHeader(MWAWDocument::FULLW, 1));
  }

  input->seek(0, WPX_SEEK_SET);
  return res;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
