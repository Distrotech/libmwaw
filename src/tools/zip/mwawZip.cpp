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
#ifndef _WIN32
#define __cdecl
#endif

#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <libmwaw_internal.hxx>
#include "input.h"
#include "xattr.h"
#include "zip.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef VERSION
#define VERSION "UNKNOWN VERSION"
#endif

static void usage(const char *cmdname)
{
  std::cerr << "Usage: " << cmdname << " [-h][-x][-D] FILENAME ZIPFILE\n";
  std::cerr << "\n";
  std::cerr << "try to zip the content of FILENAME in ZIPFILE.\n";
  std::cerr << "\n";
  std::cerr << "Options:\n";
  std::cerr << "\t -h: print this help,\n";
  std::cerr << "\t -x: do not zip a binhex or a zip file,\n";
  std::cerr << "\t -v: Output mwawZip version\n";
  std::cerr << "\t -D: only zip the file containing a resource fork or finder information.\n";
}

int printVersion()
{
  std::cerr << "mwawZip " << VERSION << "\n";
  return 0;
}

int __cdecl main(int argc, char **argv)
{
  bool checkZip=false;
  bool verbose=false;
  bool doNotCompressSimpleFile=false;
  int ch;
  while ((ch = getopt(argc, argv, "hvxD")) != -1) {
    switch (ch) {
    case 'v':
      printVersion();
      return 0;
    case 'x':
      checkZip=true;
      break;
    case 'D':
      doNotCompressSimpleFile=true;
      break;
    default:
      verbose=true;
      break;
    }
  }
  if (argc != 2+optind || verbose) {
    usage(argv[0]);
    return 1;
  }

  // check if it is a regular file
  struct stat status;
  if (stat(argv[optind], &status) || !S_ISREG(status.st_mode)) {
    std::cerr << argv[0] << ": the file " << argv[optind] << " is a not a regular file\n";
    return 1;
  }
  if (checkZip) {
    // we need to open the file to look for the file signature
    try {
      std::ifstream file(argv[optind], std::ios::binary);
      if (file.bad()) {
        std::cerr << argv[0] << ": the file " << argv[optind] << " seems bad\n";
        return 1;
      }
      file.seekg(0, std::ios::beg);
      char buff[4] = {'\0', '\0','\0','\0'};
      file.read(buff,4);
      // look for a zip file signature
      if (buff[0]=='P' && buff[1]=='K') {
        if (((buff[2]==(char)3||buff[2]==(char)5||buff[2]==(char)7) && buff[3]==buff[2]+(char)1) ||
            (buff[2]=='L'&&buff[3]=='I') || (buff[2]=='S'&&buff[3]=='p'))
          return 2;
      }
      // look for a binhex file signature
      if (buff[0]=='('&&buff[1]=='T'&&buff[2]=='h'&&buff[3]=='i') {
        file.read(buff,4);
        if (buff[0]=='s'&&buff[1]==' '&&buff[2]=='f'&&buff[3]=='i')
          return 2;
      }
    }
    catch (...) {
    }
  }
  std::string resultFile(argv[optind+1]);
  // check if the file exists
  if (stat(resultFile.c_str(), &status)==0) {
    std::cerr  << argv[0] << ": the file " << resultFile << " already exists\n";
    return 1;
  }

  std::string originalFile(argv[optind]);
#ifdef WIN32
  for (size_t i = 0; i < originalFile.size(); i++) {
    if (originalFile[i]=='\\')
      originalFile[i]='/';
  }
#endif
  /** find folder and base file name*/
  size_t sPos=originalFile.rfind('/');
  std::string folder(""), file("");
  if (sPos==std::string::npos)
    file = originalFile;
  else {
    folder=originalFile.substr(0,sPos+1);
    file=originalFile.substr(sPos+1);
  }

  libmwaw_zip::Zip zip;
  shared_ptr<libmwaw_zip::FileStream> fStream, fAuxiStream;
  shared_ptr<libmwaw_zip::InputStream> auxiStream;
  try {
    //! the main data fork
    fStream.reset(new libmwaw_zip::FileStream(originalFile.c_str()));
    if (!fStream->ok()) {
      fprintf(stderr, "Failed to create stream for %s\n", originalFile.c_str());
      return 1;
    }
    //! the attributes
    libmwaw_zip::XAttr xattr(originalFile.c_str());
    // check first the classic attributes (which are no longer reconstructed)
    auxiStream = xattr.getClassicStream();
    if (!auxiStream)
      auxiStream = xattr.getStream();
    if (!auxiStream) {
      // look for a resource file
      std::string name=folder+"._"+file;
      if (stat(name.c_str(), &status) || !S_ISREG(status.st_mode)) {
        name=folder+"__MACOSX/._"+file;
        if (stat(name.c_str(), &status) || !S_ISREG(status.st_mode))
          name = "";
      }
      if (name.length()) {
        fAuxiStream.reset(new libmwaw_zip::FileStream(name.c_str()));
        if (fAuxiStream->ok())
          auxiStream = fAuxiStream;
      }
    }
    if (!auxiStream && doNotCompressSimpleFile)
      return 2;
    if (!zip.open(argv[optind+1]))
      return 1;
    zip.add(fStream, file.c_str());
    if (auxiStream) {
      std::string name="._"+file;
      zip.add(auxiStream, name.c_str());
    }
    zip.close();
  }
  catch (...) {
    std::cerr << argv[0] << ": error when zipping file " << argv[optind] << "\n";
    return -1;
  }

  return 0;
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
