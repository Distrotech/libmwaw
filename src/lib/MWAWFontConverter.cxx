/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */

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

#include <iostream>

#include "libmwaw_internal.hxx"

#include "MWAWFontConverter.hxx"

//! Internal namespace used to store the data used by MWAWFontConverterInternal
namespace MWAWFontConverterInternal
{
//! Internal and low level: tools to convert Macintosh characters
namespace Data
{
//! Internal and low level: a class to store a conversion map for character, ...
struct ConversionData {
  //! constructor
  ConversionData(std::map<unsigned char, unsigned long> &map,
                 char const *odtName="", int delta=0)
    : m_conversion(map), m_name(odtName), m_deltaSize(delta) {}

  //! the conversion map character -> unicode
  std::map<unsigned char, unsigned long> &m_conversion;
  //! the odt font name (if empty used the name)
  std::string m_name;
  //! the size delta: odtSize = fSize + deltaSize
  int m_deltaSize;
};

// Courtesy of unicode.org: http://unicode.org/Public/MAPPINGS/VENDORS/APPLE/ROMAN.TXT
//! Internal and Low level: vector ( char, unicode) for roman font
static int const s_romanUnicode [] = {
  0x20,0x0020,  0x21,0x0021,  0x22,0x0022,  0x23,0x0023,  0x24,0x0024,
  0x25,0x0025,  0x26,0x0026,  0x27,0x0027,  0x28,0x0028,  0x29,0x0029,
  0x2A,0x002A,  0x2B,0x002B,  0x2C,0x002C,  0x2D,0x002D,  0x2E,0x002E,
  0x2F,0x002F,  0x30,0x0030,  0x31,0x0031,  0x32,0x0032,  0x33,0x0033,
  0x34,0x0034,  0x35,0x0035,  0x36,0x0036,  0x37,0x0037,  0x38,0x0038,
  0x39,0x0039,  0x3A,0x003A,  0x3B,0x003B,  0x3C,0x003C,  0x3D,0x003D,
  0x3E,0x003E,  0x3F,0x003F,  0x40,0x0040,  0x41,0x0041,  0x42,0x0042,
  0x43,0x0043,  0x44,0x0044,  0x45,0x0045,  0x46,0x0046,  0x47,0x0047,
  0x48,0x0048,  0x49,0x0049,  0x4A,0x004A,  0x4B,0x004B,  0x4C,0x004C,
  0x4D,0x004D,  0x4E,0x004E,  0x4F,0x004F,  0x50,0x0050,  0x51,0x0051,
  0x52,0x0052,  0x53,0x0053,  0x54,0x0054,  0x55,0x0055,  0x56,0x0056,
  0x57,0x0057,  0x58,0x0058,  0x59,0x0059,  0x5A,0x005A,  0x5B,0x005B,
// in icelandic : 5f='|o'
// in czech : 5f='s' (final 's' or begin word letter 'z')
  0x5C,0x005C,  0x5D,0x005D,  0x5E,0x005E,  0x5F,0x005F,  0x60,0x0060,
  0x61,0x0061,  0x62,0x0062,  0x63,0x0063,  0x64,0x0064,  0x65,0x0065,
  0x66,0x0066,  0x67,0x0067,  0x68,0x0068,  0x69,0x0069,  0x6A,0x006A,
  0x6B,0x006B,  0x6C,0x006C,  0x6D,0x006D,  0x6E,0x006E,  0x6F,0x006F,
  0x70,0x0070,  0x71,0x0071,  0x72,0x0072,  0x73,0x0073,  0x74,0x0074,
  0x75,0x0075,  0x76,0x0076,  0x77,0x0077,  0x78,0x0078,  0x79,0x0079,
  0x7A,0x007A,  0x7B,0x007B,  0x7C,0x007C,  0x7D,0x007D,  0x7E,0x007E,
  0x7F,0x007F,  0x80,0x00C4,  0x81,0x00C5,  0x82,0x00C7,  0x83,0x00C9,
  0x84,0x00D1,  0x85,0x00D6,  0x86,0x00DC,  0x87,0x00E1,  0x88,0x00E0,
  0x89,0x00E2,  0x8A,0x00E4,  0x8B,0x00E3,  0x8C,0x00E5,  0x8D,0x00E7,
  0x8E,0x00E9,  0x8F,0x00E8,  0x90,0x00EA,  0x91,0x00EB,  0x92,0x00ED,
  0x93,0x00EC,  0x94,0x00EE,  0x95,0x00EF,  0x96,0x00F1,  0x97,0x00F3,
  0x98,0x00F2,  0x99,0x00F4,  0x9A,0x00F6,  0x9B,0x00F5,  0x9C,0x00FA,
  0x9D,0x00F9,  0x9E,0x00FB,  0x9F,0x00FC,  0xA0,0x2020,  0xA1,0x00B0,
  0xA2,0x00A2,  0xA3,0x00A3,  0xA4,0x00A7,  0xA5,0x2022,  0xA6,0x00B6,
  0xA7,0x00DF,  0xA8,0x00AE,  0xA9,0x00A9,  0xAA,0x2122,  0xAB,0x00B4,
  0xAC,0x00A8,  0xAD,0x2260,  0xAE,0x00C6,  0xAF,0x00D8,  0xB0,0x221E,
  0xB1,0x00B1,  0xB2,0x2264,  0xB3,0x2265,  0xB4,0x00A5,  0xB5,0x00B5,
  0xB6,0x2202,  0xB7,0x2211,  0xB8,0x220F,  0xB9,0x03C0,  0xBA,0x222B,
  0xBB,0x00AA,  0xBC,0x00BA,  0xBD,0x03A9,  0xBE,0x00E6,  0xBF,0x00F8,
  0xC0,0x00BF,  0xC1,0x00A1,  0xC2,0x00AC,  0xC3,0x221A,  0xC4,0x0192,
  0xC5,0x2248,  0xC6,0x2206,  0xC7,0x00AB,  0xC8,0x00BB,  0xC9,0x2026,
  0xCA,0x00A0,  0xCB,0x00C0,  0xCC,0x00C3,  0xCD,0x00D5,  0xCE,0x0152,
  0xCF,0x0153,  0xD0,0x2013,  0xD1,0x2014,  0xD2,0x201C,  0xD3,0x201D,
  0xD4,0x2018,  0xD5,0x2019,  0xD6,0x00F7,  0xD7,0x25CA,  0xD8,0x00FF,
  0xD9,0x0178,  0xDA,0x2044,  0xDB,0x20AC,  0xDC,0x2039,  0xDD,0x203A,
  0xDE,0xFB01,  0xDF,0xFB02,  0xE0,0x2021,  0xE1,0x00B7,  0xE2,0x201A,
// E6 = non breaking space (instead of 0x00CA) ?
  0xE3,0x201E,  0xE4,0x2030,  0xE5,0x00C2,  0xE6,0x00CA,  0xE7,0x00C1,
  0xE8,0x00CB,  0xE9,0x00C8,  0xEA,0x00CD,  0xEB,0x00CE,  0xEC,0x00CF,
  0xED,0x00CC,  0xEE,0x00D3,  0xEF,0x00D4,  0xF0,0xF8FF,  0xF1,0x00D2,
  0xF2,0x00DA,  0xF3,0x00DB,  0xF4,0x00D9,  0xF5,0x0131,  0xF6,0x02C6,
  0xF7,0x02DC,  0xF8,0x00AF,  0xF9,0x02D8,  0xFA,0x02D9,  0xFB,0x02DA,
  0xFC,0x00B8,  0xFD,0x02DD,  0xFE,0x02DB,  0xFF,0x02C7
};

// Courtesy of unicode.org: http://unicode.org/Public/MAPPINGS/VENDORS/APPLE/SYMBOL.TXT
//! Internal and Low level: vector (char, unicode) for symbol font
static int const s_symbolUnicode[] = {
  0x20,0x0020,  0x21,0x0021,  0x22,0x2200,  0x23,0x0023,  0x24,0x2203,
  0x25,0x0025,  0x26,0x0026,  0x27,0x220D,  0x28,0x0028,  0x29,0x0029,
  0x2A,0x2217,  0x2B,0x002B,  0x2C,0x002C,  0x2D,0x2212,  0x2E,0x002E,
  0x2F,0x002F,  0x30,0x0030,  0x31,0x0031,  0x32,0x0032,  0x33,0x0033,
  0x34,0x0034,  0x35,0x0035,  0x36,0x0036,  0x37,0x0037,  0x38,0x0038,
  0x39,0x0039,  0x3A,0x003A,  0x3B,0x003B,  0x3C,0x003C,  0x3D,0x003D,
  0x3E,0x003E,  0x3F,0x003F,  0x40,0x2245,  0x41,0x0391,  0x42,0x0392,
  0x43,0x03A7,  0x44,0x0394,  0x45,0x0395,  0x46,0x03A6,  0x47,0x0393,
  0x48,0x0397,  0x49,0x0399,  0x4A,0x03D1,  0x4B,0x039A,  0x4C,0x039B,
  0x4D,0x039C,  0x4E,0x039D,  0x4F,0x039F,  0x50,0x03A0,  0x51,0x0398,
  0x52,0x03A1,  0x53,0x03A3,  0x54,0x03A4,  0x55,0x03A5,  0x56,0x03C2,
  0x57,0x03A9,  0x58,0x039E,  0x59,0x03A8,  0x5A,0x0396,  0x5B,0x005B,
  0x5C,0x2234,  0x5D,0x005D,  0x5E,0x22A5,  0x5F,0x005F,  0x60,0xF8E5,
  0x61,0x03B1,  0x62,0x03B2,  0x63,0x03C7,  0x64,0x03B4,  0x65,0x03B5,
  0x66,0x03C6,  0x67,0x03B3,  0x68,0x03B7,  0x69,0x03B9,  0x6A,0x03D5,
  0x6B,0x03BA,  0x6C,0x03BB,  0x6D,0x03BC,  0x6E,0x03BD,  0x6F,0x03BF,
  0x70,0x03C0,  0x71,0x03B8,  0x72,0x03C1,  0x73,0x03C3,  0x74,0x03C4,
  0x75,0x03C5,  0x76,0x03D6,  0x77,0x03C9,  0x78,0x03BE,  0x79,0x03C8,
  0x7A,0x03B6,  0x7B,0x007B,  0x7C,0x007C,  0x7D,0x007D,  0x7E,0x223C,
  0xA0,0x20AC,  0xA1,0x03D2,  0xA2,0x2032,  0xA3,0x2264,  0xA4,0x2044,
  0xA5,0x221E,  0xA6,0x0192,  0xA7,0x2663,  0xA8,0x2666,  0xA9,0x2665,
  0xAA,0x2660,  0xAB,0x2194,  0xAC,0x2190,  0xAD,0x2191,  0xAE,0x2192,
  0xAF,0x2193,  0xB0,0x00B0,  0xB1,0x00B1,  0xB2,0x2033,  0xB3,0x2265,
  0xB4,0x00D7,  0xB5,0x221D,  0xB6,0x2202,  0xB7,0x2022,  0xB8,0x00F7,
  0xB9,0x2260,  0xBA,0x2261,  0xBB,0x2248,  0xBC,0x2026,  0xBD,0x23D0,
  0xBE,0x23AF,  0xBF,0x21B5,  0xC0,0x2135,  0xC1,0x2111,  0xC2,0x211C,
  0xC3,0x2118,  0xC4,0x2297,  0xC5,0x2295,  0xC6,0x2205,  0xC7,0x2229,
  0xC8,0x222A,  0xC9,0x2283,  0xCA,0x2287,  0xCB,0x2284,  0xCC,0x2282,
  0xCD,0x2286,  0xCE,0x2208,  0xCF,0x2209,  0xD0,0x2220,  0xD1,0x2207,
  0xD2,0x00AE,  0xD3,0x00A9,  0xD4,0x2122,  0xD5,0x220F,  0xD6,0x221A,
  0xD7,0x22C5,  0xD8,0x00AC,  0xD9,0x2227,  0xDA,0x2228,  0xDB,0x21D4,
  0xDC,0x21D0,  0xDD,0x21D1,  0xDE,0x21D2,  0xDF,0x21D3,  0xE0,0x25CA,
  0xE1,0x3008,  0xE2,0x00AE,  0xE3,0x00A9,  0xE4,0x2122,  0xE5,0x2211,
  0xE6,0x239B,  0xE7,0x239C,  0xE8,0x239D,  0xE9,0x23A1,  0xEA,0x23A2,
  0xEB,0x23A3,  0xEC,0x23A7,  0xED,0x23A8,  0xEE,0x23A9,  0xEF,0x23AA,
  0xF0,0xF8FF,  0xF1,0x3009,  0xF2,0x222B,  0xF3,0x2320,  0xF4,0x23AE,
  0xF5,0x2321,  0xF6,0x239E,  0xF7,0x239F,  0xF8,0x23A0,  0xF9,0x23A4,
  0xFA,0x23A5,  0xFB,0x23A6,  0xFC,0x23AB,  0xFD,0x23AC,  0xFE,0x23AD
};

// Courtesy of unicode.org: http://unicode.org/Public/MAPPINGS/VENDORS/APPLE/DINGBATS.TXT
//! Internal and Low level: vector (char, unicode) for dingbats font
static int const s_dingbatsUnicode[] = {
  0x20,0x0020,  0x21,0x2701,  0x22,0x2702,  0x23,0x2703,  0x24,0x2704,
  0x25,0x260E,  0x26,0x2706,  0x27,0x2707,  0x28,0x2708,  0x29,0x2709,
  0x2A,0x261B,  0x2B,0x261E,  0x2C,0x270C,  0x2D,0x270D,  0x2E,0x270E,
  0x2F,0x270F,  0x30,0x2710,  0x31,0x2711,  0x32,0x2712,  0x33,0x2713,
  0x34,0x2714,  0x35,0x2715,  0x36,0x2716,  0x37,0x2717,  0x38,0x2718,
  0x39,0x2719,  0x3A,0x271A,  0x3B,0x271B,  0x3C,0x271C,  0x3D,0x271D,
  0x3E,0x271E,  0x3F,0x271F,  0x40,0x2720,  0x41,0x2721,  0x42,0x2722,
  0x43,0x2723,  0x44,0x2724,  0x45,0x2725,  0x46,0x2726,  0x47,0x2727,
  0x48,0x2605,  0x49,0x2729,  0x4A,0x272A,  0x4B,0x272B,  0x4C,0x272C,
  0x4D,0x272D,  0x4E,0x272E,  0x4F,0x272F,  0x50,0x2730,  0x51,0x2731,
  0x52,0x2732,  0x53,0x2733,  0x54,0x2734,  0x55,0x2735,  0x56,0x2736,
  0x57,0x2737,  0x58,0x2738,  0x59,0x2739,  0x5A,0x273A,  0x5B,0x273B,
  0x5C,0x273C,  0x5D,0x273D,  0x5E,0x273E,  0x5F,0x273F,  0x60,0x2740,
  0x61,0x2741,  0x62,0x2742,  0x63,0x2743,  0x64,0x2744,  0x65,0x2745,
  0x66,0x2746,  0x67,0x2747,  0x68,0x2748,  0x69,0x2749,  0x6A,0x274A,
  0x6B,0x274B,  0x6C,0x25CF,  0x6D,0x274D,  0x6E,0x25A0,  0x6F,0x274F,
  0x70,0x2750,  0x71,0x2751,  0x72,0x2752,  0x73,0x25B2,  0x74,0x25BC,
  0x75,0x25C6,  0x76,0x2756,  0x77,0x25D7,  0x78,0x2758,  0x79,0x2759,
  0x7A,0x275A,  0x7B,0x275B,  0x7C,0x275C,  0x7D,0x275D,  0x7E,0x275E,
  0x80,0x2768,  0x81,0x2769,  0x82,0x276A,  0x83,0x276B,  0x84,0x276C,
  0x85,0x276D,  0x86,0x276E,  0x87,0x276F,  0x88,0x2770,  0x89,0x2771,
  0x8A,0x2772,  0x8B,0x2773,  0x8C,0x2774,  0x8D,0x2775,  0xA1,0x2761,
  0xA2,0x2762,  0xA3,0x2763,  0xA4,0x2764,  0xA5,0x2765,  0xA6,0x2766,
  0xA7,0x2767,  0xA8,0x2663,  0xA9,0x2666,  0xAA,0x2665,  0xAB,0x2660,
  0xAC,0x2460,  0xAD,0x2461,  0xAE,0x2462,  0xAF,0x2463,  0xB0,0x2464,
  0xB1,0x2465,  0xB2,0x2466,  0xB3,0x2467,  0xB4,0x2468,  0xB5,0x2469,
  0xB6,0x2776,  0xB7,0x2777,  0xB8,0x2778,  0xB9,0x2779,  0xBA,0x277A,
  0xBB,0x277B,  0xBC,0x277C,  0xBD,0x277D,  0xBE,0x277E,  0xBF,0x277F,
  0xC0,0x2780,  0xC1,0x2781,  0xC2,0x2782,  0xC3,0x2783,  0xC4,0x2784,
  0xC5,0x2785,  0xC6,0x2786,  0xC7,0x2787,  0xC8,0x2788,  0xC9,0x2789,
  0xCA,0x278A,  0xCB,0x278B,  0xCC,0x278C,  0xCD,0x278D,  0xCE,0x278E,
  0xCF,0x278F,  0xD0,0x2790,  0xD1,0x2791,  0xD2,0x2792,  0xD3,0x2793,
  0xD4,0x2794,  0xD5,0x2192,  0xD6,0x2194,  0xD7,0x2195,  0xD8,0x2798,
  0xD9,0x2799,  0xDA,0x279A,  0xDB,0x279B,  0xDC,0x279C,  0xDD,0x279D,
  0xDE,0x279E,  0xDF,0x279F,  0xE0,0x27A0,  0xE1,0x27A1,  0xE2,0x27A2,
  0xE3,0x27A3,  0xE4,0x27A4,  0xE5,0x27A5,  0xE6,0x27A6,  0xE7,0x27A7,
  0xE8,0x27A8,  0xE9,0x27A9,  0xEA,0x27AA,  0xEB,0x27AB,  0xEC,0x27AC,
  0xED,0x27AD,  0xEE,0x27AE,  0xEF,0x27AF,  0xF1,0x27B1,  0xF2,0x27B2,
  0xF3,0x27B3,  0xF4,0x27B4,  0xF5,0x27B5,  0xF6,0x27B6,  0xF7,0x27B7,
  0xF8,0x27B8,  0xF9,0x27B9,  0xFA,0x27BA,  0xFB,0x27BB,  0xFC,0x27BC,
  0xFD,0x27BD,  0xFE,0x27BE
};

//
// For the next one, we stored only the "known" differences between roman and font
//
//! Internal and Low level: vector (char, unicode) for cursive font \note only characters which differs from roman
static int const s_cursiveIncompleteUnicode[] = {
  0xa2,0xBC, 0xa3,0x2153, 0xa4,0x2159, 0xaa,0xBD, 0xc1,0x2154
  // 0x40, 0xB0: some number,
};

//! Internal and Low level: vector (char, unicode) for math font \note only characters which differs from roman
static int const s_mathIncompleteUnicode[] = {
  0x22,0x222A, 0x27,0x222A,  0x28,0x2229, 0x2b,0x2260, 0x31,0x2282,
  0x33,0x2227, 0x36,0x2A2F,  0x39,0x2282, 0x43,0x2102, 0x44,0x216E,
  0x47,0x0393, 0x49,0x2160,  0x4e,0x2115, 0x52,0x211D, 0x5a,0x2124,
  0x63,0x0255, 0x64,0x03B4,  0x65,0x212F, 0x68,0x210E, 0x70,0x01BF,
  0x76,0x2174
};

//! Internal and Low level: vector (char, unicode) for scientific font \note only characters which differs from roman
static int const s_scientificIncompleteUnicode[] = {
  0x23,0x0394, 0x40,0x221A,  0x49,0x2160, 0x56,0x2164, 0x5c,0x007C,
  0x5b,0x2192, 0x5d,0x2192,  0x90,0x211D, 0x91,0x2192, 0xa7,0x03C3,
  0xa9,0x03B3, 0xab,0x03F5,  0xad,0x2260, 0xc3,0x03C8, 0xcf,0x03B8,
  0xd1,0x223C, 0xd4,0x2192,  0xe3,0x2009, 0xe8,0x00a0, 0xec,0x2303,
  0xee,0x2227, 0xef,0x0305,  0xf2,0x2192
};

// other fonts with unknown names :
//! Internal and Low level: vector (char, unicode) for font 107 \note only characters which differs from roman
static int const s_unknown107IncompleteUnicode[] = {
  0x76,0x221a
};
//! Internal and Low level: vector (char, unicode) for font 128 \note only characters which differs from roman
static int const s_unknown128IncompleteUnicode[] = {
  0x43,0x2102, 0x4e,0x2115, 0x52,0x211D, 0x61,0xFE3F, 0x76,0x2192
};
//! Internal and Low level: vector (char, unicode) for font 200 \note only characters which differs from roman
static int const s_unknown200IncompleteUnicode[] = {
  0x76,0x2192, 0x77,0x2192, 0x61,0xFE3F
};


class KnownConversion
{
public:
  //! constructor
  KnownConversion() : m_convertMap(), m_romanMap(), m_symbolMap(), m_dingbatsMap(), m_cursiveMap(),
    m_mathMap(), m_scientificMap(), m_unknown107Map(), m_unknown128Map(),
    m_unknown200Map(),
    m_defaultConv(m_romanMap), m_timeConv(m_romanMap,"Times New Roman"),
    m_zapfChanceryConv(m_romanMap,"Apple Chancery", -2), m_symbolConv(m_symbolMap),
    m_dingbatsConv(m_dingbatsMap), m_cursiveConv(m_cursiveMap,"Apple Chancery"),
    m_mathConv(m_mathMap), m_scientificConv(m_scientificMap),
    m_unknown107Conv(m_unknown107Map), m_unknown128Conv(m_unknown128Map),
    m_unknown200Conv(m_unknown200Map) {
    initMaps();
  }
  //! returns the conversion map which corresponds to a name, or the default map
  Data::ConversionData const &getConversionMaps(std::string fName);

