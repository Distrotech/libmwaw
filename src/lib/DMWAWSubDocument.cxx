/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libwpd
 * Copyright (C) 2005 Fridrich Strba (fridrich.strba@bluewin.ch)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 * For further information visit http://libwpd.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

#include "DMWAWSubDocument.hxx"
#include "libmwaw_libwpd.hxx"
#include <string.h>

DMWAWSubDocument::DMWAWSubDocument() :
  m_stream(0),
  m_streamData(0)
{
}

DMWAWSubDocument::DMWAWSubDocument(WPXInputStream *input, DMWAWEncryption *encryption, const unsigned dataSize) :
  m_stream(0),
  m_streamData(new uint8_t[dataSize])
{
  unsigned i=0;
  for (; i<dataSize; i++) {
    if (input->atEOS())
      break;
    m_streamData[i] = libmwaw_libwpd::readU8(input, encryption);
  }
  m_stream = new DMWAWMemoryInputStream(m_streamData, i);
}

DMWAWSubDocument::DMWAWSubDocument(uint8_t *streamData, const unsigned dataSize) :
  m_stream(0),
  m_streamData(0)
{
  if (streamData)
    m_stream = new DMWAWMemoryInputStream(streamData, dataSize);
}

DMWAWSubDocument::~DMWAWSubDocument()
{
  if (m_stream)
    delete m_stream;
  if (m_streamData)
    delete [] m_streamData;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
