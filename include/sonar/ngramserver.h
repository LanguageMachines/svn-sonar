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

#include "timbl/CommandLine.h"
#include "ticcutils/LogStream.h"

class NgramServerClass {
 public:
  NgramServerClass( Timbl::TimblOpts& opts );
  ~NgramServerClass();
  void Start( Timbl::TimblOpts& Opts );
  void fill_table( std::istream&, int );
  void exec( const std::string&, std::ostream& );
  size_t get_count( const std::string& );
  void count( const std::string&, std::ostream& );
  void lookup( const std::string&, std::ostream& );
  void get_results( const std::string&, size_t, size_t, std::ostream& );
 private:
  int nGram;
  int clip;
  std::multimap<std::string,std::string> *dict;
  std::string last_query;
  std::vector<std::string> result_set;
};

#endif
