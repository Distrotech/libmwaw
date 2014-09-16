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
* Major Contributor(s):
* Copyright (C) 2002 William Lachance (wrlach@gmail.com)
* Copyright (C) 2002,2004 Marc Maurer (uwog@uwog.net)
* Copyright (C) 2004-2006 Fridrich Strba (fridrich.strba@bluewin.ch)
* Copyright (C) 2006, 2007 Andrew Ziem
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

#include <limits>
#include <cmath>
#include <cstring>

#include <librevenge-stream/librevenge-stream.h>
#include <librevenge/librevenge.h>

#include "MWAWDebug.hxx"

#include "MWAWInputStream.hxx"
#include "MWAWStringStream.hxx"

MWAWInputStream::MWAWInputStream(shared_ptr<librevenge::RVNGInputStream> inp, bool inverted)
  : m_stream(inp), m_streamSize(0), m_inverseRead(inverted), m_readLimit(-1), m_prevLimits(),
    m_fInfoType(""), m_fInfoCreator(""), m_resourceFork()
{
  updateStreamSize();
}

MWAWInputStream::MWAWInputStream(librevenge::RVNGInputStream *inp, bool inverted, bool checkCompression)
  : m_stream(), m_streamSize(0), m_inverseRead(inverted), m_readLimit(-1), m_prevLimits(),
    m_fInfoType(""), m_fInfoCreator(""), m_resourceFork()
{
  if (!inp) return;

  m_stream = shared_ptr<librevenge::RVNGInputStream>(inp, MWAW_shared_ptr_noop_deleter<librevenge::RVNGInputStream>());
  updateStreamSize();
  if (!checkCompression)
    return;

  // first check the zip format
  if (unzipStream())
    updateStreamSize();

  // check if the data are in binhex format
  if (unBinHex())
    updateStreamSize();

  // now check for MacMIME format in m_stream or in m_resourceFork
  if (unMacMIME())
    updateStreamSize();
  if (m_stream)
    seek(0, librevenge::RVNG_SEEK_SET);
  if (m_resourceFork)
    m_resourceFork->seek(0, librevenge::RVNG_SEEK_SET);
}

MWAWInputStream::~MWAWInputStream()
{
}

shared_ptr<MWAWInputStream> MWAWInputStream::get(librevenge::RVNGBinaryData const &data, bool inverted)
{
  shared_ptr<MWAWInputStream> res;
  if (!data.size())
    return res;
  librevenge::RVNGInputStream *dataStream = const_cast<librevenge::RVNGInputStream *>(data.getDataStream());
  if (!dataStream) {
    MWAW_DEBUG_MSG(("MWAWInputStream::get: can not retrieve a librevenge::RVNGInputStream\n"));
    return res;
  }
  res.reset(new MWAWInputStream(dataStream, inverted));
  if (res && res->size()>=(long) data.size()) {
    res->seek(0, librevenge::RVNG_SEEK_SET);
    return res;
  }
  MWAW_DEBUG_MSG(("MWAWInputStream::get: the final stream seems bad\n"));
  res.reset();
  return res;
}

void MWAWInputStream::updateStreamSize()
{
  if (!m_stream)
    m_streamSize=0;
  else {
    long actPos = tell();
    m_stream->seek(0, librevenge::RVNG_SEEK_END);
    m_streamSize=tell();
    m_stream->seek(actPos, librevenge::RVNG_SEEK_SET);
  }
}

const uint8_t *MWAWInputStream::read(size_t numBytes, unsigned long &numBytesRead)
{
  if (!hasDataFork())
    throw libmwaw::FileException();
  return m_stream->read(numBytes,numBytesRead);
}

long MWAWInputStream::tell()
{
  if (!hasDataFork())
    return 0;
  return m_stream->tell();
}

int MWAWInputStream::seek(long offset, librevenge::RVNG_SEEK_TYPE seekType)
{
  if (!hasDataFork()) {
    if (offset == 0)
      return 0;
    throw libmwaw::FileException();
  }

  if (seekType == librevenge::RVNG_SEEK_CUR)
    offset += tell();
  else if (seekType==librevenge::RVNG_SEEK_END)
    offset += m_streamSize;

  if (offset < 0)
    offset = 0;
  if (m_readLimit > 0 && offset > (long)m_readLimit)
    offset = (long)m_readLimit;
  if (offset > size())
    offset = size();

  return m_stream->seek(offset, librevenge::RVNG_SEEK_SET);
}

