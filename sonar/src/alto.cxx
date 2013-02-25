#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cstdlib>
#include <string>
#include <list>
#include <map>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include "libxml/tree.h"
#include "libxml/xpath.h"
#include "config.h"
#ifdef HAVE_OPENMP
#include "omp.h"
#endif

using namespace	std;

  string Name( const xmlNode *node ){
    string result;
    if ( node ){
      result = (char *)node->name;
    }
    return result;
  }

  string XmlContent( const xmlNode *node ){
    string result;
    if ( node ){
      xmlChar *tmp = xmlNodeListGetString( node->doc, node->children, 1 );
      if ( tmp ){
	result = string( (char *)tmp );
	xmlFree( tmp );
      }
    }
    return result;
  }

  string getNS( const xmlNode *node, string& prefix ){
    string result;
    prefix = "";
    xmlNs *p = node->ns;
    if ( p ){
      if ( p->prefix ){
	prefix = (char *)p->prefix;
      }
      result = (char *)p->href;
    }
    return result;
  }

  map<string,string> getNSlist( const xmlNode *node ){
    map<string,string> result;
    xmlNs *p = node->ns;
    while ( p ){
      string pre;
      string val;
      if ( p->prefix ){
	pre = (char *)p->prefix;
      }
      val = (char *)p->href;
      result[pre] = val;
      p = p->next;
    }
    return result;
  }

  list<xmlNode*> FindLocal( xmlXPathContext* ctxt, 
			    const string& xpath ){
    list<xmlNode*> nodes;
    xmlXPathObject* result = xmlXPathEval((xmlChar*)xpath.c_str(), ctxt);
    if ( result ){
      if (result->type != XPATH_NODESET) {
	xmlXPathFreeObject(result);
	throw runtime_error( "sorry, only nodeset result types supported for now." );
	return nodes;
      }
      xmlNodeSet* nodeset = result->nodesetval;
      if ( nodeset ){
	for (int i = 0; i != nodeset->nodeNr; ++i)
	  nodes.push_back(nodeset->nodeTab[i]);
      }
      else {
	throw( runtime_error( "FindLocal: Missing nodeset" ) );
      }
      xmlXPathFreeObject(result);
    }
    else {
      throw runtime_error( "Invalid Xpath: '" + xpath + "'" );
    }
    return nodes;
  }

  list<xmlNode*> FindNodes( xmlNode* node,
			    const string& xpath ){
    xmlXPathContext* ctxt = xmlXPathNewContext( node->doc );
    ctxt->node = node;
    ctxt->namespaces = xmlGetNsList( node->doc, ctxt->node );
    ctxt->nsNr = 0;
    if (ctxt->namespaces != 0 ) {
      while (ctxt->namespaces[ctxt->nsNr] != 0 ){
	ctxt->nsNr++;
      }
    }
    list<xmlNode*> nodes = FindLocal( ctxt, xpath );
    if (ctxt->namespaces != NULL)
      xmlFree(ctxt->namespaces);
    xmlXPathFreeContext(ctxt);
    return nodes;
  }

xmlNode *getNode( xmlNode *pnt, const string& tag ){
  while ( pnt ){
    if ( pnt->type == XML_ELEMENT_NODE && Name(pnt) == tag ){
      return pnt;
    }
    else {
      xmlNode *res  = getNode( pnt->children, tag );
      if ( res )
	return res;
    }
    pnt = pnt->next;
  }
}

string getAttribute( const xmlNode *node, const string& att ){
  if ( node ){
    xmlAttr *a = node->properties;
    while ( a ){
      if ( att == (char*)a->name )
	return (char *)a->children->content;
      a = a->next;
    }
  }
  return "";
}

class docCache {
public:
  ~docCache() { clear(); };
  void clear();
  void fill( const list<xmlNode*>& );
  void add( const string& );
  xmlDoc *find( const string& ) const;
  map<string, xmlDoc*> cache;
};

void docCache::clear(){
  map<string,xmlDoc*>::const_iterator it = cache.begin();
  while ( it != cache.end() ){
    xmlFreeDoc( it->second );
    ++it;
  }
}

void docCache::add( const string& f ){
  if ( cache.find( f ) == cache.end() ){
    ifstream is( f.c_str() );
    if ( is ){
      cerr << "open file " << f << endl;
      xmlDoc *xmldoc = xmlReadFile( f.c_str(), 0, XML_PARSE_NOBLANKS );
      if ( xmldoc ){
	cache[f] = xmldoc;
      }
      else {
	cerr << "Unable to read the XML from " << f << endl;
      }
    }
    else {
      cerr << "unable to find file " << f << endl;
    }
  }
  else {
    cerr << "found " << f << " in cache." << endl;
  }
}

