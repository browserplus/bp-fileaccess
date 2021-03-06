/**
 *  Abstraction around embedded HTTP server to serve up files over local
 *  URLs for webpage access
 *
 *  (c) 2008 Yahoo! inc.
 */

#include "FileServer.h"
#include "bpservice/bpservice.h"
#include "littleuuid.h"
#include <mongoose/mongoose.h>
#include <sstream>
#include <assert.h>

#define FS_MAX_TEMP_FILES 1024
#define FS_MAX_TEMP_BYTES 1024 * 1024 * 512
#define BUFSIZE 1024 * 8

FileServer* FileServer::s_self = NULL;

FileServer::FileServer(const boost::filesystem::path& tempDir) :
    m_tempDir(tempDir),
    m_limit(FS_MAX_TEMP_FILES, FS_MAX_TEMP_BYTES),
    m_ctx(NULL) {
    assert(FileServer::s_self == NULL);
    FileServer::s_self = NULL;
    bplus::service::Service::log(BP_INFO, "ctor, tempDir = " + m_tempDir.string());
}

FileServer::~FileServer() {
    if (m_ctx) {
        mg_stop(m_ctx);
        mg_destroy(m_ctx);        
        m_ctx = NULL;
        assert(FileServer::s_self != NULL);
        FileServer::s_self = NULL;
    }
    bplus::service::Service::log(BP_INFO, "Stopping m_httpServer.");
    bp::file::safeRemove(m_tempDir);
}

std::string
FileServer::start() {
    std::stringstream boundTo;
    m_port = 0;
    const char* options[] = {
      "listening_ports", "0",
      NULL
    };
    m_ctx = mg_create(&FileServer::mongooseCallback, NULL, options);
    m_port = (unsigned short)atoi(mg_get_option( m_ctx, "listening_ports" ));
    if (m_port <= 0 || !mg_start(m_ctx)) {
        mg_destroy(m_ctx);
        m_ctx = NULL;
        m_port = 0;
        return std::string();
    }
    boundTo << "127.0.0.1:" << m_port;
    bplus::service::Service::log(BP_INFO, "Bound to: " + boundTo.str());
    assert(FileServer::s_self == NULL);
    FileServer::s_self = this;
    return boundTo.str();
}

std::string
FileServer::addFile(const boost::filesystem::path& path) {
    // generate a nice random url path
    std::stringstream url;    
    std::string uuid;
    uuid_generate(uuid);
    url << "http://127.0.0.1:" << m_port << "/" << uuid;
    {
        bplus::sync::Lock lck(m_lock);
        m_paths[uuid] = path;
    }
    bplus::service::Service::log(BP_DEBUG, "m_urls[" + uuid + "] = " + path.string());
    return url.str();
}

