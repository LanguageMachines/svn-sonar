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
#include "timblserver/TimblServerAPI.h"
#include "timblserver/FdStream.h"
#include "ticcutils/StringOps.h"
#include "include/sonar/ngramserver.h"

using namespace std;
using namespace TiCC;

#define SLOG (*Log(cur_log))
#define SDBG (*Dbg(cur_log))

#define LOG cerr

inline void usage(){
  cerr << "usage:  anaserv -S port [-f hashfile]" << endl;
  cerr << " or     anaserv -S port [-l lexiconfile]" << endl;
  exit( EXIT_SUCCESS );
}

NgramServerClass::NgramServerClass( Timbl::TimblOpts& opts ):
  cur_log("NgramServ", StampMessage ){
  string val;
  bool mood;
  if ( opts.Find( 'D', val, mood ) ){
    if ( val == "LogNormal" )
      cur_log.setlevel( LogNormal );
    else if ( val == "LogDebug" )
      cur_log.setlevel( LogDebug );
    else if ( val == "LogHeavy" )
      cur_log.setlevel( LogHeavy );
    else if ( val == "LogExtreme" )
      cur_log.setlevel( LogExtreme );
    else {
      cerr << "Unknown Debug mode! (-D " << val << ")" << endl;
    }
    opts.Delete( 'D' );
  }
  if ( opts.Find( 'S', val, mood ) ){
    if ( !TiCC::stringTo( val, serverPort ) ){
      cerr << "illegal value for option -S" << endl;
      usage();
    }
  }
  else {
    cerr << "Missing value for option -S" << endl;
  }
  if ( !opts.Find( 'm', val, mood ) ){
    if ( !TiCC::stringTo( val, maxConn ) ){
      cerr << "illegal value for option -m" << endl;
      usage();
    }
    else
      maxConn = 20;
  }
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
  doDaemon = true;
  dbLevel = LogNormal;
  if ( opts.Find( "pidfile", val ) ) {
    pidFile = val;
    opts.Delete( "pidfile" );
  }
  if ( opts.Find( "logfile", val ) ) {
    logFile = val;
    opts.Delete( "logfile" );
  }
  if ( opts.Find( "daemonize", val ) ) {
    doDaemon = ( val != "no" && val != "NO" && val != "false" && val != "FALSE" );
    opts.Delete( "daemonize" );
  }
  RunServer();
}

NgramServerClass::~NgramServerClass(){
}

void NgramServerClass::fill_table( std::istream& is, int ngram ){
  string line;
  while ( getline( is, line ) ){
    vector<string> vec;
    int num = TiCC::split_at( line, vec, "\t" );
    if ( num == 3 ){
      vector<string> vecn;
      num = TiCC::split( vec[0], vecn );
      if ( num != ngram ){
	throw runtime_error( "mismatch between -n value and values in file" );
      }
      else {
	string key = vecn[0] + "-" + vecn[ngram-1];
	cerr << "insert " << key << " (" << vec[0] << ")" << endl;
	table.insert( make_pair( key, vec[0] ) );
      }
    }
  }
}

void NgramServerClass::lookup( const std::string& what, ostream& os ) const {
  size_t cnt = table.count( what );
  os << cnt << " hits" << endl;
  if ( cnt > 0 ){
    size_t val = 1;
    multimap<string,string>::const_iterator pos;
    for ( pos = table.lower_bound( what );
	  pos != table.upper_bound( what );
	  ++pos ){
      os << "[" << val++ << "]: " << pos->second << endl;
    }
  }
}

struct childArgs{
  NgramServerClass *Mother;
  Sockets::ServerSocket *socket;
  int maxC;
};

void StopServerFun( int Signal ){
  if ( Signal == SIGINT ){
    exit(EXIT_FAILURE);
  }
  signal( SIGINT, StopServerFun );
}

void BrokenPipeChildFun( int Signal ){
  if ( Signal == SIGPIPE ){
    signal( SIGPIPE, BrokenPipeChildFun );
  }
}

void AfterDaemonFun( int Signal ){
  if ( Signal == SIGCHLD ){
    exit(1);
  }
}

void NgramServerClass::exec( const string& line, ostream& os ){
  vector<string> vec;
  int num = TiCC::split( line, vec );
  if ( num != 2 ){
    os << "illegal lookup " << line << endl;
  }
  else {
    string look = vec[0] + "-" + vec[1];
    lookup( look, os );
  }
}