  // FIXME:

  //! return the default convertissor
  ConversionData const &getDefault() const {
    return m_defaultConv;
  }
protected:
  //! Internal and Low level: initializes all the conversion maps
  void initMaps();

  //! Internal and Low level: initializes a map with a vector of \a numElt elements (char, unicode)
  static void initAMap(int const *arr, int numElt, std::map<unsigned char, unsigned long> &map) {
    for (size_t i = 0; i < size_t(numElt); i++) {
      unsigned char c = (unsigned char)arr[2*i];
      unsigned long unicode = (unsigned long)arr[2*i+1];
      map[c] = unicode;
    }
  }

  /** the conversiont map fName -> ConversionData */
  std::map<std::string, ConversionData const *> m_convertMap;

  //! Internal and Low level: map char -> unicode for roman font
  std::map<unsigned char, unsigned long> m_romanMap;
  //! Internal and Low level: map char -> unicode for symbol font
  std::map<unsigned char, unsigned long> m_symbolMap;
  //! Internal and Low level: map char -> unicode for dingbats font
  std::map<unsigned char, unsigned long> m_dingbatsMap;
  //! Internal and Low level: map char -> unicode for cursive font
  std::map<unsigned char, unsigned long> m_cursiveMap;
  //! Internal and Low level: map char -> unicode for math font
  std::map<unsigned char, unsigned long> m_mathMap;
  //! Internal and Low level: map char -> unicode for scientific font
  std::map<unsigned char, unsigned long> m_scientificMap;
  //! Internal and Low level: map char -> unicode for font 107
  std::map<unsigned char, unsigned long> m_unknown107Map;
  //! Internal and Low level: map char -> unicode for font 128
  std::map<unsigned char, unsigned long> m_unknown128Map;
  //! Internal and Low level: map char -> unicode for font 200
  std::map<unsigned char, unsigned long> m_unknown200Map;

