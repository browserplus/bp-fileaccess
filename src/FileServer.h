/**
 *  Abstraction around embedded HTTP server to serve up files over local
 *  URLs for webpage access
 *
 *  (c) 2008 Yahoo! inc.
 */

#ifndef __FILESERVER_H__
#define __FILESERVER_H__

#include "bp-file/bpfile.h"
#include "bputil/bpsync.h"
#include "ResourceLimit.h"
#include <mongoose/mongoose.h>
#include <string>
#include <vector>
#include <map>

class ChunkInfo {
public:
    boost::filesystem::path m_path;
    size_t m_chunkNumber;
    size_t m_numberOfChunks;
};

class FileServer {
public:
    FileServer(const boost::filesystem::path& tempDir);
    ~FileServer();
    /* start the server, returns host/port when bound, otherwise returns
     * .empty() on error */     
    std::string start();
    /* add a file to the server, returning a url, .empty() on error */ 
    std::string addFile(const boost::filesystem::path& path);
    /* add a chunked file to the server, returning a vector of 
     * ChunkInfo (empty on error)
     */
    std::vector<ChunkInfo> getFileChunks(const boost::filesystem::path& path, size_t chunkSize);
    /* get a slice of a file */
    boost::filesystem::path getSlice(const boost::filesystem::path& path, size_t offset, size_t size);
private:
    static void* mongooseCallback(enum mg_event event, struct mg_connection *conn, const struct mg_request_info *request_info);
private:
    unsigned short int m_port;
    std::map<std::string, boost::filesystem::path> m_paths;
    boost::filesystem::path m_tempDir;
    ResourceLimit m_limit;
    struct mg_context* m_ctx;
    bplus::sync::Mutex m_lock;
    static FileServer* s_self;
};

#endif