bool MWAWInputStream::isEnd()
{
  if (!hasDataFork())
    return true;
  long pos = m_stream->tell();
  if (m_readLimit > 0 && pos >= m_readLimit) return true;
  if (pos >= size()) return true;

  return m_stream->isEnd();
}

unsigned long MWAWInputStream::readULong(librevenge::RVNGInputStream *stream, int num, unsigned long a, bool inverseRead)
{
  if (!stream || num == 0 || stream->isEnd()) return a;
  if (inverseRead) {
    unsigned long val = readU8(stream);
    return val + (readULong(stream, num-1,0, inverseRead) << 8);
  }
  switch (num) {
  case 4:
  case 2:
  case 1: {
    unsigned long numBytesRead;
    uint8_t const *p = stream->read((unsigned long) num, numBytesRead);
    if (!p || int(numBytesRead) != num)
      return 0;
    switch (num) {
    case 4:
      return (unsigned long)p[3]|((unsigned long)p[2]<<8)|
             ((unsigned long)p[1]<<16)|((unsigned long)p[0]<<24)|((a<<16)<<16);
    case 2:
      return ((unsigned long)p[1])|((unsigned long)p[0]<<8)|(a<<16);
    case 1:
      return ((unsigned long)p[0])|(a<<8);
    default:
      break;
    }
    break;
  }
  default:
    return readULong(stream, num-1,(a<<8) + (unsigned long)readU8(stream), inverseRead);
  }
  return 0;
}

long MWAWInputStream::readLong(int num)
{
  long v = long(readULong(num));
  switch (num) {
  case 4:
    return (int32_t) v;
  case 2:
    return (int16_t) v;
  case 1:
    return (int8_t) v;
  default:
    break;
  }

  if ((v & long(0x1 << (num*8-1))) == 0) return v;
  return v | long(0xFFFFFFFF << 8*num);
}

uint8_t MWAWInputStream::readU8(librevenge::RVNGInputStream *stream)
{
  if (!stream)
    return 0;
  unsigned long numBytesRead;
  uint8_t const *p = stream->read(sizeof(uint8_t), numBytesRead);

  if (!p || numBytesRead != sizeof(uint8_t))
    return 0;

  return *(uint8_t const *)(p);
}

bool MWAWInputStream::readDouble8(double &res, bool &isNotANumber)
{
  if (!m_stream) return false;
  long pos=tell();
  if (m_readLimit > 0 && pos+8 > m_readLimit) return false;
  if (pos+8 > m_streamSize) return false;

  isNotANumber=false;
  res=0;
  int mantExp=int(readULong(1));
  int val=(int) readULong(1);
  int exp=(mantExp<<4)+(val>>4);
  double mantisse=double(val&0xF)/16.;
  double factor=1./16/256.;
  for (int j = 0; j < 6; ++j, factor/=256)
    mantisse+=(double)readULong(1)*factor;
  int sign = 1;
  if (exp & 0x800) {
    exp &= 0x7ff;
    sign = -1;
  }
  if (exp == 0) {
    if (mantisse <= 1.e-5 || mantisse >= 1-1.e-5)
      return true;
    // a Nan representation ?
    return false;
  }
  if (exp == 0x7FF) {
    if (mantisse >= 1.-1e-5) {
      isNotANumber=true;
      res=std::numeric_limits<double>::quiet_NaN();
      return true; // ok 0x7FF and 0xFFF are nan
    }
    return false;
  }
  exp -= 0x3ff;
  res = std::ldexp(1.+mantisse, exp);
  if (sign == -1)
    res *= -1.;
  return true;
}