  //! Internal and Low level: the default convertissor: roman
  ConversionData m_defaultConv;
  //! Internal and Low level: the convertissor for times font
  ConversionData m_timeConv;
  //! Internal and Low level: the convertissor for zapfChancery font
  ConversionData m_zapfChanceryConv;
  //! Internal and Low level: the convertissor for symbol font
  ConversionData m_symbolConv;
  //! Internal and Low level: the convertissor for dingbats font
  ConversionData m_dingbatsConv;
  //! Internal and Low level: the convertissor for cursive font
  ConversionData m_cursiveConv;
  //! Internal and Low level: the convertissor for math font
  ConversionData m_mathConv;
  //! Internal and Low level: the convertissor for scientific font
  ConversionData m_scientificConv;
  //! Internal and Low level: the convertissor for font 107
  ConversionData m_unknown107Conv;
  //! Internal and Low level: the convertissor for font 128
  ConversionData m_unknown128Conv;
  //! Internal and Low level: the convertissor for font 200
  ConversionData m_unknown200Conv;
};


//! Internal and Low level: initializes all the conversion maps
void KnownConversion::initMaps()
{
  int numRoman = sizeof(s_romanUnicode)/(2*sizeof(int));
  for (size_t i = 0; i < size_t(numRoman); i++) {
    unsigned char c = (unsigned char)s_romanUnicode[2*i];
    unsigned long unicode = (unsigned long)s_romanUnicode[2*i+1];
    m_romanMap[c] = unicode;
    m_cursiveMap[c] = m_mathMap[c] = m_scientificMap[c] = unicode;
    m_unknown107Map[c] = m_unknown128Map[c] = m_unknown200Map[c] = unicode;
  }

  initAMap(s_symbolUnicode, sizeof(s_symbolUnicode)/(2*sizeof(int)), m_symbolMap);
  initAMap(s_dingbatsUnicode, sizeof(s_dingbatsUnicode)/(2*sizeof(int)), m_dingbatsMap);
  initAMap(s_cursiveIncompleteUnicode, sizeof(s_cursiveIncompleteUnicode)/(2*sizeof(int)), m_cursiveMap);
  initAMap(s_mathIncompleteUnicode, sizeof(s_mathIncompleteUnicode)/(2*sizeof(int)), m_mathMap);
  initAMap(s_scientificIncompleteUnicode, sizeof(s_scientificIncompleteUnicode)/(2*sizeof(int)),m_scientificMap);
  initAMap(s_unknown107IncompleteUnicode, sizeof(s_unknown107IncompleteUnicode)/(2*sizeof(int)),m_unknown107Map);
  initAMap(s_unknown128IncompleteUnicode, sizeof(s_unknown128IncompleteUnicode)/(2*sizeof(int)),m_unknown128Map);
  initAMap(s_unknown200IncompleteUnicode, sizeof(s_unknown200IncompleteUnicode)/(2*sizeof(int)),m_unknown200Map);

  // init convertMap
  m_convertMap[std::string("Default")] = &m_defaultConv;
  m_convertMap[std::string("Times")] = &m_timeConv;
  m_convertMap[std::string("Zapf Chancery")] = &m_zapfChanceryConv;
  m_convertMap[std::string("Symbol")] = &m_symbolConv;
  m_convertMap[std::string("Zapf Dingbats")] = &m_dingbatsConv;
  m_convertMap[std::string("Cursive")] = &m_cursiveConv;
  m_convertMap[std::string("Math")] = &m_mathConv;
  m_convertMap[std::string("scientific")] = &m_scientificConv;
  m_convertMap[std::string("Unknown107")] = &m_unknown107Conv;
  m_convertMap[std::string("Unknown128")] = &m_unknown128Conv;
  m_convertMap[std::string("Unknown200")] = &m_unknown200Conv;
}

ConversionData const &KnownConversion::getConversionMaps(std::string fName)
{
  if (fName.empty()) return m_defaultConv;
  std::map<std::string, ConversionData const *>::iterator it= m_convertMap.find(fName);
  if (it == m_convertMap.end()) return m_defaultConv;
  return *(it->second);
}

}

//------------------------------------------------------------
//
// Font convertor imlementation
//
//------------------------------------------------------------
//! the default font converter
class State
{
public:
  //! the constructor
  State() : m_knownConversion(), m_idNameMap(), m_nameIdMap(),
    m_nameIdCounter(0), m_uniqueId(256), m_unicodeCache() {
    initMaps();
  }

