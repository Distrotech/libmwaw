/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libmwaw
 * Copyright (C) 2009, 2011 Alonso Laurent (alonso@loria.fr)
 * Copyright (C) 2006, 2007 Andrew Ziem
 * Copyright (C) 2004-2006 Fridrich Strba (fridrich.strba@bluewin.ch)
 * Copyright (C) 2004 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2003-2005 William Lachance (william.lachance@sympatico.ca)
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
 * For further information visit http://libmwaw.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

#ifndef MWAW_INPUT_STREAM_H
#define MWAW_INPUT_STREAM_H

#include <stdint.h>
#include <fstream>
#include <sstream>
#include <vector>

#include "libmwaw_tools.hxx"

#include <libwpd-stream/WPXStream.h>

#include "DMWAWOLEStream.hxx"
#include "libmwaw_tools.hxx"

class WPXBinaryData;

/*! \class TMWAWInputStream
 * \brief Internal class used to read the file stream
 *  Internal class used to read the file stream,
 *    this class adds some usefull functions to the basic WPXInputStream:
 *  - read number (int8, int16, int32) in low or end endian
 *  - selection of a section of a stream
 *  - read block of data
 *  - interface with modified WPXOLEStream
 */
class TMWAWInputStream
{
public:
  /*!\brief creates a stream with given endian from \param input
   * \param inverted must be set to true for pc doc and ole part
   * \param inverted must be set to false for mac doc
   */
  TMWAWInputStream(WPXInputStream *input, bool inverted)
    : m_stream(input), m_resp(false), m_inverseRead(inverted), m_readLimit(-1),
      m_prevLimits(), m_storageOLE(0) {}

  //! destructor
  ~TMWAWInputStream() {
    if (m_stream && m_resp) delete m_stream;
    if (m_storageOLE) delete m_storageOLE;
  }
  //! sets this input responsable/or not of the deletion of the actual WPXInputStream
  void setResponsable(bool newResp) {
    m_resp = newResp;
  }

  //! returns the basic WPXInputStream
  WPXInputStream *input() {
    return m_stream;
  }
  //! returns the endian mode (see constructor)
  bool readInverted() const {
    return m_inverseRead;
  }
  //! sets the endian mode
  void setReadInverted(bool newVal) {
    m_inverseRead = newVal;
  }

  //
  // Position: access
  //

  /*! \brief seeks to a offset position, from actual or beginning position
   * \return 0 if ok
   * \sa pushLimit popLimit
   */
  int seek(long offset, WPX_SEEK_TYPE seekType);
  //! returns actual offset position
  long tell();
  //! returns true if we are at the end of the section/file
  bool atEOS();

  /*! \brief defines a new section in the file (from actualPos to newLimit)
   * next call of seek, tell, atEos, ... will be restrained to this section
   */
  void pushLimit(long newLimit) {
    m_prevLimits.push_back(m_readLimit);
    m_readLimit = newLimit;
  }
  //! pops a section defined by pushLimit
  void popLimit() {
    if (m_prevLimits.size()) {
      m_readLimit = m_prevLimits.back();
      m_prevLimits.pop_back();
    } else m_readLimit = -1;
  }

  //
  // get data
  //

  //! returns a uint8, uint16, uint32 readed from actualPos
  unsigned long readULong(int num) {
    return readULong(num, 0);
  }
  //! return a int8, int16, int32 readed from actualPos
  long readLong(int num);

  /**! reads numbytes data, WITHOUT using any endian or section consideration
   * \return a pointer to the read elements
   */
  const uint8_t *read(size_t numBytes, unsigned long &numBytesRead);

  //! reads a WPXBinaryData with a given size in the actual section/file
  bool readDataBlock(long size, WPXBinaryData &data);
  //! reads a WPXBinaryData from actPos to the end of the section/file
  bool readEndDataBlock(WPXBinaryData &data);

  //
  // OLE access
  //

  //! return true if the stream is ole
  bool isOLEStream();
  //! return the list of all ole zone
  std::vector<std::string> allOLEEntries();
  //! return a new stream for a ole zone
  shared_ptr<TMWAWInputStream> getDocumentOLEStream(const char * name);


protected:
  //! internal function used to read a byte
  uint8_t readU8();
  /*! \brief internal function used to read num byte,
   *  - where a is the previous read data
   */
  unsigned long readULong(int num, unsigned long a);

  //! creates a storage ole
  bool createStorageOLE();

private:
  TMWAWInputStream(TMWAWInputStream const &orig);
  TMWAWInputStream &operator=(TMWAWInputStream const &orig);

protected:
  //! the initial input
  WPXInputStream *m_stream;
  //! the flag to know if we must release the input
  bool m_resp;

  //! big or normal endian
  bool m_inverseRead;

  //! actual section limit (-1 if no limit)
  long m_readLimit;
  //! list of previous limits
  std::vector<long> m_prevLimits;

  //! the ole storage
  libmwaw_libwpd::Storage *m_storageOLE;
};

//! a smart point of TMWAWInputStream
typedef shared_ptr<TMWAWInputStream> TMWAWInputStreamPtr;
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
