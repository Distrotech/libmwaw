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

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <iostream>

#include <librevenge/librevenge.h>
#include <librevenge-generators/librevenge-generators.h>
#include <librevenge-stream/librevenge-stream.h>

#include <libmwaw/libmwaw.hxx>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef VERSION
#define VERSION "UNKNOWN VERSION"
#endif

int printUsage()
{
  printf("Usage: mwaw2svg [OPTION] <Mac Graphic Document>\n");
  printf("\n");
  printf("Options:\n");
  printf(" -h                Shows this help message\n");
  printf(" -o file.svg       Define the output[default stdout]\n");
  printf(" -v:               Output mwaw2svg version \n");
  printf("\n");
  return -1;
}

int printVersion()
{
  printf("mwaw2svg %s\n", VERSION);
  return 0;
}

int main(int argc, char *argv[])
{
  if (argc < 2)
    return printUsage();

  char const *output = 0;
  bool printHelp=false;
  int ch;

  while ((ch = getopt(argc, argv, "ho:v")) != -1) {
    switch (ch) {
    case 'o':
      output=optarg;
      break;
    case 'v':
      printVersion();
      return 0;
    default:
    case 'h':
      printHelp = true;
      break;
    }
  }

  if (argc != 1+optind || printHelp) {
    printUsage();
    return -1;
  }
  librevenge::RVNGFileStream input(argv[optind]);

  MWAWDocument::Type type;
  MWAWDocument::Kind kind;
  MWAWDocument::Confidence confidence = MWAWDocument::isFileFormatSupported(&input, type, kind);
  if (confidence != MWAWDocument::MWAW_C_EXCELLENT) {
    printf("ERROR: Unsupported file format!\n");
    return 1;
  }
  if (type == MWAWDocument::MWAW_T_UNKNOWN) {
    printf("ERROR: can not determine the type of file!\n");
    return 1;
  }
  if (kind != MWAWDocument::MWAW_K_DRAW && kind != MWAWDocument::MWAW_K_PAINT) {
    fprintf(stderr,"ERROR: not a graphic document!\n");
    return 1;
  }
  MWAWDocument::Result error=MWAWDocument::MWAW_R_OK;
  librevenge::RVNGStringVector vec;

  try {
    librevenge::RVNGSVGDrawingGenerator listener(vec, "");
    error = MWAWDocument::parse(&input, &listener);
    if (error==MWAWDocument::MWAW_R_OK && (vec.empty() || vec[0].empty()))
      error = MWAWDocument::MWAW_R_UNKNOWN_ERROR;
  }
  catch (MWAWDocument::Result const &err) {
    error=err;
  }
  catch (...) {
    error=MWAWDocument::MWAW_R_UNKNOWN_ERROR;
  }
  if (error == MWAWDocument::MWAW_R_FILE_ACCESS_ERROR)
    fprintf(stderr, "ERROR: File Exception!\n");
  else if (error == MWAWDocument::MWAW_R_PARSE_ERROR)
    fprintf(stderr, "ERROR: Parse Exception!\n");
  else if (error == MWAWDocument::MWAW_R_OLE_ERROR)
    fprintf(stderr, "ERROR: File is an OLE document!\n");
  else if (error != MWAWDocument::MWAW_R_OK)
    fprintf(stderr, "ERROR: Unknown Error!\n");

  if (error != MWAWDocument::MWAW_R_OK)
    return 1;

  if (!output) {
    std::cout << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n";
    std::cout << "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\"";
    std::cout << " \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n";
    std::cout << vec[0].cstr() << std::endl;
  }
  else {
    std::ofstream out(output);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n";
    out << "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\"";
    out << " \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n";
    out << vec[0].cstr() << std::endl;
  }
  return 0;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab: