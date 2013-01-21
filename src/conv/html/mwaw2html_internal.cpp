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

#include <string.h>
#include <cstring>

#include <libwpd/libwpd.h>

#include "mwaw2html_internal.h"

namespace mwaw2html
{
bool getPointValue(WPXProperty const &prop, double &res)
{
	res = prop.getDouble();

	// try to guess the type
	WPXString str = prop.getStr();

	// point->pt, twip->*, inch -> in
	char c = str.len() ? str.cstr()[str.len()-1] : ' ';
	if (c == '*') res /= 1440.;
	else if (c == 't') res /= 72.;
	else if (c == 'n') ;
	else if (c == '%')
		return false;
	res *= 72.;
	return true;
}
}
/* vim:set shiftwidth=4 softtabstop=4 noexpandtab: */
