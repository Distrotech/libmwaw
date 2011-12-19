/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libwpd
 * Copyright (C) 2004-2005 William Lachance (wrlach@gmail.com)
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

#ifndef MEMORYSTREAM_H
#define MEMORYSTREAM_H

#include <libwpd-stream/WPXStream.h>

class DMWAWMemoryInputStream : public WPXInputStream
{
public:
  DMWAWMemoryInputStream(unsigned char *data, unsigned long size);
  virtual ~DMWAWMemoryInputStream();

  virtual bool isOLEStream() {
    return false;
  }
  virtual WPXInputStream *getDocumentOLEStream(const char *) {
    return 0;
  }

  virtual const unsigned char *read(unsigned long numBytes, unsigned long &numBytesRead);
  virtual int seek(long offset, WPX_SEEK_TYPE seekType);
  virtual long tell();
  virtual bool atEOS();
  virtual unsigned long getSize() const {
    return m_size;
  };

private:
  long m_offset;
  unsigned long m_size;
  unsigned char *m_data;
  DMWAWMemoryInputStream(const DMWAWMemoryInputStream &);
  DMWAWMemoryInputStream &operator=(const DMWAWMemoryInputStream &);
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
