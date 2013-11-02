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

#ifndef MWAW_LISTENER_H
#define MWAW_LISTENER_H

#include <vector>

#include <librevenge/librevenge.h>

#include "libmwaw_internal.hxx"

/** This class contains a virtual interface to all listener

 \note actually contains mainly code for adding text */
class MWAWListener
{
public:
  virtual ~MWAWListener() {}

  /** the different break type */
  enum BreakType { PageBreak=0, SoftPageBreak, ColumnBreak };

  /** returns true if a document is opened */
  virtual bool isDocumentStarted() const =0;
  /** returns true if we can add text data */
  virtual bool canWriteText() const =0;
  /** returns true if a subdocument is open  */
  virtual bool isSubDocumentOpened(libmwaw::SubDocumentType &subdocType) const = 0;

  // ------ text data -----------
  //! adds a basic character, ..
  virtual void insertChar(uint8_t character)=0;
  /** insert a character using the font converter to find the utf8
      character */
  virtual void insertCharacter(unsigned char c)=0;
  /** insert a character using the font converter to find the utf8
      character and if needed, input to read extra character.

      \return the number of extra character read
   */
  virtual int insertCharacter(unsigned char c, MWAWInputStreamPtr &input, long endPos=-1)=0;
  /** adds an unicode character.
   *  By convention if \a character=0xfffd(undef), no character is added */
  virtual void insertUnicode(uint32_t character)=0;
  //! adds a unicode string
  virtual void insertUnicodeString(RVNGString const &str)=0;

  //! adds a tab
  virtual void insertTab()=0;
  //! adds an end of line ( by default an hard one)
  virtual void insertEOL(bool softBreak=false)=0;

  // ------ text format -----------
  //! sets the font
  virtual void setFont(MWAWFont const &font)=0;
  //! returns the actual font
  virtual MWAWFont const &getFont() const=0;

  // ------ paragraph format -----------
  //! returns true if a paragraph or a list is opened
  virtual bool isParagraphOpened() const=0;
  //! sets the paragraph
  virtual void setParagraph(MWAWParagraph const &paragraph)=0;
  //! returns the actual paragraph
  virtual MWAWParagraph const &getParagraph() const=0;

  // ------- fields ----------------
  //! adds a field type
  virtual void insertField(MWAWField const &field)=0;

  // ------- section ---------------
  /** returns true if we can add open a section, add page break, ... */
  virtual bool canOpenSectionAddBreak() const =0;
  //! returns true if a section is opened
  virtual bool isSectionOpened() const=0;
  //! returns the actual section
  virtual MWAWSection const &getSection() const=0;
  //! open a section if possible
  virtual bool openSection(MWAWSection const &section)=0;
  //! close a section
  virtual bool closeSection()=0;
  //! inserts a break type: ColumBreak, PageBreak, ..
  virtual void insertBreak(BreakType breakType)=0;

};

#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
