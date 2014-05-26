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
* Alternatively, the contents of this file may be used under the terms of
* the GNU Lesser General Public License Version 2 or later (the "LGPLv2+"),
* in which case the provisions of the LGPLv2+ are applicable
* instead of those above.
*/

#include <librevenge-stream/librevenge-stream.h>

class MWAWStringStreamPrivate;

class MWAWStringStream: public librevenge::RVNGInputStream
{
public:
  MWAWStringStream(const unsigned char *data, const unsigned int dataSize);
  ~MWAWStringStream();

  const unsigned char *read(unsigned long numBytes, unsigned long &numBytesRead);
  long tell();
  int seek(long offset, librevenge::RVNG_SEEK_TYPE seekType);
  bool isEnd();

  bool isStructured();
  unsigned subStreamCount();
  const char *subStreamName(unsigned);
  bool existsSubStream(const char *name);
  librevenge::RVNGInputStream *getSubStreamByName(const char *name);
  librevenge::RVNGInputStream *getSubStreamById(unsigned);

private:
  MWAWStringStreamPrivate *d;
  MWAWStringStream(const MWAWStringStream &); // copy is not allowed
  MWAWStringStream &operator=(const MWAWStringStream &); // assignment is not allowed
};

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
