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

class HttpServer : public HttpServerBase {
public:
  void callback( childArgs* );
  HttpServer( const TiCC::Configuration *c ): HttpServerBase( c ){};
};

#define IS_DIGIT(x) (((x) >= '0') && ((x) <= '9'))
#define IS_HEX(x) ((IS_DIGIT(x)) || (((x) >= 'a') && ((x) <= 'f')) || \
            (((x) >= 'A') && ((x) <= 'F')))


string url_decode( const string& s ) {
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

void xml_error( ostream& os, const string& message ){
  XmlDoc doc( "searchResponse" );
  xmlNode *root = doc.getRoot();
  XmlNewTextChild( root, "diagnostics", message );
  string out = doc.toString();
  int timeout = 10;
  nb_putline( os, out , timeout );
  os << endl;
}

void HttpServer::callback( childArgs *args ){
  // process the test material
  // report connection to the server terminal
  //
  args->socket()->setNonBlocking();
  map<string, NgramServerClass*> *servers =
    static_cast<map<string, NgramServerClass*> *>(callback_data);
  char logLine[256];
  sprintf( logLine, "HTTP-Thread %zd, on Socket %d", (uintptr_t)pthread_self(),
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
	*Dbg(myLog) << "skip: read:'" << tmp << "'" << endl;;
      }
      string::size_type spos = Line.find( "GET" );
      if ( spos != string::npos ){
	string::size_type epos = Line.find( " HTTP" );
	string line = Line.substr( spos+3, epos - spos - 3 );
	*Dbg(myLog) << "URL='" << line << "'" << endl;
	epos = line.find( "?" );
	string basename;
	if ( epos != string::npos ){
	  basename = line.substr( 0, epos );
	  string qstring = line.substr( epos+1 );
	  epos = basename.find( "/" );
	  if ( epos != string::npos ){
	    basename = basename.substr( epos+1 );
	    *Dbg(myLog) << "base='" << basename << "'" << endl;
	    map<string,NgramServerClass*>::const_iterator it= servers->find(basename);
	    if ( it != servers->end() ){
	      *Dbg(myLog) << "found experiment '" << basename << "'" << endl;
	      vector<string> qargs;
	      size_t num = split_at( qstring, qargs, "&" );
	      string querys;
	      string start;
	      string max;
	      for( size_t i=0; i < num; ++i ){
		vector<string> parts;
		int nump = split_at( qargs[i], parts, "=" );
		if ( nump != 2 ){
		  xml_error( args->os(), "invalid query: " + qstring );
		  return;
		}
		if ( parts[0] == "startPosition" ){
		  start = parts[1];
		}
		else if ( parts[0] == "maximumItems" ){
		  max = parts[1];
		}
		else if ( parts[0] == "query" ){
		 querys = parts[1];
		}
		else {
		  xml_error( args->os(), "unsupported item " + parts[0]
			     + " in query" );
		  return;
		}
	      }
	      string query = url_decode( querys );
	      *Dbg(myLog) << "query='" << query << "'" << endl;
	      *Dbg(myLog) << "startPosString='" << start << "'" << endl;
	      *Dbg(myLog) << "maxItemsString='" << max << "'" << endl;
	      string command = "show";
	      string search;
	      vector<string> qparts;
	      int numq = split( query, qparts );
	      if ( numq > 0 ){
		command = qparts[0];
		if ( command == "count" ){
		  if ( numq < 3 ){
		    xml_error( args->os(), "not enough arguments for 'count' "
			       + query );
		    return;
		  }
		  else if ( numq > 3 ){
		    xml_error( args->os(), "to many arguments for 'count'"
			       + query );
		    return;
		  }
		  search = qparts[1] + "-" + qparts[2];
		}
		else if ( command == "show" ){
		  if ( numq < 3 ){
		    xml_error( args->os(), "not enough arguments for 'show' "
			       + query );
		    return;
		  }
		  else if ( numq > 3 ){
		    xml_error( args->os(), "to many arguments for 'show'"
			       + query );
		    return;
		  }
		  search = qparts[1] + "-" + qparts[2];
		}
		else {
		  xml_error( args->os(), "unsupported command"	+ command );
		  return;
		}
	      }
	      *Dbg(myLog) << "command='" << command << "'" << endl;
	      int startPos = 1;
	      if ( !start.empty() ){
		if ( !stringTo( start, startPos ) ||
		     startPos < 1 ){
		  xml_error( args->os(), "invalid value for startPosition: "
			     + start );
		  return;
		}
	      }
	      *Dbg(myLog) << "startPos='" << startPos << "'" << endl;
	      int maxItems = 0;
	      if ( !max.empty() ){
		if ( !stringTo( max, maxItems ) ){
		  xml_error( args->os(), "invalid value for maximumItems: "
			     + start );
		  return;
		}
	      }
	      *Dbg(myLog) << "maxItems='" << maxItems << "'" << endl;
	      NgramServerClass api(*it->second);
	      LogStream LS( &myLog );
	      LogStream DS( &myLog );
	      DS.message(logLine);
	      LS.message(logLine);
	      DS.setstamp( StampBoth );
	      LS.setstamp( StampBoth );
	      XmlDoc doc( "searchResponse" );
	      xmlNode *root = doc.getRoot();
	      XmlSetAttribute( root, "ngram-size",
			       toString(it->second->ngramval() ) );
	      if ( command == "count" ){
		size_t cnt = api.get_count( search );
		XmlNewTextChild( root, "numberOfItems", toString(cnt) );
	      }
	      else {
		vector<string> res;
		size_t next = 0;
		size_t cnt = api.get_result( res, search, startPos, maxItems,
					     next );
		XmlNewTextChild( root, "resultSetId", search );
		XmlNewTextChild( root, "numberOfItems", toString(cnt) );
		xmlNode *recs = XmlNewChild( root, "records" );

		for ( size_t i=0; i < res.size(); ++i ){
		  xmlNode *rec = XmlNewChild( recs, "record" );
		  XmlNewTextChild( rec, "recordSchema", "info:srw/schema/1/dc-v1.1" );
		  XmlNewTextChild( rec, "recordPacking", "string" );
		  XmlNewTextChild( rec, "recordData", res[i] );
		  XmlNewTextChild( rec, "recordPosition",
				   toString( startPos+i ) );
		}
		if ( next != 0 )
		  XmlNewTextChild( root, "nextRecordPosition", toString(next) );
	      }
	      XmlNewTextChild( root, "echoedRequest", querys );
	      string out = doc.toString();
	      *Dbg(myLog) << "serialized doc '" << out << "'" << endl;
	      timeout = 10;
	      nb_putline( args->os(), out , timeout );
	      args->os() << endl;
	    }
	    else {
	      *Dbg(myLog) << "invalid BASE! '" << basename
			  << "'" << endl;
	      xml_error( args->os(), "invalid base: " + basename );
	      return;
	    }
	    args->os() << endl;
	  }
	}
      }
    }
  }
}

