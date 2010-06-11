/**
 *  Abstraction around embedded HTTP server to serve up files over local
 *  URLs for webpage access
 *
 *  (c) 2008 Yahoo! inc.
 */

#include <fstream>

#include "FileServer.h"
#include "service.h"
#include "littleuuid.h"
#include "util/fileutil.hh"
#include "util/bpsync.hh"

#include <mongoose/mongoose.h>

#define FS_MAX_TEMP_FILES 1024
#define FS_MAX_TEMP_BYTES 1024 * 1024 * 512


FileServer::FileServer(const bp::file::Path& tempDir) 
    : m_tempDir(tempDir),
      m_limit(FS_MAX_TEMP_FILES, FS_MAX_TEMP_BYTES),
      m_ctx(NULL)
{
    BPCLOG_INFO_STRM( "ctor, tempDir = " << m_tempDir );
}

FileServer::~FileServer()
{
    if (m_ctx) {
        mg_stop((struct mg_context *) m_ctx);
        mg_destroy((struct mg_context *) m_ctx);        
        m_ctx = NULL;
    }
    BPCLOG_INFO( "Stopping m_httpServer." );
    bp::file::remove(m_tempDir);
}

std::string
FileServer::start()
{
    std::stringstream boundTo;
    m_port = 0;
    struct mg_context * ctx = mg_create();
    int nRet = mg_set_option( ctx, "ports", "0" );
    if (nRet <= 0 || !mg_start(ctx)) {
        mg_destroy(ctx);
        return std::string();
    }
    m_port = (unsigned short) nRet;
    boundTo << "127.0.0.1:" << m_port;
    BPCLOG_INFO_STRM( "Bound to: " << boundTo.str() );
    m_ctx = ctx;

    // now set up the callback function
    mg_set_uri_callback(ctx, "*", (mg_callback_t) mongooseCallback, (void *) this);
    
    return boundTo.str();
}


std::string
FileServer::addFile(const bp::file::Path& path)
{
    // generate a nice random url path
    std::stringstream url;    
    std::string uuid;

    uuid_generate(uuid);
    url << "http://127.0.0.1:" << m_port << "/" << uuid;
    {
        bp::sync::Lock lck(m_lock);
        m_paths[uuid] = path;
    }

    BPCLOG_DEBUG_STRM( "m_urls[" << uuid << "] = " << path.utf8() );
    return url.str();
}

std::vector<ChunkInfo>
FileServer::getFileChunks(const bp::file::Path& path,
                          size_t chunkSize)
{
    if (m_tempDir.empty()) {
        throw std::string("no temp dir set, internal error");        
    }

    try {
        boost::filesystem::create_directories(m_tempDir);
    } catch (const bp::file::tFileSystemError&) {
        throw std::string("unable to create temp dir");
    }

    std::vector<ChunkInfo> rval;

    std::ifstream fstream;
    if (!bp::file::openReadableStream(fstream, path, 
                                      std::ios_base::in | std::ios_base::binary)) {
        throw std::string("cannot open file for reading");
    }

    // get filesize
    fstream.seekg(0, std::ios::end);
    size_t size = fstream.tellg();
    fstream.seekg(0, std::ios::beg);
    BPCLOG_DEBUG_STRM("file size = " << size);

    if (size <= 0) throw("chunk size is invalid");

    // check resource usage
    if (m_limit.wouldExceed(size / chunkSize, size)) {
        throw std::string("allowed resources exceeded");
    }


    // if file fits in a single chunk, just return the file
    if (size <= chunkSize) {
        ChunkInfo i = {path, 1, 1};
        rval.push_back(i);
        return rval;
    }

    char* buf = new char[chunkSize];
    if (!buf) {
        throw std::string("unable to allocate memory");
    }

    try {
        size_t chunkNumber = 0;
        size_t totalRead = 0;
        while (totalRead < size) {
            // read a chunk
            fstream.read(buf, chunkSize);
            if (fstream.bad()) {
                throw std::string("error reading file");
            }
            size_t numRead = fstream.gcount();
            totalRead += numRead;
            BPCLOG_DEBUG_STRM("read " << numRead << ", totalRead = " << totalRead);

            // write chunk to a file
            std::stringstream ss;
            ss << bp::file::utf8FromNative(path.filename()) << "_chunk-" << chunkNumber << "_";
            bp::file::Path p = bp::file::getTempPath(m_tempDir, ss.str());
            BPCLOG_DEBUG_STRM("chunk file: " << p);
            std::ofstream ofs;
            if (!bp::file::openWritableStream(ofs, p, std::ios_base::out
                                              | std::ios_base::binary)) {
                throw std::string("unable to open temp chunk file");
            }
            ofs.write(buf, numRead);
            if (ofs.bad()) {
                throw std::string("error writing to temp chunk file");
            }
            ofs.close();
            ChunkInfo info;
            info.m_path = p;
            info.m_chunkNumber = chunkNumber++;
            rval.push_back(info);
        }

        for (size_t i = 0; i < rval.size(); i++) {
            rval[i].m_numberOfChunks = chunkNumber;
        }
    } catch (const std::string& e) {
        delete [] buf;
        throw(e);
    }
    delete [] buf;
    
    // update usage:
    m_limit.noteUsage(size / chunkSize, size);

    return rval;
}

