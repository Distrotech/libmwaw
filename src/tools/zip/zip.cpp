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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>

#include <fstream>
#include <iostream>

#include <libmwaw_internal.hxx>
#include "input.h"
#include "zip.h"

namespace libmwaw_zip
{
static void writeU16(char *ptr, uint16_t data)
{
  *(ptr++)=char(data&0xFF);
  *(ptr++)=char(data>>8);
}
static void writeU32(char *ptr, uint32_t data)
{
  *(ptr++)=char(data&0xFF);
  *(ptr++)=char((data>>8)&0xFF);
  *(ptr++)=char((data>>16)&0xFF);
  *(ptr++)=char((data>>24)&0xFF);
}

static void writeTimeDate(char *ptr)
{
  time_t now = time(0L);
  struct tm timeinfo = *(localtime(&now));
  uint16_t time=uint16_t((timeinfo.tm_hour<<11)|(timeinfo.tm_min<<5));
  uint16_t date=uint16_t((timeinfo.tm_mon<<5)|timeinfo.tm_mday);
  int year = 1900+timeinfo.tm_year;
  if (year > 1980)
    date |= uint16_t((year-1980)<<9);
  writeU16(ptr, time);
  writeU16(ptr+2, date);
}

// ------------------------------------------------------------
// File implementation
// ------------------------------------------------------------
Zip::File::File(std::string const &base, std::string const &dir) :
  m_base(base), m_dir(dir), m_uncompressedLength(0), m_compressedLength(0),
  m_deflate(false), m_crc32(0),  m_offsetInFile(0)
{
}

bool Zip::File::write(shared_ptr<InputStream> input, std::ostream &output)
{
  if (!m_base.length())
    return false;
  if (!input || input->length()<0) {
    MWAW_DEBUG_MSG(("Zip::File::write: called without input\n"));
    return false;
  }
  // read the file
  unsigned long numBytes = (unsigned long) input->length();
  input->seek(0, InputStream::SK_SET);
  unsigned long numBytesRead=0;
  unsigned char const *buf= numBytes==0 ? 0 : input->read(numBytes, numBytesRead);
  if (numBytesRead != (unsigned long) numBytes || (numBytesRead && !buf)) {
    MWAW_DEBUG_MSG(("Zip::File::write: can not read the input\n"));
    return false;
  }
  // crc32
  m_uncompressedLength = m_compressedLength = uInt(numBytes);
  m_crc32 = crc32(0L, Z_NULL, 0);
  if (numBytes)
    m_crc32 = crc32(m_crc32, (const Bytef *) buf, m_uncompressedLength);

  std::vector<unsigned char> buffer;
  if (numBytes>10) {
    buffer.resize(size_t(numBytes)-10);
    z_stream zip;
    zip.next_in = (Bytef *) const_cast<unsigned char *>(buf);
    zip.avail_in = m_uncompressedLength;
    zip.total_in = 0;
    zip.next_out = &(Bytef &) buffer[0];
    zip.avail_out = uInt(numBytes)-10;
    zip.total_out = 0;
    zip.msg=0;
    zip.zalloc=0;
    zip.zfree=0;
    zip.opaque=0;

    /* optimize for speed and set all to default: expected negative
       WindowBits value which supresses the zlib header (and checksum)
       from the stream. */
    if (deflateInit2(&zip, 1, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY)!=Z_OK ||
        deflate(&zip,Z_FINISH)!=Z_STREAM_END)
      buffer.resize(0);
    else
      m_compressedLength = uInt(zip.total_out);
    deflateEnd(&zip);
  }

  // write the data
  m_offsetInFile = uLong(output.tellp());
  char localHeader[] = {
    0x50, 0x4b, 0x3, 0x4 /* 0:magic */, 0x14, 0 /* 4:version */,
    0, 0 /* 6:flags */, 0, 0 /* 8:compression */,
    0, 0 /* 10:time */, 0, 0 /* 12:date */, 0, 0, 0, 0 /* 14:checksum */,
    0, 0, 0, 0 /* 18:compressed size */, 0, 0, 0, 0 /* 22:uncompressed size */,
    0, 0 /* 26:file name size */, 0, 0 /* 28:extra size */
  };
  if (buffer.size()) {
    m_deflate = true;
    localHeader[8]=0x8; // deflate
  }
  std::string fName("");
  if (m_dir.length() && m_dir[0]=='/')
    fName = m_dir.substr(1)+m_base;
  else
    fName = m_dir+m_base;
  writeTimeDate(localHeader+10);
  writeU32(localHeader+14, uint32_t(m_crc32));
  writeU32(localHeader+18, uint32_t(m_compressedLength));
  writeU32(localHeader+22, uint32_t(m_uncompressedLength));
  writeU16(localHeader+26, uint16_t(fName.length()));
  output.write(localHeader, 30);
  if (fName.length())
    output.write(fName.c_str(), std::streamsize(fName.length()));

  if (buffer.size())
    output.write((const char *)&buffer[0], std::streamsize(m_compressedLength));
  else if (numBytes)
    output.write((const char *)buf, std::streamsize(numBytes));
  return true;
}

bool Zip::File::writeCentralInformation(std::ostream &output) const
{
  if (!m_base.length())
    return false;

  char fileHeader[] = {
    0x50, 0x4b, 0x1, 0x2 /* 0:magic */,
    0x14, 0 /* 4:version */, 0x14, 0 /* 6:version needed*/,
    0, 0 /* 8:flags */, 0, 0 /* 10:compression */,
    0, 0 /* 12:time */, 0, 0 /* 14:date */, 0, 0, 0, 0 /* 16:checksum */,
    0, 0, 0, 0 /* 20:compressed size */, 0, 0, 0, 0 /* 24:uncompressed size */,
    0, 0 /* 28:file name size */, 0, 0 /* 30:extra size */,
    0, 0 /* 32:comment size */,
    0, 0 /* 34: disk # start */, 0, 0 /*36: ascii/text file */,
    0, 0, 0, 0 /* 38:external attribute */, 0, 0, 0, 0/*42: offset of local header*/
  };
  if (m_deflate)
    fileHeader[10]=0x8;
  std::string fName("");
  if (m_dir.length() && m_dir[0]=='/')
    fName = m_dir.substr(1)+m_base;
  else
    fName = m_dir+m_base;
  writeTimeDate(fileHeader+12);
  writeU32(fileHeader+16, uint32_t(m_crc32));
  writeU32(fileHeader+20, uint32_t(m_compressedLength));
  writeU32(fileHeader+24, uint32_t(m_uncompressedLength));
  writeU16(fileHeader+28, uint16_t(fName.length()));
  writeU32(fileHeader+42, uint32_t(m_offsetInFile));
  output.write(fileHeader, 46);
  if (fName.length())
    output.write(fName.c_str(), std::streamsize(fName.length()));

  return true;
}

// ------------------------------------------------------------
// Directory implementation
// ------------------------------------------------------------
Zip::Directory::Directory(std::string const &dir) : m_dir(dir), m_nameFileMap()
{
}

bool Zip::Directory::add(shared_ptr<InputStream> input, char const *base, std::ostream &output)
{
  if (!base || !input) {
    MWAW_DEBUG_MSG(("Zip::Directory::add: called without input\n"));
    return false;
  }
  std::string name(base);
  // first clean name
  size_t len=name.length();
  if (base[0]=='\\' || base[0]=='/') {
    if (len==1) {
      name="";
      MWAW_DEBUG_MSG(("Zip::Directory::add: called with name <</>>\n"));
      return false;
    }
    name=base+1;
  }
  if (m_nameFileMap.find(name) != m_nameFileMap.end()) {
    MWAW_DEBUG_MSG(("Zip::Directory::add: a file was already added with this name: %s\n", name.c_str()));
    return false;
  }
  File file(name, m_dir);
  if (!file.write(input,output))
    return false;
  m_nameFileMap.insert(std::map<std::string, File>::value_type(name, file));
  return true;
}

int Zip::Directory::writeCentralInformation(std::ostream &output) const
{
  std::map<std::string, File>::const_iterator it=m_nameFileMap.begin();
  int num=0;
  while (it!=m_nameFileMap.end()) {
    File const &file=(it++)->second;
    if (file.writeCentralInformation(output))
      num++;
  }
  return num;
}

// ------------------------------------------------------------
// Zip implementation
// ------------------------------------------------------------
Zip::Zip() : m_output(), m_nameDirectoryMap()
{
}

Zip::~Zip()
{
  if (m_output.is_open())
    close();
}

bool Zip::open(char const *filename)
{
  if (!filename) {
    MWAW_DEBUG_MSG(("Zip::open: called without any name\n"));
    return false;
  }
  if (m_output.is_open()) {
    MWAW_DEBUG_MSG(("Zip::open: oops, output is already opened\n"));
    return false;
  }
  m_output.open(filename, std::ios::out | std::ios::binary);
  if (!m_output.is_open()) {
    MWAW_DEBUG_MSG(("Zip::open: can not opend %s\n", filename));
    return false;
  }

  return true;
}

bool Zip::close()
{
  if (!m_output.is_open()) {
    MWAW_DEBUG_MSG(("Zip::close: the output is already closed\n"));
    return false;
  }

  uLong pos=uLong(m_output.tellp());
  std::map<std::string, Directory>::const_iterator it=m_nameDirectoryMap.begin();
  int numEntries = 0;
  while (it!=m_nameDirectoryMap.end()) {
    Directory const &dir=(it++)->second;
    numEntries += dir.writeCentralInformation(m_output);
  }
  char endHeader[] = {
    0x50, 0x4b, 0x5, 0x6 /* 0:magic */,
    0, 0 /*4: disk number */, 0, 0 /* 6: disk w/cd */,
    0, 0 /*8: disk entries */, 0, 0 /* 10: total entries */,
    0, 0, 0, 0 /*12: central directory size */,
    0, 0, 0, 0 /*16: begin of central directory */,
    0, 0 /* 20: comment length */
  };
  writeU16(endHeader+8, uint16_t(numEntries));
  writeU16(endHeader+10, uint16_t(numEntries));
  writeU32(endHeader+12, uint32_t(uLong(m_output.tellp())-pos));
  writeU32(endHeader+16, uint32_t(pos));
  m_output.write(endHeader,22);
  m_output.close();
  return true;
}

bool Zip::add(shared_ptr<InputStream> input, char const *base, char const *path)
{
  if (!m_output.is_open())
    return false;
  if (!base || !input) {
    MWAW_DEBUG_MSG(("Zip::add: called without input\n"));
    return false;
  }
  // clean the directory name
  std::string dir(path ? path : "");
  size_t len = dir.length();
  for (size_t c=0; c < len; c++) {
    if (dir[c]=='\\')
      dir[c]='/';
  }
  if (!len || (path && path[len-1]!='/'))
    dir += '/';

  if (m_nameDirectoryMap.find(dir)==m_nameDirectoryMap.end())
    m_nameDirectoryMap.insert(std::map<std::string, Directory>::value_type(dir, Directory(dir)));
  Directory &direct=m_nameDirectoryMap.find(dir)->second;
  return direct.add(input, base, m_output);
}

}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