void docCache::fill( const list<xmlNode*>& blocks ){
  list<xmlNode*>::const_iterator it = blocks.begin();
  while ( it != blocks.end() ){
    string alt = getAttribute( *it, "alto" );
    if ( !alt.empty() ){
      add( alt );
    }
    ++it;
  }
}

xmlDoc *docCache::find( const string& f ) const {
  map<string,xmlDoc*>::const_iterator it = cache.find( f );
  if ( it == cache.end() )
    return 0;
  else
    return it->second;
}

#include <cstdlib>

void createFile( const string& altoFile, xmlDoc *doc, 
		 const list<xmlNode*>& textblocks ){
  static int cnt=0;
  char fn[256];
  sprintf( fn, "martin-%d", ++cnt );
  ofstream os( fn );
  xmlNode *root = xmlDocGetRootElement( doc );
  list<xmlNode*>::const_iterator it = textblocks.begin();
  while ( it != textblocks.end() ){
    string id = getAttribute( *it, "ID" );
    list<xmlNode*> v = FindNodes( root, 
				  "//*[local-name()='TextBlock' and @ID='"
				  + id + "']" );
    if ( v.size() == 1 ){
      xmlNode *node = v.front();
      string tb_id =  getAttribute( node, "ID" );
      cerr << "found a TextBlock " << tb_id << endl;
      list<xmlNode*> lv = FindNodes( node, "*[local-name()='TextLine']" );
      cerr << "found " << lv.size() << " text lines" << endl;
      list<xmlNode*>::const_iterator it = lv.begin();
      while ( it != lv.end() ){
	os << altoFile << ":" << tb_id << ":" << getAttribute( *it, "ID" ) << ": ";
	xmlNode *pnt = (*it)->children;
	while ( pnt != 0 ){
	  if ( pnt->type == XML_ELEMENT_NODE ){
	    if ( Name(pnt) == "String" ){
	      os << getAttribute( pnt, "CONTENT" );
	    }
	    else if ( Name(pnt) == "SP" ){
	      os << " ";
	    }
	  }
	  pnt = pnt->next;
	}
	os << endl;
	++it;
      }
    }
    else if ( v.size() == 0 ){
      if ( id.find("CB") != string::npos )
	os << altoFile << ":" << id << ":" << "(picture) " << endl;
      else
	cerr << "found nothing, what is this? " << id << endl;
    }
    else {
      cerr << "Confusing! " << endl;
    }
    ++it;
  }
}

void processBlocks( const list<xmlNode*>& blocks, const docCache& cache ){
  list<xmlNode*>::const_iterator it = blocks.begin();
  while ( it != blocks.end() ){
    string alt = getAttribute( *it, "alto" );
    xmlDoc *doc = cache.find( alt );
    if ( doc ){
      cerr << "found doc " << alt << endl;
      list<xmlNode*> texts = FindNodes( *it, "dcx:TextBlock" );
      cerr << "found " << texts.size() << " text blocks " << endl;
      createFile( alt, doc, texts );
    }
    ++it;
  }
}

void solveAlto( const string& file ){
#pragma omp critical
  {
    cout << "resolving " << file << endl;
  }
  xmlDoc *xmldoc = xmlReadFile( file.c_str(), 0, XML_PARSE_NOBLANKS );
  if ( xmldoc ){
    xmlNode *root = xmlDocGetRootElement( xmldoc );
    xmlNode *pnt = root;
    xmlNode *metadata = getNode( root, "metadata" );
    if ( metadata ){
      cerr << "metadata OK" << endl;
      xmlNode *didl = getNode( metadata, "DIDL" );      
      if ( didl ){
	cerr << "didl OK" << endl;
	list<xmlNode*> blocks = FindNodes( didl, "//dcx:blocks" );
	cerr << "found " << blocks.size() << " blocks" << endl;
	docCache cache;
	cache.fill( blocks );
	processBlocks( blocks, cache );
      }
      else {
	cerr << "no didl" << endl;
      }
    }
    else {
      cerr << "no metadata" << endl;
    }
    xmlFreeDoc( xmldoc );
  }
  else {
    cerr << "XML failed: " << file << endl;
  }
}

int main( int argc, char *argv[] ){
  if ( argc < 2	){
    cerr << "Usage: [-t number_of_threads]  dir/filename " << endl;
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
      cerr << "Usage: alto [options] file/dir" << endl;
      cerr << "\t-t\t number_of_threads" << endl;
      cerr << "\t-h\t this messages " << endl;
      cerr << "\t-V\t show version " << endl;
      exit(EXIT_SUCCESS);
      break;
    default: /* '?' */
      cerr << "Usage: alto [-t number_of_threads]  dir/filename " << endl;
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
    solveAlto( fileNames[fn] );    
  }
  cout << "done" << endl;
  exit(EXIT_SUCCESS);
}
