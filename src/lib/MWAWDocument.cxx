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

#include "MWAWDocument.hxx"

#include "CWParser.hxx"
#include "DMParser.hxx"
#include "EDParser.hxx"
#include "FWParser.hxx"
#include "HMWJParser.hxx"
#include "HMWKParser.hxx"
#include "LWParser.hxx"
#include "MDWParser.hxx"
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
  MWAWConfidence confidence = MWAW_CONFIDENCE_NONE;
  type = UNKNOWN;
  kind = K_UNKNOWN;
  if (!input) {
    MWAW_DEBUG_MSG(("MWAWDocument::isFileFormatSupported(): no input\n"));
    return MWAW_CONFIDENCE_NONE;
  }

  MWAW_DEBUG_MSG(("MWAWDocument::isFileFormatSupported()\n"));
  MWAWInputStreamPtr ip(new MWAWInputStream(input, false, true));
  MWAWInputStreamPtr rsrc=ip->getResourceForkStream();
  shared_ptr<MWAWRSRCParser> rsrcParser;
  if (rsrc)
    rsrcParser.reset(new MWAWRSRCParser(rsrc));
  shared_ptr<MWAWHeader> header;
#ifdef OOO
  header.reset(MWAWDocumentInternal::getHeader(ip, rsrcParser, true));
#else
  header.reset(MWAWDocumentInternal::getHeader(ip, rsrcParser, false));
