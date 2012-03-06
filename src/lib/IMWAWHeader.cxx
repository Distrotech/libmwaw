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

#include "TMWAWInputStream.hxx"

#include "libmwaw_internal.hxx"

#include "IMWAWHeader.hxx"

IMWAWHeader::IMWAWHeader(TMWAWInputStreamPtr input, int majorVersion) :
  m_input(input),
  m_majorVersion(majorVersion),
  m_docType(IMWAWDocument::UNKNOWN),
  m_docKind(IMWAWDocument::K_TEXT)
{
}

IMWAWHeader::~IMWAWHeader()
{
}


/**
 * So far, we have identified
 */
IMWAWHeader * IMWAWHeader::constructHeader(TMWAWInputStreamPtr input)
{
  input->seek(8, WPX_SEEK_SET);
  if (input->atEOS() || input->tell() != 8)
    return 0;

  input->seek(0, WPX_SEEK_SET);
  int val[4];
  for (int i = 0; i < 4; i++)
    val[i] = input->readULong(2);
  IMWAWHeader *header;
  if (val[2] == 0x424F && val[3] == 0x424F && (val[0]>>8) < 8) {
    MWAW_DEBUG_MSG(("IMWAWHeader::constructHeader: find a Claris Works file[Limited parsing]\n"));
    header=new IMWAWHeader(input, ((val[0]) >> 8));
    header->m_docType=IMWAWDocument::CW;
    return header;
  }
  if (val[0]==0x5772 && val[1]==0x6974 && val[2]==0x654e && val[3]==0x6f77) {
    input->seek(8, WPX_SEEK_SET);
    int version = input->readLong(2);

#ifdef DEBUG
    if (version < 0 || version > 3) {
      MWAW_DEBUG_MSG(("IMWAWHeader::constructHeader: find a WriteNow file (unknown version %d)\n", version));

      return 0;
    }
#else
    if (version != 2) {
      MWAW_DEBUG_MSG(("IMWAWHeader::constructHeader: find a WriteNow file (unknown version %d)\n", version));

      return 0;
    }
#endif
    MWAW_DEBUG_MSG(("IMWAWHeader::constructHeader: find a WriteNow file version 3.0 or 4.0\n"));

    header=new IMWAWHeader(input, 3);
    header->m_docType=IMWAWDocument::WNOW;
    return header;
  }

  if ((val[0]==0xfe34 && val[1]==0) ||
      (val[0] == 0xfe37 && (val[1] == 0x23 || val[1] == 0x1c))) {
    switch (val[1]) {
    case 0:
      MWAW_DEBUG_MSG(("IMWAWHeader::constructHeader: find a Word 3.0 file[no parsing]\n"));
      header=new IMWAWHeader(input, 3);
      break;
    case 0x1c:
      MWAW_DEBUG_MSG(("IMWAWHeader::constructHeader: find a Word 4.0 file[minimal parsing]\n"));
      header=new IMWAWHeader(input, 4);
      break;
    case 0x23:
      MWAW_DEBUG_MSG(("IMWAWHeader::constructHeader: find a Word 5.0 file[minimal parsing]\n"));
      header=new IMWAWHeader(input, 5);
      break;
    default:
      return 0;
    }
    header->m_docType=IMWAWDocument::MSWORD;
    return header;
  }

  if (val[0] == 4 && val[1] == 4) {
    MWAW_DEBUG_MSG(("IMWAWHeader::constructHeader: find a MacWritePro file\n"));
    header=new IMWAWHeader(input, 1);
    header->m_docType=IMWAWDocument::MWPRO;
    return header;
  }

  if (val[0]==0 && val[1]==0 && val[2]==0 && val[3]==0) {
    input->seek(8, WPX_SEEK_SET);
    if (input->readULong(1) == 0x4) {
      MWAW_DEBUG_MSG(("IMWAWHeader::constructHeader: find a WriteNow 1.0 or 2.0 file\n"));
      header=new IMWAWHeader(input, 2);
      header->m_docType=IMWAWDocument::WNOW;
      return header;
    }
  }
#ifdef DEBUG
  bool mw = (val[0] > 2 && val[0] < 8);
#else
  bool mw = val[0] == 3 || val[0] == 6;
#endif
  if (mw) {
    // version will be print by MWParser::check
    header=new IMWAWHeader(input, val[0]);
    header->m_docType=IMWAWDocument::MW;
    return header;
  }
  if (val[0] == 0x110) {
    MWAW_DEBUG_MSG(("IMWAWHeader::constructHeader: find a Writerplus file\n"));
    header=new IMWAWHeader(input, 1);
    header->m_docType=IMWAWDocument::WPLUS;
    return header;
  }

  input->seek(0, WPX_SEEK_SET);
  return 0;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
