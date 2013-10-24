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

/** \file MWAWDocument.hxx
 * libmwaw API: main interface functions of the libmwaw
 *
 * \see libmwaw.hxx
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

namespace libwpg
{
class WPGPaintInterface;
}

class WPXBinaryData;
class WPXDocumentInterface;
class WPXInputStream;

/**
This class provides all the functions needed by applications to parse many pre-MacOSX documents.
*/
class MWAWDocument
{
public:
  /** an enum which defines if we have confidence that a file is supported */
  enum Confidence {
    MWAW_C_NONE=0/**< not supported */,
    MWAW_C_UNSUPPORTED_ENCRYPTION /** encryption not supported*/,
    MWAW_C_SUPPORTED_ENCRYPTION /** encryption supported */,
    MWAW_C_EXCELLENT /** supported */
  };
  /** an enum to define the kind of document */
  enum Kind {
    MWAW_K_UNKNOWN=0 /**< unknown*/,
    MWAW_K_TEXT /** word processing file*/,
    MWAW_K_DRAW /** vectorized grphic*/,
    MWAW_K_PAINT /** bitmap graphic*/,
    MWAW_K_PRESENTATION /** presentation graphic*/,
    MWAW_K_SPREADSHEET /** spreadsheet */,
    MWAW_K_DATABASE /** database */
  };
  /** an enum which defines the result of the file parsing */
  enum Result {
    MWAW_R_OK=0 /**< conversion ok*/,
    MWAW_R_FILE_ACCESS_ERROR /** problem when accessing file*/,
    MWAW_R_OLE_ERROR /** problem when reading the OLE structure*/,
    MWAW_R_PARSE_ERROR /** problem when parsing the file*/,
    MWAW_R_PASSWORD_MISSMATCH_ERROR /** problem when using the given password*/,
    MWAW_R_UNKNOWN_ERROR /** unknown error*/
  };
  /** an enum to define the different type of document */
  enum Type {
    MWAW_T_UNKNOWN=0 /**< Unrecognised file type*/,
    MWAW_T_ACTA /**Acta (v2 and Classic v1)*/,
    MWAW_T_BEAGLEWORKS /**BeagleWorks (v1.0)/WordPerfect Works (v1.2)*/,
    MWAW_T_CLARISWORKS /**ClarisWorks/AppleWorks: all versions, open text files + some draw/paint files*/,
    MWAW_T_DOCMAKER /** DocMaker (v4)*/,
    MWAW_T_EDOC /** eDOC (v2)*/,
    MWAW_T_FRAMEMAKER /** FrameMaker: TODO*/,
    MWAW_T_FULLWRITE /** FullWrite Professional: basic*/,
    MWAW_T_GREATWORKS /** GreatWorks (v1-v2): text and drawing document*/,
    MWAW_T_HANMACWORDJ /** HanMac Word-J (v2.0.4) */,
    MWAW_T_HANMACWORDK /** HanMac Word-K (v2.0.5-2.0.6) */,
    MWAW_T_LIGHTWAYTEXT /** LightWayText (only v4 Mac format) */,
    MWAW_T_MACDOC /** MacDoc (v1.3)*/,
    MWAW_T_MACDRAW /** MacDraw: TODO*/,
    MWAW_T_MACPAINT /** MacPaint: TODO*/,
    MWAW_T_MARINERWRITE /** Mariner Write (only v1.6-v3.5 Mac Classic) */,
    MWAW_T_MINDWRITE /** MindWrite */,
    MWAW_T_MORE /** More (v2-3): retrieve the organization part but not the slide/tree parts*/,
    MWAW_T_MICROSOFTWORD /** Microsoft Word  (v1-v5)*/,
    MWAW_T_MICROSOFTWORKS /** Microsoft Works Mac*/,
    MWAW_T_MACWRITE /** MacWrite */,
    MWAW_T_MACWRITEPRO /** MacWrite II/Pro*/,
    MWAW_T_NISUSWRITER /** Nisus Writer (v3.4-v6.5)*/,
    MWAW_T_PAGEMAKER /** PageMaker: TODO*/,
    MWAW_T_RAGTIME /** RagTime: TODO*/,
    MWAW_T_READYSETGO /** Ready,Set,Go!: TODO*/,
    MWAW_T_TEACHTEXT /** TeachText/SimpleText*/,
    MWAW_T_TEXEDIT /** Tex-Edit (v2)*/,
    MWAW_T_WRITENOW /** WriteNow*/,
    MWAW_T_WRITERPLUS /** WriterPlus*/,
    MWAW_T_XPRESS /** XPress: TODO*/,
    MWAW_T_ZWRITE /** Z-Write (v1.3)*/,

    MWAW_T_RESERVED1 /** Reserved for future use*/,
    MWAW_T_RESERVED2 /** Reserved for future use*/,
    MWAW_T_RESERVED3 /** Reserved for future use*/,
    MWAW_T_RESERVED4 /** Reserved for future use*/,
    MWAW_T_RESERVED5 /** Reserved for future use*/,
    MWAW_T_RESERVED6 /** Reserved for future use*/,
    MWAW_T_RESERVED7 /** Reserved for future use*/,
    MWAW_T_RESERVED8 /** Reserved for future use*/,
    MWAW_T_RESERVED9 /** Reserved for future use*/
  };

  /** Analyzes the content of an input stream to see if it can be parsed
      \param input The input stream
      \param type The document type ( filled if the file is supported )
      \param kind The document kind ( filled if the file is supported )
      \return A confidence value which represents the likelyhood that the content from
      the input stream can be parsed

      \note encryption enum appears in MWAW_TEXT_VERSION==2 */
  static MWAWLIB Confidence isFileFormatSupported(WPXInputStream *input, Type &type, Kind &kind);
  /** Parses the input stream content. It will make callbacks to the functions provided by a
     WPXDocumentInterface class implementation when needed. This is often commonly called the
     'main parsing routine'.
     \param input The input stream
     \param documentInterface A MWAWListener implementation
     \param password The file password

   \note password appears in MWAW_TEXT_VERSION==2 */
  static MWAWLIB Result parse(WPXInputStream *input, WPXDocumentInterface *documentInterface, char const *password=0);

  /** Parses the graphic contained in the binary data and called paintInterface to reconstruct
    a graphic. The input is normally send to a WPXDocumentInterface with mimeType="image/mwaw-odg",
    ie. it must correspond to a picture created by the MWAWGraphicInterface class via
    a MWAWPropertyEncoder.

   \param binary a list of WPGPaintInterface stored in a paintInterface,
   \param paintInterface the paint interface which will convert the graphic is some specific format
   (ODG, SVG, ...)

   \note this function appears in MWAW_GRAPHIC_VERSION==1 */
  static MWAWLIB bool decodeGraphic(WPXBinaryData const &binary, libwpg::WPGPaintInterface *paintInterface);
};

#endif /* MWAWDOCUMENT_HXX */
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