  /** returns the identificator for a name,
  if not set creates one */
  int getId(std::string const &name) {
    if (name.empty()) return -1;
    std::map<std::string,int>::iterator it=m_nameIdMap.find(name);
    if (it != m_nameIdMap.end()) return it->second;
    while (m_idNameMap.find(m_uniqueId)!=m_idNameMap.end())
      m_uniqueId++;
    setCorrespondance(m_uniqueId, name);
    return m_uniqueId++;
  }

  //! returns the name corresponding to an id or return std::string("")
  std::string getName(int macId) {
    std::map<int, std::string>::iterator it=m_idNameMap.find(macId);
    if (it==m_idNameMap.end()) return "";
    return it->second;
  }

  /* converts a character in unicode
     \return -1 if the character is not transformed */
  int unicode(int macId, unsigned char c);

  /** final font name and a delta which can be used to change the size
  if no name is found, return "Times New Roman" */
  void getOdtInfo(int macId, std::string &nm, int &deltaSize);

  //! fixes the name corresponding to an id
  void setCorrespondance(int macId, std::string const &name) {
    m_idNameMap[macId] = name;
    m_nameIdMap[name] = macId;
    m_nameIdCounter++;
  }

protected:
  //! initializes the map
  void initMaps();

  //! the basic conversion map
  MWAWFontConverterInternal::Data::KnownConversion m_knownConversion;
  //! map sysid -> font name
  std::map<int, std::string> m_idNameMap;
  //! map font name -> sysid
  std::map<std::string, int> m_nameIdMap;

