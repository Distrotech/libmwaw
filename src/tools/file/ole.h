/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */

/* libmwaw: tools
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

#ifndef MWAW_OLE_H
#  define MWAW_OLE_H
#include <ostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace libmwaw_tools
{
class InputStream;

/** \brief the main class to read a OLE file ( only the main dir entry to retrieve the clsid type )
 */
class OLE
{
public:
  //! the constructor
  OLE(InputStream &input) : m_input(input), m_header(), m_dirTree(), m_smallBlockPos(), m_status(S_Unchecked) { }
  //! the destructor
  ~OLE() {}
  //! returns true if the file is an ole
  bool isOLE() {
    init();
    return m_status==S_Ok;
  }
  //! returns a string correspond to the main clsid or ""
  std::string getCLSIDType();
  //! returns a string correspond to the comp obj clsid or ""
  std::string getCompObjType();

protected:
  enum { Eof = 0xfffffffe };
  //! an enum to define the different bat type
  enum Type { BigBat=0, SmallBat, Dirent, MetaBat, BigBatAlloc };
  //! try to initialised the header, ...
  bool init();
  //! try to initialized the bigbat/smallbat table
  bool initAllocTables();

  //! returns a string correspond to the clsid or ""
  static std::string getCLSIDType(unsigned const (&clsid)[4]);
  //! try to load a ole sub file
  bool load(std::string const &name, std::vector<unsigned char> &buffer);

  // low level:
  //! return true if we need to use big block
  bool useBigBlockFor(unsigned long size) const {
    return size >= m_header.m_threshold;
  }
  //! returns the address of a big/small block
  long getDataAddress(unsigned block, bool isBig) const {
    unsigned const bigSize=m_header.m_sizeBats[BigBat];
    if (isBig) return long((block+1)*bigSize);
    unsigned const smallSize=m_header.m_sizeBats[SmallBat];
    unsigned const numBigSmall=unsigned(bigSize/smallSize);
    size_t bId=size_t(block/numBigSmall);
    if (bId >= m_smallBlockPos.size()) return 0;
    return long((m_smallBlockPos[bId]+1)*bigSize+smallSize*(block%numBigSmall));
  }

  //! a struct to define the header data
  struct Header {
    //! constructor
    Header() : m_threshold(4096), m_bigBatAlloc() {
      for (int i = 0; i < 5; i++) {
        m_numBats[i] = 0;
        m_startBats[i] = Eof;
      }
      m_sizeBats[BigBat] = 0x200;
      m_sizeBats[SmallBat] = 0x40;
    }
    //! try to read the ole header
    bool read(InputStream &input);
    //! returns the start of a big block
    unsigned long getBigBlockPos(unsigned block) const {
      return (unsigned long)(block+1)*(unsigned long)m_sizeBats[BigBat];
    }
    //! the number of big bat, small bat, meta bat
    unsigned m_numBats[5];
    //! the start of the different zone
    unsigned m_startBats[5];
    //! the big and small block size ( default 0x40, 0x200)
    unsigned m_sizeBats[2];
    //! the thresold between small and bug file (usually 4K)
    unsigned m_threshold;
    //! the first 109 big bat alloc blocks
    std::vector<unsigned long> m_bigBatAlloc;
  };

  //! a struct used to store the big and small allocation table
  class AllocTable
  {
  public:
    //! constructor
    AllocTable() : m_data() {
    }
    //! return the number of elements
    unsigned long count() const {
      return (unsigned long) m_data.size();
    }
    //! accessor to the index value ( no bound check)
    unsigned long operator[](unsigned long index ) const {
      return m_data[size_t(index)];
    }
    //! accessor to the index value ( no bound check)
    unsigned long &operator[](unsigned long index )  {
      return m_data[size_t(index)];
    }
    //! resize the data
    void resize( unsigned long newsize ) {
      m_data.resize(size_t(newsize), Eof);
    }
    //! return the list of blocks which follow to start(included)
    std::vector<unsigned long> follow( unsigned long start ) const;
  private:
    //! the data
    std::vector<unsigned long> m_data;
    AllocTable( const AllocTable & );
    AllocTable &operator=( const AllocTable & );
  };

