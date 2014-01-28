/*
  $Id: anaserv.cxx 15835 2013-03-28 12:20:27Z sloot $
  $URL: https://ilk.uvt.nl/svn/sources/ticclopstools/trunk/src/anaserv.cxx $

  Copyright (c) 1998 - 2012

  This file is part of sonartools

  sonartools is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  sonartools is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.

  For questions and suggestions, see:

  or send mail to:

*/

#include <csignal>
#include <cerrno>
#include <string>
#include <map>
#include "config.h"
#include "timblserver/FdStream.h"
#include "timbl/TimblAPI.h"
#include "timblserver/ServerBase.h"
#include "ticcutils/StringOps.h"
#include "include/sonar/ngramserver.h"

using namespace std;
using namespace TiCC;
using namespace Timbl;
using namespace TimblServer;

#define SLOG (*Log(cur_log))
#define SDBG (*Dbg(cur_log))

#define LOG cerr

inline void usage(){
  cerr << "usage:  anaserv -S port [-f hashfile]" << endl;
  cerr << " or     anaserv -S port [-l lexiconfile]" << endl;
  exit( EXIT_SUCCESS );
}

class TcpServer : public TcpServerBase {
public:
  void callback( childArgs* );
  TcpServer( const TiCC::Configuration *c ): TcpServerBase( c ){};
};

class HttpServer : public HttpServerBase {
public:
  void callback( childArgs* );
  HttpServer( const TiCC::Configuration *c ): HttpServerBase( c ){};
};

void TcpServer::callback( childArgs *args ){
  int sockId = args->id();
  NgramServerClass *client = 0;
  map<string, NgramServerClass*> *servers =
    static_cast<map<string, NgramServerClass*> *>(callback_data);

  string baseName;
  args->os() << "Welcome to the NGgram server." << endl;
  if ( servers->size() == 1
       && servers->find("default") != servers->end() ){
    baseName = "default";
    //
    char line[256];
    sprintf( line, "Thread %zd, on Socket %d", (uintptr_t)pthread_self(),
	     sockId );
    *Log(myLog) << line << ", started." << endl;
  }
  else {
    args->os() << "available bases: ";
    map<string,NgramServerClass*>::const_iterator it = servers->begin();
    while ( it != servers->end() ){
      args->os() << it->first << " ";
      ++it;
    }
    args->os() << endl;
    string line;
    while ( getline( args->is(), line ) ){
      line = TiCC::trim( line );
      if ( line.empty() )
	break;
      vector<string> vec;
      int num = TiCC::split( line, vec );
      if ( num == 2 ){
	if ( vec[0] == "base" ){
	  baseName = vec[1];
	}
	else {
	  args->os() << "select a base first!" << endl;
	}
      }
      else {
	args->os() << "select a base first!" << endl;
      }
    }
  }
  size_t count = 0;
  if ( !baseName.empty() ){
    map<string,NgramServerClass*>::const_iterator it = servers->find( baseName );
    if ( it != servers->end() ){
      NgramServerClass exp(*it->second);
      string line;
      while ( getline( args->is(), line ) ){
	line = TiCC::trim( line );
	if ( line.empty() )
	  break;
	exp.exec( line, args->os() );
	++count;
      }
    }
    else {
      args->os() << "base " << baseName << " doesn't exist" << endl;
    }
  }
  *Log(myLog) << "Thread " << (uintptr_t)pthread_self()
	      << " terminated, " << count
	      << " instances processed " << endl;
}

#define IS_DIGIT(x) (((x) >= '0') && ((x) <= '9'))
#define IS_HEX(x) ((IS_DIGIT(x)) || (((x) >= 'a') && ((x) <= 'f')) || \
            (((x) >= 'A') && ((x) <= 'F')))


  string urlDecode( const string& s ) {
    int cc;
    string result;
    int len=s.size();
    for (int i=0; i<len ; ++i ) {
      cc=s[i];
      if (cc == '+') {
	result += ' ';
      }
      else if ( cc == '%' &&
		( i < len-2 &&
		  ( IS_HEX(s[i+1]) ) &&
		  ( IS_HEX(s[i+2]) ) ) ){
	std::istringstream ss( "0x"+s.substr(i+1,2) );
	int tmp;
	ss >> std::showbase >> std::hex;
	ss >> tmp;
      result = result + (char)tmp;
      i += 2;
      }
      else {
	result += cc;
      }
    }
    return result;
  }


