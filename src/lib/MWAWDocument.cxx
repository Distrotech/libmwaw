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

#include <string.h>

#include "MWAWHeader.hxx"
#include "MWAWParser.hxx"
#include "MWAWPropertyHandler.hxx"
#include "MWAWRSRCParser.hxx"

#include <libmwaw/libmwaw.hxx>

#include "ACParser.hxx"
#include "BWParser.hxx"
#include "CWParser.hxx"
#include "DMParser.hxx"
#include "EDParser.hxx"
#include "FWParser.hxx"
#include "GWParser.hxx"
#include "HMWJParser.hxx"
#include "HMWKParser.hxx"
#include "LWParser.hxx"
#include "MCDParser.hxx"
#include "MDWParser.hxx"
#include "MORParser.hxx"
#include "MRWParser.hxx"
#include "MWParser.hxx"
#include "MWProParser.hxx"
#include "MSK3Parser.hxx"
#include "MSK4Parser.hxx"
#include "MSW1Parser.hxx"
#include "MSWParser.hxx"
#include "NSParser.hxx"
#include "TTParser.hxx"
#include "WNParser.hxx"
#include "WPParser.hxx"
#include "ZWParser.hxx"

/** small namespace use to define private class/method used by MWAWDocument */
namespace MWAWDocumentInternal
{
shared_ptr<MWAWParser> getParserFromHeader(MWAWInputStreamPtr &input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
MWAWHeader *getHeader(MWAWInputStreamPtr &input, MWAWRSRCParserPtr rsrcParser, bool strict);
bool checkBasicMacHeader(MWAWInputStreamPtr &input, MWAWRSRCParserPtr rsrcParser, MWAWHeader &header, bool strict);

/** Small class used to reconstruct a RVNGBinary with mimeType="image/mwaw-odg" created by libmwaw */
class GraphicExporter : public MWAWPropertyHandler
{
public:
  /** constructor */
  GraphicExporter(RVNGDrawingInterface *output) : MWAWPropertyHandler(), m_output(output) { }
  /** destructor */
  ~GraphicExporter() {};

  /** insert an element */
  void insertElement(const char *psName);
  /** insert an element ( with a RVNGPropertyList ) */
  void insertElement(const char *psName, const RVNGPropertyList &xPropList);
  /** insert an element ( with a RVNGPropertyListVector parameter ) */
  void insertElement(const char *psName, const RVNGPropertyList &xPropList,
                     const RVNGPropertyListVector &vector);
  /** insert an element ( with a RVNGBinary parameter ) */
  void insertElement(const char *psName, const RVNGPropertyList &xPropList,
                     const RVNGBinaryData &data);
  /** insert a sequence of character */
  void characters(const RVNGString &sCharacters) {
    if (!m_output) return;
    m_output->insertText(sCharacters);
  }
private:
  /// copy constructor (undefined)
  GraphicExporter(GraphicExporter const &);
  /// operator= (undefined)
  GraphicExporter operator=(GraphicExporter const &);
  /** the interface output */
  RVNGDrawingInterface *m_output;
};

}

MWAWDocument::Confidence MWAWDocument::isFileFormatSupported(RVNGInputStream *input,  MWAWDocument::Type &type, Kind &kind)
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
    case MWAW_T_CLARISWORKS:
    case MWAW_T_DOCMAKER:
    case MWAW_T_EDOC:
    case MWAW_T_FULLWRITE:
    case MWAW_T_GREATWORKS:
    case MWAW_T_HANMACWORDJ:
    case MWAW_T_HANMACWORDK:
    case MWAW_T_LIGHTWAYTEXT:
    case MWAW_T_MACDOC:
    case MWAW_T_MACWRITE:
    case MWAW_T_MACWRITEPRO:
    case MWAW_T_MARINERWRITE:
    case MWAW_T_MINDWRITE:
    case MWAW_T_MORE:
    case MWAW_T_MICROSOFTWORD:
    case MWAW_T_MICROSOFTWORKS:
    case MWAW_T_NISUSWRITER:
    case MWAW_T_TEACHTEXT:
    case MWAW_T_TEXEDIT:
    case MWAW_T_WRITENOW:
    case MWAW_T_WRITERPLUS:
    case MWAW_T_ZWRITE:
      confidence = MWAW_C_EXCELLENT;
      break;
    case MWAW_T_FRAMEMAKER:
    case MWAW_T_MACDRAW:
    case MWAW_T_MACPAINT:
    case MWAW_T_PAGEMAKER:
    case MWAW_T_READYSETGO:
    case MWAW_T_RAGTIME:
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
  } catch (...) {
    type = MWAW_T_UNKNOWN;
    kind = MWAW_K_UNKNOWN;
    return MWAW_C_NONE;
  }
}

