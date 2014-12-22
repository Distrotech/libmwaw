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
  printf("Usage: mwaw2raw [OPTION] <Text Document>\n");
  printf("\n");
  printf("Options:\n");
  printf("\t--callgraph:   Display the call graph nesting level\n");
  printf("\t-h, --help:    Shows this help message\n");
  printf("\t-v, --version:       Output mwaw2raw version \n");
  return -1;
}

int printVersion()
{
  printf("mwaw2raw %s\n", VERSION);
  return 0;
}

int main(int argc, char *argv[])
{
  bool printIndentLevel = false;
  char *file = NULL;

  if (argc < 2)
    return printUsage();

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--callgraph"))
      printIndentLevel = true;
    else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--version"))
      return printVersion();
    else if (!file && strncmp(argv[i], "--", 2) && strcmp(argv[i], "-h"))
      file = argv[i];
    else
      return printUsage();
  }

  if (!file)
    return printUsage();


  librevenge::RVNGFileStream input(file);

  MWAWDocument::Type type;
  MWAWDocument::Kind kind;
  MWAWDocument::Confidence confidence = MWAWDocument::MWAW_C_NONE;
  try {
    confidence = MWAWDocument::isFileFormatSupported(&input, type, kind);
  }
  catch (...) {
    confidence = MWAWDocument::MWAW_C_NONE;
  }
  if (confidence != MWAWDocument::MWAW_C_EXCELLENT) {
    printf("ERROR: Unsupported file format!\n");
    return 1;
  }
  if (type == MWAWDocument::MWAW_T_UNKNOWN) {
    printf("ERROR: can not determine the file type!\n");
    return 1;
  }

  MWAWDocument::Result error = MWAWDocument::MWAW_R_OK;
  try {
    if (kind == MWAWDocument::MWAW_K_DRAW || kind == MWAWDocument::MWAW_K_PAINT) {
      librevenge::RVNGRawDrawingGenerator documentGenerator(printIndentLevel);
      error=MWAWDocument::parse(&input, &documentGenerator);
    }
    else if (kind == MWAWDocument::MWAW_K_SPREADSHEET || kind == MWAWDocument::MWAW_K_DATABASE) {
      librevenge::RVNGRawSpreadsheetGenerator documentGenerator(printIndentLevel);
      error=MWAWDocument::parse(&input, &documentGenerator);
    }
    else if (kind == MWAWDocument::MWAW_K_PRESENTATION) {
      librevenge::RVNGRawPresentationGenerator documentGenerator(printIndentLevel);
      error=MWAWDocument::parse(&input, &documentGenerator);
    }
    else {
      librevenge::RVNGRawTextGenerator documentGenerator(printIndentLevel);
      error=MWAWDocument::parse(&input, &documentGenerator);
    }
  }
  catch (MWAWDocument::Result const &err) {
    error=err;
  }
  catch (...) {
    error = MWAWDocument::MWAW_R_UNKNOWN_ERROR;
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

  return 0;
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