#define BUFSIZE (1024 * 8)
bp::file::Path
FileServer::getSlice(const bp::file::Path& path,
                     size_t offset, size_t size)
{
    if (m_tempDir.empty()) {
        throw std::string("no temp dir set, internal error");        
    }

    char buf[BUFSIZE];

    try {
        boost::filesystem::create_directories(m_tempDir);
    } catch (const bp::file::tFileSystemError&) {
        throw std::string("unable to create temp dir");
    }

    bp::file::Path s;

    std::ifstream fstream;
    if (!bp::file::openReadableStream(fstream, path, 
                                      std::ios_base::in | std::ios_base::binary))
    {
        throw std::string("cannot open file for reading");
    }

    // get filesize
    fstream.seekg(0, std::ios::end);
    size_t actual = fstream.tellg();
    fstream.seekg(0, std::ios::beg);

    // if file fits in slice, just return the file
    if (offset == 0 && size >= actual) {
        return path;
    }

    // now check arguments
    if (offset > actual) throw("offset is beyond end of file");
    if (size == (size_t) -1) size = actual - offset;
    if (size > (actual - offset)) size = actual - offset;

    // check resource usage
    if (m_limit.wouldExceed(1, size)) {
        throw std::string("allowed resources exceeded");
    }

    // create new output file
    std::ofstream ofs;
    s = bp::file::getTempPath(m_tempDir, 
		                      bp::file::utf8FromNative(path.filename()));
    if (!bp::file::openWritableStream(ofs, s, std::ios_base::out
                                      | std::ios_base::binary)) {
        throw std::string("unable to create new file");
    }

    // seek to proper point in input stream
    fstream.seekg(offset, std::ios::beg);

    // update resources used
    m_limit.noteUsage(1, size);
    
    while (size > 0) {
        size_t amt = (size < BUFSIZE) ? size : BUFSIZE;
        // read a chunk
        fstream.read(buf, amt);
        if (fstream.bad()) {
            throw std::string("error reading file");
        }
        size_t numRead = fstream.gcount();
        ofs.write(buf, numRead);
        if (ofs.bad()) throw std::string("error writing to new file");
        
        size -= numRead;
    }
    
    return s;
}

void
FileServer::mongooseCallback(void * connPtr, void * requestPtr,
                             void * user_data)
{
    struct mg_connection * conn = (struct mg_connection *) connPtr;
    const struct mg_request_info * request = (const struct mg_request_info * ) requestPtr;
    FileServer * self = (FileServer *) user_data;
    
    std::string id(request->uri);
    BPCLOG_INFO_STRM( "request.url.path = " << id );
    // drop the leading /
    id = id.substr(1, id.length()-1);

    // if we can find a slash '/', in the url, we'll drop everything
    // after it.
    size_t slashLoc = id.find('/');
    if (slashLoc != std::string::npos) {
        id = id.substr(0, slashLoc);
    }

    BPCLOG_INFO_STRM( "token '" << id << "' extracted from request path: "
                      << request->uri );

    bp::file::Path path;
    {
        bp::sync::Lock lck(self->m_lock);
        std::map<std::string,bp::file::Path>::const_iterator it;
        it = self->m_paths.find(id);
        if (it == self->m_paths.end()) {
            BPCLOG_WARN( "Requested id not found." );
            mg_printf(conn, "HTTP/1.0 404 Not Found\r\n\r\n");
            return;
        }
        path = it->second;
    }
    

    // read file and output to connection
    FILE * f = ft::fopen_binary_read(path.utf8());
    if (f == NULL) {
        BPCLOG_WARN_STRM( "Couldn't open file for reading " << path );
        mg_printf(conn, "HTTP/1.0 500 Internal Error\r\n\r\n");
        return;
    }

    long len;
    if (0 != fseek(f, 0, SEEK_END) ||
        (len = ftell(f)) < 0 ||
        0 != fseek(f, 0, SEEK_SET))
    {
        BPCLOG_WARN_STRM( "Couldn't determine file length: " << path );
        mg_printf(conn, "HTTP/1.0 500 Internal Error\r\n\r\n");
        return;
    }

    mg_printf(conn, "HTTP/1.0 200 OK\r\n");
    mg_printf(conn, "Content-Length: %ld\r\n", len);
    mg_printf(conn, "Server: FileAccess BrowserPlus service\r\n");

    // set mime type header
    {
        std::vector<std::string> mts;
        mts = bp::file::mimeTypes(path);
        if (mts.size() > 0) {
            mg_printf(conn, "Content-Type: %s\r\n", mts.begin()->c_str());
        }
    }
    mg_printf(conn, "\r\n");    

    char buf[1024 * 32];
    size_t rd;
    while ((rd = fread(buf, sizeof(char), sizeof(buf), f)) > 0) {
        if (rd != (size_t) mg_write(conn, buf, rd)) {
            BPCLOG_WARN( "partial write detected!  client left?" );
            fclose(f);
            return;
        }
    }

    BPCLOG_DEBUG( "Request processed." );
    return;
}
