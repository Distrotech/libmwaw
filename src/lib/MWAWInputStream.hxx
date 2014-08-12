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

/*! \class MWAWInputStream
 * \brief Internal class used to read the file stream
 *  Internal class used to read the file stream,
 *    this class adds some usefull functions to the basic librevenge::RVNGInputStream:
 *  - read number (int8, int16, int32) in low or end endian
 *  - selection of a section of a stream
 *  - read block of data
 *  - interface with modified librevenge::RVNGOLEStream
 */
class MWAWInputStream
{
public:
  /*!\brief creates a stream with given endian from \param inp
   * \param inverted must be set to true for pc doc and ole part
   * \param inverted must be set to false for mac doc
   */
  MWAWInputStream(shared_ptr<librevenge::RVNGInputStream> inp, bool inverted);

  /*!\brief creates a stream with given endian from an existing input
   *
   * Note: this functions does not delete input
   */
  MWAWInputStream(librevenge::RVNGInputStream *input, bool inverted, bool checkCompression=false);
  //! destructor
  ~MWAWInputStream();

  //! returns the basic librevenge::RVNGInputStream
  shared_ptr<librevenge::RVNGInputStream> input()
  {
    return m_stream;
  }
  //! returns a new input stream corresponding to a librevenge::RVNGBinaryData
  static shared_ptr<MWAWInputStream> get(librevenge::RVNGBinaryData const &data, bool inverted);

  //! returns the endian mode (see constructor)
  bool readInverted() const
  {
    return m_inverseRead;
  }
  //! sets the endian mode
  void setReadInverted(bool newVal)
  {
    m_inverseRead = newVal;
  }
  //
  // Position: access
  //

  /*! \brief seeks to a offset position, from actual, beginning or ending position
   * \return 0 if ok
   * \sa pushLimit popLimit
   */
  int seek(long offset, librevenge::RVNG_SEEK_TYPE seekType);
  //! returns actual offset position
  long tell();
  //! returns the stream size
  long size() const
  {
    return m_streamSize;
  }
  //! checks if a position is or not a valid file position
  bool checkPosition(long pos) const
  {
    if (pos < 0) return false;
    if (m_readLimit > 0 && pos > m_readLimit) return false;
    return pos<=m_streamSize;
  }
  //! returns true if we are at the end of the section/file
  bool isEnd();

  /*! \brief defines a new section in the file (from actualPos to newLimit)
   * next call of seek, tell, atEos, ... will be restrained to this section
   */
  void pushLimit(long newLimit)
  {
    m_prevLimits.push_back(m_readLimit);
    m_readLimit = newLimit > m_streamSize ? m_streamSize : newLimit;
  }
  //! pops a section defined by pushLimit
  void popLimit()
  {
    if (m_prevLimits.size()) {
      m_readLimit = m_prevLimits.back();
      m_prevLimits.pop_back();
    }
    else m_readLimit = -1;
  }

  //
  // get data
  //

  //! returns a uint8, uint16, uint32 readed from actualPos
  unsigned long readULong(int num)
  {
    return readULong(m_stream.get(), num, 0, m_inverseRead);
  }
  //! return a int8, int16, int32 readed from actualPos
  long readLong(int num);
  //! try to read a double of size 8: 1.5 bytes exponent, 6.5 bytes mantisse
  bool readDouble8(double &res, bool &isNotANumber);
  //! try to read a double of size 8: 6.5 bytes mantisse, 1.5 bytes exponent
  bool readDoubleReverted8(double &res, bool &isNotANumber);
  //! try to read a double of size 10: 2 bytes exponent, 8 bytes mantisse
  bool readDouble10(double &res, bool &isNotANumber);

  /**! reads numbytes data, WITHOUT using any endian or section consideration
   * \return a pointer to the read elements
   */
  const uint8_t *read(size_t numBytes, unsigned long &numBytesRead);
  /*! \brief internal function used to read num byte,
   *  - where a is the previous read data
   */
  static unsigned long readULong(librevenge::RVNGInputStream *stream, int num, unsigned long a, bool inverseRead);

  //! reads a librevenge::RVNGBinaryData with a given size in the actual section/file
  bool readDataBlock(long size, librevenge::RVNGBinaryData &data);
  //! reads a librevenge::RVNGBinaryData from actPos to the end of the section/file
  bool readEndDataBlock(librevenge::RVNGBinaryData &data);

  //
  // OLE/Zip access
  //

  //! return true if the stream is ole
  bool isStructured();
  //! returns the number of substream
  unsigned subStreamCount();
  //! returns the name of the i^th substream
  std::string subStreamName(unsigned id);

  //! return a new stream for a ole zone
  shared_ptr<MWAWInputStream> getSubStreamByName(std::string const &name);
  //! return a new stream for a ole zone
  shared_ptr<MWAWInputStream> getSubStreamById(unsigned id);

  //
  // Finder Info access
  //
  /** returns the finder info type and creator (if known) */
  bool getFinderInfo(std::string &type, std::string &creator) const
  {
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
  bool hasDataFork() const
  {
    return bool(m_stream);
  }
  /** returns true if the resource fork block exists */
  bool hasResourceFork() const
  {
    return bool(m_resourceFork);
  }
  /** returns the resource fork if find */
  shared_ptr<MWAWInputStream> getResourceForkStream()
  {
    return m_resourceFork;
  }


protected:
  //! update the stream size ( must be called in the constructor )
  void updateStreamSize();
  //! internal function used to read a byte
  static uint8_t readU8(librevenge::RVNGInputStream *stream);

  //! unbinhex the data in the file is a BinHex 4.0 file of a mac file
  bool unBinHex();
  //! unzip the data in the file is a zip file of a mac file
  bool unzipStream();
  //! check if some stream are in MacMIME format, if so de MacMIME
  bool unMacMIME();
  //! de MacMIME an input stream
  bool unMacMIME(MWAWInputStream *input,
                 shared_ptr<librevenge::RVNGInputStream> &dataInput,
                 shared_ptr<librevenge::RVNGInputStream> &rsrcInput) const;

private:
  MWAWInputStream(MWAWInputStream const &orig);
  MWAWInputStream &operator=(MWAWInputStream const &orig);

protected:
  //! the initial input
  shared_ptr<librevenge::RVNGInputStream> m_stream;
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
};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
