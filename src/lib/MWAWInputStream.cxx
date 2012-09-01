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

#include <libwpd-stream/libwpd-stream.h>
#include <libwpd/libwpd.h>

#include "MWAWDebug.hxx"
#include "MWAWOLEStream.hxx"
#include "MWAWZipStream.hxx"

#include "MWAWInputStream.hxx"

MWAWInputStream::MWAWInputStream(shared_ptr<WPXInputStream> inp, bool inverted)
  : m_stream(inp), m_inverseRead(inverted), m_readLimit(-1), m_prevLimits(),
    m_fInfoType(""), m_fInfoCreator(""), m_resourceFork(), m_storageOLE()
{
}

MWAWInputStream::MWAWInputStream(WPXInputStream *inp, bool inverted, bool checkCompression)
  : m_stream(), m_inverseRead(inverted), m_readLimit(-1), m_prevLimits(),
    m_fInfoType(""), m_fInfoCreator(""), m_resourceFork(), m_storageOLE()
{
  if (!inp) return;

  m_stream = shared_ptr<WPXInputStream>(inp, MWAW_shared_ptr_noop_deleter<WPXInputStream>());
  if (!checkCompression) return;

  // first check the zip format
  unzipStream();

  // check if the data are in binhex format
  unBinHex();

  // now check for MacMIME format in m_stream or in m_resourceFork
  unMacMIME();
  seek(0, WPX_SEEK_SET);
  if (m_resourceFork)
    m_resourceFork->seek(0, WPX_SEEK_SET);
}

MWAWInputStream::~MWAWInputStream()
{
}

const uint8_t *MWAWInputStream::read(size_t numBytes, unsigned long &numBytesRead)
{
  return m_stream->read(numBytes,numBytesRead);
}

long MWAWInputStream::tell()
{
  return m_stream->tell();
}

int MWAWInputStream::seek(long offset, WPX_SEEK_TYPE seekType)
{
  if (seekType == WPX_SEEK_CUR)
    offset += tell();

  if (offset < 0)
    offset = 0;
  if (m_readLimit > 0 && offset > (long)m_readLimit)
    offset = (long)m_readLimit;

  return m_stream->seek(offset, WPX_SEEK_SET);
}

bool MWAWInputStream::atEOS()
{
  if (m_readLimit > 0 && m_stream->tell() >= m_readLimit) return true;

  return m_stream->atEOS();
}

