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

/** \file MWAWDocument.cxx
 * libmwaw API: implementation of main interface functions
 */

#include "MWAWHeader.hxx"
#include "MWAWGraphicDecoder.hxx"
#include "MWAWParser.hxx"
#include "MWAWPropertyHandler.hxx"
#include "MWAWRSRCParser.hxx"

#include <libmwaw/libmwaw.hxx>

#include "ActaParser.hxx"
#include "BeagleWksParser.hxx"
#include "BeagleWksBMParser.hxx"
#include "BeagleWksSSParser.hxx"
#include "ClarisWksParser.hxx"
#include "ClarisWksBMParser.hxx"
#include "ClarisWksSSParser.hxx"
#include "DocMkrParser.hxx"
#include "EDocParser.hxx"
#include "FullWrtParser.hxx"
#include "GreatWksParser.hxx"
#include "GreatWksBMParser.hxx"
#include "GreatWksSSParser.hxx"
#include "HanMacWrdJParser.hxx"
#include "HanMacWrdKParser.hxx"
#include "LightWayTxtParser.hxx"
#include "MacDocParser.hxx"
#include "MacPaintParser.hxx"
#include "MacWrtParser.hxx"
#include "MacWrtProParser.hxx"
#include "MarinerWrtParser.hxx"
#include "MindWrtParser.hxx"
#include "MoreParser.hxx"
#include "MsWks3Parser.hxx"
#include "MsWks4Parser.hxx"
#include "MsWksSSParser.hxx"
#include "MsWrd1Parser.hxx"
#include "MsWrdParser.hxx"
#include "NisusWrtParser.hxx"
#include "SuperPaintParser.hxx"
#include "TeachTxtParser.hxx"
#include "WingzParser.hxx"
#include "WriteNowParser.hxx"
#include "WriterPlsParser.hxx"
#include "ZWrtParser.hxx"

