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

/* This header contains code specific to a pict mac file
 */

#include <librevenge/librevenge.h>

#include "libmwaw_internal.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWPictData.hxx"
#include "MWAWPictMac.hxx"

bool MWAWPictData::createFileData(RVNGBinaryData const &orig, RVNGBinaryData &result)
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
  if (!input->checkPosition(actualPos+size))
    return MWAW_R_BAD;
  MWAWPictData::ReadResult ok = MWAW_R_BAD;
  if (ok == MWAW_R_BAD) {
    input->seek(actualPos,RVNG_SEEK_SET);
    ok = MWAWPictMac::checkOrGet(input, size, box, result);
  }
  if (ok == MWAW_R_BAD) {
    input->seek(actualPos,RVNG_SEEK_SET);
    ok = MWAWPictDB3::checkOrGet(input, size, result);
  }
  if (ok == MWAW_R_BAD) {
    input->seek(actualPos,RVNG_SEEK_SET);
    ok = MWAWPictDUnknown::checkOrGet(input, size, result);
  }

  if (ok == MWAW_R_BAD) return MWAW_R_BAD;

  if (result && *result && ok != MWAW_R_OK_EMPTY) {
    // we must read the data
    input->seek(actualPos, RVNG_SEEK_SET);
    input->readDataBlock(size, (*result)->m_data);
  } else
    input->seek(actualPos+size, RVNG_SEEK_SET);
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
  input->seek(actualPos,RVNG_SEEK_SET);
  if (size <= 0xd || // to short, we ignore
      long(input->readULong(2)) != size) {
    return MWAW_R_BAD;
  }

  input->seek(actualPos+10,RVNG_SEEK_SET);
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