  //! a struct used to store a DirEntry
  struct DirEntry {
  public:
    enum { End= 0xffffffff };
    //! constructor
    DirEntry() : m_valid(false), m_macRootEntry(false), m_type(0), m_size(0), m_start(0),
      m_right(End), m_left(End), m_child(End), m_name("") {
      for (int i=0; i < 4; i++) m_clsid[i] = 0;
    }
    //! returns true for a directory
    bool is_dir() const {
      return m_type==1 || m_type==5;
    }
    //! returns the simplified file name
    std::string name() const {
      if (m_name.length() && m_name[0]<32)
        return m_name.substr(1);
      return m_name;
    }
    /** sets the file name */
    void setName(std::string const &nm) {
      m_name=nm;
    }
    //! try to read the ole header
    bool read(InputStream &input);

    bool m_valid;            /** false if invalid (should be skipped) */
    bool m_macRootEntry;      /** true if this is a classi mac directory entry */
    unsigned m_type;         /** the type */
    unsigned long m_size;    /** size (not valid if directory) */
    unsigned long m_start;   /** starting block */
    unsigned m_right;        /** previous sibling */
    unsigned m_left;         /** next sibling */
    unsigned m_child;        /** first child */

    /** four uint32_t: the clsid data */
    unsigned m_clsid[4];

  protected:
    std::string m_name;      /** the name, not in unicode anymore */
  };

  //! a struct used to store a DirTree
  class DirTree
  {
  public:
    /** constructor */
    DirTree() : m_entries() {
      clear();
    }
    /** clear all entries, leaving only a root entries */
    void clear();
    /** returns the number of entries */
    unsigned size() const {
      return unsigned(m_entries.size());
    }
    /** resize the number of entries */
    void resize(unsigned sz) {
      m_entries.resize(size_t(sz));
    }
    /** returns the entry with a given index */
    DirEntry const *entry( unsigned ind ) const {
      if( ind >= size() ) return 0;
      return &m_entries[ size_t(ind) ];
    }
    /** returns the entry with a given index */
    DirEntry *entry( unsigned ind ) {
      if( ind >= size() ) return  0;
      return &m_entries[ size_t(ind) ];
    }
    /** returns the entry with a given name */
    DirEntry *entry( const std::string &name ) {
      return entry(index(name));
    }
    /** given a fullname (e.g "/ObjectPool/_1020961869"), find the entry */
    unsigned index( const std::string &name) const;
    /** tries to find a child of ind with a given name */
    unsigned find_child( unsigned ind, const std::string &name ) const;
  protected:
    //! returns a list of siblings corresponding to a node
    std::vector<unsigned> get_siblings(unsigned ind) const {
      std::set<unsigned> seens;
      get_siblings(ind, seens);
      return std::vector<unsigned>(seens.begin(), seens.end());
    }
    //! constructs the list of siblings ( by filling the seens set )
    void get_siblings(unsigned ind, std::set<unsigned> &seens) const;

  private:
    //! the list of entry
    std::vector<DirEntry> m_entries;
    DirTree( const DirTree & );
    DirTree &operator=( const DirTree & );
  };

  //! read a short in the input file
  static unsigned short readU16(InputStream &input);
  //! read a int in the input file
  static unsigned int readU32(InputStream &input);
  //! the input stream
  InputStream &m_input;
  //! the header
  Header m_header;
  //! the dir tree
  DirTree m_dirTree;
  //! the big and small allocation table
  AllocTable m_allocTable[2];
  //! the small block file position
  std::vector<unsigned long> m_smallBlockPos;

  //! a enum to know if the ole struct are initialised
  enum Status { S_Ok, S_Bad, S_Unchecked };
  //! the actual status
  Status m_status;
private:
  OLE(OLE const &orig);
  OLE &operator=(OLE const &orig);
};

}
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
