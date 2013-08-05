/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
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

#ifndef RAWLISTENERIMPL_H
#define RAWLISTENERIMPL_H
#include <libwpd/libwpd.h>
#include <stack>

#ifndef __GNUC__
#define __attribute__(x)
#endif

using namespace std;

enum ListenerCallback
{
    LC_START_DOCUMENT = 0,
    LC_OPEN_PAGE_SPAN,
    LC_OPEN_HEADER_FOOTER,
    LC_OPEN_PARAGRAPH,
    LC_OPEN_SPAN,
    LC_OPEN_SECTION,
    LC_OPEN_ORDERED_LIST_LEVEL,
    LC_OPEN_UNORDERED_LIST_LEVEL,
    LC_OPEN_LIST_ELEMENT,
    LC_OPEN_FOOTNOTE,
    LC_OPEN_ENDNOTE,
    LC_OPEN_TABLE,
    LC_OPEN_TABLE_ROW,
    LC_OPEN_TABLE_CELL,
    LC_OPEN_COMMENT,
    LC_OPEN_TEXT_BOX,
    LC_OPEN_FRAME
};

class RawDocumentGenerator : public WPXDocumentInterface
{
public:
	RawDocumentGenerator(bool printCallgraphScore);
	virtual ~RawDocumentGenerator();

	virtual void setDocumentMetaData(const WPXPropertyList &propList);

	virtual void startDocument();
	virtual void endDocument();

	virtual void definePageStyle(const WPXPropertyList &propList);
	virtual void openPageSpan(const WPXPropertyList &propList);
	virtual void closePageSpan();
	virtual void openHeader(const WPXPropertyList &propList);
	virtual void closeHeader();
	virtual void openFooter(const WPXPropertyList &propList);
	virtual void closeFooter();

	virtual void defineParagraphStyle(const WPXPropertyList &propList, const WPXPropertyListVector &tabStops);
	virtual void openParagraph(const WPXPropertyList &propList, const WPXPropertyListVector &tabStops);
	virtual void closeParagraph();

	virtual void defineCharacterStyle(const WPXPropertyList &propList);
	virtual void openSpan(const WPXPropertyList &propList);
	virtual void closeSpan();

	virtual void defineSectionStyle(const WPXPropertyList &propList, const WPXPropertyListVector &columns);
	virtual void openSection(const WPXPropertyList &propList, const WPXPropertyListVector &columns);
	virtual void closeSection();

	virtual void insertTab();
	virtual void insertSpace();
	virtual void insertText(const WPXString &text);
	virtual void insertLineBreak();
	virtual void insertField(const WPXString &type, const WPXPropertyList &propList);

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
	virtual void insertCoveredTableCell(const WPXPropertyList &propList);
	virtual void closeTable();

	virtual void openFrame(const WPXPropertyList &propList);
	virtual void closeFrame();

	virtual void insertBinaryObject(const WPXPropertyList &propList, const WPXBinaryData &data);
	virtual void insertEquation(const WPXPropertyList &propList, const WPXString &data);

private:
	int m_indent;
	int m_callbackMisses;
	bool m_atLeastOneCallback;
	bool m_printCallgraphScore;
	stack<ListenerCallback> m_callStack;

	void __indentUp()
	{
		m_indent++;
	}
	void __indentDown()
	{
		if (m_indent > 0) m_indent--;
	}

	void __iprintf(const char *format, ...) __attribute__((format(printf,2,3)));
	void __iuprintf(const char *format, ...) __attribute__((format(printf,2,3)));
	void __idprintf(const char *format, ...) __attribute__((format(printf,2,3)));

};

#endif /* RAWLISTENERIMPL_H */
/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
