#include "DataBaseOperate.h"
#include "module/Log.h"
#include "../PublicFunction.hpp"

#include <string>

namespace database
{
    std::mutex DataBaseOperate::m_mutex;
    std::shared_ptr<DataBaseOperate> DataBaseOperate::m_ptrInstance={nullptr};

    const char* kIp="127.0.0.1";
    const char* kDataBase="chat_data";
    const char* kUser="root";
    const char* kPassword="qinbiao9704";

    std::shared_ptr<DataBaseOperate> DataBaseOperate::Instance()
    {
        if (m_ptrInstance == nullptr)
        {
            std::lock_guard<std::mutex> lck(m_mutex);
            if (m_ptrInstance == nullptr)
            {
                m_ptrInstance = std::shared_ptr<DataBaseOperate>(new DataBaseOperate(kIp,kDataBase,kUser,kPassword));
            }
        }
        return m_ptrInstance;
    }

    DataBaseOperate::DataBaseOperate(const char* ip,const char* database,const char* user,const char*password):
        m_destinationIp(ip),
        m_destinationDatabase(database),
        m_destinationUser(user),
        m_destinationPassword(password)
    {
        m_mysql=mysql_init(NULL);
        initMysql();
    }

    void DataBaseOperate::initMysql()
    {
        mysql_real_connect(m_mysql,m_destinationIp.c_str(),m_destinationUser.c_str(),
        m_destinationPassword.c_str(),m_destinationDatabase.c_str(),0,NULL,0);
        if(m_mysql==NULL)
        {
            _LOG(Logcxx::ERROR,"error occurs: %s",mysql_error(m_mysql));
        }else
        {
            //printf("connect successfully\n");
        }
        mysql_query(m_mysql,"set names utf-8");
    }

    bool DataBaseOperate::execQuery(const char *query, MYSQL_RES* result)
    {
         if(mysql_query(m_mysql,query.c_str()))
        {
            _LOG(Logcxx::ERROR,"select count(*) from user_info failed");
            return false;
        }
        result=mysql_store_result(m_mysql);
        return true;
    }

}