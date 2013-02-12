/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */

/* freely inspired from the code source of zipdir.c:
** zipdir.c
** Copyright (C) 2008-2009 Randy Heit
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
****************************************************************************/

// HEADER FILES ------------------------------------------------------------

#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <zlib.h>

#include <algorithm>
#include <fstream>
#include <iostream>

#include "input.h"
#include "zip.h"

// MACROS ------------------------------------------------------------------

#ifndef _WIN32
#define __cdecl
#endif

#ifndef __BIG_ENDIAN__
#define MAKE_ID(a,b,c,d)	((a)|((b)<<8)|((c)<<16)|((d)<<24))
#define LittleShort(x)		(x)
#define LittleLong(x)		(x)
#else
#define MAKE_ID(a,b,c,d)	((d)|((c)<<8)|((b)<<16)|((a)<<24))
static unsigned short LittleShort(unsigned short x)
{
  return (x>>8) | (x<<8);
}

static unsigned int LittleLong(unsigned int x)
{
  return (x>>24) | ((x>>8) & 0xff00) | ((x<<8) & 0xff0000) | (x<<24);
}
#endif

#define METHOD_STORED	0
#define METHOD_DEFLATE	8

// TYPES -------------------------------------------------------------------

namespace libmwaw_zip
{
// basic structure
struct FileEntry {
  FileEntry(shared_ptr<InputStream> stream, std::string thePath, time_t last_written) :
    m_stream(stream), time_write(last_written), uncompressed_size(0), compressed_size(0),
    crc32(0), zip_offset(0), date(0), time(0), method(0), path(thePath) {
  }
  shared_ptr<InputStream> m_stream;
  time_t time_write;
  unsigned int uncompressed_size;
  unsigned int compressed_size;
  unsigned int crc32;
  unsigned int zip_offset;
  short date, time;
  int method;
  std::string path;
};

struct DirTree {
  DirTree(const char *dir) : files(), path("") {
    if (dir) path=dir;
    size_t dirlen = path.size();
    if (!dirlen || dir[dirlen - 1] != '/')
      path += '/';
  }
  std::vector<FileEntry> files;
  std::string path;
};

// FileSorted implementation
bool Zip::FileSorted::Cmp::operator()(Zip::FileSorted const &a, Zip::FileSorted const &b) const
{
  bool in_dir1 = a.m_path.find('/')!=std::string::npos;
  bool in_dir2 = b.m_path.find('/')!=std::string::npos;
  if (in_dir1 && !in_dir2)
    return true;
  if (!in_dir1 && in_dir2)
    return false;
  return a.m_path.compare(b.m_path)<0;
}

// sort_files
std::vector<Zip::FileSorted> Zip::FileSorted::sortFiles(std::vector<DirTree> &trees)
{
  std::vector<FileSorted> res;
  for (size_t i = 0; i < trees.size(); i++) {
    size_t pathSize=trees[i].path.size();
    for (size_t j = 0; j < trees[i].files.size(); j++) {
      FileEntry &file=trees[i].files[j];
      res.push_back(FileSorted(&file, &file.path[pathSize]));
    }
  }
  std::sort(res.begin(), res.end(), Zip::FileSorted::Cmp());
  return res;
}


// Zip implementation
void Zip::addStream(shared_ptr<InputStream> stream, char const *base, char const *path)
{
  if (!base || !strlen(base)) return;
  std::string fBase(base), fPath("");
  if (path) fPath=path;
#ifdef WIN32
  for (size_t i = 0; i < fPath.size(); i++) {
    if (fPath[i]=='\\')
      fPath[i]='/';
  }
  if (fBase[0]=='\\')
    fBase[0]='/';
#endif
  if (!fPath.size() || fPath[fPath.size()-1]!='/')
    fPath += '/';

  // look if a DirTree exists for fPath
  DirTree *tree=0;
  for (size_t t = 0; t < m_dirTrees.size(); t++) {
    if (m_dirTrees[t].path != fPath) continue;
    tree = &m_dirTrees[t];
    break;
  }
  if (!tree) {
    m_dirTrees.push_back(DirTree(fPath.c_str()));
    tree = &m_dirTrees.back();
  }

  if (fBase[0]=='/')
    fBase.erase(0,1);
  tree->files.push_back(FileEntry(stream, fPath+fBase,time(0)));
}

Zip::Zip() : m_dirTrees() { }
Zip::~Zip() { }
void Zip::timeToDos(struct tm *time, short *dosdate, short *dostime)
{
  if (time == 0 || time->tm_year < 80)
    *dosdate = *dostime = 0;
  else {
    *dosdate = (short) LittleShort((unsigned short)((time->tm_year - 80) * 512 + (time->tm_mon + 1) * 32 + time->tm_mday));
    *dostime = (short) LittleShort((unsigned short)(time->tm_hour * 2048 + time->tm_min * 32 + time->tm_sec / 2));
  }
}

// try to compress a string
bool Zip::compressDeflate(char *out, unsigned int *outlen, char const *in,
                          unsigned int inlen)
{
  z_stream stream;
  int err;

  stream.next_in = (Bytef *)in;
  stream.avail_in = inlen;
  stream.next_out = (Bytef *)out;
  stream.avail_out = *outlen;
  stream.zalloc = (alloc_func)0;
  stream.zfree = (free_func)0;
  stream.opaque = (voidpf)0;

  err = deflateInit2(&stream, 9, Z_DEFLATED, -15, 9, Z_DEFAULT_STRATEGY);
  if (err != Z_OK) return false;

  err = deflate(&stream, Z_FINISH);
  if (err != Z_STREAM_END) {
    deflateEnd(&stream);
    return false;
  }
  *outlen = (unsigned int)stream.total_out;

  return deflateEnd(&stream) == Z_OK;
}
}

