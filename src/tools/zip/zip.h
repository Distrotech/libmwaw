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

#include <zlib.h>

#include <fstream>
#include <map>
#include <string>
#include <vector>

class InputStream;

namespace libmwaw_zip
{
//! interface to zlib.h
class Zip
{
public:
  //! constructor
  Zip();
  //! destructor
  ~Zip();

  /** try open filename ( to write data ) */
  bool open(char const *filename);
  /** returns true if the output is opened */
  bool isOpened() const
  {
    return m_output;
  }
  /** try to close the actual output */
  bool close();
  /** try add to a new file  */
  bool add(shared_ptr<InputStream> stream, char const *base, char const *path=0);

protected:
  struct Directory;
  struct File;
  //! the output
  std::ofstream m_output;
  //! a map path->directory
  std::map<std::string, Directory> m_nameDirectoryMap;

  //! small structure used to store a file data
  struct File {
    //! constructor
    File(std::string const &base, std::string const &dir);

    //! try to write the input data in output
    bool write(shared_ptr<InputStream> input, std::ostream &output);
    //! try to write the central information in output
    bool writeCentralInformation(std::ostream &output) const;
  protected:
    //! the basename
    std::string m_base;
    //! the directory
    std::string m_dir;
    //! the uncompressed length
    uInt m_uncompressedLength;
    //! the compressed length
    uInt m_compressedLength;
    //! true if we called deflate on the data
    bool m_deflate;
    //! the crc
    uLong m_crc32;
    //! the offset in file of the local file header
    uLong m_offsetInFile;
  };
  //! small structure used to store a directory and its file
  struct Directory {
    //! constructor
    Directory(std::string const &dir);
    //! try to add a file checking if a file with the same name already exist
    bool add(shared_ptr<InputStream> input, char const *base, std::ostream &output);
    /** try to write the central information in output

    \note returns the number of central information written */
    int writeCentralInformation(std::ostream &output) const;
    //! the directory
    std::string m_dir;
    //! a map of filename -> file
    std::map<std::string, File> m_nameFileMap;
  };
};
}

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
