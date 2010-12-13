/**
 *  Abstraction around embedded HTTP server to serve up files over local
 *  URLs for webpage access
 *
 *  (c) 2008 Yahoo! inc.
 */

#ifndef __FILE_SERVER_H__
#define __FILE_SERVER_H__

#include "bp-file/bpfile.h"
#include "util/bpsync.hh"

#include "ResourceLimit.h"

#include <string>
#include <vector>
#include <map>

class ChunkInfo
{
 public:
    boost::filesystem::path m_path;
    size_t m_chunkNumber;
    size_t m_numberOfChunks;
};

class FileServer
{
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
    std::vector<ChunkInfo> getFileChunks(const boost::filesystem::path& path,
                                         size_t chunkSize);

    /* get a slice of a file */
    boost::filesystem::path getSlice(const boost::filesystem::path& path,
                                     size_t offset, size_t size);

  private:

    static void mongooseCallback(void * connPtr, void * requestPtr,
                                 void * user_data);

    unsigned short int m_port;
    std::map<std::string, boost::filesystem::path> m_paths;
    boost::filesystem::path m_tempDir;
    ResourceLimit m_limit;
    void * m_ctx;
    bp::sync::Mutex m_lock;
};

#endif
