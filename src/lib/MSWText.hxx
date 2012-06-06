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

/*
 * Parser to Microsoft Word text document
 *
 */
#ifndef MSW_MWAW_TEXT
#  define MSW_MWAW_TEXT

#include <map>
#include <string>
#include <vector>

#include "libmwaw_internal.hxx"

#include "MWAWPosition.hxx"

#include "MWAWEntry.hxx"

#include "MWAWDebug.hxx"
#include "MWAWInputStream.hxx"

#include "MSWTextStyles.hxx"

typedef class MWAWContentListener MSWContentListener;
typedef shared_ptr<MSWContentListener> MSWContentListenerPtr;

class MWAWFontConverter;
typedef shared_ptr<MWAWFontConverter> MWAWFontConverterPtr;

namespace MSWTextInternal
{
struct Paragraph;
struct Section;
struct State;
}

struct MSWEntry;
class MSWParser;
class MSWTextStyles;

/** \brief the main class to read the text part of Microsoft Word file */
class MSWText
{
  friend class MSWParser;
  friend class MSWTextStyles;
public:
  //! Internal: the plc
  struct PLC {
    enum Type { Line, Section, Page, Font, Paragraph, Footnote, FootnoteDef, Field, Object, HeaderFooter, TextPosition };
    PLC(Type type, int id=0) : m_type(type), m_id(id), m_extra("") {
    }
    //! operator<<
    friend std::ostream &operator<<(std::ostream &o, PLC const &plc);

    //! the plc type
    Type m_type;
    //! the identificator
    int m_id;
    //! some extra data
    std::string m_extra;
  };
public:
  //! constructor
  MSWText(MWAWInputStreamPtr ip, MSWParser &parser, MWAWFontConverterPtr &convertissor);
  //! destructor
  virtual ~MSWText();

  /** returns the file version */
  int version() const;

  /** returns the number of pages */
  int numPages() const;


protected:
  //! sets the listener in this class and in the helper classes
  void setListener(MSWContentListenerPtr listen);

  //! send a main zone
  bool sendMainText();

  //! send a text zone
  bool sendText(MWAWEntry const &textEntry, bool mainZone);

  //! send paragraph properties
  void setProperty(MSWTextInternal::Paragraph const &para,
                   MSWTextStyles::Font &actFont, bool recursive=false);

  //! send section properties
  void setProperty(MSWTextInternal::Section const &sec,
                   MSWTextStyles::Font &actFont, bool recursive=false);

  //! finds the different zones given defaut text zone and textlength
  bool createZones(long bot, long (&textLength)[3]);

  //! read the text structure(some paragraph style+some text position?)
  bool readTextStruct(MSWEntry &entry);

  //! read the page limit ?
  bool readPageBreak(MSWEntry &entry);

  //! read the line info(zone)
  bool readLineInfo(MSWEntry &entry);

  //! read the text section ?
  bool readSection(MSWEntry &entry);

  //! read the char/paragraph plc list
  bool readPLCList(MSWEntry &entry);

  //! read the field data
  bool readFields(MSWEntry &entry, std::vector<long> const &fieldPos);

  //! send a field note to a listener
  bool sendFieldComment(int id);

  //! read the footnote pos in text + val
  bool readFootnotesPos(MSWEntry &entry, std::vector<long> const &noteDef);

  //! read the footnote data
  bool readFootnotesData(MSWEntry &entry);

  //! send a note to a listener
  bool sendFootnote(int id);

  //! read the font names
  bool readFontNames(MSWEntry &entry);

  //! try to read the styles zone
  bool readStyles(MSWEntry &entry);

  //! sends the data which have not yet been sent to the listener
  void flushExtra();

  // interface with MSWTextStyles

  //! returns the main text length
  long getMainTextLength() const;

  //! returns the text correspondance zone ( textpos, plc )
  std::multimap<long, MSWText::PLC> &getTextPLCMap();

  //
  // low level
  //

  //! try to read a paragraph
  bool readParagraph(MSWTextInternal::Paragraph &para, int dataSz=-1);

  //! try to read the styles hierachy
  bool readStylesHierarchy(MSWEntry &entry, int N, std::vector<int> &previous);
  //! try to read the styles names and fill the number of "named" styles...
  bool readStylesNames(MSWEntry const &zone, int N, int &Nnamed);
  //! try to read the styles fonts
  bool readStylesFont(MSWEntry &zone, int N, std::vector<int> const &previous,
                      std::vector<int> const &order);
  //! try to read the styles fonts
  bool readStylesParagraph(MSWEntry &zone, int N, std::vector<int> const &previous,
                           std::vector<int> const &order);
  //! try to reorder the styles to find a good order
  std::vector<int> orderStyles(std::vector<int> const &previous);

  //! try to read the section data
  bool readSection(MSWTextInternal::Section &section, long pos);

  //! read the char/paragraph plc : type=0: char, type=1: parag
  bool readPLC(MSWEntry &entry, int type);

  //! read a zone which consists in a list of int
  bool readLongZone(MSWEntry &entry, int sz, std::vector<long> &list);

  //! finds the style which must be used for each style
  void updateTextEntryStyle();

  //! returns the debug file
  libmwaw::DebugFile &ascii() {
    return m_asciiFile;
  }

private:
  MSWText(MSWText const &orig);
  MSWText &operator=(MSWText const &orig);

protected:
  //
  // data
  //
  //! the input
  MWAWInputStreamPtr m_input;

  //! the listener
  MSWContentListenerPtr m_listener;

  //! a convertissor tools
  MWAWFontConverterPtr m_convertissor;

  //! the state
  shared_ptr<MSWTextInternal::State> m_state;

  //! the style manager
  shared_ptr<MSWTextStyles> m_stylesManager;

  //! the main parser;
  MSWParser *m_mainParser;

  //! the debug file
  libmwaw::DebugFile &m_asciiFile;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