// ***** This is the routine that is executed from a new thread **********
void *runChild( void *arg ){
  childArgs *args = (childArgs *)arg;
  NgramServerClass *theServer = args->Mother;
  LogStream *cur_log = &theServer->cur_log;
  Sockets::Socket *Sock = args->socket;
  static int service_count = 0;
  static pthread_mutex_t my_lock = PTHREAD_MUTEX_INITIALIZER;
  //
  // use a mutex to update the global service counter
  //
  pthread_mutex_lock( &my_lock );
  service_count++;
  if ( service_count > args->maxC ){
    Sock->write( "Maximum connections exceeded\n" );
    Sock->write( "try again later...\n" );
    pthread_mutex_unlock( &my_lock );
    SLOG << "Thread " << pthread_self() << " refused " << endl;
  }
  else {
    // Greeting message for the client
    //
    pthread_mutex_unlock( &my_lock );
    time_t timebefore, timeafter;
    time( &timebefore );
    // report connection to the server terminal
    //
    SLOG << "Thread " << pthread_self() << ", Socket number = "
	 << Sock->getSockId() << ", started at: "
	 << asctime( localtime( &timebefore) );
    signal( SIGPIPE, BrokenPipeChildFun );
    fdistream is( Sock->getSockId() );
    fdostream os( Sock->getSockId() );
    os << "Welcome to the Ngram server." << endl;
    string line;
    int count = 0;
    while ( getline( is, line ) ){
      line = TiCC::trim( line );
      if ( line.empty() )
	break;
      SDBG << "Line='" << line << "'" << endl;
      theServer->exec( line, os );
      ++count;
    }
    time( &timeafter );
    SLOG << "Thread " << pthread_self() << ", terminated at: "
	 << asctime( localtime( &timeafter ) );
    SLOG << "Total time used in this thread: " << timeafter - timebefore
	 << " sec, " << count << " lines processed " << endl;
  }
  // exit this thread
  //
  pthread_mutex_lock( &my_lock );
  service_count--;
  SLOG << "Socket total = " << service_count << endl;
  pthread_mutex_unlock( &my_lock );
  delete Sock;
  return 0;
}

bool NgramServerClass::RunServer(){
  cerr << "trying to start a Server on port: " << serverPort << endl
       << "maximum # of simultaneous connections: " << maxConn
       << endl;
  if ( !logFile.empty() ){
    ostream *tmp = new ofstream( logFile.c_str() );
    if ( tmp && tmp->good() ){
      cerr << "switching logging to file " << logFile << endl;
      cur_log.associate( *tmp );
      cur_log.message( "T-Scan:" );
      SLOG << "Started logging " << endl;
    }
    else {
      cerr << "unable to create logfile: " << logFile << endl;
      cerr << "not started" << endl;
      exit(EXIT_FAILURE);
    }
  }
  int start = 0;
  if ( doDaemon ){
    signal( SIGCHLD, AfterDaemonFun );
    if ( logFile.empty() )
      start = TimblServer::daemonize( 1, 1 );
    else
      start = TimblServer::daemonize( 0, 0 );
  }
  if ( start < 0 ){
    LOG << "Failed to daemonize error= " << strerror(errno) << endl;
    exit(EXIT_FAILURE);
  };
  if ( !pidFile.empty() ){
    // we have a liftoff!
    // signal it to the world
    remove( pidFile.c_str() ) ;
    ofstream pid_file( pidFile.c_str() ) ;
    if ( !pid_file ){
      LOG << "Unable to create pidfile:"<< pidFile << endl;
      LOG << "TimblServer NOT Started" << endl;
      exit(EXIT_FAILURE);
    }
    else {
      pid_t pid = getpid();
      pid_file << pid << endl;
    }
  }

  // set the attributes
  pthread_attr_t attr;
  if ( pthread_attr_init(&attr) ||
       pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED ) ){
    LOG << "Threads: couldn't set attributes" << endl;
    exit(EXIT_FAILURE);
  }
  //
  // setup Signal handling to abort the server.
  signal( SIGINT, StopServerFun );

  pthread_t chld_thr;

  // start up server
  //
  LOG << "Started Server on port: " << serverPort << endl
      << "Maximum # of simultaneous connections: " << maxConn
      << endl;

  Sockets::ServerSocket server;
  string portString = TiCC::toString<int>(serverPort);
  if ( !server.connect( portString ) ){
    LOG << "Failed to start Server: " << server.getMessage() << endl;
    exit(EXIT_FAILURE);
  }
  if ( !server.listen( maxConn ) < 0 ){
    LOG << "Server: listen failed " << strerror( errno ) << endl;
    exit(EXIT_FAILURE);
  };

  while(true){ // waiting for connections loop
    Sockets::ServerSocket *newSock = new Sockets::ServerSocket();
    if ( !server.accept( *newSock ) ){
      if( errno == EINTR )
	continue;
      else {
	LOG << "Server: Accept Error: " << server.getMessage() << endl;
	exit(EXIT_FAILURE);
      }
    }
    LOG << "Accepting Connection " << newSock->getSockId()
	<< " from remote host: " << newSock->getClientName() << endl;

    // create a new thread to process the incoming request
    // (The thread will terminate itself when done processing
    // and release its socket handle)
    //
    childArgs *args = new childArgs();
    args->Mother = this;
    args->maxC = maxConn;
    args->socket = newSock;
    pthread_create( &chld_thr, &attr, runChild, (void *)args );
    // the server is now free to accept another socket request
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
  NgramServerClass server( opts );
  exit(EXIT_SUCCESS);
}