bool MWAWInputStream::readDouble10(double &res, bool &isNotANumber)
{
  if (!m_stream) return false;
  long pos=tell();
  if (m_readLimit > 0 && pos+10 > m_readLimit) return false;
  if (pos+10 > m_streamSize) return false;

  int exp = (int) readULong(2);
  int sign = 1;
  if (exp & 0x8000) {
    exp &= 0x7fff;
    sign = -1;
  }
  exp -= 0x3fff;

  isNotANumber=false;
  unsigned long mantisse = (unsigned long) readULong(4);
  if ((mantisse & 0x80000001) == 0) {
    // unormalized number are not frequent, but can appear at least for date, ...
    if (readULong(4) != 0)
      seek(-4, librevenge::RVNG_SEEK_CUR);
    else {
      if (exp == -0x3fff && mantisse == 0) {
        res=0;
        return true; // ok zero
      }
      if (exp == 0x4000 && (mantisse & 0xFFFFFFL)==0) { // ok Nan
        isNotANumber = true;
        res=std::numeric_limits<double>::quiet_NaN();
        return true;
      }
      return false;
    }
  }
  // or std::ldexp((total value)/double(0x80000000), exp);
  res=std::ldexp(double(readULong(4)),exp-63)+std::ldexp(double(mantisse),exp-31);
  if (sign == -1)
    res *= -1.;
  return true;
}

bool MWAWInputStream::readDoubleReverted8(double &res, bool &isNotANumber)
{
  if (!m_stream) return false;
  long pos=tell();
  if (m_readLimit > 0 && pos+8 > m_readLimit) return false;
  if (pos+8 > m_streamSize) return false;

  isNotANumber=false;
  res=0;
  int bytes[6];
  for (int i=0; i<6; ++i) bytes[i]=(int) readULong(1);

  int val=(int) readULong(1);
  int mantExp=int(readULong(1));
  int exp=(mantExp<<4)+(val>>4);
  double mantisse=double(val&0xF)/16.;
  double factor=1./16./256.;
  for (int j = 0; j < 6; ++j, factor/=256)
    mantisse+=(double)bytes[5-j]*factor;
  int sign = 1;
  if (exp & 0x800) {
    exp &= 0x7ff;
    sign = -1;
  }
  if (exp == 0) {
    if (mantisse <= 1.e-5 || mantisse >= 1-1.e-5)
      return true;
    // a Nan representation ?
    return false;
  }
  if (exp == 0x7FF) {
    if (mantisse >= 1.-1e-5) {
      isNotANumber=true;
      res=std::numeric_limits<double>::quiet_NaN();
      return true; // ok 0x7FF and 0xFFF are nan
    }
    return false;
  }
  exp -= 0x3ff;
  res = std::ldexp(1.+mantisse, exp);
  if (sign == -1)
    res *= -1.;
  return true;
}