  //!Internal: a counter modified when a new correspondance name<->id is found
  long m_nameIdCounter;

  //! a int used to create new id for a name
  int m_uniqueId;

  //! small structure to speedup unicode
  struct UnicodeCache {
    //! constructor
    UnicodeCache() : m_nameIdCounter(-1), m_macId(-1), m_conv(0) {}
    //! actual counter
    long m_nameIdCounter;
    //! actual macId
    int m_macId;
    //! actual convertor
    MWAWFontConverterInternal::Data::ConversionData const *m_conv;
  } m_unicodeCache;

};

// initializes the default conversion map
void State::initMaps()
{
  // see http://developer.apple.com/documentation/mac/Text/Text-277.html
  // or Apple II Technical Notes #41 (  http://www.umich.edu/~archive/apple2/technotes/tn/iigs/TN.IIGS.041 )
  // 0 system fonts, 1 appli fonts
  m_idNameMap[2] = "NewYork";
  m_idNameMap[3] = "Geneva";
  m_idNameMap[4] = "Monaco";
  m_idNameMap[5] = "Venise";
  m_idNameMap[6] = "London";
  m_idNameMap[7] = "Athens";
  m_idNameMap[8] = "SanFran";
  m_idNameMap[9] = "Toronto";

  m_idNameMap[11] = "Cairo";
  m_idNameMap[12] = "LosAngeles";
  m_idNameMap[13] = "Zapf Dingbats";
  m_idNameMap[14] = "Bookman";
  m_idNameMap[16] = "Palatino";
  m_idNameMap[18] = "Zapf Chancery";
  m_idNameMap[20] = "Times";
  m_idNameMap[21] = "Helvetica";
  m_idNameMap[22] = "Courier";
  m_idNameMap[23] = "Symbol";
  m_idNameMap[24] = "Mobile"; // or Taliesin: apple 2

  // ------- Apple II Technical Notes #41
  m_idNameMap[33] = "Avant Garde";
  m_idNameMap[34] = "New Century Schoolbook";

  // ------- Osnola: from a personal computer
  m_idNameMap[150] = "scientific";
  m_idNameMap[157] = "Cursive";
  m_idNameMap[201] = "Math";

  // ------- Osnola: unknown name
  m_idNameMap[107] = "Unknown107";
  m_idNameMap[128] = "Unknown128";
  m_idNameMap[200] = "Unknown200";

  // ------- fixme: find encoding
  m_idNameMap[174] = "Futura";
  m_idNameMap[258] = "ProFont";
  m_idNameMap[513] = "ISO Latin Nr 1";
  m_idNameMap[514] = "PCFont 437";
  m_idNameMap[515] = "PCFont 850";
  m_idNameMap[1029] = "VT80 Graphics";
  m_idNameMap[1030] = "3270 Graphics";
  m_idNameMap[1109] = "Trebuchet MS";
  m_idNameMap[1345] = "ProFont";
  m_idNameMap[1895] = "Nu Sans Regular";
  m_idNameMap[2001] = "Arial";
  m_idNameMap[2002] = "Charcoal";
  m_idNameMap[2004] = "Sand";
  m_idNameMap[2005] = "Courier New";
  m_idNameMap[2006] = "Techno";
  m_idNameMap[2010] = "Times New Roman";
  m_idNameMap[2011] = "Wingdings";
  m_idNameMap[2013] = "Hoefler Text";
  m_idNameMap[2018] = "Hoefler Text Ornaments";
  m_idNameMap[2039] = "Impact";
  m_idNameMap[2041] = "Mistral";
  m_idNameMap[2305] = "Textile";
  m_idNameMap[2307] = "Gadget";
  m_idNameMap[2311] = "Apple Chancery";
  m_idNameMap[2515] = "MT Extra";
  m_idNameMap[4513] = "Comic Sans MS";
  m_idNameMap[7092] = "Monotype.com";
  m_idNameMap[7102] = "Andale Mono";
  m_idNameMap[7203] = "Verdenal";
  m_idNameMap[9728] = "Espi Sans";
  m_idNameMap[9729] = "Charcoal";
  m_idNameMap[9840] = "Espy Sans/Copland";
  m_idNameMap[9841] = "Espy Sans/Bold";
  m_idNameMap[9842] = "Espy Sans Bold/Copland";
  m_idNameMap[10840] = "Klang MT";
  m_idNameMap[10890] = "Script MT Bold";
  m_idNameMap[10897] = "Old English Text MT";
  m_idNameMap[10909] = "New Berolina MT";
  m_idNameMap[10957] = "Bodoni MT Ultra Bold";
  m_idNameMap[10967] = "Arial MT Condensed Light";
  m_idNameMap[11103] = "Lydian MT";

  std::map<int, std::string>::iterator it;
  for(it = m_idNameMap.begin(); it != m_idNameMap.end(); it++)
    m_nameIdMap[it->second] = it->first;
}

// returns an unicode caracter
int State::unicode(int macId, unsigned char c)
{
  if (!m_unicodeCache.m_conv || m_unicodeCache.m_macId != macId ||  m_unicodeCache.m_nameIdCounter != m_nameIdCounter) {
    m_unicodeCache.m_macId = macId;
    m_unicodeCache.m_nameIdCounter = m_nameIdCounter;
    m_unicodeCache.m_conv = &m_knownConversion.getConversionMaps(getName(macId));
  }
  if (!m_unicodeCache.m_conv) {
    MWAW_DEBUG_MSG(("unicode Error: can not find a convertor\n"));
    return -1;
  }
  std::map<unsigned char, unsigned long>::iterator it = m_unicodeCache.m_conv->m_conversion.find(c);

  if (it == m_unicodeCache.m_conv->m_conversion.end()) return -1;
  return (int) it->second;
}

void State::getOdtInfo(int macId, std::string &nm, int &deltaSize)
{
  std::string nam = getName(macId);
  MWAWFontConverterInternal::Data::ConversionData const *conv = &m_knownConversion.getConversionMaps(nam);

  nm = conv->m_name;
  deltaSize = conv->m_deltaSize;

  if (!nm.empty()) return;
  if (!nam.empty()) {
    nm = nam;
    return;
  }
#ifdef DEBUG
  static int lastUnknownId = -1;
  if (macId != lastUnknownId) {
    lastUnknownId = macId;
    MWAW_DEBUG_MSG(("Unknown font with id=%d\n",macId));
  }
#endif
  nm = "Times New Roman";
}
}

