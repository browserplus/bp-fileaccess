/*
 * Copyright 2009, Yahoo!
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 * 
 *  3. Neither the name of Yahoo! nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <ServiceAPI/bperror.h>
#include <ServiceAPI/bptypes.h>
#include <ServiceAPI/bpdefinition.h>
#include <ServiceAPI/bpcfunctions.h>
#include <ServiceAPI/bppfunctions.h>

#include "bptypeutil.h"
#include "FileServer.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <fstream>


// 2mb is max allowable read
#define FA_MAX_READ (1<<21)

const BPCFunctionTable * g_bpCoreFunctions = NULL;

static int
BPPAllocate(void ** instance, unsigned int, const BPElement *)
{
    FileServer * fs = new FileServer;
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
readFileContents(const std::string & path,
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
    // XXX: abstract this guy
/*
    if (!bp::file::openReadableStream(
            fstream, path, std::ios_base::in | std::ios_base::binary))
    {
        err = "cannot open file for reading";
        return NULL;
    }
*/
    // now validate offset and size
    fstream.seekg (0, std::ios::end);
    if (offset > fstream.tellg()) {
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
/*
  XXX: reimplement/abstract me
    bp::url::Url pathUrl;
    if (!pathUrl.parse((std::string) (*(args->get("file")))))
    {
        g_bpCoreFunctions->postError(tid, "bp.fileAccessError",
                                     "invalid file URI");
        if (args) delete args;
        return;
    }

    std::string path = bp::url::pathFromURL(pathUrl.toString());
*/    
    std::string path;
    
    if (!strcmp(funcName, "Read"))
    {
        unsigned int offset = 0;
        int size = -1;
        if (args->has("offset", BPTInteger)) {
            offset = (int) *(args->get("offset"));            
        }
        if (args->has("size", BPTInteger)) {
            size = *(args->get("size"));            
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
    else if (!strcmp(funcName, "GetURL"))
    {
        FileServer * fs = (FileServer *) instance;

        std::string url = fs->addFile(path);

        if (url.empty()) {
            g_bpCoreFunctions->postError(tid, "bp.fileAccessError", NULL);
        } else {
            g_bpCoreFunctions->postResults(tid, bp::String(url).elemPtr());
        }
    }

    // platform ensures that one of the above functions is invoked
    if (args) delete args;

    return;
}

/* beneath is the definition of the service interface */
BPArgumentDefinition s_readFuncArgs[] = {
    {
        "file",
        "The file that you would like to read from.",
        BPTPath,
        BP_TRUE
    },
    {
        "offset",
        "The option byte offset at which you like to start reading.",
        BPTInteger,
        BP_FALSE
    },
    {
        "size",
        "The amount of data to read in bytes",
        BPTInteger,
        BP_FALSE
    }
};

BPArgumentDefinition s_getURLArgs[] = {
    {
        "file",
        "The file that you would like to read via a localhost url.",
        BPTPath,
        BP_TRUE
    }
};

BPArgumentDefinition s_getChunkArgs[] = {
    {
        "file",
        "The file that you would like to get a chunk out of.",
        BPTPath,
        BP_TRUE
    },
    {
        "offset",
        "The optional byte offset at which you like to start reading.",
        BPTInteger,
        BP_FALSE
    },
    {
        "size",
        "The amount of data to read in bytes",
        BPTInteger,
        BP_FALSE
    }
};


BPFunctionDefinition s_functions[] = {
    {
        "Read",
        "Read up to 2mb of a file on disk.  You may specify offset and "
        "length.",
        sizeof(s_readFuncArgs)/sizeof(s_readFuncArgs[0]),
        s_readFuncArgs
    },
    {
        "GetURL",
        "Get a localhost url that can be used to attain the full contents"
        " of a file on disk.",
        sizeof(s_getURLArgs)/sizeof(s_getURLArgs[0]),
        s_getURLArgs
    },
    {
        "GetChunk",
        "Attain a chunk (or byte range) of the specified file.",
        sizeof(s_getChunkArgs)/sizeof(s_getChunkArgs[0]),
        s_getChunkArgs
    }

};

// a description of this corelet.
BPCoreletDefinition s_coreletDef = {
    "FileAccess",
    1, 1, 0,
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
