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
 * For further information visit http://libwps.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

#ifndef MSK4_PARSER
#  define MSK4_PARSER

#include <list>
#include <vector>

#include "MWAWEntry.hxx"
#include "MWAWSubDocument.hxx"

#include "MWAWInputStream.hxx"

class WPXDocumentInterface;
class MWAWHeader;

typedef class MWAWContentListener MSKContentListener;
typedef shared_ptr<MSKContentListener> MSKContentListenerPtr;

class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

namespace MSK4ParserInternal
{
struct State;
class SubDocument;
}

class MSK4Zone;

/** \brief the main class to read a MS Works document v4
 *
 * This class is associated with a MSK4Parser which reads:
 * the main Ole zones MN0, MacWorks/QHdr, MacWorks/QFtr, MacWorks/QFootnotes
 * and which parses MacWorks/QFrm\<number\>.
 * It also uses an MWAWOleParser in order to find  pictures
 * in the other Ole zones.
 */
class MSK4Parser
{
  friend class MSK4ParserInternal::SubDocument;
  friend class MSK4Zone;
public:
  //! construtor
  MSK4Parser(MWAWInputStreamPtr input, MWAWHeader * header);
  //! destructor
  virtual ~MSK4Parser();

  //! checks if the document header is correct (or not)
  bool checkHeader(MWAWHeader *header, bool strict=false);

  //! the main parse function, called with the documentInterface
  virtual void parse(WPXDocumentInterface *documentInterface);

protected:
  //! sets the listener in this class and in the helper classes
  void setListener(MSKContentListenerPtr listen);

  /** finds the principal ole zone: Ole pictures and MN0,
      then tries to find the main structures. Finally, parses the document */
  bool createStructures();

  //! tries to parse the ole zones which have not yet been parsed
  void flushExtra();

  //
  // subdocument helper
  //
  /** creates a subdocument corresponding to a footnote (indicated by id)
   *
   * \note if \a id < 0 meaning that the text corresponding to the note was not
   * found, an empty footnote will be created */
  void sendFootNote(int id);

  /** send the frame text */
  void sendFrameText(MWAWEntry const &entry, std::string const &frame);

  //! send an OLE zone
  void sendOLE(int id, MWAWPosition const &pos, WPXPropertyList frameExtras);

private:
  MSK4Parser(MSK4Parser const &orig);
  MSK4Parser &operator=(MSK4Parser const &orig);

protected:
  //! the main input
  MWAWInputStreamPtr m_input;

  //! the header
  MWAWHeader *m_header;

  //! the state
  shared_ptr<MSK4ParserInternal::State> m_state;

  //! the listener
  MSKContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
