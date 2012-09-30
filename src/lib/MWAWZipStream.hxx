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

/*
 *  freely inspired from libwpd/WPXZipStream.h :
 */
#if !defined(__MWAWZIPSTREAM_H__) && defined(USE_ZIP)
#  define __MWAWZIPSTREAM_H__

#include <string>
#include <vector>
#include <libwpd-stream/libwpd-stream.h>

class MWAWZipStream
{
public:
  MWAWZipStream(WPXInputStream *input) : m_input(input) {}
  bool isZipStream();
  WPXInputStream *getDocumentZipStream(const std::string &name);

  /**
   * Returns the list of all ole leaves names
   **/
  std::vector<std::string> getZipNames();

protected:
  WPXInputStream *m_input;
private:
  MWAWZipStream(MWAWZipStream const &orig);
  MWAWZipStream &operator=(MWAWZipStream const &orig);
};

#endif // __MWAWZIPSTREAM_H__
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
