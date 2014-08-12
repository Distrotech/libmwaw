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
* Alternatively, the contents of this file may be used under the terms of
* the GNU Lesser General Public License Version 2 or later (the "LGPLv2+"),
* in which case the provisions of the LGPLv2+ are applicable
* instead of those above.
*/

#include <librevenge-stream/librevenge-stream.h>

class MWAWStringStreamPrivate;

/** internal class used to create a RVNGInputStream from a unsigned char's pointer

    \note this class (highly inspired from librevenge) does not
    implement the isStructured's protocol, ie. it only returns false.
 */
class MWAWStringStream: public librevenge::RVNGInputStream
{
public:
  //! constructor
  MWAWStringStream(const unsigned char *data, const unsigned int dataSize);
  //! destructor
  ~MWAWStringStream();

  /**! reads numbytes data.

   * \return a pointer to the read elements
   */
  const unsigned char *read(unsigned long numBytes, unsigned long &numBytesRead);
  //! returns actual offset position
  long tell();
  /*! \brief seeks to a offset position, from actual, beginning or ending position
   * \return 0 if ok
   */
  int seek(long offset, librevenge::RVNG_SEEK_TYPE seekType);
  //! returns true if we are at the end of the section/file
  bool isEnd();

  /** returns true if the stream is ole

   \sa returns always false*/
  bool isStructured();
  /** returns the number of sub streams.

   \sa returns always 0*/
  unsigned subStreamCount();
  /** returns the ith sub streams name

   \sa returns always 0*/
  const char *subStreamName(unsigned);
  /** returns true if a substream with name exists

   \sa returns always false*/
  bool existsSubStream(const char *name);
  /** return a new stream for a ole zone

   \sa returns always 0 */
  librevenge::RVNGInputStream *getSubStreamByName(const char *name);
  /** return a new stream for a ole zone

   \sa returns always 0 */
  librevenge::RVNGInputStream *getSubStreamById(unsigned);

private:
  MWAWStringStreamPrivate *d;
  MWAWStringStream(const MWAWStringStream &); // copy is not allowed
  MWAWStringStream &operator=(const MWAWStringStream &); // assignment is not allowed
};

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
