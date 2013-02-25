#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cstdlib>
#include <string>
#include <map>
#include <iostream>
#include <fstream>

#include "libfolia/document.h"
#include "libfolia/folia.h"

#include "config.h"
#ifdef HAVE_OPENMP
#include "omp.h"
#endif

using namespace	std;
using namespace	folia;


void create_wf_list( const map<string, unsigned int>& wc, 
		     const string& filename, unsigned int total ){
  ofstream os( filename.c_str() );
  if ( !os ){
    cerr << "failed to create outputfile '" << filename << "'" << endl;
    exit(EXIT_FAILURE);
  }
  map<unsigned int, set<string> > wf;
  map<string,unsigned int >::const_iterator cit = wc.begin();
  while( cit != wc.end()  ){
    wf[cit->second].insert( cit->first );
    ++cit;
  }
  unsigned int sum=0;
  map<unsigned int, set<string> >::const_reverse_iterator wit = wf.rbegin();
  while ( wit != wf.rend() ){
    set<string>::const_iterator sit = wit->second.begin();
    while ( sit != wit->second.end() ){
      sum += wit->first;
      os << *sit << "\t" << wit->first << "\t" << sum << "\t" 
	 << 100 * double(sum)/total << endl;
      ++sit;
    }
    ++wit;
  }
#pragma omp critical
  {
    cout << "created WordFreq list '" << filename << "'" << endl;
  }
}

struct rec {
  unsigned int count;
  map<string,unsigned int> pc;
};

void create_lf_list( const map<string, unsigned int>& lc, 
		     const string& filename, unsigned int total ){
  ofstream os( filename.c_str() );
  if ( !os ){
    cerr << "failed to create outputfile '" << filename << "'" << endl;
    exit(EXIT_FAILURE);
  }
  map<unsigned int, set<string> > lf;
  map<string,unsigned int >::const_iterator cit = lc.begin();
  while( cit != lc.end()  ){
    lf[cit->second].insert( cit->first );
    ++cit;
  }

  unsigned int sum=0;
  map<unsigned int, set<string> >::const_reverse_iterator wit = lf.rbegin();
  while ( wit != lf.rend() ){
    set<string>::const_iterator sit = wit->second.begin();
    while ( sit != wit->second.end() ){
      sum += wit->first;
      os << *sit << "\t" << wit->first << "\t" << sum << "\t"
	 << 100* double(sum)/total << endl;
      ++sit;
    }
    ++wit;
  }
#pragma omp critical 
  {
    cout << "created LemmaFreq list '" << filename << "'" << endl;
  }
}

void create_lpf_list( const multimap<string, rec>& lpc,
		      const string& filename, unsigned int total ){
  ofstream os( filename.c_str() );
  if ( !os ){
    cerr << "failed to create outputfile '" << filename << "'" << endl;
    exit(EXIT_FAILURE);
  }
  multimap<unsigned int, pair<string,string> > lpf;
  multimap<string,rec>::const_iterator cit = lpc.begin();
  while( cit != lpc.end()  ){
    map<string,unsigned int>::const_iterator pit = cit->second.pc.begin();
    while ( pit != cit->second.pc.end() ){
      lpf.insert( make_pair( pit->second, 
			     make_pair( cit->first, pit->first ) ) );
      ++pit;
    }
    ++cit;
  }
  unsigned int sum =0;
  multimap<unsigned int, pair<string,string> >::const_reverse_iterator wit = lpf.rbegin();
  while ( wit != lpf.rend() ){
    sum += wit->first;
    os << wit->second.first << " " << wit->second.second << "\t" 
       << wit->first << "\t" << sum << "\t" << 100 * double(sum)/total << endl;
    ++wit;
  }
#pragma omp critical
  {
    cout << "created LemmaPosFreq list '" << filename << "'" << endl;
  }
}

struct wlp_rec {
  string word;
  string lemma;
  string pos;
};

const string frog_cgntagset = "http://ilk.uvt.nl/folia/sets/frog-mbpos-cgn";
const string frog_mblemtagset = "http://ilk.uvt.nl/folia/sets/frog-mblem-nl";

