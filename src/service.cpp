#include "bpservice/bpservice.h"
#include "bpservice/bpcallback.h"
#include "FileServer.h"
#include "base64.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

// 2mb is max allowable read
#define FA_MAX_READ (1<<21)

// 2mb is default chunk size
#define FA_CHUNK_SIZE (1<<21)
class FileAccess : public bplus::service::Service {
public:
BP_SERVICE(FileAccess)
    FileAccess();
    ~FileAccess();
    virtual void finalConstruct();
    void read(const bplus::service::Transaction& tran, const bplus::Map& args);
    void readBase64(const bplus::service::Transaction& tran, const bplus::Map& args);
    void slice(const bplus::service::Transaction& tran, const bplus::Map& args);
    void getURL(const bplus::service::Transaction& tran, const bplus::Map& args);
    void chunk(const bplus::service::Transaction& tran, const bplus::Map& args);
private:
    void readImpl(const bplus::service::Transaction& tran, const bplus::Map& args, bool base64);
    bool hasEmbeddedNulls(unsigned char* bytes, unsigned int len);
    bplus::String* readFileContents(const boost::filesystem::path& path, unsigned int offset, int size, bool base64, std::string& err);
private:
    FileServer* m_fs;
};

BP_SERVICE_DESC(FileAccess, "FileAccess", "2.1.0",
                "Access the contents of files that the user has selected.")
ADD_BP_METHOD(FileAccess, read,
              "Read the contents of a file on disk returning a string.  "
              "If the file contains binary data an error will be returned.")
ADD_BP_METHOD_ARG(read, "file", Path, true,
                  "The input file to operate on.")
ADD_BP_METHOD_ARG(read, "offset", Integer, false,
                  "The beginning byte offset.")
ADD_BP_METHOD_ARG(read, "size", Integer, false,
                  "The amount of data.")
ADD_BP_METHOD(FileAccess, readBase64,
              "Read the contents of a file on disk returning a base64 encoded string.  "
              "Since the return is base64 encoded, it will be 4/3 times the size of the request.")
ADD_BP_METHOD_ARG(readBase64, "file", Path, true,
                  "The input file to operate on.")
ADD_BP_METHOD_ARG(readBase64, "offset", Integer, false,
                  "The beginning byte offset.")
ADD_BP_METHOD_ARG(readBase64, "size", Integer, false,
                  "The amount of data.")
ADD_BP_METHOD(FileAccess, slice,
              "Given a file and an optional offset and size, return a new "
              "file whose contents are a subset of the first.")
ADD_BP_METHOD_ARG(slice, "file", Path, true,
                  "The input file to operate on.")
ADD_BP_METHOD_ARG(slice, "offset", Integer, false,
                  "The beginning byte offset.")
ADD_BP_METHOD_ARG(slice, "size", Integer, false,
                  "The amount of data.")
ADD_BP_METHOD(FileAccess, getURL,
              "Get a localhost url that can be used to attain the full contents"
              " of a file on disk.  The URL will be of the form "
              "http://127.0.0.1:<port>/<uuid> -- The port will be an ephemerally "
              "bound port, the uuid will be a traditional GUID.  When a local "
              "client accesses the URL, the FileAccess service will ignore any "
              "appended pathing (i.e. for http://127.0.0.1:<port>/<uuid>/foo.tar.gz,"
              " '/foo.tar.gz' will be ignored).  This allows client code to supply "
              "a filename when triggering a browser supplied 'save as' dialog.")
ADD_BP_METHOD_ARG(getURL, "file", Path, true,
                  "The file that you would like to read via a localhost url.")
ADD_BP_METHOD(FileAccess, chunk,
              "Get a vector of objects that result from chunking a file. "
              "The return value will be an ordered list of file handles with each "
              "successive file representing a different chunk")
ADD_BP_METHOD_ARG(chunk, "file", Path, true,
                  "The file that you would like to chunk.")
ADD_BP_METHOD_ARG(chunk, "chunkSize", Integer, false,
                  "The desired chunk size, not to exceed 2MB.  Default is 2MB.")
END_BP_SERVICE_DESC

FileAccess::FileAccess() : bplus::service::Service(),
    m_fs(NULL) {
}

FileAccess::~FileAccess() {
    assert(m_fs != NULL);
    if (m_fs != NULL) {
        delete m_fs;
        m_fs = NULL;
    }
}

void
FileAccess::finalConstruct() {
    bplus::service::Service::finalConstruct();
    bplus::tPathString tmpDir = tempDir();
    if (tmpDir.empty()) {
        log(BP_ERROR, "(FileAccess) allocated (NO 'temp_dir' key)");
    }
    boost::filesystem::path tempDir = boost::filesystem::path(tmpDir);
    m_fs = new FileServer(tempDir);
    assert(m_fs != NULL);
    m_fs->start();
}

void
FileAccess::read(const bplus::service::Transaction& tran, const bplus::Map& args) {
    readImpl(tran, args, false);
}

void
FileAccess::readBase64(const bplus::service::Transaction& tran, const bplus::Map& args) {
    readImpl(tran, args, true);
}