/** small namespace use to define private class/method used by MWAWDocument */
namespace MWAWDocumentInternal
{
shared_ptr<MWAWGraphicParser> getGraphicParserFromHeader(MWAWInputStreamPtr &input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
shared_ptr<MWAWTextParser> getTextParserFromHeader(MWAWInputStreamPtr &input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
shared_ptr<MWAWSpreadsheetParser> getSpreadsheetParserFromHeader(MWAWInputStreamPtr &input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
MWAWHeader *getHeader(MWAWInputStreamPtr &input, MWAWRSRCParserPtr rsrcParser, bool strict);
bool checkBasicMacHeader(MWAWInputStreamPtr &input, MWAWRSRCParserPtr rsrcParser, MWAWHeader &header, bool strict);
}

MWAWDocument::Confidence MWAWDocument::isFileFormatSupported(librevenge::RVNGInputStream *input,  MWAWDocument::Type &type, Kind &kind)
{
  type = MWAW_T_UNKNOWN;
  kind = MWAW_K_UNKNOWN;

  if (!input) {
    MWAW_DEBUG_MSG(("MWAWDocument::isFileFormatSupported(): no input\n"));
    return MWAW_C_NONE;
  }

  try {
    MWAW_DEBUG_MSG(("MWAWDocument::isFileFormatSupported()\n"));
    MWAWInputStreamPtr ip(new MWAWInputStream(input, false, true));
    MWAWInputStreamPtr rsrc=ip->getResourceForkStream();
    shared_ptr<MWAWRSRCParser> rsrcParser;
    if (rsrc)
      rsrcParser.reset(new MWAWRSRCParser(rsrc));
    shared_ptr<MWAWHeader> header;
#ifdef DEBUG
    header.reset(MWAWDocumentInternal::getHeader(ip, rsrcParser, false));
#else
    header.reset(MWAWDocumentInternal::getHeader(ip, rsrcParser, true));
#endif

    if (!header.get())
      return MWAW_C_NONE;
    type = (MWAWDocument::Type)header->getType();
    kind = (MWAWDocument::Kind)header->getKind();
    Confidence confidence = MWAW_C_NONE;

    switch (type) {
    case MWAW_T_ACTA:
    case MWAW_T_BEAGLEWORKS:
    case MWAW_T_CLARISRESOLVE:
    case MWAW_T_CLARISWORKS:
    case MWAW_T_DOCMAKER:
    case MWAW_T_EDOC:
    case MWAW_T_FULLWRITE:
    case MWAW_T_GREATWORKS:
    case MWAW_T_HANMACWORDJ:
    case MWAW_T_HANMACWORDK:
    case MWAW_T_LIGHTWAYTEXT:
    case MWAW_T_MACDOC:
    case MWAW_T_MACPAINT:
    case MWAW_T_MACWRITE:
    case MWAW_T_MACWRITEPRO:
    case MWAW_T_MARINERWRITE:
    case MWAW_T_MINDWRITE:
    case MWAW_T_MORE:
    case MWAW_T_MICROSOFTWORD:
    case MWAW_T_MICROSOFTWORKS:
    case MWAW_T_NISUSWRITER:
    case MWAW_T_SUPERPAINT:
    case MWAW_T_TEACHTEXT:
    case MWAW_T_TEXEDIT:
    case MWAW_T_WINGZ:
    case MWAW_T_WRITENOW:
    case MWAW_T_WRITERPLUS:
    case MWAW_T_ZWRITE:
      confidence = MWAW_C_EXCELLENT;
      break;
    case MWAW_T_FRAMEMAKER:
    case MWAW_T_FULLIMPACT:
    case MWAW_T_FULLPAINT:
    case MWAW_T_KALEIDAGRAPH:
    case MWAW_T_MACDRAFT:
    case MWAW_T_MACDRAW:
    case MWAW_T_MICROSOFTMULTIPLAN:
    case MWAW_T_PAGEMAKER:
    case MWAW_T_PIXELPAINT:
    case MWAW_T_READYSETGO:
    case MWAW_T_RAGTIME:
    case MWAW_T_TRAPEZE:
    case MWAW_T_XPRESS:
    case MWAW_T_RESERVED1:
    case MWAW_T_RESERVED2:
    case MWAW_T_RESERVED3:
    case MWAW_T_RESERVED4:
    case MWAW_T_RESERVED5:
    case MWAW_T_RESERVED6:
    case MWAW_T_RESERVED7:
    case MWAW_T_RESERVED8:
    case MWAW_T_RESERVED9:
    case MWAW_T_UNKNOWN:
    default:
      break;
    }

    return confidence;
  }
  catch (...) {
    type = MWAW_T_UNKNOWN;
    kind = MWAW_K_UNKNOWN;
    return MWAW_C_NONE;
  }
}

MWAWDocument::Result MWAWDocument::parse(librevenge::RVNGInputStream *input, librevenge::RVNGDrawingInterface *documentInterface, char const *)
{
  if (!input)
    return MWAW_R_UNKNOWN_ERROR;
  Result error = MWAW_R_OK;

  try {
    MWAWInputStreamPtr ip(new MWAWInputStream(input, false, true));
    MWAWInputStreamPtr rsrc=ip->getResourceForkStream();
    shared_ptr<MWAWRSRCParser> rsrcParser;
    if (rsrc) {
      rsrcParser.reset(new MWAWRSRCParser(rsrc));
      rsrcParser->setAsciiName("RSRC");
      rsrcParser->parse();
    }
    shared_ptr<MWAWHeader> header(MWAWDocumentInternal::getHeader(ip, rsrcParser, false));

    if (!header.get()) return MWAW_R_UNKNOWN_ERROR;

    shared_ptr<MWAWGraphicParser> parser=MWAWDocumentInternal::getGraphicParserFromHeader(ip, rsrcParser, header.get());
    if (!parser) return MWAW_R_UNKNOWN_ERROR;
    parser->parse(documentInterface);
  }
  catch (libmwaw::FileException) {
    MWAW_DEBUG_MSG(("File exception trapped\n"));
    error = MWAW_R_FILE_ACCESS_ERROR;
  }
  catch (libmwaw::ParseException) {
    MWAW_DEBUG_MSG(("Parse exception trapped\n"));
    error = MWAW_R_PARSE_ERROR;
  }
  catch (...) {
    //fixme: too generic
    MWAW_DEBUG_MSG(("Unknown exception trapped\n"));
    error = MWAW_R_UNKNOWN_ERROR;
  }

  return error;
}

MWAWDocument::Result MWAWDocument::parse(librevenge::RVNGInputStream *, librevenge::RVNGPresentationInterface *, char const *)
{
  MWAW_DEBUG_MSG(("MWAWDocument::parse[Presentation]: unimplemented\n"));
  return MWAW_R_UNKNOWN_ERROR;
}

MWAWDocument::Result MWAWDocument::parse(librevenge::RVNGInputStream *input, librevenge::RVNGSpreadsheetInterface *documentInterface, char const *)
{
  if (!input)
    return MWAW_R_UNKNOWN_ERROR;
  Result error = MWAW_R_OK;

  try {
    MWAWInputStreamPtr ip(new MWAWInputStream(input, false, true));
    MWAWInputStreamPtr rsrc=ip->getResourceForkStream();
    shared_ptr<MWAWRSRCParser> rsrcParser;
    if (rsrc) {
      rsrcParser.reset(new MWAWRSRCParser(rsrc));
      rsrcParser->setAsciiName("RSRC");
      rsrcParser->parse();
    }
    shared_ptr<MWAWHeader> header(MWAWDocumentInternal::getHeader(ip, rsrcParser, false));

    if (!header.get()) return MWAW_R_UNKNOWN_ERROR;

    shared_ptr<MWAWSpreadsheetParser> parser=MWAWDocumentInternal::getSpreadsheetParserFromHeader(ip, rsrcParser, header.get());
    if (!parser) return MWAW_R_UNKNOWN_ERROR;
    parser->parse(documentInterface);
  }
  catch (libmwaw::FileException) {
    MWAW_DEBUG_MSG(("File exception trapped\n"));
    error = MWAW_R_FILE_ACCESS_ERROR;
  }
  catch (libmwaw::ParseException) {
    MWAW_DEBUG_MSG(("Parse exception trapped\n"));
    error = MWAW_R_PARSE_ERROR;
  }
  catch (...) {
    //fixme: too generic
    MWAW_DEBUG_MSG(("Unknown exception trapped\n"));
    error = MWAW_R_UNKNOWN_ERROR;
  }

  return error;
}

MWAWDocument::Result MWAWDocument::parse(librevenge::RVNGInputStream *input, librevenge::RVNGTextInterface *documentInterface, char const */*password*/)
{
  if (!input)
    return MWAW_R_UNKNOWN_ERROR;
  Result error = MWAW_R_OK;

  try {
    MWAWInputStreamPtr ip(new MWAWInputStream(input, false, true));
    MWAWInputStreamPtr rsrc=ip->getResourceForkStream();
    shared_ptr<MWAWRSRCParser> rsrcParser;
    if (rsrc) {
      rsrcParser.reset(new MWAWRSRCParser(rsrc));
      rsrcParser->setAsciiName("RSRC");
      rsrcParser->parse();
    }
    shared_ptr<MWAWHeader> header(MWAWDocumentInternal::getHeader(ip, rsrcParser, false));

    if (!header.get()) return MWAW_R_UNKNOWN_ERROR;

    shared_ptr<MWAWTextParser> parser=MWAWDocumentInternal::getTextParserFromHeader(ip, rsrcParser, header.get());
    if (!parser) return MWAW_R_UNKNOWN_ERROR;
    parser->parse(documentInterface);
  }
  catch (libmwaw::FileException) {
    MWAW_DEBUG_MSG(("File exception trapped\n"));
    error = MWAW_R_FILE_ACCESS_ERROR;
  }
  catch (libmwaw::ParseException) {
    MWAW_DEBUG_MSG(("Parse exception trapped\n"));
    error = MWAW_R_PARSE_ERROR;
  }
  catch (...) {
    //fixme: too generic
    MWAW_DEBUG_MSG(("Unknown exception trapped\n"));
    error = MWAW_R_UNKNOWN_ERROR;
  }

  return error;
}

bool MWAWDocument::decodeGraphic(librevenge::RVNGBinaryData const &binary, librevenge::RVNGDrawingInterface *paintInterface)
{
  if (!paintInterface || !binary.size()) {
    MWAW_DEBUG_MSG(("MWAWDocument::decodeGraphic: called with no data or no converter\n"));
    return false;
  }
  MWAWGraphicDecoder tmpHandler(paintInterface);
  try {
    if (!tmpHandler.checkData(binary) || !tmpHandler.readData(binary)) return false;
  }
  catch (...) {
    MWAW_DEBUG_MSG(("MWAWDocument::decodeGraphic: unknown error\n"));
    return false;
  }
  return true;
}

bool MWAWDocument::decodeSpreadsheet(librevenge::RVNGBinaryData const &, librevenge::RVNGSpreadsheetInterface *)
{
  MWAW_DEBUG_MSG(("MWAWDocument::decodeSpreadsheet: unimplemented\n"));
  return false;
}

bool MWAWDocument::decodeText(librevenge::RVNGBinaryData const &, librevenge::RVNGTextInterface *)
{
  MWAW_DEBUG_MSG(("MWAWDocument::decodeText: unimplemented\n"));
  return false;
}

namespace MWAWDocumentInternal
{
/** return the header corresponding to an input. Or 0L if no input are found */
MWAWHeader *getHeader(MWAWInputStreamPtr &ip,
                      MWAWRSRCParserPtr rsrcParser,
                      bool strict)
{
  std::vector<MWAWHeader> listHeaders;

  try {
    if (!ip.get()) return 0L;

    if (ip->hasDataFork()) {
      /** avoid very short file */
      if (!ip->hasResourceFork() && ip->size() < 10) return 0L;

      ip->seek(0, librevenge::RVNG_SEEK_SET);
      ip->setReadInverted(false);
    }
    else if (!ip->hasResourceFork())
      return 0L;

    listHeaders = MWAWHeader::constructHeader(ip, rsrcParser);
    size_t numHeaders = listHeaders.size();
    if (numHeaders==0) return 0L;

    for (size_t i = 0; i < numHeaders; i++) {
      if (!MWAWDocumentInternal::checkBasicMacHeader(ip, rsrcParser, listHeaders[i], strict))
        continue;
      return new MWAWHeader(listHeaders[i]);
    }
  }
  catch (libmwaw::FileException) {
    MWAW_DEBUG_MSG(("File exception trapped\n"));
  }
  catch (libmwaw::ParseException) {
    MWAW_DEBUG_MSG(("Parse exception trapped\n"));
  }
  catch (...) {
    //fixme: too generic
    MWAW_DEBUG_MSG(("Unknown exception trapped\n"));
  }
  return 0L;
}

shared_ptr<MWAWGraphicParser> getGraphicParserFromHeader(MWAWInputStreamPtr &input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header)
{
  shared_ptr<MWAWGraphicParser> parser;
  if (!header)
    return parser;
  if (header->getKind()!=MWAWDocument::MWAW_K_DRAW && header->getKind()!=MWAWDocument::MWAW_K_PAINT)
    return parser;
  if (header->getKind()==MWAWDocument::MWAW_K_DRAW &&
      (header->getType()==MWAWDocument::MWAW_T_CLARISWORKS ||header->getType()==MWAWDocument::MWAW_T_GREATWORKS))
    return parser;

  try {
    switch (header->getType()) {
    case MWAWDocument::MWAW_T_BEAGLEWORKS:
      if (header->getKind()==MWAWDocument::MWAW_K_PAINT)
        parser.reset(new BeagleWksBMParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_CLARISWORKS:
      if (header->getKind()==MWAWDocument::MWAW_K_PAINT)
        parser.reset(new ClarisWksBMParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_GREATWORKS:
      if (header->getKind()==MWAWDocument::MWAW_K_PAINT)
        parser.reset(new GreatWksBMParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_MACPAINT:
      parser.reset(new MacPaintParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_SUPERPAINT:
      parser.reset(new SuperPaintParser(input, rsrcParser, header));
      break;
    // TODO: first separate graphic format to other formats, then implement parser...
    case MWAWDocument::MWAW_T_ACTA:
    case MWAWDocument::MWAW_T_CLARISRESOLVE:
    case MWAWDocument::MWAW_T_DOCMAKER:
    case MWAWDocument::MWAW_T_EDOC:
    case MWAWDocument::MWAW_T_FRAMEMAKER:
    case MWAWDocument::MWAW_T_FULLIMPACT:
    case MWAWDocument::MWAW_T_FULLPAINT:
    case MWAWDocument::MWAW_T_FULLWRITE:
    case MWAWDocument::MWAW_T_KALEIDAGRAPH:
    case MWAWDocument::MWAW_T_HANMACWORDJ:
    case MWAWDocument::MWAW_T_HANMACWORDK:
    case MWAWDocument::MWAW_T_LIGHTWAYTEXT:
    case MWAWDocument::MWAW_T_MACDOC:
    case MWAWDocument::MWAW_T_MACDRAFT:
    case MWAWDocument::MWAW_T_MACDRAW:
    case MWAWDocument::MWAW_T_MACWRITE:
    case MWAWDocument::MWAW_T_MACWRITEPRO:
    case MWAWDocument::MWAW_T_MARINERWRITE:
    case MWAWDocument::MWAW_T_MINDWRITE:
    case MWAWDocument::MWAW_T_MICROSOFTMULTIPLAN:
    case MWAWDocument::MWAW_T_MICROSOFTWORD:
    case MWAWDocument::MWAW_T_MICROSOFTWORKS:
    case MWAWDocument::MWAW_T_MORE:
    case MWAWDocument::MWAW_T_NISUSWRITER:
    case MWAWDocument::MWAW_T_PAGEMAKER:
    case MWAWDocument::MWAW_T_PIXELPAINT:
    case MWAWDocument::MWAW_T_RAGTIME:
    case MWAWDocument::MWAW_T_READYSETGO:
    case MWAWDocument::MWAW_T_TEACHTEXT:
    case MWAWDocument::MWAW_T_TEXEDIT:
    case MWAWDocument::MWAW_T_TRAPEZE:
    case MWAWDocument::MWAW_T_WINGZ:
    case MWAWDocument::MWAW_T_WRITENOW:
    case MWAWDocument::MWAW_T_WRITERPLUS:
    case MWAWDocument::MWAW_T_XPRESS:
    case MWAWDocument::MWAW_T_ZWRITE:

    case MWAWDocument::MWAW_T_RESERVED1:
    case MWAWDocument::MWAW_T_RESERVED2:
    case MWAWDocument::MWAW_T_RESERVED3:
    case MWAWDocument::MWAW_T_RESERVED4:
    case MWAWDocument::MWAW_T_RESERVED5:
    case MWAWDocument::MWAW_T_RESERVED6:
    case MWAWDocument::MWAW_T_RESERVED7:
    case MWAWDocument::MWAW_T_RESERVED8:
    case MWAWDocument::MWAW_T_RESERVED9:
    case MWAWDocument::MWAW_T_UNKNOWN:
    default:
      break;
    }
  }
  catch (...) {
  }
  return parser;
}

/** Factory wrapper to construct a parser corresponding to an spreadsheet header */
shared_ptr<MWAWSpreadsheetParser> getSpreadsheetParserFromHeader(MWAWInputStreamPtr &input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header)
{
  shared_ptr<MWAWSpreadsheetParser> parser;
  if (!header)
    return parser;
  if (header->getKind()!=MWAWDocument::MWAW_K_SPREADSHEET && header->getKind()!=MWAWDocument::MWAW_K_DATABASE)
    return parser;

  try {
    switch (header->getType()) {
    case MWAWDocument::MWAW_T_BEAGLEWORKS:
      parser.reset(new BeagleWksSSParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_CLARISRESOLVE:
      parser.reset(new WingzParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_CLARISWORKS:
      parser.reset(new ClarisWksSSParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_GREATWORKS:
      parser.reset(new GreatWksSSParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_MICROSOFTWORKS:
      parser.reset(new MsWksSSParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_WINGZ:
      parser.reset(new WingzParser(input, rsrcParser, header));
      break;
    // TODO
    case MWAWDocument::MWAW_T_FULLIMPACT:
    case MWAWDocument::MWAW_T_KALEIDAGRAPH:
    case MWAWDocument::MWAW_T_MICROSOFTMULTIPLAN:
    case MWAWDocument::MWAW_T_TRAPEZE:

    // no spreadsheet
    case MWAWDocument::MWAW_T_ACTA:
    case MWAWDocument::MWAW_T_DOCMAKER:
    case MWAWDocument::MWAW_T_EDOC:
    case MWAWDocument::MWAW_T_FRAMEMAKER:
    case MWAWDocument::MWAW_T_FULLPAINT:
    case MWAWDocument::MWAW_T_FULLWRITE:
    case MWAWDocument::MWAW_T_HANMACWORDJ:
    case MWAWDocument::MWAW_T_HANMACWORDK:
    case MWAWDocument::MWAW_T_LIGHTWAYTEXT:
    case MWAWDocument::MWAW_T_MACDOC:
    case MWAWDocument::MWAW_T_MACDRAFT:
    case MWAWDocument::MWAW_T_MACDRAW:
    case MWAWDocument::MWAW_T_MACPAINT:
    case MWAWDocument::MWAW_T_MACWRITE:
    case MWAWDocument::MWAW_T_MACWRITEPRO:
    case MWAWDocument::MWAW_T_MARINERWRITE:
    case MWAWDocument::MWAW_T_MICROSOFTWORD:
    case MWAWDocument::MWAW_T_MINDWRITE:
    case MWAWDocument::MWAW_T_MORE:
    case MWAWDocument::MWAW_T_NISUSWRITER:
    case MWAWDocument::MWAW_T_PAGEMAKER:
    case MWAWDocument::MWAW_T_PIXELPAINT:
    case MWAWDocument::MWAW_T_RAGTIME:
    case MWAWDocument::MWAW_T_READYSETGO:
    case MWAWDocument::MWAW_T_SUPERPAINT:
    case MWAWDocument::MWAW_T_TEACHTEXT:
    case MWAWDocument::MWAW_T_TEXEDIT:
    case MWAWDocument::MWAW_T_WRITENOW:
    case MWAWDocument::MWAW_T_WRITERPLUS:
    case MWAWDocument::MWAW_T_XPRESS:
    case MWAWDocument::MWAW_T_ZWRITE:

    case MWAWDocument::MWAW_T_RESERVED1:
    case MWAWDocument::MWAW_T_RESERVED2:
    case MWAWDocument::MWAW_T_RESERVED3:
    case MWAWDocument::MWAW_T_RESERVED4:
    case MWAWDocument::MWAW_T_RESERVED5:
    case MWAWDocument::MWAW_T_RESERVED6:
    case MWAWDocument::MWAW_T_RESERVED7:
    case MWAWDocument::MWAW_T_RESERVED8:
    case MWAWDocument::MWAW_T_RESERVED9:
    case MWAWDocument::MWAW_T_UNKNOWN:
    default:
      break;
    }
  }
  catch (...) {
  }
  return parser;
}

/** Factory wrapper to construct a parser corresponding to an text header */
shared_ptr<MWAWTextParser> getTextParserFromHeader(MWAWInputStreamPtr &input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header)
{
  shared_ptr<MWAWTextParser> parser;
  if (!header)
    return parser;
  if (header->getKind()==MWAWDocument::MWAW_K_SPREADSHEET || header->getKind()==MWAWDocument::MWAW_K_DATABASE ||
      header->getKind()==MWAWDocument::MWAW_K_PAINT)
    return parser;
  // removeme: actually ClarisWorks and GreatWorks draw file are exported as text file
  if (header->getKind()==MWAWDocument::MWAW_K_DRAW  &&
      header->getType()!=MWAWDocument::MWAW_T_CLARISWORKS && header->getType()!=MWAWDocument::MWAW_T_GREATWORKS)
    return parser;
  try {
    switch (header->getType()) {
    case MWAWDocument::MWAW_T_ACTA:
      parser.reset(new ActaParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_BEAGLEWORKS:
      parser.reset(new BeagleWksParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_CLARISWORKS:
      parser.reset(new ClarisWksParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_DOCMAKER:
      parser.reset(new DocMkrParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_EDOC:
      parser.reset(new EDocParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_FULLWRITE:
      parser.reset(new FullWrtParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_GREATWORKS:
      parser.reset(new GreatWksParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_HANMACWORDJ:
      parser.reset(new HanMacWrdJParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_HANMACWORDK:
      parser.reset(new HanMacWrdKParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_LIGHTWAYTEXT:
      parser.reset(new LightWayTxtParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_MACDOC:
      parser.reset(new MacDocParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_MACWRITE:
      parser.reset(new MacWrtParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_MACWRITEPRO:
      parser.reset(new MacWrtProParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_MARINERWRITE:
      parser.reset(new MarinerWrtParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_MINDWRITE:
      parser.reset(new MindWrtParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_MORE:
      parser.reset(new MoreParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_MICROSOFTWORD:
      if (header->getMajorVersion()==1)
        parser.reset(new MsWrd1Parser(input, rsrcParser, header));
      else
        parser.reset(new MsWrdParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_MICROSOFTWORKS:
      if (header->getMajorVersion() < 100)
        parser.reset(new MsWks3Parser(input, rsrcParser, header));
      else
        parser.reset(new MsWks4Parser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_NISUSWRITER:
      parser.reset(new NisusWrtParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_TEACHTEXT:
    case MWAWDocument::MWAW_T_TEXEDIT:
      parser.reset(new TeachTxtParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_WRITENOW:
      parser.reset(new WriteNowParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_WRITERPLUS:
      parser.reset(new WriterPlsParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_ZWRITE:
      parser.reset(new ZWrtParser(input, rsrcParser, header));
      break;

    case MWAWDocument::MWAW_T_CLARISRESOLVE:
    case MWAWDocument::MWAW_T_FULLIMPACT:
    case MWAWDocument::MWAW_T_FULLPAINT:
    case MWAWDocument::MWAW_T_FRAMEMAKER:
    case MWAWDocument::MWAW_T_KALEIDAGRAPH:
    case MWAWDocument::MWAW_T_MACDRAFT:
    case MWAWDocument::MWAW_T_MACDRAW:
    case MWAWDocument::MWAW_T_MACPAINT:
    case MWAWDocument::MWAW_T_MICROSOFTMULTIPLAN:
    case MWAWDocument::MWAW_T_PAGEMAKER:
    case MWAWDocument::MWAW_T_PIXELPAINT:
    case MWAWDocument::MWAW_T_READYSETGO:
    case MWAWDocument::MWAW_T_RAGTIME:
    case MWAWDocument::MWAW_T_SUPERPAINT:
    case MWAWDocument::MWAW_T_TRAPEZE:
    case MWAWDocument::MWAW_T_WINGZ:
    case MWAWDocument::MWAW_T_XPRESS:

    case MWAWDocument::MWAW_T_RESERVED1:
    case MWAWDocument::MWAW_T_RESERVED2:
    case MWAWDocument::MWAW_T_RESERVED3:
    case MWAWDocument::MWAW_T_RESERVED4:
    case MWAWDocument::MWAW_T_RESERVED5:
    case MWAWDocument::MWAW_T_RESERVED6:
    case MWAWDocument::MWAW_T_RESERVED7:
    case MWAWDocument::MWAW_T_RESERVED8:
    case MWAWDocument::MWAW_T_RESERVED9:
    case MWAWDocument::MWAW_T_UNKNOWN:
    default:
      break;
    }
  }
  catch (...) {
  }
  return parser;
}

/** Wrapper to check a basic header of a mac file */
bool checkBasicMacHeader(MWAWInputStreamPtr &input, MWAWRSRCParserPtr rsrcParser, MWAWHeader &header, bool strict)
{
  try {
    shared_ptr<MWAWParser> parser=getTextParserFromHeader(input, rsrcParser, &header);
    if (!parser)
      parser=getSpreadsheetParserFromHeader(input, rsrcParser, &header);
    if (!parser)
      parser=getGraphicParserFromHeader(input, rsrcParser, &header);
    if (!parser)
      return false;
    return parser->checkHeader(&header, strict);
  }
  catch (...) {
  }

  return false;
}

}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