size_t inventory( const Document *d, const string& docName,
		  size_t nG,
		  map<string,unsigned int>& wc,
		  map<string,unsigned int>& lc,
		  multimap<string, rec>& lpc ){
  size_t wordTotal = 0;
  vector<Sentence *> sents = d->sentences(); 
  for ( unsigned int s=0; s < sents.size(); ++s ){  
    vector<Word*> words = sents[s]->words();
    if ( words.size() < nG )
      continue;
    vector<wlp_rec> data;
    for ( unsigned int i=0; i < words.size(); ++i ){
      wlp_rec rec;
      try {
	rec.word = words[i]->str();
      }
      catch(...){
#pragma omp critical
	{
	  cerr << "missing text for word " << words[i]->id() << endl;
	}
	break;
      }
      try {
	rec.lemma = words[i]->lemma(frog_mblemtagset);
      }
      catch(...){
	try {
	  rec.lemma = words[i]->lemma();
	}
	catch(...){
	  rec.lemma = "";
	}
      }
      try {
	rec.pos = words[i]->pos(frog_cgntagset);
      }
      catch(...){
	try {
	  rec.pos = words[i]->pos();
	}
	catch (...){
	  rec.pos = "";
	}
      }
      data.push_back( rec );
    }
    if ( data.size() != words.size() ) {
#pragma omp critical
      {
	cerr << "Error: Missing words! skipped sentence " << sents[s]->id() << " in " << docName << endl;
      }
      continue;
    }

    for ( unsigned int i=0; i <= data.size() - nG ; ++i ){
      string multiw;
      string multil;
      string multip;
      bool lem_mis = false;
      bool pos_mis = false;
      for ( size_t j=0; j < nG; ++j ){
	multiw += data[i+j].word;
	if ( data[i+j].lemma.empty() ){
	  lem_mis = true;
	}
	else if ( !lem_mis ){
	  multil += data[i+j].lemma;
	}
	if ( data[i+j].pos.empty() ){
	  pos_mis = true;
	}
	else if ( !pos_mis ){
	  multip += data[i+j].pos;
	}
	if ( j < nG-1 ){
	  multiw += " ";
	  if ( !lem_mis )
	    multil += " ";
	  if ( !pos_mis )
	    multip += " ";
	}
      }
      ++wordTotal;
#pragma omp critical
      {
	++wc[multiw];
      }
      
      if ( multil.empty() ){
#pragma omp critical
	{
	  cerr << "info: some lemma's are missing in "  << sents[s]->id() << endl;
	}	
      }
      else {
#pragma omp critical
	{
	  ++lc[multil];
	}
	
	if ( multip.empty() ){
#pragma omp critical
	  {
	    cerr << "info: some POS tags are missing in "  << sents[s]->id() << endl;
	  }	
	}
	else {
#pragma omp critical
	  {
	    multimap<string, rec >::iterator it = lpc.find(multil);
	    if ( it == lpc.end() ){
	      rec tmp;
	      tmp.count = 1;
	      tmp.pc[multip]=1;
	      lpc.insert( make_pair(multil,tmp) );
	    }
	    else {
	      ++it->second.count;
	      ++it->second.pc[multip];
	    }
	  }
	}
      }
    }
  }
  return wordTotal;
}

int main( int argc, char *argv[] ){
  if ( argc < 2	){
    cerr << "Usage: [-t number_of_threads]  [-n Ngram count] dir/filename " << endl;
    exit(EXIT_FAILURE);
  }
  int opt;
  int nG=1;
  int numThreads=0;
  while ((opt = getopt(argc, argv, "ht:n:V")) != -1) {
    switch (opt) {
    case 'n':
      nG = atoi(optarg);
      break;
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
#ifdef HAVE_OPENMP
  if ( numThreads != 0 )
    omp_set_num_threads( numThreads );
#else
  if ( numThreads != 0 )
    cerr << "-t option does not work, no OpenMP support in your compiler?" << endl;
#endif
  
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
	string::size_type pos = fullName.find( ".folia.xml" );
	if ( pos != string::npos && fullName.substr( pos ).length() == 10 ){
	  fileNames.push_back( fullName );
	}
      }
      entry = readdir( dir );
    }
    closedir( dir );
  }
  map<string,unsigned int> wc;
  map<string,unsigned int> lc;
  multimap<string, rec> lpc;
  unsigned int wordTotal =0;
  size_t toDo = fileNames.size();
  if ( toDo > 1 )
    cout << "start processing of " << toDo << " files " << endl;
  
#pragma omp parallel for shared(fileNames,wordTotal,wc,lc,lpc)
  for ( size_t fn=0; fn < fileNames.size(); ++fn ){
    string docName = fileNames[fn];
    Document *d = 0;
    try {
      d = new Document( "file='"+ docName + "'" );
    }
    catch ( exception& e ){
#pragma omp critical
      {
	cerr << "failed to load document '" << docName << "'" << endl;
	cerr << "reason: " << e.what() << endl;
      }
      continue;
    }
    size_t count = inventory( d, docName, nG, wc, lc, lpc );
    wordTotal += count;
#pragma omp critical
    {
      cout << "Processed :" << docName << " with " << count << " words."
	   << " still " << --toDo << " files to go." << endl;
    }
    delete d;
  }

  if ( toDo > 1 ){
    cout << "done processsing directory '" << dirName << "' in total " 
	 << wordTotal << " words were found." << endl;
  }
  cout << "start calculating the results" << endl;
  string ext;
  if ( nG > 1 ){
    ext = "." + toString( nG ) + "-gram";
  }
  ext += ".csv";
#pragma omp parallel sections
  {
#pragma omp section
    {
      string filename;
      if ( toDo > 1 )
	filename = dirName + ".wordfreqlist";
      else
	filename = name + ".wordfreqlist";
      filename += ext;
      create_wf_list( wc, filename, wordTotal );
    }
#pragma omp section
    {
      string filename;
      if ( toDo > 1 )
	filename = dirName + ".lemmafreqlist";
      else
	filename = name + ".lemmafreqlist";
      filename += ext;
      create_lf_list( lc, filename, wordTotal );
    }
#pragma omp section
    {
      string filename;
      if ( toDo > 1 )
	filename = dirName + ".lemmaposfreqlist";
      else
	filename = name + ".lemmaposfreqlist";
      filename += ext;
      create_lpf_list( lpc, filename, wordTotal );
    }
  }
}
