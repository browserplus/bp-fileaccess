/**
 *  Abstraction around embedded HTTP server to serve up files over local
 *  URLs for webpage access
 *
 *  (c) 2008 Yahoo! inc.
 */

#ifndef __FILE_SERVER_H__
#define __FILE_SERVER_H__

#include <string>
#include <vector>
#include "BPUtils/BPUtils.h"
#include "ResourceLimit.h"

class ChunkInfo
{
 public:
    bp::file::Path m_path;
    size_t m_chunkNumber;
    size_t m_numberOfChunks;
};

class FileServer : public bp::http::server::IHandler
{
  public:
    FileServer(const bp::file::Path& tempDir);
    ~FileServer();

    /* start the server, returns host/port when bound, otherwise returns
     * .empty() on error */     
    std::string start();

    /* add a file to the server, returning a url, .empty() on error */ 
    std::string addFile(const bp::file::Path& path);

    /* add a chunked file to the server, returning a vector of 
     * ChunkInfo (empty on error)
     */
    std::vector<ChunkInfo> getFileChunks(const bp::file::Path& path,
                                         size_t chunkSize);

    /* get a slice of a file */
    bp::file::Path getSlice(const bp::file::Path& path,
                            size_t offset, size_t size);

  private:
    virtual bool processRequest(const bp::http::Request & request,
                                bp::http::Response & response);

    bp::http::server::Server m_httpServer;
    unsigned short int m_port;
    std::map<std::string, bp::file::Path> m_paths;
    bp::file::Path m_tempDir;
    ResourceLimit m_limit;
};

#endif
