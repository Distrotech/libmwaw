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

#ifndef NS_PARSER
#  define NS_PARSER

#include <string>
#include <vector>

#include <libwpd/libwpd.h>

#include "MWAWContentListener.hxx"
#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"
#include "MWAWPageSpan.hxx"

#include "NSStruct.hxx"

#include "MWAWParser.hxx"

typedef class MWAWContentListener NSContentListener;
typedef shared_ptr<NSContentListener> NSContentListenerPtr;
class MWAWEntry;
class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;
class MWAWPosition;

namespace NSParserInternal
{
struct State;
}

class NSGraph;
class NSText;

/** \brief the main class to read a Nisus Writer file
 */
class NSParser : public MWAWParser
{
  friend struct NSStruct::RecursifData;
  friend class NSGraph;
  friend class NSText;
public:
  //! constructor
  NSParser(MWAWInputStreamPtr input, MWAWRSRCParserPtr rsrcParser, MWAWHeader *header);
  //! destructor
  virtual ~NSParser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  // the main parse function
  void parse(WPXDocumentInterface *documentInterface);

protected:
  //! inits all internal variables
  void init();

  //! sets the listener in this class and in the helper classes
  void setListener(NSContentListenerPtr listen);

  //! creates the listener which will be associated to the document
  void createDocument(WPXDocumentInterface *documentInterface);

  //! returns the page height, ie. paper size less margin (in inches)
  float pageHeight() const;
  //! returns the page width, ie. paper size less margin (in inches)
  float pageWidth() const;
  //! returns the page left top point ( in inches)
  Vec2f getPageLeftTop() const;
  //! returns the columns information
  void getColumnInfo(int &numColumns, float &colSep) const;
  //! returns the footnote information
  void getFootnoteInfo(NSStruct::FootnoteInfo &fInfo) const;

  //! adds a new page
  void newPage(int number);

  // variable access

  //! returns the date format corresponding to a variable id or ""
  std::string getDateFormat(NSStruct::ZoneType zoneId, int vId) const;

  //! returns the fieldtype or a string corresponding to a variable
  bool getReferenceData(NSStruct::ZoneType zoneId, int vId,
                        MWAWContentListener::FieldType &fType,
                        std::string &content,
                        std::vector<int> &number) const;

  // interface with the graph parser

  //! try to send a picture
  bool sendPicture(int pictId, MWAWPosition const &pictPos,
                   WPXPropertyList extras = WPXPropertyList());

protected:
  //! finds the different objects zones
  bool createZones();

  //! read the print info zone ( id=128 )
  bool readPrintInfo(MWAWEntry const &entry);
  //! read the CPRC info zone ( id=128 ), an unknown zone
  bool readCPRC(MWAWEntry const &entry);
  //! read the PGLY info zone ( id=128 )
  bool readPageLimit(MWAWEntry const &entry);

  //! read a list of strings
  bool readStringsList(MWAWEntry const &entry, std::vector<std::string> &list, bool simpleList);

  //! read the INFO info zone, an unknown zone of size 0x23a: to doo
  bool readINFO(MWAWEntry const &entry);

  //! parse the MRK7 resource
  bool readReference(NSStruct::RecursifData const &data);
  //! parse the DSPL/VARI/VRS resource: numbering definition, variable or variable ?
  bool readVariable(NSStruct::RecursifData const &data);
  //! read the CNTR resource: a list of  version controler ?
  bool readCNTR(MWAWEntry const &entry, int zoneId);
  //! parse the DPND resource: numbering reset ( one by zone ) : related to CNTR and VRS ?
  bool readNumberingReset(MWAWEntry const &entry, int zoneId);

  //! parse the SGP1 resource: a unknown resource
  bool readSGP1(NSStruct::RecursifData const &data);
  //! parse the ABBR resource: a list of abreviation?
  bool readABBR(MWAWEntry const &entry);
  //! parse the FTA2 resource: a list of ? find in v6 document
  bool readFTA2(MWAWEntry const &entry);
  //! parse the FnSc resource: a unknown resource, find in v6 document
  bool readFnSc(MWAWEntry const &entry);

  //! return the input input
  MWAWInputStreamPtr rsrcInput();

  //! a DebugFile used to write what we recognize when we parse the document in rsrc
  libmwaw::DebugFile &rsrcAscii();

  //
  // data
  //
  //! the listener
  NSContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<NSParserInternal::State> m_state;

  //! the actual document size
  MWAWPageSpan m_pageSpan;

  //! the graph parser
  shared_ptr<NSGraph> m_graphParser;

  //! the text parser
  shared_ptr<NSText> m_textParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