void HttpServer::callback( childArgs *args ){
  // process the test material
  // report connection to the server terminal
  //
  args->socket()->setNonBlocking();
  map<string, NgramServerClass*> *servers =
    static_cast<map<string, NgramServerClass*> *>(callback_data);
  char logLine[256];
  sprintf( logLine, "Thread %zd, on Socket %d", (uintptr_t)pthread_self(),
	   args->id() );
  *Log(myLog) << logLine << ", started." << endl;
  string Line;
  int timeout = 1;
  if ( nb_getline( args->is(), Line, timeout ) ){
    *Dbg(myLog) << "FirstLine='" << Line << "'" << endl;
    if ( Line.find( "HTTP" ) != string::npos ){
      // skip HTTP header
      string tmp;
      timeout = 1;
      while ( ( nb_getline( args->is(), tmp, timeout ), !tmp.empty()) ){
	//	    cerr << "skip: read:'" << tmp << "'" << endl;;
      }
      string::size_type spos = Line.find( "GET" );
      if ( spos != string::npos ){
	string::size_type epos = Line.find( " HTTP" );
	string line = Line.substr( spos+3, epos - spos - 3 );
	*Dbg(myLog) << "Line='" << line << "'" << endl;
	epos = line.find( "?" );
	string basename;
	if ( epos != string::npos ){
	  basename = line.substr( 0, epos );
	  string qstring = line.substr( epos+1 );
	  epos = basename.find( "/" );
	  if ( epos != string::npos ){
	    basename = basename.substr( epos+1 );
	    map<string,NgramServerClass*>::const_iterator it= servers->find(basename);
	    if ( it != servers->end() ){
	      NgramServerClass api(*it->second);
	      LogStream LS( &myLog );
	      LogStream DS( &myLog );
	      DS.message(logLine);
	      LS.message(logLine);
	      DS.setstamp( StampBoth );
	      LS.setstamp( StampBoth );
	      nb_putline( args->os(), tmp , timeout );
	    }
	    else {
	      *Dbg(myLog) << "invalid BASE! '" << basename
			  << "'" << endl;
	      args->os() << "invalid basename: '" << basename << "'" << endl;
	    }
	    args->os() << endl;
	  }
	}
      }
    }
  }
}

NgramServerClass::NgramServerClass( Timbl::TimblOpts& opts ){
  string val;
  bool mood;

  if ( opts.Find( 'c', val, mood ) ){
    if ( !TiCC::stringTo( val, clip ) ){
      cerr << "illegal value for option -c" << endl;
      usage();
    }
  }
  else
    clip = 0;
  if ( opts.Find( 'n', val, mood ) ){
    if ( !TiCC::stringTo( val, nGram ) ){
      cerr << "illegal value for option -n" << endl;
      usage();
    }
  }
  else {
    cerr << "Missing option -n " << endl;
    usage();
  }
  if ( opts.Find( "f", val, mood ) ){
    ifstream is( val.c_str() );
    if ( !is ){
      cerr << "couldn't open ngram file: " << val << endl;
      exit( EXIT_FAILURE );
    }
    fill_table( is, nGram );
  }
  else {
    cerr << "Missing option -f " << endl;
    usage();
  }
  cerr << "Ngram Server " << VERSION << endl;
  cerr << "based on " << TimblServer::VersionName() << endl;
}

NgramServerClass::~NgramServerClass(){
  delete dict;
}

void NgramServerClass::fill_table( std::istream& is, int ngram ){
  dict = new multimap<string,string>();
  string line;
  size_t count = 0;
  while ( getline( is, line ) ){
    vector<string> vec;
    int num = TiCC::split_at( line, vec, "\t" );
    if ( num == 3 || num == 4 ){
      size_t freq = TiCC::stringTo<size_t>( vec[1] );
      if ( clip != 0 && freq <= clip )
	break;
      vector<string> vecn;
      num = TiCC::split( vec[0], vecn );
      if ( num != ngram ){
	throw runtime_error( "mismatch between -n value and values in file" );
      }
      else {
	string key = vecn[0] + "-" + vecn[ngram-1];
	//	cerr << "insert " << key << " (" << vec[0] << ")" << endl;
	dict->insert( make_pair( key, vec[0] ) );
	++count;
      }
    }
  }
  cout << "inserted " << count << " values in the dictionary." << endl;
}

size_t NgramServerClass::get_count( const std::string& what ) {
  if ( last_query != what ){
    result_set.clear();
    last_query = what;
    size_t cnt = dict->count( what );
    if ( cnt > 0 ){
      size_t val = 1;
      multimap<string,string>::const_iterator pos;
      for ( pos = dict->lower_bound( what );
	    pos != dict->upper_bound( what );
	    ++pos ){
	result_set.push_back( pos->second );
      }
    }
  }
  return result_set.size();
}

void NgramServerClass::count( const std::string& what, ostream& os ){
  size_t size = get_count( what );
  os << result_set.size() << " hits" << endl;
}

void NgramServerClass::lookup( const std::string& what, ostream& os ){
  size_t size = get_count( what );
  for( size_t i=0; i < result_set.size(); ++i ){
    os << "[" << i << "]=" << result_set[i] << endl;
  }
}

void NgramServerClass::get_results( const std::string& what,
				    size_t start, size_t num,
				    ostream& os ){
  if ( num == 0 )
    num = result_set.size();
  for( size_t i=start-1; i < result_set.size() && i < num+start-1; ++i ){
    os << "[" << i+1 << "]=" << result_set[i] << endl;
  }
}

