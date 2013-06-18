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

#include "MWAWHeader.hxx"
#include "MWAWParser.hxx"
#include "MWAWRSRCParser.hxx"

#include <libmwaw/libmwaw.hxx>

#include "ACParser.hxx"
#include "CWParser.hxx"
#include "DMParser.hxx"
#include "EDParser.hxx"
#include "FWParser.hxx"
#include "GWParser.hxx"
#include "HMWJParser.hxx"
#include "HMWKParser.hxx"
#include "LWParser.hxx"
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

namespace MWAWDocumentInternal
{
shared_ptr<MWAWParser> getParserFromHeader(MWAWInputStreamPtr &input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
MWAWHeader *getHeader(MWAWInputStreamPtr &input, MWAWRSRCParserPtr rsrcParser, bool strict);
bool checkBasicMacHeader(MWAWInputStreamPtr &input, MWAWRSRCParserPtr rsrcParser, MWAWHeader &header, bool strict);
}

/**
\mainpage libmwaw documentation
This document contains both the libmwaw API specification and the normal libmwaw
documentation.

\section api_docs libmwaw API documentation

The external libmwaw API is provided by the MWAWDocument class. This
class with MWAWPropertyHandler, combined with the libwpd's
WPXDocumentInterface class are the only three classes that will be of
interest for the application programmer using libmwaw.

- The class MWAWPropertyHandler is a small class which can decode the
internal encoded format used to export odg's data.

- libmwaw can add some non-standart properties in some property list of WPXDocumentInterface,
  these properties can be ignored or used:
  - Properties "fo:border", "fo:border-left", "fo:border-right", "fo:border-top", "fo:border-bottom"
    can appear in the property list of OpenParagraph.
  - Encoded object with "libwpd:mimetype"="image/mwaw-odg" can be given to insertBinaryObject.
  In this case, MWAWPropertyHandler can be used to decode the data which is an encoded odg picture.

\section lib_docs libmwaw documentation
If you are interrested in the structure of libmwaw itself, this whole document
would be a good starting point for exploring the interals of libmwaw. Mind that
this document is a work-in-progress, and will most likely not cover libmwaw for
the full 100%.

\warning When compiled with -DDEBUG_WITH__FILES, code is added to store the results of the parsing in different files: one file by Ole parts ( or sometimes to reconstruct a part of file which is stored discontinuously ) and some files to store the read pictures. These files are created in the current repository, therefore it is recommended to launch the tests in a empty repository...
*/

/**
Analyzes the content of an input stream to see if it can be parsed
\param input The input stream
\param type The document type ( filled if the file is supported )
\param kind The document kind ( filled if the file is supported )
\return A confidence value which represents the likelyhood that the content from
the input stream can be parsed
*/
MWAWConfidence MWAWDocument::isFileFormatSupported(WPXInputStream *input,  MWAWDocument::DocumentType &type, DocumentKind &kind)
{
  type = UNKNOWN;
  kind = K_UNKNOWN;

  if (!input) {
    MWAW_DEBUG_MSG(("MWAWDocument::isFileFormatSupported(): no input\n"));
    return MWAW_CONFIDENCE_NONE;
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
      return MWAW_CONFIDENCE_NONE;
    type = (MWAWDocument::DocumentType)header->getType();
    kind = (MWAWDocument::DocumentKind)header->getKind();
    MWAWConfidence confidence = MWAW_CONFIDENCE_NONE;

    switch (type) {
    case ACT:
    case CW:
    case DM:
    case ED:
    case FULLW:
    case GW:
    case HMAC:
    case HMACJ:
    case LWTEXT:
    case MARIW:
    case MINDW:
    case MORE:
    case MSWORD:
    case MSWORKS:
    case MW:
    case MWPRO:
    case NISUSW:
    case TEACH:
    case TEDIT:
    case WNOW:
    case WPLUS:
    case ZWRT:
      confidence = MWAW_CONFIDENCE_GOOD;
      break;
    case BW:
    case FRM:
    case MACD:
    case MOCKP:
    case PAGEMK:
    case RSG:
    case RGTIME:
    case XP:
    case RESERVED1:
    case RESERVED2:
    case RESERVED3:
    case RESERVED4:
    case RESERVED5:
    case RESERVED6:
    case RESERVED7:
    case RESERVED8:
    case RESERVED9:
    case UNKNOWN:
    default:
      break;
    }

    return confidence;
  } catch (...) {
    type = UNKNOWN;
    kind = K_UNKNOWN;
    return MWAW_CONFIDENCE_NONE;
  }
}

/**
Parses the input stream content. It will make callbacks to the functions provided by a
WPXDocumentInterface class implementation when needed. This is often commonly called the
'main parsing routine'.
\param input The input stream
\param documentInterface A MWAWListener implementation
*/
MWAWResult MWAWDocument::parse(WPXInputStream *input, WPXDocumentInterface *documentInterface)
{
  if (!input)
    return MWAW_UNKNOWN_ERROR;
  MWAWResult error = MWAW_OK;

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

    if (!header.get()) return MWAW_UNKNOWN_ERROR;

    shared_ptr<MWAWParser> parser=MWAWDocumentInternal::getParserFromHeader(ip, rsrcParser, header.get());
    if (!parser) return MWAW_UNKNOWN_ERROR;
    parser->parse(documentInterface);
  } catch (libmwaw::FileException) {
    MWAW_DEBUG_MSG(("File exception trapped\n"));
    error = MWAW_FILE_ACCESS_ERROR;
  } catch (libmwaw::ParseException) {
    MWAW_DEBUG_MSG(("Parse exception trapped\n"));
    error = MWAW_PARSE_ERROR;
  } catch (...) {
    //fixme: too generic
    MWAW_DEBUG_MSG(("Unknown exception trapped\n"));
    error = MWAW_UNKNOWN_ERROR;
  }

