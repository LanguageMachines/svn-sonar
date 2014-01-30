#ifndef NGRAMSERVER_SERVER_H
#define NGRAMSERVER_SERVER_H
/*
  $Id: AnaServer.h 16824 2014-01-06 10:24:00Z sloot $
  $URL: https://ilk.uvt.nl/svn/sources/ticclopstools/trunk/include/ticclops/AnaServer.h $

  Copyright (c) 1998 - 2014

  This file is part of ticclopstools

  ticclopstools is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  ticclopstools is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.

  For questions and suggestions, see:

  or send mail to:

*/

#include <ctime>
#include "timbl/CommandLine.h"
#include "ticcutils/LogStream.h"

class resultSet {
 public:
 resultSet( const std::string& s ) { std::time( &start ); };
  std::time_t start;
  std::vector<std::string> rset;
};

class NgramServerClass {
 public:
  NgramServerClass( Timbl::TimblOpts& , TiCC::LogStream* );
  NgramServerClass( const NgramServerClass& );
  ~NgramServerClass();
  void Start( Timbl::TimblOpts& Opts );
  void fill_table( std::istream&, int );
  size_t get_count( const std::string& );
  size_t get_result( std::vector<std::string>&, const std::string&,
		     size_t, size_t, size_t& );
  size_t get_result( std::vector<std::string>&, size_t, size_t );
  int ngramval() const { return nGram; };
  resultSet *getResultSet( const std::string& ) const;
  void cleanResults( int );
 private:
  bool cloned;
  int nGram;
  int clip;
  TiCC::LogStream *log;
  std::multimap<std::string,std::string> *dict;
  std::map<std::string,resultSet*> *result_sets;
};

#endif
