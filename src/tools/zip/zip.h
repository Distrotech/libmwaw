/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */

/* libmwaw: tools
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

#ifndef MWAW_ZIP_H
#  define MWAW_ZIP_H

#include <ostream>
#include <string>
#include <vector>

struct tm;
class InputStream;

namespace libmwaw_zip
{
struct DirTree;
struct FileEntry;

//! interface to zlib.h
class Zip
{
public:
  //! constructor
  Zip();
  //! destructor
  ~Zip();

  /** add a new stream  */
  void addStream(shared_ptr<InputStream> stream, char const *base, char const *path=0);

  /** write the zip file to a file name */
  bool write(char const * fileName);

protected:
  struct FileSorted;

  //! try to append a stream in zip file
  bool appendStream(std::ostream &zip, FileSorted const &file);
  //! try to append a stream definition in the central directory
  bool appendInCentralDir(std::ostream &zip, FileSorted const &file);

  //! try to compress a string
  bool compressDeflate(char *out, unsigned int *outlen, char const *in,
                       unsigned int inlen);
  //! Converts time from struct tm to the DOS format used by zip files.
  void timeToDos(struct tm *time, short *dosdate, short *dostime);

  //! the list of DirTree
  std::vector<DirTree> m_dirTrees;

  //! small structure used to sort file
  struct FileSorted {
    // constructor
    FileSorted(FileEntry *theFile, char const *thePath) : m_file(theFile), m_path(thePath) { }
    FileSorted(FileSorted const &orig) : m_file(orig.m_file), m_path(orig.m_path) { }
    FileSorted &operator=(const FileSorted &orig) {
      if (this == &orig) return *this;
      m_file=orig.m_file;
      m_path=orig.m_path;
      return *this;
    }

    //! sort the data
    static std::vector<FileSorted> sortFiles(std::vector<DirTree> &trees);
    //! a comparaison structure
    struct Cmp {
      //! compare two data
      bool operator()(FileSorted const &a, FileSorted const &b) const;
    };
    //! the file entry
    FileEntry *m_file;
    //! the path in zip file
    std::string m_path;
  };
};
}

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
