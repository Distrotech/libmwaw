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

#include <iostream>
#include <sstream>

#include <libwpd/libwpd.h>

#include "TableStyle.h"

void TableStyleManager::openTable(WPXPropertyListVector const &colList)
{
	std::vector<double> colWidths;
	for (unsigned long i = 0; i < colList.count(); i++)
	{
		WPXPropertyList const &prop=colList[i];
		double tmp;
		if (prop["style:column-width"] &&
		        mwaw2html::getPointValue(*prop["style:column-width"], tmp))
			colWidths.push_back(tmp/72.);
		else
			colWidths.push_back(0.);
	}
	m_columWitdhsStack.push_back(colWidths);
}

void TableStyleManager::closeTable()
{
	if (!m_columWitdhsStack.size())
	{
		MWAW_DEBUG_MSG(("TableStyleManager::closeTable: can not find the columns witdh\n"));
		return;
	}
	m_columWitdhsStack.pop_back();
}

bool TableStyleManager::getColumnsWidth(int col, int numSpanned, double &w) const
{
	if (!m_columWitdhsStack.size())
		return false;
	std::vector<double> const &widths=m_columWitdhsStack.back();
	if (col < 0 || size_t(col+numSpanned-1) >= widths.size())
	{
		MWAW_DEBUG_MSG(("TableStyleManager::getColumnsWidth: can not compute the columns witdh\n"));
		return false;
	}
	bool fixed = true;
	w = 0;
	for (size_t i=size_t(col); i < size_t(col+numSpanned); i++)
	{
		if (widths[i] < 0)
		{
			w += -widths[i];
			fixed = false;
		}
		else if (widths[i] > 0)
			w += widths[i];
		else
		{
			w = 0;
			return true;
		}
	}
	if (!fixed) w = -w;
	return true;
}

std::string TableStyleManager::getCellClass(WPXPropertyList const &pList)
{
	std::string content=getCellContent(pList);
	std::map<std::string, std::string>::iterator it=m_cellContentNameMap.find(content);
	if (it != m_cellContentNameMap.end())
		return it->second;
	std::stringstream s;
	s << "cellTable" << m_cellContentNameMap.size();
	m_cellContentNameMap[content]=s.str();
	return s.str();
}

std::string TableStyleManager::getRowClass(WPXPropertyList const &pList)
{
	std::string content=getRowContent(pList);
	std::map<std::string, std::string>::iterator it=m_rowContentNameMap.find(content);
	if (it != m_rowContentNameMap.end())
		return it->second;
	std::stringstream s;
	s << "rowTable" << m_rowContentNameMap.size();
	m_rowContentNameMap[content]=s.str();
	return s.str();
}

void TableStyleManager::send(std::ostream &out)
{
	std::map<std::string, std::string>::iterator it=m_cellContentNameMap.begin();
	while (it != m_cellContentNameMap.end())
	{
		out << "." << it->second << " " << it->first << "\n";
		++it;
	}
	it=m_rowContentNameMap.begin();
	while (it != m_rowContentNameMap.end())
	{
		out << "." << it->second << " " << it->first << "\n";
		++it;
	}
}

std::string TableStyleManager::getCellContent(WPXPropertyList const &pList) const
{
	std::stringstream s;
	s << "{\n";
	// try to get the cell width
	if (pList["libwpd:column"])
	{
		int c=pList["libwpd:column"]->getInt();
		int span=1;
		if (pList["table:number-columns-spanned"])
			span = pList["table:number-columns-spanned"]->getInt();
		double w;
		if (!getColumnsWidth(c,span,w))
		{
			MWAW_DEBUG_MSG(("TableStyleManager::getCellContent: can not find columns witdth for %d[%d]\n", c, span));
		}
		else if (w > 0)
			s << "\twidth:" << w << "in;\n";
		else if (w < 0)
			s << "\tmin-width:" << -w << "in;\n";
	}
	if (pList["fo:text-align"])
	{
		if (pList["fo:text-align"]->getStr() == WPXString("end")) // stupid OOo convention..
			s << "\ttext-align:right;\n";
		else
			s << "\ttext-align:" << pList["fo:text-align"]->getStr().cstr() << ";\n";
	}
	if (pList["style:vertical-align"])
		s << "\tvertical-align:" << pList["style:vertical-align"]->getStr().cstr() << ";\n";
	else
		s << "\tvertical-align:top;\n";
	if (pList["fo:background-color"])
		s << "\tbackground-color:" << pList["fo:background-color"]->getStr().cstr() << ";\n";

	static char const *(type[]) = {"border", "border-left", "border-top", "border-right", "border-bottom" };
	for (int i = 0; i < 5; i++)
	{
		std::string field("fo:");
		field+=type[i];
		if (!pList[field.c_str()])
			continue;
		s << "\t" << type[i] << ": " << pList[field.c_str()]->getStr().cstr() << ";\n";
	}

	s << "}";
	return s.str();
}

std::string TableStyleManager::getRowContent(WPXPropertyList const &pList) const
{
	std::stringstream s;
	s << "{\n";
	if (pList["style:min-row-height"])
		s << "\tmin-height:" << pList["style:min-row-height"]->getStr().cstr() << ";\n";
	else if (pList["style:row-height"])
		s << "\theight:" << pList["style:row-height"]->getStr().cstr() << ";\n";
	s << "}";
	return s.str();
}

/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
