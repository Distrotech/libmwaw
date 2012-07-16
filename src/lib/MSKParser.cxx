/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */
/* libmwaw
 * Copyright (C) 2009, 2011 Alonso Laurent (alonso@loria.fr)
 * Copyright (C) 2006, 2007 Andrew Ziem
 * Copyright (C) 2004-2006 Fridrich Strba (fridrich.strba@bluewin.ch)
 * Copyright (C) 2004 Marc Maurer (uwog@uwog.net)
 * Copyright (C) 2003-2005 William Lachance (william.lachance@sympatico.ca)
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
 * For further information visit http://libmwaw.sourceforge.net
 */

/* "This product is not manufactured, approved, or supported by
 * Corel Corporation or Corel Corporation Limited."
 */

#include "MWAWContentListener.hxx"
#include "MWAWSubDocument.hxx"

#include "MSKParser.hxx"

/** Internal: the structures of a MSKParser */
namespace MSKParserInternal
{
////////////////////////////////////////
//! Internal: the state of a MSK3Parser
struct State {
  //! constructor
  State() : m_eof(-1) {
  }

  //! the last known file position
  long m_eof;
};
}

MSKParser::MSKParser(MWAWInputStreamPtr input, MWAWHeader *header) :
  MWAWParser(input, header), m_listener(), m_state(new MSKParserInternal::State)
{
}

MSKParser::~MSKParser()
{
}

void MSKParser::sendFrameText(MWAWEntry const &, std::string const &)
{
  MWAW_DEBUG_MSG(("MSKParser::sendFrameText: must not be called\n"));
  if (!m_listener) return;
  m_listener->insertCharacter(' ');
}

void MSKParser::sendOLE(int, MWAWPosition const &, WPXPropertyList)
{
  MWAW_DEBUG_MSG(("MSKParser::sendOLE: must not be called\n"));
}

bool MSKParser::checkIfPositionValid(long pos)
{
  if (pos <= m_state->m_eof)
    return true;
  MWAWInputStreamPtr input = getInput();
  long actPos = input->tell();
  input->seek(pos, WPX_SEEK_SET);
  bool ok = long(input->tell())==pos;
  if (ok) m_state->m_eof = pos;

  input->seek(actPos, WPX_SEEK_SET);
  return ok;
}

std::vector<Vec3uc> const &MSKParser::getPalette(int vers)
{
  switch(vers) {
  case 2: {
    static std::vector<Vec3uc> palette;
    palette.resize(9);
    palette[0]=Vec3uc(0,0,0); // undef
    palette[1]=Vec3uc(0,0,0);
    palette[2]=Vec3uc(255,255,255);
    palette[3]=Vec3uc(255,0,0);
    palette[4]=Vec3uc(0,255,0);
    palette[5]=Vec3uc(0,0,255);
    palette[6]=Vec3uc(0, 255,255);
    palette[7]=Vec3uc(255,0,255);
    palette[8]=Vec3uc(255,255,0);
    return palette;
  }
  case 3: {
    static std::vector<Vec3uc> palette;
    if (palette.size()==0) {
      palette.resize(256);
      size_t ind=0;
      for (int k = 0; k < 6; k++) {
        for (int j = 0; j < 6; j++) {
          for (int i = 0; i < 6; i++, ind++) {
            if (j==5 && i==2) break;
            palette[ind]=Vec3uc((unsigned char)(255-51*i), (unsigned char)(255-51*k), (unsigned char)(255-51*j));
          }
        }
      }

      // the last 2 lines
      for (int r = 0; r < 2; r++) {
        // the black, red, green, blue zone of 5*2
        for (int c = 0; c < 4; c++) {
          for (int i = 0; i < 5; i++, ind++) {
            int val = 17*r+51*i;
            if (c == 0) {
              palette[ind]=Vec3uc((unsigned char)val, (unsigned char)val, (unsigned char)val);
              continue;
            }
            int color[3]= {0,0,0};
            color[c-1]=val;
            palette[ind]=Vec3uc((unsigned char)(color[0]),(unsigned char)(color[1]),(unsigned char)(color[2]));
          }
        }
        // last part of j==5, i=2..5
        for (int k = r; k < 6; k+=2) {
          for (int i = 2; i < 6; i++, ind++)
            palette[ind]=Vec3uc((unsigned char)(255-51*i), (unsigned char)(255-51*k), (unsigned char)(255-51*5));
        }
      }
    }
    return palette;
  }
  case 4:  {
    static std::vector<Vec3uc> palette;
    if (palette.size()==0) {
      palette.resize(256);
      size_t ind=0;
      for (int k = 0; k < 6; k++) {
        for (int j = 0; j < 6; j++) {
          for (int i = 0; i < 6; i++, ind++) {
            palette[ind]=
              Vec3uc((unsigned char)(255-51*k), (unsigned char)(255-51*j),
                     (unsigned char)(255-51*i));
          }
        }
      }
      ind--; // remove the black color
      for (int c = 0; c < 4; c++) {
        unsigned char color[3] = {0,0,0};
        unsigned char val=(unsigned char) 251;
        for (int i = 0; i < 10; i++) {
          val = (unsigned char)(val-17);
          if (c == 3) palette[ind++]=Vec3uc(val, val, val);
          else {
            color[c] = val;
            palette[ind++]=Vec3uc(color[0],color[1],color[2]);
          }
          if ((i%2)==1) val = (unsigned char)(val-17);
        }
      }

      // last is black
      palette[ind++]=Vec3uc(0,0,0);
    }
    return palette;
  }
  default:
    break;
  }
  MWAW_DEBUG_MSG(("MSKParser::getPalette: can not find palette for version %d\n", vers));
  static std::vector<Vec3uc> emptyPalette;
  return emptyPalette;
}

bool MSKParser::getColor(int id, Vec3uc &col, int vers) const
{
  if (vers <= 0) vers = version();
  std::vector<Vec3uc> const &palette = getPalette(vers);
  if (palette.size()==0 || id < 0 || id >= int(palette.size()) ||
      (version()==2 && id==0)) {
    static bool first = true;
    if (first) {
      MWAW_DEBUG_MSG(("MSKParser::getColor: unknown color=%d\n", id));
      first = false;
    }
    return false;
  }
  col = palette[size_t(id)];
  return true;
}
