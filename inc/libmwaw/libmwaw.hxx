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

/** @file libmwaw.hxx
 * @brief libmwaw API: main libmwaw interface header
 *
 * Includes MWAWDocument.hxx and a set of versions' define.
 */

/**
\mainpage libmwaw documentation
This document contains both the libmwaw API specification and the normal libmwaw
documentation.

\section api_docs libmwaw API documentation

The external libmwaw API is provided by libmwaw.hxx and the MWAWDocument class. This
class, combined with the libwpd's WPXDocumentInterface class and libwpg's
WPGPaintInterface are the only three classes that will be of
interest for the application programmer using libmwaw.


\section lib_docs libmwaw documentation
If you are interrested in the structure of libmwaw itself, this whole document
would be a good starting point for exploring the interals of libmwaw. Mind that
this document is a work-in-progress, and will most likely not cover libmwaw for
the full 100%.

\warning When compiled with -DDEBUG_WITH__FILES, code is added to
store the results of the parsing in different files: one file by Ole
parts ( or sometimes to reconstruct a part of file which is stored
discontinuously ) and some files to store the read pictures. These
files are created in the current repository, therefore it is
recommended to launch the tests in a empty repository...
*/


#ifndef LIBMWAW_HXX
#define LIBMWAW_HXX

/** Defines the database possible conversion (actually none) */
#define MWAW_DATABASE_VERSION 0
/** Defines the vector graphic possible conversion:
    - 1: can create some graphic shapes in a WPXBinaryDate mimeType="image/mwaw-odg". You can use MWAWDocument::decodeGraphic to read them. */
#define MWAW_GRAPHIC_VERSION 1
/** Defines the bitmap graphic possible conversion (actually none) */
#define MWAW_PAINT_VERSION 0
/** Defines the presentation possible conversion (actually none) */
#define MWAW_PRESENTATION_VERSION 0
/** Defines the spreadsheet possible conversion (actually none) */
#define MWAW_SPREADSHEET_VERSION 0
/** Defines the word processing possible conversion:
    - 2: new interface with password encryption + API enums more meaningfull. */
#define MWAW_TEXT_VERSION 2

#include "MWAWDocument.hxx"

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
