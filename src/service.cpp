#include <ServiceAPI/bperror.h>
#include <ServiceAPI/bptypes.h>
#include <ServiceAPI/bpdefinition.h>
#include <ServiceAPI/bpcfunctions.h>
#include <ServiceAPI/bppfunctions.h>

#include "FileServer.h"
#include "service.h"
#include "bptypeutil.hh"
#include "bpurlutil.hh"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

// 2mb is max allowable read
#define FA_MAX_READ (1<<21)

// 2mb is default chunk size
#define FA_CHUNK_SIZE (1<<21)

const BPCFunctionTable * g_bpCoreFunctions = NULL;

static int
BPPAllocate(void ** instance, unsigned int, const BPElement * elem)
{
    bp::file::Path tempDir;
    bp::Object * args = bp::Object::build(elem);
    if (args->has("temp_dir", BPTString)) {
        tempDir = (std::string)(*(args->get("temp_dir")));
    } else {
        g_bpCoreFunctions->log(BP_ERROR,
                               "(FileAccess) allocated (NO 'temp_dir' key)");
    }

    FileServer * fs = new FileServer(tempDir);
    fs->start();
    *instance = (void *) fs;
    assert(*instance != NULL);
    return 0;
}

static void
BPPDestroy(void * instance)
{
    assert(instance != NULL);
    delete (FileServer *) instance;
}

static void
BPPShutdown(void)
{
}

static bool
hasEmbeddedNulls(unsigned char * bytes, unsigned int len)
{
    for (unsigned int i = 0; i < len; i++) if (bytes[i] == 0) return true;
    return false;
}

static bp::String *
readFileContents(const bp::file::Path & path,
                 unsigned int offset, int size, std::string & err)
{
    std::ifstream fstream;

    // verify size is reasonable
    if (size > FA_MAX_READ) {
        err = "size too large, greater than 2mb limit";
        return NULL;
    }

    // set to 2mb if 
    if (size <= 0) size = FA_MAX_READ;

    // verify file exists and open
    if (!bp::file::openReadableStream(
            fstream, path, std::ios_base::in | std::ios_base::binary))
    {
        err = "cannot open file for reading";
        return NULL;
    }

    // now validate offset and size
    fstream.seekg (0, std::ios::end);
    if ((size_t) offset > fstream.tellg()) {
        err = "offset out of range";        
        return NULL;
    }
    // now set size to exact amount required
    {
        int avail = ((int)fstream.tellg() - (int)offset);
        if (avail < size) size = avail;
    }
    
    fstream.seekg (offset, std::ios::beg);

    // allocate memory:
    unsigned char * buffer = new unsigned char [size];
    bp::String * s = NULL;

    // read data
    fstream.read ((char *) buffer, size);
    if (fstream.gcount() > 0) {
        // no support for "binary data" --> embedded nulls
        if (hasEmbeddedNulls(buffer, fstream.gcount())) {
            delete [] buffer;            
            err = "binary data not supported";
            return NULL; 
        }
        // encode into a js literal
        s = new bp::String((char *) buffer, fstream.gcount());
    } else {
        s = new bp::String("", 0);
    }
    
    fstream.close();
    
    delete [] buffer;

    if (fstream.fail()) {
        err = "read error";
        if (s) delete s;
        s = NULL;
    }
    
    return s;
}

static void
BPPInvoke(void * instance, const char * funcName,
          unsigned int tid, const BPElement * elem)
{
    bp::Object * args = NULL;
    if (elem) args = bp::Object::build(elem);

    // all functions have a "file" argument, validate we can parse it
    std::string url = (*(args->get("file")));
    std::string pathString = bp::urlutil::pathFromURL(url);

    if (pathString.empty())
    {
        g_bpCoreFunctions->postError(tid, "bp.fileAccessError",
                                     "invalid file URI");
        if (args) delete args;
        return;
    }

    bp::file::Path path(pathString);
    
    if (!strcmp(funcName, "read"))
    {
        BPCLOG_INFO( "read" );
        
        unsigned int offset = 0;
        int size = -1;
        if (args->has("offset", BPTInteger)) {
            offset = (int) (long long) *(args->get("offset"));            
        }
        if (args->has("size", BPTInteger)) {
            size = (int) (long long) *(args->get("size"));            
        }

        bp::String * contents = NULL;
        std::string err;
        contents = readFileContents(path, offset, size, err);

        if (!err.empty()) {
            g_bpCoreFunctions->postError(tid, "bp.fileAccessError",
                                         err.c_str());
        } else {
            g_bpCoreFunctions->postResults(tid, contents->elemPtr());
        }
        if (contents) delete contents;
    }
    else if (!strcmp(funcName, "getURL"))
    {
        BPCLOG_INFO( "getURL" );
        
        FileServer * fs = (FileServer *) instance;

        std::string url = fs->addFile(path);

        if (url.empty()) {
            g_bpCoreFunctions->postError(tid, "bp.fileAccessError", NULL);
        } else {
            g_bpCoreFunctions->postResults(tid, bp::String(url).elemPtr());
        }
    }
    else if (!strcmp(funcName, "chunk"))
    {
        BPCLOG_INFO( "chunk" );
        
        size_t chunkSize = FA_CHUNK_SIZE;
        if (args->has("chunkSize", BPTInteger)) {
            chunkSize = (size_t) (long long) *(args->get("chunkSize"));            
        }

        FileServer * fs = (FileServer *) instance;

        std::vector<ChunkInfo> v;
        std::string err;
        try {
            v = fs->getFileChunks(path, chunkSize);
        } catch (const std::string& e) {
            err = e;
            v.clear();
        }

        if (v.empty()) {
            g_bpCoreFunctions->postError(tid, "bp.fileAccessError", err.c_str());
        } else {
            bp::List* l = new bp::List;
            for (size_t i = 0; i < v.size(); i++) {
                l->append(new bp::Path(v[i].m_path.utf8()));
            }
            g_bpCoreFunctions->postResults(tid, l->elemPtr());
        }
    }
    else if (!strcmp(funcName, "slice"))
    {
        BPCLOG_INFO( "slice" );
        size_t offset = 0, size = (size_t) -1;

        if (args->has("offset", BPTInteger)) {
            offset = (int) (long long) *(args->get("offset"));            
        }
        if (args->has("size", BPTInteger)) {
            size = (int) (long long) *(args->get("size"));            
        }

        FileServer * fs = (FileServer *) instance;

        std::string err;
        try {
            bp::file::Path s;
            s = fs->getSlice(path, offset, size);
            g_bpCoreFunctions->postResults(tid, bp::Path(s.utf8()).elemPtr());
        } catch (const std::string& e) {
            g_bpCoreFunctions->postError(tid, "bp.fileAccessError", e.c_str());
        }
    }

    // platform ensures that one of the above functions is invoked
    if (args) delete args;

    return;
}