MWAWDocument::Result MWAWDocument::parse(RVNGInputStream *input, RVNGTextInterface *documentInterface, char const */*password*/)
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

    shared_ptr<MWAWParser> parser=MWAWDocumentInternal::getParserFromHeader(ip, rsrcParser, header.get());
    if (!parser) return MWAW_R_UNKNOWN_ERROR;
    parser->parse(documentInterface);
  } catch (libmwaw::FileException) {
    MWAW_DEBUG_MSG(("File exception trapped\n"));
    error = MWAW_R_FILE_ACCESS_ERROR;
  } catch (libmwaw::ParseException) {
    MWAW_DEBUG_MSG(("Parse exception trapped\n"));
    error = MWAW_R_PARSE_ERROR;
  } catch (...) {
    //fixme: too generic
    MWAW_DEBUG_MSG(("Unknown exception trapped\n"));
    error = MWAW_R_UNKNOWN_ERROR;
  }

  return error;
}

bool MWAWDocument::decodeGraphic(RVNGBinaryData const &binary, RVNGDrawingInterface *paintInterface)
{
  if (!paintInterface || !binary.size()) {
    MWAW_DEBUG_MSG(("MWAWDocument::decodeGraphic: called with no data or no converter\n"));
    return false;
  }
  MWAWDocumentInternal::GraphicExporter tmpHandler(paintInterface);
  try {
    if (!tmpHandler.checkData(binary) || !tmpHandler.readData(binary)) return false;
  } catch(...) {
    MWAW_DEBUG_MSG(("MWAWDocument::decodeGraphic: unknown error\n"));
    return false;
  }
  return true;
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

      ip->seek(0, RVNG_SEEK_SET);
      ip->setReadInverted(false);
    } else if (!ip->hasResourceFork())
      return 0L;

    listHeaders = MWAWHeader::constructHeader(ip, rsrcParser);
    size_t numHeaders = listHeaders.size();
    if (numHeaders==0) return 0L;

    for (size_t i = 0; i < numHeaders; i++) {
      if (!MWAWDocumentInternal::checkBasicMacHeader(ip, rsrcParser, listHeaders[i], strict))
        continue;
      return new MWAWHeader(listHeaders[i]);
    }
  } catch (libmwaw::FileException) {
    MWAW_DEBUG_MSG(("File exception trapped\n"));
  } catch (libmwaw::ParseException) {
    MWAW_DEBUG_MSG(("Parse exception trapped\n"));
  } catch (...) {
    //fixme: too generic
    MWAW_DEBUG_MSG(("Unknown exception trapped\n"));
  }
  return 0L;
}