////////////////////////////////////////////////////////////
//
// BinHex 4.0
//
////////////////////////////////////////////////////////////
bool MWAWInputStream::unBinHex()
{
  if (!hasDataFork() || size() < 45)
    return false;
  // check header
  seek(0, librevenge::RVNG_SEEK_SET);
  unsigned long nRead;
  char const *str=(char const *) read(45,nRead);
  if (str==0 || nRead!=45
      || strncmp(str, "(This file must be converted with BinHex 4.0)",45))
    return false;
  int numEOL = 0;
  while (!isEnd()) {
    char c = (char) readLong(1);
    if (c == '\n') {
      numEOL++;
      continue;
    }
    seek(-1, librevenge::RVNG_SEEK_CUR);
    break;
  }
  if (isEnd() || !numEOL || ((char)readLong(1))!= ':')
    return false;

  // first phase reconstruct the file
  static char const binChar[65] = "!\"#$%&'()*+,-012345689@ABCDEFGHIJKLMNPQRSTUVXYZ[`abcdefhijklmpqr";
  std::map<unsigned char,int>  binMap;
  for (int i = 0; i < 64; i++) binMap[(unsigned char)binChar[i]]=i;
  bool endData = false;
  int numActByte = 0, actVal = 0;
  librevenge::RVNGBinaryData content;
  bool findRepetitif = false;
  while (1) {
    if (isEnd()) {
      MWAW_DEBUG_MSG(("MWAWInputStream::unBinHex: do not find ending ':' character\n"));
      return false;
    }
    unsigned char c = (unsigned char) readLong(1);
    if (c == '\n') continue;
    int readVal = 0;
    if (c == ':')
      endData = true;
    else if (binMap.find(c) == binMap.end()) {
      MWAW_DEBUG_MSG(("MWAWInputStream::unBinHex: find unexpected char when decoding file\n"));
      return false;
    }
    else
      readVal = binMap.find(c)->second;
    int wVal = -1;
    if (numActByte==0)
      actVal |= (readVal<<2);
    else if (numActByte==2) {
      wVal = (actVal | readVal);
      actVal = 0;
    }
    else if (numActByte==4) {
      wVal = actVal | ((readVal>>2)&0xF);
      actVal = (readVal&0x3)<<6;
    }
    else if (numActByte==6) {
      wVal = actVal | ((readVal>>4)&0x3);
      actVal = (readVal&0xf)<<4;
    }
    numActByte = (numActByte+6)%8;

    int maxToWrite = (endData&&actVal) ? 2 : 1;
    for (int wPos = 0; wPos < maxToWrite; wPos++) {
      int value = wPos ? actVal : wVal;
      if (value == -1) continue;
      if (!findRepetitif && value != 0x90) {
        content.append((unsigned char) value);
        continue;
      }
      if (value == 0x90 && !findRepetitif) {
        findRepetitif = true;
        continue;
      }

      if (value == 1 || value == 2) {
        MWAW_DEBUG_MSG(("MWAWInputStream::unBinHex: find bad value after repetif character\n"));
        return false;
      }
      findRepetitif = false;
      if (value == 0) {
        content.append(0x90);
        continue;
      }
      if (content.size()==0) {
        MWAW_DEBUG_MSG(("MWAWInputStream::unBinHex: find repetif character in the first position\n"));
        return false;
      }
      unsigned char lChar = content.getDataBuffer()[content.size()-1];
      for (int i = 0; i < value-1; i++)
        content.append(lChar);
      continue;
    }
    if (endData) break;
  }
  if (findRepetitif) {
    MWAW_DEBUG_MSG(("MWAWInputStream::unBinHex: find repetif character in the last position\n"));
    return false;
  }
  long contentSize=(long) content.size();
  if (contentSize < 27) {
    MWAW_DEBUG_MSG(("MWAWInputStream::unBinHex: the content file is too small\n"));
    return false;
  }
  librevenge::RVNGInputStream *contentInput=const_cast<librevenge::RVNGInputStream *>(content.getDataStream());
  int fileLength = (int)  readU8(contentInput);
  if (fileLength < 1 || fileLength > 64 || long(fileLength+21) > contentSize) {
    MWAW_DEBUG_MSG(("MWAWInputStream::unBinHex: the file name size seems odd\n"));
    return false;
  }
  contentInput->seek(fileLength+1, librevenge::RVNG_SEEK_CUR); // filename + version
  // creator, type
  std::string type(""), creator("");
  for (int p = 0; p < 4; p++) {
    char c = (char) readU8(contentInput);
    if (c)
      type += c;
  }
  for (int p = 0; p < 4; p++) {
    char c = (char) readU8(contentInput);
    if (c)
      creator += c;
  }
  if (creator.length()==4 && type.length()==4) {
    m_fInfoType = type;
    m_fInfoCreator = creator;
  }
  else if (creator.length() || type.length()) {
    MWAW_DEBUG_MSG(("MWAWInputStream::unBinHex: the file name size seems odd\n"));
  }
  contentInput->seek(2, librevenge::RVNG_SEEK_CUR); // skip flags
  long dataLength = (long) readULong(contentInput,4,0,false);
  long rsrcLength = (long) readULong(contentInput,4,0,false);
  long pos = contentInput->tell()+2; // skip CRC
  if (dataLength<0 || rsrcLength < 0 || (dataLength==0 && rsrcLength==0) ||
      pos+dataLength+rsrcLength+4 > contentSize) {
    MWAW_DEBUG_MSG(("MWAWInputStream::unBinHex: the data/rsrc fork size seems odd\n"));
    return false;
  }
  // now read the rsrc and the data fork
  if (rsrcLength && getResourceForkStream()) {
    MWAW_DEBUG_MSG(("MWAWInputStream::unBinHex: I already have a resource fork!!!!\n"));
  }
  else if (rsrcLength) {
    contentInput->seek(pos+dataLength+2, librevenge::RVNG_SEEK_SET);
    unsigned long numBytesRead = 0;
    const unsigned char *data =
      contentInput->read((unsigned long)rsrcLength, numBytesRead);
    if (numBytesRead != (unsigned long)rsrcLength || !data) {
      MWAW_DEBUG_MSG(("MWAWInputStream::unBinHex: can not read the resource fork\n"));
    }
    else {
      shared_ptr<librevenge::RVNGInputStream> rsrc(new MWAWStringStream(data, (unsigned int)numBytesRead));
      m_resourceFork.reset(new MWAWInputStream(rsrc,false));
    }
  }
  if (!dataLength)
    m_stream.reset();
  else {
    contentInput->seek(pos, librevenge::RVNG_SEEK_SET);
    unsigned long numBytesRead = 0;
    const unsigned char *data =
      contentInput->read((unsigned long)dataLength, numBytesRead);
    if (numBytesRead != (unsigned long)dataLength || !data) {
      MWAW_DEBUG_MSG(("MWAWInputStream::unBinHex: can not read the data fork\n"));
      return false;
    }
    m_stream.reset(new MWAWStringStream(data, (unsigned int)numBytesRead));
  }

  return true;
}