/* here is the definition of the corelet interface */
BPArgumentDefinition s_readFileArgs[] = {
    {
        "file",
        "The input file to operate on.",
        BPTPath,
        BP_TRUE
    },
    {
        "offset",
        "The beginning byte offset.",
        BPTInteger,
        BP_FALSE
    },
    {
        "size",
        "The amount of data.",
        BPTInteger,
        BP_FALSE
    }
};

/* here is the definition of the corelet interface */
BPArgumentDefinition s_getURLArgs[] = {
    {
        "file",
        "The file that you would like to read via a localhost url.",
        BPTPath,
        BP_TRUE
    }
};

BPArgumentDefinition s_getFileChunksArgs[] = {
    {
        "file",
        "The file that you would like to chunk.",
        BPTPath,
        BP_TRUE
    },
    {  
        "chunkSize",
        "The desired chunk size, not to exceed 2MB.  Default is 2MB.",
        BPTInteger,
        BP_FALSE
    }
};

BPFunctionDefinition s_functions[] = {
    {
        "read",
        "Read the contents of a file on disk returning a string.  "
        "If the file contains binary data an error will be returned.",
        sizeof(s_readFileArgs)/sizeof(s_readFileArgs[0]),
        s_readFileArgs
    },
    {
        "slice",
        "Given a file and an optional offset and size, return a new "
        "file whose contents are a subset of the first.",
        sizeof(s_readFileArgs)/sizeof(s_readFileArgs[0]),
        s_readFileArgs
    },
    {
        "getURL",
        "Get a localhost url that can be used to attain the full contents"
        " of a file on disk.  The URL will be of the form "
        "http://127.0.0.1:<port>/<uuid> -- The port will be an ephemerally "
        "bound port, the uuid will be a traditional GUID.  When a local "
        "client accesses the URL, the FileAccess service will ignore any "
        "appended pathing (i.e. for http://127.0.0.1:<port>/<uuid>/foo.tar.gz,"
        " '/foo.tar.gz' will be ignored).  This allows client code to supply "
        "a filename when triggering a browser supplied 'save as' dialog.",
        sizeof(s_getURLArgs)/sizeof(s_getURLArgs[0]),
        s_getURLArgs
    },
    {
        "chunk",
        "Get a vector of objects that result from chunking a file. "
        "The return value will be an ordered list of file handles with each "
        "successive file representing a different chunk",
        sizeof(s_getFileChunksArgs)/sizeof(s_getFileChunksArgs[0]),
        s_getFileChunksArgs
    }
};

// a description of this corelet.
BPCoreletDefinition s_coreletDef = {
    "FileAccess",
    2, 0, 3,
    "Access the contents of files that the user has selected.",
    sizeof(s_functions)/sizeof(s_functions[0]),
    s_functions
};

const BPCoreletDefinition *
BPPInitialize(const BPCFunctionTable * bpCoreFunctions,
              const BPElement * parameterMap)
{
    g_bpCoreFunctions = bpCoreFunctions;
    return &s_coreletDef;
}

/** and finally, declare the entry point to the corelet */
BPPFunctionTable funcTable = {
    BPP_CORELET_API_VERSION,
    BPPInitialize,
    BPPShutdown,
    BPPAllocate,
    BPPDestroy,
    BPPInvoke,
    NULL,
    NULL
};

const BPPFunctionTable *
BPPGetEntryPoints(void)
{
    return &funcTable;
}
