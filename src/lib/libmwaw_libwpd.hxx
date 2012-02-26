/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libwpd
 * Copyright (C) 2002 William Lachance (wrlach@gmail.com)
 * Copyright (C) 2002,2004 Marc Maurer (uwog@uwog.net)
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

#ifndef LIBWPD_INTERNAL_H
#define LIBWPD_INTERNAL_H
#include <libwpd-stream/WPXStream.h>
#include <stdio.h>
#include <string>
#include <algorithm>
#include <libwpd/WPXString.h>
#include "DMWAWEncryption.hxx"
#include "libmwaw_libwpd_types.hxx"

/* Various functions/defines that need not/should not be exported externally */
#ifdef _MSC_VER
#include <minmax.h>
#define LIBWPD_MIN min
#define LIBWPD_MAX max
#else
#define LIBWPD_MIN std::min
#define LIBWPD_MAX std::max
#endif

#define WPD_CHECK_FILE_ERROR(v) if (v==EOF) { WPD_DEBUG_MSG(("X_CheckFileError: %d\n", __LINE__)); throw FileException(); }
#define WPD_CHECK_FILE_SEEK_ERROR(v) if (v) { WPD_DEBUG_MSG(("X_CheckFileSeekError: %d\n", __LINE__)); throw FileException(); }
#define WPD_CHECK_FILE_READ_ERROR(v,num_elements) if (v != num_elements) {\
 WPD_DEBUG_MSG(("X_CheckFileReadElementError: %d\n", __LINE__)); throw FileException(); }

#define DELETEP(m) if (m) { delete m; m = 0; }

#ifdef DEBUG
#define WPD_DEBUG_MSG(M) printf M
#else
#define WPD_DEBUG_MSG(M)
#endif

#define WPD_LE_GET_GUINT8(p) (*(uint8_t const *)(p))
#define WPD_LE_GET_GUINT16(p)				  \
        (uint16_t)((((uint8_t const *)(p))[0] << 0)  |    \
                  (((uint8_t const *)(p))[1] << 8))
#define WPD_LE_GET_GUINT32(p) \
        (uint32_t)((((uint8_t const *)(p))[0] << 0)  |    \
                  (((uint8_t const *)(p))[1] << 8)  |    \
                  (((uint8_t const *)(p))[2] << 16) |    \
                  (((uint8_t const *)(p))[3] << 24))

#define WPD_BE_GET_GUINT8(p) (*(uint8_t const *)(p))
#define WPD_BE_GET_GUINT16(p)                           \
        (uint16_t)((((uint8_t const *)(p))[1] << 0)  |    \
                  (((uint8_t const *)(p))[0] << 8))
#define WPD_BE_GET_GUINT32(p)                           \
        (uint32_t)((((uint8_t const *)(p))[3] << 0)  |    \
                  (((uint8_t const *)(p))[2] << 8)  |    \
                  (((uint8_t const *)(p))[1] << 16) |    \
                  (((uint8_t const *)(p))[0] << 24))

#define WPD_NUM_ELEMENTS(array) sizeof(array)/sizeof(array[0])

namespace libmwaw_libwpd
{
// add more of these as needed for byteswapping
// (the 8-bit functions are just there to make things consistent)
uint8_t readU8(WPXInputStream *input, DMWAWEncryption *encryption);
uint16_t readU16(WPXInputStream *input, DMWAWEncryption *encryption, bool bigendian=false);
uint32_t readU32(WPXInputStream *input, DMWAWEncryption *encryption, bool bigendian=false);

WPXString readPascalString(WPXInputStream *input, DMWAWEncryption *encryption);
WPXString readCString(WPXInputStream *input, DMWAWEncryption *encryption);

void appendUCS4(WPXString &str, uint32_t ucs4);

// Various helper structures for the libwpd parser..

int extendedCharacterWP6ToUCS4(uint8_t character, uint8_t characterSet,
                               const uint32_t **chars);

int extendedCharacterWP5ToUCS4(uint8_t character, uint8_t characterSet,
                               const uint32_t **chars);

int appleWorldScriptToUCS4(uint16_t character, const uint32_t **chars);

int extendedCharacterWP42ToUCS4(uint8_t character, const uint32_t **chars);

uint16_t fixedPointToWPUs(const uint32_t fixedPointNumber);
double fixedPointToDouble(const uint32_t fixedPointNumber);
double wpuToFontPointSize(const uint16_t wpuNumber);
}

