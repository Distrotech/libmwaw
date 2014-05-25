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

#include <cstring>
#include <vector>

#include <librevenge-stream/librevenge-stream.h>

#include "MWAWStringStream.hxx"

class MWAWStringStreamPrivate
{
public:
  MWAWStringStreamPrivate(const unsigned char *data, unsigned dataSize);
  ~MWAWStringStreamPrivate();
  std::vector<unsigned char> buffer;
  long offset;
private:
  MWAWStringStreamPrivate(const MWAWStringStreamPrivate &);
  MWAWStringStreamPrivate &operator=(const MWAWStringStreamPrivate &);
};

MWAWStringStreamPrivate::MWAWStringStreamPrivate(const unsigned char *data, unsigned dataSize) :
  buffer(dataSize),
  offset(0)
{
  std::memcpy(&buffer[0], data, dataSize);
}

MWAWStringStreamPrivate::~MWAWStringStreamPrivate()
{
}

MWAWStringStream::MWAWStringStream(const unsigned char *data, const unsigned int dataSize) :
  librevenge::RVNGInputStream(),
  d(new MWAWStringStreamPrivate(data, dataSize))
{
}

MWAWStringStream::~MWAWStringStream()
{
  delete d;
}

const unsigned char *MWAWStringStream::read(unsigned long numBytes, unsigned long &numBytesRead)
{
  numBytesRead = 0;

  if (numBytes == 0)
    return 0;

  long numBytesToRead;

  if ((unsigned long)d->offset+numBytes < d->buffer.size())
    numBytesToRead = (long) numBytes;
  else
    numBytesToRead = (long) d->buffer.size() - d->offset;

  numBytesRead = (unsigned long) numBytesToRead; // about as paranoid as we can be..

  if (numBytesToRead == 0)
    return 0;

  long oldOffset = d->offset;
  d->offset += numBytesToRead;

  return &d->buffer[size_t(oldOffset)];

}

long MWAWStringStream::tell()
{
  return d->offset;
}

int MWAWStringStream::seek(long offset, librevenge::RVNG_SEEK_TYPE seekType)
{
  if (seekType == librevenge::RVNG_SEEK_CUR)
    d->offset += offset;
  else if (seekType == librevenge::RVNG_SEEK_SET)
    d->offset = offset;
  else if (seekType == librevenge::RVNG_SEEK_END)
    d->offset += d->buffer.size();

  if (d->offset < 0) {
    d->offset = 0;
    return 1;
  }
  if ((long)d->offset > (long)d->buffer.size()) {
    d->offset = (long) d->buffer.size();
    return 1;
  }

  return 0;
}

bool MWAWStringStream::isEnd()
{
  if ((long)d->offset >= (long)d->buffer.size())
    return true;

  return false;
}

bool MWAWStringStream::isStructured()
{
  return false;
}

unsigned MWAWStringStream::subStreamCount()
{
  return 0;
}

const char *MWAWStringStream::subStreamName(unsigned)
{
  return 0;
}

bool MWAWStringStream::existsSubStream(const char *)
{
  return false;
}

librevenge::RVNGInputStream *MWAWStringStream::getSubStreamById(unsigned)
{
  return 0;
}

librevenge::RVNGInputStream *MWAWStringStream::getSubStreamByName(const char *)
{
  return 0;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
