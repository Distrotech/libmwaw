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
#include "file_internal.h"
#include "input.h"
#include "xattr.h"

#include <sys/stat.h>
#if WITH_EXTENDED_FS
#  include <sys/types.h>
#  include <sys/xattr.h>
#endif

namespace libmwaw_tools
{
InputStream *XAttr::getStream(const char *attr) const
{
  if (m_fName.empty() || !attr)
    return 0;
#if WITH_EXTENDED_FS==1
#  define MWAW_EXTENDED_FS , 0, XATTR_SHOWCOMPRESSION
#else
#  define MWAW_EXTENDED_FS
#endif

#if WITH_EXTENDED_FS
  ssize_t sz=getxattr(m_fName.c_str(), attr, 0, 0 MWAW_EXTENDED_FS);
  if (sz>0) {
    char *buffer = new char[size_t(sz)];
    if (buffer==0) {
      MWAW_DEBUG_MSG(("XAttr::getStream: can not alloc buffer\n"));
      return 0;
    }
    if (getxattr(m_fName.c_str(), attr, buffer, size_t(sz) MWAW_EXTENDED_FS) != sz) {
      MWAW_DEBUG_MSG(("XAttr::getStream: getxattr can read attribute\n"));
      delete [] buffer;
      return 0;
    }

    InputStream *res=new StringStream((unsigned char *)buffer,(unsigned long) sz);
    delete [] buffer;
    return res;
  }
#endif

  InputStream *auxi=getAuxillarInput();
  if (auxi) {
    InputStream *res = unMacMIME(auxi, attr);
    delete auxi;
    return res;
  }

  return getUsingFinderDat(attr);
}

InputStream *XAttr::getAuxillarInput() const
{
  if (m_fName.empty())
    return 0;

  /** look for file ._NAME or __MACOSX/._NAME
      Fixme: Must be probably changed to works on Windows */
  size_t sPos=m_fName.rfind('/');
  std::string folder(""), file("");
  if (sPos==std::string::npos)
    file = m_fName;
  else {
    folder=m_fName.substr(0,sPos+1);
    file=m_fName.substr(sPos+1);
  }
  std::string name=folder+"._"+file;

  struct stat status;
  if (stat(name.c_str(), &status)==0 && S_ISREG(status.st_mode))
    ;
  else {
    name=folder+"__MACOSX/._"+file;
    if (stat(name.c_str(), &status) || !S_ISREG(status.st_mode))
      return 0;
  }

  FileStream *res= new FileStream(name.c_str());
  if (res->ok()) {
    MWAW_DEBUG_MSG(("XAttr::getAuxillarInput: find an auxilliary file: %s\n",name.c_str()));
    return res;
  }
  delete res;
  return 0;
}

/* os9 way to save resource on a Fat,... disk */
InputStream *XAttr::getUsingFinderDat(char const *what) const
{
  if (m_fName.empty())
    return 0;

  bool lookForResourceFork=true;
  if (strcmp("com.apple.FinderInfo",what)==0)
    lookForResourceFork=false;
  else if (strcmp("com.apple.ResourceFork",what))
    return 0;

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
    return 0;

  FileStream *input= new FileStream(name.c_str());
  if (!input || !input->ok()) {
    if (input) delete input;
    return 0;
  }

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
        MWAW_DEBUG_MSG(("XAttr::readFinderDat: file size seems bad %d\n", nSz));
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
      // fInfo or fInfo
      if (!lookForResourceFork) {
        input->seek(pos+32, InputStream::SK_SET);
        unsigned long numBytesRead = 0;
        const unsigned char *data = input->read(16, numBytesRead);
        if (numBytesRead != 16 || !data) {
          MWAW_DEBUG_MSG(("XAttr::readFinderDat: can not read fileinfo\n"));
          break;
        }
        delete input;
        input=0;
        return new StringStream(data,(unsigned long) 16);
      }

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
      if (stat(name.c_str(), &status)!=0 || !S_ISREG(status.st_mode))
        break;
      delete input;
      input=new FileStream(name.c_str());
      if (!input || !input->ok())
        break;
      return input;
    }
  }
  catch (...) {
  }
  if (input) delete input;
  return 0;
}

/* freely inspired from http://tools.ietf.org/html/rfc1740#appendix-A
 */
InputStream *XAttr::unMacMIME(InputStream *inp, char const *what) const
{
  if (!inp || !what) return 0;
  int lookForId=0;
  if (strcmp("com.apple.FinderInfo",what)==0)
    lookForId=9;
  else if (strcmp("com.apple.ResourceFork",what)==0)
    lookForId=2;
  else if (strcmp("com.apple.DataFork",what)==0)
    lookForId=1;
  if (lookForId==0) return 0;

  try {
    inp->seek(0, InputStream::SK_SET);
    long magicNumber = (long) inp->readU32();
    if (magicNumber != 0x00051600 && magicNumber != 0x00051607)
      return 0;
    long version = (long) inp->readU32();
    if (version != 0x20000) {
      MWAW_DEBUG_MSG(("XAttr::unMacMIME: unknown version: %lx\n", version));
      return 0;
    }
    inp->seek(16, InputStream::SK_CUR); // filename
    long numEntries = (long) inp->readU16();
    if (inp->atEOS() || numEntries <= 0) {
      MWAW_DEBUG_MSG(("XAttr::unMacMIME: can not read number of entries\n"));
      return 0;
    }
    for (int i = 0; i < numEntries; i++) {
      long pos = inp->tell();
      int wh = (int) inp->readU32();
      if (wh <= 0 || wh >= 16 || inp->atEOS()) {
        MWAW_DEBUG_MSG(("XAttr::unMacMIME: find unknown id: %d\n", wh));
        return 0;
      }
      MWAW_DEBUG_MSG(("XAttr::unMacMIME: find entry %d\n", wh));
      if (wh != lookForId) {
        inp->seek(8, InputStream::SK_CUR);
        continue;
      }
      long entryPos = (long) inp->readU32();
      unsigned long entrySize = (unsigned long) inp->readU32();
      if (entrySize == 0) {
        MWAW_DEBUG_MSG(("XAttr::unMacMIME: entry %d is empty\n", wh));
        return 0;
      }
      if (entryPos <= pos || entrySize == 0) {
        MWAW_DEBUG_MSG(("XAttr::unMacMIME: find bad entry pos\n"));
        return 0;
      }
      /* try to read the data */
      inp->seek(entryPos, InputStream::SK_SET);
      if (inp->tell() != entryPos) {
        MWAW_DEBUG_MSG(("XAttr::unMacMIME: can not seek entry pos %lx\n", entryPos));
        return 0;
      }
      unsigned long numBytesRead = 0;
      const unsigned char *data = inp->read(entrySize, numBytesRead);
      if (numBytesRead != entrySize || !data) {
        MWAW_DEBUG_MSG(("XAttr::unMacMIME: can not read %lX byte\n", entryPos));
        return 0;
      }
      return new StringStream(data,(unsigned long) entrySize);
    }
  }
  catch (...) {
    return 0;
  }
  return 0;
}
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
