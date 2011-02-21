// Windows/FileDir.cpp

#include "StdAfx.h"

#include "FileDir.h"
#include "FileName.h"
#include "FileFind.h"
#include "Defs.h"
#include "../Common/StringConvert.h"

#define NEED_NAME_WINDOWS_TO_UNIX
#include "myPrivate.h"
#include "Windows/Synchronization.h"
#ifdef ENV_UNIX
#include <unistd.h> // rmdir
#include <utime.h>
#endif

#include <errno.h>

#include <sys/stat.h> // mkdir
#include <sys/types.h>
#include <fcntl.h>


// #define TRACEN(u) u;
#define TRACEN(u)  /* */

#ifdef ENV_UNIX
class Umask
{
  public:
  mode_t  current_umask;
  mode_t  mask;
  Umask() {
    current_umask = umask (0);  /* get and set the umask */
    umask(current_umask);	/* restore the umask */
    mask = 0777 & (~current_umask);
  } 
};

static Umask gbl_umask;
#endif

#ifdef _UNICODE
AString nameWindowToUnix2(LPCWSTR name) // FIXME : optimization ?
{
   AString astr = UnicodeStringToMultiByte(name);
   return AString(nameWindowToUnix((const char *)astr));
}
#endif

extern BOOLEAN WINAPI RtlTimeToSecondsSince1970( const LARGE_INTEGER *Time, DWORD *Seconds );

#ifdef _UNICODE
#ifdef ENV_UNIX
DWORD WINAPI GetFullPathName( LPCTSTR name, DWORD len, LPTSTR buffer, LPTSTR *lastpart ) { // FIXME
  if (name == 0) return 0;

  DWORD name_len = lstrlen(name);

  if (name[0] == '/') {
    DWORD ret = name_len+2;
    if (ret >= len) {
      TRACEN((printf("GetFullPathNameA(%ls,%d,)=0000 (case 0)\n",name, (int)len)))
      return 0;
    }
    lstrcpy(buffer,L"c:");
    lstrcat(buffer,name);

    *lastpart=buffer;
    TCHAR *ptr=buffer;
    while (*ptr) {
      if (*ptr == '/')
        *lastpart=ptr+1;
      ptr++;
    }
    TRACEN((printf("GetFullPathNameA(%s,%d,%ls,%ls)=%d\n",name, (int)len,buffer, *lastpart,(int)ret)))
    return ret;
  }
  if (isascii(name[0]) && (name[1] == ':')) { // FIXME isascii
    DWORD ret = name_len;
    if (ret >= len) {
      TRACEN((printf("GetFullPathNameA(%ls,%d,)=0000 (case 1)\n",name, (int)len)))
      return 0;
    }
    lstrcpy(buffer,name);

    *lastpart=buffer;
    TCHAR *ptr=buffer;
    while (*ptr) {
      if (*ptr == '/')
        *lastpart=ptr+1;
      ptr++;
    }
    TRACEN((printf("GetFullPathNameA(%sl,%d,%ls,%ls)=%d\n",name, (int)len,buffer, *lastpart,(int)ret)))
    return ret;
  }

  // name is a relative pathname.
  //
  if (len < 2) {
    TRACEN((printf("GetFullPathNameA(%s,%d,)=0000 (case 2)\n",name, (int)len)))
    return 0;
  }

  DWORD ret = 0;
  char begin[MAX_PATHNAME_LEN];
  /* DWORD begin_len = GetCurrentDirectoryA(MAX_PATHNAME_LEN,begin); */
  DWORD begin_len = 0;
  begin[0]='c';
  begin[1]=':';
  char * cret = getcwd(begin+2, MAX_PATHNAME_LEN - 3);
  if (cret) {
    begin_len = strlen(begin);
  }
   
  if (begin_len >= 1) {
    //    strlen(begin) + strlen("/") + strlen(name)
    ret = begin_len     +    1        + name_len;

    if (ret >= len) {
      TRACEN((printf("GetFullPathNameA(%s,%d,)=0000 (case 4)\n",name, (int)len)))
      return 0;
    }
    UString wbegin = GetUnicodeString(begin);
    lstrcpy(buffer,wbegin);
    lstrcat(buffer,L"/");
    lstrcat(buffer,name);

    *lastpart=buffer + begin_len + 1;
    TCHAR *ptr=buffer;
    while (*ptr) {
      if (*ptr == '/')
        *lastpart=ptr+1;
      ptr++;
    }
    TRACEN((printf("GetFullPathNameA(%s,%d,%s,%s)=%d\n",name, (int)len,buffer, *lastpart,(int)ret)))
  } else {
    ret = 0;
    TRACEN((printf("GetFullPathNameA(%s,%d,)=0000 (case 5)\n",name, (int)len)))
  }
  return ret;
}
#endif
#endif

#ifdef ENV_UNIX
DWORD WINAPI GetFullPathName( LPCSTR name, DWORD len, LPSTR buffer, LPSTR *lastpart ) {
  if (name == 0) return 0;

  DWORD name_len = strlen(name);

  if (name[0] == '/') {
    DWORD ret = name_len+2;
    if (ret >= len) {
      TRACEN((printf("GetFullPathNameA(%s,%d,)=0000 (case 0)\n",name, (int)len)))
      return 0;
    }
    strcpy(buffer,"c:");
    strcat(buffer,name);

    *lastpart=buffer;
    char *ptr=buffer;
    while (*ptr) {
      if (*ptr == '/')
        *lastpart=ptr+1;
      ptr++;
    }
    TRACEN((printf("GetFullPathNameA(%s,%d,%s,%s)=%d\n",name, (int)len,buffer, *lastpart,(int)ret)))
    return ret;
  }
  if (isascii(name[0]) && (name[1] == ':')) {
    DWORD ret = name_len;
    if (ret >= len) {
      TRACEN((printf("GetFullPathNameA(%s,%d,)=0000 (case 1)\n",name, (int)len)))
      return 0;
    }
    strcpy(buffer,name);

    *lastpart=buffer;
    char *ptr=buffer;
    while (*ptr) {
      if (*ptr == '/')
        *lastpart=ptr+1;
      ptr++;
    }
    TRACEN((printf("GetFullPathNameA(%s,%d,%s,%s)=%d\n",name, (int)len,buffer, *lastpart,(int)ret)))
    return ret;
  }

  // name is a relative pathname.
  //
  if (len < 2) {
    TRACEN((printf("GetFullPathNameA(%s,%d,)=0000 (case 2)\n",name, (int)len)))
    return 0;
  }

  DWORD ret = 0;
  char begin[MAX_PATHNAME_LEN];
  /* DWORD begin_len = GetCurrentDirectoryA(MAX_PATHNAME_LEN,begin); */
  DWORD begin_len = 0;
  begin[0]='c';
  begin[1]=':';
  char * cret = getcwd(begin+2, MAX_PATHNAME_LEN - 3);
  if (cret) {
    begin_len = strlen(begin);
  }
   
  if (begin_len >= 1) {
    //    strlen(begin) + strlen("/") + strlen(name)
    ret = begin_len     +    1        + name_len;

    if (ret >= len) {
      TRACEN((printf("GetFullPathNameA(%s,%d,)=0000 (case 4)\n",name, (int)len)))
      return 0;
    }
    strcpy(buffer,begin);
    strcat(buffer,"/");
    strcat(buffer,name);

    *lastpart=buffer + begin_len + 1;
    char *ptr=buffer;
    while (*ptr) {
      if (*ptr == '/')
        *lastpart=ptr+1;
      ptr++;
    }
    TRACEN((printf("GetFullPathNameA(%s,%d,%s,%s)=%d\n",name, (int)len,buffer, *lastpart,(int)ret)))
  } else {
    ret = 0;
    TRACEN((printf("GetFullPathNameA(%s,%d,)=0000 (case 5)\n",name, (int)len)))
  }
  return ret;
}