unsigned long MWAWInputStream::readULong(WPXInputStream *stream, int num, unsigned long a, bool inverseRead)
{
  if (!stream || num == 0 || stream->atEOS()) return a;
  if (inverseRead) {
    unsigned long val = readU8(stream);
    return val + (readULong(stream, num-1,0, inverseRead) << 8);
  }

  return readULong(stream, num-1,(a<<8) + (unsigned long)readU8(stream), inverseRead);
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

uint8_t MWAWInputStream::readU8(WPXInputStream *stream)
{
  if (!stream)
    return 0;
  unsigned long numBytesRead;
  uint8_t const *p = stream->read(sizeof(uint8_t), numBytesRead);

  if (!p || numBytesRead != sizeof(uint8_t))
    return 0;

  return *(uint8_t const *)(p);
}

////////////////////////////////////////////////////////////
//
// BinHex 4.0
//
////////////////////////////////////////////////////////////
bool MWAWInputStream::unBinHex()
{
  seek(45, WPX_SEEK_SET);
  if (atEOS() || tell() != 45)
    return false;
  // check header
  seek(0, WPX_SEEK_SET);
  std::string str("");
  for (int i = 0; i < 45; i++) {
    char c = (char) readLong(1);
    if (c == 0)
      return false;
    str += c;
  }
  if (str != "(This file must be converted with BinHex 4.0)")
    return false;
  int numEOL = 0;
  while (!atEOS()) {
    char c = (char) readLong(1);
    if (c == '\n') {
      numEOL++;
      continue;
    }
    seek(-1, WPX_SEEK_CUR);
    break;
  }
  if (atEOS() || !numEOL || ((char)readLong(1))!= ':')
    return false;

  // first phase reconstruct the file
  static char const binChar[65] = "!\"#$%&'()*+,-012345689@ABCDEFGHIJKLMNPQRSTUVXYZ[`abcdefhijklmpqr";
  std::map<unsigned char,int>  binMap;
  for (int i = 0; i < 64; i++) binMap[(unsigned char)binChar[i]]=i;
  bool endData = false;
  int numActByte = 0, actVal = 0;
  WPXBinaryData content;
  bool findRepetitif = false;
  while (1) {
    if (atEOS()) {
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
    } else
      readVal = binMap.find(c)->second;
    int wVal = -1;
    if (numActByte==0)
      actVal |= (readVal<<2);
    else if (numActByte==2) {
      wVal = (actVal | readVal);
      actVal = 0;
    } else if (numActByte==4) {
      wVal = actVal | ((readVal>>2)&0xF);
      actVal = (readVal&0x3)<<6;
    } else if (numActByte==6) {
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
  if (content.size() < 27) {
    MWAW_DEBUG_MSG(("MWAWInputStream::unBinHex: the content file is too small\n"));
    return false;
  }
  WPXInputStream *contentInput=const_cast<WPXInputStream *>(content.getDataStream());
  int fileLength = (int)  readU8(contentInput);
  if (fileLength < 1 || fileLength > 64) {
    MWAW_DEBUG_MSG(("MWAWInputStream::unBinHex: the file name size seems odd\n"));
    return false;
  }
  contentInput->seek(fileLength+1, WPX_SEEK_CUR); // filename + version
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
  } else if (creator.length() || type.length()) {
    MWAW_DEBUG_MSG(("MWAWInputStream::unBinHex: the file name size seems odd\n"));
  }
  contentInput->seek(2, WPX_SEEK_CUR); // skip flags
  long dataLength = (long) readULong(contentInput,4,0,false);
  long rsrcLength = (long) readULong(contentInput,4,0,false);
  long pos = contentInput->tell()+2; // skip CRC
  contentInput->seek(pos+dataLength+rsrcLength+4, WPX_SEEK_SET);
  if (contentInput->tell() != pos+dataLength+rsrcLength+4
      || dataLength<=0 || rsrcLength < 0) {
    MWAW_DEBUG_MSG(("MWAWInputStream::unBinHex: the data/rsrc fork size seems odd\n"));
    return false;
  }
  // now read the rsrc and the data fork
  if (rsrcLength && getResourceForkStream()) {
    MWAW_DEBUG_MSG(("MWAWInputStream::unBinHex: I already have a resource fork!!!!\n"));
  } else if (rsrcLength) {
    contentInput->seek(pos+dataLength+2, WPX_SEEK_SET);
    unsigned long numBytesRead = 0;
    const unsigned char *data =
      contentInput->read((unsigned long)rsrcLength, numBytesRead);
    if (numBytesRead != (unsigned long)rsrcLength || !data) {
      MWAW_DEBUG_MSG(("MWAWInputStream::unBinHex: can not read the resource fork\n"));
    } else {
      shared_ptr<WPXInputStream> rsrc(new MWAWStringStream(data, numBytesRead));
      m_resourceFork.reset(new MWAWInputStream(rsrc,false));
    }
  }
  contentInput->seek(pos, WPX_SEEK_SET);
  unsigned long numBytesRead = 0;
  const unsigned char *data =
    contentInput->read((unsigned long)dataLength, numBytesRead);
  if (numBytesRead != (unsigned long)dataLength || !data) {
    MWAW_DEBUG_MSG(("MWAWInputStream::unBinHex: can not read the data fork\n"));
    return false;
  }
  m_stream.reset(new MWAWStringStream(data, numBytesRead));

  return false;
}

////////////////////////////////////////////////////////////
//
// Zip
//
////////////////////////////////////////////////////////////
bool MWAWInputStream::unzipStream()
{
  seek(0, WPX_SEEK_SET);
  MWAWZipStream zStream(m_stream.get());
  bool zipFile = zStream.isZipStream();
  if (!zipFile) return false;

  seek(0, WPX_SEEK_SET);
  std::vector<std::string> names = zStream.getZipNames();
  if (names.size() == 1) {
    m_stream.reset(zStream.getDocumentZipStream(names[0]));
    return true;
  }
  if (names.size() != 2) {
    MWAW_DEBUG_MSG(("MWAWInputStream::unzipStream:find a zip file with bad number of entries\n"));
    return false;
  }
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
  if (prefix != names[1]) {
    MWAW_DEBUG_MSG(("MWAWInputStream::unzipStream: find a zip file with unknown two entries %s %s\n", names[0].c_str(), names[1].c_str()));
    return false;
  }
  shared_ptr<WPXInputStream> rsrcPtr(zStream.getDocumentZipStream(names[1]));
  m_resourceFork.reset(new MWAWInputStream(rsrcPtr,false));
  m_stream.reset(zStream.getDocumentZipStream(names[0]));
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
    shared_ptr<WPXInputStream> newDataInput, newRsrcInput;
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
    shared_ptr<WPXInputStream> newDataInput, newRsrcInput;
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
        } else
          m_resourceFork.reset(new MWAWInputStream(newRsrcInput,false));
      }
    }
  }
  return true;
}

/* freely inspired from http://tools.ietf.org/html/rfc1740#appendix-A
 */
