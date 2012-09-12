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

#include "MWAWPageSpan.hxx"

#include "MWAWContentListener.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

#include "NSStruct.hxx"

class MWAWEntry;

typedef class MWAWContentListener NSContentListener;
typedef shared_ptr<NSContentListener> NSContentListenerPtr;
class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

namespace NSParserInternal
{
struct State;
}

class NSText;

/** \brief the main class to read a Nisus Writer file
 */
class NSParser : public MWAWParser
{
  friend struct NSStruct::RecursifData;
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

  //! finds the different objects zones
  bool createZones();

protected:
  //! read the print info zone ( id=128 )
  bool readPrintInfo(MWAWEntry const &entry);
  //! read the CPRC info zone ( id=128 ), an unknown zone
  bool readCPRC(MWAWEntry const &entry);
  //! read the PGLY info zone ( id=128 )
  bool readPageLimit(MWAWEntry const &entry);

  //! read a list of strings
  bool readStringsList(MWAWEntry const &entry, std::vector<std::string> &list);

  //! read the INFO info zone, an unknown zone of size 0x23a: to doo
  bool readINFO(MWAWEntry const &entry);
  //! read the PGRA resource: a unknown number
  bool readPGRA(MWAWEntry const &entry);

  //! read the CNTR resource: a list of  version controler
  bool readCNTR(MWAWEntry const &entry, int zoneId);
  //! read the PICD resource: a list of pict ?
  bool readPICD(MWAWEntry const &entry, int zoneId);
  //! read the PLAC resource: a list of picture placements ?
  bool readPLAC(MWAWEntry const &entry);

  //! parse the MRK7 resource
  bool readReference(NSStruct::RecursifData const &data);
  //! parse the DSPL/VARI/VRS resource: numbering definition, variable or variable ?
  bool readVariable(NSStruct::RecursifData const &data);
  //! parse the DPND resource: numbering reset ( one by zone ) : related to CNTR and VRS ?
  bool readNumberingReset(MWAWEntry const &entry, int zoneId);

  //! parse the PLDT resource: a unknown resource
  bool readPLDT(NSStruct::RecursifData const &data);
  //! parse the SGP1 resource: a unknown resource
  bool readSGP1(NSStruct::RecursifData const &data);

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

  //! the text parser
  shared_ptr<NSText> m_textParser;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
