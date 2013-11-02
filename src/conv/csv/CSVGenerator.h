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

#ifndef CSV_GENERATOR_H
#define CSV_GENERATOR_H

#include <fstream>
#include <ostream>
#include <vector>

#include <librevenge/librevenge.h>

class CSVGenerator : public RVNGTextInterface
{
public:
	CSVGenerator(char const *fName=0);
	virtual ~CSVGenerator();

	virtual void setDocumentMetaData(const RVNGPropertyList &) {};

	virtual void startDocument() {}
	virtual void endDocument() {}

	virtual void definePageStyle(const RVNGPropertyList &) {}
	virtual void openPageSpan(const RVNGPropertyList & /* propList */) {}
	virtual void closePageSpan() {}
	virtual void openHeader(const RVNGPropertyList & /* propList */) {}
	virtual void closeHeader() {}
	virtual void openFooter(const RVNGPropertyList & /* propList */) {}
	virtual void closeFooter() {}

	virtual void defineSectionStyle(const RVNGPropertyList &, const RVNGPropertyListVector &) {}
	virtual void openSection(const RVNGPropertyList & /* propList */, const RVNGPropertyListVector & /* columns */) {}
	virtual void closeSection() {}

	virtual void defineParagraphStyle(const RVNGPropertyList &, const RVNGPropertyListVector &) {}
	virtual void openParagraph(const RVNGPropertyList & /* propList */, const RVNGPropertyListVector & /* tabStops */) {}
	virtual void closeParagraph() {}

	virtual void defineCharacterStyle(const RVNGPropertyList &) {}
	virtual void openSpan(const RVNGPropertyList & /* propList */) {}
	virtual void closeSpan() {}

	virtual void insertTab();
	virtual void insertText(const RVNGString &text);
	virtual void insertSpace();
	virtual void insertLineBreak();
	virtual void insertField(const RVNGString &/*type*/, const RVNGPropertyList &/*propList*/) {}

	virtual void defineOrderedListLevel(const RVNGPropertyList & /* propList */) {}
	virtual void defineUnorderedListLevel(const RVNGPropertyList & /* propList */) {}
	virtual void openOrderedListLevel(const RVNGPropertyList & /* propList */) {}
	virtual void openUnorderedListLevel(const RVNGPropertyList & /* propList */) {}
	virtual void closeOrderedListLevel() {}
	virtual void closeUnorderedListLevel() {}
	virtual void openListElement(const RVNGPropertyList & /* propList */, const RVNGPropertyListVector & /* tabStops */) {}
	virtual void closeListElement() {}

	virtual void openFootnote(const RVNGPropertyList & /* propList */) {}
	virtual void closeFootnote() {}
	virtual void openEndnote(const RVNGPropertyList & /* propList */) {}
	virtual void closeEndnote() {}
	virtual void openComment(const RVNGPropertyList & /* propList */) {}
	virtual void closeComment() {}
	virtual void openTextBox(const RVNGPropertyList & /* propList */) {}
	virtual void closeTextBox() {}

	virtual void openTable(const RVNGPropertyList & /* propList */, const RVNGPropertyListVector &columns);
	virtual void openTableRow(const RVNGPropertyList & /* propList */);
	virtual void closeTableRow();
	virtual void openTableCell(const RVNGPropertyList & /* propList */);
	virtual void closeTableCell();
	virtual void insertCoveredTableCell(const RVNGPropertyList & /* propList */) {};
	virtual void closeTable();

	virtual void openFrame(const RVNGPropertyList & /* propList */) {}
	virtual void closeFrame() {}

	virtual void insertBinaryObject(const RVNGPropertyList & /* propList */, const RVNGBinaryData & /* object */) {}
	virtual void insertEquation(const RVNGPropertyList & /* propList */, const RVNGString & /* data */) {}

private:
	std::ostream &getOutput();

	std::fstream m_output;
	bool m_outputInit;

	bool m_dataStarted;
	bool m_firstFieldSend;

	// Unimplemented to prevent compiler from creating crasher ones
	CSVGenerator(const CSVGenerator &);
	CSVGenerator &operator=(const CSVGenerator &);
};

#endif /* CSV_GENERATOR_H */
/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