void NgramServerClass::exec( const string& line, ostream& os ){
  vector<string> vec;
  int num = TiCC::split( line, vec );
  if ( num == 1 ){
    if ( TiCC::lowercase( vec[0] ) == "r" && !last_query.empty() ){
      get_results( last_query, 1, 0, os );
    }
    else {
      os << "please search something first" << endl;
      return;
    }
  }
  else if ( num == 2 ){
    string look = vec[0] + "-" + vec[1];
    lookup( look, os );
  }
  else if ( num == 3 ){
    string command = TiCC::lowercase( vec[0] );
    if ( command == "c" ){
      string look = vec[1] + "-" + vec[2];
      count( look, os );
    }
    else if ( command == "l" ){
      string look = vec[1] + "-" + vec[2];
      lookup( look, os );
    }
    else if ( command == "r" ){
      size_t start;
      if ( !TiCC::stringTo( vec[1], start ) ){
	os << "illegal start value: " << vec[1] << endl;
	return;
      }
      if ( start <= 0 ){
	os << "invalid startPosition: " << start << " should be > 0 " << endl;
	return;
      }
      size_t num;
      if ( !TiCC::stringTo( vec[2], num ) ){
	os << "illegal num value: " << vec[2] << endl;
	return;
      }
      if ( num <= 0 ){
	os << "invalid maximumItems: " << num << " should be > 0 " << endl;
	return;
      }
      get_results( last_query, start, num, os );
    }
    else {
      os << "unknown command " << command << " in " << line << endl;
    }
  }
  else {
    os << "illegal command " << line << endl;
  }
}

ServerBase *startServer( TimblOpts& opts ){
  bool mood;
  string value;
  Configuration *config = new Configuration();
  bool old = false;
  if ( !opts.Find( "config", value, mood ) ){
    if ( opts.Find( 'S', value, mood ) ){
      config->setatt( "port", value );
      old = true;
      if ( opts.Find( 'C', value, mood ) ){
	config->setatt( "maxconn", value );
      }
    }
    if ( !old ){
      cerr << "missing --config option" << endl;
      return 0;
    }
  }
  else if ( !config->fill( value ) ){
    cerr << "unable to read a configuration from " << value << endl;
    return 0;
  }
  if ( opts.Find( "pidfile", value, mood ) ){
    config->setatt( "pidfile", value );
  }
  if ( opts.Find( "logfile", value, mood ) ){
    config->setatt( "logfile", value );
  }
  if ( opts.Find( "daemonize", value, mood ) ){
    config->setatt( "daemonize", value );
  }
  if ( opts.Find( "debug", value, mood ) ){
    config->setatt( "debug", value );
    opts.Delete( "debug" );
  }
  string protocol = config->lookUp( "protocol" );
  if ( protocol.empty() )
    protocol = "tcp";
  if ( protocol == "tcp" )
    return new TcpServer( config );
  else if ( protocol == "http" )
    return new HttpServer( config );
  else {
    cerr << "unknown protocol " << protocol << endl;
    return 0;
  }
}


void init( ServerBase *server, TimblOpts& opts ){
  map<string,string> allvals;
  if ( server->config->hasSection("dicts") )
    allvals = server->config->lookUpAll("dicts");
  else {
    allvals = server->config->lookUpAll("global");
    // old style, everything is global
    // remove all already processed stuff
    map<string,string>::iterator it = allvals.begin();
    while ( it != allvals.end() ){
      if ( it->first == "port" ||
	   it->first == "protocol" ||
	   it->first == "logfile" ||
	   it->first == "debug" ||
	   it->first == "pidfile" ||
	   it->first == "daemonize" ||
	   it->first == "maxconn" ){
	allvals.erase(it++);
      }
      else {
	++it;
      }
    }
  }
  map<string, NgramServerClass*> *servers = new map<string, NgramServerClass*>();
  server->callback_data = servers;
  server->myLog.message( "ngram" );

  if ( allvals.empty() ){
    cerr << "opts: " << opts << endl;
    // old style stuff
    NgramServerClass *run = new NgramServerClass( opts );
    if ( run ){
      (*servers)["default"] = run;
      server->myLog << "started ngram server " << endl;
    }
    else {
      server->myLog << "FAILED to start ngram server " << endl;
    }
  }
  else {
    map<string,string>::iterator it = allvals.begin();
    while ( it != allvals.end() ){
      cerr << "OPTS: " << it->second << endl;
      TimblOpts opts( it->second );
      string dictName;
      NgramServerClass *run = new NgramServerClass( opts );
      if ( run ){
	(*servers)[it->first] = run;
	server->myLog << "started experiment " << it->first
		      << " with parameters: " << it->second << endl;
      }
      else {
	server->myLog << "FAILED to start experiment " << it->first
		      << " with parameters: " << it->second << endl;
      }
      ++it;
    }
  }
}

int main(int argc, char *argv[]) {
  Timbl::TimblOpts opts( argc, argv );
  string val;
  bool mood;
  if ( opts.Find( "h", val, mood ) ||
       opts.Find( "help", val, mood ) ){
    usage();
  }
  if ( opts.Find( "V", val, mood ) ||
       opts.Find( "version", val, mood ) ){
    exit( EXIT_SUCCESS );
  }
  ServerBase *server = startServer( opts );
  init( server, opts );
  return server->Run(); // returns EXIT_SUCCESS or EXIT_FAIL
}