////////////////////////////////////////////////////////////
// low level
////////////////////////////////////////////////////////////

#define ZIP_LOCALFILE	MAKE_ID('P','K',3,4)
#define ZIP_CENTRALFILE	MAKE_ID('P','K',1,2)
#define ZIP_ENDOFDIR	MAKE_ID('P','K',5,6)

namespace libmwaw_zip
{
typedef unsigned int DWORD;
typedef unsigned short WORD;
typedef unsigned char BYTE;

#pragma pack(1)
struct LocalFileHeader {
  DWORD	Magic;					// 0
  BYTE	VersionToExtract[2];			// 4
  WORD	Flags;					// 6
  WORD	Method;					// 8
  WORD	ModTime;				// 10
  WORD	ModDate;				// 12
  DWORD	CRC32;					// 14
  DWORD	CompressedSize;				// 18
  DWORD	UncompressedSize;			// 22
  WORD	NameLength;				// 26
  WORD	ExtraLength;				// 28
};

struct CentralDirectoryEntry {
  DWORD	Magic;
  BYTE	VersionMadeBy[2];
  BYTE	VersionToExtract[2];
  WORD	Flags;
  WORD	Method;
  WORD	ModTime;
  WORD	ModDate;
  DWORD	CRC32;
  DWORD	CompressedSize;
  DWORD	UncompressedSize;
  WORD	NameLength;
  WORD	ExtraLength;
  WORD	CommentLength;
  WORD	StartingDiskNumber;
  WORD	InternalAttributes;
  DWORD	ExternalAttributes;
  DWORD	LocalHeaderOffset;
};

struct EndOfCentralDirectory {
  DWORD	Magic;
  WORD	DiskNumber;
  WORD	FirstDisk;
  WORD	NumEntries;
  WORD	NumEntriesOnAllDisks;
  DWORD	DirectorySize;
  DWORD	DirectoryOffset;
  WORD	ZipCommentLength;
};

// write
bool Zip::write(const char *zipname)
{
  if (!zipname || !strlen(zipname)) {
    fprintf(stderr, "Could not find the filename\n");
    return false;
  }
  EndOfCentralDirectory dirend;
  std::vector<FileSorted> sorted = FileSorted::sortFiles(m_dirTrees);
  size_t numFiles=sorted.size();
  if (!numFiles) return false;
  std::ofstream fileZip;
  fileZip.open(zipname,std::ios::out | std::ios::binary);
  if (!fileZip) {
    fprintf(stderr, "Could not open %s\n", zipname);
    return false;
  }

  // Write each file.
  for (size_t i = 0; i < numFiles; ++i) {
    if (appendStream(fileZip, sorted[i]))
      continue;
    return false;
  }

  // Write central directory.
  std::streamoff dirOffset=fileZip.tellp();
  dirend.DirectoryOffset = (DWORD) dirOffset;
  for (size_t i = 0; i < numFiles; ++i)
    appendInCentralDir(fileZip, sorted[i]);

  // Write the directory terminator.
  dirend.Magic = ZIP_ENDOFDIR;
  dirend.DiskNumber = 0;
  dirend.FirstDisk = 0;
  dirend.NumEntriesOnAllDisks = dirend.NumEntries = LittleShort(WORD(numFiles));
  dirend.DirectorySize = LittleLong(DWORD(fileZip.tellp() - dirOffset));
  dirend.DirectoryOffset = LittleLong(dirend.DirectoryOffset);
  dirend.ZipCommentLength = 0;
  fileZip.write((char *) &dirend, sizeof(dirend));
  if (fileZip.bad()) {
    fprintf(stderr, "Failed writing zip directory terminator: %s\n", strerror(errno));
    return false;
  }
  return true;
}

// append_to_zip
bool Zip::appendStream(std::ostream &zip, Zip::FileSorted const &filep)
{
  FileEntry &file = *filep.m_file;
  shared_ptr<InputStream> input=file.m_stream;
  if (!input) return false;
  // try to determine local time
  struct tm *ltime = localtime(&file.time_write);
  timeToDos(ltime, &file.date, &file.time);

  unsigned long len=(unsigned int) input->length();
  input->seek(0, InputStream::SK_SET);

  // read the whole source file
  unsigned long readlen;
  unsigned char const *readbuf=input->read(len, readlen);

  // if read less bytes than expected,
  if (readlen != len) {
    fprintf(stderr, "Unable to read %s\n", file.path.c_str());
    return false;
  }

  // file loaded
  file.uncompressed_size = (unsigned int) len;
  file.compressed_size = (unsigned int) len;
  file.method = METHOD_STORED;

  // Calculate CRC32 for file.
  uLong crc = crc32(0, 0, 0);
  crc = crc32(crc, (Bytef const *) readbuf, (uInt)len);
  file.crc32 = (unsigned int) LittleLong(crc);

  // Allocate a buffer for compression, one byte less than the source buffer.
  // If it doesn't fit in that space, then skip compression and store it as-is.

  // note: len==0 can be ok if we need to store a empty datafork
  char *compbuf = len > 1 ? (char *) malloc(len - 1) : 0;
  unsigned int comp_len = len > 1 ? (unsigned int)(len) - 1 : 0;
  if (len > 1 && compressDeflate(compbuf, &comp_len, (char *)readbuf, (unsigned int) len)) {
    file.method = METHOD_DEFLATE;
    file.compressed_size = comp_len;
  }

  // Fill in local directory header.
  LocalFileHeader local;
  local.Magic = ZIP_LOCALFILE;
  local.VersionToExtract[0] = 20;
  local.VersionToExtract[1] = 0;
  local.Flags = file.method == METHOD_DEFLATE ? LittleShort(2) : 0;
  local.Method = LittleShort(WORD(file.method));
  local.ModTime = (WORD) file.time;
  local.ModDate = (WORD) file.date;
  local.CRC32 = file.crc32;
  local.UncompressedSize = LittleLong(file.uncompressed_size);
  local.CompressedSize = LittleLong(file.compressed_size);
  local.NameLength = LittleShort((unsigned short)filep.m_path.size());
  local.ExtraLength = 0;
  file.zip_offset = (unsigned int) zip.tellp();

  // Write out the header, file name, and file data.
  zip.write((char *)&local, sizeof(local));
  zip.write(filep.m_path.c_str(), (long)filep.m_path.size());
  if (file.method)
    zip.write(compbuf, (long)comp_len);
  else
    zip.write((char *)readbuf, (long)len);
  if (compbuf) free(compbuf);
  if (zip.bad()) {
    fprintf(stderr, "Unable to write %s to zip\n", file.path.c_str());
    return false;
  }
  return true;
}

// write_central_dir
bool Zip::appendInCentralDir(std::ostream &zip, Zip::FileSorted const &filep)
{
  CentralDirectoryEntry dir;

  FileEntry &file = *filep.m_file;
  dir.Magic = ZIP_CENTRALFILE;
  dir.VersionMadeBy[0] = 20;
  dir.VersionMadeBy[1] = 0;
  dir.VersionToExtract[0] = 20;
  dir.VersionToExtract[1] = 0;
  dir.Flags = file.method == METHOD_DEFLATE ? LittleShort(2) : 0;
  dir.Method = LittleShort(WORD(file.method));
  dir.ModTime = (WORD) file.time;
  dir.ModDate = (WORD) file.date;
  dir.CRC32 = file.crc32;
  dir.CompressedSize = LittleLong(file.compressed_size);
  dir.UncompressedSize = LittleLong(file.uncompressed_size);
  dir.NameLength = LittleShort((unsigned short)filep.m_path.size());
  dir.ExtraLength = 0;
  dir.CommentLength = 0;
  dir.StartingDiskNumber = 0;
  dir.InternalAttributes = 0;
  dir.ExternalAttributes = 0;
  dir.LocalHeaderOffset = LittleLong(file.zip_offset);

  zip.write((char *)&dir, sizeof(dir));
  zip.write(filep.m_path.c_str(), (long)filep.m_path.size());
  if (!zip.bad()) return true;

  fprintf(stderr, "Error writing central directory header for %s\n", file.path.c_str());
  return false;
}
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
