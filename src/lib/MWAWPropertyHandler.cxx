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

#include <libwpd/libwpd.h>
#include <libwpd-stream/libwpd-stream.h>
#include <libmwaw/libmwaw.hxx>

////////////////////////////////////////////////////
//
// MWAWPropertyHandlerEncoder
//
////////////////////////////////////////////////////
MWAWPropertyHandlerEncoder::MWAWPropertyHandlerEncoder()
  : m_f(std::ios::in | std::ios::out | std::ios::binary)
{
}

void MWAWPropertyHandlerEncoder::startElement
(const char *psName, const WPXPropertyList &xPropList)
{
  m_f << 'S';
  writeString(psName);
  writePropertyList(xPropList);
}

void MWAWPropertyHandlerEncoder::startElement
(const char *psName, const WPXPropertyList &xPropList, const WPXPropertyListVector &vect)
{
  m_f << 'V';
  writeString(psName);
  writePropertyList(xPropList);
  writeInteger((int)vect.count());
  for (unsigned long i=0; i < vect.count(); i++)
    writePropertyList(vect[i]);
}

void MWAWPropertyHandlerEncoder::startElement
(const char *psName, const WPXPropertyList &xPropList, const WPXBinaryData &data)
{
  m_f << 'B';
  writeString(psName);
  writePropertyList(xPropList);
  long size=(long) data.size();
  if (size<0) {
    MWAW_DEBUG_MSG(("MWAWPropertyHandlerEncoder::startElement: oops, probably the binary data is too big!!!\n"));
    size=0;
  }
  writeLong(size);
  if (size>0)
    m_f.write((const char *)data.getDataBuffer(), size);
}

void MWAWPropertyHandlerEncoder::insertElement(const char *psName)
{
  m_f << 'I';
  writeString(psName);
}

void MWAWPropertyHandlerEncoder::endElement(const char *psName)
{
  m_f << 'E';
  writeString(psName);
}

void MWAWPropertyHandlerEncoder::characters(WPXString const &sCharacters)
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

void MWAWPropertyHandlerEncoder::writeProperty(const char *key, const WPXProperty &prop)
{
  if (!key) {
    MWAW_DEBUG_MSG(("MWAWPropertyHandlerEncoder::writeProperty: key is NULL\n"));
    return;
  }
  writeString(key);
  writeString(prop.getStr().cstr());
}

void MWAWPropertyHandlerEncoder::writePropertyList(const WPXPropertyList &xPropList)
{
  WPXPropertyList::Iter i(xPropList);
  int numElt = 0;
  for (i.rewind(); i.next(); ) numElt++;
  writeInteger(numElt);
  for (i.rewind(); i.next(); )
    writeProperty(i.key(),*i());
}

bool MWAWPropertyHandlerEncoder::getData(WPXBinaryData &data)
{
  data.clear();
  std::string d=m_f.str();
  if (d.length() == 0) return false;
  data.append((const unsigned char *)d.c_str(), d.length());
  return true;
}

/*! \brief Internal: the property decoder
 *
 * In order to be read by writerperfect, we must code document consisting in
 * tag and propertyList in an intermediar format:
 *  - [string:s]: an int length(s) follow by the length(s) characters of string s
 *  - [property:p]: a string value p.getStr()
 *  - [propertyList:pList]: a int: #pList followed by pList[0].key(),pList[0], pList[1].key(),pList[1], ...
 *  - [propertyListVector:v]: a int: #v followed by v[0], v[1], ...
 *  - [binaryData:d]: a int32 d.size() followed by the data content
 *
 *  - [startElement:name proplist:prop]: char 'S', [string] name, prop
 *  - [startElement2:name proplist:prop proplistvector:vector]:
 *          char 'V', [string] name, prop, vector
 *  - [startElement3:name proplist:prop binarydata:data]:
 *          char 'B', [string] name, prop, data
 *  - [insertElement:name]: char 'I', [string] name
 *  - [endElement:name ]: char 'E', [string] name
 *  - [characters:s ]: char 'T', [string] s
 *            - if len(s)==0, we write nothing
 *            - the string is written as is (ie. we do not escaped any characters).
*/
class MWAWPropertyHandlerDecoder
{
public:
  //! constructor given a MWAWPropertyHandler
  MWAWPropertyHandlerDecoder(MWAWPropertyHandler *hdl=0L):m_handler(hdl), m_openTag() {}

  //! tries to read the data
  bool readData(WPXBinaryData const &encoded) {
    try {
      WPXInputStream *inp = const_cast<WPXInputStream *>(encoded.getDataStream());
      if (!inp) return false;

      while (!inp->atEOS()) {
        unsigned const char *c;
        unsigned long numRead;

        c = inp->read(1,numRead);
        if (!c || numRead != 1) {
          MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder: can not read data type \n"));
          return false;
        }
        switch(*c) {
        case 'B':
          if (!readStartElementWithBinary(*inp)) return false;
          break;
        case 'V':
          if (!readStartElementWithVector(*inp)) return false;
          break;
        case 'S':
          if (!readStartElement(*inp)) return false;
          break;
        case 'I':
          if (!readInsertElement(*inp)) return false;
          break;
        case 'E':
          if (!readEndElement(*inp)) return false;
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
  //! reads an startElement
  bool readStartElement(WPXInputStream &input) {
    std::string s;
    if (!readString(input, s)) return false;
    if (s.empty()) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readStartElement: can not read tag name\n"));
      return false;
    }
    WPXPropertyList lists;
    if (!readPropertyList(input, lists)) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readStartElement: can not read propertyList for tag %s\n",
                      s.c_str()));
      return false;
    }

    m_openTag.push(s);

    if (m_handler) m_handler->startElement(s.c_str(), lists);
    return true;
  }

  //! reads an startElement
  bool readStartElementWithVector(WPXInputStream &input) {
    std::string s;
    if (!readString(input, s)) return false;
    if (s.empty()) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readStartElementWithVector: can not read tag name\n"));
      return false;
    }