static BOOL WINAPI RemoveDirectory(LPCSTR path) {
  if (!path || !*path) {
    SetLastError(ERROR_PATH_NOT_FOUND);
    return FALSE;
  }
  const char * name = nameWindowToUnix(path);
  TRACEN((printf("RemoveDirectoryA(%s)\n",name)))

  if (rmdir( name ) != 0) {
    return FALSE;
  }
  return TRUE;
}
#ifdef _UNICODE
static BOOL WINAPI RemoveDirectory(LPCWSTR path) {
  if (!path || !*path) {
    SetLastError(ERROR_PATH_NOT_FOUND);
    return FALSE;
  }
  AString name = nameWindowToUnix2(path);
  TRACEN((printf("RemoveDirectoryA(%s)\n",(const char *)name)))

  if (rmdir( (const char *)name ) != 0) {
    return FALSE;
  }
  return TRUE;
}
#endif

static int copy_fd(int fin,int fout)
{
  char buffer[16384];
  ssize_t ret_in;
  ssize_t ret_out;

  do {
    ret_out = -1;
    do {
      ret_in = read(fin, buffer,sizeof(buffer));
    } while (ret_in < 0 && (errno == EINTR));
    if (ret_in >= 1) {
      do {
        ret_out = write (fout, buffer, ret_in);
      } while (ret_out < 0 && (errno == EINTR));
    } else if (ret_in == 0) {
      ret_out = 0;
    }
  } while (ret_out >= 1);
  return ret_out;
}

static BOOL CopyFile(const char *src,const char *dst)
{
  int ret = -1;

#ifdef O_BINARY
  int   flags = O_BINARY;
#else
  int   flags = 0;
#endif

#ifdef O_LARGEFILE
  flags |= O_LARGEFILE;
#endif

  int fout = open(dst,O_CREAT | O_WRONLY | O_EXCL | flags, 0600);
  if (fout != -1)
  {
    int fin = open(src,O_RDONLY | flags , 0600);
    if (fin != -1)
    {
      ret = copy_fd(fin,fout);
      if (ret == 0) ret = close(fin);
      else                close(fin);
    }
    if (ret == 0) ret = close(fout);
    else                close(fout);
  }
  if (ret == 0) return TRUE;
  return FALSE;
}
#endif

/*****************************************************************************************/


