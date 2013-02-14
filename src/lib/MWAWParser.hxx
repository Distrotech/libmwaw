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

#ifndef MWAW_PARSER_H
#define MWAW_PARSER_H

#include <ostream>
#include <string>
#include <vector>

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"
#include "MWAWRSRCParser.hxx"

#include "MWAWEntry.hxx"
#include "MWAWHeader.hxx"

class WPXDocumentInterface;

class MWAWContentListener;
typedef shared_ptr<MWAWContentListener> MWAWContentListenerPtr;
class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

/** virtual class which defines the ancestor of all main zone parser
 *
 * \note this class is generally associated with a IMWAWTextParser
 */
class MWAWParser
{
public:
  //! virtual destructor
  virtual ~MWAWParser();
  //! virtual function used to parse the input
  virtual void parse(WPXDocumentInterface *documentInterface) = 0;

  //! returns the works version
  int version() const {
    return m_version;
  }
  //! sets the works version
  void setVersion(int vers) {
    m_version = vers;
  }

protected:
  //! constructor (protected)
  MWAWParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);

  //! returns the header
  MWAWHeader *getHeader() {
    return m_header;
  }
  //! returns the actual input
  MWAWInputStreamPtr &getInput() {
    return m_input;
  }
  //! returns the rsrc parser
  MWAWRSRCParserPtr &getRSRCParser() {
    return m_rsrcParser;
  }
  //! returns the listener
  MWAWContentListenerPtr &getListener() {
    return m_listener;
  }
  //! set the listener
  void setListener(MWAWContentListenerPtr &listener);
  //! reset the listener
  void resetListener();
  //! a DebugFile used to write what we recognize when we parse the document
  libmwaw::DebugFile &ascii() {
    return m_asciiFile;
  }
  //! Debugging: change the default ascii file
  void setAsciiName(char const *name) {
    m_asciiName = name;
  }
  //! return the ascii file name
  std::string const &asciiName() const {
    return m_asciiName;
  }

protected:
  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

private:
  //! private copy constructor: forbidden
  MWAWParser(const MWAWParser &);
  //! private operator=: forbidden
  MWAWParser &operator=(const MWAWParser &);

  //! the actual version
  int m_version;

  //! the input
  MWAWInputStreamPtr m_input;
  //! the header
  MWAWHeader *m_header;

  //! the resource parser
  MWAWRSRCParserPtr m_rsrcParser;
  //! the listener
  MWAWContentListenerPtr m_listener;

  //! the debug file
  libmwaw::DebugFile m_asciiFile;
  //! the debug file name
  std::string m_asciiName;

};


#endif /* MWAWPARSER_H */
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
