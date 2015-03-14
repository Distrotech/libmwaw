/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */

/* libmwaw: tools
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

#include <string.h>
#include <iostream>

#include "file_internal.h"
#include "input.h"

namespace libmwaw_tools
{
//
// InputStream
//
unsigned char InputStream::readU8()
{
  unsigned long nRead;
  unsigned char const *data = read(1, nRead);
  if (!data || nRead != 1) return 0;
  return data[0];
}
unsigned short InputStream::readU16()
{
  unsigned long nRead;
  unsigned char const *data = read(2, nRead);
  if (!data || nRead != 2) return 0;
  return (unsigned short)((data[0]<<8)+data[1]);
}
unsigned int InputStream::readU32()
{
  unsigned long nRead;
  unsigned char const *data = read(4, nRead);
  if (!data || nRead != 4) return 0;
  return (unsigned int)((data[0]<<24)+(data[1]<<16)+(data[2]<<8)+data[3]);
}
char InputStream::read8()
{
  unsigned long nRead;
  unsigned char const *data = read(1, nRead);
  if (!data || nRead != 1) return 0;
  return (char) data[0];
}
short InputStream::read16()
{
  unsigned long nRead;
  unsigned char const *data = read(2, nRead);
  if (!data || nRead != 2) return 0;
  return (short)((data[0]<<8)+data[1]);
}
int InputStream::read32()
{
  unsigned long nRead;
  unsigned char const *data = read(4, nRead);
  if (!data || nRead != 4) return 0;
  return (int)((data[0]<<24)+(data[1]<<16)+(data[2]<<8)+data[3]);
}

int InputStream::seek(long _offset, SeekType seekType)
{
  long sz = length();
  if (seekType == SK_CUR)
    m_offset += _offset;
  else if (seekType == SK_SET)
    m_offset = _offset;
  else if (seekType == SK_END)
    m_offset = sz+_offset;

  if (m_offset < 0) {
    m_offset = 0;
    return 1;
  }
  if (m_offset > sz) {
    m_offset = sz;
    return 1;
  }
  return 0;
}

//
// StringStream
//
StringStream::StringStream(const unsigned char *data, const unsigned long dataSize) :
  InputStream(), m_buffer(dataSize)
{
  memcpy(&m_buffer[0], data, dataSize);
}

const unsigned char *StringStream::read(unsigned long numBytes, unsigned long &numBytesRead)
{
  numBytesRead = 0;

  if (numBytes == 0)
    return 0;

  unsigned long numBytesToRead;

  if (((unsigned long)m_offset+numBytes) < m_buffer.size())
    numBytesToRead = numBytes;
  else
    numBytesToRead = (unsigned long)((long)m_buffer.size() - m_offset);

  numBytesRead = numBytesToRead; // about as paranoid as we can be..

  if (numBytesToRead == 0)
    return 0;

  long oldOffset = m_offset;
  m_offset += numBytesToRead;

  return &m_buffer[size_t(oldOffset)];
}
//
// FileStream
//
FileStream::FileStream(char const *path) : InputStream(), m_file(0), m_isOk(true), m_buffer(), m_bufferPos(0)
{
  m_file = fopen(path,"r");
  if (m_file)
    return;
#ifdef DEBUG
  std::cerr << "FileStream:FileStream can not open " << path << "\n";
#endif
  m_isOk = false;
}

FileStream::~FileStream()
{
  if (!m_isOk) return;
  if (m_file) fclose(m_file);
}

unsigned char const *FileStream::read(unsigned long numBytes, unsigned long &numBytesRead)
{
  numBytesRead = 0;
  if (!m_isOk || !m_file)
    return 0;
  long lastPos = m_offset+long(numBytes);
  long fileLength = length();
  if (lastPos > fileLength) lastPos = fileLength;
  // check if the buffer is or not ok
  long bufSize = long(m_buffer.size());
  if (m_offset < m_bufferPos || lastPos > m_bufferPos+bufSize) {
    size_t numToRead = size_t(numBytes);
    if (numToRead < 4096 && m_offset+4096 <= fileLength)
      numToRead = 4096;
    else if (numToRead < 4096)
      numToRead = size_t(fileLength-m_offset);
    if (numToRead==0)
      return 0;
    // try to read numToRead bytes
    m_bufferPos = m_offset;
    m_buffer.resize(size_t(numToRead));

#if 0
    MWAW_DEBUG_MSG(("FileStream::read called with %ld[%ld]: read %ld bytes\n", m_offset, numBytes, numToRead));
#endif
    if (fseek(m_file, m_offset, SEEK_SET)==-1) {
      MWAW_DEBUG_MSG(("FileStream::read get error when doing a seek\n"));
      m_buffer.clear();
      return 0;
    }
    size_t nRead = fread(&(m_buffer[0]), 1, numToRead, m_file);
    if (nRead != numToRead) {
      MWAW_DEBUG_MSG(("FileStream::read get error when reading data\n"));
      m_buffer.resize(size_t(nRead));
    }
  }

  bufSize = long(m_buffer.size());
  if (!bufSize)
    return 0;
  numBytesRead = (unsigned long)(m_bufferPos + bufSize - m_offset);
  if (numBytesRead > numBytes)
    numBytesRead = numBytes;
  unsigned char const *res = &(m_buffer[size_t(m_offset-m_bufferPos)]);
  m_offset += long(numBytesRead);
  return res;
}

long FileStream::length()
{
  if (!m_isOk || !m_file) return 0;
  if (fseek(m_file, 0, SEEK_END)==-1) {
    MWAW_DEBUG_MSG(("FileStream::length get error when reading data\n"));
    return 0;
  }
  return long(ftell(m_file));
}
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
