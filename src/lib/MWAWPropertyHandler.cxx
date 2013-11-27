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

/* This header contains code specific to a small picture
 */
#include <iostream>
#include <sstream>
#include <string.h>

#include <stack>

#include "libmwaw_internal.hxx"

#include <librevenge/librevenge.h>
#include <librevenge-stream/librevenge-stream.h>
#include <libmwaw/libmwaw.hxx>

#include "MWAWPropertyHandler.hxx"

////////////////////////////////////////////////////
//
// MWAWPropertyHandlerEncoder
//
////////////////////////////////////////////////////
MWAWPropertyHandlerEncoder::MWAWPropertyHandlerEncoder()
  : m_f(std::ios::in | std::ios::out | std::ios::binary)
{
}

void MWAWPropertyHandlerEncoder::insertElement(const char *psName)
{
  m_f << 'E';
  writeString(psName);
}

void MWAWPropertyHandlerEncoder::insertElement
(const char *psName, const librevenge::RVNGPropertyList &xPropList)
{
  m_f << 'S';
  writeString(psName);
  writePropertyList(xPropList);
}

void MWAWPropertyHandlerEncoder::insertElement
(const char *psName, const librevenge::RVNGPropertyList &xPropList, const librevenge::RVNGPropertyListVector &vect)
{
  m_f << 'V';
  writeString(psName);
  writePropertyList(xPropList);
  writeInteger((int)vect.count());
  for (unsigned long i=0; i < vect.count(); i++)
    writePropertyList(vect[i]);
}

void MWAWPropertyHandlerEncoder::characters(librevenge::RVNGString const &sCharacters)
{
  if (sCharacters.len()==0) return;
  m_f << 'T';
  writeString(sCharacters);
}

void MWAWPropertyHandlerEncoder::writeString(const librevenge::RVNGString &string)
{
  unsigned long sz = string.size()+1;
  writeInteger((int) sz);
  m_f.write(string.cstr(), (int) sz);
}

void MWAWPropertyHandlerEncoder::writeLong(long val)
{
  int32_t value=(int32_t) val;
  unsigned char const allValue[]= {(unsigned char)(value&0xFF), (unsigned char)((value>>8)&0xFF), (unsigned char)((value>>16)&0xFF), (unsigned char)((value>>24)&0xFF)};
  m_f.write((const char *)allValue, 4);
}

void MWAWPropertyHandlerEncoder::writeProperty(const char *key, const librevenge::RVNGProperty &prop)
{
  if (!key) {
    MWAW_DEBUG_MSG(("MWAWPropertyHandlerEncoder::writeProperty: key is NULL\n"));
    return;
  }
  writeString(key);
  writeString(prop.getStr());
}

void MWAWPropertyHandlerEncoder::writePropertyList(const librevenge::RVNGPropertyList &xPropList)
{
  librevenge::RVNGPropertyList::Iter i(xPropList);
  int numElt = 0;
  for (i.rewind(); i.next();) numElt++;
  writeInteger(numElt);
  for (i.rewind(); i.next();)
    writeProperty(i.key(),*i());
}

bool MWAWPropertyHandlerEncoder::getData(librevenge::RVNGBinaryData &data)
{
  data.clear();
  std::string d=m_f.str();
  if (d.length() == 0) return false;
  data.append((const unsigned char *)d.c_str(), d.length());
  return true;
}

/* \brief Internal: the property decoder
 *
 * \note see MWAWPropertyHandlerEncoder for the format
*/
class MWAWPropertyHandlerDecoder
{
public:
  //! constructor given a MWAWPropertyHandler
  MWAWPropertyHandlerDecoder(MWAWPropertyHandler *hdl=0L):m_handler(hdl) {}