std::vector<ChunkInfo>
FileServer::getFileChunks(const boost::filesystem::path& path, size_t chunkSize) {
    if (m_tempDir.empty()) {
        throw std::string("no temp dir set, internal error");        
    }
    try {
        boost::filesystem::create_directories(m_tempDir);
    } catch (const boost::filesystem::filesystem_error&) {
        throw std::string("unable to create temp dir");
    }
    std::vector<ChunkInfo> rval;
    std::ifstream fstream;
    if (!bp::file::openReadableStream(fstream, path, std::ios_base::in | std::ios_base::binary)) {
        throw std::string("cannot open file for reading");
    }
    // get filesize
    fstream.seekg(0, std::ios::end);
    size_t size = (size_t) fstream.tellg();
    fstream.seekg(0, std::ios::beg);
    bplus::service::Service::log(BP_DEBUG, "file size = " + size);
    if (size <= 0) {
        throw std::string("chunk size is invalid");
    }
    // check resource usage
    if (m_limit.wouldExceed(size / chunkSize, size)) {
        throw std::string("allowed resources exceeded");
    }
    // if file fits in a single chunk, just return the file
    if (size <= chunkSize) {
        ChunkInfo i = { path, 1, 1 };
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
            size_t numRead = (size_t)fstream.gcount();
            totalRead += numRead;
            std::stringstream ss1;
            ss1 << "read " << numRead << ", totalRead = " << totalRead;
            bplus::service::Service::log(BP_DEBUG, ss1.str());
            // write chunk to a file
            std::stringstream ss2;
            ss2 << path.filename().string() << "_chunk-" << chunkNumber << "_";
            boost::filesystem::path p = bp::file::getTempPath(m_tempDir, ss2.str());
            bplus::service::Service::log(BP_DEBUG, "chunk file: " + p.string());
            std::ofstream ofs;
            if (!bp::file::openWritableStream(ofs, p, std::ios_base::out | std::ios_base::binary)) {
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

boost::filesystem::path
FileServer::getSlice(const boost::filesystem::path& path,
                     size_t offset, size_t size)
{
    if (m_tempDir.empty()) {
        throw std::string("no temp dir set, internal error");        
    }

    char buf[BUFSIZE];

    try {
        boost::filesystem::create_directories(m_tempDir);
    } catch (const boost::filesystem::filesystem_error&) {
        throw std::string("unable to create temp dir");
    }

    boost::filesystem::path s;

    std::ifstream fstream;
    if (!bp::file::openReadableStream(fstream, path, 
                                      std::ios_base::in | std::ios_base::binary))
    {
        throw std::string("cannot open file for reading");
    }

    // get filesize
    fstream.seekg(0, std::ios::end);
    size_t actual = (size_t) fstream.tellg();
    fstream.seekg(0, std::ios::beg);

    // if file fits in slice, just return the file
    if (offset == 0 && size >= actual) {
        return path;
    }

    // now check arguments
    if (offset > actual) throw std::string("offset is beyond end of file");
    if (size == (size_t) -1) size = actual - offset;
    if (size > (actual - offset)) size = actual - offset;

    // check resource usage
    if (m_limit.wouldExceed(1, size)) {
        throw std::string("allowed resources exceeded");
    }

    // create new output file
    std::ofstream ofs;
    s = bp::file::getTempPath(m_tempDir, path.filename().string());
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
        size_t numRead = (size_t) fstream.gcount();
        ofs.write(buf, numRead);
        if (ofs.bad()) throw std::string("error writing to new file");
        
        size -= numRead;
    }
    
    return s;
}

void*
FileServer::mongooseCallback(enum mg_event event, struct mg_connection *conn, const struct mg_request_info *request_info) {
    if (event != MG_NEW_REQUEST) {
        return NULL;
    }
    assert(FileServer::s_self != NULL);
    if (FileServer::s_self == NULL) {
        return NULL;
    }
    std::string id(request_info->uri);
    bplus::service::Service::log(BP_INFO, "request.url.path = " + id);
    // drop the leading /
    id = id.substr(1, id.length() - 1);
    // if we can find a slash '/', in the url, we'll drop everything after it.
    size_t slashLoc = id.find('/');
    if (slashLoc != std::string::npos) {
        id = id.substr(0, slashLoc);
    }
    bplus::service::Service::log(BP_INFO, "token '" + id + "' extracted from request path: " + request_info->uri);
    boost::filesystem::path path;
    {
        bplus::sync::Lock lck(FileServer::s_self->m_lock);
        std::map<std::string,boost::filesystem::path>::const_iterator it;
        it = FileServer::s_self->m_paths.find(id);
        if (it == FileServer::s_self->m_paths.end()) {
            bplus::service::Service::log(BP_WARN, "Requested id not found.");
            mg_printf(conn, "HTTP/1.0 404 Not Found\r\n\r\n");
            return conn;
        }
        path = it->second;
    }
    // read file and output to connection
    std::ifstream ifs;
    if (!bp::file::openReadableStream(ifs, path.string(), std::ios::binary)) {
        bplus::service::Service::log(BP_WARN, "Couldn't open file for reading " + path.string());
        mg_printf(conn, "HTTP/1.0 500 Internal Error\r\n\r\n");
        return conn;
    }
    // get length of file:
    long len = 0;
    try {
        ifs.seekg(0, std::ios::end);
        len = ifs.tellg();
        ifs.seekg(0, std::ios::beg);
    } catch (...) {
        len = -1;
    }
    if (len < 0) {
        bplus::service::Service::log(BP_WARN, "Couldn't determine file length: " + path.string());
        mg_printf(conn, "HTTP/1.0 500 Internal Error\r\n\r\n");
        return conn;
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
    while (!ifs.eof()) {
        size_t rd = 0;
        ifs.read(buf, sizeof(buf));
        rd = ifs.gcount();
        if (rd != (size_t) mg_write(conn, buf, rd)) {
            bplus::service::Service::log(BP_WARN, "partial write detected!  client left?");
            ifs.close();
            return conn;
        }
    }
    bplus::service::Service::log(BP_DEBUG, "Request processed.");
    return conn;
}