MWAWFontConverter::MWAWFontConverter() : m_manager(new MWAWFontConverterInternal::State) { }
MWAWFontConverter::~MWAWFontConverter() {}

// mac font name <-> id functions
void MWAWFontConverter::setCorrespondance(int macId, std::string const &name)
{
  std::string fName("");
  static bool first = true;
  for (size_t c = 0; c < name.length(); c++) {
    unsigned char ch = (unsigned char)name[c];
    if (ch > 0x1f && ch < 0x80) {
      fName+=name[c];
      continue;
    }
    if (first) {
      MWAW_DEBUG_MSG(("MWAWFontConverter::setCorrespondance: fontName contains bad character\n"));
      first = false;
    }
    fName += 'X';
  }
  m_manager->setCorrespondance(macId, fName);
}
int MWAWFontConverter::getId(std::string const &name)  const
{
  return m_manager->getId(name);
}

std::string MWAWFontConverter::getName(int macId) const
{
  return m_manager->getName(macId);
}

void MWAWFontConverter::getOdtInfo(int macId, std::string &nm, int &deltaSize) const
{
  m_manager->getOdtInfo(macId, nm, deltaSize);
}

int MWAWFontConverter::unicode(int macId, unsigned char c) const
{
  if (c < 0x20) return -1;
  return m_manager->unicode(macId, c);
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