#endif

  if (!header.get())
    return MWAW_CONFIDENCE_NONE;
  type = (MWAWDocument::DocumentType)header->getType();
  kind = (MWAWDocument::DocumentKind)header->getKind();

  switch (type) {
  case CW:
    confidence = MWAW_CONFIDENCE_EXCELLENT;
    break;
  case DM:
    confidence = MWAW_CONFIDENCE_GOOD;
    break;
  case ED:
    confidence = MWAW_CONFIDENCE_GOOD;
    break;
  case FULLW:
    confidence = MWAW_CONFIDENCE_GOOD;
    break;
  case HMAC:
    confidence = MWAW_CONFIDENCE_GOOD;
    break;
  case HMACJ:
#ifdef DEBUG
    confidence = MWAW_CONFIDENCE_GOOD;
#endif
    break;
  case LWTEXT:
    confidence = MWAW_CONFIDENCE_GOOD;
    break;
  case MARIW:
    confidence = MWAW_CONFIDENCE_GOOD;
    break;
  case MINDW:
    confidence = MWAW_CONFIDENCE_GOOD;
    break;
  case MSWORD:
    confidence = MWAW_CONFIDENCE_GOOD;
    break;
  case MSWORKS:
    confidence = MWAW_CONFIDENCE_GOOD;
    break;
  case MW:
    confidence = MWAW_CONFIDENCE_EXCELLENT;
    break;
  case MWPRO:
    confidence = MWAW_CONFIDENCE_GOOD;
    break;
  case NISUSW:
    confidence = MWAW_CONFIDENCE_GOOD;
    break;
  case TEACH:
    confidence = MWAW_CONFIDENCE_GOOD;
    break;
  case TEDIT:
    confidence = MWAW_CONFIDENCE_GOOD;
    break;
  case WNOW:
    confidence = MWAW_CONFIDENCE_EXCELLENT;
    break;
  case WPLUS:
    confidence = MWAW_CONFIDENCE_GOOD;
    break;
  case ZWRT:
    confidence = MWAW_CONFIDENCE_GOOD;
    break;
  case ACT:
  case UNKNOWN:
  default:
    break;
  }

  return confidence;
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

    switch (header->getType()) {
    case CW: {
      CWParser parser(ip, rsrcParser, header.get());
      parser.parse(documentInterface);
      break;
    }
    case DM: {
      DMParser parser(ip, rsrcParser, header.get());
      parser.parse(documentInterface);
      break;
    }
    case ED: {
      EDParser parser(ip, rsrcParser, header.get());
      parser.parse(documentInterface);
      break;
    }
    case FULLW: {
      FWParser parser(ip, rsrcParser, header.get());
      parser.parse(documentInterface);
      break;
    }
    case HMAC: {
      HMWKParser parser(ip, rsrcParser, header.get());
      parser.parse(documentInterface);
      break;
    }
    case HMACJ: {
      HMWJParser parser(ip, rsrcParser, header.get());
      parser.parse(documentInterface);
      break;
    }
    case LWTEXT: {
      LWParser parser(ip, rsrcParser, header.get());
      parser.parse(documentInterface);
      break;
    }
    case MARIW: {
      MRWParser parser (ip, rsrcParser, header.get());
      parser.parse(documentInterface);
      break;
    }
    case MINDW: {
      MDWParser parser (ip, rsrcParser, header.get());
      parser.parse(documentInterface);
      break;
    }
    case MSWORD: {
      if (header->getMajorVersion()==1) {
        MSW1Parser parser (ip, rsrcParser, header.get());
        parser.parse(documentInterface);
      } else {
        MSWParser parser (ip, rsrcParser, header.get());
        parser.parse(documentInterface);
      }
      break;
    }
    case MSWORKS: {
      if (header->getMajorVersion() < 100) {
        MSK3Parser parser (ip, rsrcParser, header.get());
        parser.parse(documentInterface);
      } else {
        MSK4Parser parser (ip, rsrcParser, header.get());
        parser.parse(documentInterface);
      }
      break;
    }
    case MW: {
      MWParser parser (ip, rsrcParser, header.get());
      parser.parse(documentInterface);
      break;
    }
    case MWPRO: {
      MWProParser parser (ip, rsrcParser, header.get());
      parser.parse(documentInterface);
      break;
    }
    case NISUSW: {
      NSParser parser (ip, rsrcParser, header.get());
      parser.parse(documentInterface);
      break;
    }
    case TEACH:
    case TEDIT: {
      TTParser parser (ip, rsrcParser, header.get());
      parser.parse(documentInterface);
      break;
    }
    case WNOW: {
      WNParser parser (ip, rsrcParser, header.get());
      parser.parse(documentInterface);
      break;
    }
    case WPLUS: {
      WPParser parser (ip, rsrcParser, header.get());
      parser.parse(documentInterface);
      break;
    }
    case ZWRT: {
      ZWParser parser (ip, rsrcParser, header.get());
      parser.parse(documentInterface);
      break;
    }
    case ACT:
    case UNKNOWN:
    default:
      break;
    }
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
    /** avoid very short file */
    if (!ip.get()) return 0L;

    if (ip->hasDataFork()) {
      if (ip->seek(10, WPX_SEEK_SET) != 0) return 0L;

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

/** Wrapper to check a basic header of a mac file */
bool checkBasicMacHeader(MWAWInputStreamPtr &input, MWAWRSRCParserPtr rsrcParser, MWAWHeader &header, bool strict)
{
  try {
    switch(header.getType()) {
    case MWAWDocument::CW: {
      CWParser parser(input, rsrcParser, &header);
      return parser.checkHeader(&header, strict);
    }
    case MWAWDocument::DM: {
      DMParser parser(input, rsrcParser, &header);
      return parser.checkHeader(&header, strict);
    }
    case MWAWDocument::ED: {
      EDParser parser(input, rsrcParser, &header);
      return parser.checkHeader(&header, strict);
    }
    case MWAWDocument::FULLW: {
      FWParser parser(input, rsrcParser, &header);
      return parser.checkHeader(&header, strict);
    }
    case MWAWDocument::HMAC: {
      HMWKParser parser(input, rsrcParser, &header);
      return parser.checkHeader(&header, strict);
    }
    case MWAWDocument::HMACJ: {
      HMWJParser parser(input, rsrcParser, &header);
      return parser.checkHeader(&header, strict);
    }
    case MWAWDocument::LWTEXT: {
      LWParser parser(input, rsrcParser, &header);
      return parser.checkHeader(&header, strict);
    }
    case MWAWDocument::MARIW: {
      MRWParser parser(input, rsrcParser, &header);
      return parser.checkHeader(&header, strict);
    }
    case MWAWDocument::MINDW: {
      MDWParser parser(input, rsrcParser, &header);
      return parser.checkHeader(&header, strict);
    }
    case MWAWDocument::MSWORD:
      if (header.getMajorVersion()==1) {
        MSW1Parser parser (input, rsrcParser, &header);
        return parser.checkHeader(&header, strict);
      } else {
        MSWParser parser(input, rsrcParser, &header);
        return parser.checkHeader(&header, strict);
      }
    case MWAWDocument::MSWORKS: {
      if (header.getMajorVersion() < 100) {
        MSK3Parser parser(input, rsrcParser, &header);
        return parser.checkHeader(&header, strict);
      } else {
        MSK4Parser parser(input, rsrcParser, &header);
        return parser.checkHeader(&header, strict);
      }
    }
    case MWAWDocument::MW: {
      MWParser parser(input, rsrcParser, &header);
      return parser.checkHeader(&header, strict);
    }
    case MWAWDocument::MWPRO: {
      MWProParser parser(input, rsrcParser, &header);
      return parser.checkHeader(&header, strict);
    }
    case MWAWDocument::NISUSW: {
      NSParser parser(input, rsrcParser, &header);
      return parser.checkHeader(&header, strict);
    }
    case MWAWDocument::TEACH:
    case MWAWDocument::TEDIT: {
      TTParser parser(input, rsrcParser, &header);
      return parser.checkHeader(&header, strict);
    }
    case MWAWDocument::WNOW: {
      WNParser parser(input, rsrcParser, &header);
      return parser.checkHeader(&header, strict);
    }
    case MWAWDocument::WPLUS: {
      WPParser parser(input, rsrcParser, &header);
      return parser.checkHeader(&header, strict);
    }
    case MWAWDocument::ZWRT: {
      ZWParser parser(input, rsrcParser, &header);
      return parser.checkHeader(&header, strict);
    }
    case MWAWDocument::ACT:
    case MWAWDocument::UNKNOWN:
    default:
      break;
    }
  } catch(...) {
  }

  return false;
}
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
