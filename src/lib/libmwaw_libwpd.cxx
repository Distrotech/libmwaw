/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libwpd
 * Copyright (C) 2002, 2005 William Lachance (wrlach@gmail.com)
 * Copyright (C) 2002, 2004 Marc Maurer (uwog@uwog.net)
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
#include "libmwaw_libwpd.hxx"
#include <libwpd-stream/WPXStream.h>
#include <ctype.h>
#include <locale.h>
#include <string>

// OSNOLA
/** namespace used to regroup all libwpd functions, enumerations which we have redefined for internal usage */
namespace libmwaw_libwpd
{
uint8_t readU8(WPXInputStream *input)
{
  unsigned long numBytesRead;
  uint8_t const *p = input->read(sizeof(uint8_t), numBytesRead);

  if (!p || numBytesRead != sizeof(uint8_t))
    throw libmwaw_libwpd::FileException();

  return WPD_LE_GET_GUINT8(p);
}

uint16_t readU16(WPXInputStream *input, bool bigendian)
{
  unsigned long numBytesRead;
  uint16_t const *val = (uint16_t const *)input->read(sizeof(uint16_t), numBytesRead);

  if (!val || numBytesRead != sizeof(uint16_t))
    throw libmwaw_libwpd::FileException();

  if (bigendian)
    return WPD_BE_GET_GUINT16(val);
  return WPD_LE_GET_GUINT16(val);
}

uint32_t readU32(WPXInputStream *input, bool bigendian)
{
  unsigned long numBytesRead;
  uint32_t const *val = (uint32_t const *)input->read(sizeof(uint32_t), numBytesRead);

  if (!val || numBytesRead != sizeof(uint32_t))
    throw libmwaw_libwpd::FileException();

  if (bigendian)
    return WPD_BE_GET_GUINT32(val);
  return WPD_LE_GET_GUINT32(val);
}

/** -- OSNOLA */
typedef struct _WPXComplexMap {
  uint16_t charToMap;
  uint32_t unicodeChars[6];
} WPXComplexMap;


// the ascii map appears stupid, but we need the const 32-bit data for now
static const uint32_t asciiMap[] = {
  32,  33,  34,  35,  36,  37,  38,  39,
  40,  41,  42,  43,  44,  45,  46,  47,
  48,  49,  50,  51,  52,  53,  54,  55,
  56,  57,  58,  59,  60,  61,  62,  63,
  64,  65,  66,  67,  68,  69,  70,  71,
  72,  73,  74,  75,  76,  77,  78,  79,
  80,  81,  82,  83,  84,  85,  86,  87,
  88,  89,  90,  91,  92,  93,  94,  95,
  96,  97,  98,  99, 100, 101, 102, 103,
  104, 105, 106, 107, 108, 109, 110, 111,
  112, 113, 114, 115, 116, 117, 118, 119,
  120, 121, 122, 123, 124, 125, 126
};


static int findSimpleMap(uint16_t character, const uint32_t **chars, const uint32_t *simpleMap, const size_t simpleMapSize)
{
  if ((character < simpleMapSize) && simpleMap[character]) {
    *chars = &simpleMap[character];
    return 1;
  }

  return 0;
}

static int findComplexMap(uint16_t character, const uint32_t **chars, const WPXComplexMap *complexMap)
{
  if (!complexMap)
    return 0;

  unsigned i = 0;
  while (complexMap[i].charToMap) {
    if (complexMap[i].charToMap == character)
      break;
    i++;
  }
  if (!(complexMap[i].charToMap) || !(complexMap[i].unicodeChars[0]))
    return 0;

  *chars = complexMap[i].unicodeChars;

  for (unsigned j = 0; j<WPD_NUM_ELEMENTS(complexMap[i].unicodeChars); j++) {
    if (!(complexMap[i].unicodeChars[j]))
      return (int)j;
  }

  return 0;
}

// OSNOLA

int extendedCharacterWP6ToUCS4(uint8_t character,
                               uint8_t characterSet, const uint32_t **chars)
{
  /*int i;
    int retVal = 0; : OSNOLA */

  if (characterSet == 0) {
    // if characterset == 0, we have ascii. note that this is different from the doc. body
    // this is not documented in the file format specifications
    if (character >= 0x20 && character < 0x7F)
      *chars = &asciiMap[character - 0x20];
    else
      *chars = &asciiMap[0x00];
    return 1;
  }

  // last resort: return whitespace
  *chars = &asciiMap[0x00];
  return 1;
}


// OSNOLA
int extendedCharacterWP5ToUCS4(uint8_t character,
                               uint8_t characterSet, const uint32_t **chars)
{
  // int retVal = 0; OSNOLA

  if (characterSet == 0) {
    // if characterset == 0, we have ascii. note that this is different from the doc. body
    // this is not documented in the file format specifications
    if (character >= 0x20 && character < 0x7F)
      *chars = &asciiMap[character - 0x20];
    else
      *chars = &asciiMap[0x00];
    return 1;
  }

  // last resort: return whitespace
  *chars = &asciiMap[0x00];
  return 1;
}

static const uint32_t extendedCharactersWP42[] = {
  /*   0 */	0x0020, 0x263a, 0x263b, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022,
  /*   8 */	0x25d8, 0x25cb, 0x25d9, 0x2642, 0x2640, 0x266a, 0x266c, 0x263c,
  /*  16 */	0x25b8, 0x25c2, 0x2195, 0x203c, 0x00b6, 0x00a7, 0x25ac, 0x21a8,
  /*  24 */	0x2191, 0x2193, 0x2192, 0x2190, 0x2319, 0x2194, 0x25b4, 0x25be,
  /*  32 */	0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
  /*  40 */	0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
  /*  48 */	0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
  /*  56 */	0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
  /*  64 */	0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
  /*  72 */	0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
  /*  80 */	0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
  /*  88 */	0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f,
  /*  96 */	0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
  /* 104 */	0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f,
  /* 112 */	0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
  /* 120 */	0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d, 0x007e, 0x2302,
  /* 128 */	0x00c7, 0x00fc, 0x00eb, 0x00e2, 0x00e4, 0x00e0, 0x00e5, 0x00e7,
  /* 136 */	0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5,
  /* 144 */	0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9,
  /* 152 */	0x00ff, 0x00d6, 0x00dc, 0x00a2, 0x00a3, 0x00a5, 0x20a7, 0x0192,
  /* 160 */	0x00e1, 0x00ed, 0x00f3, 0x00f4, 0x00f1, 0x00d1, 0x00aa, 0x00ba,
  /* 168 */	0x00bf, 0x2310, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb,
  /* 176 */	0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
  /* 184 */	0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510,
  /* 192 */	0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x255e, 0x255f,
  /* 200 */	0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567,
  /* 208 */	0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b,
  /* 216 */	0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580,
  /* 224 */	0x03b1, 0x00df, 0x0393, 0x03c0, 0x03a3, 0x03c3, 0x03bc, 0x03c4,
  /* 232 */	0x03a6, 0x0398, 0x03a9, 0x03b4, 0x221e, 0x03c6, 0x03b5, 0x22c2,
  /* 240 */	0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248,
  /* 248 */	0x00b0, 0x2022, 0x22c5, 0x221a, 0x207f, 0x00b2, 0x25fc, 0x0020
};

int extendedCharacterWP42ToUCS4(uint8_t character, const uint32_t **chars)
{
  int retVal = 0;

  if ((retVal = findSimpleMap(character, chars, extendedCharactersWP42, WPD_NUM_ELEMENTS(extendedCharactersWP42))))
    return retVal;
  *chars = &asciiMap[0x00];
  return 1;
}

uint16_t fixedPointToWPUs(const uint32_t fixedPointNumber)
{
  int16_t fixedPointNumberIntegerPart = (int16_t)((fixedPointNumber & 0xFFFF0000) >> 16);
  double fixedPointNumberFractionalPart = (double)((double)(fixedPointNumber & 0x0000FFFF)/(double)0xFFFF);
  uint16_t numberWPU = (uint16_t)((((double)fixedPointNumberIntegerPart + fixedPointNumberFractionalPart)*50)/3);
  return numberWPU;
}

double fixedPointToDouble(const uint32_t fixedPointNumber)
{
  int16_t fixedPointNumberIntegerPart = (int16_t)((fixedPointNumber & 0xFFFF0000) >> 16);
  double fixedPointNumberFractionalPart = (double)((double)(fixedPointNumber & 0x0000FFFF)/(double)0xFFFF);
  return ((double)fixedPointNumberIntegerPart + fixedPointNumberFractionalPart);
}

double wpuToFontPointSize(const uint16_t wpuNumber)
{
  return (double)((double)((((double)wpuNumber)/100.0)*2.0));
}

_RGBSColor::_RGBSColor(uint8_t r, uint8_t g, uint8_t b, uint8_t s)
  :	m_r(r),
    m_g(g),
    m_b(b),
    m_s(s)
{
}

_RGBSColor::_RGBSColor()
  :	m_r(0),
    m_g(0),
    m_b(0),
    m_s(0)
{
}

_RGBSColor::_RGBSColor(uint16_t red, uint16_t green, uint16_t blue)
  :	m_r((uint8_t)((red >> 8) & 0xFF)),
    m_g((uint8_t)((green >> 8) & 0xFF)),
    m_b((uint8_t)((blue >> 8) & 0xFF)),
    m_s(100)
{
}
}

