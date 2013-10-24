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
 * Copyright (C) 2006 Ariya Hidayat (ariya@kde.org)
 * Copyright (C) 2004 Marc Oude Kotte (marc@solcon.nl)
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

#ifndef __MWAW_GRAPHIC_INTERFACE_HXX__
#define __MWAW_GRAPHIC_INTERFACE_HXX__

#include <libwpd/libwpd.h>
#include <libmwaw_internal.hxx>

class MWAWPropertyHandlerEncoder;

namespace MWAWGraphicInterfaceInternal
{
struct State;
}
/** main class used to define the final interface to generate picture

	\note: this class is clearly inspired to libwpg/WPXPaintInterface version 0.3.0
*/
class MWAWGraphicInterface
{
public:
  /// constructor
  MWAWGraphicInterface();
  /// destructor
  ~MWAWGraphicInterface();
  /// return the final graphic
  bool getBinaryResult(WPXBinaryData &result, std::string &mimeType);

  // none of the other callback functions will be called before this function is called
  void startDocument(const ::WPXPropertyList &propList);

  // none of the other callback functions will be called after this function is called
  void endDocument();

  /**
  Called when all document metadata should be set. This is always the first callback made.
  \param propList Property list for the metadata. May contain:
  \li \c dc:creator
  \li \c dc:language The document's keywords
  \li \c dc:publisher The document's publisher
  \li \c dc:source
  \li \c dc:subject The document's subject
  \li \c dc:type The document's type
  \li \c dcterms:available Date when the document was completed
  \li \c dcterms:issued: Date of the version of the document
  \li \c libwpd:abstract Abstract of the document's contents
  \li \c libwpd:account Account
  \li \c libwpd:address Address
  \li \c libwpd:attachments
  \li \c libwpd:authorization
  \li \c libwpd:bill-to
  \li \c libwpd:blind-copy
  \li \c libwpd:carbon-copy
  \li \c libwpd:checked-by
  \li \c libwpd:client
  \li \c libwpd:comments
  \li \c libwpd:department
  \li \c libwpd:descriptive-name The descriptive name for the document
  \li \c libwpd:descriptive-type The descriptive type for the document
  \li \c libwpd:destination
  \li \c libwpd:disposition
  \li \c libwpd:division
  \li \c libwpd:document-number
  \li \c libwpd:editor
  \li \c libwpd:forward-to
  \li \c libwpd:group
  \li \c libwpd:mail-stop
  \li \c libwpd:matter
  \li \c libwpd:office
  \li \c libwpd:owner
  \li \c libwpd:project
  \li \c libwpd:purpose
  \li \c libwpd:received-from
  \li \c libwpd:recorded-by
  \li \c libwpd:recorded-date Date when the document was recorded
  \li \c libwpd:reference
  \li \c libwpd:revision-notes
  \li \c libwpd:revision-number
  \li \c libwpd:section
  \li \c libwpd:security
  \li \c libwpd:status
  \li \c libwpd:telephone-number
  \li \c libwpd:version-notes
  \li \c libwpd:version-number
  \li \c meta:creation-date Document creation date
  \li \c meta:initial-creator The document's author
  \li \c meta:keyword The document's keywords
  \li \c
  */
  void setDocumentMetaData(const WPXPropertyList &propList);

  void startPage(const ::WPXPropertyList &propList);

  void endPage();

  void setStyle(const ::WPXPropertyList &propList, const ::WPXPropertyListVector &gradient);

  void startLayer(const ::WPXPropertyList &propList);

  void endLayer();

  void startEmbeddedGraphics(const ::WPXPropertyList &propList);

  void endEmbeddedGraphics();

  // Different primitive shapes
  void drawRectangle(const ::WPXPropertyList &propList);

  void drawEllipse(const ::WPXPropertyList &propList);

  void drawPolygon(const ::WPXPropertyListVector &vertices);

  void drawPolyline(const ::WPXPropertyListVector &vertices);

  void drawPath(const ::WPXPropertyListVector &path);

  // Embedded binary/raster data
  void drawGraphicObject(const ::WPXPropertyList &propList, const ::WPXBinaryData &binaryData);

  // Embedded text object
  void startTextObject(const ::WPXPropertyList &propList, const ::WPXPropertyListVector &path);
  void endTextObject();

  /**
  Called when a TAB character should be inserted
  */
  void insertTab();

  /**
  Called when an explicit space should be inserted
  */
  void insertSpace();

  /**
  Called when a string of text should be inserted
  \param text A textbuffer encoded as a UTF8 string
  */
  void insertText(const WPXString &text);

  /**
  Called when a line break should be inserted
  */
  void insertLineBreak();

  /**
  Called when a field should be inserted.
  \param type may include:
  \li \c text:page-number Current page number
  \li \c text:page-count Total # of pages in document
  \param propList Defines a set of properties for the field. May contain:
  \li \c style:num-format Type of page number (for page number)
  */
  void insertField(const WPXString &type, const WPXPropertyList &propList);

