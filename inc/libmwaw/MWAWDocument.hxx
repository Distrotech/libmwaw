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

#ifndef MWAWDOCUMENT_HXX
#define MWAWDOCUMENT_HXX

#ifdef _WINDLL
#  ifdef BUILD_MWAW
#    define MWAWLIB _declspec(dllexport)
#  else
#    define MWAWLIB _declspec(dllimport)
#  endif
#else
#  define MWAWLIB
#endif

namespace libwpg {
  class WPGPaintInterface;
}

class WPXBinaryData;
class WPXDocumentInterface;
class WPXInputStream;

/**
This class provides all the functions an application would need to parse many pre-MacOSX documents.
*/
class MWAWDocument
{
public:
  /** enum which defines the confidence that a file format is supported */
  enum Confidence { MWAW_C_NONE=0, MWAW_C_UNSUPPORTED_ENCRYPTION, MWAW_C_SUPPORTED_ENCRYPTION, MWAW_C_EXCELLENT };
  /** an enum to define the kind of document */
  enum Kind { MWAW_K_UNKNOWN=0, MWAW_K_TEXT, MWAW_K_DRAW, MWAW_K_PAINT, MWAW_K_PRESENTATION, MWAW_K_SPREADSHEET, MWAW_K_DATABASE};
  /** enum which defines the result of the file parsing */
  enum Result { MWAW_R_OK=0, MWAW_R_FILE_ACCESS_ERROR, MWAW_R_OLE_ERROR, MWAW_R_PARSE_ERROR, MWAW_R_PASSWORD_MISSMATCH_ERROR, MWAW_R_UNKNOWN_ERROR };
  /** an enum to define the different type of document
  -MWAW_T_ACTA: Acta (v2 and Classic v1)
  -MWAW_T_BEAGLEWORKS: BeagleWorks (v1.0)/WordPerfect Works (v1.2)
  -MWAW_T_CLARISWORKS: ClarisWorks/AppleWorks document (all versions, open text files + some draw files)
  -MWAW_T_DOCMAKER: DocMaker (v4)
  -MWAW_T_EDOC: eDOC (v2)
  -MWAW_T_FRAMEMAKER: FrameMaker (TODO)
  -MWAW_T_FULLWRITE: FullWrite Professional (basic)
  -MWAW_T_GREATWORKS: GreatWorks (v1-v2, text and drawing document)
  -MWAW_T_HANMACWORDJ: HanMac Word-J (v2.0.4)
  -MWAW_T_HANMACWORDK: HanMac Word-K (v2.0.5-2.0.6)
  -MWAW_T_LIGHTWAYTEXT: LightWayText (only v4 Mac format)
  -MWAW_T_MACDOC: MacDoc (v1.3)
  -MWAW_T_MACDRAW: MacDraw (TODO)
  -MWAW_T_MACPAINT: MacPaint (TODO)
  -MWAW_T_MARINERWRITE: Mariner Write (only v1.6-v3.5 Mac Classic)
  -MWAW_T_MINDWRITE: MindWrite
  -MWAW_T_MORE: More (v2-3: retrieve the organization part but not the slide/tree parts)
  -MWAW_T_MICROSOFTWORD: Microsoft Word document (v1-v5)
  -MWAW_T_MICROSOFTWORKS: Microsoft Works Mac document
  -MWAW_T_MACWRITE: MacWrite document
  -MWAW_T_MACWRITEPRO: MacWriteII or MacWritePro document
  -MWAW_T_NISUSWRITER: Nisus Writer document: v3.4-v6.5
  -MWAW_T_PAGEMAKER: PageMaker (TODO)
  -MWAW_T_RAGTIME: RagTime (TODO)
  -MWAW_T_READYSETGO: Ready,Set,Go! (TODO)
  -MWAW_T_TEACHTEXT: TeachText or SimpleText
  -MWAW_T_TEXEDIT: Tex-Edit v2
  -MWAW_T_WRITENOW: WriteNow
  -MWAW_T_WRITERPLUS: WriterPlus document
  -MWAW_T_XPRESS: XPress (TODO)
  -MWAW_T_ZWRITE: Z-Write : v1.3

  -MWAW_T_RESERVED1-9: reserved to future use
  */
  enum Type {
    MWAW_T_UNKNOWN=0,
    MWAW_T_ACTA, MWAW_T_BEAGLEWORKS, MWAW_T_CLARISWORKS, MWAW_T_DOCMAKER, MWAW_T_EDOC, MWAW_T_FRAMEMAKER,
    MWAW_T_FULLWRITE, MWAW_T_GREATWORKS, MWAW_T_HANMACWORDJ, MWAW_T_HANMACWORDK, MWAW_T_LIGHTWAYTEXT,
    MWAW_T_MACDOC, MWAW_T_MACDRAW, MWAW_T_MACPAINT, MWAW_T_MARINERWRITE, MWAW_T_MINDWRITE, MWAW_T_MORE,
    MWAW_T_MICROSOFTWORD, MWAW_T_MICROSOFTWORKS, MWAW_T_MACWRITE, MWAW_T_MACWRITEPRO,
    MWAW_T_NISUSWRITER, MWAW_T_PAGEMAKER, MWAW_T_RAGTIME, MWAW_T_READYSETGO, MWAW_T_TEACHTEXT, MWAW_T_TEXEDIT,
    MWAW_T_WRITENOW, MWAW_T_WRITERPLUS, MWAW_T_XPRESS, MWAW_T_ZWRITE,
    
    MWAW_T_RESERVED1, MWAW_T_RESERVED2, MWAW_T_RESERVED3, MWAW_T_RESERVED4, MWAW_T_RESERVED5, MWAW_T_RESERVED6, MWAW_T_RESERVED7, MWAW_T_RESERVED8,
    MWAW_T_RESERVED9
  };

  /** check if the document defined by input is supported, and fill type and kind.

   \note: encryption enum appears in MWAW_TEXT_VERSION==2 */
  static MWAWLIB Confidence isFileFormatSupported(WPXInputStream *input, Type &type, Kind &kind);
  /** try to parse a document. Output the result in document interface.

   \note: password appears in MWAW_TEXT_VERSION==2 */
  static MWAWLIB Result parse(WPXInputStream *input, WPXDocumentInterface *documentInterface, char const *password=0);

  /** parse a local graphic created by the parse's function. Output the result in the paint interface.

   \note: this function appears in MWAW_GRAPHI_VERSION==1 */
  static MWAWLIB bool decodeGraphic(WPXBinaryData const &binary, libwpg::WPGPaintInterface *paintInterface);
};

#endif /* MWAWDOCUMENT_HXX */
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