  //! tries to read the data
  bool readData(librevenge::RVNGBinaryData const &encoded)
  {
    try {
      librevenge::RVNGInputStream *inp = const_cast<librevenge::RVNGInputStream *>(encoded.getDataStream());
      if (!inp) return false;

      while (!inp->isEnd()) {
        unsigned const char *c;
        unsigned long numRead;

        c = inp->read(1,numRead);
        if (!c || numRead != 1) {
          MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder: can not read data type \n"));
          return false;
        }
        switch (*c) {
        case 'E':
          if (!readInsertElement(*inp)) return false;
          break;
        case 'S':
          if (!readInsertElementWithList(*inp)) return false;
          break;
        case 'V':
          if (!readInsertElementWithVector(*inp)) return false;
          break;
        case 'T':
          if (!readCharacters(*inp)) return false;
          break;
        default:
          MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder: unknown type='%c' \n", *c));
          return false;
        }
      }
    }
    catch (...) {
      return false;
    }
    return true;
  }

protected:
  //! reads an simple element
  bool readInsertElement(librevenge::RVNGInputStream &input)
  {
    librevenge::RVNGString s;
    if (!readString(input, s)) return false;

    if (s.empty()) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readInsertElement find empty tag\n"));
      return false;
    }
    if (m_handler) m_handler->insertElement(s.cstr());
    return true;
  }

  //! reads an element with a property list
  bool readInsertElementWithList(librevenge::RVNGInputStream &input)
  {
    librevenge::RVNGString s;
    if (!readString(input, s)) return false;

    if (s.empty()) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readInsertElementWithProperty: find empty tag\n"));
      return false;
    }
    librevenge::RVNGPropertyList lists;
    if (!readPropertyList(input, lists)) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readInsertElementWithProperty: can not read propertyList for tag %s\n",
                      s.cstr()));
      return false;
    }

    if (m_handler) m_handler->insertElement(s.cstr(), lists);
    return true;
  }

  //! reads an insertElement
  bool readInsertElementWithVector(librevenge::RVNGInputStream &input)
  {
    librevenge::RVNGString s;
    if (!readString(input, s)) return false;
    if (s.empty()) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readInsertElementWithVector: can not read tag name\n"));
      return false;
    }

    librevenge::RVNGPropertyList lists;
    if (!readPropertyList(input, lists)) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readInsertElementWithVector: can not read propertyList for tag %s\n",
                      s.cstr()));
      return false;
    }
    librevenge::RVNGPropertyListVector vect;
    if (!readPropertyListVector(input, vect)) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readInsertElementWithVector: can not read propertyVector for tag %s\n",
                      s.cstr()));
      return false;
    }

    if (m_handler) m_handler->insertElement(s.cstr(), lists, vect);
    return true;
  }

  //! reads a set of characters
  bool readCharacters(librevenge::RVNGInputStream &input)
  {
    librevenge::RVNGString s;
    if (!readString(input, s)) return false;
    if (!s.size()) return true;
    if (m_handler) m_handler->characters(s);
    return true;
  }

  //
  // low level
  //

  //! low level: reads a property vector: number of properties list followed by list of properties list
  bool readPropertyListVector(librevenge::RVNGInputStream &input, librevenge::RVNGPropertyListVector &vect)
  {
    int numElt;
    if (!readInteger(input, numElt)) return false;

    if (numElt < 0) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readPropertyListVector: can not read numElt=%d\n",
                      numElt));
      return false;
    }
    for (int i = 0; i < numElt; i++) {
      librevenge::RVNGPropertyList lists;
      if (readPropertyList(input, lists)) {
        vect.append(lists);
        continue;
      }
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readPropertyListVector: can not read property list %d\n", i));
      return false;
    }
    return true;
  }

  //! low level: reads a property list: number of properties followed by list of properties
  bool readPropertyList(librevenge::RVNGInputStream &input, librevenge::RVNGPropertyList &lists)
  {
    int numElt;
    if (!readInteger(input, numElt)) return false;

    if (numElt < 0) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readPropertyList: can not read numElt=%d\n",
                      numElt));
      return false;
    }
    for (int i = 0; i < numElt; i++) {
      if (readProperty(input, lists)) continue;
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readPropertyList: can not read property %d\n", i));
      return false;
    }
    return true;
  }

  //! low level: reads a property and its value, adds it to \a list
  bool readProperty(librevenge::RVNGInputStream &input, librevenge::RVNGPropertyList &list)
  {
    librevenge::RVNGString key, val;
    if (!readString(input, key)) return false;
    if (!readString(input, val)) return false;

    list.insert(key.cstr(), val);
    librevenge::RVNGProperty const *prop=list[key.cstr()];
    if (!prop) return true;
    librevenge::RVNGUnit unit=prop->getUnit();
    if (unit==librevenge::RVNG_POINT)
      list.insert(key.cstr(), prop->getDouble()/72., librevenge::RVNG_INCH);
    else if (unit==librevenge::RVNG_TWIP)
      list.insert(key.cstr(), prop->getDouble()/1440., librevenge::RVNG_INCH);
    return true;
  }

  //! low level: reads a string : size and string
  bool readString(librevenge::RVNGInputStream &input, librevenge::RVNGString &s)
  {
    int numC = 0;
    if (!readInteger(input, numC)) return false;
    if (numC==0) {
      s = librevenge::RVNGString("");
      return true;
    }
    unsigned long numRead;
    const unsigned char *dt = input.read((unsigned long)numC, numRead);
    if (dt == 0L || numRead != (unsigned long) numC) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readString: can not read a string\n"));
      return false;
    }
    s = librevenge::RVNGString((const char *)dt);
    return true;
  }

  //! low level: reads an integer value
  static bool readInteger(librevenge::RVNGInputStream &input, int &val)
  {
    long res;
    if (!readLong(input, res))
      return false;
    val=int(res);
    return true;
  }
  //! low level: reads an long value
  static bool readLong(librevenge::RVNGInputStream &input, long &val)
  {
    unsigned long numRead = 0;
    const unsigned char *dt = input.read(4, numRead);
    if (dt == 0L || numRead != 4) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readInteger: can not read int\n"));
      return false;
    }
    val = long((dt[3]<<24)|(dt[2]<<16)|(dt[1]<<8)|dt[0]);
    return true;
  }
private:
  MWAWPropertyHandlerDecoder(MWAWPropertyHandlerDecoder const &orig);
  MWAWPropertyHandlerDecoder &operator=(MWAWPropertyHandlerDecoder const &);

protected:
  //! the streamfile
  MWAWPropertyHandler *m_handler;
};

////////////////////////////////////////////////////
//
// MWAWPropertyHandler
//
////////////////////////////////////////////////////
bool MWAWPropertyHandler::checkData(librevenge::RVNGBinaryData const &encoded)
{
  MWAWPropertyHandlerDecoder decod;
  return decod.readData(encoded);
}

bool MWAWPropertyHandler::readData(librevenge::RVNGBinaryData const &encoded)
{
  MWAWPropertyHandlerDecoder decod(this);
  return decod.readData(encoded);
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
