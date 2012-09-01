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

#ifndef NS_MWAW_PARSER
#  define NS_MWAW_PARSER

#include <string>
#include <vector>

#include "MWAWPageSpan.hxx"

#include "MWAWPosition.hxx"

#include "MWAWContentListener.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MWAWParser.hxx"

class MWAWEntry;

typedef class MWAWContentListener NSContentListener;
typedef shared_ptr<NSContentListener> NSContentListenerPtr;
class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

namespace NSParserInternal
{
struct RecursifData;
struct State;
class SubDocument;
}

/** a position in a Nisus Writer file */
struct NSPosition {
  NSPosition() : m_paragraph(0), m_word(0), m_char(0) {
  }
  //! operator<<: prints data in form "XxYxZ"
  friend std::ostream &operator<< (std::ostream &o, NSPosition const &pos);

  //! a small compare operator
  int cmp(NSPosition const &p2) const {
    if (m_paragraph < p2.m_paragraph) return -1;
    if (m_paragraph > p2.m_paragraph) return 1;
    if (m_word < p2.m_word) return -1;
    if (m_word > p2.m_word) return 1;
    if (m_char < p2.m_char) return -1;
    if (m_char > p2.m_char) return 1;
    return 0;
  }
  /** the paragraph */
  int m_paragraph;
  /** the word */
  int m_word;
  /** the character position */
  int m_char;

  //! a comparaison structure used to sort the position
  struct Compare {
    //! comparaison function
    bool operator()(NSPosition const &p1, NSPosition const &p2) const {
      return p1.cmp(p2) < 0;
    }
  };
};

/** \brief the main class to read a Nisus Writer file
 */
class NSParser : public MWAWParser
{
  friend class NSParserInternal::SubDocument;

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

  //! adds a new page
  void newPage(int number);

  //! finds the different objects zones
  bool createZones();

protected:
  //! read the print info zone ( id=128 )
  bool readPrintInfo(MWAWEntry const &entry);
  //! read the CPRC info zone ( id=128 ), an unknown zone
  bool readCPRC(MWAWEntry const &entry);
  //! read the PGLY info zone ( id=128 ), an unknown zone
  bool readPGLY(MWAWEntry const &entry);

  //! read the list of fonts
  bool readFontsList(MWAWEntry const &entry);
  //! read a list of strings
  bool readStringsList(MWAWEntry const &entry, std::vector<std::string> &list);

  /** read the header/footer main entry */
  bool readHFHeader(MWAWEntry const &entry);
  /** read the footnote main entry */
  bool readFootnoteHeader(MWAWEntry const &entry);

  /** read a text entry
     \note the main text is in the data fork, but the footnote/header footer is in a ??TX rsrc. This function must disappear in the final release */
  bool sendText(int zoneId);

  //! read the FTAB/STYL resource: a font format list ?
  bool readFonts(MWAWEntry const &entry);
  //! read the FRMT resource: a list of filepos + id of fonts ?
  bool readFRMT(MWAWEntry const &entry, int zoneId);
  //! read the INFO info zone, an unknown zone of size 0x23a: to doo
  bool readINFO(MWAWEntry const &entry);
  //! read the PGRA resource: a unknown number
  bool readPGRA(MWAWEntry const &entry);
  //! read the RULE resource: a list of paragraphs
  bool readParagraphs(MWAWEntry const &entry, int zoneId);

  //! read the CNTR resource: a list of  ?
  bool readCNTR(MWAWEntry const &entry, int zoneId);
  //! read the PICD resource: a list of pict ?
  bool readPICD(MWAWEntry const &entry, int zoneId);
  //! read the PLAC resource: a list of picture placements ?
  bool readPLAC(MWAWEntry const &entry);

  //! read a recursif list: only in v4 ?
  bool readRecursifList(MWAWEntry const &entry,
                        NSParserInternal::RecursifData &data,
                        int levl=0);
  //! parse the MRK7 resource
  bool readMark(NSParserInternal::RecursifData const &data);
  //! parse the DSPL resource: numbering definition
  bool readNumberingDef(NSParserInternal::RecursifData const &data);
  //! parse the DPND resource: numbering reset ( one by zone )
  bool readNumberingReset(MWAWEntry const &entry, int zoneId);

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
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
