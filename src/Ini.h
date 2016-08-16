/*
 *  project: 
 *                通用模块 ( 用STL处理 *.ini 文件 )
 *                  
 *  description:
 *
 *                  通过CHandle_IniFile类操作ini 文件，实现添加、删除、修改
 *
 *          ( the end of this file have one sample,
 *                    welcom to use... )
 *
 *
 *       file:ZLB_Handle_File.h
 *
 *  author: @ zlb
 *  
 *  time:2005-07-25  
 *
 *
 *  
 --*/
 
 
 
 
#ifndef ZLB_HANDLE_FILE__
#define ZLB_HANDLE_FILE__
 
#include <map>
#include <string>
#include <cstring>
#include <vector>
#include <algorithm>
#include <functional>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace std;
 
typedef map<string, string, less<string> > strMap;
typedef strMap::iterator strMapIt;
 
const char*const MIDDLESTRING = "_____***_______";
 
struct analyzeini{
       string strsect;
       strMap *pmap;
       analyzeini(strMap & strmap):pmap(&strmap){}       
       void operator()( const string & strini)
       {
              size_t first =strini.find('[');
              size_t last = strini.rfind(']');
              if( first != string::npos && last != string::npos && first != last+1)
              {
                     strsect = strini.substr(first+1,last-first-1);
                     return ;
              }
              if(strsect.empty())
                     return ;
              if((first=strini.find('='))== string::npos)
                     return ;
              string strtmp1= strini.substr(0,first);
              string strtmp2=strini.substr(first+1, string::npos);
              first= strtmp1.find_first_not_of(" \t");
              last = strtmp1.find_last_not_of(" \t");
              if(first == string::npos || last == string::npos)
                     return ;
              string strkey = strtmp1.substr(first, last-first+1);
              first = strtmp2.find_first_not_of(" \t");
              if(((last = strtmp2.find("\t#", first )) != -1) ||
            ((last = strtmp2.find(" #", first )) != -1) ||
            ((last = strtmp2.find("\t//", first )) != -1)||
            ((last = strtmp2.find(" //", first )) != -1))
              {
            strtmp2 = strtmp2.substr(0, last-first);
              }
              last = strtmp2.find_last_not_of(" \t");
              if(first == string::npos || last == string::npos)
                     return ;
              string value = strtmp2.substr(first, last-first+1);
              string mapkey = strsect + MIDDLESTRING;
              mapkey += strkey;
              (*pmap)[mapkey]=value;
              return ;
       }
};
 
class CIniFile
{
public:
    CIniFile( );
    ~CIniFile( );
    bool open(const char* pinipath);
	bool SetDoc(const char* pszData, unsigned int uiDataSize);

/* 
 *    读取对应的值
 *    description:
 *           psect = 主键 
 *           pkey  = 次级键
 *  return = value
 *
*/
    string read(const char*psect, const char*pkey);
/* 
 *
 *  修改主键 (rootkey)
 *  description:
 *      sItself    = 要修改的主键名
 *           sNewItself = 目标主键名
 *   return = true(成功) or false(失败)
 *
*/ 
       bool change_rootkey(char *sItself,char *sNewItself);

/* 
 *    修改次键 subkey 
 *    description:
 *      sRootkey    = 要修改的次键所属的主键名
 *           sItSubkey   = 要修改的次键名
 *           sNewSubkey  = 目标次键名
 *   return = true(成功) or false(失败)
 *
*/
       bool change_subkey(char *sRootkey, char *sItSubkey, char *sNewSubkey);
/* 
 *    修改键值 
 *    description:
 *      sRootkey    = 要修改的次键所属的主键名
 *           sSubkey     = 要修改的次键名
 *           sValue      = 次键对应的值
 *   return = true(成功) or false(失败)
 *
*/
       bool change_keyvalue(char *sRootkey, char *sSubkey, char *sValue);
/* 
 *    删除键值 
 *    description:
 *      sRootkey    = 要删除的主键名
 *           sSubkey     = 要删除的次键名 (如果为空就删除主键和主键下的所有次键)
 *    
 *   return = true(成功) or false(失败)
 *
*/
       bool del_key(char *sRootkey,
                             char *sSubkey="");
/* 
 *    增加键值
 *    description:
 *      1、sRootkey    = 要增加的主键名
 *           2、sSubkey     = 要增加的次键名 
 *           3、sKeyValue   = 要增加的次键值
 *
 *    (** 如果为1 2 同时为空就只增加主键，如果同时不为空就增加确定的键 **)
 *
 *   return = true(成功) or false(失败)
 *
*/
       bool add_key(char *sRootkey,
                             char *sSubkey="",
                             char *sKeyValue="");
/* 
 *    save ini file 
 *
 *    提供缓冲机制，当涉及到修改、删除、增加操作时，
 *    不会立即写如文件，只是改写内存，当调用次函数
 *    时写到文件，保存
 *
*/
       bool flush();

protected:
    /* 打开 ini 文件*/
       bool do_open(const char* pinipath);

private:
       /* file path */
       string s_file_path;
       /* 保存键值对应  key = value */
       strMap    c_inimap;
       /* 保存整个文件信息 */
       vector<string> vt_ini;
};
 
