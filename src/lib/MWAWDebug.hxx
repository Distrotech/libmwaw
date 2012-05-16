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

#ifndef MWAW_DEBUG
#  define MWAW_DEBUG

#include <string>

#include "MWAWInputStream.hxx"

class WPXBinaryData;

#  if defined(DEBUG_WITH_FILES)
#include <fstream>
#include <map>
#include <sstream>
#include <vector>
//! some  basic tools
namespace libmwaw
{
//! debugging tools
namespace Debug
{
//! a debug function to store in a datafile in the current directory
//! WARNING: this function erase the file fileName if it exists
//! (if debug_with_files is not defined, does nothing)
bool dumpFile(WPXBinaryData &data, char const *fileName);
//! returns a file name from an ole/... name
std::string flattenFileName(std::string const &name);
}

//! a basic stream (if debug_with_files is not defined, does nothing)
typedef std::stringstream DebugStream;

//! an interface used to insert comment in a binary file,
//! written in ascii form (if debug_with_files is not defined, does nothing)
class DebugFile
{
public:
  //! constructor given the input file
  DebugFile(MWAWInputStreamPtr ip=MWAWInputStreamPtr())
    : m_file(), m_on(false), m_input(ip), m_actOffset(-1), m_notes(), m_skipZones() { }

  //! resets the input
  void setStream(MWAWInputStreamPtr ip) {
    m_input = ip;
  }
  //! destructor
  ~DebugFile() {
    reset();
  }
  //! opens/creates a file to store a result
  bool open(std::string const &filename);
  //! writes the current file and reset to zero
  void reset() {
    write();
    m_file.close();
    m_on = false;
    m_notes.resize(0);
    m_skipZones.resize(0);
    m_actOffset = -1;
  }
  //! flushes the file
  void write();
  //! adds a new position in the file
  void addPos(long pos);
  //! adds a note in the file, in actual position
  void addNote(char const *note);
  //! adds a not breaking delimiter in position \a pos
  void addDelimiter(long pos, char c);

  //! skips the file zone defined by beginPos-endPos
  void skipZone(int beginPos, int endPos) {
    if (m_on) m_skipZones.push_back(Vec2i(beginPos, endPos));
  }

protected:
  //! sorts the position/note date
  void sort();

  //! a stream which is open to write the file
  mutable std::ofstream m_file;
  //! a flag to know if the result stream is open or note
  mutable bool m_on;

  //! the input
  MWAWInputStreamPtr m_input;

  //! \brief a note and its position (used to sort all notes)
  struct NotePos {
    //! empty constructor used by std::vector
    NotePos() : m_pos(-1), m_text(""), m_breaking(false) { }

    //! constructor: given position and note
    NotePos(long p, std::string const &n, bool br=true) : m_pos(p), m_text(n), m_breaking(br) {}
    //! note offset
    long m_pos;
    //! note text
    std::string m_text;
    //! flag to indicate a non breaking note
    bool m_breaking;

    //! comparison operator based on the position
    bool operator<(NotePos const &p) const {
      long diff = m_pos-p.m_pos;
      if (diff) return (diff < 0) ? true : false;
      if (m_breaking != p.m_breaking) return m_breaking;
      return m_text < p.m_text;
    }
    /*! \struct NotePosLt
     * \brief internal struct used to sort the notes, sorted by position
     */
    struct NotePosLt {
      //! comparison operator
      bool operator()(NotePos const &s1, NotePos const &s2) const {
        return s1 < s2;
      }
    };
    /*! \typedef Map
     *  \brief map of notes
     */
    typedef std::map<NotePos, int,struct NotePosLt> Map;
  };

  //! the actual offset (used to store note)
  int m_actOffset;
  //! list of notes
  std::vector<NotePos> m_notes;
  //! list of skipZone
  std::vector<Vec2i> m_skipZones;
};
}
#  else
namespace libmwaw
{
namespace Debug
{
inline bool dumpFile(WPXBinaryData &, char const *)
{
  return true;
}
//! returns a file name from an ole/... name
inline std::string flattenFileName(std::string const &name)
{
  return name;
}
}

class DebugStream
{
public:
  template <class T>
  DebugStream &operator<<(T const &) {
    return *this;
  }

  std::string str() const {
    return std::string("");
  }
  void str(std::string const &) { }
};

class DebugFile
{
public:
  DebugFile(MWAWInputStreamPtr) {}
  DebugFile() {}
  void setStream(MWAWInputStreamPtr) {  }
  ~DebugFile() { }

  bool open(std::string const &) {
    return true;
  }

  void addPos(long ) {}
  void addNote(char const *) {}
  void addDelimiter(long, char) {}

  void write() {}
  void reset() { }

  void skipZone(int , int ) {}
};
}
#  endif

#endif

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab: