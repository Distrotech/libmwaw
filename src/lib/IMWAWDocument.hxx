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

#ifdef _WINDLL
#  ifdef BUILD_MWAW
#    define IMWAWLIB _declspec(dllexport)
#  else
#    define IMWAWLIB _declspec(dllimport)
#  endif
#else
#  define IMWAWLIB
#endif


#include <libwpd-stream/WPXStream.h>

/** enum which defines the confidence that a file format is supported */
enum IMWAWConfidence { IMWAW_CONFIDENCE_NONE=0, IMWAW_CONFIDENCE_POOR, IMWAW_CONFIDENCE_LIKELY, IMWAW_CONFIDENCE_GOOD, IMWAW_CONFIDENCE_EXCELLENT };
/** enum which defines the result of the file parsing */
enum IMWAWResult { IMWAW_OK, IMWAW_FILE_ACCESS_ERROR, IMWAW_PARSE_ERROR, IMWAW_OLE_ERROR, IMWAW_UNKNOWN_ERROR };

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

  -CW: ClarisWorks/AppleWorks document (basic)
  -FULLW: FullWrite Professional (crude parser)
  -MINDW: MindWrite (nothing done)
  -MW: MacWrite document
  -MWPRO: MacWriteII or MacWritePro document
  -MSWORD: MSWord document (v4 v5: basic done)
  -MSWORKS: MSWorks document (v1 v2)
  -NISUSW: Nisus Writer document (nothing done)
  -WNOW: WriteNow
  -WPLUS: writerplus document ( or maybe not)
  */
  enum DocumentType { UNKNOWN, CW, FULLW, MINDW, MSWORD, MSWORKS, MW, MWPRO, NISUSW, WNOW, WPLUS };

  /** an enum to define the kind of document */
  enum DocumentKind { K_UNKNOWN, K_TEXT, K_DRAW,
                      K_PAINT, K_PRESENTATION, K_SPREADSHEET, K_DATABASE
                    };

  static IMWAWLIB IMWAWConfidence isFileFormatSupported(WPXInputStream *input, DocumentType &type, DocumentKind &kind);
  static IMWAWLIB IMWAWResult parse(WPXInputStream *input, WPXDocumentInterface *documentInterface);
};

#endif /* IMWAWDOCUMENT_H */
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