void
FileAccess::slice(const bplus::service::Transaction& tran, const bplus::Map& args) {
    // dig out args
    const bplus::Path* bpPath = dynamic_cast<const bplus::Path*>(args.value("file"));
    if (!bpPath) {
        tran.error("bp.fileAccessError", "invalid file path");
    }
    boost::filesystem::path path((bplus::tPathString)*bpPath);
    log(BP_INFO, "slice");
    size_t offset = 0, size = (size_t) -1;
    if (args.has("offset", BPTInteger)) {
        offset = (int)(long long)*(args.get("offset"));            
    }
    if (args.has("size", BPTInteger)) {
        size = (int)(long long)*(args.get("size"));            
    }
    std::string err;
    try {
        boost::filesystem::path s = m_fs->getSlice(path, offset, size);
        tran.complete(bplus::Path(bp::file::nativeString(s)));
    } catch (const std::string& e) {
        tran.error("bp.fileAccessError", e.c_str());
    }
}

void
FileAccess::getURL(const bplus::service::Transaction& tran, const bplus::Map& args) {
    // dig out args
    const bplus::Path* bpPath = dynamic_cast<const bplus::Path*>(args.value("file"));
    if (!bpPath) {
        tran.error("bp.fileAccessError", "invalid file path");
    }
    boost::filesystem::path path((bplus::tPathString)*bpPath);
    log(BP_INFO, "getURL");
    std::string url = m_fs->addFile(path);
    if (url.empty()) {
        tran.error("bp.fileAccessError", NULL);
    } else {
        tran.complete(bplus::String(url));
    }
}

void
FileAccess::chunk(const bplus::service::Transaction& tran, const bplus::Map& args) {
    // dig out args
    const bplus::Path* bpPath = dynamic_cast<const bplus::Path*>(args.value("file"));
    if (!bpPath) {
        tran.error("bp.fileAccessError", "invalid file path");
    }
    boost::filesystem::path path((bplus::tPathString)*bpPath);
    log(BP_INFO, "chunk");
    size_t chunkSize = FA_CHUNK_SIZE;
    if (args.has("chunkSize", BPTInteger)) {
        chunkSize = (size_t)(long long)*(args.get("chunkSize"));            
    }
    std::vector<ChunkInfo> v;
    std::string err;
    try {
        v = m_fs->getFileChunks(path, chunkSize);
    } catch (const std::string& e) {
        err = e;
        v.clear();
    }
    if (v.empty()) {
        tran.error("bp.fileAccessError", err.c_str());
    } else {
        bplus::List* l = new bplus::List;
        for (size_t i = 0; i < v.size(); i++) {
            l->append(new bplus::Path(bp::file::nativeString(v[i].m_path)));
        }
        tran.complete(*l);
    }
}

void
FileAccess::readImpl(const bplus::service::Transaction& tran, const bplus::Map& args, bool base64) {
    // dig out args
    const bplus::Path* bpPath = dynamic_cast<const bplus::Path*>(args.value("file"));
    if (!bpPath) {
        tran.error("bp.fileAccessError", "invalid file path");
    }
    boost::filesystem::path path((bplus::tPathString)*bpPath);
    log(BP_INFO, base64 ? "readBase64" : "read");
    unsigned int offset = 0;
    int size = -1;
    if (args.has("offset", BPTInteger)) {
        offset = (int) (long long) *(args.get("offset"));            
    }
    if (args.has("size", BPTInteger)) {
        size = (int) (long long) *(args.get("size"));            
    }
    bplus::String* contents = NULL;
    std::string err;
    contents = readFileContents(path, offset, size, base64, err);
    if (!err.empty() || contents == NULL) {
        tran.error("bp.fileAccessError", err.c_str());
    } else {
        tran.complete(*contents);
    }
    if (contents) {
        delete contents;
    }
}

bool
FileAccess::hasEmbeddedNulls(unsigned char * bytes, unsigned int len) {
    for (unsigned int i = 0; i < len; i++) {
        if (bytes[i] == 0) {
            return true;
        }
    }
    return false;
}

bplus::String*
FileAccess::readFileContents(const boost::filesystem::path& path, unsigned int offset, int size, bool base64, std::string& err) {
    std::ifstream fstream;
    // verify size is reasonable
    if (size > FA_MAX_READ) {
        err = "size too large, greater than 2mb limit";
        return NULL;
    }
    // set to 2mb if 
    if (size < 0) {
        size = FA_MAX_READ;
    }
    // verify file exists and open
    if (!bp::file::openReadableStream(fstream, path, std::ios_base::in | std::ios_base::binary)) {
        err = "cannot open file for reading";
        return NULL;
    }
    // now validate offset and size
    fstream.seekg(0, std::ios::end);
    if ((size_t)offset > fstream.tellg()) {
        err = "offset out of range";        
        return NULL;
    }
    // now set size to exact amount required
    {
        int avail = ((int)fstream.tellg() - (int)offset);
        if (avail < size) {
            size = avail;
        }
    }
    fstream.seekg(offset, std::ios::beg);
    // allocate memory:1
    unsigned char* buffer = new unsigned char[size];
    bplus::String* s = NULL;
    if (base64) {
        std::stringstream ss;
        Base64 b64;
        b64.encode(fstream, size, ss);
        // encode into a js literal
        s = new bplus::String(ss.str().c_str(), (unsigned int) ss.str().length());
    } else {
        // read data
        fstream.read((char*)buffer, size);
        if (fstream.gcount() > 0) {
            // no support for "binary data" --> embedded nulls
            if (hasEmbeddedNulls(buffer, (unsigned int)fstream.gcount())) {
                delete [] buffer;
                err = "binary data not supported";
                return NULL;
            }
            // encode into a js literal
            s = new bplus::String((char*) buffer, (unsigned int)fstream.gcount());
        } else {
            s = new bplus::String("", 0);
        }
    }
    fstream.close();
    delete []buffer;
    if (fstream.fail()) {
        err = "read error";
        if (s) {
            delete s;
        }
        s = NULL;
    }
    return s;
}
