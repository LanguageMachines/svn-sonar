#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cstdlib>
#include <string>
#include <map>
#include <vector>
#include "unicode/unistr.h"
#include "unicode/uchar.h"
#include "unicode/ustream.h"
#include <iostream>
#include <fstream>

#include "config.h"
#include "config.h"
#ifdef HAVE_OPENMP
#include "omp.h"
#endif

using namespace	std;

UnicodeString UTF8ToUnicode( const string& s ){
  return UnicodeString( s.c_str(), s.length(), "UTF-8" );
}

string UnicodeToUTF8( const UnicodeString& s ){
  string result;
  int len = s.length();
  if ( len > 0 ){
    char *buf = new char[len*6+1];
    s.extract( 0, len, buf, len*6, "UTF-8" );
    result = buf;
    delete [] buf;
  }
  return result;
}

UChar32 replace( int val ){
  switch ( val ){
  case 0x80: return 0x20AC;   // EURO SIGN
  case 0x82: return 0x201A;   // SINGLE LOW-9 QUOTATION MARK
  case 0x83: return 0x0192;   // LATIN SMALL LETTER F WITH HOOK
  case 0x84: return 0x201E;   // DOUBLE LOW-9 QUOTATION MARK
  case 0x85: return 0x2026;   // HORIZONTAL ELLIPSIS
  case 0x86: return 0x2020;   // DAGGER
  case 0x87: return 0x2021;   // DOUBLE DAGGER
  case 0x88: return 0x02C6;   // MODIFIER LETTER CIRCUMFLEX ACCENT
  case 0x89: return 0x2030;   // PER MILLE SIGN
  case 0x8a: return 0x0160;   // LATIN CAPITAL LETTER S WITH CARON
  case 0x8b: return 0x2039;   // SINGLE LEFT-POINTING ANGLE QUOTATION MARK
  case 0x8c: return 0x0152;   // LATIN CAPITAL LIGATURE OE
  case 0x8e: return 0x017D;   // LATIN CAPITAL LETTER Z WITH CARON
  case 0x91: return 0x2018;   // LEFT SINGLE QUOTATION MARK
  case 0x92: return 0x2019;   // RIGHT SINGLE QUOTATION MARK
  case 0x93: return 0x201C;   // LEFT DOUBLE QUOTATION MARK
  case 0x94: return 0x201D;   // RIGHT DOUBLE QUOTATION MARK
  case 0x95: return 0x2022;   // BULLET
  case 0x96: return 0x2013;   // EN DASH
  case 0x97: return 0x2014;   // EM DASH
  case 0x98: return 0x02DC;   // SMALL TILDE
  case 0x99: return 0x2122;   // TRADE MARK SIGN
  case 0x9a: return 0x0161;   // LATIN SMALL LETTER S WITH CARON
  case 0x9b: return 0x203A;   // SINGLE RIGHT-POINTING ANGLE QUOTATION MARK
  case 0x9c: return 0x0153;   // LATIN SMALL LIGATURE OE
  case 0x9e: return 0x017E;   // LATIN SMALL LETTER Z WITH CARON
  case 0x9f: return 0x0178;  // LATIN CAPITAL LETTER Y WITH DIAERESIS
  default:
    return val;
  }
}

void cleanup( const string& file ){
#pragma omp critical
  {
    cout << "cleaning " << file << endl;
  }
  ifstream is( file.c_str() );
  if ( !is ){
    cerr << "unable to open input file " << file << endl;
  }
  string outfile = file + ".clean";
  ofstream os( outfile.c_str() );
  if ( !os ){
    cerr << "unable to open output file " << outfile << endl;
  }
  string line;
  while ( getline( is, line ) ){
    UnicodeString us = UTF8ToUnicode( line );
    for ( int i=0; i < us.length(); ++ i ){
      int val = int(us[i]);
      if ( val > 0x80 && val <= 0x9f ){
	UChar32 rep = replace( val );
	//	cerr << "moron: " << val << " ==> " << rep << endl;
	us.replace(i,1, UChar32(rep) );
      }
      if ( !u_isdefined( us[i] ) )
	cerr << "AHA" << endl;
    }
    string trans = UnicodeToUTF8( us );
    os << trans << endl;
  }
}

int main( int argc, char *argv[] ){
  if ( argc < 2	){
    cerr << "Usage: [-t number_of_threads]  [-n Ngram count] dir/filename " << endl;
    exit(EXIT_FAILURE);
  }
  int opt;
  int numThreads=0;
  while ((opt = getopt(argc, argv, "ht:V")) != -1) {
    switch (opt) {
    case 't':
      numThreads = atoi(optarg);
      break;
    case 'V':
      cerr << PACKAGE_STRING << endl;
      exit(EXIT_SUCCESS);
      break;
    case 'h':
      cerr << "Usage: [options] file/dir" << endl;
      cerr << "\t-n\t Ngram count " << endl;
      cerr << "\t-t\t number_of_threads" << endl;
      cerr << "\t-h\t this messages " << endl;
      cerr << "\t-V\t show version " << endl;
      cerr << "\t will produce ngram statistics for a FoLiA file, " << endl;
      cerr << "\t or a whole directoy of FoLiA files " << endl;
      exit(EXIT_SUCCESS);
      break;
    default: /* '?' */
      cerr << "Usage: [-t number_of_threads]  [-n Ngram count] dir/filename " << endl;
      exit(EXIT_FAILURE);
    }
  }
  
  vector<string> fileNames;
  string dirName;
  string name = argv[optind];
  struct stat st_buf;
  int status = stat( name.c_str(), &st_buf );
  if ( status != 0 ){
    cerr << "parameter '" << name << "' doesn't seem to be a file or directory"
	 << endl;
    exit(EXIT_FAILURE);
  }
  if ( S_ISREG (st_buf.st_mode) ){
    fileNames.push_back( name );
    string::size_type pos = name.rfind( "/" );
    if ( pos != string::npos )
      dirName = name.substr(0,pos);
  }
  else if ( S_ISDIR (st_buf.st_mode) ){
    string::size_type pos = name.rfind( "/" );
    if ( pos != string::npos )
      name.erase(pos);
    dirName = name;
    DIR *dir = opendir( dirName.c_str() );
    if ( !dir ){
      cerr << "unable to open dir" << dirName << endl;
      exit(EXIT_FAILURE);
    }
    cout << "Processing dir '" << dirName << "'" << endl;
    struct stat sb;
    struct dirent *entry = readdir( dir );
    while ( entry ){
      if (entry->d_name[0] != '.') {
	string fullName = dirName + "/" + entry->d_name;
	string::size_type pos = fullName.find( ".xml" );
	if ( pos != string::npos && fullName.substr( pos ).length() == 4 ){
	  fileNames.push_back( fullName );
	}
      }
      entry = readdir( dir );
    }
    closedir( dir );
  }
  size_t toDo = fileNames.size();
  if ( toDo > 1 )
    cout << "start processing of " << toDo << " files " << endl;
#pragma omp parallel for shared(fileNames)  
  for ( size_t fn=0; fn < fileNames.size(); ++fn ){
    cleanup( fileNames[fn] );    
  }
  cout << "done" << endl;
  exit(EXIT_SUCCESS);
}