bool MWAWInputStream::unMacMIME(MWAWInputStream *inp,
                                shared_ptr<WPXInputStream> &dataInput,
                                shared_ptr<WPXInputStream> &rsrcInput) const
{
  dataInput.reset();
  rsrcInput.reset();
  if (!inp) return false;

  try {
    inp->seek(0, WPX_SEEK_SET);
    long magicNumber = (long) inp->readULong(4);
    if (magicNumber != 0x00051600 && magicNumber != 0x00051607)
      return false;
    long version = (long) inp->readULong(4);
    if (version != 0x20000) {
      MWAW_DEBUG_MSG(("MWAWInputStream::unMacMIME: unknown version: %lx\n", version));
      return false;
    }
    inp->seek(16, WPX_SEEK_CUR); // filename
    long numEntries = (long) inp->readULong(2);
    if (inp->atEOS() || numEntries <= 0) {
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
      if (wh <= 0 || wh >= 16 || inp->atEOS()) {
        MWAW_DEBUG_MSG(("MWAWInputStream::unMacMIME: find unknown id: %d\n", wh));
        return false;
      }
      MWAW_DEBUG_MSG(("MWAWInputStream::unMacMIME: find %s entry\n", what[wh]));
      if (wh > 2 && wh != 9) {
        inp->seek(8, WPX_SEEK_CUR);
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
      inp->seek(entryPos, WPX_SEEK_SET);
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
        dataInput.reset(new MWAWStringStream(data, numBytesRead));
      else if (wh==2)
        rsrcInput.reset(new MWAWStringStream(data, numBytesRead));
      else { // the finder info
        if (entrySize < 8) {
          MWAW_DEBUG_MSG(("MWAWInputStream::unMacMIME: finder info size is odd\n"));
        } else {
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
          } else if (type.length()) {
            MWAW_DEBUG_MSG(("MWAWInputStream::unMacMIME: can not read find info\n"));
          }
        }
      }

      inp->seek(pos+12, WPX_SEEK_SET);
    }
  } catch (...) {
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////
//
// OLE part
//
////////////////////////////////////////////////////////////

bool MWAWInputStream::isOLEStream()
{
  if (!createStorageOLE()) return false;
  return m_storageOLE->isOLEStream();
}

std::vector<std::string> MWAWInputStream::getOLENames()
{
  if (!createStorageOLE()) return std::vector<std::string>();
  return m_storageOLE->getOLENames();
}

shared_ptr<MWAWInputStream> MWAWInputStream::getDocumentOLEStream(std::string name)
{
  static shared_ptr<MWAWInputStream> empty;
  if (!createStorageOLE()) return empty;

  long actPos = tell();
  seek(0, WPX_SEEK_SET);
  shared_ptr<WPXInputStream> res(m_storageOLE->getDocumentOLEStream(name));
  seek(actPos, WPX_SEEK_SET);

  if (!res)
    return empty;
  shared_ptr<MWAWInputStream> inp(new MWAWInputStream(res,m_inverseRead));
  return inp;
}

bool MWAWInputStream::createStorageOLE()
{
  if (m_storageOLE) return true;

  long actPos = tell();
  seek(0, WPX_SEEK_SET);
  m_storageOLE.reset(new libmwaw::Storage(m_stream.get()));
  seek(actPos, WPX_SEEK_SET);

  return m_storageOLE;
}

////////////////////////////////////////////////////////////
//
//  a function to read a data block
//
////////////////////////////////////////////////////////////

bool MWAWInputStream::readDataBlock(long size, WPXBinaryData &data)
{
  data.clear();
  if (size < 0) return false;
  if (size == 0) return true;
  if (m_readLimit > 0 && long(tell()+size) > (long)m_readLimit) return false;

  const unsigned char *readData;
  unsigned long sizeRead;
  while (size > 2048 && (readData=m_stream->read(2048, sizeRead)) != 0 && sizeRead) {
    data.append(readData, sizeRead);
    size -= sizeRead;
  }
  if (size > 2048) return false;

  readData=m_stream->read((unsigned long)size, sizeRead);
  if (size != long(sizeRead)) return false;
  data.append(readData, sizeRead);

  return true;
}

bool MWAWInputStream::readEndDataBlock(WPXBinaryData &data)
{
  if (m_readLimit>0) return readDataBlock(1+m_readLimit-tell(), data);
  data.clear();

  const unsigned char *readData;
  unsigned long sizeRead;
  while ((readData=m_stream->read(2048, sizeRead)) != 0 && sizeRead)
    data.append(readData, sizeRead);

  return atEOS();
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
MWAWStringStream::MWAWStringStream(const unsigned char *data, const unsigned long dataSize) :
  m_buffer(dataSize), m_offset(0)
{
  memcpy(&m_buffer[0], data, dataSize);
}

int MWAWStringStream::seek(long _offset, WPX_SEEK_TYPE seekType)
{
  if (seekType == WPX_SEEK_CUR)
    m_offset += _offset;
  else if (seekType == WPX_SEEK_SET)
    m_offset = _offset;

  if (m_offset < 0) {
    m_offset = 0;
    return 1;
  }
  if (m_offset > (long)m_buffer.size()) {
    m_offset = (long) m_buffer.size();
    return 1;
  }
  return 0;
}

const unsigned char *MWAWStringStream::read(unsigned long numBytes, unsigned long &numBytesRead)
{
  numBytesRead = 0;

  if (numBytes == 0)
    return 0;

  unsigned long numBytesToRead;

  if (((unsigned long)m_offset+numBytes) < m_buffer.size())
    numBytesToRead = numBytes;
  else
    numBytesToRead = (unsigned long) ((long)m_buffer.size() - m_offset);

  numBytesRead = numBytesToRead; // about as paranoid as we can be..

  if (numBytesToRead == 0)
    return 0;

  long oldOffset = m_offset;
  m_offset += numBytesToRead;

  return &m_buffer[size_t(oldOffset)];
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
