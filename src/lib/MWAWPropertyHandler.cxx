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
(const char *psName, const RVNGPropertyList &xPropList)
{
  m_f << 'S';
  writeString(psName);
  writePropertyList(xPropList);
}

void MWAWPropertyHandlerEncoder::insertElement
(const char *psName, const RVNGPropertyList &xPropList, const RVNGPropertyListVector &vect)
{
  m_f << 'V';
  writeString(psName);
  writePropertyList(xPropList);
  writeInteger((int)vect.count());
  for (unsigned long i=0; i < vect.count(); i++)
    writePropertyList(vect[i]);
}

void MWAWPropertyHandlerEncoder::insertElement
(const char *psName, const RVNGPropertyList &xPropList, const RVNGBinaryData &data)
{
  m_f << 'B';
  writeString(psName);
  writePropertyList(xPropList);
  long size=(long) data.size();
  if (size<0) {
    MWAW_DEBUG_MSG(("MWAWPropertyHandlerEncoder::insertElement: oops, probably the binary data is too big!!!\n"));
    size=0;
  }
  writeLong(size);
  if (size>0)
    m_f.write((const char *)data.getDataBuffer(), size);
}

void MWAWPropertyHandlerEncoder::characters(RVNGString const &sCharacters)
{
  if (sCharacters.len()==0) return;
  m_f << 'T';
  writeString(sCharacters.cstr());
}

void MWAWPropertyHandlerEncoder::writeString(const char *name)
{
  int sz = (name == 0L) ? 0 : int(strlen(name));
  writeInteger(sz);
  if (sz) m_f.write(name, sz);
}

void MWAWPropertyHandlerEncoder::writeLong(long val)
{
  int32_t value=(int32_t) val;
  unsigned char const allValue[]= {(unsigned char)(value&0xFF), (unsigned char)((value>>8)&0xFF), (unsigned char)((value>>16)&0xFF), (unsigned char)((value>>24)&0xFF)};
  m_f.write((const char *)allValue, 4);
}

void MWAWPropertyHandlerEncoder::writeProperty(const char *key, const RVNGProperty &prop)
{
  if (!key) {
    MWAW_DEBUG_MSG(("MWAWPropertyHandlerEncoder::writeProperty: key is NULL\n"));
    return;
  }
  writeString(key);
  writeString(prop.getStr().cstr());
}

void MWAWPropertyHandlerEncoder::writePropertyList(const RVNGPropertyList &xPropList)
{
  RVNGPropertyList::Iter i(xPropList);
  int numElt = 0;
  for (i.rewind(); i.next(); ) numElt++;
  writeInteger(numElt);
  for (i.rewind(); i.next(); )
    writeProperty(i.key(),*i());
}

