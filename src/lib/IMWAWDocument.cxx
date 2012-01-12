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

#include "IMWAWHeader.hxx"
#include "IMWAWParser.hxx"

#include "IMWAWDocument.hxx"

#include "CWParser.hxx"
#include "MWParser.hxx"
#include "WNParser.hxx"
#include "WPParser.hxx"

namespace IMWAWDocumentInternal
{
IMWAWHeader *getHeader(TMWAWInputStreamPtr &input, bool strict);
bool checkBasicMacHeader(TMWAWInputStreamPtr &input, IMWAWHeader &header, bool strict);
}

/**
\mainpage libmwaw documentation
This document contains both the libmwaw API specification and the normal libmwaw
documentation.

\section api_docs libmwaw API documentation

The external libmwaw API is provided by the IMWAWDocument class. This
class with TMWAWPropertyHandler, combined with the libwpd's
WPXDocumentInterface class are the only three classes that will be of
interest for the application programmer using libmwaw.

- The class TMWAWPropertyHandler is a small class which can decode the
internal encoded format used to export odg's data.

- libmwaw can add some non-standart properties in some property list of WPXDocumentInterface,
  these properties can be ignored or used:
  - Properties "fo:border", "fo:border-left", "fo:border-right", "fo:border-top", "fo:border-bottom"
    can appear in the property list of OpenParagraph.
  - Encoded object with "libwpd:mimetype"="image/mwaw-odg" can be given to insertBinaryObject.
  In this case, TMWAWPropertyHandler can be used to decode the data which is an encoded odg picture.

\section lib_docs libmwaw documentation
If you are interrested in the structure of libmwaw itself, this whole document
would be a good starting point for exploring the interals of libmwaw. Mind that
this document is a work-in-progress, and will most likely not cover libmwaw for
the full 100%.

\warning When compiled with -DDEBUG_WITH__FILES, code is added to store the results of the parsing in different files: one file by Ole parts and some files to store the read pictures. These files are created in the current repository, therefore it is recommended to launch the tests in a empty repository...
*/

/**
Analyzes the content of an input stream to see if it can be parsed
\param input The input stream
\return A confidence value which represents the likelyhood that the content from
the input stream can be parsed
*/
IMWAWConfidence IMWAWDocument::isFileFormatSupported(WPXInputStream *input,  IMWAWDocument::DocumentType &type, DocumentKind &kind)
{
  IMWAWConfidence confidence = IMWAW_CONFIDENCE_NONE;
  if (!input) {
    MWAW_DEBUG_MSG(("IMWAWDocument::isFileFormatSupported(): no input\n"));
    return IMWAW_CONFIDENCE_NONE;
  }

  MWAW_DEBUG_MSG(("IMWAWDocument::isFileFormatSupported()\n"));
  TMWAWInputStreamPtr ip(new TMWAWInputStream(input, false));
  shared_ptr<IMWAWHeader> header;
#ifdef OOO
  header.reset(IMWAWDocumentInternal::getHeader(ip, true));
#else
  header.reset(IMWAWDocumentInternal::getHeader(ip, false));
#endif

  if (!header.get()) return IMWAW_CONFIDENCE_NONE;
  type = (IMWAWDocument::DocumentType)header->getType();
  kind = (IMWAWDocument::DocumentKind)header->getKind();

  switch (header->getType()) {
  case MW:
    confidence = IMWAW_CONFIDENCE_EXCELLENT;
    break;
  case CW:
    confidence = IMWAW_CONFIDENCE_EXCELLENT;
    break;
  case WNOW:
    confidence = IMWAW_CONFIDENCE_EXCELLENT;
    break;
  case WPLUS:
    confidence = IMWAW_CONFIDENCE_GOOD;
    break;
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
\param documentInterface A IMWAWListener implementation
*/
IMWAWResult IMWAWDocument::parse(WPXInputStream *input, WPXDocumentInterface *documentInterface)
{
  IMWAWResult error = IMWAW_OK;

  try {
    TMWAWInputStreamPtr ip(new TMWAWInputStream(input, false));
    shared_ptr<IMWAWHeader> header(IMWAWDocumentInternal::getHeader(ip, false));

    if (!header.get()) return IMWAW_UNKNOWN_ERROR;

    switch (header->getType()) {
    case MW: {
      MWParser parser (ip, header.get());
      parser.parse(documentInterface);
      break;
    }
    case WPLUS: {
      WPParser parser (ip, header.get());
      parser.parse(documentInterface);
      break;
    }
    case CW: {
      CWParser parser(ip, header.get());
      parser.parse(documentInterface);
      break;
    }
    case WNOW: {
      WNParser parser (ip, header.get());
      parser.parse(documentInterface);
      break;
    }
    default:
      break;
    }
  } catch (libmwaw_libwpd::FileException)	{
    MWAW_DEBUG_MSG(("File exception trapped\n"));
    error = IMWAW_FILE_ACCESS_ERROR;
  } catch (libmwaw_libwpd::ParseException) {
    MWAW_DEBUG_MSG(("Parse exception trapped\n"));
    error = IMWAW_PARSE_ERROR;
  } catch (...) {
    //fixme: too generic
    MWAW_DEBUG_MSG(("Unknown exception trapped\n"));
    error = IMWAW_UNKNOWN_ERROR;
  }

  return error;
}


namespace IMWAWDocumentInternal
{
/** return the header corresponding to an input. Or 0L if no input are found */
IMWAWHeader *getHeader(TMWAWInputStreamPtr &ip, bool strict)
{
  IMWAWHeader *header = 0L;
  try {
    /** avoid very short file */
    if (!ip.get() || ip->seek(10, WPX_SEEK_SET) != 0) return 0L;

    ip->seek(0, WPX_SEEK_SET);
    ip->setReadInverted(false);

    header = IMWAWHeader::constructHeader(ip);
    if (!header) return 0L;

    bool ok = true;
    if (strict || header->getType() == IMWAWDocument::MW)
      ok = IMWAWDocumentInternal::checkBasicMacHeader(ip, *header, strict);
    if (!ok) {
      delete header;
      return 0L;
    }

    return header;
  } catch (libmwaw_libwpd::FileException)	{
    MWAW_DEBUG_MSG(("File exception trapped\n"));
  } catch (libmwaw_libwpd::ParseException) {
    MWAW_DEBUG_MSG(("Parse exception trapped\n"));
  } catch (...) {
    //fixme: too generic
    MWAW_DEBUG_MSG(("Unknown exception trapped\n"));
  }

  if (header) delete header;
  return 0L;
}

/** Wrapper to check a basic header of a mac file */
bool checkBasicMacHeader(TMWAWInputStreamPtr &input, IMWAWHeader &header, bool strict)
{
  switch(header.getType()) {
  case IMWAWDocument::CW: {
    CWParser parser(input, &header);
    return parser.checkHeader(&header, strict);
  }
  case IMWAWDocument::MW: {
    MWParser parser(input, &header);
    return parser.checkHeader(&header, strict);
  }
  case IMWAWDocument::WNOW: {
    WNParser parser(input, &header);
    return parser.checkHeader(&header, strict);
  }
  case IMWAWDocument::WPLUS: {
    WPParser parser(input, &header);
    return parser.checkHeader(&header, strict);
  }
  default:
    break;
  }

  return true;
}
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
