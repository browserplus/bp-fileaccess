/**
 *  A class to implement resource limiting.
 *
 *  (c) 2010 Yahoo! inc.
 */

#ifndef __RESOURCE_LIMIT_H__
#define __RESOURCE_LIMIT_H__

class ResourceLimit 
{
public:
    ResourceLimit(size_t fileLimit, size_t byteLimit) :
        m_fileLimit(fileLimit), m_byteLimit(byteLimit),
        m_filesUsed(0), m_bytesUsed(0) { };    
    
    bool wouldExceed(size_t files, size_t bytes)
    {
        if (m_filesUsed + files > m_fileLimit) return true;
        if (m_bytesUsed + bytes > m_byteLimit) return true;
        return false;
    }
    

    void noteUsage(size_t files, size_t bytes)
    {
        m_filesUsed += files;
        m_bytesUsed += bytes;
    }
    
private:
    size_t m_fileLimit, m_byteLimit;
    size_t m_filesUsed, m_bytesUsed;
};

#endif
