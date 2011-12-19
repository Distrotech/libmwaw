/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libmwaw
 * Copyright (C) 2009, 2011 Alonso Laurent (alonso@loria.fr)
 * Copyright (C) 2006, 2007 Andrew Ziem
 * Copyright (C) 2004-2006 Fridrich Strba (fridrich.strba@bluewin.ch)
 * Copyright (C) 2004 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2003-2005 William Lachance (william.lachance@sympatico.ca)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 * For further information visit http://libmwaw.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

#ifndef IMWAWDOCUMENT_H
#define IMWAWDOCUMENT_H

#define MWAW_VERSION 2

#ifdef _WINDLL
#  ifdef BUILD_MWAW
#    define MWAWLIB _declspec(dllexport)
#  else
#    define MWAWLIB _declspec(dllimport)
#  endif
#else
#  define MWAWLIB
#endif


#include <libwpd-stream/WPXStream.h>

/** enum which defines the confidence that a file format is supported */
enum MWAWConfidence { MWAW_CONFIDENCE_NONE=0, MWAW_CONFIDENCE_POOR, MWAW_CONFIDENCE_LIKELY, MWAW_CONFIDENCE_GOOD, MWAW_CONFIDENCE_EXCELLENT };
/** enum which defines the result of the file parsing */
enum MWAWResult { MWAW_OK, MWAW_FILE_ACCESS_ERROR, MWAW_PARSE_ERROR, MWAW_OLE_ERROR, MWAW_UNKNOWN_ERROR };

class WPXDocumentInterface;
class WDBInterface;

/**
This class provides all the functions an application would need to parse
Works documents.
*/
class IMWAWDocument
{
public:
  /** an enum to define the different type of document

  -CW: claris works document
  -MW: macwrite document
  -WPLUS: writerplus document ( or maybe not)
  */
  enum DocumentType { UNKNOWN, CW, MW, WPLUS, WNOW };

  /** an enum to define the kind of document */
  enum DocumentKind { K_UNKNOWN, K_TEXT, K_DRAW,
                      K_PAINT, K_PRESENTATION, K_SPREADSHEET, K_DATABASE
                    };

  static MWAWLIB MWAWConfidence isFileFormatSupported(WPXInputStream *input, DocumentType &type, DocumentKind &kind);
  static MWAWLIB MWAWResult parse(WPXInputStream *input, WPXDocumentInterface *documentInterface);
};

#endif /* MWAWDOCUMENT_H */
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
