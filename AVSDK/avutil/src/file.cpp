#include "../include/common.h"
#include <stdio.h>
#include <sys/stat.h>

using namespace MediaCloud::Common;

SimpleFile* SimpleFile::OpenForWrite(const char *filename)
{
    SimpleFile *sf = nullptr;
    if (filename != nullptr)
    {
        FILE *fd = fopen(filename, "wb");
        if (fd != nullptr)
        {
            sf = new SimpleFile();
            sf->_file = fd;
        }
    }
    return sf;
}

SimpleFile* SimpleFile::OpenForRead(const char *filename)
{
    SimpleFile *sf = nullptr;
    if (filename != nullptr)
    {
        FILE *fd = fopen(filename, "rb");
        if (fd != nullptr)
        {
            sf = new SimpleFile();
            sf->_file = fd;
        }
    }
    return sf;
}

void SimpleFile::Close(SimpleFile *file)
{
    if (file != nullptr)
    {
        fclose(file->_file);
        delete file;
    }
}

void SimpleFile::Flush()
{
    fflush(_file);
}

unsigned int SimpleFile::GetSize() const
{
    size_t cur = ftell(_file);
    fseek(_file, 0, SEEK_END);
    unsigned int size = (unsigned int)ftell(_file);
    fseek(_file, cur, SEEK_SET);
    return size;
}

unsigned int SimpleFile::GetOffset() const
{
    return (unsigned int)ftell(_file);
}

int SimpleFile::SetOffset(unsigned int offset)
{
    return fseek(_file, offset, SEEK_SET);
}

int SimpleFile::Write(void *buffer, int length)
{
    if (buffer == nullptr || length <= 0)
        return 0;
    return (int)fwrite(buffer, 1, length, _file);
}

int SimpleFile::Read(void *buffer, int length)
{
    if (buffer == nullptr || length <= 0)
        return 0;
    return (int)fread(buffer, 1, length, _file);
}

int SimpleFile::Rename(const char *srcFilename, const char *dstFilename)
{
    if (srcFilename != nullptr && dstFilename != nullptr)
        return rename(srcFilename, dstFilename);
    return ErrCodeArgument;
}

int SimpleFile::Delete(const char *filename)
{
    if (filename != nullptr)
        return remove(filename);
    return ErrCodeArgument;
}

SimpleFile::Stat SimpleFile::GetStat(const char *filename)
{
    Stat sst;
    sst.size = 0;
    sst.createTime = 0;
    sst.lastModifiedTime = 0;

    if (filename == nullptr)
        return sst;
    
    struct stat st;
    int ret = stat(filename, &st);
    if (ret == 0)
    {
        sst.size = (unsigned int)st.st_size;
        sst.createTime = st.st_ctime;
        sst.lastModifiedTime = st.st_mtime;
    }
    return sst;
}

const char* SimpleFile::GetFilename(const char *path)
{
    if (path != nullptr)
    {
        int len = (int)strlen(path);
        const char *ptr = path + len;
        while (--ptr >= path)
        {
            if (*ptr == '/' || *ptr == '\\')
                return ptr + 1;
        }
        return path;
    }
    return nullptr;
}

const char* SimpleFile::GetExtensionName(const char *path)
{
    if (path != nullptr)
    {
        int len = (int)strlen(path);
        const char *ptr = path + len;
        while (--ptr >= path)
        {
            if (*ptr == '/')
                break;
            if (*ptr == '.')
                return ptr;
        }
        return path + len;
    }
    return nullptr;
}

#ifdef WIN32
int SimpleFile::EnumFilesInFolder(const char *foldername, void(*filecb)(const char *filename))
{
    if (foldername == nullptr)
        return ErrCodeArgument;

    std::string pattern = foldername;
    pattern += "/*";

    WIN32_FIND_DATA findData;
    HANDLE findhandle = ::FindFirstFile(pattern.c_str(), &findData);
    if (findhandle == INVALID_HANDLE_VALUE)
        return 0;   // no sub files or directories

    int cnt = 0;
    do
    {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            // dir
        }
        else
        {
            cnt++;
            if (filecb != nullptr)
                filecb(findData.cFileName);
        }
    } while (FindNextFile(findhandle, &findData) != 0);

    ::FindClose(findhandle);
    return cnt;
}

#else
#include <dirent.h>

int SimpleFile::EnumFilesInFolder(const char *foldername, void(*filecb)(const char *filename))
{
    if (foldername == nullptr)
        return ErrCodeArgument;

    struct dirent **namelist;
    int n = scandir(foldername, &namelist, 0, alphasort);
    if (n < 0)
        return n;

    for (int i = 0; i < n; ++i)
    {
        if (namelist[i]->d_type != DT_DIR && filecb != nullptr)
            filecb(namelist[i]->d_name);

        free(namelist[i]);
    }

    free(namelist);
    return n;
}

#endif

