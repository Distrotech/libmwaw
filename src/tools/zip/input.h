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

#ifndef MWAW_FILE_H
#  define MWAW_FILE_H
#include <stdio.h>
#include <vector>

namespace libmwaw_zip
{
/** virtual class used to define an Input Stream */
class InputStream
{
protected:
  //! constructor
  InputStream() : m_offset(0) {}
public:
  //! destructor
  virtual ~InputStream() { }
  //! returns the file length
  virtual long length() = 0;
  //! enum to define the seek type: SK_SET, SK_CUR, SK_END
  enum SeekType { SK_SET, SK_CUR, SK_END };
  //! try to read numBytes
  virtual unsigned char const *read(unsigned long numBytes, unsigned long &numBytesRead) = 0;
  //! read a unsigned char
  unsigned char readU8();
  //! read a unsigned short (big endian)
  unsigned short readU16();
  //! read a unsigned int (big endian)
  unsigned int readU32();
  //! read a char
  char read8();
  //! read a short (big endian)
  short read16();
  //! read a unsigned int (big endian)
  int read32();
  //! try to go to a position. Returns 0 if ok
  int seek(long offset, SeekType seekType);
  //! return the actual position
  long tell() const {
    return m_offset;
  }
  //! return true if we are at the end of the file
  bool atEOS() {
    return m_offset >= length();
  }

protected:
  //! the actual position in the file
  volatile long m_offset;
private:
  InputStream(const InputStream &);
  InputStream &operator=(const InputStream &);
};

class StringStream: public InputStream
{
public:
  StringStream(unsigned char const *data, const unsigned long dataSize);
  ~StringStream() { }

  unsigned char const *read(unsigned long numBytes, unsigned long &numBytesRead);
  long length() {
    return long(m_buffer.size());
  }
private:
  std::vector<unsigned char> m_buffer;
  StringStream(const StringStream &);
  StringStream &operator=(const StringStream &);
};

class FileStream: public InputStream
{
public:
  FileStream(char const *path);
  ~FileStream();
  bool ok() const {
    return m_isOk;
  }
  unsigned char const *read(unsigned long numBytes, unsigned long &numBytesRead);
  long length();
private:
  FILE *m_file;
  bool m_isOk;
  std::vector<unsigned char> m_buffer;
  long m_bufferPos;

  FileStream(const FileStream &);
  FileStream &operator=(const FileStream &);
};

}
#endif

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
