/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* POLE - Portable C++ library to access OLE Storage
   Copyright (C) 2002-2005 Ariya Hidayat <ariya@kde.org>

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   * Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   * Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   * Neither the name of the authors nor the names of its contributors may be
     used to endorse or promote products derived from this software without
     specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
   THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef MWAWOLESTREAM_HXX
#define MWAWOLESTREAM_HXX

#include <string>
#include <fstream>
#include <vector>

#include "libmwaw_internal.hxx"

class WPXInputStream;

namespace libmwaw
{

class StorageIO;
class Stream;
class StreamIO;

/** class used to read/parse an OLE file */
class Storage
{
  friend class Stream;

public:

  //! the Storage::result enum
  enum { Ok, OpenFailed, NotOLE, BadOLE, UnknownError };

  /**
   * Constructs a storage with data.
   **/
  Storage( WPXInputStream *is );

  /**
   * Destroys the storage.
   **/
  ~Storage();

  /**
   * Checks whether the storage is OLE2 storage.
   **/
  bool isOLEStream();

  /**
   * Returns the error code of last operation.
   **/
  int result();

  /**
   * Returns the list of all ole leaves names
   **/
  std::vector<std::string> getOLENames();

  /**
   * Returns a WPXInputStream corresponding to a name
   **/
  WPXInputStream *getDocumentOLEStream(const std::string &name);

private:
  /** the internal IO */
  StorageIO *io;

  // no copy or assign
  Storage( const Storage & );
  Storage &operator=( const Storage & );

};

/** a small stream used to retrieve a part in a OLE file */
class Stream
{
  friend class Storage;
  friend class StorageIO;

public:

  /**
   * Creates a new stream.
   */
  Stream( Storage *storage, const std::string &name );

  /**
   * Destroys the stream.
   */
  ~Stream();

  /**
   * Returns the stream size.
   **/
  unsigned long size();

  /**
   * Reads a block of data.
   **/
  unsigned long read( unsigned char *data, unsigned long maxlen );

private:
  /** the internal IO */
  StreamIO *io;

  // no copy or assign
  Stream( const Stream & );
  Stream &operator=( const Stream & );
};

}  // namespace libmwaw

#endif // WPXOLESTREAM_H
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
