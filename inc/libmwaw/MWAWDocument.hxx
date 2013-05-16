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

#ifndef MWAWDOCUMENT_H
#define MWAWDOCUMENT_H

#ifdef _WINDLL
#  ifdef BUILD_MWAW
#    define MWAWLIB _declspec(dllexport)
#  else
#    define MWAWLIB _declspec(dllimport)
#  endif
#else
#  define MWAWLIB
#endif


#include <libwpd-stream/libwpd-stream.h>

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
class MWAWDocument
{
public:
  /** an enum to define the different type of document
  -ACT: Acta (v2 and Classic v1)
  -CW: ClarisWorks/AppleWorks document (basic)
  -DM: DocMaker (v4)
  -ED: eDOC (v2)
  -FRM: Frame Maker (nothing done)
  -FULLW: FullWrite Professional (basic)
  -GW: GreatWorks (nothing done )
  -HMACJ: HanMac Word-J ( v2.0.4 )
  -HMAC: HanMac Word-K ( v2.0.5-2.0.6 ) -> MUST BE rename in HMACK 
  -LWTEXT: LightWayText ( only v4.5 Mac format )
  -MACD: MacDoc (nothing done )
  -MARIW: Mariner Write ( only v1.6-v3.5 Mac Classic)
  -MINDW: MindWrite
  -MOCKP: MockPackage (nothing done )
  -MORE: More (nothing done )
  -MW: MacWrite document
  -MWPRO: MacWriteII or MacWritePro document
  -MSWORD: MSWord document (v4 v5: basic done)
  -MSWORKS: MSWorks document (v1 v2)
  -NISUSW: Nisus Writer document: v3.4-v6.5
  -PAGEMK: Page Maker ( nothing done)
  -RSG: Ready Set Go (nothing done)
  -RGTIME: RagTime (nothing done)
  -TEACH: TeachText or SimpleText: v1
  -TEDIT: Tex-Edit v2
  -WNOW: WriteNow
  -WPLUS: writerplus document
  -WPW: WordPerfect Works (nothing done)
  -XP: XPress (nothing done )
  -ZWRT: Z-Write : v1.3

  -RESERVED1-9: reserved to future use (BeagleWorks, DreamWeaver?, Canvas?, InDesign?, Taste? )
  */
  enum DocumentType {
    UNKNOWN, ACT, CW, DM, ED, FRM, FULLW, GW, HMACJ, HMAC, LWTEXT, MACD, MARIW, MINDW, MOCKP, MORE, MSWORD,
    MSWORKS, MW, MWPRO, NISUSW, PAGEMK, RGTIME, RSG, TEACH, TEDIT, WNOW, WPLUS, WPW, XP, ZWRT,
    RESERVED1, RESERVED2, RESERVED3, RESERVED4, RESERVED5, RESERVED6, RESERVED7, RESERVED8,
    RESERVED9
  };

  /** an enum to define the kind of document */
  enum DocumentKind { K_UNKNOWN, K_TEXT, K_DRAW,
                      K_PAINT, K_PRESENTATION, K_SPREADSHEET, K_DATABASE
                    };

  static MWAWLIB MWAWConfidence isFileFormatSupported(WPXInputStream *input, DocumentType &type, DocumentKind &kind);
  static MWAWLIB MWAWResult parse(WPXInputStream *input, WPXDocumentInterface *documentInterface);
};

#endif /* MWAWDOCUMENT_H */
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
