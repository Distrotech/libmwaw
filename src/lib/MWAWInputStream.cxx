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

#include <libwpd-stream/libwpd-stream.h>
#include <libwpd/libwpd.h>

#include "MWAWInputStream.hxx"

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

unsigned long MWAWInputStream::readULong(int num, unsigned long a)
{
  if (num == 0 || atEOS()) return a;
  if (m_inverseRead) {
    unsigned long val = readU8();
    return val + (readULong(num-1,0) << 8);
  }

  return readULong(num-1,(a<<8) + (unsigned long)readU8());
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

uint8_t MWAWInputStream::readU8()
{
  unsigned long numBytesRead;
  uint8_t const *p = m_stream->read(sizeof(uint8_t), numBytesRead);

  if (!p || numBytesRead != sizeof(uint8_t))
    return 0;

  return *(uint8_t const *)(p);
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
  WPXInputStream *res=m_storageOLE->getDocumentOLEStream(name);
  seek(actPos, WPX_SEEK_SET);

  if (!res)
    return empty;
  shared_ptr<MWAWInputStream> inp(new MWAWInputStream(res,m_inverseRead));
  inp->setResponsable(true);
  return inp;
}

bool MWAWInputStream::createStorageOLE()
{
  if (m_storageOLE) return true;

  long actPos = tell();
  seek(0, WPX_SEEK_SET);
  m_storageOLE = new libmwaw::Storage(m_stream);
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
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