    WPXPropertyList lists;
    if (!readPropertyList(input, lists)) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readStartElementWithVector: can not read propertyList for tag %s\n",
                      s.c_str()));
      return false;
    }
    WPXPropertyListVector vect;
    if (!readPropertyListVector(input, vect)) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readStartElementWithVector: can not read propertyVector for tag %s\n",
                      s.c_str()));
      return false;
    }

    m_openTag.push(s);

    if (m_handler) m_handler->startElement(s.c_str(), lists, vect);
    return true;
  }
  //! reads an startElement
  bool readStartElementWithBinary(WPXInputStream &input) {
    std::string s;
    if (!readString(input, s)) return false;
    if (s.empty()) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readStartElementWithBinary: can not read tag name\n"));
      return false;
    }

    WPXPropertyList lists;
    if (!readPropertyList(input, lists)) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readStartElementWithBinary: can not read propertyList for tag %s\n",
                      s.c_str()));
      return false;
    }
    long sz;
    if (!readLong(input,sz) || sz<0) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readStartWithBinary: can not read binray size for tag %s\n",
                      s.c_str()));
      return false;
    }

    WPXBinaryData data;
    if (sz) {
      unsigned long read;
      unsigned char const *dt=input.read((unsigned long) sz, read);
      if (!dt || sz!=(long) read) {
        MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readStartWithBinary: can not read binray data for tag %s\n",
                        s.c_str()));
        return false;
      }
      data.append(dt, (unsigned long)read);
    }
    m_openTag.push(s);
    if (m_handler) m_handler->startElement(s.c_str(), lists, data);
    return true;
  }

  //! reads an simple element
  bool readInsertElement(WPXInputStream &input) {
    std::string s;
    if (!readString(input, s)) return false;

    if (s.empty()) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readInsertElement find empty tag\n"));
      return false;
    }
    if (m_handler) m_handler->insertElement(s.c_str());
    return true;
  }

  //! reads an endElement
  bool readEndElement(WPXInputStream &input) {
    std::string s;
    if (!readString(input, s)) return false;

    if (s.empty()) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readEndElement find empty tag\n"));
      return false;
    }
    // check stack
    if (m_openTag.empty()) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readEndElement %s with no openElement\n",
                      s.c_str()));
      return false;
    }
    if (m_openTag.top() != s) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readEndElement %s but last open %s\n",
                      m_openTag.top().c_str(), s.c_str()));
      return false;
    }
    m_openTag.pop();
    if (m_handler) m_handler->endElement(s.c_str());
    return true;
  }

  //! reads a set of characters
  bool readCharacters(WPXInputStream &input) {
    std::string s;
    if (!readString(input, s)) return false;
    if (!s.length()) return true;
    if (m_handler) m_handler->characters(WPXString(s.c_str()));
    return true;
  }

  //
  // low level
  //

  //! low level: reads a property vector: number of properties list followed by list of properties list
  bool readPropertyListVector(WPXInputStream &input, WPXPropertyListVector &vect) {
    int numElt;
    if (!readInteger(input, numElt)) return false;

    if (numElt < 0) {
      MWAW_DEBUG_MSG(("MWAWPropertyHandlerDecoder::readPropertyListVector: can not read numElt=%d\n",
                      numElt));
      return false;
    }
    for (int i = 0; i < numElt; i++) {
      WPXPropertyList lists;
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
  bool readPropertyList(WPXInputStream &input, WPXPropertyList &lists) {
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
  bool readProperty(WPXInputStream &input, WPXPropertyList &list) {
    std::string key, val;
    if (!readString(input, key)) return false;
    if (!readString(input, val)) return false;

    // check if the val can be a double, ...
    if (!val.empty() && (val[0]=='-' || val[0]=='.' || (val[0]>='0' && val[0]<='9'))) {
      std::istringstream iss(val);
      double res = 0.0;
      iss >> res;
      if (!iss.fail()) {
        if (iss.eof() || iss.peek() == std::char_traits<wchar_t>::eof()) {
          list.insert(key.c_str(), res);
          return true;
        }
        std::string remain;
        iss >> remain;
        if (iss.peek() == std::char_traits<wchar_t>::eof()) {
          if (remain=="pt") {
            list.insert(key.c_str(), res/72., WPX_INCH);
            return true;
          }
          if (remain=="in") {
            list.insert(key.c_str(), res, WPX_INCH);
            return true;
          }
          if (remain=="%") {
            list.insert(key.c_str(), res/100., WPX_PERCENT);
            return true;
          }
          if (remain=="*") {
            list.insert(key.c_str(), res/1440., WPX_INCH);
            return true;
          }
        }
      }
    }
    list.insert(key.c_str(), val.c_str());
    return true;
  }

  //! low level: reads a string : size and string
  bool readString(WPXInputStream &input, std::string &s) {
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
  static bool readInteger(WPXInputStream &input, int &val) {
    long res;
    if (!readLong(input, res))
      return false;
    val=int(res);
    return true;
  }
  //! low level: reads an long value
  static bool readLong(WPXInputStream &input, long &val) {
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
bool MWAWPropertyHandler::checkData(WPXBinaryData const &encoded)
{
  MWAWPropertyHandlerDecoder decod;
  return decod.readData(encoded);
}

bool MWAWPropertyHandler::readData(WPXBinaryData const &encoded)
{
  MWAWPropertyHandlerDecoder decod(this);
  return decod.readData(encoded);
}

void MWAWPropertyHandler::startElement(const char *, const WPXPropertyList &,
                                       const WPXPropertyListVector &)
{
  MWAW_DEBUG_MSG(("MWAWPropertyHandler::startElement: with a propertyListVector must be reimplement in subclass\n"));
}

void MWAWPropertyHandler::startElement(const char *, const WPXPropertyList &,
                                       const WPXBinaryData &)
{
  MWAW_DEBUG_MSG(("MWAWPropertyHandler::startElement: with a WPXBinaryData must be reimplement in subclass\n"));
}

void MWAWPropertyHandler::insertElement(const char *)
{
  MWAW_DEBUG_MSG(("MWAWPropertyHandler::insertElement: must be reimplement in subclass\n"));
}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