namespace NWindows {
namespace NFile {
namespace NDirectory {


bool MySetCurrentDirectory(LPCWSTR wpath)
{
#ifdef ENV_UNIX
   AString path = UnicodeStringToMultiByte(wpath);

   return chdir((const char*)path) == 0;
#else
    return BOOLToBool( ::SetCurrentDirectoryW( wpath ) );
#endif
}

#ifdef _UNICODE
bool GetOnlyName(LPCTSTR fileName, CSysString &resultName)
{
  int index;
  if (!MyGetFullPathName(fileName, resultName, index))
    return false;
  resultName = resultName.Mid(index);
  return true;
}

bool GetOnlyDirPrefix(LPCTSTR fileName, CSysString &resultName)
{
  int index;
  if (!MyGetFullPathName(fileName, resultName, index))
    return false;
  resultName = resultName.Left(index);
  return true;
}
#endif


bool MyGetCurrentDirectory(CSysString &resultPath)
{
#ifdef ENV_UNIX
  char begin[MAX_PATHNAME_LEN];
  begin[0]='c';
  begin[1]=':';
  char * cret = getcwd(begin+2, MAX_PATHNAME_LEN - 3);
  if (cret)
  {
#ifdef _UNICODE
    resultPath = GetUnicodeString(begin);
#else
    resultPath = begin;
#endif
    return true;
  }
  return false;
#else
    wchar_t buffer[ MAX_PATH + 1 ];
    const DWORD needLength = ::GetCurrentDirectoryW( MAX_PATH + 1, buffer );
    resultPath = buffer;
    return needLength > 0 && needLength <= MAX_PATH;
#endif
}

bool MyMoveFile( LPCTSTR fn1, LPCTSTR fn2 ) {
#ifdef ENV_UNIX
#ifdef _UNICODE
  AString src = nameWindowToUnix2(fn1);
  AString dst = nameWindowToUnix2(fn2);
#else
  const char * src = nameWindowToUnix(fn1);
  const char * dst = nameWindowToUnix(fn2);
#endif

  TRACEN((printf("MoveFileW(%s,%s)\n",src,dst)))

  int ret = rename(src,dst);
  if (ret != 0)
  {
    if (errno == EXDEV) // FIXED : bug #1112167 (Temporary directory must be on same partition as target)
    {
      BOOL bret = CopyFile(src,dst);
      if (bret == FALSE) return false;

      struct stat info_file;
      ret = stat(src,&info_file);
      if (ret == 0) {
	TRACEN((printf("##DBG chmod-1(%s,%o)\n",dst,(unsigned)info_file.st_mode & gbl_umask.mask)))
        ret = chmod(dst,info_file.st_mode & gbl_umask.mask);
      }
      if (ret == 0) {
         ret = unlink(src);
      }
      if (ret == 0) return true;
    }
    return false;
  }
  return true;
#else
    if( ::MoveFile( fn1, fn2 ) )
        return true;
#ifdef WIN_LONG_PATH2
    UString d1, d2;
    if( GetLongPaths( fn1, fn2, d1, d2 ) )
        return BOOLToBool( ::MoveFileW( d1, d2 ) );
#endif
    return false;
#endif
}

bool MyRemoveDirectory(LPCTSTR pathName)
{
  return BOOLToBool(::RemoveDirectory(pathName));
}

bool SetDirTime(LPCWSTR fileName, const FILETIME * lpCreationTime,
      const FILETIME *lpLastAccessTime, const FILETIME *lpLastWriteTime)
{
#ifdef ENV_UNIX
  (void)lpCreationTime;
  AString  cfilename = UnicodeStringToMultiByte(fileName);
  const char * unix_filename = nameWindowToUnix((const char *)cfilename);

  struct utimbuf buf;

  struct stat    oldbuf;
  int ret = stat(unix_filename,&oldbuf);
  if (ret == 0) {
    buf.actime  = oldbuf.st_atime;
    buf.modtime = oldbuf.st_mtime;
  } else {
    time_t current_time = time(0);
    buf.actime  = current_time;
    buf.modtime = current_time;
  }

  if (lpLastAccessTime)
  {
    LARGE_INTEGER  ltime;
    DWORD dw;
    ltime.QuadPart = lpLastAccessTime->dwHighDateTime;
    ltime.QuadPart = (ltime.QuadPart << 32) | lpLastAccessTime->dwLowDateTime;
    RtlTimeToSecondsSince1970( &ltime, &dw );
    buf.actime = dw;
  }

  if (lpLastWriteTime)
  {
    LARGE_INTEGER  ltime;
    DWORD dw;
    ltime.QuadPart = lpLastWriteTime->dwHighDateTime;
    ltime.QuadPart = (ltime.QuadPart << 32) | lpLastWriteTime->dwLowDateTime;
    RtlTimeToSecondsSince1970( &ltime, &dw );
    buf.modtime = dw;
  }

  /* ret = */ utime(unix_filename, &buf);

  return true;
#else
    HANDLE hDir = ::CreateFileW( fileName, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 
                                 NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL );
#ifdef WIN_LONG_PATH
    if( hDir == INVALID_HANDLE_VALUE )
    {
        UString longPath;
        if( GetLongPath( fileName, longPath ) )
            hDir = ::CreateFileW( longPath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL );
    }
#endif

    bool res = false;
    if( hDir != INVALID_HANDLE_VALUE )
    {
        res = BOOLToBool( ::SetFileTime( hDir, lpCreationTime, lpLastAccessTime, lpLastWriteTime ) ); 
        ::CloseHandle( hDir );
    }
    return res;
#endif
}

#ifndef _UNICODE
bool MySetFileAttributes(LPCWSTR fileName, DWORD fileAttributes)
{  
  return MySetFileAttributes(UnicodeStringToMultiByte(fileName, CP_ACP), fileAttributes);
}

bool MyRemoveDirectory(LPCWSTR pathName)
{  
  return MyRemoveDirectory(UnicodeStringToMultiByte(pathName, CP_ACP));
}

bool MyMoveFile(LPCWSTR existFileName, LPCWSTR newFileName)
{  
  UINT codePage = CP_ACP;
  return MyMoveFile(UnicodeStringToMultiByte(existFileName, codePage), UnicodeStringToMultiByte(newFileName, codePage));
}
#endif

#if ENV_UNIX
static int convert_to_symlink(const char * name) {
  FILE *file = fopen(name,"rb");
  if (file) {
    char buf[MAX_PATHNAME_LEN+1];
    char * ret = fgets(buf,sizeof(buf)-1,file);
    fclose(file);
    if (ret) {
      int ir = unlink(name);
      if (ir == 0) {
        ir = symlink(buf,name);
      }
      return ir;    
    }
  }
  return -1;
}
#endif

bool MySetFileAttributes(LPCTSTR fileName, DWORD fileAttributes)
{
#ifdef ENV_UNIX
  if (!fileName) {
    SetLastError(ERROR_PATH_NOT_FOUND);
    TRACEN((printf("MySetFileAttributes(NULL,%d) : false-1\n",fileAttributes)))
    return false;
  }
#ifdef _UNICODE
  AString name = nameWindowToUnix2(fileName);
#else
  const char * name = nameWindowToUnix(fileName);
#endif
  struct stat stat_info;
#ifdef HAVE_LSTAT
  if (global_use_lstat) {
    if(lstat(name,&stat_info)!=0) {
      TRACEN((printf("MySetFileAttributes(%s,%d) : false-2-1\n",name,fileAttributes)))
      return false;
    }
  } else
#endif
  {
    if(stat(name,&stat_info)!=0) {
      TRACEN((printf("MySetFileAttributes(%s,%d) : false-2-2\n",name,fileAttributes)))
      return false;
    }
  }

  if (fileAttributes & FILE_ATTRIBUTE_UNIX_EXTENSION) {
     stat_info.st_mode = fileAttributes >> 16;
#ifdef HAVE_LSTAT
     if (S_ISLNK(stat_info.st_mode)) {
        if ( convert_to_symlink(name) != 0) {
          TRACEN((printf("MySetFileAttributes(%s,%d) : false-3\n",name,fileAttributes)))
          return false;
        }
     } else
#endif
     if (S_ISREG(stat_info.st_mode)) {
       TRACEN((printf("##DBG chmod-2(%s,%o)\n",name,(unsigned)stat_info.st_mode & gbl_umask.mask)))
       chmod(name,stat_info.st_mode & gbl_umask.mask);
     } else if (S_ISDIR(stat_info.st_mode)) {
       // user/7za must be able to create files in this directory
       stat_info.st_mode |= (S_IRUSR | S_IWUSR | S_IXUSR);
       TRACEN((printf("##DBG chmod-3(%s,%o)\n",name,(unsigned)stat_info.st_mode & gbl_umask.mask)))
       chmod(name,stat_info.st_mode & gbl_umask.mask);
     }
#ifdef HAVE_LSTAT
  } else if (!S_ISLNK(stat_info.st_mode)) {
    // do not use chmod on a link
#else
  } else {
#endif

    /* Only Windows Attributes */
    if( S_ISDIR(stat_info.st_mode)) {
       /* Remark : FILE_ATTRIBUTE_READONLY ignored for directory. */
       TRACEN((printf("##DBG chmod-4(%s,%o)\n",name,(unsigned)stat_info.st_mode & gbl_umask.mask)))
       chmod(name,stat_info.st_mode & gbl_umask.mask);
    } else {
       if (fileAttributes & FILE_ATTRIBUTE_READONLY) stat_info.st_mode &= ~0222; /* octal!, clear write permission bits */
       TRACEN((printf("##DBG chmod-5(%s,%o)\n",name,(unsigned)stat_info.st_mode & gbl_umask.mask)))
       chmod(name,stat_info.st_mode & gbl_umask.mask);
    }
  }
  TRACEN((printf("MySetFileAttributes(%s,%d) : true\n",name,fileAttributes)))

  return true;
#else
    if( ::SetFileAttributes( fileName, fileAttributes ) )
        return true;
#ifdef WIN_LONG_PATH2
    UString longPath;
    if( GetLongPath( fileName, longPath ) )
        return BOOLToBool( ::SetfileAttributesW( longPath, fileAttributes ) );
#endif
    return false;
#endif
}

bool MyCreateDirectory(LPCTSTR pathName)
{  
#ifdef ENV_UNIX
  if (!pathName || !*pathName) {
    SetLastError(ERROR_PATH_NOT_FOUND);
    return false;
  }

#ifdef _UNICODE
  AString name = nameWindowToUnix2(pathName);
#else
  const char * name = nameWindowToUnix(pathName);
#endif
  bool bret = false;
  if (mkdir( name, 0700 ) == 0) bret = true;

  TRACEN((printf("MyCreateDirectory(%s)=%d\n",name,(int)bret)))
  return bret;
#else
    if( ::CreateDirectory( pathName, NULL ) )
        return true;
#ifdef WIN_LONG_PATH2
    if( ::GetLastError() != ERROR_ALREADY_EXISTS )
    {
        UString longPath;
        if( GetLongPath( pathName, longPath ) )
            return BOOLToBool( ::CreateDirectoryW( longPath, NULL ) );
    }
#endif
    return false;
#endif
}

#ifndef _UNICODE
bool MyCreateDirectory(LPCWSTR pathName)
{  
  return MyCreateDirectory(UnicodeStringToMultiByte(pathName, CP_ACP));
}
#endif

bool CreateComplexDirectory(LPCTSTR _aPathName)
{
  CSysString pathName = _aPathName;
  int pos = pathName.ReverseFind(TEXT(CHAR_PATH_SEPARATOR));
  if (pos > 0 && pos == pathName.Length() - 1)
  {
    if (pathName.Length() == 3 && pathName[1] == ':')
      return true; // Disk folder;
    pathName.Delete(pos);
  }
  CSysString pathName2 = pathName;
  pos = pathName.Length();
  while(true)
  {
    if(MyCreateDirectory(pathName))
      break;
    if(::GetLastError() == ERROR_ALREADY_EXISTS)
    {
#ifdef _WIN32 // FIXED for supporting symbolic link instead of a directory
      NFind::CFileInfoW fileInfo;
      if (!NFind::FindFile(pathName, fileInfo)) // For network folders
        return true;
      if (!fileInfo.IsDir())
        return false;
#endif
      break;
    }
    pos = pathName.ReverseFind(TEXT(CHAR_PATH_SEPARATOR));
    if (pos < 0 || pos == 0)
      return false;
    if (pathName[pos - 1] == ':')
      return false;
    pathName = pathName.Left(pos);
  }
  pathName = pathName2;
  while(pos < pathName.Length())
  {
    pos = pathName.Find(TEXT(CHAR_PATH_SEPARATOR), pos + 1);
    if (pos < 0)
      pos = pathName.Length();
    if(!MyCreateDirectory(pathName.Left(pos)))
      return false;
  }
  return true;
}

#ifndef _UNICODE

bool CreateComplexDirectory(LPCWSTR _aPathName)
{
  UString pathName = _aPathName;
  int pos = pathName.ReverseFind(WCHAR_PATH_SEPARATOR);
  if (pos > 0 && pos == pathName.Length() - 1)
  {
    if (pathName.Length() == 3 && pathName[1] == L':')
      return true; // Disk folder;
    pathName.Delete(pos);
  }
  UString pathName2 = pathName;
  pos = pathName.Length();
  while(true)
  {
    if(MyCreateDirectory(pathName))
      break;
    if(::GetLastError() == ERROR_ALREADY_EXISTS)
    {
#ifdef _WIN32 // FIXED for supporting symbolic link instead of a directory
      NFind::CFileInfoW fileInfo;
      if (!NFind::FindFile(pathName, fileInfo)) // For network folders
        return true;
      if (!fileInfo.IsDir())
        return false;
#endif
      break;
    }
    pos = pathName.ReverseFind(WCHAR_PATH_SEPARATOR);
    if (pos < 0 || pos == 0)
      return false;
    if (pathName[pos - 1] == L':')
      return false;
    pathName = pathName.Left(pos);
  }
  pathName = pathName2;
  while(pos < pathName.Length())
  {
    pos = pathName.Find(WCHAR_PATH_SEPARATOR, pos + 1);
    if (pos < 0)
      pos = pathName.Length();
    if(!MyCreateDirectory(pathName.Left(pos)))
      return false;
  }
  return true;
}

#endif

bool DeleteFileAlways(LPCTSTR name)
{
#ifdef ENV_UNIX
  if (!name || !*name) {
    SetLastError(ERROR_PATH_NOT_FOUND);
    return false;
  }
#ifdef _UNICODE
   AString unixname = nameWindowToUnix2(name);
#else
   const char * unixname = nameWindowToUnix(name);
#endif
   bool bret = false;
   if (remove(unixname) == 0) bret = true;
   TRACEN((printf("DeleteFileAlways(%s)=%d\n",unixname,(int)bret)))
   return bret;
#else
    if( !MySetFileAttributes( name, 0 ) )
        return false;
    if( ::DeleteFile( name ) )
        return true;
#ifdef WIN_LONG_PATH2
    UString longPath;
    if( GetLongPath( name, longPath ) )
        return BOOLToBool( ::DeleteFileW( longPath ) );
#endif
    return false;
#endif
}

#ifndef _UNICODE
bool DeleteFileAlways(LPCWSTR name)
{  
  return DeleteFileAlways(UnicodeStringToMultiByte(name, CP_ACP));
}
#endif

#ifndef _WIN32_WCE

bool MyGetFullPathName(LPCTSTR fileName, CSysString &resultPath, 
    int &fileNamePartStartIndex)
{
  LPTSTR fileNamePointer = 0;
  LPTSTR buffer = resultPath.GetBuffer(MAX_PATH);
  DWORD needLength = ::GetFullPathName(fileName, MAX_PATH + 1, 
      buffer, &fileNamePointer);
  resultPath.ReleaseBuffer();
  if (needLength == 0 || needLength >= MAX_PATH)
    return false;
  if (fileNamePointer == 0)
    fileNamePartStartIndex = lstrlen(fileName);
  else
    fileNamePartStartIndex = fileNamePointer - buffer;
  return true;
}

#ifndef _UNICODE
bool MyGetFullPathName(LPCWSTR fileName, UString &resultPath, 
    int &fileNamePartStartIndex)
{
    const UINT currentPage = CP_ACP;
    CSysString sysPath;
    if (!MyGetFullPathName(UnicodeStringToMultiByte(fileName, 
        currentPage), sysPath, fileNamePartStartIndex))
      return false;
    UString resultPath1 = MultiByteToUnicodeString(
        sysPath.Left(fileNamePartStartIndex), currentPage);
    UString resultPath2 = MultiByteToUnicodeString(
        sysPath.Mid(fileNamePartStartIndex), currentPage);
    fileNamePartStartIndex = resultPath1.Length();
    resultPath = resultPath1 + resultPath2;
    return true;
}
#endif


bool MyGetFullPathName(LPCTSTR fileName, CSysString &path)
{
  int index;
  return MyGetFullPathName(fileName, path, index);
}

#ifndef _UNICODE
bool MyGetFullPathName(LPCWSTR fileName, UString &path)
{
  int index;
  return MyGetFullPathName(fileName, path, index);
}
#endif

#endif

#ifndef ENV_UNIX
static inline UINT GetCurrentCodePage() { return ::AreFileApisANSI() ? CP_ACP : CP_OEMCP; }
static UString GetUnicodePath(const CSysString &sysPath)
{ return sysPath; }
  //{ return MultiByteToUnicodeString(sysPath, GetCurrentCodePage()); }

static CSysString GetSysPath(LPCWSTR sysPath)
{ return sysPath; }
  //{ return UnicodeStringToMultiByte(sysPath, GetCurrentCodePage()); }

bool MySearchPath( LPCTSTR path, LPCTSTR fileName, LPCTSTR extension,
                   CSysString& resultPath, UINT32& filePart )
{
    LPTSTR filePartPointer;
    DWORD value = ::SearchPath( path, fileName, extension, MAX_PATH,
                                resultPath.GetBuffer( MAX_PATH + 1 ), &filePartPointer );
    filePart = (UINT32)(filePartPointer - (LPCTSTR)resultPath );
    resultPath.ReleaseBuffer();
    return (value > 0 && value <= MAX_PATH);
}
#endif

/* needed to find .DLL/.so and SFX */
bool MySearchPath(LPCWSTR path, LPCWSTR fileName, LPCWSTR extension, UString &resultPath)
{
#ifdef ENV_UNIX
  if (path != 0) {
    printf("NOT EXPECTED : MySearchPath : path != NULL\n");
    exit(EXIT_FAILURE);
  }

  if (extension != 0) {
    printf("NOT EXPECTED : MySearchPath : extension != NULL\n");
    exit(EXIT_FAILURE);
  }

  if (fileName == 0) {
    printf("NOT EXPECTED : MySearchPath : fileName == NULL\n");
    exit(EXIT_FAILURE);
  }

  const char *p7zip_home_dir = getenv("P7ZIP_HOME_DIR");
  if (p7zip_home_dir) {
    AString file_path = p7zip_home_dir;
    file_path += UnicodeStringToMultiByte(fileName, CP_ACP);

    TRACEN((printf("MySearchPath() fopen(%s)\n",(const char *)file_path)))
    FILE *file = fopen((const char *)file_path,"r");
    if (file) {
      // file is found
      fclose(file);
      resultPath = MultiByteToUnicodeString(file_path, CP_ACP);
      return true;
    }
  }
  return false;
#else
    CSysString sysPath;
    UINT32 filePart;
    if( !MySearchPath( 
        path != 0 ? (LPCTSTR)GetSysPath( path ) : 0,
        fileName != 0 ? (LPCTSTR)GetSysPath( fileName ) : 0,
        extension != 0 ? (LPCTSTR)GetSysPath( extension ) : 0, 
        sysPath, filePart ) )
        return false;
    UString resultPath1 = GetUnicodePath( sysPath.Left( filePart ) );
    UString resultPath2 = GetUnicodePath( sysPath.Mid( filePart ) );
    filePart = resultPath1.Length();
    resultPath = resultPath1 + resultPath2;
    return true;
#endif
}

#ifndef _UNICODE
bool MyGetTempPath(CSysString &path)
{
  path = "c:/tmp/"; // final '/' is needed
  return true;
}
#endif

bool MyGetTempPath(UString &path)
{
  path = L"c:/tmp/"; // final '/' is needed
  return true;
}

static NSynchronization::CCriticalSection g_CountCriticalSection;

#ifndef ENV_UNIX
static UINT MyGetTempFileName( LPCWSTR dirPath, LPCTSTR prefix, CSysString& path )
{
    UINT number = ::GetTempFileName( dirPath, prefix, 0, path.GetBuffer( MAX_PATH + 1 ) );
    path.ReleaseBuffer();
    return number;
}
#endif

UINT CTempFile::Create(LPCTSTR dirPath, LPCTSTR prefix, CSysString &resultPath)
{
  static int memo_count = 0;
  int count;

  g_CountCriticalSection.Enter();
  count = memo_count++;
  g_CountCriticalSection.Leave();

  Remove();
#ifdef ENV_UNIX
  UINT number = (UINT)getpid();
  TCHAR * buf = resultPath.GetBuffer(MAX_PATH);
#ifdef _UNICODE
  swprintf(buf,MAX_PATH,L"%s%s#%x@%x.tmp",dirPath,prefix,(unsigned)number,count);
#else
  snprintf(buf,MAX_PATH,"%s%s#%x@%x.tmp",dirPath,prefix,(unsigned)number,count);
#endif
  buf[MAX_PATH-1]=0;
  resultPath.ReleaseBuffer();

#else
  UINT number = MyGetTempFileName(dirPath, prefix, resultPath );
#endif
 
  
  _fileName = resultPath;
  _mustBeDeleted = true;
 
  return number;
}

bool CTempFile::Remove()
{
  if (!_mustBeDeleted)
    return true;
  _mustBeDeleted = !DeleteFileAlways(_fileName);
  return !_mustBeDeleted;
}

}}}
