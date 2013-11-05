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

#include <stdio.h>
#include "TextDocumentGenerator.h"

// use the BELL code to represent a TAB for now
#define UCS_TAB 0x0009

TextDocumentGenerator::TextDocumentGenerator(const bool isInfo) :
	m_isInfo(isInfo)
{
}

TextDocumentGenerator::~TextDocumentGenerator()
{
}

void TextDocumentGenerator::setDocumentMetaData(const librevenge::RVNGPropertyList &propList)
{
	if (!m_isInfo)
		return;
	librevenge::RVNGPropertyList::Iter propIter(propList);
	for (propIter.rewind(); propIter.next(); )
	{
		printf("%s %s\n", propIter.key(), propIter()->getStr().cstr());
	}
}

void TextDocumentGenerator::closeParagraph()
{
	if (m_isInfo)
		return;
	printf("\n");
}

void TextDocumentGenerator::insertTab()
{
	if (m_isInfo)
		return;
	printf("%c", UCS_TAB);
}

void TextDocumentGenerator::insertText(const librevenge::RVNGString &text)
{
	if (m_isInfo)
		return;
	printf("%s", text.cstr());
}

void TextDocumentGenerator::insertSpace()
{
	if (m_isInfo)
		return;
	printf(" ");
}

void TextDocumentGenerator::insertLineBreak()
{
	if (m_isInfo)
		return;
	printf("\n");
}
/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