NgramServerClass::NgramServerClass( Timbl::TimblOpts& opts,
				    LogStream* ls ): cloned(false), log(ls) {
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
  result_sets = new map<string,resultSet*>();
  cerr << "Ngram Server " << VERSION << endl;
  cerr << "based on " << TimblServer::VersionName() << endl;
}

NgramServerClass::NgramServerClass( const NgramServerClass& in ){
  dict = in.dict;
  cloned = true;
  nGram = in.nGram;
  clip = in.clip;
  log = in.log;
  result_sets = in.result_sets;
}

NgramServerClass::~NgramServerClass(){
  if ( !cloned ){
    delete dict;
    map<string,resultSet*>::iterator it = result_sets->begin();
    while( it != result_sets->end() ){
      delete it->second;
      ++it;
    }
    delete result_sets;
  }
  else {
    cleanResults( 300 );
  }
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

resultSet *NgramServerClass::getResultSet( const string& what ) const {
  map<string,resultSet*>::const_iterator it = result_sets->find( what );
  if ( it == result_sets->end() ){
    return 0;
  }
  else {
    return it->second;
  }
}

size_t NgramServerClass::get_count( const std::string& what ) {
  resultSet* rs = getResultSet( what );
  size_t size = 0;
  static pthread_mutex_t rs_lock = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_lock(&rs_lock);
  // use a mutex to guard the global result_set
  if ( rs == 0 ){
    resultSet *search = new resultSet(what);
    size_t cnt = dict->count( what );
    if ( cnt > 0 ){
      size_t val = 1;
      multimap<string,string>::const_iterator pos;
      for ( pos = dict->lower_bound( what );
	    pos != dict->upper_bound( what );
	    ++pos ){
	search->rset.push_back( pos->second );
      }
    }
    result_sets->insert( make_pair( what, search ) );
    size = search->rset.size();
  }
  else {
    time(&rs->start);
    size = rs->rset.size();
  }
  pthread_mutex_unlock( &rs_lock );
  return size;
}

size_t NgramServerClass::get_result( vector<string>& res,
				     const string& what,
				     size_t start, size_t num, size_t& next ){
  res.clear();
  size_t size = get_count( what );
  resultSet *rs = getResultSet( what );
  *Log(log) << "retrieved result set: '" << what << "'" << endl;
  if ( size == 0 ){
    next = 0;
  }
  else {
    if ( num == 0 )
      num = rs->rset.size();
    next = start - 1;
    for( size_t i=start-1; i < rs->rset.size() && i < num+start-1; ++i ){
      res.push_back( rs->rset[i] );
      ++next;
    }
    if ( ++next > size )
      next = 0;
  }
  return size;
}

void NgramServerClass::cleanResults( int keep ) {
  static pthread_mutex_t rs_lock = PTHREAD_MUTEX_INITIALIZER;
  time_t now;
  time( &now );
  pthread_mutex_lock(&rs_lock);
  map<string,resultSet*>::iterator it = result_sets->begin();
  while ( it != result_sets->end() ){
    if ( (it->second->start + keep) < now ){
      *Log(log) << "erasing result set: '" << it->first << "'" << endl;
      delete it->second;
      result_sets->erase( it++ );
    }
    else {
      ++it;
    }
  }
  pthread_mutex_unlock(&rs_lock);
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
  if ( protocol == "http" || protocol.empty() )
    return new HttpServer( config );
  else {
    cerr << "unsupported protocol " << protocol << endl;
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
    NgramServerClass *run = new NgramServerClass( opts, &server->myLog );
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
      NgramServerClass *run = new NgramServerClass( opts, &server->myLog );
      if ( run ){
	(*servers)[it->first] = run;
	server->myLog << "run = " << (void*)run << endl;
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