_DMWAWTabStop::_DMWAWTabStop(double position, DMWAWTabAlignment alignment, uint16_t leaderCharacter, uint8_t leaderNumSpaces)
  :	m_position(position),
    m_alignment(alignment),
    m_leaderCharacter(leaderCharacter),
    m_leaderNumSpaces(leaderNumSpaces)
{
}

_DMWAWTabStop::_DMWAWTabStop()
  :	m_position(0.0),
    m_alignment(LEFT),
    m_leaderCharacter('\0'),
    m_leaderNumSpaces(0)
{
}

_DMWAWColumnDefinition::_DMWAWColumnDefinition()
  :	m_width(0.0),
    m_leftGutter(0.0),
    m_rightGutter(0.0)
{
}

_DMWAWColumnProperties::_DMWAWColumnProperties()
  :	m_attributes(0x00000000),
    m_alignment(0x00)
{
}

namespace libmwaw_libwpd
{
// HACK: this function is really cheesey
int _extractNumericValueFromRoman(const char romanChar)
{
  int retValue = 0;
  switch (romanChar) {
  case 'I':
  case 'i':
    retValue = 1;
    break;
  case 'V':
  case 'v':
    retValue = 5;
    break;
  case 'X':
  case 'x':
    retValue = 10;
    break;
  default:
    throw libmwaw_libwpd::ParseException();
  }
  return retValue;
}

// _extractDisplayReferenceNumberFromBuf: given a nuWP6_DEFAULT_FONT_SIZEmber string in UCS4 represented
// as letters, numbers, or roman numerals.. return an integer value representing its number
// HACK: this function is really cheesey
// NOTE: if the input is not valid, the output is unspecified
int _extractDisplayReferenceNumberFromBuf(const WPXString &buf, const DMWAWNumberingType listType)
{
  if (listType == LOWERCASE_ROMAN || listType == UPPERCASE_ROMAN) {
    int currentSum = 0;
    int lastMark = 0;
    WPXString::Iter i(buf);
    for (i.rewind(); i.next();) {
      int currentMark = _extractNumericValueFromRoman(*(i()));
      if (lastMark < currentMark) {
        currentSum = currentMark - lastMark;
      } else
        currentSum+=currentMark;
      lastMark = currentMark;
    }
    return currentSum;
  } else if (listType == LOWERCASE || listType == UPPERCASE) {
    // FIXME: what happens to a lettered list that goes past z? ah
    // the sweet mysteries of life
    if (buf.len()==0)
      throw libmwaw_libwpd::ParseException();
    char c = buf.cstr()[0];
    if (listType==LOWERCASE)
      c = (char)toupper(c);
    return (c - 64);
  } else if (listType == ARABIC) {
    int currentSum = 0;
    WPXString::Iter i(buf);
    for (i.rewind(); i.next();) {
      currentSum *= 10;
      currentSum+=(*(i())-48);
    }
    return currentSum;
  }

  return 1;
}

DMWAWNumberingType _extractDMWAWNumberingTypeFromBuf(const WPXString &buf, const DMWAWNumberingType putativeDMWAWNumberingType)
{
  WPXString::Iter i(buf);
  for (i.rewind(); i.next();) {
    if ((*(i()) == 'I' || *(i()) == 'V' || *(i()) == 'X') &&
        (putativeDMWAWNumberingType == LOWERCASE_ROMAN || putativeDMWAWNumberingType == UPPERCASE_ROMAN))
      return UPPERCASE_ROMAN;
    else if ((*(i()) == 'i' || *(i()) == 'v' || *(i()) == 'x') &&
             (putativeDMWAWNumberingType == LOWERCASE_ROMAN || putativeDMWAWNumberingType == UPPERCASE_ROMAN))
      return LOWERCASE_ROMAN;
    else if (*(i()) >= 'A' && *(i()) <= 'Z')
      return UPPERCASE;
    else if (*(i()) >= 'a' && *(i()) <= 'z')
      return LOWERCASE;
  }

  return ARABIC;
}

WPXString _numberingTypeToString(DMWAWNumberingType t)
{
  WPXString sListTypeSymbol("1");
  switch (t) {
  case ARABIC:
    sListTypeSymbol = "1";
    break;
  case LOWERCASE:
    sListTypeSymbol = "a";
    break;
  case UPPERCASE:
    sListTypeSymbol = "A";
    break;
  case LOWERCASE_ROMAN:
    sListTypeSymbol = "i";
    break;
  case UPPERCASE_ROMAN:
    sListTypeSymbol = "I";
    break;
  }

  return sListTypeSymbol;
}

/* Mapping of Apple's MacRoman character set in Unicode (UCS4)
 * used in the WordPerfect Macintosh file format */

const uint32_t macRomanCharacterMap[] = {
  0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
  0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
  0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
  0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
  0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
  0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
  0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
  0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f,
  0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
  0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f,
  0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
  0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d, 0x007e, 0x0020,
  0x00c4, 0x00c5, 0x00c7, 0x00c9, 0x00d1, 0x00d6, 0x00dc, 0x00e1,
  0x00e0, 0x00e2, 0x00e4, 0x00e3, 0x00e5, 0x00e7, 0x00e9, 0x00e8,
  0x00ea, 0x00eb, 0x00ed, 0x00ec, 0x00ee, 0x00ef, 0x00f1, 0x00f3,
  0x00f2, 0x00f4, 0x00f6, 0x00f5, 0x00fa, 0x00f9, 0x00fb, 0x00fc,
  0x2020, 0x00b0, 0x00a2, 0x00a3, 0x00a7, 0x2022, 0x00b6, 0x00df,
  0x00ae, 0x00a9, 0x2122, 0x00b4, 0x00a8, 0x2260, 0x00c6, 0x00d8,
  0x221e, 0x00b1, 0x2264, 0x2265, 0x00a5, 0x00b5, 0x2202, 0x2211,
  0x220f, 0x03c0, 0x222b, 0x00aa, 0x00ba, 0x03a9, 0x00e6, 0x00f8,
  0x00bf, 0x00a1, 0x00ac, 0x221a, 0x0192, 0x2248, 0x2206, 0x00ab,
  0x00bb, 0x2026, 0x00a0, 0x00c0, 0x00c3, 0x00d5, 0x0152, 0x0153,
  0x2013, 0x2014, 0x201c, 0x201d, 0x2018, 0x2019, 0x00f7, 0x25ca,
  0x00ff, 0x0178, 0x2044, 0x20ac, 0x2039, 0x203a, 0xfb01, 0xfb02,
  0x2021, 0x00b7, 0x201a, 0x201e, 0x2030, 0x00c2, 0x00ca, 0x00c1,
  0x00cb, 0x00c8, 0x00cd, 0x00ce, 0x00cf, 0x00cc, 0x00d3, 0x00d4,
  0xf8ff, 0x00d2, 0x00da, 0x00db, 0x00d9, 0x0131, 0x02c6, 0x02dc,
  0x00af, 0x02d8, 0x02d9, 0x02da, 0x00b8, 0x02dd, 0x02db, 0x02c7
};

WPXString doubleToString(const double value)
{
  WPXString tempString;
  if (value < 0.0001 && value > -0.0001)
    tempString.sprintf("0.0000");
  else
    tempString.sprintf("%.4f", value);
#ifndef __ANDROID__
  std::string decimalPoint(localeconv()->decimal_point);
#else
  std::string decimalPoint(".");
#endif
  if ((decimalPoint.size() == 0) || (decimalPoint == "."))
    return tempString;
  std::string stringValue(tempString.cstr());
  if (!stringValue.empty()) {
    std::string::size_type pos;
    while ((pos = stringValue.find(decimalPoint)) != std::string::npos)
      stringValue.replace(pos,decimalPoint.size(),".");
  }
  return WPXString(stringValue.c_str());
}

int appleWorldScriptToUCS4(uint16_t character, const uint32_t **chars)
{
  static const uint32_t charSimpleMap[] = {
    0x0000  // 0xfdf8 - 0xfdff
  };

  static const WPXComplexMap charComplexMap[] = {
    { 0x0000, { 0x0000 } }
  };

  if (character < 0x8140 || character >= 0xfdff) {
    // character outside our known range: return whitespace
    *chars = &asciiMap[0x00];
    return 1;
  }

  int retVal = 0;

  // Find the entry corresponding to the WorldScript character
  if ((retVal = findSimpleMap(character - 0x8140, chars, charSimpleMap, WPD_NUM_ELEMENTS(charSimpleMap))))
    return retVal;

  if ((retVal = findComplexMap(character, chars, charComplexMap)))
    return retVal;

  *chars = &asciiMap[0x00];
  return 1;

}
}
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
