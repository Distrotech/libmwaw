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

#include <string>
#include <vector>

#include <librevenge/librevenge.h>
#include <librevenge-stream/librevenge-stream.h>
#include "libmwaw_internal.hxx"

namespace libmwawOLE
{
class Storage;
}

class RVNGBinaryData;

/*! \class MWAWInputStream
 * \brief Internal class used to read the file stream
 *  Internal class used to read the file stream,
 *    this class adds some usefull functions to the basic RVNGInputStream:
 *  - read number (int8, int16, int32) in low or end endian
 *  - selection of a section of a stream
 *  - read block of data
 *  - interface with modified RVNGOLEStream
 */
class MWAWInputStream
{
public:
  /*!\brief creates a stream with given endian from \param inp
   * \param inverted must be set to true for pc doc and ole part
   * \param inverted must be set to false for mac doc
   */
  MWAWInputStream(shared_ptr<RVNGInputStream> inp, bool inverted);

  /*!\brief creates a stream with given endian from an existing input
   *
   * Note: this functions does not delete input
   */
  MWAWInputStream(RVNGInputStream *input, bool inverted, bool checkCompression=false);
  //! destructor
  ~MWAWInputStream();

  //! returns the basic RVNGInputStream
  shared_ptr<RVNGInputStream> input() {
    return m_stream;
  }
  //! returns a new input stream corresponding to a RVNGBinaryData
  static shared_ptr<MWAWInputStream> get(RVNGBinaryData const &data, bool inverted);

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
  int seek(long offset, RVNG_SEEK_TYPE seekType);
  //! returns actual offset position
  long tell();
  //! returns the stream size
  long size() const {
    return m_streamSize;
  }
  //! checks if a position is or not a valid file position
  bool checkPosition(long pos) const {
    if (pos < 0) return false;
    if (m_readLimit > 0 && pos > m_readLimit) return false;
    return pos<=m_streamSize;
  }
  //! returns true if we are at the end of the section/file
  bool atEOS();

  /*! \brief defines a new section in the file (from actualPos to newLimit)
   * next call of seek, tell, atEos, ... will be restrained to this section
   */
  void pushLimit(long newLimit) {
    m_prevLimits.push_back(m_readLimit);
    m_readLimit = newLimit > m_streamSize ? m_streamSize : newLimit;
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
  //! try to read a double (ppc)
  bool readDouble(double &res);

  /**! reads numbytes data, WITHOUT using any endian or section consideration
   * \return a pointer to the read elements
   */
  const uint8_t *read(size_t numBytes, unsigned long &numBytesRead);
  /*! \brief internal function used to read num byte,
   *  - where a is the previous read data
   */
  static unsigned long readULong(RVNGInputStream *stream, int num, unsigned long a, bool inverseRead);

  //! reads a RVNGBinaryData with a given size in the actual section/file
  bool readDataBlock(long size, RVNGBinaryData &data);
  //! reads a RVNGBinaryData from actPos to the end of the section/file
  bool readEndDataBlock(RVNGBinaryData &data);

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

  /** returns true if the data fork block exists */
  bool hasDataFork() const {
    return bool(m_stream);
  }
  /** returns true if the resource fork block exists */
  bool hasResourceFork() const {
    return bool(m_resourceFork);
  }
  /** returns the resource fork if find */
  shared_ptr<MWAWInputStream> getResourceForkStream() {
    return m_resourceFork;
  }


protected:
  //! update the stream size ( must be called in the constructor )
  void updateStreamSize();
  //! internal function used to read a byte
  static uint8_t readU8(RVNGInputStream *stream);

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
                 shared_ptr<RVNGInputStream> &dataInput,
                 shared_ptr<RVNGInputStream> &rsrcInput) const;

private:
  MWAWInputStream(MWAWInputStream const &orig);
  MWAWInputStream &operator=(MWAWInputStream const &orig);

protected:
  //! the initial input
  shared_ptr<RVNGInputStream> m_stream;
  //! the stream size
  long m_streamSize;

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
  shared_ptr<libmwawOLE::Storage> m_storageOLE;
};

/** an internal class used to return the OLE/Zip InputStream */
class MWAWStringStream: public RVNGInputStream
{
public:
  //! constructor
  MWAWStringStream(const unsigned char *data, const unsigned long dataSize);
  //! destructor
  ~MWAWStringStream() { }

  /**! reads numbytes data, WITHOUT using any endian or section consideration
   * \return a pointer to the read elements
   */
  const unsigned char *read(unsigned long numBytes, unsigned long &numBytesRead);
  //! returns actual offset position
  long tell() {
    return m_offset;
  }
  /*! \brief seeks to a offset position, from actual or beginning position
   * \return 0 if ok
   * \sa pushLimit popLimit
   */
  int seek(long offset, RVNG_SEEK_TYPE seekType);
  //! returns true if we are at the end of the section/file
  bool atEOS() {
    return ((long)m_offset >= (long)m_buffer.size());
  }

  /**
     Analyses the content of the input stream to see whether it is an Zip/OLE2 storage.
     \return return false
  */
  bool isStructuredDocument() {
    return false;
  }
  /**
     Tries to extract a stream from a structured document.
     \note not implemented
  */
  RVNGInputStream *getSubStream(const char *) {
    return 0;
  }

  /**
     Analyses the content of the input stream to see whether it is an Zip/OLE2 storage.
     \return return false
  */
  bool isOLEStream() {
    return isStructuredDocument();
  }
  /**
     Tries to extract a stream from a structured document.
     \note not implemented
  */
  RVNGInputStream *getDocumentOLEStream(const char *name) {
    return getSubStream(name);
  }

private:
  /** a buffer which contains the data */
  std::vector<unsigned char> m_buffer;
  /** the actual offset in the buffer */
  volatile long m_offset;

  MWAWStringStream(const MWAWStringStream &);
  MWAWStringStream &operator=(const MWAWStringStream &);
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
