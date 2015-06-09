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

//! internal data of a MWAWStringStream
class MWAWStringStreamPrivate
{
public:
  //! constructor
  MWAWStringStreamPrivate(const unsigned char *data, unsigned dataSize);
  //! destructor
  ~MWAWStringStreamPrivate();
  //! append some data at the end of the actual stream
  void append(const unsigned char *data, unsigned dataSize);
  //! the stream buffer
  std::vector<unsigned char> m_buffer;
  //! the stream offset
  long m_offset;
private:
  MWAWStringStreamPrivate(const MWAWStringStreamPrivate &);
  MWAWStringStreamPrivate &operator=(const MWAWStringStreamPrivate &);
};

MWAWStringStreamPrivate::MWAWStringStreamPrivate(const unsigned char *data, unsigned dataSize) :
  m_buffer(dataSize),
  m_offset(0)
{
  std::memcpy(&m_buffer[0], data, dataSize);
}

MWAWStringStreamPrivate::~MWAWStringStreamPrivate()
{
}

void MWAWStringStreamPrivate::append(const unsigned char *data, unsigned dataSize)
{
  if (!dataSize) return;
  size_t actualSize=m_buffer.size();
  m_buffer.resize(actualSize+size_t(dataSize));
  std::memcpy(&m_buffer[actualSize], data, dataSize);
}

MWAWStringStream::MWAWStringStream(const unsigned char *data, const unsigned int dataSize) :
  librevenge::RVNGInputStream(),
  m_data(new MWAWStringStreamPrivate(data, dataSize))
{
}

MWAWStringStream::~MWAWStringStream()
{
  if (m_data) delete m_data;
}

void MWAWStringStream::append(const unsigned char *data, const unsigned int dataSize)
{
  if (m_data) m_data->append(data, dataSize);
}

const unsigned char *MWAWStringStream::read(unsigned long numBytes, unsigned long &numBytesRead)
{
  numBytesRead = 0;

  if (numBytes == 0 || !m_data)
    return 0;

  long numBytesToRead;

  if ((unsigned long)m_data->m_offset+numBytes < m_data->m_buffer.size())
    numBytesToRead = (long) numBytes;
  else
    numBytesToRead = (long) m_data->m_buffer.size() - m_data->m_offset;

  numBytesRead = (unsigned long) numBytesToRead; // about as paranoid as we can be..

  if (numBytesToRead == 0)
    return 0;

  long oldOffset = m_data->m_offset;
  m_data->m_offset += numBytesToRead;

  return &m_data->m_buffer[size_t(oldOffset)];

}

long MWAWStringStream::tell()
{
  return m_data ? m_data->m_offset : 0;
}

int MWAWStringStream::seek(long offset, librevenge::RVNG_SEEK_TYPE seekType)
{
  if (!m_data) return -1;
  if (seekType == librevenge::RVNG_SEEK_CUR)
    m_data->m_offset += offset;
  else if (seekType == librevenge::RVNG_SEEK_SET)
    m_data->m_offset = offset;
  else if (seekType == librevenge::RVNG_SEEK_END)
    m_data->m_offset = offset+(long) m_data->m_buffer.size();

  if (m_data->m_offset < 0) {
    m_data->m_offset = 0;
    return -1;
  }
  if ((long)m_data->m_offset > (long)m_data->m_buffer.size()) {
    m_data->m_offset = (long) m_data->m_buffer.size();
    return -1;
  }

  return 0;
}

bool MWAWStringStream::isEnd()
{
  if (!m_data || (long)m_data->m_offset >= (long)m_data->m_buffer.size())
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