enum DMWAWFileType { WP6_DOCUMENT, WP5_DOCUMENT, WP42_DOCUMENT, OTHER };
enum DMWAWNumberingType { ARABIC, LOWERCASE, UPPERCASE, LOWERCASE_ROMAN, UPPERCASE_ROMAN };
enum DMWAWNoteType { FOOTNOTE, ENDNOTE };
enum DMWAWHeaderFooterType { HEADER, FOOTER };
enum DMWAWHeaderFooterInternalType { HEADER_A, HEADER_B, FOOTER_A, FOOTER_B, DUMMY };
enum DMWAWHeaderFooterOccurence { ODD, EVEN, ALL, NEVER };

enum DMWAWPageNumberPosition { PAGENUMBER_POSITION_NONE = 0, PAGENUMBER_POSITION_TOP_LEFT, PAGENUMBER_POSITION_TOP_CENTER,
                               PAGENUMBER_POSITION_TOP_RIGHT, PAGENUMBER_POSITION_TOP_LEFT_AND_RIGHT,
                               PAGENUMBER_POSITION_BOTTOM_LEFT, PAGENUMBER_POSITION_BOTTOM_CENTER,
                               PAGENUMBER_POSITION_BOTTOM_RIGHT, PAGENUMBER_POSITION_BOTTOM_LEFT_AND_RIGHT,
                               PAGENUMBER_POSITION_TOP_INSIDE_LEFT_AND_RIGHT,
                               PAGENUMBER_POSITION_BOTTOM_INSIDE_LEFT_AND_RIGHT
                             };
enum DMWAWFormOrientation { PORTRAIT, LANDSCAPE };
enum DMWAWTabAlignment { LEFT, RIGHT, CENTER, DECIMAL, BAR };
enum DMWAWVerticalAlignment { TOP, MIDDLE, BOTTOM, FULL };

enum DMWAWTextColumnType { NEWSPAPER, NEWSPAPER_VERTICAL_BALANCE, PARALLEL, PARALLEL_PROTECT };
enum DMWAWSubDocumentType { DMWAW_SUBDOCUMENT_NONE, DMWAW_SUBDOCUMENT_HEADER_FOOTER, DMWAW_SUBDOCUMENT_NOTE, DMWAW_SUBDOCUMENT_TEXT_BOX, DMWAW_SUBDOCUMENT_COMMENT_ANNOTATION };

// ATTRIBUTE bits
#define DMWAW_EXTRA_LARGE_BIT 1
#define DMWAW_VERY_LARGE_BIT 2
#define DMWAW_LARGE_BIT 4
#define DMWAW_SMALL_PRINT_BIT 8
#define DMWAW_FINE_PRINT_BIT 16
#define DMWAW_SUPERSCRIPT_BIT 32
#define DMWAW_SUBSCRIPT_BIT 64
#define DMWAW_OUTLINE_BIT 128
#define DMWAW_ITALICS_BIT 256
#define DMWAW_SHADOW_BIT 512
#define DMWAW_REDLINE_BIT 1024
#define DMWAW_DOUBLE_UNDERLINE_BIT 2048
#define DMWAW_BOLD_BIT 4096
#define DMWAW_STRIKEOUT_BIT 8192
#define DMWAW_UNDERLINE_BIT 16384
#define DMWAW_SMALL_CAPS_BIT 32768
#define DMWAW_BLINK_BIT 65536
#define DMWAW_REVERSEVIDEO_BIT 131072
// OSNOLA: six new attribute for MWAW
#define DMWAW_ALL_CAPS_BIT 0x40000
#define DMWAW_EMBOSS_BIT 0x80000
#define DMWAW_ENGRAVE_BIT 0x100000
#define DMWAW_SUPERSCRIPT100_BIT 0x200000
#define DMWAW_SUBSCRIPT100_BIT 0x400000
#define DMWAW_HIDDEN_BIT 0x80000


