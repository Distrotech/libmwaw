/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libwpd
 * Copyright (C) 2002 William Lachance (wrlach@gmail.com)
 * Copyright (C) 2002 Marc Maurer (uwog@uwog.net)
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
 * For further information visit http://libwpd.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

#ifndef WPXFILESTRUCTURE_H
#define WPXFILESTRUCTURE_H

#define DMWAW_NUM_WPUS_PER_INCH 1200

// header defines
#define DMWAW_HEADER_MAGIC_OFFSET 1
#define DMWAW_HEADER_DOCUMENT_POINTER_OFFSET 4
#define DMWAW_HEADER_PRODUCT_TYPE_OFFSET 8
#define DMWAW_HEADER_FILE_TYPE_OFFSET 9
#define DMWAW_HEADER_MAJOR_VERSION_OFFSET 10
#define DMWAW_HEADER_MINOR_VERSION_OFFSET 11
#define DMWAW_HEADER_ENCRYPTION_OFFSET 12

#define DMWAW_NUM_HEADER_FOOTER_TYPES 6
#define DMWAW_HEADER_A 0x00
#define DMWAW_HEADER_B 0x01
#define DMWAW_FOOTER_A 0x02
#define DMWAW_FOOTER_B 0x03
#define DMWAW_WATERMARK_A 0x04
#define DMWAW_WATERMARK_B 0x05

#define DMWAW_CHARACTER 0x00
#define DMWAW_PARAGRAPH 0x01
#define DMWAW_PAGE 0x01

#endif /* WPXFILESTRUCTURE_H */
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