  return error;
}

/** structure used to hide some functions needed by MWAWDocument (basically to check if the file is really supported). */
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
      if (ip->size() < 10) return 0L;

      ip->seek(0, WPX_SEEK_SET);
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
    case MWAWDocument::ACT:
      parser.reset(new ACParser(input, rsrcParser, header));
      break;
    case MWAWDocument::CW:
      parser.reset(new CWParser(input, rsrcParser, header));
      break;
    case MWAWDocument::DM:
      parser.reset(new DMParser(input, rsrcParser, header));
      break;
    case MWAWDocument::ED:
      parser.reset(new EDParser(input, rsrcParser, header));
      break;
    case MWAWDocument::FULLW:
      parser.reset(new FWParser(input, rsrcParser, header));
      break;
    case MWAWDocument::GW:
      parser.reset(new GWParser(input, rsrcParser, header));
      break;
    case MWAWDocument::HMAC:
      parser.reset(new HMWKParser(input, rsrcParser, header));
      break;
    case MWAWDocument::HMACJ:
      parser.reset(new HMWJParser(input, rsrcParser, header));
      break;
    case MWAWDocument::LWTEXT:
      parser.reset(new LWParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MARIW:
      parser.reset(new MRWParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MINDW:
      parser.reset(new MDWParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MORE:
      parser.reset(new MORParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MSWORD:
      if (header->getMajorVersion()==1)
        parser.reset(new MSW1Parser(input, rsrcParser, header));
      else
        parser.reset(new MSWParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MSWORKS:
      if (header->getMajorVersion() < 100)
        parser.reset(new MSK3Parser(input, rsrcParser, header));
      else
        parser.reset(new MSK4Parser(input, rsrcParser, header));
      break;
    case MWAWDocument::MW:
      parser.reset(new MWParser(input, rsrcParser, header));
      break;
    case MWAWDocument::MWPRO:
      parser.reset(new MWProParser(input, rsrcParser, header));
      break;
    case MWAWDocument::NISUSW:
      parser.reset(new NSParser(input, rsrcParser, header));
      break;
    case MWAWDocument::TEACH:
    case MWAWDocument::TEDIT:
      parser.reset(new TTParser(input, rsrcParser, header));
      break;
    case MWAWDocument::WNOW:
      parser.reset(new WNParser(input, rsrcParser, header));
      break;
    case MWAWDocument::WPLUS:
      parser.reset(new WPParser(input, rsrcParser, header));
      break;
    case MWAWDocument::ZWRT:
      parser.reset(new ZWParser(input, rsrcParser, header));
      break;

    case MWAWDocument::BW:
    case MWAWDocument::FRM:
    case MWAWDocument::MACD:
    case MWAWDocument::MOCKP:
    case MWAWDocument::PAGEMK:
    case MWAWDocument::RSG:
    case MWAWDocument::RGTIME:
    case MWAWDocument::XP:

    case MWAWDocument::RESERVED1:
    case MWAWDocument::RESERVED2:
    case MWAWDocument::RESERVED3:
    case MWAWDocument::RESERVED4:
    case MWAWDocument::RESERVED5:
    case MWAWDocument::RESERVED6:
    case MWAWDocument::RESERVED7:
    case MWAWDocument::RESERVED8:
    case MWAWDocument::RESERVED9:
    case MWAWDocument::UNKNOWN:
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
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