// JUSTIFICATION bits.
#define DMWAW_PARAGRAPH_JUSTIFICATION_LEFT 0x00
#define DMWAW_PARAGRAPH_JUSTIFICATION_FULL 0x01
#define DMWAW_PARAGRAPH_JUSTIFICATION_CENTER 0x02
#define DMWAW_PARAGRAPH_JUSTIFICATION_RIGHT 0x03
#define DMWAW_PARAGRAPH_JUSTIFICATION_FULL_ALL_LINES 0x04
#define DMWAW_PARAGRAPH_JUSTIFICATION_DECIMAL_ALIGNED 0x05

// TABLE POSITION bits.
#define DMWAW_TABLE_POSITION_ALIGN_WITH_LEFT_MARGIN 0x00
#define DMWAW_TABLE_POSITION_ALIGN_WITH_RIGHT_MARGIN 0x01
#define DMWAW_TABLE_POSITION_CENTER_BETWEEN_MARGINS 0x02
#define DMWAW_TABLE_POSITION_FULL 0x03
#define DMWAW_TABLE_POSITION_ABSOLUTE_FROM_LEFT_MARGIN 0x04

// TABLE CELL BORDER bits
const uint8_t DMWAW_TABLE_CELL_LEFT_BORDER_OFF = 0x01;
const uint8_t DMWAW_TABLE_CELL_RIGHT_BORDER_OFF = 0x02;
const uint8_t DMWAW_TABLE_CELL_TOP_BORDER_OFF = 0x04;
const uint8_t DMWAW_TABLE_CELL_BOTTOM_BORDER_OFF = 0x08;

// BREAK bits
#define DMWAW_PAGE_BREAK 0x00
#define DMWAW_SOFT_PAGE_BREAK 0x01
#define DMWAW_COLUMN_BREAK 0x02

// Generic bits
#define DMWAW_LEFT 0x00
#define DMWAW_RIGHT 0x01
#define DMWAW_CENTER 0x02
#define DMWAW_TOP 0x03
#define DMWAW_BOTTOM 0x04

namespace libmwaw_libwpd
{
typedef struct _RGBSColor RGBSColor;
struct _RGBSColor {
  _RGBSColor(uint8_t r, uint8_t g, uint8_t b, uint8_t s);
  _RGBSColor(uint16_t red, uint16_t green, uint16_t blue); // Construct
  // RBBSColor from double precision RGB color used by WP3.x for Mac
  _RGBSColor(); // initializes all values to 0
  uint8_t m_r;
  uint8_t m_g;
  uint8_t m_b;
  uint8_t m_s;
};
}

typedef struct _DMWAWColumnDefinition DMWAWColumnDefinition;
struct _DMWAWColumnDefinition {
  _DMWAWColumnDefinition(); // initializes all values to 0
  double m_width;
  double m_leftGutter;
  double m_rightGutter;
};

typedef struct _DMWAWColumnProperties DMWAWColumnProperties;
struct _DMWAWColumnProperties {
  _DMWAWColumnProperties();
  uint32_t m_attributes;
  uint8_t m_alignment;
};

typedef struct _DMWAWTabStop DMWAWTabStop;
struct _DMWAWTabStop {
  _DMWAWTabStop(double position, DMWAWTabAlignment alignment, uint16_t leaderCharacter, uint8_t leaderNumSpaces);
  _DMWAWTabStop();
  double m_position;
  DMWAWTabAlignment m_alignment;
  uint16_t m_leaderCharacter;
  uint8_t m_leaderNumSpaces;
};

namespace libmwaw_libwpd
{
// Various exceptions: libwpd does not propagate exceptions externally..

class VersionException
{
};

class FileException
{
};

class ParseException
{
};

class GenericException
{
};

class UnsupportedEncryptionException
{
};

class SupportedEncryptionException
{
};

class WrongPasswordException
{
};

// Various usefull, but cheesey functions

int _extractNumericValueFromRoman(const char romanChar);
int _extractDisplayReferenceNumberFromBuf(const WPXString &buf, const DMWAWNumberingType listType);
DMWAWNumberingType _extractDMWAWNumberingTypeFromBuf(const WPXString &buf, const DMWAWNumberingType putativeDMWAWNumberingType);
WPXString _numberingTypeToString(DMWAWNumberingType t);
extern const uint32_t macRomanCharacterMap[];
WPXString doubleToString(const double value);
}

#endif /* LIBWPD_INTERNAL_H */
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