  /**
  Defines an ordered (enumerated) list level
  \param propList Defines a set of properties for the list. May contain:
  \li \c libwpd:id A unique integer identifier for the list
  \li \c libwpd:level The level of the list in the hierarchy
  \li \c style:num-format Type of list
  \li \c style:num-prefix Text that comes before the number in the list
  \li \c style:num-suffix Text that comes after the number in the list
  \li \c text:start-value The starting number of the list
  \li \c text:min-label-width The distance between the list label and the actual text, stored in inches
  \li \c text:space-before The indentation level of the lists, stored in inches
  */
  void defineOrderedListLevel(const WPXPropertyList &propList);
  /**
  Defines an unordered (unenumerated) list level
  \param propList Defines a set of properties for the list level. May contain:
  \li \c libwpd:id A unique integer identifier for the list
  \li \c libwpd:level The level of the list in the hierarchy
  \li \c text:bullet-char The string that should be used as a bullet
  \li \c text:min-label-width The distance between the bullet and the actual text, stored in inches
  \li \c text:space-before The indentation level of the lists, stored in inches
  */
  void defineUnorderedListLevel(const WPXPropertyList &propList);
  /**
  Called when a new ordered list level should be opened
  Argument defines a set of properties for the list. Must contain:
  \li \c libwpd:id A unique integer identifier for the list
  \li \c libwpd:level The level of the list in the hierarchy
  */
  void openOrderedListLevel(const WPXPropertyList &/*propList*/) {}

  /**
  Called when a new unordered list level should be opened
  Argument defines a set of properties for the list level. Must contain:
  \li \c libwpd:id A unique integer identifier for the list
  \li \c libwpd:level The level of the list in the hierarchy
  */
  void openUnorderedListLevel(const WPXPropertyList &/*propList*/) {}

  /**
  Called when an unordered list level should be closed
  */
  void closeOrderedListLevel() {}

  /**
  Called when an ununordered list level should be closed
  */
  void closeUnorderedListLevel() {}

  /**
  Called when a list element should be opened
  \param propList Property list for the paragraph. May contain:
  \li \c fo:text-align The justification of this paragraph (left, center, end, full, or justify)
  \li \c fo:margin-left The left indentation of this paragraph, in inches
  \li \c fo:margin-right The right indentation of this paragraph, in inches
  \li \c fo:margin-top The amount of extra spacing to be placed before the paragraph, in inches
  \li \c fo:margin-bottom The amount of extra spacing to be placed after the paragraph, in inches
  \li \c fo:text-indent The indentation of first line, in inches (difference relative to margin-left)
  \li \c fo:line-height The amount of spacing between lines, in number of lines (1.0 is single spacing)
  \li \c fo:break-before Whether this paragraph should be placed in a new column or page (the value is set to column or page if so)
  \param tabStops List of tabstop definitions for the paragraph. If the list is empty, default tabstop definition should be used. Each tab stop may contain:
  \li \c style:type Type of tab (left, right, center, or char)
  \li \c style:char Alingnment character for char aligned tabs
  \li \c style:leader-text The leader character
  \li \c style:position Position of the tab
  */
  void openListElement(const WPXPropertyList &propList, const WPXPropertyListVector &tabStops);

  /**
  Called when a list element should be closed
  */
  void closeListElement();

  /**
  Called when a new paragraph is opened. This (or openListElement) will always be called before any text or span is placed into the document.
  \param propList Property list for the paragraph. May contain:
  \li \c fo:text-align The justification of this paragraph (left, center, end, full, or justify)
  \li \c fo:margin-left The left indentation of this paragraph, in inches
  \li \c fo:margin-right The right indentation of this paragraph, in inches
  \li \c fo:margin-top The amount of extra spacing to be placed before the paragraph, in inches
  \li \c fo:margin-bottom The amount of extra spacing to be placed after the paragraph, in inches
  \li \c fo:text-indent The indentation of first line, in inches (difference relative to margin-left)
  \li \c fo:line-height The amount of spacing between lines, in number of lines (1.0 is single spacing)
  \li \c fo:break-before Whether this paragraph should be placed in a new column or page (the value is set to column or page if so)
  \param tabStops List of tabstop definitions for the paragraph. If the list is empty, default tabstop definition should be used. Each tab stop may contain:
  \li \c style:type Type of tab (left, right, center, or char)
  \li \c style:char Alingnment character for char aligned tabs
  \li \c style:leader-text The leader character
  \li \c style:position Position of the tab
  */
  void openParagraph(const WPXPropertyList &propList, const WPXPropertyListVector &tabStops);

  /**
  Called when a paragraph is closed.
  */
  void closeParagraph();

  /**
  Called when a text span is opened
  \param propList Property list for the span. May contain:
  \li \c fo:font-style Font style (italic or normal)
  \li \c fo:font-weight Font style (bold or normal)
  \li \c style:text-line-through-type (double or single, if present)
  \li \c style:text-underline-type (double or single, if present)
  \li \c style:text-outline (true or false)
  \li \c fo:font-variant (small-caps, if present)
  \li \c style:font-name The name of the font used in the span, a text string in ascii
  \li \c fo:font-size The size of the font used in the span, in points (72 points per inch)
  \li \c fo:color The color of the font used in the span (encoded in hex: \#RRGGBB)
  \li \c fo:background-color The background color of the text in the span (encoded in hex: \#RRGGBB)
  \li \c style:text-blinking Whether the text should blink (true or false)
  \li \c fo:text-shadow
  */
  void openSpan(const WPXPropertyList &propList);

  /**
  Called when a text span is closed
  */
  void closeSpan();

protected:
  //! the actual state
  shared_ptr<MWAWGraphicInterfaceInternal::State> m_state;
};

#endif

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