/** Factory wrapper to construct a parser corresponding to an header */
shared_ptr<MWAWParser> getParserFromHeader(MWAWInputStreamPtr &input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header)
{
  shared_ptr<MWAWParser> parser;
  if (!header)
    return parser;
  try {
    switch(header->getType()) {
    case MWAWDocument::MWAW_T_ACTA:
      parser.reset(new ACParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_BEAGLEWORKS:
      parser.reset(new BWParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_CLARISWORKS:
      parser.reset(new CWParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_DOCMAKER:
      parser.reset(new DMParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_EDOC:
      parser.reset(new EDParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_FULLWRITE:
      parser.reset(new FWParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_GREATWORKS:
      parser.reset(new GWParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_HANMACWORDJ:
      parser.reset(new HMWJParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_HANMACWORDK:
      parser.reset(new HMWKParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_LIGHTWAYTEXT:
      parser.reset(new LWParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_MACDOC:
      parser.reset(new MCDParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_MACWRITE:
      parser.reset(new MWParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_MACWRITEPRO:
      parser.reset(new MWProParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_MARINERWRITE:
      parser.reset(new MRWParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_MINDWRITE:
      parser.reset(new MDWParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_MORE:
      parser.reset(new MORParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_MICROSOFTWORD:
      if (header->getMajorVersion()==1)
        parser.reset(new MSW1Parser(input, rsrcParser, header));
      else
        parser.reset(new MSWParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_MICROSOFTWORKS:
      if (header->getMajorVersion() < 100)
        parser.reset(new MSK3Parser(input, rsrcParser, header));
      else
        parser.reset(new MSK4Parser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_NISUSWRITER:
      parser.reset(new NSParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_TEACHTEXT:
    case MWAWDocument::MWAW_T_TEXEDIT:
      parser.reset(new TTParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_WRITENOW:
      parser.reset(new WNParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_WRITERPLUS:
      parser.reset(new WPParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWAW_T_ZWRITE:
      parser.reset(new ZWParser(input, rsrcParser, header));
      break;

    case MWAWDocument::MWAW_T_FRAMEMAKER:
    case MWAWDocument::MWAW_T_MACDRAW:
    case MWAWDocument::MWAW_T_MACPAINT:
    case MWAWDocument::MWAW_T_PAGEMAKER:
    case MWAWDocument::MWAW_T_READYSETGO:
    case MWAWDocument::MWAW_T_RAGTIME:
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
  } catch(...) {
  }
  return parser;
}

/** Wrapper to check a basic header of a mac file */
bool checkBasicMacHeader(MWAWInputStreamPtr &input, MWAWRSRCParserPtr rsrcParser, MWAWHeader &header, bool strict)
{
  try {
    shared_ptr<MWAWParser> parser=getParserFromHeader(input, rsrcParser, &header);
    if (!parser)
      return false;
    return parser->checkHeader(&header, strict);
  } catch(...) {
  }

  return false;
}

////////////////////////////////////////////////////////////
// GraphicExporter implementation
////////////////////////////////////////////////////////////
void GraphicExporter::insertElement(const char *psName)
{
  if (!m_output) return;
  if (!psName) {
    MWAW_DEBUG_MSG(("GraphicExporter::insertElement: called without any name\n"));
    return;
  }

  if (strcmp(psName,"CloseListElement")==0)
    m_output->closeListElement();
  else if (strcmp(psName,"CloseOrderedListLevel")==0)
    m_output->closeOrderedListLevel();
  else if (strcmp(psName,"CloseParagraph")==0)
    m_output->closeParagraph();
  else if (strcmp(psName,"CloseSpan")==0)
    m_output->closeSpan();
  else if (strcmp(psName,"CloseUnorderedListLevel")==0)
    m_output->closeUnorderedListLevel();

  else if (strcmp(psName,"Document")==0)
    m_output->endDocument();
  else if (strcmp(psName,"EndPage")==0)
    m_output->endPage();
  else if (strcmp(psName,"EndLayer")==0)
    m_output->endLayer();
  else if (strcmp(psName,"EndEmbeddedGraphics")==0)
    m_output->endEmbeddedGraphics();
  else if (strcmp(psName,"EndTextObject")==0)
    m_output->endTextObject();

  else if (strcmp(psName,"InsertTab")==0)
    m_output->insertTab();
  else if (strcmp(psName,"InsertSpace")==0)
    m_output->insertSpace();
  else if (strcmp(psName,"InsertLineBreak")==0)
    m_output->insertLineBreak();
  else {
    MWAW_DEBUG_MSG(("GraphicExporter::insertElement: called with unexpected name %s\n", psName));
  }
}

void GraphicExporter::insertElement(const char *psName, const RVNGPropertyList &propList)
{
  if (!m_output) return;
  if (!psName) {
    MWAW_DEBUG_MSG(("GraphicExporter::insertElement: called without any name\n"));
    return;
  }

  if (strcmp(psName,"DrawRectangle")==0)
    m_output->drawRectangle(propList);
  else if (strcmp(psName,"DrawEllipse")==0)
    m_output->drawEllipse(propList);

  else if (strcmp(psName,"InsertField")==0) {
    if (propList["libmwaw:type"])
      m_output->insertField(propList["libmwaw:type"]->getStr(), propList);
    else {
      MWAW_DEBUG_MSG(("GraphicExporter::insertElement: can not find field type\n"));
    }
  }

  else if (strcmp(psName,"OpenOrderedListLevel")==0)
    m_output->openOrderedListLevel(propList);
  else if (strcmp(psName,"OpenSpan")==0)
    m_output->openSpan(propList);
  else if (strcmp(psName,"OpenUnorderedListLevel")==0)
    m_output->openUnorderedListLevel(propList);

  else if (strcmp(psName,"SetMetaData")==0)
    m_output->setDocumentMetaData(propList);

  else if (strcmp(psName,"StartDocument")==0)
    m_output->startDocument(propList);
  else if (strcmp(psName,"StartPage")==0)
    m_output->startPage(propList);
  else if (strcmp(psName,"StartLayer")==0)
    m_output->startLayer(propList);
  else if (strcmp(psName,"StartEmbeddedGraphics")==0)
    m_output->startEmbeddedGraphics(propList);

  else {
    MWAW_DEBUG_MSG(("GraphicExporter::insertElement: called with unexpected name %s\n", psName));
  }
}

void GraphicExporter::insertElement(const char *psName, const RVNGPropertyList &propList,
                                    const RVNGPropertyListVector &vector)
{
  if (!m_output) return;
  if (!psName) {
    MWAW_DEBUG_MSG(("GraphicExporter::insertElement: called without any name\n"));
    return;
  }
  if (strcmp(psName,"DrawPath")==0 || strcmp(psName,"DrawPolygon")==0 || strcmp(psName,"DrawPolyline")==0) {
#ifdef DEBUG
    if (!RVNGPropertyList::Iter(propList).last()) {
      MWAW_DEBUG_MSG(("GraphicExporter::insertElement: Polyline, Polygon, Path called with propList, ignored\n"));
    }
#endif
    if (strcmp(psName,"DrawPolygon")==0)
      m_output->drawPolygon(vector);
    else if (strcmp(psName,"DrawPolyline")==0)
      m_output->drawPolyline(vector);
    else
      m_output->drawPath(vector);
  }
  if (strcmp(psName,"StartTextObject")==0)
    m_output->startTextObject(propList, vector);
  else if (strcmp(psName,"OpenListElement")==0)
    m_output->openListElement(propList, vector);
  else if (strcmp(psName,"OpenParagraph")==0)
    m_output->openParagraph(propList, vector);
  else if (strcmp(psName,"SetStyle")==0)
    m_output->setStyle(propList, vector);
  else {
    MWAW_DEBUG_MSG(("GraphicExporter::insertElement: called with unexpected name %s\n", psName));
  }
}

void GraphicExporter::insertElement(const char *psName, const RVNGPropertyList &propList,
                                    const RVNGBinaryData &data)
{
  if (!m_output) return;
  if (!psName) {
    MWAW_DEBUG_MSG(("GraphicExporter::insertElement: called without any name\n"));
    return;
  }
  if (strcmp(psName,"DrawGraphicObject")==0) {
    m_output->drawGraphicObject(propList, data);
    return;
  }
  MWAW_DEBUG_MSG(("GraphicExporter::insertElement: called with unexpected name %s\n", psName));
}

}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
