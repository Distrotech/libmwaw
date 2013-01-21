/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* libmwaw
 * Copyright (C) 2002-2005 William Lachance (wrlach@gmail.com)
 * Copyright (C) 2005 Net Integration Technologies (http://www.net-itech.com)
 * Copyright (C) 2002 Marc Maurer (uwog@uwog.net)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
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

#ifndef HTML_DOCUMENT_GENERATOR_H
#define HTML_DOCUMENT_GENERATOR_H

#include <ostream>
#include <sstream>
#include <libwpd/libwpd.h>

#include "mwaw2html_internal.h"

namespace HtmlDocumentGeneratorInternal
{
struct State;
}

class HtmlDocumentGenerator : public WPXDocumentInterface
{
public:
	HtmlDocumentGenerator(char const *fName=0);
	virtual ~HtmlDocumentGenerator();

	virtual void setDocumentMetaData(const WPXPropertyList &propList);

	virtual void startDocument();
	virtual void endDocument();

	virtual void definePageStyle(const WPXPropertyList &) {}
	virtual void openPageSpan(const WPXPropertyList &propList);
	virtual void closePageSpan();
	virtual void openHeader(const WPXPropertyList &propList);
	virtual void closeHeader();
	virtual void openFooter(const WPXPropertyList &propList);
	virtual void closeFooter();

	virtual void defineSectionStyle(const WPXPropertyList &, const WPXPropertyListVector &) {}
	virtual void openSection(const WPXPropertyList & /* propList */, const WPXPropertyListVector & /* columns */) {}
	virtual void closeSection() {}

	virtual void defineParagraphStyle(const WPXPropertyList &, const WPXPropertyListVector &) {}
	virtual void openParagraph(const WPXPropertyList &propList, const WPXPropertyListVector &tabStops);
	virtual void closeParagraph();

	virtual void defineCharacterStyle(const WPXPropertyList &) {}
	virtual void openSpan(const WPXPropertyList &propList);
	virtual void closeSpan();

	virtual void insertTab();
	virtual void insertText(const WPXString &text);
	virtual void insertSpace();
	virtual void insertLineBreak();
	virtual void insertField(const WPXString &/*type*/, const WPXPropertyList &/*propList*/) {}

	virtual void defineOrderedListLevel(const WPXPropertyList &propList);
	virtual void defineUnorderedListLevel(const WPXPropertyList &propList);
	virtual void openOrderedListLevel(const WPXPropertyList &propList);
	virtual void openUnorderedListLevel(const WPXPropertyList &propList);
	virtual void closeOrderedListLevel();
	virtual void closeUnorderedListLevel();
	virtual void openListElement(const WPXPropertyList &propList, const WPXPropertyListVector &tabStops);
	virtual void closeListElement();

	virtual void openFootnote(const WPXPropertyList &propList);
	virtual void closeFootnote();
	virtual void openEndnote(const WPXPropertyList &propList);
	virtual void closeEndnote();
	virtual void openComment(const WPXPropertyList &propList);
	virtual void closeComment();
	virtual void openTextBox(const WPXPropertyList &propList);
	virtual void closeTextBox();

	virtual void openTable(const WPXPropertyList &propList, const WPXPropertyListVector &columns);
	virtual void openTableRow(const WPXPropertyList &propList);
	virtual void closeTableRow();
	virtual void openTableCell(const WPXPropertyList &propList);
	virtual void closeTableCell();
	virtual void insertCoveredTableCell(const WPXPropertyList & /* propList */) {}
	virtual void closeTable();

	virtual void openFrame(const WPXPropertyList & /* propList */) {}
	virtual void closeFrame() {}

	virtual void insertBinaryObject(const WPXPropertyList & /* propList */, const WPXBinaryData & /* data */) {}
	virtual void insertEquation(const WPXPropertyList & /* propList */, const WPXString & /* data */) {}

private:
	shared_ptr<HtmlDocumentGeneratorInternal::State> m_state;

	shared_ptr<std::ostream> m_output;
	// Unimplemented to prevent compiler from creating crasher ones
	HtmlDocumentGenerator(const HtmlDocumentGenerator &);
	HtmlDocumentGenerator &operator=(const HtmlDocumentGenerator &);
};

#endif /* HTML_DOCUMENT_GENERATOR_H */
/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
