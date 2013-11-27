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

#include <algorithm>

#include <libmwaw_internal.hxx>
#include "input.h"
#include "xattr.h"

#include <sys/stat.h>
#if WITH_EXTENDED_FS
#  include <sys/types.h>
#  include <sys/xattr.h>
#endif

#ifndef __BIG_ENDIAN__
static void addShort(unsigned short x, char *&buff)
{
  *(buff++)=char(x>>8);
  *(buff++)=char(x&0xFF);
}
static void addLong(unsigned int x, char *&buff)
{
  *(buff++)=char((x>>24)&0xFF);
  *(buff++)=char((x>>16)&0xFF);
  *(buff++)=char((x>>8)&0xFF);
  *(buff++)=char(x&0xFF);
}
#else
static void addShort(unsigned short x, char *&buff)
{
  *(buff++)=char(x&0xFF);
  *(buff++)=char(x>>8);
}
static void addLong(unsigned int x, char *&buff)
{
  *(buff++)=char(x&0xFF);
  *(buff++)=char((x>>8)&0xFF);
  *(buff++)=char((x>>16)&0xFF);
  *(buff++)=char((x>>24)&0xFF);
}
#endif

namespace libmwaw_zip
{
shared_ptr<InputStream> XAttr::getStream() const
{
  shared_ptr<InputStream> res;
  if (m_fName.length()==0)
    return res;
#if WITH_EXTENDED_FS==0
  return res;
#else

#  if WITH_EXTENDED_FS==1
#    define MWAW_EXTENDED_FS , 0, XATTR_SHOWCOMPRESSION
#  else
#    define MWAW_EXTENDED_FS
#  endif
  ssize_t sz[2];
  static char const *(attr[2])= { "com.apple.FinderInfo", "com.apple.ResourceFork" };
  int find=0;
  for (int i = 0; i < 2; i++) {
    sz[i]=getxattr(m_fName.c_str(), attr[i], 0, 0 MWAW_EXTENDED_FS);
    if (sz[i]) find++;
  }
  if (!find) return res;

  //ok we must create a MIME stream
  ssize_t totalSz= sz[0]+sz[1]+find*12+30;
  char *buffer = new char[size_t(totalSz)];
  if (buffer==0) {
    MWAW_DEBUG_MSG(("XAttr::getStream: can not alloc buffer\n"));
    return res;
  }
  char *bufferPtr=buffer;
  addLong(0x00051607, bufferPtr);            // signature
  addLong(0x20000, bufferPtr);               // version
  static char defaultName[]="Mac OS X        ";
  for (int i = 0; i < 16; i++)
    *(bufferPtr++) = defaultName[i];
  addShort((unsigned short)find, bufferPtr); // num of data

  // the data header
  unsigned int offset=30+(unsigned int)find*12;
  unsigned int const wh[2]= {9,2};
  for (int i=0; i < 2; i++) {
    if (!sz[i]) continue;
    addLong(wh[i],bufferPtr);                // id
    addLong(offset,bufferPtr);               // offset
    addLong((unsigned int)sz[i],bufferPtr);  // size
    offset+=(unsigned int)sz[i];
  }
  addLong(0, bufferPtr);
  // the data
  for (int i=0; i < 2; i++) {
    if (!sz[i]) continue;
    if (getxattr(m_fName.c_str(), attr[i], bufferPtr, size_t(sz[i]) MWAW_EXTENDED_FS) != sz[i]) {
      MWAW_DEBUG_MSG(("XAttr::getStream: getxattr can read attribute\n"));
      delete [] buffer;
      return res;
    }
    bufferPtr+=sz[i];
  }

  res.reset(new StringStream((unsigned char *) buffer,(unsigned long)totalSz));
  delete [] buffer;
  return res;
#endif
}

shared_ptr<InputStream> XAttr::getClassicStream() const
{
  shared_ptr<InputStream> res;
  if (m_fName.length()==0)
    return res;

  /** look for file FINDER.DAT */
  size_t sPos=m_fName.rfind('/');
  std::string folder(""), file("");
  if (sPos==std::string::npos)
    file = m_fName;
  else {
    folder=m_fName.substr(0,sPos+1);
    file=m_fName.substr(sPos+1);
  }
  std::string name=folder+"FINDER.DAT";
  struct stat status;
  if (stat(name.c_str(), &status)!=0 || !S_ISREG(status.st_mode))
    return res;

  shared_ptr<FileStream> input(new FileStream(name.c_str()));
  if (!input || !input->ok())
    return res;
  input->seek(0, InputStream::SK_SET);
  try {
    int num=0;
    while (!input->atEOS()) {
      if (++num==23) { // must realign read on 2048
        if (input->seek(24, InputStream::SK_CUR))
          break;
        num=0;
      }
      long pos=input->tell();
      if (input->seek(pos+92, InputStream::SK_SET))
        break;
      input->seek(pos, InputStream::SK_SET);
      int nSz=(int) input->readU8();
      if (nSz<=0 || nSz>31) {
        MWAW_DEBUG_MSG(("XAttr::getClassicStream: file size seems bad %d\n", nSz));
        input->seek(pos+92, InputStream::SK_SET);
        continue;
      }
      std::string fName("");
      for (int i=0; i<nSz; ++i)
        fName+=(char) input->readU8();
      if (fName!=file) {
        input->seek(pos+92, InputStream::SK_SET);
        continue;
      }
      int const finderInfoSize=32;
      ssize_t sz[2]= {finderInfoSize,0};
      // try to find the ressource fork
      input->seek(pos+80, InputStream::SK_SET);
      std::string rsrcName("");
      for (int i=0; i<8; ++i) {
        char c=(char) input->readU8();
        if (!c) break;
        rsrcName += c;
      }
      for (int i=0; i <3; ++i) { // extension
        char c=(char) input->readU8();
        if (!c) break;
        if (i==0) rsrcName += '.';
        rsrcName += c;
      }
      name=folder+"RESOURCE.FRK/"+rsrcName;
      if (stat(name.c_str(), &status)==0 && S_ISREG(status.st_mode))
        sz[1]=ssize_t(status.st_size);
      int find=sz[1] ? 2 : 1;

      //ok we must create a MIME stream
      ssize_t totalSz= sz[0]+sz[1]+find*12+30;
      char *buffer = new char[size_t(totalSz)];
      if (buffer==0) {
        MWAW_DEBUG_MSG(("XAttr::getClassicStream: can not alloc buffer\n"));
        return res;
      }
      char *bufferPtr=buffer;
      addLong(0x00051607, bufferPtr);            // signature
      addLong(0x20000, bufferPtr);               // version
      memcpy(bufferPtr,"Mac OS X        ",16); // defaultName
      bufferPtr+=16;
      addShort((unsigned short)find, bufferPtr); // num of data

      // the data header
      unsigned int offset=30+(unsigned int)find*12;
      unsigned int const wh[2]= {9,2};
      for (int i=0; i < 2; i++) {
        if (!sz[i]) continue;
        addLong(wh[i],bufferPtr);                // id
        addLong(offset,bufferPtr);               // offset
        addLong((unsigned int)sz[i],bufferPtr);  // size
        offset+=(unsigned int)sz[i];
      }
      addLong(0, bufferPtr);

      // the data

      // fInfo or fInfo
      input->seek(pos+32, InputStream::SK_SET);
      unsigned long numBytesRead = 0;
      const unsigned char *data = input->read(finderInfoSize, numBytesRead);
      if (numBytesRead != (unsigned long)finderInfoSize || !data) {
        MWAW_DEBUG_MSG(("XAttr::getClassicStream::readFinderDat: can not read fileinfo\n"));
        delete [] buffer;
        return res;
      }
      memcpy(bufferPtr, &data[0], finderInfoSize);
      bufferPtr+=finderInfoSize;

      if (sz[1]) {
        input.reset(new FileStream(name.c_str()));
        if (!input || !input->ok()) {
          delete [] buffer;
          return res;
        }
        numBytesRead = 0;
        data = input->read((unsigned long)sz[1], numBytesRead);
        if (numBytesRead != (unsigned long) sz[1] || !data) {
          MWAW_DEBUG_MSG(("XAttr::getClassicStream::readFinderDat: can not read resource fork\n"));
          delete [] buffer;
          return res;
        }
        memcpy(bufferPtr, &data[0], (size_t)sz[1]);
        bufferPtr+=sz[1];
      }

      res.reset(new StringStream((unsigned char *) buffer,
                                 (unsigned long)totalSz));
      delete [] buffer;
      return res;
    }
  }
  catch (...) {
  }
  return res;
}

}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