bool MWAWPropertyHandlerEncoder::getData(RVNGBinaryData &data)
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
  MWAWPropertyHandlerDecoder(MWAWPropertyHandler *hdl=0L):m_handler(hdl), m_openTag() {}

  //! tries to read the data
  bool readData(RVNGBinaryData const &encoded) {
    try {
      RVNGInputStream *inp = const_cast<RVNGInputStream *>(encoded.getDataStream());
      if (!inp) return false;

      while (!inp->isEnd()) {
        unsigned const char *c;
        unsigned long numRead;

        c = inp->read(1,numRead);
        if (!c || numRead != 1) {
          MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder: can not read data type \n"));
          return false;
        }
        switch(*c) {
        case 'B':
          if (!readInsertElementWithBinary(*inp)) return false;
          break;
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
    } catch(...) {
      return false;
    }
    return true;
  }

protected:
  //! reads an simple element
  bool readInsertElement(RVNGInputStream &input) {
    std::string s;
    if (!readString(input, s)) return false;

    if (s.empty()) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readInsertElement find empty tag\n"));
      return false;
    }
    if (m_handler) m_handler->insertElement(s.c_str());
    return true;
  }

  //! reads an element with a property list
  bool readInsertElementWithList(RVNGInputStream &input) {
    std::string s;
    if (!readString(input, s)) return false;

    if (s.empty()) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readInsertElementWithProperty: find empty tag\n"));
      return false;
    }
    RVNGPropertyList lists;
    if (!readPropertyList(input, lists)) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readInsertElementWithProperty: can not read propertyList for tag %s\n",
                      s.c_str()));
      return false;
    }

    if (m_handler) m_handler->insertElement(s.c_str(), lists);
    return true;
  }

  //! reads an insertElement
  bool readInsertElementWithVector(RVNGInputStream &input) {
    std::string s;
    if (!readString(input, s)) return false;
    if (s.empty()) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readInsertElementWithVector: can not read tag name\n"));
      return false;
    }

    RVNGPropertyList lists;
    if (!readPropertyList(input, lists)) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readInsertElementWithVector: can not read propertyList for tag %s\n",
                      s.c_str()));
      return false;
    }
    RVNGPropertyListVector vect;
    if (!readPropertyListVector(input, vect)) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readInsertElementWithVector: can not read propertyVector for tag %s\n",
                      s.c_str()));
      return false;
    }

    m_openTag.push(s);

    if (m_handler) m_handler->insertElement(s.c_str(), lists, vect);
    return true;
  }
  //! reads an insertElement with a binary data
  bool readInsertElementWithBinary(RVNGInputStream &input) {
    std::string s;
    if (!readString(input, s)) return false;
    if (s.empty()) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readInsertElementWithBinary: can not read tag name\n"));
      return false;
    }

    RVNGPropertyList lists;
    if (!readPropertyList(input, lists)) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readInsertElementWithBinary: can not read propertyList for tag %s\n",
                      s.c_str()));
      return false;
    }
    long sz;
    if (!readLong(input,sz) || sz<0) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readInsertWithBinary: can not read binray size for tag %s\n",
                      s.c_str()));
      return false;
    }

    RVNGBinaryData data;
    if (sz) {
      unsigned long read;
      unsigned char const *dt=input.read((unsigned long) sz, read);
      if (!dt || sz!=(long) read) {
        MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readInsertWithBinary: can not read binray data for tag %s\n",
                        s.c_str()));
        return false;
      }
      data.append(dt, (unsigned long)read);
    }
    m_openTag.push(s);
    if (m_handler) m_handler->insertElement(s.c_str(), lists, data);
    return true;
  }

  //! reads a set of characters
  bool readCharacters(RVNGInputStream &input) {
    std::string s;
    if (!readString(input, s)) return false;
    if (!s.length()) return true;
    if (m_handler) m_handler->characters(RVNGString(s.c_str()));
    return true;
  }

  //
  // low level
  //

  //! low level: reads a property vector: number of properties list followed by list of properties list
  bool readPropertyListVector(RVNGInputStream &input, RVNGPropertyListVector &vect) {
    int numElt;
    if (!readInteger(input, numElt)) return false;

    if (numElt < 0) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readPropertyListVector: can not read numElt=%d\n",
                      numElt));
      return false;
    }
    for (int i = 0; i < numElt; i++) {
      RVNGPropertyList lists;
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
  bool readPropertyList(RVNGInputStream &input, RVNGPropertyList &lists) {
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
  bool readProperty(RVNGInputStream &input, RVNGPropertyList &list) {
    std::string key, val;
    if (!readString(input, key)) return false;
    if (!readString(input, val)) return false;

    RVNGProperty *prop=RVNGPropertyFactory::newStringProp(val.c_str());
    if (!prop) return prop;
    RVNGUnit unit=prop->getUnit();
    if (unit==RVNG_POINT) {
      list.insert(key.c_str(), prop->getDouble()/72., RVNG_INCH);
      delete prop;
    } else if (unit==RVNG_TWIP) {
      list.insert(key.c_str(), prop->getDouble()/1440., RVNG_INCH);
      delete prop;
    } else
      list.insert(key.c_str(), prop);
    return true;
  }

  //! low level: reads a string : size and string
  bool readString(RVNGInputStream &input, std::string &s) {
    int numC = 0;
    if (!readInteger(input, numC)) return false;
    if (numC==0) {
      s = std::string("");
      return true;
    }
    unsigned long numRead;
    const unsigned char *dt = input.read((unsigned long)numC, numRead);
    if (dt == 0L || numRead != (unsigned long) numC) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readString: can not read a string\n"));
      return false;
    }
    s = std::string((const char *)dt, size_t(numC));
    return true;
  }

  //! low level: reads an integer value
  static bool readInteger(RVNGInputStream &input, int &val) {
    long res;
    if (!readLong(input, res))
      return false;
    val=int(res);
    return true;
  }
  //! low level: reads an long value
  static bool readLong(RVNGInputStream &input, long &val) {
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

  //! the list of open tags
  std::stack<std::string> m_openTag;
};

////////////////////////////////////////////////////
//
// MWAWPropertyHandler
//
////////////////////////////////////////////////////
bool MWAWPropertyHandler::checkData(RVNGBinaryData const &encoded)
{
  MWAWPropertyHandlerDecoder decod;
  return decod.readData(encoded);
}

bool MWAWPropertyHandler::readData(RVNGBinaryData const &encoded)
{
  MWAWPropertyHandlerDecoder decod(this);
  return decod.readData(encoded);
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
