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

/* This header contains code specific to a pict ole file
 */

#include <libwpd/WPXBinaryData.h>

#include "libmwaw_internal.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWPictOLEContainer.hxx"

MWAWPictOLEContainer::ReadResult MWAWPictOLEContainer::checkOrGet
(MWAWInputStreamPtr input, int size,  Box2f &box, MWAWPictOLEContainer **result)
{
  if (result) *result=0L;
  box = Box2f();

  if (size <= 0) return MWAW_R_BAD;

  // we can not read the data, ...
  long actualPos = input->tell();
  if (input->seek(size,WPX_SEEK_CUR) || input->atEOS()) {
    if (input->tell() != actualPos+size) return MWAW_R_BAD;
  }

  if (result) {
    *result = new MWAWPictOLEContainer;
    input->seek(actualPos, WPX_SEEK_SET);
    input->readDataBlock(size, (*result)->m_data);
  }
  input->seek(actualPos+size, WPX_SEEK_SET);

  return MWAW_R_OK;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