#endif
 
 
/*
 *
 * sample:
                 #include "ZLB_Handle_File.h"
 
                    CHandle_IniFile ini;
 
                     //-- 在本地新建一个Test.ini的文件
 
                    if(!ini.open(".\\Test.ini"))
                           return -1;
 
                    //+++++++++++++++++++++++++++++++++++++++++
                    ini.add_key("RootKeyName");
                    ini.add_key("RootKeyName","SubKeyName_1","Test_1");
                    //-------------you can do up like down-----
                    //-- ini.add_key("RootKeyName","SubKeyName_1","Test_1");
                    //-----------------------------------------
 
 
            ini.add_key("RootKeyName","SubKeyName_2","Test_2");
                    string strvalue = ini.read("RootKeyName","SubKeyName_1");
                    if(strvalue.empty())
                           return -1;
                    else
                           cout<<"value="<<strvalue<<endl;
                    if(ini.del_key("RootKeyName","SubKeyName_2"))
                    {
                           cout<<"you del one (SubKeyName_2) !!!"<<endl;
                           ini.add_key("RootKeyName","SubKeyName_2","Test_2");
                    }else
                           cout<<" del one failed !!! please check your ini file..."<<endl;
                    if(ini.del_key("RootKeyName"))
                    {
                           cout<<"you del all under the RootKeyName (SubKeyName_1 and SubKeyName_2) !!!"
                                  <<endl;
                            ini.add_key("RootKeyName","SubKeyName_1","Test_1");
                    }else
                           cout<<" del all (RootKeyName) failed !!! please check your ini file..."
                                <<endl;
                     if(ini.change_rootkey("RootKeyName","RootKeyNewName"))
                     {
                            cout<<"you change one rootkey success !!!"
                                  <<endl;
                     }else
                     {
                            cout<<"you change one rootkey failed !!!please check your ini file..."
                                  <<endl;
                     }
                    if(ini.change_subkey("RootKeyNewName","SubKeyName_1","SubKeyNewName_1"))
                     {
                            cout<<"you change one subkey success !!!"
                                  <<endl;
                     }else
                     {
                            cout<<"you change one subkey failed !!!please check your ini file..."
                                  <<endl;
                     }
                     if(ini.change_keyvalue("RootKeyNewName","SubKeyNewName_1","TestNew_1"))
                     {
                            cout<<"you change one keyvalue success !!!"
                                  <<endl;
                     }else
                     {
                            cout<<"you change one keyvalue failed !!!please check your ini file..."
                                  <<endl;
                     }       
                     ini.flush();
--*/

