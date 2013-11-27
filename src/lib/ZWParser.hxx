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

#ifndef ZW_PARSER
#  define ZW_PARSER

#include <string>
#include <vector>

#include <librevenge/librevenge.h>

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

namespace ZWParserInternal
{
class SubDocument;
struct State;
}

class ZWText;

/** a structure to store a field of a ZWrite file */
struct ZWField {
  //! constructor
  ZWField() : m_pos()
  {
  }
  //! returns the string corresponding to a field
  bool getString(MWAWInputStreamPtr &input, std::string &str) const;
  //! returns the boolean corresponding to a field ( T or F )
  bool getBool(MWAWInputStreamPtr &input, bool &val) const;
  //! returns the int corresponding to a field
  bool getInt(MWAWInputStreamPtr &input, int &val) const;
  //! returns the float corresponding to a field
  bool getFloat(MWAWInputStreamPtr &input, float &val) const;
  //! returns a list of int corresponding to a field
  bool getIntList(MWAWInputStreamPtr &input, std::vector<int> &val) const;

  //! returns a debug string corresponding to a field ( replacing \n by ##[0d], ...)
  bool getDebugString(MWAWInputStreamPtr &input, std::string &str) const;

  //! the field position in the rsrc data file
  MWAWEntry m_pos;
};

/** \brief the main class to read a ZWrite file
 */
class ZWParser : public MWAWParser
{
  friend class ZWParserInternal::SubDocument;
  friend class ZWText;
public:
  //! constructor
  ZWParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~ZWParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(librevenge::RVNGTextInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! creates the listener which will be associated to the document
  void createDocument(librevenge::RVNGTextInterface *documentInterface);

  //! returns the page left top point ( in inches)
  Vec2f getPageLeftTop() const;
  //! adds a new page
  void newPage(int number);

  // interface with the text parser

  //! try to send the header/footer
  bool sendHeaderFooter(bool header);

protected:
  //! finds the different objects zones
  bool createZones();

  //! read the bar state
  bool readBarState(MWAWEntry const &entry);
  //! read the html export pref
  bool readHTMLPref(MWAWEntry const &entry);
  //! read a PrintInfo block
  bool readPrintInfo(MWAWEntry const &entry);
  /** try to read a xml printinfo zone */
  bool readCPRT(MWAWEntry const &entry);
  //! read the section range block
  bool readSectionRange(MWAWEntry const &entry);
  //! read the window position
  bool readWindowPos(MWAWEntry const &entry);

  //! read a unknown block
  bool readUnknownZone(MWAWEntry const &entry);

  //! read a cursor position in a section?
  bool readCPos(MWAWEntry const &entry);
  //! read a section length ?
  bool readSLen(MWAWEntry const &entry);

  //! returns a list of field
  bool getFieldList(MWAWEntry const &entry, std::vector<ZWField> &list);

  //! return the input input
  MWAWInputStreamPtr rsrcInput();

  //! a DebugFile used to write what we recognize when we parse the document in rsrc
  libmwaw::DebugFile &rsrcAscii();

  //
  // data
  //

  //! the state
  shared_ptr<ZWParserInternal::State> m_state;

  //! the text parser
  shared_ptr<ZWText> m_textParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
