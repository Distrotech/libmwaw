/* -*- Mode: C++; c-default-style: "k&r"; tab-width: 2; c-basic-offset: 2 -*- */
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
 *
 * Wrapper to old interface
 */

#ifndef MWAWDOCUMENT_H
#define MWAWDOCUMENT_H

#include "IMWAWDocument.hxx"

/**
This class provides a wrapper to IMWAWDocument
*/
class MWAWDocument : public IMWAWDocument
{
public:
  static MWAWLIB MWAWConfidence isFileFormatSupported(WPXInputStream *input) {
    IMWAWDocument::DocumentType type;
    IMWAWDocument::DocumentKind kind;
    MWAWConfidence res=IMWAWDocument::isFileFormatSupported(input, type, kind);
    if (res == MWAW_CONFIDENCE_NONE)
      return MWAW_CONFIDENCE_NONE;
#ifndef DEBUG
    if (kind != IMWAWDocument::K_TEXT || kind != IMWAWDocument::K_PRESENTATION)
      return MWAW_CONFIDENCE_NONE;
#endif
    return res;
  }

  static MWAWLIB MWAWResult parse(WPXInputStream *input, WPXDocumentInterface *documentInterface) {
    return IMWAWDocument::parse(input, documentInterface);
  }

};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
