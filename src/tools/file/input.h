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

#if defined(__MACH__)
#  include <Carbon/Carbon.h>
#else
#  include <Carbon.h>
#endif

#include <vector>

namespace libmwaw_tools
{
class InputStream
{
protected:
  InputStream() : m_offset(0) {}
public:
  virtual ~InputStream() { }

  virtual long length() = 0;
  enum SeekType { SK_SET, SK_CUR, SK_END };

  virtual unsigned char const *read(unsigned long numBytes, unsigned long &numBytesRead) = 0;

  unsigned char readU8();
  unsigned short readU16();
  unsigned int readU32();
  char read8();
  short read16();
  int read32();
  int seek(long offset, SeekType seekType);
  long tell() {
    return m_offset;
  }

  bool atEOS() {
    return m_offset >= length();
  }

protected:
  volatile long m_offset;
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
  FileStream(FSRef fRef, HFSUniStr255 const &forkName);
  ~FileStream();
  bool ok() const {
    return m_isOk;
  }
  unsigned char const *read(unsigned long numBytes, unsigned long &numBytesRead);
  long length();
private:
  FSIORefNum m_fRef;
  bool m_isOk;

  std::vector<unsigned char> m_buffer;
  long m_bufferPos;

  FileStream(const FileStream &);
  FileStream &operator=(const FileStream &);
};

}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
