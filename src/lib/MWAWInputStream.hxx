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

#ifndef MWAW_INPUT_STREAM_H
#define MWAW_INPUT_STREAM_H

#include <stdint.h>
#include <string>
#include <vector>

#include <libwpd-stream/WPXStream.h>
#include "libmwaw_internal.hxx"

namespace libmwaw
{
class Storage;
}

class WPXBinaryData;

/*! \class MWAWInputStream
 * \brief Internal class used to read the file stream
 *  Internal class used to read the file stream,
 *    this class adds some usefull functions to the basic WPXInputStream:
 *  - read number (int8, int16, int32) in low or end endian
 *  - selection of a section of a stream
 *  - read block of data
 *  - interface with modified WPXOLEStream
 */
class MWAWInputStream
{
public:
  /*!\brief creates a stream with given endian from \param inp
   * \param inverted must be set to true for pc doc and ole part
   * \param inverted must be set to false for mac doc
   */
  MWAWInputStream(shared_ptr<WPXInputStream> inp, bool inverted);

  /*!\brief creates a stream with given endian from an existing input
   *
   * Note: this functions does not delete input
   */
  MWAWInputStream(WPXInputStream *input, bool inverted, bool checkCompression=false);
  //! destructor
  ~MWAWInputStream();

  //! returns the basic WPXInputStream
  shared_ptr<WPXInputStream> input() {
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
    return readULong(m_stream.get(), num, 0, m_inverseRead);
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
  std::vector<std::string> getOLENames();
  //! return a new stream for a ole zone
  shared_ptr<MWAWInputStream> getDocumentOLEStream(std::string name);

  //
  // Finder Info access
  //
  /** returns the finder info type and creator (if known) */
  bool getFinderInfo(std::string &type, std::string &creator) const {
    if (!m_fInfoType.length() || !m_fInfoCreator.length()) {
      type = creator = "";
      return false;
    }
    type = m_fInfoType;
    creator = m_fInfoCreator;
    return true;
  }

  //
  // Resource Fork access
  //

  /** returns the resource fork if find */
  shared_ptr<MWAWInputStream> getResourceForkStream() {
    return m_resourceFork;
  }


protected:
  //! internal function used to read a byte
  static uint8_t readU8(WPXInputStream *stream);
  /*! \brief internal function used to read num byte,
   *  - where a is the previous read data
   */
  static unsigned long readULong(WPXInputStream *stream, int num, unsigned long a, bool inverseRead);

  //! creates a storage ole
  bool createStorageOLE();

  //! unbinhex the data in the file is a BinHex 4.0 file of a mac file
  bool unBinHex();
  //! unzip the data in the file is a zip file of a mac file
  bool unzipStream();
  //! check if some stream are in MacMIME format, if so de MacMIME
  bool unMacMIME();
  //! de MacMIME an input stream
  bool unMacMIME(MWAWInputStream *input,
                 shared_ptr<WPXInputStream> &dataInput,
                 shared_ptr<WPXInputStream> &rsrcInput) const;

private:
  MWAWInputStream(MWAWInputStream const &orig);
  MWAWInputStream &operator=(MWAWInputStream const &orig);

protected:
  //! the initial input
  shared_ptr<WPXInputStream> m_stream;
  //! big or normal endian
  bool m_inverseRead;

  //! actual section limit (-1 if no limit)
  long m_readLimit;
  //! list of previous limits
  std::vector<long> m_prevLimits;

  //! finder info type
  mutable std::string m_fInfoType;
  //! finder info type
  mutable std::string m_fInfoCreator;
  //! the resource fork
  shared_ptr<MWAWInputStream> m_resourceFork;
  //! the ole storage
  shared_ptr<libmwaw::Storage> m_storageOLE;
};

//! a smart point of MWAWInputStream
typedef shared_ptr<MWAWInputStream> MWAWInputStreamPtr;

/** an internal class used to return the OLE/Zip InputStream */
class MWAWStringStream: public WPXInputStream
{
public:
  MWAWStringStream(const unsigned char *data, const unsigned long dataSize);
  ~MWAWStringStream() { }

  const unsigned char *read(unsigned long numBytes, unsigned long &numBytesRead);
  long tell() {
    return m_offset;
  }
  int seek(long offset, WPX_SEEK_TYPE seekType);
  bool atEOS() {
    return ((long)m_offset >= (long)m_buffer.size());
  }

  bool isOLEStream() {
    return false;
  }
  WPXInputStream *getDocumentOLEStream(const char *) {
    return 0;
  };

private:
  std::vector<unsigned char> m_buffer;
  volatile long m_offset;
  MWAWStringStream(const MWAWStringStream &);
  MWAWStringStream &operator=(const MWAWStringStream &);
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
