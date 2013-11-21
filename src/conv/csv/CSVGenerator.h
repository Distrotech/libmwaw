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

class CSVGenerator : public librevenge::RVNGTextInterface
{
public:
	CSVGenerator(char const *fName=0);
	virtual ~CSVGenerator();

	virtual void setDocumentMetaData(const librevenge::RVNGPropertyList &) {};

	virtual void startDocument() {}
	virtual void endDocument() {}

	virtual void definePageStyle(const librevenge::RVNGPropertyList &) {}
	virtual void openPageSpan(const librevenge::RVNGPropertyList & /* propList */) {}
	virtual void closePageSpan() {}
	virtual void openHeader(const librevenge::RVNGPropertyList & /* propList */) {}
	virtual void closeHeader() {}
	virtual void openFooter(const librevenge::RVNGPropertyList & /* propList */) {}
	virtual void closeFooter() {}

	virtual void defineSectionStyle(const librevenge::RVNGPropertyList &, const librevenge::RVNGPropertyListVector &) {}
	virtual void openSection(const librevenge::RVNGPropertyList & /* propList */, const librevenge::RVNGPropertyListVector & /* columns */) {}
	virtual void closeSection() {}

	virtual void defineParagraphStyle(const librevenge::RVNGPropertyList &) {}
	virtual void openParagraph(const librevenge::RVNGPropertyList & /* propList */) {}
	virtual void closeParagraph() {}

	virtual void defineCharacterStyle(const librevenge::RVNGPropertyList &) {}
	virtual void openSpan(const librevenge::RVNGPropertyList & /* propList */) {}
	virtual void closeSpan() {}

	virtual void insertTab();
	virtual void insertText(const librevenge::RVNGString &text);
	virtual void insertSpace();
	virtual void insertLineBreak();
	virtual void insertField(const librevenge::RVNGString &/*type*/, const librevenge::RVNGPropertyList &/*propList*/) {}

	virtual void defineOrderedListLevel(const librevenge::RVNGPropertyList & /* propList */) {}
	virtual void defineUnorderedListLevel(const librevenge::RVNGPropertyList & /* propList */) {}
	virtual void openOrderedListLevel(const librevenge::RVNGPropertyList & /* propList */) {}
	virtual void openUnorderedListLevel(const librevenge::RVNGPropertyList & /* propList */) {}
	virtual void closeOrderedListLevel() {}
	virtual void closeUnorderedListLevel() {}
	virtual void openListElement(const librevenge::RVNGPropertyList & /* propList */) {}
	virtual void closeListElement() {}

	virtual void openFootnote(const librevenge::RVNGPropertyList & /* propList */) {}
	virtual void closeFootnote() {}
	virtual void openEndnote(const librevenge::RVNGPropertyList & /* propList */) {}
	virtual void closeEndnote() {}
	virtual void openComment(const librevenge::RVNGPropertyList & /* propList */) {}
	virtual void closeComment() {}
	virtual void openTextBox(const librevenge::RVNGPropertyList & /* propList */) {}
	virtual void closeTextBox() {}

	virtual void openTable(const librevenge::RVNGPropertyList & /* propList */, const librevenge::RVNGPropertyListVector &columns);
	virtual void openTableRow(const librevenge::RVNGPropertyList & /* propList */);
	virtual void closeTableRow();
	virtual void openTableCell(const librevenge::RVNGPropertyList & /* propList */);
	virtual void closeTableCell();
	virtual void insertCoveredTableCell(const librevenge::RVNGPropertyList & /* propList */) {};
	virtual void closeTable();

	virtual void openFrame(const librevenge::RVNGPropertyList & /* propList */) {}
	virtual void closeFrame() {}

	virtual void insertBinaryObject(const librevenge::RVNGPropertyList & /* propList */) {}
	virtual void insertEquation(const librevenge::RVNGPropertyList & /* propList */, const librevenge::RVNGString & /* data */) {}

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
