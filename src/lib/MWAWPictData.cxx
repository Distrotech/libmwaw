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

/* This header contains code specific to a pict mac file
 */

#include <libwpd/WPXBinaryData.h>

#include "libmwaw_internal.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWPictData.hxx"
#include "MWAWPictMac.hxx"

bool MWAWPictData::createFileData(WPXBinaryData const &orig, WPXBinaryData &result)
{
  unsigned char buf[512];
  for (int i = 0; i < 512; i++) buf[i] = 0;
  result.clear();
  result.append(buf, 512);
  result.append(orig);
  return true;
}

MWAWPictData::ReadResult MWAWPictData::checkOrGet
(MWAWInputStreamPtr input, int size,  Box2f &box, MWAWPictData **result)
{
  if (result) *result=0L;
  box = Box2f();

  if (size <= 0) return MWAW_R_BAD;

  // we can not read the data, ...
  long actualPos = input->tell();
  if (input->seek(size,WPX_SEEK_CUR) || input->atEOS()) {
    if (input->tell() != actualPos+size) return MWAW_R_BAD;
  }
  MWAWPictData::ReadResult ok = MWAW_R_BAD;
  if (ok == MWAW_R_BAD) {
    input->seek(actualPos,WPX_SEEK_SET);
    ok = MWAWPictMac::checkOrGet(input, size, box, result);
  }
  if (ok == MWAW_R_BAD) {
    input->seek(actualPos,WPX_SEEK_SET);
    ok = MWAWPictDB3::checkOrGet(input, size, result);
  }
  if (ok == MWAW_R_BAD) {
    input->seek(actualPos,WPX_SEEK_SET);
    ok = MWAWPictDUnknown::checkOrGet(input, size, result);
  }

  if (ok == MWAW_R_BAD) return MWAW_R_BAD;

  if (result && *result && ok != MWAW_R_OK_EMPTY) {
    // we must read the data
    input->seek(actualPos, WPX_SEEK_SET);
    input->readDataBlock(size, (*result)->m_data);
  } else
    input->seek(actualPos+size, WPX_SEEK_SET);
  return ok;
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
MWAWPictDB3::ReadResult MWAWPictDB3::checkOrGet
(MWAWInputStreamPtr input, int size, MWAWPictData **result)
{
  if (result) *result=0L;

  // we can not read the data, ...
  long actualPos = input->tell();
  input->seek(actualPos,WPX_SEEK_SET);
  if (size <= 0xd || // to short, we ignore
      long(input->readULong(2)) != size) {
    return MWAW_R_BAD;
  }

  input->seek(actualPos+10,WPX_SEEK_SET);
  if (input->readLong(2) != 0x11) return  MWAW_R_BAD;

  if (result) *result = new MWAWPictDB3;
  return MWAW_R_OK;
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
MWAWPictDUnknown::ReadResult MWAWPictDUnknown::checkOrGet
(MWAWInputStreamPtr /*input*/, int size, MWAWPictData **result)
{
  if (result) *result=0L;

  // two small or two dangerous
  if (size < 0xD || size > 1000) return MWAW_R_BAD;
  if (result) *result = new MWAWPictDUnknown;
  return MWAW_R_MAYBE;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