////////////////////////////////////////////////////////////
//
// Zip
//
////////////////////////////////////////////////////////////
bool MWAWInputStream::unzipStream()
{
  if (!isStructured()) return false;
  seek(0, librevenge::RVNG_SEEK_SET);
  unsigned numStream=m_stream->subStreamCount();
  std::vector<std::string> names;
  for (unsigned n=0; n < numStream; ++n) {
    char const *nm=m_stream->subStreamName(n);
    if (!nm) continue;
    std::string name(nm);
    if (name.empty() || name[name.length()-1]=='/') continue;
    names.push_back(nm);
  }
  if (names.size() == 1) {
    // ok as the OLE file must have at least MN and MN0 OLE
    m_stream.reset(m_stream->getSubStreamByName(names[0].c_str()));
    return true;
  }
  if (names.size() != 2)
    return false;
  // test if XXX and ._XXX or __MACOSX/._XXX
  if (names[1].length() < names[0].length()) {
    std::string tmp = names[1];
    names[1] = names[0];
    names[0] = tmp;
  }
  size_t length = names[0].length();
  std::string prefix("");
  if (names[1].length()==length+2)
    prefix = "._";
  else if (names[1].length()==length+11)
    prefix = "__MACOSX/._";
  prefix += names[0];
  if (prefix != names[1]) return false;
  shared_ptr<librevenge::RVNGInputStream> rsrcPtr(m_stream->getSubStreamByName(names[1].c_str()));
  m_resourceFork.reset(new MWAWInputStream(rsrcPtr,false));
  m_stream.reset(m_stream->getSubStreamByName(names[0].c_str()));
  return true;
}

////////////////////////////////////////////////////////////
//
// MacMIME part
//
////////////////////////////////////////////////////////////
bool MWAWInputStream::unMacMIME()
{
  if (m_resourceFork) {
    shared_ptr<librevenge::RVNGInputStream> newDataInput, newRsrcInput;
    bool ok = unMacMIME(m_resourceFork.get(), newDataInput, newRsrcInput);
    if (ok && newDataInput) {
      MWAW_DEBUG_MSG(("MWAWInputStream::unMacMIME: Argh!!! find data stream in the resource block\n"));
      ok = false;
    }
    if (ok && newRsrcInput)
      m_resourceFork.reset(new MWAWInputStream(newRsrcInput,false));
    else if (ok)
      m_resourceFork.reset();
  }

  if (m_stream) {
    shared_ptr<librevenge::RVNGInputStream> newDataInput, newRsrcInput;
    bool ok = unMacMIME(this, newDataInput, newRsrcInput);
    if (ok && !newDataInput) {
      MWAW_DEBUG_MSG(("MWAWInputStream::unMacMIME: Argh!!! data block contains only resources\n"));
      ok = false;
    }
    if (ok) {
      m_stream = newDataInput;
      if (newRsrcInput) {
        if (m_resourceFork) {
          MWAW_DEBUG_MSG(("MWAWInputStream::unMacMIME: Oops!!! find a second resource block, ignored\n"));
        }
        else
          m_resourceFork.reset(new MWAWInputStream(newRsrcInput,false));
      }
    }
  }
  return true;
}

/* freely inspired from http://tools.ietf.org/html/rfc1740#appendix-A
 */
bool MWAWInputStream::unMacMIME(MWAWInputStream *inp,
                                shared_ptr<librevenge::RVNGInputStream> &dataInput,
                                shared_ptr<librevenge::RVNGInputStream> &rsrcInput) const
{
  dataInput.reset();
  rsrcInput.reset();
  if (!inp || !inp->hasDataFork() || inp->size()<26) return false;

  try {
    inp->seek(0, librevenge::RVNG_SEEK_SET);
    long magicNumber = (long) inp->readULong(4);
    if (magicNumber != 0x00051600 && magicNumber != 0x00051607)
      return false;
    long version = (long) inp->readULong(4);
    if (version != 0x20000) {
      MWAW_DEBUG_MSG(("MWAWInputStream::unMacMIME: unknown version: %lx\n", version));
      return false;
    }
    inp->seek(16, librevenge::RVNG_SEEK_CUR); // filename
    long numEntries = (long) inp->readULong(2);
    if (inp->isEnd() || numEntries <= 0) {
      MWAW_DEBUG_MSG(("MWAWInputStream::unMacMIME: can not read number of entries\n"));
      return false;
    }
#ifdef DEBUG
    static const char *(what[]) = {
      "", "Data", "Rsrc", "FileName", "Comment", "IconBW", "IconColor", "",
      "FileDates", "FinderInfo", "MacInfo", "ProDosInfo",
      "MSDosInfo", "AFPName", "AFPInfo", "AFPDirId"
    };
#endif
    for (int i = 0; i < numEntries; i++) {
      long pos = inp->tell();
      int wh = (int) inp->readULong(4);
      if (wh <= 0 || wh >= 16 || inp->isEnd()) {
        MWAW_DEBUG_MSG(("MWAWInputStream::unMacMIME: find unknown id: %d\n", wh));
        return false;
      }
      MWAW_DEBUG_MSG(("MWAWInputStream::unMacMIME: find %s entry\n", what[wh]));
      if (wh > 2 && wh != 9) {
        inp->seek(8, librevenge::RVNG_SEEK_CUR);
        continue;
      }
      long entryPos = (long) inp->readULong(4);
      unsigned long entrySize = inp->readULong(4);
      if (entrySize == 0) {
        MWAW_DEBUG_MSG(("MWAWInputStream::unMacMIME: %s entry is empty\n", what[wh]));
        continue;
      }
      if (entryPos <= pos || entrySize == 0) {
        MWAW_DEBUG_MSG(("MWAWInputStream::unMacMIME: find bad entry pos\n"));
        return false;
      }
      /* try to read the data */
      inp->seek(entryPos, librevenge::RVNG_SEEK_SET);
      if (inp->tell() != entryPos) {
        MWAW_DEBUG_MSG(("MWAWInputStream::unMacMIME: can not seek entry pos %lx\n", entryPos));
        return false;
      }
      unsigned long numBytesRead = 0;
      const unsigned char *data = inp->read(entrySize, numBytesRead);
      if (numBytesRead != entrySize || !data) {
        MWAW_DEBUG_MSG(("MWAWInputStream::unMacMIME: can not read %lX byte\n", entryPos));
        return false;
      }
      if (wh==1)
        dataInput.reset(new MWAWStringStream(data, (unsigned int)numBytesRead));
      else if (wh==2)
        rsrcInput.reset(new MWAWStringStream(data, (unsigned int)numBytesRead));
      else { // the finder info
        if (entrySize < 8) {
          MWAW_DEBUG_MSG(("MWAWInputStream::unMacMIME: finder info size is odd\n"));
        }
        else {
          bool ok = true;
          std::string type(""), creator("");
          for (int p = 0; p < 4; p++) {
            if (!data[p]) {
              ok = false;
              break;
            }
            type += char(data[p]);
          }
          for (int p = 4; ok && p < 8; p++) {
            if (!data[p]) {
              ok = false;
              break;
            }
            creator += char(data[p]);
          }
          if (ok) {
            m_fInfoType = type;
            m_fInfoCreator = creator;
          }
          else if (type.length()) {
            MWAW_DEBUG_MSG(("MWAWInputStream::unMacMIME: can not read find info\n"));
          }
        }
      }

      inp->seek(pos+12, librevenge::RVNG_SEEK_SET);
    }
  }
  catch (...) {
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////
//
// OLE part
//
////////////////////////////////////////////////////////////

bool MWAWInputStream::isStructured()
{
  if (!m_stream) return false;
  long pos=m_stream->tell();
  bool ok=m_stream->isStructured();
  m_stream->seek(pos, librevenge::RVNG_SEEK_SET);
  return ok;
}

unsigned MWAWInputStream::subStreamCount()
{
  if (!m_stream || !m_stream->isStructured()) {
    MWAW_DEBUG_MSG(("MWAWInputStream::subStreamCount: called on unstructured file\n"));
    return 0;
  }
  return m_stream->subStreamCount();
}

std::string MWAWInputStream::subStreamName(unsigned id)
{
  if (!m_stream || !m_stream->isStructured()) {
    MWAW_DEBUG_MSG(("MWAWInputStream::subStreamName: called on unstructured file\n"));
    return std::string("");
  }
  char const *nm=m_stream->subStreamName(id);
  if (!nm) {
    MWAW_DEBUG_MSG(("MWAWInputStream::subStreamName: can not find stream %d\n", int(id)));
    return std::string("");
  }
  return std::string(nm);
}

shared_ptr<MWAWInputStream> MWAWInputStream::getSubStreamByName(std::string const &name)
{
  static shared_ptr<MWAWInputStream> empty;
  if (!m_stream || !m_stream->isStructured() || name.empty()) {
    MWAW_DEBUG_MSG(("MWAWInputStream::getSubStreamByName: called on unstructured file\n"));
    return empty;
  }

  long actPos = tell();
  seek(0, librevenge::RVNG_SEEK_SET);
  shared_ptr<librevenge::RVNGInputStream> res(m_stream->getSubStreamByName(name.c_str()));
  seek(actPos, librevenge::RVNG_SEEK_SET);

  if (!res)
    return empty;
  shared_ptr<MWAWInputStream> inp(new MWAWInputStream(res,m_inverseRead));
  inp->seek(0, librevenge::RVNG_SEEK_SET);
  return inp;
}

shared_ptr<MWAWInputStream> MWAWInputStream::getSubStreamById(unsigned id)
{
  static shared_ptr<MWAWInputStream> empty;
  if (!m_stream || !m_stream->isStructured()) {
    MWAW_DEBUG_MSG(("MWAWInputStream::getSubStreamById: called on unstructured file\n"));
    return empty;
  }

  long actPos = tell();
  seek(0, librevenge::RVNG_SEEK_SET);
  shared_ptr<librevenge::RVNGInputStream> res(m_stream->getSubStreamById(id));
  seek(actPos, librevenge::RVNG_SEEK_SET);

  if (!res)
    return empty;
  shared_ptr<MWAWInputStream> inp(new MWAWInputStream(res,m_inverseRead));
  inp->seek(0, librevenge::RVNG_SEEK_SET);
  return inp;
}

////////////////////////////////////////////////////////////
//
//  a function to read a data block
//
////////////////////////////////////////////////////////////

bool MWAWInputStream::readDataBlock(long sz, librevenge::RVNGBinaryData &data)
{
  if (!hasDataFork()) return false;

  data.clear();
  if (sz < 0) return false;
  if (sz == 0) return true;
  long endPos=tell()+sz;
  if (endPos > size()) return false;
  if (m_readLimit > 0 && endPos > (long)m_readLimit) return false;

  const unsigned char *readData;
  unsigned long sizeRead;
  if ((readData=m_stream->read((unsigned long)sz, sizeRead)) == 0 || long(sizeRead)!=sz)
    return false;
  data.append(readData, sizeRead);
  return true;
}

bool MWAWInputStream::readEndDataBlock(librevenge::RVNGBinaryData &data)
{
  data.clear();
  if (!hasDataFork()) return false;

  long endPos=m_readLimit>0 ? m_readLimit : size();
  return readDataBlock(endPos-tell(), data);
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
